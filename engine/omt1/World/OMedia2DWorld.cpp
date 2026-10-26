/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia2DWorld.h"
#include "OMediaBackground.h"
#include "OMediaCanvas.h"
#include "OMediaDisplay.h"

static inline long omf_ftol(float f) { return (long)f; }

//------------------------------------------------------------------------------ OMedia2DWorld

OMedia2DWorld::OMedia2DWorld() : OMedia2DElementSorter(&elements) {}		// 0x10004380
OMedia2DWorld::~OMedia2DWorld() {}											// 0x10004410

// 0x10004480
OMediaVPModel *OMedia2DWorld::create_vp_model(OMediaViewPort *vp) { return new OMedia2DVPModel(this, vp); }

void OMedia2DWorld::sort(void) { OMedia2DElementSorter::sort(); }			// 0x100044f0

// 0x10004500
void OMedia2DWorld::set_refreshcomplexity(omt_RefreshComplexity c)
{
	for (omt_ViewPortList::iterator v = viewports.begin(); v != viewports.end(); v++)
	{
		OMedia2DVPModel *m = (OMedia2DVPModel*)(*v)->get_vpmodel();
		if (m->complexity != c) m->full_redraw = true;
		m->complexity = c;
	}
}

//------------------------------------------------------------------------------ OMedia2DVPModel

// 0x10002f20
OMedia2DVPModel::OMedia2DVPModel(OMediaWorld *w, OMediaViewPort *vp) : OMediaVPModel(w, vp)
{
	complexity = omcrc_Low;
	other = 1;
	current = 0;
}

OMedia2DVPModel::~OMedia2DVPModel() {}		// 0x10002fc0

void OMedia2DVPModel::inform_vpchanged(void) { full_redraw = true; }	// 0x10004030

// 0x10003ff0 / 0x10004010: the background's size
long OMedia2DVPModel::getmaxvpwidth(void) { return world->getbackground() ? world->getbackground()->getmaxwidth() : 0; }
long OMedia2DVPModel::getmaxvpheight(void) { return world->getbackground() ? world->getbackground()->getmaxheight() : 0; }

// 0x10003020. A pure page-flipping canvas cannot keep refresh lists: complexity 0 for this frame
// (unless the screen device copies back to front); waits for the previous flip.
void OMedia2DVPModel::render(void)
{
	OMediaCanvas *c = viewport->getcanvas();
	if (!c) return;
	omt_RefreshComplexity saved = complexity;
	OMediaGfxWorld *g = c->getdefaultdrawgfxworld();
	if (complexity != omcrc_None && c->support_page_flipping())
	{
		OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
		if (!dev->get_pageflipping_finemode() && !dev->get_mode_flag()) complexity = omcrc_None;
		else if (g->back_page_lost()) full_redraw = true;
	}
	while (!g->is_page_flip_done()) ;
	g->lock();
	if (world->get_redraw_all()) full_redraw = true;
	if (complexity < omcrc_High) render_lowcomplexity();
	else render_highcomplexity();
	c->getdefaultdrawgfxworld()->unlock();
	complexity = saved;
}

// 0x100030f0
void OMedia2DVPModel::render_lowcomplexity(void)
{
	OMediaBackground *bg = world->getbackground();
	OMediaDoubleCanvas *dc = viewport->getdoublecanvas();
	if (complexity == omcrc_None || (dc && dc->get_full_refresh())) full_redraw = true;
	OMediaRect bounds = viewport->get_bounds();
	OMediaGfxWorld *g = viewport->getcanvas()->getdefaultdrawgfxworld();
	g->lock();
	if (bg) bg->draw(viewport->getcanvas(), &bounds, full_redraw ? NULL : &refresh[current], viewport);
	if (complexity != omcrc_None)
	{
		if (dc)
		{
			if (current == 0) { current = 1; other = 0; }
			else { current = 0; other = 1; }
		}
		refresh[current].clear();
		if (full_redraw) refresh[other].clear();
	}
	bool first_vp = viewport->get_node() == world->get_viewports()->begin();
	render_elements_lowcomplexity(world->get_element_list()->begin(), world->get_element_list()->end(), false, first_vp);
	viewport->getcanvas()->getdefaultdrawgfxworld()->unlock();
	if (dc) world->add_new_swapcanvas(dc, &bounds, full_redraw ? NULL : &refresh[other], dc->support_page_flipping(), false);
	full_redraw = false;
}

