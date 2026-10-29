/*  The scene object of sb2.exe (class vtable 0x44e5a0, ctor/dtor 0x436f30, loader 0x437210), 2D side:
    background, 2D objects (2DOB), 2D animations (ANIM), hot spots (SPOT), the scene-level data of the camera
    record (exits, enter event), the 3D objects (ITEM / CHAR / TRIG) as logical records, click picking and the
    letterbox / transition bars.  The 3D world and the Granny characters are lane A's; every place that would
    call them says so (// 3D:).  See docs/scene-runtime.md for the object field map.  */
#pragma once
#include "../scene/SceneData.h"
#include "OMediaCanvasElement.h"
#include "OMediaBoxElement.h"
#include "OMedia2DWorld.h"
#include "OMediaDataBase.h"
#include "OMediaCanvas.h"
#include <string>
#include <vector>

namespace sbgame {

class Game;

// The base object shared by every scene object (vtable 0x44dd90; fields at +0x14.. of the serialisable part).
// Property names are the ones the exe's own property sheets print (0x401d50 2DOB, 0x4078a0 ANIM, 0x41a4c0 SPOT,
// 0x4230a0 ITEM, 0x433120 CHAR, 0x441820 TRIG).
struct SceneObject {
    enum Kind { Obj2D, Anim2D, Spot, Item, Char, Trigger };
    SceneObject();
    Kind kind;
    unsigned tag;                       // +0x164 the factory tag ('2DOB', 'ANIM', 'SPOT', 'ITEM', 'CHAR', 'TRIG')
    std::string art;                    // OMT Name / Granny Name
    std::string event;                  // +0x20  Event
    std::string condition;              // +0x40  Condition
    std::string altEvent;               // +0x60  Alt Event / Alternate Event
    std::string itemAccept;             // +0x80  Accept this Item
    std::string itemEvent;              // +0xa0  Item Event
    std::string wrongItemEvent;         // +0xc0  Wrong Item Event
    std::string haveItemEvent;          // +0xe0  Have Item Event
    std::string visibleCond;            // +0x100 Visible Condition
    std::string description;            // +0x120 Description (hover text)
    std::string soundName;              // +0x140 Sound Name
    short x1, y1, x2, y2;               // +0x14 +0x16 +0x18 +0x1a: click rectangle (empty = never hit)
    short gridX, gridY;                 // +0x16c +0x16e: "walk here first" grid cell, 0,0 = none
    bool used;                          // +0x163: the event / item event ran once
    float px, py, pz;                   // Screen x, y, z-Depth (2D) or x y z (3D)
    bool postRender;                    // Post Render: drawn on top (z = 10000)
    bool rolloverCond;                  // SPOT: Rollover uses Cond.
    bool clickable;                     // CHAR / ITEM: Clickable
    bool userCtl;                       // CHAR: User Controlled (+0x27e)
    bool is3D() const { return kind == Item || kind == Char || kind == Trigger; }   // +0x161
    OMediaCanvasElement* elem;          // the 2D element (2DOB, ANIM); null otherwise
    // ---- ANIM (properties of 0x4078a0; runtime state at 0x2a4.. of the exe object) ----
    std::vector<OMediaCanvas*> frames;  // "%s%02d" canvases, 1..nFrames
    short nFrames, fps, clickToPlay, interval, continuous, framePerClick, loops, reverse, soundFrame, stayOnLast,
          allowToggle, startOn, eventAtBegin;
    bool playing;                       // +0x2c0
    int frame, dir, loopsDone;          // current frame, direction (reverse), +0x2bc loops played
    float frameMs, intervalSec;         // frame timer (ms), interval timer (s)
    bool paused;                        // +0x2c1 (toggled by 0x408b70)
    bool eventPending;                  // click-to-play: the event waits for the run to end
};

class DialogueUI;

class GameScene {
public:
    explicit GameScene(Game* game);
    ~GameScene();

    // 0x437210: load sceneart/<scene>.omt and scenes/<scene>.scn.  Returns false (the exe: scene not made).
    bool load(unsigned sceneId);
    // 0x437cf0 (the 2D part): start point, scene enter event, letterbox, "loading" hide.
    void enter(int startPoint);

