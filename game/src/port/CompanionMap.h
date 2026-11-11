/*  port: companion, map page data that needs no running game: scene links and a look-only room layout, the characters of a scene,
    the help-line (hint) rules, and the JSON of the extra map fields (docs/companion.md, "Map data v2").  Header-only and free of Game /
    OMT so that sb_unittest can check it against a scene of the player's install.  */
#pragma once
#include "Companion.h"
#include "../scene/SceneData.h"
#include "../script/Dialogue.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace sbport {

namespace cj {
inline void esc(std::string& out, const std::string& s)
{
    out += '"';
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '"' || ch == '\\') { out += '\\'; out += (char)ch; }
        else if (ch == '\n') out += "\\n";
        else if (ch == '\t') out += ' ';
        else if (ch < 0x20) out += ' ';
        else if (ch == 0x91 || ch == 0x92) out += '\'';        // Windows-1252 curly quotes
        else if (ch == 0x93 || ch == 0x94) out += "\\\"";
        else if (ch >= 0x80) out += '?';       // the game's text is Latin-1 (not valid UTF-8): keep the JSON clean
        else out += (char)ch;
    }
    out += '"';
}
inline void arr(std::string& out, const std::vector<std::string>& v)
{
    out += '[';
    for (size_t i = 0; i < v.size(); ++i) { if (i) out += ','; esc(out, v[i]); }
    out += ']';
}
}  // namespace cj

// "c1l6s1p1" -> chapter 1, level 6, scene 1 (the game's own test: C _ L _ S)
inline bool companionRefParts(const std::string& n, int* ch, int* lv, int* sc)
{
    if (n.size() < 6 || toupper((unsigned char)n[0]) != 'C' || toupper((unsigned char)n[2]) != 'L' || toupper((unsigned char)n[4]) != 'S') return false;
    if (!isdigit((unsigned char)n[1]) || !isdigit((unsigned char)n[3]) || !isdigit((unsigned char)n[5])) return false;
    *ch = n[1] - '0'; *lv = n[3] - '0'; *sc = n[5] - '0';
    return true;
}

// Every scene reference a scene file holds (camera exits, events and alt events of its objects): "c1l2s1p1" style, as written.
inline void companionSceneRefs(const sb::scene::Scene& s, std::vector<std::string>* out)
{
    const sb::scene::Camera& c = s.camera.data.camera;
    out->push_back(c.o0192.text()); out->push_back(c.o01b2.text()); out->push_back(c.o01d2.text()); out->push_back(c.o01f2.text());
    for (size_t i = 0; i < s.elements.size(); ++i) {
        const sb::scene::Record& r = s.elements[i];
        const sb::scene::Text32 *ev = 0, *alt = 0;
        switch (r.tag) {
        case sb::scene::Object2DTag: ev = &r.data.object2D.o0020; alt = &r.data.object2D.o0060; break;
        case sb::scene::SpotTag: ev = &r.data.spot.o0020; alt = &r.data.spot.o0060; break;
        case sb::scene::TriggerTag: ev = &r.data.trigger.o0020; alt = &r.data.trigger.o0060; break;
        default: break;
        }
        if (ev) { out->push_back(ev->text()); out->push_back(alt->text()); }
    }
}

// The people standing in a scene (the Character objects' Description).  Props that are modelled as characters (cars, a beach ball, a
// tumbleweed, jellyfish, a snake wearing a helmet) and SpongeBob himself are not listed.
inline void companionWho(const sb::scene::Scene& s, std::vector<std::string>* out)
{
    for (size_t i = 0; i < s.elements.size(); ++i) {
        const sb::scene::Record& r = s.elements[i];
        if (r.tag != sb::scene::CharacterTag) continue;
        std::string name = r.data.character.o0120.text(), mdl = r.data.character.o01e4.text();
        for (size_t k = 0; k < mdl.size(); ++k) mdl[k] = (char)tolower((unsigned char)mdl[k]);
        if (name.empty() || strcasecmp(name.c_str(), "SpongeBob") == 0) continue;
        if (mdl == "iboat1" || mdl == "boat1" || mdl == "beachball" || mdl == "tumweed" || mdl == "snakey" || mdl.compare(0, 5, "jelly") == 0) continue;
        if (std::find(out->begin(), out->end(), name) == out->end()) out->push_back(name);
    }
}

// A look-only layout of one place's rooms: x = how many doors from the first room, y = spread over the rooms at that depth (0..100).
// `links` are symmetric.  Rooms must be filled in (scene number order) with their links; x, y are set here.
inline void companionLayoutRooms(std::vector<CompanionState::Room>* rooms)
{
    size_t n = rooms->size();
    std::vector<int> depth(n, -1);
    int maxd = 0;
    std::vector<size_t> q;
    for (size_t root = 0; root < n; ++root) {
        if (depth[root] >= 0) continue;
        depth[root] = 0;
        q.assign(1, root);
        for (size_t h = 0; h < q.size(); ++h) {
            const CompanionState::Room& r = (*rooms)[q[h]];
            for (size_t k = 0; k < r.links.size(); ++k) {
                size_t t = (size_t)r.links[k];
                if (depth[t] >= 0) continue;
                depth[t] = depth[q[h]] + 1;
                if (depth[t] > maxd) maxd = depth[t];
                q.push_back(t);
            }
        }
    }
    for (int d = 0; d <= maxd; ++d) {
        std::vector<size_t> col;
        for (size_t i = 0; i < n; ++i) if (depth[i] == d) col.push_back(i);
        for (size_t k = 0; k < col.size(); ++k) {
            CompanionState::Room& r = (*rooms)[col[k]];
            r.x = maxd == 0 ? 50 : 10 + 80 * d / maxd;
            r.y = col.size() == 1 ? 50 : 12 + 76 * (int)k / (int)(col.size() - 1);
        }
    }
}

