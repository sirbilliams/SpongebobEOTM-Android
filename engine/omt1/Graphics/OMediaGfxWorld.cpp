/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaGfxWorld.h"
#include "OMediaStreamOperators.h"
#include "OMediaRLEPacker.h"
#include "OMediaEndianSupport.h"
#include "OMediaError.h"
#include "OMediaGLBackend.h"
#include <string.h>

omt_MipmapMode OMediaGfxWorld::mipmap_mode = omcmm_None;

// 0x10066a00
OMediaGfxWorld::OMediaGfxWorld()
{
	lock_count = 0;
	origin_y = 0;
	origin_x = 0;
	palette = NULL;
	alpha_channel = false;
	alpha_bits = 15;
	double_lines = false;
	pixel_format = omcpf_Unknown;
	field_40 = 0;
}

// 0x10066ae0
OMediaGfxWorld::~OMediaGfxWorld() {}

// 0x10067dd0
void OMediaGfxWorld::listen_to_message(omt_Message msg, void *param)
{
	if (msg == -201) process_palette_modified();			// omsg_PaletteModified
	else if (msg == -200)									// omsg_ReplacePalette
	{
		palette = (OMediaPalette*)param;
		// port: OMT.dll then adds this world as a listener of the new palette's broadcaster;
		// the port has no 8-bit palettes (all surfaces are 32-bit)
	}
}

void OMediaGfxWorld::create(short, short, OMediaGfxWorld *, OMediaRGBColor *, short) {}		// 0x10066b10
void OMediaGfxWorld::purge(void) { pixel_format = omcpf_Unknown; }						// 0x10066b20

// 0x103920c0. OMT.dll: GDI BitBlt of the whole source at (x,y) in dest coordinates.
void OMediaGfxWorld::draw_full(OMediaGfxWorld *dest, long x, long y, OMediaMaskWorld *)
{
	lock();
	dest->lock();
	OMediaRect r(0, 0, getwidth(), getheight());
	port_blit(this, dest, r, x - dest->origin_x, y - dest->origin_y, r.get_width(), r.get_height(), false);	// port: GDI BitBlt
	unlock();
	dest->unlock();
}

// 0x103923d0
void OMediaGfxWorld::draw(OMediaGfxWorld *dest, OMediaRect *dest_rect)
{
	OMediaRect src;
	long h = getheight();
	src.set(0, 0, getwidth(), h);
	draw(dest, &src, dest_rect);
}

// 0x10392270. Same size: BitBlt, otherwise StretchBlt.
void OMediaGfxWorld::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, OMediaRect *dest_rect)
{
	lock();
	dest->lock();
	port_blit(this, dest, *src_rect, dest_rect->left - dest->origin_x, dest_rect->top - dest->origin_y,
			  dest_rect->get_width(), dest_rect->get_height(), false);		// port: GDI BitBlt/StretchBlt
	unlock();
	dest->unlock();
}

// 0x10392190
void OMediaGfxWorld::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, long x, long y, OMediaMaskWorld *)
{
	lock();
	dest->lock();
	port_blit(this, dest, *src_rect, x - dest->origin_x, y - dest->origin_y,
			  src_rect->get_width(), src_rect->get_height(), false);			// port: GDI BitBlt
	unlock();
	dest->unlock();
}

// 0x10067c80. OMT.dll fills through an OMediaDrawPort (GDI); a NULL rect is the whole world.
void OMediaGfxWorld::fill(OMediaRGBColor *color, OMediaRect *rect)
{
	OMediaRect r;
	if (!rect) r.set(0, 0, getwidth(), getheight());
	else
	{
		r = *rect;
		if (double_lines) r.bottom = r.bottom * 2 - r.top;
	}
	r.offset(-origin_x, -origin_y);
	port_fill(this, r, 0xFF000000 | color->getrgb32bits());		// port: OMediaDrawPort::fill_rect
}

// 0x10392440 / 0x10392460 (OMT.dll: from the DIB header; a negative height is a top-down DIB)
long OMediaGfxWorld::getwidth(void) { OMediaGfxPixels *p = port_pixels(); return p ? p->width : 0; }
long OMediaGfxWorld::getheight(void) { OMediaGfxPixels *p = port_pixels(); return p ? p->height : 0; }

// 0x10392930. port: memory worlds are 32-bit.
short OMediaGfxWorld::getdepth(void) { return port_pixels() ? 32 : 0; }

// 0x10066c10
short OMediaGfxWorld::support_depth(short depth)
{
	if (depth == 24) return 32;
	if (depth == 2) depth = 4;
	return depth;
}

