/*  OMT SDL platform layer - sound objects, software mixer, sound engine.  LGPL.
    OMT's DirectSound engine duplicated a buffer per channel; here every channel is a voice in
    one SDL audio callback that resamples + mixes 8/16-bit mono/stereo PCM to S16 stereo.
    Volume and pan follow OMediaDXSoundChannel (OMT2.dll 0x1003e380 / 0x1003e450): volume 0..255
    maps to DirectSound hundredths of a dB (0 = silent, 255 = 0 dB, else -2700 - ftol(v * -2700/255)),
    pan is passed through as DirectSound pan (hundredths of a dB taken off the other side). */

#include "OMediaSDLRetarget.h"
#include "OMediaSound.h"
#ifndef OMT1_NO_SOUND_ENGINE		// port: programs on the OMT 1.x layer (omt1_sound) have no OMT 2.5 engine factory
#include "OMediaSoundEngine.h"
#endif
#include "OMediaSoundChannel.h"
#include "OMediaError.h"
#include "OMediaEndianSupport.h"
#include "bink_mixer.h"
#include <string.h>
#include <math.h>

static void omf_stop_voices_playing(const char *buffer, unsigned int size);

// ---------------------------------------------------------------------------
// OMediaSound: keep the RIFF image, locate fmt/data chunks properly.

static inline unsigned int le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }
static inline unsigned short le16(const unsigned char *p) { return p[0] | (p[1] << 8); }

void OMediaSound::init_retarget() { retarget = new OMediaSDLRtgSound; }

void OMediaSound::read_class(OMediaStreamOperators &stream)
{
	OMediaSDLRtgSound *rtg = (OMediaSDLRtgSound*)retarget;
	unsigned int l, length;

	purge();
	OMediaDBObject::read_class(stream);

	stream >> l;
	if (l != 'RIFF') omd_EXCEPTION(omcerr_BadFormat);
	stream >> length;
	length = omd_ReverseLong(length);        // RIFF sizes are little-endian, stream is big-endian
	stream >> l;
	if (l != 'WAVE') omd_EXCEPTION(omcerr_BadFormat);
	length += 8;
	stream.setposition(-12, omcfr_Current);

	rtg->sound = new char[length];
	rtg->sound_size = length;
	stream.read(rtg->sound, length);

	const unsigned char *b = (const unsigned char*)rtg->sound;
	unsigned int p = 12;
	bool have_fmt = false;
	while (p + 8 <= length)
	{
		unsigned int ck = le32(b + p), cl = le32(b + p + 4);
		const unsigned char *d = b + p + 8;
		if (ck == 0x20746d66)           // 'fmt '
		{
			rtg->channels = le16(d + 2);
			rtg->freq = le32(d + 4);
			rtg->bits = le16(d + 14);
			have_fmt = true;
		}
		else if (ck == 0x61746164)      // 'data'
		{
			rtg->pcm = (char*)d;
			rtg->pcm_length = (p + 8 + cl <= length) ? cl : length - (p + 8);
		}
		p += 8 + cl + (cl & 1);
	}
	if (!have_fmt || !rtg->pcm) omd_EXCEPTION(omcerr_BadFormat);
	unsigned short tag = 0;
	for (unsigned int q = 12; q + 8 <= length; q += 8 + le32(b + q + 4) + (le32(b + q + 4) & 1))
		if (le32(b + q) == 0x20746d66) { tag = le16(b + q + 8); break; }
	if (tag != 1 || (rtg->bits != 8 && rtg->bits != 16))
	{
		// Non-PCM (one MP3-in-WAV clip ships with the game). Keep the data, but mark it unplayable.
		string n; get_chunk_name(n);
		SDL_Log("OMT: sound '%s' has WAV format %d, not playable yet", n.c_str(), (int)tag);
		sound_secs = 0;
		rtg->channels = 0;
		return;
	}
	sound_secs = calc_sound_secs(rtg->pcm_length, rtg->freq, rtg->channels, rtg->bits);
}

