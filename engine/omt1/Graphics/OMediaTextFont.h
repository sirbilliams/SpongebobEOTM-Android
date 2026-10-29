/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMediaTextFont: what OMediaCCaption and OMediaCStringField need from a font (they call it
    through its virtual table, +0x18 / +0x1c / +0x20, and read the height at +0x0a). OMT.dll
    has OMediaQuickText for it; sb2.exe brings a font class of its own (vtable 0x44dd78) that
    draws from the glyph canvases of Font.omt. Both implement this interface.                */
#pragma once
#ifndef OMEDIA1_TextFont_H
#define OMEDIA1_TextFont_H

#include "OMediaGfxWorld.h"
#include "OMediaRect.h"
#include <string>

class OMediaTextFont
{
	public:
	virtual ~OMediaTextFont() {}
	// All the text at x,y
	virtual void draw_full(string text, OMediaGfxWorld *dest, short x, short y) = 0;						// vt+0x18
	// The part of the text inside 'clip' (the text laid out from the clip's corner), drawn with the clip's corner at x,y
	virtual void draw(string text, OMediaGfxWorld *dest, OMediaRect *clip, short x, short y) = 0;		// vt+0x1c
	virtual short get_text_length(string text) = 0;													// vt+0x20
	virtual short get_font_height(void) = 0;															// short at +0x0a
};

#endif
