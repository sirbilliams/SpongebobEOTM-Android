/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.

    OMediaAnim: frames in sequences; plays the current sequence frame by frame (a frame shows for
    'nframes' updates) or by time (a frame's milliseconds), looping, reversed or ping-pong.
    OMediaAnimFrame: one frame (offset, duration) linked into a sequence of an anim.
    Layout in OMT.dll: anim +0x08 sequences, +0x14 current frame, +0x18 current sequence,
    +0x1c update count, +0x20 pause count, +0x24 time based, +0x25 loop, +0x26 reverse,
    +0x27 started, +0x28 ping-pong, +0x2c time left; frame +0x04 anim, +0x08 offset x,y,z,
    +0x14 updates per frame, +0x18 ms per frame (40), +0x1c frames per second, +0x20 sequence,
    +0x24 list node.                                                                             */
#pragma once
#ifndef OMEDIA1_Anim_H
#define OMEDIA1_Anim_H

#include "OMediaTypes.h"
#include <list>
#include <vector>

class OMediaAnimFrame;
typedef list<OMediaAnimFrame*> omt_FrameList;

class OMediaAnim
{
	public:
	omtshared OMediaAnim();				// 0x100367d0
	omtshared virtual ~OMediaAnim();	// 0x10036830  deletes the frames

	omtshared virtual void setcurrentsequence(long seq, bool restart = true);	// 0x10036910  vt+0x04
	omtshared virtual void setcurrentframe(long seq, long pos);				// 0x10036a00  vt+0x08
	omtshared virtual void setcurrentframe(OMediaAnimFrame *frame);			// 0x100369c0  vt+0x0c
	omtshared virtual long getcurrentframe_pos(void) const;					// 0x10036a70  vt+0x10
	omtshared virtual bool advance_frame(bool reset_timer = true);			// 0x10036c10  vt+0x14  true: at the end
	omtshared virtual void setplay_timebased(bool tb);						// 0x10036ab0  vt+0x18
	omtshared virtual void update(long elapsed);							// 0x10036ac0  vt+0x1c
	omtshared virtual void pause(bool p);									// 0x10036d30  vt+0x20
	omtshared virtual void setdirty(void);									// vt+0x24 (nothing)
	omtshared virtual void presence_removed(void);							// vt+0x28 (nothing)

	inline OMediaAnimFrame *getcurrentframe(void) const {return current_frame;}
	inline long getcurrentsequence(void) const {return current_sequence;}
	inline void setplay_loop(bool l) {play_loop = l;}
	inline bool getplay_loop(void) const {return play_loop;}
	inline void setplay_reverse(bool r) {play_reverse = r;}
	inline bool getplay_reverse(void) const {return play_reverse;}
	inline void setplay_pingpong(bool p) {play_pingpong = p;}
	inline bool getplay_pingpong(void) const {return play_pingpong;}
	inline bool getplay_timebased(void) const {return play_timebased;}
	inline void reset_timebased_counter(void) {play_started = false;}
	inline bool is_anim_paused(void) const {return pause_count != 0;}
	inline long get_nsequences(void) const {return (long)sequences.size();}
	inline omt_FrameList *get_sequence(long s) {return s < (long)sequences.size() ? sequences[s] : NULL;}

	omtshared omt_FrameList *get_sequence_list(long s);		// fcn.10036f50: grows the table

	protected:
	omtshared virtual void update_sorter(OMediaAnimFrame *previous);		// vt+0x2c (nothing)

	friend class OMediaAnimFrame;

	vector<omt_FrameList*>	sequences;			// +0x08
	OMediaAnimFrame			*current_frame;		// +0x14
	long					current_sequence;	// +0x18
	long					update_count;		// +0x1c
	int						pause_count;		// +0x20
	bool					play_timebased;		// +0x24
	bool					play_loop;			// +0x25
	bool					play_reverse;		// +0x26
	bool					play_started;		// +0x27
	bool					play_pingpong;		// +0x28
	float					time_left;			// +0x2c
};

class OMediaAnimFrame
{
	public:
	omtshared OMediaAnimFrame();			// 0x10036d90
	omtshared virtual ~OMediaAnimFrame();	// 0x10036de0

	omtshared virtual void link(OMediaAnim *anim, long sequence = 0);	// 0x10036df0  vt+0x04
	omtshared virtual void unlink(void);								// 0x10036e80  vt+0x08
	omtshared virtual void setoffset(float x, float y, float z);		// 0x10036ee0  vt+0x0c
	omtshared virtual void setframespeed_tb(float fps);				// 0x10036f10  vt+0x10

	inline OMediaAnim *getanim(void) const {return anim;}
	inline long getsequence(void) const {return sequence;}
	inline float getoffsetx(void) const {return offset_x;}
	inline float getoffsety(void) const {return offset_y;}
	inline float getoffsetz(void) const {return offset_z;}
	inline void setframespeed_fb(long n) {speed_fb = n;}
	inline long getframespeed_fb(void) const {return speed_fb;}
	inline float getframespeed_tb(void) const {return speed_fps;}
	inline float getmillisecperframe(void) const {return speed_ms;}

	protected:
	friend class OMediaAnim;
	OMediaAnim				*anim;				// +0x04
	float					offset_x, offset_y, offset_z;	// +0x08
	long					speed_fb;			// +0x14
	float					speed_ms;			// +0x18
	float					speed_fps;			// +0x1c
	long					sequence;			// +0x20
	omt_FrameList::iterator	node;				// +0x24
};

#endif
