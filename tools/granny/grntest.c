/*  grntest - run a scripted sequence of Granny calls and record everything the game could observe.

    Built twice from this one file:
      Win32 (reference):  i686-w64-mingw32-gcc -O2 -o grntest.exe grntest.c        (loads the real granny.dll, run under Wine)
      native (ours):      cc -O2 -DGRNTEST_COMPAT -I game/granny grntest.c game/granny/granny_{compat,grn,bink}.cpp -lstdc++ -lm
                          (both read the .grn files of the grn directory; tools/granny/verify.sh builds and runs them)
    usage: grntest <script.txt> <out.gtst> [grn directory]

    Script: one command per line, '#' comments.  Names are symbolic handles.
      model M file.grn | seq S M | anim A file.grn | add C S A | param C id value | getparam C
      advance dt | expired C | remove C | release C | render S flags | textures cookie_base
      bones S | boneindex S name (use ~ for spaces) | attach S childbone P parentbone(name or #index) [dof0 dof1] | detach S
      closeseq S | closeanim A | closemodel M | close | time
    Output (little-endian records, read by grntest.py):
      u32 tag, u32 line, i32 result, then a payload depending on the tag (see emit_* below).          */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint32_t u32;

#ifdef GRNTEST_COMPAT
#include "granny_compat.h"
#define CALL(f) f
#else
#include <windows.h>
#undef GetCurrentTime
#define API __stdcall
static HMODULE dll;
static void *need(const char *n)
{
	void *p = (void *)GetProcAddress(dll, n);
	if (!p) { fprintf(stderr, "missing export %s\n", n); exit(2); }
	return p;
}
#define DECL(ret, name, args) typedef ret (API *t_##name) args; static t_##name p_##name;
DECL(int, OpenVersion, (const char *, const char *, const char *, const char *, u32 *))
DECL(int, AllowTextureFormat, (u32, u32))
DECL(int, OpenModel, (u32, const char *, u32 *))
DECL(int, OpenSequence, (u32, u32, u32 *))
DECL(int, OpenAnimation, (u32, const char *, u32 *))
DECL(int, AddControlledAnimation, (u32, u32, u32, u32, u32 *))
DECL(int, SetAnimationParameter, (u32, u32, u32, u32, double))
DECL(int, GetAnimationParameter, (u32, u32, u32, u32, double *))
DECL(int, HasControlExpired, (u32, u32, u32, u32 *))
DECL(int, RemoveControlledAnimation, (u32, u32, u32))
DECL(int, ReleaseControlledAnimation, (u32, u32, u32))
DECL(int, AdvanceTime, (u32, double))
DECL(int, GetCurrentTime, (u32, double *))
DECL(int, LockSequenceForRendering, (u32, u32, u32, u32 *))
DECL(int, GetRenderingStatesLeft, (u32, u32, u32 *))
DECL(int, LockNextRenderingState, (u32, u32, u32 *))
DECL(int, UnlockRenderingState, (u32 *))
DECL(int, UnlockRendering, (u32, u32))
DECL(int, GetNewTexturesLeft, (u32, u32 *))
DECL(int, LockNextNewTexture, (u32, u32 *))
DECL(int, UnlockNewTexture, (u32 *, u32))
DECL(int, GetBoneIndexFromName, (u32, u32, const char *, u32 *))
DECL(int, AttachSequence, (u32, u32, u32, u32, u32, u32, u32, u32))
DECL(int, DetachSequence, (u32, u32))
DECL(int, CloseSequence, (u32, u32))
DECL(int, CloseAnimation, (u32, u32))
DECL(int, CloseModel, (u32, u32))
DECL(int, Close, (u32))
DECL(int, GetBoneCount, (u32, u32, u32 *))
DECL(int, GetBoneState, (u32, u32, u32, u32, void *))
#define LOAD(name, nb) p_##name = (t_##name)need("_Granny" #name "@" #nb)
#define CALL(f) f

