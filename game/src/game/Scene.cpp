#include "Scene.h"
#include "MiniGame.h" // chapter 2: maze TRIG picking pass (0x43a310)
#include <stdlib.h>
#include "Game.h"
#include "World3D.h"
#include "DialogueUI.h"
#include "SceneScriptHost.h"
#include <ctype.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>

namespace sbgame {

using namespace sb::scene;

static bool nonEmptyEvent(const std::string& e)
{
    // 0x415a70: the empty marker is " " (0x4544ec) or "0" (0x454554)
    return !e.empty() && e != " " && e != "0";
}

SceneObject::SceneObject()
    : kind(Obj2D), tag(0), x1(0), y1(0), x2(0), y2(0), gridX(0), gridY(0), used(false), px(0), py(0), pz(0),
      postRender(false), rolloverCond(false), clickable(false), userCtl(false), radius(0), rotation(0), pickup(0), addInv(0),
      requireEnter(0), isDoor(0), useCond(0), faceHeading(0), facePlayer(0), hidden3D(false), elem(0), nFrames(0), fps(0), clickToPlay(0),
      interval(0), continuous(0), framePerClick(0), loops(0), reverse(0), soundFrame(0), stayOnLast(0),
      allowToggle(0), startOn(0), eventAtBegin(0), playing(false), frame(0), dir(1), loopsDone(0), frameMs(0),
      intervalSec(0), paused(false), eventPending(false)
{
}

GameScene::GameScene(Game* game)
    : game_(game), sceneId_(0), artDb_(0), artFile_(0), background_(0), skyScroll_(0), dialogue_(0), barTop_(0), barBot_(0),
      barState_(0), barHold_(0), topY_(0), botY_(240), shake_(0), shakeCount_(0), shakeX_(0), shakeY_(0), lock_(0), doorTrig_(0), lastClicked_(0)
{
    sky_[0] = sky_[1] = 0; skyX_[0] = skyX_[1] = 0;
}

GameScene::~GameScene()
{
    delete dialogue_;
    // elements unlink themselves from the world; the objects own nothing else
    for (size_t i = 0; i < objects_.size(); ++i) { delete objects_[i]->elem; delete objects_[i]; }
    delete sky_[0]; delete sky_[1];
    delete background_;
    delete barTop_;
    delete barBot_;
    delete (OmtFile*)artFile_;
}

OMedia2DWorld* GameScene::world() { return game_->world(); }

OMediaCanvas* GameScene::canvasNamed(const std::string& n)
{
    OMediaCanvas* c = 0;
    if (artDb_) c = (OMediaCanvas*)artDb_->get_object(OMediaCanvas::db_type, n);   // scene+0x23c
    if (!c && game_->sb2Db()) c = (OMediaCanvas*)game_->sb2Db()->get_object(OMediaCanvas::db_type, n);  // game+0x88 +0x18
    return c;
}

// ------------------------------------------------------------------------------------------------ load
bool GameScene::load(unsigned id)
{
    sceneId_ = id;
    char nm[32];
    sprintf(nm, "c%dl%ds%d", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf));
    name_ = nm;
    OmtFile* art = new OmtFile;
    artFile_ = art;
    // sceneart\c%dl%ds%d.omt, then <data root>sceneart\... (0x437210)
    if (!art->open(game_->root() + "/sceneart/" + name_ + ".omt")) return false;
    artDb_ = art->db;

    // the background: object "background" at 0,0 and depth -10000 (vt+0xac(name, 0, 0, -10000.0))
    OMediaCanvas* bg = canvasNamed("background");
    if (bg) {
        background_ = new OMediaCanvasElement(bg);
        background_->link(world());
        background_->place(0, 0, -10000.0f);
    } else {
        game_->log("scene %s has no 'background' canvas\n", name_.c_str());
    }

    std::string err;
    if (!sb::scene::load(game_->root() + "/scenes/" + name_ + ".scn", data_, err)) {
        game_->log("scene %s: %s\n", name_.c_str(), err.c_str());
        return false;
    }
    const Camera& cm = data_.camera.data.camera;
    exits_[0] = cm.o0192.text();   // +0x3fa
    exits_[1] = cm.o01b2.text();   // +0x41a
    exits_[2] = cm.o01d2.text();   // +0x43a
    exits_[3] = cm.o01f2.text();   // +0x45a
    enterEvent_ = cm.o0244.text(); // +0x4ac
    enterCond_ = cm.o0264.text();  // +0x4cc
    buildObjects();
    buildSky();
    dialogue_ = new DialogueUI(game_);
    dialogue_->load(id);
    return true;
}

static void baseFields(SceneObject* o, const Text32& ev, const Text32& cond, const Text32& alt, const Text32& desc,
                       short x1, short y1, short x2, short y2, short gx, short gy)
{
    o->event = ev.text(); o->condition = cond.text(); o->altEvent = alt.text(); o->description = desc.text();
    o->x1 = x1; o->y1 = y1; o->x2 = x2; o->y2 = y2; o->gridX = gx; o->gridY = gy;
}

