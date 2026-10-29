// port: the Asset Viewer (not in the original game). SB_ASSET_VIEW=models | scene[:<name>]; see docs/asset-viewer.md.
//
// models: every Granny model of the player's models/ folder by category (characters, props, items), orbit / zoom / pan, the model's
//         animations played through the game's own Granny wrapper (OMedia3DGrannyModel, the same skinning and pose copy as a scene).
// scene:  one scene loaded by the game's own Game / GameScene / World3D, then frozen (no scene update, no scripts, no saves), with a
//         pan / zoom window over its picture and the captions of its hotspots.
// Both are drawn with the game's font and simple panels; input is mouse, touch (the event manager turns fingers into the mouse) and the
// gamepad (the right stick is the pointer with A as the button, as in the game; the other buttons are read here).
#include "AssetView.h"
#include "../ui/GameFont.h"
#include "../game/Game.h"
#include "../game/World3D.h"
#include "../game/Scene.h"
#include "OMediaDisplay.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include "OMediaVPElement.h"
#include "OMediaBackground.h"
#include "OMediaBoxElement.h"
#include "OMediaEventManager.h"
#include "OMediaGLBackend.h"
#include "OMediaError.h"
#include "OMedia3DWorld.h"
#include "OMedia3DRasterizerDesc.h"
#include "OMedia3DGrannyModel.h"
#include "OMedia3DVector.h"
#include <SDL.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <algorithm>
#include <string>
#include <vector>

