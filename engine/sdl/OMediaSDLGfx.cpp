#define OMD_SDL_NO_GL_WRAP
/*  port: OpenGL back end of the OMT 1.x engine layer (engine/omt1). LGPL.
    Replaces OMT.dll's DirectDraw/Direct3D surfaces with GL framebuffers in one SDL window
    (see omt1/Graphics/OMediaGLBackend.h). Fixed-function GL; gl4es on Android.               */

#include "OMediaSysDefs.h"
#include "OMediaGLBackend.h"
#include "OMediaGfxWorld.h"
#include "SbHd.h"
#include <map>
#include <string>
#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <math.h>

extern void omf_SDLFrameHook(SDL_Window *win);		// OMediaSDLMain.cpp: frame count, OMT_SCREENSHOT

static SDL_Window		*omg_win = NULL;
static SDL_GLContext	omg_ctx = NULL;
static int				omg_lw = 640, omg_lh = 480;		// logical screen size
static OMediaGLSurface	*omg_bound = NULL;

#ifdef OMT_USE_GL4ES
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
	if (omg_win) SDL_GL_GetDrawableSize(omg_win, w, h); else { *w = 640; *h = 480; }
}
static void omf_start_gl4es(void)
{
	static bool started = false;
	if (started) return;
	started = true;
	const char *libs[] = { "libGLESv2.so", "libGLESv2.so.2", NULL };
	for (int i = 0; libs[i] && !omg_gles_lib; i++) omg_gles_lib = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
	// port: keep textures at 8 bits per channel (gl4es's PowerVR default would be 16-bit); set before initialize_gl4es reads them
	setenv("LIBGL_AVOID16BITS", "1", 0);
	setenv("LIBGL_FORCE16BITS", "0", 0);
	set_getprocaddress(omf_gl4es_getproc);
	set_getmainfbsize(omf_gl4es_fbsize);
	initialize_gl4es();
}
#endif

bool omgl_window_open(void) { return omg_win != NULL; }

bool omgl_open_window(const char *title, int width, int height)
{
	omg_lw = width;
	omg_lh = height;
	if (omg_win)
	{
		SDL_SetWindowTitle(omg_win, title);
		return true;
	}
	if (!SDL_WasInit(SDL_INIT_VIDEO))
		if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { SDL_Log("OMT: SDL video: %s", SDL_GetError()); return false; }
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

	int ww = width, wh = height;
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __ANDROID__
	flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
	const char *ws = getenv("OMT_WINDOW_SIZE");		// desktop testing, e.g. "1280x960"
	if (ws) sscanf(ws, "%dx%d", &ww, &wh);
#endif
#ifdef __ANDROID__
	// port: SDL's Android default (no colour sizes set) is an RGB565 surface, which shows as dither / banding. Ask for
	// 8/8/8/8 first; if no such EGL config exists, retry once with SDL's defaults.
	for (int pass = 0; pass < 2; pass++)
	{
		if (pass == 0)
		{
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);	// no window alpha: an 8888 surface can composite see-through on Android
		}
		else
		{
			SDL_Log("OMT: no 8888 surface (%s), using SDL's default", SDL_GetError());
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
		}
		omg_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, flags);
		if (omg_win) omg_ctx = SDL_GL_CreateContext(omg_win);
		if (omg_win && omg_ctx) break;
		if (omg_win) { SDL_DestroyWindow(omg_win); omg_win = NULL; }
	}
	if (!omg_win || !omg_ctx) { SDL_Log("OMT: window / GL context: %s", SDL_GetError()); omg_win = NULL; omg_ctx = NULL; return false; }
#else
	omg_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, flags);
	if (!omg_win) { SDL_Log("OMT: SDL_CreateWindow: %s", SDL_GetError()); return false; }
	omg_ctx = SDL_GL_CreateContext(omg_win);
	if (!omg_ctx) { SDL_Log("OMT: SDL_GL_CreateContext: %s", SDL_GetError()); return false; }
