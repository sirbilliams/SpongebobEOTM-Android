// Startup slice reconstructed from sb2.exe. See docs/startup.md for engine gaps.
#include "OMediaDisplay.h"
#include "OMediaDXSurfaceWorld.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include "OMediaEventManager.h"
#include "OMediaWorldClickDispatcher.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaError.h"
#include "OMediaSDLRetarget.h"
#include "../ui/MainMenu.h"
#include "../scene/SceneData.h"
#include <stdexcept>
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
#include "OMediaSound.h"
#include "../../granny/granny_compat.h"

extern void omf_SDLFrameHook(SDL_Window *win);		// OMediaSDLMain.cpp: frame count, OMT_SCREENSHOT

class StartupApp : public MenuHost
{
	public:
    OMediaDoubleCanvas *screen;
    OMediaDisplay *display;
    OMedia2DWorld *world;
    OMediaViewPort *viewport;
    OMediaEventManager *event_manager;
    OMediaWorldClickDispatcher *click_dispatcher;
    OMediaFileStream font_stream;
    OMediaDataBase *font_database;
    GameFont *font;
    RetailData retail;

	StartupApp()
	{
		SDL_Log("0x4097d0: application constructor");
		// register_object and cache=0 match OMT.dll; the shared implementation checks
		// !cache_size before purging just as 0x10054d30 does.
		OMediaDataBase::register_object(0x43616e76, OMediaCanvas::db_builder, 0);
		OMediaDataBase::register_object(0x57617665, OMediaSound::db_builder, 0);
		OMediaDataBase::set_cache_size(0);
		SDL_ShowCursor(SDL_DISABLE);
		init_display();
		OMediaSoundManager::setdefaultmanager(new OMediaDXSoundManager);
		init_world();
		init_menu_controller();
		event_manager = new OMediaEventManager;
		click_dispatcher = new OMediaWorldClickDispatcher;
		click_dispatcher->get_pick_broadcaster()->addlistener(world);
		init_font();
		load_config();
		SDL_Log("0x40e2c0: SetVideoMode(640,480,16,%d)", software);
		SDL_Log("TODO(0x40ceb0): select the 3D scene rasterizer when a scene starts");
		init_granny();
		init_movie_sound();
	}

