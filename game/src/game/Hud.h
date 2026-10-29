/*  The HUD object of sb2.exe (global 0x457334): the inventory bar along the top of the screen, the items, the item in hand.
    Reconstructed from 0x4201c0 (pointer over the HUD: hover captions), 0x420d50 (mouse down), 0x420b20 (the item is
    let go), 0x420f20 (take the item off the pointer), 0x421390 (open / close the bar), 0x4208f0 / 0x420990 (the chest and
    map button states), 0x41f2d0 (an item is added: 'add' command), 0x421880 (two items combined), 0x420a30 / 0x420a70
    (slot under a point / item in a slot), 0x420ab0 (slots after a removal), 0x421090 / 0x420fc0 (HUD shown / hidden).
    The art is the game's own (sb2.omt canvases inv, inv2, chest1t, chest2t, map, mapt, help, helpt, menu, menut,
    bubble1, bubblet).  Each item is a small 3D picture of its model (models/items/<name>_model): every item has its own
    64x64 viewport element in the 2D world, like the exe's item object (+0x25c).  Port: docs/scene-runtime.md "HUD".  */
#pragma once
#include "OMediaCanvasElement.h"
#include <string>
#include <vector>

class OMedia3DWorld;
class OMediaViewPort;
class OMediaVPElement;
class OMedia3DShapeElement;
class OMedia3DGrannyModel;
class OMedia3DLight;
class OMedia3DAbstractRasterizer;

namespace sbgame {

class Game;
class TextBlock;
class GameScene;
struct SceneObject;

struct HudItem {
    HudItem();
    std::string name, desc;                 // "isp1", "Shovel and Pail" (the 'add' command's two arguments)
    int slot;                               // +0x35c
    OMedia3DGrannyModel* model;
    OMedia3DWorld* w3;
    OMediaViewPort* vp;
    OMediaVPElement* vpe;
    OMedia3DShapeElement* elem;
    OMedia3DLight* light;
    float cx, cy, cz, radius;               // the model's bounding sphere
    float focal;
    float x, y;                             // where its 64x64 box is on screen
    bool shown, tried;
    int yaw;                                // 0x4000 = 360 degrees
};

class Hud {
public:
    explicit Hud(Game* g);
    ~Hud();

    void update(float dtMs);                // every frame (after the scene update)
    // pointer input, in game (640x480) coordinates.  true: the HUD used the event.
    bool onMouseDown(int x, int y);
    bool onMouseUp(int x, int y);
    void onMove(int x, int y);
    bool onEscape();                        // port: Esc takes the item off the pointer

    // script 'add' (0x41f2d0): the item appears and flies into its slot
    void addItem(const char* name, const char* desc);
    // script 'sparkle x y' (0x41ebe0): sparkles around (x, y) for three seconds (0x41e3d0 / 0x41ec90 / 0x41ec20)
    void sparkle(int x, int y);

    bool visible() const { return shownNow_; }
    bool holding() const { return held_ != 0; }
    const std::string& heldName() const;
    bool holdFromPanel(const std::string& name);    // port: the companion panel puts an item in hand
    bool overHotspot(int x, int y);                 // a button of the bar, or something clickable in the scene
    // the hover caption (0x4201c0) that replaces the scene's; false: none
    bool caption(std::string* out);
    void hint();                                    // game vt+0x100: the hint button (text + voice)
    std::string hintText();
    std::string hintId();                           // the id of the help line that is read (an alias resolved), "" none                         // the current help line ("" none)
    // the inventory as it is now (the companion panel)
    struct Entry { std::string name, desc; };
    std::vector<Entry> items() const;
    int slotOf(const std::string& name);            // port (tests): the slot an item sits in, -1 none
    bool isOpen() const { return open_; }
    bool flying() const { return fly_ != 0; }
    // port: companion pictures.  A 96x96 PNG of the item's model, written to iconDir() once.  false: cannot (yet).
    std::string iconDir() const;
    bool ensureIcon(const std::string& name);

private:
    enum Spot { None = 0, Chest, Hint, Menu, Map, Slot };
    struct Btn { OMediaCanvasElement* e; bool shown; Btn() : e(0), shown(false) {} };
    Spot spotAt(int x, int y, int* slot);
    bool active();                          // shown and the player can use it (no cutscene, no dialogue, no pickup flight)
    bool wanted();                          // 0x421090 / 0x420fc0: the bar is part of this scene
    HudItem* itemInSlot(int slot);          // 0x420a70
    HudItem* find(const std::string& name);
    int freeSlot();
    void setOpen(bool open);                // 0x421390
    void release();                         // 0x420f20: the item goes back to its slot
    void dropAt(int x, int y);              // 0x420b20
    bool combine(HudItem* a, HudItem* b);   // 0x421880
    void remove(HudItem* it);
    void applyState();
    void show(Btn& b, bool on);
    void showItem(HudItem* it, bool on);
    bool ensure3D(HudItem* it);
    void placeItem(HudItem* it, float x, float y);
    void build();
    void relink();
    OMediaCanvasElement* mk(const char* name, float x, float y);
    bool renderIcon(HudItem* it, int size, std::vector<unsigned>* rgba);
    void sound(const char* name);

    Game* g_;
    Btn inv_, invT_, inv2_, inv2T_, map_, mapT_, help_, helpT_, menu_, menuT_;
    Btn bubble_[6], bubbleT_[6];
    std::vector<HudItem*> items_;
    HudItem* held_;                         // +0xb8
    bool dragging_;                         // the mouse button is down on the item
    int downX_, downY_;
    bool open_;                             // +0xd
    bool shownNow_;
    HudItem* fly_;                          // +0x130: the item that is flying to its slot
    float flyT_, flyMs_;
    float flyX_, flyY_;                     // +0x134 +0x136
    int mx_, my_;
    Spot hover_; int hoverSlot_;
    TextBlock* hintBlock_;
    float hintMs_;
    bool hintVoice_;                                // the hint plays its voice (channel 4): the text stays until it ends
    int iconFrame_;
    bool testItemsDone_, built_, testHoldDone_;
    GameScene* lastScene_;
    // the sparkle effect: +0x124 time left, +0x128/+0x12c centre, +0xf4 spawn timer, up to 10 canvases (+0x90) aged in +0xf8
    float sparkleMs_, sparkleX_, sparkleY_, sparkleAcc_;
    OMediaCanvasElement* sparkles_[10];
    float sparkleAge_[10];
    void updateSparkle(float dtMs);
    void spawnSparkle();
    void clearSparkles();
};

}  // namespace sbgame
