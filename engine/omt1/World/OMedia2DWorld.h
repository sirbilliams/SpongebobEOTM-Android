/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMedia2DWorld: a world of 2D elements, sorted (OMedia2DElementSorter at +0x7c).
    OMedia2DVPModel: draws a 2D world's viewport: background, then the elements in list order
    (sub-elements with z < 0 before their element, the others after). With a refresh complexity
    above 0 it keeps two refresh lists (this frame's and the previous one's, for double canvases)
    of the rectangles that changed.                                                             */
#pragma once
#ifndef OMEDIA1_2DWorld_H
#define OMEDIA1_2DWorld_H

#include "OMediaWorld.h"
#include "OMediaViewPort.h"
#include "OMedia2DElement.h"
#include "OMediaRefreshList.h"
#include <vector>

enum omt_RefreshComplexity
{
	omcrc_None = 0,		// redraw the whole viewport every frame
	omcrc_Low = 1,		// redraw everything, keep the changed rectangles for the swap
	omcrc_High = 2,		// redraw only what changed
	omcrc_Higher = 3
};

class OMedia2DWorld : public OMediaWorld, public OMedia2DElementSorter
{
	public:
	omtshared OMedia2DWorld();				// 0x10004380
	omtshared virtual ~OMedia2DWorld();		// 0x10004410

	omtshared virtual OMediaVPModel *create_vp_model(OMediaViewPort *vp);		// 0x10004480
	omtshared virtual void sort(void);											// 0x100044f0
	omtshared virtual void set_refreshcomplexity(omt_RefreshComplexity c);		// 0x10004500  vt+0x44
};

class OMedia2DVPModel : public OMediaVPModel
{
	public:
	omtshared OMedia2DVPModel(OMediaWorld *world, OMediaViewPort *vp);	// 0x10002f20
	omtshared virtual ~OMedia2DVPModel();								// 0x10002fc0

	omtshared virtual void render(void);					// 0x10003020
	omtshared virtual void inform_vpchanged(void);			// 0x10004030
	omtshared virtual long getmaxvpwidth(void);			// 0x10003ff0
	omtshared virtual long getmaxvpheight(void);			// 0x10004010
	omtshared virtual void test_click(OMediaWorldClickInfo *ci);	// 0x10004040

	inline omt_RefreshComplexity get_complexity(void) const {return complexity;}
	inline void set_complexity(omt_RefreshComplexity c) {complexity = c;}
	inline OMediaRefreshList *current_refresh(void) {return &refresh[current];}
	inline OMediaRefreshList *other_refresh(void) {return &refresh[other];}

	protected:
	omtshared virtual void render_lowcomplexity(void);		// 0x100030f0  vt+0x1c
	omtshared virtual void render_highcomplexity(void);		// 0x10003460  vt+0x20
	omtshared virtual omt_ElementList::iterator render_elements_lowcomplexity(omt_ElementList::iterator first,
							omt_ElementList::iterator last, bool stop_at_front, bool first_viewport);	// 0x100032a0  vt+0x24
	omtshared virtual omt_ElementList::iterator create_flat_list(omt_ElementList::iterator first,
							omt_ElementList::iterator last, bool stop_at_front, vector<OMediaElement*> &list);	// 0x10004180  vt+0x2c

	friend class OMedia2DWorld;
	short					current;		// +0x18
	short					other;			// +0x1a
	omt_RefreshComplexity	complexity;		// +0x1c
	OMediaRefreshList		refresh[2];		// +0x20
};

#endif
