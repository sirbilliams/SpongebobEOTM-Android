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
    Commands (CompanionPop): "item:<id>", "noitem", "choice:<n>", "advance", "menu", "skip".  */
#pragma once
#include <string>
#include <vector>

namespace sbport {

struct CompanionState {
    CompanionState() : inGame(false), cutscene(false), dialogue(false), choiceMode(false) {}
    bool inGame, cutscene, dialogue, choiceMode;
    std::string chapterTitle, sceneTitle, hint, caption, selected, speaker, text;
    std::vector<std::string> items, itemNames, choices, visited;   // itemNames: the display name of each item (parallel to items)
    std::string iconDir;           // folder with <item id>.png pictures (small, rendered at run time from the player's game files)
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
