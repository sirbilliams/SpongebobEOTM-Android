/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMedia3DRasterizerDesc: one rasterizer the screen device offers (0x20 bytes in OMT.dll),
    built through the default OMedia3DRasterizerManager.                                       */
#pragma once
#ifndef OMEDIA1_3DRasterizerDesc_H
#define OMEDIA1_3DRasterizerDesc_H

#include "OMediaTypes.h"
#include <string>

class OMediaGfxWorld;
class OMedia3DAbstractRasterizer;

enum omt_3DRasterizerEngine { omcre_None = 0, omcre_OMT = 1, omcre_Direct3D = 2 };

// attributes (+0x18): bit 0 = accelerated (the game takes the first one unless -sw)
enum { omcrda_Accelerated = 1 };

class OMedia3DRasterizerDesc
{
	public:
	omt_3DRasterizerEngine	engine;			// +0x04
	string					name;			// +0x08 (the game logs +0x0c, the string's data)
	unsigned long			attributes;		// +0x18

	inline OMedia3DRasterizerDesc() {engine = omcre_None; attributes = 0;}

	omtshared OMedia3DAbstractRasterizer *build(OMediaGfxWorld *target, unsigned long flags);	// 0x1001f380
};

class OMedia3DRasterizerManager
{
	public:
	omtshared OMedia3DRasterizerManager();
	omtshared virtual ~OMedia3DRasterizerManager();
	omtshared virtual OMedia3DAbstractRasterizer *build_rasterizer(OMediaGfxWorld *target,
																	OMedia3DRasterizerDesc *desc,
																	unsigned long flags);	// 0x10396de0  vt+0x04
	omtshared static OMedia3DRasterizerManager *get_default(void);		// 0x1001f3e0
	omtshared static void set_default(OMedia3DRasterizerManager *m);	// 0x1001f410
	protected:
	omtshared static OMedia3DRasterizerManager *default_manager;
};

#endif