void GameScene::buildObjects()
{
    for (size_t i = 0; i < data_.elements.size(); ++i) {
        const Record& r = data_.elements[i];
        SceneObject* o = new SceneObject;
        o->tag = r.tag;
        switch (r.tag) {
        case Object2DTag: {
            const Object2D& d = r.data.object2D;
            o->kind = SceneObject::Obj2D;
            o->art = d.o0174.text();
            o->px = d.o0194; o->py = d.o0198; o->pz = d.o019c; o->postRender = d.o01a0 != 0;
            baseFields(o, d.o0020, d.o0040, d.o0060, d.o0120, (short)d.o0014, (short)d.o0016, (short)d.o0018, (short)d.o001a, (short)d.o016c, (short)d.o016e);
            o->visibleCond = d.o0100.text(); o->soundName = d.o0140.text();
            break; }
        case AnimationTag: {
            const Animation& d = r.data.animation;
            o->kind = SceneObject::Anim2D;
            o->art = d.o0174.text();
            o->px = d.o0194; o->py = d.o0198; o->pz = d.o019c; o->postRender = d.o01a0 != 0;
            baseFields(o, d.o0020, d.o0040, d.o0060, d.o0120, (short)d.o0014, (short)d.o0016, (short)d.o0018, (short)d.o001a, 0, 0);
            o->visibleCond = d.o0100.text(); o->soundName = d.o0140.text();
            o->nFrames = (short)d.o01b0; o->fps = (short)d.o01b2; o->soundFrame = (short)d.o01b4;
            o->clickToPlay = (short)d.o01b6; o->interval = (short)d.o01b8; o->continuous = (short)d.o01ba;
            o->loops = (short)d.o01bc; o->reverse = (short)d.o01be; o->framePerClick = (short)d.o01c0;
            o->stayOnLast = (short)d.o01ca; o->allowToggle = (short)d.o01d0; o->startOn = (short)d.o01d2;
            o->eventAtBegin = (short)d.o01d4;
            break; }
        case SpotTag: {
            const Spot& d = r.data.spot;
            o->kind = SceneObject::Spot;
            o->px = d.o0194; o->py = d.o0198;
            baseFields(o, d.o0020, d.o0040, d.o0060, d.o0120, (short)d.o0014, (short)d.o0016, (short)d.o0018, (short)d.o001a, (short)d.o016c, (short)d.o016e);
            o->soundName = d.o0140.text(); o->rolloverCond = d.o01b0 != 0;
            break; }
        case ItemTag: {
            const Item& d = r.data.item;
            o->kind = SceneObject::Item;
            o->art = d.o01e4.text();
            o->px = d.o0230; o->py = d.o0234; o->pz = d.o0238;
            baseFields(o, d.o0020, d.o0040, d.o0060, d.o0120, (short)d.o0014, (short)d.o0016, (short)d.o0018, (short)d.o001a, (short)d.o016c, (short)d.o016e);
            o->visibleCond = d.o0100.text(); o->clickable = d.o0246 != 0;
            o->radius = d.o023c; o->rotation = (short)d.o0240; o->pickup = (short)d.o0278; o->addInv = (short)d.o027a;
            break; }
        case CharacterTag: {
            const Character& d = r.data.character;
            o->kind = SceneObject::Char;
            o->art = d.o01e4.text();
            o->px = d.o0230; o->py = d.o0234; o->pz = d.o0238;
            baseFields(o, d.o0020, d.o0040, d.o0060, d.o0120, (short)d.o0014, (short)d.o0016, (short)d.o0018, (short)d.o001a, (short)d.o016c, (short)d.o016e);
            o->itemAccept = d.o0080.text(); o->itemEvent = d.o00a0.text(); o->wrongItemEvent = d.o00c0.text();
            o->haveItemEvent = d.o00e0.text(); o->visibleCond = d.o0100.text(); o->clickable = d.o0246 != 0;
            o->userCtl = d.o027e != 0;
            o->radius = d.o023c; o->rotation = (short)d.o0240; o->facePlayer = (short)d.o02a0; o->faceHeading = (short)d.o02a4;
            break; }
        case TriggerTag: {
            const Trigger& d = r.data.trigger;
            o->kind = SceneObject::Trigger;
            o->px = d.o0230; o->py = d.o0234; o->pz = d.o0238;
            baseFields(o, d.o0020, d.o0040, d.o0060, d.o0120, (short)d.o0014, (short)d.o0016, (short)d.o0018, (short)d.o001a, (short)d.o016c, (short)d.o016e);
            o->itemAccept = d.o0080.text(); o->itemEvent = d.o00a0.text(); o->wrongItemEvent = d.o00c0.text();
            o->haveItemEvent = d.o00e0.text(); o->visibleCond = d.o0100.text(); o->soundName = d.o0140.text();
            o->radius = d.o023c; o->requireEnter = (short)d.o0278; o->rotation = (short)d.o027a; o->isDoor = (short)d.o027c;
            o->useCond = (short)d.o027e; o->doorAnim = d.o0280.text();
            break; }
        default:
            delete o;
            continue;   // 3DOB / 3LIG / SCEN: empty classes
        }
        // the x1.. rectangle fields are shorts in the file
        if (o->kind == SceneObject::Obj2D || o->kind == SceneObject::Anim2D) {
            if (o->kind == SceneObject::Obj2D) {
                OMediaCanvas* c = canvasNamed(o->art);       // 0x401210: scene database, then the shared one
                if (c) { o->elem = new OMediaCanvasElement(c); o->elem->link(world()); }
                else game_->log("2DOB '%s' has no canvas\n", o->art.c_str());
            } else {
                // 0x407f90: the frames "%s%02d"
                for (int f = 1; f <= o->nFrames; ++f) {
                    char fn[64];
                    sprintf(fn, "%s%02d", o->art.c_str(), f);
                    OMediaCanvas* c = canvasNamed(fn);
                    if (!c) break;
                    o->frames.push_back(c);
                }
                if (!o->frames.empty()) { o->elem = new OMediaCanvasElement(o->frames[0]); o->elem->link(world()); }
                else game_->log("ANIM '%s' has no frames\n", o->art.c_str());
            }
            place(o);
        }
        objects_.push_back(o);
    }
    for (size_t i = 0; i < objects_.size(); ++i) {
        SceneObject* o = objects_[i];
        if (o->kind == SceneObject::Anim2D) {
            // ctor 0x407f90 end: playing, pausing off; the properties decide the start
            o->frame = 0; o->dir = 1; o->loopsDone = 0;
            o->playing = o->startOn != 0 && o->clickToPlay == 0 && o->framePerClick == 0;
            if (o->continuous) o->playing = o->startOn != 0;
            if (!o->startOn) setHidden(o, true);
        }
    }
}

