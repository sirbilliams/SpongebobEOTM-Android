#include "ScriptRunner.h"
#include "Sb2File.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

namespace sbscript {

static std::string cut31(const std::string& s)   // port: the exe's fields are char[32] (strcpy without a bound)
{
    return s.size() > 31 ? s.substr(0, 31) : s;
}

static bool is(const std::string& s, const char* lit) { return strcmp(s.c_str(), lit) == 0; }

ScriptRunner::ScriptRunner(ScriptHost* host)
    : host_(host), sound_(0), lines_(kMaxLines + 2), count_(0), cur_(0), finished_(false), control_(false),
      actor_(0), waitUi_(false), waitComment_(false), waitTalk_(false), waitSound_(false),
      mouseX_(0), mouseY_(0), idleMs_(0.0f), terrain_(true), soundShared_(false), idleRunner_(false),
      trace_(0), autoRun_(true), steps_(0)
{
    // 0x43d550: +0x190a = 0, +0x190c = 0, +4 = 0, +0x194d = 0, +0x1955..0x1957 = 0, +0x1960 = 1, +0x195c = 0,
    // +0x1961 = 0, +0x1962 = 0.  (count and the line records are left uninitialised by the exe; zeroed here.)
}

ScriptRunner::~ScriptRunner()
{
    stopSound();   // 0x43d5d0
}

void ScriptRunner::stopSound()
{
    host_->soundStopChannel(idleRunner_);
    if (sound_ && !soundShared_) host_->soundRelease(sound_);
    sound_ = 0;
}

void ScriptRunner::freeSound()
{
    if (sound_) {
        if (!soundShared_) host_->soundRelease(sound_);
        sound_ = 0;
    }
}

static const char* const kCommands[] = {
    "walk", "checkgrid", "run", "walkgrid", "move", "movey", "movegrid", "skipif", "exitif", "blending",
    "shadow", "terrain", "hideattach", "showattach", "attach", "shake", "detach", "switch", "loop", "wait",
    "idle", "climb", "movie", "sparkle", "block", "unblock", "rotate", "setchar", "add", "unlock",
    "video", "skip", "script", "comment", "hide", "save", "undo", "control", "talk", "narrate",
    "transition", "letterbox", "sound", "texture", "setdoor", "show", "swapchar", "startanim", "anim",
    "anim2d", "shock", "slots", "maze", "kickit", "goal", "phone", "darts"
};

const char* const* ScriptRunner::commandNames() { return kCommands; }
unsigned ScriptRunner::commandCount() { return (unsigned)(sizeof kCommands / sizeof kCommands[0]); }
bool ScriptRunner::isKnownCommand(const std::string& cmd)
{
    for (unsigned i = 0; i < commandCount(); ++i) if (cmd == kCommands[i]) return true;
    return false;
}

bool ScriptRunner::parseRecords(const std::vector<std::string>& records, std::vector<ScriptLine>& out, std::string* err)
{
    if (records.size() > (size_t)kMaxLines) {
        if (err) *err = "more than 50 lines (the exe would overrun the runner object)";   // port: bounds
        return false;
    }
    out.assign(kMaxLines + 2, ScriptLine());
    for (size_t i = 0; i < records.size(); ++i) {
        SbTok tok(records[i]);
        std::string t;
        ScriptLine& l = out[i];
        // fields start as "" (0x4572f8); a blank line still takes a record slot
        if (tok.next(t)) {
            l.cmd = cut31(t);
            if (tok.next(t)) {
                l.a1 = cut31(t);
                if (tok.next(t)) {
                    l.a2 = cut31(t);
                    if (tok.next(t)) l.a3 = cut31(t);
                }
            }
        }
    }
    return true;
}

bool ScriptRunner::parse(const std::vector<std::string>& records, std::string* err)
{
    if (!parseRecords(records, lines_, err)) return false;
    count_ = (int)records.size();
    return true;
}

void ScriptRunner::begin(ScriptActor* actor)
{
    cur_ = -1;
    finished_ = false;
    waitUi_ = false;
    waitTalk_ = false;
    waitSound_ = false;
    terrain_ = true;
    idleMs_ = 0.0f;
    actor_ = 0;
    if (actor == 0) {
        ScriptActor* a = host_->activeActor();
        if (a) actor_ = a;
        host_->scriptStarted();
    } else {
        actor_ = actor;   // port: the exe is handed a sub-object and subtracts 0xe0
    }
    if (actor_ && !is(lines_[0].cmd, "control") && !is(lines_[0].a1, "on"))
        host_->actorBeginScript(actor_);
    if (autoRun_) runUntilWait();
    host_->mousePosition(&mouseX_, &mouseY_);
}

bool ScriptRunner::loadBytes(const char* name, ScriptActor* actor, const unsigned char* data, unsigned size, std::string* err)
{
    std::vector<std::string> records;
    std::string e;
    bool ok = sb2_decode(data, size, records, e);
    if (!ok) { if (err) *err = e; return false; }
    name_ = cut31(name);
    if (!parse(records, err)) return false;
    begin(actor);
    return true;
}

bool ScriptRunner::loadText(const char* name, ScriptActor* actor, const std::string& text)
{
    // 0x43d750: fgets(buf, 256) per record; a longer line is split
    std::vector<std::string> records;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t e = pos;
        while (e < text.size() && e - pos < 255 && text[e] != '\n') ++e;
        if (e < text.size() && text[e] == '\n') ++e;
        records.push_back(text.substr(pos, e - pos));
        pos = e;
    }
    name_ = cut31(name);
    if (!parse(records, 0)) return false;
    begin(actor);
    return true;
}

