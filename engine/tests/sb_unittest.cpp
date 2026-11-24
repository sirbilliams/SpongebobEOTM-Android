// sb_unittest: pure-logic checks that need no game files (CI runs this): the script command table, the
// tab-delimited record tokenizer (ScriptRunner::parseRecords) and loadText on a synthetic script string.
//   sb_unittest            exit 0 = all passed
#include "../../game/src/script/ScriptRunner.h"
#include "../../game/src/script/ScriptHost.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../game/src/script/Dialogue.h"
#include <cstddef>
#include "../../game/src/port/CompanionMap.h"
#include <sys/stat.h>
#include "../../game/src/game/WalkRoute.h"

using namespace sbscript;

static int g_fail = 0, g_run = 0;
#include "../../game/src/game/BankPolicy.h"
#include "../../game/src/game/Depth.h"
#include "../../game/src/game/GameState.h"
#include "../../game/src/game/ItemVisibility.h"
#define CHECK(c) do { ++g_run; if (!(c)) { ++g_fail; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static bool near(double a, double b) { return std::fabs(a-b) < 1e-8; }
static void walkChecks()
{
    using namespace sbwalk;
    const double pi = 3.14159265358979323846;
    CHECK(sizeof(Node) == 0x18);
    CHECK(sizeof(Piece) == 0x48 && offsetof(Piece,cx) == 8 && offsetof(Piece,clockwise) == 0x18 &&
          offsetof(Piece,x) == 0x20 && offsetof(Piece,angle) == 0x30 && offsetof(Piece,length) == 0x40);
    Node start(0.5,0.5,0), corner(1.5,0.5), end(1.5,1.5);
    Path leg = straight(start,corner);
    CHECK(near(leg.length(),1) && near(leg.at(0).x,0.5) && near(leg.at(0.5).x,1));
    CHECK(near(leg.at(1).x,1.5) && near(leg.at(1).z,0.5));
    CHECK(turnToPoint(start,corner,0.1,leg));
    corner.heading = leg.end.heading;
    CHECK(near(corner.heading,0) && near(leg.length(),1));
    Path turn;
    CHECK(turnToPoint(corner,end,0.1,turn));
    CHECK(turn.pieces[0].arc && !turn.pieces[0].clockwise && !turn.pieces[1].arc);
    CHECK(near(turn.at(0).x,corner.x) && near(turn.at(0).z,corner.z));
    bool inside = true, onCircle = true;
    for (int i = 0; i <= 100; ++i) {
        Node p = turn.at(turn.pieces[0].length*i/100);
        inside = inside && p.x >= 1 && p.x <= 2 && p.z >= 0 && p.z <= 1;
        double dx = p.x-turn.pieces[0].cx, dz = p.z-turn.pieces[0].cz;
        onCircle = onCircle && near(dx*dx+dz*dz,0.01);
    }
    CHECK(inside && onCircle); // the L's turning arc stays in its corner cell
    CHECK(near(turn.at(turn.length()).x,end.x) && near(turn.at(turn.length()).z,end.z));
    CHECK(near(turn.at(turn.length()+1).x,end.x));
    Node join = turn.at(turn.pieces[0].length);
    CHECK(near(join.x,turn.pieces[1].x) && near(join.z,turn.pieces[1].z));
    Node after = turn.at(turn.pieces[0].length+1e-7);
    CHECK(std::fabs(after.x-join.x) < 1e-6 && std::fabs(after.z-join.z) < 1e-6);

    // Closed-form symmetric two-arc tangent: two 45-degree turns and a diagonal.
    CHECK(turnToHeading(Node(0.5,0.5,0),Node(1.5,1.5,pi/2),0.1,turn));
    CHECK(near(turn.pieces[0].sweep,pi/4) && near(turn.pieces[2].sweep,pi/4));
    CHECK(near(turn.pieces[1].length,std::sqrt(2.0)*0.9));
    CHECK(near(turn.length(),std::sqrt(2.0)*0.9+pi*0.05));
    Node b = turn.at(turn.pieces[0].length+turn.pieces[1].length);
    Node c = turn.at(turn.pieces[0].length+turn.pieces[1].length+1e-7);
    CHECK(std::fabs(b.x-c.x) < 1e-6 && std::fabs(b.z-c.z) < 1e-6);
    CHECK(near(turn.at(turn.length()).heading,pi/2));
    // Opposite-turn and mirrored routes: exercise the internal tangent branches.
    bool continuous = true;
    for (int h = 0; h < 8; ++h) {
        Path p;
        CHECK(turnToHeading(Node(3.5,3.5,h*pi/4),Node(4.5,4.5,(7-h)*pi/4),0.1,p));
        for (int k = 0; k < 2; ++k) {
            double d = p.pieces[0].length+(k ? p.pieces[1].length : 0);
            Node q = p.at(d), r = p.at(d+1e-7);
            continuous = continuous && std::fabs(q.x-r.x) < 1e-6 && std::fabs(q.z-r.z) < 1e-6;
        }
        Node q = p.at(p.length()-1e-7);
        continuous = continuous && std::fabs(q.x-4.5) < 1e-6 && std::fabs(q.z-4.5) < 1e-6;
    }
    CHECK(continuous);
    Grid grid;
    CHECK(clear(turn,grid));
    grid.blocked[1][1] = true;
    CHECK(!clear(turn,grid));
    grid.blocked[1][1] = false;
    // Clearance corners detect a neighbour even when the centre remains open.
    grid.blocked[1][0] = true;
    CHECK(!clear(straight(Node(0.98,0.5),Node(0.98,0.9)),grid));
    CHECK(clear(straight(Node(-2,0.5),Node(-1,0.5)),grid));
    grid.allowOutside = false;
    CHECK(!clear(straight(Node(-2,0.5),Node(-1,0.5)),grid));
    grid = Grid();
    std::vector<Node> nodes;
    nodes.push_back(corner); nodes.push_back(end);
    round(nodes,start,grid);
    CHECK(nodes.size() == 1 && near(nodes[0].x,end.x) && near(nodes[0].z,end.z) && nodes[0].heading >= 0);
    // A longer L with a blocked diagonal retains its corner and propagates headings.
    nodes.clear(); nodes.push_back(Node(3.5,0.5)); nodes.push_back(Node(3.5,3.5));
    grid.blocked[2][1] = true;
    round(nodes,start,grid);
    CHECK(nodes.size() == 2 && near(nodes[0].x,3.5) && near(nodes[0].z,0.5));
    CHECK(nodes[0].heading >= 0 && nodes[1].heading >= 0);
    nodes.clear(); round(nodes,start,grid); CHECK(nodes.empty());
    CHECK(!turnToPoint(start,end,0,turn));
}


