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
    bool shadowOn;                    // char +0x30c shown (`shadow on/off`)
    bool climbing; float climbTarget, climbPrev;   // char +0x3f8 / +0x3f4 / +0x3fc: `climb` arms a straight walk that blends y
    // ---- lane f: character idle scripts (0x42f1a0) and the shadow model (0x422070) ----
    bool idleWanted;                  // char +0x2c4 (set by the ctor, cleared when the idle runner is made)
    bool idleSuspended;               // char +0x330: the player deals with this character (0x435080 / 0x4350d0)
    SceneScriptHost* idleHost;        // port: one script host per idle runner (the exe's runner holds its own wait state)
    sbscript::ScriptRunner* idleRunner;   // char +0x32c
    bool wakeIdle;                    // an animation / walk the idle script waits on has finished
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
    bool walk(SceneObject* o, float x, float z, bool run);                      // vt+0x144
    bool walkGrid(SceneObject* o, int gx, int gy);
    void setShadow(SceneObject* o, bool on);                                    // 0x434410 / 0x434430
    void suspendIdle(SceneObject* o);                                           // 0x435080 (lane f)
    void climb(SceneObject* o, float h);                                        // vt+0x154 (0x4342a0)
    void moveGrid(SceneObject* o, int gx, int gy);
    void blockCell(int x, int z, bool blocked);
    bool clickFloor(int x, int y);
    bool approach(SceneObject* target);
    void setRunning(SceneObject* o);
    void stopWalk(SceneObject* o);                                              // vt+0x184
    void moveTo(SceneObject* o, float x, float z);                              // vt+0x148 (no animation)
    bool rotate(SceneObject* o, int angle, int tolerance);                             // vt+0x13c
    bool idle(SceneObject* o, float seconds);                                   // vt+0x168
    void setHidden(SceneObject* o, bool hide);
    void refreshVisibility();                                                   // scene vt+0x100, CHAR / ITEM part
    SceneObject* pick(int x, int y);                                            // scene vt+0xc4, the 3D passes
    bool screenBounds(SceneObject* o, int& x1, int& y1, int& x2, int& y2);      // where the character is drawn
    OMedia3DWorld* world3d() { return w3_; }

private:
    friend class SceneScriptHost;
    friend class Kick; // lane d: isolated beachball character reconstruction
    const sb::scene::Grid* grid(float y) const;
    bool cell(const sb::scene::Grid& g, int x, int z) const;
    bool gridWalk(Actor3D* a, float x, float z, int gx, int gz, bool run);
    void setAnim(Actor3D* a, const char* name, int loops, float speed, Actor3D::Mode m);
    void place(Actor3D* a);
    void updateIdle(Actor3D* a, float dtMs);                                    // lane f: the idle part of 0x42f1a0
    void resumeIdle(Actor3D* a);                                                // 0x4350d0
    void updateShadow(Actor3D* a);                                              // lane f: char +0x30c
    void finished(Actor3D* a);
    void finishInteraction(Actor3D* a);
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
