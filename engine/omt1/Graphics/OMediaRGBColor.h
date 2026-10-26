/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code for the SpongeBob
    port. LGPL like OMT.  OMediaRGBColor: 16 bits per gun (red +0, green +2, blue +4).          */
#pragma once
#ifndef OMEDIA1_RGBColor_H
#define OMEDIA1_RGBColor_H

#include "OMediaTypes.h"

class OMediaRGBColor
{
	public:
	inline OMediaRGBColor() {}
	inline OMediaRGBColor(unsigned short r, unsigned short g, unsigned short b) {red = r; green = g; blue = b;}

	inline void set(unsigned short r, unsigned short g, unsigned short b) {red = r; green = g; blue = b;}

	omtshared unsigned int getrgb32bits(void) const;		// 0x10085080  0x00RRGGBB
	omtshared void setrgb32bits(unsigned int c);			// 0x100850e0
	omtshared unsigned int getbgr32bits(void) const;		// 0x10085150  0x00BBGGRR
	omtshared void setbgr32bits(unsigned int c);			// 0x100851b0
	omtshared unsigned char getred8bits(void) const;		// 0x10085220
	omtshared unsigned char getgreen8bits(void) const;	// 0x10085240
	omtshared unsigned char getblue8bits(void) const;		// 0x10085260
	omtshared void setred8bits(unsigned char c);			// 0x10085280
	omtshared void setgreen8bits(unsigned char c);		// 0x100852a0
	omtshared void setblue8bits(unsigned char c);			// 0x100852d0
	omtshared unsigned short getrgb16bits(void) const;	// 0x10085300  x555
	omtshared void setrgb16bits(unsigned short c);		// 0x10085370
	omtshared unsigned short getbgr16bits(void) const;	// 0x100853e0
	omtshared void setbgr16bits(unsigned short c);		// 0x10085450

	inline bool operator==(const OMediaRGBColor &c) const {return red == c.red && green == c.green && blue == c.blue;}
	inline bool operator!=(const OMediaRGBColor &c) const {return !(*this == c);}

	unsigned short	red, green, blue;
};

#endif
