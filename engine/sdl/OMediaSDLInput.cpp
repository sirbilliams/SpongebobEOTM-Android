/*  OMT SDL platform layer - event manager (SDL event pump -> OMT events) and input engine
    (keyboard / mouse / game controllers exposed as OMT HID devices).  LGPL. */

#include "OMediaSDLRetarget.h"
#include "OMediaEventManager.h"
#include "OMediaInputEngine.h"
#include "OMediaSupervisor.h"
#include "OMediaWindow.h"
#include "OMediaAnimPeriodical.h"
#include "OMediaDefMessages.h"
#include "OMediaTimeCounter.h"
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Event manager

OMediaEventManager *OMediaEventManager::event_manager;

OMediaEventManager::OMediaEventManager(void) { event_manager = this; key_trap = NULL; }
OMediaEventManager::~OMediaEventManager(void) { if (event_manager == this) event_manager = NULL; }
void *OMediaEventManager::get_local_message_handler(void) { return NULL; }
void OMediaEventManager::update_menu_states(void) {}

bool OMediaEventManager::filter_event(void *local_event)
{
	OMediaFilterLocalEvent filter;
	filter.handled = false;
	filter.local_event = local_event;
	filtermsg_broadcaster.broadcast_message(omsg_LowlevelEventFilter, &filter);
	return filter.handled;
}

static omt_SpecialKey omf_special(SDL_Keycode k)
{
	switch (k)
	{
		case SDLK_RETURN:    return omtsk_Return;
		case SDLK_KP_ENTER:  return omtsk_Enter;
		case SDLK_LEFT:      return omtsk_ArrowLeft;
		case SDLK_RIGHT:     return omtsk_ArrowRight;
		case SDLK_UP:        return omtsk_ArrowTop;
		case SDLK_DOWN:      return omtsk_ArrowBottom;
		case SDLK_TAB:       return omtsk_Tab;
		case SDLK_BACKSPACE: return omtsk_Backspace;
		case SDLK_DELETE:    return omtsk_Delete;
		case SDLK_ESCAPE:    return omtsk_Escape;
		case SDLK_AC_BACK:   return omtsk_Escape;      // Android back button
		case SDLK_KP_0: return omtsk_Keypad0; case SDLK_KP_1: return omtsk_Keypad1;
		case SDLK_KP_2: return omtsk_Keypad2; case SDLK_KP_3: return omtsk_Keypad3;
		case SDLK_KP_4: return omtsk_Keypad4; case SDLK_KP_5: return omtsk_Keypad5;
		case SDLK_KP_6: return omtsk_Keypad6; case SDLK_KP_7: return omtsk_Keypad7;
		case SDLK_KP_8: return omtsk_Keypad8; case SDLK_KP_9: return omtsk_Keypad9;
		case SDLK_KP_PERIOD: return omtsk_KeypadDot;
		default: break;
	}
	if (k >= SDLK_F1 && k <= SDLK_F12) return (omt_SpecialKey)(omtsk_F1 + (k - SDLK_F1));
	if (k >= SDLK_F13 && k <= SDLK_F15) return (omt_SpecialKey)(omtsk_F13 + (k - SDLK_F13));
	return omtsk_Null;
}

static omt_CommandKey omf_cmdkeys(void)
{
	SDL_Keymod m = SDL_GetModState();
	omt_CommandKey c = 0;
	if (m & KMOD_SHIFT) c |= omtck_Shift;
	if (m & KMOD_ALT)   c |= omtck_Alt;
	if (m & KMOD_CTRL)  c |= omtck_Control;
	if (m & KMOD_GUI)   c |= omtck_Command;
	return c;
}

// Map a window-pixel mouse position to the window's logical coordinates (letterbox aware).
void omf_SDLWindowToLogical(OMediaWindow *ow, int px, int py, int &lx, int &ly);
static void omf_mouse_to_logical(OMediaWindow *w, int px, int py, int &lx, int &ly)
{
	omf_SDLWindowToLogical(w, px, py, lx, ly);
}

static void omf_fill_extra(OMediaEvent &e, int px, int py)
{
	e.command_key = omf_cmdkeys();
	if (e.window)
	{
		omf_mouse_to_logical(e.window, px, py, e.win_mouse_x, e.win_mouse_y);
		e.mouse_x = e.win_mouse_x + e.window->get_x();
		e.mouse_y = e.win_mouse_y + e.window->get_y();
	}
}

