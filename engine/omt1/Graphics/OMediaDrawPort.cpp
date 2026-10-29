/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaDrawPort.h"
#include <SDL.h>
#include <math.h>

OMediaDrawPort *OMediaDrawPort::current_lockedport = NULL;

// 0x1038fe60 (+ init_retarget: the GDI pen and brush; port: pixels)
OMediaDrawPort::OMediaDrawPort()
{
	world = NULL;
	locked = false;
	pen.set(0, 0, 0);
	pen_x = pen_y = 0;
	off_x = off_y = 0;
	origin_x = origin_y = 0;
	argb = 0xFF000000 | pen.getrgb32bits();
}

// 0x1038ff30
OMediaDrawPort::~OMediaDrawPort()
{
	if (current_lockedport == this) current_lockedport = NULL;
}

// 0x10390050: draw into the direct world (the surface behind a double canvas); the pen offset is
// kept as 'pen origin - world origin'.
void OMediaDrawPort::setgfxworld(OMediaGfxWorld *w)
{
	OMediaGfxWorld *direct = w->get_direct_gfxworld();
	world = direct ? direct : w;
	long old_x = origin_x, old_y = origin_y;
	origin_x = world->get_origin_x();
	origin_y = world->get_origin_y();
	setpenorigin(off_x + old_x, off_y + old_y);
}

// 0x103900a0. port: no GetDC / temporary 16 bit copy.
void OMediaDrawPort::lock(void)
{
	if (!world) return;
	current_lockedport = this;
	locked = true;
	world->dportlock();
}

// 0x10390280
void OMediaDrawPort::unlock(void)
{
	if (!locked) return;
	locked = false;
	if (world) world->dportunlock();
	if (current_lockedport == this) current_lockedport = NULL;
}

// 0x10390440. GDI: a new pen and brush of this colour.
void OMediaDrawPort::setpencolor(unsigned short r, unsigned short g, unsigned short b)
{
	pen.set(r, g, b);
	argb = 0xFF000000 | pen.getrgb32bits();
}

// 0x10390570
void OMediaDrawPort::placepen(long x, long y) { pen_x = x; pen_y = y; }

// 0x103905b0
void OMediaDrawPort::setpenorigin(long x, long y)
{
	off_x = x - origin_x;
	off_y = y - origin_y;
}

void OMediaDrawPort::pixel(long x, long y)
{
	OMediaRect r(x + off_x, y + off_y, x + off_x + 1, y + off_y + 1);
	OMediaGfxWorld::port_fill(world, r, argb);
}

void OMediaDrawPort::hline(long x0, long x1, long y)		// x0..x1 inclusive
{
	if (x1 < x0) { long t = x0; x0 = x1; x1 = t; }
	OMediaRect r(x0 + off_x, y + off_y, x1 + off_x + 1, y + off_y + 1);
	OMediaGfxWorld::port_fill(world, r, argb);
}

void OMediaDrawPort::vline(long x, long y0, long y1)
{
	if (y1 < y0) { long t = y0; y0 = y1; y1 = t; }
	OMediaRect r(x + off_x, y0 + off_y, x + off_x + 1, y1 + off_y + 1);
	OMediaGfxWorld::port_fill(world, r, argb);
}

// Bresenham, both ends included (GDI LineTo leaves out the last pixel; OMT.dll adds it back with SetPixel)
void OMediaDrawPort::draw_line(long x0, long y0, long x1, long y1)
{
	if (y0 == y1) { hline(x0, x1, y0); return; }
	if (x0 == x1) { vline(x0, y0, y1); return; }
	long dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
	long sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
	long err = dx - dy;
	for (;;)
	{
		pixel(x0, y0);
		if (x0 == x1 && y0 == y1) break;
		long e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx) { err += dx; y0 += sy; }
	}
}

// 0x10390600. GDI Rectangle with the pen and the brush of one colour: [l,r) x [t,b) filled.
void OMediaDrawPort::paintrect(long l, long t, long r, long b)
{
	if (current_lockedport != this || !world) return;
	OMediaRect rc(l + off_x, t + off_y, r + off_x, b + off_y);
	OMediaGfxWorld::port_fill(world, rc, argb);
}

// 0x10390650. Rectangle with the null brush: the outline only.
void OMediaDrawPort::framerect(long l, long t, long r, long b)
{
	if (current_lockedport != this || !world) return;
	if (r <= l || b <= t) return;
	hline(l, r - 1, t);
	hline(l, r - 1, b - 1);
	vline(l, t, b - 1);
	vline(r - 1, t, b - 1);
}

