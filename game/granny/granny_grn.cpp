/*  granny_grn.cpp - reads the game's RAD Granny 1.x (.grn, "1.2b", 2000) files and builds from them what
    granny.dll builds at GrannyOpenModel / GrannyOpenAnimation: the skeleton, the decoded textures, the
    render meshes (render-order vertex arrays, bone bindings with their inverse rest transforms, weight lists,
    copy lists, the triangle lists of the rendering states) and the animation tracks.  The runtime that uses
    them is granny_compat.cpp; docs/granny-replacement.md describes the format and what granny.dll does.

    File layout (little-endian):
      0x40  u32 0xca5e0000, u32 nsections, ...
      0x60  nsections x { u32 tag, u32 0, u32 offset, u32 crc, u32 0 }
      section: u32 nentries, 3 x u32, then nentries x { u32 tag, u32 offset, u32 ndescendants }: a pre-order
               tree of nodes (offsets from the section start); a node's data runs to the next entry's offset.
    The node tags (low 16 bits of 0xca5eXXXX) used are listed with the readers below.

    Portable C++98: no assumptions about byte order, alignment or pointer size.                           */
#include "granny_internal.h"
#include "granny_math.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <map>

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

namespace granny_internal {

namespace {

/* ------------------------------------------------------------------------------- file access --- */

static bool read_file(const std::string &path, std::vector<unsigned char> &out)
{
	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp) return false;
	unsigned char tmp[65536];
	size_t n;
	out.clear();
	while ((n = fread(tmp, 1, sizeof tmp, fp)) > 0) out.insert(out.end(), tmp, tmp + n);
	fclose(fp);
	return true;
}

static bool same_nocase(const char *a, const char *b)
{
	for (;; a++, b++)
	{
		char x = *a, y = *b;
		if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
		if (x != y) return false;
		if (!x) return true;
	}
}

static bool exists(const std::string &p)
{
	FILE *fp = fopen(p.c_str(), "rb");
	if (!fp) return false;
	fclose(fp);
	return true;
}

/* ------------------------------------------------------------------- granny.dll's Bink tables --- */

/* where they are in granny.dll 1.2b (virtual addresses; image base 0x10000000) and their crc32s */
struct DllBlock { u32 va, size, crc; };
static const DllBlock DLL_HUFF = { 0x1005f848, 0x3d0, 0x709313c8 };      /* 16 Huffman lookup tables       */
static const DllBlock DLL_HUFF_PTR = { 0x1006eda0, 0x40, 0xfc5c45bd };   /* their addresses (0x100448c0)  */
static const DllBlock DLL_HUFF_BITS = { 0x1005fc20, 0x10, 0xe3e16d86 };  /* their index widths            */
static const DllBlock DLL_RLE = { 0x1005fc30, 0x4, 0x37ee7637 };         /* block type run lengths        */
static const DllBlock DLL_PATTERNS = { 0x1005f440, 0x400, 0xf9e3218a };  /* 16 x 64 run-block scan orders */
static const DllBlock DLL_SCAN = { 0x1006f05c, 0x40, 0x8fbdd9f2 };       /* DCT coefficient order         */
static const DllBlock DLL_INTRA_Q = { 0x10060018, 0x1000, 0xd53c4639 };  /* 0x10049b30                    */

static u32 crc32(const unsigned char *p, size_t n)
{
	u32 c = 0xffffffffu;
	for (size_t i = 0; i < n; i++)
	{
		c ^= p[i];
		for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
	}
	return c ^ 0xffffffffu;
}

struct DllData
{
	bool tried;
	std::string path, why;
	std::vector<unsigned char> huff, huff_ptr, huff_bits, rle, patterns, scan;
	std::vector<int32_t> intra_q;
	BinkTables tab;
};

static DllData g_dll;

static bool dll_read(const std::vector<unsigned char> &f, const DllBlock &blk, std::vector<unsigned char> &out)
{
	if (f.size() < 0x40) return false;
	u32 pe = rd32(&f[0x3c]);
	if (pe + 0xf8 > f.size() || rd32(&f[pe]) != 0x4550) return false;
	u32 base = rd32(&f[pe + 0x34]);
	u32 nsec = (u32)f[pe + 6] | (u32)f[pe + 7] << 8, opt = (u32)f[pe + 0x14] | (u32)f[pe + 0x15] << 8;
	u32 sec = pe + 0x18 + opt;
	out.assign(blk.size, 0);
	bool found = false;
	for (u32 i = 0; i < nsec && sec + 40 * (i + 1) <= f.size(); i++)
	{
		const unsigned char *s = &f[sec + 40 * i];
		u32 va = base + rd32(s + 12), rawsize = rd32(s + 16), raw = rd32(s + 20);
		if (blk.va >= va && blk.va + blk.size <= va + rawsize && raw + (blk.va - va) + blk.size <= f.size())
		{
			memcpy(&out[0], &f[raw + (blk.va - va)], blk.size);
			found = true;
		}
	}
	return found && crc32(&out[0], out.size()) == blk.crc;
}

static std::string g_dll_path;

/* granny.dll: the path set by GrannyCompatSetDllPath, else next to the directory of the first .grn file
   opened or in it (the installed game has grn/ next to granny.dll), else in the current directory */
static void load_dll_tables(const std::string &near)
{
	DllData &d = g_dll;
	d.tried = true;
	d.tab.ok = false;
	std::vector<std::string> cand;
	if (!g_dll_path.empty()) cand.push_back(g_dll_path);
	else
	{
		size_t slash = near.rfind('/');
		std::string dir = slash == std::string::npos ? std::string(".") : near.substr(0, slash);
		cand.push_back(dir + "/../granny.dll");
		cand.push_back(dir + "/granny.dll");
		cand.push_back("granny.dll");
	}
	std::vector<unsigned char> f;
	for (size_t i = 0; i < cand.size(); i++)
	{
		std::string p = grn_find_file(cand[i]);
		if (exists(p) && read_file(p, f)) { d.path = p; break; }
	}
	if (d.path.empty()) { d.why = "granny.dll not found (it is needed for the Bink texture tables)"; return; }
	std::vector<unsigned char> iq;
	if (!dll_read(f, DLL_HUFF, d.huff) || !dll_read(f, DLL_HUFF_PTR, d.huff_ptr) ||
	    !dll_read(f, DLL_HUFF_BITS, d.huff_bits) || !dll_read(f, DLL_RLE, d.rle) ||
	    !dll_read(f, DLL_PATTERNS, d.patterns) || !dll_read(f, DLL_SCAN, d.scan) ||
	    !dll_read(f, DLL_INTRA_Q, iq))
	{
		d.why = d.path + " is not the game's granny.dll (1.2b)";
		return;
	}
	d.intra_q.resize(1024);
	for (int i = 0; i < 1024; i++) d.intra_q[i] = (int32_t)rd32(&iq[4 * i]);
	for (int t = 0; t < 16; t++)
	{
		u32 off = rd32(&d.huff_ptr[4 * t]) - DLL_HUFF.va;
		d.tab.huff_bits[t] = d.huff_bits[t];
		if (off + (1u << d.huff_bits[t]) > DLL_HUFF.size) { d.why = "bad Huffman tables in " + d.path; return; }
		d.tab.huff[t] = &d.huff[off];
	}
	for (int i = 0; i < 4; i++) d.tab.rle_lens[i] = d.rle[i];
	d.tab.patterns = &d.patterns[0];
	d.tab.scan = &d.scan[0];
	d.tab.intra_quant = &d.intra_q[0];
	d.tab.ok = true;
}

/* ------------------------------------------------------------------------------ the node tree --- */

struct Node
{
	u32 tag;           /* low 16 bits */
	size_t off, end;
	std::vector<int> kids;
};

struct Grn
{
	std::vector<unsigned char> b;
	std::vector<Node> nodes;
	std::vector<int> roots;            /* top-level nodes of the main section (0x0103) */
	std::vector<std::string> strings;
	std::vector<std::string> object_names;   /* objects 1.. (0 unused) */
	bool ok;

