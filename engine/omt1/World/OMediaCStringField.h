/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaCStringField: an editable text field (a 2D element). It draws a bevelled box with the
    string in an OMediaQuickText font and a blinking cursor. It is an element, a broadcaster
    (it sends the messages set with set_messages), a listener (to the event manager's key
    messages once it has the focus) and a periodical (the cursor, and the select delay).
    Clicking it selects it (at once, or after a delay: see set_select_delay); characters typed
    are added at the end, backspace removes the last one, return / escape deselect it.
    The text can be filtered: any character, whole numbers, decimal numbers, letters and digits.
    Layout in OMT.dll: +0xc8 broadcaster, +0xd8 listener, +0xec periodical; +0xf4 idle,
    +0xf8 activate message, +0xfc click message, +0x100 deselect message, +0x104 width, +0x106
    height, +0x108 margin (3), +0x10a shine, +0x110 dark, +0x116 back, +0x11c cursor colours,
    +0x124 string, +0x134 font, +0x138 cursor shown, +0x13c cursor time, +0x140 filter,
    +0x144 background, +0x145 auto size, +0x146 double click, +0x148 maximum width (640),
    +0x14a cursor width (5), +0x14c last click, +0x150 owner, +0x154 click notifies,
    +0x158 select timer, +0x168 select delay, +0x16c waiting to be selected.                  */
#pragma once
#ifndef OMEDIA1_CStringField_H
#define OMEDIA1_CStringField_H

#include "OMediaUndiv2DElement.h"
#include "OMediaBroadcaster.h"
#include "OMediaListener.h"
#include "OMediaPeriodical.h"
#include "OMediaRGBColor.h"
#include "OMediaTimer.h"
#include <string>

class OMediaQuickText;
class OMediaDrawPort;
class OMediaEvent;

enum omt_StringFieldFilter
{
	omcsff_Any = 0, omcsff_Integer = 1, omcsff_Float = 2, omcsff_AlphaNumeric = 3
};

class OMediaCStringField : public OMediaUndiv2DElement, public OMediaBroadcaster,
						   public OMediaListener, public OMediaPeriodical
{
	public:
	omtshared OMediaCStringField();						// 0x100525c0
	omtshared virtual ~OMediaCStringField();				// 0x100527d0

	omtshared virtual void getsize(long &w, long &h);						// 0x10052a50  vt+0x5c
	omtshared virtual void process_click(OMediaWorldClickInfo *ci);		// 0x10052a70  vt+0x40
	omtshared virtual void listen_to_message(omt_Message msg, void *param = NULL);	// 0x10052cd0 (OMediaListener)
	omtshared virtual void spend_time(void);								// 0x10053060 (OMediaPeriodical)
	omtshared virtual void set_string(string s);							// 0x10052910  vt+0x98
	omtshared virtual void select(void);									// 0x10053150  vt+0x9c
	omtshared virtual void deselect(void);									// 0x100531c0  vt+0xa0

	inline const string &get_string(void) const {return text;}

	// (sb2.exe sets the rest through inline code)
	void set_font(OMediaQuickText *f);
	inline void set_dimensions(short w, short h) {width = w; height = h; moved = true;}
	inline void set_margin(short m) {margin = m; moved = true;}
	inline void set_max_width(short w) {max_width = w;}
	inline void set_auto_size(bool a) {auto_size = a;}
	inline void set_filter(omt_StringFieldFilter f) {filter = f;}
	inline void set_has_background(bool b) {has_background = b; moved = true;}
	inline void set_double_click(bool d) {double_click = d;}
	inline void set_click_notifies(bool n) {click_notifies = n;}
	inline void set_select_delay(float ms) {select_delay = ms;}
	inline void set_owner(void *o) {owner = o;}
	inline void set_messages(omt_Message activate, omt_Message click, omt_Message deselected)
		{msg_activate = activate; msg_click = click; msg_deselect = deselected;}
	inline void set_colors(const OMediaRGBColor &shine_c, const OMediaRGBColor &dark_c,
						   const OMediaRGBColor &back_c, const OMediaRGBColor &cursor_c)
		{shine = shine_c; dark = dark_c; back = back_c; cursor = cursor_c; moved = true;}
	inline bool has_focus(void) const {return focus_field == this;}

	omtshared static OMediaCStringField	*focus_field;				// 0x1041c9a0
	omtshared static OMediaCStringField	*select_waiting_field;		// 0x1041c9ac
	omtshared static OMediaRGBColor		initback, initdark, initshine, initcursor;	// 0x1041c9a4 / 998 / 9b0 / 9b8

	protected:
	omtshared virtual void draw_full(OMediaGfxWorld *w, long x, long y);							// 0x10053520  vt+0x94
	omtshared virtual void draw_text(OMediaDrawPort &dp, short x, short y);						// 0x100532d0  vt+0xa4
	omtshared virtual void draw_back(OMediaDrawPort &dp, OMediaRect &r);							// 0x100534e0  vt+0xa8
	omtshared virtual void draw_cursor(OMediaDrawPort &dp, short x, short y, OMediaRect &r);		// 0x10053220  vt+0xac
	omtshared virtual bool isinbounds(short x, short y);											// 0x10053610  vt+0xb0
	omtshared virtual void recalc_size(void);														// 0x10053750  vt+0xb4
	omtshared virtual bool process_keyfilter(const OMediaEvent *e);								// 0x100538d0  vt+0xb8

	void broadcast_to_owner(omt_Message m);

	bool				idle;				// +0xf4  not selected (set when deselected)
	omt_Message			msg_activate;		// +0xf8
	omt_Message			msg_click;			// +0xfc
	omt_Message			msg_deselect;		// +0x100
	short				width, height;		// +0x104 +0x106
	short				margin;				// +0x108
	OMediaRGBColor		shine, dark, back, cursor;
	string				text;				// +0x124
	OMediaQuickText		*font;				// +0x134
	bool				cursor_shown;		// +0x138
	float				cursor_time;		// +0x13c
	omt_StringFieldFilter filter;			// +0x140
	bool				has_background;		// +0x144
	bool				auto_size;			// +0x145
	bool				double_click;		// +0x146
	short				max_width;			// +0x148
	short				cursor_width;		// +0x14a
	unsigned int		last_click;			// +0x14c
	void				*owner;				// +0x150  the message parameter (this when NULL)
	bool				click_notifies;		// +0x154
	OMediaTimer			select_timer;		// +0x158
	float				select_delay;		// +0x168
	bool				waiting;			// +0x16c
};

#endif
