/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaCStringField.h"
#include "OMediaQuickText.h"
#include "OMediaDrawPort.h"
#include "OMediaEventManager.h"
#include "OMediaViewPort.h"
#include "OMedia2DWorld.h"
#include "OMediaDefMessages.h"
#include "OMediaTimeCounter.h"

OMediaCStringField	*OMediaCStringField::focus_field = NULL;
OMediaCStringField	*OMediaCStringField::select_waiting_field = NULL;
// 0x10052510 .. 0x100525b7: the default colours
OMediaRGBColor OMediaCStringField::initback(0xaaaa, 0xaaaa, 0xaaaa);
OMediaRGBColor OMediaCStringField::initdark(0x6666, 0x6666, 0x6666);
OMediaRGBColor OMediaCStringField::initshine(0xffff, 0xffff, 0xffff);
OMediaRGBColor OMediaCStringField::initcursor(0xeeee, 0xeeee, 0xeeee);

static inline float omf_now(void) {return (float)OMediaTimeCounter::get_millisecs();}	// omt_GetMilliSecsCounter

// 0x100525c0
OMediaCStringField::OMediaCStringField()
{
	idle = true;
	msg_activate = 0;
	msg_click = 0;
	msg_deselect = 0;
	width = 0;
	height = 0;
	margin = 3;
	shine = initshine;
	dark = initdark;
	back = initback;
	cursor = initcursor;
	font = NULL;
	cursor_shown = false;
	cursor_time = -1.0f;
	filter = omcsff_Any;
	has_background = true;
	auto_size = true;
	double_click = false;
	max_width = 0x280;
	cursor_width = 5;
	last_click = 0;
	owner = NULL;
	click_notifies = false;
	select_delay = 0.0f;
	waiting = false;
	backdrop = true;			// +0xc4
	moved = true;				// +0x7c
	// the select timer starts stopped (OMediaTimer's constructor)
}

// 0x100527d0
OMediaCStringField::~OMediaCStringField()
{
	if (focus_field == this) focus_field = NULL;
	if (select_waiting_field == this) select_waiting_field = NULL;
	if (OMediaEventManager::get_event_manager() &&
		OMediaEventManager::get_event_manager()->get_focus_listener() == static_cast<OMediaListener*>(this))
		OMediaEventManager::get_event_manager()->set_focus_listener(NULL);
}

void OMediaCStringField::set_font(OMediaQuickText *f)
{
	font = f;
	recalc_size();
	moved = true;
}

void OMediaCStringField::broadcast_to_owner(omt_Message m)
{
	broadcast_message(m, owner ? owner : (void*)this);
}

// 0x10052910
void OMediaCStringField::set_string(string s)
{
	text = s;
	if (auto_size) recalc_size();
	moved = true;
}

// 0x10052a50
void OMediaCStringField::getsize(long &w, long &h)
{
	w = width;
	h = height;
}

// 0x10052a70
void OMediaCStringField::process_click(OMediaWorldClickInfo *ci)
{
	if (!point_enclosed(ci->viewport, ci->x, ci->y))
	{
		// a click outside (a button press, not a release) takes the focus away
		if (!(ci->right_button & 1))
		{
			if (focus_field) focus_field->deselect();
			OMediaCStringField *w = select_waiting_field;
			if (w)
			{
				w->waiting = false;
				if (focus_field != w) w->idle = true;
				if (select_waiting_field == w) select_waiting_field = NULL;
			}
		}
		return;
	}
	ci->type = omcct_Element;
	ci->element = this;
	if (ci->right_button & 1) return;

	if (!double_click)
	{
		if (click_notifies)
		{
			broadcast_to_owner(msg_activate);
			return;
		}
		if (select_delay == 0.0f)
		{
			select();
			return;
		}
		if (focus_field == this) return;
		if (focus_field) focus_field->deselect();
		if (select_waiting_field == this) return;
		if (select_waiting_field)
		{
			select_waiting_field->waiting = false;
			if (focus_field != select_waiting_field) select_waiting_field->idle = true;
		}
		select_waiting_field = this;
		waiting = true;
		select_timer.start();
		idle = false;
		return;
	}

	// double click mode: the first click sends the click message, a second one within 800 ms selects
	unsigned int now = OMediaTimeCounter::get_millisecs();
	if (last_click != 0 && (now - last_click) < 0x321)
	{
		if (click_notifies) broadcast_to_owner(msg_activate);
		else select();
		last_click = now;
		return;
	}
	if (msg_click) broadcast_to_owner(msg_click);
	last_click = now;
}