// 0x10068700. port: no 8/16-bit surfaces (OMT.dll asks support_depth(8)/(16) and the 565 mode)
bool OMediaGfxWorld::support_pixel_format(omt_PixelFormat pf)
{
	switch (pf)
	{
		case omcpf_8Bits: return support_depth(8) == 8;
		case omcpf_RGB555: case omcpf_RGB565: case omcpf_RGB565_Any: return support_depth(16) == 16;
		default: return false;
	}
}

// 0x10066b30
bool OMediaGfxWorld::is_format_compatible(OMediaGfxWorld *w)
{
	if (w->getdepth() != getdepth()) return false;
	omt_PixelFormat a = pixel_format, b = w->pixel_format;
	if (a == omcpf_8Bits)
	{
		if (b != omcpf_8Bits) return false;
		if (!palette) return w->palette == NULL;
		return w->palette != NULL && palette == w->palette;		// OMT.dll compares the palettes' ids (+0x9454)
	}
	if (a == b) return true;
	if ((a == omcpf_RGB555 || a == omcpf_RGB24) && (b == omcpf_RGB555 || b == omcpf_RGB24)) return true;
	if ((a == omcpf_RGB565_Any || a == omcpf_RGB32) && (b == omcpf_RGB565_Any || b == omcpf_RGB32)) return true;
	return false;
}

void OMediaGfxWorld::lock(void) { lock_count++; }				// 0x10392480
void OMediaGfxWorld::unlock(void) { lock_count--; }				// 0x10392490
void OMediaGfxWorld::pixlock(unsigned short) { lock(); }		// 0x10066be0
void OMediaGfxWorld::pixunlock(void) { unlock(); }				// 0x10066c00
void OMediaGfxWorld::dportlock(void) { lock(); }				// 0x10066bf0
void OMediaGfxWorld::dportunlock(void) { unlock(); }			// 0x10066c00

// 0x10066c30
void OMediaGfxWorld::force_unlock(void)
{
	if (lock_count)
	{
		lock_count = 1;
		unlock();
	}
}

OMediaRetarget *OMediaGfxWorld::getdrawretarget(void) { return NULL; }
unsigned long OMediaGfxWorld::getattributes(void) { return 0; }

// 0x103924a0
void OMediaGfxWorld::getpixmaplowleveldata(char **pixels, short *width, short *height, short *depth, long *rowbytes)
{
	OMediaGfxPixels *p = port_pixels();
	if (p && p->pix)
	{
		*pixels = (char*)p->pix;
		*width = (short)p->width;
		*height = (short)p->height;
		*rowbytes = p->width * 4;
		*depth = 32;
		return;
	}
	*pixels = NULL;
}

// 0x100684c0
bool OMediaGfxWorld::getpixel(short x, short y, OMediaRGBColor *rgb, bool *transparent, unsigned long *packed)
{
	char *pix; short w, h, depth; long rowbytes;
	unsigned int value = 0;

	pixlock();
	getpixmaplowleveldata(&pix, &w, &h, &depth, &rowbytes);
	if (!pix)
	{
		pixunlock();
		return false;
	}
	if (x < 0 || y < 0 || x >= w || y >= h)
	{
		pixunlock();
		return false;
	}
	value = *(unsigned int*)(pix + y * rowbytes + x * 4);		// port: always 32-bit (OMT.dll: 1/4/8/16/32)
	if (packed) *packed = value & 0xFFFFFF;
	if (rgb || transparent)
	{
		OMediaRGBColor c;
		c.setrgb32bits(value);
		if (rgb) *rgb = c;
		if (transparent)
		{
			OMediaRGBColor tc;
			if (get_transparent_color(&tc)) *transparent = (value >> 24) == 0;	// port: the key is alpha 0
			else *transparent = false;
		}
	}
	pixunlock();
	return true;
}

bool OMediaGfxWorld::get_transparent_color(OMediaRGBColor *) { return false; }
void OMediaGfxWorld::setpalette(OMediaPalette *pal) { palette = pal; }	// 0x10067d80 port: no 8-bit palettes

// 0x10067fb0
bool OMediaGfxWorld::set_alpha_channel(bool alpha, short bits)
{
	alpha_channel = alpha;
	alpha_bits = bits;
	return false;
}

bool OMediaGfxWorld::support_page_flipping(void) { return false; }
void OMediaGfxWorld::page_flip(bool) {}
bool OMediaGfxWorld::is_page_flip_done(void) { return true; }
bool OMediaGfxWorld::back_page_lost(void) { return false; }
void OMediaGfxWorld::back_to_front_copy(OMediaRefreshList *) {}		// 0x1005a390
bool OMediaGfxWorld::is_video_memory(void) { return false; }
void OMediaGfxWorld::restore(void) {}
OMediaGfxWorld *OMediaGfxWorld::get_direct_gfxworld(void) { return NULL; }

