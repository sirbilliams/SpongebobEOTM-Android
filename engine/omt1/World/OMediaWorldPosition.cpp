/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaWorldPosition.h"

OMediaWorldPosition::OMediaWorldPosition() { z = 0; y = 0; x = 0; }
OMediaWorldPosition::~OMediaWorldPosition() {}
void OMediaWorldPosition::doplace(float nx, float ny, float nz) { x = nx; y = ny; z = nz; }
void OMediaWorldPosition::domove(float dx, float dy, float dz) { x = dx + x; y = dy + y; z = dz + z; }

OMediaWorldAngle::OMediaWorldAngle() { angley = 0; anglez = 0; anglex = 0; }

// 0x1039aa00
void OMediaWorldAngle::set_angle(short ax, short ay, short az)
{
	anglex = ax & 0x3fff;
	angley = ay & 0x3fff;
	anglez = az & 0x3fff;
}

// 0x100379d0
OMediaAutoMove::OMediaAutoMove()
{
	fb_z = fb_y = fb_x = 0;
	tb_z = tb_y = tb_x = 0;
	automove = false;
	timebased = false;		// port: OMT.dll leaves it unset (only read when automove is on)
}
OMediaAutoMove::~OMediaAutoMove() {}

// 0x10037a20
void OMediaAutoMove::set_automove_fb(float dx, float dy, float dz)
{
	if (dx == 0.0f && dy == 0.0f && dz == 0.0f) automove = false;
	else
	{
		automove = true;
		timebased = false;
	}
	fb_x = dx; fb_y = dy; fb_z = dz;
}

// 0x10037a80: units per second, kept per millisecond
void OMediaAutoMove::set_automove_tb(float dx, float dy, float dz)
{
	if (dx == 0.0f && dy == 0.0f && dz == 0.0f) automove = false;
	else
	{
		automove = true;
		timebased = true;
	}
	tb_x = dx * 0.001f; tb_y = dy * 0.001f; tb_z = dz * 0.001f;
}

// 0x10037af0. Time based: the world timer's elapsed milliseconds (timer +4) times the speed.
bool OMediaAutoMove::automove_update(float &dx, float &dy, float &dz, OMediaTimer *timer)
{
	if (!automove) return false;
	if (!timebased)
	{
		dx = fb_x; dy = fb_y; dz = fb_z;
	}
	else
	{
		float e = (float)timer->get_last_elapsed();
		dx = e * tb_x; dy = e * tb_y; dz = e * tb_z;
	}
	return !(dx == 0.0f && dy == 0.0f && dz == 0.0f);
}

// 0x10037bb0
OMediaAutoRotate::OMediaAutoRotate()
{
	fb_y = 0; fb_z = 0; fb_x = 0;
	tb_z = tb_y = tb_x = 0;
	autorotate = false;
	timebased = false;
}
OMediaAutoRotate::~OMediaAutoRotate() {}

// 0x10037c10
void OMediaAutoRotate::set_autorotate_fb(short ax, short ay, short az)
{
	if (ax == 0 && ay == 0 && az == 0)
	{
		fb_y = 0; autorotate = false; fb_x = 0; fb_z = 0;
		return;
	}
	fb_y = ay;
	autorotate = true;
	timebased = false;
	fb_x = ax;
	fb_z = az;
}

// 0x10037c60
void OMediaAutoRotate::set_autorotate_tb(float ax, float ay, float az)
{
	if (ax == 0.0f && ay == 0.0f && az == 0.0f) autorotate = false;
	else
	{
		autorotate = true;
		timebased = true;
	}
	tb_x = ax * 0.001f; tb_y = ay * 0.001f; tb_z = az * 0.001f;
}

// 0x10037cd0. The elapsed time is loaded as a 64-bit integer and each product truncated (_ftol).
bool OMediaAutoRotate::autorotate_update(short &ax, short &ay, short &az, OMediaTimer *timer)
{
	if (!autorotate) return false;
	if (!timebased)
	{
		ax = fb_x; ay = fb_y; az = fb_z;
	}
	else
	{
		double e = (double)timer->get_last_elapsed();		// port: x87 extended precision -> double
		ax = (short)(long)(e * tb_x);
		ay = (short)(long)(e * tb_y);
		az = (short)(long)(e * tb_z);
	}
	return !(ax == 0 && ay == 0 && az == 0);
}
