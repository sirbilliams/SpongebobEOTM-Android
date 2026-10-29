/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia3DWorld.h"
#include "OMediaCanvas.h"
#include "OMediaGfxWorld.h"
#include <algorithm>
#include <math.h>
#include <stdio.h>

OMedia3DAbstractRasterizer::OMedia3DAbstractRasterizer() {nearz = 10.0f;}		// 0x100045a0
OMedia3DAbstractRasterizer::~OMedia3DAbstractRasterizer() {}	// 0x10004620

//------------------------------------------------------------------------------ OMedia3DWorld

// 0x10036550
OMedia3DWorld::OMedia3DWorld()
{
	sort_mode = 0;
	focal = 400.0f;
	far_z = 3200.0f;
	near_z = 10.0f;
	ambient.set(0x4444, 0x4444, 0x4444);
	rasterizer = NULL;
}

// 0x10036630: deletes its elements first (the lights leave the list as they go), then the base
OMedia3DWorld::~OMedia3DWorld()
{
	destroying = true;
	while (elements.size()) delete elements.front();
	lights.clear();
}

OMediaVPModel *OMedia3DWorld::create_vp_model(OMediaViewPort *vp) { return new OMedia3DVPModel(this, vp); }

void OMedia3DWorld::add_light(OMedia3DLight *l) { lights.push_back(l); }
void OMedia3DWorld::remove_light(OMedia3DLight *l) { lights.remove(l); }

//------------------------------------------------------------------------------ OMedia3DVPModel

OMedia3DVPModel::OMedia3DVPModel(OMediaWorld *w, OMediaViewPort *vp) : OMediaVPModel(w, vp)
{
	cam_scale_x = cam_scale_y = 1.0f;
	viewx = viewy = viewz = 0;
	viewax = vieway = viewaz = 0;
	scale_x = scale_y = center_x = center_y = left = top = right = bottom = 0;
}

OMedia3DVPModel::~OMedia3DVPModel() {}

// TODO (log): OMT.dll's picking (0x10035e10) walks the same polygon list; the port's click tests on 3D elements come with the game
void OMedia3DVPModel::test_click(OMediaWorldClickInfo *) { printf("OMedia3DVPModel::test_click: TODO\n"); }

// 0x10033490: the camera and the viewport rectangle (in the target world's own pixels)
void OMedia3DVPModel::prepare_viewport_logic(OMediaGfxWorld *g, OMediaRect &r)
{
	viewx = viewport->getx();
	viewy = viewport->gety();
	viewz = viewport->getz();
	viewax = viewport->get_anglex();
	vieway = viewport->get_angley();
	viewaz = viewport->get_anglez();
	r = viewport->get_bounds();
	r.left -= g->get_origin_x();
	r.right -= g->get_origin_x();
	r.top -= g->get_origin_y();
	r.bottom -= g->get_origin_y();
	left = (float)r.left;
	top = (float)r.top;
	right = (float)r.right;
	bottom = (float)r.bottom;
	center_x = (float)(((short)(r.right - r.left)) >> 1);
	center_y = (float)(((short)(r.bottom - r.top)) >> 1);
	OMedia3DWorld *w3 = (OMedia3DWorld*)world;
	scale_x = w3->focal * cam_scale_x;
	scale_y = -(w3->focal * cam_scale_y);
}

