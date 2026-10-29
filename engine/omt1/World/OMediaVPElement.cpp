/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaVPElement.h"
#include "OMediaViewPort.h"
#include "OMediaCanvas.h"

// 0x1038f2f0
OMediaVPElement::OMediaVPElement()
{
	viewport = NULL;
}

OMediaVPElement::~OMediaVPElement() {}		// 0x1038f350

// 0x1038f370: the viewport's size
void OMediaVPElement::getsize(long &w, long &h)
{
	if (viewport)
	{
		const OMediaRect &b = viewport->get_bounds();
		w = b.right - b.left;
		h = b.bottom - b.top;
		return;
	}
	h = 0;
	w = 0;
}

// 0x1038f3d0: a running viewport element is always redrawn
void OMediaVPElement::update(void)
{
	OMediaUndiv2DElement::update();
	if (pause_count == 0) moved = true;
}

// 0x1038f3f0. The viewport renders into the world at x,y (kept inside the world), on a canvas
// that wraps it; the viewport's own canvas and bounds are put back afterwards.
void OMediaVPElement::draw_full(OMediaGfxWorld *w, long x, long y)
{
	if (!viewport) return;
	OMediaCanvas wrap;
	OMediaRect old_bounds;
	viewport->getbounds(&old_bounds);
	long ox = w->get_origin_x(), oy = w->get_origin_y();
	OMediaRect r(x, y, x + (old_bounds.right - old_bounds.left), y + (old_bounds.bottom - old_bounds.top));
	if (r.left < ox) r.left = ox;
	if (r.top < oy) r.top = oy;
	if (r.right > w->getwidth() + ox) r.right = w->getwidth() + ox;
	if (r.bottom > w->getheight() + oy) r.bottom = w->getheight() + oy;
	viewport->setbounds(&r);
	wrap.setgfxworld(w);
	OMediaCanvas *saved = viewport->getcanvas();
	viewport->setcanvas(&wrap);
	viewport->render();
	viewport->setcanvas(saved);
	wrap.detach_gfxworld();		// port: the wrapping canvas does not own the world (its destructor would delete it)
	viewport->setbounds(&old_bounds);
}

// 0x1038f5a0. A click inside is tested against the viewport's world, with the viewport moved to where the element is.
void OMediaVPElement::process_click(OMediaWorldClickInfo *ci)
{
	if (!point_enclosed(ci->viewport, ci->x, ci->y) || !viewport) return;
	OMediaViewPort *outer = ci->viewport;
	OMediaCanvas *saved = viewport->getcanvas();
	viewport->setcanvas(outer->getcanvas());
	OMediaRect old_bounds;
	viewport->getbounds(&old_bounds);
	OMediaRect r;
	get_visual_rect(outer, &r);
	viewport->setbounds(&r);
	viewport->get_vpmodel()->test_click(ci);
	viewport->setbounds(&old_bounds);
	viewport->setcanvas(saved);
	if (ci->type == 0) ci->viewport = outer;
}
