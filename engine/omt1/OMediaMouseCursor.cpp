/*  OMT 1.x mouse cursor (OMT.dll, 0x10395090 ...), on SDL.  LGPL (OMT 2.5 base).
    hide / show count the way OMT.dll does: the system cursor goes away at the first hide and
    comes back when show has undone every hide. */

#include "OMediaMouseCursor.h"
#include "OMediaSDLRetarget.h"

omt_MouseCursorShape OMediaMouseCursor::shape = omcmct_Arrow;
int OMediaMouseCursor::hide_count = 0;

// 0x10395090: LoadCursor(IDC_ARROW) / LoadCursor(IDC_WAIT) + SetCursor. Like OMT.dll it does not
// remember the shape: get_type keeps returning what it was set to at start (the arrow).
void OMediaMouseCursor::set_shape(omt_MouseCursorShape s)
{
	SDL_SystemCursor id;
	if (s == omcmct_Arrow) id = SDL_SYSTEM_CURSOR_ARROW;
	else if (s == omcmct_Clock) id = SDL_SYSTEM_CURSOR_WAIT;
	else return;
	static SDL_Cursor *cursors[2] = { NULL, NULL };
	SDL_Cursor *&c = cursors[s == omcmct_Arrow ? 0 : 1];
	if (!c) c = SDL_CreateSystemCursor(id);
	if (c) SDL_SetCursor(c);
}

// 0x103950d0
void OMediaMouseCursor::hide(void)
{
	if (hide_count == 0) SDL_ShowCursor(SDL_DISABLE);
	hide_count++;
}

// 0x103950f0
void OMediaMouseCursor::show(void)
{
	if (hide_count < 1) { hide_count = 0; return; }
	hide_count--;
	if (hide_count == 0) SDL_ShowCursor(SDL_ENABLE);
}

// 0x10395120
void OMediaMouseCursor::force_show(void)
{
	hide_count = 0;
	SDL_ShowCursor(SDL_ENABLE);
}
