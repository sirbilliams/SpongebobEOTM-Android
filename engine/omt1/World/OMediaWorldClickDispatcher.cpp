/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaWorldClickDispatcher.h"
#include "OMediaElement.h"
#include "OMediaEventManager.h"
#include "OMediaDefMessages.h"
#include <SDL.h>

// 0x1039aa30. Listens to the event manager's mouse button messages.
OMediaWorldClickDispatcher::OMediaWorldClickDispatcher()
{
	offset_x = 0;
	offset_y = 0;
	if (OMediaEventManager::get_event_manager())
		OMediaEventManager::get_event_manager()->get_mouse_broadcaster()->addlistener(this);
	else
		SDL_Log("OMediaWorldClickDispatcher: no event manager yet (OMT.dll would crash here)");
}

OMediaWorldClickDispatcher::~OMediaWorldClickDispatcher() {}		// 0x1039aad0

// 0x1039ab30
void OMediaWorldClickDispatcher::listen_to_message(omt_Message msg, void *param)
{
	if (msg != omsg_MouseClick && msg != omsg_MouseClick_Up) return;
	OMediaEvent *ev = (OMediaEvent*)param;
	OMediaWorldClickInfo ci;
	ci.type = omcct_Nothing;
	ci.viewport = NULL;
	ci.element = NULL;
	ci.field_c = 0;
	ci.canvas = NULL;
	ci.x = (short)(ev->x - offset_x);
	ci.y = (short)(ev->y - offset_y);
	ci.click_x = ci.x;
	ci.click_y = ci.y;
	ci.modifiers = ev->modifiers;
	ci.event_type = ev->type;
	ci.right_button = (ev->type != omcet1_MouseDown) ? 1 : 0;
	pick.broadcast_message(omsg_MouseTrack, &ci);
	if (ci.type != omcct_Nothing || msg == omsg_MouseClick_Up)
	{
		if (msg == omsg_MouseClick) click.broadcast_message(omsg_ViewPortClicked, &ci);
		else click.broadcast_message(omsg_ViewPortClicked_Up, &ci);
	}
}