// companion map page v2 (CompanionMap.h): the JSON of the new fields on a hand-made state, then (when an install is found: $SB_GAME, ./.game,
// ../.game) the scene-derived parts against real scenes: doors of the Krusty Krab, who stands in Downtown, the game's help lines.
static bool readGameScene(const std::string& root, const char* name, sb::scene::Scene* sc)
{
    std::string err;
    return sb::scene::load(root + "/scenes/" + name + ".scn", *sc, err);
}

static void mapChecks()
{
    using namespace sbport;
    {   // JSON of the v2 fields (the pieces CompanionJsonOf appends; Companion.cpp needs SDL, so it is not linked here); a state without them writes none
        CompanionState s;
        CompanionState::MapArea a; a.name = "Downtown"; a.target = "c1l6s1p1"; a.ok = true;
        s.map.push_back(a);
        std::string j0, a0;
        companionMapExtraJson(s, j0); companionAreaExtraJson(s.map[0], a0);
        CHECK(j0.empty() && a0.empty());
        s.map[0].goal = true; s.map[0].who.push_back("Patrick"); s.map[0].who.push_back("Marty the \"Bus\" Driver");
        s.map[0].pic = "/x/place_c1l6s1p1.png"; s.map[0].hint = "Talk to him.";
        s.mapBg = "/x/map_c1.png"; s.mapMe = "/x/map_me.png";
        CompanionState::Room r; r.name = "Kitchen"; r.scene = "c1l2s5"; r.x = 10; r.y = 50; r.here = true; r.links.push_back(1);
        CompanionState::Room r2; r2.name = "Dish Pit"; r2.scene = "c1l2s6"; r2.x = 90; r2.y = 50; r2.links.push_back(0);
        s.rooms.push_back(r); s.rooms.push_back(r2);
        std::string j, aj;
        companionMapExtraJson(s, j); companionAreaExtraJson(s.map[0], aj);
        CHECK(aj == ",\"goal\":1,\"who\":[\"Patrick\",\"Marty the \\\"Bus\\\" Driver\"],\"pic\":\"/x/place_c1l6s1p1.png\",\"hint\":\"Talk to him.\"");
        CHECK(j == ",\"mapbg\":\"/x/map_c1.png\",\"mapme\":\"/x/map_me.png\",\"rooms\":[{\"n\":\"Kitchen\",\"s\":\"c1l2s5\",\"here\":1,\"x\":10,\"y\":50,\"l\":[1]},{\"n\":\"Dish Pit\",\"s\":\"c1l2s6\",\"here\":0,\"x\":90,\"y\":50,\"l\":[0]}]");
    }
    {   // refs and layout without any file
        int c, l, sn;
        CHECK(companionRefParts("c1l6s1p1", &c, &l, &sn) && c == 1 && l == 6 && sn == 1);
        CHECK(companionRefParts("C3L6S0P1", &c, &l, &sn) && c == 3 && l == 6 && sn == 0);
        CHECK(!companionRefParts("h126d", &c, &l, &sn) && !companionRefParts("swindow", &c, &l, &sn) && !companionRefParts("", &c, &l, &sn));
        std::vector<CompanionState::Room> rs(4);        // a chain 0-1-2 and a fork 1-3
        int lk[4][2] = { {1,-1}, {0,2}, {1,-1}, {1,-1} };
        for (int i = 0; i < 4; ++i) for (int k = 0; k < 2; ++k) if (lk[i][k] >= 0) rs[i].links.push_back(lk[i][k]);
        rs[1].links.push_back(3);
        companionLayoutRooms(&rs);
        CHECK(rs[0].x == 10 && rs[1].x == 50 && rs[2].x == 90 && rs[3].x == 90 && rs[0].y == 50 && rs[2].y == 12 && rs[3].y == 88);
    }
    std::string root;
    const char* e = getenv("SB_GAME");
    const char* cand[3] = { e ? e : "", ".game", "../.game" };
    for (int i = 0; i < 3 && root.empty(); ++i) {
        struct stat st;
        if (cand[i][0] && stat((std::string(cand[i]) + "/scenes/c1l2s6.scn").c_str(), &st) == 0) root = cand[i];
    }
    if (root.empty()) { printf("sb_unittest: map checks on real scenes skipped (no game install found; set SB_GAME)\n"); return; }
    {   // the Krusty Krab (c1l2): scene 6 (the outside) has a door to scene 1; every link stays inside the place; the layout is inside 0..100
        std::vector<int> nos; std::vector<sb::scene::Scene> scs; std::vector<std::string> names;
        for (int n = 1; n <= 7; ++n) { sb::scene::Scene sc; if (readGameScene(root, ("c1l2s" + std::string(1, (char)('0' + n))).c_str(), &sc)) { nos.push_back(n); scs.push_back(sc); names.push_back("R"); } }
        CHECK(nos.size() >= 6);
        std::vector<std::string> refs;
        companionSceneRefs(scs[nos.size() > 5 ? 5 : 0], &refs);       // c1l2s6
        CHECK(std::find(refs.begin(), refs.end(), "c1l2s1p1") != refs.end() && std::find(refs.begin(), refs.end(), "c1l1s1p1") != refs.end());
        std::vector<CompanionState::Room> rooms;
        companionBuildRooms(1, 2, nos, scs, names, &rooms);
        CHECK(rooms.size() == nos.size() && rooms[0].scene == "c1l2s1");
        bool sym = true, inside = true, linked = false;
        for (size_t i = 0; i < rooms.size(); ++i) {
            if (rooms[i].x < 0 || rooms[i].x > 100 || rooms[i].y < 0 || rooms[i].y > 100) inside = false;
            for (size_t k = 0; k < rooms[i].links.size(); ++k) {
                size_t t = (size_t)rooms[i].links[k];
                if (t >= rooms.size() || t == i || std::find(rooms[t].links.begin(), rooms[t].links.end(), (int)i) == rooms[t].links.end()) sym = false;
                else linked = true;
            }
        }
        CHECK(sym && inside && linked);
        size_t s6 = 0; for (size_t i = 0; i < rooms.size(); ++i) if (rooms[i].scene == "c1l2s6") s6 = i;
        CHECK(std::find(rooms[s6].links.begin(), rooms[s6].links.end(), 0) != rooms[s6].links.end());      // outside <-> dining room
    }
    {   // who stands in Downtown (c1l6): SpongeBob and props are left out, names are not repeated
        std::vector<std::string> who;
        const char* sceneNames[3] = { "c1l6s1", "c1l6s3", "c1l6s4" };
        for (int i = 0; i < 3; ++i) { sb::scene::Scene sc; CHECK(readGameScene(root, sceneNames[i], &sc)); companionWho(sc, &who); }
        CHECK(who.size() == 4 && who[0] == "Patrick" && who[1] == "Marty the Bus Driver" && who[2] == "Shop Owner" && who[3] == "Customer");
        std::vector<std::string> none;
        sb::scene::Scene k; CHECK(readGameScene(root, "c1l2s6", &k)); companionWho(k, &none);
        CHECK(none.empty());                                                  // only SpongeBob in the Krusty Krab outside
        sb::scene::Scene car; CHECK(readGameScene(root, "c1l3s6", &car)); companionWho(car, &none);
        CHECK(none.empty());                                                  // a car modelled as a character is not a person
    }
    {   // the game's help lines: level from the id, aliases resolved, chapter-0 lines are "generic"
        struct Disk : sbscript::FileSource {
            std::string root;
            bool readFile(const char* p, std::vector<unsigned char>& out) {
                FILE* f = fopen((root + "/" + p).c_str(), "rb");
                if (!f) return false;
                unsigned char b[4096]; size_t n;
                while ((n = fread(b, 1, sizeof b, f)) > 0) out.insert(out.end(), b, b + n);
                fclose(f); return true;
            }
        } disk;
        disk.root = root;
        sbscript::HelpTable h; std::string err;
        CHECK(h.load(disk, 1, &err));
        const sbscript::HelpEntry* a = h.find("F126A");       // "This is where SpongeBob's quest begins!"  (cond !c1l2s1)
        const sbscript::HelpEntry* b = h.find("F121A");       // an alias of the shared F000D
        const sbscript::HelpEntry* c = h.find("F122A");       // an alias of F123A (the salad bar)
        CHECK(a && b && c);
        if (a && b && c) {
            bool gen = true;
            CHECK(companionHelpLevel(*a) == 2 && companionHelpLevel(*b) == 2);
            CHECK(companionHelpText(h, *a, &gen).compare(0, 28, "This is where SpongeBob's qu") == 0 && !gen);
            std::string t = companionHelpText(h, *b, &gen);
            CHECK(gen && t.compare(0, 8, "Remember") == 0);
            t = companionHelpText(h, *c, &gen);
            CHECK(!gen && t.compare(0, 15, "The Salad Bar! ") == 0);
        }
        // coverage at the map's places (static: any non-shared line of the place's level): Krusty Krab, Grandma's, Barg-N-Mart, Goo Lagoon, Downtown
        int covered = 0;
        const int lv[5] = { 2, 3, 4, 5, 6 };
        for (int i = 0; i < 5; ++i) {
            bool any = false;
            for (size_t k = 0; k < h.entries().size(); ++k) {
                bool gen2 = false;
                if (companionHelpLevel(h.entries()[k]) == lv[i]) { companionHelpText(h, h.entries()[k], &gen2); if (!gen2) any = true; }
            }
            covered += any;
        }
        CHECK(covered == 5);
    }
}

