/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMediaVPElement: a 2D element that shows another world's viewport (+0xc8), e.g. a 3D scene
    inside a 2D screen. Clicks are passed on to the viewport's world.                       */
#pragma once
#ifndef OMEDIA1_VPElement_H
#define OMEDIA1_VPElement_H

#include "OMediaUndiv2DElement.h"

class OMediaViewPort;

class OMediaVPElement : public OMediaUndiv2DElement
{
	public:
	omtshared OMediaVPElement();						// 0x1038f2f0
	omtshared virtual ~OMediaVPElement();				// 0x1038f350

	omtshared virtual void getsize(long &w, long &h);					// 0x1038f370  vt+0x5c
	omtshared virtual void update(void);								// 0x1038f3d0  vt+0x20
	omtshared virtual void process_click(OMediaWorldClickInfo *ci);	// 0x1038f5a0  vt+0x40

	inline void set_viewport(OMediaViewPort *v) {viewport = v; moved = true;}
	inline OMediaViewPort *get_viewport(void) {return viewport;}

	protected:
	omtshared virtual void draw_full(OMediaGfxWorld *w, long x, long y);	// 0x1038f3f0  vt+0x94

	OMediaViewPort	*viewport;		// +0xc8
};

#endif
