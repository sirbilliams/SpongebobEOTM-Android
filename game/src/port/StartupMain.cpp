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
#include "../game/Game.h"
#include "../game/World3D.h"
#include "OMediaGLBackend.h"
#include "../game/SceneScriptHost.h"
#include "../game/SaveGame.h"
#include "../game/Hud.h"
#include "../game/Reveal.h"
#include "Playthrough.h"
#include "Companion.h"
#include "AssetView.h"
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
extern void omf_SDLTestReady(void);

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
    // ---- save games (lane b): the game kept alive behind the menu, and what the next run_game() does ----
    sbgame::Game *paused;
    std::vector<unsigned char> paused_thumb;        // the screen when the game was left for the menu (0x41db50 saves it)
    enum { L_NEW, L_RESUME, L_LOAD } launch;
    int launch_slot;
    bool start_game;          // New Game was chosen: the menu loop ends and the scene runtime starts
    bool skip_movies;         // port: SB_SKIP_MOVIES=1, tests only

	StartupApp() : paused(NULL), launch(L_NEW), launch_slot(-1), start_game(false), skip_movies(getenv("SB_SKIP_MOVIES") && atoi(getenv("SB_SKIP_MOVIES")) != 0), completed(false)
	{
		sbgame::SaveGame::setDir(".");
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
		// 0x40ceb0 InitRasterizer: done by World3D::rasterizer() when a scene starts (the 2D menu needs none).
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
		apply_settings();
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
    virtual bool menu_movie(const char *name) { return skip_movies ? true : play_movie(SDL_GL_GetCurrentWindow(),name); }
    // 0x4248b0 -> 0x41c1c0 -> 0x40a090: New Game. The menu ends, then run_game() starts chapter 1 (intro movie, c1l0s1, its script).
    virtual bool menu_new_game() { launch = L_NEW; start_game = true; return true; }
    // ---- save games (docs/saves.md) ----
    virtual bool menu_game_active() { return paused != NULL; }
    virtual bool menu_can_resume() { return !completed && (paused != NULL || (save >= 0 && sbgame::SaveGame::exists(save))); }
    // 0x4248b0: back to the game in progress; with none, the last used slot (port: the Save value of the settings, game+0x26c)
    virtual void menu_resume() {
        if (completed) return; // lane c: 0x40a090 has removed the resumable scene
        if (paused) launch = L_RESUME;
        else if (save >= 0 && sbgame::SaveGame::exists(save)) { launch = L_LOAD; launch_slot = save; }
        else return;
        start_game = true;
    }
    // 0x416690 for View Movies (0x424d60): the game's progress, else the last used slot's (port: the exe keeps one progress object; here it lives in the paused game or the save)
    virtual bool menu_script_seen(const char *name) {
        std::string n(name); for (size_t i = 0; i < n.size(); ++i) n[i] = (char)tolower((unsigned char)n[i]);
        if (const char *t = getenv("SB_TEST_VM_SEEN")) {     // port: tests only, comma list of scripts seen
            std::string l(",");  l += t; l += ",";
            if (l.find("," + n + ",") != std::string::npos) return true;
        }
        if (paused) return paused->state().scriptSeen(name);
        if (completed) return completed_state.scriptSeen(name); // lane c: exe retains progress (+0x254)
        sbgame::SaveFile f;
        if (save >= 0 && sbgame::SaveGame::exists(save) && sbgame::SaveGame::read(sbgame::SaveGame::slotPath(save), f)) {
            for (size_t i = 0; i < f.scrp.size(); ++i) { std::string s = f.scrp[i]; for (size_t k = 0; k < s.size(); ++k) s[k] = (char)tolower((unsigned char)s[k]); if (s == n) return true; }
        }
        return false;
    }
    // port: voice gallery (View Movies > Voices): the progress behind the menu, like menu_script_seen
    sbgame::GameState vstate;
    virtual const sbgame::GameState* menu_progress() {
        if (paused) return &paused->state();
        if (completed) return &completed_state;
        sbgame::SaveFile f;
        if (save >= 0 && sbgame::SaveGame::exists(save) && sbgame::SaveGame::read(sbgame::SaveGame::slotPath(save), f)) { vstate.clearProgress(); sbgame::SaveGame::apply(f, vstate); return &vstate; }
        return NULL;
    }
    virtual bool menu_save(int slot, const char *name) {     // 0x428740 -> 0x41db50
        if (!paused) return false;
        sbgame::SaveFile f;
        if (paused->hud()) paused->hud()->toBar(paused->state());     // 0x421c40: the item bar goes into the file
        sbgame::SaveGame::capture(paused->state(), paused->state().sceneId, name, paused_thumb, f);
        if (!sbgame::SaveGame::write(sbgame::SaveGame::slotPath(slot), f)) return false;
        save = slot; menu_settings_changed();                // game+0x26c: the slot Load Game and Resume start from
        SDL_Log("0x41db50: saved slot %d scene %08x", slot, f.sceneId);
        return true;
    }
    virtual bool menu_load(int slot) { launch = L_LOAD; launch_slot = slot; start_game = true; return true; }   // 0x429120
    // 0x40c3b0 (the settings apply): the sound manager's channel groups get 6*level+201 (0 for level 0): 3 = Music, 4 = Voice, the others (not 0) = Sound;
    // the Text / Shadows values go to the game object.
    void apply_settings() {
        OMediaSoundManager *m = OMediaSoundManager::getdefaultmanager();
        for (long i = 1; i < m->get_nchannels() && i < 15; ++i) {
            int lv = i == 3 ? music : i == 4 ? voice : sound;
            if (lv < 0) lv = 0; if (lv > 9) lv = 9;
            m->get_channel(i)->set_volume((short)(lv == 0 ? 0 : lv * 6 + 201));
        }
        SDL_Log("0x40c3b0: mixer music=%d (ch3 vol %d) sound=%d (ch1,2,5.. vol %d) voice=%d (ch4 vol %d) text=%d shadows=%d",
            music, music ? music * 6 + 201 : 0, sound, sound ? sound * 6 + 201 : 0, voice, voice ? voice * 6 + 201 : 0, text, shadows);
        if (paused) { paused->optText = text; paused->optShadows = shadows; }
    }
    virtual void menu_preview(int kind) { if (paused) paused->previewSound(kind == 3); }      // 0x4256b0 (only with a game's banks open)
    virtual void menu_settings_changed() {
        apply_settings();
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
		// Scenes and the menu are drawn into the 640x480 screen canvas and scaled to the drawable by omgl_present
		// (engine/sdl/OMediaSDLGfx.cpp); input uses the same widescreen transform via window_to_logical.
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

    // port: tests only. SB_TEST_CLICKS (menu loop) / SB_TEST_GAME_CLICKS (game loop) ="frame:x:y,..." send a click at logical 640x480 coordinates (as SDL window events, through
    // the real event path) at that frame of the running loop.
    void test_clicks(unsigned frame, const char *var) {
        const char *replay=getenv(var);
        SDL_Window *win=SDL_GL_GetCurrentWindow();
        for(const char *p=replay;p && *p && win;) {
            unsigned at; int x,y;
            if(sscanf(p,"%u:%d:%d",&at,&x,&y)==3 && at==frame) {
                int px=x, py=y; omgl_logical_to_window(px,py);
                SDL_WarpMouseInWindow(win,px,py);
                SDL_Event e; SDL_zero(e); e.type=SDL_MOUSEMOTION; e.motion.x=px; e.motion.y=py; SDL_PushEvent(&e);
                e.type=SDL_MOUSEBUTTONDOWN; e.button.button=SDL_BUTTON_LEFT; e.button.x=px; e.button.y=py; SDL_PushEvent(&e);
                e.type=SDL_MOUSEBUTTONUP; SDL_PushEvent(&e);
            }
            p=strchr(p,','); if(p) ++p;
        }
    }

    void main_menu(SDL_Window *win) {
        for(;;) {
            start_game=false;
            if(!menu_loop(win) || !start_game) return;
            if(!run_game(win)) return;
        }
    }

    // false: quit the application
    bool menu_loop(SDL_Window *win) {
        (void)win;
        OMediaFileStream stream;
        OMediaFilePath path("sb2.omt"); stream.setpath(&path); stream.open(omcfp_Read);
        OMediaDataBase database(&stream);
        MainMenu menu(world,&database,font,retail,this);
        click_dispatcher->get_click_broadcaster()->addlistener(&menu);
        SDL_ShowCursor(SDL_ENABLE);
        unsigned frame=0;
        bool quit_app=false;
        while(!menu.quit && !start_game) {
            if(event_manager->quit_requested()) { quit_app=true; break; }
            SDL_Event ev;
            while(OMediaEventManager::poll(&ev)) {
                event_manager->handle_sdl_event(&ev);
                if(ev.type==SDL_MOUSEMOTION) { int x=ev.motion.x,y=ev.motion.y; OMediaEventManager::window_to_logical(x,y); menu.hover(x,y); }
                // port: Escape, F10 and Android Back leave a screen (and the menu over a game); pad B too
                if(ev.type==SDL_KEYDOWN && (ev.key.keysym.sym==SDLK_ESCAPE || ev.key.keysym.sym==SDLK_F10 || ev.key.keysym.sym==SDLK_AC_BACK)) { if(menu.atTop()) { if(paused) menu_resume(); } else menu.back(); }
                else if(ev.type==SDL_CONTROLLERBUTTONDOWN) menu.padButton(ev.cbutton.button);
                else if(ev.type==SDL_KEYDOWN) menu.key(ev.key.keysym.sym);
                else if(ev.type==SDL_TEXTINPUT) menu.text(ev.text.text);
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
            test_clicks(frame,paused?"SB_TEST_PCLICKS":"SB_TEST_CLICKS");      // port: tests only; PCLICKS: the menu over a paused game
            static bool autoStarted=false;      // port: tests only, once per run
            if(getenv("SB_TEST_NEWGAME") && frame==20 && !autoStarted) { autoStarted=true; launch=L_NEW; start_game=true; }
            if(getenv("SB_TEST_RESUME") && frame==20 && !autoStarted) { autoStarted=true; menu_resume(); }
            if(getenv("SB_TEST_LOADSLOT") && frame==20 && !autoStarted) { autoStarted=true; menu_load(atoi(getenv("SB_TEST_LOADSLOT"))); }   // port: tests only
            if(const char *k=getenv("SB_TEST_MENUKEYS")) {      // port: tests only, "frame:key,...": a key pressed in the menu (SDL key name; a single character is typed)
                for(const char *p=k;p && *p;) {
                    unsigned at=(unsigned)atoi(p); const char *c=strchr(p,':'); const char *e=strchr(p,','); std::string n=c?std::string(c+1,e?e:p+strlen(p)):std::string();
                    if(c && at==frame && !n.empty()) {
                        if(n.size()==1) menu.text(n.c_str());
                        else { SDL_Keycode kc=SDL_GetKeyFromName(n.c_str()); if(kc!=SDLK_UNKNOWN) menu.key(kc); }
                    }
                    p=e?e+1:NULL;
                }
            }
#endif
            world->update(); ++frame; SDL_Delay(16);
        }
        click_dispatcher->get_click_broadcaster()->removelistener(&menu);
        SDL_Log("main menu: clean exit");
        return !quit_app && !(menu.quit);
    }

    // The scene runtime (game/src/game): chapter 1 starts with the intro movie, then scene c1l0s1 and its script (sload1).
    static void game_movie(const char *name, void *data) {
        StartupApp *app=(StartupApp*)data;
        char buf[64]; size_t n=0; while(name[n] && name[n]!=' ' && n<sizeof buf-1) { buf[n]=name[n]; ++n; } buf[n]=0;
        sbport::CompanionMovie(true);    // port: companion (its SKIP is a key press while the movie holds the loop)
        if(!app->skip_movies) app->movie_ok=app->play_movie(SDL_GL_GetCurrentWindow(),buf) && app->movie_ok;
        sbport::CompanionMovie(false);
        app->finished_movie=buf;
        app->movie_done=true;      // the classifier continues with game->chained after the movie (game+0x186)
    }
    bool movie_ok, movie_done;
    bool completed; // lane c: ended games cannot Resume, even with a last save slot
    sbgame::GameState completed_state;
    std::string finished_movie;

    // port: tests only. SB_TEST_DUMP_PAUSE / SB_TEST_DUMP_QUIT=<file>: the game state as a save file (no thumbnail), for comparing runs
    void test_dump(sbgame::Game *game,const char *var) {
        const char *path=getenv(var);
        if(!path || !game) return;
        sbgame::SaveFile f; std::vector<unsigned char> none;
        if(game->hud()) game->hud()->toBar(game->state());
        sbgame::SaveGame::capture(game->state(),game->state().sceneId,"dump",none,f);
        bool ok=sbgame::SaveGame::write(path,f);
        SDL_Log("test dump %s: scene %08x %s",var,f.sceneId,ok?"written":"FAILED");
    }

    // port: tests only (SB_TEST_PLAYTHROUGH, game/src/port/Playthrough.h): a logical click through the SDL event path, a save
    static void pt_click(int x, int y) {
        SDL_Window *win=SDL_GL_GetCurrentWindow();
        if(!win) return;
        int px=x, py=y; omgl_logical_to_window(px,py);
        SDL_WarpMouseInWindow(win,px,py);
        SDL_Event e; SDL_zero(e); e.type=SDL_MOUSEMOTION; e.motion.x=px; e.motion.y=py; SDL_PushEvent(&e);
        e.type=SDL_MOUSEBUTTONDOWN; e.button.button=SDL_BUTTON_LEFT; e.button.x=px; e.button.y=py; SDL_PushEvent(&e);
        e.type=SDL_MOUSEBUTTONUP; SDL_PushEvent(&e);
    }
    // lane e: a pan scene puts the pointer in the middle of the screen (0x437cf0)
    static void game_warp(int x, int y) {
        SDL_Window *win=SDL_GL_GetCurrentWindow();
        if(!win) return;
        int px=x, py=y; omgl_logical_to_window(px,py);
        SDL_WarpMouseInWindow(win,px,py);
    }
    static void pt_move(int x, int y) {
        SDL_Window *win=SDL_GL_GetCurrentWindow();
        if(!win) return;
        int px=x, py=y; omgl_logical_to_window(px,py);
        SDL_Event e; SDL_zero(e); e.type=SDL_MOUSEMOTION; e.motion.x=px; e.motion.y=py; SDL_PushEvent(&e);
    }
    static StartupApp *pt_app;
    static void pt_save(sbgame::Game *game, int slot) {
        std::vector<unsigned char> thumb;
        sbgame::SaveGame::grabThumb(pt_app->screen->getdefaultdrawgfxworld(),thumb);
        sbgame::SaveFile f;
        if(game->hud()) game->hud()->toBar(game->state());
        sbgame::SaveGame::capture(game->state(),game->state().sceneId,"Playthrough",thumb,f);
        bool ok=sbgame::SaveGame::write(sbgame::SaveGame::slotPath(slot),f);
        SDL_Log("playthrough: saved slot %d scene %08x %s",slot,f.sceneId,ok?"written":"FAILED");
    }

    // false: quit the application; true: back to the menu
    bool run_game(SDL_Window *win) {
        (void)win;
        sbgame::Game *game=NULL;
        bool result=true, keep=false;
        try {
            if(launch==L_RESUME && paused) {            // 0x4248b0: the menu closes and the game in progress goes on
                game=paused; paused=NULL; game->suspended=false; game->pauseMusic(false); apply_settings();   // port: music paused behind the menu (0x40c780)
                game->optText=text; game->optShadows=shadows;
                game->forceRedraw();
            } else {
                delete paused; paused=NULL;
                completed=false;
                game=new sbgame::Game;
                if(!game->init(".",screen)) { SDL_Log("game: init failed"); delete game; return true; }
                apply_settings(); game->optText=text; game->optShadows=shadows;
                game->attachInput();
                movie_ok=true; movie_done=false;
                game->movieHook=game_movie; game->movieHookData=this;
                game->warpHook=game_warp;
                game->logFile=getenv("SB_GAME_LOG")?stdout:NULL;
                if(game->logFile) setvbuf(stdout,NULL,_IOLBF,0);      // port: tests read the log of a run that is cut off
                SDL_ShowCursor(SDL_ENABLE);
                const char *first=getenv("SB_TEST_SCENE");
                std::string scene;
                if(launch==L_LOAD) {                    // 0x417460: the state of the file replaces the game's, then its scene starts
                    sbgame::SaveFile f;
                    if(!sbgame::SaveGame::read(sbgame::SaveGame::slotPath(launch_slot),f)) {
                        SDL_Log("0x417460: slot %d cannot be loaded",launch_slot); delete game; return true;
                    }
                    scene=sbgame::SaveGame::apply(f,game->state());
                    if(game->hud()) game->hud()->fromBar(game->state());      // the item bar as it was saved
                    save=launch_slot; menu_settings_changed();
                    SDL_Log("0x417460: loaded slot %d \"%s\" -> %s",launch_slot,f.name,scene.c_str());
                } else scene=first?first:"mintro c1l0s1";
                if(getenv("SB_TEST_SEED")) {            // port: tests only: some progress to save
                    sbgame::GameState &st=game->state();
                    st.markScriptSeen("sload1"); st.markScriptSeen("sc1l2s6a"); st.giveItem("ikc1"); st.giveItem("ihat");
                    st.storeDialogue(0x20201,"G001","H0021",3,7); st.storeDialogue(0x60201,"G010","H1002",1,0);
                    st.setTexture("sandy","sandy2"); st.setBlocked(0x60201,12,34,true); st.setFlag("c1x2y3",true); st.lastScene=0x20201;
                }
                game->switchTo(scene.c_str());           // 0x40a230: movie, then the scene (game+0x186)
            }
            SDL_ShowCursor(SDL_ENABLE);
            unsigned frame=0, last=0, readyFrame=0;
            bool testReady=false;
            sbport::Playthrough *pt=NULL;
#ifndef __ANDROID__
            if(getenv("SB_TEST_PLAYTHROUGH")) { pt=new sbport::Playthrough(getenv("SB_TEST_PLAYTHROUGH")); pt_app=this; }
#endif
            for(;;) {
                if(event_manager->quit_requested()) { result=false; break; }
                SDL_Event ev; bool leave=false;
                if(pt) {
                    bool failed=false;
                    if(!pt->tick(game,pt_click,pt_move,pt_save,&failed)) { SDL_Log("playthrough %s", failed?"FAILED":"done"); delete pt; pt=NULL; result=false; break; }
                }
                // port: replay against the displayed transform before this frame can pan.
                if(!getenv("SB_TEST_READY_FRAMES") || testReady)
                    test_clicks(getenv("SB_TEST_READY_FRAMES") ? readyFrame++ : frame,"SB_TEST_GAME_CLICKS");
                if(const char *ts=getenv("SB_TEST_SCRIPT")) {    // port: tests only, "frame:name": game->switchTo(name) (a script, comment or scene) at that game frame
                    // "frame:name[@actor][,frame:name...]"
                    for(const char *q=ts; q && *q; ) {
                        unsigned at=0; int n=0;
                        if(sscanf(q,"%u:%n",&at,&n)!=1) break;
                        const char *e=strchr(q+n,','); std::string nm(q+n, e ? (size_t)(e-(q+n)) : strlen(q+n));
                        if(at==frame) {                  // "name@actor": that character is the active one first (as for an idle script)
                            size_t at2=nm.find('@');
                            if(at2!=std::string::npos) { std::string who=nm.substr(at2+1); nm.resize(at2);
                                if(game->host()) game->host()->setActiveActor(game->host()->findActor(who.c_str())); }
                            game->switchTo(nm.c_str());
                        }
                        q = e ? e+1 : 0;
                    }
                }
                while(OMediaEventManager::poll(&ev)) {
                    event_manager->handle_sdl_event(&ev);
                    sbgame::revealEvent(game,ev);      // port: hotspot reveal (L1 / Tab held)
                    if(ev.type==SDL_MOUSEMOTION) { int x=ev.motion.x,y=ev.motion.y; OMediaEventManager::window_to_logical(x,y); game->mouseMove(x,y); }
                    if(ev.type==SDL_KEYDOWN && (ev.key.keysym.sym==SDLK_F10 || ev.key.keysym.sym==SDLK_AC_BACK)) leave=true;   // port: back to the menu
                }
                const char *tf=getenv("SB_TEST_F10"), *tq=getenv("SB_TEST_GAMEQUIT");      // port: tests only
                static bool f10Done=false;
                if(tf && (unsigned)atoi(tf)==frame && !f10Done) { f10Done=true; leave=true; }
                if(tq && (unsigned)atoi(tq)==frame) { test_dump(game,"SB_TEST_DUMP_QUIT"); result=false; break; }
                if(leave) {                              // the game stays alive behind the menu (Save Game, Resume Game)
                    sbgame::SaveGame::grabThumb(screen->getdefaultdrawgfxworld(),paused_thumb);
                    test_dump(game,"SB_TEST_DUMP_PAUSE");
                    game->suspended=true; game->pauseMusic(true); paused=game; keep=true; break;   // port: 0x40c730
                }
                if(movie_done) {
                    movie_done=false;
                    if(!movie_ok) { result=false; break; }
                    std::string finished=finished_movie;
                    if(game->movieFinished(finished.c_str())) {
                        completed_state=game->state(); completed=true;
                        SDL_Log("0x40a090: ending complete -> main menu (Resume disabled, last slot %d retained)", save);
                        break;
                    }
                }
                unsigned now=SDL_GetTicks();
                float dt=last?(float)(now-last):16.0f; last=now;
                if(dt>66.0f) dt=66.0f;       // a slow frame must not skip a script
                game->update(dt);
                // port: no OMediaPeriodical::devote_time(): it updates the worlds a second time per frame (a black frame every few frames)
                float top=-1,bottom=-1;
                if(game->scene() && game->world3d()) {
                    std::vector<sbgame::SceneObject*>& objects=game->scene()->objects();
                    for(size_t i=0;i<objects.size();++i) if(objects[i]->userCtl) {
                        int l,t,r,b;
                        if(game->world3d()->screenBounds(objects[i],l,t,r,b)) { top=t; bottom=b; break; }
                    }
                }
                omgl_widescreen_scene(true,top,bottom);
                game->world()->update();
                if(!testReady && game->scene() && game->scene()->barState()==0) {
                    testReady=true; omf_SDLTestReady();
                    if(getenv("SB_TEST_READY_FRAMES")) SDL_Log("test: scene %s ready", game->scene()->name().c_str());
                }
                ++frame; SDL_Delay(10);
            }
        } catch(OMediaError &e) {
            SDL_Log("game: OMT error %d at %s:%d",(int)e.errorcode,e.file.c_str(),e.line);
        }
        omgl_widescreen_scene(false);
        if(!keep) delete game;
        return result;
    }

	void run(void)
	{
		SDL_Window *win = SDL_GL_GetCurrentWindow();
		if (!win) return;

		if (const char *av = getenv("SB_ASSET_VIEW"))		// port: the Asset Viewer (docs/asset-viewer.md): no movies, no menu, no saves; leaving ends the app
			if (*av) { sbport::AssetViewRun(av, screen, event_manager, font); return; }

		const char *chain[] = {"mlegal", "mthq", "mawe", "mnick", "mspongebob"};
		for (unsigned i = 0; i < sizeof chain / sizeof chain[0] && !skip_movies; ++i)
			if (!play_movie(win, chain[i])) return;
		main_menu(win);
	}

};
StartupApp *StartupApp::pt_app = NULL;

void omt_main(void)
{
	port_install_crash_log();
	SDL_Log("0x42a540: WinMain -> 0x40e580 game entry; SDL data root is imported install");
	if (getenv("OMT_CRASHTEST")) *(volatile int*)0 = 1;		// port: test hook for the crash backtrace
	if (getenv("SB_TEST_SAVEFMT")) { int rc = sbgame::SaveGame::selfTest(); exit(rc); }		// port: tests only (docs/saves.md)
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
