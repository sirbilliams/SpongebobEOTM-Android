/*****************************************************************

        O P E N      M E D I A     T O O L K I T              V2.5    
 
        Copyright Yves Schmid 1996-2003
 
        See www.garagecube.com for more informations about this library.
        
        Author(s): Yves Schmid
 
        OMT is provided under LGPL:
 
          This library is free software; you can redistribute it and/or
          modify it under the terms of the GNU Lesser General Public
          License as published by the Free Software Foundation; either
          version 2.1 of the License, or (at your option) any later version.

          This library is distributed in the hope that it will be useful,
          but WITHOUT ANY WARRANTY; without even the implied warranty of
          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
          Lesser General Public License for more details.

          You should have received a copy of the GNU Lesser General Public
          License along with this library; if not, write to the Free Software
          Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

          The full text of the license can be found in lgpl.txt          

******************************************************************/
 
#include "OMediaStream.h"
#include "OMediaMemTools.h"
#include "OMediaError.h"

//----------------------------------------------------------

OMediaStream::OMediaStream(void)
{
	buffer = NULL;
	buffer_size=0;
	buffer_start = buffer_end=buffer_pointer=0 ;
	buffer_dirty = false;
}

//----------------------------------------------------------

OMediaStream::~OMediaStream(void)
{
	delete [] buffer;
}

//----------------------------------------------------------

unsigned int OMediaStream::getsize(void) 
{
	if (!buffer) return do_getsize();

	unsigned int siz;
	
	siz = do_getsize();
	if ((unsigned int)buffer_end>siz) siz = buffer_end;
	
	return siz;
}

void OMediaStream::setsize(unsigned int newsize) 
{
	if (!buffer) do_setsize(newsize);
	else
	{
		if (newsize>=getsize())
		{
			if (buffer_start==buffer_end) stream2buffer(buffer_start);
		
			if ((buffer_start+buffer_size)>=(int)newsize) 
			{
				buffer_end = newsize;
				buffer_dirty = true;
			}
			else do_setsize(newsize);
		}
		else
		{	
			int coff = buffer_start+buffer_pointer;
	
			flush_buffer();
			do_setsize(newsize);
			if (coff>(int)newsize) coff = newsize;
			stream2buffer(coff);
		}
	}
}

//----------------------------------------------------------

void OMediaStream::read(void *dat, unsigned int nbytes)
{
	if (!buffer) 
	{
		do_read(dat,nbytes);
		return;
	}

	if (buffer_start==buffer_end) stream2buffer(buffer_start);
	
	if (int(buffer_start+buffer_pointer+nbytes)<=buffer_end)
	{
		switch(nbytes)
		{
			case 0:
			return;
		
			case 1:
			*((char*)dat) = buffer[buffer_pointer];
			buffer_pointer++;
			break;

			case 2:
			*((short*)dat) = *((short*)(buffer+buffer_pointer));
			buffer_pointer+=2;
			break;

			case 4:
			*((int*)dat) = *((int*)(buffer+buffer_pointer));
			buffer_pointer+=4;
			break;
	
			default:
			OMediaMemTools::copy(buffer+buffer_pointer,dat,nbytes);	
			buffer_pointer+=nbytes;
			break;
		}
	}
	else if ((int)nbytes>(buffer_size<<1))
	{
		flush_buffer();
		do_setposition(buffer_start+buffer_pointer);
		do_read(dat,nbytes);
		stream2buffer(buffer_start+buffer_pointer+nbytes);
	}
	else
	{	
		int p = buffer_start+buffer_pointer,n;
	
		while(nbytes)
		{
			n = buffer_size;
			if (n>(int)nbytes) n = nbytes;
		
			stream2buffer(p);
			OMediaMemTools::copy(buffer,dat,n);
			nbytes-=n;
			dat = ((char*)dat)+n;
			p += n;		
		}
		
		buffer_pointer = p-buffer_start;
	}

}

