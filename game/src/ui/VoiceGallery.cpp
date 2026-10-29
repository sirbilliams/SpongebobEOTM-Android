// port: voice gallery (VoiceGallery.h, docs/reveal-and-gallery.md)
#include "VoiceGallery.h"
#include "../script/Dialogue.h"
#include "OMediaSound.h"
#include "OMediaSoundManager.h"
#include <SDL.h>
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <set>

namespace sbgame {

namespace {

class LocalFiles : public sbscript::FileSource {          // the install is the working directory (Game::init(".", ...))
public:
    virtual bool readFile(const char* path, std::vector<unsigned char>& out) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        unsigned char b[4096]; size_t n;
        out.clear();
        while ((n = fread(b, 1, sizeof b, f)) > 0) out.insert(out.end(), b, b + n);
        fclose(f);
        return true;
    }
};

struct Pretty { const char* id; const char* name; };
const Pretty kNames[] = {
    { "sb", "SpongeBob" }, { "sbhelm", "SpongeBob" }, { "squid", "Squidward" }, { "pat", "Patrick" }, { "patalt", "Patrick" },
    { "patalteat", "Patrick" }, { "krabs", "Mr. Krabs" }, { "eugene", "Mr. Krabs" }, { "krabsnude", "Mr. Krabs" }, { "san", "Sandy" },
    { "sandress", "Sandy" }, { "gary", "Gary" }, { "garyg", "Gary" }, { "plankton", "Plankton" }, { "leon", "Leon" }, { "leon2", "Leon" },
    { "leonmop", "Leon" }, { "bob", "Bob" }, { "gran", "Grandma" }, { "fish", "Fish" }, { "teller", "Teller" }, { "marlin", "Marlin" },
};

std::string displayName(const std::string& id)
{
    if (id.empty()) return "Others";
    std::string l = id;
    for (size_t i = 0; i < l.size(); ++i) l[i] = (char)tolower((unsigned char)l[i]);
    for (size_t i = 0; i < sizeof kNames / sizeof kNames[0]; ++i) if (l == kNames[i].id) return kNames[i].name;
    std::string r = id;
    r[0] = (char)toupper((unsigned char)r[0]);
    return r;
}

// Same seven case combinations DialogueUI::startVoice tries (0x412d00).
OMediaSound* findVoice(OMediaDataBase* db, unsigned id, const std::string& key)
{
    if (!db || key.size() < 3) return 0;
    static const int combos[7][3] = { {0,0,0},{1,0,1},{1,1,0},{0,1,1},{0,1,0},{1,0,0},{0,0,1} };
    for (int c = 0; c < 7; ++c) {
        char n[16];
        sprintf(n, "%d%d%d%c%c%c", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf),
                combos[c][0] ? toupper((unsigned char)key[0]) : tolower((unsigned char)key[0]),
                combos[c][1] ? toupper((unsigned char)key[1]) : tolower((unsigned char)key[1]),
                combos[c][2] ? toupper((unsigned char)key[2]) : tolower((unsigned char)key[2]));
        if (db->object_exists(OMediaSound::db_type, std::string(n))) return (OMediaSound*)db->get_object(OMediaSound::db_type, std::string(n));
    }
    return 0;
}

std::string heardKey(unsigned scene, const std::string& key)
{
    char b[16]; sprintf(b, "%08x:", scene);
    std::string k = b;
    for (size_t i = 0; i < key.size(); ++i) k += (char)toupper((unsigned char)key[i]);
    return k;
}

struct OrderLess {
    bool operator()(const std::pair<std::string, VoiceClip>& a, const std::pair<std::string, VoiceClip>& b) const { return a.first < b.first; }
};

}  // namespace

VoiceGallery::VoiceGallery() : bankScene_(0) {}
VoiceGallery::~VoiceGallery() { stop(); }

