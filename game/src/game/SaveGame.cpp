#include "SaveGame.h"
#include "OMediaCanvas.h"
#include "OMediaGfxWorld.h"
#include "OMediaPixmapWorld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <map>

namespace sbgame {

static std::string g_dir = ".";

SaveFile::SaveFile() : hasThumb(false), sceneId(0), px(0), py(0), pz(0), facing(0), puzzle(false), high(0), hasLast(false), last(0)
{
    memset(name, 0, sizeof name);
    memset(puzzleBytes, 0, sizeof puzzleBytes);
}

// ------------------------------------------------------------------------------------------------ slot files
void SaveGame::setDir(const std::string& dir) { g_dir = dir.empty() ? std::string(".") : dir; }

static std::string savedDir() { return g_dir + "/saved"; }
std::string SaveGame::slotPath(int slot) { char b[32]; sprintf(b, "/slot%d.sb2", slot); return savedDir() + b; }   // 0x455350 "Saved\slot%d.sb2"
std::string SaveGame::quickPath() { return savedDir() + "/quick.sb2"; }                                           // 0x455340

// 0x40e120: the file opens and has a length (the installed game ships empty slot files; they are not saves)
bool SaveGame::fileExists(const std::string& path)
{
    struct stat sb;
    return stat(path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode) && sb.st_size > 0;
}
bool SaveGame::exists(int slot) { return slot >= 0 && slot < kSlots && fileExists(slotPath(slot)); }   // 0x428950
int SaveGame::firstFreeSlot()      // 0x427ef0: the first of 0..98 whose file does not exist; kSlots when all are used
{
    for (int i = 0; i < kSlots; ++i) if (!exists(i)) return i;
    return kSlots;
}

// 0x428a70: the slot is removed (0x44366a) and each following slot is renamed one down (0x4439f7) while they exist
bool SaveGame::deleteSlot(int slot)
{
    if (!exists(slot)) return false;
    remove(slotPath(slot).c_str());
    int i = slot + 1;
    for (; i < kSlots && exists(i); ++i) rename(slotPath(i).c_str(), slotPath(i - 1).c_str());
    return true;
}

// ------------------------------------------------------------------------------------------------ writer 0x416ed0
namespace {
struct Out {
    std::vector<unsigned char> b;
    void raw(const void* p, size_t n) { const unsigned char* c = (const unsigned char*)p; b.insert(b.end(), c, c + n); }
    void u32(unsigned v) { raw(&v, 4); }
    void s32(int v) { raw(&v, 4); }
    void s16(short v) { raw(&v, 2); }
    void f32(float v) { raw(&v, 4); }
    void str(const char* s, size_t n) { std::vector<unsigned char> z(n, 0); if (s) strncpy((char*)&z[0], s, n - 1); raw(&z[0], n); }
    void tag(unsigned t) { u32(t); }
};
struct In {
    const std::vector<unsigned char>& b; size_t pos; bool bad;
    In(const std::vector<unsigned char>& v) : b(v), pos(0), bad(false) {}
    bool raw(void* p, size_t n) { if (bad || pos + n > b.size()) { bad = true; memset(p, 0, n); return false; } memcpy(p, &b[pos], n); pos += n; return true; }
    unsigned u32() { unsigned v; raw(&v, 4); return v; }
    int s32() { int v; raw(&v, 4); return v; }
    short s16() { short v; raw(&v, 2); return v; }
    float f32() { float v; raw(&v, 4); return v; }
    void str(char* d, size_t n) { raw(d, n); d[n - 1] = 0; }
    bool end() const { return pos >= b.size(); }
};
enum { T_MIET = 0x4954454d, T_CHAR = 0x43484152, T_THRD = 0x54485244, T_SCRP = 0x53435250, T_SCEN = 0x5343454e, T_TEXT = 0x54455854,
       T_POSI = 0x504f5349, T_BLOK = 0x424c4f4b, T_PUZZ = 0x50555a5a, T_HIGH = 0x48494748, T_LAST = 0x4c415354 };
}

bool SaveGame::write(const std::string& path, const SaveFile& f)
{
    Out o;
    o.raw(f.name, 0x20);
    o.u32(kMagic);
    if (f.hasThumb && !f.thumb.empty()) { o.s32((int)f.thumb.size()); o.raw(&f.thumb[0], f.thumb.size()); }
    else o.s32(-1);
    o.u32(f.sceneId);
    o.f32(f.px); o.f32(f.py); o.f32(f.pz); o.s16(f.facing);
    o.s16((short)f.bar.size());                                                          // 0x421c40
    for (size_t i = 0; i < f.bar.size(); ++i) { o.raw(f.bar[i].name, 0x20); o.raw(f.bar[i].tex, 0x20); o.s16(f.bar[i].slot); }
    for (size_t i = 0; i < f.miet.size(); ++i) { o.tag(T_MIET); o.str(f.miet[i].c_str(), 0x20); }
    for (size_t i = 0; i < f.chars.size(); ++i) { o.tag(T_CHAR); o.str(f.chars[i].c_str(), 0x20); }
    for (size_t i = 0; i < f.thrd.size(); ++i) {
        const SaveFile::Thrd& t = f.thrd[i];
        o.tag(T_THRD); o.raw(t.group, 0x10); o.raw(t.cur, 0x10); o.s16(t.a); o.s16(t.b); o.u32(t.scene);
    }
    for (size_t i = 0; i < f.scrp.size(); ++i) { o.tag(T_SCRP); o.str(f.scrp[i].c_str(), 0x20); }
    for (size_t i = 0; i < f.scen.size(); ++i) { o.tag(T_SCEN); o.u32(f.scen[i]); }
    for (size_t i = 0; i < f.text.size(); ++i) { o.tag(T_TEXT); o.raw(f.text[i].object, 0x20); o.raw(f.text[i].texture, 0x20); }
    for (size_t i = 0; i < f.posi.size(); ++i) {
        const GameState::PosiRec& p = f.posi[i];
        o.tag(T_POSI); o.raw(p.name, 0x20); o.u32(p.a); o.u32(p.b); o.u32(p.c); o.u32(p.d); o.s16(p.e);
    }
    for (size_t i = 0; i < f.blok.size(); ++i) {
        const SaveFile::Blok& k = f.blok[i];
        o.tag(T_BLOK); o.u32(k.scene); o.s16(k.a); o.s16(k.b); o.s16(k.c); o.s16(k.d);
    }
    if (f.puzzle) {
        o.tag(T_PUZZ); o.raw(f.puzzleBytes, 42);
        if (f.high > 0) { o.tag(T_HIGH); o.s16(f.high); }
    }
    o.tag(T_LAST); o.u32(f.hasLast ? f.last : 0);

    // port: the saved folder exists, and the file appears whole or not at all
    mkdir(savedDir().c_str(), 0755);
    std::string tmp = path + ".tmp";
    FILE* fp = fopen(tmp.c_str(), "wb");
    if (!fp) return false;
    bool ok = fwrite(&o.b[0], 1, o.b.size(), fp) == o.b.size();
    ok = fclose(fp) == 0 && ok;
    if (!ok || rename(tmp.c_str(), path.c_str()) != 0) { remove(tmp.c_str()); return false; }
    return true;
}

// ------------------------------------------------------------------------------------------------ loader 0x417460
static bool slurp(const std::string& path, std::vector<unsigned char>& v)
{
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    unsigned char buf[4096]; size_t n;
    v.clear();
    while ((n = fread(buf, 1, sizeof buf, fp)) > 0) v.insert(v.end(), buf, buf + n);
    fclose(fp);
    return !v.empty();
}

// the header up to the position; `in` is left at the item bar
static bool readHead(In& in, SaveFile& f, bool needAll)
{
    in.str(f.name, 0x20);
    if (in.u32() != SaveGame::kMagic) return false;          // port: only the SHOT format (the exe skips the thumbnail only for it)
    int tb = in.s32();
    f.hasThumb = false; f.thumb.clear();
    if (tb != -1) {
        if (tb < 0 || in.pos + (size_t)tb > in.b.size()) return false;
        if (needAll) { f.thumb.assign(in.b.begin() + in.pos, in.b.begin() + in.pos + tb); f.hasThumb = true; }
        in.pos += tb;
    }
    f.sceneId = in.u32();
    f.px = in.f32(); f.py = in.f32(); f.pz = in.f32(); f.facing = in.s16();
    return !in.bad;
}

bool SaveGame::readHeader(const std::string& path, SaveFile& f)
{
    std::vector<unsigned char> v;
    if (!slurp(path, v)) return false;
    f = SaveFile();
    In in(v);
    if (!readHead(in, f, true)) return false;
    return true;
}

bool SaveGame::read(const std::string& path, SaveFile& f)
{
    std::vector<unsigned char> v;
    if (!slurp(path, v)) return false;
    f = SaveFile();
    In in(v);
    if (!readHead(in, f, true)) return false;
    short n = in.s16();                                                                  // 0x41f2d0 items of the bar
    for (int i = 0; i < n && !in.bad; ++i) {
        GameState::BarRec r; in.str(r.name, 0x20); in.str(r.tex, 0x20); r.slot = in.s16(); f.bar.push_back(r);
    }
    while (!in.end() && !in.bad) {
        unsigned tag = in.u32();
        if (in.bad) break;
        char s[0x20];
        switch (tag) {
        case T_MIET: in.str(s, 0x20); f.miet.push_back(s); break;
        case T_CHAR: in.str(s, 0x20); f.chars.push_back(s); break;
        case T_THRD: { SaveFile::Thrd t; in.raw(t.group, 0x10); in.raw(t.cur, 0x10); t.group[15] = t.cur[15] = 0;
                       t.a = in.s16(); t.b = in.s16(); t.scene = in.u32(); f.thrd.push_back(t); break; }
        case T_SCRP: in.str(s, 0x20); f.scrp.push_back(s); break;
        case T_SCEN: f.scen.push_back(in.u32()); break;
        case T_TEXT: { SaveFile::Text t; in.str(t.object, 0x20); in.str(t.texture, 0x20); f.text.push_back(t); break; }
        case T_POSI: { GameState::PosiRec p; in.str(p.name, 0x20); p.a = in.u32(); p.b = in.u32(); p.c = in.u32(); p.d = in.u32();
                       p.e = in.s16(); f.posi.push_back(p); break; }
        case T_BLOK: { SaveFile::Blok k; k.scene = in.u32(); k.a = in.s16(); k.b = in.s16(); k.c = in.s16(); k.d = in.s16();
                       f.blok.push_back(k); break; }
        case T_PUZZ: in.raw(f.puzzleBytes, 42); f.puzzle = true; break;
        case T_HIGH: f.high = in.s16(); break;
        case T_LAST: f.last = in.u32(); f.hasLast = true; break;
        default: return false;       // port: an unknown tag ends the load (the exe would read on from the wrong place)
        }
    }
    return !in.bad;
}

// ------------------------------------------------------------------------------------------------ state <-> file
static void copyStr(char* d, size_t n, const std::string& s) { memset(d, 0, n); strncpy(d, s.c_str(), n - 1); }

void SaveGame::capture(GameState& st, unsigned sceneId, const char* name, const std::vector<unsigned char>& thumb, SaveFile& f)
{
    f = SaveFile();
    copyStr(f.name, sizeof f.name, name ? name : "");
    if ((int)thumb.size() == kThumbBytes) { f.thumb = thumb; f.hasThumb = true; }
    f.sceneId = sceneId;
    if (st.hasPlayer) { f.px = st.playerX; f.py = st.playerY; f.pz = st.playerZ; f.facing = st.playerFacing; }
    f.bar = st.bar_;
    f.miet = st.items_;
    f.chars = st.chars_;
    f.scrp.assign(st.seen_.begin(), st.seen_.end());
    std::set<unsigned> flags = st.scen_;
    for (std::set<std::string>::const_iterator it = st.flags_.begin(); it != st.flags_.end(); ++it) {
        unsigned id = GameState::flagId(it->c_str());
        if (id != 0xffffffffu) flags.insert(id);
    }
    f.scen.assign(flags.begin(), flags.end());
    for (std::map<std::string, GameState::DEntry>::const_iterator it = st.dialogue_.begin(); it != st.dialogue_.end(); ++it) {
        SaveFile::Thrd t; memset(&t, 0, sizeof t);
        t.scene = (unsigned)strtoul(it->first.c_str(), NULL, 16);
        size_t c = it->first.find(':');
        copyStr(t.group, sizeof t.group, c == std::string::npos ? std::string() : it->first.substr(c + 1));
        copyStr(t.cur, sizeof t.cur, it->second.key);
        t.a = it->second.a; t.b = it->second.b;
        f.thrd.push_back(t);
    }
    for (std::map<std::string, std::string>::const_iterator it = st.textures_.begin(); it != st.textures_.end(); ++it) {
        SaveFile::Text t; copyStr(t.object, sizeof t.object, it->first); copyStr(t.texture, sizeof t.texture, it->second);
        f.text.push_back(t);
    }
    f.posi = st.posi_;
    for (std::set<unsigned long long>::const_iterator it = st.blocked_.begin(); it != st.blocked_.end(); ++it) {
        SaveFile::Blok k; k.scene = (unsigned)(*it >> 32);
        k.a = k.c = (short)((*it >> 16) & 0xffff); k.b = k.d = (short)(*it & 0xffff);      // port: one cell stored as x,y,x,y
        f.blok.push_back(k);
    }
    f.puzzle = st.hasPuzzle_; memcpy(f.puzzleBytes, st.puzzle_, 42); f.high = st.high_;
    f.hasLast = true; f.last = st.lastScene;
}

std::string SaveGame::apply(const SaveFile& f, GameState& st)
{
    st.clearProgress();                                                   // 0x4163b0 + 0x41e160
    st.bar_ = f.bar;
    for (size_t i = 0; i < f.miet.size(); ++i) st.giveItem(f.miet[i].c_str());                 // 0x4169a0
    st.chars_ = f.chars;
    for (size_t i = 0; i < f.thrd.size(); ++i)                                                 // 0x416d30
        st.storeDialogue(f.thrd[i].scene, f.thrd[i].group, f.thrd[i].cur, f.thrd[i].a, f.thrd[i].b);
    for (size_t i = 0; i < f.scrp.size(); ++i) st.markScriptSeen(f.scrp[i].c_str());           // 0x416610
    for (size_t i = 0; i < f.scen.size(); ++i) st.scen_.insert(f.scen[i]);                     // 0x416a60
    for (size_t i = 0; i < f.text.size(); ++i) st.setTexture(f.text[i].object, f.text[i].texture);   // 0x4166d0
    st.posi_ = f.posi;
    for (size_t i = 0; i < f.blok.size(); ++i) st.setBlocked(f.blok[i].scene, f.blok[i].a, f.blok[i].b, true);   // 0x416ba0
    st.hasPuzzle_ = f.puzzle; memcpy(st.puzzle_, f.puzzleBytes, 42); st.high_ = f.high;
    st.lastScene = f.hasLast ? f.last : 0;
    st.playerX = f.px; st.playerY = f.py; st.playerZ = f.pz; st.playerFacing = f.facing;
    st.hasPlayer = false;
    st.resumePos = !(f.px == 0.0f && f.py == 0.0f && f.pz == 0.0f && f.facing == 0);           // progress+0x64
    st.sceneId = f.sceneId;
    st.chapter = -1;                                                      // game+0x154 = 0xffff: the chapter's sound banks load again
    char b[32];
    sprintf(b, "c%dl%ds%dp1", (int)(f.sceneId & 0xf), (int)((f.sceneId >> 8) & 0xf), (int)((f.sceneId >> 16) & 0xf));   // 0x455164
    return b;
}

// ------------------------------------------------------------------------------------------------ thumbnails
bool SaveGame::grabThumb(OMediaGfxWorld* screen, std::vector<unsigned char>& out)
{
    out.clear();
    if (!screen) return false;
    OMediaPixBlitterWorld mem;
    mem.create(640, 480);
    OMediaGfxPixels* px = mem.port_pixels();
    if (!px || !px->pix) return false;
    OMediaRect r(0, 0, 640, 480);
    screen->lock();
    screen->draw(&mem, &r, 0, 0);
    screen->unlock();
    out.assign(kThumbBytes, 0);
    for (int y = 0; y < kThumbH; ++y)
        for (int x = 0; x < kThumbW; ++x) {
            unsigned rs = 0, gs = 0, bs = 0;
            for (int k = 0; k < 4; ++k) {
                unsigned p = px->pix[(2 * y + (k >> 1)) * 640 + 2 * x + (k & 1)];
                rs += (p >> 16) & 255; gs += (p >> 8) & 255; bs += p & 255;
            }
            rs /= 4; gs /= 4; bs /= 4;
            unsigned short v = (unsigned short)(((rs >> 3) << 11) | ((gs >> 2) << 5) | (bs >> 3));
            out[2 * (y * kThumbW + x)] = (unsigned char)(v & 255);
            out[2 * (y * kThumbW + x) + 1] = (unsigned char)(v >> 8);
        }
    return true;
}

OMediaCanvas* SaveGame::thumbCanvas(const std::vector<unsigned char>& t)
{
    if ((int)t.size() != kThumbBytes) return NULL;
    OMediaPixBlitterWorld* w = new OMediaPixBlitterWorld;
    w->create(kThumbW, kThumbH);
    OMediaGfxPixels* px = w->port_pixels();
    if (!px || !px->pix) { delete w; return NULL; }
    for (int i = 0; i < kThumbW * kThumbH; ++i) {
        unsigned v = t[2 * i] | (t[2 * i + 1] << 8);
        unsigned r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
        r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
        px->pix[i] = 0xff000000u | (r << 16) | (g << 8) | b;
    }
    px->version++;
    OMediaCanvas* c = new OMediaCanvas;
    c->setgfxworld(w);
    return c;
}

// ------------------------------------------------------------------------------------------------ round trip test
int SaveGame::selfTest()
{
    GameState a;
    a.markScriptSeen("sload1"); a.markScriptSeen("sc1l2s6a");
    a.giveItem("ikc1"); a.giveItem("ihat");
    a.storeDialogue(0x20201, "G001", "H0021", 3, 7);
    a.storeDialogue(0x60201, "G010", "H1002", 1, 0);
    a.setTexture("sandy", "sandy2");
    a.setBlocked(0x60201, 12, 34, true);
    a.setFlag("c1x2y3", true);
    a.hasPlayer = true; a.playerX = 1.5f; a.playerY = -2.25f; a.playerZ = 300.0f; a.playerFacing = 4096;
    a.lastScene = 0x30201;
    std::vector<unsigned char> thumb(kThumbBytes);
    for (size_t i = 0; i < thumb.size(); ++i) thumb[i] = (unsigned char)(i * 7);
    SaveFile f1;
    capture(a, 0x60201, "Selftest", thumb, f1);
    std::string path = savedDir() + "/selftest.sb2";
    mkdir(savedDir().c_str(), 0755);
    if (!write(path, f1)) { fprintf(stderr, "selftest: write failed\n"); return 1; }
    SaveFile f2;
    if (!read(path, f2)) { fprintf(stderr, "selftest: read failed\n"); return 2; }
    GameState b;
    std::string scene = apply(f2, b);
    SaveFile f3;
    capture(b, f2.sceneId, f2.name, f2.thumb, f3);
    f3.px = f2.px; f3.py = f2.py; f3.pz = f2.pz; f3.facing = f2.facing;        // apply keeps the position for the scene (hasPlayer false)
    std::string path2 = savedDir() + "/selftest2.sb2";
    write(path2, f3);
    std::vector<unsigned char> v1, v2;
    slurp(path, v1); slurp(path2, v2);
    remove(path.c_str()); remove(path2.c_str());
    int rc = 0;
    if (v1 != v2) { fprintf(stderr, "selftest: files differ (%u vs %u bytes)\n", (unsigned)v1.size(), (unsigned)v2.size()); rc = 3; }
    if (scene != "c1l2s6p1") { fprintf(stderr, "selftest: scene %s\n", scene.c_str()); rc = 4; }
    if (!b.scriptSeen("sload1") || !b.hasItem("ihat") || !b.isBlocked(0x60201, 12, 34) || !b.flagSet("c1x2y3") || !b.dialogueStarted(0x20201, "G001")
        || !b.resumePos || b.playerZ != 300.0f) { fprintf(stderr, "selftest: state mismatch\n"); rc = 5; }
    short x, y; std::string cur;
    if (!b.lookup(0x60201, "G010", &cur, &x, &y) || cur != "H1002" || x != 1) { fprintf(stderr, "selftest: dialogue mismatch\n"); rc = 6; }
    if (rc == 0) fprintf(stderr, "selftest: round trip OK (%u bytes)\n", (unsigned)v1.size());
    return rc;
}

}  // namespace sbgame
