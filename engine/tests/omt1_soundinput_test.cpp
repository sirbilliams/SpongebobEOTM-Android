/*  OMT 1.x sound manager and input driver on the SDL layer (engine/omt1).

    Sound: loads a sounds/*.omt database, plays one of its sounds through OMediaDXSoundManager twice
    (centre, then hard left with set_pan), and checks what reached the audio device. Run with
    SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=<file>: the test reads the file back at the end.
    Input: opens a hidden window, pushes SDL mouse and finger events and checks that they reach an
    OMediaListener on the event manager's click broadcaster, and what the input driver reports.

    usage: omt1_soundinput_test [<sounds/x.omt>]      (default: OMT_DATA/sounds/help.omt)
    needs a display for the window (Xvfb) - SDL_VIDEODRIVER=dummy is no good for OpenGL windows.   */

#include "OMediaApplication.h"
#include "OMediaSingleWindow.h"
#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaSound.h"
#include "OMediaSoundManager.h"
#include "OMediaInputManager.h"
#include "OMediaEventManager.h"
#include "OMediaListener.h"
#include "OMediaBroadcaster.h"
#include "OMediaDefMessages.h"
#include "OMediaError.h"
#include "OMediaTimeCounter.h"
#include "OMediaSDLRetarget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { g_fail++; printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

class TestApp : public OMediaApplication {};

// An OMT listener on the click broadcaster, like the ones sb2.exe adds.
class ClickListener : public OMediaListener
{
	public:
	ClickListener() { downs = ups = 0; x = y = -1; }
	virtual void listen_to_message(omt_Message msg, void *param)
	{
		OMediaEvent *e = (OMediaEvent*)param;
		if (msg == omsg_MouseClick) { downs++; x = e->win_mouse_x; y = e->win_mouse_y; type = e->type; }
		else if (msg == omsg_MouseClick_Up) { ups++; x = e->win_mouse_x; y = e->win_mouse_y; type = e->type; }
	}
	int downs, ups, x, y;
	omt_EventType type;
};

static void run_for(unsigned int ms)
{
	unsigned int t0 = SDL_GetTicks();
	while (SDL_GetTicks() - t0 < ms) { OMediaPeriodical::devote_time(); SDL_Delay(5); }
}

static void push_mouse(Uint32 id, Uint32 type, int x, int y, Uint32 which = 0)
{
	SDL_Event e; SDL_zero(e);
	if (type == SDL_MOUSEMOTION) { e.motion.windowID = id; e.motion.x = x; e.motion.y = y; e.motion.which = which; }
	else { e.button.windowID = id; e.button.x = x; e.button.y = y; e.button.button = SDL_BUTTON_LEFT;
	       e.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED; e.button.which = which; }
	e.type = type;
	SDL_PushEvent(&e);
}

static void push_finger(Uint32 type, float x, float y)
{
	SDL_Event e; SDL_zero(e);
	e.type = type; e.tfinger.touchId = 1; e.tfinger.fingerId = 7; e.tfinger.x = x; e.tfinger.y = y;
	SDL_PushEvent(&e);
}

