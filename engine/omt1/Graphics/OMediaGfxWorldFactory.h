/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT.
    OMediaGfxWorldFactory: picks the world class for a set of attributes (canvas streams store
    the attributes they need). Builders are kept sorted by priority, highest first.             */
#pragma once
#ifndef OMEDIA1_GfxWorldFactory_H
#define OMEDIA1_GfxWorldFactory_H

#include "OMediaGfxWorld.h"
#include <vector>

typedef OMediaGfxWorld *(*omt_GfxWorldBuilder)(void);

struct OMediaGfxWorldBuilderEntry
{
	omt_GfxWorldBuilder	builder;		// +0
	unsigned long		attributes;		// +4
	short				priority;		// +8
};

struct OMediaTextureBuilderEntry
{
	int					engine;			// omt_3DRasterizerEngine
	omt_GfxWorldBuilder	builder;
};

class OMediaGfxWorldFactory
{
	public:

	omtshared OMediaGfxWorldFactory();				// 0x10068de0
	omtshared virtual ~OMediaGfxWorldFactory();		// 0x10068ed0

	omtshared virtual OMediaGfxWorld *createbestgfxworld(unsigned long attributes);		// 0x10068f90  vt+0x04
	omtshared virtual void register_gfxworld(omt_GfxWorldBuilder builder, unsigned long attributes,
											 short priority);								// 0x10069050  vt+0x08
	omtshared virtual void unregister_gfxworld(omt_GfxWorldBuilder builder);				// 0x100693c0  vt+0x0c
	omtshared virtual void register_texture(omt_GfxWorldBuilder builder, int engine);		// 0x10069430  vt+0x10
	omtshared virtual void unregister_texture(int engine);									// 0x10069620  vt+0x14
	omtshared virtual OMediaGfxWorld *create_texture(void);									// 0x10069680  vt+0x18

	omtshared omt_GfxWorldBuilder find_bestgfxworld_builder(unsigned long attributes);	// 0x10069020

	omtshared static OMediaGfxWorldFactory *getfactory(void);		// 0x10068f10
	omtshared static void setfactory(OMediaGfxWorldFactory *f);	// 0x10068f70

	inline void set_forced_builder(omt_GfxWorldBuilder b) {forced_builder = b;}

	protected:

	vector<OMediaGfxWorldBuilderEntry>	builders;		// +0x04
	vector<OMediaTextureBuilderEntry>	textures;		// +0x14
	omt_GfxWorldBuilder					forced_builder;	// +0x24  when set, createbestgfxworld uses it

	omtshared static OMediaGfxWorldFactory	*factory;
};

#endif
