/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaBoxElement.h"
#include "OMediaDrawPort.h"

// 0x1003ad60
OMediaBoxElement::OMediaBoxElement()
{
	height = 0x20;
	width = 0x20;
	light.set(0xffff, 0xffff, 0xffff);
	dark.set(0x6666, 0x6666, 0x6666);
	color.set(0xaaaa, 0xaaaa, 0xaaaa);
	bevel = false;
	filled = true;
	sunken = false;			// (not set by OMT.dll; the drawing treats any value as given)
}

OMediaBoxElement::~OMediaBoxElement() {}		// 0x1003ae20

// 0x1003ae40
void OMediaBoxElement::getsize(long &w, long &h)
{
	w = width < 0 ? 0 : width;
	h = height < 0 ? 0 : height;
}

// 0x1003ae80
void OMediaBoxElement::draw_full(OMediaGfxWorld *w, long x, long y)
{
	if (width <= 0 || height <= 0) return;
	OMediaDrawPort dp;
	long r = width + x, b = height + y;
	dp.setgfxworld(w);
	dp.lock();
	if (!bevel)
	{
		dp.setpencolor(color.red, color.green, color.blue);
		if (!filled) dp.framerect(x, y, r, b);
		else dp.paintrect(x, y, r, b);
	}
	else if (!filled) dp.framerect3D(x, y, r, b, !sunken, &dark, &light);
	else dp.paintrect3D(x, y, r, b, !sunken, &dark, &light, &color);
	dp.unlock();
}
