/*  OMT Viewer - loads Jimmy Neutron vs. Jimmy Negatron .omt databases through the real OMT engine
    (OMediaDataBase -> OMedia3DShape/Material/Canvas) and renders them with OMT's renderer.
    Desktop and Android (AYN Thor etc.).

    Controls
      Gamepad   left stick move · right stick look · L2/R2 down/up · hold A fast
                L1/R1 previous/next level · Start reset camera · B/Back quit
      Keyboard  WASD/arrows move+turn · R/F tilt · Space/Ctrl up/down · Shift fast
                PageUp/PageDown or [ ] change level · Home reset · Esc quit
      Touch     drag look · two-finger drag move · tap left/right edge change level

    Env (desktop testing): OMT_DB=omt\\level1.omt  OMT_CAM="x,y,z,pitch,yaw"  OMT_LIGHT=<deg>
                           OMT_PLAYSOUND=1  OMT_43=1 (4:3 instead of widescreen)
*/

#include "OMediaApplication.h"
#include "OMediaFullscreenWindow.h"
#include "OMediaMonitorMap.h"
#include "OMediaCanvas.h"
#include "OMediaFilePath.h"
#include "OMediaFileStream.h"
#include "OMediaRendererDef.h"
#include "OMedia3DShapeElement.h"
#include "OMediaWorld.h"
#include "OMediaViewPort.h"
#include "OMediaLayer.h"
#include "OMediaDataBase.h"
#include "OMedia3DMaterial.h"
#include "OMediaLight.h"
#include "OMedia3DShape.h"
#include "OMediaSound.h"
#include "OMediaSoundEngine.h"
#include "OMediaEngineFactory.h"
#include "OMediaPeriodical.h"
#include "OMediaTrigo.h"
#include "OMediaError.h"
#include "OMediaDefMessages.h"
#include "OMediaSDLRetarget.h"
#include <SDL.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <float.h>
#include <math.h>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Touch input collected by an SDL event watch (OMT's event manager owns the event queue)

static float g_touch_look_x = 0, g_touch_look_y = 0, g_touch_move_x = 0, g_touch_move_y = 0;
static int g_touch_fingers = 0, g_touch_tap = 0;      // tap: -1 left edge, +1 right edge
static Uint32 g_touch_down_time = 0;
static float g_touch_down_x = 0, g_touch_down_y = 0;

static int SDLCALL touch_watch(void *, SDL_Event *e)
{
	switch (e->type)
	{
		case SDL_FINGERDOWN:
			g_touch_fingers = SDL_GetNumTouchFingers(e->tfinger.touchId);
			if (g_touch_fingers == 1) { g_touch_down_time = SDL_GetTicks(); g_touch_down_x = e->tfinger.x; g_touch_down_y = e->tfinger.y; }
			break;
		case SDL_FINGERUP:
			if (g_touch_fingers == 1 && SDL_GetTicks() - g_touch_down_time < 250 &&
				fabsf(e->tfinger.x - g_touch_down_x) < 0.02f && fabsf(e->tfinger.y - g_touch_down_y) < 0.02f)
			{
				if (e->tfinger.x < 0.12f) g_touch_tap = -1;
				else if (e->tfinger.x > 0.88f) g_touch_tap = 1;
			}
			g_touch_fingers = SDL_GetNumTouchFingers(e->tfinger.touchId);
			break;
		case SDL_FINGERMOTION:
			if (SDL_GetNumTouchFingers(e->tfinger.touchId) >= 2)
			{ g_touch_move_x += e->tfinger.dx * 0.5f; g_touch_move_y += e->tfinger.dy * 0.5f; }
			else { g_touch_look_x += e->tfinger.dx; g_touch_look_y += e->tfinger.dy; }
			break;
	}
	return 1;
}

// ---------------------------------------------------------------------------

static bool nocase_less(const string &a, const string &b) { return strcasecmp(a.c_str(), b.c_str()) < 0; }

static bool ends_with_omt(const char *n)
{
	size_t l = strlen(n);
	return l > 4 && strcasecmp(n + l - 4, ".omt") == 0;
}

