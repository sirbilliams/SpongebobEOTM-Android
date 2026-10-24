// port: placeholder until the startup reconstruction (sb2.exe WinMain 0x... and the movie chain / main
// menu) replaces it. Opens the engine's fullscreen window and shows a plain text screen, so the
// Android "Play" path (SetupActivity -> SpongeActivity -> libspongebob.so -> SDL_main -> omt_main)
// can be tested end to end: installed files, window, GL, log, crash backtrace.
// Exits on SDL_QUIT / Escape / Back, or by OMT_SCREENSHOT / OMT_FRAMES (OMediaSDLMain.cpp's hook).

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

extern void omf_SDLFrameHook(SDL_Window *win);		// OMediaSDLMain.cpp: frame count, OMT_SCREENSHOT

// 5x7 glyphs, one row per entry (bit 4 = left), for the letters the message uses
struct Glyph { char c; unsigned char rows[7]; };
static const Glyph glyphs[] = {
	{ 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } }, { 'a', { 0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F } },
	{ 'm', { 0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15 } }, { 'e', { 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E } },
	{ 'i', { 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E } }, { 'n', { 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11 } },
	{ 's', { 0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E } }, { 't', { 0x08, 0x08, 0x1C, 0x08, 0x08, 0x09, 0x06 } },
	{ 'l', { 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, { 'd', { 0x01, 0x01, 0x0D, 0x13, 0x11, 0x13, 0x0D } },
	{ '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } }, { 'w', { 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A } },
	{ 'o', { 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E } }, { 'r', { 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10 } },
	{ 'k', { 0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12 } }, { 'p', { 0x00, 0x00, 0x1E, 0x11, 0x1E, 0x10, 0x10 } },
	{ 'g', { 0x00, 0x00, 0x0F, 0x11, 0x0F, 0x01, 0x0E } },
};

static void draw_text(const char *s, float x, float y, float px)
{
	glBegin(GL_QUADS);
	for (; *s; s++, x += 6 * px)
		for (size_t g = 0; g < sizeof glyphs / sizeof glyphs[0]; g++)
			if (glyphs[g].c == *s)
				for (int row = 0; row < 7; row++)
					for (int col = 0; col < 5; col++)
						if (glyphs[g].rows[row] & (0x10 >> col))
						{
							float x0 = x + col * px, y0 = y + row * px;
							glVertex2f(x0, y0); glVertex2f(x0 + px, y0); glVertex2f(x0 + px, y0 + px); glVertex2f(x0, y0 + px);
						}
	glEnd();
}

class PlaceholderApp : public OMediaApplication, public OMediaPeriodical
{
	public:
	OMediaMonitorMap	*monitors;
	OMediaWindow		*window;
	OMediaWorld			*world;
	OMediaViewPort		*viewport;
	OMediaLayer			*layer;

	PlaceholderApp()
	{
		// Same path as the game's own startup: a fullscreen window on the first video engine/card.
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

		// An empty world with a clear-colour layer: its first render makes the GL context (and starts
		// gl4es on Android); spend_time() then takes over the window.
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

	virtual ~PlaceholderApp()
	{
		delete world;
		delete monitors;
	}

	virtual void spend_time(void)
	{
		if (!SDL_GL_GetCurrentWindow()) return;		// the engine has not rendered yet
		run();
		do_quit = true;
	}

	void run(void)
	{
		SDL_Window *win = SDL_GL_GetCurrentWindow();
		if (!win) { SDL_Log("placeholder: no GL window"); return; }
		struct stat st;
		bool installed = stat("sb2.exe", &st) == 0;
		const char *msg = installed ? "Game installed - work in progress" : "Game files missing";
		bool quit = false;
		while (!quit)
		{
			SDL_Event ev;
			while (SDL_PollEvent(&ev))
				if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_AC_BACK))
					|| (ev.type == SDL_CONTROLLERBUTTONDOWN && ev.cbutton.button == SDL_CONTROLLER_BUTTON_B))
					quit = true;
			// port: the engine's header redirects glViewport (logical 640x480 window -> letterboxed
			// drawable), so this screen is drawn in the game's logical 640x480 space
			const int w = 640, h = 480;
			glViewport(0, 0, w, h);
			glClearColor(0.05f, 0.08f, 0.16f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, w, h, 0, -1, 1);
			glMatrixMode(GL_MODELVIEW); glLoadIdentity();
			glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_CULL_FACE);
			float px = (float)w / 220.0f;			// the message is 33 glyphs of 6 pixels
			float tw = (float)strlen(msg) * 6 * px;
			glColor4f(1.0f, 0.85f, 0.25f, 1.0f);
			draw_text(msg, ((float)w - tw) / 2, ((float)h - 7 * px) / 2, px);
			omf_SDLFrameHook(win);					// frame count, OMT_SCREENSHOT / OMT_FRAMES
			SDL_GL_SwapWindow(win);
			SDL_Delay(16);
		}
	}
};

void omt_main(void)
{
	port_install_crash_log();
	if (getenv("OMT_CRASHTEST")) *(volatile int*)0 = 1;		// port: test hook for the crash backtrace
	try
	{
		PlaceholderApp *app = new PlaceholderApp;
		app->start();
		delete app;
	}
	catch (OMediaError &e)
	{
		SDL_Log("OMT error %d (data %d) at %s:%d %s", (int)e.errorcode, (int)e.errordata,
				e.file.c_str(), e.line, e.error_string.c_str());
	}
}