//------------------------------------------------------------------------------------------
static void test_input(TestApp *app)
{
	printf("-- input\n");
	OMediaSingleWindow *win = new OMediaSingleWindow(app, false);
	win->set_size(640, 480);
	win->show();
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)win->get_retarget();
	Uint32 id = SDL_GetWindowID(rtg->sdlwin);
	OMediaInputDriver::set_game_window(win);
	run_for(100);

	OMediaInputDriver *drv = OMediaInputManager::get_input_manager()->get_driver();
	CHECK(drv != NULL, "no driver");
	ClickListener l;
	OMediaEventManager::get_event_manager()->getclickbroadcaster()->addlistener(&l);

	// mouse: a click at (300,200) of the game's 640x480 window
	push_mouse(id, SDL_MOUSEBUTTONDOWN, 300, 200);
	run_for(50);
	CHECK(l.downs == 1 && l.x == 300 && l.y == 200 && l.type == omtet_LMouseDown, "mouse down: downs %d at %d,%d", l.downs, l.x, l.y);
	push_mouse(id, SDL_MOUSEBUTTONUP, 300, 200);
	run_for(50);
	CHECK(l.ups == 1 && l.x == 300 && l.y == 200, "mouse up: ups %d at %d,%d", l.ups, l.x, l.y);
	printf("mouse click reached the listener: down %d up %d at game %d,%d\n", l.downs, l.ups, l.x, l.y);

	// touch: a finger at 25% / 50% of the window is (160,240) in the game's space, with no mouse event at all
	short x = -1, y = -1;
	push_finger(SDL_FINGERDOWN, 0.25f, 0.5f);
	CHECK(drv->mouse_down(), "finger down is the mouse button");
	drv->get_mouse_position(x, y);
	CHECK(x == 160 && y == 240, "touch position %d,%d", x, y);
	printf("finger at (0.25,0.5): driver pointer %d,%d, button %d\n", x, y, (int)drv->mouse_down());
	push_finger(SDL_FINGERMOTION, 1.0f, 0.0f);
	drv->get_mouse_position(x, y);
	CHECK(x >= 639 && y <= 0, "touch move %d,%d", x, y);
	push_finger(SDL_FINGERUP, 0.75f, 0.25f);
	CHECK(!drv->mouse_down(), "finger up releases the button");
	run_for(20);

	// the offset is added; the custom mouse follows the system pointer's movement
	drv->set_mouse_offset(5, -3);
	SDL_WarpMouseInWindow(rtg->sdlwin, 200, 100);
	run_for(100);
	drv->get_mouse_position(x, y);
	CHECK(x == 205 && y == 97, "mouse + offset %d,%d", x, y);
	drv->set_mouse_offset(0, 0);

	// command keys, devices
	CHECK(drv->get_command_keys_status() == 0, "command keys");
	omt_HIDList *hid = drv->get_hid();
	int kb = 0, pads = 0;
	for (omt_HIDList::iterator i = hid->begin(); i != hid->end(); i++)
	{
		if ((*i)->type == omhidtc_Keyboard) kb++;
		if ((*i)->type == omhidtc_Joystick) pads++;
	}
	CHECK(kb == 1, "keyboards %d", kb);
	printf("hid devices %d (keyboard %d, controllers %d)\n", (int)hid->size(), kb, pads);

	OMediaEventManager::get_event_manager()->getclickbroadcaster()->removelistener(&l);
	OMediaInputDriver::set_game_window(NULL);
	delete win;
}

//------------------------------------------------------------------------------------------
static OMediaSound *find_sound(OMediaDataBase *db, float min_secs, float max_secs)
{
	omt_DBIterator it;
	for (OMediaDBObject *o = db->get_first_object(OMediaSound::db_type, it); o; o = db->get_next_object(it))
	{
		OMediaSound *s = (OMediaSound*)o;
		if (s->get_sound_secs() >= min_secs && s->get_sound_secs() <= max_secs) return s;
	}
	return NULL;
}