// The rooms of one place from its scenes (scene number -> parsed file), links only inside the place.  `names[i]` is the room's display name.
inline void companionBuildRooms(int ch, int lv, const std::vector<int>& sceneNos, const std::vector<sb::scene::Scene>& scenes,
                                const std::vector<std::string>& names, std::vector<CompanionState::Room>* out)
{
    out->clear();
    for (size_t i = 0; i < sceneNos.size(); ++i) {
        CompanionState::Room r;
        char b[16];
        snprintf(b, sizeof b, "c%dl%ds%d", ch, lv, sceneNos[i]);
        r.scene = b;
        r.name = names[i];
        out->push_back(r);
    }
    for (size_t i = 0; i < scenes.size(); ++i) {
        std::vector<std::string> refs;
        companionSceneRefs(scenes[i], &refs);
        for (size_t k = 0; k < refs.size(); ++k) {
            int c, l, s;
            if (!companionRefParts(refs[k], &c, &l, &s) || c != ch || l != lv || s == sceneNos[i]) continue;
            for (size_t j = 0; j < sceneNos.size(); ++j) {
                if (sceneNos[j] != s) continue;
                if (std::find((*out)[i].links.begin(), (*out)[i].links.end(), (int)j) == (*out)[i].links.end()) (*out)[i].links.push_back((int)j);
                if (std::find((*out)[j].links.begin(), (*out)[j].links.end(), (int)i) == (*out)[j].links.end()) (*out)[j].links.push_back((int)i);
            }
        }
    }
    for (size_t i = 0; i < out->size(); ++i) std::sort((*out)[i].links.begin(), (*out)[i].links.end());
    companionLayoutRooms(out);
}

// The text of a help line the way the HINT button reads it (Hud::hintText: an id in the text is another line, quotes are dropped).
// `generic` = the line is one of the chapter-0 lines every chapter shares ("Talk to everyone!"): not tied to a place.
inline std::string companionHelpText(const sbscript::HelpTable& h, const sbscript::HelpEntry& e, bool* generic)
{
    std::string t = e.text;
    const sbscript::HelpEntry* alias = h.find(t);
    *generic = e.id.size() > 1 && e.id[1] == '0';
    if (alias) { t = alias->text; *generic = alias->id.size() > 1 && alias->id[1] == '0'; }
    if (t.size() >= 2 && t[0] == '"' && t[t.size() - 1] == '"') t = t.substr(1, t.size() - 2);
    return t;
}

// The level a help line belongs to: F<chapter><level><scene><letter> ("F126A": the Krusty Krab, scene 6).  -1: not a place line.
inline int companionHelpLevel(const sbscript::HelpEntry& e)
{
    return e.id.size() >= 4 && isdigit((unsigned char)e.id[2]) ? e.id[2] - '0' : -1;
}

// ---- JSON of the v2 map fields (Companion.cpp calls these)
inline void companionAreaExtraJson(const CompanionState::MapArea& m, std::string& o)     // appended inside the area's object
{
    if (m.goal) o += ",\"goal\":1";
    if (!m.who.empty()) { o += ",\"who\":"; cj::arr(o, m.who); }
    if (!m.pic.empty()) { o += ",\"pic\":"; cj::esc(o, m.pic); }
    if (!m.hint.empty()) { o += ",\"hint\":"; cj::esc(o, m.hint); }
}

inline void companionMapExtraJson(const CompanionState& s, std::string& o)            // after "map" / "mapbusy"
{
    if (!s.mapBg.empty()) { o += ",\"mapbg\":"; cj::esc(o, s.mapBg); }
    if (!s.mapMe.empty()) { o += ",\"mapme\":"; cj::esc(o, s.mapMe); }
    if (!s.mapSnd.empty()) { o += ",\"mapsnd\":"; cj::esc(o, s.mapSnd); }
    if (!s.rooms.empty()) {
        o += ",\"rooms\":[";
        for (size_t i = 0; i < s.rooms.size(); ++i) {
            const CompanionState::Room& r = s.rooms[i];
            char b[48];
            if (i) o += ',';
            o += "{\"n\":"; cj::esc(o, r.name);
            o += ",\"s\":"; cj::esc(o, r.scene);
            snprintf(b, sizeof b, ",\"here\":%d,\"x\":%d,\"y\":%d,\"l\":[", r.here ? 1 : 0, r.x, r.y);
            o += b;
            for (size_t k = 0; k < r.links.size(); ++k) { snprintf(b, sizeof b, k ? ",%d" : "%d", r.links[k]); o += b; }
            o += "]}";
        }
        o += ']';
    }
}

}  // namespace sbport
