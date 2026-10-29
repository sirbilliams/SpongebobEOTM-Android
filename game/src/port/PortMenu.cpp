/*  port: the port menu (PortMenu.h, docs/port-menu.md).  */
#include "PortMenu.h"
#include "PortGfx.h"
#include "OMediaSDLRetarget.h"
#include "OMediaGLBackend.h"
#include "OMediaEventManager.h"
#include "OMediaTimeCounter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <jni.h>
#include <SDL_system.h>
#endif

void omf_SDLAudioPause(bool pause);        // OMediaSDLSound.cpp: the mixer (all voices and the Bink streams) stops / goes on

namespace sbport {

namespace {
const char* const kBottom[3] = { "Panel", "Touchpad", "Off" };
const char* const kCursor[4] = { "Spatula", "Net", "Bubble", "Classic" };
const char* const kWide[4] = { "Fill", "Side panels", "Stretch", "Off" };
const char* const kWideEnv[4] = { "fill", "panels", "stretch", "off" };
const char* const kCursorEnv[4] = { "spatula", "net", "bubble", "classic" };
const char* const kLabel[7] = { "Bottom screen", "Cursor theme", "Widescreen", "Right-stick cursor", "Diagnostics", "Resume", "Game menu" };
enum { ROWS = 7, R_BOTTOM = 0, R_CURSOR, R_WIDE, R_PAD, R_DIAG, R_RESUME, R_GAME };

const Uint32 kHoldMs = 600, kDoubleMs = 350;
const float kRowX = 650, kRowY = 262, kRowW = 620, kRowH = 80, kRowStep = 92;

bool g_clockPaused = false;
void lifeHook(bool off)
{
    omf_SDLAudioPause(off);                                   // the mixer: music and sounds stop and keep their place
    if (off && !OMediaTimeCounter::is_paused_millisecs()) { OMediaTimeCounter::pause_millisecs(true); g_clockPaused = true; }
    else if (!off && g_clockPaused) { OMediaTimeCounter::pause_millisecs(false); g_clockPaused = false; }
    SDL_Log("port: %s (mixer %s, clock %s)", off ? "suspended" : "resumed", off ? "paused" : "running", off ? "stopped" : "running");
}

void overlayThunk(int dw, int dh) { PortMenu::get().draw(dw, dh); }

int wrap(int v, int n) { return ((v % n) + n) % n; }
Rgba pal(unsigned rgb, unsigned a = 255) { return (a << 24) | rgb; }

// ---- persistence: Android -> PortPrefs.java (the same SharedPreferences the launcher uses); the desktop has only the environment of the run
#ifdef __ANDROID__
jclass prefsClass(JNIEnv* env, jobject act)
{
    jclass actCls = env->GetObjectClass(act);
    jmethodID getLoader = env->GetMethodID(actCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = env->CallObjectMethod(act, getLoader);
    jclass loaderCls = env->GetObjectClass(loader);
    jmethodID load = env->GetMethodID(loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring name = env->NewStringUTF("com.sirbilliams.spongebob.PortPrefs");
    jclass c = (jclass)env->CallObjectMethod(loader, load, name);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return NULL; }
    return c;
}
bool loadPrefs(PortSettings& s)
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject act = (jobject)SDL_AndroidGetActivity();
    if (!env || !act) return false;
    jclass c = prefsClass(env, act);
    if (!c) return false;
    jmethodID m = env->GetStaticMethodID(c, "load", "(Landroid/app/Activity;)[I");
    if (!m) { env->ExceptionClear(); return false; }
    jintArray a = (jintArray)env->CallStaticObjectMethod(c, m, act);
    if (env->ExceptionCheck() || !a || env->GetArrayLength(a) < 5) { env->ExceptionClear(); return false; }
    jint v[5]; env->GetIntArrayRegion(a, 0, 5, v);
    s.bottom = wrap(v[0], 3); s.cursor = wrap(v[1], 4); s.widescreen = wrap(v[2], 4); s.padCursor = v[3] != 0; s.diag = v[4] != 0;
    return true;
}
void savePrefs(const PortSettings& s)
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject act = (jobject)SDL_AndroidGetActivity();
    if (!env || !act) return;
    jclass c = prefsClass(env, act);
    if (!c) return;
    jmethodID m = env->GetStaticMethodID(c, "save", "(Landroid/app/Activity;IIIII)V");
    if (!m) { env->ExceptionClear(); return; }
    env->CallStaticVoidMethod(c, m, act, (jint)s.bottom, (jint)s.cursor, (jint)s.widescreen, (jint)(s.padCursor ? 1 : 0), (jint)(s.diag ? 1 : 0));
    if (env->ExceptionCheck()) env->ExceptionClear();
}
#else
bool loadPrefs(PortSettings&) { return false; }
void savePrefs(const PortSettings& s)
{
    SDL_Log("port menu: (desktop) would save bottom %d cursor %d widescreen %d pad %d diag %d", s.bottom, s.cursor, s.widescreen, s.padCursor, s.diag);
}
#endif

int envIndex(const char* name, const char* const* names, int n, int dflt)
{
    const char* v = getenv(name);
    if (!v) return dflt;
    for (int i = 0; i < n; ++i) if (!strcmp(v, names[i])) return i;
    return dflt;
}
}  // namespace

