// sb_unittest: pure-logic checks that need no game files (CI runs this): the script command table, the
// tab-delimited record tokenizer (ScriptRunner::parseRecords) and loadText on a synthetic script string.
//   sb_unittest            exit 0 = all passed
#include "../../game/src/script/ScriptRunner.h"
#include "../../game/src/script/ScriptHost.h"
#include <stdio.h>
#include <string>
#include <vector>

using namespace sbscript;

static int g_fail = 0, g_run = 0;
#define CHECK(c) do { ++g_run; if (!(c)) { ++g_fail; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main()
{
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

    printf("sb_unittest: %d checks, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
