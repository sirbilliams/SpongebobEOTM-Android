/*  The script interpreter of sb2.exe: object of 0x1964 bytes (ctor 0x43d550), loader 0x43dae0 (text fallback
    0x43d750), single-step dispatcher 0x43e110.  A script is a list of tab separated command lines, at most
    four fields (command + 3 arguments); the dispatcher runs ONE line per call and returns whether the
    script now waits.  Everything that touches the world goes through ScriptHost.  See docs/script.md.  */
#pragma once
#include "ScriptHost.h"
#include <string>
#include <vector>

namespace sbscript {

struct ScriptLine {            // 0x80-byte record at runner+8+i*0x80: command +0, args +0x20, +0x40, +0x60
    std::string cmd, a1, a2, a3;   // port: the exe uses char[32] fields; fields are cut to 31 chars here
};

class ScriptRunner {
public:
    enum { kMaxLines = 50 };   // 0x1900 / 0x80: room for the lines before runner+0x1908

    explicit ScriptRunner(ScriptHost* host);      // 0x43d550
    ~ScriptRunner();                              // 0x43d5d0 (stops the sound channel, frees an own sound)

    // 0x43dae0: `name` without extension, chapter from the host.  `actor` null: the scene's active actor.
    // Returns false if no file was found (the exe then tries 0x43d750).  After a successful load the
    // exe runs the dispatcher until the first wait; so does this (runUntilWait).
    bool load(const char* name, ScriptActor* actor);
    // Decode already read bytes (the .sb2 container) instead of going through host->readFile.
    bool loadBytes(const char* name, ScriptActor* actor, const unsigned char* data, unsigned size, std::string* err);
    bool loadText(const char* name, ScriptActor* actor, const std::string& text);   // 0x43d750 body

    // Every command name the dispatcher compares against, in chain order (59).
    static const char* const* commandNames();
    static unsigned commandCount();
    static bool isKnownCommand(const std::string& cmd);
    // The loader's field splitting alone (no run): 0x43dae0 record loop.
    static bool parseRecords(const std::vector<std::string>& records, std::vector<ScriptLine>& out, std::string* err);

    // Test hooks (no equivalent in the exe): print each line before it runs; don't run after a load.
    void setTrace(void* file) { trace_ = file; }
    void setAutoRun(bool v) { autoRun_ = v; }
    unsigned stepCount() const { return steps_; }

    bool step();            // 0x43e110: returns true when the script waits (or ended), false to go on
    void runUntilWait();    // scene vt+0xe0 (0x43aa80): `do step() while (!waits)`, the call every wake-up event makes
    bool finished() const { return finished_; }                  // 0x43df20
    void stopSound();                                            // 0x43d5e0: stop the channel, free an own sound
    bool skipToLetterboxClose();                                 // 0x43df30 (cutscene skip)
    bool talkAhead() const;                                      // 0x43e0b0: any 'talk' after the current line
    bool idleElapsed(float dtMs);                                // 0x43d650: scene frame update, idle timer
    bool soundDone();                                            // 0x43d6e0: scene frame update, sound wait
    void onClick(short x, short y);                              // 0x43dea0: stops a walk if the click repeats the start position

    // ---- state (offsets in the exe object) ----
    int  current() const { return cur_; }                        // +0x190a (starts at -1)
    int  lineCount() const { return count_; }                    // +0x1908
    const ScriptLine& line(int i) const { return lines_[i]; }
    const std::string& name() const { return name_; }            // +0x190d
    const std::string& lastCommand() const { return lastCmd_; }  // +0x192d
    ScriptActor* actor() const { return actor_; }                // +0x1950
    bool keepControl() const { return control_; }                // +0x194d ('control on' sets it)
    bool waitingUi() const { return waitUi_; }                   // +0x1954 add/unlock/video
    bool waitingComment() const { return waitComment_; }         // +0x1955
    bool waitingTalk() const { return waitTalk_; }               // +0x1956 talk/narrate dialogue running
    bool waitingSound() const { return waitSound_; }             // +0x1957
    bool terrain() const { return terrain_; }                    // +0x1960
    float idleMs() const { return idleMs_; }                     // +0x195c
    void setIdleRunner(bool v) { idleRunner_ = v; }              // +0x1962: scene idle scripts use sound channel 9
    bool isIdleRunner() const { return idleRunner_; }
    void backUp() { if (--cur_ < -1) cur_ = -1; }                // 0x4350d0: an interrupted idle script repeats its line (+0x190a - 1)

private:
    void end();                                                  // 0x43dfd0
    bool parse(const std::vector<std::string>& records, std::string* err);
    void begin(ScriptActor* actor);                              // tail of the loaders
    void freeSound();
    bool startSound(const ScriptLine& l);
    bool doGoal();

    ScriptHost* host_;
    ScriptSound* sound_;        // +4
    std::vector<ScriptLine> lines_;
    int count_;                 // +0x1908
    int cur_;                   // +0x190a
    bool finished_;             // +0x190c
    std::string name_;          // +0x190d
    std::string lastCmd_;       // +0x192d
    bool control_;              // +0x194d
    ScriptActor* actor_;        // +0x1950
    bool waitUi_, waitComment_, waitTalk_, waitSound_;   // +0x1954..0x1957
    short mouseX_, mouseY_;     // +0x1958, +0x195a
    float idleMs_;              // +0x195c
    bool terrain_;              // +0x1960
    bool soundShared_;          // +0x1961: sound belongs to the shared bank, not freed
    bool idleRunner_;           // +0x1962
    void* trace_;
    bool autoRun_;
    unsigned steps_;
};

}  // namespace sbscript
