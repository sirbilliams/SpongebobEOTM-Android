/*  The 3D side of a scene (sb2.exe scene init 0x4033f0 and the character class, vtable 0x44d83c), on the OMT 1.x 3D
    path (engine/omt1/3D): an OMedia3DWorld with the scene's camera and lights shown through an OMediaVPElement in the
    2D world, one OMedia3DGrannyModel + OMedia3DShapeElement per CHAR / ITEM record of the scene.  The script host
    (SceneScriptHost) turns the character commands into calls here; GameScene asks it to pick a character under the
    mouse.  Simplified against the exe, see docs/scene-runtime.md ("3D side").                                  */
#pragma once
#include "Scene.h"
#include "OMedia3DWorld.h"
#include "OMedia3DGrannyModel.h"
#include "OMediaVPElement.h"
#include "../script/ScriptRunner.h"
#include <string>
#include <vector>

namespace sbgame {

class Game;
class SceneScriptHost;

struct Actor3D {
    Actor3D();
    SceneObject* obj;
    std::string name;                 // lower case model name ("sb")
    OMedia3DGrannyModel* model;
    OMedia3DShapeElement* elem;
    std::string anim;                 // the animation that plays
    enum Mode { Idle, Loop, OneShot, Talk };
    Mode mode;
    float x, y, z;                    // world position
    float prevX, prevY, prevZ;        // 0x404..: the position of the last place (the click rectangle follows the screen delta)
    int heading;                      // 0x4000 = 360 degrees
    bool kicking; // lane d: 0x410 kick movement, handled by Kick.cpp
    bool walking; float tx, tz, speed;
    std::vector< std::pair<float,float> > route;
    size_t waypoint;
    SceneObject* interaction;
    float radius;
    bool control, useGrid;
    bool rotating; int rotTarget; float rotTolerance;
    bool idling; float idleLeft;
    bool waited;                      // the script waits for this actor's walk / rotate / animation
    bool scheduled;                   // the idle animation switch is due next frame
    float arrive;                     // +0x3f0: the walk ends this close to the destination
    std::vector<SceneObject*> touches;// +0x368: what the character touches (0x4313e0 / 0x431610)
    SceneObject* pickupItem;          // +0x398: the item being picked up (0x433910)
    bool doorWalk;                    // +0x356: walking straight ahead (through a door, into a scene)
    float dirX, dirZ;                 // port: the straight-ahead direction of doorWalk
    // the attached object (+0xad8, 0x433780): an item model hanging from a bone of this character
    OMedia3DGrannyModel* attachModel; OMedia3DShapeElement* attachElem;
    std::string attachName, attachBone, attachParentBone, attachCond;   // +0xa56, +0xa76, +0xa96, +0xab6 (CHAR record +0x976..)
    bool shadowOn;                    // char +0x30c shown (`shadow on/off`)
    bool climbing; float climbTarget, climbPrev;   // char +0x3f8 / +0x3f4 / +0x3fc: `climb` arms a straight walk that blends y
    // ---- lane f: character idle scripts (0x42f1a0) and the shadow model (0x422070) ----
    bool idleWanted;                  // char +0x2c4 (set by the ctor, cleared when the idle runner is made)
    bool idleSuspended;               // char +0x330: the player deals with this character (0x435080 / 0x4350d0)
    SceneScriptHost* idleHost;        // port: one script host per idle runner (the exe's runner holds its own wait state)
    sbscript::ScriptRunner* idleRunner;   // char +0x32c
    bool wakeIdle;                    // an animation / walk the idle script waits on has finished
    bool animSnd;                     // char +0xfc: the per-animation sound is playing (0x405040 / 0x404fe0)
    float fidgetAcc, fidgetNext;      // char +0x38c / +0x390: random idle2 / idle3 timer (seconds)
    std::string shadowName;           // CHAR record "Shadow Name" (+0x204)
    OMedia3DGrannyModel* shadowModel; // char +0x22c of the 0x354 byte object: models/items/<name>_model
    OMedia3DShapeElement* shadowElem;
};

class World3D {
public:
    // Builds the world for the scene's camera and lights and loads every CHAR / ITEM record's model.
    World3D(Game* game, GameScene* scene);
    ~World3D();
    bool ok() const { return w3_ != 0; }

    void update(float dtMs);                     // Granny clock, walks / rotations, poses, the 3D world render
    void enter(int startPoint);                  // 0x437cf0: the player goes to the scene's start point

