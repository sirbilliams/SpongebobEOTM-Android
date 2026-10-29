#include "Hud.h"
#include "Game.h"
#include "DialogueUI.h"
#include "World3D.h"
#include "Cursor.h"
#include "OMedia3DWorld.h"
#include "OMedia3DGrannyModel.h"
#include "OMediaVPElement.h"
#include "OMediaViewPort.h"
#include "OMediaDXSurfaceWorld.h"
#include "OMediaGLBackend.h"
#include "OMediaCanvas.h"
#include "OMediaRGBColor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <vector>

namespace sbgame {

static const float kZButton = 9990.0f;     // below the letterbox bars' 9999: the bar is hidden while they are up
static const float kZItem = 10000.0f;      // (0x461f6400 / 0x461f6800 in the exe: the item element's depth)
static const float kFlyMs = 1200.0f;       // port: the exe's pickup animation (+0x140 = 2000) is shortened to the script's wait (uiMs_)

HudItem::HudItem()
    : slot(0), model(0), w3(0), vp(0), vpe(0), elem(0), light(0), cx(0), cy(0), cz(0), radius(1), focal(400), x(0), y(0),
      shown(false), tried(false), yaw(0x0800)
{
}

static std::string lowerStr(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

Hud::Hud(Game* g)
    : g_(g), held_(0), dragging_(false), downX_(0), downY_(0), open_(false), shownNow_(false), fly_(0), flyT_(0), flyMs_(0),
      flyX_(0), flyY_(0), mx_(320), my_(240), hover_(None), hoverSlot_(-1), hintBlock_(0), hintMs_(0), iconFrame_(0),
      testItemsDone_(false), built_(false), testHoldDone_(false), lastScene_(0), sparkleMs_(0), sparkleX_(0), sparkleY_(0), sparkleAcc_(0)
{
    for (int i = 0; i < 10; ++i) { sparkles_[i] = 0; sparkleAge_[i] = 0; }
}

Hud::~Hud()
{
    clearSparkles();
    delete hintBlock_;
    for (size_t i = 0; i < items_.size(); ++i) {
        HudItem* it = items_[i];
        delete it->vpe; delete it->elem; delete it->light; delete it->vp; delete it->w3; delete it->model; delete it;
    }
    Btn* all[] = { &inv_, &invT_, &inv2_, &inv2T_, &help_, &helpT_, &menu_, &menuT_, &map_, &mapT_ };
    for (size_t i = 0; i < sizeof all / sizeof all[0]; ++i) delete all[i]->e;
    for (int i = 0; i < 6; ++i) { delete bubble_[i].e; delete bubbleT_[i].e; }
}

OMediaCanvasElement* Hud::mk(const char* name, float x, float y)
{
    OMediaDataBase* db = g_->sb2Db();
    OMediaCanvas* c = db ? (OMediaCanvas*)db->get_object(OMediaCanvas::db_type, std::string(name)) : 0;
    if (!c) { g_->log("HUD: canvas '%s' not found in sb2.omt\n", name); return 0; }
    OMediaCanvasElement* e = new OMediaCanvasElement(c);
    e->link(g_->world());
    e->place(x, y, kZButton);
    e->hide(true);
    return e;
}

void Hud::show(Btn& b, bool on)
{
    if (!b.e || b.shown == on) return;
    b.shown = on;
    b.e->hide(!on);
}

void Hud::showItem(HudItem* it, bool on)
{
    if (!it->vpe || it->shown == on) return;
    it->shown = on;
    it->vpe->hide(!on);
}

const std::string& Hud::heldName() const
{
    static const std::string none;
    return held_ ? held_->name : none;
}

// ------------------------------------------------------------------------------------------------ state
bool Hud::wanted()          // 0x421090 / 0x420fc0: the bar belongs to the scenes of a level (level 0 = the cutscene scenes), not to scripts without control
{
    GameScene* s = g_->scene();
    if (!s) return false;
    unsigned id = g_->state().sceneId;
    if (((id >> 8) & 0xf) == 0) return false;
    if (g_->pendingScene()) return false;
    sbscript::ScriptRunner* r = g_->runner();
    if (r && !r->keepControl()) return false;
    return true;
}

bool Hud::active()
{
    if (!wanted() || fly_) return false;
    GameScene* s = g_->scene();
    if (s->dialogue() && s->dialogue()->busy()) return false;
    return true;
}

Hud::Spot Hud::spotAt(int x, int y, int* slot)      // 0x4201c0 / 0x420d50 boundaries
{
    if (slot) *slot = -1;
    if (y >= 0x40) return None;
    if (x < 0x40) return Chest;
    if (x > 0x240) return Map;
    if (!open_) return None;
    if (x > 0x200) return Menu;
    if (x > 0x1c0) return Hint;
    for (int i = 0; i < 6; ++i)                     // 0x420a30
        if (x > 0x40 + 0x40 * i && x < 0x80 + 0x40 * i) { if (slot) *slot = i; return Slot; }
    return None;
}

int Hud::slotOf(const std::string& name) { HudItem* it = find(name); return it ? it->slot : -1; }

HudItem* Hud::itemInSlot(int slot)
{
    for (size_t i = 0; i < items_.size(); ++i) if (items_[i]->slot == slot) return items_[i];
    return 0;
}

HudItem* Hud::find(const std::string& name)
{
    std::string n = lowerStr(name);
    for (size_t i = 0; i < items_.size(); ++i) if (lowerStr(items_[i]->name) == n) return items_[i];
    return 0;
}

int Hud::freeSlot()
{
    for (int s = 0; s < 6; ++s) if (!itemInSlot(s)) return s;
    return -1;
}

void Hud::setOpen(bool open)          // 0x421390
{
    if (held_ && open == open_) return;
    open_ = open;
    sound(open ? "inv" : "inv2");
}

void Hud::sound(const char* name)     // the wave 'inv' / 'inv2' of the shared bank (game+0x68), when it has one
{
    bool shared = false;
    OMediaSound* s = g_->findSound(name, &shared);
    if (s) g_->playOnChannel(5, s, 0);
}

// ------------------------------------------------------------------------------------------------ items
void Hud::addItem(const char* name, const char* desc)      // 0x41f2d0
{
    if (find(name)) return;                                // port: the 'add' command runs again when a script is replayed
    int slot = freeSlot();
    if (slot < 0) { g_->log("HUD: inventory full, '%s' not added\n", name); return; }
    HudItem* it = new HudItem;
    it->name = name;
    it->desc = desc ? desc : "";
    it->slot = slot;
    items_.push_back(it);
    int sx = 0x140, sy = 0xf0;                             // where it appears first
    if (strcasecmp(name, "ikp5") == 0) { sx = 0xdc; sy = 200; }
    unsigned id = g_->state().sceneId;
    if (id == 0x20204 && strcasecmp(name, "ivt1") == 0) { sx = 0xb4; sy = 0x172; }
    if (id == 0x10304 && strcasecmp(name, "inc1") == 0) { sx = 0x1ef; sy = 0x195; }
    fly_ = it; flyT_ = 0; flyMs_ = kFlyMs;
    flyX_ = (float)(sx - 0x20); flyY_ = (float)(sy - 0x20);
    placeItem(it, flyX_, flyY_);
    g_->log("HUD: item %s '%s' -> slot %d\n", name, it->desc.c_str(), slot);
    sound(name);
}

void Hud::remove(HudItem* it)           // 0x42a290 + the slots are laid out again (0x420ab0)
{
    if (held_ == it) held_ = 0;
    if (fly_ == it) fly_ = 0;
    for (size_t i = 0; i < items_.size(); ++i)
        if (items_[i] == it) { items_.erase(items_.begin() + i); break; }
    delete it->vpe; delete it->elem; delete it->light; delete it->vp; delete it->w3; delete it->model;
    delete it;
}

std::vector<Hud::Entry> Hud::items() const
{
    std::vector<Entry> v;
    for (int s = 0; s < 6; ++s)
        for (size_t i = 0; i < items_.size(); ++i)
            if (items_[i]->slot == s) { Entry e; e.name = items_[i]->name; e.desc = items_[i]->desc; v.push_back(e); }
    return v;
}

bool Hud::holdFromPanel(const std::string& name)
{
    HudItem* it = find(name);
    if (!it || !active()) return false;
    if (held_) release();
    held_ = it;
    dragging_ = false;
    return true;
}

// 0x421880: the combinations (either order)
static const char* kCombos[][4] = {
    { "ikp1", "ikp2", "ikp3", "Krabby Patty" },
    { "ikp2", "ikp7", "ikp6", "Krabby Patty on Plate" },
    { "ikp1", "ikp5", "ikp7", "Plain Patty on Plate" },
    { "ikp3", "ikp5", "ikp6", "Krabby Patty on Plate" },
    { "iuc1", "ict1", "ict2", "Urchin Chip Pie" },
    { "ifm1", "ikc1", "ikc2", "Kelpsi Can Full of Diet Cola" },
};

bool Hud::combine(HudItem* a, HudItem* b)
{
    for (size_t i = 0; i < sizeof kCombos / sizeof kCombos[0]; ++i) {
        bool ab = strcasecmp(a->name.c_str(), kCombos[i][0]) == 0 && strcasecmp(b->name.c_str(), kCombos[i][1]) == 0;
        bool ba = strcasecmp(a->name.c_str(), kCombos[i][1]) == 0 && strcasecmp(b->name.c_str(), kCombos[i][0]) == 0;
        if (!ab && !ba) continue;
        int slot = b->slot;
        std::string nn = kCombos[i][2], nd = kCombos[i][3];
        remove(a);
        remove(b);
        g_->state().giveItem(nn.c_str());                  // 0x4169a0
        HudItem* it = new HudItem;
        it->name = nn; it->desc = nd; it->slot = slot;
        items_.push_back(it);
        g_->log("HUD: %s + %s -> %s\n", kCombos[i][0], kCombos[i][1], nn.c_str());
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------ pointer
void Hud::release()                     // 0x420f20
{
    held_ = 0;
    dragging_ = false;
}

bool Hud::onEscape()
{
    if (!held_) return false;
    release();
    return true;
}

static SceneObject* sceneObjectAt(Game* g, int x, int y)
{
    return g->scene() ? g->scene()->pickAll(x, y) : 0;         // 0x43a310: triggers, characters / items, 2D objects
}

void Hud::dropAt(int x, int y)          // 0x420b20: the item is let go at x,y
{
    HudItem* it = held_;
    if (!it) return;
    release();
    int slot;
    Spot sp = spotAt(x, y, &slot);
    if (sp == Slot) {
        HudItem* other = itemInSlot(slot);
        if (!other || other == it) return;                 // back into a free (or its own) slot: it keeps the slot
        if (!combine(it, other)) g_->switchTo("H000F");    // "that doesn't work"
        return;
    }
    if (sp != None) return;                                // a button of the bar: nothing
    SceneObject* o = sceneObjectAt(g_, x, y);
    if (!o) return;
    setOpen(false);                                        // 0x421390(0)
    GameScene* sc = g_->scene();
    bool used = sc && sc->useItem(o, it->name.c_str());    // base vt+0x48 (0x415c30)
    g_->log("HUD: %s used on '%s': %s\n", it->name.c_str(), o->description.c_str(), used ? "accepted" : "not accepted");
    if (used) remove(it);                                  // 0x42a290, the item count goes down
}

bool Hud::onMouseDown(int x, int y)     // 0x420d50
{
    mx_ = x; my_ = y;
    if (!active()) return false;
    if (held_) {                                           // an item is in hand (picked from the panel, or earlier): this click uses it
        dropAt(x, y);
        return true;
    }
    int slot;
    Spot sp = spotAt(x, y, &slot);
    if (sp == Chest) { setOpen(!open_); return true; }
    if (sp == Map) {                                       // game+0x57 = the scene to return to, then the chapter's map scene
        char b[32];
        snprintf(b, sizeof b, "c%dl1s1p1", g_->state().chapter);
        g_->log("HUD: map -> %s\n", b);
        g_->switchTo(b);
        return true;
    }
    if (sp == Menu) { g_->log("HUD: main menu (STUB: the pause menu is not reconstructed yet)\n"); return true; }
    if (sp == Hint) { setOpen(false); hint(); return true; }
    if (sp == Slot) {
        HudItem* it = itemInSlot(slot);
        if (it) { held_ = it; dragging_ = true; downX_ = x; downY_ = y; }
        return true;
    }
    if (open_ && y < 0x40) return true;
    return false;
}

bool Hud::onMouseUp(int x, int y)
{
    mx_ = x; my_ = y;
    if (!held_ || !dragging_) return false;
    dragging_ = false;
    int dx = x - downX_, dy = y - downY_;
    int slot;
    Spot sp = spotAt(x, y, &slot);
    if (sp == Slot && itemInSlot(slot) == held_ && dx * dx + dy * dy < 100) return true;   // a click: the item stays in hand
    dropAt(x, y);                                          // 0x420b20 on the release
    return true;
}

void Hud::onMove(int x, int y) { mx_ = x; my_ = y; }

std::string Hud::hintText()             // the first help line whose condition holds; a line that is another line's id (F121A -> F000D) is that line's text
{
    const std::vector<sbscript::HelpEntry>& h = g_->help().entries();
    for (size_t i = 0; i < h.size(); ++i)
        if (!h[i].condition.empty() && g_->cond(h[i].condition.c_str())) {
            std::string t = h[i].text;
            const sbscript::HelpEntry* alias = g_->help().find(t);
            if (alias) t = alias->text;
            if (t.size() >= 2 && t[0] == '"' && t[t.size() - 1] == '"') t = t.substr(1, t.size() - 2);
            return t;
        }
    return std::string();
}

void Hud::hint()                        // game vt+0x100
{
    std::string t = hintText();
    if (t.empty()) { g_->log("HUD: hint: no help line applies\n"); return; }
    delete hintBlock_;
    hintBlock_ = new TextBlock;
    hintBlock_->show(g_, 5, 440, 10100, false, t, 630);   // STUB: the voice (sounds/help.omt) is not played
    hintMs_ = 1500.0f + 50.0f * (float)t.size();
}

bool Hud::overHotspot(int x, int y)
{
    if (!active()) return false;
    int slot;
    Spot sp = spotAt(x, y, &slot);
    if (sp == Chest || sp == Map || sp == Menu || sp == Hint) return true;
    if (sp == Slot) return itemInSlot(slot) != 0;
    if (y < 0x40 && open_) return false;
    GameScene* s = g_->scene();
    if (!s) return false;
    if (sceneObjectAt(g_, x, y) != 0) return true;
    return s->hover(x, y) != 0;
}

// 0x4201c0: the caption under the pointer
bool Hud::caption(std::string* out)
{
    if (!active()) return false;
    int slot;
    Spot sp = spotAt(mx_, my_, &slot);
    char b[200];
    if (!held_) {
        switch (sp) {
        case Chest: *out = open_ ? "Hide Inventory" : "Show Inventory"; return true;
        case Map: *out = "Use Map"; return true;
        case Hint: *out = "Get a Hint"; return true;
        case Menu: *out = "Main Menu"; return true;
        case Slot: { HudItem* it = itemInSlot(slot); *out = it ? it->desc : "Empty"; return true; }
        default: return false;
        }
    }
    *out = held_->desc;                                    // the default: what is in hand
    if (sp == Slot) {
        HudItem* o = itemInSlot(slot);
        if (o && o != held_) { snprintf(b, sizeof b, "Use %s with %s", held_->desc.c_str(), o->desc.c_str()); *out = b; }
        return true;
    }
    if (sp != None) return true;
    SceneObject* o = sceneObjectAt(g_, mx_, my_);
    if (!o || o->description.empty()) return true;
    const char* fmt = "Use %s with %s";
    if (o->kind == SceneObject::Char) {
        std::string art = lowerStr(o->art);
        fmt = (art == "squidon" || art == "snakey" || art == "sb") ? "Use %s on %s" : "Give %s to %s";
    } else if (o->kind == SceneObject::Trigger && strcasecmp(o->itemAccept.c_str(), "inc1") == 0 &&
               strcasecmp(held_->name.c_str(), "inc1") == 0) {
        fmt = "Swap %s with %s";
    }
    snprintf(b, sizeof b, fmt, held_->desc.c_str(), o->description.c_str());
    *out = b;
    return true;
}

// ------------------------------------------------------------------------------------------------ 3D pictures
static OMedia3DAbstractRasterizer* sharedRasterizer(Game* g)
{
    static OMedia3DAbstractRasterizer* ras = 0;         // the exe makes one per process (0x40ceb0); World3D made it
    if (!ras && g->world3d() && g->world3d()->world3d()) ras = g->world3d()->world3d()->rasterizer;
    return ras;
}

bool Hud::ensure3D(HudItem* it)
{
    if (it->w3) return true;
    if (it->tried) return false;
    OMedia3DAbstractRasterizer* ras = sharedRasterizer(g_);
    if (!ras) return false;
    it->tried = true;
    OMedia3DGrannyModel* m = new OMedia3DGrannyModel;
    if (!m->load(g_->root() + "/models/items/" + lowerStr(it->name) + "_model")) {
        g_->log("HUD: item model %s: %s\n", it->name.c_str(), m->get_error().c_str());
        delete m;
        return false;
    }
    m->update_pose();
    OMedia3DShape* sh = m->get_shape();
    std::vector<OMedia3DPoint>* vs = sh ? sh->get_vertices() : 0;
    if (!vs || vs->empty()) { delete m; return false; }
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
    for (size_t i = 0; i < vs->size(); ++i) {
        const float v[3] = { (*vs)[i].x, (*vs)[i].y, (*vs)[i].z };
        for (int k = 0; k < 3; ++k) { if (v[k] < lo[k]) lo[k] = v[k]; if (v[k] > hi[k]) hi[k] = v[k]; }
    }
    it->cx = (lo[0] + hi[0]) * 0.5f; it->cy = (lo[1] + hi[1]) * 0.5f; it->cz = (lo[2] + hi[2]) * 0.5f;
    float r2 = 0;
    for (size_t i = 0; i < vs->size(); ++i) {
        float dx = (*vs)[i].x - it->cx, dy = (*vs)[i].y - it->cy, dz = (*vs)[i].z - it->cz;
        float d = dx * dx + dy * dy + dz * dz;
        if (d > r2) r2 = d;
    }
    it->radius = sqrtf(r2) > 0.001f ? sqrtf(r2) : 1.0f;
    it->model = m;
    // a world of its own: camera at the origin looking down +z, the model 400 away, sized to fill the 64x64 box
    it->w3 = new OMedia3DWorld;
    it->w3->set_framerate_mode(omfrc_Max);
    it->w3->setbackground(NULL);
    it->w3->rasterizer = ras;
    it->w3->far_z = 4500.0f;
    it->w3->near_z = 10.0f;
    it->w3->ambient.setred8bits(125); it->w3->ambient.setgreen8bits(125); it->w3->ambient.setblue8bits(125);
    it->focal = 58.0f * 400.0f / (2.0f * it->radius);
    it->w3->focal = it->focal;
    it->light = new OMedia3DLight;
    it->light->link(it->w3);
    it->light->kind = 1;
    it->light->diffuse[0] = it->light->diffuse[1] = it->light->diffuse[2] = 1.0f;
    it->light->place(-300.0f, 300.0f, -200.0f);
    it->elem = new OMedia3DShapeElement;
    it->elem->setshape(sh);
    it->elem->link(it->w3);
    it->vp = new OMediaViewPort(it->w3);
    OMediaRect b(0, 0, 64, 64);
    it->vp->setbounds(&b);
    it->vp->place(0, 0, 0);
    it->vp->set_angle(0, 0, 0);
    it->vpe = new OMediaVPElement;
    it->vpe->set_viewport(it->vp);
    it->vpe->link(g_->world());
    it->vpe->place(it->x, it->y, kZItem);
    it->shown = true;
    return true;
}

static void poseElement(HudItem* it)
{
    OMedia3DVector v(-it->cx, -it->cy, -it->cz);
    v.rotate(0, (short)(it->yaw & 0x3fff), 0);
    it->elem->place(v.x, v.y, v.z + 400.0f);
    it->elem->set_angle(0, (short)(it->yaw & 0x3fff), 0);
}

void Hud::placeItem(HudItem* it, float x, float y)
{
    it->x = x; it->y = y;
    if (it->vpe) it->vpe->place(x, y, kZItem);
}

// ------------------------------------------------------------------------------------------------ frame
static float smooth(float t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3.0f - 2.0f * t); }

void Hud::build()
{
    // 0x41f6a0: the buttons and the six bubbles, all made hidden; applyState() shows the right ones.
    // port: built with the first scene, and linked again after every scene load (relink()): the 2D world draws in link order
    // (its sort mode is None, as the exe's), so what is linked before the scene's own objects would be drawn under them.
    inv_.e = mk("inv", 0, 0);       invT_.e = mk("chest1t", 0, 0);
    inv2_.e = mk("inv2", 0, 0);     inv2T_.e = mk("chest2t", 0, 0);
    help_.e = mk("help", 448, 0);   helpT_.e = mk("helpt", 448, 0);
    menu_.e = mk("menu", 512, 0);   menuT_.e = mk("menut", 512, 0);
    map_.e = mk("map", 576, 0);     mapT_.e = mk("mapt", 576, 0);
    for (int i = 0; i < 6; ++i) {
        bubble_[i].e = mk("bubble1", (float)((i + 1) * 64), 0);
        bubbleT_[i].e = mk("bubblet", (float)((i + 1) * 64), 0);
    }
}

void Hud::relink()
{
    Btn* order[] = { &inv_, &invT_, &inv2_, &inv2T_, &help_, &helpT_, &menu_, &menuT_, &map_, &mapT_ };
    for (int i = 0; i < 6; ++i) { if (bubbleT_[i].e) bubbleT_[i].e->link(g_->world()); if (bubble_[i].e) bubble_[i].e->link(g_->world()); }
    for (size_t i = 0; i < sizeof order / sizeof order[0]; ++i) if (order[i]->e) order[i]->e->link(g_->world());
    for (size_t i = 0; i < items_.size(); ++i) if (items_[i]->vpe) items_[i]->vpe->link(g_->world());
    for (int i = 0; i < 10; ++i) if (sparkles_[i]) sparkles_[i]->link(g_->world());
}

void Hud::sparkle(int x, int y)             // 0x41ebe0
{
    sparkleMs_ = 3000.0f;
    sparkleX_ = (float)(short)x; sparkleY_ = (float)(short)y;
    g_->log("HUD: sparkle at %d,%d (0x41ebe0)\n", x, y);
}

void Hud::clearSparkles()
{
    for (int i = 0; i < 10; ++i) { delete sparkles_[i]; sparkles_[i] = 0; sparkleAge_[i] = 0; }
}

// 0x41ec90: one sparkle canvas (sparkle1..3 of sb2.omt) at a random offset (10..41 px) in a random quadrant of the centre
void Hud::spawnSparkle()
{
    int i = 0;
    while (i < 10 && sparkles_[i]) ++i;
    if (i == 10) return;
    static const char* names[3] = { "sparkle1", "sparkle2", "sparkle3" };
    OMediaDataBase* db = g_->sb2Db();
    OMediaCanvas* c = db ? (OMediaCanvas*)db->get_object(OMediaCanvas::db_type, std::string(names[rand() % 3])) : 0;
    if (!c) return;
    OMediaCanvasElement* e = new OMediaCanvasElement(c);
    int cx = (int)sparkleX_, cy = (int)sparkleY_;
    int dx = (rand() & 31) + 10, dy = (rand() & 31) + 10;
    switch (rand() & 3) {
    case 0: e->place((float)(cx + dx), (float)(cy + dy), 10200.0f); break;
    case 1: e->place((float)(cx - dx), (float)(cy - dy), 10200.0f); break;
    case 2: e->place((float)(cx - dx), (float)(cy + dy), 10200.0f); break;
    default: e->place((float)(cx + dx), (float)(cy - dy), 10200.0f); break;
    }
    e->link(g_->world());
    sparkles_[i] = e; sparkleAge_[i] = 0;
}

// 0x41e3d0 (the effect part): every frame while the effect runs the sparkles age (gone after 250 ms), a new one is made
// when more than 10 ms went by, and everything goes when the 3000 ms are used up.
void Hud::updateSparkle(float dtMs)
{
    if (sparkleMs_ == 0 || fly_) return;
    for (int i = 0; i < 10; ++i)
        if (sparkles_[i] && (sparkleAge_[i] += dtMs) >= 250.0f) { delete sparkles_[i]; sparkles_[i] = 0; sparkleAge_[i] = 0; }
    sparkleAcc_ += dtMs;
    sparkleMs_ -= dtMs;
    if (sparkleMs_ <= 0) { sparkleMs_ = 0; clearSparkles(); sparkleAcc_ = 0; return; }
    if (sparkleAcc_ > 10.0f) { spawnSparkle(); sparkleAcc_ = 0; }
}

void Hud::update(float dtMs)
{
    updateSparkle(dtMs);
    if (!built_ && g_->scene()) { built_ = true; build(); lastScene_ = g_->scene(); }
    else if (built_ && g_->scene() != lastScene_) { lastScene_ = g_->scene(); relink(); }
    // port: tests only.  SB_TEST_ITEMS="isp1:Shovel and Pail,ikp5:Clean Plate" puts items in the inventory once a scene runs.
    if (!testItemsDone_ && wanted()) {
        testItemsDone_ = true;
        const char* t = getenv("SB_TEST_ITEMS");
        while (t && *t) {
            std::string n, d;
            while (*t && *t != ':' && *t != ',') n += *t++;
            if (*t == ':') { ++t; while (*t && *t != ',') d += *t++; }
            if (*t == ',') ++t;
            if (!n.empty()) { addItem(n.c_str(), d.c_str()); g_->state().giveItem(n.c_str()); if (fly_) flyMs_ = 1.0f; }
        }
    }
    if (testItemsDone_ && !testHoldDone_ && !fly_) {          // port: tests only: SB_TEST_MOUSE=x:y moves the pointer, SB_TEST_HOLD=name takes that item in hand
        testHoldDone_ = true;
        int tx, ty;
        if (getenv("SB_TEST_MOUSE") && sscanf(getenv("SB_TEST_MOUSE"), "%d:%d", &tx, &ty) == 2) g_->mouseMove(tx, ty);
        if (getenv("SB_TEST_HOLD")) holdFromPanel(getenv("SB_TEST_HOLD"));
    }
    bool on = wanted();
    shownNow_ = on;
    if (getenv("SB_HUD_DEBUG")) { static int n = 0; if (++n % 60 == 0) g_->log("HUD: wanted=%d open=%d fly=%d held=%d items=%d mouse=%d,%d cursor=%d\n", on, open_, fly_ != 0, held_ != 0, (int)items_.size(), mx_, my_, g_->cursor() ? g_->cursor()->ok() : -1); }
    if (!on) {
        if (held_) release();
        open_ = false;
    }
    if (hintBlock_ && (hintMs_ -= dtMs) <= 0) { delete hintBlock_; hintBlock_ = 0; }

    // the pickup flight (+0x130 .. +0x140)
    if (fly_) {
        flyT_ += dtMs;
        float t = flyT_ / flyMs_;
        float k = smooth((t - 0.4f) / 0.6f);               // port: rests where it appeared for 40 percent, then flies to its slot
        float tx = (float)((fly_->slot + 1) * 64), ty = 0;
        placeItem(fly_, flyX_ + (tx - flyX_) * k, flyY_ + (ty - flyY_) * k);
        if (t >= 1.0f) { placeItem(fly_, tx, ty); fly_ = 0; }
    }

    hover_ = None; hoverSlot_ = -1;
    if (on) hover_ = spotAt(mx_, my_, &hoverSlot_);

    // buttons (0x4208f0 / 0x420990: the faint variant, the solid one under the pointer)
    bool hc = on && hover_ == Chest, hm = on && hover_ == Map, hh = on && open_ && hover_ == Hint, hn = on && open_ && hover_ == Menu;
    show(inv_, on && !open_ && hc);   show(invT_, on && !open_ && !hc);
    show(inv2_, on && open_ && hc);   show(inv2T_, on && open_ && !hc);
    show(map_, hm);                   show(mapT_, on && !hm);
    show(help_, hh);                  show(helpT_, on && open_ && !hh);
    show(menu_, hn);                  show(menuT_, on && open_ && !hn);
    for (int i = 0; i < 6; ++i) {
        bool occ = itemInSlot(i) != 0;
        show(bubble_[i], on && open_ && occ);
        show(bubbleT_[i], on && open_ && !occ);
    }

    // the items
    for (size_t i = 0; i < items_.size(); ++i) {
        HudItem* it = items_[i];
        bool vis = (on && open_) || it == held_ || it == fly_;
        if (!on && it != fly_) vis = false;
        if (!vis) { showItem(it, false); continue; }
        if (!ensure3D(it)) continue;
        if (it == held_) {                                  // 0x4201c0: follows the pointer, the box kept on the screen
            int ix = mx_ - 0x20, iy = my_ - 0x20;
            if (ix < 1) ix = 1; if (iy < 1) iy = 1; if (ix > 0x23f) ix = 0x23f; if (iy > 0x19f) iy = 0x19f;
            placeItem(it, (float)ix, (float)iy);
        } else if (it != fly_) {
            placeItem(it, (float)((it->slot + 1) * 64), 0);
        }
        if (hover_ == Slot && itemInSlot(hoverSlot_) == it && !held_) it->yaw += (int)(dtMs * 6.0f);   // port: the item under the pointer turns
        poseElement(it);
        it->model->update_pose();
        it->w3->update();
        showItem(it, true);
    }
}

// ------------------------------------------------------------------------------------------------ companion pictures
std::string Hud::iconDir() const        // absolute, so the panel (Java, same process) needs no idea of the working directory
{
    char buf[4096];
    std::string root = g_->root();
    if (realpath(root.c_str(), buf)) root = buf;
    return root + "/companion_icons";
}

static unsigned crcTable[256];
static void crcInit()
{
    if (crcTable[1]) return;
    for (unsigned n = 0; n < 256; ++n) {
        unsigned c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crcTable[n] = c;
    }
}
static unsigned crcUpdate(unsigned c, const unsigned char* p, size_t n)
{
    for (size_t i = 0; i < n; ++i) c = crcTable[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c;
}
static void put32(std::string& s, unsigned v) { for (int i = 3; i >= 0; --i) s += (char)(v >> (8 * i)); }
static void chunk(std::string& out, const char* type, const std::string& data)
{
    put32(out, (unsigned)data.size());
    std::string td = std::string(type, 4) + data;
    out += td;
    crcInit();
    put32(out, crcUpdate(0xffffffffu, (const unsigned char*)td.data(), td.size()) ^ 0xffffffffu);
}

// An RGBA PNG with stored (uncompressed) deflate blocks: no zlib needed, 96x96 is 37 KB.
static bool writePng(const std::string& path, int w, int h, const std::vector<unsigned>& argb)
{
    std::string raw;
    for (int y = 0; y < h; ++y) {
        raw += (char)0;
        for (int x = 0; x < w; ++x) {
            unsigned c = argb[y * w + x];
            raw += (char)(c >> 16); raw += (char)(c >> 8); raw += (char)c; raw += (char)(c >> 24);
        }
    }
    std::string z;
    z += (char)0x78; z += (char)0x01;
    for (size_t pos = 0; pos < raw.size() || pos == 0;) {
        size_t n = raw.size() - pos; if (n > 65535) n = 65535;
        bool last = pos + n >= raw.size();
        z += (char)(last ? 1 : 0);
        z += (char)(n & 0xff); z += (char)(n >> 8); z += (char)(~n & 0xff); z += (char)((~n >> 8) & 0xff);
        z.append(raw, pos, n);
        pos += n;
        if (last) break;
    }
    unsigned a = 1, b = 0;
    for (size_t i = 0; i < raw.size(); ++i) { a = (a + (unsigned char)raw[i]) % 65521; b = (b + a) % 65521; }
    put32(z, (b << 16) | a);
    std::string out("\x89PNG\r\n\x1a\n", 8);
    std::string ihdr;
    put32(ihdr, (unsigned)w); put32(ihdr, (unsigned)h);
    ihdr += (char)8; ihdr += (char)6; ihdr += (char)0; ihdr += (char)0; ihdr += (char)0;
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", std::string());
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok && rename(tmp.c_str(), path.c_str()) == 0;
}

// The model's picture with an alpha channel: rendered twice, on black and on white, then un-matted.
bool Hud::renderIcon(HudItem* it, int size, std::vector<unsigned>* rgba)
{
    if (!ensure3D(it)) return false;
    static OMediaDXSurfaceWorld* off = 0;
    static int offSize = 0;
    if (!off || offSize != size) {
        delete off;
        off = new OMediaDXSurfaceWorld(false);
        off->create((short)size, (short)size);
        offSize = size;
    }
    if (!off->port_surface() || !off->port_surface()->fbo) return false;
    float f0 = it->w3->focal;
    it->w3->focal = it->focal * (float)size / 64.0f;
    poseElement(it);
    it->w3->update();
    std::vector<unsigned> px[2];
    OMediaRGBColor cols[2] = { OMediaRGBColor(0, 0, 0), OMediaRGBColor(0xffff, 0xffff, 0xffff) };
    OMediaCanvas* wrap = new OMediaCanvas;
    wrap->setgfxworld(off);
    OMediaViewPort* ivp = new OMediaViewPort(it->w3);
    OMediaRect b(0, 0, size, size);
    ivp->setbounds(&b);
    ivp->place(0, 0, 0);
    ivp->set_angle(0, 0, 0);
    ivp->setcanvas(wrap);
    for (int pass = 0; pass < 2; ++pass) {
        off->fill(&cols[pass]);
        ivp->render();
        px[pass].resize(size * size);
        omgl_surface_read(off->port_surface(), &px[pass][0]);
    }
    ivp->setcanvas(0);
    delete ivp;
    wrap->detach_gfxworld();                               // port: the wrapper does not own the offscreen world
    delete wrap;
    it->w3->focal = f0;
    rgba->assign(size * size, 0);
    for (int i = 0; i < size * size; ++i) {
        int dr = (int)((px[1][i] >> 16) & 255) - (int)((px[0][i] >> 16) & 255);
        int dg = (int)((px[1][i] >> 8) & 255) - (int)((px[0][i] >> 8) & 255);
        int db = (int)(px[1][i] & 255) - (int)(px[0][i] & 255);
        int d = (dr + dg + db) / 3;
        float a = 1.0f - (float)d / 255.0f;
        if (a < 0.02f) continue;
        if (a > 1) a = 1;
        int r = (int)(((px[0][i] >> 16) & 255) / a), g = (int)(((px[0][i] >> 8) & 255) / a), bl = (int)((px[0][i] & 255) / a);
        if (r > 255) r = 255; if (g > 255) g = 255; if (bl > 255) bl = 255;
        (*rgba)[i] = ((unsigned)(a * 255.0f + 0.5f) << 24) | ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)bl;
    }
    return true;
}

bool Hud::ensureIcon(const std::string& name)
{
    std::string dir = iconDir(), path = dir + "/" + lowerStr(name) + ".png";
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return true;
    HudItem* it = find(name);
    if (!it) return false;
    mkdir(dir.c_str(), 0755);
    std::vector<unsigned> px;
    const int size = 96;
    if (!renderIcon(it, size, &px)) return false;
    bool ok = writePng(path, size, size, px);
    g_->log("HUD: companion picture %s: %s\n", path.c_str(), ok ? "written" : "FAILED");
    return ok;
}

}  // namespace sbgame
