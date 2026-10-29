/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaRGBColor.h"

// 0x10085080
unsigned int OMediaRGBColor::getrgb32bits(void) const
{
	return ((((unsigned int)red * 0xff) / 0xffff) << 16) | ((((unsigned int)green * 0xff) / 0xffff) << 8)
		 | (((unsigned int)blue * 0xff) / 0xffff);
}

// 0x100850e0
void OMediaRGBColor::setrgb32bits(unsigned int c)
{
	red = (unsigned short)((((c >> 16) & 0xff) * 0xffff) / 0xff);
	green = (unsigned short)((((c >> 8) & 0xff) * 0xffff) / 0xff);
	blue = (unsigned short)(((c & 0xff) * 0xffff) / 0xff);
}

// 0x10085150
unsigned int OMediaRGBColor::getbgr32bits(void) const
{
	return ((((unsigned int)blue * 0xff) / 0xffff) << 16) | ((((unsigned int)green * 0xff) / 0xffff) << 8)
		 | (((unsigned int)red * 0xff) / 0xffff);
}

// 0x100851b0
void OMediaRGBColor::setbgr32bits(unsigned int c)
{
	red = (unsigned short)(((c & 0xff) * 0xffff) / 0xff);
	green = (unsigned short)((((c >> 8) & 0xff) * 0xffff) / 0xff);
	blue = (unsigned short)((((c >> 16) & 0xff) * 0xffff) / 0xff);
}

unsigned char OMediaRGBColor::getred8bits(void) const { return (unsigned char)(((unsigned int)red * 0xff) / 0xffff); }		// 0x10085220
unsigned char OMediaRGBColor::getgreen8bits(void) const { return (unsigned char)(((unsigned int)green * 0xff) / 0xffff); }	// 0x10085240
unsigned char OMediaRGBColor::getblue8bits(void) const { return (unsigned char)(((unsigned int)blue * 0xff) / 0xffff); }		// 0x10085260
void OMediaRGBColor::setred8bits(unsigned char c) { red = (unsigned short)(((unsigned int)c * 0xffff) / 0xff); }			// 0x10085280
void OMediaRGBColor::setgreen8bits(unsigned char c) { green = (unsigned short)(((unsigned int)c * 0xffff) / 0xff); }		// 0x100852a0
void OMediaRGBColor::setblue8bits(unsigned char c) { blue = (unsigned short)(((unsigned int)c * 0xffff) / 0xff); }		// 0x100852d0

// 0x10085300
unsigned short OMediaRGBColor::getrgb16bits(void) const
{
	return (unsigned short)(((((unsigned int)red * 0x1f) / 0xffff) << 10) | ((((unsigned int)green * 0x1f) / 0xffff) << 5)
		 | (((unsigned int)blue * 0x1f) / 0xffff));
}

// 0x10085370
void OMediaRGBColor::setrgb16bits(unsigned short c)
{
	red = (unsigned short)((((c >> 10) & 0x1f) * 0xffff) / 0x1f);
	green = (unsigned short)((((c >> 5) & 0x1f) * 0xffff) / 0x1f);
	blue = (unsigned short)(((c & 0x1f) * 0xffff) / 0x1f);
}

// 0x100853e0
unsigned short OMediaRGBColor::getbgr16bits(void) const
{
	return (unsigned short)(((((unsigned int)blue * 0x1f) / 0xffff) << 10) | ((((unsigned int)green * 0x1f) / 0xffff) << 5)
		 | (((unsigned int)red * 0x1f) / 0xffff));
}

// 0x10085450
void OMediaRGBColor::setbgr16bits(unsigned short c)
{
	red = (unsigned short)(((c & 0x1f) * 0xffff) / 0x1f);
	green = (unsigned short)((((c >> 5) & 0x1f) * 0xffff) / 0x1f);
	blue = (unsigned short)((((c >> 10) & 0x1f) * 0xffff) / 0x1f);
}
