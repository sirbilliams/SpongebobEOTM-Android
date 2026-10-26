/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia2DElement.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include "OMediaCanvas.h"
#include "OMediaMSVCSort.h"
#include <vector>

// _ftol: truncation toward zero
static inline long omf_ftol(float f) { return (long)f; }

//------------------------------------------------------------------------------ OMedia2DElementSorter

// 0x10002900
OMedia2DElementSorter::OMedia2DElementSorter(omt_ElementList *list)
{
	sort_list = list;
	sort_dirty = false;
	sort_mode = omc2dsort_None;
	sort_by_size = false;
}

OMedia2DElementSorter::~OMedia2DElementSorter() {}		// 0x10002940

struct omt_SortEntry
{
	OMedia2DElement	*element;
	long			key;
	inline bool operator<(const omt_SortEntry &o) const {return key < o.key;}
};

// 0x10002950. The key is the integer part of the final x (+ width), y (+ height) or z.
void OMedia2DElementSorter::sort(void)
{
	if (!sort_dirty || sort_mode == omc2dsort_None || sort_list->size() < 2) return;
	std::vector<omt_SortEntry> v;
	long key = 0;
	for (omt_ElementList::iterator i = sort_list->begin(); i != sort_list->end(); i++)
	{
		OMedia2DElement *e = (OMedia2DElement*)*i;
		long w, h;
		switch (sort_mode)
		{
			case omc2dsort_X:
				if (sort_by_size) { e->getsize(w, h); key = omf_ftol(e->get_final_x() + (float)w); }
				else key = omf_ftol(e->get_final_x());
				break;
			case omc2dsort_Y:
				if (sort_by_size) { e->getsize(w, h); key = omf_ftol(e->get_final_y() + (float)h); }
				else key = omf_ftol(e->get_final_y());
				break;
			case omc2dsort_Z: key = omf_ftol(e->getz()); break;
			default: break;
		}
		omt_SortEntry s;
		s.element = e;
		s.key = key;
		v.push_back(s);
	}
	omt_msvc_sort(&v[0], &v[0] + v.size());
	sort_list->clear();
	for (size_t i = 0; i < v.size(); i++)
	{
		sort_list->push_back(v[i].element);
		v[i].element->set_node(--sort_list->end());
	}
	sort_dirty = false;
}

//------------------------------------------------------------------------------ OMedia2DElement

// 0x10001680. Sub-elements sorted by z.
OMedia2DElement::OMedia2DElement() : OMedia2DElementSorter(&elements)
{
	pixel_click = true;
	clipping = false;
	no_erase = false;
	moved = false;
	drawn = false;
	sort_mode = omc2dsort_Z;
	sort_dirty = true;
	presence.set(0, 0, 0, 0);
	temp_rect.set(0, 0, 0, 0);
	clip_rect.set(0, 0, 0, 0);
}

// 0x10001740
OMedia2DElement::~OMedia2DElement()
{
	while (elements.size()) delete elements.front();
	OMedia2DElement::unlink();
}

omt_ElementType OMedia2DElement::get_type(void) const { return omcet_2DElement; }	// 0x100017f0

// The sorter of the list this element is in: its super-element's, else its world's
OMedia2DElementSorter *OMedia2DElement::parent_sorter(void)
{
	OMedia2DElementSorter *s = NULL;
	if (world) s = (OMedia2DWorld*)world;
	if (superelement) s = (OMedia2DElement*)superelement;
	return s;
}

// 0x10001800. A change of the sort coordinate marks the list for sorting.
void OMedia2DElement::doplace(float nx, float ny, float nz)
{
	OMedia2DElementSorter *s = parent_sorter();
	if (s && !s->sort_dirty)
	{
		switch (s->sort_mode)
		{
			case omc2dsort_X: if (x != nx) s->sort_dirty = true; break;
			case omc2dsort_Y: if (y != ny) s->sort_dirty = true; break;
			case omc2dsort_Z: if (z != nz) s->sort_dirty = true; break;
			default: break;
		}
	}
	if (x != nx || y != ny)
	{
		y = ny;
		x = nx;
		moved = true;
		update_abs_position();
	}
	if (z != nz)
	{
		moved = true;
		z = nz;
	}
}

