/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    Position, angle and automatic move/rotate mix-ins of elements and viewports.
    Angles are OMT angle units, 0..0x3fff for a full turn.                                      */
#pragma once
#ifndef OMEDIA1_WorldPosition_H
#define OMEDIA1_WorldPosition_H

#include "OMediaTypes.h"
#include "OMediaTimer.h"

class OMediaWorldPosition
{
	public:
	omtshared OMediaWorldPosition();				// 0x1039ac10
	omtshared virtual ~OMediaWorldPosition();		// 0x1039ac50

	inline void place(float nx, float ny, float nz) {doplace(nx, ny, nz);}
	inline void move(float dx, float dy, float dz) {domove(dx, dy, dz);}
	inline void placex(float nx) {doplace(nx, y, z);}
	inline void placey(float ny) {doplace(x, ny, z);}
	inline void placez(float nz) {doplace(x, y, nz);}
	inline float getx(void) const {return x;}
	inline float gety(void) const {return y;}
	inline float getz(void) const {return z;}

	protected:
	omtshared virtual void doplace(float nx, float ny, float nz);	// 0x1039ac60  vt+0x04
	omtshared virtual void domove(float dx, float dy, float dz);	// 0x1039ac80  vt+0x08

	float	x, y, z;		// +0x04 +0x08 +0x0c
};

class OMediaWorldAngle
{
	public:
	omtshared OMediaWorldAngle();					// 0x1039a9e0
	inline virtual ~OMediaWorldAngle() {}

	omtshared virtual void set_angle(short ax, short ay, short az);	// 0x1039aa00  vt+0x00 (masks to 0x3fff)

	inline short get_anglex(void) const {return anglex;}
	inline short get_angley(void) const {return angley;}
	inline short get_anglez(void) const {return anglez;}

	protected:
	short	anglex, angley, anglez;	// +0x04 +0x06 +0x08
};

class OMediaAutoMove
{
	public:
	omtshared OMediaAutoMove();						// 0x100379d0
	omtshared virtual ~OMediaAutoMove();			// 0x10037a10

	omtshared virtual void set_automove_fb(float dx, float dy, float dz);	// 0x10037a20  vt+0x04 per update
	omtshared virtual void set_automove_tb(float dx, float dy, float dz);	// 0x10037a80  vt+0x08 per second
	omtshared virtual bool automove_update(float &dx, float &dy, float &dz, OMediaTimer *timer);	// 0x10037af0  vt+0x0c

	inline bool get_automove(void) const {return automove;}

	protected:
	float	fb_x, fb_y, fb_z;		// +0x04
	float	tb_x, tb_y, tb_z;		// +0x10  per millisecond
	bool	automove;				// +0x1c
	bool	timebased;				// +0x1d
};

class OMediaAutoRotate
{
	public:
	omtshared OMediaAutoRotate();					// 0x10037bb0
	omtshared virtual ~OMediaAutoRotate();			// 0x10037c00

	omtshared virtual void set_autorotate_fb(short ax, short ay, short az);	// 0x10037c10  vt+0x04
	omtshared virtual void set_autorotate_tb(float ax, float ay, float az);	// 0x10037c60  vt+0x08
	omtshared virtual bool autorotate_update(short &ax, short &ay, short &az, OMediaTimer *timer);	// 0x10037cd0  vt+0x0c

	inline bool get_autorotate(void) const {return autorotate;}

	protected:
	short	fb_x, fb_y, fb_z;		// +0x04
	float	tb_x, tb_y, tb_z;		// +0x0c
	bool	autorotate;				// +0x18
	bool	timebased;				// +0x19
};

#endif
