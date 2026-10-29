/*  port: tiny immediate-mode GL drawing for the port overlay (port menu, diagnostics): rectangles, rounded rectangles, discs, lines and text
    (an embedded 2-bit font atlas, PortFontData.inc).  Coordinates are window pixels, y down.  Drawn inside the omgl_present overlay hook, so
    nothing here touches the game's own surfaces.  docs/port-menu.md.  */
#pragma once

namespace sbport {

typedef unsigned int Rgba;                      // 0xAARRGGBB
inline Rgba rgba(unsigned a, unsigned r, unsigned g, unsigned b) { return (a << 24) | (r << 16) | (g << 8) | b; }

void gfxBegin(int dw, int dh);                  // 2D state for the drawable dw x dh
void gfxEnd();                                  // back to the state omgl_present expects
void gfxFill(float x, float y, float w, float h, Rgba c);
void gfxGradient(float x, float y, float w, float h, Rgba top, Rgba bottom);
void gfxRound(float x, float y, float w, float h, float r, Rgba c);       // rounded rectangle
void gfxDisc(float x, float y, float r, Rgba c);
void gfxRing(float x, float y, float r, float width, Rgba c);
void gfxLine(float x0, float y0, float x1, float y1, float width, Rgba c);
// size: the height of a capital-height-ish line in pixels (the atlas is 26 px at size 1.0 = 26).  align: 0 left, 1 centre, 2 right of x.
void gfxText(float x, float y, float size, const char* s, Rgba c, int align = 0);
float gfxTextWidth(const char* s, float size);

}  // namespace sbport
