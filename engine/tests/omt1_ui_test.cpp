/*  omt1_ui_test: the OMT 1.x UI classes (engine/omt1) on the game's own files, headless.
    Draws a 2D world on the display path sb2.exe uses: a scene background (sceneart/*.omt), boxes,
    captions and a string field in a font made of Font.omt's glyph canvases, a viewport element
    showing a second world, then clicks the field and types into it through the event manager and
    the click dispatcher.
    usage: omt1_ui_test <game folder>      env: OMT_SCREENSHOT=x.bmp OMT_FRAMES=n OMT_WINDOW_SIZE=640x480 */

#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaError.h"
#include "OMediaCanvas.h"
#include "OMediaDXSurfaceWorld.h"
#include "OMediaDisplay.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include "OMediaCanvasElement.h"
#include "OMediaBoxElement.h"
#include "OMediaCCaption.h"
#include "OMediaCStringField.h"
#include "OMediaVPElement.h"
#include "OMediaWorldClickDispatcher.h"
#include "OMediaEventManager.h"
#include "OMediaTextFont.h"
#include "OMediaQuickText.h"
#include "OMediaDrawPort.h"
#include "OMediaBackground.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **omt_argv;
extern int omt_argc;

// Font.omt keeps one canvas per glyph: digits "0".."9", capitals "lA".., small letters "sa"..,
// and a few signs. This is the kind of font class sb2.exe has of its own.
class GlyphFont : public OMediaTextFont
{
	public:
	GlyphFont(OMediaDataBase *db) : database(db) {}

	OMediaCanvas *glyph(char c)
	{
		char n[3] = {0, 0, 0};
		if (c >= '0' && c <= '9') n[0] = c;
		else if (c >= 'A' && c <= 'Z') { n[0] = 'l'; n[1] = c; }
		else if (c >= 'a' && c <= 'z') { n[0] = 's'; n[1] = c; }
		else if (c == '!') { n[0] = 'e'; n[1] = 'x'; }
		else if (c == '\'') { n[0] = 'a'; n[1] = 'p'; }
		else if (c == ',') { n[0] = 'c'; n[1] = 'o'; }
		else if (c == '-') { n[0] = 'd'; n[1] = 'a'; }
		else if (c == '.') { n[0] = 'd'; n[1] = 'o'; }
		else if (c == '?') { n[0] = 'q'; n[1] = 'u'; }
		else return NULL;
		return (OMediaCanvas*)database->get_object(OMediaCanvas::db_type, string(n));
	}
	short advance(char c)
	{
		OMediaCanvas *g = glyph(c);
		return g ? (short)(g->getwidth() + 1) : (short)8;
	}
	virtual short get_font_height(void) {return 18;}
	virtual short get_text_length(string t)
	{
		short w = 0;
		for (size_t i = 0; i < t.size(); i++) w = (short)(w + advance(t[i]));
		return w;
	}
	virtual void draw_full(string t, OMediaGfxWorld *dest, short x, short y)
	{
		OMediaRect clip(0, 0, get_text_length(t), 18);
		draw(t, dest, &clip, x, y);
	}
	virtual void draw(string t, OMediaGfxWorld *dest, OMediaRect *clip, short x, short y)
	{
		long tx = x - clip->left, ty = y - clip->top;
		long cl = clip->left + tx, ct = clip->top + ty, cr = clip->right + tx, cb = clip->bottom + ty;
		long px = tx;
		for (size_t i = 0; i < t.size(); i++)
		{
			OMediaCanvas *g = glyph(t[i]);
			if (g)
			{
				long cx0 = px, cx1 = px + g->getwidth(), cy0 = ty, cy1 = ty + g->getheight();
				long ix0 = cl > cx0 ? cl : cx0, iy0 = ct > cy0 ? ct : cy0;
				long ix1 = cr < cx1 ? cr : cx1, iy1 = cb < cy1 ? cb : cy1;
				if (ix0 < ix1 && iy0 < iy1)
				{
					OMediaRect src(ix0 - cx0, iy0 - cy0, ix1 - cx0, iy1 - cy0);
					g->draw(dest, &src, (short)ix0, (short)iy0, NULL);
				}
			}
			px += advance(t[i]);
		}
	}
	protected:
	OMediaDataBase	*database;
};

// Records what the click dispatcher sends
class ClickLog : public OMediaListener
{
	public:
	int downs, ups;
	ClickLog() {downs = ups = 0;}
	virtual void listen_to_message(omt_Message msg, void *param)
	{
		OMediaWorldClickInfo *ci = (OMediaWorldClickInfo*)param;
		if (msg == omsg_ViewPortClicked) downs++;
		else if (msg == omsg_ViewPortClicked_Up) ups++;
		printf("click message %d: type %d element %p at %d,%d right %d\n", (int)msg, ci->type, (void*)ci->element,
			   ci->x, ci->y, ci->right_button);
	}
};

