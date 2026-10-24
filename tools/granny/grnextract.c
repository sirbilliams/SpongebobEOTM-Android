/*  grnextract - pull what granny.dll builds from a model .grn: decoded textures, the rendering states of the
    bind pose, and granny's internal per-mesh skinning data (render-order vertex arrays, bone-binding inverse
    transforms, per-vertex weight lists), losslessly (raw float bits).  Win32 tool, runs under Wine:

      i686-w64-mingw32-gcc -O2 -o grnextract.exe grnextract.c
      grnextract.exe out.gxtr model.grn [model2.grn ...]          (one output record per input file)

    The internal mesh object of the state just locked is reached from granny's version object V (node of the
    global handle tree, *(*(0x100721fc) + 0x10)): mesh = *((V[0x194] & ~3) + 0x0c), checked against the
    rendering state's texcoord / colour pointers (mesh +0x2c / +0x34).  Its layout (granny.dll 1.2b, see
    docs/granny-replacement.md):
      +0x0c bindings   +0x14 binding[] (0x7c bytes: +0x14 f32 bt[3], +0x20 f32 B[9])
      +0x18/+0x1c render positions   +0x20/+0x24 render normals   +0x28/+0x2c texcoords   +0x30/+0x34 colours
      +0x40/+0x48 per-state index lists {u32 count, u16 *}
      +0x4c source vertices  +0x50/+0x54 weight list {n, n x {binding, f32 w}}   +0x5c copy list
      +0x60/+0x64 per render-normal pointer into the weight list
    Output (little-endian), per file:  "GXTR" u32 version(1) u32 namelen name u32 ok(0 = OpenModel failed)
      u32 ntextures { w h format bpp pitch size pixels }
      u32 nstates { u32 hdr[8] (present, no_texture, cookie, has_transform, deformed, mesh_cookie, src_blend, dst_blend)
                    f32 transform[16], u32 mesh (index into the mesh list below), u32 state_in_mesh,
                    6 x { present type dim count unknown } , pos/col/uv/nrm data (count*3 f32 each, if present),
                    tri data (count*3 u16) }
      u32 nmeshes { u32 nbindings { f32 bt[3] f32 B[9] u32 raw[31] }, u32 nrender, f32 pos/nrm/uv/col [nrender*3] each,
                    u32 nsource, u32 nweightwords, u32 weights[...], u32 copylist_words, u32 copylist[...],
                    u32 nnormals, u32 normal_vertex[nnormals] (source vertex whose weights a render normal uses, or ~0),
                    u32 nstatelists { u32 count, u16 idx[count] } }                                          */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int u32;
#define API __stdcall
typedef int (API *tOV)(const char *, const char *, const char *, const char *, u32 *);
typedef int (API *tAF)(u32, u32);
typedef int (API *tOM)(u32, const char *, u32 *);
typedef int (API *tOS)(u32, u32, u32 *);
typedef int (API *tLS)(u32, u32, u32, u32 *);
typedef int (API *tGR)(u32, u32, u32 *);
typedef int (API *tLN)(u32, u32, u32 *);
typedef int (API *tUS)(u32 *);
typedef int (API *tUR)(u32, u32);
typedef int (API *tNT)(u32, u32 *);
typedef int (API *tUT)(u32 *, u32);
typedef int (API *tC2)(u32, u32);
static HMODULE d;
static void *P(const char *n) { void *p = GetProcAddress(d, n); if (!p) { fprintf(stderr, "missing %s\n", n); exit(2); } return p; }
static FILE *out;
static void w32(u32 v) { fwrite(&v, 4, 1, out); }
static void wraw(const void *p, u32 bytes) { if (bytes) fwrite(p, 1, bytes, out); }

/* the mesh object of the rendering state just locked */
static int mesh_matches(const u32 *me, u32 uvptr, u32 colptr, u32 count)
{
	return me && !IsBadReadPtr(me, 0x68) && me[0x28 / 4] == count && me[0x2c / 4] == uvptr && me[0x30 / 4] == count &&
	       me[0x34 / 4] == colptr;
}

static u32 *current_mesh(u32 uvptr, u32 colptr, u32 count)
{
	u32 root = *(u32 *)0x100721fc, V, X;
	if (!root || IsBadReadPtr((void *)root, 0x14)) return 0;
	V = *(u32 *)(root + 0x10);
	if (!V || IsBadReadPtr((void *)V, 0x198)) return 0;
	X = *(u32 *)(V + 0x194) & ~3u;
	if (!X || IsBadReadPtr((void *)X, 0x10)) return 0;
	return mesh_matches((u32 *)*(u32 *)(X + 0x0c), uvptr, colptr, count) ? (u32 *)*(u32 *)(X + 0x0c) : 0;
}

