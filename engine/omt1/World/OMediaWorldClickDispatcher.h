/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMediaWorldClickDispatcher: listens to the event manager's mouse button messages and turns
    them into clicks on the world(s): the first broadcaster (+0x14) is sent omsg_MouseTrack with
    an OMediaWorldClickInfo that the worlds fill in (what is under the pointer); when something
    was found (always for a button up) the second broadcaster (+0x24) is sent
    omsg_ViewPortClicked (button down) or omsg_ViewPortClicked_Up with the same info. Worlds
    listen to the first, game code to the second.                                             */
#pragma once
#ifndef OMEDIA1_WorldClickDispatcher_H
#define OMEDIA1_WorldClickDispatcher_H

#include "OMediaTypes.h"
#include "OMediaBroadcaster.h"
#include "OMediaListener.h"

class OMediaWorldClickDispatcher : public OMediaListener
{
	public:
	omtshared OMediaWorldClickDispatcher();					// 0x1039aa30
	omtshared virtual ~OMediaWorldClickDispatcher();		// 0x1039aad0

	omtshared virtual void listen_to_message(omt_Message msg, void *param = NULL);		// 0x1039ab30

	inline OMediaBroadcaster *get_pick_broadcaster(void) {return &pick;}		// +0x14
	inline OMediaBroadcaster *get_click_broadcaster(void) {return &click;}	// +0x24
	inline void set_offset(short x, short y) {offset_x = x; offset_y = y;}		// +0x34 +0x36

	protected:
	OMediaBroadcaster	pick;			// +0x14
	OMediaBroadcaster	click;			// +0x24
	short				offset_x, offset_y;
};

#endif
