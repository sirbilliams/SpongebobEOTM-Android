/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaWorld.h"
#include "OMediaViewPort.h"
#include "OMediaBackground.h"
#include "OMediaCanvas.h"
#include "OMediaRefreshList.h"
#include "OMediaPixmapWorld.h"
#include "OMediaDisplay.h"

int OMediaWorld::world_count = 0;
OMediaGfxWorld *OMediaWorld::temp_buffer = NULL;

// 0x10399db0. Auto-regulated at 40 ms, at most 500 ms per frame, a black colour background.
OMediaWorld::OMediaWorld()
{
	redraw_all = false;
	framerate_mode = omfrc_AutoRegulate;
	fixed_framerate = 40;
	autoreg_count = 0;
	autoreg_sum = 0;
	max_elapsed = 500;
	destroying = false;
	wait_flip = false;
	pause_count = 0;
	background = NULL;
	new OMediaColorBackground(this);
	world_count++;
	redraw_all = false;
}

// 0x10399ee0
OMediaWorld::~OMediaWorld()
{
	destroying = true;
	if (background) delete background;
	while (viewports.size()) delete viewports.front();		// a viewport's destructor unlinks it
	world_count--;
	if (world_count == 0 && temp_buffer)
	{
		temp_buffer->purge();
		delete temp_buffer;
		temp_buffer = NULL;
	}
	// OMT.dll: the elements are deleted by ~OMediaElementContainer, with the world's memory still readable.
	// port: deleted here, while this is still an OMediaWorld (the elements ask it is_destroying())
	while (elements.size()) delete elements.front();
}

OMediaVPModel *OMediaWorld::create_vp_model(OMediaViewPort *) { return NULL; }	// 0x1039a050
void OMediaWorld::sort(void) {}

// 0x1039a060
void OMediaWorld::setbackground(OMediaBackground *bg)
{
	if (background) delete background;
	background = bg;
	if (bg) bg->world = this;
	background_changed();
}

void OMediaWorld::background_changed(void) { redraw_all = true; }	// 0x1039a090

// 0x1039a0a0
void OMediaWorld::set_framerate_mode(omt_FrameRateMode m)
{
	timer.stop();
	timer.setelapsed(0);
	framerate_mode = m;
}

// 0x1039a0f0
void OMediaWorld::set_fixed_framerate(unsigned long ms)
{
	timer.stop();
	timer.setelapsed(0);
	fixed_framerate = ms;
}

// 0x1039a140. The timer's (locked) elapsed time decides; the timer restarts at each frame.
bool OMediaWorld::is_time_to_update(void)
{
	if (pause_count) return false;
	if (!timer.is_running()) timer.start();
	unsigned int e = timer.get_last_elapsed();
	if (framerate_mode == omfrc_AutoRegulate) update_autoregulate();
	if (framerate_mode == omfrc_Fixed || framerate_mode == omfrc_AutoRegulate)
		if (e < fixed_framerate) return false;
	timer.start();
	return true;
}

// 0x1039a220. The frame time becomes the average of 16 frames (or of fewer once a frame
// takes 500 ms or more); no time at all resets it to 20 ms.
void OMediaWorld::update_autoregulate(void)
{
	unsigned long e = timer.get_last_elapsed();
	if (e == 0)
	{
		autoreg_count = 0;
		autoreg_sum = 0;
		fixed_framerate = 20;
		return;
	}
	if (autoreg_count == 16)
	{
		unsigned long s = autoreg_sum;
		autoreg_sum = 0;
		autoreg_count = 0;
		fixed_framerate = s >> 4;
		return;
	}
	if (e > 499 && autoreg_count != 0)
	{
		unsigned long s = autoreg_sum;
		autoreg_sum = 0;
		fixed_framerate = s / autoreg_count;
		autoreg_count = 0;
		return;
	}
	autoreg_count++;
	autoreg_sum += e;
}

// 0x1039a290
void OMediaWorld::spend_time(void) { update(); }

// 0x1039a2a0. The timer is frozen while the world draws; the time per frame is clamped.
void OMediaWorld::update(void)
{
	timer.lock(true);
	if (max_elapsed && max_elapsed < timer.get_last_elapsed()) timer.setelapsed(max_elapsed);
	if (is_time_to_update()) draw(false);
	timer.lock(false);
}

// 0x1039a330
void OMediaWorld::draw(bool force)
{
	if (force) redraw_all = true;
	for (omt_ViewPortList::iterator v = viewports.begin(); v != viewports.end(); v++) (*v)->update();
	if (pause_count == 0)
	{
		update_elements(elements.begin(), elements.end());
		sort();
		if (pause_count == 0) goto render;
	}
	if (!force)
	{
		redraw_all = false;
		return;
	}
render:
	for (omt_ViewPortList::iterator v = viewports.begin(); v != viewports.end(); v++) (*v)->render();
	swapdoublecanvass();
	redraw_all = false;
}