// 0x100018c0
void OMedia2DElement::domove(float dx, float dy, float dz)
{
	OMedia2DElementSorter *s = parent_sorter();
	if (s && !s->sort_dirty)
	{
		switch (s->sort_mode)
		{
			case omc2dsort_X: if (dx != 0.0f) s->sort_dirty = true; break;
			case omc2dsort_Y: if (dy != 0.0f) s->sort_dirty = true; break;
			case omc2dsort_Z: if (dz != 0.0f) s->sort_dirty = true; break;
			default: break;
		}
	}
	if (dx != 0.0f || dy != 0.0f)
	{
		moved = true;
		x = dx + x;
		y = dy + y;
		update_abs_position();
	}
	if (dz != 0.0f)
	{
		moved = true;
		z = dz + z;
	}
}

// 0x10001990
void OMedia2DElement::link(OMedia2DWorld *w)
{
	OMediaElement::link((OMediaWorld*)w);
	moved = true;
	presence.set(0, 0, 0, 0);
	w->sort_dirty = true;
}

// 0x100019d0
void OMedia2DElement::link(OMedia2DElement *super)
{
	OMediaElement::link((OMediaElement*)super);
	moved = true;
	presence.set(0, 0, 0, 0);
	super->sort_dirty = true;
}

void OMedia2DElement::link(OMediaElementContainer *c) { OMediaElement::link(c); }		// 0x10001a10

// 0x10001a20: the place it was drawn must be redrawn
void OMedia2DElement::unlink(void)
{
	if (!container)
	{
		if (!world) return;
		presence_removed();
	}
	OMediaElement::unlink();
}

void OMedia2DElement::update(void) { OMediaElement::update(); }		// 0x10001a50

// 0x10001a60
void OMedia2DElement::getsize(long &w, long &h)
{
	OMediaCanvas *c = getcanvas();
	if (c)
	{
		w = c->getwidth();
		h = c->getheight();
		return;
	}
	h = 0;
	w = 0;
}

// 0x10001aa0: position relative to the viewport's top-left world point
void OMedia2DElement::get_visual_pos(OMediaViewPort *vp, long &px, long &py)
{
	float fx = getabsoluteposx() - vp->getx();
	float fy = getabsoluteposy() - vp->gety();
	px = omf_ftol(fx);
	py = omf_ftol(fy);
}

// 0x10001af0
void OMedia2DElement::get_visual_rect(OMediaViewPort *vp, OMediaRect *r)
{
	float ax = getabsoluteposx(), ay = getabsoluteposy();
	long w, h;
	getsize(w, h);
	long l = omf_ftol(ax - vp->getx()), t = omf_ftol(ay - vp->gety());
	r->left = l;
	r->top = t;
	r->right = l + w;
	r->bottom = t + h;
}

// 0x10001b60
void OMedia2DElement::get_world_absrect(OMedia2DWorldRect *r)
{
	float ax = getabsoluteposx(), ay = getabsoluteposy();
	long w, h;
	getsize(w, h);
	r->left = ax;
	r->top = ay;
	r->right = (float)w + ax;
	r->bottom = (float)h + ay;
}

// 0x10001bc0
void OMedia2DElement::get_world_absrect(OMediaRect *r)
{
	float ax = getabsoluteposx(), ay = getabsoluteposy();
	long w, h;
	getsize(w, h);
	long l = omf_ftol(ax), t = omf_ftol(ay);
	r->left = l;
	r->top = t;
	r->right = l + w;
	r->bottom = t + h;
}

// 0x10001c30. Draws the canvas at (x,y) of 'dest' (src: the part of the canvas), or through the
// refresh list's rectangles.
void OMedia2DElement::draw(OMediaRect *src, long px, long py, OMediaCanvas *dest, OMediaRefreshList *rl)
{
	OMediaCanvas *c = getcanvas();
	if (!c || !dest) return;
	if (!rl)
	{
		if (src)
		{
			if (c->getwidth() == src->get_width() && c->getheight() == src->get_height())
			{
				c->draw_full(dest->getdefaultdrawgfxworld(), (short)px, (short)py, NULL);
				return;
			}
			c->draw(dest->getdefaultdrawgfxworld(), src, (short)px, (short)py, NULL);
			return;
		}
		c->draw_full(dest->getdefaultdrawgfxworld(), (short)px, (short)py, NULL);
		return;
	}
	if (c->getgfxworld()) c->getgfxworld()->lock();
	if (dest->getgfxworld()) dest->getgfxworld()->lock();
	for (omt_ShortRectList::iterator i = rl->get_list()->begin(); i != rl->get_list()->end(); i++)
	{
		OMediaRect s((*i).left - px, (*i).top - py, (*i).right - px, (*i).bottom - py);
		c->draw(dest->getdefaultdrawgfxworld(), &s, (*i).left, (*i).top, NULL);
	}
	if (c->getgfxworld()) c->getgfxworld()->unlock();
	if (dest->getgfxworld()) dest->getgfxworld()->unlock();
}

