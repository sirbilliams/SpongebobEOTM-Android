/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaEventManager.h"
#include "OMediaDisplay.h"
#include "OMediaGLBackend.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

OMediaEventManager *OMediaEventManager::event_manager = NULL;

// 0x10390a20
OMediaEventManager::OMediaEventManager()
{
	event_manager = this;
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");		// port: touch is translated below, once (poll)
	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
	focus = NULL;
	quit = false;
}

// 0x10390ab0
OMediaEventManager::~OMediaEventManager()
{
	if (event_manager == this) event_manager = NULL;
}

// port: window pixels -> logical screen pixels (the GL back end letterboxes the screen, omgl_present)
void OMediaEventManager::window_to_logical(int &px, int &py)
{
	omgl_window_to_logical(px, py);
}

// 0x10390f70: the pointer (OMT.dll asks the input driver) and the modifier keys
void OMediaEventManager::fill_pointer(const SDL_Event *e, OMediaEvent *ev)
{
	int px = 0, py = 0;
	if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) { px = e->button.x; py = e->button.y; }
	else SDL_GetMouseState(&px, &py);

	window_to_logical(px, py);
	ev->x = (short)px;
	ev->y = (short)py;

	SDL_Keymod m = SDL_GetModState();
	ev->modifiers = 0;
	if (m & KMOD_SHIFT) ev->modifiers |= omcem_Shift;
	if (m & KMOD_ALT) ev->modifiers |= omcem_Alt;
	if (m & KMOD_CTRL) ev->modifiers |= omcem_Control;
	if (m & KMOD_GUI) ev->modifiers |= omcem_Command;
}

// 0x10390ff0: the virtual-key table, on SDL keys
int OMediaEventManager::map_key(SDL_Keycode k)
{
	switch (k)
	{
		case SDLK_ESCAPE: return omck_Escape;
		case SDLK_BACKSPACE: return omck_Backspace;
		case SDLK_RETURN: case SDLK_KP_ENTER: return omck_Enter;
		case SDLK_DELETE: return omck_Delete;
		case SDLK_LEFT: return omck_Left;
		case SDLK_RIGHT: return omck_Right;
		case SDLK_UP: return omck_Up;
		case SDLK_DOWN: return omck_Down;
		case SDLK_TAB: return omck_Tab;
		case SDLK_PAGEUP: return omck_PageUp;
		case SDLK_PAGEDOWN: return omck_PageDown;
		default: break;
	}
	if (k >= SDLK_F1 && k <= SDLK_F12) return omck_F1 + (int)(k - SDLK_F1);
	return omck_None;
}

// 0x10390bb0. A mouse button down also takes the focus away from a text field: the focus is
// cleared before the click is sent, and a field that did not take it back gets omsg_LostFocus.
bool OMediaEventManager::osevent_to_omtevent(const SDL_Event *e, OMediaEvent *ev)
{
	switch (e->type)
	{
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		{
			bool down = e->type == SDL_MOUSEBUTTONDOWN;
			if (e->button.button == SDL_BUTTON_LEFT) ev->type = down ? omcet1_MouseDown : omcet1_MouseUp;
			else if (e->button.button == SDL_BUTTON_RIGHT) ev->type = down ? omcet1_RightMouseDown : omcet1_RightMouseUp;
			else return false;
			fill_pointer(e, ev);
			return true;
		}
		case SDL_TEXTINPUT:		// port: WM_CHAR (a character; '\b' never comes as text)
		{
			const unsigned char *t = (const unsigned char*)e->text.text;
			if (!t[0] || t[0] == '\b') return false;
			ev->type = omcet1_Char;
			ev->character = (char)t[0];
			ev->key = 0;
			fill_pointer(e, ev);
			return true;
		}
		case SDL_KEYDOWN:		// WM_KEYDOWN: the keys that are not characters
		{
			int k = map_key(e->key.keysym.sym);
			if (!k) return false;		// port: OMT.dll also sends an event for unmapped keys, with key 0
			ev->type = omcet1_Char;
			ev->character = 0;
			ev->key = k;
			fill_pointer(e, ev);
			return true;
		}
		default: return false;
	}
}

