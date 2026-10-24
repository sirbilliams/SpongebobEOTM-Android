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

#include "OMedia3DMorphAnimFrame.h"
#include "OMediaStreamOperators.h"

OMedia3DMorphAnimFrame::OMedia3DMorphAnimFrame()
{
}

OMedia3DMorphAnimFrame::~OMedia3DMorphAnimFrame()
{
}

inline float omf_ReverseL2Float(unsigned int l)
{
	l = omd_IfLittleEndianReverseLong(l);
	return *((float*)(&l));
}

void OMedia3DMorphAnimFrame::read_class(OMediaStreamOperators &stream)
{
	int	n;
	OMedia3DPoint	p;
	OMedia3DVector	v;
	OMediaFARGBColor	c;

	#define	buffer_pop(x) x = omf_ReverseL2Float(*buffer);	buffer++


	OMediaAnimFrame::read_class(stream);

	vertices.erase(vertices.begin(),vertices.end());
	normals.erase(normals.begin(),normals.end());
	colors.erase(colors.begin(),colors.end());

	unsigned int	*buffer,*base_buffer;

	stream>>n;
	if (n)
	{
		base_buffer = buffer = new unsigned int[n*3];
		stream.read(buffer,n*3*4);
		while(n--)
		{
			buffer_pop(p.x);
			buffer_pop(p.y);
			buffer_pop(p.z);
			vertices.push_back(p);
		}

		delete base_buffer;
	}

	stream>>n;
	if (n)
	{
		base_buffer = buffer = new unsigned int[n*3];
		stream.read(buffer,n*3*4);
		while(n--)
		{
			buffer_pop(v.x);
			buffer_pop(v.y);
			buffer_pop(v.z);
			normals.push_back(v);
		}

		delete base_buffer;
	}

	stream>>n;
	if (n)
	{
		base_buffer = buffer = new unsigned int[n*4];
		stream.read(buffer,n*4*4);
		while(n--)
		{
			buffer_pop(c.alpha);
			buffer_pop(c.red);
			buffer_pop(c.green);
			buffer_pop(c.blue);
			colors.push_back(c);
		}

		delete base_buffer;
	}
}

inline unsigned int omf_ReverseFloat(float f)
{
	unsigned int	l = *((unsigned int*)(&f));
	return omd_IfLittleEndianReverseLong(l);
}

void OMedia3DMorphAnimFrame::write_class(OMediaStreamOperators &stream)
{
	#define	buffer_push(x) (*buffer) = x;	buffer++

	OMediaAnimFrame::write_class(stream);

	unsigned int	n;
	unsigned int	*buffer,*buf_base;
	unsigned int	buf_siz = 3+
								(3 * vertices.size()) +
								(3 * normals.size()) +
								(4 * colors.size());

	buf_base = buffer = new unsigned int[buf_siz];

	n = vertices.size();
	n = omd_IfLittleEndianReverseLong(n);

	buffer_push(n);
	for(omt_VertexList::iterator vi = vertices.begin();
		vi!=vertices.end();
		vi++)
	{
		buffer_push(omf_ReverseFloat((*vi).x));
		buffer_push(omf_ReverseFloat((*vi).y));
		buffer_push(omf_ReverseFloat((*vi).z));
	}

	n = normals.size();
	n = omd_IfLittleEndianReverseLong(n);

	buffer_push(n);
	for(omt_NormalList::iterator ni = normals.begin();
		ni!=normals.end();
		ni++)
	{
		buffer_push(omf_ReverseFloat((*ni).x));
		buffer_push(omf_ReverseFloat((*ni).y));
		buffer_push(omf_ReverseFloat((*ni).z));
	}


	n = colors.size(); 
	n = omd_IfLittleEndianReverseLong(n);

	buffer_push(n);
	for(omt_ColorList::iterator ci = colors.begin();
		ci!=colors.end();
		ci++)
	{
		buffer_push(omf_ReverseFloat((*ci).alpha));
		buffer_push(omf_ReverseFloat((*ci).red));
		buffer_push(omf_ReverseFloat((*ci).green));
		buffer_push(omf_ReverseFloat((*ci).blue));
	}

	stream.write(buf_base,buf_siz*4);
	delete buf_base;

}