// 0x10066c50
unsigned long OMediaGfxWorld::get_approximate_size(void)
{
	unsigned long size = 0x4c;
	char *pix; short w, h, depth; long rowbytes;
	lock();
	getpixmaplowleveldata(&pix, &w, &h, &depth, &rowbytes);
	if (pix) size += h * rowbytes;
	unlock();
	return size;
}

void *OMediaGfxWorld::getdrawinfofromretarget(void) { return NULL; }	// 0x10392420 port: no GDI draw info
void OMediaGfxWorld::quick_remap8bits(OMediaPalette *) {}				// 0x10067e10 port: no 8-bit worlds
bool OMediaGfxWorld::quiet_dport(void) { return false; }
void OMediaGfxWorld::process_palette_modified(void) {}					// 0x103928d0 port: no 8-bit worlds
void *OMediaGfxWorld::getlowlevelport(void) { return NULL; }			// 0x10392510 port: no HDC


// 0x10066cb0. The 'OmGW' stream: header, transparent colour, optional palette, then RLE packed
// rows (8/16/32-bit units by 'compression'), each row prefixed by its packed size.
// port: OMT.dll remaps the image to the screen's depth/palette and to the world type the
// streamer remap mode asks for (through temporary OMediaPixBlitterWorlds); the port's memory
// worlds are always 32-bit, so the rows are decoded straight to 0xAARRGGBB with alpha 0 on the
// transparent colour. The world is created through its own create(), as OMT.dll does.
void OMediaGfxWorld::read_class(OMediaStreamOperators &stream)
{
	const unsigned int omc_GfxWorldType = 'OmGW';
	unsigned int hdr, transp_packed = 0xFFFFFFFF, bufsize;
	short width, height, depth, nbytes;
	char compression, version;
	bool dotransparent, dopalette;
	OMediaRGBColor transp_rgb;
	unsigned short pal[256][3];
	int pal_size = 0;

	stream >> hdr;
	if (hdr != omc_GfxWorldType) omd_EXCEPTION(omcerr_BadFormat);
	stream >> width >> height >> depth >> compression >> version >> nbytes;

	omt_RLEPackMode pmode;
	switch (compression)
	{
		case 0: pmode = omrlec_8Bits; break;
		case 1: pmode = omrlec_16Bits; break;
		case 2: pmode = omrlec_32Bits; break;
		default: omd_EXCEPTION(omcerr_BadFormat); return;
	}

	stream >> dotransparent;
	if (version == 0) stream >> transp_rgb.red >> transp_rgb.green >> transp_rgb.blue;
	else if (version > 0 && dotransparent) stream >> transp_packed;

	stream >> dopalette;
	if (dopalette)
	{
		// OMediaPalette's stream: 'OmPa' (old) or 'OPa2' + name + colour count + 16-bit guns,
		// 'OPa2' then a 16x16x16 inverse table
		unsigned int phdr, psize;
		string desc;
		stream >> phdr >> desc >> psize;
		if ((phdr != 'OmPa' && phdr != 'OPa2') || psize > 256) omd_EXCEPTION(omcerr_BadFormat);
		pal_size = (int)psize;
		for (int i = 0; i < pal_size; i++) stream >> pal[i][0] >> pal[i][1] >> pal[i][2];
		if (phdr == 'OPa2')
		{
			char *dum = new char[16 * 16 * 16];
			stream.read(dum, 16 * 16 * 16);
			delete [] dum;
		}
		if (pal_size <= 2)
		{
			pal[0][0] = pal[0][1] = pal[0][2] = 0xFFFF;
			pal[1][0] = pal[1][1] = pal[1][2] = 0;
		}
	}

	if (dotransparent && version == 0)
	{
		switch (depth)
		{
			case 1: case 4: case 8:
			for (int i = 0; i < pal_size; i++)
				if (pal[i][0] == transp_rgb.red && pal[i][1] == transp_rgb.green && pal[i][2] == transp_rgb.blue)
				{ transp_packed = i; break; }
			break;
			case 16: transp_packed = transp_rgb.getrgb16bits(); break;
			case 32: transp_packed = transp_rgb.getrgb32bits(); break;
			default: transp_packed = 0xFFFFFFFF; break;
		}
	}
	else if (!dotransparent) transp_packed = 0xFFFFFFFF;

	stream >> bufsize;
	char *buffer = new char[bufsize];
	stream.read(buffer, bufsize);
	char *rows = new char[nbytes * height];
	char *src = buffer, *dst = rows;
	for (int y = 0; y < height; y++)
	{
		short n = *(short*)src;
		n = omd_IfLittleEndianReverseShort(n);
		src += 2;
		if (!OMediaRLEPacker::rle_unpack(&src, &dst, n, nbytes, pmode))
		{
			delete [] buffer; delete [] rows;
			omd_EXCEPTION(omcerr_BadFormat);
		}
	}
	delete [] buffer;

	OMediaRGBColor tc;
	if (dotransparent)
	{
		if (version == 0) tc = transp_rgb;
		else if (depth == 16) tc.setrgb16bits((unsigned short)transp_packed);
		else if (depth == 32) tc.setrgb32bits(transp_packed);
		else if (transp_packed < 256) tc.set(pal[transp_packed][0], pal[transp_packed][1], pal[transp_packed][2]);
	}
	create(width, height, NULL, dotransparent ? &tc : NULL, 32);

	OMediaGfxPixels *px = port_pixels();
	if (px && px->pix && px->width == width && px->height == height)
	{
		pixlock();
		unsigned int *out = px->pix;
		for (int y = 0; y < height; y++)
		{
			unsigned char *row = (unsigned char*)rows + y * nbytes;
			for (int x = 0; x < width; x++)
			{
				unsigned int v = 0, rgb = 0;
				switch (depth)
				{
					case 1: v = (row[x >> 3] >> (7 - (x & 7))) & 1; break;
					case 4: v = row[x >> 1]; v = ((x & 1) ? (v >> 4) : v) & 15; break;
					case 8: v = row[x]; break;
					case 16: v = (row[x * 2] << 8) | row[x * 2 + 1]; break;		// stored big-endian
					case 32: v = ((unsigned int)row[x * 4] << 24) | (row[x * 4 + 1] << 16) | (row[x * 4 + 2] << 8) | row[x * 4 + 3]; break;
				}
				if (depth <= 8)
				{
					if ((int)v < pal_size) rgb = ((pal[v][0] >> 8) << 16) | ((pal[v][1] >> 8) << 8) | (pal[v][2] >> 8);
				}
				else if (depth == 16)
				{
					unsigned int r = (v >> 10) & 0x1f, g = (v >> 5) & 0x1f, b = v & 0x1f;
					rgb = (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
				}
				else rgb = v & 0xFFFFFF;
				*out++ = (v == transp_packed ? 0 : 0xFF000000) | rgb;
			}
		}
		px->version++;
		pixunlock();
	}
	delete [] rows;
}

// 0x10067760. port: worlds are not written back (the game never saves canvases)
void OMediaGfxWorld::write_class(OMediaStreamOperators &)
{
	omd_EXCEPTION(omcerr_BadFormat);
}


//----------------------------------------------------------------------------------------------
// port: the blit/fill back end. Memory to memory in software; anything touching a GL world
// goes through OMediaGLBackend.

void OMediaGfxWorld::port_blit(OMediaGfxWorld *src, OMediaGfxWorld *dest, const OMediaRect &src_rect,
							   long dx, long dy, long dw, long dh, bool keyed)
{
	if (dest->port_is_gl())
	{
		omgl_blit(src, dest, src_rect, dx, dy, dw, dh, keyed);
		return;
	}
	OMediaGfxPixels *s = src->port_pixels(), *d = dest->port_pixels();
	if (!d || !d->pix) return;
	if (src->port_is_gl())
	{
		omgl_read_into(src, src_rect, d, dx, dy, dw, dh);
		return;
	}
	if (!s || !s->pix) return;
	long sw = src_rect.get_width(), sh = src_rect.get_height();
	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
	for (long y = 0; y < dh; y++)
	{
		long ty = dy + y;
		if (ty < 0 || ty >= d->height) continue;
		long sy = src_rect.top + (dh == sh ? y : (y * sh) / dh);
		if (sy < 0 || sy >= s->height) continue;
		unsigned int *drow = d->pix + ty * d->width, *srow = s->pix + sy * s->width;
		for (long x = 0; x < dw; x++)
		{
			long tx = dx + x;
			if (tx < 0 || tx >= d->width) continue;
			long sx = src_rect.left + (dw == sw ? x : (x * sw) / dw);
			if (sx < 0 || sx >= s->width) continue;
			unsigned int p = srow[sx];
			if (keyed && !(p >> 24)) continue;
			drow[tx] = p;
		}
	}
	d->version++;
}

void OMediaGfxWorld::port_fill(OMediaGfxWorld *dest, const OMediaRect &r, unsigned int argb)
{
	if (dest->port_is_gl())
	{
		omgl_fill(dest, r, argb);
		return;
	}
	OMediaGfxPixels *d = dest->port_pixels();
	if (!d || !d->pix) return;
	long l = r.left < 0 ? 0 : r.left, t = r.top < 0 ? 0 : r.top;
	long rr = r.right > d->width ? d->width : r.right, b = r.bottom > d->height ? d->height : r.bottom;
	for (long y = t; y < b; y++)
		for (long x = l; x < rr; x++) d->pix[y * d->width + x] = argb;
	d->version++;
}
