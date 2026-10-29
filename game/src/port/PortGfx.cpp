/*  port: see PortGfx.h.  */
#include "PortGfx.h"
#include "OMediaSDLRetarget.h"
#include "OMediaGLBackend.h"
#include <math.h>
#include <string.h>
#include <vector>

namespace sbport {

#include "PortFontData.inc"

namespace {
unsigned g_tex = 0, g_epoch = 0;

void colour(Rgba c) { glColor4ub((GLubyte)(c >> 16), (GLubyte)(c >> 8), (GLubyte)c, (GLubyte)(c >> 24)); }

void ensureFont()
{
    if (g_tex && g_epoch == omgl_context_epoch()) return;       // a re-created GL context (suspend / resume) loses the texture: made again
    const int W = kFontAtlasW, H = kFontAtlasH;
    std::vector<unsigned char> px((size_t)W * H * 4);
    static const unsigned char level[4] = { 0, 90, 180, 255 };
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            unsigned char b = kFontBits[y * (W / 4) + x / 4];
            unsigned char a = level[(b >> ((x & 3) * 2)) & 3];
            unsigned char* o = &px[((size_t)y * W + x) * 4];
            o[0] = o[1] = o[2] = 255; o[3] = a;
        }
    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    g_epoch = omgl_context_epoch();
}
}  // namespace

void gfxBegin(int dw, int dh)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dw, dh);
    glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, dw, dh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_LIGHTING); glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    ensureFont();
}

void gfxEnd()
{
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1, 1, 1, 1);
}

void gfxFill(float x, float y, float w, float h, Rgba c)
{
    glDisable(GL_TEXTURE_2D); colour(c);
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}

void gfxGradient(float x, float y, float w, float h, Rgba t, Rgba b)
{
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    colour(t); glVertex2f(x, y); glVertex2f(x + w, y);
    colour(b); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

void gfxDisc(float x, float y, float r, Rgba c)
{
    glDisable(GL_TEXTURE_2D); colour(c);
    glBegin(GL_TRIANGLE_FAN); glVertex2f(x, y);
    for (int k = 0; k <= 32; ++k) { float a = k * 6.2831853f / 32; glVertex2f(x + cosf(a) * r, y + sinf(a) * r); }
    glEnd();
}

void gfxRing(float x, float y, float r, float width, Rgba c)
{
    glDisable(GL_TEXTURE_2D); colour(c);
    glBegin(GL_TRIANGLE_STRIP);
    for (int k = 0; k <= 32; ++k) {
        float a = k * 6.2831853f / 32;
        glVertex2f(x + cosf(a) * r, y + sinf(a) * r); glVertex2f(x + cosf(a) * (r + width), y + sinf(a) * (r + width));
    }
    glEnd();
}

void gfxRound(float x, float y, float w, float h, float r, Rgba c)
{
    if (r > h * 0.5f) r = h * 0.5f;
    if (r > w * 0.5f) r = w * 0.5f;
    glDisable(GL_TEXTURE_2D); colour(c);
    glBegin(GL_TRIANGLE_FAN); glVertex2f(x + w * 0.5f, y + h * 0.5f);
    const float cx[4] = { x + w - r, x + r, x + r, x + w - r }, cy[4] = { y + r, y + r, y + h - r, y + h - r };
    for (int q = 0; q < 4; ++q)
        for (int k = 0; k <= 8; ++k) { float a = (q * 8 + k) * 6.2831853f / 32; glVertex2f(cx[q] + cosf(a) * r, cy[q] - sinf(a) * r); }
    glVertex2f(x + w, y + r);
    glEnd();
}

void gfxLine(float x0, float y0, float x1, float y1, float width, Rgba c)
{
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
    if (l <= 0) return;
    float nx = -dy / l * width * 0.5f, ny = dx / l * width * 0.5f;
    glDisable(GL_TEXTURE_2D); colour(c);
    glBegin(GL_QUADS); glVertex2f(x0 + nx, y0 + ny); glVertex2f(x1 + nx, y1 + ny); glVertex2f(x1 - nx, y1 - ny); glVertex2f(x0 - nx, y0 - ny); glEnd();
}

float gfxTextWidth(const char* s, float size)
{
    float w = 0, k = size / 26.0f;
    for (; *s; ++s) { unsigned char ch = (unsigned char)*s; if (ch < 32 || ch > 126) ch = '?'; w += kFontAdv[ch - 32] * k; }
    return w;
}

void gfxText(float x, float y, float size, const char* s, Rgba c, int align)
{
    float k = size / 26.0f;
    if (align == 1) x -= gfxTextWidth(s, size) * 0.5f; else if (align == 2) x -= gfxTextWidth(s, size);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    colour(c);
    glBegin(GL_QUADS);
    for (; *s; ++s) {
        unsigned char ch = (unsigned char)*s; if (ch < 32 || ch > 126) ch = '?';
        int i = ch - 32;
        float u0 = (float)((i % kFontCols) * kFontCellW) / kFontAtlasW, v0 = (float)((i / kFontCols) * kFontCellH) / kFontAtlasH;
        float u1 = u0 + (float)kFontCellW / kFontAtlasW, v1 = v0 + (float)kFontCellH / kFontAtlasH;
        float x1 = x + kFontCellW * k, y1 = y + kFontCellH * k;
        glTexCoord2f(u0, v0); glVertex2f(x, y);
        glTexCoord2f(u1, v0); glVertex2f(x1, y);
        glTexCoord2f(u1, v1); glVertex2f(x1, y1);
        glTexCoord2f(u0, v1); glVertex2f(x, y1);
        x += kFontAdv[i] * k;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

}  // namespace sbport
