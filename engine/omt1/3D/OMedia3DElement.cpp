/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia3DElement.h"
#include "OMedia3DWorld.h"
#include <stdio.h>

//------------------------------------------------------------------------------ OMedia3DElement

// 0x10004d10
OMedia3DElement::OMedia3DElement()
{
	navigator_mode = false;
	gen_state = 0;
	set_default_gen_state(0xf);
}

OMedia3DElement::~OMedia3DElement() {}		// 0x10004de0

omt_ElementType OMedia3DElement::get_type(void) const { return omcet_3DElement; }	// 0x10004e80

// 0x10004e90: when it runs in a world: navigator mode lets the navigator move/turn it, otherwise the
// element's automatic move (OMediaElement::update) and its automatic rotation
void OMedia3DElement::update(void)
{
	if (world && pause_count == 0)
	{
		if (navigator_mode)
		{
			OMedia3DNavigator::update(this, this, this, this, world->get_timer());
			return;
		}
		OMediaElement::update();
		short ax, ay, az;
		if (autorotate_update(ax, ay, az, world->get_timer()))
			set_angle(anglex + ax, angley + ay, anglez + az);
	}
}

// 0x100055c0
void OMedia3DElement::set_angle(short ax, short ay, short az)
{
	if (navigator_mode) OMedia3DNavigator::set_angle(ax, ay, az);
	else OMediaWorldAngle::set_angle(ax, ay, az);
}

// 0x10008410: also keeps the state's derived value (0xd8, fcn.100083c0) for the generation functions. port: not needed
void OMedia3DElement::set_default_gen_state(unsigned short s) { gen_state = s; }

//------------------------------------------------------------------------------ OMedia3DShapeElement

// 0x100313a0
OMedia3DShapeElement::OMedia3DShapeElement()
{
	shape = NULL;
	owns_shape = false;
}

// 0x10031410
OMedia3DShapeElement::~OMedia3DShapeElement()
{
	if (owns_shape) delete shape;
}

// 0x10031450. port: the element does not own a shape handed in (OMT.dll's shape is a counted database object)
void OMedia3DShapeElement::setshape(OMedia3DShape *s) { shape = s; owns_shape = false; }

OMedia3DShape *OMedia3DShapeElement::getshape(void) { return shape; }		// 0x10031480

void OMedia3DShapeElement::deselect_all(void) {}		// 0x10031640 (selection is a modeller feature)

//------------------------------------------------------------------------------ OMedia3DLight

// 0x1001c380
OMedia3DLight::OMedia3DLight()
{
	kind = 0;
	dir.set(0, 0, 1);
	diffuse[0] = diffuse[1] = diffuse[2] = 1.0f;
	specular[0] = specular[1] = specular[2] = 0.0f;
	range = 128.0f;
	falloff = 0.5f;
	f100 = 0.5f;
	f104 = 0.3f;
	mask = 0xffffffffUL;
}

// 0x1001c460
OMedia3DLight::~OMedia3DLight() { unlink(); }

omt_ElementType OMedia3DLight::get_type(void) const { return omcet_3DLight; }		// 0x1001c4e0

// 0x1001c5c0: the direction is (0,0,1) rotated by the angles (rotation order z, x, y)
void OMedia3DLight::set_angle(short ax, short ay, short az)
{
	OMedia3DElement::set_angle(ax, ay, az);
	dir.set(0, 0, 1);
	dir.rotate(ax, ay, az);
}

// 0x1001c4f0 is OMediaElement::unlink
void OMedia3DLight::unlink(void) { OMediaElement::unlink(); }

// 0x1001c500: a light is in the light list of the world it is in
void OMedia3DLight::update_world_ptr(OMediaWorld *w)
{
	OMediaWorld *old = world;
	if (w != old)
	{
		if (!w)
		{
			if (old) ((OMedia3DWorld*)old)->remove_light(this);
		}
		else if (!old) ((OMedia3DWorld*)w)->add_light(this);
	}
	OMediaElement::update_world_ptr(w);
}
