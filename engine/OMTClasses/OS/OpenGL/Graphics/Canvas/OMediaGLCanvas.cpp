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

#include "OMediaBuildSwitch.h"

#ifdef omd_ENABLE_OPENGL

#include "OMediaCanvas.h"
#include "OMediaGLCanvas.h"
#include "OMediaGLRenderer.h"
#include "OMediaEngineImplementation.h"
#include "OMediaMipmapGenerator.h"
#include "OMediaVideoEngine.h"
#include "OMediaBlitter.h"
#include "OMediaHDPack.h"

OMediaGLCanvas::OMediaGLCanvas(	OMediaGLRenderTarget *atarget,
							    OMediaCanvas *master,
							    omt_CanvasSlaveKeyFlags key_flags)
								:OMediaCanvasSlave(atarget,master)
{
	texturegrid_id = NULL;
	hd_src = NULL;						// port: HD texture pack
	hd_scale = 0;
	hd_bytes = 0;
	slave_key_long = 0;
	slave_key_ptr = 0;
	
	target = atarget;

	internal_format =  find_internal_format(master->get_internal_pixel_format());

	create_texture(true, key_flags);

	dirty = false;
}

OMediaGLCanvas::~OMediaGLCanvas()
{
	target->set_context();

	glFinish();

	if (texturegrid_id)
	{
		glDeleteTextures(n_texture,texturegrid_id);
		delete [] texturegrid_id;
	}
	else glDeleteTextures(1,&texture_id);

	omhd::sub_bytes(hd_bytes);		// port: HD texture pack
}

// port: HD texture pack: drops the textures (HD ones, to be made again from the canvas pixels)
void OMediaGLCanvas::release_textures(void)
{
	if (texturegrid_id)
	{
		glDeleteTextures(n_texture,texturegrid_id);
		delete [] texturegrid_id;
		texturegrid_id = NULL;
	}
	else glDeleteTextures(1,&texture_id);
	omhd::sub_bytes(hd_bytes);
	hd_bytes = 0;
	hd_scale = 0;
}

// port: HD texture pack: uploads 'src' (any size, already a valid texture size) as the bound texture,
// with mipmaps when the master canvas's filtering wants them
static void upload_full(OMediaCanvas *master, OMediaCanvas *src, GLuint internal_format)
{
	src->lock(omlf_Read);
	if (master->get_filtering_min()<=omtfc_Linear)
	{
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glTexImage2D(GL_TEXTURE_2D,0,internal_format,src->get_width(),src->get_height(),0,GL_RGBA,GL_UNSIGNED_BYTE,src->get_pixels());
	}
	else
	{
		OMediaMipmapGenerator *mm_gen = new OMediaMipmapGenerator(src);
		do
		{
			glPixelStorei(GL_UNPACK_ROW_LENGTH, mm_gen->full_width);
			glTexImage2D(GL_TEXTURE_2D,mm_gen->level,internal_format,mm_gen->cur_width,mm_gen->cur_height,0,
						 GL_RGBA,GL_UNSIGNED_BYTE,mm_gen->pixels);
		}
		while(mm_gen->generate_next_mipmap());
		delete mm_gen;
	}
	src->unlock();
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}


static unsigned int getnearestpow(unsigned int n)
{
	unsigned int bit;

	for(bit= 32;;)
	{
		bit--;
		
		if (n&(1<<bit)) return (1<<bit);
		if (bit==0) break;
	}
	
	return 0;
}

static unsigned int getnearestpow_shift(unsigned int n)
{
	unsigned int bit;

	for(bit= 32;;)
	{
		bit--;
		
		if (n&(1<<bit)) return bit;
		if (bit==0) break;
	}
	
	return 0;
}