namespace sbport {
namespace {

const int Z_PANEL = 20000, Z_TEXT = 21000;		// above everything a scene draws (post-render objects sit at 10000)

// The game's rasterizer (World3D::rasterizer, sb2.exe 0x40ceb0): the first accelerated one of the screen device, made once.
OMedia3DAbstractRasterizer *rasterizer()
{
	static OMedia3DAbstractRasterizer *ras = 0;
	if (ras) return ras;
	OMediaScreenDevice *dev = OMediaScreenDevice::getdefaultdevice();
	std::vector<OMedia3DRasterizerDesc*> *rl = dev ? dev->get_rasterizers() : 0;
	for (size_t i = 0; rl && i < rl->size() && !ras; i++)
		if (((*rl)[i]->attributes & omcrda_Accelerated)) ras = (*rl)[i]->build(NULL, 0);
	if (!ras) ras = OMedia3DRasterizerManager::get_default()->build_rasterizer(NULL, NULL, 0);
	if (ras) { ras->set_anti_aliasing(3); ras->set_texture_filtering(3); }
	return ras;
}

std::string lower(const std::string &s)
{
	std::string r = s;
	for (size_t i = 0; i < r.size(); i++) r[i] = (char)tolower((unsigned char)r[i]);
	return r;
}
bool endsWith(const std::string &s, const char *suffix)
{
	size_t n = strlen(suffix);
	return s.size() >= n && !strcasecmp(s.c_str() + s.size() - n, suffix);
}
std::string num(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); return b; }
bool ciLess(const std::string &a, const std::string &b) { return strcasecmp(a.c_str(), b.c_str()) < 0; }

// ------------------------------------------------------------------------------------------------------ the panels and text
// Boxes are pooled (hidden when not used in a frame); text goes through the game font's own element pool.
class Ui
{
	public:
	Ui(OMedia2DWorld *w, GameFont *f) : world(w), font(f), used(0) {}
	void begin() { font->begin_frame(); used = 0; }
	void end() { for (size_t i = used; i < boxes.size(); i++) if (!boxes[i]->gethide()) boxes[i]->hide(true); }
	void box(int x, int y, int w, int h, const OMediaRGBColor &c, int z)
	{
		if (w <= 0 || h <= 0) return;
		if (used == boxes.size())
		{
			OMediaBoxElement *b = new OMediaBoxElement;
			b->set_filled(true);
			b->link(world);
			boxes.push_back(b);
		}
		OMediaBoxElement *b = boxes[used++];
		if (b->gethide()) b->hide(false);		// hiding is counted
		b->set_size((short)w, (short)h);
		b->set_color(c);
		b->place((float)x, (float)y, (float)z);
	}
	// the game's blue panel with a gold edge (the dialogue box colours)
	void panel(int x, int y, int w, int h, int state)	// 0 normal, 1 lit (pointer over / pressed), 2 disabled
	{
		OMediaRGBColor edge(state == 2 ? 0x6000 : 0xe800, state == 2 ? 0x5000 : 0xb000, state == 2 ? 0x3000 : 0x4200);
		OMediaRGBColor fill(state == 1 ? 0x3000 : 0x1800, state == 1 ? 0x5800 : 0x2800, state == 1 ? 0xb000 : 0x7000);
		box(x, y, w, h, edge, Z_PANEL);
		box(x + 1, y + 1, w - 2, h - 2, fill, Z_PANEL + 1);
	}
	void frame(int x, int y, int w, int h, int t, const OMediaRGBColor &c)
	{
		box(x, y, w, t, c, Z_PANEL + 2); box(x, y + h - t, w, t, c, Z_PANEL + 2);
		box(x, y, t, h, c, Z_PANEL + 2); box(x + w - t, y, t, h, c, Z_PANEL + 2);
	}
	int width(const std::string &s) { return font->get_text_length(s); }
	void text(int x, int y, const std::string &s, short variant = 0)
	{
		std::string t;
		for (size_t i = 0; i < s.size(); i++) if (s[i] != '*') t += s[i];		// '*' is the font's emphasis switch
		font->draw_world(world, x, y, Z_TEXT, variant, t);
	}
	void ctext(int cx, int y, const std::string &s, short variant = 0) { text(cx - width(s) / 2, y, s, variant); }
	OMedia2DWorld *world;
	GameFont *font;
	std::vector<OMediaBoxElement*> boxes;
	size_t used;
};

struct Btn
{
	int x, y, w, h, id;
	std::string label;
	bool on, disabled;
	bool hit(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

// A row of buttons from x, left to right, in the bar at y.
void addBtn(Ui &ui, std::vector<Btn> &v, int &x, int y, int id, const std::string &label, bool on = false)
{
	Btn b; b.x = x; b.y = y; b.w = ui.width(label) + 14; b.h = 21; b.id = id; b.label = label; b.on = on; b.disabled = false;
	v.push_back(b);
	x += b.w + 5;
}

// ------------------------------------------------------------------------------------------------------ input
struct Input
{
	Input() : x(320), y(240), have(false), down(false), pressed(false), released(false), alt(false), dx(0), dy(0), wheel(0), leave(false) {}
	int x, y;
	bool have, down, pressed, released, alt;
	int dx, dy, wheel;
	bool leave;
	std::vector<SDL_Keycode> keys;
	bool key(SDL_Keycode k) const { return std::find(keys.begin(), keys.end(), k) != keys.end(); }
	void pump(OMediaEventManager *em)
	{
		pressed = released = false; dx = dy = wheel = 0; keys.clear();
		SDL_Event ev;
		while (OMediaEventManager::poll(&ev))
		{
			em->handle_sdl_event(&ev);
			int lx, ly;
			switch (ev.type)
			{
				case SDL_QUIT: leave = true; break;
				case SDL_MOUSEMOTION:
					lx = ev.motion.x; ly = ev.motion.y; OMediaEventManager::window_to_logical(lx, ly);
					if (have) { dx += lx - x; dy += ly - y; }
					x = lx; y = ly; have = true;
					alt = (ev.motion.state & (SDL_BUTTON_RMASK | SDL_BUTTON_MMASK)) != 0;
					break;
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
				{
					lx = ev.button.x; ly = ev.button.y; OMediaEventManager::window_to_logical(lx, ly);
					if (have) { dx += lx - x; dy += ly - y; }
					x = lx; y = ly; have = true;
					bool d = ev.type == SDL_MOUSEBUTTONDOWN;
					if (ev.button.button == SDL_BUTTON_LEFT) { down = d; if (d) pressed = true; else released = true; }
					else alt = d;
					break;
				}
				case SDL_MOUSEWHEEL: wheel += ev.wheel.y; break;
				case SDL_KEYDOWN:
					if (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_AC_BACK || ev.key.keysym.sym == SDLK_BACKSPACE) leave = true;
					else keys.push_back(ev.key.keysym.sym);
					break;
				default: break;
			}
		}
		if (em->quit_requested()) leave = true;
	}
};

// The gamepad buttons and the left stick / triggers (the right stick is the pointer, see the event manager).
struct Pad
{
	Pad() : count(-1), cur(0), prev(0), lx(0), ly(0), lt(0), rt(0) {}
	std::vector<SDL_GameController*> pads;
	int count;
	unsigned cur, prev;
	float lx, ly, lt, rt;
	bool edge(SDL_GameControllerButton b) const { return (cur & ~prev & (1u << b)) != 0; }
	void poll()
	{
		int n = SDL_NumJoysticks();
		if (n != count)
		{
			for (size_t i = 0; i < pads.size(); i++) SDL_GameControllerClose(pads[i]);
			pads.clear();
			for (int i = 0; i < n; i++) if (SDL_IsGameController(i)) if (SDL_GameController *c = SDL_GameControllerOpen(i)) pads.push_back(c);
			count = n;
		}
		prev = cur; cur = 0; lx = ly = lt = rt = 0;
		for (size_t i = 0; i < pads.size(); i++)
		{
			for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX && b < 31; b++) if (SDL_GameControllerGetButton(pads[i], (SDL_GameControllerButton)b)) cur |= 1u << b;
			lx += SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
			ly += SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
			lt = std::max(lt, SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f);
			rt = std::max(rt, SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f);
		}
		lx = dead(lx); ly = dead(ly);
	}
	static float dead(float v) { if (v > 1) v = 1; if (v < -1) v = -1; return fabsf(v) < 0.2f ? 0 : v; }
};

bool testing() { return getenv("SB_TEST_PAD") || getenv("SB_TEST_TOUCH") || getenv("SB_ASSET_TEST"); }
float frameMs(unsigned &last)
{
	unsigned now = SDL_GetTicks();
	float dt = last ? (float)(now - last) : 16.0f;
	last = now;
	if (dt > 66.0f) dt = 66.0f;
	return testing() ? 16.0f : dt;		// deterministic when scripted
}

// The pointer: a small gold cross, drawn by the presentation over the finished picture (so a zoomed view does not scale it).
void drawPointer(const Input &in) { omgl_view_pointer(in.have, (float)in.x, (float)in.y); }

void drawButtons(Ui &ui, const std::vector<Btn> &bs, const Input &in, int pressedId)
{
	for (size_t i = 0; i < bs.size(); i++)
	{
		const Btn &b = bs[i];
		bool lit = b.on || (in.have && b.hit(in.x, in.y) && (pressedId == b.id || !in.down));
		ui.panel(b.x, b.y, b.w, b.h, b.disabled ? 2 : (lit ? 1 : 0));
		ui.text(b.x + 7, b.y + 1, b.label, 0);
	}
}

int hitButton(const std::vector<Btn> &bs, int x, int y)
{
	for (size_t i = 0; i < bs.size(); i++) if (!bs[i].disabled && bs[i].hit(x, y)) return bs[i].id;
	return -1;
}

short angleOf(float deg) { return (short)(((int)(deg * (16384.0f / 360.0f))) & 0x3fff); }

// ====================================================================================================== model browser
enum Cat { CAT_CHARACTERS, CAT_PROPS, CAT_ITEMS, CAT_COUNT };
const char *const kCatName[CAT_COUNT] = { "Characters", "Props", "Items" };

struct Entry
{
	std::string base, name;				// path without ".grn"; display name
	std::vector<std::string> anim, animName;
};

void listGrn(const std::string &dir, std::vector<std::string> &names)
{
	DIR *d = opendir(dir.c_str());
	if (!d) return;
	for (dirent *e = readdir(d); e; e = readdir(d))
		if (e->d_name[0] != '.' && endsWith(e->d_name, ".grn")) names.push_back(e->d_name);
	closedir(d);
	std::sort(names.begin(), names.end(), ciLess);
}

bool isProp(const std::string &prefix)
{
	static const char *const props[] = { "bb", "beachball", "boat1", "can", "goal", "shadow", "shadow2", 0 };
	std::string p = lower(prefix);
	for (int i = 0; props[i]; i++) if (p == props[i]) return true;
	return false;
}

std::string prettyName(const std::string &prefix)
{
	std::string s = prefix;
	if (!s.empty()) s[0] = (char)toupper((unsigned char)s[0]);
	return s;
}

// Scans models/<folder>/ for "<x>_model.grn" and the animation files "<x>_<anim>.grn" next to it.
void scanModels(std::vector<Entry> lists[CAT_COUNT])
{
	std::vector<std::string> folders;
	DIR *d = opendir("models");
	if (d)
	{
		for (dirent *e = readdir(d); e; e = readdir(d))
		{
			if (e->d_name[0] == '.') continue;
			struct stat st;
			std::string p = std::string("models/") + e->d_name;
			if (!stat(p.c_str(), &st) && S_ISDIR(st.st_mode)) folders.push_back(e->d_name);
		}
		closedir(d);
	}
	std::sort(folders.begin(), folders.end(), ciLess);
	for (size_t f = 0; f < folders.size(); f++)
	{
		std::string dir = "models/" + folders[f];
		std::vector<std::string> names;
		listGrn(dir, names);
		for (size_t i = 0; i < names.size(); i++)
		{
			if (!endsWith(names[i], "_model.grn")) continue;
			std::string prefix = names[i].substr(0, names[i].size() - 10);
			Entry e;
			e.base = dir + "/" + names[i].substr(0, names[i].size() - 4);
			e.name = prettyName(prefix);
			std::string pre = lower(prefix) + "_";
			for (size_t k = 0; k < names.size(); k++)
			{
				if (k == i || lower(names[k]).compare(0, pre.size(), pre) != 0 || lower(names[k]).find("_model") != std::string::npos) continue;
				e.anim.push_back(dir + "/" + names[k]);
				e.animName.push_back(names[k].substr(pre.size(), names[k].size() - pre.size() - 4));
			}
			int cat = lower(folders[f]) == "items" ? CAT_ITEMS : (isProp(prefix) ? CAT_PROPS : CAT_CHARACTERS);
			lists[cat].push_back(e);
		}
	}
	for (int c = 0; c < CAT_COUNT; c++)
		for (size_t i = 1; i < lists[c].size(); i++)			// insertion sort by name (small lists)
			for (size_t j = i; j > 0 && ciLess(lists[c][j].name, lists[c][j - 1].name); j--) std::swap(lists[c][j], lists[c][j - 1]);
}

enum { B_TAB0 = 0, B_BACK = 10, B_PREV, B_NEXT, B_APREV, B_ANEXT, B_ZOUT, B_ZIN, B_PAN, B_SPIN, B_RESET, B_BARS, B_SPOTS };

class ModelBrowser
{
	public:
	ModelBrowser(OMediaDoubleCanvas *screen, OMediaEventManager *em, GameFont *font)
		: em_(em), font_(font), cat_(CAT_CHARACTERS), anim_(-1), model_(0), elem_(0), yaw_(295), pitch_(10), zoom_(1), panx_(0), pany_(0),
		  spin_(true), panMode_(false), pressed_(-1), age_(0), sign_(1), lastTicks_(0), D0_(480), err_(false), leave_(false)
	{
		for (int i = 0; i < CAT_COUNT; i++) idx_[i] = 0;
		t_[0] = t_[1] = t_[2] = 0;
		scanModels(lists_);
		world_ = new OMedia2DWorld;
		world_->set_sort_mode(omc2dsort_Z);
		world_->set_framerate_mode(omfrc_Max);
		OMediaRGBColor bg(0x0c00, 0x1c00, 0x3c00);
		((OMediaColorBackground*)world_->getbackground())->setcolor(&bg);
		OMediaViewPort *vp = new OMediaViewPort(world_);
		OMediaRect bounds(0, 0, 640, 480);
		vp->setbounds(&bounds); vp->setcanvas(screen);
		OMedia3DAbstractRasterizer *ras = rasterizer();
		omd_REGISTERCLASS(OMedia3DMaterial);
		OMedia3DGrannyModel::init(".");
		w3_ = new OMedia3DWorld;
		w3_->set_framerate_mode(omfrc_Max); w3_->setbackground(NULL);
		w3_->focal = 400; w3_->near_z = 5; w3_->far_z = 6000; w3_->rasterizer = ras;
		w3_->ambient.setred8bits(255); w3_->ambient.setgreen8bits(255); w3_->ambient.setblue8bits(255);
		vp3_ = new OMediaViewPort(w3_);
		vp3_->setbounds(&bounds);
		OMediaVPElement *vpe = new OMediaVPElement;
		vpe->set_viewport(vp3_); vpe->link(world_); vpe->place(0, 0, 100);
		ui_ = new Ui(world_, font);
		// which way is "up" for the pitch angle: the sign that puts the camera above the model
		OMedia3DVector f(0, 0, 1); f.rotate(angleOf(10), 0, 0);
		sign_ = f.y > 0 ? -1 : 1;
		// test hooks: SB_ASSET_MODEL=<name> SB_ASSET_ANIM=<name> SB_ASSET_YAW / _PITCH / _ZOOM
		if (const char *m = getenv("SB_ASSET_MODEL"))
			for (int c = 0; c < CAT_COUNT; c++)
				for (size_t i = 0; i < lists_[c].size(); i++)
					if (!strcasecmp(lists_[c][i].name.c_str(), m)) { cat_ = c; idx_[c] = (int)i; }
		load();
		if (const char *a = getenv("SB_ASSET_ANIM"))
		{
			const Entry *e = cur();
			for (size_t i = 0; e && i < e->animName.size(); i++) if (!strcasecmp(e->animName[i].c_str(), a)) setAnim((int)i);
		}
		if (const char *v = getenv("SB_ASSET_YAW")) yaw_ = (float)atof(v);
		if (const char *v = getenv("SB_ASSET_PITCH")) pitch_ = (float)atof(v);
		if (const char *v = getenv("SB_ASSET_ZOOM")) zoom_ = (float)atof(v);
		if (getenv("SB_ASSET_NOSPIN")) spin_ = false;
	}

