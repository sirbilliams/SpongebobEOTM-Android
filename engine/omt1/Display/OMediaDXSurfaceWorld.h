/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaDXSurfaceWorld: a DirectDraw surface. The primary one (the screen) is made by
    OMediaDXScreenDevice::get_directscreen_world; others by create_offscreen_gfxworld.
    port: a GL framebuffer (OMediaGLBackend). The primary surface keeps its pixels between frames
    like a DirectDraw back buffer; page_flip and back_to_front_copy present it in the window.
    pixlock gives a CPU copy (read back, written back on pixunlock).                           */
#pragma once
#ifndef OMEDIA1_DXSurfaceWorld_H
#define OMEDIA1_DXSurfaceWorld_H

#include "OMediaGfxWorld.h"
#include "OMediaGLBackend.h"

class OMedia3DRasterizerDesc;

class OMediaDXSurfaceWorld : public OMediaGfxWorld
{
	public:

	omtshared OMediaDXSurfaceWorld(bool primary = false);		// 0x1005ce80 (??0OMediaDXSurfaceWorld@@QAE@_N@Z)
	omtshared virtual ~OMediaDXSurfaceWorld();

	omtshared virtual void create(short width, short height, OMediaGfxWorld *model = NULL,
								  OMediaRGBColor *transparent = NULL, short depth = 0);	// 0x1005d400
	omtshared virtual void purge(void);											// 0x1005d300
	omtshared virtual void draw_full(OMediaGfxWorld *dest, long x, long y, OMediaMaskWorld *mask = NULL);	// 0x1005e150
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *dest_rect) {OMediaGfxWorld::draw(dest, dest_rect);}
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, OMediaRect *dest_rect);		// 0x1005e560
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, long x, long y,
								OMediaMaskWorld *mask = NULL);										// 0x1005e390
	omtshared virtual void fill(OMediaRGBColor *color, OMediaRect *rect = NULL);							// 0x1005e820
	omtshared virtual long getwidth(void);				// 0x1005d990
	omtshared virtual long getheight(void);				// 0x1005d9a0
	omtshared virtual short getdepth(void);				// 0x1005d9b0
	omtshared virtual short support_depth(short depth);	// 0x1005d3f0
	omtshared virtual void pixlock(unsigned short flags = 0);	// 0x1005da10
	omtshared virtual void pixunlock(void);				// 0x1005dc10
	omtshared virtual void dportlock(void);				// 0x1005dce0
	omtshared virtual void dportunlock(void);			// 0x1005dec0
	omtshared virtual unsigned long getattributes(void);	// 0x1005e770
	omtshared virtual void getpixmaplowleveldata(char **pixels, short *width, short *height,
												 short *depth, long *rowbytes);	// 0x1005d9c0
	omtshared virtual bool getpixel(short x, short y, OMediaRGBColor *rgb, bool *transparent = NULL,
									unsigned long *packed = NULL);			// 0x1005edd0
	omtshared virtual bool get_transparent_color(OMediaRGBColor *rgb);		// 0x1005d3c0
	omtshared virtual bool support_page_flipping(void);					// 0x1005eae0
	omtshared virtual void page_flip(bool wait);							// 0x1005eb00
	omtshared virtual bool is_page_flip_done(void);						// 0x1005ebc0
	omtshared virtual bool back_page_lost(void);							// 0x1005ec40
	omtshared virtual void back_to_front_copy(OMediaRefreshList *rl);		// 0x1005ec50
	omtshared virtual bool is_video_memory(void);							// 0x1005d3b0
	omtshared virtual OMediaGfxWorld *get_direct_gfxworld(void);			// 0x1005ec30
	omtshared virtual bool quiet_dport(void);								// 0x1005edc0
	omtshared virtual void set_page_flipping(bool pf);						// 0x1005eac0  vt+0xb0
	omtshared virtual unsigned long get_approximative_size(void);			// 0x1005e7e0  vt+0xb4

	omtshared static OMediaGfxWorld *builder(void);

	virtual OMediaGfxPixels *port_pixels(void) {return locked_pixels.pix ? &locked_pixels : NULL;}
	virtual OMediaGLSurface *port_surface(void) {return &surface;}

	inline bool is_primary(void) const {return primary;}

	protected:

	bool				primary;			// the screen's surface
	bool				page_flipping;		// set_page_flipping
	bool				has_transparent;
	OMediaRGBColor		transparent_rgb;
	int					pix_lock_count;
	OMediaGLSurface		surface;			// port: the DirectDraw surface
	OMediaGfxPixels		locked_pixels;		// port: the CPU copy while pixlocked
};

#endif