static OMediaDataBase *open_db(const char *folder, const char *name, OMediaFileStream **keep)
{
	string p = string(folder) + "/" + name;
	OMediaFilePath *path = new OMediaFilePath(p);
	OMediaFileStream *f = new OMediaFileStream;
	f->setpath(path);
	f->open(omcfp_Read);
	*keep = f;
	return new OMediaDataBase(f);
}

static OMediaCanvas *canvas_named(OMediaDataBase *db, const char *n)
{
	OMediaCanvas *c = (OMediaCanvas*)db->get_object(OMediaCanvas::db_type, string(n));
	if (!c) printf("no canvas %s\n", n);
	return c;
}

static void send_mouse(OMediaEventManager *em, int type, int x, int y, int button)
{
	SDL_Event e;
	memset(&e, 0, sizeof(e));
	e.type = type;
	e.button.button = button;
	e.button.x = x;
	e.button.y = y;
	em->handle_sdl_event(&e);
}

static void send_text(OMediaEventManager *em, const char *s)
{
	for (; *s; s++)
	{
		SDL_Event e;
		memset(&e, 0, sizeof(e));
		e.type = SDL_TEXTINPUT;
		e.text.text[0] = *s;
		em->handle_sdl_event(&e);
	}
}

static void send_key(OMediaEventManager *em, SDL_Keycode k)
{
	SDL_Event e;
	memset(&e, 0, sizeof(e));
	e.type = SDL_KEYDOWN;
	e.key.keysym.sym = k;
	em->handle_sdl_event(&e);
}

