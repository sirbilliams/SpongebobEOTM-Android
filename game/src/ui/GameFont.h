#ifndef SB_GAME_FONT_H
#define SB_GAME_FONT_H
#include "OMediaTextFont.h"
#include "RetailData.h"
#include <map>
#include <vector>
class OMediaDataBase;
class OMediaCanvas;
class OMediaCanvasElement;
class OMediaWorld;
// sb2.exe vtable 0x44dd78; port: OMediaTextFont adapter for the 1.x captions.
class GameFont : public OMediaTextFont {
public:
    GameFont(OMediaDataBase *database, const RetailData &data);
    virtual ~GameFont();
    virtual short get_text_length(string text);                 // 0x414fe0
    virtual short get_font_height() { return 18; }
    virtual void draw_full(string text, OMediaGfxWorld *dest, short x, short y);
    virtual void draw(string text, OMediaGfxWorld *dest, OMediaRect *clip, short x, short y);
    void begin_frame(); // 0x414a90: hide glyph elements for reuse
    void draw_world(OMediaWorld *world, int x, int y, int z, short variant, const string &text); // 0x414de0
    void print(OMediaWorld *world, int x, int y, const char *format, ...); // 0x414b00
    short print_wrapped(OMediaWorld *world, int x, int y, int z, bool centered,
                        short variant, const char *format, ...); // 0x414b50
    void set_bank(short value) { bank=value; }
    void drop_pool() { pool.clear(); next=0; }   // port: the world that owns the pooled glyph elements is about to be destroyed (asset viewer)
private:
    OMediaCanvas *glyph(unsigned char c, short variant);
    OMediaDataBase *db;
    long table[128];
    std::map<long,OMediaCanvas*> cache;
    short bank, emphasis;
    std::vector<OMediaCanvasElement*> pool;
    size_t next;
};
#endif