// 0x10001db0. Draws the element into the viewport's canvas, clipped to the viewport's bounds
// (and to its clip rect), and returns where (empty when outside).
void OMedia2DElement::draw(OMediaViewPort *vp, OMediaRefreshList *rl, OMediaRect *drawn_rect)
{
	OMediaRect C = vp->get_bounds();
	if (clipping)
	{
		long ox = C.left - omf_ftol(vp->getx()), oy = C.top - omf_ftol(vp->gety());
		OMediaRect cr(clip_rect.left + ox, clip_rect.top + oy, clip_rect.right + ox, clip_rect.bottom + oy);
		if (!(cr.top < C.bottom && cr.left < C.right && C.top < cr.bottom && C.left < cr.right)) goto none;
		if (cr.left > C.left) C.left = cr.left;
		if (cr.top > C.top) C.top = cr.top;
		if (cr.right <= C.right) C.right = cr.right;
		if (cr.bottom <= C.bottom) C.bottom = cr.bottom;
	}
	{
		OMediaRect er;
		get_world_absrect(&er);
		long ox = vp->get_bounds().left - omf_ftol(vp->getx()), oy = vp->get_bounds().top - omf_ftol(vp->gety());
		er.left += ox; er.right += ox; er.top += oy; er.bottom += oy;
		if (!(C.top < er.bottom && C.left < er.right && er.top < C.bottom && er.left < C.right)) goto none;
		OMediaRect in = er;
		if (C.left > in.left) in.left = C.left;
		if (C.top > in.top) in.top = C.top;
		if (C.right <= in.right) in.right = C.right;
		if (C.bottom <= in.bottom) in.bottom = C.bottom;
		if (drawn_rect) *drawn_rect = in;
		if (rl) draw(NULL, er.left, er.top, vp->getcanvas(), rl);
		else
		{
			OMediaRect src(in.left - er.left, in.top - er.top, in.right - er.left, in.bottom - er.top);
			draw(&src, in.left, in.top, vp->getcanvas(), NULL);
		}
		return;
	}
none:
	if (drawn_rect) drawn_rect->set(0, 0, 0, 0);
}

OMediaCanvas *OMedia2DElement::getcanvas(void) { return NULL; }
OMediaCanvas *OMedia2DElement::get_collision_mask(void) { return NULL; }

// 0x10002020. Where it was drawn goes into the refresh lists of the viewports that use them.
void OMedia2DElement::presence_removed(void)
{
	if (!world || world->is_destroying() || !(presence.left < presence.right && presence.top < presence.bottom)) return;
	if (!world->get_redraw_all())
	{
		for (omt_ViewPortList::iterator v = world->get_viewports()->begin(); v != world->get_viewports()->end(); v++)
		{
			OMediaViewPort *vp = *v;
			OMedia2DVPModel *m = (OMedia2DVPModel*)vp->get_vpmodel();
			if (m->get_complexity() == omcrc_None) continue;
			const OMediaRect &B = vp->get_bounds();
			long ox = B.left - omf_ftol(vp->getx()), oy = B.top - omf_ftol(vp->gety());
			long l = presence.left + ox, t = presence.top + oy, r = presence.right + ox, b = presence.bottom + oy;
			if (B.top < b && B.left < r && t < B.bottom && l < B.right)
			{
				if (l < B.left) l = B.left;
				if (t < B.top) t = B.top;
				if (B.right <= r) r = B.right;
				if (B.bottom <= b) b = B.bottom;
				OMediaShortRect sr((short)l, (short)t, (short)r, (short)b);
				m->current_refresh()->newrect(&sr);
				OMediaShortRect sr2((short)l, (short)t, (short)r, (short)b);
				m->other_refresh()->newrect(&sr2);
			}
		}
	}
	presence.set(0, 0, 0, 0);
}

// 0x100021f0
void OMedia2DElement::hide(bool h)
{
	if (!h)
	{
		if (hide_count == 1) moved = true;
		hide_count--;
	}
	else if (++hide_count == 1) presence_removed();
	if (hide_sub) update_sub_presence(h);
}

