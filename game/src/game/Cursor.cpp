#include "Cursor.h"
#include "Game.h"
#include "Hud.h"
#include "World3D.h"
#include "OMediaPixmapWorld.h"
#include "OMediaRGBColor.h"
#include <SDL.h>
#include <string.h>
#include <stdlib.h>

namespace sbgame {

// '#' outline, '.' fill, 'o' second fill, ' ' transparent.  Drawn here, not taken from the game.
static const char* const kArrow[] = {
    "#           ", "##          ", "#.#         ", "#..#        ", "#...#       ", "#....#      ", "#.....#     ",
    "#......#    ", "#.......#   ", "#........#  ", "#.........# ", "#......#####", "#...#..#    ", "#..# #..#   ",
    "#.#  #..#   ", "##    #..#  ", "#     #..#  ", "       ##   " };
static const char* const kHand[] = {                                    // pointing hand: a hotspot is under it
    "    ##          ", "   #..#         ", "   #..#         ", "   #..#         ", "   #..###       ", "   #..#..###    ",
    "####..#..#..#   ", "#..##.....#..#  ", "#...#........#  ", " #...........#  ", "  #..........#  ", "  #.........#   ",
    "   #........#   ", "   #.......#    ", "    #......#    ", "    ########    " };
static const char* const kWait[] = {                                    // hourglass
    "##############", "#............#", " #..........# ", "  #........#  ", "   #..oo..#   ", "    #.oo.#    ",
    "     #oo#     ", "     #oo#     ", "    #.oo.#    ", "   #..oo..#   ", "  #..oooo..#  ", " #..oooooo..# ",
    "#oooooooooooo#", "##############" };

Cursor::Cursor(Game* g) : g_(g), sprite_(0), shape_(Normal), sysHidden_(false), tried_(false)
{
    for (int i = 0; i < ShapeCount; ++i) { canv_[i] = 0; hotX_[i] = hotY_[i] = 0; }
    build(Normal, kArrow, 18, 0xFFFFFFFF, 0);
    build(Hotspot, kHand, 16, 0xFFFFE070, 0);
    hotX_[Hotspot] = 4; hotY_[Hotspot] = 0;
    build(Hand, kArrow, 18, 0xFFFFD43B, 0);                          // the arrow in gold: the item is the picture
    build(Wait, kWait, 14, 0xFFFFFFFF, 0xFFE8A820);
    hotX_[Wait] = 7; hotY_[Wait] = 7;
}

Cursor::~Cursor()
{
    delete sprite_;
    for (int i = 0; i < ShapeCount; ++i) delete canv_[i];
    if (sysHidden_) SDL_ShowCursor(SDL_ENABLE);
}

void Cursor::build(int i, const char* const* rows, int h, unsigned fill, unsigned fill2)
{
    int w = (int)strlen(rows[0]);
    OMediaPixBlitterWorld* wd = new OMediaPixBlitterWorld;
    OMediaRGBColor key(0xffff, 0, 0xffff);
    wd->create((short)w, (short)h, NULL, &key);
    OMediaGfxPixels* p = wd->port_pixels();
    if (!p || !p->pix) { delete wd; return; }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            char c = (x < (int)strlen(rows[y])) ? rows[y][x] : ' ';
            unsigned v = 0;                                                  // alpha 0: transparent
            if (c == '#') v = 0xFF000000; else if (c == '.') v = fill; else if (c == 'o') v = fill2;
            p->pix[y * p->width + x] = v;
        }
    p->version++;
    OMediaCanvas* c = new OMediaCanvas;
    c->setgfxworld(wd);
    canv_[i] = c;
}

void Cursor::set(Shape s)
{
    if (s == shape_ || !sprite_ || !canv_[s]) return;
    shape_ = s;
    sprite_->setcanvas(canv_[s]);
}

void Cursor::update(int x, int y)
{
    if (!sprite_ && !tried_ && g_->scene()) {          // port: made with the first scene (see Hud::update)
        tried_ = true;
    if (canv_[Normal] && g_->world()) {
        sprite_ = new OMediaCanvasElement(canv_[Normal]);
        sprite_->link(g_->world());
        sprite_->place(-100, -100, 10500.0f);
        if (getenv("SB_NO_GAME_CURSOR")) { delete sprite_; sprite_ = 0; }      // port: tests only
    }
    }
    if (!sprite_) return;
    if (!sysHidden_) { SDL_ShowCursor(SDL_DISABLE); sysHidden_ = true; }     // port: the sprite replaces the system cursor
    Shape s = Normal;
    Hud* hud = g_->hud();
    if (g_->pendingScene()) s = Wait;
    else if (hud && hud->holding()) s = Hand;
    else if (hud && hud->overHotspot(x, y)) s = Hotspot;
    if (getenv("SB_TEST_CURSOR")) s = (Shape)(atoi(getenv("SB_TEST_CURSOR")) % ShapeCount);   // port: tests only
    set(s);
    sprite_->link(g_->world());                                       // port: last in the list = drawn last (the world draws in link order)
    if (getenv("SB_HUD_DEBUG")) { static int n = 0; if (++n % 60 == 0) g_->log("cursor: shape %d hide %d size %dx%d at %d,%d\n", (int)s, sprite_->get_hide_count(), canv_[s]->getwidth(), canv_[s]->getheight(), x, y); }
    // port: the picture is the 640x480 logical screen, so a pointer at its right / bottom edge (the right-stick cursor in SB_WIDESCREEN=panels stops
    // there; it cannot enter the side panels) would show a sprite cut to 1 px.  The sprite is kept fully inside the picture; the click point stays x,y.
    int px = x - hotX_[s], py = y - hotY_[s];
    int w = canv_[s]->getwidth(), h = canv_[s]->getheight();
    if (px > 640 - w) px = 640 - w;
    if (py > 480 - h) py = 480 - h;
    sprite_->place((float)px, (float)py, 10500.0f);
}

}  // namespace sbgame
