#include "Dialogue.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

namespace sbscript {

static std::string capped(const std::string& s, size_t n)   // port: the exe's fields are fixed-size char arrays
{
    return s.size() > n ? s.substr(0, n) : s;
}

static int chAt(const std::string& s, size_t i) { return i < s.size() ? (unsigned char)s[i] : 0; }

static bool decodeFile(FileSource& fs, const std::string& path, std::vector<std::string>& records, std::string* err)
{
    std::vector<unsigned char> data;
    if (!fs.readFile(path.c_str(), data)) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    std::string e;
    bool ok = sb2_decode(data.empty() ? (const unsigned char*)"" : &data[0], (unsigned)data.size(), records, e);
    if (!ok && err) *err = path + ": " + e;
    return ok;
}

// ---- scene dialogue (0x4122f0) ------------------------------------------------------------------

std::string DialogueTable::fileName(unsigned id)
{
    char b[64];
    snprintf(b, sizeof b, "dialogue/c%dl%ds%d.sb2", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf));
    return b;
}

std::string DialogueTable::soundBankName(unsigned id)
{
    char b[64];
    snprintf(b, sizeof b, "sounds/c%dl%ds%d.omt", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf));
    return b;
}

std::string DialogueTable::voiceName(unsigned id, const std::string& key)
{
    char b[64];
    char k0 = key.size() > 0 ? key[0] : 0, k1 = key.size() > 1 ? key[1] : 0, k2 = key.size() > 2 ? key[2] : 0;
    snprintf(b, sizeof b, "%d%d%d%c%c%c", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf),
             toupper((unsigned char)k0), toupper((unsigned char)k1), toupper((unsigned char)k2));
    return b;
}

bool DialogueTable::load(FileSource& fs, unsigned sceneId, DialogueProgress* progress, std::string* err)
{
    groups_.clear();
    sceneId_ = sceneId;
    std::vector<std::string> records;
    if (!decodeFile(fs, fileName(sceneId), records, err)) return false;

    std::string key;   // acStack_260: kept from the previous row if a row lacks the columns
    for (size_t r = 0; r < records.size(); ++r) {
        const std::string& rec = records[r];
        bool alnum = false;
        for (size_t k = 0; k < rec.size(); ++k)
            if (isalpha((unsigned char)rec[k]) || isdigit((unsigned char)rec[k])) { alnum = true; break; }
        if (!alnum) continue;                      // blank / punctuation-only rows are skipped

        SbTok tok(rec);
        std::string t;
        tok.next(t);                               // column 1 (chapter/level/scene tag) is dropped
        if (tok.next(t)) key = capped(t, 15);      // branch
        if (tok.next(t)) key += t;                 // node
        if (tok.next(t)) key += t;                 // line
        key = capped(key, 15);

        bool newGroup = groups_.empty();
        if (!newGroup) {
            const DialogueLine& prev = groups_.back().lines.back();
            if (toupper(chAt(key, 1)) != toupper(chAt(prev.key, 1))) newGroup = true;
            else if (toupper(chAt(key, 0)) != toupper(chAt(prev.key, 0))) newGroup = true;
        }
        if (newGroup) {
            DialogueGroup g;
            g.key = key;
            g.stateA = 0;
            g.stateB = 0;
            g.current = 0;
            groups_.push_back(g);
        }
        DialogueLine ln;
        ln.key = key;
        ln.flag1 = ln.flag2 = ln.flag3 = 0;
        ln.textHeight = 0x12;
        if (tok.next(t)) ln.speaker = capped(t, 31);
        if (tok.next(t)) ln.listener = t;          // dropped by the exe
        if (tok.next(t)) ln.text = capped(t, 255);
        if (tok.next(t)) ln.flag1 = (short)atoi(t.c_str());
        if (tok.next(t)) ln.flag2 = (short)atoi(t.c_str());
        if (tok.next(t)) ln.flag3 = (short)atoi(t.c_str());
        if (tok.next(t)) ln.action = capped(t, 31);
        groups_.back().lines.push_back(ln);
    }

    // saved progress per group (the loop after fcn.00443694)
    if (progress) {
        for (size_t gi = 0; gi < groups_.size(); ++gi) {
            DialogueGroup& g = groups_[gi];
            std::string cur;
            short a = 0, b = 0;
            if (progress->lookup(sceneId, g.key, &cur, &a, &b)) {
                for (size_t li = 0; li < g.lines.size(); ++li)
                    if (g.lines[li].key == cur) { g.current = (int)li; break; }
                g.stateA = a;
                g.stateB = b;
                if (b != 0) g.current = -1;
            }
        }
    }
    return true;
}