// 0x10002240
void OMedia2DElement::update_sub_presence(bool hidden)
{
	for (omt_ElementList::iterator i = elements.begin(); i != elements.end(); i++)
	{
		OMedia2DElement *e = (OMedia2DElement*)*i;
		if (e->hide_count == 0)
		{
			if (!hidden) e->moved = true;
			else e->presence_removed();
			e->update_sub_presence(hidden);
		}
	}
}

// 0x10002290: sub-elements move with it
void OMedia2DElement::update_abs_position(void)
{
	if (!superelement)
	{
		absx = x; absy = y; absz = z;
	}
	else
	{
		absx = superelement->getabsoluteposx() + x;
		absy = superelement->getabsoluteposy() + y;
		absz = superelement->getabsoluteposz() + z;
	}
	for (omt_ElementList::iterator i = elements.begin(); i != elements.end(); i++)
	{
		OMedia2DElement *e = (OMedia2DElement*)*i;
		e->update_abs_position();
		e->moved = true;
	}
}

float OMedia2DElement::get_final_x(void) { return x; }		// 0x10002300
float OMedia2DElement::get_final_y(void) { return y; }		// 0x10002310

// 0x10002320. Inside its visual rect (and clip rect); with pixel clicks, not on a transparent
// pixel of the canvas.
bool OMedia2DElement::point_enclosed(OMediaViewPort *vp, short px, short py)
{
	if (!getvisible_extend()) return false;
	if (clipping && (px < clip_rect.left || py < clip_rect.top || px >= clip_rect.right || py >= clip_rect.bottom)) return false;
	OMediaRect r;
	get_visual_rect(vp, &r);
	if (r.left <= px && r.top <= py && px < r.right && py < r.bottom)
	{
		if (pixel_click)
		{
			OMediaCanvas *c = getcanvas();
			if (c && c->getgfxworld())
			{
				bool transparent = false;
				if (!c->getgfxworld()->getpixel((short)(px - r.left), (short)(py - r.top), NULL, &transparent, NULL) || transparent)
					return false;
			}
		}
		return true;
	}
	return false;
}

// 0x10002430
void OMedia2DElement::process_click(OMediaWorldClickInfo *ci)
{
	OMediaCanvas *c = getcanvas();
	if (point_enclosed(ci->viewport, ci->x, ci->y))
	{
		ci->element = this;
		ci->canvas = c;
		ci->type = c ? omcct_Canvas : omcct_Element;
	}
}

// 0x10002480. Pixel collision of the two collision masks where their rectangles overlap.
// port: OMT.dll compares 1-bit masks 32 pixels at a time (fcn.10002750); the port tests the
// masks' opaque pixels (alpha) one by one, which gives the same answer.
bool OMedia2DElement::collide(OMediaElement *other)
{
	OMedia2DElement *o = (OMedia2DElement*)other;
	OMediaCanvas *mo = o->get_collision_mask(), *mt = get_collision_mask();
	if (!mo || !mt || !mo->getgfxworld() || !mt->getgfxworld()) return false;
	OMediaGfxWorld *wo = mo->getgfxworld(), *wt = mt->getgfxworld();
	long ox = omf_ftol(o->get_final_x()), oy = omf_ftol(o->get_final_y());
	long tx = omf_ftol(get_final_x()), ty = omf_ftol(get_final_y());
	long ow = wo->getwidth(), oh = wo->getheight(), tw = wt->getwidth(), th = wt->getheight();
	if (!(ty < oy + oh && tx < ox + ow && oy < ty + th && ox < tx + tw)) return false;
	OMediaGfxPixels *po = wo->port_pixels(), *pt = wt->port_pixels();
	if (!po || !pt || !po->pix || !pt->pix) return false;
	long l = ox > tx ? ox : tx, t = oy > ty ? oy : ty;
	long r = (ox + ow) < (tx + tw) ? ox + ow : tx + tw, b = (oy + oh) < (ty + th) ? oy + oh : ty + th;
	wo->lock(); wt->lock();
	bool hit = false;
	for (long yy = t; yy < b && !hit; yy++)
		for (long xx = l; xx < r; xx++)
			if ((po->pix[(yy - oy) * po->width + (xx - ox)] >> 24) && (pt->pix[(yy - ty) * pt->width + (xx - tx)] >> 24)) { hit = true; break; }
	wo->unlock(); wt->unlock();
	return hit;
}
