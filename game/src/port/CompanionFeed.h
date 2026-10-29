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
            g->switchTo("skip");                       // same event path as a script's 'skip'
        } else if (cmd == "menu") {
            g->log("companion: menu tapped (STUB: the pause / main menu is not reconstructed yet)\n");
        }
    }
}

inline void companionTick(sbgame::Game* g, float /*dtMs*/)
{
    if (!CompanionEnabled()) return;
    CompanionFeedState& f = companionFeed();
    companionCommands(g);

    CompanionState s;
    sbgame::GameScene* sc = g->scene();
    unsigned id = g->state().sceneId;
    s.inGame = sc != 0;
    if (sc && id != f.lastScene) {
        f.lastScene = id;
        std::string n = companionSceneName(id);
        if (f.visited.empty() || f.visited.back() != n) f.visited.push_back(n);
        if (f.visited.size() > 24) f.visited.erase(f.visited.begin());
    }
    if (sc) {
        char b[40];
        snprintf(b, sizeof b, "Chapter %u", id & 0xf);
        s.chapterTitle = b;
        // STUB: the scenes' real names are not read yet (no name table found in sb2.omt / scn); "Level L, Scene S"
        snprintf(b, sizeof b, "Level %u, Scene %u", (id >> 8) & 0xf, (id >> 16) & 0xf);
        s.sceneTitle = b;
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
        // hint: the first help line of the chapter whose condition holds (the HUD's hint button shows the same).  STUB: how the exe picks one is unknown
        if (g->hud()) s.hint = g->hud()->hintText();
    }
    CompanionPublish(s);
}

}  // namespace sbport
