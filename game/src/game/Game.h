/*  The game object of sb2.exe (global 0x457310, vtable 0x44db24), 2D side: scene switching, the event
    classifier (vt+0xb0), the script runner of the current scene (scene+0x1c7c), sound banks, the hover
    caption and the mouse input of the scene.  The interpreter itself is game/src/script.  */
#pragma once
#include "GameState.h"
#include "../ui/GameFont.h"
#include "DialogueUI.h"
#include "Scene.h"
#include "../script/ScriptRunner.h"
#include "../script/Dialogue.h"
#include "../script/ScriptCondition.h"
#include "OMedia2DWorld.h"
#include "OMediaViewPort.h"
#include "OMediaCCaption.h"
#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaSound.h"
#include "OMediaListener.h"
#include <map>
#include <string>
#include <vector>

namespace sbscript { class ScriptHost; }

namespace sbgame {

class MiniGame;
class SceneScriptHost;
class World3D;
class Hud;      // port: hud
class Cursor;   // port: cursor

// A file-backed OMT database kept open (the exe keeps them for the whole scene).
struct OmtFile {
    OmtFile() : db(0), stream(0), path(0) {}
    ~OmtFile();
    bool open(const std::string& file);
    void reset();
    OMediaDataBase* db;
    OMediaFileStream* stream;
    OMediaFilePath* path;
};

class Game : public OMediaListener {
public:
    Game();
    virtual ~Game();

    // `root` is the game folder (sb2.exe, scenes/, scripts/, ...).  Creates the display world / viewport on
    // `canvas` (the 640x480 back buffer).  Returns false if sb2.omt or Font.omt cannot be opened.
    bool init(const std::string& root, OMediaCanvas* canvas);
    // Listen to the event manager's mouse and key broadcasters.
    void attachInput();

    // ---- game vt+0x94 (0x40ab60) / 0x40ac20 / DoSwitch 0x40b720 ----
    void requestScene(const char* name);            // "c1l2s6p1": scene id packed, start point after 'p'
    void exitScene(int dir);                        // 0x40ac20: the camera record's exit string
    // ---- game vt+0xb0 (0x40a230) the event classifier ----
    void switchTo(const char* name);
    // ---- per frame: DoSwitch, the scene update, script wake-ups, the world ----
    void update(float dtMs);

