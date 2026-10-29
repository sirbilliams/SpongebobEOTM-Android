/*  sb_hdtool: the HD background pack tool (docs/hd-pack.md).  Works only on the player's own installed game; it
    never ships or contains any art.

      sb_hdtool export <game dir> <out dir> [scene prefix]
          every scene background ("background*") and sky ("sky*") canvas of <game dir>/sceneart/*.omt as an RGBA PNG:
          <out>/sceneart/<scene>/<canvas>.png, plus <out>/manifest.tsv.  The prefix (e.g. "c1") limits the scenes.
      sb_hdtool pack <export dir> <pack dir|pack.zip> [--scale N] [--upscaler "<cmd>"] [--upscaled <dir>]
          upscales every exported picture N times (default 2) and writes the pack with its manifest.  The picture
          comes from, in this order: --upscaled <dir> (the same names, upscaled by a batch tool you ran yourself),
          --upscaler (a command run per picture; {in} {out} {scale} are replaced, e.g.
          realesrgan-ncnn-vulkan -i {in} -o {out} -s {scale} -n realesrgan-x4plus), else the built-in Lanczos
          resampler (a plain 2x for testing: no new detail).  A result of the wrong size is resampled to
          original*N; the transparency of the original is kept as it was.
      sb_hdtool verify <pack dir>
          decodes every picture with the game's own loader (CRC, size) and checks it against manifest.tsv.

    manifest.tsv, one row per picture (header first): file  scene  canvas  orig_w  orig_h  scale  canvas_w  canvas_h
    offset_x  offset_y  sha256.  canvas_w/h is the area the picture covers, in original pixels: orig_w x orig_h for a
    plain upscale; a wider area (outpainted 16:9 pack) needs the camera work of the docs and is not drawn yet.   */

#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaError.h"
#include "OMediaCanvas.h"
#include "OMediaGfxWorld.h"
#include "OMediaHDPng.h"
#include <zlib.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <algorithm>
#include <string>
#include <vector>

extern char **omt_argv;
extern int omt_argc;

using std::string;
using std::vector;

typedef vector<unsigned char> Bytes;

struct Image { int w, h; Bytes rgba; Image() : w(0), h(0) {} };

// ------------------------------------------------------------------------------------------------ files

static bool is_dir(const string &p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }

static void make_dirs(const string &path)
{
	for (size_t i = 1; i <= path.size(); i++)
		if (i == path.size() || path[i] == '/') mkdir(path.substr(0, i).c_str(), 0755);
}

static bool read_file(const string &p, Bytes &out)
{
	FILE *f = fopen(p.c_str(), "rb");
	if (!f) return false;
	unsigned char buf[65536];
	size_t n;
	out.clear();
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
	fclose(f);
	return true;
}

static bool write_file(const string &p, const Bytes &b)
{
	FILE *f = fopen(p.c_str(), "wb");
	if (!f) return false;
	const bool ok = b.empty() || fwrite(&b[0], 1, b.size(), f) == b.size();
	return fclose(f) == 0 && ok;
}

static string lower(string s) { for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]); return s; }

static vector<string> list_dir(const string &d)
{
	vector<string> v;
	DIR *dir = opendir(d.c_str());
	if (!dir) return v;
	struct dirent *e;
	while ((e = readdir(dir)) != NULL) if (e->d_name[0] != '.') v.push_back(e->d_name);
	closedir(dir);
	std::sort(v.begin(), v.end());
	return v;
}

// ------------------------------------------------------------------------------------------------ SHA-256

