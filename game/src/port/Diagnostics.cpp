/*  port: diagnostics overlay (Diagnostics): FPS, frame time, resident memory, GL framebuffer bits, bottom-screen state, scene.  docs/port-menu.md.
    One frame = one omgl_present.  The numbers are also written to the log (omt-log.txt on Android) every 10 s while the overlay is on.  */
#include "PortMenu.h"
#include "PortGfx.h"
#include "OMediaSDLRetarget.h"
#include "OMediaGLBackend.h"
#include "OMediaEventManager.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace sbport {

namespace {
double nowMs() { return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency(); }

long residentKb()
{
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return -1;
    long size = 0, res = 0;
    int n = fscanf(f, "%ld %ld", &size, &res);
    fclose(f);
    return n == 2 ? res * (sysconf(_SC_PAGESIZE) / 1024) : -1;
}

double g_last = 0, g_winStart = 0, g_logAt = 0, g_worst = 0, g_sum = 0;
int g_frames = 0;
double g_fps = 0, g_avg = 0, g_max = 0;
}  // namespace

void DiagFrame(int dw, int dh, bool draw, const PortSettings& s, const char* scene)
{
    double t = nowMs();
    if (g_last > 0) {
        double dt = t - g_last;
        if (dt > 1000) dt = 0;                      // a suspend / a movie is not a frame time
        else { g_sum += dt; if (dt > g_worst) g_worst = dt; ++g_frames; }
    }
    g_last = t;
    if (g_winStart == 0) g_winStart = t;
    if (t - g_winStart >= 500 && g_frames > 0) {   // twice a second
        g_fps = g_frames * 1000.0 / (t - g_winStart);
        g_avg = g_sum / g_frames; g_max = g_worst;
        g_winStart = t; g_frames = 0; g_sum = 0; g_worst = 0;
    }
    if (!draw) { g_logAt = 0; return; }

    int fb[4]; omgl_framebuffer_bits(fb);
    long rss = residentKb();
    char l1[64], l2[64], l3[64], l4[96], l5[96], l6[112];
    snprintf(l1, sizeof l1, "FPS %.0f", g_fps);
    snprintf(l2, sizeof l2, "frame %.1f ms (max %.0f)", g_avg, g_max);
    snprintf(l3, sizeof l3, "memory %.1f MB", rss / 1024.0);
    snprintf(l4, sizeof l4, "GL %d/%d/%d/%d", fb[0], fb[1], fb[2], fb[3]);
    const char* bm = s.bottom == BOTTOM_PANEL ? "panel" : s.bottom == BOTTOM_PAD ? "touchpad" : "off";
    snprintf(l5, sizeof l5, "bottom %s, touches %u", bm, OMediaEventManager::touchpad_events());
    snprintf(l6, sizeof l6, "scene %s", scene && *scene ? scene : "-");

    if (g_logAt == 0) g_logAt = t;
    if (t - g_logAt >= 10000) {
        g_logAt = t;
        SDL_Log("diag: %s, %s, %s, framebuffer %d/%d/%d/%d, %s, %s", l1, l2, l3, fb[0], fb[1], fb[2], fb[3], l5, l6);
    }

    float k = (float)dh / 1080.0f; if (k < 0.6f) k = 0.6f;
    float fs = 24 * k, lh = 30 * k, pad = 12 * k;
    const char* lines[6] = { l1, l2, l3, l4, l5, l6 };
    float w = 0;
    for (int i = 0; i < 6; ++i) { float lw = gfxTextWidth(lines[i], fs); if (lw > w) w = lw; }
    float bw = w + pad * 2, bh = lh * 6 + pad * 2, bx = 16 * k, by = 16 * k;
    gfxBegin(dw, dh);
    gfxRound(bx, by, bw, bh, 12 * k, rgba(170, 4, 30, 42));
    gfxRound(bx + 2 * k, by + 2 * k, bw - 4 * k, bh - 4 * k, 10 * k, rgba(120, 8, 56, 70));
    for (int i = 0; i < 6; ++i) gfxText(bx + pad, by + pad + i * lh - 2 * k, fs, lines[i], i == 0 ? rgba(255, 255, 230, 140) : rgba(255, 185, 229, 221));
    gfxEnd();
}

}  // namespace sbport