// 0x1039a3b0. An element flagged for deletion is deleted instead of its sub-elements updated.
void OMediaWorld::update_elements(omt_ElementList::iterator first, omt_ElementList::iterator last)
{
	while (first != last)
	{
		OMediaElement *e = *first;
		first++;
		e->update();
		if (e->delete_flag) delete e;
		else if (e->elements.size()) update_elements(e->elements.begin(), e->elements.end());
	}
}

// 0x1039a410. Swaps every double canvas drawn this frame (once each).
void OMediaWorld::swapdoublecanvass(void)
{
	while (swap_canvases.size())
	{
		OMediaSwapCanvasEntry &s = swap_canvases.back();
		OMediaShortRect r;
		if (s.has_rect) r.set((short)s.rect.left, (short)s.rect.top, (short)s.rect.right, (short)s.rect.bottom);
		s.canvas->swap(s.has_rect ? &r : NULL, s.refresh, wait_flip);
		s.canvas->set_full_refresh(false);
		if (s.clear_refresh && s.refresh) s.refresh->clear();
		swap_canvases.pop_back();
	}
}

// 0x1039a4f0
void OMediaWorld::add_new_swapcanvas(OMediaDoubleCanvas *dc, OMediaRect *r, OMediaRefreshList *rl, bool unique, bool clear_rl)
{
	OMediaSwapCanvasEntry e;
	e.canvas = dc;
	e.has_rect = r != NULL;
	if (r) e.rect = *r;
	e.refresh = rl;
	e.clear_refresh = clear_rl;
	if (unique)
		for (list<OMediaSwapCanvasEntry>::iterator i = swap_canvases.begin(); i != swap_canvases.end(); i++)
			if ((*i).canvas == dc) return;
	swap_canvases.push_back(e);
}

// 0x1039a5b0. The timer stops and restarts from 0 on the first pause and on the last resume.
void OMediaWorld::pause(bool p)
{
	if (!p)
	{
		if (pause_count == 1)
		{
			timer.stop();
			timer.setelapsed(0);
			pause_elements(false, elements.begin(), elements.end());
		}
		pause_count--;
	}
	else
	{
		if (++pause_count == 1)
		{
			timer.stop();
			timer.setelapsed(0);
			pause_elements(p, elements.begin(), elements.end());
		}
	}
}

// 0x1039a660
void OMediaWorld::pause_elements(bool p, omt_ElementList::iterator first, omt_ElementList::iterator last)
{
	for (; first != last; first++)
	{
		OMediaElement *e = *first;
		e->pause(p);
		if (e->elements.size()) pause_elements(p, e->elements.begin(), e->elements.end());
	}
}

// 0x1039a6b0. A shared memory world at least that big.
OMediaGfxWorld *OMediaWorld::get_temp_buffer(short width, short height)
{
	if (!temp_buffer)
	{
		temp_buffer = new OMediaPixBlitterWorld;
		temp_buffer->create(width, height, NULL, NULL, 0);
		temp_buffer->lock();
	}
	else if (temp_buffer->getwidth() < width || temp_buffer->getheight() < height)
	{
		temp_buffer->unlock();
		delete temp_buffer;
		temp_buffer = new OMediaPixBlitterWorld;
		temp_buffer->create(width, height, NULL, NULL, 0);
		temp_buffer->lock();
	}
	return temp_buffer;
}

// 0x1039a7e0. A click (-602) goes to the viewports' models until one finds something.
void OMediaWorld::listen_to_message(omt_Message msg, void *param)
{
	if (msg == -602 && viewports.size())
	{
		OMediaWorldClickInfo *ci = (OMediaWorldClickInfo*)param;
		for (omt_ViewPortList::iterator v = viewports.begin(); v != viewports.end(); v++)
		{
			(*v)->get_vpmodel()->test_click(ci);
			if (ci->type != omcct_Nothing)
			{
				abort_broadcast();
				return;
			}
		}
	}
}

// 0x1039a900
void OMediaWorld::get_world_position_info(long x, long y, OMediaWorldPositionInfo &info)
{
	OMediaWorldClickInfo ci;
	ci.x = (short)x;
	ci.y = (short)y;
	for (omt_ViewPortList::iterator v = viewports.begin(); v != viewports.end(); v++)
	{
		(*v)->get_vpmodel()->test_click(&ci);
		if (ci.type != omcct_Nothing) break;
	}
	info = ci;
}

// 0x1039a830. The input driver's mouse position (OMT.dll: the global input driver at 0x104211fc,
// relative to the screen device's position).
extern bool omf_get_mouse_position(short &x, short &y) __attribute__((weak));	// lane B: input
void OMediaWorld::get_mouse_position_info(OMediaWorldPositionInfo &info)
{
	short mx = 0, my = 0, sx, sy;
	if (omf_get_mouse_position) omf_get_mouse_position(mx, my);
	OMediaScreenDevice::getdefaultdevice()->getposition(sx, sy);
	get_world_position_info(mx - sx, my - sy, info);
}