    Actor3D* actorFor(SceneObject* o);
    bool exists(Actor3D* a, const char* name);
    bool hasModel(SceneObject* o) { return actorFor(o) != 0; }
    // character commands (the char object's vtable slots named in ScriptHost.h); true = the script waits (wakeFinished())
    bool playAnim(SceneObject* o, const char* name, bool wait, double speed);   // vt+0xb8
    void loopAnim(SceneObject* o, const char* name);                            // vt+0xb4 (name, FLT_MAX)
    void oneShotAnim(SceneObject* o, const char* name);                         // vt+0xb4 (name, 0, 1.0)
    void talk(SceneObject* o, bool on);                                         // vt+0xc0("talk")
    void talkAll(bool on);
    void setTexture(SceneObject* o, const char* name); // lane c: vt+0xf0
    bool walk(SceneObject* o, float x, float z, bool run);                      // vt+0x144
    bool walkGrid(SceneObject* o, int gx, int gy);
    void setShadow(SceneObject* o, bool on);                                    // 0x434410 / 0x434430
    void suspendIdle(SceneObject* o);                                           // 0x435080 (lane f)
    void climb(SceneObject* o, float h);                                        // vt+0x154 (0x4342a0)
    void moveGrid(SceneObject* o, int gx, int gy);
    void blockCell(int x, int z, bool blocked);
    bool clickFloor(int x, int y, SceneObject* target = 0);      // 0x4398f0 tail: walk to the floor point under x,y
    bool turnToward(SceneObject* o, Actor3D* to);                // o's vt+0x13c toward `to` (500)
    void setRunning(SceneObject* o);
    void stopWalk(SceneObject* o);                                              // vt+0x184
    void moveTo(SceneObject* o, float x, float z);                              // vt+0x148 (no animation)
    bool rotate(SceneObject* o, int angle, int tolerance);                             // vt+0x13c
    bool idle(SceneObject* o, float seconds);                                   // vt+0x168
    void setHidden(SceneObject* o, bool hide);
    // character vt+0x12c / +0x130 and the attached object's vt+0x30 (script attach / detach / hideattach / showattach)
    bool attach(SceneObject* o, const char* name, const char* bone, const char* parentBone);
    void detach(SceneObject* o);
    void hideAttach(SceneObject* o, bool hide);
    void refreshVisibility();                                                   // scene vt+0x100, CHAR / ITEM part
    SceneObject* pick(int x, int y);                                            // scene vt+0xc4, the 3D passes
    bool screenBounds(SceneObject* o, int& x1, int& y1, int& x2, int& y2);      // where the character is drawn
    // lane e (chapter 3): char vt+0xec (project a world point to the 640x480 screen) and the 'swapchar' body 0x43fced
    bool project(float x, float y, float z, int& sx, int& sy);
    Actor3D* controlled();                                                      // scene+0x248: the player character
    bool swapControl(Actor3D* from, Actor3D* to);
    OMedia3DWorld* world3d() { return w3_; }

    // ---- the player (scene+0x248) as the object code uses it (character vtable 0x44e358) ----
    Actor3D* player();
    bool walkTo(SceneObject* target, float x, float z);      // vt+0x144 (x, z, target, -1, -1): true = walking there
    bool rotateToward(SceneObject* target);                  // vt+0x13c (heading toward target, 500): true = turning
    bool rotatePlayer(int heading);                          // vt+0x13c (heading, 500)
    bool touching(SceneObject* o);                           // vt+0x128 (0x4318c0)
    void startPickup(SceneObject* item);
    void takeItem(SceneObject* it, Actor3D* a);
    void playerDoor();                                       // vt+0xb4 ("door", 0, 1.0)
    void walkIn();                                           // DoSwitch tail (0x40b720): the player walks into the scene
    bool doorBusy();                                         // DoSwitch head: the door / walk animation or a turn is running
    bool playerBusy();                                       // port (tests): walking, turning or a one-shot animation
    void clearTarget();                                      // vt+0x180 (0x4343d0)
    bool samePlane(SceneObject* o);                          // player y == o y

private:
    friend class SceneScriptHost;
    friend class Kick; // lane d: isolated beachball character reconstruction
    const sb::scene::Grid* grid(float y) const;
    bool cell(const sb::scene::Grid& g, int x, int z) const;
    bool gridWalk(Actor3D* a, float x, float z, int gx, int gz, bool run, bool neighbour = false);
    void setAnim(Actor3D* a, const char* name, int loops, float speed, Actor3D::Mode m);
    void place(Actor3D* a);
    void updateIdle(Actor3D* a, float dtMs);                                    // lane f: the idle part of 0x42f1a0
    void resumeIdle(Actor3D* a);                                                // 0x4350d0
    void animSound(Actor3D* a, const char* anim);                               // 0x405040 (char vt+0xf8): sound "<char><anim>" of the chapter bank on channel 0
    void stopAnimSound(Actor3D* a);                                             // 0x404fe0 (char vt+0xfc)
    void updateShadow(Actor3D* a);                                              // lane f: char +0x30c
    void finished(Actor3D* a);
    void finishInteraction(Actor3D* a);
    void animEnded(Actor3D* a);                              // 0x405620 tail: what follows a one-shot animation
    void updateTouches(Actor3D* a);                          // vt+0x11c (0x4313e0)
    void addTouch(Actor3D* a, SceneObject* o);               // vt+0x120 (0x431610)
    void runPending();                                       // port: the interaction left by game vt+0xac
    std::string animPath(Actor3D* a, const char* name);
    static OMedia3DAbstractRasterizer* rasterizer();

    Game* game_;
    GameScene* scene_;
    std::string dir_;
    OMedia3DWorld* w3_;
    OMediaViewPort* vp_;
    OMediaVPElement* vpe_;
    std::vector<OMedia3DLight*> lights_;
    std::vector<Actor3D*> actors_;
    sb::scene::Grid grids_[2];
    bool wake_;
};

}  // namespace sbgame
