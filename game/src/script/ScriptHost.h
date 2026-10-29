/*  The world-side interface of the script interpreter (sb2.exe 0x43e110).  Every command that acts on
    the scene, a character, the sound manager or the game state calls one virtual here; the scene/engine
    side implements them later.  Comments give the exe object and vtable slot the original calls.
    Naming follows what the command does, the exe has no symbols.  See docs/script.md.  */
#pragma once
#include "Sb2File.h"
#include <string>
#include <vector>

namespace sbscript {

struct ScriptActor;   // opaque: the scene's character object (runner+0x1950, scene+0x248, scene vt+0xe4)
struct ScriptSound;   // opaque: a loaded 'Wave' sound handle (runner+4)

class ScriptHost : public FileSource {
public:
    virtual ~ScriptHost() {}

    // ---- environment -------------------------------------------------------------------------
    virtual int  chapter() = 0;                                  // game+0x154 (short), picks the scripts/c%d folder
    virtual unsigned sceneId() = 0;                              // scene+0x264: scene<<16 | level<<8 | chapter (chapter is the LOW nibble)
    virtual bool condition(const char* expr) = 0;                // game vt+0xb8 (expr, 0): see ScriptCondition.h
    virtual void scriptStarted() = 0;                            // game+0x88 vt+0x38, called by the loader (param_3 == 0)
    virtual void mousePosition(short* x, short* y) = 0;          // input manager vt+8 (stored at runner+0x1958)
    virtual void scriptEnded() = 0;                              // scene vt+0x100, from the end-of-script routine 0x43dfd0
    virtual void markScriptSeen(const char* scriptName) = 0;     // progress(0x457330) 0x416610
    virtual bool scriptSeen(const char* scriptName) = 0;         // progress 0x416690

    // ---- actors (scene vt+0xe4 lookup, character vtable slots) ---------------------------------
    virtual ScriptActor* activeActor() = 0;                      // scene+0x248
    virtual void setActiveActor(ScriptActor* a) = 0;             // scene+0x248 = a
    virtual ScriptActor* findActor(const char* name) = 0;        // scene vt+0xe4 (may return 0)
    virtual void actorBeginScript(ScriptActor* a) = 0;           // loader: char+0x37c=0, +0x3cc=0, +0x358=0
    virtual void actorSetControl(ScriptActor* a, bool on) = 0;   // char+0x32a
    virtual bool actorWalk(ScriptActor* a, float x, float y, int flag, int gridX, int gridY) = 0;  // vt+0x144; true: wait
    virtual void actorPrepareWalkGrid(ScriptActor* a) = 0;       // vt+0xfc, before 'walkgrid'
    virtual void actorSetRunning(ScriptActor* a) = 0;            // char+0x358 = 1 after a 'run' that started
    virtual void actorPosition(ScriptActor* a, float* x, float* y, float* z) = 0;   // char+0x310/0x314/0x318
    virtual void actorReposition(ScriptActor* a, float x, float z) = 0;             // vt+0x178 (checkgrid mismatch)
    virtual void actorMove(ScriptActor* a, float x, float y) = 0;                   // vt+0x148
    virtual void actorMoveY(ScriptActor* a, float y) = 0;                           // vt+0x14c
    virtual void actorMoveGrid(ScriptActor* a, int gx, int gy) = 0;                 // vt+0x150
    virtual void actorClimb(ScriptActor* a, float h) = 0;                           // vt+0x154
    virtual bool actorRotate(ScriptActor* a, int angle, int time) = 0;              // vt+0x13c; true: wait
    virtual void actorIdle(ScriptActor* a, float seconds) = 0;                      // vt+0x168
    virtual void actorAttach(ScriptActor* a, const char* object, const char* bone1, const char* bone2) = 0;  // vt+0x12c
    virtual void actorDetach(ScriptActor* a) = 0;                                   // vt+0x130
    virtual void actorHideAttach(ScriptActor* a) = 0;                               // char+0xad8 object vt+0x30(1)
    virtual void actorShowAttach(ScriptActor* a) = 0;                               // char+0xad8 object vt+0x30(0)
    virtual bool actorPlayAnim(ScriptActor* a, const char* name, bool wait, double speed) = 0;  // vt+0xb8
    virtual void actorLoopAnim(ScriptActor* a, const char* name) = 0;               // vt+0xb4 (name, FLT_MAX, 0, 0)
    virtual void actorTalkAnim(ScriptActor* a) = 0;                                 // vt+0xc0("talk", 0, FLT_MAX)
    virtual void actorOneShotAnim(ScriptActor* a, const char* name) = 0;            // vt+0xb4 (name, 0, 1.0, 0, 0): kickdrop/kickup on "can"
    virtual void actorKick(ScriptActor* a) = 0;                                     // vt+0x16c
    virtual bool actorGoal(ScriptActor* a) = 0;                                     // vt+0x174
    virtual bool actorGoalMissed(ScriptActor* a) = 0;                               // vt+0x188
    virtual void actorStopWalk(ScriptActor* a) = 0;                                 // vt+0x184 (0x43dea0)
    virtual void actorShadow(ScriptActor* a, bool on) = 0;                          // 0x434430 / 0x434410
    virtual void actorSetTexture(ScriptActor* a, const char* texture) = 0;          // vt+0xf0
    virtual void swapPlayerHelmet() = 0;                                            // 'swapchar': sb <-> sbhelm (see docs)
    virtual void shockEffect() = 0;                                                 // 'shock': 2D object "elec" at the player's screen position