void OMediaDrawPort::oval(long l, long t, long r, long b, bool filled)
{
	if (r <= l || b <= t) return;
	double cx = (l + r) / 2.0, cy = (t + b) / 2.0, rx = (r - l) / 2.0, ry = (b - t) / 2.0;
	long prev_l = 0, prev_r = 0;
	for (long y = t; y < b; y++)
	{
		double dy = (y + 0.5 - cy) / ry;
		double w = 1.0 - dy * dy;
		if (w < 0) w = 0;
		double half = rx * sqrt(w);
		long xl = (long)floor(cx - half + 0.5), xr = (long)floor(cx + half + 0.5) - 1;
		if (xr < xl) xr = xl;
		if (filled) hline(xl, xr, y);
		else
		{
			if (y == t || y == b - 1) hline(xl, xr, y);
			else
			{
				// join to the previous row's span so the outline has no gaps
				long a = xl < prev_l ? xl : prev_l, c = xl > prev_l ? xl : prev_l;
				hline(a, c, y); pixel(xl, y);
				a = xr < prev_r ? xr : prev_r; c = xr > prev_r ? xr : prev_r;
				hline(a, c, y); pixel(xr, y);
			}
		}
		prev_l = xl;
		prev_r = xr;
	}
}

// 0x103906c0
void OMediaDrawPort::paintoval(long l, long t, long r, long b)
{
	if (current_lockedport == this && world) oval(l, t, r, b, true);
}

// 0x10390710
void OMediaDrawPort::frameoval(long l, long t, long r, long b)
{
	if (current_lockedport == this && world) oval(l, t, r, b, false);
}

// 0x10390780: line from the pen to (x,y), the pen moves there
void OMediaDrawPort::lineto(long x, long y)
{
	if (current_lockedport != this || !world) return;
	long px = pen_x, py = pen_y;
	pen_x = x;
	pen_y = y;
	draw_line(px, py, x, y);
}

// 0x103907f0: the same line, without moving the pen's remembered position
void OMediaDrawPort::lineto_staticpen(long x, long y)
{
	if (current_lockedport != this || !world) return;
	draw_line(pen_x, pen_y, x, y);
}

// 0x10390870 (GDI TextOut). port: OMT.dll's menus draw text with OMediaQuickText, not with this.
void OMediaDrawPort::drawstring(string text, long x, long y)
{
	if (current_lockedport != this) return;
	SDL_Log("TODO(OMT 0x10390870) OMediaDrawPort::drawstring '%s' at %ld,%ld", text.c_str(), x, y);
}

// 0x10390910
void OMediaDrawPort::readpixel(long x, long y, OMediaRGBColor *c)
{
	if (current_lockedport != this || !world) return;
	world->getpixel((short)(x + off_x), (short)(y + off_y), c);
}

// 0x10390950
void OMediaDrawPort::writepixel(long x, long y, const OMediaRGBColor *c)
{
	if (current_lockedport != this || !world) return;
	OMediaRect r(x + off_x, y + off_y, x + off_x + 1, y + off_y + 1);
	OMediaGfxWorld::port_fill(world, r, 0xFF000000 | c->getrgb32bits());
}

// 0x100575a0: the frame, then the inside in 'fill' (one pixel in), the pen back to black
void OMediaDrawPort::paintrect3D(long l, long t, long r, long b, bool invert,
								 OMediaRGBColor *c1, OMediaRGBColor *c2, OMediaRGBColor *fill)
{
	framerect3D(l, t, r, b, invert, c1, c2);
	setpencolor(fill->red, fill->green, fill->blue);
	paintrect(l + 1, t + 1, r - 1, b - 1);
	setpencolor(0, 0, 0);
}

// 0x10057610: left and top edges in one colour, right and bottom in the other (swapped by 'invert')
void OMediaDrawPort::framerect3D(long l, long t, long r, long b, bool invert, OMediaRGBColor *c1, OMediaRGBColor *c2)
{
	OMediaRGBColor *light = c1, *dark = c2;
	if (invert) { light = c2; dark = c1; }
	long rr = r - 1, bb = b - 1;
	setpencolor(light->red, light->green, light->blue);
	placepen(l, bb);
	lineto(l, t);
	lineto(rr, t);
	setpencolor(dark->red, dark->green, dark->blue);
	placepen(rr, t);
	lineto(rr, bb);
	lineto(l, bb);
	setpencolor(0, 0, 0);
}