	u32 u(size_t o) const { return o + 4 <= b.size() ? rd32(&b[o]) : 0; }
	float f(size_t o) const { return o + 4 <= b.size() ? rdf32(&b[o]) : 0.0f; }
	size_t own(int n) const   /* bytes of the node's own data (before its first child) */
	{
		const Node &x = nodes[n];
		size_t e = x.kids.empty() ? x.end : nodes[x.kids[0]].off;
		return e > x.off ? e - x.off : 0;
	}
	void find(int n, u32 tag, std::vector<int> &out) const
	{
		for (size_t i = 0; i < nodes[n].kids.size(); i++) if (nodes[nodes[n].kids[i]].tag == tag) out.push_back(nodes[n].kids[i]);
	}
	int first(int n, u32 tag) const
	{
		for (size_t i = 0; i < nodes[n].kids.size(); i++) if (nodes[nodes[n].kids[i]].tag == tag) return nodes[n].kids[i];
		return -1;
	}
	/* the first node with the tag in n's subtree (pre-order, n included) */
	int walk_first(int n, u32 tag) const
	{
		if (nodes[n].tag == tag) return n;
		for (size_t i = 0; i < nodes[n].kids.size(); i++)
		{
			int r = walk_first(nodes[n].kids[i], tag);
			if (r >= 0) return r;
		}
		return -1;
	}
	void walk_all(int n, u32 tag, std::vector<int> &out) const
	{
		if (nodes[n].tag == tag) out.push_back(n);
		for (size_t i = 0; i < nodes[n].kids.size(); i++) walk_all(nodes[n].kids[i], tag, out);
	}
	/* children with the tag of the top-level nodes with tag 'top' */
	void items(u32 top, u32 tag, std::vector<int> &out) const
	{
		for (size_t i = 0; i < roots.size(); i++) if (nodes[roots[i]].tag == top) find(roots[i], tag, out);
	}
	/* __ObjectName of the object a 0f04 child of n refers to */
	std::string name_of(int n) const
	{
		int r = nodes[n].tag == 0x0f04 ? n : first(n, 0x0f04);
		if (r < 0) return std::string();
		u32 i = u(nodes[r].off);
		return i > 0 && i < object_names.size() ? object_names[i] : std::string();
	}
};

static bool parse(Grn &g)
{
	const std::vector<unsigned char> &b = g.b;
	g.ok = false;
	if (b.size() < 0x60 || rd32(&b[0x40]) != 0xca5e0000u) return false;
	u32 nsec = rd32(&b[0x44]);
	if (nsec > 64 || 0x60 + (size_t)nsec * 20 > b.size()) return false;
	int main_first = -1;
	for (u32 s = 0; s < nsec; s++)
	{
		u32 tag = rd32(&b[0x60 + 20 * s]) & 0xffff, off = rd32(&b[0x60 + 20 * s + 8]);
		if ((size_t)off + 16 > b.size()) return false;
		u32 n = rd32(&b[off]);
		if (n > (b.size() - off - 16) / 12) return false;
		size_t first = g.nodes.size();
		bool is_main = tag == 0x0103 && main_first < 0;  /* the first main section (as grnfile.py) */
		std::vector<std::pair<int, size_t> > stack;   /* node, index of its last descendant */
		for (u32 k = 0; k < n; k++)
		{
			const unsigned char *e = &b[off + 16 + 12 * k];
			Node x;
			x.tag = rd32(e) & 0xffff;
			x.off = off + rd32(e + 4);
			x.end = b.size();
			if (x.off > b.size()) return false;
			size_t idx = g.nodes.size();
			if (k) g.nodes[idx - 1].end = x.off;
			while (!stack.empty() && stack.back().second < k) stack.pop_back();
			if (!stack.empty()) g.nodes[stack.back().first].kids.push_back((int)idx);
			else if (is_main) g.roots.push_back((int)idx);
			g.nodes.push_back(x);
			stack.push_back(std::make_pair((int)idx, (size_t)k + rd32(e + 8)));
		}
		for (size_t k = first; k < g.nodes.size(); k++)
			if (g.nodes[k].end < g.nodes[k].off) g.nodes[k].end = g.nodes[k].off;
		if (is_main) main_first = (int)first;
	}
	if (main_first < 0) return false;
	/* 0200: string table { u32 count, u32 bytes, NUL-terminated strings } */
	for (size_t i = 0; i < g.roots.size(); i++)
		if (g.nodes[g.roots[i]].tag == 0x0200)
		{
			size_t p = g.nodes[g.roots[i]].off;
			u32 cnt = g.u(p);
			p += 8;
			for (u32 k = 0; k < cnt && p < b.size(); k++)
			{
				size_t e = p;
				while (e < b.size() && b[e]) e++;
				g.strings.push_back(std::string((const char *)&b[p], e - p));
				p = e + 1;
			}
			break;
		}
	/* 0f03 > 0f00 object (numbered 1..) > 0f05 > 0f01 { u32 key } > ... 0f02 { u32 type (0: string), u32 value } */
	g.object_names.push_back(std::string());
	std::vector<int> objs;
	g.items(0x0f03, 0x0f00, objs);
	for (size_t i = 0; i < objs.size(); i++)
	{
		std::string name;
		std::vector<int> lists;
		g.find(objs[i], 0x0f05, lists);
		for (size_t l = 0; l < lists.size(); l++)
		{
			std::vector<int> props;
			g.find(lists[l], 0x0f01, props);
			for (size_t k = 0; k < props.size(); k++)
			{
				u32 key = g.u(g.nodes[props[k]].off);
				int v = g.walk_first(props[k], 0x0f02);
				if (key < g.strings.size() && g.strings[key] == "__ObjectName" && v >= 0 && g.u(g.nodes[v].off) == 0)
				{
					u32 s = g.u(g.nodes[v].off + 4);
					if (s < g.strings.size()) name = g.strings[s];
				}
			}
		}
		g.object_names.push_back(name);
	}
	g.ok = true;
	return true;
}

/* ------------------------------------------------------------------------------- the readers --- */

struct GMesh                      /* 0602 > 0601 */
{
	std::vector<float> pos, nrm;  /* 0603: 0801 f32[n][3], 0802 f32[m][3] */
	std::vector<std::vector<float> > layers;  /* 0804 > 0803 { u32 dim, f32[k][dim] } (dim 3) */
	bool skinned;
	u32 nskin;                    /* 0702 { u32 nverts, u32 max_binding, u32 max_influences, per vertex
	                                 { u32 n, n x { u32 binding, f32 weight } } } */
	std::vector<u32> skin;
	std::vector<u32> tris;        /* 0901: per triangle { u32 pos[3], u32 normal[3] } */
};

struct GTexMap                    /* 0e01 > 0e00 { u32 material } > 0e07 > 0e02 { u32 mesh, u32 ? }, */
{                                 /*   0e03 > { 0e05 f32 rgb[3], 0e04 { u32 layer } ... },          */
	u32 material;                 /*   0e06 { u32 n, n x { u32 triangle, per corner per layer u32 index } } */
	u32 mesh;
	std::vector<u32> layers;
	std::map<u32, std::vector<u32> > tri_uv;   /* triangle -> 3 x nlayers indices (corner-major) */
};

struct GMaterial { bool has_color; float color[4]; u32 texture; };   /* 0d00: 0d02 f32 rgba, 0d03 { ?, u32 texture, ? } */

static void read_meshes(const Grn &g, std::vector<GMesh> &out)
{
	std::vector<int> ms;
	g.items(0x0602, 0x0601, ms);
	for (size_t i = 0; i < ms.size(); i++)
	{
		GMesh m;
		m.skinned = false;
		m.nskin = 0;
		int v = g.walk_first(ms[i], 0x0603);
		if (v >= 0)
		{
			int pn = g.first(v, 0x0801), nn = g.first(v, 0x0802);
			if (pn >= 0) { size_t n = (g.nodes[pn].end - g.nodes[pn].off) / 12 * 3; for (size_t k = 0; k < n; k++) m.pos.push_back(g.f(g.nodes[pn].off + 4 * k)); }
			if (nn >= 0) { size_t n = (g.nodes[nn].end - g.nodes[nn].off) / 12 * 3; for (size_t k = 0; k < n; k++) m.nrm.push_back(g.f(g.nodes[nn].off + 4 * k)); }
			std::vector<int> uls;
			g.find(v, 0x0804, uls);
			for (size_t l = 0; l < uls.size(); l++)
			{
				std::vector<int> us;
				g.find(uls[l], 0x0803, us);
				for (size_t k = 0; k < us.size(); k++)
				{
					const Node &u = g.nodes[us[k]];
					u32 dim = g.u(u.off);
					size_t cnt = dim ? (u.end - u.off - 4) / (4 * dim) : 0;
					std::vector<float> uv;
					for (size_t j = 0; j < cnt * dim; j++) uv.push_back(g.f(u.off + 4 + 4 * j));
					if (dim != 3) uv.clear();    /* the game's layers are all (u, v, w) */
					m.layers.push_back(uv);
				}
			}
		}
		int sk = g.first(ms[i], 0x0702);
		if (sk >= 0)
		{
			const Node &s = g.nodes[sk];
			m.nskin = g.u(s.off);
			size_t p = s.off + 12;
			for (u32 k = 0; k < m.nskin && p + 4 <= s.end; k++)
			{
				u32 n = g.u(p);
				m.skin.push_back(n);
				p += 4;
				for (u32 j = 0; j < 2 * n; j++, p += 4) m.skin.push_back(g.u(p));
			}
			m.skinned = m.nskin > 0;
		}
		int tr = g.first(ms[i], 0x0901);
		if (tr >= 0)
		{
			size_t n = (g.nodes[tr].end - g.nodes[tr].off) / 24 * 6;
			for (size_t k = 0; k < n; k++) m.tris.push_back(g.u(g.nodes[tr].off + 4 * k));
		}
		out.push_back(m);
	}
}

static void read_texmaps(const Grn &g, std::vector<GTexMap> &out)
{
	std::vector<int> ts;
	g.items(0x0e01, 0x0e00, ts);
	for (size_t i = 0; i < ts.size(); i++)
	{
		GTexMap t;
		t.material = g.own(ts[i]) >= 4 ? g.u(g.nodes[ts[i]].off) : 0;
		int e2 = g.walk_first(ts[i], 0x0e02);
		t.mesh = e2 >= 0 ? g.u(g.nodes[e2].off) : 0;
		std::vector<int> ls;
		g.walk_all(ts[i], 0x0e04, ls);
		for (size_t k = 0; k < ls.size(); k++) t.layers.push_back(g.u(g.nodes[ls[k]].off));
		int e6 = g.walk_first(ts[i], 0x0e06);
		if (e6 >= 0)
		{
			size_t p = g.nodes[e6].off;
			u32 n = g.u(p), stride = 1 + 3 * (u32)t.layers.size();
			p += 4;
			for (u32 k = 0; k < n && p + 4 * stride <= g.nodes[e6].end; k++, p += 4 * stride)
			{
				std::vector<u32> idx;
				for (u32 j = 1; j < stride; j++) idx.push_back(g.u(p + 4 * j));
				t.tri_uv[g.u(p)] = idx;
			}
		}
		out.push_back(t);
	}
}

static void read_materials(const Grn &g, std::vector<GMaterial> &out)
{
	std::vector<int> ms;
	g.items(0x0d01, 0x0d00, ms);
	for (size_t i = 0; i < ms.size(); i++)
	{
		GMaterial m;
		m.has_color = false;
		m.color[0] = m.color[1] = m.color[2] = m.color[3] = 1.0f;
		m.texture = 0;
		int c = g.walk_first(ms[i], 0x0d02);
		if (c >= 0) { m.has_color = true; for (int k = 0; k < 4; k++) m.color[k] = g.f(g.nodes[c].off + 4 * k); }
		int t = g.walk_first(ms[i], 0x0d03);
		if (t >= 0) m.texture = g.u(g.nodes[t].off + 4);
		out.push_back(m);
	}
}

/* 1205 > 1200 > 1203 > 1204 track: { u32 tool node, u32 ?, u32 kind, ... }
     kind 0 (raw keys): { f32 time, pos[3], quat[4], scale_shear[9] } to the end of the node
     else (b-splines): u32 degree[3], u32 nknots[3], u32 knot_array[3], u32 ?, f32 knots[nknots[0..2]],
                       control points of position (3), orientation (4), scale/shear (9) */
static void read_tracks(const Grn &g, const std::vector<std::string> &tool, FileData &f)
{
	std::vector<int> as;
	g.items(0x1205, 0x1200, as);
	if (as.empty()) return;
	f.has_anim = true;
	std::vector<int> lists, ts;
	g.find(as[0], 0x1203, lists);
	for (size_t l = 0; l < lists.size(); l++) g.find(lists[l], 0x1204, ts);
	D dur = 0.0;
	for (size_t i = 0; i < ts.size(); i++)
	{
		const Node &n = g.nodes[ts[i]];
		Track t;
		u32 node = g.u(n.off), kind = g.u(n.off + 8);
		t.name = node >= 1 && node <= tool.size() ? tool[node - 1] : std::string();
		t.nknots[0] = t.nknots[1] = t.nknots[2] = 0;
		if (kind == 0)
		{
			t.kind = 0;
			size_t cnt = n.end > n.off + 12 ? (n.end - n.off - 12) / 68 : 0;
			for (size_t k = 0; k < cnt * 17; k++) t.keys.push_back(g.f(n.off + 12 + 4 * k));
			if (cnt && (D)t.keys[(cnt - 1) * 17] > dur) dur = t.keys[(cnt - 1) * 17];
		}
		else
		{
			t.kind = 1;
			size_t p = n.off + 13 * 4;
			for (int a = 0; a < 3; a++)
			{
				t.nknots[a] = (int)g.u(n.off + 4 * (6 + a));
				for (int k = 0; k < t.nknots[a]; k++, p += 4) t.knots[a].push_back(g.f(p));
			}
			static const int dim[3] = { 3, 4, 9 };
			for (int c = 0; c < 3; c++)
			{
				t.curve[c].degree = (int)g.u(n.off + 4 * (3 + c));
				u32 ka = g.u(n.off + 4 * (9 + c));
				t.curve[c].knots = ka < 3 ? (int)ka : 0;
				int cnt = t.nknots[t.curve[c].knots] * dim[c];
				for (int k = 0; k < cnt; k++, p += 4) t.curve[c].cps.push_back(g.f(p));
			}
			if (t.nknots[0] && (D)t.knots[0].back() > dur) dur = t.knots[0].back();
		}
		f.tracks.push_back(t);
	}
	f.duration = dur;
}

/* --------------------------------------------------------------------------- the bindings --- */

/* a mesh binding (0x7c bytes, built in 0x10023e50 at 0x100247a3..0x1002494e): the bone's composed rest
   transform {q, t, M} copied, then q conjugated, t negated (bt) and M inverted by cofactors in place
   (the determinant 0x10018fd0 and 1/det kept in double; a singular M is left as it is) = mat_inverse */
static void binding_inverse(const World &w, Binding &b)
{
	float inv[9];
	mat_inverse(w.M, inv);
	memcpy(b.B, inv, sizeof inv);
	for (int i = 0; i < 3; i++) b.bt[i] = -w.t[i];
}

/* ------------------------------------------------------------------------------ the meshes --- */

static inline bool feq(float a, float b) { return a == b; }   /* IEEE: NaN never equal, -0 == +0 */

struct Corner { float n[3]; float a[2][3]; int nl; };

static bool same_corner(const Corner &x, const Corner &y)
{
	if (x.nl != y.nl) return false;
	for (int i = 0; i < 3; i++) if (!feq(x.n[i], y.n[i])) return false;
	for (int l = 0; l < x.nl; l++)
		for (int i = 0; i < 3; i++) if (!feq(x.a[l][i], y.a[l][i])) return false;
	return true;
}

/* render vertices: the first nsource are the file's vertices (positions) in order, each taking the normal
   and texture layer values of the first triangle corner that uses it; a later corner of the same position
   with a different normal or layer value (compared as floats) reuses an earlier duplicate with the same
   values or appends a new render vertex.  Triangles in file order, corners in order. */
static void build_mesh(const GMesh &gm, const GTexMap *tm, const float base_color[3], Mesh &m,
                       std::vector<std::string> *warn, const std::string &what)
{
	size_t npos = gm.pos.size() / 3, nnrm = gm.nrm.size() / 3, ntri = gm.tris.size() / 6;
	int nl = tm ? (int)tm->layers.size() : 0;
	if (nl > 2) { if (warn) warn->push_back(what + ": more than 2 texture layers"); nl = 2; }
	std::vector<Corner> rv(npos);
	std::vector<int> rv_pos(npos), claimed(npos, 0);
	std::vector<std::vector<u32> > dups(npos);
	for (size_t i = 0; i < npos; i++) rv_pos[i] = (int)i;
	std::vector<u16> idx;
	size_t uncovered = 0;
	for (size_t t = 0; t < ntri; t++)
	{
		const u32 *tr = &gm.tris[6 * t];
		const std::vector<u32> *uv = 0;
		if (tm && nl)
		{
			std::map<u32, std::vector<u32> >::const_iterator it = tm->tri_uv.find((u32)t);
			if (it != tm->tri_uv.end()) uv = &it->second;
			else uncovered++;
		}
		for (int c = 0; c < 3; c++)
		{
			u32 p = tr[c], n = tr[3 + c];
			Corner k;
			memset(&k, 0, sizeof k);
			k.nl = nl;
			if (n < nnrm) memcpy(k.n, &gm.nrm[3 * n], 12);
			for (int l = 0; l < nl && uv; l++)
			{
				u32 layer = tm->layers[l], ui = (*uv)[c * nl + l];
				if (layer < gm.layers.size() && (size_t)(ui + 1) * 3 <= gm.layers[layer].size())
					memcpy(k.a[l], &gm.layers[layer][3 * ui], 12);
			}
			if (p >= npos) { idx.push_back(0); continue; }
			u32 r;
			if (!claimed[p]) { claimed[p] = 1; rv[p] = k; r = p; }
			else if (same_corner(rv[p], k)) r = p;
			else
			{
				r = 0xffffffffu;
				for (size_t d = 0; d < dups[p].size(); d++) if (same_corner(rv[dups[p][d]], k)) { r = dups[p][d]; break; }
				if (r == 0xffffffffu)
				{
					r = (u32)rv.size();
					rv.push_back(k);
					rv_pos.push_back((int)p);
					claimed.push_back(1);
					dups[p].push_back(r);
				}
			}
			idx.push_back((u16)r);
		}
	}
	if (uncovered && warn) warn->push_back(what + ": triangles without texture coordinates");
	if (rv.size() > 0xffff && warn) warn->push_back(what + ": more than 65535 render vertices");
	size_t n = rv.size();
	m.nrender = (u32)n;
	m.pos.assign(3 * n, 0.0f); m.nrm.assign(3 * n, 0.0f); m.uv.assign(3 * n, 0.0f); m.col.assign(3 * n, 0.0f);
	for (size_t i = 0; i < n; i++)
	{
		if (!claimed[i]) continue;          /* a position no triangle uses: all zero */
		memcpy(&m.pos[3 * i], &gm.pos[3 * rv_pos[i]], 12);
		memcpy(&m.nrm[3 * i], rv[i].n, 12);
		if (nl) memcpy(&m.uv[3 * i], rv[i].a[nl - 1], 12);
		if (nl >= 2) memcpy(&m.col[3 * i], rv[i].a[0], 12);
		else memcpy(&m.col[3 * i], base_color, 12);
	}
	m.nsource = gm.skinned ? gm.nskin : 0;
	m.weights = gm.skinned ? gm.skin : std::vector<u32>();
	m.copies.clear();
	m.normal_vertex.clear();
	if (m.nsource)
	{
		for (u32 v = 0; v < m.nsource; v++)
		{
			m.copies.push_back(v < npos ? (u32)dups[v].size() : 0);
			if (v < npos) for (size_t d = 0; d < dups[v].size(); d++) m.copies.push_back(dups[v][d] - v);
		}
		for (size_t i = 0; i < n; i++) m.normal_vertex.push_back((u32)rv_pos[i] < m.nsource ? (u32)rv_pos[i] : 0xffffffffu);
	}
	m.wofs.resize(m.nsource + 1);
	{
		u32 p = 0;
		for (u32 v = 0; v < m.nsource; v++) { m.wofs[v] = p; p += 1 + 2 * (p < m.weights.size() ? m.weights[p] : 0); }
		m.wofs[m.nsource] = p;
	}
	MeshState st;
	st.texture = 0xffffffffu;
	st.src_blend = 1;
	st.dst_blend = 0;
	st.tris = idx;
	m.states.push_back(st);
}

} /* namespace */