/* fallback: scan memory for the dword pattern {count, uv*, count, col*} at +0x28 (can also hit a stale copy
   of an earlier, identical mesh) */
static u32 *find_mesh(u32 uvptr, u32 colptr, u32 count, u32 *skip)
{
	MEMORY_BASIC_INFORMATION mi;
	unsigned char *a = (unsigned char *)0x10000;
	while (VirtualQuery(a, &mi, sizeof mi))
	{
		if (mi.State == MEM_COMMIT && (mi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) && !(mi.Protect & PAGE_GUARD))
		{
			u32 *p = (u32 *)mi.BaseAddress, *e = (u32 *)((char *)mi.BaseAddress + mi.RegionSize) - 4;
			for (; p < e; p++)
				if (p[1] == uvptr && p[3] == colptr && p[0] == count && p[2] == count && p != skip) return p - 0x28 / 4;
		}
		a = (unsigned char *)mi.BaseAddress + mi.RegionSize;
		if ((u32)a >= 0x7ffe0000) break;
	}
	return 0;
}

#define MAXMESH 32
static u32 *meshes[MAXMESH];
static int nmeshes;

static void dump_file(u32 ver, const char *path)
{
	u32 m[2], s[2], l[3] = { 0, 0, 0 }, n = 0, i, k;
	const char *nm = strrchr(path, '\\'); nm = nm ? nm + 1 : path;
	if (getenv("GRNEXTRACT_VERBOSE")) { fprintf(stderr, "file %s\n", nm); fflush(stderr); }
	fwrite("GXTR", 1, 4, out); w32(1); w32((u32)strlen(nm)); wraw(nm, (u32)strlen(nm));
	if (((tOM)P("_GrannyOpenModel@12"))(ver, path, m)) { w32(0); fprintf(stderr, "%s: OpenModel failed\n", nm); return; }
	w32(1);
	/* textures: cookie = index + 1 */
	((tNT)P("_GrannyGetNewTexturesLeft@8"))(ver, &n);
	w32(n);
	for (i = 0; i < n; i++)
	{
		u32 t[16];
		memset(t, 0, sizeof t);
		((tNT)P("_GrannyLockNextNewTexture@8"))(ver, t);
		w32(t[5]); w32(t[6]); w32(t[4]); w32(t[7]); w32(t[8]); w32(t[9]);
		wraw((void *)t[10], t[9]);
		((tUT)P("_GrannyUnlockNewTexture@8"))(t, i + 1);
	}
	((tOS)P("_GrannyOpenSequence@12"))(m[0], m[1], s);
	((tLS)P("_GrannyLockSequenceForRendering@16"))(s[0], s[1], 0x3f, l);
	((tGR)P("_GrannyGetRenderingStatesLeft@12"))(l[0], l[1], &n);
	w32(n);
	nmeshes = 0;
	{
		u32 *mesh_of_state[256];
		for (i = 0; i < n; i++)
		{
			u32 st[0xcc / 4 + 8];
			int mi = -1;
			memset(st, 0, sizeof st);
			((tLN)P("_GrannyLockNextRenderingState@12"))(l[0], l[1], st);
			{
				u32 *q = st + 0x74 / 4, *c = st + 0x44 / 4;
				u32 *mesh = current_mesh(q[5], c[5], q[3]);
				if (!mesh)
				{
					mesh = find_mesh(q[5], c[5], q[3], 0);
					fprintf(stderr, "%s: state %u: mesh object found by memory scan only\n", nm, i);
				}
				for (k = 0; k < (u32)nmeshes; k++) if (meshes[k] == mesh) mi = (int)k;
				if (mi < 0 && mesh && nmeshes < MAXMESH) { meshes[nmeshes] = mesh; mi = nmeshes++; }
				if (!mesh) fprintf(stderr, "%s: state %u: mesh object not found\n", nm, i);
				mesh_of_state[i] = mesh;
			}
			w32(st[4]); w32(st[5]); w32(st[6]); w32(st[7]); w32(st[9]); w32(st[10]); w32(st[0x2f]); w32(st[0x30]);
			if (st[7] && st[8]) wraw((void *)st[8], 64); else { float z[16]; memset(z, 0, sizeof z); wraw(z, 64); }
			w32((u32)mi);
			{	/* which state of its mesh */
				u32 j, c = 0;
				for (j = 0; j < i; j++) if (mesh_of_state[j] == mesh_of_state[i]) c++;
				w32(c);
			}
			for (k = 0; k < 6; k++)
			{
				u32 *q = st + (0x2c + 0x18 * k) / 4;
				w32(q[0]); w32(q[1]); w32(q[2]); w32(q[3]); w32(q[4]);
			}
			for (k = 0; k < 6; k++)
			{
				u32 *q = st + (0x2c + 0x18 * k) / 4;
				if (k == 1 || k == 2) continue;           /* colours are dumped with the mesh */
				if (q[0] && q[5]) wraw((void *)q[5], q[3] * 3 * (k == 5 ? 2 : 4));
			}
			((tUS)P("_GrannyUnlockRenderingState@4"))(st);
		}
	}
	((tUR)P("_GrannyUnlockRendering@8"))(l[0], l[1]);
	w32((u32)nmeshes);
	for (i = 0; i < (u32)nmeshes; i++)
	{
		u32 *me = meshes[i], nb = me[0x0c / 4], *b = (u32 *)me[0x14 / 4];
		u32 nr = me[0x18 / 4];
		w32(nb);
		for (k = 0; k < nb; k++)
		{
			u32 *bb = b + k * 0x7c / 4;
			wraw(bb + 0x14 / 4, 12); wraw(bb + 0x20 / 4, 36); wraw(bb, 0x7c);
		}
		w32(nr);
		wraw((void *)me[0x1c / 4], nr * 12);
		if (me[0x20 / 4] != nr || me[0x28 / 4] != nr || me[0x30 / 4] != nr)
			fprintf(stderr, "%s: mesh %u: array sizes differ (%u %u %u %u)\n", nm, i, nr, me[0x20 / 4], me[0x28 / 4], me[0x30 / 4]);
		wraw((void *)me[0x24 / 4], nr * 12);
		wraw((void *)me[0x2c / 4], nr * 12);
		wraw((void *)me[0x34 / 4], nr * 12);
		{
			u32 ns = me[0x4c / 4], nw = me[0x50 / 4], *wl = (u32 *)me[0x54 / 4];
			u32 *cl = (u32 *)me[0x5c / 4], nn = me[0x60 / 4], **np = (u32 **)me[0x64 / 4];
			u32 j, cw = 0, *vofs;
			w32(ns); w32(nw); if (nw) wraw(wl, nw * 4);
			/* copy list: per source vertex {count, offsets...} */
			if (cl && ns) { u32 p = 0; for (j = 0; j < ns; j++) { p += 1 + cl[p]; } cw = p; }
			w32(cw); if (cw) wraw(cl, cw * 4);
			/* normal -> source vertex */
			vofs = (u32 *)malloc((ns + 1) * 4);
			{ u32 p = 0; for (j = 0; j < ns && wl; j++) { vofs[j] = p; p += 1 + 2 * wl[p]; } vofs[ns] = p; }
			w32(np ? nn : 0);
			for (j = 0; np && j < nn; j++)
			{
				u32 off = (u32)(np[j] - wl), v = ~0u, a;
				for (a = 0; a < ns; a++) if (vofs[a] == off) { v = a; break; }
				w32(v);
			}
			free(vofs);
		}
		{
			u32 nsl = me[0x40 / 4], *sl = (u32 *)me[0x48 / 4];
			w32(nsl);
			for (k = 0; k < nsl; k++) { w32(sl[2 * k]); wraw((void *)sl[2 * k + 1], sl[2 * k] * 2); }
		}
	}
	((tC2)P("_GrannyCloseSequence@8"))(s[0], s[1]);
	((tC2)P("_GrannyCloseModel@8"))(m[0], m[1]);
}

