/*  port: companion -- the game side of the dual-screen panel (docs/companion.md).  Header-only on purpose: it needs
    game/src/game, which not every target links.  The game calls companionTick(this, dtMs) once a frame from
    Game::update (one `// port: companion` line); everything else is read-only getters, plus the panel's taps:
    choices (DialogueUI::pickChoice), skip (the 'skip' event), advance (DialogueUI::onKey), item (see below).  */
#pragma once
#include "Companion.h"
#include "../game/Game.h"
#include "../game/Hud.h"
#include "../game/Reveal.h"
#include "../scene/SceneData.h"
#include <dirent.h>
#include <strings.h>
#include <ctype.h>
#include <algorithm>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <string.h>
#include <string>
#include <vector>

namespace sbport {

struct CompanionFeedState {
    CompanionFeedState() : lastScene(0), mapChapter(0), scnChapter(0), lastDlg(0), lastLineSerial(0), talkId(0), replayScene(0), combineSerial(0), combineOk(false) {}
    unsigned lastScene;
    std::vector<std::string> visited;
    // map page: the objects of the chapter's map scene c<N>l1s1 (loaded once per chapter)
    struct MapObj { std::string ev, cond, alt, vis, desc; int x1, y1, x2, y2; };
    int mapChapter;
    std::vector<MapObj> mapObjs;
    // goals page: scenes of the chapter on disk (ids as in GameState::sceneId)
    int scnChapter;
    std::vector<unsigned> scnIds;
    // talk page: the lines said this session (port: the panel's replay needs the scene and key)
    struct TalkRec { unsigned id, scene; std::string key; };
    const void* lastDlg;
    unsigned lastLineSerial, talkId;
    std::vector<CompanionState::TalkLine> talk;
    std::vector<TalkRec> talkRec;
    sbgame::OmtFile replayBank;       // the sound bank of another scene, opened for a replay
    unsigned replayScene;
    // the last combine
    unsigned combineSerial;
    bool combineOk;
    std::string combineText;
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


// ---------------------------------------------------------------------------------------------------- map page
// The in-game map is the scene c<N>l1s1 (the HUD's map button switches to c<N>l1s1p1, 0x455234): each place is a 2DOB / SPOT whose
// Event is the scene to go to; the 2DOB's Visible Condition shows it, the base event (0x415a70) takes Event or Alt Event by the Condition.
// The panel reads those objects from the player's own file and applies the same rules; nothing is copied from the exe.
inline void companionLoadMap(sbgame::Game* g, int ch)
{
    CompanionFeedState& f = companionFeed();
    if (f.mapChapter == ch) return;
    f.mapChapter = ch;
    f.mapObjs.clear();
    char b[24];
    snprintf(b, sizeof b, "/scenes/c%dl1s1.scn", ch);
    sb::scene::Scene sc;
    std::string err;
    if (!sb::scene::load(g->root() + b, sc, err)) return;
    for (size_t i = 0; i < sc.elements.size(); ++i) {
        const sb::scene::Record& r = sc.elements[i];
        CompanionFeedState::MapObj m;
        if (r.tag == sb::scene::Object2DTag) {
            const sb::scene::Object2D& d = r.data.object2D;
            m.ev = d.o0020.text(); m.cond = d.o0040.text(); m.alt = d.o0060.text(); m.desc = d.o0120.text(); m.vis = d.o0100.text();
            m.x1 = (short)d.o0014; m.y1 = (short)d.o0016; m.x2 = (short)d.o0018; m.y2 = (short)d.o001a;
        } else if (r.tag == sb::scene::SpotTag) {
            const sb::scene::Spot& d = r.data.spot;
            m.ev = d.o0020.text(); m.cond = d.o0040.text(); m.alt = d.o0060.text(); m.desc = d.o0120.text();
            m.x1 = (short)d.o0014; m.y1 = (short)d.o0016; m.x2 = (short)d.o0018; m.y2 = (short)d.o001a;
        } else continue;
        f.mapObjs.push_back(m);
    }
}

inline bool companionIsSceneRef(const std::string& n)      // "c1l6s1p1": the game's own test (Game::switchTo: C _ L _ S)
{
    return n.size() >= 5 && toupper((unsigned char)n[0]) == 'C' && toupper((unsigned char)n[2]) == 'L' && toupper((unsigned char)n[4]) == 'S';
}

inline bool companionBusy(sbgame::Game* g)               // the HUD's map button is dead: a dialogue, a cutscene or a script runs, a scene switch waits
{
    sbgame::GameScene* sc = g->scene();
    sbgame::DialogueUI* d = sc ? sc->dialogue() : 0;
    return !sc || (d && d->busy()) || companionCutscene(g) || g->runner() || g->pendingScene();
}

inline std::string companionAreaName(const std::string& desc)    // "Go to the Barg-N-Mart" -> "The Barg-N-Mart"
{
    std::string n = desc;
    if (n.size() > 6 && strncasecmp(n.c_str(), "go to ", 6) == 0) n = n.substr(6);
    if (!n.empty()) n[0] = (char)toupper((unsigned char)n[0]);
    return n;
}

inline void companionMapAreas(sbgame::Game* g, std::vector<CompanionState::MapArea>* out)
{
    int ch = g->state().chapter;
    if (ch < 1) ch = (int)(g->state().sceneId & 0xf);
    if (ch < 1) return;
    companionLoadMap(g, ch);
    CompanionFeedState& f = companionFeed();
    const std::set<unsigned>& seen = g->state().scenesEntered();
    unsigned cur = g->state().sceneId;
    for (size_t i = 0; i < f.mapObjs.size(); ++i) {
        const CompanionFeedState::MapObj& m = f.mapObjs[i];
        std::string ev = g->cond(m.cond.c_str()) ? m.ev : m.alt;          // baseEvent: Event or Alt Event by the Condition
        if (!companionIsSceneRef(ev) || m.desc.empty()) continue;
        if (strcasecmp(m.cond.c_str(), "debug") == 0) continue;           // the CHEAT spots of the debug build
        CompanionState::MapArea a;
        a.target = ev;
        a.x = m.x1; a.y = m.y1; a.w = m.x2 - m.x1; a.h = m.y2 - m.y1;
        a.ok = m.vis.empty() || g->cond(m.vis.c_str());                  // Visible Condition (2DOB): hidden = not clickable
        unsigned lv = (unsigned)(ev[3] - '0'), cl = (unsigned)(ev[1] - '0');
        for (std::set<unsigned>::const_iterator it = seen.begin(); it != seen.end() && !a.visited; ++it)
            if ((*it & 0xf) == cl && ((*it >> 8) & 0xf) == lv) a.visited = true;
        a.here = (cur & 0xf) == cl && ((cur >> 8) & 0xf) == lv;
        if (a.ok || a.visited) a.name = companionAreaName(m.desc);       // a place not opened yet stays a "?"
        out->push_back(a);
    }
}

// The panel's travel tap: the same rule as clicking the place on the map scene (Visible Condition, Condition), and the map button needs a free HUD.
inline void companionTravel(sbgame::Game* g, const std::string& target)
{
    std::vector<CompanionState::MapArea> areas;
    companionMapAreas(g, &areas);
    for (size_t i = 0; i < areas.size(); ++i) {
        if (strcasecmp(areas[i].target.c_str(), target.c_str()) != 0) continue;
        if (!areas[i].ok) { g->log("companion: travel %s refused (the map does not offer it yet)\n", target.c_str()); return; }
        if (companionBusy(g)) { g->log("companion: travel %s refused (busy: dialogue %d, cutscene %d, script %d, switch %d)\n", target.c_str(), g->scene() && g->scene()->dialogue() && g->scene()->dialogue()->busy(), companionCutscene(g), g->runner() != 0, g->pendingScene()); return; }
        g->log("companion: travel %s\n", target.c_str());
        g->switchTo(target.c_str());
        return;
    }
    g->log("companion: travel %s refused (not on the map)\n", target.c_str());
}

// ---------------------------------------------------------------------------------------------------- talk page
// The voice of a dialogue line: the scene's sound bank, the key letters in the seven case combinations (DialogueUI::startVoice, 0x412d00).
inline OMediaSound* companionVoice(sbgame::Game* g, unsigned scene, const std::string& key)
{
    CompanionFeedState& f = companionFeed();
    OMediaDataBase* bank = 0;
    if (scene == g->state().sceneId) bank = g->sceneSoundDb();
    else {
        if (f.replayScene != scene || !f.replayBank.db) {
            g->stopChannel(4);                                // the old bank's sound must not play on
            f.replayBank.reset();
            char b[96];
            snprintf(b, sizeof b, "/sounds/c%ul%us%u.omt", scene & 0xf, (scene >> 8) & 0xf, (scene >> 16) & 0xf);
            f.replayBank.open(g->root() + b);
            f.replayScene = scene;
        }
        bank = f.replayBank.db;
    }
    if (!bank || key.empty()) return 0;
    char ch[4] = { key[0], key.size() > 1 ? key[1] : ' ', key.size() > 2 ? key[2] : ' ', 0 };
    static const int combos[7][3] = { {0,0,0},{1,0,1},{1,1,0},{0,1,1},{0,1,0},{1,0,0},{0,0,1} };
    for (int c = 0; c < 7; ++c) {
        char n[16];
        snprintf(n, sizeof n, "%d%d%d%c%c%c", (int)(scene & 0xf), (int)((scene >> 8) & 0xf), (int)((scene >> 16) & 0xf),
                 combos[c][0] ? toupper((unsigned char)ch[0]) : tolower((unsigned char)ch[0]),
                 combos[c][1] ? toupper((unsigned char)ch[1]) : tolower((unsigned char)ch[1]),
                 combos[c][2] ? toupper((unsigned char)ch[2]) : tolower((unsigned char)ch[2]));
        if (!bank->object_exists(OMediaSound::db_type, std::string(n))) continue;
        OMediaSound* s = (OMediaSound*)bank->get_object(OMediaSound::db_type, std::string(n));
        if (s) return s;
    }
    return 0;
}

inline void companionRecordTalk(sbgame::Game* g, sbgame::DialogueUI* d)
{
    CompanionFeedState& f = companionFeed();
    if (d != f.lastDlg) { f.lastDlg = d; f.lastLineSerial = 0; }
    if (!d || !d->busy() || d->lineSerial() == f.lastLineSerial || d->currentText().empty()) return;
    f.lastLineSerial = d->lineSerial();
    CompanionState::TalkLine t;
    CompanionFeedState::TalkRec r;
    r.id = t.id = ++f.talkId;
    r.scene = g->state().sceneId;
    r.key = d->currentKey();
    t.speaker = d->currentSpeaker();
    t.text = d->currentText();
    t.voice = companionVoice(g, r.scene, r.key) != 0;
    f.talk.push_back(t);
    f.talkRec.push_back(r);
    if (f.talk.size() > 60) { f.talk.erase(f.talk.begin()); f.talkRec.erase(f.talkRec.begin()); }
    g->log("companion: talk %u [%s] %s\n", t.id, t.speaker.c_str(), t.text.c_str());
}

inline void companionReplayVoice(sbgame::Game* g, unsigned id)
{
    CompanionFeedState& f = companionFeed();
    sbgame::GameScene* sc = g->scene();
    sbgame::DialogueUI* d = sc ? sc->dialogue() : 0;
    if (d && d->busy()) { g->log("companion: voice %u refused (a line is being said)\n", id); return; }
    for (size_t i = 0; i < f.talkRec.size(); ++i) {
        if (f.talkRec[i].id != id) continue;
        OMediaSound* s = companionVoice(g, f.talkRec[i].scene, f.talkRec[i].key);
        if (!s) { g->log("companion: voice %u: none\n", id); return; }
        g->stopChannel(4);
        bool ok = g->playOnChannel(4, s, 0);                // the dialogue's own channel
        g->log("companion: voice %u replay %s\n", id, ok ? "playing" : "failed");
        return;
    }
}

// ---------------------------------------------------------------------------------------------------- combine
inline void companionCombine(sbgame::Game* g, const std::string& a, const std::string& b)
{
    CompanionFeedState& f = companionFeed();
    if (!g->hud()) return;
    std::vector<sbgame::Hud::Entry> before = g->hud()->items();
    unsigned cs = g->commentSerial;
    bool made = false;
    bool tried = g->hud()->combineFromPanel(a, b, &made);       // Hud::combine (0x421880) or the refusal comment H000F
    ++f.combineSerial;
    f.combineOk = made;
    if (!tried) f.combineText = "Not right now.";
    else if (made) {
        f.combineText = "Made something new.";
        std::vector<sbgame::Hud::Entry> after = g->hud()->items();
        for (size_t i = 0; i < after.size(); ++i) {
            bool had = false;
            for (size_t k = 0; k < before.size(); ++k) if (before[k].name == after[i].name) had = true;
            if (!had) f.combineText = "Made: " + after[i].desc;
        }
    } else f.combineText = g->commentSerial != cs ? g->lastCommentText : std::string("That doesn't work.");
    g->log("companion: combine %s + %s -> %s: %s\n", a.c_str(), b.c_str(), made ? "made" : "refused", f.combineText.c_str());
}

// ---------------------------------------------------------------------------------------------------- goals page
inline bool companionSceneLess(unsigned a, unsigned b)    // by level, then scene
{
    unsigned la = (a >> 8) & 0xf, lb = (b >> 8) & 0xf;
    return la != lb ? la < lb : (a >> 16) < (b >> 16);
}

inline void companionGoals(sbgame::Game* g, CompanionState* s)
{
    CompanionFeedState& f = companionFeed();
    int ch = (int)(g->state().sceneId & 0xf);
    if (ch < 1) ch = g->state().chapter;
    if (ch < 1) return;
    if (f.scnChapter != ch) {                                // the chapter's scenes: the player's scenes/ folder
        f.scnChapter = ch;
        f.scnIds.clear();
        DIR* dir = opendir((g->root() + "/scenes").c_str());
        while (dir) {
            struct dirent* e = readdir(dir);
            if (!e) break;
            unsigned c, l, sn;
            char tail[8];
            if (sscanf(e->d_name, "c%ul%us%u.%3s", &c, &l, &sn, tail) == 4 && (int)c == ch && l >= 2) f.scnIds.push_back(c | (l << 8) | (sn << 16));
        }
        if (dir) closedir(dir);
        std::sort(f.scnIds.begin(), f.scnIds.end(), companionSceneLess);
    }
    const std::set<unsigned>& seen = g->state().scenesEntered();
    for (size_t i = 0; i < f.scnIds.size(); ++i) {
        unsigned lv = (f.scnIds[i] >> 8) & 0xf;
        bool v = seen.count(f.scnIds[i]) != 0;
        // one row per level
        const CompanionPlace* p = companionPlace((unsigned)ch, lv);
        std::string nm = p ? p->place : "Area";
        if (i == 0 || ((f.scnIds[i - 1] >> 8) & 0xf) != lv) { CompanionState::Place pl; pl.name = nm; s->places.push_back(pl); }
        s->places.back().total++;
        if (v) { s->places.back().visited++; s->scenesVisited++; }
        s->scenesTotal++;
    }
    s->scriptsSeen = (int)g->state().scriptsSeenCount();
}

inline void companionCommands(sbgame::Game* g)
{
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
        } else if (cmd == "hint") {
            if (g->hud()) g->hud()->hint();            // the HUD's hint button (Hud::hint): text and, if the help bank is installed, its voice
        } else if (cmd == "reveal") {                  // port: hotspot reveal (game/src/game/Reveal.h): outline everything clickable for about 2 s
            if (g->reveal()) g->reveal()->pulse();
        } else if (cmd.compare(0, 7, "travel:") == 0) {
            companionTravel(g, cmd.substr(7));         // the map page: only where the in-game map would let you go
        } else if (cmd.compare(0, 6, "voice:") == 0) {
            companionReplayVoice(g, (unsigned)atoi(cmd.c_str() + 6));
        } else if (cmd.compare(0, 8, "combine:") == 0) {
            size_t k = cmd.find(':', 8);
            if (k != std::string::npos) companionCombine(g, cmd.substr(8, k - 8), cmd.substr(k + 1));
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
        companionRecordTalk(g, d);
        companionMapAreas(g, &s.map);
        s.mapBusy = companionBusy(g);
        companionGoals(g, &s);
        s.combineSerial = f.combineSerial; s.combineOk = f.combineOk; s.combineText = f.combineText;
        if (d && d->busy()) {
            s.dialogue = true;
            s.choiceMode = d->choiceMode();
            s.speaker = d->currentSpeaker();
            s.text = d->currentText();
            if (s.choiceMode) s.choices = d->choiceTexts();
        }
        // hint: the first help line of the chapter whose condition holds, an alias (F121A -> F000D) resolved to its text (the HUD's hint button shows the same).  The hint voice (sounds/help.omt) plays in the game via the "hint" command, not in the panel
        if (g->hud()) s.hint = g->hud()->hintText();
    }
    s.talk = f.talk;
    CompanionPublish(s);
}

}  // namespace sbport
