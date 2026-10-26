/* Hook between the Bink compat library and the OMT sound mixer (engine/sdl/OMediaSDLSound.cpp).
   port: OMT 2002 handed Bink its DirectSound object (BinkSetSoundSystem(BinkOpenDirectSound,
   manager->direct_sound)), so a movie's audio went through the same sound card as the game's own
   sounds. Here the sound manager's mixer offers the same service: Bink registers a pull
   callback and the mixer's SDL audio thread calls it. When no mixer is linked (bink_play) Bink
   opens a small SDL device of its own. */
#ifndef BINK_MIXER_H
#define BINK_MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Fills out with frames*channels interleaved floats (-1..1), at the rate given to open(). Called on
   the audio thread, with the mixer locked. */
typedef void (*BinkMixerPull)(void *ud, float *out, int frames);

typedef struct BinkMixer {
	void *(*open)(int rate, int channels, BinkMixerPull pull, void *ud);  /* paused; NULL when no audio device */
	void (*pause)(void *h, int paused);
	void (*lock)(void *h);      /* keeps the pull callback from running */
	void (*unlock)(void *h);
	void (*close)(void *h);     /* after this the callback is never called again */
} BinkMixer;

/* Defined by the engine's sound layer; weak in the Bink library, NULL if the engine is not linked. */
const BinkMixer *omt_sound_get_bink_mixer(void);

#ifdef __cplusplus
}
#endif
#endif
