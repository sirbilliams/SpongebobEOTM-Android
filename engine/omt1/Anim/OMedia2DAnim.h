/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMedia2DAnim: a 2D element showing the canvas of its anim's current frame, offset by the
    frame's offset (OMediaAnim at +0xc4). OMedia2DAnimFrame: a frame with a canvas (+0x28),
    a canvas handle (+0x2c) and a collision mask (+0x30).                                      */
#pragma once
#ifndef OMEDIA1_2DAnim_H
#define OMEDIA1_2DAnim_H

#include "OMedia2DElement.h"
#include "OMediaAnim.h"
#include "OMediaCanvasElement.h"

class OMedia2DAnimFrame : public OMediaAnimFrame
{
	public:
	omtshared OMedia2DAnimFrame();				// 0x10001470
	omtshared virtual ~OMedia2DAnimFrame();		// 0x100014b0

	omtshared virtual void setcanvas(OMediaCanvas *c);							// 0x10001540  vt+0x14
	omtshared virtual void setcanvas_handle(OMediaDBHandle<OMediaCanvas> *h);	// 0x100015b0  vt+0x18
	omtshared virtual void set_collision_mask(OMediaCanvas *c);					// 0x10001600  vt+0x1c

	inline OMediaCanvas *getcanvas(void) const {return canvas;}
	inline OMediaCanvas *get_collision_mask(void) const {return collision_mask;}

	protected:
	OMediaCanvas					*canvas;			// +0x28
	OMediaDBHandle<OMediaCanvas>	*canvas_handle;		// +0x2c
	OMediaCanvas					*collision_mask;	// +0x30
};

class OMedia2DAnim : public OMedia2DElement, public OMediaAnim
{
	public:
	omtshared OMedia2DAnim();				// 0x10001000
	omtshared virtual ~OMedia2DAnim();		// 0x10001090

	omtshared virtual void update(void);							// 0x10001110
	omtshared virtual float getabsoluteposx(void) const;			// 0x100012a0
	omtshared virtual float getabsoluteposy(void) const;			// 0x100012c0
	omtshared virtual float getabsoluteposz(void) const;			// 0x100012e0
	omtshared virtual void pause(bool p);							// 0x10001340
	omtshared virtual OMediaCanvas *getcanvas(void);				// 0x10001280
	omtshared virtual void presence_removed(void);					// 0x10001370
	omtshared virtual float get_final_x(void);						// 0x10001300
	omtshared virtual float get_final_y(void);						// 0x10001320
	omtshared virtual OMediaCanvas *get_collision_mask(void);		// 0x10001380
	omtshared virtual void setdirty(void);							// 0x10001360 (OMediaAnim)

	protected:
	omtshared virtual void update_sorter(OMediaAnimFrame *previous);	// 0x10001160

	inline OMedia2DAnimFrame *frame(void) const {return (OMedia2DAnimFrame*)current_frame;}
};

#endif
