/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMedia2DElementSorter: keeps a list of 2D elements in draw order (by x, y or z; x and y can
    include the size, i.e. sort by right or bottom edge).
    OMedia2DElement: an element drawn from a canvas (getcanvas()) at its position, in world
    pixels; sub-elements with z < 0 are drawn before it, the others after.
    Layout in OMT.dll after OMediaElement: +0x6c sorter (list +0x70, dirty +0x74, by size +0x75,
    mode +0x78), +0x7c moved, +0x7d drawn this frame (high complexity), +0x7e no erase,
    +0x7f clipped, +0x80 pixel-precise clicks, +0x84 presence (where it was last drawn, world),
    +0x94 temporary rect, +0xa4 clip rect, +0xb4 refresh list.                                 */
#pragma once
#ifndef OMEDIA1_2DElement_H
#define OMEDIA1_2DElement_H

#include "OMediaElement.h"
#include "OMediaRefreshList.h"
#include "OMediaRect.h"

class OMedia2DWorld;
class OMediaCanvas;
class OMediaViewPort;

class OMedia2DWorldRect
{
	public:
	float	left, top, right, bottom;
};

enum omt_2DSortMode { omc2dsort_None = 0, omc2dsort_X = 1, omc2dsort_Y = 2, omc2dsort_Z = 3 };

class OMedia2DElementSorter
{
	public:
	omtshared OMedia2DElementSorter(omt_ElementList *list);		// 0x10002900
	omtshared virtual ~OMedia2DElementSorter();					// 0x10002940

	omtshared virtual void sort(void);							// 0x10002950  vt+0x04

	inline void set_sort_mode(omt_2DSortMode m, bool by_size = false) {sort_mode = m; sort_by_size = by_size; sort_dirty = true;}
	inline omt_2DSortMode get_sort_mode(void) const {return sort_mode;}
	inline void set_sort_dirty(void) {sort_dirty = true;}
	inline bool get_sort_dirty(void) const {return sort_dirty;}

	protected:
	friend class OMedia2DElement;
	friend class OMedia2DAnim;
	omt_ElementList		*sort_list;		// +0x04
	bool				sort_dirty;		// +0x08
	bool				sort_by_size;	// +0x09
	omt_2DSortMode		sort_mode;		// +0x0c
};

class OMedia2DElement : public OMediaElement, public OMedia2DElementSorter
{
	public:

	omtshared OMedia2DElement();				// 0x10001680
	omtshared virtual ~OMedia2DElement();		// 0x10001740

	using OMediaElement::link;
	omtshared virtual omt_ElementType get_type(void) const;			// 0x100017f0
	omtshared virtual void link(OMediaElementContainer *c);			// 0x10001a10
	omtshared virtual void unlink(void);							// 0x10001a20
	omtshared virtual void update(void);							// 0x10001a50
	omtshared virtual void hide(bool h);							// 0x100021f0
	omtshared virtual void update_abs_position(void);				// 0x10002290
	omtshared virtual void process_click(OMediaWorldClickInfo *ci);	// 0x10002430
	omtshared virtual bool collide(OMediaElement *e);				// 0x10002480

	omtshared virtual void link(OMedia2DElement *super);			// 0x100019d0  vt+0x54
	omtshared virtual void link(OMedia2DWorld *world);				// 0x10001990  vt+0x58
	omtshared virtual void getsize(long &w, long &h);				// 0x10001a60  vt+0x5c
	omtshared virtual void get_visual_rect(OMediaViewPort *vp, OMediaRect *r);	// 0x10001af0  vt+0x60
	omtshared virtual void get_visual_pos(OMediaViewPort *vp, long &x, long &y);	// 0x10001aa0  vt+0x64
	omtshared virtual void get_world_absrect(OMediaRect *r);		// 0x10001bc0  vt+0x68
	omtshared virtual void get_world_absrect(OMedia2DWorldRect *r);	// 0x10001b60  vt+0x6c
	omtshared virtual bool point_enclosed(OMediaViewPort *vp, short x, short y);	// 0x10002320  vt+0x70
	omtshared virtual void draw(OMediaRect *src, long x, long y, OMediaCanvas *dest, OMediaRefreshList *rl);	// 0x10001c30  vt+0x74
	omtshared virtual void draw(OMediaViewPort *vp, OMediaRefreshList *rl, OMediaRect *drawn);				// 0x10001db0  vt+0x78
	omtshared virtual OMediaCanvas *getcanvas(void);				// vt+0x7c (NULL)
	omtshared virtual void presence_removed(void);					// 0x10002020  vt+0x80
	omtshared virtual float get_final_x(void);						// 0x10002300  vt+0x84
	omtshared virtual float get_final_y(void);						// 0x10002310  vt+0x88
	omtshared virtual OMediaCanvas *get_collision_mask(void);		// vt+0x8c (NULL)

	inline void set_clipping(bool c) {clipping = c;}
	inline void set_clip_rect(const OMediaRect &r) {clip_rect = r; clipping = true; moved = true;}
	inline void set_pixel_click(bool p) {pixel_click = p;}
	inline void set_no_erase(bool n) {no_erase = n;}
	inline void setdirty(void) {moved = true;}
	inline const OMediaRect &get_presence(void) const {return presence;}

	protected:

	omtshared virtual void doplace(float nx, float ny, float nz);	// 0x10001800
	omtshared virtual void domove(float dx, float dy, float dz);	// 0x100018c0
	omtshared virtual void update_sub_presence(bool hidden);		// 0x10002240  vt+0x90

	OMedia2DElementSorter *parent_sorter(void);

	friend class OMedia2DVPModel;
	friend class OMedia2DWorld;

	bool				moved;			// +0x7c
	bool				drawn;			// +0x7d
	bool				no_erase;		// +0x7e
	bool				clipping;		// +0x7f
	bool				pixel_click;	// +0x80
	OMediaRect			presence;		// +0x84
	OMediaRect			temp_rect;		// +0x94
	OMediaRect			clip_rect;		// +0xa4
	OMediaRefreshList	refresh;		// +0xb4
};

#endif