GLuint OMediaGLCanvas::find_internal_format(omt_PixelFormat pix_fmt)
{
	short depth;

	if (pix_fmt==ompixfc_Best) return GL_RGBA;
	else if (pix_fmt==ompixfc_ResBest)
	{
            if (((OMediaRenderTarget*)get_engine())->get_renderer())
		depth = ((OMediaRenderTarget*)get_engine())->get_renderer()->get_video_engine()->get_current_video_mode()->depth;
            else return GL_RGBA;
											
		switch(depth)
		{
			case 16: return GL_RGB5_A1;
			default: return GL_RGBA;		
		}							
	}
	else if (pix_fmt==ompixfc_ResBestAlpha)
	{
           if (((OMediaRenderTarget*)get_engine())->get_renderer())        
		depth = ((OMediaRenderTarget*)get_engine())->get_renderer()->get_video_engine()->get_current_video_mode()->depth;
            else
            return GL_RGBA;
											
		switch(depth)
		{
			case 16: return GL_RGBA4;
			default: return GL_RGBA;
		}								
	}
	else if (pix_fmt&ompixfc_ARGB8888) return GL_RGBA;
	else if (pix_fmt&ompixfc_ARGB4444) return GL_RGBA4;
	else if (pix_fmt&ompixfc_RGB888) return GL_RGB;
	else if (pix_fmt&ompixfc_ARGB1555) return GL_RGB5_A1;
	else if (pix_fmt&ompixfc_RGB565) return GL_RGB5;
	else if (pix_fmt&ompixfc_RGB555) return GL_RGB5_A1;
	
	return GL_RGBA;
}

void OMediaGLCanvas::create_texture(bool new_texture, omt_CanvasSlaveKeyFlags key_flags)
{
	int					w,nw,h,nh;
	OMediaCanvas			*master_canv;	
	OMediaMipmapGenerator	*mm_gen;

	master_canv = (OMediaCanvas*)master;


	target->set_context();

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	nw = w = master_canv->get_width();
	nh = h = master_canv->get_height();

	target->check_texture_size(nw,nh);

	// port: HD texture pack (engine/sdl/OMediaHDPack.h): a canvas with a pack picture gets the picture as
	// its texture; the layout (exact / scaled / subdivided) and every coordinate stay those of the canvas.
	if (!new_texture && hd_scale)		// the game drew into it: its own pixels again
	{
		release_textures();
		new_texture = true;
	}
	if (new_texture && !master_canv->hd_key.empty())
	{
		GLint max_size;
		int k = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_size);
		OMediaCanvas *hd = omhd::load(master_canv, max_size, k);
		if (hd && !(w==nw && h==nh) && (key_flags&omcskf_SubDivided))
		{
			hd_src = hd;
			hd_scale = k;
			create_subimage(master_canv,w,h,new_texture,nw,nh);
			hd_src = NULL;
			delete hd;
			return;
		}
		if (hd)
		{
			int tw = hd->get_width(), th = hd->get_height();
			target->check_texture_size(tw,th);
			const double bytes = tw * (double)th * 4.0 * ((master_canv->get_filtering_min()<=omtfc_Linear) ? 1.0 : 4.0 / 3.0);
			if (omhd::add_bytes(master_canv, bytes))
			{
				OMediaCanvas *scaled = NULL;
				if (tw!=hd->get_width() || th!=hd->get_height())
				{
					scaled = new OMediaCanvas;
					scaled->create(tw,th,hd);
				}
				n_texture = 1;
				slave_key_long = (w==nw && h==nh) ? omcskf_Exact : omcskf_Scaled;
				glGenTextures(1,&texture_id);
				glBindTexture(GL_TEXTURE_2D, texture_id);
				upload_full(master_canv, scaled ? scaled : hd, internal_format);
				delete scaled;
				delete hd;
				hd_scale = k;
				hd_bytes = bytes;
				return;
			}
			delete hd;
		}
	}
	
	if (w==nw && h==nh)		// Exact size
	{
		n_texture = 1;
		slave_key_long = omcskf_Exact;

		master_canv->lock(omlf_Read);

		if (new_texture) glGenTextures(1,&texture_id);
		glBindTexture(GL_TEXTURE_2D, texture_id);
		
		if (master_canv->get_filtering_min()<=omtfc_Linear)
		{
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		
			if (new_texture)
			{
				glTexImage2D(GL_TEXTURE_2D,0,internal_format,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,master_canv->get_pixels());
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,master_canv->get_pixels());
			}
		}
		else
		{			
			mm_gen = new OMediaMipmapGenerator(master_canv);
			
			do
			{
				glPixelStorei(GL_UNPACK_ROW_LENGTH, mm_gen->full_width);		
				
				if (new_texture)
				{
					glTexImage2D(GL_TEXTURE_2D,mm_gen->level,internal_format,
												mm_gen->cur_width,
												mm_gen->cur_height,
												0,
												GL_RGBA,
												GL_UNSIGNED_BYTE,
												mm_gen->pixels);
				}
				else
				{
					glTexSubImage2D(GL_TEXTURE_2D,mm_gen->level,0,0,
									mm_gen->cur_width,
									mm_gen->cur_height,
									GL_RGBA,GL_UNSIGNED_BYTE,
									mm_gen->pixels);
				}
			}
			while(mm_gen->generate_next_mipmap());

			delete mm_gen;		
		}
	
		master_canv->unlock();
	}
	else if (key_flags&omcskf_SubDivided)	// Sub divised
	{
		create_subimage(master_canv,w,h,new_texture,nw,nh);	
	}
	else
	{
		slave_key_long = omcskf_Scaled;	
		n_texture = 1;
	
		OMediaCanvas	*canv;
		canv = new OMediaCanvas;
		canv->create(nw,nh,master_canv);

		canv->lock(omlf_Read);
		
		if (new_texture) glGenTextures(1,&texture_id);
		glBindTexture(GL_TEXTURE_2D, texture_id);

		if (master_canv->get_filtering_min()<=omtfc_Linear)
		{
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		
			if (new_texture)
			{
				glTexImage2D(GL_TEXTURE_2D,0,internal_format,nw,nh,0,GL_RGBA,GL_UNSIGNED_BYTE,canv->get_pixels());
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D,0,0,0,nw,nh,GL_RGBA,GL_UNSIGNED_BYTE,canv->get_pixels());
			}
		}
		else
		{
			mm_gen = new OMediaMipmapGenerator(canv);

			do
			{
				glPixelStorei(GL_UNPACK_ROW_LENGTH, mm_gen->full_width);
				
				if (new_texture)
				{
					glTexImage2D(GL_TEXTURE_2D,mm_gen->level,internal_format,
												mm_gen->cur_width,
												mm_gen->cur_height,
												0,
												GL_RGBA,
												GL_UNSIGNED_BYTE,
												mm_gen->pixels);
				}
				else
				{
					glTexSubImage2D(GL_TEXTURE_2D,mm_gen->level,0,0,
									mm_gen->cur_width,
									mm_gen->cur_height,
									GL_RGBA,GL_UNSIGNED_BYTE,
									mm_gen->pixels);
				}
			}
			while(mm_gen->generate_next_mipmap());

			delete mm_gen;				
		}
	
		canv->unlock();
		
		delete canv;
	}
}