void OMediaEventManager::osevent_to_omtevent(void *localevent, OMediaEvent *ev)
{
	SDL_Event *se = (SDL_Event*)localevent;
	int mx, my;
	switch (se->type)
	{
		case SDL_TEXTINPUT:
			ev->window = OMediaSDLRtgWindow::find(se->text.windowID);
			if (!ev->window || (unsigned char)se->text.text[0] >= 0x80) return;
			ev->type = omtet_KeyDown;
			ev->ascii_key = se->text.text[0];
			ev->special_key = omtsk_Null;
			SDL_GetMouseState(&mx, &my); omf_fill_extra(*ev, mx, my);
			if (key_trap) key_trap->listen_to_message(omsg_KeyDown, ev);
			else key_broadcaster.broadcast_message(omsg_KeyDown, ev);
			return;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
		{
			ev->window = OMediaSDLRtgWindow::find(se->key.windowID);
			if (!ev->window) return;
			bool down = se->type == SDL_KEYDOWN;
			ev->type = down ? omtet_KeyDown : omtet_KeyUp;
			ev->ascii_key = 0;
			ev->special_key = omf_special(se->key.keysym.sym);
			// Printable keys arrive through SDL_TEXTINPUT on key-down; send key-ups for everything
			// and key-downs only for non-printables.
			if (down && ev->special_key == omtsk_Null) return;
			SDL_GetMouseState(&mx, &my); omf_fill_extra(*ev, mx, my);
			omt_Message m = down ? omsg_KeyDown : omsg_KeyUp;
			if (key_trap) key_trap->listen_to_message(m, ev);
			else key_broadcaster.broadcast_message(m, ev);
			return;
		}

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		{
			ev->window = OMediaSDLRtgWindow::find(se->button.windowID);
			if (!ev->window) return;
			bool down = se->type == SDL_MOUSEBUTTONDOWN;
			bool left = se->button.button == SDL_BUTTON_LEFT;
			ev->type = left ? (down ? omtet_LMouseDown : omtet_LMouseUp) : (down ? omtet_RMouseDown : omtet_RMouseUp);
			omf_fill_extra(*ev, se->button.x, se->button.y);
			send_click_message(ev, left && down, down);
			return;
		}

		case SDL_WINDOWEVENT:
			ev->window = OMediaSDLRtgWindow::find(se->window.windowID);
			if (!ev->window) return;
			switch (se->window.event)
			{
				case SDL_WINDOWEVENT_EXPOSED:      ev->type = omtet_WindowRefresh; break;
				case SDL_WINDOWEVENT_FOCUS_GAINED: ev->type = omtet_WindowActivated; ev->window->activated(); break;
				case SDL_WINDOWEVENT_FOCUS_LOST:   ev->type = omtet_WindowDeactivated; ev->window->deactivated(); break;
				case SDL_WINDOWEVENT_CLOSE:
					ev->type = omtet_WindowCloseButton;
					ev->window->listen_to_message(omsg_Close, ev->window);
					break;
				case SDL_WINDOWEVENT_RESIZED:
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				case SDL_WINDOWEVENT_MOVED:
					ev->window->bounds_changed();
					break;
			}
			return;
	}
}

// ---------------------------------------------------------------------------
// Port: gamepad extras (not in OMT; the table is in docs/controls.md)
//
// The game reads the gamepad through the HID elements below, as it read DirectInput on Windows.
// What it reads only from the keyboard or the mouse gets a gamepad control here:
//   start          Esc     (Start is also button 4, the pause menu; Esc on its own closes text
//                           boxes, skips cut scenes and puts the item away, as on the keyboard)
//   leftshoulder   R       (shows / hides the HUD panel)
//   righttrigger   Y       (yes, in the confirmation boxes)
//   lefttrigger    N       (no)
//   a              Space   (A is also button 0, the action; Space is Key_Action in keys.txt, so in
//                           play it does the same, and "press any key" screens - the score
//                           screen - take A)
//   right stick    moves the mouse pointer. The menus follow the pointer, and in play the game
//                  turns the camera by how far the pointer moved since it re-centred it (mouse
//                  look, 0x4a3990). Speed: up to 3/4 of the window width per second, on a square
//                  curve past a 20% dead zone.
// The keys go through the same path as the keyboard's (pushed as SDL key events; the game's
// event filter turns them into Windows key codes), so for the game they are key presses. A key
// is down while the control is held on any controller (a trigger: past half way, up again below
// a quarter). The event manager keeps every game controller open for this (the HID devices come
// and go with the input engines the game makes).
// The Android back button arrives as SDL_SCANCODE_AC_BACK, which the game takes as Esc (as does
// OMT, omf_special); omf_SDLPadInit sets SDL_HINT_ANDROID_TRAP_BACK_BUTTON so that Android never
// handles it as "close the app".

static std::vector<SDL_GameController *> omg_pads;

static void omf_pad_added(int device_index)
{
	SDL_GameController *gc = SDL_GameControllerOpen(device_index);
	if (!gc) return;
	for (size_t i = 0; i < omg_pads.size(); i++)
		if (omg_pads[i] == gc) { SDL_GameControllerClose(gc); return; }	// (open is reference counted)
	omg_pads.push_back(gc);
}

static void omf_pad_removed(SDL_JoystickID id)
{
	for (size_t i = 0; i < omg_pads.size(); i++)
		if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(omg_pads[i])) == id)
		{
			SDL_GameControllerClose(omg_pads[i]);
			omg_pads.erase(omg_pads.begin() + i);
			return;
		}
}

// A stick axis of all open controllers, -1..1 after the dead zone, on a square curve.
static float omf_pad_stick(SDL_GameControllerAxis axis)
{
	float v = 0;
	for (size_t i = 0; i < omg_pads.size(); i++)
	{
		float a = SDL_GameControllerGetAxis(omg_pads[i], axis) / 32767.0f;
		if (a > 1) a = 1; else if (a < -1) a = -1;
		const float dead = 0.2f;
		if (a > dead) a = (a - dead) / (1 - dead);
		else if (a < -dead) a = (a + dead) / (1 - dead);
		else a = 0;
		v += a * (a < 0 ? -a : a);
	}
	return v;
}

struct omt_PadKey
{
	int				button;		// SDL_GameControllerButton, or -1
	int				trigger;	// SDL_GameControllerAxis (a trigger), or -1
	SDL_Scancode	key;
	bool			down;
};

static omt_PadKey omg_pad_keys[] = {
	{ SDL_CONTROLLER_BUTTON_START,        -1, SDL_SCANCODE_ESCAPE, false },
	{ SDL_CONTROLLER_BUTTON_LEFTSHOULDER, -1, SDL_SCANCODE_R,      false },
	{ SDL_CONTROLLER_BUTTON_LEFTSTICK,    -1, SDL_SCANCODE_L,      false },	// port: L3 = key L (the mini sub's headlight, keyboard only in the original)
	{ -1, SDL_CONTROLLER_AXIS_TRIGGERRIGHT,   SDL_SCANCODE_Y,      false },
	{ -1, SDL_CONTROLLER_AXIS_TRIGGERLEFT,    SDL_SCANCODE_N,      false },
	{ SDL_CONTROLLER_BUTTON_A,            -1, SDL_SCANCODE_SPACE,  false },
};

