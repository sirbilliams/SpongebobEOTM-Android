/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia2DAnim.h"
#include "OMedia2DWorld.h"
#include "OMediaCanvas.h"

//------------------------------------------------------------------------------ OMedia2DAnimFrame

OMedia2DAnimFrame::OMedia2DAnimFrame()		// 0x10001470
{
	canvas = NULL;
	canvas_handle = NULL;
	collision_mask = NULL;
}

// 0x100014b0
OMedia2DAnimFrame::~OMedia2DAnimFrame()
{
	if (canvas) canvas->element_unlink();
	if (collision_mask) collision_mask->element_unlink();
	delete canvas_handle;
}

// 0x10001540
void OMedia2DAnimFrame::setcanvas(OMediaCanvas *c)
{
	delete canvas_handle;
	canvas_handle = NULL;
	if (canvas) canvas->element_unlink();
	canvas = c;
	if (c) c->element_link();
	if (anim && anim->getcurrentframe() == this) anim->setdirty();
}

// 0x100015b0
void OMedia2DAnimFrame::setcanvas_handle(OMediaDBHandle<OMediaCanvas> *h)
{
	setcanvas(NULL);
	if (h) canvas_handle = new OMediaDBHandle<OMediaCanvas>(*h);
}

// 0x10001600
void OMedia2DAnimFrame::set_collision_mask(OMediaCanvas *c)
{
	if (collision_mask) collision_mask->element_unlink();
	collision_mask = c;
	if (c) c->element_link();
}

//------------------------------------------------------------------------------ OMedia2DAnim

OMedia2DAnim::OMedia2DAnim() {}				// 0x10001000
OMedia2DAnim::~OMedia2DAnim() {}			// 0x10001090

// 0x10001110: the anim runs on the world's frame time
void OMedia2DAnim::update(void)
{
	OMedia2DElement::update();
	OMediaAnim::update((long)world->get_timer()->getelapsed());
}

// 0x10001160. A frame change can change the sort key: x/y offsets (and sizes when sorting by size).
void OMedia2DAnim::update_sorter(OMediaAnimFrame *prev)
{
	if (superelement) return;
	OMedia2DWorld *w = (OMedia2DWorld*)world;
	if (!w || w->get_sort_dirty()) return;		// port: OMT.dll assumes a world
	omt_2DSortMode m = w->get_sort_mode();
	if (m == omc2dsort_None || m == omc2dsort_Z) return;
	OMedia2DAnimFrame *p = (OMedia2DAnimFrame*)prev, *c = frame();
	if (!p || !p->getcanvas())
	{
		if (!c->getcanvas()) return;
		w->set_sort_dirty();
		return;
	}
	if (!c->getcanvas()) return;
	if (!w->sort_by_size)
	{
		if (m == omc2dsort_X ? c->getoffsetx() == p->getoffsetx() : c->getoffsety() == p->getoffsety()) return;
		w->set_sort_dirty();
		return;
	}
	if (m == omc2dsort_X)
	{
		if (c->getoffsetx() == p->getoffsetx() && p->getcanvas()->getwidth() == c->getcanvas()->getwidth()) return;
	}
	else if (c->getoffsety() == p->getoffsety() && p->getcanvas()->getheight() == c->getcanvas()->getheight()) return;
	w->set_sort_dirty();
}

OMediaCanvas *OMedia2DAnim::getcanvas(void) { return frame() ? frame()->getcanvas() : NULL; }		// 0x10001280
float OMedia2DAnim::getabsoluteposx(void) const { return frame() ? frame()->getoffsetx() + absx : absx; }	// 0x100012a0
float OMedia2DAnim::getabsoluteposy(void) const { return frame() ? frame()->getoffsety() + absy : absy; }	// 0x100012c0
float OMedia2DAnim::getabsoluteposz(void) const { return frame() ? frame()->getoffsetz() + absz : absz; }	// 0x100012e0
float OMedia2DAnim::get_final_x(void) { return frame() ? frame()->getoffsetx() + x : x; }			// 0x10001300
float OMedia2DAnim::get_final_y(void) { return frame() ? frame()->getoffsety() + y : y; }			// 0x10001320
OMediaCanvas *OMedia2DAnim::get_collision_mask(void) { return frame() ? frame()->get_collision_mask() : NULL; }	// 0x10001380

// 0x10001340
void OMedia2DAnim::pause(bool p)
{
	OMediaElement::pause(p);
	OMediaAnim::pause(p);
}

void OMedia2DAnim::setdirty(void) { moved = true; }						// 0x10001360
void OMedia2DAnim::presence_removed(void) { OMedia2DElement::presence_removed(); }	// 0x10001370
