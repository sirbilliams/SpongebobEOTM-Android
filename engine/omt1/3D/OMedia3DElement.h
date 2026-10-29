/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMedia3DElement: an element of a 3D world: a position (OMediaElement), an angle (OMediaWorldAngle, +0x6c),
    an automatic rotation (+0x78) and a navigator (+0xd0 navigator mode). OMedia3DShapeElement shows an
    OMedia3DShape at that position and angle. OMedia3DLight is a light of the world.
    Layout in OMT.dll: +0x00 OMediaElement (0x6c bytes), +0x6c OMediaWorldAngle, +0x78 OMediaAutoRotate,
    +0x94 OMedia3DNavigator (flag +0xd0), +0xd6 default generation state.
    port: the generation functions (gen_single_polygon, gen_array_shape, ... 0x10005ab0 .. 0x10008320) feed
    OMT.dll's software rasterizer; the GL rasterizer draws the shape from OMedia3DVPModel instead, so they
    are not rebuilt.                                                                                      */
#pragma once
#ifndef OMEDIA1_3DElement_H
#define OMEDIA1_3DElement_H

#include "OMediaElement.h"
#include "OMedia3DNavigator.h"
#include "OMedia3DShape.h"

class OMedia3DWorld;

class OMedia3DElement : public OMediaElement, public OMediaWorldAngle, public OMediaAutoRotate, public OMedia3DNavigator
{
	public:
	omtshared OMedia3DElement();							// 0x10004d10
	omtshared virtual ~OMedia3DElement();					// 0x10004de0
	omtshared virtual omt_ElementType get_type(void) const;	// 0x10004e80  vt+0x0c (2)
	omtshared virtual void update(void);					// 0x10004e90  vt+0x20
	omtshared virtual void set_angle(short ax, short ay, short az);	// 0x100055c0
	omtshared virtual void set_default_gen_state(unsigned short s);	// 0x10008410

	inline void set_navigator_mode(bool n) {navigator_mode = n;}
	inline bool get_navigator_mode(void) const {return navigator_mode;}
	inline unsigned short get_default_gen_state(void) const {return gen_state;}

	protected:
	bool			navigator_mode;		// +0xd0
	unsigned short	gen_state;			// +0xd6
};

class OMedia3DShapeElement : public OMedia3DElement
{
	public:
	omtshared OMedia3DShapeElement();						// 0x100313a0
	omtshared virtual ~OMedia3DShapeElement();				// 0x10031410
	omtshared virtual void setshape(OMedia3DShape *s);		// 0x10031450
	omtshared virtual OMedia3DShape *getshape(void);		// 0x10031480  (vt+0x9c in sb2.exe's calls)
	omtshared virtual void deselect_all(void);				// 0x10031640 (nothing to do here)

	protected:
	OMedia3DShape	*shape;
	bool			owns_shape;		// the constructor's (sb2.exe's own "new OMedia3DShape(1)" is handed in by setshape)
};

class OMedia3DLight : public OMedia3DElement
{
	public:
	omtshared OMedia3DLight();								// 0x1001c380
	omtshared virtual ~OMedia3DLight();						// 0x1001c460
	omtshared virtual omt_ElementType get_type(void) const;	// 0x1001c4e0  (3)
	omtshared virtual void set_angle(short ax, short ay, short az);	// 0x1001c5c0  rotates (0,0,1) into dir
	omtshared virtual void unlink(void);					// 0x1001c4f0

	// kind (+0xfc): 0 parallel (direction), 1 point (no range), 2 point with range and falloff
	int				kind;
	OMedia3DVector	dir;				// +0xe8 (0,0,1)
	float			diffuse[3];			// +0xdc 16 bit guns /65535 (white)
	float			specular[3];		// +0xe2 (black)
	float			range;				// +0x108 (128)
	float			falloff;			// +0x10c (0.5): the part of the range where the light is at full strength is 1-falloff
	float			f100, f104;			// +0x100 (0.5), +0x104 (0.3): not read by the reached code
	unsigned long	mask;				// +0x110 (-1) layers

	protected:
	omtshared virtual void update_world_ptr(OMediaWorld *w);	// 0x1001c500  joins/leaves the world's light list
};

#endif