// 0x43bb60 (sky setup) / 0x43ae70 (scroll): two 640-wide tiles of the camera's Sky Index canvas at x 0 and 640, y = Sky Offset, z -10100
// (just behind the background, whose sky area is transparent); the scroll accumulates dt * Sky Speed and moves both tiles by whole
// pixels (Sky Direction non-zero: right, else left), each tile wrapping inside +-640 (0x4022c0).
void GameScene::buildSky()
{
    const Camera& cm = data_.camera.data.camera;
    if (cm.o0238 == 0) return;
    OMediaCanvas* c = game_->skyCanvas((short)cm.o0238);
    if (!c) { game_->log("scene %s: sky %d not found\n", name_.c_str(), (int)cm.o0238); return; }
    for (int i = 0; i < 2; ++i) {
        sky_[i] = new OMediaCanvasElement(c);
        sky_[i]->link(world());
        skyX_[i] = i ? 640.0f : 0.0f;
        sky_[i]->place(i ? 640.0f : 0.0f, (float)(short)cm.o023c, -10100.0f);
    }
}

void GameScene::updateSky(float dt)
{
    if (!sky_[0] || !sky_[1]) return;
    const Camera& cm = data_.camera.data.camera;
    skyScroll_ += dt * cm.o0240;
    if (skyScroll_ < 1.0f) return;
    int px = (int)skyScroll_;
    if (cm.o023a) px = -px;
    for (int i = 0; i < 2; ++i) {
        float x = skyX_[i], y = (float)(short)cm.o023c, z = -10100.0f;
        x -= (float)px;                                  // 0x4022c0: x - delta, wrapped into (-640, 640]
        if (x <= -640.0f) x += 1280.0f;
        if (x > 640.0f) x -= 1280.0f;
        skyX_[i] = x;
        sky_[i]->place(x, y, z);
    }
    skyScroll_ -= (float)(short)(px < 0 ? -px : px);
}

void GameScene::place(SceneObject* o)
{
    if (!o->elem) return;
    float z;
    if (o->kind == SceneObject::Anim2D) z = o->postRender ? 10000.0f : -o->pz;      // 0x4096b0
    else z = o->postRender ? 10000.0f : o->pz;                                        // 0x402400 (flag +0x23a = 0)
    o->elem->place(o->px + shakeX_, o->py + shakeY_, z);   // shake offset: 0x43d130 / 0x43b08b
}

void GameScene::setHidden(SceneObject* o, bool hide)
{
    if (!o->elem) return;
    if (hide) { if (o->elem->get_hide_count() == 0) o->elem->hide(true); }
    else while (o->elem->get_hide_count() > 0) o->elem->hide(false);
}

