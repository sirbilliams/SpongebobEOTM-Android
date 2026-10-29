/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    The world's background, drawn by the 2D viewport model before the elements.                */
#pragma once
#ifndef OMEDIA1_Background_H
#define OMEDIA1_Background_H

#include "OMediaRGBColor.h"
#include "OMediaRect.h"

class OMediaWorld;
class OMediaCanvas;
class OMediaRefreshList;
class OMediaViewPort;

class OMediaBackground
{
	public:
	omtshared OMediaBackground();					// 0x10037d90
	omtshared OMediaBackground(OMediaWorld *world);	// 0x10037dd0  world->setbackground(this)
	omtshared virtual ~OMediaBackground();			// 0x10037df0

	omtshared virtual void draw(OMediaCanvas *canvas, OMediaRect *bounds, OMediaRefreshList *rl, OMediaViewPort *vp);	// 0x10037e20  vt+0x04
	omtshared virtual long getmaxwidth(void);		// vt+0x08 (0)
	omtshared virtual long getmaxheight(void);		// vt+0x0c (0)

	protected:
	friend class OMediaWorld;
	OMediaWorld		*world;			// +0x04
	bool			unicolor;		// +0x08
};

class OMediaColorBackground : public OMediaBackground
{
	public:
	omtshared OMediaColorBackground(OMediaWorld *world);	// 0x1003e350
	omtshared OMediaColorBackground();						// 0x1003e3a0
	omtshared virtual ~OMediaColorBackground();				// 0x1003e3d0

	omtshared virtual void draw(OMediaCanvas *canvas, OMediaRect *bounds, OMediaRefreshList *rl, OMediaViewPort *vp);	// 0x1003e410
	omtshared virtual void get_unicolor_packed(unsigned int &argb);			// 0x1003e4a0  vt+0x10
	omtshared virtual void setcolor(const OMediaRGBColor *c);				// 0x1003e3e0  vt+0x14

	inline void getcolor(OMediaRGBColor &c) const {c = color;}

	protected:
	OMediaRGBColor	color;			// +0x0c
};

#endif