void OMediaSound::write_class(OMediaStreamOperators &stream)
{
	OMediaSDLRtgSound *rtg = (OMediaSDLRtgSound*)retarget;
	OMediaDBObject::write_class(stream);
	if (rtg->sound) stream.write(rtg->sound, rtg->sound_size);
}

unsigned int OMediaSound::get_approximate_size(void)
{
	OMediaSDLRtgSound *rtg = (OMediaSDLRtgSound*)retarget;
	return sizeof(*this) + sizeof(OMediaSDLRtgSound) + rtg->sound_size;
}

void OMediaSound::purge()
{
	OMediaSDLRtgSound *rtg = (OMediaSDLRtgSound*)retarget;
	delete_imp_slaves();				// (DirectSound: releasing the buffers stops them)
	omf_stop_voices_playing(rtg->sound, rtg->sound_size);
	delete [] rtg->sound;
	rtg->sound = NULL; rtg->sound_size = 0;
	rtg->pcm = NULL; rtg->pcm_length = 0;
	sound_secs = 0;
}

// ---------------------------------------------------------------------------
// Mixer

struct OMediaSDLVoice
{
	const unsigned char	*pcm;
	unsigned int		frames;
	double				pos, step;
	int					channels, bits;
	float				gain_l, gain_r;		// from the channel's volume and pan
	bool				loop, active;
	bool				paused;			// port: the Game pauses the music while the menu is up (0x40c730)
};

static SDL_AudioDeviceID	omg_audio_dev = 0;
static int					omg_out_freq = 44100;
static vector<OMediaSDLVoice*> omg_voices;

static inline int omf_sample(const OMediaSDLVoice *v, unsigned int frame, int ch)
{
	if (ch >= v->channels) ch = v->channels - 1;
	if (v->bits == 8) return ((int)v->pcm[frame * v->channels + ch] - 128) << 8;
	const unsigned char *p = v->pcm + (frame * v->channels + ch) * 2;
	return (short)(p[0] | (p[1] << 8));
}

// port: stream sources (Bink movies). OMT 2002 had no such thing - Bink opened DirectSound itself
// (BinkSetSoundSystem got the sound manager's IDirectSound); here Bink pulls its movie audio
// through the one SDL device, so a movie and the game's sounds share the mixer.
struct OMediaSDLStream
{
	BinkMixerPull		pull;
	void				*ud;
	int					rate, channels;
	bool				paused;
	double				pos;			// position in 'src', in input frames
	std::vector<float>	src;			// pulled input, stereo float
	float				last[2];		// the last input frame of the previous chunk
};
static vector<OMediaSDLStream*> omg_streams;
static vector<int> omg_mix;			// callback scratch: 2 ints per frame

// Reads one more chunk of input into the stream (silence when the source is dry).
static void omf_stream_refill(OMediaSDLStream *st)
{
	const int chunk = 512;
	if (st->src.size() >= 2) { st->last[0] = st->src[st->src.size() - 2]; st->last[1] = st->src[st->src.size() - 1]; }
	st->pos -= (double)(st->src.size() / 2);
	st->src.resize((size_t)chunk * 2);
	if (st->channels == 2) st->pull(st->ud, &st->src[0], chunk);
	else
	{
		vector<float> mono((size_t)chunk);
		st->pull(st->ud, &mono[0], chunk);
		for (int i = 0; i < chunk; i++) st->src[i * 2] = st->src[i * 2 + 1] = mono[i];
	}
}

static void omf_stream_mix(OMediaSDLStream *st, int *mix, int frames)
{
	double step = (double)st->rate / (double)omg_out_freq;
	for (int i = 0; i < frames; i++)
	{
		if (st->src.empty() || st->pos >= (double)(st->src.size() / 2) - 1.0) omf_stream_refill(st);
		double fpos = st->pos;
		long i0 = (long)floor(fpos);
		double fr = fpos - (double)i0;
		float a0, a1, b0, b1;
		if (i0 < 0) { a0 = st->last[0]; a1 = st->last[1]; }
		else { a0 = st->src[(size_t)i0 * 2]; a1 = st->src[(size_t)i0 * 2 + 1]; }
		b0 = st->src[(size_t)(i0 + 1) * 2]; b1 = st->src[(size_t)(i0 + 1) * 2 + 1];
		mix[i * 2] += (int)((a0 + (b0 - a0) * (float)fr) * 32767.0f);
		mix[i * 2 + 1] += (int)((a1 + (b1 - a1) * (float)fr) * 32767.0f);
		st->pos += step;
	}
}

