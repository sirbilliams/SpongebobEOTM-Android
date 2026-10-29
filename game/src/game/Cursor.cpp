#include "Cursor.h"
#include "Game.h"
#include "Hud.h"
#include "World3D.h"
#include "OMediaPixmapWorld.h"
#include "OMediaRGBColor.h"
#include <SDL.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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

// port: original nautical art, sampled at 4x4 coverage for smooth outlined edges at Thor scale.
namespace {
const unsigned ink = 0xff101820, gold = 0xffffd64e, wood = 0xffb97436, white = 0xfff4ffff;
double segment(double x,double y,double ax,double ay,double bx,double by) {
    double dx=bx-ax,dy=by-ay,t=((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy);
    if(t<0)t=0; if(t>1)t=1;
    return sqrt((x-ax-t*dx)*(x-ax-t*dx)+(y-ay-t*dy)*(y-ay-t*dy));
}
void stroke(unsigned& c,double d,double r,unsigned fill) { if(d<r+1.1)c=ink; if(d<r)c=fill; }
void bubble(unsigned& c,double x,double y,double cx,double cy,double r,bool ring) {
    double d=sqrt((x-cx)*(x-cx)+(y-cy)*(y-cy));
    if(d<r+1.2 && d>r-1.8)c=ink;
    if(d<r && d>r-1.4)c=0xff83e5ee;
    if(!ring && d<r-1.8)c=0x3877dcec;
    if(segment(x,y,cx-r*.45,cy-r*.63,cx-r*.68,cy-r*.2)<1.1)c=white;
    if(segment(x,y,cx+r*.5,cy+r*.4,cx+r*.65,cy+r*.15)<.65)c=0xffb9fbff;
}
}
void Cursor::buildTheme(int state,int frame)
{
    const int size=state==Hand?76:44;
    OMediaPixBlitterWorld* wd=new OMediaPixBlitterWorld;
    OMediaRGBColor key(0xffff,0,0xffff); wd->create(size,size,NULL,&key);
    OMediaGfxPixels* p=wd->port_pixels();
    if(!p || !p->pix){delete wd;return;}
    double phase=frame*6.28318530718/Frames;
    for(int py=0;py<size;++py)for(int px=0;px<size;++px) {
        unsigned sumA=0,sumR=0,sumG=0,sumB=0;
        for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx) {
            double x=px+(sx+.5)/4,y=py+(sy+.5)/4; unsigned c=0;
            if(state==Hand) {
                bubble(c,x,y,38,38,35,true);
                if(theme_==1) { // mesh at the rim leaves the held item's centre readable
                    double d=sqrt((x-38)*(x-38)+(y-38)*(y-38));
                    if(d<33 && d>27 && (fmod(x+y,9)<.8 || fmod(x-y+100,9)<.8))c=0xffa2dcd8;
                    stroke(c,segment(x,y,62,62,73,73),1.6,wood);
                }
            } else if(theme_==2) {
                if(state==Wait) {
                    for(int b=0;b<3;++b) {double a=phase+b*6.28318530718/3; bubble(c,x,y,20+11*cos(a),20+11*sin(a),5,false);}
                } else {
                    // pointed tail is the exact (4,4) hotspot
                    if(x>=4 && y>=4 && x<14 && y<14 && fabs(x-y)<(x-4)*.6+0.6)c=ink;
                    if(x>6 && y>6 && x<14 && y<14 && fabs(x-y)<(x-6)*.4)c=0xff83e5ee;
                    bubble(c,x,y,18,18,12,false);
                    if(state==Hotspot)bubble(c,x,y,18,18,16,true);
                }
            } else {
                double angle=state==Wait ? (theme_==1 ? .45*sin(phase) : phase) : state==Hotspot ? -.18+.10*sin(phase) : 0;
                double ox=state==Wait?20:4,oy=state==Wait?20:4;
                double tx=x-ox,ty=y-oy;
                double u=tx*cos(angle)+ty*sin(angle),v=-tx*sin(angle)+ty*cos(angle);
                if(state==Wait){u=u/.8+12;v=v/.8+12;}
                if(theme_==0) {
                    stroke(c,segment(u,v,12,16,25,31),2.2,wood);
                    stroke(c,segment(u,v,13,16,18,22),1.6,gold);
                    // slotted, yellow-edged steel head; its upper corner is the pointer tip.
                    if(u>=0 && v>=0 && u<=16 && v<=18 && u+v<29) {
                        c=ink;
                        if(u>1.2 && v>1.2 && u<14.8 && v<16.8 && u+v<27.5)c=gold;
                        if(u>3 && v>3 && u<13 && v<14)c=0xffb6cbd0;
                        for(int k=0;k<3;++k)if(segment(u,v,4+k*3,5,4+k*3,11)<.65)c=ink;
                    }
                } else {
                    stroke(c,segment(u,v,17,19,29,33),1.8,wood);
                    double dx=u-10,dy=v-11,d=sqrt(dx*dx/81+dy*dy/100);
                    if(d<1.10)c=ink;
                    if(d<.97)c=gold;
                    if(d<.82) {
                        c=0;
                        if(fmod(u+v+60,5)<.7 || fmod(u-v+60,5)<.7)c=0xff92c8ce;
                    }
                }
                if(state==Hotspot) {
                    stroke(c,segment(x,y,31,6,31,14),.7,white);
                    stroke(c,segment(x,y,27,10,35,10),.7,white);
                }
            }
            unsigned a=c>>24;sumA+=a;sumR+=((c>>16)&255)*a;sumG+=((c>>8)&255)*a;sumB+=(c&255)*a;
        }
        p->pix[py*p->width+px]=sumA ? ((sumA/16)<<24)|((sumR/sumA)<<16)|((sumG/sumA)<<8)|(sumB/sumA):0;
    }
    ++p->version; OMediaCanvas* canvas=new OMediaCanvas;canvas->setgfxworld(wd);themed_[state][frame]=canvas;
}