	void run()
	{
		for (;;)
		{
			in_.pump(em_);
			pad_.poll();
			if (in_.leave || leave_ || pad_.edge(SDL_CONTROLLER_BUTTON_B) || pad_.edge(SDL_CONTROLLER_BUTTON_BACK)) break;
			float dt = frameMs(lastTicks_) / 1000.0f;
			age_ += dt;
			input(dt);
			if (leave_) break;
			frame(dt);
			SDL_Delay(10);
		}
	}

	private:
	const Entry *cur() const
	{
		return idx_[cat_] >= 0 && idx_[cat_] < (int)lists_[cat_].size() ? &lists_[cat_][idx_[cat_]] : 0;
	}

	void unload()
	{
		delete elem_; elem_ = 0;
		delete model_; model_ = 0;
	}

	void setAnim(int a)
	{
		const Entry *e = cur();
		if (!e || !model_ || e->anim.empty()) { anim_ = -1; return; }
		int n = (int)e->anim.size();
		anim_ = ((a % n) + n) % n;
		if (!model_->play(e->anim[anim_], 1.0f, 9999, 0.15f)) err_ = true;
	}

	void load()
	{
		unload();
		err_ = false; anim_ = -1;
		const Entry *e = cur();
		if (!e) return;
		model_ = new OMedia3DGrannyModel;
		if (!model_->load(e->base)) { err_ = true; delete model_; model_ = 0; return; }
		int pick = 0;
		for (size_t i = 0; i < e->animName.size(); i++) if (!strcasecmp(e->animName[i].c_str(), "idle1")) pick = (int)i;
		if (!e->anim.empty()) setAnim(pick);
		// frame the model by its first pose: 300 units across, centred on its bounding box
		OMedia3DGrannyModel::advance_time(0.5);
		model_->update_pose();
		float lo[3], hi[3];
		bounds(lo, hi);
		float extent = std::max(std::max(hi[0] - lo[0], hi[1] - lo[1]), hi[2] - lo[2]);
		if (!(extent > 0)) extent = 1;
		float s = 300.0f / extent;
		model_->set_scale(s);
		model_->update_pose();
		for (int k = 0; k < 3; k++) t_[k] = (lo[k] + hi[k]) * 0.5f * s;
		elem_ = new OMedia3DShapeElement;
		elem_->setshape(model_->get_shape());
		elem_->link(w3_);
		elem_->place(0, 0, 0);
		resetView();
	}

