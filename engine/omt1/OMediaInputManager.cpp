/*  OMT 1.x input manager and input driver (OMT.dll), on the SDL layer.  See OMediaInputManager.h.  LGPL. */

#include "OMediaInputManager.h"
#include "OMediaPeriodical.h"
#include "OMediaListener.h"
#include "OMediaMouseCursor.h"
#include "OMediaDefMessages.h"
#include "OMediaWindow.h"
#include "OMediaSDLRetarget.h"
#include "OMediaEventManager.h"

// sdl/OMediaSDLInput.cpp, sdl/OMediaSDLVideo.cpp
OMediaInputEngine *omf_SDLCreateInputEngine(OMediaWindow *master);
void omf_SDLSetMousePosition(short x, short y);
void omf_SDLWindowToLogical(OMediaWindow *ow, int px, int py, int &lx, int &ly);

//------------------------------------------------------------------------------------------
// The game window, and the finger on it

// The window the pointer is in: the one named with set_game_window, else the one OpenGL draws into
// (the SDL layer keeps its OMediaWindow in the window data).
static OMediaWindow *omg_game_window = NULL;

static SDL_Window *omf_game_window(OMediaWindow **ow)
{
	if (omg_game_window)
	{
		OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)omg_game_window->get_retarget();
		if (rtg && rtg->sdlwin) { *ow = omg_game_window; return rtg->sdlwin; }
	}
	SDL_Window *w = SDL_GL_GetCurrentWindow();
	*ow = w ? (OMediaWindow*)SDL_GetWindowData(w, "omt") : NULL;
	return *ow ? w : NULL;
}



// The screen OMT.dll asked OMediaScreenDevice::getdefaultdevice() for: the game window's logical size.
static void omf_screen_size(int &w, int &h)
{
	OMediaWindow *ow;
	if (omf_game_window(&ow)) { w = ow->get_width(); h = ow->get_height(); }
	else { w = 640; h = 480; }
}

struct OMediaTouchState
{
	volatile int	down;				// a finger is on the screen
	volatile float	nx, ny;				// where it is, 0..1 of the window
	volatile int	fingers;
	SDL_FingerID	finger;				// the one we follow (the first one that came down)
	bool			watching;
};
static OMediaTouchState omg_touch = { 0, 0.5f, 0.5f, 0, 0, false };

// Sees every SDL event as it is pushed (on Android that is the UI thread), before the event
// manager takes them. It only records the finger; the events go on to the event manager as usual.
static int SDLCALL omf_touch_watch(void *, SDL_Event *e)
{
	switch (e->type)
	{
		case SDL_FINGERDOWN:
			omg_touch.fingers++;
			if (!omg_touch.down) { omg_touch.finger = e->tfinger.fingerId; omg_touch.down = 1; }
			if (omg_touch.finger == e->tfinger.fingerId) { omg_touch.nx = e->tfinger.x; omg_touch.ny = e->tfinger.y; }
			break;
		case SDL_FINGERMOTION:
			if (omg_touch.down && omg_touch.finger == e->tfinger.fingerId) { omg_touch.nx = e->tfinger.x; omg_touch.ny = e->tfinger.y; }
			break;
		case SDL_FINGERUP:
			if (omg_touch.fingers > 0) omg_touch.fingers--;
			if (omg_touch.down && omg_touch.finger == e->tfinger.fingerId)
			{
				omg_touch.nx = e->tfinger.x; omg_touch.ny = e->tfinger.y;
				omg_touch.down = 0;
			}
			break;
	}
	return 0;
}

static void omf_start_touch_watch(void)
{
	if (omg_touch.watching) return;
	omg_touch.watching = true;
	SDL_AddEventWatch(omf_touch_watch, NULL);
}

// Finger position (0..1 of the window) to the game's logical coordinates.
static bool omf_touch_logical(int &lx, int &ly)
{
	OMediaWindow *ow;
	SDL_Window *w = omf_game_window(&ow);
	if (!w) return false;
	int ww, wh;
	SDL_GetWindowSize(w, &ww, &wh);
	omf_SDLWindowToLogical(ow, (int)(omg_touch.nx * (float)ww), (int)(omg_touch.ny * (float)wh), lx, ly);
	return true;
}