PortMenu& PortMenu::get() { static PortMenu m; return m; }

PortMenu::PortMenu()
    : open_(false), gameMenu_(false), holding_(false), tapPending_(false), ignoreUp_(false), swallowUp_(false), downAt_(0), tapAt_(0), sel_(0),
      ptrX_(-1), ptrY_(-1), scale_(1), offX_(0), offY_(0)
{
    scene_[0] = 0;
}

void PortMenu::install()
{
    static bool done = false;
    if (done) return;
    done = true;
    // the starting values: the launcher's preferences (Android) or the environment of this run (desktop)
    s_.widescreen = envIndex("SB_WIDESCREEN", kWideEnv, 4, 0);
    s_.cursor = envIndex("SB_CURSOR", kCursorEnv, 4, 0);
    { const char* v = getenv("SB_PAD_CURSOR"); s_.padCursor = !(v && (v[0] == '0' || v[0] == 'n' || v[0] == 'f')); }
    { const char* v = getenv("SB_BOTTOM"); s_.bottom = v ? (!strcmp(v, "off") ? BOTTOM_OFF : !strcmp(v, "touchpad") ? BOTTOM_PAD : BOTTOM_PANEL) : BOTTOM_PANEL; }
    { const char* v = getenv("SB_DIAG"); s_.diag = v && *v && *v != '0'; }
    if (loadPrefs(s_)) SDL_Log("port menu: settings from the launcher preferences (bottom %d, cursor %d, widescreen %d, pad %d, diagnostics %d)",
                               s_.bottom, s_.cursor, s_.widescreen, s_.padCursor, s_.diag);
    apply(false);
    omgl_set_overlay(overlayThunk);
    OMediaEventManager::set_lifecycle_hook(lifeHook);
}

void PortMenu::setScene(const char* name)
{
    if (!name) name = "";
    strncpy(scene_, name, sizeof scene_ - 1);
    scene_[sizeof scene_ - 1] = 0;
}

void PortMenu::apply(bool save)
{
    setenv("SB_WIDESCREEN", kWideEnv[s_.widescreen], 1);
    setenv("SB_CURSOR", kCursorEnv[s_.cursor], 1);
    setenv("SB_PAD_CURSOR", s_.padCursor ? "1" : "0", 1);
    OMediaEventManager::set_pad_cursor(s_.padCursor);
    if (save) { savePrefs(s_); SDL_Log("port menu: bottom %s, cursor %s, widescreen %s, right-stick cursor %s, diagnostics %s", kBottom[s_.bottom], kCursor[s_.cursor],
                                         kWide[s_.widescreen], s_.padCursor ? "on" : "off", s_.diag ? "on" : "off"); }
}

