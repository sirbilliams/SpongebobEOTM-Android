/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMedia3DMaterial: a database object ('3DMa', 52 bytes in the game's model .omt files) that says how a
    polygon is coloured: a fixed colour, or lit (emission + ambient + diffuse of the lights), with an optional
    texture canvas. sb2.exe finds the material of a Granny texture by name in the model's database and hands
    it back to Granny as the texture cookie; every polygon of the shape then carries it.
    port: OMT.dll keeps the colours of its OMediaAbstract3DMaterial (+0x10 emission, +0x16 diffuse, +0x1c
    specular, +0x22 ambient as 16 bit guns); here they are 0..1 floats read from the 1.x stream.        */
#pragma once
#ifndef OMEDIA1_3DMaterial_H
#define OMEDIA1_3DMaterial_H

#include "OMediaDBObject.h"
#include "OMediaCanvas.h"

enum omt_MaterialMode { ommm_Color = 0, ommm_Light = 1, ommm_TexturedLight = 2 };	// stream "mtype"

class OMedia3DMaterial : public OMediaDBObject
{
	public:
	enum { db_type = '3DMa' };					// 0x33444d61
	omtshared OMedia3DMaterial();				// 0x1001c920
	omtshared virtual ~OMedia3DMaterial();		// 0x1001cba0
	omtshared static OMediaDBObject *db_builder(void);	// 0x1001ce20
	omtshared virtual unsigned int get_approximate_size(void);	// 0x1001d220
	omtshared virtual void read_class(OMediaStreamOperators &stream);	// OMT 1.x stream (version 1)
	omtshared virtual void write_class(OMediaStreamOperators &stream);

	inline OMediaCanvas *get_texture(void) const {return texture;}
	inline void set_texture(OMediaCanvas *c) {texture = c;}

	int				mode;			// omt_MaterialMode
	bool			shaded;			// lit (mode 2: +0x30)
	bool			gouraud;
	bool			no_fog;
	float			alpha;			// 1 = opaque (the stream stores the transparency)
	float			emission[3], diffuse[3], specular[3], ambient[3];	// 0..1
	unsigned long	layers;			// +0x2c light mask (all)
	OMediaCanvas	*texture;
};

#endif