// The finger is only seen from the moment the watch exists: the game names its window (set_game_window)
// when it makes it, which is before anything can touch it.
void OMediaInputDriver::set_game_window(OMediaWindow *w)
{
	omg_game_window = w;
	omf_start_touch_watch();
}

//------------------------------------------------------------------------------------------
// OMediaCustomMouse (0x10069bd0 ...)

// Where the last custom mouse stopped (0x10420a94: set, a96 / a98: position), so that the next
// one starts there.
static bool  omg_custom_saved = false;
static short omg_custom_saved_x = 0, omg_custom_saved_y = 0;

class OMediaCustomMouse : public OMediaPeriodical, public OMediaListener
{
	public:
	// 0x10069bd0
	OMediaCustomMouse(OMediaInputDriver *adriver, OMediaRect *r)
	{
		driver = adriver;
		bounds = *r;
		OMediaMouseCursor::hide();
		if (!omg_custom_saved)
		{
			x = (short)((bounds.right - bounds.left) / 2 + bounds.left);
			y = (short)((bounds.bottom - bounds.top) / 2 + bounds.top);
		}
		else set_position(omg_custom_saved_x, omg_custom_saved_y);
		int sw, sh;
		omf_screen_size(sw, sh);
		lastx = (short)(sw >> 1);
		lasty = (short)(sh >> 1);
		driver->set_sysmouse_position(lastx, lasty);
	}

	// 0x10069d00
	virtual ~OMediaCustomMouse()
	{
		omg_custom_saved = true;
		omg_custom_saved_x = x;
		omg_custom_saved_y = y;
		OMediaMouseCursor::show();
	}

	// 0x10069ec0: the position, kept inside the rectangle
	void set_position(short nx, short ny)
	{
		x = nx; y = ny;
		if (x < bounds.left) x = (short)bounds.left;
		if (y < bounds.top) y = (short)bounds.top;
		if (bounds.right <= x) x = (short)(bounds.right - 1);
		if (bounds.bottom <= y) y = (short)(bounds.bottom - 1);
	}

	// 0x10069f20
	void set_bounds(OMediaRect *r)
	{
		bounds = *r;
		set_position(x, y);
	}

	// 0x10069e00: moves by how far the system pointer went since last time, and puts the system
	// pointer back in the middle of the screen when it gets near an edge.
	virtual void spend_time(void)
	{
		short sx, sy;
		driver->get_sysmouse_position(sx, sy);
		set_position((short)(x - lastx + sx), (short)(y - lasty + sy));
		lastx = sx; lasty = sy;
		int sw, sh;
		omf_screen_size(sw, sh);
		if (lastx < 1 || sw - 1 <= lastx || lasty < 1 || sh - 1 <= lasty)
		{
			lastx = (short)(sw >> 1);
			lasty = (short)(sh >> 1);
			driver->set_sysmouse_position(lastx, lasty);
		}
	}

	// 0x10069d90: the screen changed: start again from the middle
	virtual void listen_to_message(omt_Message msg, void *param =NULL)
	{
		if (msg != omsg_ScreenModeChanged) return;
		int sw, sh;
		omf_screen_size(sw, sh);
		lastx = (short)(sw >> 1);
		lasty = (short)(sh >> 1);
		driver->set_sysmouse_position(lastx, lasty);
		x = (short)((bounds.right - bounds.left) / 2 + bounds.left);
		y = (short)((bounds.bottom - bounds.top) / 2 + bounds.top);
	}

	OMediaInputDriver	*driver;
	OMediaRect			bounds;
	short				x, y;			// the custom pointer
	short				lastx, lasty;	// the system pointer when we looked last
};

//------------------------------------------------------------------------------------------
// OMediaInputDriver

// 0x...: OMediaWinInputDriver (direct_input false) / OMediaDXInputDriver (true)
OMediaInputDriver::OMediaInputDriver(bool adirect_input)
{
	custom_mouse = NULL;
	mouse_offsetx = mouse_offsety = 0;
	direct_input = adirect_input;
	hid_engine = NULL;
}

OMediaInputDriver::~OMediaInputDriver()
{
	remove_custom_mouse();
	// port: the static input_manager is destroyed after SDL_Quit, when the controllers are already closed
	if (SDL_WasInit(0)) delete hid_engine;
}

