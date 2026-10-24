/*  grncheck - compares what game/granny/granny_grn.cpp builds from the .grn files with what granny.dll builds
    from them, as dumped by grnextract.exe under Wine (a .gxtr file): decoded textures, the bind-pose rendering
    states (texture, blend modes, triangle lists) and the internal mesh objects (render-order vertex arrays,
    bone bindings with their inverse rest transforms, weight lists, copy lists, normal-to-vertex mapping),
    all bit for bit.

      grncheck <dump.gxtr> <grn dir> [granny.dll]          (exit status 1 on any difference)

    Build: g++ -O2 -ffp-contract=off -Igame/granny tools/granny/grncheck.cpp game/granny/granny_grn.cpp
           game/granny/granny_bink.cpp -o grncheck                                                      */
#include "granny_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace granny_internal;

struct R
{
	const std::vector<unsigned char> &b;
	size_t p;
	bool ok;
	R(const std::vector<unsigned char> &v) : b(v), p(0), ok(true) {}
	u32 u() { if (p + 4 > b.size()) { ok = false; return 0; } u32 v = rd32(&b[p]); p += 4; return v; }
	std::vector<unsigned char> raw(size_t n)
	{
		if (p + n > b.size()) { ok = false; n = b.size() - p; }
		std::vector<unsigned char> v(b.begin() + (long)p, b.begin() + (long)(p + n));
		p += n;
		return v;
	}
};

struct XTex { u32 w, h, format, bpp, pitch; std::vector<unsigned char> pix; };
struct XState { u32 hdr[8]; u32 mesh, index; u32 st[6][5]; std::vector<unsigned char> data[6]; };
struct XMesh
{
	std::vector<std::vector<unsigned char> > bt, B;
	u32 nr;
	std::vector<unsigned char> pos, nrm, uv, col;
	u32 nsource;
	std::vector<unsigned char> weights, copies, nv;
	std::vector<std::vector<unsigned char> > lists;
};
struct XFile { std::string name; u32 ok; std::vector<XTex> tex; std::vector<XState> states; std::vector<XMesh> meshes; };

static bool read_gxtr(const char *path, std::vector<XFile> &out)
{
	std::vector<unsigned char> b;
	FILE *fp = fopen(path, "rb");
	if (!fp) return false;
	unsigned char tmp[65536];
	size_t n;
	while ((n = fread(tmp, 1, sizeof tmp, fp)) > 0) b.insert(b.end(), tmp, tmp + n);
	fclose(fp);
	R r(b);
	while (r.ok && r.p < b.size())
	{
		std::vector<unsigned char> tag = r.raw(4);
		if (!memcmp(&tag[0], "GXOF", 4))
		{
			r.raw(r.u()); r.raw(r.u());
			if (r.u()) { u32 k = r.u(); for (u32 i = 0; i < k; i++) { r.u(); r.raw(64); } }
			continue;
		}
		if (memcmp(&tag[0], "GXTR", 4)) return false;
		r.u();
		XFile f;
		std::vector<unsigned char> nm = r.raw(r.u());
		f.name.assign(nm.begin(), nm.end());
		f.ok = r.u();
		if (f.ok)
		{
			u32 nt = r.u();
			for (u32 i = 0; i < nt; i++)
			{
				XTex t;
				t.w = r.u(); t.h = r.u(); t.format = r.u(); t.bpp = r.u(); t.pitch = r.u();
				t.pix = r.raw(r.u());
				f.tex.push_back(t);
			}
			u32 ns = r.u();
			for (u32 i = 0; i < ns; i++)
			{
				XState s;
				for (int k = 0; k < 8; k++) s.hdr[k] = r.u();
				r.raw(64);
				s.mesh = r.u(); s.index = r.u();
				for (int k = 0; k < 6; k++) for (int j = 0; j < 5; j++) s.st[k][j] = r.u();
				static const int ks[4] = { 0, 3, 4, 5 };
				for (int j = 0; j < 4; j++)
				{
					int k = ks[j];
					if (s.st[k][0]) s.data[k] = r.raw(s.st[k][3] * 3 * (k == 5 ? 2 : 4));
				}
				f.states.push_back(s);
			}
			u32 nm2 = r.u();
			for (u32 i = 0; i < nm2; i++)
			{
				XMesh m;
				u32 nb = r.u();
				for (u32 k = 0; k < nb; k++) { m.bt.push_back(r.raw(12)); m.B.push_back(r.raw(36)); r.raw(0x7c); }
				m.nr = r.u();
				m.pos = r.raw(12 * m.nr); m.nrm = r.raw(12 * m.nr); m.uv = r.raw(12 * m.nr); m.col = r.raw(12 * m.nr);
				m.nsource = r.u();
				m.weights = r.raw(4 * r.u());
				m.copies = r.raw(4 * r.u());
				m.nv = r.raw(4 * r.u());
				u32 nl = r.u();
				for (u32 k = 0; k < nl; k++) m.lists.push_back(r.raw(2 * r.u()));
				f.meshes.push_back(m);
			}
		}
		out.push_back(f);
	}
	return r.ok;
}

