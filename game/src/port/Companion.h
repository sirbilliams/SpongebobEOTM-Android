/*  port: the dual-screen companion (not in the original). A snapshot of what the player is doing, for the second
    screen (the AYN Thor's bottom display), as a small JSON string, and a queue of commands coming back from its
    touch buttons.  docs/companion.md.

    This file does not know the game: the game side fills a CompanionState (CompanionFeed.h, header-only, built
    with game/src/game) and calls CompanionPublish once a frame; the Android panel reads CompanionJson over JNI
    (Companion.nativeSnapshot) and sends taps back with CompanionPush (Companion.nativeCommand).

    JSON fields (missing = not known):
      game 1 once a scene is running (0: nothing yet)      chapter_title "Chapter 1"     scene_title "The Krusty Krab - Kitchen" (CompanionFeed.h)
      cut  1 in a cutscene (a movie plays or the cinematic bars are in): SKIP is only live then or on a dialogue line                  hint  the current help line   caption  the comment being shown
      items ["isp1", ...] the inventory                     sel   the item held as the cursor ("" none)
      names ["Shovel and Pail", ...] display names, parallel to items     icons  folder holding <id>.png pictures of the items
      visited ["Krusty Krab - Dish Pit", ...] scene names entered, oldest first (not tappable: the game's map has no travel list)
      dlg  {"choice":0/1, "speaker", "text", "choices":["...", ...]} while a dialogue is open
      map  [{"n":name ("" = not known yet),"c":"c1l6s1p1" (the map's own target),"x","y","w","h" (the map scene's click rectangle, 640x480),
            "ok":1 (the in-game map would let you go there now),"v":1 (visited),"here":1}], "mapbusy":1 (a dialogue / cutscene is on: the map button is dead)
      talk [{"i":serial,"s":speaker,"t":text,"v":1 (a voice exists)}], oldest first       combine {"n":serial,"ok":0/1,"t":text}  the last drag-combine / its refusal comment
      goals {"sv":scenes visited,"st":scenes of the chapter,"sc":scripts seen,"places":[{"n","v","t"}]}  (the hint is the 'what next')
    Commands (CompanionPop): "item:<id>", "noitem", "choice:<n>", "advance", "menu", "skip", "hint", "reveal" (lane e),
      "travel:<map target>" (the map; refused unless the in-game map would allow it), "voice:<serial>" (replay a talk line's voice),
      "combine:<item>:<item>" (an item card dragged onto another).  */
#pragma once
#include <string>
#include <vector>

namespace sbport {

struct CompanionState {
    CompanionState() : inGame(false), cutscene(false), dialogue(false), choiceMode(false), mapBusy(false), combineSerial(0), combineOk(false), scenesVisited(0), scenesTotal(0), scriptsSeen(0) {}
    bool inGame, cutscene, dialogue, choiceMode;
    std::string chapterTitle, sceneTitle, hint, caption, selected, speaker, text;
    std::vector<std::string> items, itemNames, choices, visited;   // itemNames: the display name of each item (parallel to items)
    std::string iconDir;           // folder with <item id>.png pictures (small, rendered at run time from the player's game files)
    // map page (CompanionFeed.h, from the chapter's map scene c<N>l1s1)
    struct MapArea { MapArea() : x(0), y(0), w(0), h(0), ok(false), visited(false), here(false) {}
                     std::string name, target; int x, y, w, h; bool ok, visited, here; };
    std::vector<MapArea> map;
    bool mapBusy;
    // talk page: the spoken lines of this session
    struct TalkLine { TalkLine() : id(0), voice(false) {} unsigned id; std::string speaker, text; bool voice; };
    std::vector<TalkLine> talk;
    // last combine (drag one item onto another)
    unsigned combineSerial; bool combineOk; std::string combineText;
    // goals page
    struct Place { Place() : visited(0), total(0) {} std::string name; int visited, total; };
    std::vector<Place> places;
    int scenesVisited, scenesTotal, scriptsSeen;
};

void CompanionSetEnabled(bool on);                  // any thread (the panel is up)
bool CompanionEnabled();                            // game thread: build snapshots only when wanted
void CompanionPublish(const CompanionState& s);     // game thread, once a frame (rate-limited inside)
std::string CompanionJson();                        // any thread: the latest snapshot ("" if none)
void CompanionPush(const std::string& cmd);         // any thread: a panel tap
bool CompanionPop(std::string* cmd);                // game thread: the next tap, false if none (taps older than 1.5 s are dropped)
void CompanionMovie(bool playing);                  // game thread: a Bink movie plays (it blocks the game loop: "skip" is then a key press the movie loop sees)
void CompanionKey(int sdlKeycode);                  // any thread: a key press as if typed (SDL_PushEvent): "menu" sends F10, the same key as the keyboard
std::string CompanionJsonOf(const CompanionState& s);   // pure; tests and the desktop sample writer

}  // namespace sbport
