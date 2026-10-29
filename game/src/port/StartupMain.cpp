// Startup slice reconstructed from sb2.exe. See docs/startup.md for engine gaps.
#include "OMediaApplication.h"
#include "OMediaFullscreenWindow.h"
#include "OMediaMonitorMap.h"
#include "OMediaRendererDef.h"
#include "OMediaPeriodical.h"
#include "OMediaWorld.h"
#include "OMediaViewPort.h"
#include "OMediaLayer.h"
#include "OMediaEngineFactory.h"
#include "OMediaError.h"
#include "OMediaSDLRetarget.h"
#include "CrashLog.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <dirent.h>
#include "bink.h"
#include "OMediaSoundManager.h"
#include "OMediaDataBase.h"
#include "OMediaCanvas.h"
#include "OMedia3DMaterial.h"
#include "OMediaSound.h"
#include "../../granny/granny_compat.h"

extern void omf_SDLFrameHook(SDL_Window *win);		// OMediaSDLMain.cpp: frame count, OMT_SCREENSHOT

class StartupApp : public OMediaApplication, public OMediaPeriodical
{
	public:
	OMediaMonitorMap	*monitors;
	OMediaWindow		*window;
	OMediaWorld			*world;
	OMediaViewPort		*viewport;
	OMediaLayer			*layer;

	StartupApp()
	{
		SDL_Log("0x4097d0: application constructor");
		// register_object and cache=0 match OMT.dll; the shared implementation checks
		// !cache_size before purging just as 0x10054d30 does.
		OMediaDataBase::register_object(0x43616e76, OMediaCanvas::db_builder, 0);
		OMediaDataBase::register_object(0x33444d61, OMedia3DMaterial::db_builder, 0);
		OMediaDataBase::register_object(0x57617665, OMediaSound::db_builder, 0);
		OMediaDataBase::set_cache_size(0);
		SDL_ShowCursor(SDL_DISABLE);
		init_display();
		// port: DirectX::install(7)'s sound backend is already rebuilt on SDL.
		OMediaSoundManager::setdefaultmanager(new OMediaDXSoundManager);
		init_world();
		init_menu_controller();
		SDL_Log("TODO(0x40cdb0): connect OMT1 event broadcaster and window controller");
		init_font();
		load_config();
		SDL_Log("0x40e2c0: SetVideoMode(640,480,16,%d)", software);
		SDL_Log("TODO(0x40e2c0): bind OMT1 ScreenDevice driver/mode to the SDL display bridge");
		SDL_Log("TODO(0x40ceb0): implement missing OMT1 OMedia3DAbstractRasterizer; SDL GL presents movies");
		init_granny();
		init_movie_sound();
		SDL_Log("TODO(0x4097d0): connect missing OMT1 WorldClickDispatcher");
	}

