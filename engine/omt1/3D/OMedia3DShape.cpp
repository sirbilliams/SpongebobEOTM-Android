/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia3DShape.h"
#include "OMedia3DMaterial.h"
#include <stdio.h>

//------------------------------------------------------------------------------ OMedia3DPolygon

// 0x1001eec0
OMedia3DPolygon::OMedia3DPolygon()
{
	material = NULL;
	for (int i = 0; i < 4; i++) {vertex[i] = 0; normal_index[i] = 0; u[i] = 0; v[i] = 0;}
	num_points = 0;
	normal.set(0, 0, 0);
	user_id = 0;
	flags = 0;
}

// 0x1001ee30
OMedia3DPolygon::OMedia3DPolygon(const OMedia3DPolygon &x)
{
	material = NULL;
	num_points = 0;
	user_id = 0;
	flags = 0;
	*this = x;
}

// 0x1001ef00: gives the material back
OMedia3DPolygon::~OMedia3DPolygon()
{
	if (material) material->db_unlock();
}

// 0x1001f230: set_material for the new material, then the points, flags and the face normal
OMedia3DPolygon &OMedia3DPolygon::operator=(const OMedia3DPolygon &x)
{
	if (this == &x) return *this;
	set_material(x.material);
	num_points = x.num_points;
	for (int i = 0; i < num_points && i < 4; i++)
	{
		vertex[i] = x.vertex[i];
		normal_index[i] = x.normal_index[i];
		u[i] = x.u[i];
		v[i] = x.v[i];
	}
	flags = x.flags;
	normal = x.normal;
	user_id = x.user_id;
	return *this;
}

// 0x100321f0: the polygon counts as a user of its material (+0x14)
void OMedia3DPolygon::set_material(OMedia3DMaterial *m)
{
	if (material) material->db_unlock();
	material = m;
	if (m) m->db_lock();
}

//------------------------------------------------------------------------------ OMedia3DShape

OMedia3DShape::OMedia3DShape() {}		// 0x1001f500
OMedia3DShape::~OMedia3DShape() {}		// 0x1001f610

// 0x1001fc40
void OMedia3DShape::scale(float sx, float sy, float sz)
{
	for (size_t i = 0; i < vertices.size(); i++)
	{
		vertices[i].x *= sx;
		vertices[i].y *= sy;
		vertices[i].z *= sz;
	}
}

// 0x10020330: the face normal of the first three points (OMedia3DVector::compute_normal, left-handed)
void OMedia3DShape::compute_polygon_normals(bool only_selected)
{
	for (size_t i = 0; i < polygons.size(); i++)
	{
		OMedia3DPolygon &p = polygons[i];
		if (only_selected && !(p.flags & om3pf_Selected)) continue;
		if (p.num_points < 3) continue;
		if (p.vertex[0] >= vertices.size() || p.vertex[1] >= vertices.size() || p.vertex[2] >= vertices.size()) continue;
		p.normal.compute_normal(vertices[p.vertex[0]], vertices[p.vertex[1]], vertices[p.vertex[2]]);
	}
}

// 0x10020410. sb2.exe only asks for 0x20 (polygon normals). TODO: the other steps (sphere, vertex normals,
// texture orientation, polygon order) are not needed by the characters.
void OMedia3DShape::prepare(unsigned short flags)
{
	if (flags & 0x20) compute_polygon_normals(false);
	if (flags & ~0x20) printf("OMedia3DShape::prepare: flags %#x not rebuilt (TODO)\n", (unsigned)(flags & ~0x20));
}
