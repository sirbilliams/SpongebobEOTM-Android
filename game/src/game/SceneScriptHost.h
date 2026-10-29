/*  The ScriptHost of the scene runtime: executes the script interpreter's world calls (game/src/script/ScriptHost.h)
    on the 2D side (captions, dialogue, scene switch, 2D objects, sounds, waits) and logs the 3D / character
    calls with the lane A engine call they will become.  Wake-ups after a wait go through Game::wake().  */
#pragma once
#include "../script/ScriptHost.h"
#include "Game.h"
#include <map>
#include <string>
#include <vector>

namespace sbscript {
// the opaque types of ScriptHost.h
struct ScriptActor { sbgame::SceneObject* obj; std::string name; };
struct ScriptSound { OMediaSound* snd; };
}

namespace sbgame {

using sbscript::ScriptActor;
using sbscript::ScriptSound;

class SceneScriptHost : public sbscript::ScriptHost {
public:
    explicit SceneScriptHost(Game* g);
    virtual ~SceneScriptHost();
    // per frame: the 3D stubs' simulated durations, timers the exe keeps on characters
    void update(float dtMs);
    // the 3D / character calls have no engine yet: a script that waits on one is woken after this long
    float sim3dMs;                       // default 700
    bool waitingOn3d() const { return wait3dMs_ > 0; }
    bool waitingAnim2d() const { return waitAnim_; }
    void anim2dFinished();               // an ANIM the script waits on has run to its end
    void talkFinished();                 // dialogue ended: wakes a script waiting in talk / narrate
    unsigned stubCalls() const { return stubCalls_; }
    // Game hooks
    void resetForScene();                // the old scene is gone: forget its actors
    void sceneLoaded();                  // pick the active actor (scene+0x248)
    void commentTalk();                  // 'H...' event: the active actor plays "talk"
    void dialogueStarted();              // 0x4397c0: the actor loses control
    void scriptRunnerCreated() {}
    void controlLostToScript(const char* name);
    sbscript::ScriptActor* actorFor(SceneObject* o);
    // base vt+0x50 (0x415710): play the object's Sound Name on the pan side of its screen x
    void playObjectSound(SceneObject* o);
    // dialogue 0x412d00: the speaker's "talk" animation (3D: character vt+0xc0)
    void speakerStarted(SceneObject* actor, const char* name);

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
    void stub(const char* fmt, ...);
    void arm3d(float ms) { wait3dMs_ = ms; }
    Game* g_;
    float wait3dMs_;
    bool waitAnim_;
    unsigned stubCalls_;
    std::map<SceneObject*, sbscript::ScriptActor*> actors_;
    sbscript::ScriptActor* active_;
    std::vector<sbscript::ScriptSound*> sounds_;
    float uiMs_;
};

}  // namespace sbgame
