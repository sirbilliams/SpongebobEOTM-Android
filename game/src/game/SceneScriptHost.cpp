#include "SceneScriptHost.h"
#include "Kick.h" // lane d
#include "MiniGame.h" // chapter 2 mini-games
#include "DialogueUI.h"
#include "World3D.h"
#include "Hud.h"                   // port: hud
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace sbgame {

using sbscript::ScriptActor;
using sbscript::ScriptSound;

SceneScriptHost::SceneScriptHost(Game* g)
    : sim3dMs(700.0f), g_(g), wait3dMs_(0), wait3d_(false), waitAnim_(false), stubCalls_(0), active_(0), uiMs_(0), own_(0)
{
}

SceneScriptHost::~SceneScriptHost()
{
    for (std::map<SceneObject*, ScriptActor*>::iterator i = actors_.begin(); i != actors_.end(); ++i) delete i->second;
    for (size_t i = 0; i < sounds_.size(); ++i) delete sounds_[i];
}

void SceneScriptHost::stub(const char* fmt, ...)
{
    ++stubCalls_;
    if (!g_->logFile) return;
    va_list ap;
    va_start(ap, fmt);
    fputs("      3D: ", g_->logFile);
    vfprintf(g_->logFile, fmt, ap);
    fputc('\n', g_->logFile);
    va_end(ap);
}

static const char* an(ScriptActor* a) { return a ? a->name.c_str() : "(null)"; }

ScriptActor* SceneScriptHost::actorFor(SceneObject* o)
{
    if (!o) return 0;
    std::map<SceneObject*, ScriptActor*>::iterator i = actors_.find(o);
    if (i != actors_.end()) return i->second;
    ScriptActor* a = new ScriptActor;
    a->obj = o;
    a->name = o->art;
    actors_[o] = a;
    return a;
}

void SceneScriptHost::resetForScene()
{
    for (std::map<SceneObject*, ScriptActor*>::iterator i = actors_.begin(); i != actors_.end(); ++i) delete i->second;
    actors_.clear();
    active_ = 0;
    wait3dMs_ = 0;
    wait3d_ = false;
    waitAnim_ = false;
    uiMs_ = 0;
}

void SceneScriptHost::sceneLoaded()
{
    // 0x437cf0 "here 2": the first character the player controls becomes the active one (scene+0x248)
    GameScene* s = g_->scene();
    for (size_t i = 0; s && i < s->objects().size(); ++i) {
        SceneObject* o = s->objects()[i];
        if (o->kind == SceneObject::Char && o->userCtl) { active_ = actorFor(o); break; }
    }
    if (!active_ && s) {
        SceneObject* sb = s->find("sb", 1 << SceneObject::Char);
        if (sb) active_ = actorFor(sb);
    }
}

void SceneScriptHost::commentTalk() { if (active_ && g_->world3d()) g_->world3d()->oneShotAnim(active_->obj, "talk"); }    // vt+0xc0("talk")
void SceneScriptHost::dialogueStarted() {}      // char+0x32a = 0: the player has no walk input in this port yet
void SceneScriptHost::controlLostToScript(const char* name) { (void)name; }
void SceneScriptHost::speakerStarted(SceneObject* a, const char* name)
{
    (void)name;
    if (!g_->world3d()) return;
    g_->world3d()->talkAll(false);               // the previous speaker stops
    if (a) g_->world3d()->talk(a, true);
}

void SceneScriptHost::actorDone()
{
    if (wait3d_) { wait3d_ = false; wakeNow(); }
}

void SceneScriptHost::speakersIdle() { if (g_->world3d()) g_->world3d()->talkAll(false); }

void SceneScriptHost::update(float dt)
{
    if (wait3dMs_ > 0) {
        wait3dMs_ -= dt;
        if (wait3dMs_ <= 0) { wait3dMs_ = 0; wakeNow(); }
    }
    if (uiMs_ > 0) {
        uiMs_ -= dt;
        if (uiMs_ <= 0) { uiMs_ = 0; if (runnerNow() && runnerNow()->waitingUi()) wakeNow(); }
    }
}

void SceneScriptHost::anim2dFinished()
{
    if (waitAnim_) { waitAnim_ = false; g_->wake(); }
}

void SceneScriptHost::talkFinished()
{
    speakersIdle();
    sbscript::ScriptRunner* r = g_->runner();
    if (r && r->waitingTalk()) g_->wake();       // 0x411ba0
}

// ---------------------------------------------------------------------------- environment
int SceneScriptHost::chapter() { return g_->state().chapter; }

bool SceneScriptHost::readFile(const char* path, std::vector<unsigned char>& out)
{
    std::string p = g_->root() + "/" + path;
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    out.clear();
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}

unsigned SceneScriptHost::sceneId() { return g_->scene() ? g_->scene()->sceneId() : g_->state().sceneId; }
bool SceneScriptHost::condition(const char* expr) { return g_->cond(expr); }
void SceneScriptHost::scriptStarted() {}
void SceneScriptHost::mousePosition(short* x, short* y) { *x = (short)g_->mouseX(); *y = (short)g_->mouseY(); }
void SceneScriptHost::scriptEnded() { if (g_->scene()) g_->scene()->refreshVisibility(); }    // scene vt+0x100
void SceneScriptHost::markScriptSeen(const char* n) { g_->state().markScriptSeen(n); }
bool SceneScriptHost::scriptSeen(const char* n) { return g_->state().scriptSeen(n); }

// -------------------------------------------------------------------------------------- actors (3D)
ScriptActor* SceneScriptHost::activeActor() { return active_; }
void SceneScriptHost::setActiveActor(ScriptActor* a) { active_ = a; }
ScriptActor* SceneScriptHost::findActor(const char* name)
{
    return g_->scene() ? actorFor(g_->scene()->find(name, 1 << SceneObject::Char)) : 0;
}
void SceneScriptHost::actorBeginScript(ScriptActor* a) { if (a && g_->world3d()) g_->world3d()->stopWalk(a->obj); }
void SceneScriptHost::actorSetControl(ScriptActor* a, bool on) { Actor3D* actor = a && g_->world3d() ? g_->world3d()->actorFor(a->obj) : 0; if (actor) actor->control = on; }
bool SceneScriptHost::actorWalk(ScriptActor* a, float x, float y, int flag, int gx, int gy)
{
    (void)flag;
    if (!a || !a->obj) return false;

    World3D* w = g_->world3d();
    if (w && w->hasModel(a->obj)) {
        bool wait = gx >= 0 && gy >= 0 ? w->walkGrid(a->obj, gx, gy) : w->walk(a->obj, x, y, false);
        if (wait) wait3d_ = true;
        return wait;
    }
    a->obj->px = x; a->obj->pz = y;
    return false;
}
void SceneScriptHost::actorPrepareWalkGrid(ScriptActor* a) { (void)a; }
void SceneScriptHost::actorSetRunning(ScriptActor* a) { if (a && g_->world3d()) g_->world3d()->setRunning(a->obj); }
void SceneScriptHost::actorPosition(ScriptActor* a, float* x, float* y, float* z)
{
    if (a && a->obj) { *x = a->obj->px; *y = a->obj->py; *z = a->obj->pz; }
    else *x = *y = *z = 0;
}
void SceneScriptHost::actorReposition(ScriptActor* a, float x, float z) { actorMove(a, x, z); }
void SceneScriptHost::actorMove(ScriptActor* a, float x, float y)
{
    if (!a || !a->obj) return;
    if (g_->world3d() && g_->world3d()->hasModel(a->obj)) g_->world3d()->moveTo(a->obj, x, y);
    else { a->obj->px = x; a->obj->pz = y; }
}
void SceneScriptHost::actorMoveY(ScriptActor* a, float y) { Actor3D* actor = a && g_->world3d() ? g_->world3d()->actorFor(a->obj) : 0; if (actor) { actor->y = y; g_->world3d()->place(actor); } else if (a) a->obj->py = y; }
void SceneScriptHost::actorMoveGrid(ScriptActor* a, int gx, int gy) { if (a && g_->world3d()) g_->world3d()->moveGrid(a->obj,gx,gy); }
void SceneScriptHost::actorClimb(ScriptActor* a, float h) { if (a && a->obj && g_->world3d()) g_->world3d()->climb(a->obj, h); }    // vt+0x154
bool SceneScriptHost::actorRotate(ScriptActor* a, int angle, int time)
{
    if (!a || !g_->world3d() || !g_->world3d()->hasModel(a->obj)) return false;
    bool wait = g_->world3d()->rotate(a->obj, angle, time);
    if (wait) wait3d_ = true;
    return wait;
}
void SceneScriptHost::actorIdle(ScriptActor* a, float seconds)      // vt+0x168 (0x42f170): the actor plays idle1 and idles for `seconds`
{
    if (a && g_->world3d()) g_->world3d()->loopAnim(a->obj, "idle1");     // lane f
    arm3d(seconds * 1000.0f);
}
void SceneScriptHost::actorAttach(ScriptActor* a, const char* o, const char* b1, const char* b2) { stub("%s attach %s %s %s (vt+0x12c)", an(a), o, b1, b2); }
void SceneScriptHost::actorDetach(ScriptActor* a) { stub("%s detach (vt+0x130)", an(a)); }
void SceneScriptHost::actorHideAttach(ScriptActor* a) { stub("%s hide attached object", an(a)); }
void SceneScriptHost::actorShowAttach(ScriptActor* a) { stub("%s show attached object", an(a)); }
bool SceneScriptHost::actorPlayAnim(ScriptActor* a, const char* name, bool wait, double speed)
{
    World3D* w = g_->world3d();
    if (!a || !w) return false;
    bool r = w->playAnim(a->obj, name, wait, speed);
    if (r) wait3d_ = true;
    else if (!w->hasModel(a->obj) || !w->exists(w->actorFor(a->obj), name)) stub("%s play anim %s: no such animation / model", an(a), name);
    return r;
}
void SceneScriptHost::actorLoopAnim(ScriptActor* a, const char* name) { if (a && g_->world3d()) g_->world3d()->loopAnim(a->obj, name); }
void SceneScriptHost::actorTalkAnim(ScriptActor* a) { if (a && g_->world3d()) g_->world3d()->talk(a->obj, true); }
void SceneScriptHost::actorOneShotAnim(ScriptActor* a, const char* name) { if (a && g_->world3d()) g_->world3d()->oneShotAnim(a->obj, name); }
// lane d: character vtable slots 0x16c / 0x174 / 0x188 (not the shared mini-game object).
void SceneScriptHost::actorKick(ScriptActor* a) {
    World3D* w=g_->world3d();
    if (a && w && Kick::start(*w,w->actorFor(a->obj))) wait3d_=true;
    else arm3d(1); // port: allow a missing model to release the script.
}
bool SceneScriptHost::actorGoal(ScriptActor* a) {
    World3D* w=g_->world3d(); return a && w && Kick::goal(*w,w->actorFor(a->obj));
}
bool SceneScriptHost::actorGoalMissed(ScriptActor* a) {
    World3D* w=g_->world3d(); return a && w && Kick::missed(*w,w->actorFor(a->obj));
}
void SceneScriptHost::actorStopWalk(ScriptActor* a) { if (a && g_->world3d()) g_->world3d()->stopWalk(a->obj); }
void SceneScriptHost::actorShadow(ScriptActor* a, bool on) { if (a && g_->world3d()) g_->world3d()->setShadow(a->obj, on); }      // the shadow model is hidden / shown (World3D)
void SceneScriptHost::actorSetTexture(ScriptActor* a, const char* t) { stub("%s texture %s (vt+0xf0)", an(a), t); }
void SceneScriptHost::swapPlayerHelmet() { stub("swapchar: sb <-> sbhelm"); }
void SceneScriptHost::shockEffect() { stub("shock: 2D object \"elec\" at the player's screen position"); }

// ------------------------------------------------------------------------------------------ scene
void SceneScriptHost::sceneHide(const char* n) { if (g_->scene()) g_->scene()->hide(n); }
void SceneScriptHost::sceneShow(const char* n) { if (g_->scene()) g_->scene()->show(n); }
bool SceneScriptHost::sceneAnim2D(const char* n)
{
    if (!g_->scene()) return false;
    bool w = g_->scene()->animate(n);
    waitAnim_ = w;
    return w;
}
void SceneScriptHost::sceneSetDoor(const char* n) { if (g_->scene()) g_->scene()->setDoor(n); }
void SceneScriptHost::sceneShake() { if (g_->scene()) g_->scene()->shake(); }
void SceneScriptHost::sceneTransition(bool close)
{
    GameScene* s = g_->scene();
    if (!s) return;
    if (close) s->transitionClose(); else s->transitionOpen();
}
void SceneScriptHost::sceneLetterbox(const char* what)
{
    GameScene* s = g_->scene();
    if (!s) return;
    if (!strcasecmp(what, "init")) s->letterboxInit();
    else if (!strcasecmp(what, "open")) s->letterboxOpen();
    else if (!strcasecmp(what, "close")) s->letterboxClose();
    else if (!strcasecmp(what, "out")) s->transitionOut();
    else if (!strcasecmp(what, "in")) g_->letterboxIn = true;       // game+0x25c = 1
}
void SceneScriptHost::sceneSpeak(const char* key)
{
    GameScene* s = g_->scene();
    if (!s || !s->dialogue()) return;
    bool ok = s->dialogue()->speak(key);        // 0x4397c0
    if (ok) dialogueStarted();
    else {
        g_->switchTo("H000F");
        wakeNow();                               // port: the exe would wait for a dialogue that never starts
    }
}
void SceneScriptHost::sceneBlock(int x, int y, bool b) { g_->state().setBlocked(sceneId(), x, y, b); if (g_->world3d()) g_->world3d()->blockCell(x,y,b); }
void SceneScriptHost::sceneSparkle(int x, int y) { if (g_->hud()) g_->hud()->sparkle(x, y); }      // HUD object 0x457334, 0x41ebe0

// ------------------------------------------------------------------------------------------ game
void SceneScriptHost::gameSwitch(const char* t) { g_->switchTo(t); }
void SceneScriptHost::gameSkip() { stub("skip: scene vt+0x10c cutscene skip"); }
void SceneScriptHost::gameSetBlending(bool on) { g_->blending = on; }      // game+0x251
void SceneScriptHost::gameComment(const char* id) { g_->showComment(id); }
void SceneScriptHost::setChainedTarget(const char* n) { g_->chained = n; }
void SceneScriptHost::progressSave(ScriptActor* a) { stub("save: %s position + scene id into the progress object", an(a)); }
void SceneScriptHost::progressUndo(const char* n) { g_->state().undoScript(n); }
void SceneScriptHost::progressTexture(const char* n, const char* t) { g_->state().setTexture(n, t); }
void SceneScriptHost::progressGiveItem(const char* n) { g_->state().giveItem(n); }
void SceneScriptHost::uiAddItem(const char* name, const char* arg2, bool flag)
{
    g_->log("      HUD: add item %s (%s) flag=%d\n", name, arg2, flag ? 1 : 0);
    if (g_->hud()) g_->hud()->addItem(name, arg2);      // port: hud (0x41f2d0)
    uiMs_ = 1200.0f;     // the HUD's pickup animation (0x41f2d0); port: fixed wait
}
void SceneScriptHost::uiAddItemRefresh() {}
void SceneScriptHost::uiUnlock() { g_->log("      HUD: unlock\n"); uiMs_ = 1200.0f; }
void SceneScriptHost::uiVideo() { g_->log("      HUD: video unlocked\n"); uiMs_ = 1200.0f; }

// ----------------------------------------------------------------------------------------- sound
bool SceneScriptHost::voiceBusy() { return g_->scene() && g_->scene()->dialogue() && g_->scene()->dialogue()->busy(); }
bool SceneScriptHost::voiceDone()       // scene vt+0xdc (0x4371c0)
{
    GameScene* s = g_->scene();
    if (!s) return true;
    unsigned id = s->sceneId();
    if (id == 0x60301 || id == 0x404 || id == 0x40204 || id == 0x90402) return true;
    if ((id & 0xf00) == 0) return true;
    return s->barState() != 0;
}
ScriptSound* SceneScriptHost::soundOpen(const char* name, bool* shared)
{
    OMediaSound* s = g_->findSound(name, shared);
    if (!s) return 0;
    ScriptSound* h = new ScriptSound;
    h->snd = s;
    sounds_.push_back(h);
    return h;
}
void SceneScriptHost::soundPlay(ScriptSound* s, int channel, int pan)
{
    if (s && s->snd) g_->playOnChannel(channel, s->snd, pan == 1 ? -1 : pan == 2 ? 1 : 0);
}
void SceneScriptHost::soundRelease(ScriptSound* s) { (void)s; }      // handles are owned by the bank
bool SceneScriptHost::soundPlaying(bool idleRunner) { return g_->channelBusy(idleRunner ? 9 : 5); }
void SceneScriptHost::soundStopChannel(bool idleRunner) { g_->stopChannel(idleRunner ? 9 : 5); }

void SceneScriptHost::playObjectSound(SceneObject* o)           // 0x415710
{
    if (o->soundName.empty()) return;
    bool shared;
    OMediaSound* s = g_->findSound(o->soundName.c_str(), &shared);
    if (!s) return;
    int pan = 0;
    if (!o->is3D()) { if (o->px < 221) pan = -1; else if (o->px > 419) pan = 1; }
    g_->playOnChannel(5, s, pan);
}

// ---------------------------------------------------------------------------------- mini games
bool SceneScriptHost::miniGameExists() { return g_->miniGame() != 0; }
void SceneScriptHost::miniGameCreate() { g_->createMiniGame(); }
void SceneScriptHost::miniSlotsInit() { g_->miniGame()->slotsInit(); }
void SceneScriptHost::miniSlotsSpin() { g_->miniGame()->slotsSpin(); }
void SceneScriptHost::miniMaze(int n) { g_->miniGame()->maze(n); }
void SceneScriptHost::miniPhoneKey(int n) { g_->miniGame()->phone().key(n); } // chapter 4 phone (Phone.cpp)
void SceneScriptHost::miniPhoneReset() { g_->miniGame()->phone().reset(); } // chapter 4 phone (Phone.cpp)
void SceneScriptHost::miniDarts(int op) { if(g_->miniGame()) g_->miniGame()->darts(op); } // lane c: 0x436990 / 0x4369c0

}  // namespace sbgame
