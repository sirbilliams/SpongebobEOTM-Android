/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaBackground.h"
#include "OMediaWorld.h"
#include "OMediaCanvas.h"
#include "OMediaRefreshList.h"

OMediaBackground::OMediaBackground() { world = NULL; unicolor = false; }
OMediaBackground::OMediaBackground(OMediaWorld *w) { world = NULL; unicolor = false; w->setbackground(this); }
OMediaBackground::~OMediaBackground() { if (world) world->background = NULL; }
void OMediaBackground::draw(OMediaCanvas *, OMediaRect *, OMediaRefreshList *, OMediaViewPort *) {}
long OMediaBackground::getmaxwidth(void) { return 0; }
long OMediaBackground::getmaxheight(void) { return 0; }

OMediaColorBackground::OMediaColorBackground(OMediaWorld *w) : OMediaBackground(w) { color.set(0, 0, 0); unicolor = true; }
OMediaColorBackground::OMediaColorBackground() { color.set(0, 0, 0); unicolor = true; }
OMediaColorBackground::~OMediaColorBackground() {}

// 0x1003e3e0
void OMediaColorBackground::setcolor(const OMediaRGBColor *c)
{
	color = *c;
	if (world) world->background_changed();
}

// 0x1003e410: fills the bounds, or each rectangle of the refresh list
void OMediaColorBackground::draw(OMediaCanvas *canvas, OMediaRect *bounds, OMediaRefreshList *rl, OMediaViewPort *)
{
	OMediaGfxWorld *g = canvas->getdefaultdrawgfxworld();
	if (!rl)
	{
		g->fill(&color, bounds);
		return;
	}
	for (omt_ShortRectList::iterator i = rl->get_list()->begin(); i != rl->get_list()->end(); i++)
	{
		OMediaRect r((*i).left, (*i).top, (*i).right, (*i).bottom);
		g->fill(&color, &r);
	}
}

// 0x1003e4a0
void OMediaColorBackground::get_unicolor_packed(unsigned int &argb)
{
	argb = ((unsigned int)(color.red >> 8) << 16) | ((unsigned int)(color.green >> 8) << 8) | (color.blue >> 8);
}