static std::vector<unsigned char> bytes_f(const float *p, size_t n)
{
	std::vector<unsigned char> v(4 * n);
	for (size_t i = 0; i < n; i++)
	{
		union { float f; u32 u; } x;
		x.f = p[i];
		v[4 * i] = (unsigned char)x.u; v[4 * i + 1] = (unsigned char)(x.u >> 8);
		v[4 * i + 2] = (unsigned char)(x.u >> 16); v[4 * i + 3] = (unsigned char)(x.u >> 24);
	}
	return v;
}
static std::vector<unsigned char> bytes_u(const u32 *p, size_t n) { return bytes_f((const float *)(const void *)p, n); }
static std::vector<unsigned char> bytes_h(const u16 *p, size_t n)
{
	std::vector<unsigned char> v(2 * n);
	for (size_t i = 0; i < n; i++) { v[2 * i] = (unsigned char)p[i]; v[2 * i + 1] = (unsigned char)(p[i] >> 8); }
	return v;
}

static int g_bad, g_items;
static std::string g_cur;

static void diff(const char *what, const std::vector<unsigned char> &a, const std::vector<unsigned char> &b, int elem)
{
	g_items++;
	if (a == b) return;
	g_bad++;
	size_t i = 0;
	while (i < a.size() && i < b.size() && a[i] == b[i]) i++;
	printf("%s: %s differs (%u vs %u bytes, first at element %u)\n", g_cur.c_str(), what, (unsigned)a.size(),
	       (unsigned)b.size(), (unsigned)(i / elem));
}

