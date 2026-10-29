/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaDisplay.h"
#include "OMediaDXSurfaceWorld.h"
#include "OMediaCanvas.h"
#include "OMedia3DRasterizerDesc.h"
#include "OMediaGfxWorldFactory.h"
#include "OMediaPeriodical.h"
#include "OMediaTimeCounter.h"
#include "OMediaGLBackend.h"

OMediaScreenDevice *OMediaScreenDevice::defaultdevice = NULL;
bool OMediaDisplay::debug_mode = false;
unsigned short OMediaDirectX::components = 0;

//------------------------------------------------------------------------------ OMediaScreenDevice

// 0x10397b70. The GDI screen device: a DC on the display, its drivers and rasterizers, and the
// texture world builder for the OMT rasterizer.
OMediaScreenDevice::OMediaScreenDevice(bool dont_init)
{
	direct_world = NULL;
	pageflip_finemode = false;
	mode_flag = false;
	width = 640;		// port: OMT.dll reads the desktop mode
	height = 480;
	depth = 32;
	if (!dont_init)
	{
		init_retarget();
		create_video_drivers();
		update_rasterizers();
	}
}

// 0x10397c60
OMediaScreenDevice::~OMediaScreenDevice()
{
	delete direct_world;
	for (size_t i = 0; i < rasterizers.size(); i++) delete rasterizers[i];
}

// ?getdefaultdevice@OMediaScreenDevice@@SAPAV1@XZ. OMT.dll makes a GDI device when none is installed.
// port: the GL device (what OMediaDirectX::install(2) installs) so tools need not install it
OMediaScreenDevice *OMediaScreenDevice::getdefaultdevice(void)
{
	if (!defaultdevice) defaultdevice = new OMediaDXScreenDevice;
	return defaultdevice;
}

void OMediaScreenDevice::setdefaultdevice(OMediaScreenDevice *d)
{
	if (defaultdevice && defaultdevice != d) delete defaultdevice;
	defaultdevice = d;
}

OMediaPaletteDriver *OMediaScreenDevice::get_palette_driver(void) { return NULL; }	// port: no 8-bit modes
void OMediaScreenDevice::set_pageflipping_finemode(bool fine) { pageflip_finemode = fine; }
bool OMediaScreenDevice::setmode(OMediaVideoMode *, unsigned short) { return false; }
void OMediaScreenDevice::get_video_memory(unsigned long &total, unsigned long &free_mem) { total = free_mem = 64 << 20; }
void OMediaScreenDevice::getposition(short &x, short &y) { x = y = 0; }
short OMediaScreenDevice::getwidth(void) { return width; }
short OMediaScreenDevice::getheight(void) { return height; }
short OMediaScreenDevice::getdepth(void) { return depth; }
long OMediaScreenDevice::get_refresh_rate(void) { return 60; }
OMediaGfxWorld *OMediaScreenDevice::create_offscreen_gfxworld(void) { return OMediaGfxWorldFactory::getfactory()->createbestgfxworld(0x22d); }
void OMediaScreenDevice::inform_palette_changed(void) {}
OMediaGfxWorld *OMediaScreenDevice::get_directscreen_world(void) { return NULL; }
void OMediaScreenDevice::inform_main_display_deleted(void) {}
void OMediaScreenDevice::init_retarget(void) {}
void OMediaScreenDevice::create_video_drivers(void) {}
void OMediaScreenDevice::update_rasterizers(void) {}

//------------------------------------------------------------------------------ OMediaDXScreenDevice

OMediaDXScreenDevice::OMediaDXScreenDevice() : OMediaScreenDevice(true)
{
	display_deleted = false;
	init_retarget();
	create_video_drivers();
	update_rasterizers();
}

OMediaDXScreenDevice::~OMediaDXScreenDevice() {}

// 0x1005b7f0: one driver per DirectDraw device, with its display modes.
// port: one driver (primary and 3D) with the resolutions the game knows
void OMediaDXScreenDevice::create_video_drivers(void)
{
	static const short res[][2] = { {640, 480}, {800, 600}, {1024, 768} };
	OMediaVideoDriver d;
	d.name = "OpenGL";
	d.flags = omcvdf_Primary | omcvdf_Secondary;
	for (int i = 0; i < 3; i++)
		for (int b = 16; b <= 32; b += 16)
		{
			OMediaVideoMode m;
			m.width = res[i][0];
			m.height = res[i][1];
			m.depth = (short)b;
			m.refresh_rate = 60;
			d.modes.push_back(m);
		}
	video_drivers.push_back(d);
}