Cursor::Cursor(Game* g) : g_(g), sprite_(0), shape_(Normal), sysHidden_(false), tried_(false)
{
    // port: themed sprite art replaces the platform cursor; unrecognised values use Spatula.
    const char* theme = getenv("SB_CURSOR");
    theme_ = theme && !strcmp(theme, "classic") ? 3 : theme && !strcmp(theme, "bubble") ? 2 : theme && !strcmp(theme, "net") ? 1 : 0;
    for (int a = 0; a < ShapeCount; ++a) for (int f = 0; f < Frames; ++f) themed_[a][f] = 0;
    for (int i = 0; i < ShapeCount; ++i) { canv_[i] = 0; hotX_[i] = hotY_[i] = 0; }
    buildAll();
}

// lane c: the sprite sets of the current theme_ (also used by setTheme, the port menu)
void Cursor::buildAll()
{
    frame_ = -1;
    build(Normal, kArrow, 18, 0xFFFFFFFF, 0);
    build(Hotspot, kHand, 16, 0xFFFFE070, 0);
    hotX_[Hotspot] = 4; hotY_[Hotspot] = 0;
    build(Hand, kArrow, 18, 0xFFFFD43B, 0);                          // the arrow in gold: the item is the picture
    build(Wait, kWait, 14, 0xFFFFFFFF, 0xFFE8A820);
    hotX_[Wait] = 7; hotY_[Wait] = 7;
    if (theme_ != 3) {
        for (int i = 0; i < ShapeCount; ++i) {
            delete canv_[i]; canv_[i] = 0;
            for (int f = 0; f < Frames; ++f) buildTheme(i, f);
            canv_[i] = themed_[i][0];
            hotX_[i] = hotY_[i] = i == Hand ? 38 : i == Wait ? 20 : 4;
        }
    }
}

// lane c: port menu, live: 0 Spatula, 1 Net, 2 Bubble, 3 Classic. The old sprites are freed after the sprite element points at the new ones.
void Cursor::setTheme(int t)
{
    t = t < 0 ? 0 : t > 3 ? 3 : t;
    if (t == theme_) return;
    OMediaCanvas* oldC[ShapeCount]; OMediaCanvas* oldT[ShapeCount][Frames]; int oldTheme = theme_;
    for (int i = 0; i < ShapeCount; ++i) { oldC[i] = canv_[i]; for (int f = 0; f < Frames; ++f) { oldT[i][f] = themed_[i][f]; themed_[i][f] = 0; } canv_[i] = 0; hotX_[i] = hotY_[i] = 0; }
    theme_ = t;
    buildAll();
    shape_ = Normal;
    if (sprite_ && canv_[Normal]) sprite_->setcanvas(theme_ == 3 ? canv_[Normal] : themed_[Normal][0]);
    for (int i = 0; i < ShapeCount; ++i) {
        if (oldTheme == 3) delete oldC[i];
        for (int f = 0; f < Frames; ++f) delete oldT[i][f];
    }
}

Cursor::~Cursor()
{
    delete sprite_;
    for (int i = 0; i < ShapeCount; ++i) {
        if (theme_ == 3) delete canv_[i];
        for (int f = 0; f < Frames; ++f) delete themed_[i][f];
    }
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
        sprite_ = new OMediaCanvasElement(theme_ == 3 ? canv_[Normal] : themed_[Normal][0]);
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
    if (getenv("SB_TEST_CURSOR")) s = (Shape)((atoi(getenv("SB_TEST_CURSOR")) % ShapeCount + ShapeCount) % ShapeCount);   // port: tests only
    if (theme_ != 3) {
        int f = (SDL_GetTicks() / 100) % Frames;
        for (int i = 0; i < ShapeCount; ++i) canv_[i] = themed_[i][f];
        if (frame_ != f || s != shape_) { sprite_->setcanvas(canv_[s]); frame_ = f; }
    }
    set(s);
    sprite_->link(g_->world());                                       // port: last in the list = drawn last (the world draws in link order)
    if (getenv("SB_HUD_DEBUG")) { static int n = 0; if (++n % 60 == 0) g_->log("cursor: shape %d hide %d size %dx%d at %d,%d\n", (int)s, sprite_->get_hide_count(), canv_[s]->getwidth(), canv_[s]->getheight(), x, y); }
    // port: preserve the visual hotspot even at picture edges; clipping must never move the tip away from the click.
    int px = x - hotX_[s], py = y - hotY_[s];
    sprite_->place((float)px, (float)py, 10500.0f);
}

}  // namespace sbgame
