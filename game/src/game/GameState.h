/*  The progress object of sb2.exe (global 0x457330) as the game uses it: what the condition atoms and the
    dialogue tables read, and what the script commands write.  Method names follow the exe address they stand
    for.  The exe's own list layout (+8 scripts seen, +0x14 flags, +0x20 items, +0x38 dialogue progress) is only
    visible through these queries and through the save-game writer (0x416ed0, not reconstructed), so the lists are
    plain containers here.  See docs/scene-runtime.md.  */
#pragma once
#include "../script/ScriptCondition.h"
#include "../script/Dialogue.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace sbgame {

class SaveGame;     // game/SaveGame.cpp: the save-game writer / loader (0x416ed0 / 0x417460) reads and rebuilds the lists

class GameState : public sbscript::ConditionAtoms, public sbscript::DialogueProgress {
public:
    friend class SaveGame;
    GameState();

    // ---- ConditionAtoms (game vt+0xb8 / 0xbc, ScriptCondition.cpp) ----
    virtual bool debugAll() { return debug; }                       // game+0x24f
    virtual bool scriptSeen(const char* term);                      // 0x416690
    virtual bool hasItem(const char* term);                         // 0x416a20
    virtual bool flagSet(const char* term);                         // 0x416ad0
    virtual bool dialogueStarted(unsigned sceneId, const char* groupKey);   // 0x416e50, first short != 0
    virtual unsigned currentSceneId() { return sceneId; }           // scene+0x264
    virtual bool miniGameFlag() { return miniFlag; }

    // ---- DialogueProgress (0x416e50) ----
    virtual bool lookup(unsigned scene, const std::string& groupKey, std::string* currentKey, short* a, short* b);
    // 0x416d30: store the state of a group (current line key and the two shorts)
    void storeDialogue(unsigned scene, const std::string& groupKey, const std::string& currentKey, short a, short b);

    // ---- script / item / flag lists ----
    void markScriptSeen(const char* name);                          // 0x416610
    void undoScript(const char* name);                              // 0x416590 ('undo')
    void giveItem(const char* name);                                // 0x4169a0
    void removeItem(const char* name);
    const std::vector<std::string>& items() const { return items_; }
    void setFlag(const char* name, bool on);
    // 0x416ba0 'block' / 'unblock': (scene, x, y) cells the player may not walk on
    void setBlocked(unsigned scene, int x, int y, bool blocked);
    bool isBlocked(unsigned scene, int x, int y) const;
    // 0x4166d0 'texture': remembered per object name so the next scene load re-applies it
    void setTexture(const std::string& object, const std::string& texture) { textures_[object] = texture; }
    const std::map<std::string, std::string>& textures() const { return textures_; }

    // ---- save games (SaveGame.cpp), lane b ----
    void clearProgress();                                           // 0x4163b0 + 0x41e160: the lists emptied before a load
    // The lists the save file carries that nothing in the port sets yet (flags by scene id 0x416a60, CHAR 0x416c70, POSI 0x416880,
    // the item bar 0x421c40, the puzzle object); they are kept and written back unchanged.
    struct PosiRec { char name[32]; unsigned a, b, c, d; short e; };
    struct BarRec { char name[32]; char tex[32]; short slot; };
    bool hasPlayer;            // scene+0x248 exists: the controlled character (writer 0x416ed0)
    float playerX, playerY, playerZ; short playerFacing;   // character +0x310/+0x314/+0x318/+0x320; filled by the walking code
    bool resumePos;            // progress+0x64: a save restored a non-zero position; playerX.. hold it for the scene to place
    unsigned lastScene;        // game+0x15c ('LAST')

    bool debug;                // game+0x24f
    bool miniFlag;             // mini game object shorts
    unsigned sceneId;          // current scene id (scene+0x264)
    int chapter;               // game+0x154

private:
    static std::string low(const std::string& s);
    std::set<std::string> seen_, flags_;
    std::vector<std::string> items_;
    std::set<unsigned long long> blocked_;
    struct DEntry { std::string key; short a, b; };
    std::map<std::string, DEntry> dialogue_;
    std::map<std::string, std::string> textures_;
    std::set<unsigned> scen_;                       // progress+0x14 list: 24-bit flag ids (0x416ad0 decodes a 'C' term)
    std::vector<std::string> chars_;                // progress+0x2c list
    std::vector<PosiRec> posi_;                     // progress+0x50 list
    std::vector<BarRec> bar_;                       // item bar (0x457334) entries
    bool hasPuzzle_; unsigned char puzzle_[42]; short high_;   // game+0x12c puzzle object, +0x58 high score
    static unsigned flagId(const char* term);
};

}  // namespace sbgame