// 0x10003460. port: the dirty-rectangle renderer (prepare_dirty_list, spread_dirty,
// spread_dirty_fine, highrender_push_refresh_rect: 0x10003890..0x10003f20) is not rebuilt yet;
// the GL back buffer keeps its pixels, so drawing everything gives the same picture.
void OMedia2DVPModel::render_highcomplexity(void)
{
	full_redraw = true;
	render_lowcomplexity();
}

// 0x100032a0. Elements before 'last' (stopping at the first one with z >= 0 when asked):
// sub-elements behind, the element, sub-elements in front; the first viewport also takes the
// elements' presence rectangles.
omt_ElementList::iterator OMedia2DVPModel::render_elements_lowcomplexity(omt_ElementList::iterator first,
		omt_ElementList::iterator last, bool stop_at_front, bool first_viewport)
{
	for (; first != last; first++)
	{
		OMedia2DElement *e = (OMedia2DElement*)*first;
		if (stop_at_front && e->getz() >= 0.0f) return first;
		bool branch = !e->hide_sub || e->hide_count == 0;
		omt_ElementList::iterator front = e->elements.begin();
		if (e->elements.size() && branch)
		{
			e->OMedia2DElementSorter::sort();
			front = render_elements_lowcomplexity(e->elements.begin(), e->elements.end(), true, first_viewport);
		}
		OMediaRect drawn;
		drawn.set(0, 0, 0, 0);
		if (e->hide_count == 0)
		{
			e->draw(viewport, NULL, &drawn);
			if (first_viewport)
			{
				e->moved = false;
				e->get_world_absrect(&e->presence);
			}
		}
		else if (first_viewport) e->get_world_absrect(&e->presence);
		if (complexity != omcrc_None && e->hide_count == 0)
		{
			OMediaShortRect r((short)drawn.left, (short)drawn.top, (short)drawn.right, (short)drawn.bottom);
			if (viewport->getdoublecanvas()) refresh[other].newrect(&r);
			OMediaShortRect r2((short)drawn.left, (short)drawn.top, (short)drawn.right, (short)drawn.bottom);
			refresh[current].newrect(&r2);
		}
		if (e->elements.size() && branch)
			render_elements_lowcomplexity(front, e->elements.end(), false, first_viewport);
	}
	return first;
}

// 0x10004180: the elements in draw order
omt_ElementList::iterator OMedia2DVPModel::create_flat_list(omt_ElementList::iterator first, omt_ElementList::iterator last,
		bool stop_at_front, vector<OMediaElement*> &list)
{
	for (; first != last; first++)
	{
		OMedia2DElement *e = (OMedia2DElement*)*first;
		if (stop_at_front && e->getz() >= 0.0f) break;
		bool branch = !e->hide_sub || e->hide_count == 0;
		omt_ElementList::iterator front = e->elements.begin();
		if (e->elements.size() && branch)
		{
			e->OMedia2DElementSorter::sort();
			front = create_flat_list(e->elements.begin(), e->elements.end(), true, list);
		}
		list.push_back(e);
		if (e->elements.size() && branch) create_flat_list(front, e->elements.end(), false, list);
	}
	return first;
}

// 0x10004040. Front-most element first; nothing found is the background (or nothing).
void OMedia2DVPModel::test_click(OMediaWorldClickInfo *ci)
{
	const OMediaRect &B = viewport->get_bounds();
	short ox = ci->x, oy = ci->y;
	if (ox < B.left || oy < B.top || ox >= B.right || oy >= B.bottom) return;
	ci->x = (short)(ox - B.left);
	ci->y = (short)(oy - B.top);
	ci->viewport = viewport;
	vector<OMediaElement*> list;
	create_flat_list(world->get_element_list()->begin(), world->get_element_list()->end(), false, list);
	for (size_t i = list.size(); i > 0; i--)
	{
		OMediaElement *e = list[i - 1];
		if (e->get_clickable()) e->process_click(ci);
		if (ci->type != omcct_Nothing) return;
	}
	if (ci->type == omcct_Nothing)
	{
		if (world->getbackground()) ci->type = omcct_Background;
		else
		{
			ci->viewport = NULL;
			ci->x = ox;
			ci->y = oy;
		}
	}
}
