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
#include "../../game/src/game/WalkRoute.h"

using namespace sbscript;

static int g_fail = 0, g_run = 0;
#include "../../game/src/game/BankPolicy.h"
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

    printf("sb_unittest: %d checks, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