bool ScriptRunner::load(const char* name, ScriptActor* actor)
{
    char path[256];
    std::vector<unsigned char> data;
    snprintf(path, sizeof path, "scripts/c%d/%s.sb2", host_->chapter(), name);   // port: '/' for '\\'
    if (host_->readFile(path, data)) {
        std::string err;
        return loadBytes(name, actor, data.empty() ? (const unsigned char*)"" : &data[0], (unsigned)data.size(), &err);
    }
    // 0x43d750: developer text files, chapter folder first
    snprintf(path, sizeof path, "scripts/c%d/%s.txt", host_->chapter(), name);
    if (!host_->readFile(path, data)) {
        snprintf(path, sizeof path, "scripts/%s.txt", name);
        if (!host_->readFile(path, data)) return false;
    }
    return loadText(name, actor, std::string(data.begin(), data.end()));
}

void ScriptRunner::runUntilWait()
{
    // do { cVar = 0x43e110(); } while (cVar == 0);   (0x43aa80, 0x406050, the loaders)
    // port: stop after 100000 lines so a script that is all 'loop' and no wait cannot hang the caller
    for (int guard = 0; guard < 100000; ++guard)
        if (step()) return;
}

void ScriptRunner::end()
{
    finished_ = true;
    waitUi_ = waitComment_ = waitTalk_ = waitSound_ = false;
    terrain_ = true;
    idleMs_ = 0.0f;
    host_->markScriptSeen(name_.c_str());
    if (host_->sceneId() != 0x50203) {
        const char* n = name_.c_str();
        if (!strstr(n, "up") && !strstr(n, "down") && strcmp(n, "sinitslots") != 0 &&
            strcmp(n, "sspinslots") != 0 && strcmp(n, "sreturn") != 0 && strcmp(n, "swash") != 0)
            host_->scriptEnded();
    }
}

bool ScriptRunner::skipToLetterboxClose()
{
    int saved = cur_;
    for (;;) {
        do {
            ++cur_;
            if (cur_ == count_) { cur_ = saved; return false; }
        } while (!is(lines_[cur_].cmd, "letterbox"));
        if (is(lines_[cur_].a1, "close")) break;
    }
    --cur_;
    waitUi_ = waitComment_ = waitTalk_ = waitSound_ = false;
    terrain_ = true;
    idleMs_ = 0.0f;
    return true;
}

bool ScriptRunner::talkAhead() const
{
    for (int i = cur_ + 1; i < count_; ++i)
        if (is(lines_[i].cmd, "talk")) return true;
    return false;
}

