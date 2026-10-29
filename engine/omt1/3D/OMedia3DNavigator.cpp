/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia3DNavigator.h"

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

// phase 2 (3D layer): 0x1001df70 update, 0x1001e1a0 set_angle, 0x1001db60 find_direction_vector,
// 0x1001dc50 update_angles. Only viewports in navigator mode (+0x94) reach them; the 2D layer does not.
void OMedia3DNavigator::update(OMediaWorldPosition *, OMediaWorldAngle *, OMediaAutoMove *, OMediaAutoRotate *, OMediaTimer *) {}
void OMedia3DNavigator::set_angle(short, short, short) { angle_changed = true; }
void OMedia3DNavigator::find_direction_vector(omt_3DAxis, float, short, short, short, OMedia3DVector &) {}
void OMedia3DNavigator::update_angles(OMediaWorldAngle *, OMediaAutoMove *) {}