/* per-track offset transforms granny computes when it binds an animation to a model's skeleton
   (rst_affector, vtable 0x1005eb04; binding table at +0x6c: u32 n, f32 offset[n][16], ?, bone *[n])
   output: "GXOF" u32 namelen model, u32 namelen anim, u32 ok, u32 ntracks { u32 has_bone, f32 offset[16] } */
typedef int (API *tAC)(u32, u32, u32, u32, u32 *);
typedef int (API *tRC)(u32, u32, u32);
static u32 *find_affector(void)
{
	MEMORY_BASIC_INFORMATION mi;
	unsigned char *a = (unsigned char *)0x10000;
	while (VirtualQuery(a, &mi, sizeof mi))
	{
		if (mi.State == MEM_COMMIT && (mi.Protect & PAGE_READWRITE) && !(mi.Protect & PAGE_GUARD))
		{
			u32 *p = (u32 *)mi.BaseAddress, *e = (u32 *)((char *)mi.BaseAddress + mi.RegionSize) - 64;
			for (; p < e; p++) if (*p == 0x1005eb04) return p;
		}
		a = (unsigned char *)mi.BaseAddress + mi.RegionSize;
		if ((u32)a >= 0x7ffe0000) break;
	}
	return 0;
}

static void dump_offsets(u32 ver, const char *model, const char *anim)
{
	u32 m[2], s[2], an[2], c[3];
	const char *mn = strrchr(model, '\\'), *ann = strrchr(anim, '\\');
	mn = mn ? mn + 1 : model; ann = ann ? ann + 1 : anim;
	fwrite("GXOF", 1, 4, out); w32((u32)strlen(mn)); wraw(mn, (u32)strlen(mn)); w32((u32)strlen(ann)); wraw(ann, (u32)strlen(ann));
	if (((tOM)P("_GrannyOpenModel@12"))(ver, model, m)) { w32(0); return; }
	{	/* drop its textures */
		u32 n = 0, i, t[16];
		((tNT)P("_GrannyGetNewTexturesLeft@8"))(ver, &n);
		for (i = 0; i < n; i++) { ((tNT)P("_GrannyLockNextNewTexture@8"))(ver, t); ((tUT)P("_GrannyUnlockNewTexture@8"))(t, 0); }
	}
	((tOS)P("_GrannyOpenSequence@12"))(m[0], m[1], s);
	if (((tOM)P("_GrannyOpenAnimation@12"))(ver, anim, an) || ((tAC)P("_GrannyAddControlledAnimation@20"))(s[0], s[1], an[0], an[1], c))
		w32(0);
	else
	{
		u32 *aff = find_affector();
		if (!aff) { w32(0); fprintf(stderr, "%s + %s: affector not found\n", mn, ann); }
		else
		{
			u32 *bt = aff + 0x6c / 4, n = bt[0], i;
			float *off = (float *)bt[1];
			u32 *bones = (u32 *)bt[3];
			w32(1); w32(n);
			for (i = 0; i < n; i++) { w32(bones && bones[i] ? 1 : 0); wraw(off + 16 * i, 64); }
		}
		((tRC)P("_GrannyRemoveControlledAnimation@12"))(c[0], c[1], c[2]);
		((tC2)P("_GrannyCloseAnimation@8"))(an[0], an[1]);
	}
	((tC2)P("_GrannyCloseSequence@8"))(s[0], s[1]);
	((tC2)P("_GrannyCloseModel@8"))(m[0], m[1]);
}

