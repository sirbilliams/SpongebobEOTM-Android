#include "ScriptHost.h"
#include "ScriptCondition.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

namespace sbscript {

// Placeholder objects handed back by the logging host.
struct ScriptActor { std::string name; };
struct ScriptSound { std::string name; };

namespace {
struct LogAtoms : public ConditionAtoms {
    LogAtoms(const std::vector<std::string>& f, std::vector<std::string>& seen, unsigned scene)
        : flags(f), seen_(seen), scene_(scene) {}
    const std::vector<std::string>& flags;
    std::vector<std::string>& seen_;
    unsigned scene_;
    bool has(const char* t) const { return std::find(flags.begin(), flags.end(), std::string(t)) != flags.end(); }
    virtual bool debugAll() { return false; }
    virtual bool scriptSeen(const char* t) { return std::find(seen_.begin(), seen_.end(), std::string(t)) != seen_.end() || has(t); }
    virtual bool hasItem(const char* t) { return has(t); }
    virtual bool flagSet(const char* t) { return has(t); }
    virtual bool dialogueStarted(unsigned, const char* k) { return has(k); }
    virtual unsigned currentSceneId() { return scene_; }
    virtual bool miniGameFlag() { return false; }
};
}  // namespace

LoggingScriptHost::LoggingScriptHost()
    : walkBlocks(true), rotateBlocks(true), animBlocks(true), anim2dBlocks(true), goalReached(false),
      goalMissedValue(false), out_(stdout), chapter_(1), sceneId_(0x00000001), calls_(0), miniExists_(false), active_(0)
{
}

LoggingScriptHost::~LoggingScriptHost()
{
    for (size_t i = 0; i < actors_.size(); ++i) delete actors_[i];
}

void LoggingScriptHost::logf(const char* fmt, ...)
{
    ++calls_;
    if (!out_) return;
    va_list ap;
    va_start(ap, fmt);
    fputs("      host: ", (FILE*)out_);
    vfprintf((FILE*)out_, fmt, ap);
    fputc('\n', (FILE*)out_);
    va_end(ap);
}

static const char* an(ScriptActor* a) { return a ? a->name.c_str() : "(null)"; }

int  LoggingScriptHost::chapter() { return chapter_; }
bool LoggingScriptHost::readFile(const char* path, std::vector<unsigned char>& out)
{
    std::string p = root_.empty() ? std::string(path) : root_ + "/" + path;
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    out.clear();
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}
unsigned LoggingScriptHost::sceneId() { return sceneId_; }
bool LoggingScriptHost::condition(const char* expr)
{
    LogAtoms atoms(flags, seen_, sceneId_);
    bool r = evalCondition(expr, false, atoms);
    logf("condition(\"%s\") -> %d", expr, r ? 1 : 0);
    return r;
}
void LoggingScriptHost::scriptStarted() { logf("scriptStarted()"); }
void LoggingScriptHost::mousePosition(short* x, short* y) { *x = 320; *y = 240; }
void LoggingScriptHost::scriptEnded() { logf("scriptEnded()"); }
void LoggingScriptHost::markScriptSeen(const char* n)
{
    if (std::find(seen_.begin(), seen_.end(), std::string(n)) == seen_.end()) seen_.push_back(n);
    logf("markScriptSeen(\"%s\")", n);
}
bool LoggingScriptHost::scriptSeen(const char* n)
{
    return std::find(seen_.begin(), seen_.end(), std::string(n)) != seen_.end();
}

ScriptActor* LoggingScriptHost::activeActor()
{
    if (!active_) active_ = findActor("sb");
    return active_;
}
void LoggingScriptHost::setActiveActor(ScriptActor* a) { active_ = a; logf("setActiveActor(%s)", an(a)); }
ScriptActor* LoggingScriptHost::findActor(const char* name)
{
    for (size_t i = 0; i < actors_.size(); ++i) if (actors_[i]->name == name) return actors_[i];
    ScriptActor* a = new ScriptActor();
    a->name = name;
    actors_.push_back(a);
    return a;
}
void LoggingScriptHost::actorBeginScript(ScriptActor* a) { logf("actorBeginScript(%s)", an(a)); }
void LoggingScriptHost::actorSetControl(ScriptActor* a, bool on) { logf("actorSetControl(%s, %d)", an(a), on); }
bool LoggingScriptHost::actorWalk(ScriptActor* a, float x, float y, int flag, int gx, int gy)
{
    logf("actorWalk(%s, %g, %g, %d, %d, %d) -> %d", an(a), x, y, flag, gx, gy, walkBlocks);
    return walkBlocks;
}
void LoggingScriptHost::actorPrepareWalkGrid(ScriptActor* a) { logf("actorPrepareWalkGrid(%s)", an(a)); }
void LoggingScriptHost::actorSetRunning(ScriptActor* a) { logf("actorSetRunning(%s)", an(a)); }
void LoggingScriptHost::actorPosition(ScriptActor*, float* x, float* y, float* z) { *x = 0; *y = 0; *z = 0; }
void LoggingScriptHost::actorReposition(ScriptActor* a, float x, float z) { logf("actorReposition(%s, %g, %g)", an(a), x, z); }
void LoggingScriptHost::actorMove(ScriptActor* a, float x, float y) { logf("actorMove(%s, %g, %g)", an(a), x, y); }
void LoggingScriptHost::actorMoveY(ScriptActor* a, float y) { logf("actorMoveY(%s, %g)", an(a), y); }
void LoggingScriptHost::actorMoveGrid(ScriptActor* a, int gx, int gy) { logf("actorMoveGrid(%s, %d, %d)", an(a), gx, gy); }
void LoggingScriptHost::actorClimb(ScriptActor* a, float h) { logf("actorClimb(%s, %g)", an(a), h); }
bool LoggingScriptHost::actorRotate(ScriptActor* a, int angle, int time)
{
    logf("actorRotate(%s, %d, %d) -> %d", an(a), angle, time, rotateBlocks);
    return rotateBlocks;
}
void LoggingScriptHost::actorIdle(ScriptActor* a, float s) { logf("actorIdle(%s, %g)", an(a), s); }
void LoggingScriptHost::actorAttach(ScriptActor* a, const char* o, const char* b1, const char* b2) { logf("actorAttach(%s, \"%s\", \"%s\", \"%s\")", an(a), o, b1, b2); }
void LoggingScriptHost::actorDetach(ScriptActor* a) { logf("actorDetach(%s)", an(a)); }
void LoggingScriptHost::actorHideAttach(ScriptActor* a) { logf("actorHideAttach(%s)", an(a)); }
void LoggingScriptHost::actorShowAttach(ScriptActor* a) { logf("actorShowAttach(%s)", an(a)); }
bool LoggingScriptHost::actorPlayAnim(ScriptActor* a, const char* n, bool wait, double speed)
{
    bool r = wait && animBlocks;
    logf("actorPlayAnim(%s, \"%s\", wait=%d, speed=%g) -> %d", an(a), n, wait, speed, r);
    return r;
}
void LoggingScriptHost::actorLoopAnim(ScriptActor* a, const char* n) { logf("actorLoopAnim(%s, \"%s\")", an(a), n); }
void LoggingScriptHost::actorTalkAnim(ScriptActor* a) { logf("actorTalkAnim(%s)", an(a)); }
void LoggingScriptHost::actorOneShotAnim(ScriptActor* a, const char* n) { logf("actorOneShotAnim(%s, \"%s\")", an(a), n); }
void LoggingScriptHost::actorKick(ScriptActor* a) { logf("actorKick(%s)", an(a)); }
bool LoggingScriptHost::actorGoal(ScriptActor* a) { logf("actorGoal(%s) -> %d", an(a), goalReached); return goalReached; }
bool LoggingScriptHost::actorGoalMissed(ScriptActor* a) { logf("actorGoalMissed(%s) -> %d", an(a), goalMissedValue); return goalMissedValue; }
void LoggingScriptHost::actorStopWalk(ScriptActor* a) { logf("actorStopWalk(%s)", an(a)); }
void LoggingScriptHost::actorShadow(ScriptActor* a, bool on) { logf("actorShadow(%s, %d)", an(a), on); }
void LoggingScriptHost::actorSetTexture(ScriptActor* a, const char* t) { logf("actorSetTexture(%s, \"%s\")", an(a), t); }
void LoggingScriptHost::swapPlayerHelmet() { logf("swapPlayerHelmet()"); }
void LoggingScriptHost::shockEffect() { logf("shockEffect()"); }
void LoggingScriptHost::sceneHide(const char* n) { logf("sceneHide(\"%s\")", n); }
void LoggingScriptHost::sceneShow(const char* n) { logf("sceneShow(\"%s\")", n); }
bool LoggingScriptHost::sceneAnim2D(const char* n) { logf("sceneAnim2D(\"%s\")", n); return anim2dBlocks; }
void LoggingScriptHost::sceneSetDoor(const char* n) { logf("sceneSetDoor(\"%s\")", n); }
void LoggingScriptHost::sceneShake() { logf("sceneShake()"); }
void LoggingScriptHost::sceneTransition(bool close) { logf("sceneTransition(%s)", close ? "close" : "open"); }
void LoggingScriptHost::sceneLetterbox(const char* w) { logf("sceneLetterbox(\"%s\")", w); }
void LoggingScriptHost::sceneSpeak(const char* k) { logf("sceneSpeak(\"%s\")", k); }
void LoggingScriptHost::sceneBlock(int x, int y, bool b) { logf("sceneBlock(%d, %d, %d)", x, y, b); }
void LoggingScriptHost::sceneSparkle(int x, int y) { logf("sceneSparkle(%d, %d)", x, y); }
void LoggingScriptHost::gameSwitch(const char* t) { logf("gameSwitch(\"%s\")", t); }
void LoggingScriptHost::gameSkip() { logf("gameSkip()"); }
void LoggingScriptHost::gameSetBlending(bool on) { logf("gameSetBlending(%d)", on); }
void LoggingScriptHost::gameComment(const char* id) { logf("gameComment(\"%s\")", id); }
void LoggingScriptHost::setChainedTarget(const char* n) { logf("setChainedTarget(\"%s\")", n); }
void LoggingScriptHost::progressSave(ScriptActor* a) { logf("progressSave(%s)", an(a)); }
void LoggingScriptHost::progressUndo(const char* n) { logf("progressUndo(\"%s\")", n); }
void LoggingScriptHost::progressTexture(const char* n, const char* t) { logf("progressTexture(\"%s\", \"%s\")", n, t); }
void LoggingScriptHost::progressGiveItem(const char* n) { logf("progressGiveItem(\"%s\")", n); }
void LoggingScriptHost::uiAddItem(const char* n, const char* a2, bool f) { logf("uiAddItem(\"%s\", \"%s\", %d)", n, a2, f); }
void LoggingScriptHost::uiAddItemRefresh() { logf("uiAddItemRefresh()"); }
void LoggingScriptHost::uiUnlock() { logf("uiUnlock()"); }
void LoggingScriptHost::uiVideo() { logf("uiVideo()"); }
bool LoggingScriptHost::voiceBusy() { return false; }
bool LoggingScriptHost::voiceDone() { return true; }
ScriptSound* LoggingScriptHost::soundOpen(const char* name, bool* shared)
{
    ScriptSound* s = new ScriptSound();   // released in soundRelease
    s->name = name;
    *shared = false;
    logf("soundOpen(\"%s\")", name);
    return s;
}
void LoggingScriptHost::soundPlay(ScriptSound* s, int ch, int pan) { logf("soundPlay(\"%s\", channel=%d, pan=%d)", s->name.c_str(), ch, pan); }
void LoggingScriptHost::soundRelease(ScriptSound* s) { delete s; }
bool LoggingScriptHost::soundPlaying(bool) { return false; }
void LoggingScriptHost::soundStopChannel(bool idleRunner) { ++calls_; (void)idleRunner; }
bool LoggingScriptHost::miniGameExists() { return miniExists_; }
void LoggingScriptHost::miniGameCreate() { miniExists_ = true; logf("miniGameCreate()"); }
void LoggingScriptHost::miniSlotsInit() { logf("miniSlotsInit()"); }
void LoggingScriptHost::miniSlotsSpin() { logf("miniSlotsSpin()"); }
void LoggingScriptHost::miniMaze(int n) { logf("miniMaze(%d)", n); }
void LoggingScriptHost::miniPhoneKey(int n) { logf("miniPhoneKey(%d)", n); }
void LoggingScriptHost::miniPhoneReset() { logf("miniPhoneReset()"); }
void LoggingScriptHost::miniDarts(int op) { logf("miniDarts(%d)", op); }

}  // namespace sbscript
