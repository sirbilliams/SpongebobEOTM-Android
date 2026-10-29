/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaEventManager: turns the operating system's mouse and keyboard events into OMediaEvents
    and sends them: the mouse buttons to the mouse broadcaster (+0x0c), characters and keys to
    the key broadcaster (+0x1c), or only to the focus listener (+0x2c, a text field) when there is
    one. OMT.dll pumped the Win32 message queue (spend_time, osevent_to_omtevent); here the SDL
    events are translated instead (port).
    Messages: omsg_MouseClick (-102) / omsg_MouseClick_Up (-103) with the mouse button event,
    omsg_KeyDown (-104) with the character or key, omsg_KeyUp (-105) with the raw key,
    omsg_LostFocus (-109) to a focus listener that a click took the focus from.                   */
#pragma once
#ifndef OMEDIA1_EventManager_H
#define OMEDIA1_EventManager_H

#include "OMediaTypes.h"
#include "OMediaBroadcaster.h"
#include "OMediaListener.h"
#include "OMediaPeriodical.h"
#include "OMediaDefMessages.h"
#include <SDL.h>

// OMT.dll's OMediaEvent (the part the engine fills): type, modifiers, key, character, pointer
enum omt_EventType
{
	omcet1_Nothing = 0, omcet1_MouseDown = 1, omcet1_MouseUp = 2, omcet1_RightMouseDown = 3,
	omcet1_RightMouseUp = 4, omcet1_Char = 5, omcet1_Key = 6, omcet1_Update = 7, omcet1_Redraw = 9
};
enum omt_EventModifier
{
	omcem_Shift = 1, omcem_Command = 2, omcem_Control = 4, omcem_Alt = 8
};
// Keys that are not characters (OMediaEvent +0x08)
enum omt_EventKey
{
	omck_None = 0, omck_Escape = 1, omck_Backspace = 2, omck_Enter = 3, omck_Delete = 4,
	omck_Left = 6, omck_Right = 7, omck_Up = 8, omck_Down = 9, omck_Tab = 10,
	omck_PageUp = 11, omck_PageDown = 12, omck_F1 = 13		// F1..F12 = 13..24
};

class OMediaEvent
{
	public:
	int				type;			// +0x00 omt_EventType
	unsigned short	modifiers;		// +0x04 omt_EventModifier bits
	int				key;			// +0x08 omt_EventKey, 0 for a character
	char			character;		// +0x0c
	int				x, y;			// +0x10 +0x14 pointer, in screen coordinates
	OMediaEvent() {type = 0; modifiers = 0; key = 0; character = 0; x = y = 0;}
};

class OMediaEventManager : public OMediaPeriodical
{
	public:
	omtshared OMediaEventManager();					// 0x10390a20
	omtshared virtual ~OMediaEventManager();			// 0x10390ab0

	omtshared virtual void spend_time(void);			// 0x10390b40  (OMT.dll: PeekMessage / GetMessage / Dispatch)

	// OMT.dll: osevent_to_omtevent(MSG*, OMediaEvent*), then the sends above. port: an SDL event.
	// Returns false when the event is not one the engine uses.
	omtshared virtual bool osevent_to_omtevent(const SDL_Event *e, OMediaEvent *ev);	// 0x10390bb0
	omtshared void handle_sdl_event(const SDL_Event *e);

	// The message the key events go to instead of the key broadcaster
	inline OMediaListener *get_focus_listener(void) {return focus;}
	inline void set_focus_listener(OMediaListener *l) {focus = l;}
	inline OMediaBroadcaster *get_mouse_broadcaster(void) {return &mouse_broadcaster;}
	inline OMediaBroadcaster *get_key_broadcaster(void) {return &key_broadcaster;}

	inline bool quit_requested(void) const {return quit;}
	inline static OMediaEventManager *get_event_manager(void) {return event_manager;}

	omtshared static OMediaEventManager		*event_manager;		// 0x10420d84

	protected:
	void fill_pointer(const SDL_Event *e, OMediaEvent *ev);		// 0x10390f70
	static int map_key(SDL_Keycode k);							// 0x10390ff0 (virtual-key table)

	OMediaBroadcaster	mouse_broadcaster;		// +0x0c
	OMediaBroadcaster	key_broadcaster;		// +0x1c
	OMediaListener		*focus;					// +0x2c
	bool				quit;					// port: SDL_QUIT
};

#endif