void OMediaEventManager::handle_sdl_event(const SDL_Event *e)
{
	if (e->type == SDL_QUIT) { quit = true; return; }
	OMediaEvent ev;
	if (!osevent_to_omtevent(e, &ev)) return;
	switch (ev.type)
	{
		case omcet1_MouseDown:
		case omcet1_RightMouseDown:
		{
			OMediaListener *saved = focus;
			focus = NULL;
			mouse_broadcaster.broadcast_message(omsg_MouseClick, &ev);
			if (focus != saved && saved) saved->listen_to_message(omsg_LostFocus, NULL);
			break;
		}
		case omcet1_MouseUp:
		case omcet1_RightMouseUp:
			mouse_broadcaster.broadcast_message(omsg_MouseClick_Up, &ev);
			break;
		case omcet1_Char:
			if (focus) focus->listen_to_message(omsg_KeyDown, &ev);
			else key_broadcaster.broadcast_message(omsg_KeyDown, &ev);
			break;
		default: break;
	}
}

// 0x10390b40: one message from the queue
void OMediaEventManager::spend_time(void)
{
	SDL_Event e;
	if (poll(&e)) handle_sdl_event(&e);
}

// ---------------------------------------------------------------------------------------------
// port: touch and gamepad pointer (see docs/controls.md)
// ---------------------------------------------------------------------------------------------
static std::deque<SDL_Event> pend;				// events made here, returned before SDL's own
static std::vector<SDL_GameController*> pads;
static bool pad_inited = false, in_frame = false;
static int pad_enabled = -1;
static unsigned frame_no = 0, last_ticks = 0;
static bool r3_down = false;			// the pointer's left button held by the pad (R3 or A)
static bool held_r3 = false, held_a = false;
static bool pad_pos_valid = false;
static float pad_x = 0, pad_y = 0, rest_x = 0, rest_y = 0;
static int warped_wx = -1000, warped_wy = -1000;
static bool finger_down = false;
static SDL_FingerID finger_id = 0;

static bool pad_cursor_enabled(void);
bool OMediaEventManager::pad_cursor_enabled(void) { return ::pad_cursor_enabled(); }
static bool pad_cursor_enabled(void)
{
	if (pad_enabled < 0)
	{
		const char *v = getenv("SB_PAD_CURSOR");
		pad_enabled = (v && (v[0] == '0' || v[0] == 'n' || v[0] == 'f')) ? 0 : 1;
	}
	return pad_enabled != 0;
}

bool OMediaEventManager::pad_button_down(void) { return r3_down; }

static void pad_open_all(void)
{
	if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER) && SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) return;
	for (int i = 0; i < SDL_NumJoysticks(); i++)
		if (SDL_IsGameController(i))
		{
			SDL_GameController *gc = SDL_GameControllerOpen(i);
			if (!gc) continue;
			bool have = false;
			for (size_t k = 0; k < pads.size(); k++) if (pads[k] == gc) have = true;
			if (!have) pads.push_back(gc);
		}
}

static void pad_removed(SDL_JoystickID id)
{
	for (size_t i = 0; i < pads.size(); i++)
		if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == id)
		{
			SDL_GameControllerClose(pads[i]);
			pads.erase(pads.begin() + i);
			return;
		}
}

static SDL_Window *game_window(void) { return SDL_GL_GetCurrentWindow(); }

// A pointer event at logical (lx, ly) -> window coordinates, clamped to the window.
static void logical_to_window_clamped(float lx, float ly, int &wx, int &wy)
{
	int ix = (int)floorf(lx), iy = (int)floorf(ly);
	omgl_logical_to_window(ix, iy);
	SDL_Window *w = game_window();
	int ww = 1, wh = 1;
	if (w) SDL_GetWindowSize(w, &ww, &wh);
	if (ix < 0) ix = 0; else if (ix > ww - 1) ix = ww - 1;
	if (iy < 0) iy = 0; else if (iy > wh - 1) iy = wh - 1;
	wx = ix; wy = iy;
}

static void push_mouse(Uint32 type, int wx, int wy, bool left)
{
	SDL_Event e;
	SDL_zero(e);
	e.type = type;
	if (type == SDL_MOUSEMOTION)
	{
		e.motion.x = wx; e.motion.y = wy;
		e.motion.state = left ? SDL_BUTTON_LMASK : 0;
	}
	else
	{
		e.button.button = SDL_BUTTON_LEFT;
		e.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
		e.button.clicks = 1;
		e.button.x = wx; e.button.y = wy;
	}
	if (getenv("OMT_PAD_LOG")) SDL_Log("pointer: frame %u type %d window %d,%d", frame_no, (int)type, wx, wy);
	pend.push_back(e);
}

