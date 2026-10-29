/*  The text font of the game on the OMediaTextFont interface (what OMediaCCaption / OMediaCStringField call).
    sb2.exe has a font class of its own (vtable 0x44dd78, ctor 0x414884, built on Font.omt's glyph canvases);
    lane C reconstructs it in StartupMain.cpp.  Until that lands the scene runtime draws with this stand-in,
    which reads the same Font.omt canvases (digits "0".."9", capitals "lA".., small letters "sa"..).
    port: stand-in, replace by lane C's class (same interface) when it is merged.  */
#pragma once
#include "OMediaDataBase.h"
#include "OMediaCanvas.h"
#include "OMediaTextFont.h"
#include <string>

namespace sbgame {

class GlyphFont : public OMediaTextFont {
public:
    explicit GlyphFont(OMediaDataBase* db) : database(db) {}

    OMediaCanvas* glyph(char c) const
    {
        char n[3] = { 0, 0, 0 };
        if (c >= '0' && c <= '9') n[0] = c;
        else if (c >= 'A' && c <= 'Z') { n[0] = 'l'; n[1] = c; }
        else if (c >= 'a' && c <= 'z') { n[0] = 's'; n[1] = c; }
        else if (c == '!') { n[0] = 'e'; n[1] = 'x'; }
        else if (c == '\'') { n[0] = 'a'; n[1] = 'p'; }
        else if (c == ',') { n[0] = 'c'; n[1] = 'o'; }
        else if (c == '-') { n[0] = 'd'; n[1] = 'a'; }
        else if (c == '.') { n[0] = 'd'; n[1] = 'o'; }
        else if (c == '?') { n[0] = 'q'; n[1] = 'u'; }
        else return 0;
        return (OMediaCanvas*)database->get_object(OMediaCanvas::db_type, string(n));
    }
    short advance(char c) const
    {
        OMediaCanvas* g = glyph(c);
        return g ? (short)(g->getwidth() + 1) : (short)8;
    }
    virtual short get_font_height(void) { return 18; }
    virtual short get_text_length(string t)
    {
        short w = 0;
        for (size_t i = 0; i < t.size(); i++) w = (short)(w + advance(t[i]));
        return w;
    }
    virtual void draw_full(string t, OMediaGfxWorld* dest, short x, short y)
    {
        OMediaRect clip(0, 0, get_text_length(t), 18);
        draw(t, dest, &clip, x, y);
    }
    virtual void draw(string t, OMediaGfxWorld* dest, OMediaRect* clip, short x, short y)
    {
        long tx = x - clip->left, ty = y - clip->top;
        long cl = clip->left + tx, ct = clip->top + ty, cr = clip->right + tx, cb = clip->bottom + ty;
        long px = tx;
        for (size_t i = 0; i < t.size(); i++) {
            OMediaCanvas* g = glyph(t[i]);
            if (g) {
                long cx0 = px, cx1 = px + g->getwidth(), cy0 = ty, cy1 = ty + g->getheight();
                long ix0 = cl > cx0 ? cl : cx0, iy0 = ct > cy0 ? ct : cy0;
                long ix1 = cr < cx1 ? cr : cx1, iy1 = cb < cy1 ? cb : cy1;
                if (ix0 < ix1 && iy0 < iy1) {
                    OMediaRect src(ix0 - cx0, iy0 - cy0, ix1 - cx0, iy1 - cy0);
                    g->draw(dest, &src, (short)ix0, (short)iy0, NULL);
                }
            }
            px += advance(t[i]);
        }
    }

protected:
    OMediaDataBase* database;
};

}  // namespace sbgame