// 0x1005b840: the Direct3D devices, then the OMT software rasterizer.
// port: both draw through GL (the game takes the accelerated one, or engine 1 with -sw)
void OMediaDXScreenDevice::update_rasterizers(void)
{
	for (size_t i = 0; i < rasterizers.size(); i++) delete rasterizers[i];
	rasterizers.clear();
	OMedia3DRasterizerDesc *d = new OMedia3DRasterizerDesc;
	d->engine = omcre_Direct3D;
	d->name = "OpenGL";
	d->attributes = omcrda_Accelerated;
	rasterizers.push_back(d);
	d = new OMedia3DRasterizerDesc;
	d->engine = omcre_OMT;
	d->name = "OMT software (OpenGL)";
	d->attributes = 0;
	rasterizers.push_back(d);
}

// 0x1005ac30. Releases the surfaces, sets the cooperative level and the display mode (none:
// windowed), recreates the primary surface, broadcasts the change and lets a second pass.
// port: the SDL window takes the mode's size as its logical size
bool OMediaDXScreenDevice::setmode(OMediaVideoMode *mode, unsigned short flags)
{
	mode_flag = (flags & 1) != 0;
	broadcast_message(omsg1_ScreenModeWillChange, NULL);
	if (direct_world) direct_world->purge();
	if (mode)
	{
		width = mode->width;
		height = mode->height;
		depth = mode->depth;
	}
	if (!omgl_window_open()) omgl_open_window("", width, height);
	else omgl_open_window("", width, height);		// new logical size
	if (direct_world)
	{
		((OMediaDXSurfaceWorld*)direct_world)->set_page_flipping(true);
		direct_world->create(width, height, NULL, NULL, 0);
	}
	if (!mode)
	{
		short size[2] = { getwidth(), getheight() };
		broadcast_message(omsg1_ResetDisplaySize, size);
	}
	else broadcast_message(omsg1_ScreenModeChanged, NULL);
	// OMT.dll lets the periodicals run for a second after a mode change
	unsigned int t = OMediaTimeCounter::get_millisecs();
	while (OMediaTimeCounter::get_millisecs() - t < 1000)
	{
		OMediaPeriodical::devote_time();
		omgl_pump_events();
	}
	return true;
}

// 0x1005b210
OMediaGfxWorld *OMediaDXScreenDevice::create_offscreen_gfxworld(void)
{
	return new OMediaDXSurfaceWorld(false);
}

// 0x1005b160
OMediaGfxWorld *OMediaDXScreenDevice::get_directscreen_world(void)
{
	if (!direct_world)
	{
		if (!omgl_window_open()) omgl_open_window("", width, height);	// port: GL needs the window
		OMediaDXSurfaceWorld *w = new OMediaDXSurfaceWorld(true);
		w->set_page_flipping(true);
		w->create(width, height, NULL, NULL, 0);
		direct_world = w;
	}
	return direct_world;
}

// 0x1005aa90
void OMediaDXScreenDevice::inform_main_display_deleted(void)
{
	if (!display_deleted)
	{
		delete direct_world;
		direct_world = NULL;
		display_deleted = true;
	}
}

//------------------------------------------------------------------------------ OMediaWindowPortWorld
// The display's world: everything goes to the screen device's direct world (0x48), found lazily.

OMediaWindowPortWorld::OMediaWindowPortWorld()
{
	direct = NULL;
	pixlock_count = 0;
	init_retarget();
}

OMediaWindowPortWorld::~OMediaWindowPortWorld() {}
void OMediaWindowPortWorld::init_retarget(void) {}		// 0x10399690 port: no OMediaWinRtgWindowPort

void OMediaWindowPortWorld::find_direct(void)
{
	if (!direct) direct = OMediaScreenDevice::getdefaultdevice()->get_directscreen_world();
}

OMediaGLSurface *OMediaWindowPortWorld::port_surface(void) { find_direct(); return direct ? direct->port_surface() : NULL; }
OMediaGfxPixels *OMediaWindowPortWorld::port_pixels(void) { find_direct(); return direct ? direct->port_pixels() : NULL; }