static SDL_Window *omf_game_window(OMediaWindow **ow);

static void omf_pad_keys(void)
{
	for (size_t k = 0; k < sizeof omg_pad_keys / sizeof omg_pad_keys[0]; k++)
	{
		omt_PadKey &m = omg_pad_keys[k];
		bool held = false;
		for (size_t i = 0; i < omg_pads.size() && !held; i++)
		{
			if (m.button >= 0) held = SDL_GameControllerGetButton(omg_pads[i], (SDL_GameControllerButton)m.button) != 0;
			else
			{
				int v = SDL_GameControllerGetAxis(omg_pads[i], (SDL_GameControllerAxis)m.trigger);
				held = v > (m.down ? 8192 : 16384);
			}
		}
		if (held == m.down) continue;
		m.down = held;
		OMediaWindow *ow;
		SDL_Window *w = omf_game_window(&ow);
		SDL_Event e;
		SDL_zero(e);
		e.type = held ? SDL_KEYDOWN : SDL_KEYUP;
		e.key.timestamp = SDL_GetTicks();
		e.key.windowID = w ? SDL_GetWindowID(w) : 0;
		e.key.state = held ? SDL_PRESSED : SDL_RELEASED;
		e.key.keysym.scancode = m.key;
		e.key.keysym.sym = SDL_GetKeyFromScancode(m.key);
		SDL_PushEvent(&e);
		if (getenv("OMT_PAD_LOG")) SDL_Log("OMT pad: key %s %s", SDL_GetScancodeName(m.key), held ? "down" : "up");
	}
}

void omf_SDLWindowToLogical(OMediaWindow *ow, int px, int py, int &lx, int &ly);
void omf_SDLLogicalToWindow(OMediaWindow *ow, int lx, int ly, int &px, int &py);

// The game window: the one OpenGL draws into.
static SDL_Window *omf_game_window(OMediaWindow **ow)
{
	SDL_Window *w = SDL_GL_GetCurrentWindow();
	*ow = w ? (OMediaWindow*)SDL_GetWindowData(w, "omt") : NULL;
	return *ow ? w : NULL;
}

// Puts the mouse pointer at logical window coordinates (OMT 2002's input engine had
// set_mouse_position, vtable +0x24; OMT 2.5 has none). On Android, where there is nothing to
// warp, SDL moves its own pointer position, which is what the game reads back.
void omf_SDLSetMousePosition(short x, short y)
{
	OMediaWindow *ow;
	SDL_Window *w = omf_game_window(&ow);
	if (!w) return;
	int px, py;
	omf_SDLLogicalToWindow(ow, x, y, px, py);
	SDL_WarpMouseInWindow(w, px, py);
}

static void omf_pad_pointer(float dt)
{
	static float rest_x = 0, rest_y = 0;		// fractions of a pixel carried to the next frame
	float sx = omf_pad_stick(SDL_CONTROLLER_AXIS_RIGHTX), sy = omf_pad_stick(SDL_CONTROLLER_AXIS_RIGHTY);
	if (sx == 0 && sy == 0) { rest_x = rest_y = 0; return; }
	OMediaWindow *ow;
	SDL_Window *w = omf_game_window(&ow);
	if (!w) return;
	const float speed = 0.75f * ow->get_width();
	rest_x += sx * speed * dt;
	rest_y += sy * speed * dt;
	int dx = int(rest_x), dy = int(rest_y);
	rest_x -= dx; rest_y -= dy;
	if (dx == 0 && dy == 0) return;
	int mx, my, lx, ly;
	SDL_GetMouseState(&mx, &my);
	omf_SDLWindowToLogical(ow, mx, my, lx, ly);
	lx += dx; ly += dy;
	if (lx < 0) lx = 0; else if (lx >= ow->get_width()) lx = ow->get_width() - 1;
	if (ly < 0) ly = 0; else if (ly >= ow->get_height()) ly = ow->get_height() - 1;
	omf_SDLSetMousePosition(short(lx), short(ly));
	if (getenv("OMT_PAD_LOG")) SDL_Log("OMT pad: pointer %d,%d", lx, ly);
}

// Test hook: OMT_FAKE_PAD="<tick>:<control>=<value>,..." attaches an SDL virtual game controller
// ("OMT fake pad") and sets its controls at the given event-manager ticks (about one per frame).
// Controls are SDL's names (a, b, x, y, back, guide, start, leftstick, rightstick, leftshoulder,
// rightshoulder, dpup, dpdown, dpleft, dpright; leftx, lefty, rightx, righty, lefttrigger,
// righttrigger); buttons take 0/1, sticks -32768..32767, triggers 0..32767; lmouse / rmouse
// (0/1) click at the pointer, acback (0/1) is the Android back key. OMT_PAD_LOG=1 logs the gamepad keys, the pointer and the devices.
static SDL_Joystick *omg_fake_pad = NULL;
static const char *omg_fake_script = NULL;

