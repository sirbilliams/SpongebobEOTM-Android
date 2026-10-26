/*  OMT 1.x sound manager (OMT.dll), on the SDL layer.  See OMediaSoundManager.h.  LGPL. */

#include "OMediaSoundManager.h"
#include "OMediaSound.h"
#include "OMediaError.h"

// sdl/OMediaSDLSound.cpp
OMediaSoundChannel *omf_SDLCreateSoundChannel(void);
void omf_SDLOpenAudio(void);
OMediaRetarget *omf_SDLCreateSoundRetarget(void);

OMediaSoundManager *OMediaSoundManager::default_manager = NULL;

// OMT.dll: constructor 0x100017f0 area; the base manager has no channels.
OMediaSoundManager::OMediaSoundManager()
{
}

OMediaSoundManager::~OMediaSoundManager()
{
	set_nchannels(0);
}

// 0x10069b30
unsigned short OMediaSoundManager::get_attributes(void)
{
	return 0;
}

void OMediaSoundManager::set_buffer_format(OMediaSound *sound)
{
}

void OMediaSoundManager::compact_buffer(void)
{
}

// 0x100017f0
long OMediaSoundManager::get_max_channels(void)
{
	return 1;
}

// 0x10087060
long OMediaSoundManager::get_nchannels(void)
{
	return (long)channels.size();
}

// 0x10086f10. Grows with create_sound_channel up to get_max_channels (-1: unlimited), shrinks by
// deleting the last channels. Returns the number of channels there are afterwards.
long OMediaSoundManager::set_nchannels(long n)
{
	long cursize = (long)channels.size();

	if (n != cursize)
	{
		if (cursize < n)
		{
			long ntoadd = n - cursize;
			while (ntoadd != 0)
			{
				ntoadd--;
				if (get_max_channels() != -1 && (unsigned long)get_max_channels() <= (unsigned long)channels.size()) break;
				OMediaSoundChannel *c = create_sound_channel();
				channels.push_back(c);			// (a NULL goes into the vector like in OMT.dll)
			}
		}
		else
		{
			long ntorem = cursize - n;
			while (ntorem != 0)
			{
				OMediaSoundChannel *c = channels.back();
				delete c;
				channels.pop_back();
				ntorem--;
			}
		}
		n = (long)channels.size();
	}
	return n;
}

// 0x10087000: OMediaSoundChannel, the playing-nothing one
OMediaSoundChannel *OMediaSoundManager::create_sound_channel(void)
{
	return new OMediaSoundChannel;
}

// 0x10087080: false when the channel does not exist, else the channel's play.
bool OMediaSoundManager::play(long channel, OMediaSound *sound, bool queue_if_busy, bool loop)
{
	if (channel < 0 || (unsigned long)channel >= (unsigned long)channels.size()) return false;
	return channels[channel]->play(sound, queue_if_busy, loop);
}

// 0x10399410 made an OMediaWinRtgSound (the WinMM PlaySound image), 0x1005... the DX one;
// port: the SDL layer's sound retarget (the RIFF image and where its PCM is).
OMediaRetarget *OMediaSoundManager::create_sound_retarget(void)
{
	return omf_SDLCreateSoundRetarget();
}

// 0x10086ee0. OMT.dll makes a plain OMediaSoundManager here.
OMediaSoundManager *OMediaSoundManager::getdefaultmanager(void)
{
	if (!default_manager) default_manager = new OMediaSoundManager;
	return default_manager;
}

// 0x100870c0. The old default manager is deleted. (The decompile returns the old pointer in the
// else branch; the intent, and the Windows behaviour, is that the new one becomes the default.)
void OMediaSoundManager::setdefaultmanager(OMediaSoundManager *m)
{
	if (default_manager && default_manager != m) delete default_manager;
	default_manager = m;
}

//------------------------------------------------------------------------------------------
// OMediaDXSoundManager

// 0x1005cba0: DirectSoundCreate, SetCooperativeLevel(window, DSSCL_PRIORITY), primary buffer.
// port: opens the SDL audio device that the mixer runs on.
OMediaDXSoundManager::OMediaDXSoundManager()
{
	omf_SDLOpenAudio();
}

OMediaDXSoundManager::~OMediaDXSoundManager()
{
	set_nchannels(0);			// (the base destructor has no channels left, the channels are ours)
}

// 0x1005cc60
unsigned short OMediaDXSoundManager::get_attributes(void)
{
	return omsmaf_NoChannelLimitation | omsmaf_Volume | omsmaf_Frequence;
}

// 0x1005cc30: SetFormat on the primary buffer from the sound's WAVEFORMATEX.
// port: the mixer converts every sound as it plays, nothing to set.
void OMediaDXSoundManager::set_buffer_format(OMediaSound *sound)
{
}

// 0x1005cc50: IDirectSound::Compact
void OMediaDXSoundManager::compact_buffer(void)
{
}

// 0x1005cc70
long OMediaDXSoundManager::get_max_channels(void)
{
	return -1;
}

// 0x1005cc80
OMediaSoundChannel *OMediaDXSoundManager::create_sound_channel(void)
{
	return omf_SDLCreateSoundChannel();
}