// Chapter 2: 0x43ad10 / 0x438fa0 / 0x439110 use case-insensitive 0x44b2d0.
// Required by sdrapes (252drapes vs the authored 252Drapes).
SceneObject* GameScene::find(const char* name, int kindMask)
{
    for (size_t i = 0; i < objects_.size(); ++i) {
        SceneObject* o = objects_[i];
        if (kindMask && !(kindMask & (1 << o->kind))) continue;
        if (strcasecmp(o->art.c_str(), name) == 0) return o;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------- enter
void GameScene::enter(int startPoint)
{
    game_->state().sceneId = sceneId_;
    if (game_->world3d()) game_->world3d()->enter(startPoint);       // 0x437cf0 "here 1".."3": the player to the start point
    refreshVisibility();                                // vt+0x100 at "here 4"
    game_->log("scene %s entered (start point %d)\n", name_.c_str(), startPoint);
    // "here 10a": the scene's own enter event, or the event left by the last movie / script (game+0x186)
    if (game_->chained.empty()) {
        if (!enterEvent_.empty()) {
            game_->log("enter scene event %s\n", enterEvent_.c_str());
            if (game_->cond(enterCond_.c_str())) {
                game_->log("condition met\n");
                if (toupper((unsigned char)enterEvent_[0]) == 'H') pendingEvent_ = enterEvent_;   // scene+0x1cb4, run by the next update
                else game_->switchTo(enterEvent_.c_str());
            }
        }
    } else {
        game_->log("next event %s\n", game_->chained.c_str());
        if (toupper((unsigned char)game_->chained[0]) == 'H') pendingEvent_ = game_->chained;
        else game_->switchTo(game_->chained.c_str());
        game_->chained.clear();
    }
    // "here 12": scenes without a level (and the loading kind) hide the "loading" object
    if ((sceneId_ & 0xf00) == 0) hide("loading");
    lock_ = 500.0f;     // 0x437cf0 "here 16": param_1[0x735] = 500.0
}

// -------------------------------------------------------------------------------- show / hide
void GameScene::show(const char* name)          // 0x438fa0
{
    for (size_t i = 0; i < objects_.size(); ++i) {
        SceneObject* o = objects_[i];
        if (o->kind == SceneObject::Anim2D || o->kind == SceneObject::Obj2D) {
            if (strcasecmp(o->art.c_str(), name) == 0) {
                if (o->elem && o->elem->get_hide_count() != 0) {
                    setHidden(o, false);
                    if (o->kind == SceneObject::Anim2D) startAnim(o);    // vt+0xb0
                }
            }
        } else if (o->is3D() && strcasecmp(o->art.c_str(), name) == 0) {
            if (game_->world3d()) game_->world3d()->setHidden(o, false);
        }
    }
}

void GameScene::hide(const char* name)          // 0x439110
{
    for (size_t i = 0; i < objects_.size(); ++i) {
        SceneObject* o = objects_[i];
        if ((o->kind == SceneObject::Anim2D || o->kind == SceneObject::Obj2D) && strcasecmp(o->art.c_str(), name) == 0) {
            if (o->elem && o->elem->get_hide_count() == 0) setHidden(o, true);
            if (o->kind == SceneObject::Anim2D) o->playing = false;      // vt+0xb4 also stops its sound
        } else if (o->is3D() && strcasecmp(o->art.c_str(), name) == 0) {
            if (game_->world3d()) game_->world3d()->setHidden(o, true);
        }
    }
}

void GameScene::refreshVisibility()             // 0x438ad0
{
    if (game_->world3d()) game_->world3d()->refreshVisibility();      // the CHAR / ITEM cases
    for (size_t i = 0; i < objects_.size(); ++i) {
        SceneObject* o = objects_[i];
        if (o->kind == SceneObject::Trigger && !o->visibleCond.empty()) o->hidden3D = !game_->cond(o->visibleCond.c_str());
        if (!o->elem || o->visibleCond.empty()) continue;
        bool vis = game_->cond(o->visibleCond.c_str());
        bool hidden = o->elem->get_hide_count() != 0;
        if (!vis && !hidden) setHidden(o, true);
        else if (vis && hidden) {
            // an object the script hid on purpose stays hidden only if its condition is false: shown again here
            setHidden(o, false);
        }
    }
}

// ---------------------------------------------------------------------------------------- ANIM
void GameScene::startAnim(SceneObject* o)       // 0x407f40 (vt+0x9c)
{
    if (o->frames.empty()) return;
    o->playing = true;
    o->frame = 0; o->dir = 1; o->loopsDone = 0; o->frameMs = 0; o->intervalSec = 0;
    if (o->elem) o->elem->setcanvas(o->frames[0]);
}

bool GameScene::animate(const char* name)       // 'anim2d': start it, wait until it has run (true = wait)
{
    SceneObject* o = find(name, (1 << SceneObject::Anim2D));
    if (!o) {
        // the exe calls through a null pointer here (crash); port: nothing to wait for
        game_->log("anim2d: no 2D animation '%s'\n", name);
        return false;
    }
    setHidden(o, false);
    startAnim(o);
    return !o->frames.empty();
}

void GameScene::tickAnim(SceneObject* o, float dtMs)   // 0x408cb0, simplified: one frame per 1000/fps ms
{
    if (!o->elem || o->frames.empty() || o->paused || o->elem->get_hide_count() != 0) return;
    if (!o->playing) {
        // idle: a play interval (seconds) restarts it (0x408cb0 first branch)
        if (o->interval > 0 && !o->clickToPlay) {
            o->intervalSec += dtMs * 0.001f;
            if (o->intervalSec >= (float)o->interval) startAnim(o);
        }
        return;
    }
    if (o->framePerClick) return;                       // advanced by clicks
    float step = 1000.0f / (o->fps > 0 ? (float)o->fps : 10.0f);
    o->frameMs += dtMs;
    int n = (int)o->frames.size();
    while (o->frameMs >= step && o->playing) {
        o->frameMs -= step;
        int next = o->frame + o->dir;
        if (next >= n || next < 0) {
            // end of a pass
            if (o->reverse && o->dir == 1 && n > 1) { o->dir = -1; next = n - 2; }
            else {
                o->dir = 1;
                o->loopsDone++;
                bool more = o->continuous || o->loopsDone < (o->loops > 0 ? o->loops : 1);
                if (more) next = 0;
                else {
                    o->playing = false;
                    if (o->stayOnLast) next = n - 1; else next = 0;
                    o->frame = next;
                    o->elem->setcanvas(o->frames[next]);
                    if (o->eventPending) { o->eventPending = false; baseEvent(o, false, false); }
                    game_->host()->anim2dFinished();
                    return;
                }
            }
        }
        o->frame = next;
        o->elem->setcanvas(o->frames[next]);
    }
}

// ---------------------------------------------------------------------------------------- picking
static bool upDown(const SceneObject* o)            // 0x43a750: a TRIG whose Event says "up" or "down" (a height connector)
{
    return o->kind == SceneObject::Trigger && (strstr(o->event.c_str(), "up") || strstr(o->event.c_str(), "down"));
}

static bool inRect(const SceneObject* o, int x, int y)  // 0x415e00: an empty rectangle is never hit
{
    if (o->x2 - o->x1 == 0 || o->y2 == o->y1) return false;
    return o->x1 <= x && o->y1 <= y && x <= o->x2 && y <= o->y2;
}

SceneObject* GameScene::pick(int x, int y)      // 0x43a310, the TRIG pass and the 2D pass (the CHAR / ITEM pass is World3D::pick)
{
    World3D* w = game_->world3d();
    // pass 1: triggers whose "use Condition" flag is off or whose Condition holds, visible, not a connector of another height
    for (size_t i = 0; i < objects_.size(); ++i) {
        SceneObject* o = objects_[i];
        if (o->kind != SceneObject::Trigger) continue;
        if (o->useCond && !game_->cond(o->condition.c_str())) continue;
        if (o->hidden3D) continue;
        if (w && w->player() && !w->samePlane(o) && upDown(o)) continue;
        if (inRect(o, x, y)) return o;
    }
    return pick2D(x, y);
}

SceneObject* GameScene::pick2D(int x, int y)    // 0x43a310, third pass
{
    for (size_t i = 0; i < objects_.size(); ++i) {
        SceneObject* o = objects_[i];
        if (o->is3D()) continue;
        bool hidden = o->elem && o->elem->get_hide_count() != 0;
        if (o->kind == SceneObject::Obj2D) {
            if (hidden && !game_->state().debugAll() && o != lastClicked_) continue;
        } else if (o->kind == SceneObject::Anim2D) {
            if (hidden && o->allowToggle == 0 && !game_->state().debugAll()) continue;
            if (o->stayOnLast && o->playing) continue;    // 0x409520
        } else if (o->kind == SceneObject::Spot) {
            if (o->rolloverCond && !game_->cond(o->condition.c_str())) continue;
        }
        if (inRect(o, x, y)) return o;
    }
    return 0;
}

SceneObject* GameScene::hover(int x, int y)
{
    // 0x43a310 order: triggers, then the 3D characters / items, then the 2D objects
    if (game_->miniGame()) { SceneObject* m = game_->miniGame()->pickMazeDoor(x, y); if (m) return m; }
    for (size_t i = 0; i < objects_.size(); ++i) {
        SceneObject* o = objects_[i];
        if (o->kind != SceneObject::Trigger || o->hidden3D) continue;
        if (o->useCond && !game_->cond(o->condition.c_str())) continue;
        if (inRect(o, x, y)) return o;
    }
    SceneObject* o = game_->world3d() ? game_->world3d()->pick(x, y) : 0;
    return o ? o : pick2D(x, y);
}

SceneObject* GameScene::pickAll(int x, int y)
{
    if (game_->miniGame()) { SceneObject* m = game_->miniGame()->pickMazeDoor(x, y); if (m) return m; }
    SceneObject* t = 0;
    for (size_t i = 0; i < objects_.size() && !t; ++i) {
        SceneObject* o = objects_[i];
        if (o->kind != SceneObject::Trigger || o->hidden3D) continue;
        if (o->useCond && !game_->cond(o->condition.c_str())) continue;
        World3D* w = game_->world3d();
        if (w && w->player() && !w->samePlane(o) && upDown(o)) continue;
        if (inRect(o, x, y)) t = o;
    }
    if (t) return t;
    SceneObject* o = game_->world3d() ? game_->world3d()->pick(x, y) : 0;
    return o ? o : pick2D(x, y);
}

bool GameScene::baseEvent(SceneObject* o, bool touching, bool fromClick)    // 0x415a70
{
    std::string ev;
    if (o->used && nonEmptyEvent(o->haveItemEvent)) ev = o->haveItemEvent;
    else if (game_->cond(o->condition.c_str(), touching)) {
        ev = o->event;
        if (fromClick) game_->host()->playObjectSound(o);       // vt+0x50(1)
    } else {
        ev = o->altEvent;
        if (fromClick && ev.size() > 5 && toupper((unsigned char)ev[0]) == 'C' && toupper((unsigned char)ev[2]) == 'L')
            game_->host()->playObjectSound(o);
    }
    if (ev.empty() || !nonEmptyEvent(ev)) return false;
    game_->log("event '%s' (%s)\n", ev.c_str(), o->description.c_str());
    game_->switchTo(ev.c_str());
    return true;
}

bool GameScene::pickEvent(SceneObject* o, bool touching, bool fromClick)
{
    World3D* w = game_->world3d();
    Actor3D* p = w ? w->player() : 0;
    switch (o->kind) {
    case SceneObject::Trigger: triggerEvent(o, touching); return true;
    case SceneObject::Char: charEvent(o, touching); return true;
    case SceneObject::Item: itemEvent(o, touching); return true;
    case SceneObject::Spot:                                 // 0x41a700
    case SceneObject::Obj2D:                                // 0x402240
        if (o->gridX != 0 && o->gridY != 0 && !game_->state().debugAll() && p) {
            if (w->walkTo(o, o->px, o->kind == SceneObject::Spot ? o->py : o->pz)) return true;
        }
        return baseEvent(o, touching, true);
    default:
        return baseEvent(o, touching, fromClick);
    }
}

// 0x441cb0: a trigger.  Not touching yet: walk to it.  Then turn to its heading (Require Enter), a door plays "door" and the
// door's 2D animation, then the base event with `touching` set.
void GameScene::triggerEvent(SceneObject* o, bool touching)
{
    World3D* w = game_->world3d();
    Actor3D* p = w ? w->player() : 0;
    if (!game_->state().debugAll() && p) {
        if (o->event.empty() && o->altEvent.empty()) return;
        // port: the cross-height connector walk (vt+0x17c, 0x434520) is not reconstructed; a trigger on another height is used in place
        if (!touching && w->walkTo(o, o->px, o->pz)) return;
        if (o->rotation != 0 && o->requireEnter != 0 && w->rotatePlayer(o->rotation)) {
            game_->setPending(o, true, "");           // game vt+0xac (o, 1, "")
            return;
        }
        if (o->isDoor) {
            bool c = game_->cond(o->condition.c_str(), touching);
            if (c || (o->altEvent.size() > 5 && toupper((unsigned char)o->altEvent[0]) == 'C' && toupper((unsigned char)o->altEvent[2]) == 'L')) {
                doorTrig_ = o;                         // scene+0x1c80
                w->playerDoor();                       // player vt+0xb4("door", 0, 1.0)
                playDoor();                            // scene vt+0x110
                baseEvent(o, touching, true);
                return;
            }
        }
        doorTrig_ = 0;
    }
    baseEvent(o, touching, true);
}

static bool isDialogueKey(const std::string& n)     // game vt+0xb4 (0x40a5f0)
{
    if (n == "unlock" || n == "skip" || n.empty()) return false;
    int c0 = toupper((unsigned char)n[0]);
    if (c0 == 'H' || c0 == 'M' || c0 == 'S') return false;
    if (n.size() > 4 && c0 == 'C' && toupper((unsigned char)n[2]) == 'L' && toupper((unsigned char)n[4]) == 'S') return false;
    return true;
}

// 0x433a60: a character.  A comment or a key with nothing to say goes straight to the base event; else walk to it, the
// player turns toward it (or to its fixed heading +0x2a4), the character turns to the player (+0x2a0), then the base event.
void GameScene::charEvent(SceneObject* o, bool touching)
{
    if (!game_->pendingItem.empty()) return;                    // game+0x22c: an item use is pending
    if (game_->runner()) return;
    if (!(o->used && nonEmptyEvent(o->haveItemEvent))) {
        const std::string& ev = game_->cond(o->condition.c_str()) ? o->event : o->altEvent;
        if (!ev.empty() && toupper((unsigned char)ev[0]) == 'H') { baseEvent(o, touching, true); return; }
        if (isDialogueKey(ev) && !(dialogue_ && dialogue_->hasSomething(ev.c_str()))) { baseEvent(o, touching, true); return; }
    }
    World3D* w = game_->world3d();
    Actor3D* p = w ? w->player() : 0;
    if (!game_->state().debugAll() && p) {
        if (!touching && w->walkTo(o, o->px, o->pz)) return;
        bool turning = o->faceHeading == 0 ? w->rotateToward(o) : w->rotatePlayer(o->faceHeading);
        if (turning) { game_->setPending(o, true, ""); return; }
    }
    if (o->facePlayer != 0 && p && w) w->turnToward(o, p);        // the character looks at the player
    baseEvent(o, touching, true);
}

// 0x4234d0: an item.  Without Can Pickup it is a plain object.  Its Condition true: walk to it, turn, pick it up (the player's
// vt+0x100, the rest follows the "pickup" animation); false: the Alt Event.
void GameScene::itemEvent(SceneObject* o, bool touching)
{
    if (game_->hudHolding()) return;                            // +0x248 / HUD +0x130: something in hand
    if (o->pickup == 0) { baseEvent(o, touching, false); return; }
    if (game_->cond(o->condition.c_str(), touching)) {
        World3D* w = game_->world3d();
        Actor3D* p = w ? w->player() : 0;
        if (!p) return;                                         // port: item vt+0x104 without a player does not occur in the data
        if (!game_->state().debugAll()) {
            if (!touching && w->walkTo(o, o->px, o->pz)) return;
            if (w->rotateToward(o)) { game_->setPending(o, true, ""); return; }
        }
        game_->inputConsumed = true;
        w->startPickup(o);
        return;
    }
    if (nonEmptyEvent(o->altEvent)) game_->switchTo(o->altEvent.c_str());
}

bool GameScene::useItem(SceneObject* o, const char* item)
{
    if (o->kind == SceneObject::Char) return charUseItem(o, item);
    if (o->kind == SceneObject::Trigger) return trigUseItem(o, item);
    return baseUseItem(o, item);
}

bool GameScene::baseUseItem(SceneObject* o, const char* item)    // 0x415c30
{
    if (game_->pendingScene()) return false;                     // game+0x158
    if (strcasecmp(o->itemAccept.c_str(), item) == 0) {
        if (o->kind == SceneObject::Trigger && o->used && !o->haveItemEvent.empty()) {
            game_->switchTo(o->haveItemEvent.c_str());
            return false;
        }
        game_->switchTo(o->itemEvent.c_str());
        o->used = true;
        if (o->kind == SceneObject::Char || o->kind == SceneObject::Trigger) game_->state().addChar(o->art.c_str());   // 0x416c70
        return true;
    }
    if (!o->wrongItemEvent.empty() && !o->used) {
        game_->switchTo(o->wrongItemEvent.c_str());
        return false;
    }
    if (o->kind == SceneObject::Char && dialogue_) {             // the character's own refusal lines (dialogue vt+0x64)
        std::string k;
        if (dialogue_->refusal(o->art.c_str(), &k)) { game_->switchTo(k.c_str()); return false; }
    }
    static unsigned n = 0;                                       // rand() % 3 in the exe; port: in turn (deterministic tests)
    const char* c[3] = { "H000F", "H000G", "H000N" };
    game_->switchTo(c[n++ % 3]);
    return false;
}

// 0x433f00: an item given to a character.  The right item (or a refusal to say): walk to the character first and give it
// when there (game vt+0xac (char, 0, item)); the character turns to the player; then the base routine.
bool GameScene::charUseItem(SceneObject* o, const char* item)
{
    if (game_->pendingScene()) return false;
    World3D* w = game_->world3d();
    Actor3D* p = w ? w->player() : 0;
    bool match = strcasecmp(o->itemAccept.c_str(), item) == 0;
    if (!match || (o->used && (!p || p->obj != o))) {
        if ((o->wrongItemEvent.empty() || o->used) && !(dialogue_ && dialogue_->refusal(o->art.c_str(), 0)))
            return baseUseItem(o, item);
    }
    if (!w || !p) return baseUseItem(o, item);
    if (!game_->pendingObj && p->obj != o && !w->touching(o) && w->walkTo(o, o->px, o->pz)) {
        game_->setPending(o, false, item);
    } else {
        if (o->facePlayer != 0) w->turnToward(o, p);
        if (p->obj == o || !w->rotateToward(o)) return baseUseItem(o, item);
        game_->setPending(o, false, item);
    }
    return match && !o->used;
}

bool GameScene::trigUseItem(SceneObject* o, const char* item)    // 0x441ed0
{
    if (game_->pendingScene()) return false;
    // port: an accepted item on a trigger of another height would walk the connector first (vt+0x17c, not reconstructed)
    return baseUseItem(o, item);
}

void GameScene::click(int x, int y)    // 0x4398f0
{
    sbscript::ScriptRunner* r = game_->runner();
    game_->inputConsumed = false;
    if ((r && !r->keepControl()) || lock_ > 0) return;
    World3D* w = game_->world3d();
    Actor3D* p = w ? w->player() : 0;
    SceneObject* o = pickAll(x, y);
    if (o) {
        if (o->is3D() && p && o == p->obj) return;
        if (upDown(o) && p) w->clearTarget();                    // scene vt+0x148, player vt+0x180
        game_->log("click %d,%d: %s '%s'\n", x, y, o->kind == SceneObject::Obj2D ? "2DOB" : o->kind == SceneObject::Anim2D ? "ANIM" : o->kind == SceneObject::Char ? "CHAR" : o->kind == SceneObject::Item ? "ITEM" : o->kind == SceneObject::Trigger ? "TRIG" : "SPOT", o->description.c_str());
        bool touching = p ? w->touching(o) : false;              // player vt+0x128(o, 1)
        if (o->kind == SceneObject::Anim2D) clickAnim(o, touching);
        else pickEvent(o, touching, true);
        if (o->kind == SceneObject::Spot || o->kind == SceneObject::Anim2D || o->kind == SceneObject::Obj2D) lastClicked_ = o;
    }
    if (!game_->inputConsumed && sceneId_ == 0x70401) {   // 0x4398f0 tail: the car wash squeezes the sponge (mini game 0x4367a0)
        game_->stopChannel(7);
        OMediaSound* snd = game_->soundCI(game_->sceneSoundDb(), "squeeze");
        if (snd) game_->playOnChannel(7, snd, 0);
    }
    if (!p || game_->inputConsumed || game_->pendingObj) return;
    // nothing took the click: the player walks to the floor point (with the object as the target when it is a SPOT without a
    // grid cell); 0x4184f0 / 0x418170 (the cell under the click is open), else the click on the other height's grid (port: not
    // reconstructed, vt+0x178)
    // (any other object that was hit does not walk the player here: its own vt+0x40 did, or it does not walk)
    if (!o) w->clickFloor(x, y, 0);
    else if (o->kind == SceneObject::Spot && o->gridX == 0 && o->gridY == 0) w->clickFloor(x, y, o);
    else if (o->kind == SceneObject::Trigger && o->event.empty() && o->altEvent.empty()) w->clickFloor(x, y, 0);
}

// scene vt+0x160 (0x43d270): the car wash (scene 0x70401).  The pointer over nothing rubs the "cardirty" overlay clean: every pixel
// of the box col x-11..x+9, row y-21..y+9 takes the overlay's first pixel (its transparent colour); the "wash" sound plays while
// something changes; at more than 99% of 640x480 transparent the event "swash" runs.
void GameScene::carWash(int x, int y)
{
    SceneObject* o = find("cardirty", (1 << SceneObject::Obj2D));
    OMediaCanvas* c = o && o->elem ? canvasNamed(o->art) : 0;
    OMediaGfxWorld* gw = c ? c->getgfxworld() : 0;
    OMediaGfxPixels* px = gw ? gw->port_pixels() : 0;
    if (!px || !px->pix) return;
    gw->pixlock();
    int w = px->width, h = px->height;
    unsigned key = px->pix[0];
    bool changed = false;
    double clean = 0;
    for (int row = 0; row < h; ++row) {
        int er = h - 1 - row;
        for (int col = 0; col < w; ++col) {
            unsigned& p = px->pix[row * w + col];
            int ec = w - 1 - col;
            if (er >= (480 - y) - 10 && er <= (480 - y) + 20 && ec >= (640 - x) - 10 && ec <= (640 - x) + 10 && p != key) { p = key; changed = true; }
            if (p == key) clean += 1.0;
        }
    }
    if (changed) px->version++;
    gw->pixunlock();
    if (clean * (1.0f / 307200.0f) * 100.0f > 99.0f) {
        game_->switchTo("swash");
        game_->stopChannel(7);
        return;
    }
    if (changed && !game_->channelBusy(7)) {              // mini game object 0x4367a0("wash"): channel 7, the scene bank
        OMediaSound* snd = game_->soundCI(game_->sceneSoundDb(), "wash");
        if (snd) game_->playOnChannel(7, snd, 0);
    }
}

void GameScene::clickAnim(SceneObject* o, bool touching)      // 0x4093a0 (simplified: click-to-play / frame-per-click / toggle)
{
    if (o->allowToggle && !o->clickToPlay) { o->paused = !o->paused; }
    if (o->framePerClick && !o->frames.empty()) {
        o->frame = (o->frame + 1) % (int)o->frames.size();
        o->elem->setcanvas(o->frames[o->frame]);
    }
    if (o->clickToPlay) {
        startAnim(o);
        game_->inputConsumed = true;
        if (o->eventAtBegin) baseEvent(o, touching, false); else o->eventPending = true;
        return;
    }
    baseEvent(o, touching, false);
}

// ------------------------------------------------------------------------------- update / bars
void GameScene::update(float dt)
{
    if (!pendingEvent_.empty()) {                      // 0x43ae70, pending event string at scene+0x1cb4
        std::string e = pendingEvent_;
        pendingEvent_.clear();
        game_->switchTo(e.c_str());
    }
    if (lock_ > 0) { lock_ -= dt; if (lock_ < 0) lock_ = 0; }
    if (shakeCount_ != 0) {                            // 0x43afce: ten steps of 50 ms; the script waits for the last one
        shake_ -= dt;
        if (shake_ <= 0) {
            --shakeCount_;
            shake_ = 50.0f;
            if (shakeCount_ == 0) {                    // 0x43b00f: everything goes back, the script goes on (vt+0xe0)
                shakeX_ = shakeY_ = 0;
                shakeObjects();
                if (background_) background_->place(0, 0, -10000.0f);
                shake_ = 0;
                game_->wake();
            } else shakeStep(true);
        }
    }
    updateSky(dt);
    for (size_t i = 0; i < objects_.size(); ++i)
        if (objects_[i]->kind == SceneObject::Anim2D) tickAnim(objects_[i], dt);

    // 0x43b760: bars move 0.25 px per ms
    float mv = dt * 0.25f;
    switch (barState_) {
    case 1:     // letterbox open: top bar to -176, bottom bar to 416
        topY_ = std::max(topY_ - mv, -176.0f); botY_ = std::min(botY_ + mv, 416.0f);
        moveBars(topY_, botY_, 9999.0f);
        if (botY_ >= 416.0f) { barState_ = 0; game_->wake(); }
        break;
    case 2:     // letterbox close
        topY_ = std::min(topY_ + mv, 0.0f); botY_ = std::max(botY_ - mv, 240.0f);
        moveBars(topY_, botY_, 9999.0f);
        if (botY_ <= 240.0f) { barState_ = 5; barHold_ = 0; }
        break;
    case 3:     // transition close
        topY_ = std::min(topY_ + mv, 0.0f); botY_ = std::max(botY_ - mv, 240.0f);
        moveBars(topY_, botY_, 10300.0f);
        if (botY_ <= 240.0f) { barState_ = 5; barHold_ = 0; }
        break;
    case 4:     // transition open
        topY_ = std::max(topY_ - mv, -240.0f); botY_ = std::min(botY_ + mv, 480.0f);
        moveBars(topY_, botY_, 10300.0f);
        if (botY_ >= 480.0f) { barState_ = 0; game_->wake(); }
        break;
    case 5:     // hold 250 ms, then wake the script
        barHold_ += dt;
        if (barHold_ > 250.0f) { barState_ = 0; game_->wake(); }
        break;
    default: break;
    }
}

void GameScene::ensureBars()
{
    if (!barTop_) {
        barTop_ = new OMediaBoxElement;
        barTop_->set_size(640, 240); barTop_->set_filled(true); barTop_->set_color(OMediaRGBColor(0, 0, 0));
        barTop_->link(world()); barTop_->place(0, 0, 9999.0f);
    }
    if (!barBot_) {
        barBot_ = new OMediaBoxElement;
        barBot_->set_size(640, 240); barBot_->set_filled(true); barBot_->set_color(OMediaRGBColor(0, 0, 0));
        barBot_->link(world()); barBot_->place(0, 240, 9999.0f);
    }
}

void GameScene::moveBars(float topY, float botY, float z)
{
    ensureBars();
    barTop_->place(0, topY, z);
    barBot_->place(0, botY, z);
}

void GameScene::letterboxInit()     // 0x43bd90: both bars cover the screen
{
    ensureBars();
    topY_ = 0; botY_ = 240;
    moveBars(0, 240, 9999.0f);
}

void GameScene::transitionOut()     // 0x43bf50: bars off screen (top -240, bottom 480); state 3 follows
{
    ensureBars();
    topY_ = -240; botY_ = 480;
    moveBars(topY_, botY_, 10300.0f);
}

void GameScene::transitionOpen()    // 0x43c100: bars cover everything, state 4 opens them
{
    ensureBars();
    topY_ = 0; botY_ = 240;
    moveBars(topY_, botY_, 10300.0f);
    barState_ = 4;
}

void GameScene::transitionClose()   // 'transition close' (0x43f3b3): bars come in, then state 3
{
    transitionOut();
    barState_ = 3;
}

// 0x43d130 (scene vt+0x15c): ten steps, 50 ms apart; the 2D objects move to their place plus a random -5..4 px in x and y
void GameScene::shake()
{
    shakeCount_ = 10; shake_ = 50.0f;
    shakeStep(false);
}

void GameScene::shakeObjects()
{
    for (size_t i = 0; i < objects_.size(); ++i)
        if (objects_[i]->kind == SceneObject::Obj2D || objects_[i]->kind == SceneObject::Anim2D) place(objects_[i]);
}

// 0x43b08b: the steps after the first also move the background (0x1c88, z -10000) by its own random offset
void GameScene::shakeStep(bool background)
{
    if (background && background_) {
        float bx = (float)(rand() % 10 - 5), by = (float)(rand() % 10 - 5);
        background_->place(bx, by, -10000.0f);
    }
    shakeX_ = (float)(rand() % 10 - 5); shakeY_ = (float)(rand() % 10 - 5);
    shakeObjects();
}

void GameScene::setDoor(const char* name)       // 'setdoor' (0x43fbed): scene+0x1c80 = the trigger of that name
{
    doorTrig_ = 0;
    for (size_t i = 0; i < objects_.size(); ++i)
        if (objects_[i]->kind == SceneObject::Trigger && strcasecmp(objects_[i]->doorAnim.c_str(), name) == 0) doorTrig_ = objects_[i];
}

void GameScene::playDoor()                      // 0x43adb0: the door trigger's Door Anim Name, an ANIM of the scene
{
    if (!doorTrig_ || doorTrig_->doorAnim.empty()) return;
    SceneObject* a = find(doorTrig_->doorAnim.c_str(), 1 << SceneObject::Anim2D);
    if (a) { setHidden(a, false); startAnim(a); }
    else game_->log("door: no 2D animation '%s'\n", doorTrig_->doorAnim.c_str());
}

}  // namespace sbgame
