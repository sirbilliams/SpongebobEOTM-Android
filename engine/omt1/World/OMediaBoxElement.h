/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMediaBoxElement: a coloured box, flat or bevelled (3D), filled or a frame.
    Layout in OMT.dll: +0xc8 width, +0xca height (0x20), +0xcc/+0xd2/+0xd8 colours (white, grey
    0x6666, light grey 0xaaaa), +0xde filled (1), +0xdf bevelled (0), +0xe0 raised/sunken.     */
#pragma once
#ifndef OMEDIA1_BoxElement_H
#define OMEDIA1_BoxElement_H

#include "OMediaUndiv2DElement.h"
#include "OMediaRGBColor.h"

class OMediaBoxElement : public OMediaUndiv2DElement
{
	public:
	omtshared OMediaBoxElement();						// 0x1003ad60
	omtshared virtual ~OMediaBoxElement();				// 0x1003ae20

	omtshared virtual void getsize(long &w, long &h);	// 0x1003ae40  vt+0x5c

	inline void set_size(short w, short h) {width = w; height = h; moved = true;}
	inline void set_light_color(const OMediaRGBColor &c) {light = c; moved = true;}	// +0xcc
	inline void set_dark_color(const OMediaRGBColor &c) {dark = c; moved = true;}		// +0xd2
	inline void set_color(const OMediaRGBColor &c) {color = c; moved = true;}			// +0xd8 (fill, or the frame)
	inline void set_filled(bool f) {filled = f; moved = true;}
	inline void set_3d(bool b) {bevel = b; moved = true;}
	inline void set_sunken(bool s) {sunken = s; moved = true;}

	protected:
	omtshared virtual void draw_full(OMediaGfxWorld *w, long x, long y);	// 0x1003ae80  vt+0x94

	short			width, height;	// +0xc8 +0xca
	OMediaRGBColor	light;			// +0xcc
	OMediaRGBColor	dark;			// +0xd2
	OMediaRGBColor	color;			// +0xd8
	bool			filled;			// +0xde
	bool			bevel;			// +0xdf
	bool			sunken;			// +0xe0 (OMT.dll never sets it in the constructor)
};

#endif
