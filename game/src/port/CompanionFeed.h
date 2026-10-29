/*  port: companion -- the game side of the dual-screen panel (docs/companion.md).  Header-only on purpose: it needs
    game/src/game, which not every target links.  The game calls companionTick(this, dtMs) once a frame from
    Game::update (one `// port: companion` line); everything else is read-only getters, plus the panel's taps:
    choices (DialogueUI::pickChoice), skip (the 'skip' event), advance (DialogueUI::onKey), item (see below).  */
#pragma once
#include "Companion.h"
#include "../game/Game.h"
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
    std::string selected;          // the item taken in hand from the panel
};

inline CompanionFeedState& companionFeed() { static CompanionFeedState s; return s; }

// The item the panel put in hand ("" none).  STUB: the game's inventory bar / cursor object (game+0x88, 0x4201c0) is
// not reconstructed yet; when it is, it reads this and calls GameScene::useItem(object, item) on the next click.
inline const std::string& companionSelectedItem() { return companionFeed().selected; }

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
            const std::vector<std::string>& have = g->state().items();
            std::string id = cmd.substr(5);
            for (size_t i = 0; i < have.size(); ++i) if (have[i] == id) f.selected = id;
        } else if (cmd == "noitem") {
            f.selected.clear();
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
        s.items = g->state().items();
        // the held item must still be held
        bool held = false;
        for (size_t i = 0; i < s.items.size(); ++i) if (s.items[i] == f.selected) held = true;
        if (!held) f.selected.clear();
        s.selected = f.selected;
        s.visited = f.visited;
        sbgame::DialogueUI* d = sc->dialogue();
        if (d && d->busy()) {
            s.dialogue = true;
            s.choiceMode = d->choiceMode();
            s.speaker = d->currentSpeaker();
            s.text = d->currentText();
            if (s.choiceMode) s.choices = d->choiceTexts();
        }
        // hint: the first help line of the chapter whose condition holds.  STUB: how the exe picks one is unknown
        const std::vector<sbscript::HelpEntry>& h = g->help().entries();
        for (size_t i = 0; i < h.size(); ++i)
            if (!h[i].condition.empty() && g->cond(h[i].condition.c_str())) { s.hint = h[i].text; break; }
    }
    CompanionPublish(s);
}

}  // namespace sbport