	void bounds(float *lo, float *hi)
	{
		std::vector<OMedia3DPoint> &vs = *model_->get_shape()->get_vertices();
		lo[0] = lo[1] = lo[2] = 1.e30f; hi[0] = hi[1] = hi[2] = -1.e30f;
		for (size_t v = 0; v < vs.size(); v++)
		{
			float p[3] = { vs[v].x, vs[v].y, vs[v].z };
			for (int k = 0; k < 3; k++) if (p[k] == p[k] && fabsf(p[k]) < 1.e20f) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); }
		}
		for (int k = 0; k < 3; k++) if (lo[k] > hi[k]) lo[k] = hi[k] = 0;
	}

	void resetView() { yaw_ = 295; pitch_ = 10; zoom_ = 1; panx_ = pany_ = 0; }
	void step(int d)
	{
		int n = (int)lists_[cat_].size();
		if (n) { idx_[cat_] = ((idx_[cat_] + d) % n + n) % n; load(); }
	}
	void setCat(int c)
	{
		cat_ = ((c % CAT_COUNT) + CAT_COUNT) % CAT_COUNT;
		load();
	}
	void zoomBy(float f) { zoom_ *= f; if (zoom_ < 0.3f) zoom_ = 0.3f; if (zoom_ > 6) zoom_ = 6; }

	void act(int id)
	{
		switch (id)
		{
			case B_TAB0: case B_TAB0 + 1: case B_TAB0 + 2: setCat(id - B_TAB0); break;
			case B_PREV: step(-1); break;
			case B_NEXT: step(1); break;
			case B_APREV: setAnim(anim_ < 0 ? -1 : anim_ - 1); break;
			case B_ANEXT: setAnim(anim_ + 1); break;
			case B_ZOUT: zoomBy(1 / 1.25f); break;
			case B_ZIN: zoomBy(1.25f); break;
			case B_PAN: panMode_ = !panMode_; break;
			case B_SPIN: spin_ = !spin_; break;
			case B_RESET: resetView(); break;
			case B_BACK: leave_ = true; break;
			default: break;
		}
	}

	float panScale() const { return (D0_ / zoom_) / 400.0f; }
	void doPan(float dxPix, float dyPix) { panx_ -= dxPix * panScale(); pany_ += dyPix * panScale(); }