static void SDLCALL omf_audio_cb(void *, Uint8 *stream, int len)
{
	int frames = len / 4;
	Sint16 *out = (Sint16*)stream;
	if ((int)omg_mix.size() < frames * 2) omg_mix.resize((size_t)frames * 2);
	int *mix = &omg_mix[0];
	for (int i = 0; i < frames; i++)
	{
		int l = 0, r = 0;
		for (size_t vi = 0; vi < omg_voices.size(); vi++)
		{
			OMediaSDLVoice *v = omg_voices[vi];
			if (!v->active || v->paused || !v->frames) continue;
			unsigned int f = (unsigned int)v->pos;
			if (f >= v->frames)
			{
				if (v->loop) { v->pos -= v->frames; f = (unsigned int)v->pos; }
				else { v->active = false; continue; }
			}
			l += (int)((float)omf_sample(v, f, 0) * v->gain_l);
			r += (int)((float)omf_sample(v, f, 1) * v->gain_r);
			v->pos += v->step;
		}
		mix[i * 2] = l; mix[i * 2 + 1] = r;
	}
	for (size_t si = 0; si < omg_streams.size(); si++)
		if (!omg_streams[si]->paused) omf_stream_mix(omg_streams[si], mix, frames);
	for (int i = 0; i < frames * 2; i++)
	{
		int v = mix[i];
		if (v > 32767) v = 32767;
		if (v < -32768) v = -32768;
		out[i] = (Sint16)v;
	}
}

static void omf_stop_voices_playing(const char *buffer, unsigned int size)
{
	if (!buffer) return;
	if (omg_audio_dev) SDL_LockAudioDevice(omg_audio_dev);
	const unsigned char *b = (const unsigned char*)buffer;
	for (size_t i = 0; i < omg_voices.size(); i++)
		if (omg_voices[i]->pcm >= b && omg_voices[i]->pcm < b + size) omg_voices[i]->active = false;
	if (omg_audio_dev) SDL_UnlockAudioDevice(omg_audio_dev);
}

// DirectSound attenuation (hundredths of a dB, <= 0) as an amplitude factor; -10000 is silence.
static inline float omf_ds_gain(long att) { return att <= -10000 ? 0.0f : powf(10.0f, (float)att / 2000.0f); }

static void omf_open_audio(void)
{
	if (omg_audio_dev) return;
	if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
	want.callback = omf_audio_cb;
	omg_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (!omg_audio_dev) { SDL_Log("OMT: no audio device (%s), sound disabled", SDL_GetError()); return; }
	omg_out_freq = have.freq;
	SDL_Log("OMT: audio %s, %d Hz, %d channels, format 0x%x, %d samples", SDL_GetCurrentAudioDriver(),
			have.freq, have.channels, have.format, have.samples);
	SDL_PauseAudioDevice(omg_audio_dev, 0);
}

// ---------------------------------------------------------------------------
// Channel

class OMediaSDLSoundChannel : public OMediaSoundChannel
{
	public:
	OMediaSDLSoundChannel()
	{
		voice.active = false; voice.paused = false; voice.frames = 0; voice.pcm = NULL;
		ds_volume = 0; ds_pan = 0;
		update_gains();
		if (omg_audio_dev) SDL_LockAudioDevice(omg_audio_dev);
		omg_voices.push_back(&voice);
		if (omg_audio_dev) SDL_UnlockAudioDevice(omg_audio_dev);
	}
	virtual ~OMediaSDLSoundChannel()
	{
		stop();
		if (omg_audio_dev) SDL_LockAudioDevice(omg_audio_dev);
		for (size_t i = 0; i < omg_voices.size(); i++)
			if (omg_voices[i] == &voice) { omg_voices.erase(omg_voices.begin() + i); break; }
		if (omg_audio_dev) SDL_UnlockAudioDevice(omg_audio_dev);
	}