void PortMenu::openMenu()
{
    if (open_) return;
    open_ = true; sel_ = 0; holding_ = tapPending_ = false;
    SDL_Log("port menu: opened");
}

void PortMenu::closeMenu()
{
    if (!open_) return;
    open_ = false;
    SDL_Log("port menu: closed");
}

bool PortMenu::takeGameMenu() { bool r = gameMenu_; gameMenu_ = false; return r; }

void PortMenu::tick()
{
    Uint32 now = SDL_GetTicks();
    if (holding_ && now - downAt_ >= kHoldMs) { holding_ = false; ignoreUp_ = true; openMenu(); }
    if (tapPending_ && now - tapAt_ >= kDoubleMs) { tapPending_ = false; gameMenu_ = true; }       // one short tap: what Back always did
}

void PortMenu::change(int row, int dir)
{
    switch (row) {
    case R_BOTTOM: s_.bottom = wrap(s_.bottom + dir, 3); break;
    case R_CURSOR: s_.cursor = wrap(s_.cursor + dir, 4); break;
    case R_WIDE: s_.widescreen = wrap(s_.widescreen + dir, 4); break;
    case R_PAD: s_.padCursor = !s_.padCursor; break;
    case R_DIAG: s_.diag = !s_.diag; break;
    default: return;
    }
    apply(true);
}

void PortMenu::activate(int row)
{
    if (row == R_RESUME) closeMenu();
    else if (row == R_GAME) { closeMenu(); gameMenu_ = true; }
    else change(row, 1);
}

void PortMenu::toVirtual(int wx, int wy, float* vx, float* vy)
{
    SDL_Window* w = SDL_GL_GetCurrentWindow();
    int ww = 1, wh = 1, dw = 1, dh = 1;
    if (w) { SDL_GetWindowSize(w, &ww, &wh); SDL_GL_GetDrawableSize(w, &dw, &dh); }
    float px = wx * (float)dw / (ww > 0 ? ww : 1), py = wy * (float)dh / (wh > 0 ? wh : 1);
    float s = (float)dw / 1920.0f; if ((float)dh / 1080.0f < s) s = (float)dh / 1080.0f;
    *vx = (px - (dw - 1920 * s) * 0.5f) / s;
    *vy = (py - (dh - 1080 * s) * 0.5f) / s;
}

bool PortMenu::rowAt(int wx, int wy, int* row)
{
    float vx, vy; toVirtual(wx, wy, &vx, &vy);
    if (vx < kRowX || vx > kRowX + kRowW) return false;
    int r = (int)((vy - kRowY) / kRowStep);
    if (vy < kRowY || r < 0 || r >= ROWS || vy > kRowY + r * kRowStep + kRowH) return false;
    *row = r;
    return true;
}

void PortMenu::command(const char* n)
{
    SDL_Log("port menu: test command %s (row %d)", n, sel_);
    if (!strcmp(n, "open")) openMenu();
    else if (!strcmp(n, "close")) closeMenu();
    else if (!open_) return;
    else if (!strcmp(n, "up")) sel_ = wrap(sel_ - 1, ROWS);
    else if (!strcmp(n, "down")) sel_ = wrap(sel_ + 1, ROWS);
    else if (!strcmp(n, "left")) change(sel_, -1);
    else if (!strcmp(n, "right")) change(sel_, 1);
    else if (!strcmp(n, "enter")) activate(sel_);
}