static void omf_fake_pad_init(void)
{
	omg_fake_script = getenv("OMT_FAKE_PAD");
	if (!omg_fake_script || !*omg_fake_script) return;
	SDL_VirtualJoystickDesc d;
	SDL_zero(d);
	d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
	d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	d.naxes = SDL_CONTROLLER_AXIS_MAX;
	d.nbuttons = SDL_CONTROLLER_BUTTON_MISC1;		// a .. dpright
	d.axis_mask = (1u << d.naxes) - 1;
	d.button_mask = (1u << d.nbuttons) - 1;
	d.name = "OMT fake pad";
	int index = SDL_JoystickAttachVirtualEx(&d);
	if (index >= 0) omg_fake_pad = SDL_JoystickOpen(index);
	if (!omg_fake_pad) { SDL_Log("OMT_FAKE_PAD: %s", SDL_GetError()); return; }
	SDL_JoystickSetVirtualAxis(omg_fake_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);	// released
	SDL_JoystickSetVirtualAxis(omg_fake_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
	SDL_Log("OMT_FAKE_PAD: attached");
}

static void omf_push_app_event(Uint32 type)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = type;
	SDL_PushEvent(&e);
}

static Uint32 omf_resume_timer(Uint32, void *)
{
	omf_push_app_event(SDL_APP_WILLENTERFOREGROUND);
	omf_push_app_event(SDL_APP_DIDENTERFOREGROUND);
	return 0;
}

static void omf_fake_pad_step(int tick)
{
	if (!omg_fake_pad) return;
	for (const char *p = omg_fake_script; *p; )
	{
		char name[32];
		int t, v, n = 0;
		if (sscanf(p, "%d:%31[a-z0-9]=%d%n", &t, name, &v, &n) != 3 || n == 0) break;
		p += n;
		if (*p == ',') p++;
		if (t != tick) continue;
		if (strcmp(name, "lmouse") == 0 || strcmp(name, "rmouse") == 0)		// a click at the pointer
		{
			OMediaWindow *ow;
			SDL_Window *w = omf_game_window(&ow);
			SDL_Event e;
			SDL_zero(e);
			e.type = v ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
			e.button.windowID = w ? SDL_GetWindowID(w) : 0;
			e.button.button = name[0] == 'l' ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
			e.button.state = v ? SDL_PRESSED : SDL_RELEASED;
			e.button.clicks = 1;
			SDL_GetMouseState(&e.button.x, &e.button.y);
			SDL_PushEvent(&e);
			continue;
		}
		if (strcmp(name, "suspend") == 0)		// port test: background for v ms (as Android's pause), then back
		{
			omf_push_app_event(SDL_APP_WILLENTERBACKGROUND);
			omf_push_app_event(SDL_APP_DIDENTERBACKGROUND);
			SDL_AddTimer(Uint32(v > 0 ? v : 1), omf_resume_timer, NULL);
			continue;
		}
		if (strcmp(name, "glreset") == 0)		// port test: as if the GL context had been lost
		{
			omf_push_app_event(SDL_RENDER_DEVICE_RESET);
			continue;
		}
		if (strcmp(name, "acback") == 0)		// the Android back button
		{
			OMediaWindow *ow;
			SDL_Window *w = omf_game_window(&ow);
			SDL_Event e;
			SDL_zero(e);
			e.type = v ? SDL_KEYDOWN : SDL_KEYUP;
			e.key.windowID = w ? SDL_GetWindowID(w) : 0;
			e.key.state = v ? SDL_PRESSED : SDL_RELEASED;
			e.key.keysym.scancode = SDL_SCANCODE_AC_BACK;
			e.key.keysym.sym = SDLK_AC_BACK;
			SDL_PushEvent(&e);
			continue;
		}
		SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(name);
		SDL_GameControllerAxis a = SDL_GameControllerGetAxisFromString(name);
		if (b != SDL_CONTROLLER_BUTTON_INVALID) SDL_JoystickSetVirtualButton(omg_fake_pad, b, Uint8(v != 0));
		else if (a == SDL_CONTROLLER_AXIS_TRIGGERLEFT || a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
			SDL_JoystickSetVirtualAxis(omg_fake_pad, a, Sint16(v * 2 - 32768 > 32767 ? 32767 : v * 2 - 32768));
		else if (a != SDL_CONTROLLER_AXIS_INVALID) SDL_JoystickSetVirtualAxis(omg_fake_pad, a, Sint16(v));
		if (getenv("OMT_PAD_LOG")) SDL_Log("OMT_FAKE_PAD: tick %d %s=%d", tick, name, v);
	}
}

// Called by omf_SDLInit (OMediaSDLVideo.cpp) once SDL is up.
void omf_SDLPadInit(void)
{
	SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
	omf_fake_pad_init();
}

// Once per event-manager tick, before the events are read.
static void omf_pad_tick(void)
{
	static int tick = 0;
	static Uint64 last = 0;
	Uint64 now = SDL_GetPerformanceCounter();
	float dt = last ? float(double(now - last) / double(SDL_GetPerformanceFrequency())) : 0.0f;
	if (dt > 0.1f) dt = 0.1f;
	last = now;
	omf_fake_pad_step(++tick);
	SDL_PumpEvents();							// the controllers' state of now, as the HID poll will read it
	omf_pad_keys();
	omf_pad_pointer(dt);
}

// ---------------------------------------------------------------------------
// port: suspend. When the app goes to the background (Android: the screen turns off, the Thor's
// lid closes, another app comes up; the desktop: the window is minimised), the mix is paused and
// OMT's clock stops. On Android SDL itself then blocks the main loop in SDL_PumpEvents until the
// app comes back (SDL_HINT_ANDROID_BLOCK_ON_PAUSE, its default) and restores the GL context; on
// the desktop the loop waits here for the window to come back. The game pauses itself from the
// same events (CAweReal: the original's WM_ACTIVATEAPP handling). docs/controls.md.

void omf_SDLAudioPause(bool pause);			// OMediaSDLSound.cpp
void omf_SDLContextReset(void);				// OMediaSDLVideo.cpp

static bool s_suspended = false, s_clock_paused = false;

static void omf_suspend(bool on)
{
	if (on == s_suspended) return;
	s_suspended = on;
	SDL_Log("port: %s", on ? "suspended (in the background)" : "resumed");
	omf_SDLAudioPause(on);
	if (on && !OMediaTimeCounter::is_paused_millisecs())
	{
		OMediaTimeCounter::pause_millisecs(true);
		s_clock_paused = true;
	}
	else if (!on && s_clock_paused)
	{
		OMediaTimeCounter::pause_millisecs(false);
		s_clock_paused = false;
	}
}

// SDL's app / window events that suspend or resume (before the game's own filter sees them).
static void omf_lifecycle(SDL_Event &se)
{
	if (se.type == SDL_APP_WILLENTERBACKGROUND) omf_suspend(true);
	else if (se.type == SDL_APP_DIDENTERFOREGROUND) omf_suspend(false);
	else if (se.type == SDL_RENDER_DEVICE_RESET) omf_SDLContextReset();
	else if (se.type == SDL_WINDOWEVENT && se.window.event == SDL_WINDOWEVENT_MINIMIZED) omf_suspend(true);
	else if (se.type == SDL_WINDOWEVENT && se.window.event == SDL_WINDOWEVENT_RESTORED) omf_suspend(false);
	else if (se.type == SDL_QUIT) omf_suspend(false);
}

void OMediaEventManager::spend_time(void)
{
	SDL_Event se;
	omf_pad_tick();
	for (;;)
	{
		int got = SDL_PollEvent(&se);
#ifndef __ANDROID__
		// port: suspended (window minimised): sleep in the event queue until it comes back
		if (!got && s_suspended) got = SDL_WaitEvent(&se);
#endif
		if (!got) break;
		if (se.type == SDL_CONTROLLERDEVICEADDED) omf_pad_added(se.cdevice.which);
		else if (se.type == SDL_CONTROLLERDEVICEREMOVED) omf_pad_removed(se.cdevice.which);
		omf_lifecycle(se);
		if (filter_event(&se)) continue;
		if (se.type == SDL_QUIT)
		{
			if (OMediaSupervisor::get_main_supervisor())
				OMediaSupervisor::get_main_supervisor()->listen_to_message(omsg_Quit);
			continue;
		}
		OMediaEvent ev;
		osevent_to_omtevent(&se, &ev);
		// OMT 2002's window procedure (OMediaWinEventManager.cpp, omt_WinMessageHandler) also sent
		// every translated event to the main supervisor as omsg_Event; games read the mouse buttons
		// there. Port: only the mouse buttons are sent, the window events are handled above.
		if ((ev.type == omtet_LMouseDown || ev.type == omtet_LMouseUp ||
		     ev.type == omtet_RMouseDown || ev.type == omtet_RMouseUp) && OMediaSupervisor::get_main_supervisor())
			OMediaSupervisor::get_main_supervisor()->listen_to_message(omsg_Event, &ev);
	}
}

// ---------------------------------------------------------------------------
// Input engine: HID devices backed by SDL state polling

class OMediaSDLKeyElement : public OMediaHIDElement
{
	public:
	OMediaSDLKeyElement(OMediaHumanInterfaceDevice *d, SDL_Scancode sc) : OMediaHIDElement(d), scancode(sc)
	{
		type = omhideltc_Button;
		name = SDL_GetScancodeName(sc);
	}
	virtual void read_data(OMediaHIDElementData &data)
	{
		const Uint8 *ks = SDL_GetKeyboardState(NULL);
		data.down = ks[scancode] != 0;
	}
	SDL_Scancode scancode;
};

class OMediaSDLMouseElement : public OMediaHIDElement
{
	public:
	OMediaSDLMouseElement(OMediaHumanInterfaceDevice *d, int which, const char *n) : OMediaHIDElement(d), which(which)
	{
		type = which < 2 ? omhideltc_Delta : omhideltc_Button;
		axis_type = which == 0 ? omhtdat_X : (which == 1 ? omhtdat_Y : omhtdat_Null);
		name = n;
	}
	virtual void read_data(OMediaHIDElementData &data)
	{
		int dx, dy;
		if (which < 2) { SDL_GetRelativeMouseState(&dx, &dy); data.delta = float(which == 0 ? dx : dy); }
		else data.down = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(which - 1)) != 0;
	}
	int which;    // 0=x 1=y 2..=button (2 -> SDL_BUTTON_LEFT)
};

