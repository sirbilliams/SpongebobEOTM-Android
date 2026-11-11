// sb_voicetest: resolves the voice Wave of every dialogue line of the install the way DialogueUI::startVoice does
// (0x412d00: eight upper / lower case spellings of the key in the scene's sounds/c<n>l<n>s<n>.omt) and fails if a line
// has a recording in its bank that the lookup cannot find.  Counts the old seven-spelling lookup for comparison.
//   sb_voicetest <game folder>
#include "../../game/src/script/Dialogue.h"
#include "OMediaDataBase.h"
#include "OMediaFilePath.h"
#include "OMediaFileStream.h"
#include "OMediaSound.h"
#include "OMediaError.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <set>
#include <string>
#include <vector>

using namespace sbscript;

struct Bank { std::string name; OMediaDataBase* db; };

class DiskFiles : public FileSource {
public:
    explicit DiskFiles(const std::string& r) : root_(r) {}
    std::string root_;
    virtual bool readFile(const char* path, std::vector<unsigned char>& out) {
        FILE* f = fopen((root_ + "/" + path).c_str(), "rb");
        if (!f) return false;
        unsigned char b[4096]; size_t n;
        out.clear();
        while ((n = fread(b, 1, sizeof b, f)) > 0) out.insert(out.end(), b, b + n);
        fclose(f);
        return true;
    }
};

static std::string oldSpelling(unsigned id, const std::string& key, int c)   // the port's lookup before the fix: no all-upper spelling
{
    static const int combos[7][3] = { {0,0,0},{1,0,1},{1,1,0},{0,1,1},{0,1,0},{1,0,0},{0,0,1} };
    char n[16];
    sprintf(n, "%d%d%d%c%c%c", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf),
            combos[c][0] ? toupper((unsigned char)key[0]) : tolower((unsigned char)key[0]),
            combos[c][1] ? toupper((unsigned char)key[1]) : tolower((unsigned char)key[1]),
            combos[c][2] ? toupper((unsigned char)key[2]) : tolower((unsigned char)key[2]));
    return n;
}

extern int omt_argc;
extern char** omt_argv;