static void warp(int wx, int wy)
{
	SDL_Window *w = game_window();
	if (w) SDL_WarpMouseInWindow(w, wx, wy);
	warped_wx = wx; warped_wy = wy;
}

// Right-stick vector (-1..1 each), after a 20% radial dead zone, on a square curve.
static void stick(float &sx, float &sy)
{
	float x = sx, y = sy;		// in: the raw value (a test's), plus every open pad
	for (size_t i = 0; i < pads.size(); i++)
	{
		x += SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
		y += SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
	}
	float r = sqrtf(x * x + y * y);
	if (r > 1) { x /= r; y /= r; r = 1; }
	if (r < 0.2f) { sx = sy = 0; return; }
	float s = (r - 0.2f) / 0.8f;
	s = s * s / r;
	sx = x * s; sy = y * s;
}

#ifndef __ANDROID__
// port: desktop tests only. SB_TEST_PAD="frame:rx=<-1..1>,ry=<>,r3=<0|1>;..." sets the fake right stick / R3 from that
// frame on; SB_TEST_TOUCH="frame:d|m|u:x:y,..." is a finger down / move / up at logical x,y. Frames count the
// event polls of the process (one per game frame).
static float t_rx = 0, t_ry = 0;
static bool t_r3 = false, t_pad = false, t_a = false, t_r3k = false;
static void test_hooks(unsigned frame)
{
	if (const char *pv = getenv("SB_TEST_PAD"))
	{
		for (const char *p = pv; p && *p;)
		{
			unsigned at = 0; int n = 0;
			if (sscanf(p, "%u:%n", &at, &n) == 1 && at == frame)
			{
				t_pad = true;
				const char *q = p + n;
				while (*q && *q != ';')
				{
					float v = 0;
					if (sscanf(q, "rx=%f", &v) == 1) t_rx = v;
					else if (sscanf(q, "ry=%f", &v) == 1) t_ry = v;
					else if (sscanf(q, "r3=%f", &v) == 1 || sscanf(q, "a=%f", &v) == 1)
					{
						bool d = v != 0;
						if (!strncmp(q, "a=", 2)) t_a = d; else t_r3k = d;
						d = t_a || t_r3k;
						if (d != t_r3 && pad_cursor_enabled())
						{
							int wx, wy; SDL_GetMouseState(&wx, &wy);
							if (pad_pos_valid) logical_to_window_clamped(pad_x, pad_y, wx, wy);
							push_mouse(d ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP, wx, wy, d);
							r3_down = d;
						}
						t_r3 = d;
					}
					while (*q && *q != ',' && *q != ';') q++;
					if (*q == ',') q++;
				}
			}
			p = strchr(p, ';'); if (p) ++p;
		}
	}
	if (const char *tv = getenv("SB_TEST_TOUCHPAD"))		// "frame:dx,dy,btn;..." the bottom touchpad: a relative move (logical units) and the button (0 up, 1 down, -1 unchanged)
	{
		for (const char *p = tv; p && *p;)
		{
			unsigned at = 0; float dx = 0, dy = 0; int b = -1, n = 0;
			if (sscanf(p, "%u:%f,%f,%d%n", &at, &dx, &dy, &b, &n) >= 3 && at == frame)
			{
				if (dx != 0 || dy != 0) OMediaEventManager::pad_cursor_delta(dx, dy);
				if (b >= 0) OMediaEventManager::pad_cursor_button(b != 0);
			}
			p = strchr(p, ';'); if (p) ++p;
		}
	}
	if (const char *tv = getenv("SB_TEST_TOUCH"))
	{
		for (const char *p = tv; p && *p;)
		{
			unsigned at = 0; char k = 0; int x = 0, y = 0;
			if (sscanf(p, "%u:%c:%d:%d", &at, &k, &x, &y) == 4 && at == frame)
			{
				int wx = x, wy = y; omgl_logical_to_window(wx, wy);
				int ww = 1, wh = 1; SDL_Window *w = game_window();
				if (w) SDL_GetWindowSize(w, &ww, &wh);
				SDL_Event e; SDL_zero(e);
				e.type = k == 'd' ? SDL_FINGERDOWN : (k == 'u' ? SDL_FINGERUP : SDL_FINGERMOTION);
				e.tfinger.touchId = 1; e.tfinger.fingerId = 7;
				e.tfinger.x = (wx + 0.5f) / ww; e.tfinger.y = (wy + 0.5f) / wh;
				SDL_PushEvent(&e);		// through SDL's queue, so poll() translates it like a real finger
			}
			p = strchr(p, ','); if (p) ++p;
		}
	}
}
#endif