#endif
	SDL_GL_MakeCurrent(omg_win, omg_ctx);
#ifdef OMT_USE_GL4ES
	omf_start_gl4es();
#endif
	glDisable(GL_DITHER);		// port: GL's default is on; with a 16-bit surface it was the checkerboard, and it does nothing useful on 8888
	{
		int r = 0, g = 0, b = 0, a = 0;
		SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r); SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
		SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b); SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
		SDL_Log("OMT: framebuffer %d/%d/%d/%d", r, g, b, a);
	}
	SDL_GL_SetSwapInterval(1);
	SDL_Log("OMT: window %dx%d, logical %dx%d, GL %s", ww, wh, width, height, (const char*)glGetString(GL_RENDERER));
	return true;
}

void omgl_close_window(void)
{
	sbhd::log_summary();
	if (omg_ctx) SDL_GL_DeleteContext(omg_ctx);
	if (omg_win) SDL_DestroyWindow(omg_win);
	omg_ctx = NULL;
	omg_win = NULL;
	omg_bound = NULL;
}

void omgl_pump_events(void) { SDL_PumpEvents(); }

// --------------------------------------------------------------------------- surfaces

void omgl_surface_create(OMediaGLSurface *s, int width, int height, bool primary)
{
	memset(s, 0, sizeof(*s));
	s->width = width;
	s->height = height;
	s->primary = primary;
	s->scale = 1;
	if (!omg_win || width <= 0 || height <= 0) return;
	if (primary)		// port: HD pack: the screen surface is rendered at a multiple of the logical size
	{
		GLint mt = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mt);
		s->scale = sbhd::scale((int)mt);
		if (s->scale > 1) SDL_Log("HD: screen surface %dx%d (x%d)", width * s->scale, height * s->scale, s->scale);
	}
	const int pw = width * s->scale, ph = height * s->scale;
	glGenTextures(1, &s->tex);
	glBindTexture(GL_TEXTURE_2D, s->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, s->scale > 1 ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, s->scale > 1 ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glGenFramebuffers(1, &s->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s->tex, 0);
	glGenRenderbuffers(1, &s->depth);
	glBindRenderbuffer(GL_RENDERBUFFER, s->depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, pw, ph);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s->depth);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) SDL_Log("OMT: framebuffer %dx%d incomplete", pw, ph);
	glViewport(0, 0, pw, ph);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	omg_bound = NULL;
}

void omgl_surface_destroy(OMediaGLSurface *s)
{
	if (omg_win)
	{
		if (s->fbo) glDeleteFramebuffers(1, &s->fbo);
		if (s->depth) glDeleteRenderbuffers(1, &s->depth);
		if (s->tex) glDeleteTextures(1, &s->tex);
	}
	if (omg_bound == s) omg_bound = NULL;
	memset(s, 0, sizeof(*s));
}

// 2D state: y down, one unit per surface pixel. The framebuffer's texture is stored bottom-up.
void omgl_surface_bind(OMediaGLSurface *s)
{
	if (!s->fbo) return;
	glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
	glViewport(0, 0, s->width * s->scale, s->height * s->scale);		// the projection below stays in logical pixels
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, s->width, 0, s->height, -1, 1);		// bottom-up in the texture
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, (float)s->height, 0);
	glScalef(1, -1, 1);								// so y = 0 is the top row
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_SCISSOR_TEST);
	glColor4f(1, 1, 1, 1);
	omg_bound = s;
}

void omgl_surface_read(OMediaGLSurface *s, unsigned int *argb)
{
	if (!s->fbo) return;
	const int k = s->scale, pw = s->width * k, ph = s->height * k;
	std::vector<unsigned char> tmp(pw * ph * 4);
	glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, &tmp[0]);
	for (int y = 0; y < s->height; y++)
	{
		const unsigned char *row = &tmp[(ph - 1 - (y * k + k / 2)) * pw * 4];	// bottom-up; port: HD: the centre sample of the k x k block
		for (int x = 0; x < s->width; x++)
		{
			const unsigned char *src = row + (x * k + k / 2) * 4;
			*argb++ = ((unsigned int)src[3] << 24) | (src[0] << 16) | (src[1] << 8) | src[2];
		}
	}
	omg_bound = NULL;
}

