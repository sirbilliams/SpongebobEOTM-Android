/*  OMT 1.x input manager and input driver (OMT.dll, Aug 2002), on the SDL layer.  LGPL (OMT 2.5 base).

    OMT 2.5 has OMediaInputEngine, made per window; OMT.dll has one OMediaInputManager::input_manager
    holding one OMediaInputDriver, which sb2.exe asks for the mouse position (vtable +8, +0xc), the
    mouse button, the command keys and the human interface devices (list at driver + 0x10).
    OMediaWinInputDriver read the cursor and keys with USER32; OMediaDXInputDriver added DirectInput
    devices. Here one driver reads the pointer from SDL and takes its HID devices (keyboard, mouse,
    game controllers) from the SDL layer's input engine.

    The pointer is in the game window's logical coordinates (640x480 for sb2.exe), whatever the
    window or screen size is, by sdl/OMediaSDLVideo.cpp's omf_SDLWindowToLogical:
      mouse            SDL's mouse position (desktop).
      touch (Android)  the finger, as an absolute position on the game window: a finger down is a
                       left button down at that point, moving it moves the pointer. The finger is
                       followed through SDL's finger events (so that it also works where SDL does
                       not turn touches into mouse events), and it is the pointer while it is down;
                       afterwards the pointer stays where the finger left it.
      game controller  the SDL layer moves the pointer (right stick: omf_SDLSetMousePosition) and
                       clicks; sb2.exe's own joystick code reads the controller's HID elements
                       (left stick / D-pad move the pointer, button 0 clicks).               */

#pragma once
#ifndef OMEDIA_InputManager_H
#define OMEDIA_InputManager_H

#include "OMediaTypes.h"
#include "OMediaRect.h"
#include "OMediaInputEngine.h"

class OMediaCustomMouse;
class OMediaWindow;

class OMediaInputDriver
{
	public:
	// * Constructor/Destructor
	omtshared OMediaInputDriver(bool direct_input = false);
	omtshared virtual ~OMediaInputDriver();

	// * Mouse (the button is the left one, or a finger)
	omtshared virtual bool mouse_down(void);
	omtshared virtual void get_mouse_position(short &x, short &y);
	omtshared virtual void set_mouse_position(short x, short y);

	// * Command keys
	omtshared virtual unsigned short get_command_keys_status(void);

	// * Custom mouse: a pointer moved by how far the system pointer moves, kept inside a rectangle,
	//   which re-centres the system pointer (mouse look). It hides the system pointer.
	omtshared virtual void install_custom_mouse(OMediaRect *bounds);
	omtshared virtual void remove_custom_mouse(void);
	omtshared virtual void set_custom_mouse_bounds(OMediaRect *bounds);

	// * System pointer (mouse offset is not applied)
	omtshared virtual void get_sysmouse_position(short &x, short &y);
	omtshared virtual void set_sysmouse_position(short x, short y);

	// * Mouse offset, added by get_mouse_position
	inline void set_mouse_offset(short mx, short my) {mouse_offsetx = mx; mouse_offsety = my;}

	// * port: the window whose coordinates the pointer is in (default: the window OpenGL draws into)
	omtshared static void set_game_window(OMediaWindow *w);

	// * Human interface devices (sb2.exe walks driver + 0x10). The list is made on first use.
	omtshared omt_HIDList *get_hid(void);
	omtshared void rescan_hid(void);			// after a controller was plugged in or out

	protected:
	OMediaCustomMouse		*custom_mouse;
	short					mouse_offsetx, mouse_offsety;
	bool					direct_input;
	OMediaInputEngine		*hid_engine;
};

class OMediaInputManager
{
	public:
	omtshared OMediaInputManager();
	omtshared virtual ~OMediaInputManager();

	inline OMediaInputDriver *get_driver(void) {return driver;}
	inline static OMediaInputManager *get_input_manager(void) {return &input_manager;}

	protected:
	OMediaInputDriver		*driver;
	omtshared static OMediaInputManager	input_manager;
};

#endif