/* ------------------------------------------------------------------------------------- API --- */

std::string grn_find_file(const std::string &path)
{
	if (path.empty() || exists(path)) return path;
	/* resolve each component without regard to case */
	std::string out;
	size_t i = 0;
	if (path[0] == '/') { out = "/"; i = 1; }
	while (i <= path.size())
	{
		size_t e = path.find('/', i);
		if (e == std::string::npos) e = path.size();
		std::string comp = path.substr(i, e - i);
		i = e + 1;
		if (comp.empty()) continue;
		std::string dir = out.empty() ? std::string(".") : out;
		std::string pick = comp;
		if (comp != "." && comp != "..")
		{
			DIR *d = opendir(dir.c_str());
			if (d)
			{
				struct dirent *de;
				bool exact = false;
				std::string found;
				while ((de = readdir(d)) != 0)
				{
					if (!strcmp(de->d_name, comp.c_str())) { exact = true; break; }
					if (found.empty() && same_nocase(de->d_name, comp.c_str())) found = de->d_name;
				}
				closedir(d);
				if (!exact && !found.empty()) pick = found;
			}
		}
		if (!out.empty() && out[out.size() - 1] != '/') out += "/";
		out += pick;
	}
	return out;
}

void grn_set_dll_path(const char *path)
{
	g_dll_path = path ? path : "";
	g_dll = DllData();
}