static void load_dll(void)
{
	dll = LoadLibraryA("granny.dll");
	if (!dll) { fprintf(stderr, "cannot load granny.dll\n"); exit(2); }
	LOAD(OpenVersion, 20); LOAD(AllowTextureFormat, 8); LOAD(OpenModel, 12); LOAD(OpenSequence, 12);
	LOAD(OpenAnimation, 12); LOAD(AddControlledAnimation, 20); LOAD(SetAnimationParameter, 24);
	LOAD(GetAnimationParameter, 20); LOAD(HasControlExpired, 16); LOAD(RemoveControlledAnimation, 12);
	LOAD(ReleaseControlledAnimation, 12); LOAD(AdvanceTime, 12); LOAD(GetCurrentTime, 8);
	LOAD(LockSequenceForRendering, 16); LOAD(GetRenderingStatesLeft, 12); LOAD(LockNextRenderingState, 12);
	LOAD(UnlockRenderingState, 4); LOAD(UnlockRendering, 8); LOAD(GetNewTexturesLeft, 8);
	LOAD(LockNextNewTexture, 8); LOAD(UnlockNewTexture, 8); LOAD(GetBoneIndexFromName, 16);
	LOAD(AttachSequence, 32); LOAD(DetachSequence, 8); LOAD(CloseSequence, 8); LOAD(CloseAnimation, 8);
	LOAD(CloseModel, 8); LOAD(Close, 4); LOAD(GetBoneCount, 12); LOAD(GetBoneState, 20);
}
#endif

/* name the compat functions like the DLL pointers so the code below is shared */
#ifdef GRNTEST_COMPAT
#define p_OpenVersion GrannyOpenVersion
#define p_AllowTextureFormat GrannyAllowTextureFormat
#define p_OpenModel GrannyOpenModel
#define p_OpenSequence GrannyOpenSequence
#define p_OpenAnimation GrannyOpenAnimation
#define p_AddControlledAnimation GrannyAddControlledAnimation
#define p_SetAnimationParameter GrannySetAnimationParameter
#define p_GetAnimationParameter GrannyGetAnimationParameter
#define p_HasControlExpired GrannyHasControlExpired
#define p_RemoveControlledAnimation GrannyRemoveControlledAnimation
#define p_ReleaseControlledAnimation GrannyReleaseControlledAnimation
#define p_AdvanceTime GrannyAdvanceTime
#define p_GetCurrentTime GrannyGetCurrentTime
#define p_LockSequenceForRendering GrannyLockSequenceForRendering
#define p_GetRenderingStatesLeft GrannyGetRenderingStatesLeft
#define p_UnlockRendering GrannyUnlockRendering
#define p_GetNewTexturesLeft GrannyGetNewTexturesLeft
#define p_GetBoneIndexFromName GrannyGetBoneIndexFromName
#define p_AttachSequence GrannyAttachSequence
#define p_DetachSequence GrannyDetachSequence
#define p_CloseSequence GrannyCloseSequence
#define p_CloseAnimation GrannyCloseAnimation
#define p_CloseModel GrannyCloseModel
#define p_Close GrannyClose
#define p_GetBoneCount GrannyGetBoneCount
#endif

static FILE *out;
static int line_no;
static void w32(u32 v) { fwrite(&v, 4, 1, out); }
static void wf(float v) { fwrite(&v, 4, 1, out); }
static void wd(double v) { fwrite(&v, 8, 1, out); }
static void rec(const char tag[4], int result) { fwrite(tag, 1, 4, out); w32((u32)line_no); w32((u32)result); }

/* symbolic handles */
#define MAXH 4096
static struct { char name[64]; u32 h[3]; } handles[MAXH];
static int nhandles;
static u32 *H(const char *name)
{
	int i;
	for (i = 0; i < nhandles; i++) if (!strcmp(handles[i].name, name)) return handles[i].h;
	if (nhandles == MAXH) { fprintf(stderr, "too many handles\n"); exit(3); }
	strncpy(handles[nhandles].name, name, 63);
	memset(handles[nhandles].h, 0, sizeof handles[nhandles].h);
	return handles[nhandles++].h;
}

static u32 ver;
static char grndir[1024];
static void path(char *dst, const char *file)
{
#ifdef GRNTEST_COMPAT
	sprintf(dst, "%s/%s", grndir, file);
#else
	char *p;
	sprintf(dst, "%s\\%s", grndir, file);
	for (p = dst; *p; p++) if (*p == '/') *p = '\\';
#endif
}

/* ---- rendering state: header u32[11] (pointers as 0), transform f32[16] if has_transform,
        6 x (present, type, dim, count, unknown, data), tail u32[4] */
