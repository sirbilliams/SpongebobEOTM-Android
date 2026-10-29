/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMediaRefreshList: the dirty rectangles of a frame (merged when they overlap).              */
#pragma once
#ifndef OMEDIA1_RefreshList_H
#define OMEDIA1_RefreshList_H

#include "OMediaRect.h"
#include <list>

typedef list<OMediaShortRect> omt_ShortRectList;

class OMediaRefreshList
{
	public:
	omtshared OMediaRefreshList();					// 0x10084b30
	omtshared virtual ~OMediaRefreshList();			// 0x10084b80

	omtshared virtual bool touchrect(OMediaShortRect *r);									// 0x10084d70  vt+0x04
	omtshared virtual void newrect(OMediaShortRect *r);										// 0x10084be0  vt+0x08
	omtshared virtual void clipped_copy(OMediaRefreshList *src, OMediaShortRect *clip);	// 0x10084dc0  vt+0x0c
	omtshared virtual void copy_or(OMediaRefreshList *src);								// 0x10084f00  vt+0x10
	omtshared virtual void copy_or_offset(OMediaRefreshList *src, short dx, short dy);		// 0x10084f70  vt+0x14

	inline omt_ShortRectList *get_list(void) {return &rects;}
	inline void clear(void) {rects.clear();}

	protected:
	omt_ShortRectList	rects;		// +0x04
};

#endif