void OMediaStream::write(void *dat, unsigned int nbytes)
{
	if (!buffer) 
	{
		do_write(dat,nbytes);
		return;
	}

	if (buffer_start==buffer_end) stream2buffer(buffer_start);
	
	if (int(buffer_pointer+nbytes)<=buffer_size)
	{
		switch(nbytes)
		{
			case 0:
			return;
		
			case 1:
			buffer[buffer_pointer] = *((char*)dat);
			buffer_pointer++;
			break;

			case 2:
			*((short*)(buffer+buffer_pointer)) = *((short*)dat);
			buffer_pointer+=2;
			break;

			case 4:
			*((int*)(buffer+buffer_pointer)) = *((int*)dat);
			buffer_pointer+=4;
			break;

			default:
			OMediaMemTools::copy(dat,buffer+buffer_pointer,nbytes);
			buffer_pointer+=nbytes;	
			break;
		}

		buffer_dirty = true;
		if ((buffer_start+buffer_pointer)>buffer_end) buffer_end = buffer_start+buffer_pointer;
	}
	else
	{
		int p = buffer_start+buffer_pointer,n;
	
		while(nbytes)
		{
			n = buffer_size;
			if (n>(int)nbytes) n = nbytes;

			stream2buffer(p,n==buffer_size);		
	
			OMediaMemTools::copy(dat,buffer,n);
			buffer_dirty = true;			
			nbytes-=n;
			dat = ((char*)dat)+n;
			p += n;
			if (p>buffer_end) buffer_end = p;		
		}
		
		buffer_pointer = p-buffer_start;
	}	
}

//----------------------------------------------------------

int OMediaStream::getposition(void) 
{
	if (!buffer) return do_getposition();
	return buffer_start+buffer_pointer;
}

void OMediaStream::setposition(int offset, short relative)
{
	if (!buffer) do_setposition(offset,relative);
	else
	{
		unsigned int off;
		
		switch(relative)
		{	
			case omcfr_Start:
			off = offset;
			break;
			
			case omcfr_End:
			off = getsize()-offset;
			break;
			
			case omcfr_Current:
			off = getposition()+offset;
			break;	
		}
				
		if (buffer_start==buffer_end ||
			(int)off<buffer_start || 
			(int)off>buffer_end) stream2buffer(off);

		else buffer_pointer = off-buffer_start;
	}
}

//----------------------------------------------------------

void OMediaStream::stream2buffer(int pos, bool disable_read)
{
	if (!buffer) return;

	flush_buffer();
	
	int	cursize = do_getsize();
	
	buffer_start = pos;
	buffer_end = pos + buffer_size;
	if (buffer_end>cursize) buffer_end=cursize;
	buffer_pointer = 0;
	
	if (buffer_end!=buffer_start && !disable_read) 
	{
		do_setposition(buffer_start);
		do_read(buffer,buffer_end-buffer_start);
	}
}

//----------------------------------------------------------

void OMediaStream::flush_buffer(void)
{	
	if (buffer && buffer_dirty)
	{
		do_setposition(buffer_start,omcfr_Start);
		do_write(buffer,buffer_end-buffer_start);
		buffer_dirty = false;
	}
}

//----------------------------------------------------------

void OMediaStream::set_buffer_size(int siz)
{
	flush_buffer();

	delete [] buffer;
	buffer = NULL;
	
	buffer_size = siz;
	if (buffer_size) buffer = new char[buffer_size];	
	
	buffer_start = buffer_end = buffer_pointer = 0;
}

//----------------------------------------------------------
// Abstract method

void OMediaStream::do_read(void *buffer, unsigned int nbytes) {}
void OMediaStream::do_write(void *buffer, unsigned int nbytes) {}

void OMediaStream::do_setposition(int offset, short relative) {}
int OMediaStream::do_getposition(void) {return 0L;}

unsigned int OMediaStream::do_getsize(void) {return 0L;}
void OMediaStream::do_setsize(unsigned int newsize) {}