    unsigned sceneId() const { return sceneId_; }
    const std::string& name() const { return name_; }
    const sb::scene::Scene& data() const { return data_; }
    std::vector<SceneObject*>& objects() { return objects_; }
    SceneObject* find(const char* name, int kindMask = 0);            // scene vt+0xe4 (characters) and by-name lookups
    const std::string& exit(int dir) const { return exits_[dir & 3]; }   // camera +0x3fa .. +0x45a (0x40ac20)
    const std::string& enterEvent() const { return enterEvent_; }        // camera +0x4ac, condition +0x4cc
    OMedia2DWorld* world();                                            // the game's display world

    // scene vt+0xf8 / +0xfc (0x438fa0 / 0x439110): show / hide every object called `name`
    void show(const char* name);
    void hide(const char* name);
    // scene vt+0x100 (0x438ad0): re-evaluate every object's Visible Condition
    void refreshVisibility();
    // scene vt+0xf4 object then its vt+0x9c: start an ANIM (true while it runs)
    bool animate(const char* name);
    // scene vt+0xc4 (0x43a310): what is under x,y (2D objects; the 3D passes are lane A's)
    SceneObject* pick(int x, int y);
    // base vt+0x40 (0x415a70): choose Event / Alt Event by Condition and send it to game vt+0xb0.  `fromClick`
    // is the exe's second argument (it lets a "C?L?S" alt event mark the object used).  Returns true if an event ran.
    bool pickEvent(SceneObject* o, bool fromClick);
    // base vt+0x48 (0x415c30): use item `item` on o.  true: the item was accepted (and consumed by the UI)
    bool useItem(SceneObject* o, const char* item);
    // scene vt+0xbc (0x4398f0): a click at x,y that was not on the HUD
    void click(int x, int y);
    // game cursor object vt+0x30: hover description under x,y
    SceneObject* hover(int x, int y);
    // per-frame update (scene vt+0x118 0x43ae70 timers, ANIM ticks 0x408cb0, bars 0x43b760)
    void update(float dtMs);

    // ---- letterbox / transition bars (0x43bd90, 0x43bf30/40/50, 0x43c100, state machine 0x43b760) ----
    void letterboxInit();
    void letterboxOpen()  { barState_ = 1; }        // 0x43bf30  state 1
    void letterboxClose() { barState_ = 2; }        // 0x43bf40  state 2
    void transitionOut();                           // 0x43bf50  bars off screen, then closes (state 3)
    void transitionOpen();                          // 0x43c100  bars cover the screen, then open (state 4)
    void transitionClose();
    int  barState() const { return barState_; }     // +0x1c98 (0 = idle)
    float shakeMs() const { return shake_; }        // camera shake timer
    float lockMs() const { return lock_; }          // +0x1d4 (0x735): clicks are ignored for 500 ms after entering
    void shake();                                   // scene vt+0x15c
    void setDoor(const char* name);                 // scene+0x1c80
    const std::string& door() const { return door_; }

    SceneObject* lastClicked() { return lastClicked_; }   // +0x1cd8 / +0x1cb0
    DialogueUI* dialogue() { return dialogue_; }
    OMediaCanvas* canvasNamed(const std::string& n);       // scene art database, then sb2.omt (0x407f90 order)

private:
    friend class Game;
    void buildObjects();
    void place(SceneObject* o);
    void setHidden(SceneObject* o, bool hidden);
    void tickAnim(SceneObject* o, float dtMs);
    void startAnim(SceneObject* o);
    void ensureBars();
    void moveBars(float topY, float botY, float z);

    Game* game_;
    unsigned sceneId_;
    std::string name_;
    sb::scene::Scene data_;
    OMediaDataBase* artDb_;
    void* artFile_;
    OMediaCanvasElement* background_;
    OMediaCanvasElement* sky_[2];                      // scene+0x1c8c / +0x1c90: the scrolling sky tiles behind the background
    float skyScroll_, skyX_[2];                                  // scene+0x1c94
    void buildSky();
    void updateSky(float dtMs);
    std::vector<SceneObject*> objects_;
    std::string exits_[4];
    std::string enterEvent_, enterCond_;
    DialogueUI* dialogue_;
    OMediaBoxElement* barTop_;
    OMediaBoxElement* barBot_;
    int barState_;
    float barHold_;
    float topY_, botY_;
    float shake_;
    float lock_;
    std::string door_;
    SceneObject* lastClicked_;
    std::string pendingEvent_;       // scene+0x1cb4
};

}  // namespace sbgame