static void same(const char *what, u32 a, u32 b)
{
	g_items++;
	if (a == b) return;
	g_bad++;
	printf("%s: %s: %u (theirs) vs %u (ours)\n", g_cur.c_str(), what, a, b);
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: grncheck dump.gxtr grn_dir [granny.dll]\n"); return 2; }
	if (argc > 3) grn_set_dll_path(argv[3]);
	std::vector<XFile> files;
	if (!read_gxtr(argv[1], files)) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
	int nfiles = 0, nbadfiles = 0, ntex = 0, nmesh = 0;
	for (size_t fi = 0; fi < files.size(); fi++)
	{
		const XFile &x = files[fi];
		g_cur = x.name;
		int bad0 = g_bad;
		std::vector<std::string> warn;
		granny_result err = GrannyOK;
		FileData *f = grn_load(std::string(argv[2]) + "/" + x.name, &err, &warn);
		nfiles++;
		for (size_t i = 0; i < warn.size(); i++) printf("%s: warning: %s\n", x.name.c_str(), warn[i].c_str());
		if (!f) { printf("%s: not loaded (error %d)\n", x.name.c_str(), err); g_bad++; nbadfiles++; continue; }
		same("model opens", x.ok, f->has_model ? 1 : 0);
		if (x.ok && f->has_model)
		{
			same("textures", (u32)x.tex.size(), (u32)f->textures.size());
			for (size_t i = 0; i < x.tex.size() && i < f->textures.size(); i++)
			{
				const XTex &a = x.tex[i];
				const TextureData &b = f->textures[i];
				ntex++;
				if (a.pix.empty())     /* no image: the DLL leaves width/height uninitialised */
				{
					same("texture format", a.format, b.format);
					same("texture size", 0, (u32)b.pixels.size());
					continue;
				}
				same("texture width", a.w, b.w); same("texture height", a.h, b.h); same("texture format", a.format, b.format);
				same("texture bpp", a.bpp, b.bpp); same("texture pitch", a.pitch, b.pitch);
				diff("texture pixels", a.pix, b.pixels, 3);
			}
			same("meshes", (u32)x.meshes.size(), (u32)f->meshes.size());
			for (size_t mi = 0; mi < x.meshes.size() && mi < f->meshes.size(); mi++)
			{
				const XMesh &a = x.meshes[mi];
				const Mesh &b = f->meshes[mi];
				nmesh++;
				char what[64];
				same("bindings", (u32)a.bt.size(), (u32)b.bindings.size());
				for (size_t k = 0; k < a.bt.size() && k < b.bindings.size(); k++)
				{
					snprintf(what, sizeof what, "mesh %u binding %u bt", (unsigned)mi, (unsigned)k);
					diff(what, a.bt[k], bytes_f(b.bindings[k].bt, 3), 4);
					snprintf(what, sizeof what, "mesh %u binding %u B", (unsigned)mi, (unsigned)k);
					diff(what, a.B[k], bytes_f(b.bindings[k].B, 9), 4);
				}
				same("render vertices", a.nr, b.nrender);
				if (a.nr == b.nrender)
				{
					diff("positions", a.pos, bytes_f(&b.pos[0], b.pos.size()), 12);
					diff("normals", a.nrm, bytes_f(&b.nrm[0], b.nrm.size()), 12);
					diff("texcoords", a.uv, bytes_f(&b.uv[0], b.uv.size()), 12);
					diff("colours", a.col, bytes_f(&b.col[0], b.col.size()), 12);
				}
				same("source vertices", a.nsource, b.nsource);
				diff("weights", a.weights, bytes_u(b.weights.empty() ? 0 : &b.weights[0], b.weights.size()), 4);
				diff("copies", a.copies, bytes_u(b.copies.empty() ? 0 : &b.copies[0], b.copies.size()), 4);
				diff("normal vertices", a.nv, bytes_u(b.normal_vertex.empty() ? 0 : &b.normal_vertex[0], b.normal_vertex.size()), 4);
				same("state lists", (u32)a.lists.size(), (u32)b.states.size());
				for (size_t s = 0; s < a.lists.size() && s < b.states.size(); s++)
					diff("triangles", a.lists[s], bytes_h(b.states[s].tris.empty() ? 0 : &b.states[s].tris[0], b.states[s].tris.size()), 6);
			}
			/* rendering states in lock order: mesh by mesh */
			size_t si = 0;
			for (size_t mi = 0; mi < f->meshes.size(); mi++)
				for (size_t k = 0; k < f->meshes[mi].states.size(); k++, si++)
				{
					if (si >= x.states.size()) { same("rendering states", (u32)x.states.size(), (u32)si + 1); break; }
					const XState &a = x.states[si];
					const MeshState &b = f->meshes[mi].states[k];
					same("state mesh", a.mesh, (u32)mi);
					same("state texture", a.hdr[2] ? a.hdr[2] - 1 : 0xffffffffu, b.texture);
					same("state src blend", a.hdr[6], b.src_blend);
					same("state dst blend", a.hdr[7], b.dst_blend);
				}
		}
		if (g_bad != bad0) nbadfiles++;
		delete f;
	}
	printf("%d files (%d textures, %d meshes): %d compared items, %d differences, %d files with differences\n",
	       nfiles, ntex, nmesh, g_items, g_bad, nbadfiles);
	return g_bad ? 1 : 0;
}
