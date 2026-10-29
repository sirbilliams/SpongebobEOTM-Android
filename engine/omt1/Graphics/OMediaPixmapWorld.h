/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaPixmapWorld: a memory world (OMT.dll: a GDI DIB section, OMediaWinRtgPixmap retarget).
    OMediaPixBlitterWorld: a pixmap world with a transparent colour and OMT's software blitters;
    every 2D canvas of the game's databases is one (stream flags 0x22d).                        */
#pragma once
#ifndef OMEDIA1_PixmapWorld_H
#define OMEDIA1_PixmapWorld_H

#include "OMediaGfxWorld.h"

class OMediaPixmapWorld : public OMediaGfxWorld
{
	public:

	omtshared OMediaPixmapWorld();					// 0x103965d0
	omtshared virtual ~OMediaPixmapWorld();			// 0x103966a0

	omtshared virtual void create(short width, short height, OMediaGfxWorld *model = NULL,
								  OMediaRGBColor *transparent = NULL, short depth = 0);	// 0x10396780
	omtshared virtual void purge(void);										// 0x10396720
	omtshared virtual OMediaRetarget *getdrawretarget(void);					// 0x10399670
	omtshared virtual unsigned long getattributes(void);						// 0x10396d30

	omtshared static OMediaGfxWorld *builder(void);							// 0x10396d40

	virtual OMediaGfxPixels *port_pixels(void) {return &pixels;}

	protected:

	omtshared virtual void init_retarget(void);								// 0x10396cc0  vt+0xb0

	OMediaRetarget		*retarget;		// +0x44 (OMediaWinRtgPixmap)
	OMediaGfxPixels		pixels;			// port: the DIB section
};

class OMediaPixBlitterWorld : public OMediaPixmapWorld
{
	public:

	omtshared OMediaPixBlitterWorld();				// 0x10083110
	omtshared virtual ~OMediaPixBlitterWorld();		// 0x100831e0

	omtshared virtual void create(short width, short height, OMediaGfxWorld *model = NULL,
								  OMediaRGBColor *transparent = NULL, short depth = 0);	// 0x10083210
	omtshared virtual void draw_full(OMediaGfxWorld *dest, long x, long y, OMediaMaskWorld *mask = NULL);	// 0x100834d0
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, OMediaRect *dest_rect);		// 0x10083580
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, long x, long y,
								OMediaMaskWorld *mask = NULL);										// 0x10083520
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *dest_rect) {OMediaGfxWorld::draw(dest, dest_rect);}
	omtshared virtual void fill(OMediaRGBColor *color, OMediaRect *rect = NULL);							// 0x10083660
	omtshared virtual unsigned long getattributes(void);						// 0x100835f0
	omtshared virtual bool getpixel(short x, short y, OMediaRGBColor *rgb, bool *transparent = NULL,
									unsigned long *packed = NULL);			// 0x100839b0
	omtshared virtual bool get_transparent_color(OMediaRGBColor *rgb);		// 0x100834a0

	omtshared static OMediaGfxWorld *builder(void);							// 0x10083600

	inline bool has_transparency(void) const {return transparent_packed != 0xFFFFFFFF;}

	protected:

	bool				gdi_draw;				// +0x48  draw through the GDI first (OMediaGfxWorld::draw)
	unsigned int		transparent_packed;		// +0x4c  -1: no transparent colour
	OMediaRGBColor		transparent_rgb;		// +0x50
};

#endif