// ---------------------------------------------------------------------------
// Game controllers and joysticks
//
// They are described the way OMT 2002's DirectInput engine (OMT2.dll: OMediaDXHID::lock, its
// EnumObjects callback and OMediaDXHIDElement::read_data) described a DirectInput joystick,
// because games written against OMT 2002 read exactly that:
//
//  * device type omhidtc_Joystick (DirectInput gamepads and joysticks both became Joystick), with
//    a non-empty product_name;
//  * one element per DirectInput object, in c_dfDIJoystick order: the axes, the POV hat, the
//    buttons;
//  * axis elements: omhideltc_Axis with axis_type X, Y, Z, rX, rY or rZ (DirectInput sliders:
//    omhtdat_Null). The value is the 2002 formula ((v - (max - min) / 2) / (max - min)) * 2 over
//    the DirectInput range 0..65535, so full travel is -1..+1, and it is negated for the Y axis
//    only: stick up is +1 on Y, while rY keeps DirectInput's down-is-positive;
//  * the POV hat: omhideltc_POV, angle in degrees clockwise from up (DirectInput's hundredths
//    times 0.01), -1 when centred;
//  * buttons: omhideltc_Button, data.down. OMT 2002 parsed the button number out of the
//    DirectInput name ("Button 3" -> 3) into the 16-bit member of the element's key union
//    (2002 offset +0x24, 2.5's command_key); games use it to index their button arrays, so it
//    is set here too. Other elements carry -1 there, as in 2002.
//
// SDL game controller -> elements (any controller SDL has a mapping for; on Android this
// includes built-in gamepads such as the AYN Thor's):
//
//   leftx          X Axis       axis X     -1 left .. +1 right
//   lefty          Y Axis       axis Y     +1 up .. -1 down (negated, as OMT 2002 did)
//                  (port: the d-pad also drives X/Y at full travel, see read_data)
//   lefttrigger    Z Axis       axis Z     -1 released .. +1 pressed (a DirectInput trigger axis)
//   rightx         X Rotation   axis rX    -1 left .. +1 right
//   righty         Y Rotation   axis rY    -1 up .. +1 down (not negated, as in 2002)
//   righttrigger   Z Rotation   axis rZ    -1 released .. +1 pressed
//   d-pad          Hat Switch   POV        0 up, 45 up-right, 90 right ... 315 up-left, -1 centred
//   buttons        Button n     see kPadButtonNumber below:
//                    a 0   b 1   x 2   y 3   start 4   leftstick 5   rightstick 6   guide 7
//                    rightshoulder 8   back 9   leftshoulder 10   misc1 11   paddle1..4 12..15
//                    touchpad 16
//
// The button numbers: DirectInput numbered a gamepad's buttons in whatever order its driver
// gave, so there is no one "Windows" order; games of the time read fixed numbers. Port: the
// numbers are laid out for Neutron2's fixed ones (game/src/Input.cpp; docs/controls.md), with
// the face buttons in keys.txt's JOY_01.. order: 0 action (a), 1 jump / speed (b), 2 brake (x),
// 3 inventory (y), 4 pause menu (start), 8 turn around (right shoulder), 9 help (back/select).
// The elements come in button-number order, as DirectInput listed them. A button or axis the
// controller does not have gets no element; the numbers stay the same.
//
// Other SDL joysticks (no game controller mapping) are exposed raw, like DirectInput did: axes 0..5
// as X, Y, Z, rX, rY, rZ (Y negated), further axes as sliders, every hat as a POV, the buttons
// numbered in SDL's order.

