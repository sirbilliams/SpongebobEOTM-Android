/*  OMT SDL platform layer - entry point, data folder, logging, test hooks.  LGPL.
    main() -> omt_main(), like OMediaWinMain.cpp. (On Android SDL renames main to SDL_main.)

    Data folder (where omt/, gam/, grn/ ... live):
      OMT_DATA=<dir> if set, else on Android the app's external files dir
      (Android/data/<package>/files), or its Program_Executable_Files subfolder.
    Log: also written to <data>/omt-log.txt on Android.
    Test env: OMT_SCREENSHOT=<x.bmp> saves frame OMT_FRAMES (default 30) and quits;
    OMT_SHOTS / OMT_AUTOKEYS (desktop only): see omf_test_hooks.

    This file is compiled into each app (not the static lib) so SDL can always find main. */

#include "OMediaSDLRetarget.h"
#include "OMediaSupervisor.h"
#include "OMediaDefMessages.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif
// GL comes from OMediaSysDefs.h: system GL on desktop, gl4es's headers (and gl4es itself) on Android.

extern void omt_main(void);

static int omg_frame = 0;
static FILE *omg_logfile = NULL;

static void omf_save_shot(SDL_Window *win, const char *path)
{
	int w, h;
	SDL_GL_GetDrawableSize(win, &w, &h);
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
	unsigned char *tmp = (unsigned char*)malloc(w * h * 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tmp);      // RGBA works on GL and GLES
	for (int y = 0; y < h; y++)                                     // GL is bottom-up
	{
		unsigned char *d = (unsigned char*)s->pixels + y * s->pitch, *src = tmp + (h - 1 - y) * w * 4;
		for (int x = 0; x < w; x++) { d[x * 3] = src[x * 4]; d[x * 3 + 1] = src[x * 4 + 1]; d[x * 3 + 2] = src[x * 4 + 2]; }
	}
	free(tmp);
	SDL_SaveBMP(s, path);
	SDL_FreeSurface(s);
	SDL_Log("OMT: saved %s (%dx%d) at frame %d", path, w, h, omg_frame);
}

#ifndef __ANDROID__
// port: test-only (desktop). OMT_AUTOKEYS="<frame>:+<key>,<frame>:-<key>,..." presses (+) or
// releases (-) a key at that frame by pushing an SDL key event; <key> is an SDL scancode name
// ("Up", "Space", "Left Ctrl", "A"...). OMT_SHOTS="<frame>,<frame>,..." also saves a screenshot
// at each of those frames, as <OMT_SCREENSHOT without .bmp>-<frame>.bmp.
static void omf_test_hooks(SDL_Window *win)
{
	const char *keys = getenv("OMT_AUTOKEYS");
	for (const char *p = keys; p && *p; )
	{
		int frame = atoi(p);
		const char *c = strchr(p, ':');
		const char *e = strchr(p, ',');
		if (!e) e = p + strlen(p);
		if (c && c < e && frame == omg_frame && (c[1] == '+' || c[1] == '-'))
		{
			string name(c + 2, e - (c + 2));
			SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
			if (sc == SDL_SCANCODE_UNKNOWN) SDL_Log("OMT_AUTOKEYS: unknown key %s", name.c_str());
			else
			{
				SDL_Event ev; SDL_zero(ev);
				ev.type = c[1] == '+' ? SDL_KEYDOWN : SDL_KEYUP;
				ev.key.windowID = SDL_GetWindowID(win);
				ev.key.state = c[1] == '+' ? SDL_PRESSED : SDL_RELEASED;
				ev.key.keysym.scancode = sc;
				ev.key.keysym.sym = SDL_GetKeyFromScancode(sc);
				SDL_PushEvent(&ev);
				SDL_Log("OMT_AUTOKEYS: frame %d %c%s", omg_frame, c[1], name.c_str());
			}
		}
		p = *e ? e + 1 : e;
	}
	const char *shots = getenv("OMT_SHOTS");
	const char *shot = getenv("OMT_SCREENSHOT");
	for (const char *p = shots; shot && p && *p; )
	{
		if (atoi(p) == omg_frame)
		{
			string path(shot);
			if (path.size() > 4 && path.compare(path.size() - 4, 4, ".bmp") == 0) path.resize(path.size() - 4);
			char n[32]; snprintf(n, sizeof n, "-%d.bmp", omg_frame);
			omf_save_shot(win, (path + n).c_str());
		}
		const char *e = strchr(p, ',');
		p = e ? e + 1 : p + strlen(p);
	}
}
#endif

// Called by OMediaGLRenderTarget::flip_buffers before swapping.
void omf_SDLFrameHook(SDL_Window *win)
{
	omg_frame++;
#ifndef __ANDROID__
	omf_test_hooks(win);
#endif
	// OMT_SCREENSHOT saves frame OMT_FRAMES (default 30) and quits; OMT_FRAMES alone just quits there
	const char *shot = getenv("OMT_SCREENSHOT");
	const char *frames = getenv("OMT_FRAMES");
	if (!shot && !frames) return;
	int want = frames ? atoi(frames) : 30;
	if (omg_frame != want) return;

	if (shot) omf_save_shot(win, shot);
	SDL_Event q; SDL_zero(q); q.type = SDL_QUIT; SDL_PushEvent(&q);
}

static void SDLCALL omf_log(void *, int category, SDL_LogPriority priority, const char *message)
{
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "OMT", "%s", message);
#else
	fprintf(stderr, "%s\n", message);
#endif
	if (omg_logfile) { fprintf(omg_logfile, "%s\n", message); fflush(omg_logfile); }
}

static bool omf_isdir(const string &p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }

static string omf_find_data_dir(void)
{
	const char *env = getenv("OMT_DATA");
	if (env && *env) return env;
#ifdef __ANDROID__
	const char *ext = SDL_AndroidGetExternalStoragePath();
	if (ext)
	{
		string base = ext;
		// files/game is written by the in-app importer (SetupActivity) and always readable.
		if (omf_isdir(base + "/game/omt")) return base + "/game";
		if (omf_isdir(base + "/omt")) return base;
		if (omf_isdir(base + "/Program_Executable_Files/omt")) return base + "/Program_Executable_Files";
		return base;      // nothing copied yet; the app reports it
	}
#endif
	return "";
}

int main(int argc, char **argv)
{
	SDL_LogSetOutputFunction(omf_log, NULL);
	string data = omf_find_data_dir();
#ifdef __ANDROID__
	const char *ext = SDL_AndroidGetExternalStoragePath();
	if (ext) omg_logfile = fopen((string(ext) + "/omt-log.txt").c_str(), "w");
#endif
	if (!data.empty() && chdir(data.c_str()) != 0) SDL_Log("OMT: cannot open data folder %s", data.c_str());
	SDL_Log("OMT: data folder %s (%s)", data.empty() ? "." : data.c_str(),
			omf_isdir("omt") ? "game data found" : "NO omt/ folder here - copy the game files first");
	omt_main();
	if (omg_logfile) fclose(omg_logfile);
	SDL_Quit();
	return 0;
}
