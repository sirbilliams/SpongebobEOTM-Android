/*  Bink movie audio through OMT's sound mixer (BinkSetSoundSystem hooked to the sound manager).

    Plays a movie headless (no picture) while a sound plays on a sound manager channel, with
    SDL_AUDIODRIVER=disk: both must end up in the one device, i.e. in the one audio file. The
    movie's end is checked against the file's length.
    env: OMT_DATA (game folder), OMT_MOVIE (default movies/mLEGAL.bik), OMT_SOUNDDB (default sounds/help.omt)    */

#include "OMediaApplication.h"
#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaSound.h"
#include "OMediaSoundManager.h"
#include "OMediaError.h"
#include "OMediaSDLRetarget.h"
#include "bink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

void omt_main(void)
{
	const char *d = getenv("OMT_DATA");
	string data = d ? d : ".";
	string movie = getenv("OMT_MOVIE") ? getenv("OMT_MOVIE") : data + "/movies/mLEGAL.bik";
	string sdb = getenv("OMT_SOUNDDB") ? getenv("OMT_SOUNDDB") : data + "/sounds/help.omt";
	const char *audiofile = getenv("SDL_DISKAUDIOFILE");
	int fail = 0;

	OMediaDataBase::set_cache_size(0);
	omd_REGISTERCLASS(OMediaSound);
	OMediaSoundManager::setdefaultmanager(new OMediaDXSoundManager);	// opens the mixer
	OMediaSoundManager *m = OMediaSoundManager::getdefaultmanager();
	m->set_nchannels(4);

	// sb2.exe, 0x40c2a0
	BinkSetSoundSystem(BinkOpenDirectSound, 0);
	HBINK b = BinkOpen(movie.c_str(), 0);
	if (!b) { printf("FAIL: cannot open %s\n", movie.c_str()); exit(1); }
	double secs = (double)b->Frames * b->FrameRateDiv / b->FrameRate;
	printf("movie %ux%u, %u frames, %.2f s\n", b->Width, b->Height, b->Frames, secs);

	OMediaFilePath path(sdb);
	OMediaFileStream f; f.setpath(&path); f.open(omcfp_Read);
	OMediaDataBase *db = new OMediaDataBase(&f);
	omt_DBIterator it;
	OMediaSound *snd = (OMediaSound*)db->get_first_object(OMediaSound::db_type, it);

	std::vector<unsigned char> buf(b->Width * b->Height * 4);
	unsigned int t0 = SDL_GetTicks();
	bool played = false;
	for (;;)
	{
		OMediaPeriodical::devote_time();
		if (!played && !getenv("OMT_NOSOUND") && SDL_GetTicks() - t0 > 500 && snd) { played = m->play(0, snd, false, false); }
		if (BinkWait(b)) { SDL_Delay(1); continue; }
		BinkDoFrame(b);
		BinkCopyToBuffer(b, &buf[0], (int)b->Width * 4, b->Height, 0, 0, BINKSURFACE32);
		if (b->FrameNum == b->Frames) break;
		BinkNextFrame(b);
	}
	unsigned int ms = SDL_GetTicks() - t0;
	printf("%u frames in %u ms (movie %.0f ms), sound played: %d\n", b->FrameNum, ms, secs * 1000, (int)played);
	if (ms < secs * 1000 * 0.9 || ms > secs * 1000 * 1.3 + 500) { printf("FAIL: pacing\n"); fail++; }
	BinkClose(b);
	delete db;
	m->set_nchannels(0);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	if (audiofile && getenv("SDL_AUDIODRIVER") && !strcmp(getenv("SDL_AUDIODRIVER"), "disk"))
	{
		FILE *fp = fopen(audiofile, "rb");
		if (!fp) { printf("FAIL: no audio file\n"); exit(1); }
		fseek(fp, 0, SEEK_END);
		long frames = ftell(fp) / 4;
		fseek(fp, 0, SEEK_SET);
		std::vector<short> pcm(frames * 2);
		size_t got = fread(&pcm[0], sizeof(short), pcm.size(), fp);
		fclose(fp);
		// non-silent stretches: the movie's audio runs the whole time, the sound is a second source
		long nonzero = 0, first = -1, last = -1;
		for (long i = 0; i < frames; i++) if (pcm[i * 2] || pcm[i * 2 + 1]) { nonzero++; if (first < 0) first = i; last = i; }
		printf("audio file: %ld frames (%.2f s), %ld non-silent, first %ld last %ld\n", frames, frames / 44100.0, nonzero, first, last);
		if (frames < secs * 44100 * 0.9) { printf("FAIL: audio file shorter than the movie\n"); fail++; }
		if (nonzero < 44100) { printf("FAIL: no audio\n"); fail++; }
	}
	printf("%s\n", fail ? "FAILED" : "OK");
	if (fail) exit(1);
}