enum omt_SDLPadElementKind
{
	omsdlpe_GCAxis,			// game controller stick axis (-32768..32767)
	omsdlpe_GCTrigger,		// game controller trigger (0..32767)
	omsdlpe_GCButton,
	omsdlpe_GCDPad,			// the four d-pad buttons as one hat
	omsdlpe_JoyAxis,
	omsdlpe_JoyButton,
	omsdlpe_JoyHat
};

// OMT 2002's axis conversion (OMediaDXHIDElement::read_data, range from DIPROP_RANGE in lock).
static float omf_dx_axis(long v, long min, long max)
{
	float range = float(max - min);
	return ((float(v) - range * 0.5f) / range) * 2.0f;
}

// DirectInput POV angle of a hat in degrees (-1 centred). Opposite directions cancel out.
static float omf_hat_angle(bool up, bool right, bool down, bool left)
{
	static const float deg[3][3] = { { 225.0f, 180.0f, 135.0f },		// down-left, down, down-right
	                                 { 270.0f,  -1.0f,  90.0f },		// left, centred, right
	                                 { 315.0f,   0.0f,  45.0f } };		// up-left, up, up-right
	int x = int(right) - int(left), y = int(up) - int(down);
	return deg[y + 1][x + 1];
}

class OMediaSDLPadElement : public OMediaHIDElement
{
	public:
	OMediaSDLPadElement(OMediaHumanInterfaceDevice *d, SDL_GameController *gc, SDL_Joystick *js,
	                    omt_SDLPadElementKind kind, int idx, int number = -1)
		: OMediaHIDElement(d), gc(gc), js(js), kind(kind), idx(idx)
	{
		static const omt_HIDAxisType kAxis[6] = { omhtdat_X, omhtdat_Y, omhtdat_Z, omhtdat_rX, omhtdat_rY, omhtdat_rZ };
		static const char *const kAxisName[6] = { "X Axis", "Y Axis", "Z Axis", "X Rotation", "Y Rotation", "Z Rotation" };
		static const int kAxisUsage[6] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35 };	// USB generic desktop X..Rz
		char buf[64];

