/*  The game cursor object.  sb2.exe has no pointer art of its own: it uses the Windows arrow, and the hourglass
    (OMediaMouseCursor::set_shape(Arrow / Clock), OMT.dll 0x10395090) while a scene loads; an item taken from the
    inventory follows the pointer as its own little 3D picture (Hud.cpp, 0x4201c0).  The port replaces the system
    cursor by a sprite in the 2D world so that the same cursor is drawn on the desktop and under a finger on the
    Android touch screen (the pointer there is the last touch).  The sprites are drawn in code (no art copied).
    // port: SB_CURSOR selects original Spatula (default), Net, Bubble or Classic sprite sets.
    Shapes: Normal, Hotspot (clickable), Hand (item in hand), Wait.   */
#pragma once
#include "OMediaCanvasElement.h"
#include "OMediaCanvas.h"

namespace sbgame {

class Game;

class Cursor {
public:
    enum Shape { Normal = 0, Hotspot, Hand, Wait, ShapeCount };
    explicit Cursor(Game* g);
    ~Cursor();
    bool ok() const { return sprite_ != 0; }
    Shape shape() const { return shape_; }
    // once a frame: chooses the shape from the game state and puts the sprite at the pointer
    void update(int x, int y);
    void set(Shape s);
    void setTheme(int t);                  // lane c: the port menu changes the cursor theme live (0 Spatula, 1 Net, 2 Bubble, 3 Classic)
    int theme() const { return theme_; }
private:
    void build(int i, const char* const* rows, int h, unsigned fill, unsigned fill2);
    void buildAll();
    void buildTheme(int state, int frame); // port: original procedural cursor themes
    enum { Frames = 12 };
    int theme_, frame_;
    OMediaCanvas* themed_[ShapeCount][Frames];
    Game* g_;
    OMediaCanvas* canv_[ShapeCount];
    OMediaCanvasElement* sprite_;
    Shape shape_;
    int hotX_[ShapeCount], hotY_[ShapeCount];
    bool sysHidden_, tried_;
};

}  // namespace sbgame
