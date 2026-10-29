/*  port: see OMedia3DGLRasterizer.h */
#define OMD_SDL_NO_GL_WRAP
#include "OMediaSysDefs.h"
#include "OMedia3DGLRasterizer.h"
#include "OMediaGfxWorld.h"
#include "OMediaGLBackend.h"
#include "OMediaCanvas.h"
#include <vector>
#include <stdio.h>

OMedia3DGLRasterizer::OMedia3DGLRasterizer()
{
	target = NULL;
	farz = 3200.0f;
	anti_aliasing = texture_filtering = 0;
	bound = 0;
}

OMedia3DGLRasterizer::~OMedia3DGLRasterizer() {}	// port: bitmap worlds own their GL copies

// the GL texture of a material's canvas, uploaded again when the pixels' version changes
unsigned int OMedia3DGLRasterizer::texture_of(OMedia3DMaterial *m)
{
	if (!m || !m->get_texture() || !m->get_texture()->getgfxworld()) return 0;
	OMediaGfxPixels *px = m->get_texture()->getgfxworld()->port_pixels();
	if (!px || !px->pix) return 0;
	// port: the bitmap owns this copy. A rasterizer-wide pointer cache survives
	// deleted models and mistakes reused addresses for the previous texture.
	if (px->gl_3d_tex && px->gl_3d_epoch != omgl_context_epoch()) px->gl_3d_tex = 0;		// lane c: the GL context was re-created
	if (!px->gl_3d_tex || px->gl_3d_version != px->version || px->gl_3d_w != (unsigned int)px->width || px->gl_3d_h != (unsigned int)px->height)
	{
		if (!px->gl_3d_tex) { glGenTextures(1, &px->gl_3d_tex); px->gl_3d_epoch = omgl_context_epoch(); }
		std::vector<unsigned char> rgba(px->width * px->height * 4);
		for (int i = 0; i < px->width * px->height; i++)
		{
			unsigned int c = px->pix[i];
			rgba[i * 4 + 0] = (unsigned char)(c >> 16);
			rgba[i * 4 + 1] = (unsigned char)(c >> 8);
			rgba[i * 4 + 2] = (unsigned char)c;
			rgba[i * 4 + 3] = (unsigned char)(c >> 24);
		}
		glBindTexture(GL_TEXTURE_2D, px->gl_3d_tex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, px->width, px->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &rgba[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		px->gl_3d_version = px->version;
		px->gl_3d_w = px->width;
		px->gl_3d_h = px->height;
	}
	glBindTexture(GL_TEXTURE_2D, px->gl_3d_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_filtering ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_filtering ? GL_LINEAR : GL_NEAREST);
	return px->gl_3d_tex;
}

// OMedia3DAbstractRasterizer::start (0x10004660): the target, the rectangle (target pixels, y down)
void OMedia3DGLRasterizer::start(OMediaGfxWorld *w, short x, short y, short rw, short rh, float fz)
{
	target = w;
	farz = fz;
	OMediaGLSurface *s = w->port_surface();
	if (!s || !s->fbo) {target = NULL; return;}
	omgl_surface_bind(s);
	glEnable(GL_SCISSOR_TEST);
	glScissor(x, s->height - (y + rh), rw, rh);
	glClearDepth(1.0);
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
}

// One polygon. The points are viewport pixels with the camera distance z. The surface's projection is an
// orthographic pixel one (omgl_surface_bind), so a point goes in as (x*z, y*z, depth*z, z): after the divide
// by w the texture and colours are perspective-correct and the depth is 1/z mapped to [-1,1] (clip z = -depth*w).
void OMedia3DGLRasterizer::rasterize(OMedia3DTranslatedPolygon *p)
{
	if (!target || p->num_points < 3) return;
	unsigned int tex = texture_of(p->material);
	if (tex)
	{
		glEnable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, p->alpha < 0.999f ? 0.0f : 0.5f);	// lane f: a blended material (the characters' shadow, alpha 0.47) would fail the 0.5 cut-out test everywhere
	}
	else glDisable(GL_TEXTURE_2D);
	if (p->alpha < 0.999f)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
	}
	else
	{
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}
	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < p->num_points; i++)
	{
		OMedia3DRenderPoint &q = p->pts[i];
		float w = q.z;
		float d = 1.0f - 2.0f * ((1.0f / q.z) - (1.0f / farz)) / ((1.0f / nearz) - (1.0f / farz));
		glColor4f(q.r, q.g, q.b, p->alpha);
		glTexCoord2f(q.u, q.v);
		glVertex4f(q.x * w, q.y * w, -d * w, w);
	}
	glEnd();
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

void OMedia3DGLRasterizer::end(void)
{
	if (!target) return;
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glColor4f(1, 1, 1, 1);
	target = NULL;
}