void OMediaGLCanvas::master_modified(void)
{
	dirty = true;
}


void OMediaGLCanvas::check_texture_subsize(OMediaCanvas *master, int w, int h, int &nw, int &nh)
{
	GLint	max_size;

	if (master->get_2Dsubdivision_width()!=-1)  
	{
		max_size = master->get_2Dsubdivision_width();
		if (w>max_size) w = max_size;
	
		nw = getnearestpow(w);
		if (w!=nw) nw <<=1;
		
		if (nw>max_size) nw = max_size;
	}

	if (master->get_2Dsubdivision_height()!=-1)  
	{
		max_size = master->get_2Dsubdivision_height();
	
		if (h>max_size) h = max_size;
	
		nh = getnearestpow(h);
		
		if (h!=nh) nh <<=1;

		if (nh>max_size) nh = max_size;	
	}
}

void OMediaGLCanvas::create_subimage(OMediaCanvas *canv, int w, int h, bool new_texture, int nw, int nh)
{
	check_texture_subsize(canv, w, h, nw, nh);

	slave_key_long = omcskf_SubDivided;	
	
	subdiv_shift_w = getnearestpow_shift((nw>w)?nw>>1:nw);
	subdiv_shift_h = getnearestpow_shift((nh>h)?nh>>1:nh);

	subdiv_w = float(1<<subdiv_shift_w);
	subdiv_h = float(1<<subdiv_shift_h);

	subdiv_w_mask = (1<<subdiv_shift_w)-1;
	subdiv_h_mask = (1<<subdiv_shift_h)-1;

	n_textw = w>>subdiv_shift_w;	if (w&subdiv_w_mask) n_textw++;
	n_texth = h>>subdiv_shift_h;	if (h&subdiv_h_mask) n_texth++;

	n_texture = n_textw * n_texth;

	// port: HD texture pack: the tiles of the pack picture are 2^ks times larger (ks = 0 without one)
	OMediaCanvas	*src = canv, *hd_resampled = NULL;
	int				ks = 0;
	if (hd_src)
	{
		GLint	max_size;
		int		kp = 1;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_size);
		while (kp*2<=hd_scale) kp <<= 1;
		while (kp>1 && (((subdiv_w_mask+1)*kp)>max_size || ((subdiv_h_mask+1)*kp)>max_size)) kp >>= 1;
		const double bytes = double(n_textw<<subdiv_shift_w) * double(n_texth<<subdiv_shift_h) * kp * kp * 4.0 *
							 ((canv->get_filtering_min()<=omtfc_Linear) ? 1.0 : 4.0 / 3.0);
		if (kp>1 && omhd::add_bytes(canv, bytes))
		{
			if (kp!=hd_scale)
			{
				hd_resampled = new OMediaCanvas;
				hd_resampled->create(w*kp,h*kp,hd_src);
			}
			src = hd_resampled ? hd_resampled : hd_src;
			while ((1<<ks)<kp) ks++;
			hd_scale = kp;
			hd_bytes = bytes;
		}
		else hd_scale = 0;
	}

	if (new_texture)
	{
		texturegrid_id = new GLuint[n_texture];
		glGenTextures(n_texture,texturegrid_id);
	}

	GLuint			*tptr = texturegrid_id;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);	
	
	int			tw,th;
	omt_RGBAPixel	*pixels,*temp_buffer;
	
	int			last_th,
					last_tw;

	if ((w&subdiv_w_mask)!=0)
	{
		last_tw = getnearestpow(w&subdiv_w_mask);
		if (last_tw!=(w&subdiv_w_mask)) last_tw<<=1L;
		last_u = float(w&subdiv_w_mask)/float(last_tw);
	}
	else last_u = 0.9999f;

	if ((h&subdiv_h_mask)!=0)
	{
		last_th = getnearestpow(h&subdiv_h_mask);
		if (last_th!=(h&subdiv_h_mask)) last_th<<=1L;
		last_v = float(h&subdiv_h_mask)/float(last_th);
	}
	else last_v = 0.9999f;

	src->lock(omlf_Read);
	temp_buffer = new omt_RGBAPixel[((n_textw<<subdiv_shift_w)*(n_texth<<subdiv_shift_h))<<(2*ks)];
	OMediaMemTools::copy(src->get_pixels(),temp_buffer,((w*h)<<2L)<<(2*ks));

	for(int ty = 0; ty<n_texth; ty++)
	{
		if ((ty==n_texth-1 && (h&subdiv_h_mask)!=0))
		{
			th = last_th<<ks;			
		}
		else 
		{
			th = (subdiv_h_mask+1)<<ks;
		}
	
		for(int tx = 0; tx<n_textw; tx++,tptr++)
		{
			if (tx==n_textw-1 && (w&subdiv_w_mask)!=0)
			{
				tw = last_tw<<ks;
			}
			else				
			{
				tw = (subdiv_w_mask+1)<<ks;
			}

			if (canv->get_filtering_min()<=omtfc_Linear)
			{			
				glPixelStorei(GL_UNPACK_ROW_LENGTH, w<<ks);
				pixels = temp_buffer + ( (ty<<(subdiv_shift_h+ks)) * (w<<ks)) + (tx<<(subdiv_shift_w+ks));
				
				glBindTexture(GL_TEXTURE_2D, *tptr);
				if (new_texture)
				{
					glTexImage2D(GL_TEXTURE_2D,0,internal_format,tw,th,0, GL_RGBA,GL_UNSIGNED_BYTE, pixels);
				}
				else
				{
					glTexSubImage2D(GL_TEXTURE_2D,0,0,0,tw,th,GL_RGBA,GL_UNSIGNED_BYTE,pixels);			
				}
			}
			else
			{
				OMediaMipmapGenerator	*mm_gen;
				mm_gen = new OMediaMipmapGenerator(src,tx<<(subdiv_shift_w+ks),ty<<(subdiv_shift_h+ks),tw,th);

				glBindTexture(GL_TEXTURE_2D, *tptr);

				do
				{
					glPixelStorei(GL_UNPACK_ROW_LENGTH, mm_gen->full_width);
					
					if (new_texture)
					{
						glTexImage2D(GL_TEXTURE_2D,mm_gen->level,internal_format,
													mm_gen->cur_width,
													mm_gen->cur_height,
													0,
													GL_RGBA,
													GL_UNSIGNED_BYTE,
													mm_gen->pixels);
					}
					else
					{
						glTexSubImage2D(GL_TEXTURE_2D,mm_gen->level,0,0,
										mm_gen->cur_width,
										mm_gen->cur_height,
										GL_RGBA,GL_UNSIGNED_BYTE,
										mm_gen->pixels);
					}
				}
				while(mm_gen->generate_next_mipmap());
			
				delete mm_gen;
			}
		}
	}

	src->unlock();
	delete temp_buffer;
	delete hd_resampled;
	if (ks) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}




#endif