void omgl_surface_write(OMediaGLSurface *s, const unsigned int *argb)
{
	if (!s->tex) return;
	const int k = s->scale, pw = s->width * k, ph = s->height * k;
	std::vector<unsigned char> tmp(pw * ph * 4);
	for (int y = 0; y < s->height; y++)
	{
		for (int x = 0; x < s->width; x++)
		{
			const unsigned int p = *argb++;
			for (int j = 0; j < k; j++)		// port: HD: each logical pixel fills a k x k block
				for (int i = 0; i < k; i++)
				{
					unsigned char *d = &tmp[((ph - 1 - (y * k + j)) * pw + x * k + i) * 4];
					d[0] = (unsigned char)(p >> 16); d[1] = (unsigned char)(p >> 8); d[2] = (unsigned char)p; d[3] = 0xFF;
				}
		}
	}
	glBindTexture(GL_TEXTURE_2D, s->tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, &tmp[0]);
}

// --------------------------------------------------------------------------- textures of memory worlds

// --------------------------------------------------------------------------- HD pack pictures (port)

// A memory world with a pack picture: the picture's texture is drawn in place of the world's own.
struct OmfHd
{
	std::string		key;
	unsigned int	tex;			// 0: not loaded (failed or dropped)
	unsigned int	version;		// the world's version when the picture was attached: a change means the game drew into it
	double			bytes;
	OmfHd() : tex(0), version(0), bytes(0) {}
};
static std::map<OMediaGfxPixels *, OmfHd> omf_hd;

static void omf_hd_drop(std::map<OMediaGfxPixels *, OmfHd>::iterator it)
{
	if (it->second.tex && omg_win) glDeleteTextures(1, &it->second.tex);
	if (it->second.bytes > 0) sbhd::budget_sub(it->second.bytes);
	omf_hd.erase(it);
}

void omgl_hd_attach(OMediaGfxWorld *w, const char *key)
{
	if (!w || !key || !omg_win || !sbhd::enabled()) return;
	OMediaGfxPixels *p = w->port_pixels();
	if (!p || !p->pix || p->width <= 0 || p->height <= 0) return;
	std::map<OMediaGfxPixels *, OmfHd>::iterator old = omf_hd.find(p);
	if (old != omf_hd.end())
	{
		if (old->second.key == key && old->second.tex && old->second.version == p->version) return;		// already there
		omf_hd_drop(old);
	}
	if (!sbhd::has(key)) return;
	std::vector<unsigned char> rgba;
	int hw = 0, hh = 0;
	if (!sbhd::load(key, p->width, p->height, rgba, hw, hh)) return;
	GLint mt = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mt);
	if (hw > mt || hh > mt) { SDL_Log("HD: %s is %dx%d, over GL_MAX_TEXTURE_SIZE %d - using the original", key, hw, hh, (int)mt); return; }
	const double bytes = (double)hw * hh * 4.0;
	if (!sbhd::budget_add(key, bytes)) return;
	OmfHd e;
	e.key = key;
	e.version = p->version;
	e.bytes = bytes;
	glGenTextures(1, &e.tex);
	glBindTexture(GL_TEXTURE_2D, e.tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, hw, hh, 0, GL_RGBA, GL_UNSIGNED_BYTE, &rgba[0]);
	omf_hd[p] = e;
}

// The pack texture for a world, or 0 (none, or the game drew into the world since: the picture is dropped).
static unsigned int omf_hd_texture(OMediaGfxPixels *p)
{
	if (omf_hd.empty()) return 0;
	std::map<OMediaGfxPixels *, OmfHd>::iterator it = omf_hd.find(p);
	if (it == omf_hd.end()) return 0;
	if (it->second.version != p->version) { omf_hd_drop(it); return 0; }
	return it->second.tex;
}

