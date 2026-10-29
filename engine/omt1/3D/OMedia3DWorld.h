/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMedia3DWorld: a world of 3D elements and lights, seen through viewports (camera = the viewport's position
    and angle). sb2.exe makes one per scene (0x4033f0): no background (the 2D world's art shows through),
    focal length (+0x80, from the scene camera), far/near planes (+0x84, +0x88), ambient colour (+0x8c) and the
    rasterizer (+0xa0), then a viewport and an OMediaVPElement that puts the viewport into the 2D world.
    OMedia3DVPModel (0x10032ee0) renders it: transform (view = inverse rotation of the viewport's angles, then
    x/z*focal), light, sort, rasterize. OMedia3DAbstractRasterizer is the rasterizer interface (vtable 0x103a9554):
    port: OMT.dll's software/Direct3D rasterizers are replaced by a GL one (OMedia3DGLRasterizer).        */
#pragma once
#ifndef OMEDIA1_3DWorld_H
#define OMEDIA1_3DWorld_H

#include "OMediaWorld.h"
#include "OMediaViewPort.h"
#include "OMediaRGBColor.h"
#include "OMedia3DElement.h"
#include "OMedia3DMaterial.h"
#include <list>
#include <map>

class OMediaGfxWorld;
class OMedia3DAbstractRasterizer;

// What the rasterizer gets: a convex polygon in viewport pixels (x,y in the target world's coordinates), z the
// distance from the camera, lit colour per point (0..1), texture coordinates of the material's canvas.
struct OMedia3DRenderPoint
{
	float x, y, z;
	float u, v;
	float r, g, b;
};

class OMedia3DTranslatedPolygon
{
	public:
	int					num_points;
	OMedia3DRenderPoint	pts[8];
	OMedia3DMaterial	*material;
	float				alpha;
	float				sort_z;
};

class OMedia3DAbstractRasterizer
{
	public:
	omtshared OMedia3DAbstractRasterizer();
	omtshared virtual ~OMedia3DAbstractRasterizer();
	float nearz;			// port: the world's near plane, set before start() (OMT.dll passes it in the polygons' z range)
	// vt+0x04: draw into 'target' inside the rectangle x,y,w,h (target pixels); clears the depth there
	virtual void start(OMediaGfxWorld *target, short x, short y, short w, short h, float farz) = 0;
	virtual void rasterize(OMedia3DTranslatedPolygon *p) = 0;		// vt+0x08
	virtual void end(void) = 0;										// vt+0x0c
	virtual void process_background(OMediaBackground *) {}			// vt+0x10
	virtual void set_anti_aliasing(int mode) {}						// vt+0x14 (sb2.exe passes 3)
	virtual void set_texture_filtering(int mode) {}					// vt+0x18 (sb2.exe passes 3)
	virtual void nframe_reset_cache(void) {}
};

class OMedia3DWorld : public OMediaWorld
{
	public:
	omtshared OMedia3DWorld();					// 0x10036550
	omtshared virtual ~OMedia3DWorld();			// 0x10036630
	omtshared virtual OMediaVPModel *create_vp_model(OMediaViewPort *vp);	// OMedia3DVPModel

	void add_light(OMedia3DLight *l);			// the light's update_world_ptr (+0x98 list, +0x9c count)
	void remove_light(OMedia3DLight *l);
	inline std::list<OMedia3DLight*> &get_lights(void) {return lights;}

	int					sort_mode;			// +0x7c (0)
	float				focal;				// +0x80 (400) pixels at distance 1
	float				far_z;				// +0x84 (3200)
	float				near_z;				// +0x88 (10)
	OMediaRGBColor		ambient;			// +0x8c (0x4444 each)
	OMedia3DAbstractRasterizer	*rasterizer;	// +0xa0 (set by the game; not owned)

	protected:
	std::list<OMedia3DLight*>	lights;
};

class OMedia3DVPModel : public OMediaVPModel
{
	public:
	omtshared OMedia3DVPModel(OMediaWorld *w, OMediaViewPort *vp);	// 0x10032ee0
	omtshared virtual ~OMedia3DVPModel();							// 0x10033260
	omtshared virtual void render(void);							// 0x10033660
	omtshared virtual void test_click(OMediaWorldClickInfo *ci);	// 0x10035e10  TODO (log): picking is not needed to draw

	// port: picking (test_click 0x10035e10 is not rebuilt): the screen rectangle (target pixels) the element's visible
	// polygons covered in the last render, and the nearest view depth among them. false: it was not drawn.
	bool get_element_bounds(const OMedia3DShapeElement *e, float &l, float &t, float &r, float &b, float &z) const;
	// port: a world point through the last render's view transform and projection (0x10033380 + 0x10034eb0), target pixels.
	// false: behind the near plane or nothing rendered yet.
	bool project_point(float x, float y, float z, float &sx, float &sy) const;

	protected:
	struct ElementBounds {float l, t, r, b, z;};
	std::map<const OMedia3DShapeElement*, ElementBounds>	bounds;
	void prepare_viewport_logic(OMediaGfxWorld *g, OMediaRect &r);	// 0x10033490
	void add_element(OMedia3DShapeElement *e, std::vector<OMedia3DTranslatedPolygon> &opaque, std::vector<OMedia3DTranslatedPolygon> &blended);
	void gather(omt_ElementList::iterator first, omt_ElementList::iterator last, std::vector<OMedia3DTranslatedPolygon> &opaque, std::vector<OMedia3DTranslatedPolygon> &blended);
	void light_vertex(OMedia3DMaterial *m, const OMedia3DVector &n, const OMedia3DPoint &p, float *rgb);	// 0x100351e0

	float	viewx, viewy, viewz;			// the viewport's position
	short	viewax, vieway, viewaz;			// and angles
	float	scale_x, scale_y;				// +0x34, +0x38: focal * model scale (y flipped)
	float	center_x, center_y;				// +0x5c, +0x60 (viewport centre in target pixels)
	float	left, top, right, bottom;		// +0x48.. in target pixels
	float	cam_scale_x, cam_scale_y;		// +0x2c, +0x30 (1, 1)
};

#endif