int main()
{
    walkChecks();
    // command table
    CHECK(ScriptRunner::commandCount() > 0);
    CHECK(ScriptRunner::isKnownCommand("control"));
    CHECK(!ScriptRunner::isKnownCommand("no_such_command"));
    CHECK(!ScriptRunner::isKnownCommand(""));
    for (unsigned i = 0; i < ScriptRunner::commandCount(); ++i) CHECK(ScriptRunner::isKnownCommand(ScriptRunner::commandNames()[i]));

    // tokenizer: up to four fields per record, a blank record still takes a slot
    std::vector<std::string> rec;
    rec.push_back("control\ton");
    rec.push_back("");
    rec.push_back("a\tb\tc\td");
    rec.push_back(std::string(40, 'x') + "\ty");
    std::vector<ScriptLine> out;
    std::string err;
    CHECK(ScriptRunner::parseRecords(rec, out, &err));
    CHECK(out[0].cmd == "control" && out[0].a1 == "on" && out[0].a2.empty());
    CHECK(out[1].cmd.empty() && out[1].a1.empty());
    CHECK(out[2].cmd == "a" && out[2].a1 == "b" && out[2].a2 == "c" && out[2].a3 == "d");
    CHECK(out[3].cmd.size() == 31 && out[3].a1 == "y");      // fields are cut to 31 characters

    // more than kMaxLines records are rejected, exactly kMaxLines are accepted
    std::vector<std::string> many((size_t)ScriptRunner::kMaxLines, "wait");
    CHECK(ScriptRunner::parseRecords(many, out, &err));
    many.push_back("wait");
    err.clear();
    CHECK(!ScriptRunner::parseRecords(many, out, &err) && !err.empty());

    // loadText on a synthetic script against the silent logging host
    LoggingScriptHost host;
    host.setOutput(0);
    ScriptRunner run(&host);
    run.setAutoRun(false);
    CHECK(run.loadText("synthetic", 0, "control\ton\nwait\t1\ncontrol\toff\n"));
    CHECK(run.lineCount() == 3);
    CHECK(run.line(0).cmd == "control" && run.line(2).a1 == "off");
    CHECK(run.name() == "synthetic");

    // chapter sound banks: reload on a chapter change (previous vs new), or when none is open
    CHECK(sharedBankNeedsReload(1, 2, true));
    CHECK(!sharedBankNeedsReload(2, 2, true));
    CHECK(sharedBankNeedsReload(2, 2, false));

    // dialogue voice lookup (0x412d00): eight spellings of the key, all lower case first, all upper case second
    // (SpongeBob's lines of the jewelry shop, c1l6s3, are stored as "163AAA")
    {
        unsigned id = sbscript::sceneIdPack(1, 6, 3);
        CHECK(sbscript::DialogueTable::voiceCandidate(id, "aaa", 0) == "163aaa");
        CHECK(sbscript::DialogueTable::voiceCandidate(id, "aaa", 1) == "163AAA");
        CHECK(sbscript::DialogueTable::voiceCandidate(id, "BaC", 2) == "163BaC");
        CHECK(sbscript::DialogueTable::voiceCandidate(id, "bac", 7) == "163baC");
        CHECK(sbscript::DialogueTable::voiceCandidate(id, "bac", 8).empty());
        std::vector<std::string> seen;
        for (int c = 0; c < sbscript::DialogueTable::kVoiceTries; ++c) seen.push_back(sbscript::DialogueTable::voiceCandidate(id, "abc", c));
        std::sort(seen.begin(), seen.end());
        CHECK(std::unique(seen.begin(), seen.end()) == seen.end());      // eight different spellings
    }
    // layering of 2D objects and 3D characters (c1l6s3: the register / counter cutouts cover the clerk, SpongeBob stands before the pz 0 one)
    CHECK(sbgame::objectCoversActor(false, -440.0f, -431.522f));    // 163cutout2 over dude1
    CHECK(!sbgame::objectCoversActor(false, -440.0f, -713.205f));   // ... but not over dude3, nearer than it
    CHECK(!sbgame::objectCoversActor(false, 0.0f, -639.627f));      // 163cutout1 (pz 0) behind SpongeBob
    CHECK(sbgame::objectCoversActor(false, -1500.0f, -639.0f));     // a far cutout covers everyone
    CHECK(!sbgame::objectCoversActor(false, 500.0f, -100.0f));      // a hole (positive pz) lies behind the characters
    CHECK(sbgame::objectCoversActor(true, 0.0f, -5000.0f));         // Post Render: always on top

    // Scene entry and script visibility refresh must respect saved collection
    // history, even after the item has left the HUD inventory.
    {
        sbgame::GameState progress;
        auto evaluate = [&progress](const char* c) { return sbscript::evalCondition(c, false, progress); };
        CHECK(!sbgame::actorHidden(true, "ikp4", "", false, progress, evaluate));
        CHECK(!sbgame::actorHidden(true, "ikp4", "0", false, progress, evaluate));
        CHECK(sbgame::actorHidden(true, "ikp4", "zero", false, progress, evaluate));
        progress.giveItem("IKP4");
        CHECK(sbgame::actorHidden(true, "ikp4", "", false, progress, evaluate)); // re-enter
        CHECK(sbgame::actorHidden(true, "ikp4", "0", false, progress, evaluate)); // true condition
        CHECK(sbgame::actorHidden(true, "ikp4", "zero", false, progress, evaluate));
        progress.bar().clear(); // consumed/combined items remain in collection history
        CHECK(sbgame::actorHidden(true, "ikp4", "", false, progress, evaluate));
        CHECK(!sbgame::actorHidden(true, "ikp1", "", false, progress, evaluate));
        CHECK(!sbgame::actorHidden(false, "ikp4", "0", true, progress, evaluate)); // scenery/character/script pickup
        CHECK(sbgame::actorHidden(false, "ikp4", "", true, progress, evaluate)); // explicit hide
        CHECK(!sbgame::actorHidden(false, "ikp4", "", false, progress, evaluate));
        progress.clearProgress(); // new game / loading a different save
        CHECK(!sbgame::actorHidden(true, "ikp4", "", false, progress, evaluate));
    }
    mapChecks();
    printf("sb_unittest: %d checks, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