static void test_sound(const char *dbpath)
{
	printf("-- sound\n");
	OMediaSoundManager::setdefaultmanager(new OMediaDXSoundManager);
	OMediaSoundManager *m = OMediaSoundManager::getdefaultmanager();
	CHECK(m->get_attributes() == 7, "attributes %d", (int)m->get_attributes());
	CHECK(m->get_max_channels() == -1, "max channels %ld", m->get_max_channels());
	CHECK(m->get_nchannels() == 0, "channels at start %ld", m->get_nchannels());
	CHECK(m->set_nchannels(0xf) == 15 && m->get_nchannels() == 15, "set_nchannels(15)");
	CHECK(m->set_nchannels(3) == 3, "set_nchannels(3)");
	CHECK(m->set_nchannels(4) == 4, "set_nchannels(4)");
	CHECK(!m->play(4, NULL, false, false), "play on a channel that does not exist");

	omd_REGISTERCLASS(OMediaSound);
	OMediaFilePath path(dbpath);
	OMediaFileStream f; f.setpath(&path); f.open(omcfp_Read);
	OMediaDataBase *db = new OMediaDataBase(&f);
	OMediaSound *snd = find_sound(db, 0.3f, 1.5f);
	if (!snd) snd = find_sound(db, 0.05f, 4.0f);
	CHECK(snd != NULL, "no sound in %s", dbpath);
	if (!snd) { delete db; return; }
	string name; snd->get_chunk_name(name);
	printf("sound '%s' %.2f s from %s\n", name.c_str(), snd->get_sound_secs(), dbpath);

	CHECK(m->play(0, snd, false, false), "play on channel 0");
	CHECK(!m->play(0, snd, false, false), "channel 0 is busy and the sound is not queued");
	unsigned int ms = (unsigned int)(snd->get_sound_secs() * 1000) + 400;
	run_for(ms);
	unsigned int e1, t1;
	m->get_channel(0)->get_playing_sound_info(e1, t1);

	// second time: channel 1, hard left (pan -10000), after a silent gap
	m->get_channel(1)->set_pan(-10000);
	CHECK(m->play(1, snd, false, false), "play on channel 1");
	run_for(ms);

	m->set_nchannels(0);
	delete db;
}

struct Region { size_t a, b; int pl, pr; };

// Reads what the disk driver wrote: stereo S16 at the device rate, split at silent gaps.
static void check_audio(const char *file)
{
	FILE *fp = fopen(file, "rb");
	CHECK(fp != NULL, "no audio file %s", file);
	if (!fp) return;
	std::vector<short> pcm;
	short buf[4096];
	size_t n;
	while ((n = fread(buf, sizeof(short), 4096, fp)) > 0) pcm.insert(pcm.end(), buf, buf + n);
	fclose(fp);
	size_t frames = pcm.size() / 2;
	printf("audio file: %lu frames\n", (unsigned long)frames);
	// regions: runs of non-silent frames separated by >= 2000 silent frames
	std::vector<Region> regions;
	size_t silent = 0;
	for (size_t i = 0; i < frames; i++)
	{
		int l = abs(pcm[i * 2]), r = abs(pcm[i * 2 + 1]);
		if (l || r)
		{
			if (regions.empty() || silent >= 2000) { Region g = { i, i, 0, 0 }; regions.push_back(g); }
			Region &g = regions.back();
			g.b = i; if (l > g.pl) g.pl = l; if (r > g.pr) g.pr = r;
			silent = 0;
		}
		else silent++;
	}
	for (size_t i = 0; i < regions.size(); i++)
		printf("  region %lu: frames %lu..%lu  peak left %d right %d\n", (unsigned long)i, (unsigned long)regions[i].a, (unsigned long)regions[i].b, regions[i].pl, regions[i].pr);
	CHECK(regions.size() == 2, "expected 2 sounds in the audio file, found %lu", (unsigned long)regions.size());
	if (regions.size() == 2)
	{
		CHECK(regions[0].pl > 100 && regions[0].pr > 100, "first sound should be centred");
		CHECK(regions[1].pl > 100 && regions[1].pr == 0, "second sound should be hard left: right peak %d", regions[1].pr);
	}
}

void omt_main(void)
{
	const char *dbpath = getenv("OMT_SOUNDDB");
	string def;
	if (!dbpath)
	{
		const char *d = getenv("OMT_DATA");
		def = string(d ? d : ".") + "/sounds/help.omt";
		dbpath = def.c_str();
	}
	const char *audiofile = getenv("SDL_DISKAUDIOFILE");
	const char *drv = getenv("SDL_AUDIODRIVER");
	bool disk = drv && !strcmp(drv, "disk");

	OMediaDataBase::set_cache_size(0);
	TestApp *app = new TestApp;
	test_input(app);
	test_sound(dbpath);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);			// closes the device: the disk file is complete
	if (disk) check_audio(audiofile ? audiofile : "sdlaudio.raw");
	else printf("(SDL_AUDIODRIVER is not disk: audio not checked)\n");
	delete app;
	printf("%s\n", g_fail ? "FAILED" : "OK");
	if (g_fail) exit(1);
}