static void omf_upload(OMediaGfxPixels *p)
{
	if (p->gl_tex && p->gl_version == p->version && p->gl_w == (unsigned int)p->width && p->gl_h == (unsigned int)p->height) return;
	if (!p->gl_tex)
	{
		glGenTextures(1, &p->gl_tex);
		glBindTexture(GL_TEXTURE_2D, p->gl_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else glBindTexture(GL_TEXTURE_2D, p->gl_tex);
	std::vector<unsigned char> tmp(p->width * p->height * 4);
	const unsigned int *s = p->pix;
	for (size_t i = 0; i < tmp.size(); i += 4, s++)
	{
		tmp[i] = (unsigned char)(*s >> 16); tmp[i + 1] = (unsigned char)(*s >> 8); tmp[i + 2] = (unsigned char)*s; tmp[i + 3] = (unsigned char)(*s >> 24);
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p->width, p->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &tmp[0]);
	p->gl_version = p->version;
	p->gl_w = p->width;
	p->gl_h = p->height;
}

void omgl_forget(OMediaGfxPixels *p)
{
	if (!omf_hd.empty()) { std::map<OMediaGfxPixels *, OmfHd>::iterator it = omf_hd.find(p); if (it != omf_hd.end()) omf_hd_drop(it); }
	if (p->gl_tex && omg_win) glDeleteTextures(1, &p->gl_tex);
	if (p->gl_3d_tex && omg_win) glDeleteTextures(1, &p->gl_3d_tex);
	p->gl_tex = p->gl_3d_tex = 0;
}

static void omf_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1)
{
	glBegin(GL_QUADS);
	glTexCoord2f(u0, v0); glVertex2f(x0, y0);
	glTexCoord2f(u1, v0); glVertex2f(x1, y0);
	glTexCoord2f(u1, v1); glVertex2f(x1, y1);
	glTexCoord2f(u0, v1); glVertex2f(x0, y1);
	glEnd();
}

void omgl_blit(OMediaGfxWorld *src, OMediaGfxWorld *dest, const OMediaRect &sr, long dx, long dy, long dw, long dh, bool keyed)
{
	OMediaGLSurface *d = dest->port_surface();
	if (!d || !d->fbo || dw <= 0 || dh <= 0) return;
	OMediaGfxPixels *p = src->port_pixels();
	OMediaGLSurface *ss = src->port_surface();
	float tw, th, v0, v1;
	unsigned int hdtex = 0;
	if (p)
	{
		if (!p->pix || p->width <= 0 || p->height <= 0) return;
		hdtex = omf_hd_texture(p);
		if (!hdtex) omf_upload(p);
		tw = (float)p->width; th = (float)p->height;
		v0 = sr.top / th; v1 = sr.bottom / th;			// memory textures are top-down
	}
	else if (ss && ss->tex)
	{
		if (ss == d) return;		// port: a surface onto itself (DirectDraw Blt) is not used by the game
		glBindTexture(GL_TEXTURE_2D, ss->tex);
		tw = (float)ss->width; th = (float)ss->height;
		v0 = 1.0f - sr.top / th; v1 = 1.0f - sr.bottom / th;	// framebuffer textures are bottom-up
	}
	else return;
	omgl_surface_bind(d);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, hdtex ? hdtex : (p ? p->gl_tex : ss->tex));
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	if (keyed)
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
	}
	omf_quad((float)dx, (float)dy, (float)(dx + dw), (float)(dy + dh), sr.left / tw, v0, sr.right / tw, v1);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
}

