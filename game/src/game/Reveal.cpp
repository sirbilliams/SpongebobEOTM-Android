// port: hotspot reveal (Reveal.h, docs/reveal-and-gallery.md)
#include "Reveal.h"
#include "Game.h"
#include "World3D.h"
#include "OMediaPixmapWorld.h"
#include "OMediaCanvasElement.h"
#include "OMediaCanvas.h"
#include "OMediaCCaption.h"
#include "OMediaRGBColor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace sbgame {

static const float kFadeMs = 400.0f;
static const int kW = 640, kH = 480;

Reveal::Reveal(Game* g)
    : g_(g), held_(false), pulseMs_(0), level_(0), tick_(0), canvas_(0), elem_(0), pix_(0), stride_(0), capsShown_(true) {}

Reveal::~Reveal() { release(); }

void Reveal::clearCaps()
{
    for (size_t i = 0; i < caps_.size(); ++i) delete caps_[i];
    caps_.clear();
}

void Reveal::release()
{
    clearCaps();
    delete elem_; elem_ = 0;
    delete canvas_; canvas_ = 0;
    pix_ = 0; sig_.clear();
}

void Reveal::collect(std::vector<Spot>& out)
{
    GameScene* sc = g_->scene();
    std::vector<SceneObject*> v;
    if (sc) sc->collectClickable(v);
    for (size_t i = 0; i < v.size(); ++i) {
        SceneObject* o = v[i];
        Spot s;
        s.l = o->x1 < o->x2 ? o->x1 : o->x2; s.r = o->x1 < o->x2 ? o->x2 : o->x1;
        s.t = o->y1 < o->y2 ? o->y1 : o->y2; s.b = o->y1 < o->y2 ? o->y2 : o->y1;
        if (s.r < 2 || s.l > kW - 3 || s.b < 2 || s.t > kH - 3) continue;          // off the picture
        if (s.l < 3) s.l = 3; if (s.t < 3) s.t = 3; if (s.r > kW - 4) s.r = kW - 4; if (s.b > kH - 4) s.b = kH - 4;
        s.text = o->description;
        out.push_back(s);
    }
}

static inline unsigned blend(unsigned old, int a, int r, int gg, int b)      // keep the stronger of two straight-alpha pixels
{
    if ((int)(old >> 24) >= a) return old;
    return ((unsigned)a << 24) | ((unsigned)r << 16) | ((unsigned)gg << 8) | (unsigned)b;
}

// Soft glowing rounded outline: a bright core, a cyan glow on both sides.  `e` = fade level 0..1.
static void outline(unsigned* pix, int stride, int l, int t, int r, int b, float e, float pulse)
{
    const float R = 6.0f, W = 8.0f;
    float cx = (l + r) * 0.5f, cy = (t + b) * 0.5f;
    float hx = (r - l) * 0.5f, hy = (b - t) * 0.5f;
    float rr = R < hx ? R : hx; if (rr > hy) rr = hy;
    int pad = (int)W + 2;
    int y0 = t - pad, y1 = b + pad, x0 = l - pad, x1 = r + pad;
    if (y0 < 0) y0 = 0; if (y1 > kH - 1) y1 = kH - 1; if (x0 < 0) x0 = 0; if (x1 > kW - 1) x1 = kW - 1;
    for (int y = y0; y <= y1; ++y) {
        bool band = y < t + pad || y > b - pad;
        for (int x = x0; x <= x1; ++x) {
            if (!band && x > l + pad && x < r - pad) x = r - pad;
            float qx = fabsf(x + 0.5f - cx) - (hx - rr), qy = fabsf(y + 0.5f - cy) - (hy - rr);
            float mx = qx > 0 ? qx : 0, my = qy > 0 ? qy : 0;
            float d = sqrtf(mx * mx + my * my) + (qx > qy ? (qx < 0 ? qx : 0) : (qy < 0 ? qy : 0)) - rr;
            float core = 2.1f - fabsf(d); if (core < 0) core = 0; if (core > 1) core = 1;
            float k = d > 0 ? 1.0f - d / W : 1.0f + d / W;
            float glow = k > 0 ? 0.75f * k * k * pulse : 0;
            int ac = (int)(255 * core * e), ag = (int)(255 * glow * e);
            unsigned* px = &pix[y * stride + x];
            float ink = 1.5f - fabsf(d - 2.8f); if (ink < 0) ink = 0; if (ink > 1) ink = 1;       // a dark edge so it reads on pale art
            int ai = (int)(255 * 0.65f * ink * e);
            if (ag > 0) *px = blend(*px, ag, 90, 220, 255);
            if (ai > 0) *px = blend(*px, ai, 12, 40, 60);
            if (ac > 0) *px = blend(*px, ac, 255, 244, 190);
        }
    }
}

