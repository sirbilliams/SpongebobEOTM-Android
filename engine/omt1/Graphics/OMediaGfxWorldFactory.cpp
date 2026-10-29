/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaGfxWorldFactory.h"
#include "OMediaPixmapWorld.h"

OMediaGfxWorldFactory *OMediaGfxWorldFactory::factory = NULL;

// 0x10068de0. The memory worlds OMT.dll registers at start (the attributes are each class's
// static getattributes; DirectX adds OMediaDXSurfaceWorld at priority -10, OMediaDirectX::install).
// port: OMediaCompBlitterWorld (-65), OMediaMaskWorld (-127) and OMediaDeepMaskWorld (-127) are
// not rebuilt: no canvas of the game asks for them (its streams want 0x22d or 0x100c).
OMediaGfxWorldFactory::OMediaGfxWorldFactory()
{
	forced_builder = NULL;
	register_gfxworld(OMediaPixBlitterWorld::builder, 0x22f, -1);
	register_gfxworld(OMediaPixmapWorld::builder, 0x2ad, -2);
}

OMediaGfxWorldFactory::~OMediaGfxWorldFactory() {}		// 0x10068ed0

// 0x10068f10
OMediaGfxWorldFactory *OMediaGfxWorldFactory::getfactory(void)
{
	if (!factory) factory = new OMediaGfxWorldFactory;
	return factory;
}

// 0x10068f70
void OMediaGfxWorldFactory::setfactory(OMediaGfxWorldFactory *f)
{
	delete factory;
	factory = f;
}

// 0x10068f90. Texture requests (0x1000) go to create_texture with the mipmap mode the flags ask
// for; otherwise the first builder (by priority) whose attributes cover the request, retrying
// once without the alpha bit (0x40).
OMediaGfxWorld *OMediaGfxWorldFactory::createbestgfxworld(unsigned long attributes)
{
	if (attributes & 0x1000)
	{
		omt_MipmapMode saved = OMediaGfxWorld::mipmap_mode;
		if (attributes & 0x20000) OMediaGfxWorld::mipmap_mode = omcmm_None;
		else if (attributes & 0x8000) OMediaGfxWorld::mipmap_mode = omcmm_Mipmap;
		else if (attributes & 0x10000) OMediaGfxWorld::mipmap_mode = omcmm_Trilinear;
		OMediaGfxWorld *w = create_texture();
		OMediaGfxWorld::mipmap_mode = saved;
		return w;
	}
	if (forced_builder) return forced_builder();
	omt_GfxWorldBuilder b = find_bestgfxworld_builder(attributes);
	if (!b)
	{
		if (!(attributes & 0x40)) return NULL;
		b = find_bestgfxworld_builder(attributes & ~0x40UL);
		if (!b) return NULL;
	}
	return b();
}

// 0x10069020
omt_GfxWorldBuilder OMediaGfxWorldFactory::find_bestgfxworld_builder(unsigned long attributes)
{
	for (vector<OMediaGfxWorldBuilderEntry>::iterator i = builders.begin(); i != builders.end(); i++)
		if (((*i).attributes & attributes) == attributes) return (*i).builder;
	return NULL;
}

// 0x10069050. push_back, then a stable insertion sort by descending priority (the MSVC sort
// helpers inlined; with fewer than 17 entries it is a plain insertion sort).
void OMediaGfxWorldFactory::register_gfxworld(omt_GfxWorldBuilder builder, unsigned long attributes, short priority)
{
	OMediaGfxWorldBuilderEntry e;
	e.builder = builder;
	e.attributes = attributes;
	e.priority = priority;
	builders.push_back(e);
	for (size_t i = 1; i < builders.size(); i++)
	{
		OMediaGfxWorldBuilderEntry v = builders[i];
		size_t j = i;
		while (j > 0 && builders[j - 1].priority < v.priority) { builders[j] = builders[j - 1]; j--; }
		builders[j] = v;
	}
}

// 0x100693c0
void OMediaGfxWorldFactory::unregister_gfxworld(omt_GfxWorldBuilder builder)
{
	for (vector<OMediaGfxWorldBuilderEntry>::iterator i = builders.begin(); i != builders.end(); i++)
		if ((*i).builder == builder) { builders.erase(i); return; }
}

// 0x10069430. One builder per rasterizer engine (a previous one is replaced).
void OMediaGfxWorldFactory::register_texture(omt_GfxWorldBuilder builder, int engine)
{
	unregister_texture(engine);
	OMediaTextureBuilderEntry e;
	e.engine = engine;
	e.builder = builder;
	textures.push_back(e);
}

// 0x10069620
void OMediaGfxWorldFactory::unregister_texture(int engine)
{
	for (vector<OMediaTextureBuilderEntry>::iterator i = textures.begin(); i != textures.end(); i++)
		if ((*i).engine == engine) { textures.erase(i); return; }
}

// 0x10069680. The builder of the current rasterizer's engine (the global rasterizer at
// 0x103ce0a4; its descriptor's engine at +0x2c -> +4).
extern int omf_current_rasterizer_engine(void);		// OMedia3DRasterizerDesc.cpp; -1 when none
OMediaGfxWorld *OMediaGfxWorldFactory::create_texture(void)
{
	int engine = omf_current_rasterizer_engine();
	if (engine < 0) return NULL;
	for (vector<OMediaTextureBuilderEntry>::iterator i = textures.begin(); i != textures.end(); i++)
		if ((*i).engine == engine) return (*i).builder();
	return NULL;
}