void omgl_fill(OMediaGfxWorld *dest, const OMediaRect &r, unsigned int argb)
{
	OMediaGLSurface *d = dest->port_surface();
	if (!d || !d->fbo) return;
	omgl_surface_bind(d);
	glDisable(GL_TEXTURE_2D);
	glColor4ub((GLubyte)(argb >> 16), (GLubyte)(argb >> 8), (GLubyte)argb, 0xFF);
	glBegin(GL_QUADS);
	glVertex2f((float)r.left, (float)r.top); glVertex2f((float)r.right, (float)r.top);
	glVertex2f((float)r.right, (float)r.bottom); glVertex2f((float)r.left, (float)r.bottom);
	glEnd();
	glColor4f(1, 1, 1, 1);
}

void omgl_read_into(OMediaGfxWorld *src, const OMediaRect &sr, OMediaGfxPixels *dest, long dx, long dy, long dw, long dh)
{
	OMediaGLSurface *s = src->port_surface();
	if (!s || !s->fbo || !dest->pix) return;
	std::vector<unsigned int> all(s->width * s->height);
	omgl_surface_read(s, &all[0]);
	for (long y = 0; y < dh; y++)
	{
		long sy = sr.top + (y * sr.get_height()) / (dh ? dh : 1), ty = dy + y;
		if (sy < 0 || sy >= s->height || ty < 0 || ty >= dest->height) continue;
		for (long x = 0; x < dw; x++)
		{
			long sx = sr.left + (x * sr.get_width()) / (dw ? dw : 1), tx = dx + x;
			if (sx < 0 || sx >= s->width || tx < 0 || tx >= dest->width) continue;
			dest->pix[ty * dest->width + tx] = all[sy * s->width + sx] | 0xFF000000;
		}
	}
	dest->version++;
}

// port: presentation only; the authored camera and scene remain 640x480.
static bool ws_scene = false;
static float ws_top = -1, ws_bottom = -1, ws_pan = 60;
static Uint32 ws_tick = 0;
static float ws_x=0, ws_y=0, ws_w=640, ws_h=480;
static int ws_dw=640, ws_dh=480;
static const char *ws_mode() {
    const char *m=getenv("SB_WIDESCREEN");
    return ws_scene ? (m ? m : "fill") : "off";
}
void omgl_widescreen_scene(bool active, float top, float bottom) {
    if(active != ws_scene) { ws_pan=60; ws_tick=0; }
    ws_scene=active; ws_top=top; ws_bottom=bottom;
}
void omgl_window_to_logical(int &x, int &y) {
    if(!omg_win) return;
    int w,h; SDL_GetWindowSize(omg_win,&w,&h);
    if(w<=0 || h<=0) return;
    x=(int)floor((x*(float)ws_dw/w-ws_x)*omg_lw/ws_w);
    y=(int)floor((y*(float)ws_dh/h-ws_y)*omg_lh/ws_h);
}
void omgl_logical_size(int &w, int &h) { w=omg_lw; h=omg_lh; }
void omgl_logical_to_window(int &x, int &y) {
    if(!omg_win) return;
    int w,h; SDL_GetWindowSize(omg_win,&w,&h);
    x=(int)((ws_x+(x+0.5f)*ws_w/omg_lw)*w/ws_dw);
    y=(int)((ws_y+(y+0.5f)*ws_h/omg_lh)*h/ws_dh);
}
static void ws_disc(float x, float y, float r) {
    glBegin(GL_TRIANGLE_FAN); glVertex2f(x,y);
    for(int k=0;k<=48;++k) { float a=k*6.2831853f/48; glVertex2f(x+cos(a)*r,y+sin(a)*r); }
    glEnd();
}
static void ws_panels(int dw, int dh) {
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.18f,0.38f,0.40f);
    omf_quad(0,0,dw,dh,0,0,0,0);
    // Original procedural planks, seams, and bubble/porthole rings.
    for(int y=0;y<dh;y+=90) {
        glColor3f(0.63f+(y%3)*0.025f,0.43f,0.22f);
        omf_quad(0,y+3,dw,y+87,0,0,0,0);
        glColor3f(0.39f,0.25f,0.12f);
        for(int x=18;x<dw;x+=71) omf_quad(x,y+20,x+35,y+22,0,0,0,0);
    }
    for(int side=0;side<2;++side) for(int j=0;j<5;++j) {
        float cx=side?dw-ws_x*0.5f:ws_x*0.5f, cy=(j+0.5f)*dh/5;
        float r=(j%2?22:42);
        glColor3f(0.91f,0.69f,0.26f); ws_disc(cx,cy,r+7);
        glColor3f(0.08f,0.40f,0.51f); ws_disc(cx,cy,r);
        glColor3f(0.52f,0.83f,0.86f);
        glBegin(GL_LINE_LOOP);
        for(int k=0;k<48;++k) { float a=k*6.2831853f/48; glVertex2f(cx+cos(a)*r,cy+sin(a)*r); }
        glEnd();
        for(int k=0;k<8;++k) {
            float a=k*6.2831853f/8;
            glColor3f(0.36f,0.28f,0.15f); ws_disc(cx+cos(a)*(r+3.5f),cy+sin(a)*(r+3.5f),1.5f);
        }
    }
    glColor4f(1,1,1,1);
}