	void input(float dt)
	{
		bool dragging = false;
		// pointer: a press on a button is a click on release over it; elsewhere a drag turns (or pans) the model
		if (in_.pressed) pressed_ = hitButton(buttons_, in_.x, in_.y);
		if (in_.down && pressed_ < 0 && (in_.dx || in_.dy) && in_.y > 24 && in_.y < 428)
		{
			dragging = true;
			if (panMode_) doPan((float)in_.dx, (float)in_.dy);
			else { yaw_ -= in_.dx * 0.6f; pitch_ += in_.dy * 0.5f; }
		}
		if (in_.alt && (in_.dx || in_.dy)) { doPan((float)in_.dx, (float)in_.dy); dragging = true; }
		if (in_.released)
		{
			if (pressed_ >= 0 && hitButton(buttons_, in_.x, in_.y) == pressed_) act(pressed_);
			pressed_ = -1;
		}
		if (in_.wheel) zoomBy(powf(1.15f, (float)in_.wheel));
		for (size_t i = 0; i < in_.keys.size(); i++)
		{
			switch (in_.keys[i])
			{
				case SDLK_LEFT: act(B_PREV); break;
				case SDLK_RIGHT: act(B_NEXT); break;
				case SDLK_UP: act(B_APREV); break;
				case SDLK_DOWN: act(B_ANEXT); break;
				case SDLK_PAGEUP: setCat(cat_ - 1); break;
				case SDLK_PAGEDOWN: setCat(cat_ + 1); break;
				case SDLK_PLUS: case SDLK_EQUALS: case SDLK_KP_PLUS: act(B_ZIN); break;
				case SDLK_MINUS: case SDLK_KP_MINUS: act(B_ZOUT); break;
				case SDLK_HOME: case SDLK_r: act(B_RESET); break;
				case SDLK_SPACE: act(B_SPIN); break;
				case SDLK_p: act(B_PAN); break;
				case SDLK_a: yaw_ += 8; dragging = true; break;
				case SDLK_d: yaw_ -= 8; dragging = true; break;
				case SDLK_w: pitch_ -= 6; dragging = true; break;
				case SDLK_s: pitch_ += 6; dragging = true; break;
				default: break;
			}
		}
		// gamepad: d-pad left / right model, up / down animation, L1 / R1 category, left stick turns (or pans), triggers zoom, X pan, Y spin, Start reset
		if (pad_.edge(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) act(B_PREV);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) act(B_NEXT);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_DPAD_UP)) act(B_APREV);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) act(B_ANEXT);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) setCat(cat_ - 1);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) setCat(cat_ + 1);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_X)) act(B_PAN);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_Y)) act(B_SPIN);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_START)) act(B_RESET);
		if (pad_.lx || pad_.ly)
		{
			dragging = true;
			if (panMode_) doPan(pad_.lx * 300 * dt, pad_.ly * 300 * dt);
			else { yaw_ -= pad_.lx * 120 * dt; pitch_ += pad_.ly * 90 * dt; }
		}
		if (pad_.rt > 0.1f || pad_.lt > 0.1f) zoomBy(powf(2.0f, (pad_.rt - pad_.lt) * dt * 1.5f));
		if (pitch_ > 85) pitch_ = 85;
		if (pitch_ < -85) pitch_ = -85;
		if (spin_ && !dragging && !in_.down) yaw_ -= 18 * dt;
		yaw_ = fmodf(yaw_, 360.0f);
	}

	void camera()
	{
		short ax = angleOf(pitch_ * sign_), ay = angleOf(yaw_);
		OMedia3DVector f(0, 0, 1), r(1, 0, 0), u(0, 1, 0);
		f.rotate(ax, ay, 0); r.rotate(ax, ay, 0); u.rotate(ax, ay, 0);
		float tx = t_[0] + r.x * panx_ + u.x * pany_, ty = t_[1] + r.y * panx_ + u.y * pany_, tz = t_[2] + r.z * panx_ + u.z * pany_;
		float D = D0_ / zoom_;
		vp3_->place(tx - f.x * D, ty - f.y * D, tz - f.z * D);
		vp3_->set_angle(ax, ay, 0);
	}

	void frame(float dt)
	{
		OMedia3DGrannyModel::advance_time(dt);
		if (model_) model_->update_pose();
		camera();
		Ui &ui = *ui_;
		ui.begin();
		OMediaRGBColor bar(0x0800, 0x1000, 0x3800);
		// top bar: category tabs, Back
		ui.box(0, 0, 640, 25, bar, Z_PANEL - 2);
		buttons_.clear();
		int x = 4;
		for (int c = 0; c < CAT_COUNT; c++) addBtn(ui, buttons_, x, 2, B_TAB0 + c, std::string(kCatName[c]) + " " + num((int)lists_[c].size()), c == cat_);
		x = 640 - 4 - (ui.width("Back") + 14);
		addBtn(ui, buttons_, x, 2, B_BACK, "Back");
		// bottom bar: two rows
		ui.box(0, 428, 640, 52, bar, Z_PANEL - 2);
		x = 4;
		addBtn(ui, buttons_, x, 431, B_PREV, "Prev model");
		addBtn(ui, buttons_, x, 431, B_NEXT, "Next model");
		addBtn(ui, buttons_, x, 431, B_APREV, "Prev anim");
		addBtn(ui, buttons_, x, 431, B_ANEXT, "Next anim");
		x = 4;
		addBtn(ui, buttons_, x, 455, B_ZOUT, "Zoom out");
		addBtn(ui, buttons_, x, 455, B_ZIN, "Zoom in");
		addBtn(ui, buttons_, x, 455, B_PAN, "Pan", panMode_);
		addBtn(ui, buttons_, x, 455, B_SPIN, "Spin", spin_);
		addBtn(ui, buttons_, x, 455, B_RESET, "Reset");
		drawButtons(ui, buttons_, in_, pressed_);
		// names
		const Entry *e = cur();
		std::string title = e ? e->name : std::string("(none)");
		if (e) title += "   " + num(idx_[cat_] + 1) + " of " + num((int)lists_[cat_].size());
		ui.ctext(320, 30, title);
		std::string a;
		if (err_) a = "This model cannot be loaded";
		else if (e && anim_ >= 0) a = "Animation " + e->animName[anim_] + "   " + num(anim_ + 1) + " of " + num((int)e->anim.size());
		else a = "No animations";
		ui.ctext(320, 49, a, 0);
		if (age_ < 8) ui.ctext(320, 405, "Drag to turn   wheel to zoom   Pan button or right-drag to move");
		drawPointer(in_);
		ui.end();
		w3_->update();
		world_->update();
	}

	OMediaEventManager *em_;
	GameFont *font_;
	std::vector<Entry> lists_[CAT_COUNT];
	int cat_, idx_[CAT_COUNT], anim_;
	OMedia2DWorld *world_;
	OMedia3DWorld *w3_;
	OMediaViewPort *vp3_;
	Ui *ui_;
	OMedia3DGrannyModel *model_;
	OMedia3DShapeElement *elem_;
	float yaw_, pitch_, zoom_, panx_, pany_, t_[3];
	bool spin_, panMode_;
	int pressed_;
	float age_;
	int sign_;
	unsigned lastTicks_;
	float D0_;
	bool err_, leave_;
	Input in_;
	Pad pad_;
	std::vector<Btn> buttons_;
};