const BinkTables *grn_bink_tables(const std::string &near, std::string *why)
{
	if (!g_dll.tried) load_dll_tables(near);
	if (!g_dll.tab.ok) { if (why) *why = g_dll.why; return 0; }
	return &g_dll.tab;
}

FileData *grn_load(const std::string &path, granny_result *err, std::vector<std::string> *warn)
{
	Grn g;
	if (!read_file(path, g.b)) { *err = GrannyUnableToReadFromFile; return 0; }
	if (!parse(g)) { *err = GrannyUnableToParseFile; return 0; }
	FileData *f = new FileData;
	f->path = path;
	f->refs = 0;
	f->has_model = f->has_anim = false;
	f->duration = 0.0;

	/* 0b01 > 0b00 tool nodes (bone and track names) */
	std::vector<std::string> tool;
	{
		std::vector<int> tn;
		g.items(0x0b01, 0x0b00, tn);
		for (size_t i = 0; i < tn.size(); i++) tool.push_back(g.name_of(tn[i]));
	}

	/* 0c01 > 0c00 model > 0c07 > 0c08 { u32 skeleton } > 0c02 { u32 tool node per bone };
	                     0c06 > 0c03 { u32 mesh } > 0c09 > 0c0a { u32 bone, f32 bounds[7] } */
	std::vector<int> models;
	g.items(0x0c01, 0x0c00, models);
	u32 skel = 0;
	std::vector<u32> bone_nodes;
	std::vector<std::pair<u32, std::vector<u32> > > model_meshes;
	if (!models.empty())
	{
		int mo = models[0];
		std::vector<int> c7;
		g.find(mo, 0x0c07, c7);
		for (size_t i = 0; i < c7.size(); i++)
		{
			int c8 = g.walk_first(c7[i], 0x0c08);
			if (c8 >= 0 && g.own(c8) >= 4) skel = g.u(g.nodes[c8].off);
			int c2 = g.walk_first(c7[i], 0x0c02);
			if (c2 >= 0) { bone_nodes.clear(); for (size_t k = 0; k + 4 <= g.own(c2); k += 4) bone_nodes.push_back(g.u(g.nodes[c2].off + k)); }
		}
		std::vector<int> c6;
		g.find(mo, 0x0c06, c6);
		for (size_t i = 0; i < c6.size(); i++)
		{
			std::vector<int> c3;
			g.find(c6[i], 0x0c03, c3);
			for (size_t k = 0; k < c3.size(); k++)
			{
				std::vector<u32> bones;
				std::vector<int> bs;
				g.walk_all(c3[k], 0x0c0a, bs);
				for (size_t j = 0; j < bs.size(); j++) bones.push_back(g.u(g.nodes[bs[j]].off));
				model_meshes.push_back(std::make_pair(g.u(g.nodes[c3[k]].off), bones));
			}
		}
	}

	/* 0507 > 0505 skeleton > 0508 > 0506 bone { u32 parent, f32 pos[3], f32 quat[4] (x y z w), f32 ss[9] } */
	std::vector<int> skels;
	g.items(0x0507, 0x0505, skels);
	if (skel >= 1 && skel <= skels.size())
	{
		f->has_model = true;
		std::vector<int> lists, bs;
		g.find(skels[skel - 1], 0x0508, lists);
		for (size_t l = 0; l < lists.size(); l++) g.find(lists[l], 0x0506, bs);
		for (size_t i = 0; i < bs.size(); i++)
		{
			size_t o = g.nodes[bs[i]].off;
			BoneDef b;
			u32 tn = i < bone_nodes.size() ? bone_nodes[i] : 0;
			b.name = tn >= 1 && tn <= tool.size() ? tool[tn - 1] : std::string();
			b.parent = i ? (int)g.u(o) : 0;
			for (int k = 0; k < 3; k++) b.rest.pos[k] = g.f(o + 4 + 4 * k);
			for (int k = 0; k < 4; k++) b.rest.q[k] = g.f(o + 16 + 4 * k);
			for (int k = 0; k < 9; k++) b.rest.ss[k] = g.f(o + 32 + 4 * k);
			memset(&b.offset, 0, sizeof b.offset);
			f->bones.push_back(b);
		}
	}

	if (f->has_model)
	{
		std::vector<GMaterial> mats;
		read_materials(g, mats);
		std::vector<GTexMap> maps;
		read_texmaps(g, maps);
		std::vector<int> texs;
		g.items(0x0304, 0x0301, texs);

		/* 0e00 texture maps: each is one "new texture" of the model; its image is the texture of its
		   material (0d03, 1-based) - 0301 > 0305 > 0303 { u32 w, h, format, Bink file }, decoded to RGB24 */
		const BinkTables *bt = 0;
		for (size_t i = 0; i < maps.size(); i++)
		{
			if (maps[i].material < 1 || maps[i].material > mats.size()) continue;   /* no such material: not queued */
			TextureData td;
			td.w = td.h = td.format = td.bpp = td.pitch = 0;
			const GMaterial *mat = maps[i].material >= 1 && maps[i].material <= mats.size() ? &mats[maps[i].material - 1] : 0;
			u32 ti = mat ? mat->texture : 0;
			if (ti >= 1 && ti <= texs.size())
			{
				int d = g.walk_first(texs[ti - 1], 0x0303);
				if (d >= 0 && g.nodes[d].end >= g.nodes[d].off + 12)
				{
					const Node &n = g.nodes[d];
					std::string why;
					if (!bt) bt = grn_bink_tables(path, &why);
					u32 w = 0, h = 0;
					if (!bt)
					{
						if (warn) warn->push_back(why);
						static bool told = false;
						if (!told) { fprintf(stderr, "granny_compat: %s: textures are left out\n", why.c_str()); told = true; }
					}
					else if (bink_decode_rgb24(&g.b[n.off + 12], n.end - n.off - 12, *bt, &w, &h, td.pixels))
					{
						td.w = w; td.h = h; td.format = 4; td.bpp = 3; td.pitch = 3 * w;
					}
					else if (warn) warn->push_back(path + ": texture not decoded");
				}
			}
			f->textures.push_back(td);
		}

		std::vector<GMesh> gms;
		read_meshes(g, gms);
		float base_color[3] = { 1.0f, 1.0f, 1.0f };
		if (!maps.empty() && maps[0].material >= 1 && maps[0].material <= mats.size() && mats[maps[0].material - 1].has_color)
			memcpy(base_color, mats[maps[0].material - 1].color, 12);

		/* bind pose: composed rest transforms */
		std::vector<World> rest(f->bones.size());
		for (size_t i = 0; i < f->bones.size(); i++)
		{
			local_to_world(f->bones[i].rest, rest[i]);
			if (i) world_compose(rest[i], rest[f->bones[i].parent]);
		}
		for (size_t k = 0; k < model_meshes.size(); k++)
		{
			u32 mi = model_meshes[k].first;
			Mesh m;
			if (mi < 1 || mi > gms.size()) { if (warn) warn->push_back(path + ": bad mesh reference"); continue; }
			const GTexMap *tm = 0;
			for (size_t i = 0; i < maps.size(); i++) if (maps[i].mesh == mi - 1) { tm = &maps[i]; break; }
			build_mesh(gms[mi - 1], tm, base_color, m, warn, path);
			const std::vector<u32> &bones = model_meshes[k].second;
			for (size_t j = 0; j < bones.size(); j++)
			{
				Binding b;
				b.bone = bones[j];
				if (b.bone < rest.size()) binding_inverse(rest[b.bone], b);
				else { memset(b.bt, 0, sizeof b.bt); memset(b.B, 0, sizeof b.B); }
				m.bindings.push_back(b);
			}
			if (!maps.empty())
			{
				m.states[0].texture = 0;
				if (f->textures[0].pixels.empty()) { m.states[0].src_blend = 6; m.states[0].dst_blend = 7; }
			}
			f->meshes.push_back(m);
		}
	}
	read_tracks(g, tool, *f);
	return f;
}

}  /* namespace granny_internal */
