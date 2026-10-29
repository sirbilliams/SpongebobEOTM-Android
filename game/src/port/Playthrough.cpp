#include "Playthrough.h"
#include "../game/Game.h"
#include "../game/World3D.h"
#include "../game/DialogueUI.h"
#include "../game/Hud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "OMediaEventManager.h"

extern void omf_request_shot(const char* path);

namespace sbport {

using namespace sbgame;

static std::vector<std::string> words(const std::string& s)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i >= s.size() || s[i] == '#') break;
        std::string w;
        if (s[i] == '"') { size_t e = s.find('"', i + 1); if (e == std::string::npos) e = s.size(); w = s.substr(i + 1, e - i - 1); i = e + 1; }
        else { size_t e = i; while (e < s.size() && s[e] != ' ' && s[e] != '\t') ++e; w = s.substr(i, e - i); i = e; }
        out.push_back(w);
    }
    return out;
}

Playthrough::Playthrough(const char* file) : cur_(0), phase_(0), frames_(0), waitLeft_(0), idleFrames_(0), file_(file), failed_(false)
{
    FILE* f = fopen(file, "r");
    if (!f) { fprintf(stdout, "PLAYTHROUGH: cannot open %s\n", file); return; }
    char buf[512]; int n = 0;
    while (fgets(buf, sizeof buf, f)) {
        ++n;
        std::string l(buf);
        while (!l.empty() && (l[l.size() - 1] == '\n' || l[l.size() - 1] == '\r')) l.erase(l.size() - 1);
        std::vector<std::string> w = words(l);
        if (w.empty()) continue;
        Step s; s.line = n; s.cmd = w[0]; s.args.assign(w.begin() + 1, w.end());
        steps_.push_back(s);
    }
    fclose(f);
}

void Playthrough::fail(Game* g, const char* why)
{
    const Step& s = steps_[cur_];
    std::string a; for (size_t i = 0; i < s.args.size(); ++i) a += " " + s.args[i];
    fprintf(stdout, "PLAYTHROUGH FAIL line %d (%s%s): %s; scene %s runner %d dialogue %d pending %d comment %d player-busy %d\n", s.line,
            s.cmd.c_str(), a.c_str(), why, g->scene() ? g->scene()->name().c_str() : "-", g->runner() ? 1 : 0,
            g->scene() && g->scene()->dialogue() && g->scene()->dialogue()->busy() ? 1 : 0, g->pendingScene() ? 1 : 0,
            g->commentActive() ? 1 : 0, g->world3d() && g->world3d()->playerBusy() ? 1 : 0);
    fflush(stdout);
    failed_ = true;
}

bool Playthrough::idle(Game* g)
{
    bool busy = g->pendingScene() || g->runner() || g->commentActive() || g->pendingObj || !g->scene();
    if (!busy) {
        GameScene* s = g->scene();
        busy = (s->dialogue() && s->dialogue()->busy()) || s->barState() != 0 || s->lockMs() > 0 || s->hasPendingEvent() ||
               (g->world3d() && g->world3d()->playerBusy()) || (g->hud() && g->hud()->flying()) || g->walkInMs > 0;
    }
    idleFrames_ = busy ? 0 : idleFrames_ + 1;
    return idleFrames_ >= 8;
}

// the dialogue is answered while any step waits: a line is skipped with Space after a while, a choice list gets the next KEY
bool Playthrough::dialogueStep(Game* g)
{
    GameScene* s = g->scene();
    DialogueUI* d = s ? s->dialogue() : 0;
    if (!d || !d->busy()) return false;
    static int lineFrames = 0;
    if (!d->choiceMode()) {
        if (++lineFrames > 25) { lineFrames = 0; d->onKey(' '); }
        return true;
    }
    lineFrames = 0;
    std::vector<std::string> keys = d->choiceKeys();
    if (answers_.empty()) { fprintf(stdout, "PLAYTHROUGH: choices left, Escape\n"); d->onKey(omck_Escape); return true; }
    std::string want = answers_[0];
    answers_.erase(answers_.begin());
    for (size_t i = 0; i < keys.size(); ++i)
        if (strcasecmp(keys[i].c_str(), want.c_str()) == 0) { fprintf(stdout, "PLAYTHROUGH: choice %s\n", want.c_str()); d->pickChoice((int)i); return true; }
    std::string all; for (size_t i = 0; i < keys.size(); ++i) all += " " + keys[i];
    fprintf(stdout, "PLAYTHROUGH: choice %s not offered (offered:%s)\n", want.c_str(), all.c_str());
    failed_ = true;
    return true;
}

bool Playthrough::locate(Game* g, const std::string& target, int* x, int* y)
{
    if (!target.empty() && target[0] == '@') return sscanf(target.c_str() + 1, "%d,%d", x, y) == 2;
    GameScene* s = g->scene();
    if (!s) return false;
    std::string name = target; int nth = 1;
    size_t h = name.rfind('#');
    if (h != std::string::npos && h > 0) { nth = atoi(name.c_str() + h + 1); name.resize(h); }
    for (int pass = 0; pass < 2; ++pass) {
        int n = 0;
        for (size_t i = 0; i < s->objects().size(); ++i) {
            SceneObject* o = s->objects()[i];
            const std::string& k = pass == 0 ? o->art : o->description;
            if (k.empty() || strcasecmp(k.c_str(), name.c_str()) != 0 || ++n != nth) continue;
            if (o->kind == SceneObject::Char || o->kind == SceneObject::Item) {
                int l, t, r, b;
                if (!g->world3d() || !g->world3d()->screenBounds(o, l, t, r, b)) return false;
                int px = -1, py = -1, fx = -1, fy = -1; g->world3d()->project(o->px, o->py, o->pz, px, py);
                g->world3d()->project(o->px, -100, o->pz, fx, fy);
                fprintf(stdout, "PLAYTHROUGH: %s drawn at %d,%d-%d,%d (origin %d,%d floor %d,%d)\n", o->art.c_str(), l, t, r, b, px, py, fx, fy);
                if (l < 0) l = 0; if (t < 0) t = 0; if (r > 639) r = 639; if (b > 479) b = 479;     // the part on the screen
                if (l > r || t > b) return false;
                *x = (l + r) / 2; *y = (t + b) / 2;
            } else { *x = (o->x1 + o->x2) / 2; *y = (o->y1 + o->y2) / 2; }
            return true;
        }
    }
    return false;
}

