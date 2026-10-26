/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaAnim.h"

//------------------------------------------------------------------------------ OMediaAnim

// 0x100367d0
OMediaAnim::OMediaAnim()
{
	current_frame = NULL;
	current_sequence = 0;
	update_count = 0;
	play_timebased = false;
	play_loop = true;
	play_reverse = false;
	play_started = false;
	play_pingpong = false;
	pause_count = 0;
	time_left = 0;
}

// 0x10036830
OMediaAnim::~OMediaAnim()
{
	for (size_t s = 0; s < sequences.size(); s++)
	{
		while (sequences[s]->size()) delete sequences[s]->front();
		delete sequences[s];
	}
	sequences.clear();
}

// fcn.10036f50
omt_FrameList *OMediaAnim::get_sequence_list(long s)
{
	while ((long)sequences.size() <= s) sequences.push_back(new omt_FrameList);
	return sequences[s];
}

// 0x10036910. Without restart the new sequence starts at the same position (or its last frame).
void OMediaAnim::setcurrentsequence(long seq, bool restart)
{
	if (seq == current_sequence) return;
	omt_FrameList *l = get_sequence_list(seq);
	if (l->size() == 0)
	{
		current_frame = NULL;
		current_sequence = seq;
		setdirty();
		play_started = false;
		return;
	}
	if (!restart && current_frame)
	{
		omt_FrameList *old = sequences[current_sequence];
		omt_FrameList::iterator n = l->begin();
		bool past = false;
		for (omt_FrameList::iterator i = current_frame->node; i != old->begin(); i--)
		{
			if (n == l->end()) { past = true; break; }
			n++;
		}
		if (!past) past = n == l->end();
		if (past) n = --l->end();
		current_frame = *n;
		current_sequence = seq;
		setdirty();
		play_started = false;
		return;
	}
	current_frame = l->front();
	current_sequence = seq;
	setdirty();
	play_started = false;
}

// 0x100369c0
void OMediaAnim::setcurrentframe(OMediaAnimFrame *frame)
{
	if (!frame)
	{
		current_frame = NULL;
		return;
	}
	setcurrentsequence(frame->sequence, true);
	current_frame = frame;
	setdirty();
	play_started = false;
}

// 0x10036a00
void OMediaAnim::setcurrentframe(long seq, long pos)
{
	setcurrentsequence(seq, true);
	omt_FrameList *l = get_sequence_list(seq);
	if (l->size() == 0)
	{
		current_frame = NULL;
		play_started = false;
		setdirty();
		return;
	}
	omt_FrameList::iterator n = l->begin();
	for (; pos > 0; pos--)
	{
		if (n == l->end()) break;
		n++;
	}
	if (n == l->end()) n = --l->end();
	current_frame = *n;
	play_started = false;
	setdirty();
}

// 0x10036a70
long OMediaAnim::getcurrentframe_pos(void) const
{
	if (sequences.size() == 0) return 0;
	omt_FrameList *l = sequences[current_sequence];
	long p = 0;
	for (omt_FrameList::iterator i = l->begin(); i != l->end(); i++, p++)
		if (*i == current_frame) return p;
	return 0;
}

// 0x10036ab0
void OMediaAnim::setplay_timebased(bool tb)
{
	play_started = false;
	play_timebased = tb;
}

// 0x10036ac0. Frame based: a frame shows for its 'speed_fb' updates. Time based: the elapsed
// milliseconds (of the world's frame) are spent on the frames' durations.
void OMediaAnim::update(long elapsed)
{
	if (pause_count != 0) return;
	OMediaAnimFrame *previous = current_frame;
	if (!current_frame)
	{
		if (sequences.size() == 0) return;
		omt_FrameList *l = sequences[current_sequence];
		if (l->size() == 0) return;
		play_started = false;
		current_frame = l->front();
	}
	if (!play_timebased)
	{
		if (!play_started)
		{
			play_started = true;
			update_count = 0;
		}
		else if (current_frame->speed_fb <= ++update_count)
		{
			update_count = 0;
			advance_frame(false);
		}
	}
	else if (!play_started)
	{
		play_started = true;
		time_left = current_frame->speed_ms;
	}
	else
	{
		float e = (float)elapsed;
		if (current_frame->speed_ms != 0.0f)
		{
			if (time_left <= e)
			{
				float f;
				do
				{
					e = e - time_left;
					if (advance_frame(false))
					{
						time_left = current_frame->speed_ms;
						goto done;
					}
					f = current_frame->speed_ms;
					time_left = f;
					if (f == 0.0f) goto done;
				} while (f <= e);
			}
			time_left = time_left - e;
		}
	}
done:
	if (current_frame != previous)
	{
		setdirty();
		update_sorter(previous);
	}
}

