/*  port: HD background pack (docs/hd-pack.md) - not part of OMT 1.x.

    A pack is a folder hd/ in the game folder holding upscaled copies of the scene backgrounds and sky
    canvases, made from the player's own game by tools/hd/sb_hdtool:
        hd/sceneart/<scene>/<canvas>.png     canvas <canvas> of sceneart/<scene>.omt (lower case)
        hd/manifest.tsv                      scene, canvas, original size, scale, sha256 (see the docs)
    The folder is scanned once.  SB_HD=on|off (default: on when a pack is present).  Nothing here
    needs GL: engine/sdl/OMediaSDLGfx.cpp uploads what load() returns.  Any error (missing file, bad CRC,
    wrong size, memory budget) makes load() fail, and the original canvas is used.                       */
#ifndef SB_HD_H
#define SB_HD_H

#include <string>
#include <vector>

namespace sbhd
{
	// SB_HD=on forces the pack on (and logs when there is none), SB_HD=off disables it, unset: on if hd/ has pictures.
	bool enabled();
	// The factor of the 640x480 screen surface: the largest pack scale (SB_HD_SCALE overrides), 1 without a pack.
	// max_tex is GL_MAX_TEXTURE_SIZE (the screen surface must fit); 0 = not known yet.
	int scale(int max_tex);
	// Does the pack have this picture ("sceneart/c1l0s1/background")?
	bool has(const std::string &key);
	// Decodes the pack picture for a canvas of ow x oh.  RGBA, top row first.  False (logged once per key)
	// when it is missing or invalid: bad PNG / CRC, size not a whole multiple of the canvas, over the budget.
	bool load(const std::string &key, int ow, int oh, std::vector<unsigned char> &rgba, int &w, int &h);
	// GPU memory accounting of the HD textures in bytes (SB_HD_BUDGET_MB, default 256); add() is false over it.
	bool budget_add(const std::string &key, double bytes);
	void budget_sub(double bytes);
	double budget_left();
	double budget_used();
	void log_summary();
}

#endif