    // ---- accessors ----
    MiniGame* miniGame() { return mini_; } // chapter 2 mini-games
    void createMiniGame();
    GameState& state() { return state_; }
    // game vt+0xb8 (0x40a6b0): is the condition expression true ("", "0" and " " are true)
    bool cond(const char* expr, bool touching = false) { return sbscript::evalCondition(expr, touching, state_); }
    // wake the scene's script after a wait (scene vt+0xe0 = 0x43aa80: do step() while no wait; delete when finished)
    void wake();
    void forceRedraw();                             // port (saves): the menu drew over the canvas; the next frame repaints all
    GameScene* scene() { return scene_; }
    World3D* world3d() { return w3_; }
    Hud* hud() { return hud_; }                    // port: hud
    Cursor* cursor() { return cursor_; }           // port: cursor
    bool pendingScene() const { return pendingId_ != 0; }   // port: cursor (a scene switch is waiting: the wait cursor)
    OMedia2DWorld* world() { return world_; }
    OMediaViewPort* viewport() { return vp_; }
    OMediaDataBase* sb2Db() { return sb2_.db; }
    OMediaCanvas* skyCanvas(int index);                    // 0x40b590 (vt+0xcc): canvas "sky0%d" of the chapter's skies database
    OMediaDataBase* sceneSoundDb() { return sceneBank_.db; }     // game+0x5c
    OMediaDataBase* sharedSoundDb() { return sharedBank_.db; }   // game+0x68
    OMediaDataBase* commentDb() { return commentBank_.db; }
    OMediaTextFont* font() { return font_; }
    sbscript::ScriptRunner* runner() { return runner_; }          // scene+0x1c7c
    SceneScriptHost* host() { return host_; }
    const std::string& root() const { return root_; }
    sbscript::CommentTable& comments() { return comments_; }
    sbscript::HelpTable& help() { return help_; }
    int mouseX() const { return mx_; }
    int mouseY() const { return my_; }
    std::string chained;                            // game+0x186: the target to go to after a movie / script
    bool blending;                                  // game+0x251 (set by the `blending on/off` script command, 1 at start): animations ease in / out over 0.4 s (0x4046d0)
    int optText, optShadows;                        // port: the Options Text / Shadows values (exe +0x270 / +0x274 of the app object); set by the app
    void previewSound(bool voice);                  // 0x4256b0: the Options sample of the Sound / Voice volume
    bool suspended;                                 // port (saves): the game sits behind the menu; mouse and key messages are ignored
    bool inputConsumed;                             // game+0x24e
    bool letterboxIn;                               // game+0x25c
    // game vt+0xac (0x40de30): the interaction left for when the player has walked / turned there: +0x228 object, +0x24c
    // (true: its event, false: use the item +0x22c on it)
    SceneObject* pendingObj; bool pendingEvent; std::string pendingItem;
    void setPending(SceneObject* o, bool event, const char* item) { pendingObj = o; pendingEvent = event; pendingItem = item ? item : ""; }
    float walkInMs;                                 // game+0x258: after a scene switch the walk-in triggers wait (500 ms)
    bool hudHolding();                              // HUD +0x130: an item is on the pointer
    void hudAddFromScene(SceneObject* item);        // 0x41f4d0: a picked-up item flies from its screen point into the bar
    // Show a comment / hover caption line (game+0xc4 vt+0x14 draws a comment: bottom-line caption)
    void showComment(const char* id);
    bool commentActive() const { return commentMs_ > 0; }
    // Sound helpers (OMT sound manager, 15 channels like the exe)
    OMediaSound* findSound(const char* name, bool* shared);
    OMediaSound* soundCI(OMediaDataBase* db, const std::string& name);
    OMediaSound* chapterSound(const char* name) { return soundCI(sharedBank_.db, name); }   // sounds\c<chapter>.omt only (game+0x5c)
    // 0x419210: the hint voice, "%ssounds\help.omt" (the CD) else "sounds\help.omt"; opened on first use (a 34 MB bank)
    OMediaSound* helpVoice(const std::string& id);
    // sound manager channels: 4 = dialogue voice, 5 = script 'sound', 9 = idle scripts (as the exe)
    bool playOnChannel(int ch, OMediaSound* s, int pan);
    bool channelBusy(int ch);
    void stopChannel(int ch);
    // Mouse position for tests / synthetic events
    void mouseDown(int x, int y);
    void mouseMove(int x, int y);
    // port: the movie player (Bink) is the startup's; a hook receives the movie name
    void (*movieHook)(const char* name, void* data);
    void* movieHookData;
    virtual void listen_to_message(omt_Message msg, void* param = NULL);

    // movies the classifier would start (the Bink player is lane C's / the startup's): recorded for the host
    std::vector<std::string> movieLog;
    void log(const char* fmt, ...);
    FILE* logFile;

private:
    friend class GameScene;
    friend class SceneScriptHost;
    void doSwitch();
    void loadSceneBanks(unsigned sceneId);
    void loadSkies(int chapter);
    void runScript(const char* name);
    void updateComment(float dt);

    std::string root_;
    GameState state_;
    OMedia2DWorld* world_;
    OMediaViewPort* vp_;
    OmtFile skies_;                         // game+0x64: sceneart/c%dskies.omt
    OMediaCanvas* skyCache_[10];            // game+0x70 + index*4
    OmtFile sb2_, fontDb_, sceneBank_, sharedBank_, commentBank_, itemsBank_, helpBank_, globalCommentBank_;
    RetailData* retail_;
    GameFont* font_;
    World3D* w3_;
    GameScene* scene_;
    GameScene* pending_;
    unsigned pendingId_;          // game+0x158: scene requested (nonzero: a switch is pending)
    int pendingStart_;            // game+0x160
    sbscript::ScriptRunner* runner_;
    SceneScriptHost* host_;
    sbscript::CommentTable comments_;
    sbscript::HelpTable help_;
    OMediaCCaption* hoverCap_;
    OMediaCCaption* commentCap_;
    float commentMs_;
    int mx_, my_;
    bool inputAttached_;
    TextBlock* commentBlock_;
    bool commentVoice_;
    std::string hoverText_;
    Hud* hud_;                    // port: hud
    MiniGame* mini_; // persistent across scenes (game+0x12c)
    Cursor* cursor_;              // port: cursor
};

}  // namespace sbgame