// port: the bottom-screen touchpad (android Companion.nativeTouchpad -> pad_cursor_delta / pad_cursor_button). Relative moves and
// button edges queue up from the UI thread and are applied in order once a frame, through the same pointer state as the
// right stick (pad_x / pad_y, warp, push_mouse), so the two never fight. Independent of SB_PAD_CURSOR (a touch surface).
struct TpOp { int kind; float dx, dy; };		// kind 0 move (logical units), 1 button down, 2 button up
static std::deque<TpOp> tp_ops;
static pthread_mutex_t tp_lock = PTHREAD_MUTEX_INITIALIZER;
void OMediaEventManager::pad_cursor_delta(float dx, float dy)
{
	pthread_mutex_lock(&tp_lock);
	if (tp_ops.size() < 256) { TpOp o = {0, dx, dy}; tp_ops.push_back(o); }
	pthread_mutex_unlock(&tp_lock);
}
void OMediaEventManager::pad_cursor_button(bool down)
{
	pthread_mutex_lock(&tp_lock);
	if (tp_ops.size() < 256) { TpOp o = {down ? 1 : 2, 0, 0}; tp_ops.push_back(o); }
	pthread_mutex_unlock(&tp_lock);
}

// Makes pad_x / pad_y the pointer's logical position (first use, or a mouse / finger moved it since). false: no window.
static bool pad_sync_pos(void)
{
	if (!game_window()) return false;
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	if (!pad_pos_valid || abs(mx - warped_wx) > 2 || abs(my - warped_wy) > 2)
	{
		int lx = mx, ly = my;
		omgl_window_to_logical(lx, ly);
		pad_x = (float)lx; pad_y = (float)ly;
		pad_pos_valid = true;
	}
	return true;
}

static void tp_tick(void)
{
	std::deque<TpOp> ops;
	pthread_mutex_lock(&tp_lock);
	ops.swap(tp_ops);
	pthread_mutex_unlock(&tp_lock);
	if (ops.empty()) return;
	int lw = 640, lh = 480;
	omgl_logical_size(lw, lh);
	for (size_t i = 0; i < ops.size(); i++)
	{
		if (!pad_sync_pos()) return;
		if (ops[i].kind == 0)
		{
			pad_x += ops[i].dx; pad_y += ops[i].dy;
			if (pad_x < 0) pad_x = 0; else if (pad_x > lw - 1) pad_x = (float)(lw - 1);
			if (pad_y < 0) pad_y = 0; else if (pad_y > lh - 1) pad_y = (float)(lh - 1);
			int wx, wy;
			logical_to_window_clamped(pad_x, pad_y, wx, wy);
			warp(wx, wy);
			push_mouse(SDL_MOUSEMOTION, wx, wy, r3_down);
		}
		else
		{
			bool d = ops[i].kind == 1;
			if (!d) d = held_a || held_r3;			// the pad may still hold the button
			int wx, wy;
			logical_to_window_clamped(pad_x, pad_y, wx, wy);
			if (d != r3_down || ops[i].kind == 1) { r3_down = d; push_mouse(d ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP, wx, wy, d); }
		}
	}
}

// One tick of the pad pointer: the right stick moves the pointer, about 3/4 of the logical width per second at full tilt.
static void pad_tick(float dt)
{
	if (!pad_cursor_enabled()) return;
	float sx = 0, sy = 0;
#ifndef __ANDROID__
	if (t_pad) { sx = t_rx; sy = t_ry; }
#endif
	stick(sx, sy);
	if (sx == 0 && sy == 0) { rest_x = rest_y = 0; return; }
	SDL_Window *w = game_window();
	if (!w) return;
	int lw = 640, lh = 480;
	omgl_logical_size(lw, lh);
	pad_sync_pos();
	const float speed = 0.75f * lw;
	pad_x += sx * speed * dt;
	pad_y += sy * speed * dt;
	if (pad_x < 0) pad_x = 0; else if (pad_x > lw - 1) pad_x = (float)(lw - 1);
	if (pad_y < 0) pad_y = 0; else if (pad_y > lh - 1) pad_y = (float)(lh - 1);
	int wx, wy;
	logical_to_window_clamped(pad_x, pad_y, wx, wy);
	warp(wx, wy);
	push_mouse(SDL_MOUSEMOTION, wx, wy, r3_down);
}