bool Playthrough::tick(Game* g, void (*click)(int, int), void (*save)(Game*, int), bool* failed)
{
    *failed = failed_;
    if (failed_ || cur_ >= steps_.size()) return false;
    const Step& st = steps_[cur_];
    static int limit = getenv("SB_TEST_STEPFRAMES") ? atoi(getenv("SB_TEST_STEPFRAMES")) : 6000;
    if (++frames_ > limit) { fail(g, "timed out"); *failed = true; return false; }
    if (dialogueStep(g)) { *failed = failed_; return !failed_; }
    if (waitLeft_ > 0) { --waitLeft_; return true; }
    bool isIdle = idle(g);
    const std::string& c = st.cmd;
    bool done = false;
    if (c == "wait") {
        if (phase_ == 0) { waitLeft_ = st.args.empty() ? 1 : atoi(st.args[0].c_str()); phase_ = 1; }
        else done = true;
    } else if (c == "scene") {                     // the scene is current (busy or not)
        if (g->scene() && !g->pendingScene() && !st.args.empty() && strcasecmp(g->scene()->name().c_str(), st.args[0].c_str()) == 0) done = true;
        else return true;
    } else if (!isIdle) {
        return true;
    } else if (c == "idle") {
        done = true;
    } else if (c == "click" || c == "talk") {
        int x, y;
        if (st.args.empty() || !locate(g, st.args[0], &x, &y)) { fail(g, "target not found / not on screen"); *failed = true; return false; }
        answers_.assign(st.args.begin() + 1, st.args.end());
        fprintf(stdout, "PLAYTHROUGH line %d: %s %s at %d,%d\n", st.line, c.c_str(), st.args[0].c_str(), x, y);
        click(x, y);
        done = true;
    } else if (c == "say") {
        answers_.assign(st.args.begin(), st.args.end());
        done = true;
    } else if (c == "use" || c == "combine") {
        Hud* hud = g->hud();
        if (st.args.size() < 2 || !hud) { fail(g, "bad step"); *failed = true; return false; }
        int slot = hud->slotOf(st.args[0]);
        if (slot < 0) { fail(g, "item not in the bar"); *failed = true; return false; }
        if (phase_ == 0) { if (!hud->isOpen()) click(32, 32); phase_ = 1; waitLeft_ = 10; return true; }
        if (phase_ == 1) { click(0x40 + 0x40 * slot + 32, 32); phase_ = 2; waitLeft_ = 10; return true; }
        int x, y;
        if (c == "combine") {
            int other = hud->slotOf(st.args[1]);
            if (other < 0) { fail(g, "second item not in the bar"); *failed = true; return false; }
            x = 0x40 + 0x40 * other + 32; y = 32;
        } else if (!locate(g, st.args[1], &x, &y)) { fail(g, "target not found / not on screen"); *failed = true; return false; }
        fprintf(stdout, "PLAYTHROUGH line %d: %s %s -> %s at %d,%d\n", st.line, c.c_str(), st.args[0].c_str(), st.args[1].c_str(), x, y);
        answers_.assign(st.args.begin() + 2, st.args.end());
        click(x, y);
        done = true;
    } else if (c == "map") {
        click(608, 32);
        done = true;
    } else if (c == "expect") {
        std::string e; for (size_t i = 0; i < st.args.size(); ++i) e += (i ? " " : "") + st.args[i];
        if (!g->cond(e.c_str())) { fail(g, "condition false"); *failed = true; return false; }
        fprintf(stdout, "PLAYTHROUGH line %d: expect %s: ok\n", st.line, e.c_str());
        done = true;
    } else if (c == "have") {
        if (!g->hud() || st.args.empty() || g->hud()->slotOf(st.args[0]) < 0) { fail(g, "item not in the bar"); *failed = true; return false; }
        done = true;
    } else if (c == "shot") {
        const char* dir = getenv("SB_TEST_SHOTDIR");
        std::string p = std::string(dir ? dir : ".") + "/" + (st.args.empty() ? "shot" : st.args[0]) + ".bmp";
        omf_request_shot(p.c_str());
        fprintf(stdout, "PLAYTHROUGH line %d: shot %s\n", st.line, p.c_str());
        done = true;
    } else if (c == "save") {
        save(g, st.args.empty() ? 0 : atoi(st.args[0].c_str()));
        done = true;
    } else if (c == "end") {
        fprintf(stdout, "PLAYTHROUGH DONE (%u steps, scene %s)\n", (unsigned)steps_.size(), g->scene() ? g->scene()->name().c_str() : "-");
        fflush(stdout);
        cur_ = steps_.size();
        return false;
    } else { fail(g, "unknown step"); *failed = true; return false; }
    if (done) { ++cur_; phase_ = 0; frames_ = 0; idleFrames_ = 0; waitLeft_ = 2; }
    return true;
}

}  // namespace sbport
