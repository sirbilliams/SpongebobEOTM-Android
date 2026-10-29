/*  port: HD background pack (see SbHd.h, docs/hd-pack.md). */

#include "SbHd.h"
#include "OMediaHDPng.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <map>
#include <set>

using std::string;

namespace
{
	struct Entry { string path; int scale; int canvas_w, canvas_h, orig_w, orig_h; Entry() : scale(0), canvas_w(0), canvas_h(0), orig_w(0), orig_h(0) {} };

	struct Pack
	{
		bool scanned, on;
		string dir;
		std::map<string, Entry> files;		// key ("sceneart/c1l0s1/background") -> file and manifest data
		std::set<string> logged;
		int max_scale, loaded, failed;
		double used, peak, budget;

		Pack() : scanned(false), on(false), max_scale(1), loaded(0), failed(0), used(0), peak(0), budget(0) {}
	};

	Pack g;

	string lower(string s) { for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]); return s; }

	void scan_dir(const string &real, const string &rel, int depth)
	{
		DIR *d = opendir(real.c_str());
		if (!d) return;
		struct dirent *e;
		while ((e = readdir(d)) != NULL)
		{
			if (e->d_name[0] == '.') continue;
			const string r = real + "/" + e->d_name, l = rel + lower(e->d_name);
			struct stat st;
			if (stat(r.c_str(), &st) != 0) continue;
			if (S_ISDIR(st.st_mode)) { if (depth < 3) scan_dir(r, l + "/", depth + 1); }
			else if (l.size() > 4 && l.compare(l.size() - 4, 4, ".png") == 0)
			{
				Entry en;
				en.path = r;
				g.files[l.substr(0, l.size() - 4)] = en;
			}
		}
		closedir(d);
	}

	// manifest.tsv: file<TAB>scene<TAB>canvas<TAB>orig_w<TAB>orig_h<TAB>scale<TAB>canvas_w<TAB>canvas_h<TAB>offset_x<TAB>offset_y<TAB>sha256
	void read_manifest(const string &file)
	{
		FILE *f = fopen(file.c_str(), "r");
		if (!f) return;
		char line[1024];
		while (fgets(line, sizeof line, f))
		{
			if (line[0] == '#' || line[0] == '\n' || !strncmp(line, "file\t", 5)) continue;
			char *col[12];
			int n = 0;
			for (char *p = strtok(line, "\t\r\n"); p && n < 12; p = strtok(NULL, "\t\r\n")) col[n++] = p;
			if (n < 8) continue;
			string key = lower(col[0]);
			if (key.size() > 4 && key.compare(key.size() - 4, 4, ".png") == 0) key.resize(key.size() - 4);
			std::map<string, Entry>::iterator it = g.files.find(key);
			if (it == g.files.end()) continue;
			it->second.orig_w = atoi(col[3]); it->second.orig_h = atoi(col[4]);
			it->second.scale = atoi(col[5]);
			it->second.canvas_w = atoi(col[6]); it->second.canvas_h = atoi(col[7]);
		}
		fclose(f);
	}

	Pack &pack()
	{
		if (!g.scanned)
		{
			g.scanned = true;
			const char *mode = getenv("SB_HD");
			const bool forced_off = mode && (!strcasecmp(mode, "off") || !strcmp(mode, "0"));
			const char *dir = getenv("SB_HD_DIR");
			g.dir = dir && *dir ? dir : "hd";
			const char *mb = getenv("SB_HD_BUDGET_MB");
			g.budget = (mb && atoi(mb) > 0 ? atoi(mb) : 256) * 1024.0 * 1024.0;
			if (forced_off) { SDL_Log("HD: off (SB_HD=off)"); return g; }
			scan_dir(g.dir, "", 0);
			read_manifest(g.dir + "/manifest.tsv");
			for (std::map<string, Entry>::iterator it = g.files.begin(); it != g.files.end(); ++it)
				if (it->second.scale > g.max_scale) g.max_scale = it->second.scale;
			if (g.max_scale > 3) g.max_scale = 3;		// the default screen factor; SB_HD_SCALE can ask for 4
			g.on = !g.files.empty();
			if (g.on) SDL_Log("HD: background pack with %d pictures (screen factor up to %d, budget %.0f MB)", (int)g.files.size(), g.max_scale, g.budget / (1024.0 * 1024.0));
			else if (mode) SDL_Log("HD: SB_HD=%s but there is no pack in %s/", mode, g.dir.c_str());
		}
		return g;
	}

	void log_once(const string &key, const string &msg)
	{
		if (g.logged.insert(key).second) SDL_Log("HD: %s %s - using the original", key.c_str(), msg.c_str());
	}
}