bool PortMenu::filter(const SDL_Event& e)
{
    Uint32 now = SDL_GetTicks();
    // ---- the Back / F9 gestures
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
        SDL_Keycode k = e.key.keysym.sym;
        bool back = k == SDLK_AC_BACK, f9 = k == SDLK_F9;
        if (back || f9) {
            if (e.type == SDL_KEYDOWN) {
                if (e.key.repeat) return true;
                if (open_) { closeMenu(); ignoreUp_ = true; return true; }
                if (back && tapPending_) { tapPending_ = false; ignoreUp_ = true; openMenu(); return true; }   // the second tap
                holding_ = true; downAt_ = now; ignoreUp_ = false;
            } else {
                if (ignoreUp_) { ignoreUp_ = false; holding_ = false; return true; }
                if (holding_) {
                    holding_ = false;
                    if (back) { tapPending_ = true; tapAt_ = now; }          // might become a double tap
                    // F9 released early: nothing (hold it to open the menu)
                }
            }
            return true;
        }
    }
    if (!open_) {
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && swallowUp_) { swallowUp_ = false; return true; }   // the up of the tap that closed the menu
        return false;
    }

    // ---- the open menu takes the keys, the pad's d-pad and B, and the pointer
    switch (e.type) {
    case SDL_KEYDOWN: {
        SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_UP) sel_ = wrap(sel_ - 1, ROWS);
        else if (k == SDLK_DOWN || k == SDLK_TAB) sel_ = wrap(sel_ + 1, ROWS);
        else if (k == SDLK_LEFT) change(sel_, -1);
        else if (k == SDLK_RIGHT) change(sel_, 1);
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) activate(sel_);
        else if (k == SDLK_ESCAPE) closeMenu();
        else if (k == SDLK_F10) { closeMenu(); gameMenu_ = true; }
        return true;
    }
    case SDL_KEYUP: return true;
    case SDL_CONTROLLERBUTTONDOWN:
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: sel_ = wrap(sel_ - 1, ROWS); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: sel_ = wrap(sel_ + 1, ROWS); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: change(sel_, -1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: change(sel_, 1); break;
        case SDL_CONTROLLER_BUTTON_B: closeMenu(); break;
        default: break;                         // A / R3 are the pointer's button (OMediaEventManager): they click where the pointer is
        }
        return true;
    case SDL_CONTROLLERBUTTONUP: case SDL_CONTROLLERAXISMOTION: return true;
    case SDL_MOUSEMOTION: {
        bool moved = ptrX_ >= 0 && (abs(e.motion.x - ptrX_) > 2 || abs(e.motion.y - ptrY_) > 2);   // a pointer that only rests there does not take the highlight
        if (moved || ptrX_ < 0) { ptrX_ = e.motion.x; ptrY_ = e.motion.y; }
        int r; if (moved && rowAt(ptrX_, ptrY_, &r)) sel_ = r;
        return true;
    }
    case SDL_MOUSEBUTTONDOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
            ptrX_ = e.button.x; ptrY_ = e.button.y;
            int r; if (rowAt(ptrX_, ptrY_, &r)) { sel_ = r; activate(r); }
            swallowUp_ = true;
        }
        return true;
    case SDL_MOUSEBUTTONUP: swallowUp_ = false; return true;
    case SDL_MOUSEWHEEL: return true;
    default: return false;
    }
}

// ---------------------------------------------------------------------------------------------- drawing

