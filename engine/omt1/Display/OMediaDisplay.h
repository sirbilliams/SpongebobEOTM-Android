/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    The screen: OMediaScreenDevice (video drivers and modes, rasterizers, the screen's surfaces),
    OMediaDXScreenDevice (its DirectDraw implementation, installed by OMediaDirectX::install),
    OMediaWindowPortWorld (a gfx world that draws into the screen's direct world) and
    OMediaDisplay (the main window: a supervisor that is also the window port world, and owns
    the screen canvas, normally an OMediaDoubleCanvas).
    port: one SDL window with a GL context (OMediaGLBackend); one video driver whose modes are
    the game's resolutions; one rasterizer (OpenGL, accelerated).                              */
#pragma once
#ifndef OMEDIA1_Display_H
#define OMEDIA1_Display_H

#include "OMediaBroadcaster.h"
#include "OMediaSupervisor.h"
#include "OMediaGfxWorld.h"
#include <list>
#include <vector>
#include <string>

class OMediaCanvas;
class OMedia3DRasterizerDesc;
class OMediaPaletteDriver;

// Messages the screen device broadcasts / the display listens to
const omt_Message omsg1_ScreenModeChanged		= -500;
const omt_Message omsg1_ScreenModeWillChange	= -501;
const omt_Message omsg1_ScreenDeviceTypeChanged	= -502;
const omt_Message omsg1_ResetDisplaySize		= -503;
const omt_Message omsg1_ScreenDeviceInstalled	= -502;		// OMediaDirectX::install (0xfffffe0a)

// 0x24 bytes in OMT.dll
class OMediaVideoMode
{
	public:
	unsigned short	flags;			// +0x00
	short			width;			// +0x02
	short			height;			// +0x04
	short			depth;			// +0x06
	long			refresh_rate;	// +0x08
	OMediaVideoMode() {flags = 0; width = height = depth = 0; refresh_rate = 0;}
};

// Video driver flags (+0x10): the game picks 1 for the primary display, 2 for a 3D card
enum { omcvdf_Primary = 1, omcvdf_Secondary = 2 };

class OMediaVideoDriver
{
	public:
	string					name;		// +0x00
	unsigned long			flags;		// +0x10
	void					*id;		// +0x14 (DirectDraw GUID)
	vector<OMediaVideoMode>	modes;		// +0x18
	OMediaVideoDriver() {flags = 0; id = NULL;}
};

class OMediaScreenDevice : public OMediaBroadcaster
{
	public:

	omtshared OMediaScreenDevice(bool dont_init = false);		// 0x10397b70
	omtshared virtual ~OMediaScreenDevice();					// 0x10397c60

	omtshared virtual OMediaPaletteDriver *get_palette_driver(void);			// vt+0x10
	omtshared virtual void set_pageflipping_finemode(bool fine);				// vt+0x14
	omtshared virtual bool setmode(OMediaVideoMode *mode, unsigned short flags);	// vt+0x18
	// vt+0x1c search_video_mode, vt+0x24 search_rasterizer: port: not rebuilt (the game walks
	// get_video_drivers()/get_rasterizers() itself)
	omtshared virtual void get_video_memory(unsigned long &total, unsigned long &free_mem);	// vt+0x20
	omtshared virtual void getposition(short &x, short &y);					// vt+0x28
	omtshared virtual short getwidth(void);									// vt+0x2c
	omtshared virtual short getheight(void);									// vt+0x30
	omtshared virtual short getdepth(void);									// vt+0x34
	omtshared virtual long get_refresh_rate(void);								// vt+0x38
	omtshared virtual OMediaGfxWorld *create_offscreen_gfxworld(void);		// vt+0x40
	omtshared virtual void inform_palette_changed(void);						// vt+0x44
	omtshared virtual OMediaGfxWorld *get_directscreen_world(void);			// vt+0x48
	omtshared virtual void inform_main_display_deleted(void);					// vt+0x4c

	omtshared static OMediaScreenDevice *getdefaultdevice(void);		// ?getdefaultdevice@OMediaScreenDevice@@SAPAV1@XZ
	omtshared static void setdefaultdevice(OMediaScreenDevice *d);

	inline list<OMediaVideoDriver> *get_video_drivers(void) {return &video_drivers;}
	inline vector<OMedia3DRasterizerDesc*> *get_rasterizers(void) {return &rasterizers;}
	inline bool get_pageflipping_finemode(void) const {return pageflip_finemode;}
	inline bool get_mode_flag(void) const {return mode_flag;}

	protected:

	omtshared virtual void init_retarget(void);				// vt+0x54
	omtshared virtual void create_video_drivers(void);		// vt+0x58
	omtshared virtual void update_rasterizers(void);		// vt+0x5c

	OMediaGfxWorld					*direct_world;		// +0x14  the screen's primary surface
	bool							pageflip_finemode;	// +0x1e
	list<OMediaVideoDriver>			video_drivers;		// +0x2c
	vector<OMedia3DRasterizerDesc*>	rasterizers;		// +0x38 (OMT.dll: by value, 0x20 bytes each)
	bool							mode_flag;			// +0x44  setmode's flags & 1
	short							width, height, depth;	// DX: +0x48 +0x4a +0x4c

	omtshared static OMediaScreenDevice	*defaultdevice;
	friend class OMediaDirectX;
};

class OMediaDXScreenDevice : public OMediaScreenDevice
{
	public:

	omtshared OMediaDXScreenDevice();						// 0x1005a7c0
	omtshared virtual ~OMediaDXScreenDevice();

	omtshared virtual bool setmode(OMediaVideoMode *mode, unsigned short flags);	// 0x1005ac30
	omtshared virtual OMediaGfxWorld *create_offscreen_gfxworld(void);		// 0x1005b210
	omtshared virtual OMediaGfxWorld *get_directscreen_world(void);			// 0x1005b160
	omtshared virtual void inform_main_display_deleted(void);					// 0x1005aa90

	protected:
	omtshared virtual void create_video_drivers(void);		// 0x1005b7f0
	omtshared virtual void update_rasterizers(void);		// 0x1005b840
	bool	display_deleted;	// +0x50
};

class OMediaWindowPortWorld : public OMediaGfxWorld
{
	public:

	omtshared OMediaWindowPortWorld();				// 0x10399510
	omtshared virtual ~OMediaWindowPortWorld();		// 0x103995f0

	omtshared virtual void draw_full(OMediaGfxWorld *dest, long x, long y, OMediaMaskWorld *mask = NULL);	// 0x10399c10
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *dest_rect) {OMediaGfxWorld::draw(dest, dest_rect);}
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, OMediaRect *dest_rect);		// 0x10399ce0
	omtshared virtual void draw(OMediaGfxWorld *dest, OMediaRect *src_rect, long x, long y,
								OMediaMaskWorld *mask = NULL);										// 0x10399c70
	omtshared virtual void fill(OMediaRGBColor *color, OMediaRect *rect = NULL);	// 0x10399a60
	omtshared virtual long getwidth(void);				// 0x10399940
	omtshared virtual long getheight(void);				// 0x103999a0
	omtshared virtual short getdepth(void);				// 0x10399a00
	omtshared virtual void lock(void);					// 0x103997f0
	omtshared virtual void unlock(void);				// 0x10399900
	omtshared virtual void pixlock(unsigned short flags = 0);	// 0x10399730
	omtshared virtual void pixunlock(void);				// 0x103997b0
	omtshared virtual unsigned long getattributes(void);	// 0x10399700
	omtshared virtual void getpixmaplowleveldata(char **pixels, short *width, short *height,
												 short *depth, long *rowbytes);	// 0x10399a10
	omtshared virtual bool support_page_flipping(void);	// 0x10399ac0
	omtshared virtual void page_flip(bool wait);			// 0x10399af0
	omtshared virtual bool is_page_flip_done(void);		// 0x10399b90
	omtshared virtual bool back_page_lost(void);			// 0x10399b60
	omtshared virtual void back_to_front_copy(OMediaRefreshList *rl);	// 0x10399b20
	omtshared virtual bool is_video_memory(void);			// 0x10399bc0
	omtshared virtual OMediaGfxWorld *get_direct_gfxworld(void);	// 0x10399bf0

	virtual OMediaGLSurface *port_surface(void);
	virtual OMediaGfxPixels *port_pixels(void);

	protected:
	omtshared virtual void init_retarget(void);			// 0x10399690
	OMediaGfxWorld	*direct;			// +0x48  the screen device's direct world
	int				pixlock_count;		// +0x4c
	void			find_direct(void);
};

class OMediaDisplay : public OMediaSupervisor, public OMediaWindowPortWorld
{
	public:

	omtshared OMediaDisplay(OMediaSupervisor *supervisor, OMediaCanvas *canvas, string title);	// 0x1038f8a0
	omtshared OMediaDisplay();					// 0x1038fb30
	omtshared virtual ~OMediaDisplay();			// 0x1038fbe0

	omtshared virtual void listen_to_message(omt_Message msg, void *param = NULL);	// 0x1038fcb0

	inline OMediaCanvas *getcanvas(void) {return canvas;}
	inline OMediaGfxWorld *getgfxworld(void) {return this;}		// sb2: display+0x18

	omtshared static bool debug_mode;		// ?debug_mode@OMediaDisplay@@1_NA (a window instead of a popup)

	protected:
	OMediaCanvas	*canvas;		// +0x68
};

class OMediaDirectX
{
	public:
	enum { omcdx_Sound = 1, omcdx_Video = 2, omcdx_Input = 4 };
	omtshared static void install(unsigned short components);		// 0x10056480
	omtshared static unsigned short get_installed_components(void);	// 0x10056730
	protected:
	omtshared static unsigned short components;
};

#endif
