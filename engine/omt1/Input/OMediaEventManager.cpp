/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaEventManager.h"
#include "OMediaDisplay.h"

OMediaEventManager *OMediaEventManager::event_manager = NULL;

// 0x10390a20
OMediaEventManager::OMediaEventManager()
{
	event_manager = this;
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
	SDL_Window *win = SDL_GL_GetCurrentWindow();
	OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
	if (win && dev && dev->getwidth() > 0 && dev->getheight() > 0)
	{
		int ww, wh;
		SDL_GetWindowSize(win, &ww, &wh);
		float lw = dev->getwidth(), lh = dev->getheight();
		float sc = (float)ww / lw;
		if ((float)wh / lh < sc) sc = (float)wh / lh;
		float x0 = (ww - lw * sc) * 0.5f, y0 = (wh - lh * sc) * 0.5f;
		px = (int)((px - x0) / sc);
		py = (int)((py - y0) / sc);
	}
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
	if (SDL_PollEvent(&e)) handle_sdl_event(&e);
}