// 0x10399700
unsigned long OMediaWindowPortWorld::getattributes(void)
{
	find_direct();
	if (direct) return direct->getattributes();
	return 0x805;
}

// 0x10399730
void OMediaWindowPortWorld::pixlock(unsigned short flags)
{
	if (pixlock_count == 0)
	{
		find_direct();
		if (direct)
		{
			direct->pixlock(flags);
			field_40 = 0;
			pixlock_count++;
			return;
		}
		lock();
	}
	pixlock_count++;
}

// 0x103997b0
void OMediaWindowPortWorld::pixunlock(void)
{
	if (--pixlock_count == 0)
	{
		if (direct) { direct->pixunlock(); return; }
		unlock();
	}
}

// 0x103997f0 / 0x10399900: the window's DC (GetDC) and the direct world's lock
void OMediaWindowPortWorld::lock(void)
{
	if (lock_count == 0)
	{
		find_direct();
		if (direct) direct->lock();
	}
	lock_count++;
}

void OMediaWindowPortWorld::unlock(void)
{
	if (--lock_count == 0 && direct) direct->unlock();
}

long OMediaWindowPortWorld::getwidth(void) { find_direct(); return direct ? direct->getwidth() : OMediaScreenDevice::getdefaultdevice()->getwidth(); }		// 0x10399940
long OMediaWindowPortWorld::getheight(void) { find_direct(); return direct ? direct->getheight() : OMediaScreenDevice::getdefaultdevice()->getheight(); }	// 0x103999a0
short OMediaWindowPortWorld::getdepth(void) { return OMediaScreenDevice::getdefaultdevice()->getdepth(); }		// 0x10399a00

// 0x10399a10
void OMediaWindowPortWorld::getpixmaplowleveldata(char **pixels, short *w, short *h, short *d, long *rowbytes)
{
	find_direct();
	if (direct) direct->getpixmaplowleveldata(pixels, w, h, d, rowbytes);
	else *pixels = NULL;
}

// 0x10399a60
void OMediaWindowPortWorld::fill(OMediaRGBColor *color, OMediaRect *rect)
{
	find_direct();
	if (direct) direct->fill(color, rect);
	else OMediaGfxWorld::fill(color, rect);
}

bool OMediaWindowPortWorld::support_page_flipping(void) { find_direct(); return direct ? direct->support_page_flipping() : false; }	// 0x10399ac0
void OMediaWindowPortWorld::page_flip(bool wait) { find_direct(); if (direct) direct->page_flip(wait); }					// 0x10399af0
void OMediaWindowPortWorld::back_to_front_copy(OMediaRefreshList *rl) { find_direct(); if (direct) direct->back_to_front_copy(rl); }	// 0x10399b20
bool OMediaWindowPortWorld::back_page_lost(void) { find_direct(); return direct ? direct->back_page_lost() : false; }		// 0x10399b60
bool OMediaWindowPortWorld::is_page_flip_done(void) { find_direct(); return direct ? direct->is_page_flip_done() : true; }	// 0x10399b90
bool OMediaWindowPortWorld::is_video_memory(void) { find_direct(); return direct ? direct->is_video_memory() : false; }		// 0x10399bc0
OMediaGfxWorld *OMediaWindowPortWorld::get_direct_gfxworld(void) { find_direct(); return direct; }						// 0x10399bf0

// 0x10399c10 / 0x10399c70 / 0x10399ce0: the direct world draws, else the GDI path
void OMediaWindowPortWorld::draw_full(OMediaGfxWorld *dest, long x, long y, OMediaMaskWorld *mask)
{
	find_direct();
	if (direct) direct->draw_full(dest, x, y, mask);
	else OMediaGfxWorld::draw_full(dest, x, y, mask);
}

void OMediaWindowPortWorld::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, long x, long y, OMediaMaskWorld *mask)
{
	find_direct();
	if (direct) direct->draw(dest, src_rect, x, y, mask);
	else OMediaGfxWorld::draw(dest, src_rect, x, y, mask);
}

void OMediaWindowPortWorld::draw(OMediaGfxWorld *dest, OMediaRect *src_rect, OMediaRect *dest_rect)
{
	find_direct();
	if (direct) direct->draw(dest, src_rect, dest_rect);
	else OMediaGfxWorld::draw(dest, src_rect, dest_rect);
}

