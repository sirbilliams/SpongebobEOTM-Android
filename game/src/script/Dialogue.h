/*  Dialogue tables (the dialogue folder of the install): the three readers of sb2.exe.
      scene dialogue  dialogue\c%dl%ds%d.sb2   0x4122f0   groups of lines, with saved progress
      comments        dialogue\comments.sb2    0x4106d0   lookup 0x410a60
      help            dialogue\help.sb2        0x419210
    All use the .sb2 container (Sb2File.h).  See docs/script.md.  */
#pragma once
#include "Sb2File.h"
#include <string>
#include <vector>

namespace sbscript {

// scene ids are packed as scene<<16 | level<<8 | chapter (nibbles), e.g. c1l2s2 = 0x20201
inline unsigned sceneIdPack(int chapter, int level, int scene) { return (unsigned)(((scene & 0xf) << 16) | ((level & 0xf) << 8) | (chapter & 0xf)); }

struct DialogueLine {            // 0x160-byte record
    std::string key;             // +0x00  branch + node + line letters, e.g. "BAA"
    short flag1, flag2, flag3;   // +0x10, +0x12, +0x14 (atoi of the three numeric columns)
    std::string action;          // +0x16  "0" or a script command line ("anim sb emoexplain", "unlock")
    std::string speaker;         // +0x36
    std::string text;            // +0x56  (256 bytes, ends before the height at +0x156)
    short textHeight;            // +0x156 = 0x12 until the UI measures it
    std::string listener;        // port: column 6, parsed and dropped by the exe; kept for tools
};

struct DialogueGroup {           // 0x24-byte node: all lines sharing branch+node
    std::string key;             // +0x00 key of its first line
    short stateA, stateB;        // +0x10, +0x12 (saved progress; stateB != 0: finished, no current line)
    int current;                 // +0x14 index of the current line, -1 = none
    std::vector<DialogueLine> lines;   // +0x18 first line, next at +0x158
};

// Saved dialogue progress, progress object (0x457330) 0x416e50.  Returns false if the scene has no
// entry for this group key.
class DialogueProgress {
public:
    virtual ~DialogueProgress() {}
    virtual bool lookup(unsigned sceneId, const std::string& groupKey, std::string* currentKey, short* a, short* b) = 0;
};

class DialogueTable {
public:
    DialogueTable() : sceneId_(0) {}
    static std::string fileName(unsigned sceneId);        // "dialogue/c%dl%ds%d.sb2"
    static std::string soundBankName(unsigned sceneId);   // "sounds/c%dl%ds%d.omt" (the Wave bank with the voices)
    // "%d%d%d%c%c%c": chapter, level, scene, then the three key letters upper-cased, e.g. "122AAH".
    static std::string voiceName(unsigned sceneId, const std::string& key);

    // 0x4122f0.  `fs` supplies the file; progress may be 0.  Returns false if the file is missing
    // (the exe then calls its virtual +0x10) or cannot be decoded.
    bool load(FileSource& fs, unsigned sceneId, DialogueProgress* progress, std::string* err);

    unsigned sceneId() const { return sceneId_; }
    const std::vector<DialogueGroup>& groups() const { return groups_; }
    // First line whose key equals `key` exactly (strcmp), or 0.
    const DialogueLine* find(const std::string& key) const;
    unsigned lineCount() const;

private:
    unsigned sceneId_;
    std::vector<DialogueGroup> groups_;
};

struct CommentEntry {            // 0x100-byte node payload
    std::string id;              // +0x00  "H14xA": H, chapter, level, ...
    std::string text;            // +0x10
};

class CommentTable {
public:
    // 0x4106d0: keeps entries whose id[1] == chapter and id[2] == level (as digits).
    bool load(FileSource& fs, int chapter, int level, std::string* err);
    static std::string soundBankName(int chapter);        // "sounds/comments%d.omt"
    // "%c%c%c%c%c": id[0] and id[4] upper-cased, id[1..3] as they are.
    static std::string voiceName(const std::string& id);
    const std::vector<CommentEntry>& entries() const { return entries_; }
    const CommentEntry* find(const std::string& id) const;   // strcmp
    // 0x410450: the leading rows whose id is "H000.." (the comments of every chapter, voice bank sounds/comments.omt);
    // 0x410a60 looks an id with id[1] == '0' up there
    bool loadGlobal(FileSource& fs, std::string* err);
    const std::vector<CommentEntry>& globals() const { return globals_; }
private:
    std::vector<CommentEntry> entries_, globals_;
};

struct HelpEntry {               // 0x130-byte node payload
    std::string id;              // +0x00  "F120A": F, chapter, ...
    std::string text;            // +0x10
    std::string condition;       // +0x110  ConditionAtoms expression, "" if none
};

class HelpTable {
public:
    // 0x419210: keeps entries whose id[1] == chapter (as a digit).
    bool load(FileSource& fs, int chapter, std::string* err);
    static const char* soundBankName() { return "sounds/help.omt"; }
    const std::vector<HelpEntry>& entries() const { return entries_; }
    const HelpEntry* find(const std::string& id) const;   // port: also finds the chapter-0 lines (F000x), the targets of other lines' texts
private:
    std::vector<HelpEntry> entries_;
    std::vector<HelpEntry> common_;      // port: id[1] == '0' lines, kept apart so entries() stays the exe's list
};

}  // namespace sbscript
