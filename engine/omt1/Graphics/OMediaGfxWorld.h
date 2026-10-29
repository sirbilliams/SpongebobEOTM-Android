/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code for the SpongeBob
    port. LGPL like OMT.

    OMediaGfxWorld: a pixel surface. OMT 1.x draws 2D by asking the *source* world to draw itself
    into a destination world (src->draw(dest, ...)). Concrete worlds: OMediaPixmapWorld (memory),
    OMediaPixBlitterWorld (memory + transparent colour: every 2D canvas of the game's .omt files),
    OMediaDXSurfaceWorld (DirectDraw surface: the screen), OMediaWindowPortWorld (the display).

    port: OMT.dll keeps pixels in GDI DIB sections and DirectDraw surfaces (OMediaWinRtgDrawInfo,
    BitBlt, Blt). Here a memory world keeps 32-bit pixels (OMediaGfxPixels, 0xAARRGGBB words, alpha
    0 = transparent colour key) and the screen is an OpenGL surface (engine/sdl/OMediaSDLGfx.cpp).
    A blit from a memory world into a GL world draws the source as a cached texture. The virtual
    methods keep OMT.dll's vtable order; the comment on each gives its vtable offset.            */
#pragma once
#ifndef OMEDIA1_GfxWorld_H
#define OMEDIA1_GfxWorld_H

#include "OMediaListener.h"
#include "OMediaClassStreamer.h"
#include "OMediaRect.h"
#include "OMediaRGBColor.h"

class OMediaMaskWorld;
class OMediaPalette;
class OMediaRefreshList;
class OMediaRetarget;
class OMediaStreamOperators;

// Attributes (getattributes, OMediaGfxWorldFactory::createbestgfxworld). Names follow what the
// bits select in OMT.dll's factory; the game's canvases ask for 0x22d (2D) or 0x100c (textures).
enum
{
	omcgwa_Draw			= 0x0001,
	omcgwa_Blit			= 0x0002,
	omcgwa_Pixels		= 0x0004,
	omcgwa_Fill			= 0x0008,
	omcgwa_Compiled		= 0x0010,
	omcgwa_Transparent	= 0x0020,
	omcgwa_Alpha		= 0x0040,		// createbestgfxworld retries without it
	omcgwa_Mask			= 0x0080,
	omcgwa_DeepMask		= 0x0100,
	omcgwa_Stream		= 0x0200,
	omcgwa_Texture		= 0x1000,		// OMediaGfxWorldFactory::create_texture
	omcgwa_Mipmap1		= 0x8000,
	omcgwa_Mipmap2		= 0x10000,
	omcgwa_NoMipmap		= 0x20000
};

enum omt_PixelFormat
{
	omcpf_Unknown = 0, omcpf_8Bits = 1, omcpf_RGB555 = 2, omcpf_RGB24 = 3, omcpf_RGB565 = 4,
	omcpf_RGB565_Any = 5, omcpf_RGB32 = 6
};

enum omt_MipmapMode { omcmm_None = 0, omcmm_Mipmap = 1, omcmm_Trilinear = 2 };

// port: the pixels of a memory world (OMT.dll: a DIB section behind the retarget)
struct OMediaGfxPixels
{
	unsigned int	*pix;		// width*height words 0xAARRGGBB, alpha 0 where the transparent colour is
	int				width, height;
	unsigned int	version;	// bumped on every change, so a GL texture copy knows it is stale
	unsigned int	gl_tex, gl_version, gl_w, gl_h;
	// port: 3D repeats UVs; own its separate GL copy so destruction invalidates it.
	unsigned int	gl_3d_tex, gl_3d_version, gl_3d_w, gl_3d_h;
	unsigned int	gl_epoch, gl_3d_epoch;		// lane c: GL context generation of gl_tex / gl_3d_tex (omgl_context_reset)
};

class OMediaGfxWorld : public OMediaListener, public virtual OMediaClassStreamer
{
	public:

	omtshared OMediaGfxWorld();						// 0x10066a00
	omtshared virtual ~OMediaGfxWorld();				// 0x10066ae0  vt+0x04

	omtshared virtual void listen_to_message(omt_Message msg, void *param = NULL);	// 0x10067dd0  vt+0x00

	omtshared virtual void create(short width, short height, OMediaGfxWorld *model = NULL,
								  OMediaRGBColor *transparent = NULL, short depth = 0);	// 0x10066b10  vt+0x0c
	omtshared virtual void purge(void);										// 0x10066b20  vt+0x10

	// Draw this world into 'dest'
	omtshared virtual void draw_full(OMediaGfxWorld *dest, long x, long y, OMediaMaskWorld *mask = NULL);	// 0x103920c0  vt+0x14
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *dest_rect);								// 0x103923d0  vt+0x18
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, OMediaRect *dest_rect);		// 0x10392270  vt+0x1c
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, long x, long y,
								OMediaMaskWorld *mask = NULL);										// 0x10392190  vt+0x20
	omtshared virtual void fill(OMediaRGBColor *color, OMediaRect *rect = NULL);							// 0x10067c80  vt+0x24

	omtshared virtual long getwidth(void);				// 0x10392440  vt+0x28
	omtshared virtual long getheight(void);				// 0x10392460  vt+0x2c
	omtshared virtual short getdepth(void);				// 0x10392930  vt+0x30
	omtshared virtual short support_depth(short depth);	// 0x10066c10  vt+0x34
	omtshared virtual bool support_pixel_format(omt_PixelFormat pf);	// 0x10068700  vt+0x38
	omtshared virtual bool is_format_compatible(OMediaGfxWorld *w);	// 0x10066b30  vt+0x3c

	omtshared virtual void lock(void);					// 0x10392480  vt+0x40
	omtshared virtual void unlock(void);				// 0x10392490  vt+0x44
	omtshared virtual void pixlock(unsigned short flags = 0);	// 0x10066be0  vt+0x48
	omtshared virtual void pixunlock(void);				// 0x10066c00  vt+0x4c
	omtshared virtual void dportlock(void);				// 0x10066bf0  vt+0x50
	omtshared virtual void dportunlock(void);			// 0x10066c00  vt+0x54
	omtshared virtual void force_unlock(void);			// 0x10066c30  vt+0x58
	omtshared virtual OMediaRetarget *getdrawretarget(void);	// vt+0x5c (base: NULL)
	omtshared virtual unsigned long getattributes(void);		// vt+0x60
	omtshared virtual void getpixmaplowleveldata(char **pixels, short *width, short *height,
												 short *depth, long *rowbytes);	// 0x103924a0  vt+0x64
	omtshared virtual bool getpixel(short x, short y, OMediaRGBColor *rgb, bool *transparent = NULL,
									unsigned long *packed = NULL);			// 0x100684c0  vt+0x68
	omtshared virtual bool get_transparent_color(OMediaRGBColor *rgb);		// vt+0x6c (base: false)
	omtshared virtual void setpalette(OMediaPalette *palette);				// 0x10067d80  vt+0x70
	omtshared virtual bool set_alpha_channel(bool alpha, short bits);		// 0x10067fb0  vt+0x74
	omtshared virtual bool support_page_flipping(void);					// vt+0x78 (base: false)
	omtshared virtual void page_flip(bool wait);							// vt+0x7c
	omtshared virtual bool is_page_flip_done(void);						// vt+0x80 (base: true)
	omtshared virtual bool back_page_lost(void);							// vt+0x84 (base: false)
	omtshared virtual void back_to_front_copy(OMediaRefreshList *rl);		// 0x1005a390  vt+0x88
	omtshared virtual bool is_video_memory(void);							// vt+0x8c (base: false)
	omtshared virtual void restore(void);									// vt+0x90 (base: nothing; name not exported)
	omtshared virtual OMediaGfxWorld *get_direct_gfxworld(void);			// vt+0x94 (base: NULL)
	omtshared virtual unsigned long get_approximate_size(void);			// 0x10066c50  vt+0x98
	omtshared virtual void *getdrawinfofromretarget(void);					// 0x10392420  vt+0x9c
	omtshared virtual void quick_remap8bits(OMediaPalette *pal);			// 0x10067e10  vt+0xa0
	omtshared virtual bool quiet_dport(void);								// vt+0xa4 (base: false)

	omtshared virtual void read_class(OMediaStreamOperators &stream);		// 0x10066cb0 (OMediaClassStreamer)
	omtshared virtual void write_class(OMediaStreamOperators &stream);		// 0x10067760

	protected:
	omtshared virtual void process_palette_modified(void);					// 0x103928d0  vt+0xa8
	omtshared virtual void *getlowlevelport(void);							// 0x10392510  vt+0xac

	public:

	inline long get_origin_x(void) const {return origin_x;}
	inline long get_origin_y(void) const {return origin_y;}
	inline void set_origin(long x, long y) {origin_x = x; origin_y = y;}
	inline int get_lock_count(void) const {return lock_count;}
	inline bool get_alpha_channel(void) const {return alpha_channel;}

	static omt_MipmapMode	mipmap_mode;		// ?mipmap_mode@OMediaGfxWorld@@1W4omt_MipmapMode@@A

	// port: the back end. A world is either a memory world (pixels()) or a GL world (is_gl()).
	virtual OMediaGfxPixels *port_pixels(void) {return NULL;}
	virtual struct OMediaGLSurface *port_surface(void) {return NULL;}
	inline bool port_is_gl(void) {return port_surface() != NULL;}
	static void port_blit(OMediaGfxWorld *src, OMediaGfxWorld *dest, const OMediaRect &src_rect,
						  long dx, long dy, long dw, long dh, bool keyed);
	static void port_fill(OMediaGfxWorld *dest, const OMediaRect &r, unsigned int argb);

	protected:

	// +0x00 OMediaListener (vtable, broadcaster list), +0x14 OMediaClassStreamer (virtual base)
	OMediaPalette	*palette;			// +0x1c
	int				lock_count;			// +0x20
	long			origin_x, origin_y;	// +0x24 +0x28  coordinates of this world's (0,0) in its parent
	bool			alpha_channel;		// +0x2c
	short			alpha_bits;			// +0x2e (15)
	bool			double_lines;		// +0x30  fill() covers twice the height (interlaced surfaces)
	omt_PixelFormat	pixel_format;		// +0x3c
	short			field_40;			// +0x40
};

#endif