//------------------------------------------------------------------------------ OMediaDisplay

// 0x1038f8a0. Listens to the screen device, registers the window class once, creates the
// window at the screen's size (a WS_POPUP unless debug_mode), and gives the canvas this world.
OMediaDisplay::OMediaDisplay(OMediaSupervisor *supervisor, OMediaCanvas *c, string title) : OMediaSupervisor(supervisor)
{
	OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
	dev->addlistener(this);
	// port: the SDL window (CreateWindowEx "OPEN MEDIA TOOLKIT DISPLAY", ShowWindow, SetFocus)
	omgl_open_window(title.empty() ? "OMT" : title.c_str(), dev->getwidth(), dev->getheight());
	canvas = c;
	canvas->setgfxworld(this);
	set_main_supervisor(this);
}

// 0x1038fb30
OMediaDisplay::OMediaDisplay() : OMediaSupervisor()
{
	canvas = NULL;
}

// 0x1038fbe0
OMediaDisplay::~OMediaDisplay()
{
	if (canvas)
	{
		canvas->detach_gfxworld();		// OMT.dll: canvas->gfxworld = NULL (the world is this display)
		canvas->coordinate();
		delete canvas;
	}
	OMediaScreenDevice::getdefaultdevice()->inform_main_display_deleted();
	direct = NULL;
	omgl_close_window();		// port: DestroyWindow
}

// 0x1038fcb0
void OMediaDisplay::listen_to_message(omt_Message msg, void *param)
{
	switch (msg)
	{
		case omsg1_ResetDisplaySize:
		case omsg1_ScreenModeChanged:
		{
			// port: SetWindowPos to the new size is the logical size change in setmode
			if (canvas) canvas->coordinate();
			if (canvas && canvas->is_double_canvas()) ((OMediaDoubleCanvas*)canvas)->empty_coverrect();
			direct = NULL;		// port: the direct world was recreated
			break;
		}
		case omsg1_ScreenDeviceTypeChanged:
			if (canvas && canvas->is_double_canvas()) ((OMediaDoubleCanvas*)canvas)->update_second_world_type();
			return;
		case omsg1_ScreenModeWillChange:
			if (canvas && canvas->is_double_canvas()) ((OMediaDoubleCanvas*)canvas)->purge_secondworld();
			return;
		default:
			OMediaSupervisor::listen_to_message(msg, param);
			return;
	}
	pixel_format = omcpf_RGB32;		// fcn.10068470(screen depth) port: always 32-bit
}

//------------------------------------------------------------------------------ OMediaDirectX

extern void omf_install_dx_sound(void) __attribute__((weak));	// lane B: OMediaDXSoundManager
extern void omf_install_dx_input(void) __attribute__((weak));	// lane B: OMediaDXInputDriver

// 0x10056480. 2: the DirectDraw surface world (priority -10) and screen device, then the
// Direct3D rasterizer manager; 1: the DirectSound manager; 4: the DirectInput driver.
void OMediaDirectX::install(unsigned short what)
{
	if ((what & omcdx_Video) && !(components & omcdx_Video))
	{
		components |= omcdx_Video;
		OMediaGfxWorldFactory::getfactory()->register_gfxworld(OMediaDXSurfaceWorld::builder, 0x604e, -10);
		OMediaScreenDevice *old = OMediaScreenDevice::defaultdevice;
		if (!old || !dynamic_cast<OMediaDXScreenDevice*>(old))
		{
			OMediaDXScreenDevice *d = new OMediaDXScreenDevice;
			OMediaScreenDevice::setdefaultdevice(d);
			d->broadcast_message(omsg1_ScreenDeviceInstalled, NULL);
		}
		// OMT.dll: OMedia3DRasterizerManager::set_default(new DX manager) port: the GL manager
	}
	if ((what & omcdx_Sound) && !(components & omcdx_Sound))
	{
		components |= omcdx_Sound;
		if (omf_install_dx_sound) omf_install_dx_sound();
	}
	if ((what & omcdx_Input) && !(components & omcdx_Input))
	{
		components |= omcdx_Input;
		if (omf_install_dx_input) omf_install_dx_input();
	}
}

unsigned short OMediaDirectX::get_installed_components(void) { return components; }	// 0x10056730
