// sb_scripttest: parses every .sb2 script and dialogue table of the installed game, reports unknown
// commands and parse errors, smoke-runs every script against the logging host and prints the command
// trace of one script (default scripts/c1/sload1, the first scene's script).
//   sb_scripttest <game folder> [steps=40] [script=c1/sload1]
#include "../../game/src/script/ScriptRunner.h"
#include "../../game/src/script/Dialogue.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace sbscript;

static std::vector<std::string> listDir(const std::string& dir, const char* ext)
{
    std::vector<std::string> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    while (struct dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() > strlen(ext) && n.compare(n.size() - strlen(ext), strlen(ext), ext) == 0) out.push_back(n);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <game folder> [steps] [script]\n", argv[0]); return 2; }
    std::string root = argv[1];
    int steps = argc > 2 ? atoi(argv[2]) : 40;
    std::string pick = argc > 3 ? argv[3] : "c1/sload1";
    int problems = 0;

    LoggingScriptHost host;
    host.setRoot(root);
    host.setOutput(0);

    // ---- scripts ----
    unsigned nScripts = 0, nLines = 0, nUnknown = 0, nParseErr = 0;
    std::map<std::string, unsigned> unknown;
    std::map<std::string, unsigned> used;
    for (int c = 1; c <= 4; ++c) {
        char dir[512];
        snprintf(dir, sizeof dir, "%s/scripts/c%d", root.c_str(), c);
        std::vector<std::string> files = listDir(dir, ".sb2");
        for (size_t f = 0; f < files.size(); ++f) {
            char rel[256];
            snprintf(rel, sizeof rel, "scripts/c%d/%s", c, files[f].c_str());
            std::vector<unsigned char> data;
            std::vector<std::string> records;
            std::string err;
            ++nScripts;
            if (!host.readFile(rel, data) || !sb2_decode(data.empty() ? (const unsigned char*)"" : &data[0], (unsigned)data.size(), records, err)) {
                printf("PARSE ERROR %s: %s\n", rel, err.c_str());
                ++nParseErr;
                continue;
            }
            std::vector<ScriptLine> lines;
            if (!ScriptRunner::parseRecords(records, lines, &err)) {
                printf("PARSE ERROR %s: %s\n", rel, err.c_str());
                ++nParseErr;
                continue;
            }
            for (size_t i = 0; i < records.size(); ++i) {
                ++nLines;
                ++used[lines[i].cmd];
                if (!ScriptRunner::isKnownCommand(lines[i].cmd)) {
                    ++nUnknown;
                    ++unknown[lines[i].cmd];
                    printf("note: command '%s' in %s line %u is not in the dispatcher (the exe falls through its chain: the line does nothing)\n", lines[i].cmd.c_str(), rel, (unsigned)i);
                }
            }
            // smoke run: load, then wake the script after every wait until it ends or loops 3000 steps
            LoggingScriptHost h2;
            h2.setRoot(root);
            h2.setChapter(c);
            h2.setOutput(0);
            h2.flags.push_back("sjukebox");
            ScriptRunner r(&h2);
            r.setAutoRun(false);
            std::string e2;
            if (!r.loadBytes(files[f].substr(0, files[f].size() - 4).c_str(), 0, &data[0], (unsigned)data.size(), &e2)) {
                printf("LOAD ERROR %s: %s\n", rel, e2.c_str());
                ++problems;
                continue;
            }
            while (!r.finished() && r.stepCount() < 3000) r.step();
        }
    }
    printf("scripts: %u files, %u lines, %u distinct commands used (dispatcher knows %u), lines the dispatcher does not know %u, parse errors %u\n",
           nScripts, nLines, (unsigned)used.size(), ScriptRunner::commandCount(), nUnknown, nParseErr);
    unsigned unused = 0;
    for (unsigned i = 0; i < ScriptRunner::commandCount(); ++i)
        if (!used.count(ScriptRunner::commandNames()[i])) { printf("  command never used by the data: %s\n", ScriptRunner::commandNames()[i]); ++unused; }

    // ---- dialogue ----
    unsigned nDlgFiles = 0, nRecords = 0, nLinesKept = 0, nGroups = 0, dlgErr = 0;
    {
        std::vector<std::string> files = listDir(root + "/dialogue", ".sb2");
        for (size_t f = 0; f < files.size(); ++f) {
            std::string n = files[f];
            ++nDlgFiles;
            std::vector<unsigned char> data;
            std::vector<std::string> recs;
            std::string err;
            host.readFile(("dialogue/" + n).c_str(), data);
            if (!sb2_decode(data.empty() ? (const unsigned char*)"" : &data[0], (unsigned)data.size(), recs, err)) {
                printf("DIALOGUE ERROR %s: %s\n", n.c_str(), err.c_str());
                ++dlgErr;
                continue;
            }
            nRecords += (unsigned)recs.size();
            int c, l, s;
            if (sscanf(n.c_str(), "c%dl%ds%d.sb2", &c, &l, &s) == 3) {
                DialogueTable t;
                if (!t.load(host, sceneIdPack(c, l, s), 0, &err)) { printf("DIALOGUE ERROR %s: %s\n", n.c_str(), err.c_str()); ++dlgErr; continue; }
                nLinesKept += t.lineCount();
                nGroups += (unsigned)t.groups().size();
            }
        }
        // comments for every chapter/level, help for every chapter
        unsigned comments = 0, help = 0;
        for (int c = 0; c <= 5; ++c) {
            HelpTable h;
            std::string err;
            if (h.load(host, c, &err)) help += (unsigned)h.entries().size();
            for (int l = 0; l <= 7; ++l) {
                CommentTable ct;
                if (ct.load(host, c, l, &err)) comments += (unsigned)ct.entries().size();
            }
        }
        printf("dialogue: %u files, %u records (scene tables keep %u lines in %u groups; comments entries %u; help entries %u), errors %u\n",
               nDlgFiles, nRecords, nLinesKept, nGroups, comments, help, dlgErr);
    }

    // ---- trace ----
    {
        LoggingScriptHost h3;
        h3.setRoot(root);
        h3.setChapter(pick.size() > 1 && pick[0] == 'c' ? atoi(pick.c_str() + 1) : 1);
        h3.setOutput(stdout);
        h3.walkBlocks = true;
        std::string path = "scripts/" + pick + ".sb2";
        std::vector<unsigned char> data;
        if (!h3.readFile(path.c_str(), data)) { printf("cannot read %s\n", path.c_str()); return 1; }
        ScriptRunner r(&h3);
        r.setAutoRun(false);
        r.setTrace(stdout);
        std::string err;
        printf("--- trace of %s (%d steps; a waiting line is woken straight away) ---\n", pick.c_str(), steps);
        std::string nm = pick.substr(pick.find('/') + 1);
        if (!r.loadBytes(nm.c_str(), 0, &data[0], (unsigned)data.size(), &err)) { printf("load failed: %s\n", err.c_str()); return 1; }
        for (int i = 0; i < steps && !r.finished(); ++i) {
            bool waits = r.step();
            if (waits) printf("      -> waits\n");
        }
        printf("--- %s after %u steps ---\n", r.finished() ? "script finished" : "stopped", r.stepCount());
    }

    problems += (int)(nParseErr + dlgErr);   // unknown commands are only reported: the exe ignores them
    printf("%s\n", problems ? "RESULT: problems found" : "RESULT: OK");
    return problems ? 1 : 0;
}
