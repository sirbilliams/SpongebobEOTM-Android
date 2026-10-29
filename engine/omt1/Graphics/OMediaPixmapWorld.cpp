/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaPixmapWorld.h"
#include "OMediaGLBackend.h"
#include <string.h>

// 0x103965d0
OMediaPixmapWorld::OMediaPixmapWorld()
{
	memset(&pixels, 0, sizeof(pixels));
	retarget = NULL;
	init_retarget();
}

// 0x103966a0
OMediaPixmapWorld::~OMediaPixmapWorld()
{
	OMediaPixmapWorld::purge();
	omgl_forget(&pixels);			// port: its GL texture copy
}

// 0x10396720. OMT.dll: deletes the DIB section and its DC.
void OMediaPixmapWorld::purge(void)
{
	delete [] pixels.pix;			// port: the DIB section
	pixels.pix = NULL;
	pixels.width = pixels.height = 0;
	pixels.version++;
	pixel_format = omcpf_Unknown;
}

// 0x10396780. Creates a DIB section of the requested depth (0: the screen's; 24 -> 32; 2 -> 4)
// with the world's or the screen's palette, then draws 'model' into it, through a temporary
// OMediaPixBlitterWorld when the formats differ.
// port: always a 32-bit buffer, cleared to black; no palettes.
void OMediaPixmapWorld::create(short width, short height, OMediaGfxWorld *model, OMediaRGBColor *, short)
{
	if (model)
	{
		width = (short)model->getwidth();
		height = (short)model->getheight();
	}
	purge();
	if (width > 0 && height > 0)
	{
		pixels.pix = new unsigned int[width * height];
		for (int i = 0; i < width * height; i++) pixels.pix[i] = 0xFF000000;
		pixels.width = width;
		pixels.height = height;
	}
	pixels.version++;
	pixel_format = omcpf_RGB32;
	if (model)
	{
		OMediaRGBColor tc;
		if (model->get_transparent_color(&tc)) fill(&tc, NULL);
		model->draw_full(this, 0, 0, NULL);
	}
}

OMediaRetarget *OMediaPixmapWorld::getdrawretarget(void) { return retarget; }	// 0x10399670
unsigned long OMediaPixmapWorld::getattributes(void) { return 0x2ad; }			// 0x10396d30
OMediaGfxWorld *OMediaPixmapWorld::builder(void) { return new OMediaPixmapWorld; }	// 0x10396d40
void OMediaPixmapWorld::init_retarget(void) { retarget = NULL; }				// 0x10396cc0 port: no OMediaWinRtgPixmap


// 0x10083110
OMediaPixBlitterWorld::OMediaPixBlitterWorld()
{
	gdi_draw = false;
	transparent_packed = 0xFFFFFFFF;
	transparent_rgb.set(0, 0, 0);
}

OMediaPixBlitterWorld::~OMediaPixBlitterWorld() {}		// 0x100831e0

// 0x10083210. Creates the pixmap, then packs the transparent colour in the world's format.
void OMediaPixBlitterWorld::create(short width, short height, OMediaGfxWorld *model, OMediaRGBColor *transparent, short depth)
{
	OMediaPixmapWorld::create(width, height, model, transparent, depth);
	if (!transparent) transparent_packed = 0xFFFFFFFF;
	else
	{
		transparent_rgb = *transparent;
		transparent_packed = transparent->getrgb32bits();	// port: OMT.dll packs it through a drawport (screen format)
	}
}

// 0x100834d0
void OMediaPixBlitterWorld::draw_full(OMediaGfxWorld *dest, long x, long y, OMediaMaskWorld *mask)
{
	if (gdi_draw) OMediaGfxWorld::draw_full(dest, x, y, mask);
	// fcn.1005f200: OMT's software blitter (transparent colour, alpha channel)
	OMediaRect r(0, 0, pixels.width, pixels.height);
	lock();
	dest->lock();
	port_blit(this, dest, r, x - dest->get_origin_x(), y - dest->get_origin_y(), r.get_width(), r.get_height(), has_transparency());
	unlock();
	dest->unlock();
}

// 0x10083520
void OMediaPixBlitterWorld::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, long x, long y, OMediaMaskWorld *mask)
{
	if (gdi_draw) OMediaGfxWorld::draw(dest, src_rect, x, y, NULL);
	// fcn.100605b0: OMT's software blitter
	lock();
	dest->lock();
	port_blit(this, dest, *src_rect, x - dest->get_origin_x(), y - dest->get_origin_y(),
			  src_rect->get_width(), src_rect->get_height(), has_transparency());
	unlock();
	dest->unlock();
}

// 0x10083580. Same size and no GDI: the blitter; otherwise the GDI's StretchBlt.
void OMediaPixBlitterWorld::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, OMediaRect *dest_rect)
{
	if (!gdi_draw && src_rect->get_width() == dest_rect->get_width() && src_rect->get_height() == dest_rect->get_height())
	{
		draw(dest, src_rect, dest_rect->left, dest_rect->top, NULL);
		return;
	}
	OMediaGfxWorld::draw(dest, src_rect, dest_rect);
}

// 0x10083660. Fills in the world's pixel format with OMT's span filler; the whole world when
// 'rect' is NULL. Falls back to OMediaGfxWorld::fill when the world has no pixels.
void OMediaPixBlitterWorld::fill(OMediaRGBColor *color, OMediaRect *rect)
{
	pixlock();
	if (!pixels.pix)
	{
		pixunlock();
		OMediaGfxWorld::fill(color, rect);
		return;
	}
	OMediaRect r;
	if (rect) r = *rect;
	else r.set(0, 0, 0, 0);
	if (!rect) r.set(0, 0, pixels.width, pixels.height);
	else r.offset(-origin_x, -origin_y);
	// port: a NULL colour fills with 0 in OMT.dll's packed format
	port_fill(this, r, color ? (0xFF000000 | color->getrgb32bits()) : 0xFF000000);
	pixunlock();
}

unsigned long OMediaPixBlitterWorld::getattributes(void) { return 0x22f; }				// 0x100835f0
OMediaGfxWorld *OMediaPixBlitterWorld::builder(void) { return new OMediaPixBlitterWorld; }	// 0x10083600

// 0x100839b0
bool OMediaPixBlitterWorld::getpixel(short x, short y, OMediaRGBColor *rgb, bool *transparent, unsigned long *packed)
{
	unsigned long p;
	if (OMediaGfxWorld::getpixel(x, y, rgb, NULL, &p))
	{
		if (transparent) *transparent = !(pixels.pix[y * pixels.width + x] >> 24);	// port: OMT.dll compares p with the packed key
		if (packed) *packed = p;
		return true;
	}
	return false;
}

// 0x100834a0
bool OMediaPixBlitterWorld::get_transparent_color(OMediaRGBColor *rgb)
{
	if (transparent_packed == 0xFFFFFFFF) return false;
	*rgb = transparent_rgb;
	return true;
}