static void emit_stream(u32 present, u32 type, u32 dim, u32 count, u32 unknown, const void *data)
{
	u32 esz = type == 3 ? 2 : 4, bytes = present && data ? count * dim * esz : 0;
	w32(present); w32(type); w32(dim); w32(count); w32(unknown); w32(bytes);
	if (bytes) fwrite(data, 1, bytes, out);
}

static void do_render(const char *sname, u32 flags)
{
	u32 *s = H(sname), lock[3] = { 0, 0, 0 }, n = 0, i;
	int r = CALL(p_LockSequenceForRendering)(s[0], s[1], flags, lock);
	if (r) { rec("RNDR", r); w32(0); w32(0); return; }   /* no states, no unlock */
	r = CALL(p_GetRenderingStatesLeft)(lock[0], lock[1], &n);
	rec("RNDR", r); w32(n);
	for (i = 0; i < n; i++)
	{
#ifdef GRNTEST_COMPAT
		granny_rendering_state st;
		const granny_stream *sp[6];
		int k;
		memset(&st, 0, sizeof st);
		r = GrannyLockNextRenderingState(lock[0], lock[1], &st);
		w32((u32)r);
		w32(st.lock[0] == lock[0] && st.lock[1] == lock[1]); w32(st.sequence[0] == s[0] && st.sequence[1] == s[1]);
		w32(st.present); w32(st.texture_pending); w32((u32)st.texture_cookie); w32(st.has_transform);
		w32(st.deformed); w32(st.mesh_cookie);
		if (st.has_transform && st.transform) { for (k = 0; k < 16; k++) wf(st.transform[k]); } else for (k = 0; k < 16; k++) wf(0);
		sp[0] = &st.positions; sp[1] = &st.color0; sp[2] = &st.color1; sp[3] = &st.texcoords; sp[4] = &st.normals; sp[5] = &st.triangles;
		for (k = 0; k < 6; k++) emit_stream(sp[k]->present, sp[k]->type, sp[k]->dim, sp[k]->count, sp[k]->unknown, sp[k]->data);
		w32(st.src_blend); w32(st.dst_blend); w32(st.reserved[0]); w32(st.reserved[1]);
		GrannyUnlockRenderingState(&st);
#else
		u32 st[0xcc / 4 + 16];
		int k;
		memset(st, 0, sizeof st);
		r = p_LockNextRenderingState(lock[0], lock[1], st);
		w32((u32)r);
		w32(st[0] == lock[0] && st[1] == lock[1]); w32(st[2] == s[0] && st[3] == s[1]);
		w32(st[4]); w32(st[5]); w32(st[6]); w32(st[7]); w32(st[9]); w32(st[10]);
		if (st[7] && st[8]) { const float *m = (const float *)st[8]; for (k = 0; k < 16; k++) wf(m[k]); } else for (k = 0; k < 16; k++) wf(0);
		for (k = 0; k < 6; k++)
		{
			u32 *q = st + (0x2c + 0x18 * k) / 4;
			emit_stream(q[0], q[1], q[2], q[3], q[4], (const void *)q[5]);
		}
		w32(st[0x2f]); w32(st[0x30]); w32(st[0x31]); w32(st[0x32]);
		p_UnlockRenderingState(st);
#endif
	}
	r = CALL(p_UnlockRendering)(lock[0], lock[1]);
	w32((u32)r);
}

static void do_textures(u32 cookie_base)
{
	u32 n = 0, i;
	int r = CALL(p_GetNewTexturesLeft)(ver, &n);
	rec("TEXS", r); w32(n);
	for (i = 0; i < n; i++)
	{
#ifdef GRNTEST_COMPAT
		granny_new_texture t;
		memset(&t, 0, sizeof t);
		r = GrannyLockNextNewTexture(ver, &t);
		w32((u32)r);
		w32(t.version == ver); w32(t.format); w32(t.width); w32(t.height); w32(t.bytes_per_pixel); w32(t.pitch); w32(t.size);
		if (!r && t.pixels) fwrite(t.pixels, 1, t.size, out);
		r = GrannyUnlockNewTexture(&t, cookie_base + i);
#else
		u32 t[0x2c / 4 + 8];
		memset(t, 0, sizeof t);
		r = p_LockNextNewTexture(ver, t);
		w32((u32)r);
		w32(t[0] == ver); w32(t[4]); w32(t[5]); w32(t[6]); w32(t[7]); w32(t[8]); w32(t[9]);
		if (!r && t[10]) fwrite((const void *)t[10], 1, t[9], out);
		r = p_UnlockNewTexture(t, cookie_base + i);
#endif
		w32((u32)r);
	}
}

