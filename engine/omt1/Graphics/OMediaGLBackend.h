/*  port: the OpenGL back end of the OMT 1.x engine layer (engine/sdl/OMediaSDLGfx.cpp).
    OMT.dll draws through DirectDraw/Direct3D (and GDI); here the screen is an SDL window with
    a GL context, and the DirectDraw surfaces are GL framebuffers:
      - the primary surface is an off-screen framebuffer (it keeps its pixels between frames,
        like a DirectDraw back buffer); presenting draws it into the window, letterboxed, and swaps;
      - an off-screen surface is a framebuffer of its own size;
      - a memory world drawn into a GL surface is uploaded as a texture (cached by version).  */
#pragma once
#ifndef OMEDIA1_GLBackend_H
#define OMEDIA1_GLBackend_H

#include "OMediaRect.h"

class OMediaGfxWorld;
struct OMediaGfxPixels;

struct OMediaGLSurface
{
	unsigned int	fbo, tex, depth;
	int				width, height;		// logical size (what the game draws in)
	bool			primary;
	int				scale;				// port: HD pack: the texture is width*scale x height*scale (the primary surface only)
};

// Window and context
bool omgl_open_window(const char *title, int width, int height);		// logical size of the screen
void omgl_close_window(void);
bool omgl_window_open(void);
void omgl_present(OMediaGLSurface *s);			// draw the surface into the window, frame hook, swap
void omgl_pump_events(void);					// keep the window alive between presents

// port: one presentation transform shared by rendering, input and click replay.
void omgl_widescreen_scene(bool active, float playerTop = -1, float playerBottom = -1);
void omgl_window_to_logical(int &x, int &y);
void omgl_logical_to_window(int &x, int &y);
void omgl_logical_size(int &w, int &h);

// Surfaces
void omgl_surface_create(OMediaGLSurface *s, int width, int height, bool primary);
void omgl_surface_destroy(OMediaGLSurface *s);
void omgl_surface_bind(OMediaGLSurface *s);		// render target, 2D projection in surface pixels
void omgl_surface_read(OMediaGLSurface *s, unsigned int *argb);	// whole surface, top-down 0xAARRGGBB
void omgl_surface_write(OMediaGLSurface *s, const unsigned int *argb);

// Drawing (called from OMediaGfxWorld::port_blit/port_fill)
void omgl_blit(OMediaGfxWorld *src, OMediaGfxWorld *dest, const OMediaRect &src_rect,
			   long dx, long dy, long dw, long dh, bool keyed);
void omgl_fill(OMediaGfxWorld *dest, const OMediaRect &r, unsigned int argb);
void omgl_read_into(OMediaGfxWorld *src, const OMediaRect &src_rect, OMediaGfxPixels *dest,
					long dx, long dy, long dw, long dh);
void omgl_forget(OMediaGfxPixels *p);

// port: HD background pack (engine/sdl/SbHd.h). Standing in for the memory world's pixels, the pack picture
// 'key' ("sceneart/c1l0s1/background") is drawn when the world is blitted into a GL surface; the layout stays
// that of the world (same size, same coordinates).  Any problem leaves the world as it was.
void omgl_hd_attach(OMediaGfxWorld *w, const char *key);			// the memory world is gone: delete its texture

#endif
