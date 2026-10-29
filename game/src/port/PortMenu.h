/*  port: the port menu and the diagnostics overlay (not in the original).  docs/port-menu.md.

    Hold Android Back for 0.6 s (or tap it twice within 0.35 s; desktop: hold F9) while playing and a menu opens over the game: bottom screen
    (Panel / Touchpad / Off), cursor theme, widescreen, right-stick cursor, diagnostics, Resume, Game menu.  A single short Back still
    does what it did (the game menu), 0.35 s after the release.  The game stands still while the menu is open.  Changes apply at once and are
    saved where the launcher keeps them (Android: PortPrefs.java through JNI; desktop: the SB_* environment of the run).

    The menu and the diagnostics are drawn in code on top of the presented picture (omgl_set_overlay), so they are in screenshots at any size.  */
#pragma once
#include <SDL.h>

namespace sbport {

enum { BOTTOM_PANEL = 0, BOTTOM_PAD = 1, BOTTOM_OFF = 2 };

struct PortSettings {
    PortSettings() : bottom(BOTTOM_PANEL), cursor(0), widescreen(0), padCursor(true), diag(false) {}
    int bottom;         // BOTTOM_*
    int cursor;         // 0 Spatula, 1 Net, 2 Bubble, 3 Classic
    int widescreen;     // 0 Fill, 1 Side panels, 2 Stretch, 3 Off
    bool padCursor;     // right-stick cursor
    bool diag;          // diagnostics overlay
};

class PortMenu {
public:
    static PortMenu& get();
    void install();                         // once the window exists: overlay hook, suspend hook, settings from the launcher's preferences
    bool filter(const SDL_Event& e);        // game loop: true = the event is the menu's (Back / F9 gestures, everything while it is open)
    void tick();                            // game loop, once per iteration: the hold / double-tap timers
    bool isOpen() const { return open_; }
    bool takeGameMenu();                    // true once: a short Back or "Game menu" was chosen (the loop then does what F10 does)
    void openMenu();
    void closeMenu();
    void command(const char* name);         // tests (SB_TEST_PM): open, close, up, down, left, right, enter
    const PortSettings& settings() const { return s_; }
    void setScene(const char* name);        // diagnostics: the scene being played ("" none)
    void draw(int dw, int dh);              // the overlay hook (omgl_present)

    PortSettings s_;
private:
    PortMenu();
    void change(int row, int dir);
    void apply(bool save);
    void activate(int row);
    bool rowAt(int wx, int wy, int* row);
    void toVirtual(int wx, int wy, float* vx, float* vy);

    bool open_, gameMenu_, holding_, tapPending_, ignoreUp_, swallowUp_;
    Uint32 downAt_, tapAt_;
    int sel_;
    int ptrX_, ptrY_;
    float scale_, offX_, offY_;
    char scene_[96];
};

// diagnostics (Diagnostics.cpp): called once per presented frame; draws the corner box when on and logs every 10 s
void DiagFrame(int dw, int dh, bool draw, const PortSettings& s, const char* scene);

}  // namespace sbport