void omt_main(void)
{
    if (omt_argc < 2) { printf("usage: sb_voicetest <game folder>\n"); return; }
    std::string root = omt_argv[1];
    std::set<unsigned> scenes;
    if (DIR* d = opendir((root + "/dialogue").c_str())) {
        while (struct dirent* e = readdir(d)) {
            int c, l, s;
            if (sscanf(e->d_name, "c%dl%ds%d.sb2", &c, &l, &s) == 3) scenes.insert(sceneIdPack(c, l, s));
        }
        closedir(d);
    }
    DiskFiles fs(root);
    std::vector<Bank> banks;                                // every .omt under sounds/ and cd/sounds/
    const char* dirs[] = { "sounds", "cd/sounds" };
    for (int di = 0; di < 2; ++di)
        if (DIR* d = opendir((root + "/" + dirs[di]).c_str())) {
            while (struct dirent* e = readdir(d)) {
                size_t n = strlen(e->d_name);
                if (n < 5 || strcasecmp(e->d_name + n - 4, ".omt") != 0) continue;
                try {
                    OMediaFilePath* path = new OMediaFilePath(root + "/" + dirs[di] + "/" + e->d_name);
                    OMediaFileStream* st = new OMediaFileStream;
                    st->setpath(path);
                    st->open(omcfp_Read);
                    Bank b; b.name = std::string(dirs[di]) + "/" + e->d_name; b.db = new OMediaDataBase(st);
                    banks.push_back(b);
                } catch (OMediaError&) {}
            }
            closedir(d);
        }
    unsigned missing = 0, noBank = 0;
    unsigned lines = 0, voiced = 0, oldVoiced = 0;
    std::set<std::string> silentScenes;
    for (std::set<unsigned>::iterator it = scenes.begin(); it != scenes.end(); ++it) {
        unsigned id = *it;
        DialogueTable t;
        std::string err;
        if (!t.load(fs, id, 0, &err)) { printf("cannot load %s: %s\n", DialogueTable::fileName(id).c_str(), err.c_str()); continue; }
        OMediaDataBase* db = 0;
        try {
            OMediaFilePath* path = new OMediaFilePath(root + "/" + DialogueTable::soundBankName(id));
            OMediaFileStream* st = new OMediaFileStream;
            st->setpath(path);
            st->open(omcfp_Read);
            db = new OMediaDataBase(st);
        } catch (OMediaError&) { db = 0; }
        std::set<std::string> names;                       // every spelling that exists in the bank, upper-cased
        for (size_t g = 0; g < t.groups().size(); ++g)
            for (size_t l = 0; l < t.groups()[g].lines.size(); ++l) {
                const DialogueLine& ln = t.groups()[g].lines[l];
                if (ln.key.size() < 3) continue;
                ++lines;
                bool found = false, old = false;
                if (!db) { printf("NOVOICE c%dl%ds%d %s: no scene bank\n", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf), ln.key.c_str());
                    ++missing; ++noBank;
                    for (size_t b = 0; b < banks.size(); ++b)    // is the recording in some other bank?
                        for (int c = 0; c < DialogueTable::kVoiceTries; ++c)
                            if (banks[b].db->object_exists(OMediaSound::db_type, DialogueTable::voiceCandidate(id, ln.key, c))) { printf("  FOUND in %s\n", banks[b].name.c_str()); break; }
                    continue; }
                for (int c = 0; c < DialogueTable::kVoiceTries && !found; ++c)
                    found = db->object_exists(OMediaSound::db_type, DialogueTable::voiceCandidate(id, ln.key, c));
                for (int c = 0; c < 7 && !old; ++c)
                    old = db->object_exists(OMediaSound::db_type, oldSpelling(id, ln.key, c));
                if (!found) {                                 // list it, and look for the recording in every other bank of the install
                    ++missing;
                    std::string where;
                    for (size_t b = 0; b < banks.size(); ++b)
                        for (int c = 0; c < DialogueTable::kVoiceTries; ++c)
                            if (banks[b].db->object_exists(OMediaSound::db_type, DialogueTable::voiceCandidate(id, ln.key, c))) { where += " " + banks[b].name; break; }
                    printf("NOVOICE c%dl%ds%d %s bank=%s elsewhere=[%s] \"%s\"\n", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf),
                           ln.key.c_str(), db ? "yes" : "MISSING", where.c_str(), ln.text.substr(0, 40).c_str());
                }
                if (found) ++voiced;
                if (old) ++oldVoiced;
                if (found && !old) { char b[32]; sprintf(b, "c%dl%ds%d", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf)); silentScenes.insert(b); }
            }
    }
    printf("dialogue lines %u, voiced (0x412d00, 8 spellings) %u, voiced by the old 7-spelling lookup %u\n", lines, voiced, oldVoiced);
    printf("lines silenced by the old lookup: %u, in scenes:", voiced - oldVoiced);
    for (std::set<std::string>::iterator s = silentScenes.begin(); s != silentScenes.end(); ++s) printf(" %s", s->c_str());
    printf("\n");
    // c1l6s3 (the jewelry shop counter): SpongeBob's lines AAA, AAC, AAE, AAG, BAA, BAC, BAE are stored under all-upper Wave names
    int bad = 0;
    {
        unsigned id = sceneIdPack(1, 6, 3);
        const char* keys[] = { "AAA", "AAC", "AAE", "AAG", "BAA", "BAC", "BAE" };
        OMediaFilePath* path = new OMediaFilePath(root + "/" + DialogueTable::soundBankName(id));
        OMediaFileStream* st = new OMediaFileStream;
        st->setpath(path);
        st->open(omcfp_Read);
        OMediaDataBase* db = new OMediaDataBase(st);
        for (int k = 0; k < 7; ++k) {
            bool found = false;
            for (int c = 0; c < DialogueTable::kVoiceTries && !found; ++c)
                found = db->object_exists(OMediaSound::db_type, DialogueTable::voiceCandidate(id, keys[k], c));
            if (!found) { printf("c1l6s3 %s: no voice\n", keys[k]); ++bad; }
        }
    }
    printf("lines without a voice: %u (original silent: the scene's sounds bank is not in the install)\n", missing);
    // Audit result: the 34 lines are the whole of c4l2s3 (19) and c2l2s4 (15); sounds/c4l2s3.omt and sounds/c2l2s4.omt do not exist
    // and no other bank holds those Waves, and 0x412d00 only searches the scene bank (game+0x5c, 0x4122be / 0x412898), so the original is silent too.
    if (lines != 1149 || voiced != 1115 || missing != 34 || noBank != 34) {
        printf("expected 1149 lines, 1115 voiced, 34 silent (all in scenes without a bank); got %u, %u, %u (%u without bank)\n", lines, voiced, missing, noBank);
        ++bad;
    }
    printf("%s\n", bad ? "RESULT: FAIL" : "RESULT: OK");
    if (bad) exit(1);
}
