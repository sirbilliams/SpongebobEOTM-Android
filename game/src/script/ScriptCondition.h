/*  Condition expressions of the script commands 'skipif' and 'exitif' and of the help table's Condition
    column: sb2.exe game vt+0xb8 (0x40a6b0, expression) and vt+0xbc (0x40a7d0, one term).  See docs/script.md.
    The terms query the game's progress object (0x457330); that object is reached through ConditionAtoms.  */
#pragma once

namespace sbscript {

class ConditionAtoms {
public:
    virtual ~ConditionAtoms() {}
    virtual bool debugAll() = 0;                         // game+0x24f: every expression is true
    virtual bool scriptSeen(const char* term) = 0;       // 'S...'  progress 0x416690
    virtual bool hasItem(const char* term) = 0;          // 'I...'  progress 0x416a20
    virtual bool flagSet(const char* term) = 0;          // 'C' + digits at [1],[3],[5], length > 3: progress 0x416ad0
    // Dialogue progress entry (progress 0x416e50): found, and its first short (entry+0x20) != 0.
    virtual bool dialogueStarted(unsigned sceneId, const char* groupKey) = 0;
    virtual unsigned currentSceneId() = 0;               // scene+0x264
    virtual bool miniGameFlag() = 0;                     // term "xxxx": game+0x12c object short +0x14 != 0 && short +0x12 == 0
};

// 0x40a6b0.  Terms are separated by spaces; " or " anywhere switches the whole expression to OR,
// otherwise it is AND.  The tokens "or" and "and" are skipped.  `touching` is the second argument
// (the script dispatcher always passes 0).
bool evalCondition(const char* expr, bool touching, ConditionAtoms& atoms);

// 0x40a7d0: one term (optional leading '!').  "0", "" and " " are true.
bool evalConditionTerm(const char* term, bool touching, ConditionAtoms& atoms);

}  // namespace sbscript