	bool busy(void) const { return voice.active; }

	void start(OMediaSound *sound, bool loop)
	{
		OMediaSDLRtgSound *s = (OMediaSDLRtgSound*)sound->get_retarget();
		if (!s->pcm || !s->channels || !s->bits) return;
		if (omg_audio_dev) SDL_LockAudioDevice(omg_audio_dev);
		voice.pcm = (const unsigned char*)s->pcm;
		voice.channels = s->channels; voice.bits = s->bits;
		voice.frames = s->pcm_length / (s->channels * (s->bits / 8));
		voice.pos = 0;
		voice.step = double(frequence ? frequence : s->freq) / double(omg_out_freq);
		voice.loop = loop; voice.active = true; voice.paused = false;
		if (omg_audio_dev) SDL_UnlockAudioDevice(omg_audio_dev);
		last_played = sound;
		current_looped = loop;
		current_total_time = (unsigned int)(sound->get_sound_secs() * 1000.0 + 0.5);
		timer.start();
	}

	virtual bool play(OMediaSound *sound, bool queue_if_busy, bool loop)
	{
		if (busy())
		{
			if (!queue_if_busy) return false;
			sound->inc_wait_count();
			sound_queue.push_back(OMediaQueuedSound(sound, loop));
			return true;
		}
		start(sound, loop);
		return true;
	}

	virtual void stop(void)
	{
		if (omg_audio_dev) SDL_LockAudioDevice(omg_audio_dev);
		voice.active = false;
		if (omg_audio_dev) SDL_UnlockAudioDevice(omg_audio_dev);
		current_looped = false;
		current_total_time = 0;
	}

	virtual void spend_time(void)
	{
		if (current_looped || busy() || sound_queue.empty()) return;
		OMediaQueuedSound q = sound_queue.front();
		sound_queue.pop_front();
		q.sound->dec_wait_count();
		start(q.sound, q.loop);
	}

	// OMediaDXSoundChannel::set_volume: the value is kept, and a level DirectSound refuses
	// (above 0 dB, from volumes over 255) leaves the buffer's volume unchanged.
	virtual void set_volume(short v)
	{
		volume = v;
		long att;
		if (v == 0) att = -10000;
		else if (v == 255) att = 0;
		else att = -2700 - (long)((double)v * -10.588235294117647);
		if (att > 0 || att < -10000) return;
		ds_volume = att;
		update_gains();
	}

	virtual void set_pan(long p)
	{
		pan = p;
		if (p > 10000 || p < -10000) return;		// DSERR_INVALIDPARAM
		ds_pan = p;
		update_gains();
	}

	void update_gains(void)
	{
		float g = omf_ds_gain(ds_volume);
		float l = ds_pan > 0 ? g * omf_ds_gain(-ds_pan) : g;
		float r = ds_pan < 0 ? g * omf_ds_gain(ds_pan) : g;
		if (omg_audio_dev) SDL_LockAudioDevice(omg_audio_dev);
		voice.gain_l = l; voice.gain_r = r;
		if (omg_audio_dev) SDL_UnlockAudioDevice(omg_audio_dev);
	}

	virtual void set_frequency(unsigned int f)
	{
		if (f && (f < 100 || f > 100000)) return;		// outside DSBFREQUENCY_MIN..MAX: DirectSound refuses it
		frequence = f;
		if (f && voice.active) voice.step = double(f) / double(omg_out_freq);
	}

	OMediaSDLVoice voice;
	long ds_volume, ds_pan;				// what DirectSound accepted last
};

