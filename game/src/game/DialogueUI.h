/*  The dialogue manager of sb2.exe: class vtable 0x44dcfc, object at scene+0xd24.  Ctor/loader 0x4122f0 (the table is
    game/src/script/Dialogue.cpp), start 0x411920, filter 0x411aa0, line player 0x412d00, advance 0x413c40, confirm
    0x412bb0, end 0x411ba0, per-frame 0x412c00, choice list renderer 0x4128d0, hover 0x4129e0/0x412af0, keys 0x413e90.
    Text goes through OMediaTextFont (the game's font); the choice list stacks upwards from y = 450 (0x1c2). */
#pragma once
#include "../script/Dialogue.h"
#include "OMediaCCaption.h"
#include "OMediaBoxElement.h"
#include "OMediaSound.h"
#include <string>
#include <vector>

namespace sbgame {

class Game;

// Word-wrapped text as a stack of captions (the exe's text drawer, object 0x45732c, vt+4: x, y, z, highlighted,
// flag, text -> height).  port: wraps at maxWidth with one OMediaCCaption per row.
class TextBlock {
public:
    TextBlock() : box_(0) {}
    ~TextBlock() { clear(); }
    int show(Game* g, int x, int y, int z, bool highlighted, const std::string& text, int maxWidth);
    void clear();
    int bottom() const { return bottom_; }
private:
    std::vector<OMediaCCaption*> rows_;
    OMediaBoxElement* box_;
    int bottom_;
};

class DialogueUI {
public:
    explicit DialogueUI(Game* game);
    ~DialogueUI();

    // 0x4122f0 + the sound bank (game vt+0x64).  false: the scene has no dialogue file.
    bool load(unsigned sceneId);
    // vt+0x1c (0x411920): start the conversation `key` (3 letters, or 6 with a scene prefix).  true: something to say
    bool speak(const char* key);
    bool busy() const { return active_; }                 // vt+0x18 (0x411790), byte +0x19
    void update(float dtMs);                              // vt+0x54 (0x412c00)
    void render();                                        // vt+0x08 (0x4128d0)
    bool onMove(int x, int y);                            // vt+0x14 / 0x4129e0 hover; true: on a choice
    void onClick(int x, int y);                           // the click that ends a line or picks a choice
    void onKey(int key);                                  // 0x413e90: Up/Down/Enter/Space
    void stop();                                          // 0x411ba0 (end of conversation)
    bool choiceMode() const { return choice_; }           // byte +0x18
    const std::string& currentText() const { return curText_; }
    const std::string& currentSpeaker() const { return curSpeaker_; }
    // port: companion (docs/companion.md): the open choice list as text, and picking the n-th eligible choice
    std::vector<std::string> choiceTexts();
    bool pickChoice(int n);
    // test hooks
    int eligibleCount();
    sbscript::DialogueGroup* groupByKey(const std::string& key);
    std::vector<sbscript::DialogueGroup>& groups() { return groups_; }

private:
    bool eligible(const sbscript::DialogueGroup& g, bool flagZ);   // vt+0x40 (0x411aa0)
    static bool isAnim(const std::string& action);                 // 0x411b40
    void confirm();                                                // vt+0x28 (0x412bb0)
    void playLine();                                               // vt+0x50 (0x412d00)
    void advance();                                                // 0x413c40
    void selectNext(int step);                                     // 0x413e30 / 0x413e60
    void lineDone();                                               // voice finished / timer ran out
    void rebuild();                                                // redraw choices / current line
    void storeProgress(sbscript::DialogueGroup& g);
    bool startVoice(const std::string& key);
    void stopVoice();

    Game* game_;
    sbscript::DialogueTable table_;
    std::vector<sbscript::DialogueGroup> groups_;
    unsigned sceneId_;
    bool active_;            // +0x19
    bool choice_;            // +0x18 (the choice list is on screen)
    bool exiting_;           // +0x2d: the picked node is an exit ("X") node: the conversation ends after it
    bool zOpen_;             // +0x2c: an opening ("Z") node or exit node: spoken once, restarts
    char branch_;            // +0x1c
    int cur_;                // +0x0c selected group, -1 none
    OMediaSound* voice_;     // +0x08 (owned by the bank)
    float timerMs_;          // +0x30 text-only duration
    std::string curText_, curSpeaker_;
    std::vector<TextBlock*> blocks_;
    bool dirty_;
};

}  // namespace sbgame