    // ---- scene (game+0x90) ---------------------------------------------------------------------
    virtual void sceneHide(const char* name) = 0;                // vt+0xfc
    virtual void sceneShow(const char* name) = 0;                // vt+0xf8
    virtual bool sceneAnim2D(const char* name) = 0;              // vt+0xf4 object, its vt+0x9c(); true: wait (null object: port no-op)
    virtual void sceneSetDoor(const char* name) = 0;             // scene+0x1c80 = vt+0xf0(name)
    virtual void sceneShake() = 0;                               // vt+0x15c
    virtual void sceneTransition(bool close) = 0;                // vt+0x134 / 0x138 (+ scene+0x1c9a/0x1c98 on close)
    virtual void sceneLetterbox(const char* what) = 0;           // init=vt+0x128 open=+0x12c close=+0x130 out=+0x134, in: game+0x25c=1
    virtual void sceneSpeak(const char* key) = 0;                // vt+0xb4: 'talk' / 'narrate' start dialogue line `key` (e.g. BAA)
    virtual void sceneBlock(int x, int y, bool blocked) = 0;     // grid 0x4181c0 + progress 0x416ba0 + game vt+0x84
    virtual void sceneSparkle(int x, int y) = 0;                 // object 0x457334 0x41ebe0

    // ---- game ----------------------------------------------------------------------------------
    virtual void gameSwitch(const char* target) = 0;             // game vt+0xb0: scene / movie / script / caption (see docs)
    virtual void gameSkip() = 0;                                 // game vt+0xfc
    virtual void gameSetBlending(bool on) = 0;                   // game+0x251
    virtual void gameComment(const char* id) = 0;                // game+0xc4 vt+0x14
    virtual void setChainedTarget(const char* name) = 0;         // 'script': game+0x186 = name
    virtual void progressSave(ScriptActor* a) = 0;               // 'save': progress 0x416880
    virtual void progressUndo(const char* name) = 0;             // 'undo': progress 0x416590
    virtual void progressTexture(const char* name, const char* texture) = 0;  // progress 0x4166d0
    virtual void progressGiveItem(const char* name) = 0;         // progress 0x4169a0
    virtual void uiAddItem(const char* name, const char* arg2, bool flag) = 0;   // object 0x457334 0x41f2d0(name, arg2, flag, -1)
    virtual void uiAddItemRefresh() = 0;                         // 0x41f2b0
    virtual void uiUnlock() = 0;                                 // 0x41f060
    virtual void uiVideo() = 0;                                  // 0x41f160

    // ---- sound ---------------------------------------------------------------------------------
    virtual bool voiceBusy() = 0;                                // scene+0xd24 vt+0x18
    virtual bool voiceDone() = 0;                                // scene vt+0xdc
    // Looks the 'Wave' up in the scene sound bank (game+0x5c), then the shared one (game+0x68).
    // Returns 0 if neither has it; *shared is true for the second.
    virtual ScriptSound* soundOpen(const char* name, bool* shared) = 0;
    virtual void soundPlay(ScriptSound* s, int channel, int pan) = 0;   // mgr vt+0x1c(channel, s, 0, 0); pan 0/1/2 (centre/left/right) then 0x40c360(pan, channel); pan -1 (the 'goal' cheer): no pan call
    virtual void soundRelease(ScriptSound* s) = 0;               // handle vt dtor(1), not for shared ones
    virtual bool soundPlaying(bool idleRunner) = 0;              // 0x43d6e0 (mgr channel vt+0x14)
    virtual void soundStopChannel(bool idleRunner) = 0;          // dtor 0x43d5d0 (mgr channel vt+0xc, vt+0x10(0))

    // ---- mini games (object game+0x12c, 0x5c bytes, ctor 0x4354d0) ------------------------------
    virtual bool miniGameExists() = 0;
    virtual void miniGameCreate() = 0;
    virtual void miniSlotsInit() = 0;                            // 0x436030
    virtual void miniSlotsSpin() = 0;                            // 0x436090
    virtual void miniMaze(int n) = 0;                            // 0x436510
    virtual void miniPhoneKey(int n) = 0;                        // 0x436640 (1..11)
    virtual void miniPhoneReset() = 0;                           // 0x436610 ('phone 0')
    virtual void miniDarts(int op) = 0;                          // 0x436990 (0) / 0x4369c0 (1)
};

// A host that only prints what it is asked to do.  Used by sb_scripttest.  `script` states are plain
// sets so conditions and 'seen' queries work without a game.
class LoggingScriptHost : public ScriptHost {
public:
    LoggingScriptHost();
    virtual ~LoggingScriptHost();
    // Where the trace goes (default stdout); 0 = silent but still counted.
    void setOutput(void* file) { out_ = file; }
    void setRoot(const std::string& dir) { root_ = dir; }
    void setChapter(int c) { chapter_ = c; }
    void setSceneId(unsigned id) { sceneId_ = id; }
    unsigned calls() const { return calls_; }
    // Return values for the "block / wait" style calls.  true makes the script wait.
    bool walkBlocks, rotateBlocks, animBlocks, anim2dBlocks, goalReached, goalMissedValue;
    std::vector<std::string> flags;   // names for which conditions answer true

