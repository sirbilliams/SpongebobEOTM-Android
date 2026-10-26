/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    port: DirectDraw surfaces as GL framebuffers (see the header).                              */
#include "OMediaDXSurfaceWorld.h"
#include <string.h>

OMediaDXSurfaceWorld::OMediaDXSurfaceWorld(bool p)
{
	primary = p;
	page_flipping = false;
	has_transparent = false;
	transparent_rgb.set(0, 0, 0);
	pix_lock_count = 0;
	memset(&surface, 0, sizeof(surface));
	memset(&locked_pixels, 0, sizeof(locked_pixels));
	pixel_format = omcpf_RGB32;
}

OMediaDXSurfaceWorld::~OMediaDXSurfaceWorld()
{
	OMediaDXSurfaceWorld::purge();
}

// 0x1005d400. port: a framebuffer of that size (the primary one is the screen's back buffer)
void OMediaDXSurfaceWorld::create(short width, short height, OMediaGfxWorld *model, OMediaRGBColor *transparent, short)
{
	if (model)
	{
		width = (short)model->getwidth();
		height = (short)model->getheight();
	}
	purge();
	omgl_surface_create(&surface, width, height, primary);
	has_transparent = transparent != NULL;
	if (transparent) transparent_rgb = *transparent;
	if (model) model->draw_full(this, 0, 0, NULL);
}

// 0x1005d300
void OMediaDXSurfaceWorld::purge(void)
{
	omgl_surface_destroy(&surface);
	delete [] locked_pixels.pix;
	memset(&locked_pixels, 0, sizeof(locked_pixels));
	pix_lock_count = 0;
}

// 0x1005e150 / 0x1005e390 / 0x1005e560: DirectDraw Blt (with the source colour key when set)
void OMediaDXSurfaceWorld::draw_full(OMediaGfxWorld *dest, long x, long y, OMediaMaskWorld *)
{
	OMediaRect r(0, 0, surface.width, surface.height);
	port_blit(this, dest, r, x - dest->get_origin_x(), y - dest->get_origin_y(), r.get_width(), r.get_height(), has_transparent);
}

void OMediaDXSurfaceWorld::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, long x, long y, OMediaMaskWorld *)
{
	port_blit(this, dest, *src_rect, x - dest->get_origin_x(), y - dest->get_origin_y(),
			  src_rect->get_width(), src_rect->get_height(), has_transparent);
}

void OMediaDXSurfaceWorld::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, OMediaRect *dest_rect)
{
	port_blit(this, dest, *src_rect, dest_rect->left - dest->get_origin_x(), dest_rect->top - dest->get_origin_y(),
			  dest_rect->get_width(), dest_rect->get_height(), has_transparent);
}

// 0x1005e820: DirectDraw colour fill
void OMediaDXSurfaceWorld::fill(OMediaRGBColor *color, OMediaRect *rect)
{
	OMediaRect r;
	if (rect) { r = *rect; r.offset(-origin_x, -origin_y); }
	else r.set(0, 0, surface.width, surface.height);
	port_fill(this, r, 0xFF000000 | (color ? color->getrgb32bits() : 0));
}

long OMediaDXSurfaceWorld::getwidth(void) { return surface.width; }		// 0x1005d990
long OMediaDXSurfaceWorld::getheight(void) { return surface.height; }		// 0x1005d9a0
short OMediaDXSurfaceWorld::getdepth(void) { return 32; }					// 0x1005d9b0 port: always 32-bit
short OMediaDXSurfaceWorld::support_depth(short) { return 32; }			// 0x1005d3f0 port

// 0x1005da10 (DirectDraw Lock). port: read the framebuffer back into a CPU copy
void OMediaDXSurfaceWorld::pixlock(unsigned short)
{
	if (pix_lock_count++ == 0 && surface.fbo)
	{
		locked_pixels.width = surface.width;
		locked_pixels.height = surface.height;
		locked_pixels.pix = new unsigned int[surface.width * surface.height];
		omgl_surface_read(&surface, locked_pixels.pix);
		locked_pixels.version = 0;
	}
	lock();
}

// 0x1005dc10 (DirectDraw Unlock). port: write the CPU copy back
void OMediaDXSurfaceWorld::pixunlock(void)
{
	unlock();
	if (pix_lock_count > 0 && --pix_lock_count == 0 && locked_pixels.pix)
	{
		omgl_surface_write(&surface, locked_pixels.pix);	// the caller may have written anywhere
		omgl_forget(&locked_pixels);
		delete [] locked_pixels.pix;
		memset(&locked_pixels, 0, sizeof(locked_pixels));
	}
}

void OMediaDXSurfaceWorld::dportlock(void) { lock(); }		// 0x1005dce0 (GetDC) port: no GDI on surfaces
void OMediaDXSurfaceWorld::dportunlock(void) { unlock(); }	// 0x1005dec0
unsigned long OMediaDXSurfaceWorld::getattributes(void) { return 0x604e; }	// 0x1005e770

// 0x1005d9c0: the locked surface memory
void OMediaDXSurfaceWorld::getpixmaplowleveldata(char **pixels, short *width, short *height, short *depth, long *rowbytes)
{
	OMediaGfxWorld::getpixmaplowleveldata(pixels, width, height, depth, rowbytes);
}

// 0x1005edd0
bool OMediaDXSurfaceWorld::getpixel(short x, short y, OMediaRGBColor *rgb, bool *transparent, unsigned long *packed)
{
	return OMediaGfxWorld::getpixel(x, y, rgb, transparent, packed);
}

// 0x1005d3c0
bool OMediaDXSurfaceWorld::get_transparent_color(OMediaRGBColor *rgb)
{
	if (!has_transparent) return false;
	*rgb = transparent_rgb;
	return true;
}

bool OMediaDXSurfaceWorld::support_page_flipping(void) { return page_flipping; }		// 0x1005eae0
void OMediaDXSurfaceWorld::set_page_flipping(bool pf) { page_flipping = pf; }		// 0x1005eac0

// 0x1005eb00 (DirectDraw Flip). port: present the back buffer
void OMediaDXSurfaceWorld::page_flip(bool)
{
	if (primary) omgl_present(&surface);
}

bool OMediaDXSurfaceWorld::is_page_flip_done(void) { return true; }		// 0x1005ebc0 (GetFlipStatus)
bool OMediaDXSurfaceWorld::back_page_lost(void) { return false; }			// 0x1005ec40 port: GL surfaces are not lost

// 0x1005ec50: copies the refresh list's rectangles from the back buffer to the front.
// port: the whole back buffer is presented (the window shows a scaled copy of it)
void OMediaDXSurfaceWorld::back_to_front_copy(OMediaRefreshList *)
{
	if (primary) omgl_present(&surface);
}

bool OMediaDXSurfaceWorld::is_video_memory(void) { return true; }					// 0x1005d3b0
OMediaGfxWorld *OMediaDXSurfaceWorld::get_direct_gfxworld(void) { return this; }	// 0x1005ec30
bool OMediaDXSurfaceWorld::quiet_dport(void) { return false; }						// 0x1005edc0
unsigned long OMediaDXSurfaceWorld::get_approximative_size(void) { return 0xa4 + surface.width * surface.height * 4; }	// 0x1005e7e0
OMediaGfxWorld *OMediaDXSurfaceWorld::builder(void) { return new OMediaDXSurfaceWorld(false); }