		command_key = (omt_CommandKey)0xffff;	// 2002: -1 = no button number
		switch (kind)
		{
			case omsdlpe_GCAxis:
			case omsdlpe_GCTrigger:
			case omsdlpe_JoyAxis:
				type = omhideltc_Axis;
				// number = DirectInput axis slot 0..5 (X, Y, Z, rX, rY, rZ), 6+ = slider
				if (number >= 0 && number < 6)
				{
					axis_type = kAxis[number];
					name = kAxisName[number];
					hdi_usage.usage_page = omhidupc_GenericDesktopControl;
					hdi_usage.desktop_type = (omt_HIDGenericDesktopUsage)kAxisUsage[number];	// (OMT's omhidgduc_Rz is 0x36, a typo)
				}
				else
				{
					axis_type = omhtdat_Null;
					name = "Slider";
					hdi_usage.usage_page = omhidupc_GenericDesktopControl;
					hdi_usage.desktop_type = omhidgduc_Slider;
				}
				break;

			case omsdlpe_GCButton:
			case omsdlpe_JoyButton:
				type = omhideltc_Button;
				command_key = (omt_CommandKey)number;
				if (kind == omsdlpe_GCButton)
					snprintf(buf, sizeof buf, "Button %d (%s)", number, SDL_GameControllerGetStringForButton((SDL_GameControllerButton)idx));
				else
					snprintf(buf, sizeof buf, "Button %d", number);
				name = buf;
				hdi_usage.usage_page = (omt_HIDUsagePage)0x09;		// USB button page, usage = button + 1
				hdi_usage.unknown_type = (unsigned short)(number + 1);
				break;

			case omsdlpe_GCDPad:
			case omsdlpe_JoyHat:
				type = omhideltc_POV;
				name = "Hat Switch";
				hdi_usage.usage_page = omhidupc_GenericDesktopControl;
				hdi_usage.desktop_type = omhidgduc_HatSwitch;
				break;
		}
	}

	virtual void read_data(OMediaHIDElementData &data)
	{
		switch (kind)
		{
			case omsdlpe_GCAxis:			// DirectInput value = SDL value + 32768, range 0..65535
				data.axis = omf_dx_axis(long(SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)idx)) + 32768, 0, 65535);
				if (axis_type == omhtdat_Y) data.axis = -data.axis;
				// Port: a held d-pad direction drives the left stick's X/Y at full travel, as on the
				// digital PC gamepads of 2002 (DirectInput reported their pads as X/Y axes). The game
				// only reads the X/Y axes; the hat element below is still there for completeness.
				if (idx == SDL_CONTROLLER_AXIS_LEFTX)
				{
					if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) data.axis = -1.0f;
					else if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) data.axis = 1.0f;
				}
				else if (idx == SDL_CONTROLLER_AXIS_LEFTY)
				{
					if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP)) data.axis = 1.0f;			// (+1 up after the negation)
					else if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) data.axis = -1.0f;
				}
				break;

			case omsdlpe_GCTrigger:			// range 0..32767
				data.axis = omf_dx_axis(SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)idx), 0, 32767);
				break;

			case omsdlpe_JoyAxis:
				data.axis = omf_dx_axis(long(SDL_JoystickGetAxis(js, idx)) + 32768, 0, 65535);
				if (axis_type == omhtdat_Y) data.axis = -data.axis;
				break;

			case omsdlpe_GCButton:
				data.down = SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)idx) != 0;
				break;

			case omsdlpe_JoyButton:
				data.down = SDL_JoystickGetButton(js, idx) != 0;
				break;

			case omsdlpe_GCDPad:
				data.angle = omf_hat_angle(SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0,
				                           SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0,
				                           SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0,
				                           SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0);
				break;

			case omsdlpe_JoyHat:
			{
				Uint8 h = SDL_JoystickGetHat(js, idx);
				data.angle = omf_hat_angle((h & SDL_HAT_UP) != 0, (h & SDL_HAT_RIGHT) != 0,
				                           (h & SDL_HAT_DOWN) != 0, (h & SDL_HAT_LEFT) != 0);
				break;
			}
		}
	}

	SDL_GameController *gc; SDL_Joystick *js; omt_SDLPadElementKind kind; int idx;
};

class OMediaSDLHID : public OMediaHumanInterfaceDevice
{
	public:
	OMediaSDLHID(OMediaInputEngine *e) : OMediaHumanInterfaceDevice(e), gc(NULL), js(NULL) { element_list = &elements; }
	virtual ~OMediaSDLHID()
	{
		for (omt_HIDElementList::iterator i = elements.begin(); i != elements.end(); i++) delete *i;
		if (gc) SDL_GameControllerClose(gc);	// the engine is recreated (games re-enumerate devices)
		if (js) SDL_JoystickClose(js);
	}
	virtual omt_HIDElementList *lock(void) { lock_count++; SDL_PumpEvents(); return &elements; }
	virtual void unlock(void) { lock_count--; }
	omt_HIDElementList elements;
	SDL_GameController *gc;
	SDL_Joystick *js;
};

// DirectInput button number of each SDL game controller button (-1: none; the d-pad is the hat).
static const int kPadButtonNumber[SDL_CONTROLLER_BUTTON_MAX] = {
	0,		// A
	1,		// B
	2,		// X
	3,		// Y
	9,		// BACK (select)
	7,		// GUIDE
	4,		// START
	5,		// LEFTSTICK
	6,		// RIGHTSTICK
	10,		// LEFTSHOULDER
	8,		// RIGHTSHOULDER
	-1, -1, -1, -1,	// DPAD_UP, DOWN, LEFT, RIGHT
	11,		// MISC1
	12, 13, 14, 15,	// PADDLE1..4
	16,		// TOUCHPAD
};

// The elements of a game controller, see the table above.
static void omf_add_controller(OMediaSDLHID *pad, SDL_GameController *gc)
{
	static const struct { SDL_GameControllerAxis axis; int slot; bool trigger; } kAxes[] = {
		{ SDL_CONTROLLER_AXIS_LEFTX, 0, false },        { SDL_CONTROLLER_AXIS_LEFTY, 1, false },
		{ SDL_CONTROLLER_AXIS_TRIGGERLEFT, 2, true },   { SDL_CONTROLLER_AXIS_RIGHTX, 3, false },
		{ SDL_CONTROLLER_AXIS_RIGHTY, 4, false },       { SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 5, true },
	};
	for (size_t i = 0; i < sizeof kAxes / sizeof kAxes[0]; i++)
		if (SDL_GameControllerHasAxis(gc, kAxes[i].axis))
			pad->elements.push_back(new OMediaSDLPadElement(pad, gc, NULL,
				kAxes[i].trigger ? omsdlpe_GCTrigger : omsdlpe_GCAxis, kAxes[i].axis, kAxes[i].slot));

	if (SDL_GameControllerHasButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP) || SDL_GameControllerHasButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ||
	    SDL_GameControllerHasButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || SDL_GameControllerHasButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
		pad->elements.push_back(new OMediaSDLPadElement(pad, gc, NULL, omsdlpe_GCDPad, 0));

	for (int number = 0; number < SDL_CONTROLLER_BUTTON_MAX; number++)
		for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
			if (kPadButtonNumber[b] == number && SDL_GameControllerHasButton(gc, (SDL_GameControllerButton)b))
				pad->elements.push_back(new OMediaSDLPadElement(pad, gc, NULL, omsdlpe_GCButton, b, number));
}

