#define OMD_SDL_NO_GL_WRAP
/*  OMT SDL platform layer - video engine, engine factory, windows, OpenGL context, offscreen buffers.
    Port of OS/MSWindows (GDI/WGL path) to SDL2.  LGPL. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "OMediaSDLRetarget.h"
#include "OMediaEngineFactory.h"
#include "OMediaSingleWindow.h"
#include "OMediaFullscreenWindow.h"
#include "OMediaSoundEngine.h"
#include "OMediaInputEngine.h"
#include "OMediaRenderer.h"
#include "OMediaDefMessages.h"
#include "OMediaError.h"
#include "OMediaGLRenderer.h"
#include "OMediaOMTRenderer.h"

OMediaSoundEngine *omf_SDLCreateSoundEngine(OMediaWindow *master);
OMediaInputEngine *omf_SDLCreateInputEngine(OMediaWindow *master);
void omf_SDLPadInit(void);

static void omf_SDLInit(void)
{
	static bool done = false;
	if (done) return;
	done = true;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0)
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	omf_SDLPadInit();
	// OMT's GL renderer is fixed-function GL 1.x. With gl4es (Android, or OMT_GL4ES builds on
	// desktop) it is translated onto an OpenGL ES 2.0 context that SDL creates.
#ifdef OMT_USE_GL4ES
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
}

// ---------------------------------------------------------------------------
// Engine factory

OMediaEngineFactory *OMediaEngineFactory::factory = NULL;

OMediaEngineFactory::OMediaEngineFactory()
{
	omf_SDLInit();
	video_engines.push_back(ommeic_OS);
	sound_engines.push_back(ommeic_OS);
	input_engines.push_back(ommeic_OS);
}

OMediaEngineFactory::~OMediaEngineFactory() {}

OMediaEngineFactory *OMediaEngineFactory::get_factory(void)
{
	if (!factory) factory = new OMediaEngineFactory;
	return factory;
}

omt_VideoEngineAttr OMediaEngineFactory::get_video_engine_attr(omt_EngineID) { return 0; }
omt_SoundEngineAttr OMediaEngineFactory::get_sound_engine_attr(omt_EngineID)
{ return omseaf_NoChannelLimitation | omseaf_Volume; }
omt_InputEngineAttr OMediaEngineFactory::get_input_engine_attr(omt_EngineID) { return omieaf_Controller; }

OMediaVideoEngine *OMediaEngineFactory::create_video_engine(omt_EngineID, OMediaWindow *master)
{ return new OMediaSDLVideoEngine(master); }
OMediaSoundEngine *OMediaEngineFactory::create_sound_engine(omt_EngineID, OMediaWindow *master)
{ return omf_SDLCreateSoundEngine(master); }
OMediaInputEngine *OMediaEngineFactory::create_input_engine(omt_EngineID, OMediaWindow *master)
{ return omf_SDLCreateInputEngine(master); }

// ---------------------------------------------------------------------------
// Video engine: one "card" = the SDL display, one mode = logical game resolution.

int OMediaSDLVideoEngine::logical_width = 640;
int OMediaSDLVideoEngine::logical_height = 480;

void OMediaSDLVideoEngine::set_widescreen(int height)
{
	omf_SDLInit();
	SDL_DisplayMode dm;
	logical_height = height;
	int dw = 0, dh = 0;
	const char *ws = getenv("OMT_WINDOW_SIZE");          // desktop testing: the window's aspect
	if (!(ws && sscanf(ws, "%dx%d", &dw, &dh) == 2 && dw > 0 && dh > 0))
	{
		dw = dh = 0;
		if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { dw = dm.w; dh = dm.h; }
	}
	if (dh > 0 && dw > 0)
	{
		// Landscape game: use long side / short side (Android may report portrait at start-up).
		// Rounded up to an even width, so the picture fills the panel (at most half a pixel of
		// letterbox instead of a pillarbox).
		float lo = float(dw < dh ? dw : dh), hi = float(dw > dh ? dw : dh);
		logical_width = ((int)ceilf(height * hi / lo - 0.01f) + 1) & ~1;
		if (logical_width < height * 4 / 3) logical_width = height * 4 / 3;
	}
}

static OMediaSDLVideoEngine *s_video_engine = NULL;		// port: for omf_SDLContextReset

OMediaSDLVideoEngine::OMediaSDLVideoEngine(OMediaWindow *master_window) :
	OMediaVideoEngine(ommeic_OS, master_window)
{
	s_video_engine = this;
	omf_SDLInit();
	state = 0;
	current_video_mode = default_video_mode = NULL;
	linked_card = NULL;
	create_video_cards();
}

OMediaSDLVideoEngine::~OMediaSDLVideoEngine() { unlink(); if (s_video_engine == this) s_video_engine = NULL; }

// port: the GL context was lost while the app was in the background (SDL_RENDER_DEVICE_RESET,
// Android): every texture / display list the targets made is gone, so their implementations
// are dropped; the canvases and shapes upload themselves again when next drawn.
void omf_SDLContextReset(void)
{
	OMediaRenderer *r = s_video_engine ? s_video_engine->get_selected_renderer() : NULL;
	if (!r) return;
	omt_RenderTargetList *t = r->get_targets();
	for (omt_RenderTargetList::iterator i = t->begin(); i != t->end(); ++i) (*i)->delete_all_implementations();
	SDL_Log("port: GL context reset - textures will be uploaded again");
}

void OMediaSDLVideoEngine::create_video_cards(void)
{
	OMediaVideoCard empty;
	video_cards.push_back(empty);
	OMediaVideoCard *card = &video_cards.back();
	card->name = "SDL Display";
	card->flags = omcvdf_MainMonitor | omcvdf_3D;
	card->positionx = card->positiony = 0;
	card->private_id = NULL;

	OMediaVideoMode mode;
	mode.its_card = card;
	mode.flags = omcvmf_Null;
	mode.width = logical_width;
	mode.height = logical_height;
	mode.depth = 32;
	mode.refresh_rate = 0;
	mode.rgbmask[0] = 0xFF0000; mode.rgbmask[1] = 0x00FF00; mode.rgbmask[2] = 0x0000FF;
	mode.private_id[0] = mode.private_id[1] = NULL;
	card->modes.push_back(mode);
}

OMediaVideoMode *OMediaSDLVideoEngine::get_current_video_mode(void)
{
	if (!(state & omvesc_Linked)) omd_EXCEPTION(omcerr_NoVideoCardLinked);
	return current_video_mode;
}

void OMediaSDLVideoEngine::link(OMediaVideoCard *vcard)
{
	if (state & omvesc_Linked) { if (linked_card == vcard) return; unlink(); }
	linked_card = vcard;
	state |= omvesc_Linked;
	current_video_mode = default_video_mode = &linked_card->modes.front();
	update_renderer_defs();
	broadcast_message(omsg_VideoEngineLinked, this);
}

void OMediaSDLVideoEngine::unlink(void)
{
	if (!(state & omvesc_Linked)) return;
	broadcast_message(omsg_VideoEngineUnlinked, this);
	delete_offscreen_buffers();
	current_video_mode = default_video_mode = NULL;
	linked_card = NULL;
	state = 0;
	deselect_renderer();
	update_renderer_defs();
}

void OMediaSDLVideoEngine::get_bounds(OMediaRect &rect) const
{
	if ((state & omvesc_Linked) && current_video_mode)
	{
		rect.left = current_video_mode->its_card->positionx;
		rect.top = current_video_mode->its_card->positiony;
		rect.right = rect.left + current_video_mode->width;
		rect.bottom = rect.top + current_video_mode->height;
	}
	else rect.set(0, 0, 0, 0);
}

void OMediaSDLVideoEngine::set_mode(OMediaVideoMode *vmode)
{
	if (!(state & omvesc_Linked)) omd_EXCEPTION(omcerr_NoVideoCardLinked);
	if (current_video_mode == vmode) return;
	deselect_renderer();
	if (!vmode) vmode = default_video_mode;
	broadcast_message(omsg_ScreenModeWillChange, this);
	current_video_mode = vmode;
	if (current_video_mode != default_video_mode) state |= omvesc_VideoModeSet;
	else state &= ~omvesc_VideoModeSet;
	update_renderer_defs();
	broadcast_message(omsg_ScreenModeChanged, this);
}

void OMediaSDLVideoEngine::update_renderer_defs(void)
{
	renderer_list.erase(renderer_list.begin(), renderer_list.end());
	if (!linked_card) return;
	OMediaRendererDef def;

	// OpenGL first so "best" selection picks it.
	def.private_id = (void*)1;
	def.engine_id = ommeic_OpenGL;
	def.attributes = omcrdattr_Blending | omcrdattr_TextureFiltering | omcrdattr_Gouraud |
					 omcrdattr_TextureColor | omcrdattr_MipMapping | omcrdattr_Fog |
					 omcrdattr_Texture | omcrdattr_Accelerated;
	def.zbuffer_depth = omfzbc_16Bits | omfzbc_24Bits;
	def.pixel_format = ompixfc_RGB888 | ompixfc_ARGB8888;
	def.name = "OpenGL";
	renderer_list.push_back(def);

	def.private_id = (void*)0;
	def.engine_id = ommeic_OMT;
	def.name = "Open Media Toolkit";
	def.attributes = omcrdattr_Blending | omcrdattr_Texture | omcrdattr_TextureColor |
					 omcrdattr_Fog | omcrdattr_Gouraud;
	def.zbuffer_depth = omfzbc_16Bits;
	def.pixel_format = ompixfc_ARGB8888 | ompixfc_RGB888 | ompixfc_RGB555 | ompixfc_ARGB1555;
	renderer_list.push_back(def);
}

void OMediaSDLVideoEngine::listen_to_message(omt_Message msg, void *)
{
	if (msg == omsg_RendererDeleted) renderer = NULL;
}

void OMediaSDLVideoEngine::select_renderer(OMediaRendererDef *def, omt_ZBufferBitDepthFlags zbuffer)
{
	deselect_renderer();
	switch (def->engine_id)
	{
		case ommeic_OpenGL: renderer = new OMediaGLRenderer(this, def, zbuffer); break;
		case ommeic_OMT:    renderer = new OMediaOMTRenderer(this, def, zbuffer); break;
		default: break;
	}
	broadcast_message(omsg_RendererSelected, this);
}

void OMediaSDLVideoEngine::deselect_renderer(void)
{
	delete renderer;
	renderer = NULL;
	broadcast_message(omsg_RendererSelected, this);
}

OMediaOffscreenBuffer *OMediaSDLVideoEngine::create_offscreen_buffer(int width, int height,
																	 omt_OffscreenBufferPixelFormat pf)
{
	return new OMediaSDLOffscreenBuffer(this, width, height, pf);
}

// ---------------------------------------------------------------------------
// Offscreen buffer (used by the software renderer)

OMediaSDLOffscreenBuffer::OMediaSDLOffscreenBuffer(OMediaVideoEngine *engine, int w, int h,
												   omt_OffscreenBufferPixelFormat pf) :
	OMediaOffscreenBuffer(engine, w, h, pf)
{
	lock_count = 0;
	if (pf == omobfc_ARGB1555)
		surface = SDL_CreateRGBSurface(0, w, h, 16, 0x7C00, 0x03E0, 0x001F, 0);
	else
		surface = SDL_CreateRGBSurface(0, w, h, 32, 0xFF0000, 0x00FF00, 0x0000FF, 0);
	if (!surface) omd_EXCEPTION(omcerr_OutOfMemory);
}

OMediaSDLOffscreenBuffer::~OMediaSDLOffscreenBuffer() { if (surface) SDL_FreeSurface(surface); }
void OMediaSDLOffscreenBuffer::lock(void) { if (lock_count++ == 0) SDL_LockSurface(surface); }
void OMediaSDLOffscreenBuffer::unlock(void) { if (--lock_count == 0) SDL_UnlockSurface(surface); }

void OMediaSDLOffscreenBuffer::get_pixmap(void *&pixels, int &rowbytes)
{
	if (lock_count) { pixels = surface->pixels; rowbytes = surface->pitch; }
	else { pixels = NULL; rowbytes = 0; }
}

void OMediaSDLOffscreenBuffer::draw(OMediaWindow *window, int x, int y)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)window->get_retarget();
	if (!rtg || !rtg->sdlwin) return;
	SDL_Surface *ws = SDL_GetWindowSurface(rtg->sdlwin);
	if (!ws) return;
	SDL_Rect r = { x, y, surface->w, surface->h };
	SDL_BlitSurface(surface, NULL, ws, &r);
	SDL_UpdateWindowSurface(rtg->sdlwin);
}

// ---------------------------------------------------------------------------
// Windows

OMediaWindow *OMediaSDLRtgWindow::find(Uint32 id)
{
	SDL_Window *w = SDL_GetWindowFromID(id);
	return w ? (OMediaWindow*)SDL_GetWindowData(w, "omt") : NULL;
}

static SDL_Window *omf_SDLCreateWindow(OMediaWindow *owner, const string &title, int w, int h, Uint32 extra)
{
	omf_SDLInit();
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | extra;
	SDL_Window *win = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
	if (!win) omd_EXCEPTION(omcerr_CantBuild);
	SDL_SetWindowData(win, "omt", owner);
	return win;
}

static void omf_SDLReadBounds(SDL_Window *win, short &x, short &y, short &w, short &h)
{
	int ix, iy, iw, ih;
	SDL_GetWindowPosition(win, &ix, &iy);
	SDL_GL_GetDrawableSize(win, &iw, &ih);
	x = ix; y = iy; w = iw; h = ih;
}

// * Single window

OMediaSingleWindow::OMediaSingleWindow(OMediaSupervisor *supervisor, bool initially_visible, bool dont_initialize) :
	OMediaWindow(supervisor)
{
	OMediaSDLRtgWindow *rtg = new OMediaSDLRtgWindow;
	rtg->window = this;
	retarget = rtg;
	min_width = 100; min_height = 80;
	max_width = 30000; max_height = 30000;
	visible = initially_visible;
	if (!dont_initialize) init_window(initially_visible);
}

OMediaSingleWindow::~OMediaSingleWindow() { destroy_dependencies(); cleanup_window(); }

void OMediaSingleWindow::close(void)
{
	OMediaWindow::close();
	if (window_list.size() == 1) listen_to_message(omsg_Quit);
}

omt_WindowStyle OMediaSingleWindow::get_style(void) { return omwstyle_Single; }

void OMediaSingleWindow::place(short nx, short ny)
{
	SDL_SetWindowPosition(((OMediaSDLRtgWindow*)retarget)->sdlwin, nx, ny);
	bounds_changed();
}

void OMediaSingleWindow::set_size(short w, short h)
{
	SDL_SetWindowSize(((OMediaSDLRtgWindow*)retarget)->sdlwin, w, h);
	bounds_changed();
}

void OMediaSingleWindow::bounds_changed(void)
{
	omf_SDLReadBounds(((OMediaSDLRtgWindow*)retarget)->sdlwin, x, y, width, height);
	OMediaWindow::bounds_changed();
}

void OMediaSingleWindow::set_max_size(short w, short h) { max_width = w; max_height = h; }
void OMediaSingleWindow::set_min_size(short w, short h) { min_width = w; min_height = h; }
void OMediaSingleWindow::get_max_size(short &w, short &h) { w = max_width; h = max_height; }
void OMediaSingleWindow::get_min_size(short &w, short &h) { w = min_width; h = min_height; }

void OMediaSingleWindow::set_title(string str)
{
	title = str;
	SDL_SetWindowTitle(((OMediaSDLRtgWindow*)retarget)->sdlwin, str.c_str());
}

void OMediaSingleWindow::init_window(bool initially_visible)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)retarget;
	rtg->sdlwin = omf_SDLCreateWindow(this, title, 640, 480, SDL_WINDOW_RESIZABLE);
	OMediaSingleWindow::bounds_changed();
	set_backcolor(back_color);
	if (initially_visible) show(); else visible = false;
}

void OMediaSingleWindow::cleanup_window(void)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)retarget;
	if (rtg && rtg->sdlwin) { SDL_DestroyWindow(rtg->sdlwin); rtg->sdlwin = NULL; }
}

void OMediaSingleWindow::activate(void) { SDL_RaiseWindow(((OMediaSDLRtgWindow*)retarget)->sdlwin); }
void OMediaSingleWindow::activated(void) { OMediaWindow::activated(); }

omt_Bool OMediaSingleWindow::is_active(void)
{
	return (SDL_GetWindowFlags(((OMediaSDLRtgWindow*)retarget)->sdlwin) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

void OMediaSingleWindow::show(void) { SDL_ShowWindow(((OMediaSDLRtgWindow*)retarget)->sdlwin); visible = true; }
void OMediaSingleWindow::hide(void) { SDL_HideWindow(((OMediaSDLRtgWindow*)retarget)->sdlwin); visible = false; }
void OMediaSingleWindow::set_menu(OMediaMenu *) {}

bool OMediaSingleWindow::update_message_state(omt_Message msg, bool &enabled, bool &mark)
{
	if (msg == omsg_Close) { enabled = true; return true; }
	return OMediaSupervisor::update_message_state(msg, enabled, mark);
}

void OMediaSingleWindow::set_backcolor(const OMediaRGBColor &rgb) { OMediaWindow::set_backcolor(rgb); }

// * Fullscreen window: the game's 640x480 screen. On desktop it is a window
//   (OMT_FULLSCREEN=1 for real fullscreen); on Android SDL makes it fullscreen anyway.

OMediaFullscreenWindow::OMediaFullscreenWindow(OMediaSupervisor *supervisor, bool initially_visible) :
	OMediaWindow(supervisor)
{
	OMediaSDLRtgWindow *rtg = new OMediaSDLRtgWindow;
	rtg->window = this;
	retarget = rtg;
	visible = false;
	asleep = true;
	init_window();
	update_video_engine_connection();
	if (initially_visible) show();
}

OMediaFullscreenWindow::~OMediaFullscreenWindow() { destroy_dependencies(); cleanup_window(); }

omt_WindowStyle OMediaFullscreenWindow::get_style(void) { return omwstyle_Fullscreen; }

void OMediaFullscreenWindow::bounds_changed(void)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)retarget;
	// The game expects its window to be the logical screen size; the GL viewport is
	// scaled up to the real drawable size at flip time.
	int ix, iy; SDL_GetWindowPosition(rtg->sdlwin, &ix, &iy);
	x = ix; y = iy;
	width = OMediaSDLVideoEngine::logical_width;
	height = OMediaSDLVideoEngine::logical_height;
	OMediaWindow::bounds_changed();
}

void OMediaFullscreenWindow::set_title(string str)
{
	title = str;
	SDL_SetWindowTitle(((OMediaSDLRtgWindow*)retarget)->sdlwin, str.c_str());
}

void OMediaFullscreenWindow::init_window(void)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)retarget;
	const char *fs = getenv("OMT_FULLSCREEN");
	Uint32 extra = (fs && fs[0] == '1') ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE;
#ifdef __ANDROID__
	extra = SDL_WINDOW_FULLSCREEN;
#endif
	int iw = OMediaSDLVideoEngine::logical_width * 2, ih = OMediaSDLVideoEngine::logical_height * 2;
	const char *ws = getenv("OMT_WINDOW_SIZE");          // e.g. "1920x1080" (testing / desktop)
	if (ws) sscanf(ws, "%dx%d", &iw, &ih);
	rtg->sdlwin = omf_SDLCreateWindow(this, title, iw, ih, extra);
	OMediaFullscreenWindow::bounds_changed();
	set_backcolor(back_color);
}

void OMediaFullscreenWindow::cleanup_window(void)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)retarget;
	if (rtg && rtg->sdlwin) { SDL_DestroyWindow(rtg->sdlwin); rtg->sdlwin = NULL; }
}

void OMediaFullscreenWindow::activate(void) { SDL_RaiseWindow(((OMediaSDLRtgWindow*)retarget)->sdlwin); }
void OMediaFullscreenWindow::activated(void) { OMediaWindow::activated(); }

omt_Bool OMediaFullscreenWindow::is_active(void)
{
	return (SDL_GetWindowFlags(((OMediaSDLRtgWindow*)retarget)->sdlwin) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

void OMediaFullscreenWindow::show(void)
{
	if (!asleep) SDL_ShowWindow(((OMediaSDLRtgWindow*)retarget)->sdlwin);
	visible = true;
}

void OMediaFullscreenWindow::hide(void)
{
	if (!asleep) SDL_HideWindow(((OMediaSDLRtgWindow*)retarget)->sdlwin);
	visible = false;
}

void OMediaFullscreenWindow::listen_to_message(omt_Message msg, void *param)
{
	switch (msg)
	{
		case omsg_VideoEngineLinked:   exit_sleep_mode(); break;
		case omsg_VideoEngineUnlinked: enter_sleep_mode(); break;
		case omsg_ScreenModeChanged:   update_window_posiz(); break;
	}
	OMediaWindow::listen_to_message(msg, param);
}

void OMediaFullscreenWindow::link_video_engine(OMediaVideoEngine *engine)
{
	if (win2engines.size()) return;
	OMediaWindow::link_video_engine(engine);
	update_video_engine_connection();
}

void OMediaFullscreenWindow::unlink_video_engine(OMediaVideoEngine *engine)
{
	OMediaWindow::unlink_video_engine(engine);
	update_video_engine_connection();
}

void OMediaFullscreenWindow::set_backcolor(const OMediaRGBColor &rgb) { OMediaWindow::set_backcolor(rgb); }

void OMediaFullscreenWindow::update_video_engine_connection(void)
{
	if (win2engines.size() && ((*win2engines.begin()).engine->get_state() & omvesc_Linked))
	{
		asleep = true;
		exit_sleep_mode();
		return;
	}
	asleep = false;
	enter_sleep_mode();
}

void OMediaFullscreenWindow::enter_sleep_mode(void)
{
	if (!asleep) { asleep = true; SDL_HideWindow(((OMediaSDLRtgWindow*)retarget)->sdlwin); }
}

void OMediaFullscreenWindow::exit_sleep_mode(void)
{
	if (asleep) { asleep = false; update_window_posiz(); }
}

void OMediaFullscreenWindow::update_window_posiz(void)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)retarget;
	if (!win2engines.size()) return;
	OMediaVideoEngine *eng = (*win2engines.begin()).engine;
	if (!(eng->get_state() & omvesc_Linked)) return;
	if (visible) SDL_ShowWindow(rtg->sdlwin); else SDL_HideWindow(rtg->sdlwin);
	bounds_changed();
}

// ---------------------------------------------------------------------------
// OpenGL renderer retargets

class OMediaSDLGLRtgRenderer : public OMediaRetarget
{
	public:
	OMediaSDLGLRtgRenderer() : OMediaRetarget(omcrtg_GLRenderer) {}
};

class OMediaSDLGLRtgRenderTarget : public OMediaRetarget
{
	public:
	OMediaSDLGLRtgRenderTarget() : OMediaRetarget(omcrtg_GLRenderTarget) { context = NULL; window = NULL; }
	virtual ~OMediaSDLGLRtgRenderTarget()
	{
		if (context)
		{
			if (current_context == context) current_context = NULL;
			SDL_GL_DeleteContext(context);
		}
	}
	SDL_GLContext	context;
	OMediaWindow	*window;
	static SDL_GLContext current_context;
};

SDL_GLContext OMediaSDLGLRtgRenderTarget::current_context = NULL;

void OMediaGLRenderer::init_retarget(OMediaVideoMode *, OMediaRendererDef *)
{
	retarget = new OMediaSDLGLRtgRenderer;
}

void OMediaGLRenderTarget::init_retarget(void)
{
	retarget = new OMediaSDLGLRtgRenderTarget;
}

#ifdef OMT_USE_GL4ES
// ---------------------------------------------------------------------------
// gl4es bootstrap: GLES entry points come from the system GLES library (dlsym) with SDL's
// eglGetProcAddress as fallback; the main framebuffer size comes from the current SDL window.
#include <dlfcn.h>
extern "C" {
	void set_getprocaddress(void *(*new_proc_address)(const char *));
	void set_getmainfbsize(void (*new_getMainFBSize)(int *width, int *height));
	void initialize_gl4es(void);
}

static void *omg_gles_lib = NULL;

static void *omf_gl4es_getproc(const char *name)
{
	void *p = omg_gles_lib ? dlsym(omg_gles_lib, name) : NULL;
	if (!p) p = SDL_GL_GetProcAddress(name);
	return p;
}

static void omf_gl4es_fbsize(int *w, int *h)
{
	SDL_Window *win = SDL_GL_GetCurrentWindow();
	if (win) SDL_GL_GetDrawableSize(win, w, h); else { *w = 640; *h = 480; }
}

void omf_SDLStartGL4ES(void)
{
	static bool started = false;
	if (started) return;
	started = true;
	const char *libs[] = { "libGLESv2.so", "libGLESv2.so.2", NULL };
	for (int i = 0; libs[i] && !omg_gles_lib; i++) omg_gles_lib = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
	set_getprocaddress(omf_gl4es_getproc);
	set_getmainfbsize(omf_gl4es_fbsize);
	initialize_gl4es();
	SDL_Log("OMT: gl4es started on %s", (const char*)glGetString(GL_RENDERER));
}
#endif

static SDL_Window *omf_SDLWindowOf(OMediaWindow *w)
{
	OMediaSDLRtgWindow *r = w ? (OMediaSDLRtgWindow*)w->get_retarget() : NULL;
	return r ? r->sdlwin : NULL;
}

void OMediaGLRenderTarget::prepare_context(OMediaWindow *awindow)
{
	if (!awindow) return;
	OMediaSDLGLRtgRenderTarget *rtg = (OMediaSDLGLRtgRenderTarget*)retarget;
	SDL_Window *sw = omf_SDLWindowOf(awindow);
	if (!rtg->context)
	{
		window = awindow;
		rtg->window = awindow;
		rtg->context = SDL_GL_CreateContext(sw);
		if (!rtg->context)
		{
			window = NULL;
			SDL_Log("OMT: SDL_GL_CreateContext failed: %s", SDL_GetError());
			omd_EXCEPTION(omcerr_GLCantCreateContext);
		}
		SDL_GL_SetSwapInterval(1);
		rtg->current_context = NULL;
#ifdef OMT_USE_GL4ES
		SDL_GL_MakeCurrent(sw, rtg->context);
		omf_SDLStartGL4ES();
#endif
	}
	if (rtg->current_context != rtg->context)
	{
		rtg->current_context = rtg->context;
		SDL_GL_MakeCurrent(sw, rtg->context);
	}
}

void OMediaGLRenderTarget::set_context(void)
{
	OMediaSDLGLRtgRenderTarget *rtg = (OMediaSDLGLRtgRenderTarget*)retarget;
	if (rtg->context && rtg->current_context != rtg->context)
	{
		rtg->current_context = rtg->context;
		SDL_GL_MakeCurrent(omf_SDLWindowOf(rtg->window), rtg->context);
	}
}

void omf_SDLFrameHook(SDL_Window *win);
void omf_SDLClearBars(SDL_Window *w);

void OMediaGLRenderTarget::flip_buffers(void)
{
	OMediaSDLGLRtgRenderTarget *rtg = (OMediaSDLGLRtgRenderTarget*)retarget;
	if (!rtg->context) return;
	omf_SDLFrameHook(omf_SDLWindowOf(rtg->window));
	SDL_GL_SwapWindow(omf_SDLWindowOf(rtg->window));
	omf_SDLClearBars(omf_SDLWindowOf(rtg->window));
}

// ---------------------------------------------------------------------------
// Logical -> drawable mapping. OMT computes viewports in window coordinates; the fullscreen
// window is logically 640x480 (the game's resolution) but the drawable can be any size and
// aspect (HiDPI desktops, the Thor's 16:9 panel). Uniform scale + centring = letterbox/pillarbox.

static float omg_scale = 1.0f, omg_offx = 0.0f, omg_offy = 0.0f;
static int omg_dw = 0, omg_dh = 0;

static void omf_SDLComputeMapping(SDL_Window *w)
{
	if (!w) return;
	OMediaWindow *ow = (OMediaWindow*)SDL_GetWindowData(w, "omt");
	SDL_GL_GetDrawableSize(w, &omg_dw, &omg_dh);
	if (!ow || ow->get_width() <= 0 || ow->get_height() <= 0) { omg_scale = 1; omg_offx = omg_offy = 0; return; }
	float sx = float(omg_dw) / ow->get_width(), sy = float(omg_dh) / ow->get_height();
	omg_scale = sx < sy ? sx : sy;
	omg_offx = (omg_dw - ow->get_width() * omg_scale) * 0.5f;
	omg_offy = (omg_dh - ow->get_height() * omg_scale) * 0.5f;
}

// Logical OMT window coordinates -> window (SDL) coordinates, the centre of that logical pixel.
void omf_SDLLogicalToWindow(OMediaWindow *ow, int lx, int ly, int &px, int &py)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)ow->get_retarget();
	px = lx; py = ly;
	if (!rtg || !rtg->sdlwin) return;
	int ww, wh; SDL_GetWindowSize(rtg->sdlwin, &ww, &wh);
	omf_SDLComputeMapping(rtg->sdlwin);
	if (omg_dw <= 0 || omg_dh <= 0 || omg_scale <= 0) return;
	float dx = omg_offx + (lx + 0.5f) * omg_scale, dy = omg_offy + (ly + 0.5f) * omg_scale;	// drawable pixels
	px = int(dx * ww / omg_dw);
	py = int(dy * wh / omg_dh);
}

// Window (SDL event) coordinates -> logical OMT window coordinates.
void omf_SDLWindowToLogical(OMediaWindow *ow, int px, int py, int &lx, int &ly)
{
	OMediaSDLRtgWindow *rtg = (OMediaSDLRtgWindow*)ow->get_retarget();
	lx = px; ly = py;
	if (!rtg || !rtg->sdlwin) return;
	int ww, wh; SDL_GetWindowSize(rtg->sdlwin, &ww, &wh);
	omf_SDLComputeMapping(rtg->sdlwin);
	if (ww <= 0 || wh <= 0 || omg_scale <= 0) return;
	float dx = px * float(omg_dw) / ww, dy = py * float(omg_dh) / wh;    // to drawable pixels
	lx = int((dx - omg_offx) / omg_scale);
	ly = int((dy - omg_offy) / omg_scale);
}

void omf_SDLglViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
	omf_SDLComputeMapping(SDL_GL_GetCurrentWindow());
	if (getenv("OMT_DEBUG_VIEWPORT"))
	{
		static Uint32 t = 0;
		if (SDL_GetTicks() - t > 500)
		{
			t = SDL_GetTicks();
			OMediaWindow *ow = (OMediaWindow*)SDL_GetWindowData(SDL_GL_GetCurrentWindow(), "omt");
			int ww, wh; SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &ww, &wh);
			SDL_Log("viewport in %d,%d %dx%d  logical %dx%d  window %dx%d  drawable %dx%d  scale %.3f off %.1f,%.1f",
					x, y, w, h, ow ? ow->get_width() : -1, ow ? ow->get_height() : -1, ww, wh, omg_dw, omg_dh, omg_scale, omg_offx, omg_offy);
		}
	}
	glViewport(GLint(omg_offx + x * omg_scale), GLint(omg_offy + y * omg_scale),
			   GLsizei(w * omg_scale + 0.5f), GLsizei(h * omg_scale + 0.5f));
}

void omf_SDLglScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
	omf_SDLComputeMapping(SDL_GL_GetCurrentWindow());
	glScissor(GLint(omg_offx + x * omg_scale), GLint(omg_offy + y * omg_scale),
			  GLsizei(w * omg_scale + 0.5f), GLsizei(h * omg_scale + 0.5f));
}

// After a swap the back buffer is undefined: clear it whole so the bars stay black.
void omf_SDLClearBars(SDL_Window *w)
{
	omf_SDLComputeMapping(w);
	if (omg_offx < 0.5f && omg_offy < 0.5f) return;
	glDisable(GL_SCISSOR_TEST);
	glViewport(0, 0, omg_dw, omg_dh);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}
