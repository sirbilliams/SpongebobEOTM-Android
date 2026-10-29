/*  sb_scenetest: the 2D scene runtime of the reconstructed game (game/src/game) on lane A's OMT 1.x layer, headless.
    Starts the first scene (c1l0s1) with its script, lets the script switch to c1l2s6, clicks objects with
    synthetic SDL mouse events, then enters c1l2s1 and runs a dialogue; frames are saved to <shots dir>.
    usage: sb_scenetest <game folder> [frames=1600] [shots dir=.shots]      env: OMT_WINDOW_SIZE=640x480 SDL_AUDIODRIVER=dummy
    A step list "frame:action" can replace the default run: SBT_STEPS="120:click 140 80,200:key 3,..." (actions:
    click X Y | move X Y | key N | scene NAME | event NAME | shot) ; SBT_SCENE picks the first scene.            */

#include "Game.h"
#include "SceneScriptHost.h"
#include "OMediaDXSurfaceWorld.h"
#include "OMediaDisplay.h"
#include "OMediaEventManager.h"
#include "OMediaError.h"
#include "OMediaPeriodical.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <string>

int omf_SDLFrameCount(void);		// sdl/OMediaSDLMain.cpp
extern char **omt_argv;
extern int omt_argc;

using namespace sbgame;

struct Step { int frame; std::string what; };

static void send_mouse(OMediaEventManager *em, int type, int x, int y)
{
	SDL_Event e;
	memset(&e, 0, sizeof(e));
	e.type = type;
	e.button.button = SDL_BUTTON_LEFT;
	e.button.x = x;
	e.button.y = y;
	em->handle_sdl_event(&e);
}

static void send_key(OMediaEventManager *em, SDL_Keycode k)
{
	SDL_Event e;
	memset(&e, 0, sizeof(e));
	e.type = SDL_KEYDOWN;
	e.key.keysym.sym = k;
	em->handle_sdl_event(&e);
}

static std::vector<Step> default_steps()
{
	// frame numbers at 33 ms per frame
	const char *s[][2] = {
		{"60", "shot"},                          // letterbox init: black
		{"110", "shot"},                         // the transition opens
		{"150", "shot"},
		{"330", "shot"},
		{"420", "shot"},                         // c1l2s6 after the loading script
		{"440", "move 390 160"}, {"445", "shot"},   // hover: the antenna ("Antenna")
		{"450", "click 390 160"}, {"460", "shot"},  // click: event h126c (comment)
		{"560", "click 140 80"}, {"570", "shot"},   // the sign spot: h126b
		{"680", "scene c1l2s1"},                 // the Krusty Krab
		{"700", "shot"},
		{"730", "click 560 200"}, {"740", "shot"}, // the weirdo in the window (ANIM, click to play)
		{"790", "event AAA"}, {"800", "shot"},   // the hungry customer: dialogue, the opening statement (voice)
		{"880", "shot"},
		{"1050", "click 300 100"},               // a click skips the line that is still playing
		{"1100", "shot"},                        // the choice list
		{"1110", "move 100 458"}, {"1115", "shot"},   // hover on a choice
		{"1120", "click 100 458"}, {"1130", "shot"},  // pick it: its lines play
		{"1300", "shot"},
		{"1500", "key 1"}, {"1510", "shot"},     // Escape ends the conversation
		{"1560", "click 60 440"}, {"1570", "shot"},
	};
	std::vector<Step> v;
	for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); ++i) { Step st; st.frame = atoi(s[i][0]); st.what = s[i][1]; v.push_back(st); }
	return v;
}

static std::vector<Step> parse_steps(const char *env)
{
	std::vector<Step> v;
	std::string e = env;
	size_t p = 0;
	while (p < e.size()) {
		size_t c = e.find(',', p);
		if (c == std::string::npos) c = e.size();
		std::string item = e.substr(p, c - p);
		size_t k = item.find(':');
		if (k != std::string::npos) { Step st; st.frame = atoi(item.c_str()); st.what = item.substr(k + 1); v.push_back(st); }
		p = c + 1;
	}
	return v;
}