static void begin_frame(void)
{
	in_frame = true;
	frame_no++;
	unsigned now = SDL_GetTicks();
	float dt = last_ticks ? (now - last_ticks) * 0.001f : 0;
	last_ticks = now;
	if (dt > 0.05f) dt = 0.05f;
#ifndef __ANDROID__
	if (getenv("SB_TEST_PAD") || getenv("SB_TEST_TOUCH") || getenv("SB_TEST_TOUCHPAD")) dt = 1.0f / 60;		// deterministic
	test_hooks(frame_no);
#endif
	if (!pad_inited) { pad_inited = true; if (pad_cursor_enabled()) pad_open_all(); }
	pad_tick(dt);
	tp_tick();
}

// Turns one SDL event into what the game loops see; false: drop it.
static bool translate(SDL_Event *e)
{
	switch (e->type)
	{
		case SDL_MOUSEMOTION:
			return e->motion.which != SDL_TOUCH_MOUSEID;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			return e->button.which != SDL_TOUCH_MOUSEID;
		case SDL_FINGERDOWN:
		case SDL_FINGERMOTION:
		case SDL_FINGERUP:
		{
			if (e->type == SDL_FINGERDOWN)
			{
				if (finger_down) return true;			// a second finger: ignored
				finger_down = true; finger_id = e->tfinger.fingerId;
			}
			else if (!finger_down || e->tfinger.fingerId != finger_id) return true;
			SDL_Window *w = game_window();
			if (!w) return true;
			int ww = 1, wh = 1; SDL_GetWindowSize(w, &ww, &wh);
			int wx = (int)(e->tfinger.x * ww), wy = (int)(e->tfinger.y * wh);
			if (wx < 0) wx = 0; else if (wx > ww - 1) wx = ww - 1;
			if (wy < 0) wy = 0; else if (wy > wh - 1) wy = wh - 1;
			warp(wx, wy);
			pad_pos_valid = false;
			push_mouse(SDL_MOUSEMOTION, wx, wy, finger_down);
			if (e->type == SDL_FINGERDOWN) push_mouse(SDL_MOUSEBUTTONDOWN, wx, wy, true);
			if (e->type == SDL_FINGERUP) { push_mouse(SDL_MOUSEBUTTONUP, wx, wy, false); finger_down = false; }
			return true;
		}
		case SDL_CONTROLLERDEVICEADDED:
			if (pad_cursor_enabled() && pad_inited) pad_open_all();
			return true;
		case SDL_CONTROLLERDEVICEREMOVED:
			pad_removed(e->cdevice.which);
			if (r3_down && pads.empty()) { r3_down = false; int wx, wy; logical_to_window_clamped(pad_x, pad_y, wx, wy); push_mouse(SDL_MOUSEBUTTONUP, wx, wy, false); }
			return true;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			if ((e->cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK || e->cbutton.button == SDL_CONTROLLER_BUTTON_A) && pad_cursor_enabled())
			{
				bool d = e->type == SDL_CONTROLLERBUTTONDOWN;
				if (e->cbutton.button == SDL_CONTROLLER_BUTTON_A) held_a = d; else held_r3 = d;
				d = held_a || held_r3;						// R3 and A are both the left button
				if (d == r3_down) return true;
				int mx, my; SDL_GetMouseState(&mx, &my);
				int wx = mx, wy = my;
				if (pad_pos_valid) logical_to_window_clamped(pad_x, pad_y, wx, wy);
				r3_down = d;
				push_mouse(d ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP, wx, wy, d);
			}
			return true;
		default:
			return true;
	}
}

bool OMediaEventManager::poll(SDL_Event *out)
{
	if (!in_frame) begin_frame();
	for (;;)
	{
		if (!pend.empty()) { *out = pend.front(); pend.pop_front(); return true; }
		SDL_Event e;
		if (!SDL_PollEvent(&e)) { in_frame = false; return false; }
		if (translate(&e)) { *out = e; return true; }
	}
}
