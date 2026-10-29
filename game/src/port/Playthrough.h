/*  port: tests only (desktop).  SB_TEST_PLAYTHROUGH=<file>: a scripted playthrough of the real game, one step per line,
    driven through the real input path (clicks are SDL window events, keys go to the dialogue as a key press would).
    Each step waits until the game is idle (no scene switch, script, dialogue, comment, walk or pickup going on), then acts.
    Steps (see docs/chapter1.md):
      scene <cXlYsZ>         wait until this scene is the current one (and idle)
      idle                   wait until idle
      click <target>         a click on the target: @x,y | an object's art name | "its description"
      talk <target> [KEY..]  click the target, then answer each choice list with the next KEY (none left: Escape); lines are
                             skipped with Space
      say [KEY..]            answer the next choice lists of a conversation a script started
      use <item> <target>    open the bar, pick the item up from its slot, click the target with it
      combine <a> <b>        open the bar, pick up a, click b's slot
      map                    click the bar's map button
      expect <condition>     a condition expression (docs/script.md) must hold
      have <item>            the item must be in the bar
      shot <name>            save the screen as <SB_TEST_SHOTDIR>/<name>.bmp
      save <slot>            write a save game to the slot (the Save Game screen's writer)
      wait <frames>          let frames pass
      end                    the playthrough succeeded: the game quits
    A step that does not finish within SB_TEST_STEPFRAMES (default 6000) frames fails the run ("PLAYTHROUGH FAIL").  */
#pragma once
#include <string>
#include <vector>

namespace sbgame { class Game; }

namespace sbport {

class Playthrough {
public:
    explicit Playthrough(const char* file);
    bool ok() const { return !steps_.empty(); }
    // once per game-loop frame, before the events are read.  `click` sends a logical click, `key` a key to the dialogue,
    // `save` writes slot n.  Returns false when the run is over (end or failure); *failed tells which.
    bool tick(sbgame::Game* g, void (*click)(int x, int y), void (*save)(sbgame::Game* g, int slot), bool* failed);
private:
    struct Step { int line; std::string cmd; std::vector<std::string> args; };
    std::vector<Step> steps_;
    size_t cur_;
    int phase_, frames_, waitLeft_, idleFrames_;
    std::vector<std::string> answers_;
    std::string file_;
    bool idle(sbgame::Game* g);
    bool locate(sbgame::Game* g, const std::string& target, int* x, int* y);
    bool dialogueStep(sbgame::Game* g);
    void fail(sbgame::Game* g, const char* why);
    bool failed_;
};

}  // namespace sbport
