/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMedia3DPolygon (0x70 bytes: +0x04 material, +0x08 four vertex indices, +0x18 four normal indices, +0x28 point
    count, +0x2c face normal, +0x3c user id, +0x40 flags, +0x50 four u,v pairs) and OMedia3DShape: the vertices
    (16 bytes each), normals (16 bytes), polygons and a few flags of one 3D object. sb2.exe builds a Granny
    character's shape itself (0x4039c0 BuildShape, per frame 0x4056xx copies the skinned vertices in).
    port: the shape is not a database object here (the game never loads one from a file).               */
#pragma once
#ifndef OMEDIA1_3DShape_H
#define OMEDIA1_3DShape_H

#include "OMedia3DPoint.h"
#include "OMedia3DVector.h"
#include <vector>

class OMedia3DMaterial;

typedef unsigned short omt_3DPolygonFlags;
const omt_3DPolygonFlags om3pf_Selected = (1<<0);
const omt_3DPolygonFlags om3pf_TwoSided = (1<<1);
const omt_3DPolygonFlags om3pf_Hide		= (1<<2);

class OMedia3DPolygon
{
	public:
	omtshared OMedia3DPolygon();									// 0x1001eec0
	omtshared OMedia3DPolygon(const OMedia3DPolygon &x);			// 0x1001ee30
	omtshared virtual ~OMedia3DPolygon();							// 0x1001ef00
	omtshared OMedia3DPolygon &operator=(const OMedia3DPolygon &x);	// 0x1001f230
	omtshared void set_material(OMedia3DMaterial *m);				// 0x100321f0 (counts the use)

	inline OMedia3DMaterial *get_material(void) const {return material;}

	OMedia3DMaterial	*material;			// +0x04
	unsigned long		vertex[4];			// +0x08
	unsigned long		normal_index[4];	// +0x18
	short				num_points;			// +0x28 (3 or 4)
	OMedia3DVector		normal;				// +0x2c face normal
	unsigned long		user_id;			// +0x3c
	unsigned char		flags;				// +0x40
	float				u[4], v[4];			// +0x50 (stored as pairs)
};

class OMedia3DShape
{
	public:
	omtshared OMedia3DShape();						// 0x1001f500
	omtshared virtual ~OMedia3DShape();				// 0x1001f610

	omtshared virtual void prepare(unsigned short flags);	// 0x10020410 vt+0x20 (0x20: polygon normals; the rest TODO)
	omtshared virtual void scale(float sx, float sy, float sz);	// 0x1001fc40 vt+0x2c
	omtshared virtual void compute_polygon_normals(bool only_selected = false);	// 0x10020330 vt+0x78

	inline std::vector<OMedia3DPoint> *get_vertices(void) {return &vertices;}		// shape +0x28
	inline std::vector<OMedia3DVector> *get_normals(void) {return &normals;}		// shape +0x38
	inline std::vector<OMedia3DPolygon> *get_polygons(void) {return &polygons;}	// shape +0x48 (0x70 each)

	protected:
	std::vector<OMedia3DPoint>	vertices;
	std::vector<OMedia3DVector>	normals;
	std::vector<OMedia3DPolygon> polygons;
};

#endif