	// 0x40cc10 / 0x40e2c0. port: existing SDL/OMT 2.5 window bridge until
	// OMT1 has the Application/Display lifecycle; never mix their world ABIs.
	void init_display()
	{
		SDL_Log("0x40cc10: InitDisplay");
		window = new OMediaFullscreenWindow(this, true);
		window->set_title("SpongeBob EOTM");
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

	// 0x40d010 / 0x40d1d0. port: the bootstrap world establishes the GL context.
	void init_world()
	{
		SDL_Log("TODO(0x40d010): connect OMT1 game world and viewport to display");
		OMediaRect r;
		world = new OMediaWorld;
		viewport = new OMediaViewPort(window);
		viewport->link(world);
		viewport->link_window(window);
		r.set(0, 0, 0, 0);
		viewport->setbounds(&r);
		viewport->set_bounds_mode(omcpbc_Right, omvpbmc_RightRelative);
		viewport->set_bounds_mode(omcpbc_Bottom, omvpbmc_BottomRelative);
		OMediaFARGBColor sky(1.0f, 0.05f, 0.08f, 0.16f);
		layer = new OMediaLayer;
		layer->link(world);
		layer->add_flags(omlayerf_ClearColor);
		layer->set_clear_color(sky);
	}

	void init_menu_controller() // 0x40d120
	{
		SDL_Log("TODO(0x40d120): connect menu/inventory controller construction");
	}

	void init_font() // 0x40d0b0
	{
		SDL_Log("TODO(0x40d0b0): connect OMT1 font database and missing CStringField/CCaption");
	}

	granny_uint32 granny_version;
	void init_granny() // 0x40d2d0
	{
		granny_version = 0;
		granny_result result = GrannyOpenVersion("1.2b", "win32", "10-4-2000",
			"(C) Copyright 1999-2000 RAD Game Tools, Inc.  All Rights Reserved.", &granny_version);
		if (result != GrannyOK) {
			SDL_Log("0x40d2d0: GRANNY FAILED TO INITIALIZE (%d)", (int)result);
			throw result;
		}
		GrannyAllowTextureFormat(granny_version, 0xb);
		SDL_Log("0x40d2d0: Granny 1.2b initialized; texture format 0xb");
	}

	void init_movie_sound() // 0x40c2a0
	{
		OMediaSoundManager *manager = OMediaSoundManager::getdefaultmanager();
		BinkSetSoundSystem(BinkOpenDirectSound, 0); // port: shared SDL mixer, no DirectSound pointer.
		manager->set_nchannels(15);
		SDL_Log("0x40c2a0: Sound Manager Channels = %ld", manager->get_nchannels());
		BinkPortSetSurfaceType(BINKSURFACE32);
	}

	virtual ~StartupApp()
	{
		delete world;
		delete monitors;
		OMediaSoundManager::setdefaultmanager(NULL);
		GrannyClose(granny_version); // 0x40d330
	}

	virtual void spend_time(void)
	{
		if (!SDL_GL_GetCurrentWindow()) return;		// the engine has not rendered yet
		run();
		do_quit = true;
	}

	bool software;
	int music, sound, voice, save, text, shadows;

	// 0x409ed0: registry-free equivalent of the six DWORD queries.
	void load_config()
	{
		music = sound = voice = 7; save = -1; text = shadows = 1;
		software = false;
		extern int omt_argc; extern char **omt_argv;
		for (int i = 1; i < omt_argc; ++i)
			if (strcmp(omt_argv[i], "-sw") == 0) software = true;
		// port: per-user config replaces the registry; never write into the install.
		char *pref = SDL_GetPrefPath("THQ", "SpongeBob 2");
		std::string path = getenv("OMT_CONFIG") ? getenv("OMT_CONFIG") :
			std::string(pref ? pref : "") + "settings.cfg";
		SDL_free(pref);
		FILE *f = fopen(path.c_str(), "r");
		if (f) {
			char line[128], key[32]; int value;
			while (fgets(line, sizeof line, f))
				if (sscanf(line, "%31[^=]=%d", key, &value) == 2) {
					if (!strcmp(key, "Music")) music = value;
					else if (!strcmp(key, "Sound")) sound = value;
					else if (!strcmp(key, "Voice")) voice = value;
					else if (!strcmp(key, "Save")) save = value;
					else if (!strcmp(key, "Text")) text = value;
					else if (!strcmp(key, "Shadows")) shadows = value;
				}
			fclose(f);
		}
		if (software) shadows = 0;
		SDL_Log("0x409ed0: Music=%d Sound=%d Voice=%d Save=%d Text=%d Shadows=%d",
			music, sound, voice, save, text, shadows);
	}

	bool events(bool &skip)
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) return false;
			// 0x40bcc0: input may finish a movie; quit remains a distinct event.
			if (ev.type == SDL_KEYDOWN || ev.type == SDL_MOUSEBUTTONDOWN ||
				ev.type == SDL_FINGERDOWN || ev.type == SDL_CONTROLLERBUTTONDOWN) skip = true;
		}
		return true;
	}

	// 0x429b60: Windows filenames are case-insensitive; preserve the player's spelling.
	std::string movie_path(const char *name)
	{
		std::string filename = std::string(name) + ".bik";
		DIR *dir = opendir("movies");
		if (dir) {
			struct dirent *entry;
			while ((entry = readdir(dir)) != NULL)
				if (!SDL_strcasecmp(entry->d_name, filename.c_str())) {
					filename = entry->d_name; break;
				}
			closedir(dir);
		}
		return std::string("movies/") + filename;
	}

	void present(SDL_Window *win, GLuint texture)
	{
		glViewport(0, 0, 640, 480);
		glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, 640, 480, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
		glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND); glColor4f(1, 1, 1, 1);
		if (texture) {
			glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texture);
			glBegin(GL_QUADS);
			glTexCoord2f(0,0); glVertex2f(0,0); glTexCoord2f(1,0); glVertex2f(640,0);
			glTexCoord2f(1,1); glVertex2f(640,480); glTexCoord2f(0,1); glVertex2f(0,480);
			glEnd(); glDisable(GL_TEXTURE_2D);
		}
		omf_SDLFrameHook(win); SDL_GL_SwapWindow(win);
	}

	// 0x429af0 / 0x429b40 / 0x429c80 / 0x429ce0.
	bool play_movie(SDL_Window *win, const char *name)
	{
		SDL_Log("0x40a230: Switch to scene %s", name);
		std::string path = movie_path(name);
		HBINK movie = BinkOpen(path.c_str(), 0);
		if (!movie) { SDL_Log("0x429b60: Could not open Bink file: %s", path.c_str()); return true; }
		SDL_Log("0x429b60: %s: %ux%u, %u frames, %u/%u fps", path.c_str(),
			movie->Width, movie->Height, movie->Frames, movie->FrameRate, movie->FrameRateDiv);
		std::vector<unsigned char> pixels(movie->Width * movie->Height * 4);
		GLuint texture; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		bool running = true, skip = false;
		while ((running = events(skip)) && !skip) {
			if (BinkWait(movie)) { SDL_Delay(1); continue; }
			BinkDoFrame(movie);
			BinkCopyToBuffer(movie, &pixels[0], movie->Width * 4, movie->Height, 0, 0, BINKSURFACE32);
			// port: Bink's 32-bit surface is BGRA; upload portable RGBA for GLES/gl4es.
			for (size_t i = 0; i < pixels.size(); i += 4) {
				unsigned char red = pixels[i]; pixels[i] = pixels[i+2]; pixels[i+2] = red; pixels[i+3] = 255;
			}
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, movie->Width, movie->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
			present(win, texture);
			if (movie->FrameNum == movie->Frames) break;
			BinkNextFrame(movie);
		}
		BinkClose(movie); glDeleteTextures(1, &texture);
		SDL_Log("0x40bcc0: movie %s finished%s", name, skip ? " (input)" : "");
		return running;
	}

	void main_menu(SDL_Window *win)
	{
		// port: stop at the menu handoff until scene/save integration is available.
		// Retail +0x104 is 0x40e160 (slot0 probe), not the menu function.
		SDL_Log("0x41bbb0: main menu path reached (startup handoff)");
		SDL_Log("TODO(0x40d340): connect main menu sb2.omt database objects");
		SDL_Log("TODO(0x41f6a0): connect OMT1 CanvasElement menu/menut canvases, captions and click routing");
		SDL_ShowCursor(SDL_ENABLE);
		bool skip = false;
		while (events(skip)) { present(win, 0); SDL_Delay(16); }
	}

	void run(void)
	{
		SDL_Window *win = SDL_GL_GetCurrentWindow();
		if (!win) return;

		const char *chain[] = {"mlegal", "mthq", "mawe", "mnick", "mspongebob"};
		for (unsigned i = 0; i < sizeof chain / sizeof chain[0]; ++i)
			if (!play_movie(win, chain[i])) return;
		main_menu(win);
	}

};

void omt_main(void)
{
	port_install_crash_log();
	SDL_Log("0x42a540: WinMain -> 0x40e580 game entry; SDL data root is imported install");
	if (getenv("OMT_CRASHTEST")) *(volatile int*)0 = 1;		// port: test hook for the crash backtrace
	try
	{
		StartupApp *app = new StartupApp;
		app->start();
		delete app;
	}
	catch (granny_result result)
	{
		SDL_Log("Startup stopped: Granny result %d", (int)result);
	}
	catch (OMediaError &e)
	{
		SDL_Log("OMT error %d (data %d) at %s:%d %s", (int)e.errorcode, (int)e.errordata,
				e.file.c_str(), e.line, e.error_string.c_str());
	}
}
