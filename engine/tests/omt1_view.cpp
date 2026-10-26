/*  omt1_view: desktop test tool for the OMT 1.x engine layer (engine/omt1).
    Lists a database's objects, or draws canvases / a 2D anim through a 2D world, a viewport,
    a double canvas and the display (the same path sb2.exe uses).

    usage: omt1_view <file.omt>                       list chunks (type, id, name, size; canvases: size, world)
           omt1_view <file.omt> canvas <name|#id>...  draw canvases side by side (wraps)
           omt1_view <file.omt> anim <name|#id>...    play the canvases as a 2D anim (10 fps), centred
           omt1_view <file.omt> all                   draw every canvas, tiled
    env:   OMT_SCREENSHOT=x.bmp OMT_FRAMES=n (saves frame n and quits), OMT_WINDOW_SIZE=WxH      */

#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaError.h"
#include "OMediaCanvas.h"
#include "OMediaPixmapWorld.h"
#include "OMediaDXSurfaceWorld.h"
#include "OMediaDisplay.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include "OMediaCanvasElement.h"
#include "OMedia2DAnim.h"
#include "OMediaBackground.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **omt_argv;		// OMediaSDLMain.cpp
extern int omt_argc;

// The chunk table is protected in OMT (no public listing): a subclass reads it.
class ListingDataBase : public OMediaDataBase
{
	public:
	ListingDataBase(OMediaStream *s) : OMediaDataBase(s) {}
	void list(void)
	{
		for (omt_ChunkTypeList::iterator t = chunk_type_list.begin(); t != chunk_type_list.end(); t++)
		{
			omt_ChunkType type = (*t).get_chunk_type();
			char tc[5] = { (char)(type >> 24), (char)(type >> 16), (char)(type >> 8), (char)type, 0 };
			omt_ChunkList *cl = (*t).get_chunk_list();
			printf("type '%s': %d chunks\n", tc, (int)cl->size());
			for (omt_ChunkList::iterator c = cl->begin(); c != cl->end(); c++)
			{
				printf("  %-4s %6d  %-24s %8u", tc, (int)(*c).ckid, (*c).name.c_str(), (unsigned)(*c).size);
				if (type == OMediaCanvas::db_type)
				{
					OMediaCanvas *cv = (OMediaCanvas*)get_object(type, (*c).ckid);
					OMediaGfxWorld *g = cv ? cv->getgfxworld() : NULL;
					OMediaRGBColor tcol;
					printf("  %4dx%-4d world attr %#lx%s", cv ? cv->getwidth() : 0, cv ? cv->getheight() : 0,
						   g ? g->getattributes() : 0UL, g && g->get_transparent_color(&tcol) ? " transparent" : "");
				}
				printf("\n");
			}
		}
	}
};

static OMediaCanvas *find_canvas(OMediaDataBase *db, const char *n)
{
	if (n[0] == '#') return (OMediaCanvas*)db->get_object(OMediaCanvas::db_type, atoi(n + 1));
	return (OMediaCanvas*)db->get_object(OMediaCanvas::db_type, string(n));
}

void omt_main(void)
{
	int argc = omt_argc;
	char **argv = omt_argv;
	if (argc < 2)
	{
		printf("usage: omt1_view <file.omt> [canvas|anim <name|#id>...|all]\n");
		return;
	}
	OMediaDataBase::set_cache_size(0);
	omd_REGISTERCLASS(OMediaCanvas);
	try
	{
		OMediaFilePath path(argv[1]);
		OMediaFileStream file;
		file.setpath(&path);
		file.open(omcfp_Read);
		ListingDataBase *db = new ListingDataBase(&file);
		if (argc < 3)
		{
			db->list();
			delete db;
			return;
		}

		// The display, as sb2.exe makes it: DirectX, 640x480x16, a double canvas, the display.
		OMediaDirectX::install(OMediaDirectX::omcdx_Video);
		OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
		OMediaVideoMode *mode = NULL;
		OMediaVideoDriver &drv = dev->get_video_drivers()->front();
		for (size_t i = 0; i < drv.modes.size(); i++)
			if (drv.modes[i].width == 640 && drv.modes[i].height == 480 && drv.modes[i].depth == 16) mode = &drv.modes[i];
		dev->setmode(mode, 0);
		OMediaDoubleCanvas *dc = new OMediaDoubleCanvas;
		OMediaDisplay *display = new OMediaDisplay(NULL, dc, "omt1_view");

		OMedia2DWorld *world = new OMedia2DWorld;
		OMediaRGBColor bg(0x2000, 0x3000, 0x5000);
		((OMediaColorBackground*)world->getbackground())->setcolor(&bg);
		OMediaViewPort *vp = new OMediaViewPort(world);
		OMediaRect b(0, 0, dc->getwidth(), dc->getheight());
		vp->setbounds(&b);
		vp->setcanvas(dc);

		string what = argv[2];
		if (what == "canvas" || what == "all")
		{
			vector<OMediaCanvas*> list;
			if (what == "all")
			{
				omt_DBIterator it;
				for (OMediaDBObject *o = db->get_first_object(OMediaCanvas::db_type, it); o; o = db->get_next_object(it))
					list.push_back((OMediaCanvas*)o);
			}
			else for (int i = 3; i < argc; i++)
			{
				OMediaCanvas *c = find_canvas(db, argv[i]);
				if (!c) printf("no canvas %s\n", argv[i]);
				else list.push_back(c);
			}
			long x = 0, y = 0, row = 0;
			for (size_t i = 0; i < list.size(); i++)
			{
				OMediaCanvas *c = list[i];
				if (x && x + c->getwidth() > b.right) { x = 0; y += row; row = 0; }
				OMediaCanvasElement *e = new OMediaCanvasElement(c);
				e->link(world);
				e->place((float)x, (float)y, (float)i);
				x += c->getwidth() + 2;
				if (c->getheight() + 2 > row) row = c->getheight() + 2;
			}
		}
		else if (what == "anim")
		{
			OMedia2DAnim *a = new OMedia2DAnim;
			long w = 0, h = 0;
			for (int i = 3; i < argc; i++)
			{
				OMediaCanvas *c = find_canvas(db, argv[i]);
				if (!c) { printf("no canvas %s\n", argv[i]); continue; }
				OMedia2DAnimFrame *f = new OMedia2DAnimFrame;
				f->setcanvas(c);
				f->setframespeed_tb(10);
				f->link(a, 0);
				if (c->getwidth() > w) w = c->getwidth();
				if (c->getheight() > h) h = c->getheight();
			}
			a->setplay_timebased(true);
			a->link(world);
			a->place((float)((b.right - w) / 2), (float)((b.bottom - h) / 2), 0);
		}

		bool quit = false;
		while (!quit)
		{
			SDL_Event ev;
			while (SDL_PollEvent(&ev))
				if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)) quit = true;
			world->update();
			SDL_Delay(2);
		}
		delete world;
		delete display;
		delete db;
	}
	catch (OMediaError &e)
	{
		printf("OMT error %d at %s:%d\n", (int)e.errorcode, e.file.c_str(), e.line);
	}
}