// ====================================================================================================== scene explorer
struct Spot
{
	int x1, y1, x2, y2;
	std::string text, kind;
};

class SceneExplorer
{
	public:
	SceneExplorer(OMediaDoubleCanvas *screen, OMediaEventManager *em, const std::string &first)
		: screen_(screen), em_(em), g_(0), ui_(0), idx_(0), frames_(0), settle_(0), frozen_(false), zoom_(1), cx_(320), cy_(240),
		  spots_(true), bars_(true), pressed_(-1), age_(0), hideAge_(99), lastTicks_(0), leave_(false), failed_(false)
	{
		setenv("SB_NO_GAME_CURSOR", "1", 1);		// the viewer draws its own pointer (the game's sprite would be zoomed with the scene)
		DIR *d = opendir("scenes");
		if (d)
		{
			for (dirent *e = readdir(d); e; e = readdir(d))
			{
				std::string n = e->d_name;
				if (endsWith(n, ".scn")) names_.push_back(n.substr(0, n.size() - 4));
			}
			closedir(d);
		}
		std::sort(names_.begin(), names_.end(), ciLess);
		for (size_t i = 0; i < names_.size(); i++) if (!first.empty() && !strcasecmp(names_[i].c_str(), first.c_str())) idx_ = (int)i;
		if (names_.empty()) failed_ = true;
		else open(idx_);
		if (const char *v = getenv("SB_ASSET_ZOOM")) zoom_ = (float)atof(v);
		if (const char *v = getenv("SB_ASSET_CX")) cx_ = (float)atof(v);
		if (const char *v = getenv("SB_ASSET_CY")) cy_ = (float)atof(v);
	}

	void run()
	{
		while (!failed_)
		{
			in_.pump(em_);
			pad_.poll();
			if (in_.leave || leave_ || pad_.edge(SDL_CONTROLLER_BUTTON_B) || pad_.edge(SDL_CONTROLLER_BUTTON_BACK)) break;
			float dt = frameMs(lastTicks_) / 1000.0f;
			age_ += dt; hideAge_ += dt;
			input(dt);
			if (leave_ || failed_) break;
			frame();
			SDL_Delay(10);
		}
		omgl_view_window(false, 0, 0, 640, 480, 0, 0);
		omgl_view_pointer(false, 0, 0);
		release();
	}

	private:
	// the Game's world owns the glyph and box elements the viewer made: forget them before it is destroyed
	void release()
	{
		if (g_ && g_->font()) static_cast<GameFont*>(g_->font())->drop_pool();
		delete ui_; ui_ = 0;
		delete g_; g_ = 0;
	}

	void open(int i)
	{
		release();
		idx_ = ((i % (int)names_.size()) + (int)names_.size()) % (int)names_.size();
		g_ = new sbgame::Game;
		if (!g_->init(".", screen_)) { SDL_Log("asset view: game init failed"); delete g_; g_ = 0; failed_ = true; return; }
		g_->logFile = getenv("SB_GAME_LOG") ? stdout : NULL;
		g_->switchTo(names_[idx_].c_str());
		ui_ = new Ui(g_->world(), static_cast<GameFont*>(g_->font()));
		frames_ = settle_ = 0; frozen_ = false;
		resetView();
	}

