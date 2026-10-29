/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaQuickText: draws text from a font canvas, one cell per character in a vertical table
    (cell n is at y = n * vertical_delta, fwidth x fheight), characters found through a 256 byte
    translation table. Fixed width, or proportional with a width per character.
    Layout in OMT.dll: +0x04 canvas, +0x08 fwidth, +0x0a fheight, +0x0c translation table,
    +0x10c proportional, +0x10e widths (256 shorts), +0x30e space width, +0x310 character
    spacing (2), +0x312 vertical delta.                                                        */
#pragma once
#ifndef OMEDIA1_QuickText_H
#define OMEDIA1_QuickText_H

#include "OMediaCanvas.h"
#include "OMediaTextFont.h"
#include "OMediaRect.h"
#include <string>

class OMediaQuickText : public OMediaTextFont
{
	public:
	omtshared OMediaQuickText();					// 0x10083fe0
	omtshared virtual ~OMediaQuickText();			// 0x10084050

	omtshared virtual void set_font_canvas(OMediaCanvas *c);										// 0x100321f0  vt+0x04
	omtshared virtual void set_font_canvas(OMediaCanvas *c, short fw, short fh, short vertical_delta = -1);	// 0x10084070  vt+0x08
	omtshared virtual void set_translation_tab(const unsigned char *tab, short size);				// 0x100840c0  vt+0x0c
	omtshared virtual void set_proportional_tab(short *tab, short size);							// 0x100846d0  vt+0x10
	omtshared virtual void create_proportional_tab(const OMediaRGBColor *background);				// 0x10084700  vt+0x14
	omtshared virtual void draw_full(string text, OMediaGfxWorld *dest, short x, short y);			// 0x100840f0  vt+0x18
	omtshared virtual void draw(string text, OMediaGfxWorld *dest, OMediaRect *clip, short x, short y);	// 0x10084310  vt+0x1c
	omtshared virtual short get_text_length(string text);											// 0x100848e0  vt+0x20

	inline OMediaCanvas *get_font_canvas(void) const {return font;}
	inline short get_font_width(void) const {return fwidth;}
	virtual short get_font_height(void) {return fheight;}
	inline void set_proportional(bool p) {proportional = p;}
	inline void set_char_space(short s) {char_space = s;}
	inline void set_space_size(short s) {space_size = s;}

	protected:
	OMediaCanvas	*font;				// +0x04
	short			fwidth, fheight;	// +0x08 +0x0a
	unsigned char	trans_tab[256];		// +0x0c
	bool			proportional;		// +0x10c
	short			widths[256];		// +0x10e
	short			space_size;			// +0x30e
	short			char_space;			// +0x310
	short			vertical_delta;		// +0x312
};

#endif