static string sha256(const Bytes &data)
{
	static const unsigned int K[64] = {
		0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
		0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
		0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
		0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
		0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
		0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
	unsigned int h[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
	Bytes m(data);
	const unsigned long long bits = (unsigned long long)data.size() * 8;
	m.push_back(0x80);
	while (m.size() % 64 != 56) m.push_back(0);
	for (int i = 7; i >= 0; i--) m.push_back((unsigned char)(bits >> (i * 8)));
	for (size_t o = 0; o < m.size(); o += 64)
	{
		unsigned int w[64];
		for (int i = 0; i < 16; i++) w[i] = (m[o + i*4] << 24) | (m[o + i*4 + 1] << 16) | (m[o + i*4 + 2] << 8) | m[o + i*4 + 3];
		#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
		for (int i = 16; i < 64; i++)
		{
			const unsigned int s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
			w[i] = w[i-16] + s0 + w[i-7] + s1;
		}
		unsigned int a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
		for (int i = 0; i < 64; i++)
		{
			const unsigned int S1 = ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25), ch = (e & f) ^ (~e & g), t1 = hh + S1 + ch + K[i] + w[i];
			const unsigned int S0 = ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = S0 + mj;
			hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
		}
		#undef ROR
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
	}
	char out[65];
	for (int i = 0; i < 8; i++) snprintf(out + i * 8, 9, "%08x", h[i]);
	return out;
}

// ------------------------------------------------------------------------------------------------ PNG

static void put32(Bytes &b, unsigned int v) { b.push_back(v >> 24); b.push_back(v >> 16); b.push_back(v >> 8); b.push_back(v); }

static void chunk(Bytes &out, const char *type, const Bytes &data)
{
	put32(out, (unsigned)data.size());
	Bytes td(type, type + 4);
	td.insert(td.end(), data.begin(), data.end());
	out.insert(out.end(), td.begin(), td.end());
	put32(out, (unsigned)crc32(crc32(0L, Z_NULL, 0), &td[0], (uInt)td.size()));
}

static bool encode_png(const Image &im, Bytes &out)
{
	static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	out.assign(sig, sig + 8);
	Bytes ihdr;
	put32(ihdr, im.w); put32(ihdr, im.h);
	ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
	chunk(out, "IHDR", ihdr);
	Bytes raw;
	raw.reserve((size_t)(im.w * 4 + 1) * im.h);
	for (int y = 0; y < im.h; y++)
	{
		raw.push_back(0);
		raw.insert(raw.end(), im.rgba.begin() + (size_t)y * im.w * 4, im.rgba.begin() + (size_t)(y + 1) * im.w * 4);
	}
	uLongf n = compressBound((uLong)raw.size());
	Bytes z(n);
	if (compress2(&z[0], &n, &raw[0], (uLong)raw.size(), 6) != Z_OK) return false;
	z.resize(n);
	chunk(out, "IDAT", z);
	chunk(out, "IEND", Bytes());
	return true;
}

static bool load_png(const string &file, Image &im, string &err)
{
	omhd::DecodedPng d;
	const char *e = omhd::decode_png(file, d, 1024u * 1024u * 1024u);
	if (e) { err = e; return false; }
	im.w = (int)d.width; im.h = (int)d.height; im.rgba.swap(d.pixels);
	return true;
}

// ------------------------------------------------------------------------------------------------ Lanczos resampler

static double lanczos3(double x)
{
	if (x < 0) x = -x;
	if (x < 1e-9) return 1.0;
	if (x >= 3.0) return 0.0;
	const double px = 3.14159265358979323846 * x;
	return 3.0 * sin(px) * sin(px / 3.0) / (px * px);
}

struct Tap { int first, n; vector<float> w; };

static vector<Tap> make_taps(int src, int dst)
{
	vector<Tap> taps(dst);
	const double scale = (double)src / dst, fscale = scale > 1.0 ? scale : 1.0, support = 3.0 * fscale;
	for (int i = 0; i < dst; i++)
	{
		const double centre = (i + 0.5) * scale;
		int lo = (int)floor(centre - support + 0.5), hi = (int)floor(centre + support + 0.5);
		if (lo < 0) lo = 0;
		if (hi > src) hi = src;
		Tap &t = taps[i];
		t.first = lo; t.n = hi - lo;
		double sum = 0;
		for (int j = lo; j < hi; j++) { const double w = lanczos3((j + 0.5 - centre) / fscale); t.w.push_back((float)w); sum += w; }
		if (sum != 0) for (size_t j = 0; j < t.w.size(); j++) t.w[j] = (float)(t.w[j] / sum);
	}
	return taps;
}

// Alpha is resampled premultiplied so the colour key's hidden RGB does not bleed into the edge.
static Image resize(const Image &s, int dw, int dh)
{
	Image o;
	o.w = dw; o.h = dh;
	vector<float> pre((size_t)s.w * s.h * 4);
	for (size_t i = 0; i < (size_t)s.w * s.h; i++)
	{
		const float a = s.rgba[i * 4 + 3] / 255.0f;
		pre[i * 4] = s.rgba[i * 4] * a; pre[i * 4 + 1] = s.rgba[i * 4 + 1] * a; pre[i * 4 + 2] = s.rgba[i * 4 + 2] * a; pre[i * 4 + 3] = a * 255.0f;
	}
	const vector<Tap> tx = make_taps(s.w, dw), ty = make_taps(s.h, dh);
	vector<float> mid((size_t)dw * s.h * 4);
	for (int y = 0; y < s.h; y++)
		for (int x = 0; x < dw; x++)
		{
			float acc[4] = { 0, 0, 0, 0 };
			for (int k = 0; k < tx[x].n; k++)
				for (int c = 0; c < 4; c++) acc[c] += tx[x].w[k] * pre[((size_t)y * s.w + tx[x].first + k) * 4 + c];
			for (int c = 0; c < 4; c++) mid[((size_t)y * dw + x) * 4 + c] = acc[c];
		}
	o.rgba.resize((size_t)dw * dh * 4);
	for (int y = 0; y < dh; y++)
		for (int x = 0; x < dw; x++)
		{
			float acc[4] = { 0, 0, 0, 0 };
			for (int k = 0; k < ty[y].n; k++)
				for (int c = 0; c < 4; c++) acc[c] += ty[y].w[k] * mid[((size_t)(ty[y].first + k) * dw + x) * 4 + c];
			float a = acc[3] / 255.0f;
			if (a < 0) a = 0;
			if (a > 1) a = 1;
			for (int c = 0; c < 3; c++)
			{
				float v = a > 0.0001f ? acc[c] / a : 0.0f;
				o.rgba[((size_t)y * dw + x) * 4 + c] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5f);
			}
			o.rgba[((size_t)y * dw + x) * 4 + 3] = (unsigned char)(a * 255.0f + 0.5f);
		}
	return o;
}