	void resetView() { zoom_ = 1; cx_ = 320; cy_ = 240; }
	int bt() const { return bars_ ? 22 : 0; }
	int bb() const { return bars_ ? 48 : 0; }
	float bandH() const { return (float)(480 - bt() - bb()); }
	float sw() const { return 640.0f / zoom_; }
	float sh() const { return bandH() / zoom_; }
	void clampView()
	{
		if (zoom_ < 1) zoom_ = 1;
		if (zoom_ > 5) zoom_ = 5;
		if (cx_ < sw() / 2) cx_ = sw() / 2;
		if (cx_ > 640 - sw() / 2) cx_ = 640 - sw() / 2;
		float lo = bt() + sh() / 2, hi = 480 - bb() - sh() / 2;
		if (cy_ < lo) cy_ = lo;
		if (cy_ > hi) cy_ = hi;
	}
	// window point (logical) -> scene point, when it is in the picture band
	bool toScene(int x, int y, float &sx, float &sy) const
	{
		if (y < bt() || y >= 480 - bb()) return false;
		sx = cx_ - sw() / 2 + x * sw() / 640.0f;
		sy = cy_ - sh() / 2 + (y - bt()) * sh() / bandH();
		return true;
	}
	void zoomAt(float f, float px, float py)		// keeps the scene point under (px, py) in place
	{
		float sx, sy;
		bool ok = toScene((int)px, (int)py, sx, sy);
		zoom_ *= f;
		if (zoom_ < 1) zoom_ = 1;
		if (zoom_ > 5) zoom_ = 5;
		if (ok)
		{
			cx_ = sx - (px * sw() / 640.0f) + sw() / 2;
			cy_ = sy - ((py - bt()) * sh() / bandH()) + sh() / 2;
		}
		clampView();
	}
	void pan(float dxPix, float dyPix) { cx_ -= dxPix * sw() / 640.0f; cy_ -= dyPix * sh() / bandH(); clampView(); }
	void showBars(bool on) { bars_ = on; if (!on) hideAge_ = 0; clampView(); }

	void act(int id)
	{
		switch (id)
		{
			case B_BACK: leave_ = true; break;
			case B_PREV: open(idx_ - 1); break;
			case B_NEXT: open(idx_ + 1); break;
			case B_ZOUT: zoomAt(1 / 1.3f, 320, 240); break;
			case B_ZIN: zoomAt(1.3f, 320, 240); break;
			case B_SPOTS: spots_ = !spots_; break;
			case B_BARS: showBars(!bars_); break;
			case B_RESET: resetView(); break;
			default: break;
		}
	}

