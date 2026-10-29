/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMedia3DNavigator: moves/turns a viewport (or 3D element) along its own axes: a 3x3
    orientation (rows at +0x04, +0x14, +0x24) kept with the element's angles.                 */
#pragma once
#ifndef OMEDIA1_3DNavigator_H
#define OMEDIA1_3DNavigator_H

#include "OMediaWorldPosition.h"

class OMedia3DVector;

enum omt_3DAxis { omc3daxis_X = 0, omc3daxis_Y = 1, omc3daxis_Z = 2 };

class OMedia3DNavigator
{
	public:
	omtshared OMedia3DNavigator();			// 0x1001daf0
	omtshared virtual ~OMedia3DNavigator();	// 0x1001db30

	omtshared virtual void set_move_on_axis(omt_3DAxis axis, float speed);	// 0x1001db40  vt+0x04
	omtshared virtual void update(OMediaWorldPosition *pos, OMediaWorldAngle *angle, OMediaAutoMove *am,
								  OMediaAutoRotate *ar, OMediaTimer *timer);	// 0x1001df70  vt+0x08
	omtshared void set_angle(short ax, short ay, short az);					// 0x1001e1a0

	protected:
	omtshared virtual void find_direction_vector(omt_3DAxis axis, float speed, short ax, short ay, short az,
												 OMedia3DVector &v);			// 0x1001db60  vt+0x0c
	omtshared virtual void update_angles(OMediaWorldAngle *angle, OMediaAutoMove *am);	// 0x1001dc50  vt+0x10

	float		m[3][4];			// +0x04 orientation rows (1,0,0 / 0,1,0 / 0,0,1)
	omt_3DAxis	move_axis;			// +0x34 (Z)
	float		move_speed;			// +0x38
	bool		updating_angle;		// +0x3c
	bool		move_changed;		// +0x3d
	bool		angle_changed;		// +0x3e
};

#endif
