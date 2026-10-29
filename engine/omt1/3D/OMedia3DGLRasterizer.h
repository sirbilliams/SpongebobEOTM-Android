/*  port: the 3D rasterizer of the OMT 1.x layer on OpenGL. OMT.dll's rasterizers (OMediaOMTRasterizer:
    software, OMediaDXRasterizer: Direct3D, OMedia3DClickTestRasterizer) draw OMedia3DTranslatedPolygons into an
    OMediaGfxWorld. This one draws them into the GL surface of that world (the screen's framebuffer): a depth
    buffer cleared inside the viewport rectangle, perspective-correct textures from the material's canvas
    (colour-keyed texels are cut out), Gouraud colours.                                                    */
#pragma once
#ifndef OMEDIA1_3DGLRasterizer_H
#define OMEDIA1_3DGLRasterizer_H

#include "OMedia3DWorld.h"

struct OMediaGfxPixels;

class OMedia3DGLRasterizer : public OMedia3DAbstractRasterizer
{
	public:
	OMedia3DGLRasterizer();
	virtual ~OMedia3DGLRasterizer();
	virtual void start(OMediaGfxWorld *target, short x, short y, short w, short h, float farz);
	virtual void rasterize(OMedia3DTranslatedPolygon *p);
	virtual void end(void);
	virtual void set_anti_aliasing(int mode) {anti_aliasing = mode;}
	virtual void set_texture_filtering(int mode) {texture_filtering = mode;}

	protected:
	unsigned int texture_of(OMedia3DMaterial *m);

	OMediaGfxWorld	*target;
	float			farz;
	int				anti_aliasing, texture_filtering;
	unsigned int	bound;
};

#endif
