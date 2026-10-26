/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaViewPort: a window on a world, drawn into a canvas (normally the screen's double
    canvas) at its bounds; its position is the world point at the bounds' top-left corner.
    The world makes its OMediaVPModel (2D or 3D), which renders it.
    Layout in OMT.dll: +0x00 OMediaAutoMove, +0x20 OMediaAutoRotate, +0x3c OMediaWorldPosition,
    +0x4c OMediaWorldAngle, +0x58 OMedia3DNavigator, +0x94 navigator mode, +0x98 world,
    +0x9c canvas, +0xa0 double canvas (or NULL), +0xa4 model, +0xa8 bounds, +0xb8 list node,
    +0xbc pause count.                                                                          */
#pragma once
#ifndef OMEDIA1_ViewPort_H
#define OMEDIA1_ViewPort_H

#include "OMediaWorldPosition.h"
#include "OMedia3DNavigator.h"
#include "OMediaElement.h"
#include "OMediaRect.h"
#include "OMediaWorld.h"

class OMediaCanvas;
class OMediaDoubleCanvas;
class OMediaVPModel;

class OMediaVPModel
{
	public:
	omtshared OMediaVPModel(OMediaWorld *world, OMediaViewPort *vp);	// 0x1038f6e0
	omtshared virtual ~OMediaVPModel();									// 0x1038f730

	omtshared virtual void render(void);					// 0x1038f780  vt+0x04
	omtshared virtual void inform_vpchanged(void);			// vt+0x08 (nothing)
	omtshared virtual void inform_vpsizechanged(void);		// 0x1038f7e0  vt+0x0c
	omtshared virtual long getmaxvpwidth(void);			// vt+0x10 (0)
	omtshared virtual long getmaxvpheight(void);			// vt+0x14 (0)
	omtshared virtual void test_click(OMediaWorldClickInfo *ci);	// vt+0x18 (nothing)

	inline void set_full_redraw(void) {full_redraw = true;}

	protected:
	OMediaWorld		*world;			// +0x04
	OMediaViewPort	*viewport;		// +0x08
	OMediaCanvas	*buffer;		// +0x0c  a work canvas (3D)
	void			*work;			// +0x10
	bool			full_redraw;	// +0x14
};

class OMediaViewPort : public OMediaAutoMove, public OMediaAutoRotate, public OMediaWorldPosition,
					   public OMediaWorldAngle, public OMedia3DNavigator
{
	public:

	omtshared OMediaViewPort(OMediaWorld *world);		// 0x1038ec00
	omtshared virtual ~OMediaViewPort();				// 0x1038ed60

	omtshared virtual void setbounds(OMediaRect *r);	// 0x1038ee70  vt+0x10
	omtshared virtual void update(void);				// 0x1038efb0  vt+0x14
	omtshared virtual void render(void);				// 0x1038ef50  vt+0x18
	omtshared virtual void setcanvas(OMediaCanvas *c);	// 0x1038f190  vt+0x1c
	omtshared virtual void lock_canvas(void);			// 0x1038f1e0  vt+0x20
	omtshared virtual void unlock_canvas(void);			// 0x1038f220  vt+0x24
	omtshared virtual void pause(bool p);				// 0x1038f260  vt+0x28
	omtshared virtual void set_angle(short ax, short ay, short az);	// 0x1038ef70

	inline void getbounds(OMediaRect *r) const {*r = bounds;}
	inline const OMediaRect &get_bounds(void) const {return bounds;}
	inline OMediaWorld *getworld(void) {return world;}
	inline OMediaCanvas *getcanvas(void) {return canvas;}
	inline OMediaDoubleCanvas *getdoublecanvas(void) {return double_canvas;}
	inline OMediaVPModel *get_vpmodel(void) {return vpmodel;}
	inline void set_navigator_mode(bool n) {navigator_mode = n;}
	inline bool get_navigator_mode(void) const {return navigator_mode;}
	inline bool is_paused(void) const {return pause_count != 0;}
	inline omt_ViewPortList::iterator get_node(void) {return node;}

	protected:

	omtshared virtual void doplace(float nx, float ny, float nz);	// 0x1038ef00
	omtshared virtual void domove(float dx, float dy, float dz);	// 0x1038ef20

	bool					navigator_mode;		// +0x94
	OMediaWorld				*world;				// +0x98
	OMediaCanvas			*canvas;			// +0x9c
	OMediaDoubleCanvas		*double_canvas;		// +0xa0
	OMediaVPModel			*vpmodel;			// +0xa4
	OMediaRect				bounds;				// +0xa8
	omt_ViewPortList::iterator	node;			// +0xb8
	int						pause_count;		// +0xbc
};

#endif