bool ScriptRunner::idleElapsed(float dtMs)
{
    if (idleMs_ > 0.0f) {
        idleMs_ -= dtMs;
        if (idleMs_ <= 0.0f) { idleMs_ = 0.0f; return true; }
    }
    return false;
}

bool ScriptRunner::soundDone()
{
    if (waitSound_ && sound_) {
        if (!host_->soundPlaying(idleRunner_)) { waitSound_ = false; return true; }
    }
    return false;
}

void ScriptRunner::onClick(short x, short y)
{
    if (x == mouseX_ && y == mouseY_) {
        if (!is(lastCmd_, "walk") && !is(lastCmd_, "walkgrid")) return;
        if (actor_ && actor_ == host_->activeActor()) host_->actorStopWalk(actor_);
    }
}

bool ScriptRunner::startSound(const ScriptLine& l)
{
    // 'sound' (0x43f5f4)
    int pan = 0;
    if (!l.a2.empty()) {
        if (is(l.a2, "left")) pan = 1;
        else if (is(l.a2, "right")) pan = 2;
    }
    if (sound_) {
        if (!soundShared_) host_->soundRelease(sound_);
        sound_ = 0;
    }
    if (host_->voiceBusy()) {
        if (!host_->voiceDone()) return false;
    }
    if (!l.a1.empty()) {
        bool shared = false;
        ScriptSound* s = host_->soundOpen(l.a1.c_str(), &shared);
        if (s) { sound_ = s; soundShared_ = shared; }
    }
    if (!sound_) return false;
    host_->soundPlay(sound_, idleRunner_ ? 9 : 5, pan);
    return false;
}

bool ScriptRunner::doGoal()
{
    // 'goal' (0x44027b): the dart / token game ends the script by rewriting the lines after it
    if (actor_ && host_->actorGoal(actor_)) {
        freeSound();
        bool shared = false;
        ScriptSound* s = host_->soundOpen("cheer", &shared);
        if (s) { sound_ = s; soundShared_ = shared; }
        if (sound_) host_->soundPlay(sound_, idleRunner_ ? 9 : 5, -1);
        ScriptActor* sb = host_->findActor("sb");
        if (sb) actor_ = sb;
        host_->actorPlayAnim(actor_, "dance01", true, 1.0);
        if (cur_ + 2 < kMaxLines) {   // port: bounds
            lines_[cur_ + 1].cmd = "setchar";
            lines_[cur_ + 1].a1 = "beachball";
            ++count_;
            lines_[cur_ + 2].cmd = "move";
            lines_[cur_ + 2].a1 = "150";
            lines_[cur_ + 2].a2 = "-200";
            ++count_;
        }
        return true;
    }
    if (actor_ && host_->actorGoalMissed(actor_)) {
        if (cur_ + 1 < kMaxLines) {   // port: bounds
            lines_[cur_ + 1].cmd = "move";
            lines_[cur_ + 1].a1 = "150";
            lines_[cur_ + 1].a2 = "-200";
            ++count_;
        }
    }
    return false;
}

// The dispatcher.  Same order as the exe's strcmp chain; true = "return 1" (the script waits or ended),
// false = "jmp 0x440aa5" (return 0, run the next line).  `A` is a null guard: the exe dereferences
// the actor unchecked, a script without an actor would crash it  // port: no-op instead.
#define A_OR_RETURN if (!actor_) return false