// 0x10392a20: the left button (GetAsyncKeyState(VK_LBUTTON)).
// port: or a finger on the screen.
bool OMediaInputDriver::mouse_down(void)
{
	omf_start_touch_watch();
#ifdef OMT1_ENGINE
	if (omg_touch.down || OMediaEventManager::pad_button_down()) return true;	// port: R3 / A (omt1 event manager only)
#else
	if (omg_touch.down) return true;	// (also built into the OMT 2.5 'omt' library, whose event manager has no pad state)
#endif
	return (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
}

// 0x10069a80: the custom mouse if there is one, else the system pointer, plus the mouse offset.
void OMediaInputDriver::get_mouse_position(short &x, short &y)
{
	if (custom_mouse)
	{
		x = custom_mouse->x;
		y = custom_mouse->y;
	}
	else get_sysmouse_position(x, y);
	x = (short)(x + mouse_offsetx);
	y = (short)(y + mouse_offsety);
}

// 0x10069ae0
void OMediaInputDriver::set_mouse_position(short x, short y)
{
	if (custom_mouse) custom_mouse->set_position(x, y);
	else set_sysmouse_position(x, y);
}

// 0x10392a30: GetCursorPos (desktop pixels).
// port: the game window's logical coordinates; a finger down is the pointer.
void OMediaInputDriver::get_sysmouse_position(short &x, short &y)
{
	omf_start_touch_watch();
	int lx, ly;
	if (omg_touch.down && omf_touch_logical(lx, ly)) { x = (short)lx; y = (short)ly; return; }

	int mx, my;
	SDL_GetMouseState(&mx, &my);
	OMediaWindow *ow;
	if (omf_game_window(&ow)) omf_SDLWindowToLogical(ow, mx, my, lx, ly);
	else { lx = mx; ly = my; }
	x = (short)lx; y = (short)ly;
}

// 0x10392ab0: SetCursorPos, unless the application is in the background.
// port: SDL warps the pointer in the window (on Android it moves SDL's own pointer position).
void OMediaInputDriver::set_sysmouse_position(short x, short y)
{
	SDL_Window *w = SDL_GL_GetCurrentWindow();
	if (w && !(SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS)) return;
	omf_SDLSetMousePosition(x, y);
}

// 0x10392a60: Shift 1, Windows key 2 (the Command key), Control 4, Alt 8
unsigned short OMediaInputDriver::get_command_keys_status(void)
{
	const Uint8 *k = SDL_GetKeyboardState(NULL);
	unsigned short r = 0;
	if (k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT]) r |= omtck_Shift;
	if (k[SDL_SCANCODE_LALT] || k[SDL_SCANCODE_RALT]) r |= omtck_Alt;
	if (k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL]) r |= omtck_Control;
	if (k[SDL_SCANCODE_LGUI] || k[SDL_SCANCODE_RGUI]) r |= omtck_Command;
	return r;
}

// 0x10069b40: replaces a custom mouse already there
void OMediaInputDriver::install_custom_mouse(OMediaRect *bounds)
{
	delete custom_mouse;
	custom_mouse = NULL;
	custom_mouse = new OMediaCustomMouse(this, bounds);
}

// 0x10069bb0
void OMediaInputDriver::remove_custom_mouse(void)
{
	delete custom_mouse;
	custom_mouse = NULL;
}

// 0x10069b10
void OMediaInputDriver::set_custom_mouse_bounds(OMediaRect *bounds)
{
	if (custom_mouse) custom_mouse->set_bounds(bounds);
}

// OMediaDXInputDriver made the devices with DirectInput; port: the SDL layer's input engine
// (keyboard, mouse, then game controllers and joysticks), which has no window.
omt_HIDList *OMediaInputDriver::get_hid(void)
{
	if (!hid_engine) hid_engine = omf_SDLCreateInputEngine(NULL);
	return hid_engine->get_hid();
}

void OMediaInputDriver::rescan_hid(void)
{
	delete hid_engine;
	hid_engine = NULL;
}

//------------------------------------------------------------------------------------------
// OMediaInputManager

OMediaInputManager OMediaInputManager::input_manager;

// 0x10393f80: a Windows input driver
OMediaInputManager::OMediaInputManager()
{
	driver = new OMediaInputDriver(false);
}

// 0x10394000
OMediaInputManager::~OMediaInputManager()
{
	delete driver;
}
