/*  port: HD texture pack loader (see OMediaHDPack.h, docs/hd-pack.md). */

#include "OMediaHDPack.h"
#include "OMediaCanvas.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaError.h"
#include "OMediaSDLRetarget.h"
#include <SDL.h>
#include <zlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <set>
#include <string>
#include <vector>

using std::string;

namespace
{
	struct Pack
	{
		bool scanned;
		std::map<string, string> files;		// lower-case relative name -> real path
		std::set<string> logged;			// names already reported (bad size, too big)
		int replaced;
		double bytes, budget;
		bool budget_logged;

		Pack() : scanned(false), replaced(0), bytes(0), budget(0), budget_logged(false) {}
		~Pack()
		{
			if (!files.empty())
				SDL_Log("HD: %d textures replaced (%d files in the pack, %.0f MB at the end)", replaced,
						(int)files.size(), bytes / (1024.0 * 1024.0));
		}
	};

	Pack g_pack;

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
			else if (l.size() > 4 && l.compare(l.size() - 4, 4, ".png") == 0) g_pack.files[l] = r;
		}
		closedir(d);
	}

	Pack &pack()
	{
		if (!g_pack.scanned)
		{
			g_pack.scanned = true;
			const char *dir = getenv("NEG_HD_DIR");			// default: hd/ in the game folder (the cwd)
			scan_dir(dir && *dir ? dir : "hd", "", 0);
			const char *mb = getenv("NEG_HD_BUDGET_MB");
			g_pack.budget = (mb && atoi(mb) > 0 ? atoi(mb) : 1536) * 1024.0 * 1024.0;
			if (!g_pack.files.empty())
				SDL_Log("HD: texture pack with %d pictures (budget %.0f MB)", (int)g_pack.files.size(),
						g_pack.budget / (1024.0 * 1024.0));
		}
		return g_pack;
	}

	void log_once(const string &key, const char *msg)
	{
		if (g_pack.logged.insert(key).second) SDL_Log("HD: %s %s", key.c_str(), msg);
	}
}

void omhd::tag(OMediaCanvas *c, const string &rel)
{
	if (!c || c->hd_drawn) return;
	Pack &p = pack();
	if (p.files.empty()) return;
	const string l = lower(rel);
	if (p.files.find(l) != p.files.end()) c->hd_key = l;
}

// A complete PNG reader for the pack pictures (all colour types and bit depths, tRNS, several IDAT chunks,
// Adam7): OMT's own reader only knows the subset the 2002 game files use, and stays as it is for them.
// Fills 'c' with RGBA (memory order R, G, B, A).  Returns an error text or NULL.
static unsigned int be32(const unsigned char *p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }

static int paeth(int a, int b, int c)
{
	const int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
	return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

static const char *read_png(const string &file, OMediaCanvas *c)
{
	FILE *f = fopen(file.c_str(), "rb");
	if (!f) return "cannot be opened";
	std::vector<unsigned char> data;
	unsigned char buf[65536];
	size_t got;
	while ((got = fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + got);
	fclose(f);
	static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	if (data.size() < 8 || memcmp(&data[0], sig, 8) != 0) return "is not a PNG file";

	unsigned int w = 0, h = 0;
	int depth = 0, ctype = -1, interlace = 0;
	std::vector<unsigned char> idat, plte, trns;
	size_t pos = 8;
	bool end = false;
	while (!end && pos + 12 <= data.size())
	{
		const unsigned int len = be32(&data[pos]);
		if (len > data.size() - pos - 12) return "is truncated";
		const unsigned char *t = &data[pos + 4], *d = &data[pos + 8];
		if (!memcmp(t, "IHDR", 4) && len >= 13)
		{
			w = be32(d); h = be32(d + 4); depth = d[8]; ctype = d[9]; interlace = d[12];
		}
		else if (!memcmp(t, "PLTE", 4)) plte.assign(d, d + len);
		else if (!memcmp(t, "tRNS", 4)) trns.assign(d, d + len);
		else if (!memcmp(t, "IDAT", 4)) idat.insert(idat.end(), d, d + len);
		else if (!memcmp(t, "IEND", 4)) end = true;
		pos += 12 + len;
	}
	int chans;
	switch (ctype) { case 0: chans = 1; break; case 2: chans = 3; break; case 3: chans = 1; break;
					 case 4: chans = 2; break; case 6: chans = 4; break; default: return "has an unknown colour type"; }
	if (w == 0 || h == 0 || w > 32768 || h > 32768) return "has a bad size";
	if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) return "has a bad bit depth";
	if (ctype == 3 && plte.empty()) return "has no palette";
	const int bpp = (chans * depth + 7) / 8;			// bytes per pixel for the filters (at least 1)

	// the passes (Adam7 or one), their raw size, then inflate everything at once
	static const int ax[7] = { 0, 4, 0, 2, 0, 1, 0 }, ay[7] = { 0, 0, 4, 0, 2, 0, 1 },
					 dx[7] = { 8, 8, 4, 4, 2, 2, 1 }, dy[7] = { 8, 8, 8, 4, 4, 2, 2 };
	const int npass = interlace ? 7 : 1;
	unsigned int pw[7], ph[7];
	size_t total = 0;
	for (int p = 0; p < npass; p++)
	{
		pw[p] = interlace ? (w > (unsigned)ax[p] ? (w - ax[p] + dx[p] - 1) / dx[p] : 0) : w;
		ph[p] = interlace ? (h > (unsigned)ay[p] ? (h - ay[p] + dy[p] - 1) / dy[p] : 0) : h;
		if (pw[p] && ph[p]) total += (size_t)ph[p] * (1 + ((size_t)pw[p] * chans * depth + 7) / 8);
	}
	std::vector<unsigned char> raw(total);
	uLongf rawlen = (uLongf)total;
	if (idat.empty() || uncompress(&raw[0], &rawlen, &idat[0], (uLong)idat.size()) != Z_OK || rawlen != total)
		return "cannot be decompressed";

	c->create((int)w, (int)h);
	c->lock(omlf_Write);
	unsigned char *out = (unsigned char *)c->get_pixels();
	size_t off = 0;
	for (int p = 0; p < npass; p++)
	{
		if (!pw[p] || !ph[p]) continue;
		const size_t stride = ((size_t)pw[p] * chans * depth + 7) / 8;
		std::vector<unsigned char> prev(stride, 0);
		for (unsigned int y = 0; y < ph[p]; y++)
		{
			const int filter = raw[off];
			unsigned char *row = &raw[off + 1];
			for (size_t i = 0; i < stride; i++)
			{
				const int a = i >= (size_t)bpp ? row[i - bpp] : 0, b = prev[i], cc = i >= (size_t)bpp ? prev[i - bpp] : 0;
				switch (filter)
				{
					case 1: row[i] = (unsigned char)(row[i] + a); break;
					case 2: row[i] = (unsigned char)(row[i] + b); break;
					case 3: row[i] = (unsigned char)(row[i] + ((a + b) >> 1)); break;
					case 4: row[i] = (unsigned char)(row[i] + paeth(a, b, cc)); break;
				}
			}
			for (unsigned int x = 0; x < pw[p]; x++)
			{
				int v[4];									// samples scaled to 8 bits
				unsigned int sraw[4];						// raw samples (for tRNS)
				for (int k = 0; k < chans; k++)
				{
					unsigned int s;
					if (depth == 16) { s = (row[(x * chans + k) * 2] << 8) | row[(x * chans + k) * 2 + 1]; v[k] = s >> 8; }
					else if (depth == 8) { s = row[x * chans + k]; v[k] = s; }
					else
					{
						const size_t bit = (size_t)(x * chans + k) * depth;
						s = (row[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
						v[k] = ctype == 3 ? s : s * 255 / ((1 << depth) - 1);
					}
					sraw[k] = s;
				}
				unsigned char r, g, bl, al = 255;
				if (ctype == 3)
				{
					const unsigned int i = sraw[0];
					r = 3 * i + 2 < plte.size() ? plte[3 * i] : 0;
					g = 3 * i + 2 < plte.size() ? plte[3 * i + 1] : 0;
					bl = 3 * i + 2 < plte.size() ? plte[3 * i + 2] : 0;
					if (i < trns.size()) al = trns[i];
				}
				else if (ctype == 0 || ctype == 4)
				{
					r = g = bl = (unsigned char)v[0];
					if (ctype == 4) al = (unsigned char)v[1];
					else if (trns.size() >= 2 && sraw[0] == ((unsigned)trns[0] << 8 | trns[1])) al = 0;
				}
				else
				{
					r = (unsigned char)v[0]; g = (unsigned char)v[1]; bl = (unsigned char)v[2];
					if (ctype == 6) al = (unsigned char)v[3];
					else if (trns.size() >= 6 && sraw[0] == ((unsigned)trns[0] << 8 | trns[1]) &&
							 sraw[1] == ((unsigned)trns[2] << 8 | trns[3]) && sraw[2] == ((unsigned)trns[4] << 8 | trns[5])) al = 0;
				}
				const size_t ox = interlace ? ax[p] + (size_t)x * dx[p] : x, oy = interlace ? ay[p] + (size_t)y * dy[p] : y;
				unsigned char *o = out + (oy * w + ox) * 4;
				o[0] = r; o[1] = g; o[2] = bl; o[3] = al;
			}
			memcpy(&prev[0], row, stride);
			off += 1 + stride;
		}
	}
	c->unlock();
	return NULL;
}

// The path a file stream was opened with, '/' separated ("" if not a file stream).
static string stream_path(OMediaStreamOperators *stream)
{
	OMediaFileStream *fs = dynamic_cast<OMediaFileStream *>(stream);
	if (!fs) return "";
	OMediaFilePath fp;
	fs->getpath(&fp);
	string p = ((OMediaSDLRtgFilePath *)fp.getretarget())->path;
	for (size_t i = 0; i < p.size(); i++) if (p[i] == '\\') p[i] = '/';
	return p;
}

void omhd::tag_file(OMediaCanvas *c, OMediaStreamOperators *stream)
{
	if (pack().files.empty()) return;
	const string p = lower(stream_path(stream));
	const size_t s = p.find_last_of('/');
	if (s == string::npos) return;
	const string head = p.substr(0, s);
	const size_t s2 = head.find_last_of('/');
	if ((s2 == string::npos ? head : head.substr(s2 + 1)) == "png") tag(c, "png/" + p.substr(s + 1));
}

string omhd::db_name(OMediaStream *stream)
{
	if (pack().files.empty()) return "";
	string p = lower(stream_path(stream));
	const size_t s = p.find_last_of('/');
	if (s != string::npos) p = p.substr(s + 1);
	if (p.size() > 4 && p.compare(p.size() - 4, 4, ".omt") == 0) p.resize(p.size() - 4);
	return p;
}

OMediaCanvas *omhd::load(OMediaCanvas *orig, int max_size, int &scale)
{
	scale = 0;
	if (!orig || orig->hd_key.empty()) return NULL;
	Pack &p = pack();
	std::map<string, string>::iterator it = p.files.find(orig->hd_key);
	if (it == p.files.end()) return NULL;
	const int w = orig->get_width(), h = orig->get_height();
	if (w <= 0 || h <= 0) return NULL;

	OMediaCanvas *hd = new OMediaCanvas;
	const char *err = read_png(it->second, hd);
	if (err)
	{
		log_once(orig->hd_key, (string(err) + ": skipped").c_str());
		delete hd;
		return NULL;
	}
	const int hw = hd->get_width(), hh = hd->get_height();
	const int k = hw / w;
	char m[160];
	if (k < 1 || hw != w * k || hh != h * k)
	{
		snprintf(m, sizeof m, "is %dx%d, not an integer multiple of %dx%d: skipped", hw, hh, w, h);
		log_once(orig->hd_key, m);
		delete hd;
		return NULL;
	}
	int kf = k;
	while (kf > 1 && (w * kf > max_size || h * kf > max_size)) kf--;
	if (kf < 2)
	{
		snprintf(m, sizeof m, "is %dx%d, over the texture size limit %d: skipped", hw, hh, max_size);
		if (k > 1) log_once(orig->hd_key, m);
		delete hd;
		return NULL;
	}
	if (kf != k)
	{
		snprintf(m, sizeof m, "is %dx%d, over the texture size limit %d: using %dx", hw, hh, max_size, kf);
		log_once(orig->hd_key, m);
		OMediaCanvas *s = new OMediaCanvas;
		s->create(w * kf, h * kf, hd);
		delete hd;
		hd = s;
	}
	scale = kf;
	return hd;
}

bool omhd::add_bytes(OMediaCanvas *orig, double bytes)
{
	Pack &p = pack();
	if (p.bytes + bytes > p.budget)
	{
		if (!p.budget_logged) SDL_Log("HD: memory budget (%.0f MB) full at %s: the rest stays original",
									  p.budget / (1024.0 * 1024.0), orig ? orig->hd_key.c_str() : "?");
		p.budget_logged = true;
		return false;
	}
	p.bytes += bytes;
	p.replaced++;
	static const bool verbose = getenv("NEG_HD_LOG") && atoi(getenv("NEG_HD_LOG")) > 0;
	if (verbose && orig) SDL_Log("HD: %s (%.1f MB)", orig->hd_key.c_str(), bytes / (1024.0 * 1024.0));
	return true;
}

void omhd::sub_bytes(double bytes)
{
	g_pack.bytes -= bytes;
	if (g_pack.bytes < 0) g_pack.bytes = 0;
}