int main(int argc, char **argv)
{
	u32 ver;
	int i;
	if (argc < 3) { fprintf(stderr, "usage: grnextract out.gxtr model.grn...\n"); return 1; }
	d = LoadLibraryA("granny.dll");
	if (!d) { fprintf(stderr, "cannot load granny.dll\n"); return 2; }
	((tOV)P("_GrannyOpenVersion@20"))("1.2b", "win32", "10-4-2000",
		"(C) Copyright 1999-2000 RAD Game Tools, Inc.  All Rights Reserved.", &ver);
	((tAF)P("_GrannyAllowTextureFormat@8"))(ver, 4);
	out = fopen(argv[1], "wb");
	if (!out) return 3;
	for (i = 2; i < argc; i++)
	{
		if (!strcmp(argv[i], "-f") && i + 1 < argc)
		{	/* list file: "model <path>" or "offsets <model path> <animation path>" */
			char line[2048], a[1024], b[1024], c2[1024];
			FILE *lf = fopen(argv[++i], "r");
			if (!lf) { fprintf(stderr, "cannot read %s\n", argv[i]); return 4; }
			while (fgets(line, sizeof line, lf))
			{
				int k = sscanf(line, "%1023s %1023s %1023s", a, b, c2);
				if (k >= 2 && !strcmp(a, "model")) dump_file(ver, b);
				else if (k == 3 && !strcmp(a, "offsets")) dump_offsets(ver, b, c2);
			}
			fclose(lf);
		}
		else dump_file(ver, argv[i]);
	}
	fclose(out);
	return 0;
}
