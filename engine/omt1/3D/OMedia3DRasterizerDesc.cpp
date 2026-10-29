/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia3DRasterizerDesc.h"
#include "OMedia3DGLRasterizer.h"

OMedia3DRasterizerManager *OMedia3DRasterizerManager::default_manager = NULL;
static int omv_current_engine = -1;		// the engine of the last rasterizer built (OMT.dll: the global at 0x103ce0a4)

// 0x1001f380
OMedia3DAbstractRasterizer *OMedia3DRasterizerDesc::build(OMediaGfxWorld *target, unsigned long flags)
{
	return OMedia3DRasterizerManager::get_default()->build_rasterizer(target, this, flags);
}

OMedia3DRasterizerManager::OMedia3DRasterizerManager() {}
OMedia3DRasterizerManager::~OMedia3DRasterizerManager() {}

// 0x10396de0: engine 1 builds an OMediaOMTRasterizer (DirectX's manager adds Direct3D).
// port: both are the one GL rasterizer (the target is the GL surface of 'target').
OMedia3DAbstractRasterizer *OMedia3DRasterizerManager::build_rasterizer(OMediaGfxWorld *, OMedia3DRasterizerDesc *, unsigned long)
{
	omv_current_engine = omcre_OMT;
	return new OMedia3DGLRasterizer;
}

// 0x1001f3e0
OMedia3DRasterizerManager *OMedia3DRasterizerManager::get_default(void)
{
	if (!default_manager) default_manager = new OMedia3DRasterizerManager;
	return default_manager;
}

// 0x1001f410
void OMedia3DRasterizerManager::set_default(OMedia3DRasterizerManager *m)
{
	delete default_manager;
	default_manager = m;
}

// the engine of the current rasterizer (OMT.dll: the global at 0x103ce0a4); -1: none yet
int omf_current_rasterizer_engine(void) { return omv_current_engine; }