void omt_main(void)
{
	if (omt_argc < 2)
	{
		printf("usage: omt1_ui_test <game folder>\n");
		return;
	}
	const char *folder = omt_argv[1];
	OMediaDataBase::set_cache_size(0);
	omd_REGISTERCLASS(OMediaCanvas);
	try
	{
		OMediaFileStream *f1, *f2, *f3;
		OMediaDataBase *sb2 = open_db(folder, "sb2.omt", &f1);
		OMediaDataBase *fontdb = open_db(folder, "Font.omt", &f2);
		OMediaDataBase *scene = open_db(folder, "sceneart/c1l0s1.omt", &f3);

		OMediaDirectX::install(OMediaDirectX::omcdx_Video);
		OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
		OMediaVideoMode *mode = NULL;
		OMediaVideoDriver &drv = dev->get_video_drivers()->front();
		for (size_t i = 0; i < drv.modes.size(); i++)
			if (drv.modes[i].width == 640 && drv.modes[i].height == 480 && drv.modes[i].depth == 16) mode = &drv.modes[i];
		dev->setmode(mode, 0);
		OMediaDoubleCanvas *dc = new OMediaDoubleCanvas;
		OMediaDisplay *display = new OMediaDisplay(NULL, dc, "omt1_ui_test");

		OMediaEventManager *em = new OMediaEventManager;
		OMediaWorldClickDispatcher *disp = new OMediaWorldClickDispatcher;
		ClickLog *log = new ClickLog;
		disp->get_click_broadcaster()->addlistener(log);

		OMedia2DWorld *world = new OMedia2DWorld;
		disp->get_pick_broadcaster()->addlistener(world);
		OMediaRGBColor bg(0, 0, 0);
		((OMediaColorBackground*)world->getbackground())->setcolor(&bg);
		OMediaViewPort *vp = new OMediaViewPort(world);
		OMediaRect b(0, 0, dc->getwidth(), dc->getheight());
		vp->setbounds(&b);
		vp->setcanvas(dc);

		GlyphFont *font = new GlyphFont(fontdb);

		// the scene's background, and the menu button art from sb2.omt
		OMediaCanvasElement *back = new OMediaCanvasElement(canvas_named(scene, "background"));
		back->link(world);
		back->place(0, 0, 0);
		OMediaCanvasElement *menu = new OMediaCanvasElement(canvas_named(sb2, "menu"));
		menu->link(world);
		menu->place(560, 20, 1);

		// boxes: filled flat, filled bevelled, a bevelled frame, a flat frame
		OMediaBoxElement *box;
		box = new OMediaBoxElement; box->set_size(120, 40); box->set_color(OMediaRGBColor(0x2000, 0x4000, 0xa000));
		box->link(world); box->place(20, 20, 2);
		box = new OMediaBoxElement; box->set_size(120, 40); box->set_3d(true);
		box->link(world); box->place(160, 20, 2);
		box = new OMediaBoxElement; box->set_size(120, 40); box->set_3d(true); box->set_filled(false);
		box->link(world); box->place(300, 20, 2);
		box = new OMediaBoxElement; box->set_size(120, 40); box->set_filled(false); box->set_color(OMediaRGBColor(0xffff, 0xffff, 0));
		box->link(world); box->place(440, 20, 2);
		box = new OMediaBoxElement; box->set_size(120, 40); box->set_3d(true); box->set_sunken(true);
		box->link(world); box->place(20, 80, 2);

		// captions
		OMediaCCaption *cap = new OMediaCCaption;
		cap->set_font(font);
		cap->set_string("Employee of the Month!");
		cap->link(world);
		cap->place(160, 90, 3);
		long cw, ch;
		cap->getsize(cw, ch);
		printf("caption size %ld x %ld\n", cw, ch);
		OMediaCCaption *cap2 = new OMediaCCaption;
		cap2->set_font(font);
		cap2->set_string("Name:");
		cap2->link(world);
		cap2->place(20, 150, 3);

		// the string field, set up the way sb2.exe does (0x414340)
		OMediaCStringField *field = new OMediaCStringField;
		field->set_font(font);
		field->set_margin(1);
		field->set_transparent_idle(false);
		field->set_backdrop(false);
		field->set_auto_size(false);
		field->set_max_width(0x96);
		field->recalc_size();
		field->set_dimensions(0x96, 0x14);
		field->set_filter(4);
		field->link(world);
		field->set_string("Spongebob");
		field->place(90, 150, 4);
		// a second one with a visible box, autosizing, letters and digits only
		OMediaCStringField *field2 = new OMediaCStringField;
		field2->set_font(font);
		field2->set_filter(omcsff_AlphaNumeric);
		field2->set_dimensions(80, 22);
		field2->link(world);
		field2->place(90, 190, 4);
		field2->set_string("abc");

		// a viewport element showing a second world (a small scene)
		OMedia2DWorld *world2 = new OMedia2DWorld;
		OMediaRGBColor bg2(0x6000, 0x1000, 0x1000);
		((OMediaColorBackground*)world2->getbackground())->setcolor(&bg2);
		OMediaViewPort *vp2 = new OMediaViewPort(world2);
		OMediaRect b2(0, 0, 160, 100);
		vp2->setbounds(&b2);
		OMediaCanvasElement *inner = new OMediaCanvasElement(canvas_named(sb2, "bubble1"));
		inner->link(world2);
		inner->place(20, 10, 0);
		OMediaCCaption *icap = new OMediaCCaption;
		icap->set_font(font);
		icap->set_string("Inside");
		icap->link(world2);
		icap->place(60, 40, 1);
		OMediaVPElement *vpe = new OMediaVPElement;
		vpe->set_viewport(vp2);
		vpe->link(world);
		vpe->place(400, 150, 5);
		long vw, vh;
		vpe->getsize(vw, vh);
		printf("vp element size %ld x %ld\n", vw, vh);

		// a QuickText check: text length of a made-up fixed font
		OMediaQuickText qt;
		printf("quicktext length (no font) %d\n", (int)qt.get_text_length("abc"));

		const char *shot_env = getenv("OMT_SCREENSHOT");
		(void)shot_env;
		int frame = 0;
		bool quit = false;
		while (!quit)
		{
			SDL_Event ev;
			while (SDL_PollEvent(&ev))
			{
				if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)) quit = true;
				else em->handle_sdl_event(&ev);
			}
			frame++;
			if (frame == 3)		// click the first field: it takes the focus
			{
				send_mouse(em, SDL_MOUSEBUTTONDOWN, 100, 156, SDL_BUTTON_LEFT);
				send_mouse(em, SDL_MOUSEBUTTONUP, 100, 156, SDL_BUTTON_LEFT);
				printf("frame 3: focus %s\n", field->has_focus() ? "first field" : "none");
			}
			if (frame == 5)
			{
				send_key(em, SDLK_BACKSPACE);
				send_text(em, "XY 2");
				printf("frame 5: text '%s'\n", field->get_string().c_str());
			}
			if (frame == 7)		// click the second one: the first lets go, the second keys filter
			{
				send_mouse(em, SDL_MOUSEBUTTONDOWN, 100, 195, SDL_BUTTON_LEFT);
				send_mouse(em, SDL_MOUSEBUTTONUP, 100, 195, SDL_BUTTON_LEFT);
				send_text(em, "d-e f!9");
				printf("frame 7: first %s, second %s '%s' (%d wide)\n", field->has_focus() ? "focus" : "no focus",
					   field2->has_focus() ? "focus" : "no focus", field2->get_string().c_str(), (int)(field2->get_string().size()));
			}
			if (frame == 9 && getenv("UI_BLUR")) { send_mouse(em, SDL_MOUSEBUTTONDOWN, 5, 300, SDL_BUTTON_LEFT); printf("frame 9: second %s\n", field2->has_focus() ? "focus" : "no focus"); }
			OMediaPeriodical::devote_time();
			world->update();
			SDL_Delay(2);
		}
		printf("clicks: %d down, %d up\n", log->downs, log->ups);
		delete world;
		delete display;
	}
	catch (OMediaError &e)
	{
		printf("OMT error %d at %s:%d\n", (int)e.errorcode, e.file.c_str(), e.line);
	}
}