void omt_main(void)
{
	if (omt_argc < 2) { printf("usage: sb_scenetest <game folder> [frames] [shots dir]\n"); return; }
	std::string folder = omt_argv[1];
	int frames = omt_argc > 2 ? atoi(omt_argv[2]) : 1600;
	std::string shots = omt_argc > 3 ? omt_argv[3] : ".shots";
	setenv("SDL_AUDIODRIVER", "dummy", 0);
	try
	{
		OMediaDirectX::install(OMediaDirectX::omcdx_Video);
		OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
		OMediaVideoMode *mode = NULL;
		OMediaVideoDriver &drv = dev->get_video_drivers()->front();
		for (size_t i = 0; i < drv.modes.size(); i++)
			if (drv.modes[i].width == 640 && drv.modes[i].height == 480 && drv.modes[i].depth == 16) mode = &drv.modes[i];
		dev->setmode(mode, 0);
		OMediaDoubleCanvas *dc = new OMediaDoubleCanvas;
		OMediaDisplay *display = new OMediaDisplay(NULL, dc, "sb_scenetest");
		OMediaEventManager *em = new OMediaEventManager;

		Game *game = new Game;
		if (!game->init(folder, dc)) { printf("game init failed\n"); return; }
		game->attachInput();
		const char *first = getenv("SBT_SCENE");
		game->requestScene(first ? first : "c1l0s1");

		std::vector<Step> steps = getenv("SBT_STEPS") ? parse_steps(getenv("SBT_STEPS")) : default_steps();
		std::string base = shots + "/sb_scenetest.bmp";
		setenv("OMT_SCREENSHOT", base.c_str(), 1);
		int pendingShot = 0;		// engine frame the engine will save next, renamed to the step's frame afterwards
		int pendingIter = 0;
		setenv("OMT_FRAMES", "99999999", 1);		// the engine hook quits at OMT_FRAMES (default 30) when OMT_SCREENSHOT is set

		unsigned last = 0;
		for (int frame = 1; frame <= frames; ++frame)
		{
			if (pendingShot) {
				char from[256], to[256];
				snprintf(from, sizeof from, "%s/sb_scenetest-%d.bmp", shots.c_str(), pendingShot);
				snprintf(to, sizeof to, "%s/f%04d.bmp", shots.c_str(), pendingIter);
				if (rename(from, to) != 0) printf("shot of frame %d was not written\n", pendingIter);
				pendingShot = 0;
			}
			SDL_Event ev;
			while (SDL_PollEvent(&ev))
			{
				if (ev.type == SDL_QUIT) { frame = frames + 1; break; }
				em->handle_sdl_event(&ev);
			}
			for (size_t i = 0; i < steps.size(); ++i)
			{
				if (steps[i].frame != frame) continue;
				const std::string &w = steps[i].what;
				int x = 0, y = 0, k = 0;
				char name[64];
				if (sscanf(w.c_str(), "click %d %d", &x, &y) == 2) {
					printf("[%d] click %d,%d\n", frame, x, y);
					game->mouseMove(x, y);
					send_mouse(em, SDL_MOUSEBUTTONDOWN, x, y);
					send_mouse(em, SDL_MOUSEBUTTONUP, x, y);
				} else if (sscanf(w.c_str(), "move %d %d", &x, &y) == 2) {
					game->mouseMove(x, y);
				} else if (sscanf(w.c_str(), "key %d", &k) == 1) {
					printf("[%d] key %d\n", frame, k);
					if (k == 3) send_key(em, SDLK_RETURN);
					else if (k == 1) send_key(em, SDLK_ESCAPE);
					else if (k == 8) send_key(em, SDLK_UP);
					else if (k == 9) send_key(em, SDLK_DOWN);
				} else if (sscanf(w.c_str(), "scene %63s", name) == 1) {
					printf("[%d] scene %s\n", frame, name);
					game->switchTo(name);
				} else if (sscanf(w.c_str(), "event %63s", name) == 1) {
					// 3D: picking a character is lane A's; the test sends the character's Event straight to the classifier
					printf("[%d] event %s\n", frame, name);
					game->switchTo(name);
				} else if (w == "shot") {
					pendingShot = omf_SDLFrameCount() + 1;
					pendingIter = frame;
					char b[32]; sprintf(b, "%d", pendingShot);
					setenv("OMT_SHOTS", b, 1);
					printf("[%d] shot (scene %s, script %s, dialogue %s)\n", frame,
						   game->scene() ? game->scene()->name().c_str() : "-", game->runner() ? game->runner()->name().c_str() : "-",
						   game->scene() && game->scene()->dialogue() && game->scene()->dialogue()->busy() ? "on" : "off");
				}
			}
			unsigned now = SDL_GetTicks();
			float dt = last ? (float)(now - last) : 16.0f;
			last = now;
			if (dt > 66.0f) dt = 66.0f;		// frames the machine could not render in time do not skip a script
			game->update(dt);
			OMediaPeriodical::devote_time();
			game->world()->update();
			SDL_Delay(10);
		}
		printf("stub calls (3D/character): %u, movies: %u\n", game->host()->stubCalls(), (unsigned)game->movieLog.size());
		delete game;
		delete display;
	}
	catch (OMediaError &e)
	{
		printf("OMT error %d at %s:%d\n", (int)e.errorcode, e.file.c_str(), e.line);
	}
}
