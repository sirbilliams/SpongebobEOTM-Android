/*  port: hotspot reveal (docs/reveal-and-gallery.md).  Briefly outlines everything clickable in the scene, with its caption, drawn in code in
    the 640x480 logical world (so every widescreen mode shows it through the same transform as the picture).  The click rectangles are the
    game's own pick data (GameScene::collectClickable mirrors the 0x43a310 passes).  Triggers: hold() while L1 / Tab is down, pulse() for the
    companion command 'reveal' (about 2 s).  */
#pragma once
#include <SDL.h>
#include <string>
#include <vector>

class OMediaCanvas;
class OMediaCanvasElement;
class OMediaCCaption;

namespace sbgame {

class Game;

class Reveal {
public:
    explicit Reveal(Game* g);
    ~Reveal();
    void hold(bool on) { held_ = on; }
    void pulse() { pulseMs_ = 1200.0f; }            // 400 ms in + 1200 ms + 400 ms out
    void update(float dtMs);
    float level() const { return level_; }
    int count() const { return (int)caps_.size(); }
private:
    struct Spot { int l, t, r, b; std::string text; };
    void collect(std::vector<Spot>& out);
    void draw(const std::vector<Spot>& spots);
    void clearCaps();
    void release();
    Game* g_;
    bool held_;
    float pulseMs_, level_;
    unsigned tick_;
    OMediaCanvas* canvas_;
    OMediaCanvasElement* elem_;
    unsigned* pix_;
    int stride_;
    std::vector<OMediaCCaption*> caps_;
    std::string sig_;
    bool capsShown_;
};

// L1 (SDL_CONTROLLER_BUTTON_LEFTSHOULDER) and Tab: true if the event was one of them
bool revealEvent(Game* g, const SDL_Event& ev);

}  // namespace sbgame