// The elements of a joystick without a game controller mapping.
static void omf_add_joystick(OMediaSDLHID *pad, SDL_Joystick *js)
{
	for (int a = 0; a < SDL_JoystickNumAxes(js); a++)
		pad->elements.push_back(new OMediaSDLPadElement(pad, NULL, js, omsdlpe_JoyAxis, a, a));
	for (int h = 0; h < SDL_JoystickNumHats(js); h++)
		pad->elements.push_back(new OMediaSDLPadElement(pad, NULL, js, omsdlpe_JoyHat, h));
	for (int b = 0; b < SDL_JoystickNumButtons(js); b++)
		pad->elements.push_back(new OMediaSDLPadElement(pad, NULL, js, omsdlpe_JoyButton, b, b));
}

static bool omg_logged_pads = false;

class OMediaSDLInputEngine : public OMediaInputEngine
{
	public:
	OMediaSDLInputEngine(OMediaWindow *master) : OMediaInputEngine(ommeic_OS, master)
	{
		OMediaSDLHID *kb = new OMediaSDLHID(this);
		kb->type = omhidtc_Keyboard; kb->product_name = kb->instance_name = "Keyboard";
		for (int sc = SDL_SCANCODE_A; sc < SDL_NUM_SCANCODES; sc++)
			if (SDL_GetScancodeName((SDL_Scancode)sc)[0])
				kb->elements.push_back(new OMediaSDLKeyElement(kb, (SDL_Scancode)sc));
		hdi_list.push_back(kb);

		OMediaSDLHID *ms = new OMediaSDLHID(this);
		ms->type = omhidtc_Mouse; ms->product_name = ms->instance_name = "Mouse";
		ms->elements.push_back(new OMediaSDLMouseElement(ms, 0, "X Axis"));
		ms->elements.push_back(new OMediaSDLMouseElement(ms, 1, "Y Axis"));
		ms->elements.push_back(new OMediaSDLMouseElement(ms, 2, "Button 0"));
		ms->elements.push_back(new OMediaSDLMouseElement(ms, 4, "Button 1"));
		ms->elements.push_back(new OMediaSDLMouseElement(ms, 3, "Button 2"));
		hdi_list.push_back(ms);

		// Game controllers, then the other joysticks, each in SDL's device order (see the element
		// table above). Port: DirectInput listed devices in its own order; games such as Neutron2
		// take the first joystick, so a mapped gamepad (the Thor's built-in one) goes first, ahead
		// of any other input device Android reports as a joystick.
		if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER)) SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
		for (int pass = 0; pass < 2; pass++)
		for (int j = 0; j < SDL_NumJoysticks(); j++)
		{
			if ((SDL_IsGameController(j) != SDL_FALSE) != (pass == 0)) continue;
			OMediaSDLHID *pad = new OMediaSDLHID(this);
			const char *n = NULL;
			if (SDL_IsGameController(j) && (pad->gc = SDL_GameControllerOpen(j)) != NULL)
			{
				n = SDL_GameControllerName(pad->gc);
				omf_add_controller(pad, pad->gc);
			}
			else if ((pad->js = SDL_JoystickOpen(j)) != NULL)
			{
				n = SDL_JoystickName(pad->js);
				omf_add_joystick(pad, pad->js);
			}
			else
			{
				delete pad;
				continue;
			}
			pad->type = omhidtc_Joystick;
			pad->hdi_usage.usage_page = omhidupc_GenericDesktopControl;
			pad->hdi_usage.desktop_type = pad->gc ? omhidgduc_GamePad : omhidgduc_Joystick;
			// never empty: OMT 2002 games may pick a device by comparing names
			pad->product_name = pad->instance_name = (n && n[0]) ? n : (pad->gc ? "Controller" : "Joystick");
			hdi_list.push_back(pad);
			if (!omg_logged_pads)
			{
				SDL_Log("OMT input: %s \"%s\", %d elements", pad->gc ? "game controller" : "joystick",
				        pad->product_name.c_str(), (int)pad->elements.size());
				if (getenv("OMT_PAD_LOG"))
					for (omt_HIDElementList::iterator i = pad->elements.begin(); i != pad->elements.end(); i++)
						SDL_Log("OMT input:   %s", (*i)->name.c_str());
			}
		}
		omg_logged_pads = true;		// (games make a new input engine each time they look for devices)
	}

	virtual bool mouse_down(void) { return (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0; }

	virtual void get_sysmouse_position(short &x, short &y)
	{
		int mx, my; SDL_GetMouseState(&mx, &my);
		OMediaWindow *w = master_window;
		int lx = mx, ly = my;
		if (w) omf_mouse_to_logical(w, mx, my, lx, ly);
		x = lx; y = ly;
	}

	virtual omt_CommandKey get_command_keys_status(void) { return omf_cmdkeys(); }
};

OMediaInputEngine *omf_SDLCreateInputEngine(OMediaWindow *master) { return new OMediaSDLInputEngine(master); }