static bool has_alpha(const Image &im)
{
	for (size_t i = 3; i < im.rgba.size(); i += 4) if (im.rgba[i] != 255) return true;
	return false;
}

// ------------------------------------------------------------------------------------------------ manifest

struct Row { string file, scene, canvas; int ow, oh, scale, cw, ch, ox, oy; string sha; Row() : ow(0), oh(0), scale(0), cw(0), ch(0), ox(0), oy(0) {} };

static const char *kHeader = "file\tscene\tcanvas\torig_w\torig_h\tscale\tcanvas_w\tcanvas_h\toffset_x\toffset_y\tsha256\n";

static bool write_manifest(const string &path, const vector<Row> &rows)
{
	string t = kHeader;
	for (size_t i = 0; i < rows.size(); i++)
	{
		const Row &r = rows[i];
		char b[1024];
		snprintf(b, sizeof b, "%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n", r.file.c_str(), r.scene.c_str(), r.canvas.c_str(), r.ow, r.oh, r.scale, r.cw, r.ch, r.ox, r.oy, r.sha.c_str());
		t += b;
	}
	return write_file(path, Bytes(t.begin(), t.end()));
}

static vector<Row> read_manifest(const string &path)
{
	vector<Row> rows;
	Bytes b;
	if (!read_file(path, b)) return rows;
	string t(b.begin(), b.end());
	size_t pos = 0;
	while (pos < t.size())
	{
		size_t e = t.find('\n', pos);
		if (e == string::npos) e = t.size();
		string line = t.substr(pos, e - pos);
		pos = e + 1;
		if (line.empty() || line[0] == '#' || line.compare(0, 5, "file\t") == 0) continue;
		vector<string> c;
		size_t p = 0;
		for (;;) { size_t q = line.find('\t', p); c.push_back(line.substr(p, q == string::npos ? q : q - p)); if (q == string::npos) break; p = q + 1; }
		if (c.size() < 11) continue;
		Row r;
		r.file = c[0]; r.scene = c[1]; r.canvas = c[2]; r.ow = atoi(c[3].c_str()); r.oh = atoi(c[4].c_str()); r.scale = atoi(c[5].c_str());
		r.cw = atoi(c[6].c_str()); r.ch = atoi(c[7].c_str()); r.ox = atoi(c[8].c_str()); r.oy = atoi(c[9].c_str()); r.sha = c[10];
		rows.push_back(r);
	}
	return rows;
}

