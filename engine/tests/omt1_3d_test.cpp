/*  omt1_3d_test: the OMT 1.x 3D path with Granny characters over a scene's 2D background (engine/omt1/3D).
    usage: omt1_3d_test <game folder> [scene, default c1l0s1] [shot folder, default .shots]
    Loads <game>/scenes/<scene>.scn (scene loader) and <game>/sceneart/<scene>.omt (the "background" canvas), builds
    the scene's 3D world the way sb2.exe's scene does (0x4033f0: world, viewport at the scene camera, a viewport
    element in the 2D world, lights), puts every CHAR record's model at its position with its idle animation,
    renders headless frames 0, 30 and 60 and writes frame-1/31/61.bmp (frames 0/30/60) into the shot folder.                    */

#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaError.h"
#include "OMediaCanvas.h"
#include "OMediaDisplay.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include "OMediaVPElement.h"
#include "OMediaCanvasElement.h"
#include "OMediaBackground.h"
#include "OMediaGLBackend.h"
#include "OMediaPeriodical.h"
#include "OMedia3DWorld.h"
#include "OMedia3DRasterizerDesc.h"
#include "OMedia3DGrannyModel.h"
#include "../../game/src/scene/SceneData.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>

extern char **omt_argv;
extern int omt_argc;

static OMediaDataBase *open_db(const string &p, OMediaFileStream **keep)
{
	OMediaFilePath *path = new OMediaFilePath(p);
	OMediaFileStream *f = new OMediaFileStream;
	f->setpath(path);
	f->open(omcfp_Read);
	*keep = f;
	return new OMediaDataBase(f);
}

static std::string lower(std::string s) {for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]); return s;}

struct Actor
{
	OMedia3DShapeElement	*element;
	OMedia3DGrannyModel		*model;
};