const DialogueLine* DialogueTable::find(const std::string& key) const
{
    for (size_t g = 0; g < groups_.size(); ++g)
        for (size_t l = 0; l < groups_[g].lines.size(); ++l)
            if (groups_[g].lines[l].key == key) return &groups_[g].lines[l];
    return 0;
}

unsigned DialogueTable::lineCount() const
{
    unsigned n = 0;
    for (size_t g = 0; g < groups_.size(); ++g) n += (unsigned)groups_[g].lines.size();
    return n;
}

// ---- comments (0x4106d0) ------------------------------------------------------------------------

std::string CommentTable::soundBankName(int chapter)
{
    char b[48];
    snprintf(b, sizeof b, "sounds/comments%d.omt", chapter);
    return b;
}

std::string CommentTable::voiceName(const std::string& id)
{
    std::string s;
    for (size_t i = 0; i < 5 && i < id.size(); ++i)
        s += (char)((i == 0 || i == 4) ? toupper((unsigned char)id[i]) : id[i]);
    return s;
}

bool CommentTable::load(FileSource& fs, int chapter, int level, std::string* err)
{
    entries_.clear();
    std::vector<std::string> records;
    if (!decodeFile(fs, "dialogue/comments.sb2", records, err)) return false;
    for (size_t r = 1; r < records.size(); ++r) {          // record 0 is the header row
        SbTok tok(records[r]);
        std::string id, text;
        if (!tok.next(id)) continue;
        if (id.size() > 2 && (int)id[1] == chapter + '0' && (int)id[2] == level + '0') {
            CommentEntry e;
            e.id = capped(id, 15);
            if (tok.next(text)) e.text = capped(text, 239);
            entries_.push_back(e);
        }
    }
    return true;
}

const CommentEntry* CommentTable::find(const std::string& id) const
{
    for (size_t i = 0; i < entries_.size(); ++i) if (entries_[i].id == id) return &entries_[i];
    return 0;
}

// ---- help (0x419210) ----------------------------------------------------------------------------

bool HelpTable::load(FileSource& fs, int chapter, std::string* err)
{
    entries_.clear();
    common_.clear();
    std::vector<std::string> records;
    if (!decodeFile(fs, "dialogue/help.sb2", records, err)) return false;
    for (size_t r = 1; r < records.size(); ++r) {
        SbTok tok(records[r]);
        std::string id, text, cond;
        if (!tok.next(id)) continue;
        if (id.size() > 1 && (int)id[1] == chapter + '0') {
            HelpEntry e;
            e.id = capped(id, 15);
            if (tok.next(text)) e.text = capped(text, 255);
            if (tok.next(cond)) e.condition = capped(cond, 31);
            entries_.push_back(e);
        } else if (id.size() > 1 && id[1] == '0') {      // port: F000A..: texts that other chapters' lines point to (a hint "F121A" -> "F000D")
            HelpEntry e;
            e.id = capped(id, 15);
            if (tok.next(text)) e.text = capped(text, 255);
            if (tok.next(cond)) e.condition = capped(cond, 31);
            common_.push_back(e);
        }
    }
    return true;
}

const HelpEntry* HelpTable::find(const std::string& id) const
{
    for (size_t i = 0; i < entries_.size(); ++i) if (entries_[i].id == id) return &entries_[i];
    for (size_t i = 0; i < common_.size(); ++i) if (common_[i].id == id) return &common_[i];   // port: chapter-0 lines
    return 0;
}

}  // namespace sbscript
