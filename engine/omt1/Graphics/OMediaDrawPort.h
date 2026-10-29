/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaDrawPort: GDI-style drawing (pen, rectangles, ovals, lines, pixels) on a gfx world.
    OMT.dll got the world's device context (dportlock) and drew with GDI pens and brushes; here
    the same calls draw pixels through the gfx world back end (port).
    Layout in OMT.dll: +0x04 world, +0x0c world of a temporary 16 bit copy, +0x24 pen colour (3 x
    16 bits), +0x2c pen position, +0x34 pen offset (pen origin - world origin), +0x3c world origin.
    Drawing only works between lock() and unlock() (static current_lockedport).              */
#pragma once
#ifndef OMEDIA1_DrawPort_H
#define OMEDIA1_DrawPort_H

#include "OMediaGfxWorld.h"
#include "OMediaRGBColor.h"
#include <string>

class OMediaDrawPort
{
	public:
	omtshared OMediaDrawPort();							// 0x1038fe60
	omtshared virtual ~OMediaDrawPort();					// 0x1038ff30

	omtshared virtual void setgfxworld(OMediaGfxWorld *w);			// 0x10390050  vt+0x04
	omtshared virtual void lock(void);								// 0x103900a0  vt+0x08
	omtshared virtual void unlock(void);							// 0x10390280  vt+0x0c
	omtshared virtual void setpencolor(unsigned short r, unsigned short g, unsigned short b);	// 0x10390440  vt+0x10
	omtshared virtual void placepen(long x, long y);				// 0x10390570  vt+0x14
	omtshared virtual void setpenorigin(long x, long y);			// 0x103905b0  vt+0x18
	omtshared virtual void paintrect(long l, long t, long r, long b);	// 0x10390600  vt+0x1c
	omtshared virtual void framerect(long l, long t, long r, long b);	// 0x10390650  vt+0x20
	omtshared virtual void paintoval(long l, long t, long r, long b);	// 0x103906c0  vt+0x24
	omtshared virtual void frameoval(long l, long t, long r, long b);	// 0x10390710  vt+0x28
	omtshared virtual void lineto(long x, long y);					// 0x10390780  vt+0x2c
	omtshared virtual void lineto_staticpen(long x, long y);		// 0x103907f0  vt+0x30
	omtshared virtual void drawstring(string text, long x, long y);	// 0x10390870  vt+0x34
	omtshared virtual void readpixel(long x, long y, OMediaRGBColor *c);			// 0x10390910  vt+0x38
	omtshared virtual void writepixel(long x, long y, const OMediaRGBColor *c);	// 0x10390950  vt+0x3c
	// A bevelled frame: the top and left edges in c1, the bottom and right ones in c2 (swapped when 'invert')
	omtshared virtual void paintrect3D(long l, long t, long r, long b, bool invert,
									   OMediaRGBColor *c1, OMediaRGBColor *c2, OMediaRGBColor *fill);	// 0x100575a0  vt+0x40
	omtshared virtual void framerect3D(long l, long t, long r, long b, bool invert,
									   OMediaRGBColor *c1, OMediaRGBColor *c2);	// 0x10057610  vt+0x44

	inline OMediaGfxWorld *getgfxworld(void) {return world;}			// fcn.10053b70

	omtshared static OMediaDrawPort *current_lockedport;		// 0x10420d7c

	protected:
	void pixel(long x, long y);				// port: the pen colour at one pixel (offset applied)
	void hline(long x0, long x1, long y);
	void vline(long x, long y0, long y1);
	void draw_line(long x0, long y0, long x1, long y1);
	void oval(long l, long t, long r, long b, bool filled);

	OMediaGfxWorld	*world;				// +0x04
	bool			locked;				// port
	OMediaRGBColor	pen;				// +0x24
	long			pen_x, pen_y;		// +0x2c
	long			off_x, off_y;		// +0x34
	long			origin_x, origin_y;	// +0x3c
	unsigned int	argb;				// port: pen colour as pixels
};

#endif