    // 0x40cc10 / 0x40e2c0: port: OMT1 Display maps DirectDraw onto SDL/GL.
    void init_display() {
        OMediaDirectX::install(OMediaDirectX::omcdx_Video);
        OMediaScreenDevice *dev=OMediaScreenDevice::getdefaultdevice();
        OMediaVideoMode *mode=NULL;
        OMediaVideoDriver &driver=dev->get_video_drivers()->front();
        for(size_t i=0;i<driver.modes.size();++i)
            if(driver.modes[i].width==640 && driver.modes[i].height==480 && driver.modes[i].depth==16)
                mode=&driver.modes[i];
        dev->setmode(mode,0);
        screen=new OMediaDoubleCanvas;
        display=new OMediaDisplay(NULL,screen,"SpongeBob EOTM");
    }
    void init_world() { // 0x40d010 / 0x40d1d0
        world=new OMedia2DWorld;
        world->set_framerate_mode(omfrc_Max);
        viewport=new OMediaViewPort(world);
        OMediaRect bounds(0,0,640,480);
        viewport->setbounds(&bounds); viewport->setcanvas(screen);
    }
    void init_menu_controller() { SDL_Log("0x40d120: menu controller initialized (lazy sb2.omt)"); }
    void init_font() { // 0x40d0b0 -> 0x414820
        OMediaFilePath path("Font.omt");
        font_stream.setpath(&path); font_stream.open(omcfp_Read);
        font_database=new OMediaDataBase(&font_stream);
        font=new GameFont(font_database,retail);
        SDL_Log("0x414820: Font.omt glyph font initialized");
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

    virtual ~StartupApp() {
        delete click_dispatcher;
        delete event_manager;
        delete font;
        delete world;
        delete font_database;
        delete display;
        OMediaSoundManager::setdefaultmanager(NULL);
        GrannyClose(granny_version);
        OMediaScreenDevice::setdefaultdevice(NULL);
    }
    virtual bool menu_movie(const char *name) { return play_movie(SDL_GL_GetCurrentWindow(),name); }
    virtual bool menu_new_game() {
        if(!menu_movie("mintro")) return false;
        sb::scene::Scene scene; std::string error;
        if(sb::scene::load("scenes/c1l0s1.scn",scene,error))
            SDL_Log("0x437210: c1l0s1 parsed (%lu bytes, %lu objects)",
                (unsigned long)scene.bytesConsumed,(unsigned long)scene.elements.size());
        else SDL_Log("0x437210: %s",error.c_str());
        SDL_Log("TODO(0x40a230): instantiate c1l0s1 3D scene and script VM");
        return true;
    }
    virtual void menu_settings_changed() {
        // port: persist 0x409ed0's registry settings under SDL's per-user directory.
        char *pref=SDL_GetPrefPath("THQ","SpongeBob 2");
        std::string path=getenv("OMT_CONFIG")?getenv("OMT_CONFIG"):std::string(pref?pref:"")+"settings.cfg";
        SDL_free(pref);
        FILE *f=fopen(path.c_str(),"w");
        if(f) { fprintf(f,"Music=%d\nSound=%d\nVoice=%d\nSave=%d\nText=%d\nShadows=%d\n",music,sound,voice,save,text,shadows); fclose(f); }
    }

	bool software;

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
		// port: the movie is 640x480; scale it to the real drawable (1920x1080 on the Thor), 4:3 centred
		// with black bars. A fixed 640x480 viewport drew it in the bottom-left corner.
		int dw = 640, dh = 480;
		SDL_GL_GetDrawableSize(win, &dw, &dh);
		glDisable(GL_SCISSOR_TEST);
		glViewport(0, 0, dw, dh);
		glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
		float k = (float)dw / 640.0f < (float)dh / 480.0f ? (float)dw / 640.0f : (float)dh / 480.0f;
		int vw = (int)(640 * k + 0.5f), vh = (int)(480 * k + 0.5f);
		glViewport((dw - vw) / 2, (dh - vh) / 2, vw, vh);
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

    void main_menu(SDL_Window *win) {
        (void)win;
        OMediaFileStream stream;
        OMediaFilePath path("sb2.omt"); stream.setpath(&path); stream.open(omcfp_Read);
        OMediaDataBase database(&stream);
        MainMenu menu(world,&database,font,retail,this);
        click_dispatcher->get_click_broadcaster()->addlistener(&menu);
        SDL_ShowCursor(SDL_ENABLE);
        unsigned frame=0;
        while(!menu.quit && !event_manager->quit_requested()) {
            SDL_Event ev;
            while(SDL_PollEvent(&ev)) {
                event_manager->handle_sdl_event(&ev);
                if(ev.type==SDL_MOUSEMOTION) menu.hover(ev.motion.x,ev.motion.y);
                if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_ESCAPE) menu.back();
            }
#ifndef __ANDROID__
            // port: deterministic desktop input replay, routed through the real SDL/OMT event path.
            const char *replay=getenv("OMT_MENU_CLICKS");
            for(const char *p=replay;p && *p;) {
                unsigned at; int x,y;
                if(sscanf(p,"%u:%d:%d",&at,&x,&y)==3 && at==frame) {
                    SDL_Event e; SDL_zero(e); e.type=SDL_MOUSEMOTION; e.motion.x=x; e.motion.y=y;
                    SDL_PushEvent(&e);
                    e.type=SDL_MOUSEBUTTONDOWN; e.button.button=SDL_BUTTON_LEFT; e.button.x=x; e.button.y=y; SDL_PushEvent(&e);
                    e.type=SDL_MOUSEBUTTONUP; SDL_PushEvent(&e);
                }
                p=strchr(p,','); if(p) ++p;
            }
#endif
            world->update(); ++frame; SDL_Delay(16);
        }
        click_dispatcher->get_click_broadcaster()->removelistener(&menu);
        SDL_Log("main menu: clean exit");
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
		app->run();
		delete app;
	}
    catch (std::exception &e) { SDL_Log("Startup stopped: %s",e.what()); }
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