bool sbhd::enabled() { return pack().on; }

int sbhd::scale(int max_tex)
{
	Pack &p = pack();
	if (!p.on) return 1;
	int s = p.max_scale;
	const char *e = getenv("SB_HD_SCALE");
	if (e && atoi(e) > 0) s = atoi(e);
	if (s > 4) s = 4;
	if (max_tex > 0) while (s > 1 && (640 * s > max_tex || 480 * s > max_tex)) s--;
	return s < 1 ? 1 : s;
}

bool sbhd::has(const string &key) { Pack &p = pack(); return p.on && p.files.find(lower(key)) != p.files.end(); }

bool sbhd::load(const string &key, int ow, int oh, std::vector<unsigned char> &rgba, int &w, int &h)
{
	Pack &p = pack();
	if (!p.on || ow <= 0 || oh <= 0) return false;
	std::map<string, Entry>::iterator it = p.files.find(lower(key));
	if (it == p.files.end()) return false;
	const Entry &en = it->second;
	if (en.canvas_w > ow || en.canvas_h > oh)
	{
		// the manifest door for a true 16:9 (outpainted) pack: a wider canvas needs the camera work of docs/hd-pack.md
		log_once(key, "is a wider-than-original canvas, not supported by this build");
		p.failed++;
		return false;
	}
	const double left = p.budget - p.used;
	if (left <= 0) { log_once(key, "is over the memory budget"); p.failed++; return false; }
	omhd::DecodedPng img;
	// the decode's working set (compressed + inflated + pixels, x2) must fit what is left of the budget
	const char *err = omhd::decode_png(en.path, img, (size_t)(left > 4e9 ? 4e9 : left));
	if (err) { log_once(key, string(err)); p.failed++; return false; }
	if ((int)img.width % ow != 0 || (int)img.height % oh != 0 || (int)img.width / ow != (int)img.height / oh || img.width < (unsigned)ow)
	{
		char m[96];
		snprintf(m, sizeof m, "is %ux%u, not a whole multiple of the canvas %dx%d", img.width, img.height, ow, oh);
		log_once(key, m);
		p.failed++;
		return false;
	}
	if (en.orig_w && (en.orig_w != ow || en.orig_h != oh))
	{
		char m[96];
		snprintf(m, sizeof m, "was made for a %dx%d canvas, this one is %dx%d", en.orig_w, en.orig_h, ow, oh);
		log_once(key, m);
		p.failed++;
		return false;
	}
	w = (int)img.width; h = (int)img.height;
	rgba.swap(img.pixels);
	p.loaded++;
	return true;
}

bool sbhd::budget_add(const string &key, double bytes)
{
	Pack &p = pack();
	if (p.used + bytes > p.budget)
	{
		log_once(key, "does not fit the memory budget");
		p.failed++;
		return false;
	}
	p.used += bytes;
	if (p.used > p.peak) p.peak = p.used;
	SDL_Log("HD: %s %.1f MB (budget used %.1f of %.0f MB)", key.c_str(), bytes / 1048576.0, p.used / 1048576.0, p.budget / 1048576.0);
	return true;
}

void sbhd::budget_sub(double bytes) { g.used -= bytes; if (g.used < 0) g.used = 0; }
double sbhd::budget_left() { return g.budget - g.used; }
double sbhd::budget_used() { return g.used; }

void sbhd::log_summary()
{
	if (g.on) SDL_Log("HD: %d pictures used, %d fell back to the original, peak %.1f MB of %.0f MB", g.loaded, g.failed, g.peak / 1048576.0, g.budget / 1048576.0);
}
