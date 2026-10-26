/*  OMT 1.x sound manager (OMT.dll, Aug 2002), on the SDL layer.  LGPL (OMT 2.5 base).

    OMT 2.5 named this OMediaSoundEngine; sb2.exe imports the 2002 names and reads inside the
    objects. The class here has OMT.dll's interface and vtable order (dtor, get_attributes,
    set_buffer_format, compact_buffer, get_max_channels, set_nchannels, get_nchannels, play,
    create_sound_retarget, create_sound_channel). The channels are OMT 2.5's OMediaSoundChannel
    (the SDL channel in sdl/OMediaSDLSound.cpp): play / stop / flush / spend_time /
    get_playing_sound_info are the same code OMT.dll's OMediaDXSoundChannel has, and the volume,
    pan and frequency that sb2.exe set by calling IDirectSoundBuffer::SetVolume / SetPan /
    SetFrequency on the channel's DirectSound buffer (channel + 0x40) are set_volume / set_pan /
    set_frequency here.
      sb2.exe                                   here
      mgr + 8  (channel array, 4 bytes each)    get_channel(i)
      mgr + 0x14 (IDirectSound*)                (none; Bink gets omt_sound_get_bink_mixer())
      channel + 0x34 (looping flag)             channel->get_current_looped()
      buffer->SetPan(p) / SetVolume / SetFreq   channel->set_pan(p) / ... (DirectSound units)  */

#pragma once
#ifndef OMEDIA_SoundManager_H
#define OMEDIA_SoundManager_H

#include "OMediaTypes.h"
#include "OMediaSoundChannel.h"
#include "OMediaRetarget.h"
#include <vector>

class OMediaSound;

// Attributes (OMediaDXSoundManager::get_attributes returns 7)
const unsigned short omsmaf_NoChannelLimitation	= (1<<0);
const unsigned short omsmaf_Volume				= (1<<1);
const unsigned short omsmaf_Frequence			= (1<<2);

class OMediaSoundManager
{
	public:
	// * Constructor/Destructor
	omtshared OMediaSoundManager();
	omtshared virtual ~OMediaSoundManager();

	// * Attributes
	omtshared virtual unsigned short get_attributes(void);

	// * Hardware buffer
	omtshared virtual void set_buffer_format(OMediaSound *sound);
	omtshared virtual void compact_buffer(void);

	// * Channels
	omtshared virtual long get_max_channels(void);		// -1: no limit
	omtshared virtual long set_nchannels(long n);		// returns the number of channels there are afterwards
	omtshared virtual long get_nchannels(void);
	inline OMediaSoundChannel *get_channel(long c) {return channels[c];}	// port: replaces reading mgr + 8

	// * Sound
	omtshared virtual bool play(long channel, OMediaSound *sound, bool queue_if_busy, bool loop);
	omtshared virtual OMediaRetarget *create_sound_retarget(void);

	// * Default manager (created on demand: an OMediaSoundManager that has one channel and plays
	//   nothing, until the application installs a real one with setdefaultmanager)
	omtshared static OMediaSoundManager *getdefaultmanager(void);
	omtshared static void setdefaultmanager(OMediaSoundManager *m);

	protected:
	omtshared virtual OMediaSoundChannel *create_sound_channel(void);
	vector<OMediaSoundChannel*>	channels;
	omtshared static OMediaSoundManager *default_manager;
};

// OMT.dll's DirectSound manager: one SDL audio device, every channel a voice in its mixer.
class OMediaDXSoundManager : public OMediaSoundManager
{
	public:
	omtshared OMediaDXSoundManager();
	omtshared virtual ~OMediaDXSoundManager();
	omtshared virtual unsigned short get_attributes(void);		// 7
	omtshared virtual void set_buffer_format(OMediaSound *sound);
	omtshared virtual void compact_buffer(void);
	omtshared virtual long get_max_channels(void);				// -1
	protected:
	omtshared virtual OMediaSoundChannel *create_sound_channel(void);
};

#endif