// --------------------------------------------------------------------------- present

void omgl_present(OMediaGLSurface *s)
{
	if (!omg_win || !s || !s->tex) return;
	int dw, dh;
	SDL_GL_GetDrawableSize(omg_win, &dw, &dh);
	if(dw<=0 || dh<=0) return;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	omg_bound = NULL;
	glViewport(0, 0, dw, dh);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	// letterbox: uniform scale, centred
	float sc = (float)dw / s->width;
	if ((float)dh / s->height < sc) sc = (float)dh / s->height;
	float w = s->width * sc, h = s->height * sc, x0 = (dw - w) * 0.5f, y0 = (dh - h) * 0.5f;
    const char *mode=ws_mode();
    if(!strcmp(mode,"stretch")) { w=dw; h=dh; x0=y0=0; }
    if(!strcmp(mode,"fill") && (float)dw/dh > (float)s->width/s->height) {
        sc=(float)dw/s->width; w=dw; h=s->height*sc; x0=0;
        float visible=dh/sc, limit=s->height-visible;
        int mx,my,ww,wh; SDL_GetMouseState(&mx,&my); SDL_GetWindowSize(omg_win,&ww,&wh);
        // A pointer near a window edge requests that authored edge, allowing the
        // cropped area to be explored without changing the scene camera.
        float focus=wh>0 ? my*(float)s->height/wh : s->height*0.5f;
        if(focus<0) focus=0; if(focus>s->height) focus=(float)s->height;
        float low=ws_top>=0 ? ws_top-12 : focus, high=ws_bottom>=0 ? ws_bottom+12 : focus;
        if(focus<low) low=focus; if(focus>high) high=focus;
        float target=ws_pan;
        if(high-low>visible) target=(low+high-visible)*0.5f;
        else { if(high>target+visible) target=high-visible; if(low<target) target=low; }
        if(target<0) target=0; if(target>limit) target=limit;
        Uint32 now=SDL_GetTicks(); float dt=ws_tick?(now-ws_tick)*0.001f:1; ws_tick=now;
        ws_pan+=(target-ws_pan)*(1-expf(-8*dt));
        if(ws_pan<0) ws_pan=0; if(ws_pan>limit) ws_pan=limit;
        y0=dh-h+ws_pan*sc; // GL bottom origin; input uses the top origin below.
    }
    ws_dw=dw; ws_dh=dh; ws_x=x0; ws_y=dh-y0-h; ws_w=w; ws_h=h;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, dw, 0, dh, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_LIGHTING);
	if(!strcmp(mode,"panels")) ws_panels(dw,dh);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, s->tex);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	omf_quad(x0, y0, x0 + w, y0 + h, 0, 0, 1, 1);
	glDisable(GL_TEXTURE_2D);
	omf_SDLFrameHook(omg_win);
	SDL_GL_SwapWindow(omg_win);
}