void omt_main(void)
{
	if (omt_argc < 2)
	{
		printf("usage: omt1_3d_test <game folder> [scene] [shot folder]\n");
		return;
	}
	const std::string folder = omt_argv[1];
	const std::string scene_name = omt_argc > 2 ? omt_argv[2] : "c1l0s1";
	const std::string shots = omt_argc > 3 ? omt_argv[3] : ".shots";
	mkdir(shots.c_str(), 0755);
	// the engine's own screenshot hook (OMediaSDLMain.cpp): the presented window, BMP, frame numbers = presents (the first is 1)
	setenv("OMT_SCREENSHOT", (shots + "/frame.bmp").c_str(), 0);
	setenv("OMT_SHOTS", "1,31,61", 0);
	setenv("OMT_FRAMES", "100000", 0);

	sb::scene::Scene scene;
	std::string err;
	if (!sb::scene::load(folder + "/scenes/" + scene_name + ".scn", scene, err))
	{
		printf("scene %s: %s\n", scene_name.c_str(), err.c_str());
		return;
	}
	OMediaDataBase::set_cache_size(0);
	omd_REGISTERCLASS(OMediaCanvas);
	omd_REGISTERCLASS(OMedia3DMaterial);
	try
	{
		OMediaFileStream *f1;
		OMediaDataBase *art = open_db(folder + "/sceneart/" + scene_name + ".omt", &f1);

		OMediaDirectX::install(OMediaDirectX::omcdx_Video);
		OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
		OMediaVideoMode *mode = NULL;
		OMediaVideoDriver &drv = dev->get_video_drivers()->front();
		for (size_t i = 0; i < drv.modes.size(); i++)
			if (drv.modes[i].width == 640 && drv.modes[i].height == 480 && drv.modes[i].depth == 16) mode = &drv.modes[i];
		dev->setmode(mode, 0);
		OMediaDoubleCanvas *dc = new OMediaDoubleCanvas;
		OMediaDisplay *display = new OMediaDisplay(NULL, dc, "omt1_3d_test");

		// 2D world: the scene's background
		OMedia2DWorld *world = new OMedia2DWorld;
		world->set_framerate_mode(omfrc_Max);
		OMediaRGBColor bg(0, 0, 0);
		((OMediaColorBackground*)world->getbackground())->setcolor(&bg);
		OMediaViewPort *vp = new OMediaViewPort(world);
		OMediaRect b(0, 0, dc->getwidth(), dc->getheight());
		vp->setbounds(&b);
		vp->setcanvas(dc);
		OMediaCanvas *back = (OMediaCanvas*)art->get_object(OMediaCanvas::db_type, string("background"));
		if (back)
		{
			OMediaCanvasElement *be = new OMediaCanvasElement(back);
			be->link(world);
			be->place(0, 0, 0);
		}

		// the rasterizer (sb2.exe 0x40ceb0 InitRasterizer: the first accelerated one of the screen device)
		OMedia3DAbstractRasterizer *ras = NULL;
		std::vector<OMedia3DRasterizerDesc*> *rl = dev->get_rasterizers();
		for (size_t i = 0; i < rl->size() && !ras; i++)
			if (((*rl)[i]->attributes & omcrda_Accelerated)) ras = (*rl)[i]->build(NULL, 0);
		if (!ras) ras = OMedia3DRasterizerManager::get_default()->build_rasterizer(NULL, NULL, 0);
		ras->set_anti_aliasing(3);
		ras->set_texture_filtering(3);

		// 3D world, as sb2.exe 0x4033f0 makes it
		sb::scene::Camera &cam = scene.camera.data.camera;
		OMedia3DWorld *w3 = new OMedia3DWorld;
		w3->set_framerate_mode(omfrc_Max);
		w3->setbackground(NULL);
		w3->focal = cam.o0188;
		w3->far_z = 4500.0f;
		w3->near_z = 10.0f;
		w3->ambient.setred8bits((unsigned char)cam.o018c);
		w3->ambient.setgreen8bits((unsigned char)cam.o018e);
		w3->ambient.setblue8bits((unsigned char)cam.o0190);
		w3->rasterizer = ras;
		OMediaViewPort *vp3 = new OMediaViewPort(w3);
		OMediaRect b3(0, 0, 640, 480);
		vp3->setbounds(&b3);
		vp3->place(cam.o0174, cam.o0178, cam.o017c);
		vp3->set_angle((short)((cam.o0180 * 0x4000) / 360), (short)((cam.o0182 * 0x4000) / 360), (short)((cam.o0184 * 0x4000) / 360));
		OMediaVPElement *vpe = new OMediaVPElement;
		vpe->set_viewport(vp3);
		vpe->link(world);
		vpe->place(0, 0, 100);
		printf("camera: pos %g %g %g angle %u %u %u focal %g\n", cam.o0174, cam.o0178, cam.o017c, cam.o0180, cam.o0182, cam.o0184, cam.o0188);

		// the scene's lights (LITE records; the type-0 ones are switched off)
		for (size_t i = 0; i < scene.tiles.size(); i++)
		{
			sb::scene::Light &l = scene.tiles[i].data.light;
			if (!l.o0180) continue;
			OMedia3DLight *light = new OMedia3DLight;
			light->link(w3);
			light->kind = 2;
			light->range = l.o0184;
			light->falloff = l.o0188;
			light->diffuse[0] = l.o018c; light->diffuse[1] = l.o0190; light->diffuse[2] = l.o0194;
			light->place(l.o0174, l.o0178, l.o017c);
			printf("light %lu: at %g %g %g range %g\n", (unsigned long)i, l.o0174, l.o0178, l.o017c, l.o0184);
		}

		// the characters
		OMedia3DGrannyModel::init(folder);
		std::vector<Actor> actors;
		for (size_t i = 0; i < scene.elements.size(); i++)
		{
			if (scene.elements[i].tag != sb::scene::CharacterTag) continue;
			sb::scene::Character &c = scene.elements[i].data.character;
			std::string name = lower(c.o01e4.text());
			Actor a;
			a.model = new OMedia3DGrannyModel;
			std::string base = folder + "/models/people/" + name + "_model";
			if (!a.model->load(base)) {printf("character %s: %s\n", name.c_str(), a.model->get_error().c_str()); delete a.model; continue;}
			if (!a.model->play(folder + "/models/people/" + name + "_idle1.grn", 1.0f, 9999, 0.0f))
				printf("character %s: %s\n", name.c_str(), a.model->get_error().c_str());
			a.element = new OMedia3DShapeElement;
			a.element->setshape(a.model->get_shape());
			a.element->link(w3);
			a.element->place(c.o0230, c.o0234, c.o0238);
			short heading = (short)(c.o0240 & 0x3fff);
			a.element->set_angle(0, heading, 0);
			printf("character %s at %g %g %g heading %d\n", name.c_str(), c.o0230, c.o0234, c.o0238, (int)heading);
			actors.push_back(a);
		}

		for (int frame = 0; frame <= 60; frame++)
		{
			SDL_Event ev;
			while (SDL_PollEvent(&ev)) {}
			OMedia3DGrannyModel::advance_time(frame ? 1.0 / 30.0 : 0.0);
			for (size_t i = 0; i < actors.size(); i++) actors[i].model->update_pose();
			w3->update();
			world->update();
		}
		for (size_t i = 0; i < actors.size(); i++) delete actors[i].element;
		for (size_t i = 0; i < actors.size(); i++) delete actors[i].model;
		delete w3;
		delete world;
		delete ras;
		delete display;
	}
	catch (OMediaError &e)
	{
		printf("OMT error %d at %s:%d\n", (int)e.errorcode, e.file.c_str(), e.line);
	}
}
