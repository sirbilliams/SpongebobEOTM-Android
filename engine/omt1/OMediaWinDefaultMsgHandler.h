/*  OMT 1.x: the window procedure's message broadcaster (OMT.dll, OMediaWinDefaultMsgHandler).  LGPL.
    OMT.dll's window procedure sends the window messages it does not handle itself to
    msg_broadcaster (WinSock and IPX use it for their asynchronous socket messages; sb2.exe adds
    one listener). Nothing on SDL produces such messages, so here it is a broadcaster that stays
    quiet; the listeners still link and unlink as usual. */

#pragma once
#ifndef OMEDIA_WinDefaultMsgHandler_H
#define OMEDIA_WinDefaultMsgHandler_H

#include "OMediaTypes.h"
#include "OMediaBroadcaster.h"

class OMediaWinDefaultMsgHandler
{
	public:
	omtshared static OMediaBroadcaster	msg_broadcaster;
	omtshared static void				*handler;		// port: the HWND of the game window (none)
};

#endif