class Viewer : public OMediaApplication, public OMediaPeriodical
{
	public:
	OMediaMonitorMap	*monitors;
	OMediaWindow		*window;
	OMediaWorld			*world;
	OMediaViewPort		*viewport;
	OMediaLayer			*layer;
	OMediaDataBase		*database;
	OMediaFileStream	*dbfile;
	OMediaSoundEngine	*sound_engine;

	vector<string>					levels;
	int								level_index;
	vector<OMedia3DShapeElement*>	elements;
	vector<OMedia3DShape*>			locked_shapes;
	float							speed;
	float							home[5];
	Uint32							last_ticks;
	SDL_GameController				*pad;
	Uint32							pad_scan_ticks;
	bool							prev_l1, prev_r1, prev_start;

	Viewer()
	{
		database = NULL; dbfile = NULL; sound_engine = NULL; pad = NULL;
		level_index = 0; speed = 3000; last_ticks = 0; pad_scan_ticks = 0;
		prev_l1 = prev_r1 = prev_start = false;

		OMediaDataBase::set_cache_size(0);
		omd_REGISTERCLASS(OMediaCanvas);
		omd_REGISTERCLASS(OMedia3DShape);
		omd_REGISTERCLASS(OMedia3DMaterial);
		omd_REGISTERCLASS(OMediaSound);

		find_levels();
		if (!getenv("OMT_43")) OMediaSDLVideoEngine::set_widescreen(480);
		init_display();
		init_world();
		SDL_AddEventWatch(touch_watch, NULL);
		if (!load_level(level_index)) change_level(+1);
		// Test hook: step through N levels at start (exercises unload/reload).
		for (int i = getenv("OMT_STEP_LEVELS") ? atoi(getenv("OMT_STEP_LEVELS")) : 0; i > 0; i--) change_level(+1);
	}

	virtual ~Viewer()
	{
		SDL_DelEventWatch(touch_watch, NULL);
		unload_level();
		delete world;
		delete monitors;
		delete sound_engine;
		if (pad) SDL_GameControllerClose(pad);
	}

	// * Level list: every .omt with 3D shapes, levels first. OMT_DB picks the starting one.