void VoiceGallery::build(const GameState* st)
{
    clips_.clear();
    std::set<unsigned> scenes;
    if (DIR* d = opendir("dialogue")) {
        while (struct dirent* e = readdir(d)) {
            int c, l, s; char tail[8] = { 0 };
            if (sscanf(e->d_name, "c%dl%ds%d.s%1[b]", &c, &l, &s, tail) >= 3 && strstr(e->d_name, ".sb2"))
                scenes.insert(sbscript::sceneIdPack(c, l, s));
        }
        closedir(d);
    }
    // heard sources: this run's voices, the saved dialogue progress (lines before the group's current line, all of a finished group)
    std::set<std::string> heard;
    std::vector<GameState::DProg> prog;
    if (st) { heard = st->voicesHeard(); st->dialogueEntries(prog); }
    const char* seed = getenv("SB_TEST_VOICES_HEARD");       // port: tests only, "all" or "c1l2s1,c1l2s2:AAB,..."
    LocalFiles fs;
    std::vector<std::pair<std::string, VoiceClip> > ordered;
    for (std::set<unsigned>::iterator it = scenes.begin(); it != scenes.end(); ++it) {
        unsigned id = *it;
        sbscript::DialogueTable t;
        std::string err;
        if (!t.load(fs, id, 0, &err)) continue;
        OmtFile bank;
        bank.open(sbscript::DialogueTable::soundBankName(id));
        if (!bank.db) continue;
        char sname[16]; sprintf(sname, "c%dl%ds%d", (int)(id & 0xf), (int)((id >> 8) & 0xf), (int)((id >> 16) & 0xf));
        for (size_t gi = 0; gi < t.groups().size(); ++gi) {
            const sbscript::DialogueGroup& g = t.groups()[gi];
            int curIdx = -1; bool finished = false, started = false;
            for (size_t pi = 0; pi < prog.size(); ++pi)
                if (prog[pi].scene == id && strcasecmp(prog[pi].group.c_str(), g.key.c_str()) == 0) {
                    started = prog[pi].a != 0 || prog[pi].b != 0; finished = prog[pi].b != 0;
                    for (size_t li = 0; li < g.lines.size(); ++li) if (g.lines[li].key == prog[pi].cur) curIdx = (int)li;
                }
            for (size_t li = 0; li < g.lines.size(); ++li) {
                const sbscript::DialogueLine& ln = g.lines[li];
                if (!findVoice(bank.db, id, ln.key)) continue;           // a line without a recording is not a clip
                VoiceClip c;
                c.scene = id; c.key = ln.key; c.speaker = displayName(ln.speaker); c.text = ln.text;
                c.heard = heard.count(heardKey(id, ln.key)) != 0 || (started && (finished || (curIdx >= 0 && (int)li < curIdx)));
                if (seed) {
                    std::string sd = std::string(",") + seed + ",";
                    std::string up = ln.key; for (size_t k = 0; k < up.size(); ++k) up[k] = (char)toupper((unsigned char)up[k]);
                    if (sd.find(",all,") != std::string::npos || sd.find(std::string(",") + sname + ",") != std::string::npos ||
                        sd.find(std::string(",") + sname + ":" + up + ",") != std::string::npos) c.heard = true;
                }
                char ord[24]; sprintf(ord, "%d%08x%04d%04d", chapterOf(id), id, (int)gi, (int)li);
                ordered.push_back(std::make_pair(std::string(ord), c));
            }
        }
    }
    std::sort(ordered.begin(), ordered.end(), OrderLess());
    for (size_t i = 0; i < ordered.size(); ++i) clips_.push_back(ordered[i].second);
}

std::vector<std::string> VoiceGallery::speakers(int chapter) const
{
    std::vector<std::string> r;
    for (size_t i = 0; i < clips_.size(); ++i) {
        if (chapterOf(clips_[i].scene) != chapter) continue;
        if (std::find(r.begin(), r.end(), clips_[i].speaker) == r.end()) r.push_back(clips_[i].speaker);
    }
    return r;
}

std::vector<int> VoiceGallery::select(int chapter, const std::string& speaker) const
{
    std::vector<int> r;
    for (size_t i = 0; i < clips_.size(); ++i)
        if (chapterOf(clips_[i].scene) == chapter && clips_[i].speaker == speaker) r.push_back((int)i);
    return r;
}

void VoiceGallery::count(int chapter, const std::string& speaker, int* heard, int* total) const
{
    int h = 0, t = 0;
    for (size_t i = 0; i < clips_.size(); ++i) {
        if (chapterOf(clips_[i].scene) != chapter || (!speaker.empty() && clips_[i].speaker != speaker)) continue;
        ++t; if (clips_[i].heard) ++h;
    }
    if (heard) *heard = h;
    if (total) *total = t;
}

bool VoiceGallery::play(int index)
{
    if (index < 0 || index >= (int)clips_.size() || !clips_[index].heard) return false;
    const VoiceClip& c = clips_[index];
    if (!bank_.db || bankScene_ != c.scene) {
        stop();
        bank_.reset();
        if (!bank_.open(sbscript::DialogueTable::soundBankName(c.scene))) return false;
        bankScene_ = c.scene;
    }
    OMediaSound* s = findVoice(bank_.db, c.scene, c.key);
    if (!s) return false;
    OMediaSoundManager* m = OMediaSoundManager::getdefaultmanager();
    if (!m || 4 >= m->get_nchannels()) return false;
    m->get_channel(4)->stop();
    m->get_channel(4)->set_pan(0);
    bool ok = m->play(4, s, false, false);
    SDL_Log("voice gallery: play %s %s (%.2f s) -> %d", sbscript::DialogueTable::voiceName(c.scene, c.key).c_str(), c.speaker.c_str(), s->get_sound_secs(), ok ? 1 : 0);
    return ok;
}

void VoiceGallery::stop()
{
    OMediaSoundManager* m = OMediaSoundManager::getdefaultmanager();
    if (m && bank_.db && 4 < m->get_nchannels()) m->get_channel(4)->stop();
}

bool VoiceGallery::playing() const
{
    OMediaSoundManager* m = OMediaSoundManager::getdefaultmanager();
    return m && 4 < m->get_nchannels() && m->get_channel(4)->get_playing_sound() != NULL;
}

}  // namespace sbgame
