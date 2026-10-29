/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaUndiv2DElement: a 2D element that cannot be drawn in pieces. draw_full() paints the
    whole element into a gfx world; draw() paints it into a temporary world of the element's
    size (first with the canvas below it when +0xc4 'backdrop' is set, for elements that blend
    with what is behind them) and copies the pieces that changed to the canvas.
    Base of OMediaVPElement, OMediaBoxElement and OMediaCStringField.                          */
#pragma once
#ifndef OMEDIA1_Undiv2DElement_H
#define OMEDIA1_Undiv2DElement_H

#include "OMedia2DElement.h"

class OMediaGfxWorld;

class OMediaUndiv2DElement : public OMedia2DElement
{
	public:
	omtshared OMediaUndiv2DElement();					// 0x1038e7c0
	omtshared virtual ~OMediaUndiv2DElement();			// 0x1038e810

	omtshared virtual void update(void);				// 0x1038eb20  vt+0x20
	using OMedia2DElement::draw;
	omtshared virtual void draw(OMediaRect *src, long x, long y, OMediaCanvas *dest, OMediaRefreshList *rl);	// 0x1038e830  vt+0x74

	inline void set_backdrop(bool b) {backdrop = b;}
	inline bool get_backdrop(void) const {return backdrop;}

	protected:
	omtshared virtual void draw_full(OMediaGfxWorld *w, long x, long y);	// 0x10397770  vt+0x94 (nothing)

	bool		backdrop;		// +0xc4
};

#endif