	void find_levels(void)
	{
		// The game's level archives. Opened by name: on Android, folders copied over USB into
		// Android/data/<app>/files can be readable while listing them is refused.
		static const char *known[] = {
			"level1.omt", "level1_a.omt", "level1_b.omt", "level1_c.omt", "level1b.omt", "level1C.omt",
			"level1d.omt", "level3.omt", "level3a.omt", "level3c.omt", "level3d.omt", "level4.omt",
			"level4a.omt", "level4b.omt", "level5.omt", "level5a.omt", "level5b.omt", "level5b1.omt",
			"level5c.omt", "VR01.omt", "VR02.omt", "bonus01.omt", "promotion.omt", NULL };
		vector<string> names;
		for (int i = 0; known[i]; i++)
		{
			string real = omf_SDLResolvePath(string("omt/") + known[i]);
			FILE *f = fopen(real.c_str(), "rb");
			if (f) { fclose(f); names.push_back(known[i]); }
			else if (i == 0) SDL_Log("viewer: cannot open %s: %s", real.c_str(), strerror(errno));
		}
		// Also pick up anything else in the folder, if listing is allowed.
		DIR *d = opendir("omt");
		if (d)
		{
			struct dirent *e;
			while ((e = readdir(d)))
			{
				if (!ends_with_omt(e->d_name)) continue;
				bool have = false;
				for (size_t k = 0; k < names.size(); k++) if (strcasecmp(names[k].c_str(), e->d_name) == 0) have = true;
				if (!have) names.push_back(e->d_name);
			}
			closedir(d);
		}
		else SDL_Log("viewer: cannot list omt/: %s (using the built-in level list)", strerror(errno));
		if (names.empty())
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game files not readable",
				"The game's level files (omt/level1.omt ...) could not be opened.\n"
				"Clear the app's storage (App info > Storage) and start it again to import\n"
				"Negatron-GameFiles.zip. Details: Android/data/com.sirbilliams.negatron/files/omt-log.txt", NULL);
			omd_EXCEPTION(omcerr_CantOpen);
		}
		sort(names.begin(), names.end(), nocase_less);
		// Real levels first (level*, then VR*, bonus, promotion); skip music/voice/2D/sky archives.
		static const char *skip[] = { "music", "voice", "sound", "screens", "sprites", "icons",
									  "editor", "retained", "alpha", "camera", "effects", "permanent", "doors", NULL };
		vector<string> rest;
		for (size_t i = 0; i < names.size(); i++)
		{
			string low = names[i]; for (size_t k = 0; k < low.size(); k++) low[k] = tolower(low[k]);
			bool s = low.find("sky") != string::npos;
			for (int k = 0; skip[k]; k++) if (low.find(skip[k]) == 0) s = true;
			if (s) continue;
			if (low.find("level") == 0) levels.push_back(names[i]); else rest.push_back(names[i]);
		}
		levels.insert(levels.end(), rest.begin(), rest.end());
		SDL_Log("viewer: %d level archives", (int)levels.size());
		const char *want = getenv("OMT_DB") ? getenv("OMT_DB") : "level1.omt";
		if (want)
		{
			string w = want; size_t p = w.find_last_of("\\/"); if (p != string::npos) w = w.substr(p + 1);
			for (size_t i = 0; i < levels.size(); i++) if (strcasecmp(levels[i].c_str(), w.c_str()) == 0) level_index = i;
			if (strcasecmp(levels.empty() ? "" : levels[level_index].c_str(), w.c_str()) != 0)
			{ levels.insert(levels.begin(), w); level_index = 0; }
		}
		if (levels.empty()) omd_EXCEPTION(omcerr_CantFind);
	}

	void init_display(void)
	{
		// The game itself uses OMediaFullscreenWindow; use the same path here.
		window = new OMediaFullscreenWindow(this, true);
		window->set_title("Negatron Viewer");
		omt_EngineID video_engine = *(OMediaEngineFactory::get_factory()->video_engines.begin());
		monitors = new OMediaMonitorMap(video_engine, window);
		window->link_monitor_map(monitors);
		for (omt_VideoEngineList::iterator vi = monitors->engines.begin(); vi != monitors->engines.end(); vi++)
		{
			(*vi)->link(&(*(*vi)->get_video_cards()->begin()));
			OMediaRendererDef *def = NULL;
			for (omt_RendererDefList::iterator ri = (*vi)->get_renderer_list()->begin(); ri != (*vi)->get_renderer_list()->end(); ri++)
				if ((*ri).attributes & omcrdattr_Accelerated) { def = &(*ri); break; }
			if (!def && (*vi)->get_renderer_list()->size()) def = &(*(*vi)->get_renderer_list()->begin());
			if (def) { (*vi)->select_renderer(def, def->zbuffer_depth); SDL_Log("renderer: %s", def->name.c_str()); }
		}
		window->show();
	}

	void init_world(void)
	{
		OMediaRect r;
		world = new OMediaWorld;
		viewport = new OMediaViewPort(window);
		viewport->link(world);
		viewport->link_window(window);
		r.set(0, 0, 0, 0);
		viewport->setbounds(&r);
		viewport->set_bounds_mode(omcpbc_Right, omvpbmc_RightRelative);
		viewport->set_bounds_mode(omcpbc_Bottom, omvpbmc_BottomRelative);

		OMediaFARGBColor sky(1.0f, 0.55f, 0.72f, 0.95f);
		layer = new OMediaLayer;
		layer->link(world);
		layer->add_flags(omlayerf_ClearColor);
		layer->set_clear_color(sky);
		layer->set_near_clip(10.0f);
		layer->set_far_clip(200000.0f);

		// Directional sun + strong ambient term (the game's levels are mostly ambient-lit).
		int lp = getenv("OMT_LIGHT") ? atoi(getenv("OMT_LIGHT")) : -50;
		OMediaLight *light = new OMediaLight;
		light->link(world);
		light->set_light_type(omclt_Directional);
		light->set_angle(omd_Deg2Angle(lp), omd_Deg2Angle(30), 0);
		OMediaFARGBColor amb(1.0f, 0.55f, 0.55f, 0.55f), dif(1.0f, 0.6f, 0.6f, 0.6f);
		light->set_ambient(amb);
		light->set_diffuse(dif);
	}

	// * Load / unload one archive

	void unload_level(void)
	{
		for (size_t i = 0; i < elements.size(); i++) delete elements[i];
		elements.clear();
		for (size_t i = 0; i < locked_shapes.size(); i++) locked_shapes[i]->db_unlock();
		locked_shapes.clear();
		delete database; database = NULL;
		delete dbfile; dbfile = NULL;
	}

	bool load_level(int index)
	{
		unload_level();
		const string fn = "omt/" + levels[index];
		try
		{
			OMediaFilePath path(fn);
			dbfile = new OMediaFileStream;
			dbfile->setpath(&path);
			dbfile->open(omcfp_Read);
			database = new OMediaDataBase(dbfile);
		}
		catch (OMediaError &e)
		{
			SDL_Log("viewer: cannot open %s (error %d)", fn.c_str(), (int)e.errorcode);
			delete dbfile; dbfile = NULL; database = NULL;
			return false;
		}

		omt_DBIterator it;
		int npolys = 0, skipped = 0;
		float mn[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, mx[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (OMediaDBObject *o = database->get_first_object(OMedia3DShape::db_type, it); o; o = database->get_next_object(it))
		{
			OMedia3DShape *shape = (OMedia3DShape*)o;
			string name; shape->get_chunk_name(name);
			string up = name; for (size_t i = 0; i < up.size(); i++) up[i] = toupper(up[i]);
			if (up.find("BLOCK") == 0 || up.find("COLLI") == 0) { skipped++; continue; }
			shape->db_lock();
			locked_shapes.push_back(shape);
			OMedia3DShapeElement *e = new OMedia3DShapeElement;
			e->link(world);
			e->place(0, 0, 0);
			e->set_shape(shape);
			elements.push_back(e);
			npolys += shape->get_polygons()->size();
			omt_VertexList *vl = shape->get_vertices();
			for (omt_VertexList::iterator v = vl->begin(); v != vl->end(); v++)
			{
				float p[3] = { (*v).x, (*v).y, (*v).z };
				for (int k = 0; k < 3; k++) { if (p[k] < mn[k]) mn[k] = p[k]; if (p[k] > mx[k]) mx[k] = p[k]; }
			}
		}
		if (elements.empty()) { SDL_Log("viewer: %s has no 3D shapes", fn.c_str()); return false; }

		if (getenv("OMT_PLAYSOUND"))
		{
			OMediaDBObject *o = database->get_first_object(OMediaSound::db_type, it);
			if (o)
			{
				o->db_lock();
				if (!sound_engine)
				{
					omt_EngineID se = *(OMediaEngineFactory::get_factory()->sound_engines.begin());
					sound_engine = OMediaEngineFactory::get_factory()->create_sound_engine(se, window);
					sound_engine->set_nchannels(4);
				}
				((OMediaSound*)o)->play(sound_engine, 0, false, false);
			}
		}

		float ext = fmaxf(mx[0] - mn[0], mx[2] - mn[2]);
		speed = fmaxf(500.0f, ext * 0.12f);
		const char *c = getenv("OMT_CAM");
		if (!(c && index == level_index && sscanf(c, "%f,%f,%f,%f,%f", &home[0], &home[1], &home[2], &home[3], &home[4]) == 5))
		{
			home[0] = (mn[0] + mx[0]) / 2; home[2] = mn[2] - ext * 0.15f; home[1] = mx[1] + ext * 0.45f;
			home[3] = -40; home[4] = 0;
		}
		reset_camera();
		window->set_title("Negatron Viewer - " + levels[index]);
		SDL_Log("viewer: %s  %d shapes, %d polygons (%d collision-only hidden)", levels[index].c_str(),
				(int)elements.size(), npolys, skipped);
		return true;
	}

	void reset_camera(void)
	{
		viewport->place(home[0], home[1], home[2]);
		viewport->set_angle(omd_Deg2Angle(home[3]), omd_Deg2Angle(home[4]), 0);
	}

	void change_level(int dir)
	{
		int n = levels.size();
		for (int tries = 0; tries < n; tries++)
		{
			level_index = (level_index + dir + n) % n;
			if (load_level(level_index)) return;
		}
	}

	// * Controls, run once per main-loop pass (outside world rendering, so reloading is safe)

	static float deadzone(Sint16 v) { float f = v / 32767.0f; return fabsf(f) < 0.18f ? 0 : f; }

	virtual void spend_time(void)
	{
		Uint32 now = SDL_GetTicks();
		float ms = last_ticks ? float(now - last_ticks) : 0;
		last_ticks = now;
		if (ms > 100) ms = 100;

		if (!pad && now - pad_scan_ticks > 1000)
		{
			pad_scan_ticks = now;
			for (int j = 0; j < SDL_NumJoysticks() && !pad; j++)
				if (SDL_IsGameController(j)) { pad = SDL_GameControllerOpen(j); if (pad) SDL_Log("viewer: controller %s", SDL_GameControllerName(pad)); }
		}
		if (pad && !SDL_GameControllerGetAttached(pad)) { SDL_GameControllerClose(pad); pad = NULL; }

		const Uint8 *k = SDL_GetKeyboardState(NULL);
		float fwd = 0, strafe = 0, rise = 0, turn = 0, tilt = 0;
		bool fast = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT];
		bool l1 = k[SDL_SCANCODE_PAGEUP] || k[SDL_SCANCODE_LEFTBRACKET];
		bool r1 = k[SDL_SCANCODE_PAGEDOWN] || k[SDL_SCANCODE_RIGHTBRACKET];
		bool start = k[SDL_SCANCODE_HOME];
		bool quit = k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_AC_BACK];

		if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) fwd += 1;
		if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) fwd -= 1;
		if (k[SDL_SCANCODE_D]) strafe += 1;
		if (k[SDL_SCANCODE_A]) strafe -= 1;
		if (k[SDL_SCANCODE_SPACE]) rise += 1;
		if (k[SDL_SCANCODE_LCTRL]) rise -= 1;
		if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_E]) turn += 1;
		if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_Q]) turn -= 1;
		if (k[SDL_SCANCODE_R]) tilt += 1;
		if (k[SDL_SCANCODE_F]) tilt -= 1;

		if (pad)
		{
			fwd -= deadzone(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY));
			strafe += deadzone(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX));
			turn += deadzone(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX));
			tilt -= deadzone(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY));
			rise += SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f;
			rise -= SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f;
			if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) fwd += 1;
			if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) fwd -= 1;
			if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) turn -= 1;
			if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) turn += 1;
			fast |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
			l1 |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
			r1 |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
			start |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START) != 0;
			quit |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
		}

		if (quit) { OMediaApplication::listen_to_message(omsg_Quit, NULL); return; }
		if (l1 && !prev_l1) change_level(-1);
		if (r1 && !prev_r1) change_level(+1);
		if (start && !prev_start) reset_camera();
		prev_l1 = l1; prev_r1 = r1; prev_start = start;
		if (g_touch_tap) { change_level(g_touch_tap); g_touch_tap = 0; }

		// Touch: drag = look (absolute angle change), two fingers = move
		float look_yaw = g_touch_look_x * 180.0f, look_pitch = -g_touch_look_y * 120.0f;
		fwd -= g_touch_move_y * 60.0f; strafe += g_touch_move_x * 60.0f;
		g_touch_look_x = g_touch_look_y = g_touch_move_x = g_touch_move_y = 0;

		float s = speed * (fast ? 3.0f : 1.0f) * ms / 1000.0f;
		float yaw = omd_Angle2DegF(viewport->get_angley()) * float(M_PI) / 180.0f;
		float dx = sinf(yaw) * fwd + cosf(yaw) * strafe;
		float dz = cosf(yaw) * fwd - sinf(yaw) * strafe;
		viewport->place(viewport->get_x() + dx * s, viewport->get_y() + rise * s, viewport->get_z() + dz * s);

		float dyaw = turn * 90.0f * ms / 1000.0f + look_yaw;
		float dpitch = tilt * 60.0f * ms / 1000.0f + look_pitch;
		viewport->set_angle(viewport->get_anglex() + int(omd_Deg2AngleF(1) * dpitch),
							viewport->get_angley() + int(omd_Deg2AngleF(1) * dyaw), 0);
	}
};

void omt_main(void)
{
	try
	{
		Viewer *app = new Viewer;
		app->start();
		delete app;
	}
	catch (OMediaError &e)
	{
		SDL_Log("OMT error %d (data %d) at %s:%d %s", (int)e.errorcode, (int)e.errordata,
				e.file.c_str(), e.line, e.error_string.c_str());
	}
}