// 0x100351e0 (normal_to_rgb_material). The retail code adds, per light that is on, in the world's layer mask,
// the light's colour times the cosine to the material's diffuse; emission and the ambient colour times the
// material's ambient come first. Light kinds: 0 a direction, 1 a point, 2 a point with range and a falloff
// ((1 - falloff) of the range at full strength, then linear to the edge).
// port: the retail code does the products in 16 bit fixed point on the 1.x materials' 0..0xffff guns; floats here.
void OMedia3DVPModel::light_vertex(OMedia3DMaterial *m, const OMedia3DVector &n, const OMedia3DPoint &p, float *rgb)
{
	OMedia3DWorld *w3 = (OMedia3DWorld*)world;
	if (m->mode == ommm_Color || (m->mode == ommm_TexturedLight && !m->shaded))
	{
		if (m->mode == ommm_Color) {rgb[0] = m->diffuse[0]; rgb[1] = m->diffuse[1]; rgb[2] = m->diffuse[2];}
		else rgb[0] = rgb[1] = rgb[2] = 1.0f;
		return;
	}
	float amb[3] = {w3->ambient.red / 65535.0f, w3->ambient.green / 65535.0f, w3->ambient.blue / 65535.0f};
	for (int c = 0; c < 3; c++) rgb[c] = m->emission[c] + amb[c] * m->ambient[c];
	for (std::list<OMedia3DLight*>::iterator li = w3->get_lights().begin(); li != w3->get_lights().end(); li++)
	{
		OMedia3DLight *l = *li;
		if (l->gethide() || !(l->mask & m->layers)) continue;
		OMedia3DVector ld;
		float atten = 1.0f;
		if (l->kind == 0) ld = l->dir;
		else
		{
			OMedia3DVector d(p.x - l->getabsoluteposx(), p.y - l->getabsoluteposy(), p.z - l->getabsoluteposz());
			float dist = d.quick_magnitude();
			if (dist <= 0) continue;
			if (l->kind == 2)
			{
				if (dist > l->range) continue;
				float inner = 1.0f - l->falloff, t = dist / l->range;
				atten = (inner < t) ? 1.0f - (t - inner) / (1.0f - inner) : 1.0f;
			}
			ld.set(d.x / dist, d.y / dist, d.z / dist);
		}
		float cs = -(ld.x * n.x + ld.y * n.y + ld.z * n.z) * atten;
		if (!(cs > 0)) continue;		// (also a NaN from a degenerate normal)
		for (int c = 0; c < 3; c++) rgb[c] += l->diffuse[c] * cs * m->diffuse[c];
	}
	for (int c = 0; c < 3; c++) if (rgb[c] > 1.0f) rgb[c] = 1.0f;
}

// One shape element: its vertices into world space, then the camera, per polygon: light, clip at the near plane,
// project. port: replaces gen_single_polygon & co (0x10006a00 ..) that feed the software rasterizer.
void OMedia3DVPModel::add_element(OMedia3DShapeElement *e, std::vector<OMedia3DTranslatedPolygon> &opaque,
								  std::vector<OMedia3DTranslatedPolygon> &blended)
{
	OMedia3DShape *shape = e->getshape();
	if (!shape) return;
	OMedia3DWorld *w3 = (OMedia3DWorld*)world;
	std::vector<OMedia3DPoint> &vs = *shape->get_vertices();
	std::vector<OMedia3DVector> &ns = *shape->get_normals();
	std::vector<OMedia3DPolygon> &ps = *shape->get_polygons();
	std::vector<OMedia3DPoint> wp(vs.size()), cp(vs.size());
	std::vector<OMedia3DVector> wn(ns.size());
	short ax = e->get_anglex(), ay = e->get_angley(), az = e->get_anglez();
	float px = e->getabsoluteposx(), py = e->getabsoluteposy(), pz = e->getabsoluteposz();
	for (size_t i = 0; i < vs.size(); i++)
	{
		OMedia3DVector v(vs[i].x, vs[i].y, vs[i].z);
		v.rotate(ax, ay, az);
		wp[i].set(v.x + px, v.y + py, v.z + pz);
		OMedia3DVector c(wp[i].x - viewx, wp[i].y - viewy, wp[i].z - viewz);
		c.inv_rotate(-viewax, -vieway, -viewaz);			// 0x10033380 transform_point
		cp[i].set(c.x, c.y, c.z);
	}
	for (size_t i = 0; i < ns.size(); i++)
	{
		wn[i] = ns[i];
		wn[i].rotate(ax, ay, az);
	}
	for (size_t pi = 0; pi < ps.size(); pi++)
	{
		OMedia3DPolygon &pl = ps[pi];
		if (!pl.material || pl.num_points < 3 || (pl.flags & om3pf_Hide)) continue;
		OMedia3DTranslatedPolygon poly;
		poly.num_points = 0;
		poly.material = pl.material;
		poly.alpha = pl.material->alpha;
		bool bad = false;
		OMedia3DRenderPoint in[4];
		for (int k = 0; k < pl.num_points && k < 4; k++)
		{
			unsigned long vi = pl.vertex[k];
			if (vi >= vs.size()) {bad = true; break;}
			in[k].x = cp[vi].x;
			in[k].y = cp[vi].y;
			in[k].z = cp[vi].z;
			in[k].u = pl.u[k];
			in[k].v = pl.v[k];
			unsigned long ni = pl.normal_index[k];
			OMedia3DVector nn = ni < wn.size() ? wn[ni] : pl.normal;
			light_vertex(pl.material, nn, wp[vi], &in[k].r);
		}
		if (bad) continue;
		// clip against the near plane (Sutherland-Hodgman)
		OMedia3DRenderPoint cl[8];
		int nc = 0, n = pl.num_points > 4 ? 4 : pl.num_points;
		for (int k = 0; k < n; k++)
		{
			OMedia3DRenderPoint &a = in[k], &b = in[(k + 1) % n];
			bool ain = a.z >= w3->near_z, bin = b.z >= w3->near_z;
			if (ain) cl[nc++] = a;
			if (ain != bin)
			{
				float t = (w3->near_z - a.z) / (b.z - a.z);
				OMedia3DRenderPoint m;
				m.x = a.x + (b.x - a.x) * t; m.y = a.y + (b.y - a.y) * t; m.z = w3->near_z;
				m.u = a.u + (b.u - a.u) * t; m.v = a.v + (b.v - a.v) * t;
				m.r = a.r + (b.r - a.r) * t; m.g = a.g + (b.g - a.g) * t; m.b = a.b + (b.b - a.b) * t;
				cl[nc++] = m;
			}
		}
		if (nc < 3) continue;
		float zsum = 0;
		for (int k = 0; k < nc; k++)
		{
			OMedia3DRenderPoint &o = poly.pts[k];
			o = cl[k];
			float iz = 1.0f / cl[k].z;
			o.x = cl[k].x * iz * scale_x + center_x + left;			// 0x10034eb0 project_points
			o.y = cl[k].y * iz * scale_y + center_y + top;
			zsum += cl[k].z;
		}
		poly.num_points = nc;
		poly.sort_z = zsum / nc;
		// facing: the screen-space area; polygons facing away are dropped unless two-sided. port: OMT.dll culls
		// in the rasterizer (back-face flag of the material); sb2.exe's shapes are one-sided.
		float area = 0;
		for (int k = 0; k < nc; k++)
		{
			OMedia3DRenderPoint &a = poly.pts[k], &b = poly.pts[(k + 1) % nc];
			area += a.x * b.y - b.x * a.y;
		}
		if (area > 0 && !(pl.flags & om3pf_TwoSided)) continue;
		if (poly.alpha < 0.999f) blended.push_back(poly);
		else opaque.push_back(poly);
	}
}

