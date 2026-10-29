/*  port: companion -- the game side of the dual-screen panel (docs/companion.md).  Header-only on purpose: it needs
    game/src/game, which not every target links.  The game calls companionTick(this, dtMs) once a frame from
    Game::update (one `// port: companion` line); everything else is read-only getters, plus the panel's taps:
    choices (DialogueUI::pickChoice), skip (the 'skip' event), advance (DialogueUI::onKey), item (see below).  */
#pragma once
#include "Companion.h"
#include "../game/Game.h"
#include "../game/Hud.h"
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <string.h>
#include <string>
#include <vector>

namespace sbport {

struct CompanionFeedState {
    CompanionFeedState() : lastScene(0) {}
    unsigned lastScene;
    std::vector<std::string> visited;
};

inline CompanionFeedState& companionFeed() { static CompanionFeedState s; return s; }

// The item in hand ("" none): the HUD's (game/src/game/Hud.cpp).  The panel's taps put an item in hand there; the next
// click in the scene uses it (GameScene::useItem), exactly like an item taken from the bar.
inline std::string companionSelectedItem(sbgame::Game* g) { return g->hud() ? g->hud()->heldName() : std::string(); }

inline std::string companionSceneName(unsigned id)    // "c1l2s6" (chapter in the low nibble, see script.md)
{
    char b[24];
    snprintf(b, sizeof b, "c%ul%us%u", id & 0xf, (id >> 8) & 0xf, (id >> 16) & 0xf);
    return b;
}

// port: place names.  The game files name no scene (scene captions, dialogue and comment tables hold no place names), so this is a short
// hand-written table of the places, from the clickable labels of the scenes themselves ("Go to The Krusty Krab", "Walk to Salad Bar"...).
// Level = the place, scene = the room where known; otherwise "<place> - Area <scene>".  No art.
struct CompanionPlace { const char* place; const char* rooms[10]; };    // rooms[s] for scene s ("" = unknown)
inline const CompanionPlace* companionPlace(unsigned ch, unsigned lv)
{
    static const CompanionPlace t[5][7] = {
        { { 0 } },
        { { "Intro", { 0 } }, { "Bikini Bottom", { "Map" } }, { "The Krusty Krab", { "", "Dining Room", "Order Counter", "Salad Bar", "Kitchen", "Dish Pit", "Outside", "Storage Cabinet" } },
          { "The Neighborhood", { "", "Garden", "Front Yard", "Back Yard", "Living Room", "Kitchen", "Parking Sign" } },
          { "The Supermarket", { "", "Entrance", "Parking Lot", "Checkout", "Food Aisle", "Food Aisle", "Food Aisle", "Parking Lot" } },
          { "Goo Lagoon", { "", "Beach", "Parking Lot", "Lifeguard Area", "Lifeguard Station", "Sand Castle", "Castle Exit", "Boom Box", "Treasure Chest", "Treasure Chest" } },
          { "Downtown", { "", "Bus Stop", "Jewelry Shop", "Jewelry Shop Counter", "Bus Stop" } } },
        { { "Intro", { 0 } }, { "Rock Bottom", { "Map" } }, { "The Bus Station", { "", "Ticket Window", "Waiting Area", "Road", "Road", "Road", "Road", "Entrance" } },
          { "The Rusty Anchor", { "", "Entrance", "Parking Lot", "Bar", "Cola Bar", "Kegs", "" } },
          { "WSEA TV", { "Camera", "Entrance", "Parking Lot", "News Room", "Studio", "Control Room", "Weather Control Room", "Camera Room", "Broadcast Room", "Stage" } },
          { "Marlin's Cave", { "", "Entrance", "Magic Hat", "Slot Machine", "Cavern", "Stairs", "Stairs", "", "Exit" } }, { 0 } },
        { { "Intro", { 0 } }, { "Bikini Bottom", { "Map" } }, { "The Diner", { "", "Outside", "Entrance", "Dining Area", "Jukebox", "Dart Board" } },
          { "Jellyfish Fields", { "", "Fields", "Fields", "", "Fields" } },
          { "Home Lane", { "Living Room", "Pineapple Row", "Squidward's House", "Stairs", "Road", "Pineapple Row", "Pyramid", "Stage", "SpongeBob's House", "Living Room" } },
          { "Sardine's", { "", "Entrance", "Dining Room", "Dining Room" } },
          { "Sandy's Tree", { "Entrance", "Dome Door", "Picnic Area", "Picnic Table", "Dome", "Picnic Area" } } },
        { { "Intro", { 0 } }, { "Bikini Bottom", { "Map" } }, { "The Rocket Landing", { "", "Bottoms Up", "Phone Booth", "Rocket", "" } },
          { "Waverly Hills", { "", "Gate", "Fountain", "Rodeo Drive", "Rodeo Drive", "Shack", "", "Shack Back Room", "Mansion Living Room", "Pedestal" } },
          { "Sublime Seafoods", { "Entrance", "", "Bottoms Up", "Security Gate", "Path", "Entrance", "Dining Room", "Lobby", "Reserved Table", "Binoculars" } },
          { "Oxygen Springs", { "", "Entrance", "Geysers", "Locker Room", "Geysers", "Locker Room" } }, { 0 } },
    };
    if (ch < 1 || ch > 4 || lv > 6 || !t[ch][lv].place) return 0;
    return &t[ch][lv];
}

inline std::string companionPrettyName(unsigned id)    // "The Krusty Krab - Kitchen"; unknown places keep a plain "Chapter 1, Level 2, Scene 6"
{
    unsigned ch = id & 0xf, lv = (id >> 8) & 0xf, sc = (id >> 16) & 0xf;
    const CompanionPlace* p = companionPlace(ch, lv);
    char b[64];
    if (!p) { snprintf(b, sizeof b, "Chapter %u, Level %u, Scene %u", ch, lv, sc); return b; }
    if (lv == 0 || lv == 1) return p->place;           // intro / map hub
    const char* room = sc < 10 ? p->rooms[sc] : 0;
    if (room && *room) { snprintf(b, sizeof b, "%s - %s", p->place, room); return b; }
    snprintf(b, sizeof b, "%s - Area %u", p->place, sc);
    return b;
}

// The game's cutscene skip is a click while the cinematic bars are in (0x41ae00): ScriptRunner::skipToLetterboxClose (0x43df30), then the
// script runs on (scene vt+0xe0 = Game::wake).
inline bool companionCutscene(sbgame::Game* g)
{
    sbgame::GameScene* sc = g->scene();
    return sc && g->runner() && sc->letterboxed();
}

inline void companionCommands(sbgame::Game* g)
{
    CompanionFeedState& f = companionFeed();
    std::string cmd;
    while (CompanionPop(&cmd)) {
        sbgame::GameScene* sc = g->scene();
        sbgame::DialogueUI* d = sc ? sc->dialogue() : 0;
        if (cmd.compare(0, 5, "item:") == 0) {
            if (g->hud()) g->hud()->holdFromPanel(cmd.substr(5));
        } else if (cmd == "noitem") {
            if (g->hud()) g->hud()->onEscape();
        } else if (cmd.compare(0, 7, "choice:") == 0) {
            if (d) d->pickChoice(atoi(cmd.c_str() + 7));
        } else if (cmd == "advance") {
            if (d && d->busy() && !d->choiceMode()) d->onKey(' ');
        } else if (cmd == "skip") {
            // a dialogue line: the click that closes it; else the cutscene skip of the exe's click handler (a movie is handled in Companion.cpp)
            if (d && d->busy() && !d->choiceMode()) d->onKey(' ');
            else if (companionCutscene(g) && g->runner()->skipToLetterboxClose()) g->wake();
        } else if (cmd == "menu") {
            CompanionKey(SDLK_F10);                    // exactly the F10 / Android Back key: StartupMain pauses the game behind the main menu
        }
    }
}

inline void companionTick(sbgame::Game* g, float /*dtMs*/)
{
    if (!CompanionEnabled()) return;
    CompanionFeedState& f = companionFeed();
    {   // port: tests only. SB_TEST_COMPANION="<tick>:<command>" taps the panel at that tick (menu / skip / advance ...)
        static unsigned tick = 0;
        const char* t = getenv("SB_TEST_COMPANION");      // "60:switch=smeanwhile1,200:skip"
        ++tick;
        while (t && *t) {
            const char* comma = strchr(t, ',');
            std::string item = comma ? std::string(t, comma - t) : std::string(t);
            size_t col = item.find(':');
            if (col != std::string::npos && (unsigned)atoi(item.c_str()) == tick) {
                std::string c = item.substr(col + 1);
                if (c.compare(0, 7, "switch=") == 0) g->switchTo(c.c_str() + 7); else CompanionPush(c);
            }
            t = comma ? comma + 1 : 0;
        }
    }
    companionCommands(g);

    CompanionState s;
    sbgame::GameScene* sc = g->scene();
    unsigned id = g->state().sceneId;
    s.inGame = sc != 0;
    if (sc && id != f.lastScene) {
        f.lastScene = id;
        std::string n = companionPrettyName(id);
        if (f.visited.empty() || f.visited.back() != n) f.visited.push_back(n);
        if (f.visited.size() > 24) f.visited.erase(f.visited.begin());
    }
    if (sc) {
        char b[40];
        snprintf(b, sizeof b, "Chapter %u", id & 0xf);
        s.chapterTitle = b;
        s.sceneTitle = companionPrettyName(id);
        s.cutscene = companionCutscene(g);
        if (g->hud()) {                          // the inventory as the bar has it (items used up are gone), with the 'add' text as name
            std::vector<sbgame::Hud::Entry> e = g->hud()->items();
            for (size_t i = 0; i < e.size(); ++i) {
                s.items.push_back(e[i].name);
                s.itemNames.push_back(e[i].desc);
                g->hud()->ensureIcon(e[i].name);   // the 96x96 picture of the model, rendered once, read by the panel from iconDir
            }
            s.iconDir = g->hud()->iconDir();
            s.selected = g->hud()->heldName();
        }
        s.visited = f.visited;
        sbgame::DialogueUI* d = sc->dialogue();
        if (d && d->busy()) {
            s.dialogue = true;
            s.choiceMode = d->choiceMode();
            s.speaker = d->currentSpeaker();
            s.text = d->currentText();
            if (s.choiceMode) s.choices = d->choiceTexts();
        }
        // hint: the first help line of the chapter whose condition holds, an alias (F121A -> F000D) resolved to its text (the HUD's hint button shows the same).  The help voice bank sounds/help.omt is not in the install, so no voice/speaker
        if (g->hud()) s.hint = g->hud()->hintText();
    }
    CompanionPublish(s);
}

}  // namespace sbport