// ------------------------------------------------------------------------------------------------ zip (deflate)

struct ZipItem { string name; unsigned int crc, csize, usize, off; unsigned short method; };

static void le16(Bytes &b, unsigned v) { b.push_back(v); b.push_back(v >> 8); }
static void le32(Bytes &b, unsigned v) { b.push_back(v); b.push_back(v >> 8); b.push_back(v >> 16); b.push_back(v >> 24); }

static bool zip_dir(const string &dir, const string &zip)
{
	Bytes out;
	vector<ZipItem> items;
	vector<string> stack(1, "");
	vector<string> files;
	while (!stack.empty())
	{
		const string rel = stack.back(); stack.pop_back();
		const vector<string> names = list_dir(rel.empty() ? dir : dir + "/" + rel);
		for (size_t i = 0; i < names.size(); i++)
		{
			const string r = rel.empty() ? names[i] : rel + "/" + names[i];
			if (is_dir(dir + "/" + r)) stack.push_back(r); else files.push_back(r);
		}
	}
	std::sort(files.begin(), files.end());
	for (size_t i = 0; i < files.size(); i++)
	{
		Bytes data;
		if (!read_file(dir + "/" + files[i], data)) return false;
		ZipItem it;
		it.name = files[i]; it.off = (unsigned)out.size(); it.usize = (unsigned)data.size();
		it.crc = (unsigned)crc32(crc32(0L, Z_NULL, 0), data.empty() ? (const Bytef *)"" : &data[0], (uInt)data.size());
		Bytes z(compressBound((uLong)data.size()) + 16);
		z_stream zs; memset(&zs, 0, sizeof zs);
		if (deflateInit2(&zs, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;
		zs.next_in = data.empty() ? (Bytef *)"" : &data[0]; zs.avail_in = (uInt)data.size();
		zs.next_out = &z[0]; zs.avail_out = (uInt)z.size();
		const int rc = deflate(&zs, Z_FINISH);
		it.csize = (unsigned)zs.total_out;
		deflateEnd(&zs);
		if (rc != Z_STREAM_END) return false;
		it.method = 8;
		le32(out, 0x04034b50); le16(out, 20); le16(out, 0); le16(out, it.method); le16(out, 0); le16(out, 0x21);
		le32(out, it.crc); le32(out, it.csize); le32(out, it.usize); le16(out, (unsigned)it.name.size()); le16(out, 0);
		out.insert(out.end(), it.name.begin(), it.name.end());
		out.insert(out.end(), z.begin(), z.begin() + it.csize);
		items.push_back(it);
	}
	const unsigned cd = (unsigned)out.size();
	for (size_t i = 0; i < items.size(); i++)
	{
		const ZipItem &it = items[i];
		le32(out, 0x02014b50); le16(out, 20); le16(out, 20); le16(out, 0); le16(out, it.method); le16(out, 0); le16(out, 0x21);
		le32(out, it.crc); le32(out, it.csize); le32(out, it.usize); le16(out, (unsigned)it.name.size()); le16(out, 0); le16(out, 0);
		le16(out, 0); le16(out, 0); le32(out, 0); le32(out, it.off);
		out.insert(out.end(), it.name.begin(), it.name.end());
	}
	const unsigned cdsize = (unsigned)out.size() - cd;
	le32(out, 0x06054b50); le16(out, 0); le16(out, 0); le16(out, (unsigned)items.size()); le16(out, (unsigned)items.size());
	le32(out, cdsize); le32(out, cd); le16(out, 0);
	return write_file(zip, out);
}

// ------------------------------------------------------------------------------------------------ export

static bool wanted_canvas(const string &n) { const string l = lower(n); return l.compare(0, 10, "background") == 0 || l.compare(0, 3, "sky") == 0; }

static bool safe_name(const string &n) { if (n.empty()) return false; for (size_t i = 0; i < n.size(); i++) if (n[i] == '/' || n[i] == '\\' || n[i] == ':' || (unsigned char)n[i] < 32) return false; return n != "." && n != ".."; }

static int cmd_export(const string &game, const string &out, const string &prefix)
{
	const string dir = game + "/sceneart";
	if (!is_dir(dir)) { fprintf(stderr, "sb_hdtool: %s has no sceneart/ folder\n", game.c_str()); return 1; }
	OMediaDataBase::set_cache_size(0);
	omd_REGISTERCLASS(OMediaCanvas);
	vector<Row> rows;
	const vector<string> names = list_dir(dir);
	for (size_t n = 0; n < names.size(); n++)
	{
		const string f = lower(names[n]);
		if (f.size() < 5 || f.compare(f.size() - 4, 4, ".omt") != 0) continue;
		const string scene = f.substr(0, f.size() - 4);
		if (!prefix.empty() && scene.compare(0, prefix.size(), lower(prefix)) != 0) continue;
		try
		{
			OMediaFilePath path(dir + "/" + names[n]);
			OMediaFileStream file;
			file.setpath(&path);
			file.open(omcfp_Read);
			OMediaDataBase *db = new OMediaDataBase(&file);
			omt_DBIterator it;
			for (OMediaDBObject *o = db->get_first_object(OMediaCanvas::db_type, it); o; o = db->get_next_object(it))
			{
				string cname;
				o->get_chunk_name(cname);
				if (!wanted_canvas(cname) || !safe_name(cname)) continue;
				OMediaCanvas *cv = (OMediaCanvas *)o;
				OMediaGfxWorld *g = cv->getgfxworld();
				OMediaGfxPixels *px = g ? g->port_pixels() : NULL;
				if (!px || !px->pix || px->width <= 0 || px->height <= 0) { fprintf(stderr, "  %s/%s: no pixels, skipped\n", scene.c_str(), cname.c_str()); continue; }
				Image im;
				im.w = px->width; im.h = px->height; im.rgba.resize((size_t)im.w * im.h * 4);
				for (size_t i = 0; i < (size_t)im.w * im.h; i++)
				{
					const unsigned int p = px->pix[i];
					im.rgba[i * 4] = (unsigned char)(p >> 16); im.rgba[i * 4 + 1] = (unsigned char)(p >> 8); im.rgba[i * 4 + 2] = (unsigned char)p;
					im.rgba[i * 4 + 3] = (unsigned char)(p >> 24);
				}
				Bytes png;
				Row r;
				r.scene = scene; r.canvas = lower(cname);
				r.file = "sceneart/" + scene + "/" + r.canvas + ".png";
				if (!encode_png(im, png)) { fprintf(stderr, "  %s: PNG encoding failed\n", r.file.c_str()); return 1; }
				make_dirs(out + "/sceneart/" + scene);
				if (!write_file(out + "/" + r.file, png)) { fprintf(stderr, "  cannot write %s/%s\n", out.c_str(), r.file.c_str()); return 1; }
				r.ow = r.cw = im.w; r.oh = r.ch = im.h; r.scale = 1; r.sha = sha256(png);
				rows.push_back(r);
			}
			delete db;
		}
		catch (OMediaError &) { fprintf(stderr, "  %s: cannot be read, skipped\n", names[n].c_str()); }
	}
	make_dirs(out);
	if (!write_manifest(out + "/manifest.tsv", rows)) { fprintf(stderr, "sb_hdtool: cannot write the manifest\n"); return 1; }
	printf("exported %d pictures to %s\n", (int)rows.size(), out.c_str());
	return rows.empty() ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------ pack

static string shell_quote(const string &s)
{
	string q = "'";
	for (size_t i = 0; i < s.size(); i++) { if (s[i] == '\'') q += "'\\''"; else q += s[i]; }
	return q + "'";
}

static string replace_all(string s, const string &k, const string &v)
{
	for (size_t p = 0; (p = s.find(k, p)) != string::npos; p += v.size()) s.replace(p, k.size(), v);
	return s;
}

static int cmd_pack(const string &exp, const string &dest, int scale, const string &upscaler, const string &upscaled)
{
	const vector<Row> src = read_manifest(exp + "/manifest.tsv");
	if (src.empty()) { fprintf(stderr, "sb_hdtool: %s/manifest.tsv is missing or empty (run export first)\n", exp.c_str()); return 1; }
	const bool zip = dest.size() > 4 && lower(dest.substr(dest.size() - 4)) == ".zip";
	const string out = zip ? dest + ".tmp-pack" : dest;
	if (zip ? is_dir(out) : is_dir(dest) && !list_dir(dest).empty()) { fprintf(stderr, "sb_hdtool: %s already exists; pick a new name\n", out.c_str()); return 1; }
	if (zip) { FILE *t = fopen(dest.c_str(), "rb"); if (t) { fclose(t); fprintf(stderr, "sb_hdtool: %s already exists; pick a new name\n", dest.c_str()); return 1; } }
	vector<Row> rows;
	int hooked = 0, builtin = 0, resized = 0;
	for (size_t i = 0; i < src.size(); i++)
	{
		const Row &s = src[i];
		Image orig;
		string err;
		if (!load_png(exp + "/" + s.file, orig, err)) { fprintf(stderr, "  %s: %s, skipped\n", s.file.c_str(), err.c_str()); continue; }
		if (orig.w != s.ow || orig.h != s.oh) { fprintf(stderr, "  %s: is %dx%d, the manifest says %dx%d, skipped\n", s.file.c_str(), orig.w, orig.h, s.ow, s.oh); continue; }
		const int tw = orig.w * scale, th = orig.h * scale;
		Image hd;
		bool got = false;
		string from = upscaled.empty() ? "" : upscaled + "/" + s.file;
		if (!upscaled.empty())
		{
			if (!load_png(from, hd, err)) fprintf(stderr, "  %s: %s, trying the next source\n", from.c_str(), err.c_str()); else got = true;
		}
		if (!got && !upscaler.empty())
		{
			const string tmp = out + ".hook.png";
			string cmd = replace_all(replace_all(replace_all(upscaler, "{in}", shell_quote(exp + "/" + s.file)), "{out}", shell_quote(tmp)), "{scale}", "4");
			if (upscaler.find("{scale}") != string::npos) cmd = replace_all(replace_all(replace_all(upscaler, "{in}", shell_quote(exp + "/" + s.file)), "{out}", shell_quote(tmp)), "{scale}", string(1, (char)('0' + scale)));
			remove(tmp.c_str());
			const int rc = system(cmd.c_str());
			if (rc != 0 || !load_png(tmp, hd, err)) fprintf(stderr, "  %s: the upscaler failed (%s), using the built-in resampler\n", s.file.c_str(), rc != 0 ? "exit code" : err.c_str());
			else { got = true; hooked++; }
			remove(tmp.c_str());
		}
		if (!got) { hd = resize(orig, tw, th); builtin++; }
		else if (hd.w != tw || hd.h != th) { hd = resize(hd, tw, th); resized++; }
		if (got && has_alpha(orig))		// the transparency is the original's, not what an AI upscaler made of it
		{
			const Image a = resize(orig, tw, th);
			for (size_t p = 3; p < hd.rgba.size(); p += 4) hd.rgba[p] = a.rgba[p];
		}
		Bytes png;
		if (!encode_png(hd, png)) { fprintf(stderr, "  %s: PNG encoding failed\n", s.file.c_str()); return 1; }
		make_dirs(out + "/sceneart/" + s.scene);
		if (!write_file(out + "/" + s.file, png)) { fprintf(stderr, "  cannot write %s/%s\n", out.c_str(), s.file.c_str()); return 1; }
		Row r = s;
		r.scale = scale; r.cw = s.ow; r.ch = s.oh; r.sha = sha256(png);
		rows.push_back(r);
	}
	if (rows.empty()) { fprintf(stderr, "sb_hdtool: nothing packed\n"); return 1; }
	make_dirs(out);
	if (!write_manifest(out + "/manifest.tsv", rows)) { fprintf(stderr, "sb_hdtool: cannot write the manifest\n"); return 1; }
	if (zip)
	{
		const bool ok = zip_dir(out, dest);
		for (size_t i = 0; i < rows.size(); i++) remove((out + "/" + rows[i].file).c_str());
		for (size_t i = 0; i < rows.size(); i++) rmdir((out + "/sceneart/" + rows[i].scene).c_str());
		remove((out + "/manifest.tsv").c_str()); rmdir((out + "/sceneart").c_str()); rmdir(out.c_str());
		if (!ok) { fprintf(stderr, "sb_hdtool: cannot write %s\n", dest.c_str()); return 1; }
	}
	printf("packed %d pictures at %dx into %s (%d from the upscaler, %d built-in Lanczos, %d resized to fit)\n", (int)rows.size(), scale, dest.c_str(), hooked, builtin, resized);
	return 0;
}

// ------------------------------------------------------------------------------------------------ verify

static int cmd_verify(const string &pack)
{
	const vector<Row> rows = read_manifest(pack + "/manifest.tsv");
	if (rows.empty()) { fprintf(stderr, "sb_hdtool: %s/manifest.tsv is missing or empty\n", pack.c_str()); return 1; }
	int bad = 0;
	for (size_t i = 0; i < rows.size(); i++)
	{
		const Row &r = rows[i];
		Bytes raw;
		Image im;
		string err;
		if (!read_file(pack + "/" + r.file, raw)) err = "is missing";
		else if (sha256(raw) != r.sha) err = "does not match the manifest sha256";
		else if (!load_png(pack + "/" + r.file, im, err)) {}
		else if (im.w != r.ow * r.scale || im.h != r.oh * r.scale) err = "is not original size x scale";
		if (!err.empty()) { printf("BAD  %s %s\n", r.file.c_str(), err.c_str()); bad++; }
	}
	printf("%d pictures checked, %d bad\n", (int)rows.size(), bad);
	return bad ? 1 : 0;
}

void omt_main(void)
{
	int argc = omt_argc;
	char **argv = omt_argv;
	int rc = 2;
	const string cmd = argc > 1 ? argv[1] : "";
	if (cmd == "export" && argc >= 4) rc = cmd_export(argv[2], argv[3], argc > 4 ? argv[4] : "");
	else if (cmd == "pack" && argc >= 4)
	{
		int scale = 2;
		string up, upd;
		for (int i = 4; i < argc; i++)
		{
			if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
			else if (!strcmp(argv[i], "--upscaler") && i + 1 < argc) up = argv[++i];
			else if (!strcmp(argv[i], "--upscaled") && i + 1 < argc) upd = argv[++i];
			else { fprintf(stderr, "sb_hdtool: unknown option %s\n", argv[i]); scale = 0; }
		}
		if (scale < 1 || scale > 4) { fprintf(stderr, "sb_hdtool: --scale is 1 to 4\n"); rc = 2; }
		else rc = cmd_pack(argv[2], argv[3], scale, up, upd);
	}
	else if (cmd == "verify" && argc >= 3) rc = cmd_verify(argv[2]);
	else
	{
		fprintf(stderr, "usage: sb_hdtool export <game dir> <out dir> [scene prefix]\n"
						"       sb_hdtool pack <export dir> <pack dir|pack.zip> [--scale N] [--upscaler \"cmd {in} {out} {scale}\"] [--upscaled <dir>]\n"
						"       sb_hdtool verify <pack dir>\n");
	}
	if (rc) exit(rc);
}