static void do_bones(const char *sname)
{
	u32 *s = H(sname), n = 0, b, k;
	int r = CALL(p_GetBoneCount)(s[0], s[1], &n);
	rec("BONE", r); w32(n);
	for (b = 0; b < n; b++)
	{
		u32 st[0xdc / 4];
		memset(st, 0, sizeof st);
#ifdef GRNTEST_COMPAT
		r = GrannyGetBoneState(s[0], s[1], b, 0x1ff, (granny_transform_state *)st);
#else
		r = p_GetBoneState(s[0], s[1], b, 0x1ff, st);
#endif
		w32((u32)r); w32(st[7]);
		for (k = 8; k < 42; k++) fwrite(&st[k], 4, 1, out);   /* pos3 rot9 quat4 ss9 combined9 */
	}
}

int main(int argc, char **argv)
{
	char line[1024];
	FILE *in;
	if (argc < 3) { fprintf(stderr, "usage: grntest script.txt out.gtst [grn dir]\n"); return 1; }
	strcpy(grndir, argc > 3 ? argv[3] : ".");
#ifndef GRNTEST_COMPAT
	load_dll();
#endif
	in = fopen(argv[1], "r");
	out = fopen(argv[2], "wb");
	if (!in || !out) { fprintf(stderr, "cannot open files\n"); return 1; }
	fwrite("GTST", 1, 4, out); w32(1);
	{
		int r = CALL(p_OpenVersion)("1.2b", "win32", "10-4-2000",
		                            "(C) Copyright 1999-2000 RAD Game Tools, Inc.  All Rights Reserved.", &ver);
		rec("VERS", r);
		r = CALL(p_AllowTextureFormat)(ver, 4);
		rec("AFMT", r);
	}
	while (fgets(line, sizeof line, in))
	{
		char cmd[64] = "", a[256] = "", b[256] = "", c[256] = "", d[64] = "", e[64] = "";
		int r;
		char *q;
		line_no++;
		if (line[0] == '#' || sscanf(line, "%63s %255s %255s %255s %63s %63s", cmd, a, b, c, d, e) < 1) continue;
		for (q = b; *q; q++) if (*q == '~') *q = ' ';     /* '~' stands for a space in bone names */
		for (q = d; *q; q++) if (*q == '~') *q = ' ';
		if (!strcmp(cmd, "model")) { char p[1100]; path(p, b); r = CALL(p_OpenModel)(ver, p, H(a)); rec("MODL", r); }
		else if (!strcmp(cmd, "anim")) { char p[1100]; path(p, b); r = CALL(p_OpenAnimation)(ver, p, H(a)); rec("ANIM", r); }
		else if (!strcmp(cmd, "seq")) { u32 *m = H(b); r = CALL(p_OpenSequence)(m[0], m[1], H(a)); rec("SEQU", r); }
		else if (!strcmp(cmd, "add"))
		{
			u32 *s = H(b), *an = H(c);
			r = CALL(p_AddControlledAnimation)(s[0], s[1], an[0], an[1], H(a));
			rec("ADDC", r);
		}
		else if (!strcmp(cmd, "param"))
		{
			u32 *ct = H(a);
			r = CALL(p_SetAnimationParameter)(ct[0], ct[1], ct[2], (u32)atoi(b), atof(c));
			rec("PARM", r);
		}
		else if (!strcmp(cmd, "getparam"))
		{
			u32 *ct = H(a), k;
			rec("GETP", 0);
			for (k = 0; k < 9; k++) { double v = -1; r = CALL(p_GetAnimationParameter)(ct[0], ct[1], ct[2], k, &v); w32((u32)r); wd(v); }
		}
		else if (!strcmp(cmd, "advance")) { r = CALL(p_AdvanceTime)(ver, atof(a)); rec("ADVT", r); }
		else if (!strcmp(cmd, "advancen"))    /* advancen <count> <dt>: many frames, one record */
		{
			int i, n = atoi(a);
			r = 0;
			for (i = 0; i < n && !r; i++) r = CALL(p_AdvanceTime)(ver, atof(b));
			rec("ADVT", r);
		}
		else if (!strcmp(cmd, "time")) { double t = -1; r = CALL(p_GetCurrentTime)(ver, &t); rec("TIME", r); wd(t); }
		else if (!strcmp(cmd, "expired"))
		{
			u32 *ct = H(a), x = 12345;
			r = CALL(p_HasControlExpired)(ct[0], ct[1], ct[2], &x);
			rec("EXPD", r); w32(x);
		}
		else if (!strcmp(cmd, "remove")) { u32 *ct = H(a); r = CALL(p_RemoveControlledAnimation)(ct[0], ct[1], ct[2]); rec("REMV", r); }
		else if (!strcmp(cmd, "release")) { u32 *ct = H(a); r = CALL(p_ReleaseControlledAnimation)(ct[0], ct[1], ct[2]); rec("RELS", r); }
		else if (!strcmp(cmd, "render")) do_render(a, (u32)strtoul(b, 0, 0));
		/* raw lock calls, for the error paths: lock <lock> <seq> <flags> / left <lock> / unlock <lock> */
		else if (!strcmp(cmd, "lock")) { u32 *s = H(b); r = CALL(p_LockSequenceForRendering)(s[0], s[1], (u32)strtoul(c, 0, 0), H(a)); rec("LOCK", r); }
		else if (!strcmp(cmd, "left")) { u32 *l = H(a), n = 12345; r = CALL(p_GetRenderingStatesLeft)(l[0], l[1], &n); rec("LEFT", r); w32(n); }
		else if (!strcmp(cmd, "unlock")) { u32 *l = H(a); r = CALL(p_UnlockRendering)(l[0], l[1]); rec("UNLK", r); }
		/* handle <name>: which words of a handle are filled in (the id values themselves differ: the
		   DLL numbers its objects in steps that also count internal objects) */
		else if (!strcmp(cmd, "handle")) { u32 *h = H(a); rec("HNDL", 0); w32(h[0] == ver); w32(h[1] != 0); w32(h[2] != 0); }
		else if (!strcmp(cmd, "texleft")) { u32 n = 12345; r = CALL(p_GetNewTexturesLeft)(ver, &n); rec("TXLF", r); w32(n); }
		else if (!strcmp(cmd, "textures")) do_textures((u32)strtoul(a, 0, 0));
		else if (!strcmp(cmd, "bones")) do_bones(a);
		else if (!strcmp(cmd, "boneindex"))
		{
			u32 *s = H(a), idx = 0xdeadbeef;
			r = CALL(p_GetBoneIndexFromName)(s[0], s[1], b, &idx);
			rec("BIDX", r); w32(idx);
		}
		else if (!strcmp(cmd, "attach"))
		{
			u32 *s = H(a), *p = H(c), pb = 0;
			if (d[0] == '#') pb = (u32)atoi(d + 1);
			else { int rr = CALL(p_GetBoneIndexFromName)(p[0], p[1], d, &pb); (void)rr; }
			r = CALL(p_AttachSequence)(s[0], s[1], (u32)atoi(b), p[0], p[1], pb,
			                           e[0] ? (u32)strtoul(e, 0, 0) : 0x1ff, 0x1ff);
			rec("ATCH", r); w32(pb);
		}
		else if (!strcmp(cmd, "detach")) { u32 *s = H(a); r = CALL(p_DetachSequence)(s[0], s[1]); rec("DTCH", r); }
		else if (!strcmp(cmd, "closeseq")) { u32 *s = H(a); r = CALL(p_CloseSequence)(s[0], s[1]); rec("CLSS", r); }
		else if (!strcmp(cmd, "closeanim")) { u32 *s = H(a); r = CALL(p_CloseAnimation)(s[0], s[1]); rec("CLSA", r); }
		else if (!strcmp(cmd, "closemodel")) { u32 *s = H(a); r = CALL(p_CloseModel)(s[0], s[1]); rec("CLSM", r); }
		else if (!strcmp(cmd, "close")) { r = CALL(p_Close)(ver); rec("CLOS", r); }
		else { fprintf(stderr, "line %d: unknown command %s\n", line_no, cmd); return 4; }
	}
	fwrite("END!", 1, 4, out); w32((u32)line_no); w32(0);
	fclose(out);
	return 0;
}