// 0x10036c10. Returns true when the sequence ended (no loop) or has a single frame.
bool OMediaAnim::advance_frame(bool reset_timer)
{
	if (reset_timer) play_started = false;
	if (!current_frame)
	{
		if (sequences.size() == 0) return true;
		omt_FrameList *l = sequences[current_sequence];
		if (l->size() == 0) return true;
		current_frame = l->front();
	}
	omt_FrameList *l = sequences[current_sequence];
	if (l->size() != 1)
	{
		omt_FrameList::iterator n = current_frame->node;
		if (!play_reverse)
		{
			if (n != --l->end())
			{
				current_frame = *(++n);
				return false;
			}
			if (play_pingpong)
			{
				play_reverse = true;
				current_frame = *(--n);
				return false;
			}
			if (play_loop)
			{
				current_frame = l->front();
				return false;
			}
		}
		else
		{
			if (n != l->begin())
			{
				current_frame = *(--n);
				return false;
			}
			if (play_pingpong)
			{
				play_reverse = false;
				current_frame = *(++n);
				return false;
			}
			if (play_loop)
			{
				current_frame = l->back();
				return false;
			}
		}
	}
	return true;
}

// 0x10036d30
void OMediaAnim::pause(bool p)
{
	if (!p)
	{
		if (pause_count == 1) play_started = false;
		pause_count--;
		return;
	}
	pause_count++;
}

void OMediaAnim::setdirty(void) {}
void OMediaAnim::presence_removed(void) {}
void OMediaAnim::update_sorter(OMediaAnimFrame *) {}

//------------------------------------------------------------------------------ OMediaAnimFrame

// 0x10036d90
OMediaAnimFrame::OMediaAnimFrame()
{
	anim = NULL;
	offset_z = offset_y = offset_x = 0;
	speed_fb = 1;
	speed_ms = 40.0f;
	speed_fps = 0;			// port: OMT.dll leaves it unset until setframespeed_tb
	sequence = 0;
}

// 0x10036de0
OMediaAnimFrame::~OMediaAnimFrame()
{
	OMediaAnimFrame::unlink();
}

// 0x10036df0. The first frame of the anim's current sequence becomes its current frame.
void OMediaAnimFrame::link(OMediaAnim *a, long seq)
{
	unlink();
	anim = a;
	sequence = seq;
	omt_FrameList *l = a->get_sequence_list(seq);
	l->push_back(this);
	node = --l->end();
	if (l->size() == 1 && seq == a->current_sequence) a->setcurrentframe(this);
}

// 0x10036e80
void OMediaAnimFrame::unlink(void)
{
	if (!anim) return;
	if (anim->current_frame == this)
	{
		anim->setcurrentframe((OMediaAnimFrame*)NULL);
		anim->presence_removed();
	}
	anim->get_sequence_list(sequence)->erase(node);
	anim = NULL;
}

// 0x10036ee0
void OMediaAnimFrame::setoffset(float x, float y, float z)
{
	offset_x = x;
	offset_z = z;
	offset_y = y;
	if (anim && anim->current_frame == this) anim->setdirty();
}

// 0x10036f10. Frames per second, kept as milliseconds per frame.
void OMediaAnimFrame::setframespeed_tb(float fps)
{
	speed_fps = fps;
	speed_ms = fps == 0.0f ? 0.0f : 1000.0f / fps;
	if (anim && anim->current_frame == this) anim->play_started = false;
}