	void input(float dt)
	{
		bool hiddenCorner = !bars_ && in_.have && in_.x < 44 && in_.y < 44;
		if (in_.pressed)
		{
			if (hiddenCorner) { showBars(true); pressed_ = -2; }
			else pressed_ = bars_ ? hitButton(buttons_, in_.x, in_.y) : -1;
		}
		if (in_.down && pressed_ == -1 && (in_.dx || in_.dy)) pan((float)in_.dx, (float)in_.dy);
		if (in_.alt && (in_.dx || in_.dy)) pan((float)in_.dx, (float)in_.dy);
		if (in_.released)
		{
			if (pressed_ >= 0 && hitButton(buttons_, in_.x, in_.y) == pressed_) act(pressed_);
			pressed_ = -1;
		}
		if (in_.wheel) zoomAt(powf(1.2f, (float)in_.wheel), (float)in_.x, (float)in_.y);
		for (size_t i = 0; i < in_.keys.size(); i++)
		{
			switch (in_.keys[i])
			{
				case SDLK_LEFT: pan(40, 0); break;
				case SDLK_RIGHT: pan(-40, 0); break;
				case SDLK_UP: pan(0, 40); break;
				case SDLK_DOWN: pan(0, -40); break;
				case SDLK_PAGEUP: act(B_PREV); break;
				case SDLK_PAGEDOWN: act(B_NEXT); break;
				case SDLK_PLUS: case SDLK_EQUALS: case SDLK_KP_PLUS: act(B_ZIN); break;
				case SDLK_MINUS: case SDLK_KP_MINUS: act(B_ZOUT); break;
				case SDLK_HOME: case SDLK_r: act(B_RESET); break;
				case SDLK_h: act(B_BARS); break;
				case SDLK_t: act(B_SPOTS); break;
				default: break;
			}
		}
		// gamepad: d-pad left / right scene, up / down zoom, left stick pans, triggers zoom, X hotspot captions, Y bars, Start reset
		if (pad_.edge(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) act(B_PREV);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) act(B_NEXT);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_DPAD_UP)) act(B_ZIN);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) act(B_ZOUT);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_X)) act(B_SPOTS);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_Y)) act(B_BARS);
		if (pad_.edge(SDL_CONTROLLER_BUTTON_START)) act(B_RESET);
		if (pad_.lx || pad_.ly) pan(-pad_.lx * 320 * dt, -pad_.ly * 320 * dt);
		if (pad_.rt > 0.1f || pad_.lt > 0.1f) zoomAt(powf(2.0f, (pad_.rt - pad_.lt) * dt * 1.5f), 320, 240);
		clampView();
	}

	// what can be pointed at: the hotspot rectangles (2D objects, hot spots) and the drawn extents of characters and items
	void collect(std::vector<Spot> &out)
	{
		sbgame::GameScene *sc = g_->scene();
		if (!sc) return;
		std::vector<sbgame::SceneObject*> &os = sc->objects();
		for (size_t i = 0; i < os.size(); i++)
		{
			sbgame::SceneObject *o = os[i];
			if (o->description.empty() || o->kind == sbgame::SceneObject::Trigger) continue;
			Spot s;
			if (o->kind == sbgame::SceneObject::Char || o->kind == sbgame::SceneObject::Item)
			{
				if (!g_->world3d() || !g_->world3d()->screenBounds(o, s.x1, s.y1, s.x2, s.y2)) continue;
			}
			else
			{
				if (o->x2 <= o->x1 || o->y2 <= o->y1) continue;
				if (o->elem && o->elem->gethide()) continue;
				if (!o->visibleCond.empty() && !g_->cond(o->visibleCond.c_str())) continue;
				s.x1 = o->x1; s.y1 = o->y1; s.x2 = o->x2; s.y2 = o->y2;
			}
			if (s.x2 <= s.x1 || s.y2 <= s.y1 || s.x2 < 0 || s.y2 < 0 || s.x1 > 640 || s.y1 > 480) continue;
			for (size_t k = 0; k < o->description.size(); k++) if (o->description[k] != '*') s.text += o->description[k];
			switch (o->kind)
			{
				case sbgame::SceneObject::Obj2D: s.kind = "picture"; break;
				case sbgame::SceneObject::Anim2D: s.kind = "animation"; break;
				case sbgame::SceneObject::Spot: s.kind = "hotspot"; break;
				case sbgame::SceneObject::Item: s.kind = "item"; break;
				default: s.kind = "character"; break;
			}
			out.push_back(s);
		}
	}

	void frame()
	{
		if (!frozen_)
		{
			// let the scene come up (enter script, bars) a little, then freeze it
			try { g_->update(16.0f); } catch (OMediaError &) { frozen_ = true; }
			frames_++;
			if (g_->scene() && g_->scene()->barState() == 0 && !g_->pendingScene() && frames_ > 10) settle_++;
			if (settle_ >= 40 || frames_ > 900) frozen_ = true;
		}
		clampView();
		Ui &ui = *ui_;
		ui.begin();
		std::vector<Spot> spots;
		collect(spots);
		float sx = -1, sy = -1;
		bool over = in_.have && toScene(in_.x, in_.y, sx, sy);
		const Spot *hover = 0;
		for (size_t i = spots.size(); over && i-- > 0 && !hover;)
			if (sx >= spots[i].x1 && sx < spots[i].x2 && sy >= spots[i].y1 && sy < spots[i].y2) hover = &spots[i];
		if (spots_)
		{
			OMediaRGBColor gold(0xf000, 0xc800, 0x4000), lit(0xffff, 0xffff, 0xffff);
			for (size_t i = 0; i < spots.size(); i++)
			{
				const Spot &s = spots[i];
				bool h = &s == hover;
				int w = s.x2 - s.x1, hh = s.y2 - s.y1;
				ui.frame(s.x1, s.y1, w, hh, 1, h ? lit : gold);
				int tw = ui.width(s.text);
				int tx = std::max(2, std::min(s.x1, 640 - tw - 2));
				int ty = s.y1 - 19;
				if (ty < 24) ty = std::min(s.y2 + 1, 432 - 19);
				ui.box(tx - 2, ty, tw + 4, 18, OMediaRGBColor(0x0800, 0x1000, 0x3800), Z_PANEL + 3);
				ui.text(tx, ty, s.text, 0);
			}
		}
		buttons_.clear();
		if (bars_)
		{
			OMediaRGBColor bar(0x0800, 0x1000, 0x3800);
			ui.box(0, 0, 640, 22, bar, Z_PANEL - 2);
			ui.box(0, 432, 640, 48, bar, Z_PANEL - 2);
			std::string t = "Scene " + names_[idx_] + "   " + num(idx_ + 1) + " of " + num((int)names_.size()) + "   zoom " + fixed1(zoom_);
			if (!frozen_) t += "   loading";
			ui.text(6, 2, t);
			int x = 640 - 4 - (ui.width("Back") + 14);
			addBtn(ui, buttons_, x, 0, B_BACK, "Back");
			if (hover) ui.text(6, 434, hover->text + "   " + hover->kind);
			else ui.text(6, 434, spots.empty() ? "No hotspots with a caption in this scene" : "Point at something for its caption");
			x = 4;
			addBtn(ui, buttons_, x, 457, B_PREV, "Prev");
			addBtn(ui, buttons_, x, 457, B_NEXT, "Next");
			addBtn(ui, buttons_, x, 457, B_ZOUT, "Zoom out");
			addBtn(ui, buttons_, x, 457, B_ZIN, "Zoom in");
			addBtn(ui, buttons_, x, 457, B_SPOTS, "Captions", spots_);
			addBtn(ui, buttons_, x, 457, B_BARS, "Bars");
			addBtn(ui, buttons_, x, 457, B_RESET, "Reset");
			// the buttons sit in a bar drawn above the scene (z): lift their panels over it
			drawButtons(ui, buttons_, in_, pressed_);
		}
		else if (hideAge_ < 4) ui.text(6, 6, "Tap here for the bars");
		drawPointer(in_);
		ui.end();
		clampView();
		omgl_view_window(true, cx_ - sw() / 2, cy_ - sh() / 2, sw(), sh(), (float)bt(), (float)bb());
		g_->world()->update();
	}

	static std::string fixed1(float v) { char b[16]; snprintf(b, sizeof b, "%.1f", v); return b; }

	OMediaDoubleCanvas *screen_;
	OMediaEventManager *em_;
	sbgame::Game *g_;
	Ui *ui_;
	std::vector<std::string> names_;
	int idx_, frames_, settle_;
	bool frozen_;
	float zoom_, cx_, cy_;
	bool spots_, bars_;
	int pressed_;
	float age_, hideAge_;
	unsigned lastTicks_;
	bool leave_, failed_;
	Input in_;
	Pad pad_;
	std::vector<Btn> buttons_;
};

}	// namespace

void AssetViewRun(const char *spec, OMediaDoubleCanvas *screen, OMediaEventManager *events, GameFont *font)
{
	std::string s = spec ? spec : "";
	SDL_Log("asset viewer: %s", s.c_str());
	try
	{
		if (s == "models") { ModelBrowser b(screen, events, font); b.run(); }
		else if (s.compare(0, 5, "scene") == 0 && (s.size() == 5 || s[5] == ':'))
		{
			SceneExplorer x(screen, events, s.size() > 6 ? s.substr(6) : std::string());
			x.run();
		}
		else SDL_Log("asset viewer: unknown mode '%s' (models | scene[:<name>])", s.c_str());
	}
	catch (OMediaError &e)
	{
		SDL_Log("asset viewer: OMT error %d at %s:%d", (int)e.errorcode, e.file.c_str(), e.line);
	}
	omgl_view_window(false, 0, 0, 640, 480, 0, 0);
		omgl_view_pointer(false, 0, 0);
}

}	// namespace sbport
