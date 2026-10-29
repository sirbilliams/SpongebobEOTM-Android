/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMediaCCaption: a 2D element showing a string with an OMediaQuickText font. Its size is the
    string's size in that font. Layout in OMT.dll: +0xc4 string, +0xd4 font, +0xd8 width, +0xda height. */
#pragma once
#ifndef OMEDIA1_CCaption_H
#define OMEDIA1_CCaption_H

#include "OMedia2DElement.h"
#include <string>

class OMediaQuickText;

class OMediaCCaption : public OMedia2DElement
{
	public:
	omtshared OMediaCCaption();						// 0x1003dc10
	omtshared virtual ~OMediaCCaption();				// 0x1003dc80

	omtshared virtual void getsize(long &w, long &h);			// 0x1003dcf0  vt+0x5c
	using OMedia2DElement::draw;
	omtshared virtual void draw(OMediaRect *src, long x, long y, OMediaCanvas *dest, OMediaRefreshList *rl);	// 0x1003dd10  vt+0x74

	// (sb2.exe sets these through inline code: the string, then compute_size)
	void set_string(const string &s);
	void set_font(OMediaQuickText *f);
	inline const string &get_string(void) const {return text;}
	inline OMediaQuickText *get_font(void) const {return font;}

	protected:
	omtshared virtual void compute_size(void);		// 0x1003ded0

	string				text;		// +0xc4
	OMediaQuickText		*font;		// +0xd4
	short				width;		// +0xd8
	short				height;		// +0xda
};

#endif