bool ScriptRunner::step()
{
    ++cur_;
    waitUi_ = false;
    waitComment_ = false;
    waitTalk_ = false;
    waitSound_ = false;
    if (cur_ == count_) {
        end();
        return true;
    }
    if (cur_ < 0 || cur_ >= (int)lines_.size()) return true;   // port: bounds
    const ScriptLine& L = lines_[cur_];
    ++steps_;
    if (trace_)
        fprintf((FILE*)trace_, "[%2d] %s\t%s\t%s\t%s\n", cur_, L.cmd.c_str(), L.a1.c_str(), L.a2.c_str(), L.a3.c_str());
    lastCmd_ = cut31(L.cmd);   // +0x192d
    const std::string& c = L.cmd;
    const char* a1 = L.a1.c_str();
    const char* a2 = L.a2.c_str();
    const char* a3 = L.a3.c_str();

    if (is(c, "walk")) {                                         // 0x43e19c
        A_OR_RETURN;
        return host_->actorWalk(actor_, (float)atof(a1), (float)atof(a2), 0, -1, -1);
    }
    if (is(c, "checkgrid")) {                                    // 0x43e23c
        A_OR_RETURN;
        double v = atof(a1);
        float x, y, z;
        host_->actorPosition(actor_, &x, &y, &z);
        if (v == (double)y) return false;
        host_->actorReposition(actor_, x, z);
        return true;
    }
    if (is(c, "run")) {                                          // 0x43e2b5
        A_OR_RETURN;
        if (host_->actorWalk(actor_, (float)atof(a1), (float)atof(a2), 0, -1, -1)) {
            host_->actorSetRunning(actor_);
            return true;
        }
        return false;
    }
    if (is(c, "walkgrid")) {                                     // 0x43e347
        if (!actor_) return false;
        host_->actorPrepareWalkGrid(actor_);
        return host_->actorWalk(actor_, 0.0f, 0.0f, 0, atoi(a1), atoi(a2));
    }
    if (is(c, "move")) {                                         // 0x43e3e4
        A_OR_RETURN;
        host_->actorMove(actor_, (float)atof(a1), (float)atof(a2));
        return false;
    }
    if (is(c, "movey")) {                                        // 0x43e44b
        A_OR_RETURN;
        host_->actorMoveY(actor_, (float)atof(a1));
        return false;
    }
    if (is(c, "movegrid")) {                                     // 0x43e49b
        A_OR_RETURN;
        host_->actorMoveGrid(actor_, atoi(a1), atoi(a2));
        return false;
    }
    if (is(c, "skipif")) {                                       // 0x43e504
        if (host_->condition(a1)) cur_ += 2;
        return false;
    }
    if (is(c, "exitif")) {                                       // 0x43e557
        if (strlen(a1) > 1 && host_->condition(a1)) {
            end();
            return true;
        }
        return false;
    }
    if (is(c, "blending")) {                                     // 0x43e5cf
        if (is(L.a1, "on")) host_->gameSetBlending(true);
        if (is(L.a1, "off")) host_->gameSetBlending(false);
        return false;
    }
    if (is(c, "shadow")) {                                       // 0x43e656
        if (is(L.a1, "on")) { A_OR_RETURN; host_->actorShadow(actor_, true); }
        if (is(L.a1, "off")) { A_OR_RETURN; host_->actorShadow(actor_, false); }
        return false;
    }
    if (is(c, "terrain")) {                                      // 0x43e6da
        if (is(L.a1, "on")) terrain_ = true;
        if (is(L.a1, "off")) terrain_ = false;
        return false;
    }
    if (is(c, "hideattach")) {                                   // 0x43e755
        if (actor_) host_->actorHideAttach(actor_);
        return false;
    }
    if (is(c, "showattach")) {                                   // 0x43e79d
        if (actor_) host_->actorShowAttach(actor_);
        return false;
    }
    if (is(c, "attach")) {                                       // 0x43e7f7
        A_OR_RETURN;
        host_->actorAttach(actor_, a1, a2, a3);
        return false;
    }
    if (is(c, "shake")) {                                        // 0x43e8c7
        host_->sceneShake();
        return true;
    }
    if (is(c, "detach")) {                                       // 0x43e90f
        A_OR_RETURN;
        host_->actorDetach(actor_);
        return false;
    }
    if (is(c, "switch")) {                                       // 0x43e942
        host_->gameSwitch(a1);
        return false;
    }
    if (is(c, "loop")) {                                         // 0x43e984
        cur_ = -1;
        return false;
    }
    if (is(c, "wait")) {                                         // 0x43e9b2: looks at the line before it
        static const ScriptLine none;
        const ScriptLine& p = cur_ > 0 ? lines_[cur_ - 1] : none;   // port: the exe reads memory before line 0
        if (is(p.cmd, "add") || is(p.cmd, "unlock") || is(p.cmd, "video")) {
            waitUi_ = true;
        } else {
            if (is(p.cmd, "comment")) { waitComment_ = true; return true; }
            if (is(p.cmd, "sound") && sound_) { waitSound_ = true; return true; }
            if (is(p.cmd, "narrate")) { waitTalk_ = true; return true; }
        }
        return true;
    }
    if (is(c, "idle")) {                                         // 0x43eb19
        if (actor_ == 0) {
            idleMs_ = (float)(atof(a1) * 1000.0);
            return true;
        }
        host_->actorIdle(actor_, (float)atof(a1));
        return true;
    }
    if (is(c, "climb")) {                                        // 0x43ebb5
        A_OR_RETURN;
        host_->actorClimb(actor_, (float)atof(a1));
        return false;
    }
    if (is(c, "movie")) {                                        // 0x43ec05
        host_->gameSwitch(a1);
        return false;
    }
    if (is(c, "sparkle")) {                                      // 0x43ec47
        host_->sceneSparkle(atoi(a1), atoi(a2));
        return false;
    }
    if (is(c, "block")) {                                        // 0x43eca7
        host_->sceneBlock(atoi(a1) - 1, atoi(a2) - 1, true);
        return false;
    }
    if (is(c, "unblock")) {                                      // 0x43ed67
        host_->sceneBlock(atoi(a1) - 1, atoi(a2) - 1, false);
        return false;
    }
    if (is(c, "rotate")) {                                       // 0x43edf9: degrees -> 1/16384 turn, 500 ms
        A_OR_RETURN;
        int32_t t = (int32_t)((uint32_t)atoi(a1) << 14);
        return host_->actorRotate(actor_, t / 360, 500);
    }
    if (is(c, "setchar")) {                                      // 0x43ee7f
        ScriptActor* a = host_->findActor(a1);
        if (a) actor_ = a;
        return false;
    }
    if (is(c, "add")) {                                          // 0x43eed4
        if (is(L.a1, "isp1") || is(L.a1, "iem1") || is(L.a1, "ikc1")) {
            host_->uiAddItem(a1, a2, false);
        } else {
            host_->uiAddItem(a1, a2, true);
            host_->uiAddItemRefresh();
        }
        host_->progressGiveItem(a1);
        return false;
    }
    if (is(c, "unlock")) {                                       // 0x43efcb
        if (host_->sceneId() == 0x403 && host_->scriptSeen("snelson")) ++cur_;
        else host_->uiUnlock();
        return false;
    }
    if (is(c, "video")) {                                        // 0x43f032
        host_->uiVideo();
        return false;
    }
    if (is(c, "skip")) {                                         // 0x43f062
        host_->gameSkip();
        return false;
    }
    if (is(c, "script")) {                                       // 0x43f095
        host_->setChainedTarget(a1);
        return false;
    }
    if (is(c, "comment")) {                                      // 0x43f0f1
        host_->gameComment(a1);
        if (actor_) host_->actorTalkAnim(actor_);
        return false;
    }
    if (is(c, "hide")) {                                         // 0x43f15f
        host_->sceneHide(a1);
        return false;
    }
    if (is(c, "save")) {                                         // 0x43f1a7
        A_OR_RETURN;
        host_->progressSave(actor_);
        return false;
    }
    if (is(c, "undo")) {                                         // 0x43f213
        host_->progressUndo(a1);
        return false;
    }
    if (is(c, "control")) {                                      // 0x43f252
        if (is(L.a1, "on")) {
            ScriptActor* a = host_->activeActor();
            if (a) host_->actorSetControl(a, true);
            control_ = true;
        }
        if (is(L.a1, "off")) {
            ScriptActor* a = host_->activeActor();
            if (a) host_->actorSetControl(a, false);
            control_ = false;
        }
        return false;
    }
    if (is(c, "talk")) {                                         // 0x43f306
        host_->sceneSpeak(a1);
        waitTalk_ = true;
        return true;
    }
    if (is(c, "narrate")) {                                      // 0x43f365
        host_->sceneSpeak(a1);
        waitTalk_ = false;
        return false;
    }
    if (is(c, "transition")) {                                   // 0x43f3b3
        if (is(L.a1, "close")) { host_->sceneTransition(true); return true; }
        if (is(L.a1, "open")) { host_->sceneTransition(false); return true; }
        return false;
    }
    if (is(c, "letterbox")) {                                    // 0x43f496
        if (is(L.a1, "init")) { host_->sceneLetterbox("init"); return false; }
        if (is(L.a1, "open")) { host_->sceneLetterbox("open"); return true; }
        if (is(L.a1, "close")) { host_->sceneLetterbox("close"); return true; }
        if (is(L.a1, "out")) { host_->sceneLetterbox("out"); return false; }
        if (is(L.a1, "in")) { host_->sceneLetterbox("in"); return false; }
        return false;
    }
    if (is(c, "sound")) {                                        // 0x43f5f4
        return startSound(L);
    }
    if (is(c, "texture")) {                                      // 0x43fb68
        ScriptActor* a = host_->findActor(a1);
        if (a) host_->actorSetTexture(a, a2);
        host_->progressTexture(a1, a2);
        return false;
    }
    if (is(c, "setdoor")) {                                      // 0x43fbed
        host_->sceneSetDoor(a1);
        return false;
    }
    if (is(c, "show")) {                                         // 0x43fc47
        host_->sceneShow(a1);
        if (is(L.a1, "can")) {
            ScriptActor* a = host_->findActor("can");
            if (a) host_->actorLoopAnim(a, "idle1");   // port: null check
        }
        return false;
    }
    if (is(c, "swapchar")) {                                     // 0x43fced
        host_->swapPlayerHelmet();
        return false;
    }
    if (is(c, "startanim")) {                                    // 0x43fdb0
        A_OR_RETURN;
        int speed = L.a2.empty() ? 1 : atoi(a2);
        host_->actorPlayAnim(actor_, a1, false, (double)speed);
        return false;
    }
    if (is(c, "anim")) {                                         // 0x43fe31
        A_OR_RETURN;
        int speed = L.a2.empty() ? 1 : atoi(a2);
        if (host_->actorPlayAnim(actor_, a1, true, (double)speed)) {
            if (is(L.a1, "kickdrop") || is(L.a1, "kickup")) {
                ScriptActor* can = host_->findActor("can");
                if (can) host_->actorOneShotAnim(can, a1);
            }
            return true;
        }
        return false;
    }
    if (is(c, "anim2d")) {                                       // 0x43ff4d
        host_->sceneAnim2D(a1);
        return true;
    }
    if (is(c, "shock")) {                                        // 0x43ffda
        host_->shockEffect();
        return false;
    }
    if (is(c, "slots")) {                                        // 0x4400b6
        if (!host_->miniGameExists()) host_->miniGameCreate();
        if (is(L.a1, "init")) host_->miniSlotsInit();
        else if (is(L.a1, "spin")) { host_->miniSlotsSpin(); return true; }
        return false;
    }
    if (is(c, "maze")) {                                         // 0x4401a3
        if (!host_->miniGameExists()) host_->gameSwitch("c2l5s2p2");
        else host_->miniMaze(atoi(a1));
        return false;
    }
    if (is(c, "kickit")) {                                       // 0x440213
        A_OR_RETURN;
        host_->actorKick(actor_);
        return true;
    }
    if (is(c, "goal")) {                                         // 0x44027b
        return doGoal();
    }
    if (is(c, "phone")) {                                        // 0x440774
        if (is(L.a1, "11")) {
            if (host_->miniGameExists()) host_->miniPhoneKey(11);
            return false;
        }
        if (!host_->miniGameExists()) host_->miniGameCreate();
        if (is(L.a1, "0")) { host_->miniPhoneReset(); return false; }
        static const char* const keys[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
        for (int i = 0; i < 10; ++i)
            if (is(L.a1, keys[i])) { host_->miniPhoneKey(i + 1); break; }
        return false;
    }
    if (is(c, "darts")) {                                        // 0x4409de
        if (!host_->miniGameExists()) host_->miniGameCreate();
        if (is(L.a1, "0")) host_->miniDarts(0);
        else if (is(L.a1, "1")) host_->miniDarts(1);
        return false;
    }
    return false;   // unknown command: the chain falls through to 0x440aa5, the line does nothing
}

}  // namespace sbscript