void OMedia3DVPModel::gather(omt_ElementList::iterator first, omt_ElementList::iterator last,
							 std::vector<OMedia3DTranslatedPolygon> &opaque, std::vector<OMedia3DTranslatedPolygon> &blended)
{
	for (; first != last; first++)
	{
		OMediaElement *e = *first;
		if (e->gethide()) continue;
		if (e->get_type() == omcet_3DElement)
		{
			OMedia3DShapeElement *se = dynamic_cast<OMedia3DShapeElement*>(e);
			if (se) add_element(se, opaque, blended);
		}
		if (!e->get_element_list()->empty()) gather(e->get_element_list()->begin(), e->get_element_list()->end(), opaque, blended);
	}
}

static bool omf_far_first(const OMedia3DTranslatedPolygon &a, const OMedia3DTranslatedPolygon &b) {return a.sort_z > b.sort_z;}

// 0x10033660. Without a canvas or a rasterizer nothing is drawn. The double canvas is swapped as the base model does.
void OMedia3DVPModel::render(void)
{
	OMediaCanvas *c = viewport->getcanvas();
	OMedia3DWorld *w3 = (OMedia3DWorld*)world;
	if (!c) return;
	OMedia3DAbstractRasterizer *ras = w3->rasterizer;
	if (ras)
	{
		OMediaGfxWorld *g = c->getdefaultdrawgfxworld();
		g->lock();
		OMediaRect r;
		prepare_viewport_logic(g, r);
		std::vector<OMedia3DTranslatedPolygon> opaque, blended;
		gather(world->get_element_list()->begin(), world->get_element_list()->end(), opaque, blended);
		std::stable_sort(blended.begin(), blended.end(), omf_far_first);		// 0x1004c6xx eyeaverage_sort
		ras->nearz = w3->near_z;
		ras->start(g, (short)r.left, (short)r.top, (short)(r.right - r.left), (short)(r.bottom - r.top), w3->far_z);
		for (size_t i = 0; i < opaque.size(); i++) ras->rasterize(&opaque[i]);
		for (size_t i = 0; i < blended.size(); i++) ras->rasterize(&blended[i]);
		ras->end();
		g->unlock();
	}
	OMediaDoubleCanvas *dc = viewport->getdoublecanvas();
	if (dc)
	{
		OMediaRect b = viewport->get_bounds();
		world->add_new_swapcanvas(dc, &b, NULL, dc->support_page_flipping(), false);
	}
}