// 0x10052cd0. Key messages reach the field through the event manager once it has the focus.
void OMediaCStringField::listen_to_message(omt_Message msg, void *param)
{
	if (!getvisible_extend())
	{
		if (focus_field == this) deselect();
		return;
	}
	if (msg == omsg_LostFocus)
	{
		deselect();
		return;
	}
	if (msg != omsg_KeyDown) return;
	if (focus_field != this) return;
	OMediaEvent *ev = (OMediaEvent*)param;
	switch (ev->key)
	{
		case 0:			// a character
		{
			if (!process_keyfilter(ev)) return;
			text += ev->character;
			if (font)
			{
				short len = font->get_text_length(text);
				if (auto_size)
				{
					if (len <= max_width - margin * 2)
					{
						cursor_time = omf_now();
						cursor_shown = true;
						recalc_size();
						moved = true;
						return;
					}
				}
				else if (len <= width - margin * 2) goto accepted;
				text.erase(text.size() - 1);		// it does not fit
				return;
			}
		accepted:
			cursor_time = omf_now();
			cursor_shown = true;
			moved = true;
			return;
		}
		case omck_Backspace:
			if (text.empty()) return;
			text.erase(text.size() - 1);
			if (auto_size) recalc_size();
			cursor_time = omf_now();
			cursor_shown = true;
			moved = true;
			return;
		case omck_Enter:
		case 5:
			deselect();
			return;
		default: return;
	}
}

// 0x10053060: the select delay, then the cursor blinking (every 600 ms)
void OMediaCStringField::spend_time(void)
{
	if (waiting)
	{
		float t = (float)select_timer.getelapsed();
		if (select_delay <= t)
		{
			select_waiting_field = NULL;
			waiting = false;
			select();
		}
	}
	if (focus_field == this)
	{
		if (hide_count != 0)
		{
			deselect();
			return;
		}
		float now = omf_now();
		if (cursor_time == -1.0f)
		{
			cursor_time = now;
			return;
		}
		if (600.0f <= now - cursor_time)
		{
			cursor_time = now;
			cursor_shown = !cursor_shown;
			moved = true;
		}
	}
}

// 0x10053150
void OMediaCStringField::select(void)
{
	if (focus_field == this) return;
	if (focus_field) focus_field->deselect();
	idle = false;
	cursor_shown = true;
	cursor_time = -1.0f;
	moved = true;
	if (OMediaEventManager::get_event_manager())
		OMediaEventManager::get_event_manager()->set_focus_listener(static_cast<OMediaListener*>(this));
	focus_field = this;
}

// 0x100531c0
void OMediaCStringField::deselect(void)
{
	if (focus_field != this) return;
	if (OMediaEventManager::get_event_manager()) OMediaEventManager::get_event_manager()->set_focus_listener(NULL);
	idle = true;
	cursor_shown = false;
	moved = true;
	focus_field = NULL;
	if (msg_deselect) broadcast_to_owner(msg_deselect);
}

// 0x10053520
void OMediaCStringField::draw_full(OMediaGfxWorld *w, long x, long y)
{
	OMediaDrawPort dp;
	dp.setgfxworld(w);
	dp.lock();
	OMediaRect r(x, y, x + width, y + height);
	if (!has_background || focus_field == this || text.empty()) draw_back(dp, r);
	draw_text(dp, (short)x, (short)y);
	dp.unlock();
}

