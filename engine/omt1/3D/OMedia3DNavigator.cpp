/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia3DNavigator.h"
#include "OMedia3DVector.h"
#include <stdio.h>

// 0x1001daf0
OMedia3DNavigator::OMedia3DNavigator()
{
	for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) m[r][c] = 0;
	m[0][0] = m[1][1] = m[2][2] = 1.0f;
	move_axis = omc3daxis_Z;
	move_speed = 0;
	updating_angle = false;
	move_changed = false;
	angle_changed = false;
}

OMedia3DNavigator::~OMedia3DNavigator() {}		// 0x1001db30

// 0x1001db40
void OMedia3DNavigator::set_move_on_axis(omt_3DAxis axis, float speed)
{
	move_axis = axis;
	move_speed = speed;
	move_changed = true;
}

// 0x1001db60: the axis vector of length 'speed' turned by the angles (rotation order z, x, y)
void OMedia3DNavigator::find_direction_vector(omt_3DAxis axis, float speed, short ax, short ay, short az, OMedia3DVector &v)
{
	if (axis == omc3daxis_X) v.set(speed, 0, 0);
	else if (axis == omc3daxis_Y) v.set(0, speed, 0);
	else if (axis == omc3daxis_Z) v.set(0, 0, speed);
	v.rotate(ax, ay, az);
}

// The navigator only runs for a viewport/element in navigator mode (+0x94 / +0xd0); sb2.exe never switches it on.
// TODO (log): 0x1001df70 update, 0x1001dc50 update_angles, 0x1001e1a0 set_angle's matrix; the x87 code of these
// (angle-from-matrix) is not rebuilt.
void OMedia3DNavigator::update(OMediaWorldPosition *, OMediaWorldAngle *, OMediaAutoMove *, OMediaAutoRotate *, OMediaTimer *)
{
	static bool once = false;
	if (!once) {once = true; printf("OMedia3DNavigator::update: TODO (navigator mode is not used by sb2.exe)\n");}
}
void OMedia3DNavigator::update_angles(OMediaWorldAngle *, OMediaAutoMove *) {printf("OMedia3DNavigator::update_angles: TODO\n");}
void OMedia3DNavigator::set_angle(short, short, short) {angle_changed = true; printf("OMedia3DNavigator::set_angle: TODO\n");}