    virtual int  chapter();
    virtual bool readFile(const char* path, std::vector<unsigned char>& out);
    virtual unsigned sceneId();
    virtual bool condition(const char* expr);
    virtual void scriptStarted();
    virtual void mousePosition(short* x, short* y);
    virtual void scriptEnded();
    virtual void markScriptSeen(const char* scriptName);
    virtual bool scriptSeen(const char* scriptName);
    virtual ScriptActor* activeActor();
    virtual void setActiveActor(ScriptActor* a);
    virtual ScriptActor* findActor(const char* name);
    virtual void actorBeginScript(ScriptActor* a);
    virtual void actorSetControl(ScriptActor* a, bool on);
    virtual bool actorWalk(ScriptActor* a, float x, float y, int flag, int gridX, int gridY);
    virtual void actorPrepareWalkGrid(ScriptActor* a);
    virtual void actorSetRunning(ScriptActor* a);
    virtual void actorPosition(ScriptActor* a, float* x, float* y, float* z);
    virtual void actorReposition(ScriptActor* a, float x, float z);
    virtual void actorMove(ScriptActor* a, float x, float y);
    virtual void actorMoveY(ScriptActor* a, float y);
    virtual void actorMoveGrid(ScriptActor* a, int gx, int gy);
    virtual void actorClimb(ScriptActor* a, float h);
    virtual bool actorRotate(ScriptActor* a, int angle, int time);
    virtual void actorIdle(ScriptActor* a, float seconds);
    virtual void actorAttach(ScriptActor* a, const char* object, const char* bone1, const char* bone2);
    virtual void actorDetach(ScriptActor* a);
    virtual void actorHideAttach(ScriptActor* a);
    virtual void actorShowAttach(ScriptActor* a);
    virtual bool actorPlayAnim(ScriptActor* a, const char* name, bool wait, double speed);
    virtual void actorLoopAnim(ScriptActor* a, const char* name);
    virtual void actorTalkAnim(ScriptActor* a);
    virtual void actorOneShotAnim(ScriptActor* a, const char* name);
    virtual void actorKick(ScriptActor* a);
    virtual bool actorGoal(ScriptActor* a);
    virtual bool actorGoalMissed(ScriptActor* a);
    virtual void actorStopWalk(ScriptActor* a);
    virtual void actorShadow(ScriptActor* a, bool on);
    virtual void actorSetTexture(ScriptActor* a, const char* texture);
    virtual void swapPlayerHelmet();
    virtual void shockEffect();
    virtual void sceneHide(const char* name);
    virtual void sceneShow(const char* name);
    virtual bool sceneAnim2D(const char* name);
    virtual void sceneSetDoor(const char* name);
    virtual void sceneShake();
    virtual void sceneTransition(bool close);
    virtual void sceneLetterbox(const char* what);
    virtual void sceneSpeak(const char* key);
    virtual void sceneBlock(int x, int y, bool blocked);
    virtual void sceneSparkle(int x, int y);
    virtual void gameSwitch(const char* target);
    virtual void gameSkip();
    virtual void gameSetBlending(bool on);
    virtual void gameComment(const char* id);
    virtual void setChainedTarget(const char* name);
    virtual void progressSave(ScriptActor* a);
    virtual void progressUndo(const char* name);
    virtual void progressTexture(const char* name, const char* texture);
    virtual void progressGiveItem(const char* name);
    virtual void uiAddItem(const char* name, const char* arg2, bool flag);
    virtual void uiAddItemRefresh();
    virtual void uiUnlock();
    virtual void uiVideo();
    virtual bool voiceBusy();
    virtual bool voiceDone();
    virtual ScriptSound* soundOpen(const char* name, bool* shared);
    virtual void soundPlay(ScriptSound* s, int channel, int pan);
    virtual void soundRelease(ScriptSound* s);
    virtual bool soundPlaying(bool idleRunner);
    virtual void soundStopChannel(bool idleRunner);
    virtual bool miniGameExists();
    virtual void miniGameCreate();
    virtual void miniSlotsInit();
    virtual void miniSlotsSpin();
    virtual void miniMaze(int n);
    virtual void miniPhoneKey(int n);
    virtual void miniPhoneReset();
    virtual void miniDarts(int op);

private:
    void logf(const char* fmt, ...);
    void* out_;
    std::string root_;
    int chapter_;
    unsigned sceneId_;
    unsigned calls_;
    bool miniExists_;
    std::vector<std::string> seen_;
    std::vector<ScriptActor*> actors_;
    ScriptActor* active_;
};

}  // namespace sbscript