void Reveal::draw(const std::vector<Spot>& spots)
{
    if (!canvas_) {
        OMediaPixBlitterWorld* wd = new OMediaPixBlitterWorld;
        OMediaRGBColor key(0xffff, 0, 0xffff);
        wd->create(kW, kH, NULL, &key);
        OMediaGfxPixels* p = wd->port_pixels();
        if (!p || !p->pix) { delete wd; return; }
        pix_ = (unsigned*)p->pix; stride_ = p->width;
        canvas_ = new OMediaCanvas; canvas_->setgfxworld(wd);
    }
    OMediaGfxPixels* p = ((OMediaPixBlitterWorld*)canvas_->getgfxworld())->port_pixels();
    pix_ = (unsigned*)p->pix; stride_ = p->width;
    memset(pix_, 0, sizeof(unsigned) * stride_ * kH);
    float pulse = 0.85f + 0.15f * sinf(SDL_GetTicks() * 0.006f);
    float e = level_ * level_ * (3.0f - 2.0f * level_);          // smoothstep
    for (size_t i = 0; i < spots.size(); ++i) outline(pix_, stride_, spots[i].l, spots[i].t, spots[i].r, spots[i].b, e, pulse);
    ++p->version;
    if (!elem_) {
        elem_ = new OMediaCanvasElement(canvas_);
        elem_->link(g_->world());
        elem_->set_clickable(false);
        elem_->place(0, 0, 10150.0f);
    }
}

void Reveal::update(float dtMs)
{
    ++tick_;
    if (const char* t = getenv("SB_TEST_REVEAL")) {        // port: tests only: "on", or "<tick>" = a pulse at that game frame
        if (!strcmp(t, "on")) held_ = true;
        else if (tick_ == (unsigned)atoi(t)) pulse();
    }
    GameScene* sc = g_->scene();
    bool busy = !sc || g_->pendingScene() || sc->letterboxed() || (sc->dialogue() && sc->dialogue()->busy()) || g_->runner();
    bool want = (held_ || pulseMs_ > 0) && !busy;
    if (pulseMs_ > 0) pulseMs_ -= dtMs;
    if (want && level_ == 0) g_->log("reveal: on\n");
    float step = dtMs / kFadeMs;
    level_ += want ? step : -step;
    if (level_ > 1) level_ = 1;
    if (level_ <= 0) {
        level_ = 0;
        if (elem_ || !caps_.empty()) { g_->log("reveal: off\n"); release(); }               // nothing shown: no element, no canvas
        return;
    }
    std::vector<Spot> spots;
    collect(spots);
    draw(spots);
    // captions: rebuilt when the set of hotspots changes; shown once the outlines are about a third in
    std::string sig;
    char b[64];
    for (size_t i = 0; i < spots.size(); ++i) { sprintf(b, "%d,%d,%d,%d;", spots[i].l, spots[i].t, spots[i].r, spots[i].b); sig += b; sig += spots[i].text; }
    if (sig != sig_) {
        sig_ = sig;
        clearCaps();
        capsShown_ = true;               // new captions are visible
        std::vector<int> used;       // caption boxes placed so far (l,t,r,b) so labels do not pile up
        for (size_t i = 0; i < spots.size(); ++i) {
            if (spots[i].text.empty()) continue;
            OMediaCCaption* c = new OMediaCCaption;
            c->set_font(g_->font());
            c->set_string(spots[i].text);
            c->link(g_->world());
            c->set_clickable(false);
            long w, h;
            c->getsize(w, h);
            int x = (spots[i].l + spots[i].r) / 2 - (int)w / 2;
            int y = spots[i].t - (int)h - 4;
            if (y < 4) y = spots[i].b + 4;
            if (y + (int)h > kH - 4) y = spots[i].t + 4;
            if (x < 4) x = 4; if (x + (int)w > kW - 4) x = kW - 4 - (int)w;
            for (int tries = 0; tries < 6; ++tries) {          // slide down past captions already there
                bool hit = false;
                for (size_t u = 0; u + 3 < used.size(); u += 4)
                    if (x < used[u + 2] && x + (int)w > used[u] && y < used[u + 3] && y + (int)h > used[u + 1]) { hit = true; y = used[u + 3] + 1; }
                if (!hit) break;
            }
            if (y + (int)h > kH - 2) y = kH - 2 - (int)h;
            used.push_back(x); used.push_back(y); used.push_back(x + (int)w); used.push_back(y + (int)h);
            c->place((float)x, (float)y, 10160.0f);
            caps_.push_back(c);
        }
    }
    bool showCaps = level_ > 0.35f;                           // hide() is counted: only on a change
    if (showCaps != capsShown_) { for (size_t i = 0; i < caps_.size(); ++i) caps_[i]->hide(!showCaps); capsShown_ = showCaps; }
}

bool revealEvent(Game* g, const SDL_Event& ev)
{
    if (!g || !g->reveal()) return false;
    if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) && ev.key.keysym.sym == SDLK_TAB) {
        if (ev.key.repeat) return true;
        g->reveal()->hold(ev.type == SDL_KEYDOWN);
        return true;
    }
    if ((ev.type == SDL_CONTROLLERBUTTONDOWN || ev.type == SDL_CONTROLLERBUTTONUP) && ev.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
        g->reveal()->hold(ev.type == SDL_CONTROLLERBUTTONDOWN);
        return true;
    }
    return false;
}

}  // namespace sbgame