// 0x100532d0. The text clipped to the box inside the margin, then the cursor.
void OMediaCStringField::draw_text(OMediaDrawPort &dp, short x, short y)
{
	if (!font) return;
	OMediaRect inner(0, 0, width - margin * 2, height - margin * 2);
	font->draw(text, dp.getgfxworld(), &inner, x + margin, y + margin);
	if (!cursor_shown) return;
	OMediaRect box(x + 1, y + 1, x + width - 1, y + height - 1);
	short len = font->get_text_length(text);
	draw_cursor(dp, (short)(len + x + 1), (short)(y + 1), box);
}

// 0x100534e0
void OMediaCStringField::draw_back(OMediaDrawPort &dp, OMediaRect &r)
{
	dp.paintrect3D(r.left, r.top, r.right, r.bottom, false, &dark, &shine, &back);
}

// 0x10053220. The cursor is a raised block after the text, cut to the box.
void OMediaCStringField::draw_cursor(OMediaDrawPort &dp, short x, short y, OMediaRect &r)
{
	long l = x, rr = x + cursor_width;
	if (text.size())
	{
		l += margin;
		rr += margin;
	}
	if (r.top < r.bottom && r.left < rr && y < r.bottom && l < r.right)
	{
		long cl = r.left > l ? r.left : l;
		long ct = r.top > y ? r.top : y;
		long cr = r.right < rr ? r.right : rr;
		dp.paintrect3D(cl, ct, cr, r.bottom, true, &dark, &shine, &cursor);
	}
}

// 0x10053610. Is the screen point in the field? (through the world's viewport that contains it)
bool OMediaCStringField::isinbounds(short x, short y)
{
	omt_ViewPortList *vps = world->get_viewports();
	omt_ViewPortList::iterator v;
	long px = x, py = y;
	for (v = vps->begin(); v != vps->end(); v++)
	{
		const OMediaRect &b = (*v)->get_bounds();
		if (x >= b.left && y >= b.top && x < b.right && y < b.bottom)
		{
			px = (long)((float)x - (float)b.left + (*v)->getx());		// the viewport's position in the world
			py = (long)((float)y - (float)b.top + (*v)->gety());
			break;
		}
	}
	if (v == vps->end()) return false;
	if (clipping)
	{
		if (px < clip_rect.left || py < clip_rect.top || px >= clip_rect.right || py >= clip_rect.bottom) return false;
	}
	OMediaRect r;
	get_world_absrect(&r);
	return r.left <= py && r.top <= px && py < r.bottom && px < r.right;
}

// 0x10053750: the string's width plus the margins and the cursor, at most max_width
void OMediaCStringField::recalc_size(void)
{
	if (!font) return;
	short w = font->get_text_length(text);
	w = (short)(w + margin * 2 + cursor_width);
	width = w;
	if (max_width < w) width = max_width;
}

// 0x100538d0
bool OMediaCStringField::process_keyfilter(const OMediaEvent *e)
{
	char c = e->character;
	if (filter != omcsff_Integer && filter != omcsff_Float)
	{
		if (filter == omcsff_AlphaNumeric)
		{
			if (c > '/' && c < ':') return true;
			if (c > '`' && c < '{') return true;
			if (c > '@' && c < '[') return true;
			unsigned char u = (unsigned char)c;
			static const unsigned char extra[] = {0x8e, 0x88, 0x8f, 0x9f, 0x8a, 0x9a, 0x95, 0x9d, 0x99, 0x90, 0x89,
												  0x94, 0x9e, 0x93, 0x8b, 0x9b, 0x98, 0x8d, ' '};
			bool ok = false;
			for (size_t i = 0; i < sizeof(extra); i++) if (extra[i] == u) ok = true;
			if (!ok) return false;
		}
		return true;
	}
	if (c < '0' || c > '9')
	{
		if (c != '.')
		{
			if (c != '-') return false;
			return text.empty();
		}
		if (filter != omcsff_Float) return false;
	}
	else if (c != '.') return true;
	return text.find('.') == string::npos;
}