void PortMenu::draw(int dw, int dh)
{
    if (open_) {
        float s = (float)dw / 1920.0f; if ((float)dh / 1080.0f < s) s = (float)dh / 1080.0f;
        float ox = (dw - 1920 * s) * 0.5f, oy = (dh - 1080 * s) * 0.5f;
        #define X(v) (ox + (v) * s)
        #define Y(v) (oy + (v) * s)
        #define L(v) ((v) * s)
        gfxBegin(dw, dh);
        gfxFill(0, 0, (float)dw, (float)dh, pal(0x021e35, 165));
        // rope frame around the timber board (the look of the bottom panel / launcher): twisted strands on the rope, planks inside
        gfxRound(X(600), Y(120), L(720), L(840), L(40), pal(0xd5ac68));
        for (float y = 150; y < 940; y += 19) { gfxLine(X(601), Y(y), X(617), Y(y + 12), L(3), pal(0x866137)); gfxLine(X(1303), Y(y), X(1319), Y(y + 12), L(3), pal(0x866137)); }
        for (float x = 640; x < 1290; x += 19) { gfxLine(X(x), Y(121), X(x + 12), Y(137), L(3), pal(0x866137)); gfxLine(X(x), Y(943), X(x + 12), Y(959), L(3), pal(0x866137)); }
        gfxFill(X(622), Y(142), L(676), L(796), pal(0x684330));
        for (int i = 0; i < 10; ++i) {
            float y = 142 + i * 80;
            gfxFill(X(622), Y(y), L(676), L(i == 9 ? 76 : 80), pal(i % 2 ? 0x734c34 : 0x684330));
            gfxFill(X(622), Y(y + 79), L(676), L(3), pal(0x382e28));
            gfxLine(X(640), Y(y + 17), X(1280), Y(y + 22), L(2), pal(0x8c6140, 160));
        }
        gfxText(X(960), Y(150), L(54), "PORT MENU", pal(0xffe9a6), 1);
        gfxText(X(960), Y(222), L(22), "game paused", pal(0xb0d8db), 1);
        for (int i = 0; i < ROWS; ++i) {
            float y = kRowY + i * kRowStep;
            bool sel = i == sel_;
            gfxRound(X(kRowX), Y(y), L(kRowW), L(kRowH), L(40), pal(sel ? 0xffd47d : 0xb59255));
            gfxRound(X(kRowX + 8), Y(y + 7), L(kRowW - 16), L(kRowH - 14), L(33), pal(sel ? 0x106d7b : 0x143d4b));
            gfxDisc(X(kRowX + 36), Y(y + 40), L(8), pal(0xebc77c));
            gfxDisc(X(kRowX + kRowW - 36), Y(y + 40), L(8), pal(0xebc77c));
            Rgba tc = pal(sel ? 0xffe6a5 : 0xd9e5df);
            char val[48] = "";
            switch (i) {
            case R_BOTTOM: snprintf(val, sizeof val, "< %s >", kBottom[s_.bottom]); break;
            case R_CURSOR: snprintf(val, sizeof val, "< %s >", kCursor[s_.cursor]); break;
            case R_WIDE: snprintf(val, sizeof val, "< %s >", kWide[s_.widescreen]); break;
            case R_PAD: snprintf(val, sizeof val, "< %s >", s_.padCursor ? "On" : "Off"); break;
            case R_DIAG: snprintf(val, sizeof val, "< %s >", s_.diag ? "On" : "Off"); break;
            default: break;
            }
            if (i >= R_RESUME) gfxText(X(kRowX + kRowW * 0.5f), Y(y + 16), L(36), kLabel[i], tc, 1);
            else {
                gfxText(X(kRowX + 60), Y(y + 17), L(32), kLabel[i], tc, 0);
                gfxText(X(kRowX + kRowW - 60), Y(y + 17), L(32), val, pal(sel ? 0xffd47d : 0xa9e5dd), 2);
            }
        }
        gfxText(X(960), Y(904), L(22), "D-pad: move / change     B or Back: resume", pal(0xc9dfd7), 1);
        if (ptrX_ >= 0) {                         // the pointer (finger, mouse or right stick), as a bubble
            SDL_Window* w = SDL_GL_GetCurrentWindow(); int ww = 1, wh = 1; if (w) SDL_GetWindowSize(w, &ww, &wh);
            float px = ptrX_ * (float)dw / ww, py = ptrY_ * (float)dh / wh;
            gfxRing(px, py, L(12), L(3), pal(0xffffff, 230));
            gfxDisc(px, py, L(4), pal(0xffe6a5, 230));
        }
        gfxEnd();
        #undef X
        #undef Y
        #undef L
    }
    DiagFrame(dw, dh, s_.diag, s_, scene_);
}

}  // namespace sbport