// port: pause / resume one channel's voice (DirectSound buffer Stop / Play, game vt+0x7c / +0x80 -> 0x40c730 / 0x40c780)
void omf_SDLPauseChannel(OMediaSoundChannel *c, bool pause)
{
	OMediaSDLSoundChannel *ch = dynamic_cast<OMediaSDLSoundChannel*>(c);
	if (!ch) return;
	if (omg_audio_dev) SDL_LockAudioDevice(omg_audio_dev);
	ch->voice.paused = pause;
	if (omg_audio_dev) SDL_UnlockAudioDevice(omg_audio_dev);
}

// ---------------------------------------------------------------------------
// Engine

#ifndef OMT1_NO_SOUND_ENGINE
class OMediaSDLSoundEngine : public OMediaSoundEngine
{
	public:
	OMediaSDLSoundEngine(OMediaWindow *master) : OMediaSoundEngine(ommeic_OS, master) { omf_open_audio(); }
	virtual ~OMediaSDLSoundEngine() { delete_all_channels(); }
	virtual int get_max_channels(void) { return 64; }
	protected:
	virtual OMediaSoundChannel *create_sound_channel(void) { return new OMediaSDLSoundChannel; }
};

OMediaSoundEngine *omf_SDLCreateSoundEngine(OMediaWindow *master) { return new OMediaSDLSoundEngine(master); }

#endif

// port: suspend (OMediaSDLInput.cpp): the whole mix stops while the app is in the background
void omf_SDLAudioPause(bool pause)
{
	if (omg_audio_dev) SDL_PauseAudioDevice(omg_audio_dev, pause ? 1 : 0);
}

// ---------------------------------------------------------------------------
// OMT 1.x sound manager hooks (omt1/OMediaSoundManager.cpp)

OMediaSoundChannel *omf_SDLCreateSoundChannel(void) { return new OMediaSDLSoundChannel; }
void omf_SDLOpenAudio(void) { omf_open_audio(); }
bool omf_SDLAudioIsOpen(void) { return omg_audio_dev != 0; }
OMediaRetarget *omf_SDLCreateSoundRetarget(void) { return new OMediaSDLRtgSound; }

// ---------------------------------------------------------------------------
// Bink: stream sources in the mixer (bink_mixer.h)

static void *omf_mixer_open(int rate, int channels, BinkMixerPull pull, void *ud)
{
	if (rate <= 0 || channels < 1 || channels > 2 || !pull) return NULL;
	omf_open_audio();
	if (!omg_audio_dev) return NULL;
	OMediaSDLStream *st = new OMediaSDLStream;
	st->pull = pull; st->ud = ud; st->rate = rate; st->channels = channels;
	st->paused = true; st->pos = 0; st->last[0] = st->last[1] = 0;
	SDL_LockAudioDevice(omg_audio_dev);
	omg_streams.push_back(st);
	SDL_UnlockAudioDevice(omg_audio_dev);
	SDL_Log("OMT: audio stream in the mixer, %d Hz, %d channels", rate, channels);
	return st;
}
static void omf_mixer_pause(void *h, int paused)
{
	OMediaSDLStream *st = (OMediaSDLStream*)h;
	SDL_LockAudioDevice(omg_audio_dev);
	st->paused = paused != 0;
	SDL_UnlockAudioDevice(omg_audio_dev);
}
static void omf_mixer_lock(void *) { SDL_LockAudioDevice(omg_audio_dev); }
static void omf_mixer_unlock(void *) { SDL_UnlockAudioDevice(omg_audio_dev); }
static void omf_mixer_close(void *h)
{
	OMediaSDLStream *st = (OMediaSDLStream*)h;
	SDL_LockAudioDevice(omg_audio_dev);
	for (size_t i = 0; i < omg_streams.size(); i++)
		if (omg_streams[i] == st) { omg_streams.erase(omg_streams.begin() + i); break; }
	SDL_UnlockAudioDevice(omg_audio_dev);
	delete st;
}
static const BinkMixer omg_bink_mixer = { omf_mixer_open, omf_mixer_pause, omf_mixer_lock, omf_mixer_unlock, omf_mixer_close };
extern "C" const BinkMixer *omt_sound_get_bink_mixer(void) { return &omg_bink_mixer; }
