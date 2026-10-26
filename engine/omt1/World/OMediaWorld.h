/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaWorld: elements, viewports, a background, a frame timer. update() (a periodical) draws
    the world when its frame rate says so: viewports update (scroll), elements update (auto
    moves, animations), the world sorts, the viewports render into their canvases, then the
    double canvases they drew into are swapped.
    Layout in OMT.dll: +0x00 OMediaElementContainer, +0x10 OMediaPeriodical, +0x1c OMediaListener,
    +0x30 viewports, +0x3c frame rate mode, +0x40 fixed frame rate (ms), +0x44/+0x48 auto
    regulation count/sum, +0x4c maximum elapsed (500), +0x50 being destroyed, +0x54 pause count,
    +0x58 redraw all, +0x59 wait for the flip, +0x5c timer, +0x6c canvases to swap, +0x78 background. */
#pragma once
#ifndef OMEDIA1_World_H
#define OMEDIA1_World_H

#include "OMediaElement.h"
#include "OMediaPeriodical.h"
#include "OMediaListener.h"
#include "OMediaTimer.h"
#include "OMediaRect.h"
#include <list>

class OMediaViewPort;
class OMediaVPModel;
class OMediaBackground;
class OMediaDoubleCanvas;
class OMediaRefreshList;
class OMediaGfxWorld;

typedef list<OMediaViewPort*> omt_ViewPortList;

enum omt_FrameRateMode
{
	omfrc_Fixed = 0,			// draw when the fixed frame time has passed
	omfrc_Max = 1,				// draw at every update
	omfrc_AutoRegulate = 2		// fixed, the time measured over 16 frames (default)
};

struct OMediaSwapCanvasEntry
{
	OMediaDoubleCanvas	*canvas;		// +0x00
	bool				has_rect;		// +0x04
	OMediaRect			rect;			// +0x08
	OMediaRefreshList	*refresh;		// +0x18
	bool				clear_refresh;	// +0x1c
};

class OMediaWorld : public OMediaElementContainer, public OMediaPeriodical, public OMediaListener
{
	public:

	omtshared OMediaWorld();				// 0x10399db0
	omtshared virtual ~OMediaWorld();		// 0x10399ee0

	omtshared virtual OMediaVPModel *create_vp_model(OMediaViewPort *vp);	// 0x1039a050  vt+0x08
	omtshared virtual void update(void);									// 0x1039a2a0  vt+0x0c
	omtshared virtual void draw(bool force = false);						// 0x1039a330  vt+0x10
	omtshared virtual void set_framerate_mode(omt_FrameRateMode m);		// 0x1039a0a0  vt+0x14
	omtshared virtual void set_fixed_framerate(unsigned long ms);			// 0x1039a0f0  vt+0x18
	omtshared virtual bool is_time_to_update(void);						// 0x1039a140  vt+0x1c
	omtshared virtual void sort(void);										// vt+0x20 (OMediaWorld: nothing)
	omtshared virtual void setbackground(OMediaBackground *bg);			// 0x1039a060  vt+0x24
	omtshared virtual void background_changed(void);						// 0x1039a090  vt+0x28
	omtshared virtual void pause(bool p);									// 0x1039a5b0  vt+0x2c
	omtshared virtual void add_new_swapcanvas(OMediaDoubleCanvas *dc, OMediaRect *r, OMediaRefreshList *rl,
											  bool unique, bool clear_rl);	// 0x1039a4f0  vt+0x30

	omtshared virtual void spend_time(void);								// 0x1039a290 (OMediaPeriodical)
	omtshared virtual void listen_to_message(omt_Message msg, void *param = NULL);	// 0x1039a7e0 (OMediaListener)

	omtshared OMediaGfxWorld *get_temp_buffer(short width, short height);	// 0x1039a6b0
	omtshared void get_mouse_position_info(OMediaWorldPositionInfo &info);	// 0x1039a830
	omtshared void get_world_position_info(long x, long y, OMediaWorldPositionInfo &info);	// 0x1039a900

	inline OMediaTimer *get_timer(void) {return &timer;}
	inline omt_ViewPortList *get_viewports(void) {return &viewports;}
	inline OMediaBackground *getbackground(void) {return background;}
	inline bool is_paused(void) const {return pause_count != 0;}
	inline bool get_redraw_all(void) const {return redraw_all;}
	inline bool is_destroying(void) const {return destroying;}
	inline void set_wait_flip(bool w) {wait_flip = w;}
	inline void set_max_elapsed(unsigned long ms) {max_elapsed = ms;}
	inline omt_FrameRateMode get_framerate_mode(void) const {return framerate_mode;}
	inline unsigned long get_fixed_framerate(void) const {return fixed_framerate;}

	protected:

	omtshared virtual void update_autoregulate(void);						// 0x1039a220  vt+0x34
	omtshared virtual void update_elements(omt_ElementList::iterator first, omt_ElementList::iterator last);	// 0x1039a3b0  vt+0x38
	omtshared virtual void pause_elements(bool p, omt_ElementList::iterator first, omt_ElementList::iterator last);	// 0x1039a660  vt+0x3c
	omtshared virtual void swapdoublecanvass(void);						// 0x1039a410  vt+0x40

	friend class OMediaViewPort;
	friend class OMediaBackground;

	omt_ViewPortList			viewports;			// +0x30
	omt_FrameRateMode			framerate_mode;		// +0x3c
	unsigned long				fixed_framerate;	// +0x40
	unsigned long				autoreg_count;		// +0x44
	unsigned long				autoreg_sum;		// +0x48
	unsigned long				max_elapsed;		// +0x4c
	bool						destroying;			// +0x50
	int							pause_count;		// +0x54
	bool						redraw_all;			// +0x58
	bool						wait_flip;			// +0x59
	OMediaTimer					timer;				// +0x5c
	list<OMediaSwapCanvasEntry>	swap_canvases;		// +0x6c
	OMediaBackground			*background;		// +0x78

	omtshared static int				world_count;	// 0x10421260
	omtshared static OMediaGfxWorld		*temp_buffer;	// 0x1042125c
};

#endif
