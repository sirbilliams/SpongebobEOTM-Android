/*  grndump - dump RAD Granny 1.x (.grn) models/animations using the game's own granny.dll.
    Win32 console tool; runs under Wine. API recovered from how Neutron2.exe drives granny.dll
    (C3DGrannyObj, see docs/granny-api.md).

    usage:  grndump.exe <out.gdmp> <model.grn> [anim.grn [anim.grn ...]] [-fps N]

    Output (little-endian):
      "GDMP" u32 version(1)
      u32 ntextures; per texture: u32 w, u32 h, w*h*3 RGB bytes
      u32 nposes;   per pose: u32 name_len, name, u32 nframes, f32 seconds,
                    per frame: u32 nstates, per state:
                        i32 texture (-1 none), u32 nverts, nverts*(pos3 nrm3 uv3) f32,
                        u32 ntris, ntris*3 u16
    Pose 0 is the bind pose (no animation, 1 frame).                                      */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int u32;
#define API __stdcall

typedef int (API *pOpenVersion)(const char*, const char*, const char*, const char*, u32 *ver);
typedef int (API *pAllowTextureFormat)(u32 ver, u32 fmt);
typedef int (API *pOpenModel)(u32 ver, const char *file, u32 model[2]);
typedef int (API *pOpenSequence)(u32 m0, u32 m1, u32 seq[2]);
typedef int (API *pOpenAnimation)(u32 ver, const char *file, u32 anim[2]);
typedef int (API *pAddControlledAnimation)(u32 s0, u32 s1, u32 a0, u32 a1, u32 ctrl[3]);
typedef int (API *pRemoveControlledAnimation)(u32 c0, u32 c1, u32 c2);
typedef int (API *pSetAnimationParameter)(u32 c0, u32 c1, u32 c2, u32 param, double value);
typedef int (API *pHasControlExpired)(u32 c0, u32 c1, u32 c2, u32 *expired);
typedef int (API *pAdvanceTime)(u32 ver, double seconds);
typedef int (API *pGetNewTexturesLeft)(u32 ver, u32 *n);
typedef int (API *pLockNextNewTexture)(u32 ver, u32 *tex);
typedef int (API *pUnlockNewTexture)(u32 *tex, u32 cookie);
typedef int (API *pLockSequenceForRendering)(u32 s0, u32 s1, u32 flags, u32 *out);
typedef int (API *pGetRenderingStatesLeft)(u32 r0, u32 r1, u32 *n);
typedef int (API *pLockNextRenderingState)(u32 r0, u32 r1, u32 *state);
typedef int (API *pUnlockRenderingState)(u32 *state);
typedef int (API *pUnlockRendering)(u32 r0, u32 r1);
typedef int (API *pGetLastError)(void);

static pOpenVersion OpenVersion; static pAllowTextureFormat AllowTextureFormat;
static pOpenModel OpenModel; static pOpenSequence OpenSequence; static pOpenAnimation OpenAnimation;
static pAddControlledAnimation AddControlledAnimation; static pRemoveControlledAnimation RemoveControlledAnimation;
static pSetAnimationParameter SetAnimationParameter; static pHasControlExpired HasControlExpired;
static pAdvanceTime AdvanceTime; static pGetNewTexturesLeft GetNewTexturesLeft;
static pLockNextNewTexture LockNextNewTexture; static pUnlockNewTexture UnlockNewTexture;
static pLockSequenceForRendering LockSequenceForRendering; static pGetRenderingStatesLeft GetRenderingStatesLeft;
static pLockNextRenderingState LockNextRenderingState; static pUnlockRenderingState UnlockRenderingState;
static pUnlockRendering UnlockRendering; static pGetLastError GetLastErr;

static HMODULE dll;
static void *need(const char *n)
{
	void *p = (void*)GetProcAddress(dll, n);
	if (!p) { fprintf(stderr, "missing export %s\n", n); exit(2); }
	return p;
}

/* Rendering-state layout (u32 index = byte offset / 4), from Neutron2.exe 0x41d7a0:
   +0x18 texture cookie; streams of 0x18 bytes: {present, ?, ?, count, ?, ptr}
   positions @0x2c (f32x3), uvs @0x74 (f32x3), normals @0x8c (f32x3), triangles @0xa4 (u16x3) */
#define ST(o) (state[(o) / 4])
enum { S_COOKIE = 0x18, S_POS = 0x2c, S_UV = 0x74, S_NRM = 0x8c, S_TRI = 0xa4 };

static FILE *out;
static void w32(u32 v) { fwrite(&v, 4, 1, out); }

static void dump_frame(u32 seq[2], long *nstates_pos)
{
	u32 r[3] = { 0, 0, 0 }, nstates = 0, i, k;
	static u32 state[1024];
	if (LockSequenceForRendering(seq[0], seq[1], 0x3b, r)) { w32(0); return; }
	GetRenderingStatesLeft(r[0], r[1], &r[2]);
	long cnt_at = ftell(out); w32(0);
	for (i = 0; i < r[2]; i++)
	{
		memset(state, 0, sizeof(state));
		if (LockNextRenderingState(r[0], r[1], state)) continue;
		u32 nv = ST(S_POS) ? ST(S_POS + 0xc) : 0;
		float *pos = (float*)ST(S_POS + 0x14);
		float *uv = ST(S_UV) ? (float*)ST(S_UV + 0x14) : NULL;
		u32 nuv = ST(S_UV) ? ST(S_UV + 0xc) : 0;
		float *nrm = ST(S_NRM) ? (float*)ST(S_NRM + 0x14) : NULL;
		u32 nnrm = ST(S_NRM) ? ST(S_NRM + 0xc) : 0;
		u32 nt = ST(S_TRI) ? ST(S_TRI + 0xc) : 0;
		unsigned short *tri = (unsigned short*)ST(S_TRI + 0x14);
		w32(ST(S_COOKIE) ? ST(S_COOKIE) - 1 : (u32)-1);
		w32(nv);
		for (k = 0; k < nv; k++)
		{
			float z[3] = { 0, 0, 0 };
			fwrite(pos + k * 3, 4, 3, out);
			fwrite(nrm && k < nnrm ? nrm + k * 3 : z, 4, 3, out);
			fwrite(uv && k < nuv ? uv + k * 3 : z, 4, 3, out);
		}
		w32(nt);
		if (nt) fwrite(tri, 2, nt * 3, out);
		UnlockRenderingState(state);
		nstates++;
	}
	UnlockRendering(r[0], r[1]);
	long end = ftell(out);
	fseek(out, cnt_at, SEEK_SET); w32(nstates); fseek(out, end, SEEK_SET);
	(void)nstates_pos;
}

static void wname(const char *s) { w32((u32)strlen(s)); fwrite(s, 1, strlen(s), out); }

int main(int argc, char **argv)
{
	int i, fps = 15;
	if (argc < 3) { fprintf(stderr, "usage: grndump out.gdmp model.grn [anim.grn ...] [-fps N]\n"); return 1; }
	for (i = 1; i < argc; i++) if (!strcmp(argv[i], "-fps") && i + 1 < argc) fps = atoi(argv[i + 1]);

	dll = LoadLibraryA("granny.dll");
	if (!dll) { fprintf(stderr, "cannot load granny.dll (%lu)\n", GetLastError()); return 2; }
	OpenVersion = need("_GrannyOpenVersion@20"); AllowTextureFormat = need("_GrannyAllowTextureFormat@8");
	OpenModel = need("_GrannyOpenModel@12"); OpenSequence = need("_GrannyOpenSequence@12");
	OpenAnimation = need("_GrannyOpenAnimation@12"); AddControlledAnimation = need("_GrannyAddControlledAnimation@20");
	RemoveControlledAnimation = need("_GrannyRemoveControlledAnimation@12");
	SetAnimationParameter = need("_GrannySetAnimationParameter@24"); HasControlExpired = need("_GrannyHasControlExpired@16");
	AdvanceTime = need("_GrannyAdvanceTime@12"); GetNewTexturesLeft = need("_GrannyGetNewTexturesLeft@8");
	LockNextNewTexture = need("_GrannyLockNextNewTexture@8"); UnlockNewTexture = need("_GrannyUnlockNewTexture@8");
	LockSequenceForRendering = need("_GrannyLockSequenceForRendering@16");
	GetRenderingStatesLeft = need("_GrannyGetRenderingStatesLeft@12");
	LockNextRenderingState = need("_GrannyLockNextRenderingState@12");
	UnlockRenderingState = need("_GrannyUnlockRenderingState@4"); UnlockRendering = need("_GrannyUnlockRendering@8");
	GetLastErr = need("_GrannyGetLastError@0");

	u32 ver = 0, model[2] = { 0, 0 }, seq[2] = { 0, 0 };
	if (OpenVersion("1.2b", "win32", "10-4-2000",
					"(C) Copyright 1999-2000 RAD Game Tools, Inc.  All Rights Reserved.", &ver))
	{ fprintf(stderr, "GrannyOpenVersion failed (%d)\n", GetLastErr()); return 3; }
	AllowTextureFormat(ver, 4);
	if (OpenModel(ver, argv[2], model)) { fprintf(stderr, "OpenModel %s failed (%d)\n", argv[2], GetLastErr()); return 4; }
	if (OpenSequence(model[0], model[1], seq)) { fprintf(stderr, "OpenSequence failed (%d)\n", GetLastErr()); return 5; }

	out = fopen(argv[1], "wb");
	if (!out) { fprintf(stderr, "cannot write %s\n", argv[1]); return 6; }
	fwrite("GDMP", 1, 4, out); w32(1);

	/* Textures appear after the first render lock; render once, then collect them. */
	{ long p = ftell(out); u32 r[3]; if (!LockSequenceForRendering(seq[0], seq[1], 0x3b, r)) UnlockRendering(r[0], r[1]); fseek(out, p, SEEK_SET); }
	u32 ntex = 0, t;
	GetNewTexturesLeft(ver, &ntex);
	w32(ntex);
	for (t = 0; t < ntex; t++)
	{
		static u32 tex[256];
		memset(tex, 0, sizeof(tex));
		if (LockNextNewTexture(ver, tex)) { w32(0); w32(0); continue; }
		u32 w = tex[0x14 / 4], h = tex[0x18 / 4];
		w32(w); w32(h);
		fwrite((void*)tex[0x28 / 4], 1, w * h * 3, out);
		UnlockNewTexture(tex, t + 1);           /* cookie = texture index + 1 */
	}

	/* Poses: bind pose, then each animation sampled until it expires (max 60 s). */
	int nanims = 0;
	for (i = 3; i < argc; i++) { if (!strcmp(argv[i], "-fps")) { i++; continue; } nanims++; }
	w32(1 + nanims);
	wname("bind"); w32(1); { float z = 0; fwrite(&z, 4, 1, out); }
	dump_frame(seq, NULL);

	for (i = 3; i < argc; i++)
	{
		if (!strcmp(argv[i], "-fps")) { i++; continue; }
		u32 anim[2] = { 0, 0 }, ctrl[3] = { 0, 0, 0 }, expired = 0;
		const char *nm = strrchr(argv[i], '\\'); nm = nm ? nm + 1 : argv[i];
		{ const char *s2 = strrchr(nm, '/'); if (s2) nm = s2 + 1; }
		wname(nm);
		long frames_at = ftell(out); w32(0); { float z = 0; fwrite(&z, 4, 1, out); }
		u32 nframes = 0;
		if (!OpenAnimation(ver, argv[i], anim) &&
			!AddControlledAnimation(seq[0], seq[1], anim[0], anim[1], ctrl))
		{
			SetAnimationParameter(ctrl[0], ctrl[1], ctrl[2], 3, 1.0);   /* play once */
			for (nframes = 0; nframes < (u32)(60 * fps); nframes++)
			{
				dump_frame(seq, NULL);
				AdvanceTime(ver, 1.0 / fps);
				HasControlExpired(ctrl[0], ctrl[1], ctrl[2], &expired);
				if (expired) { nframes++; break; }
			}
			RemoveControlledAnimation(ctrl[0], ctrl[1], ctrl[2]);
		}
		else fprintf(stderr, "animation %s failed (%d)\n", argv[i], GetLastErr());
		long end = ftell(out);
		float secs = (float)nframes / fps;
		fseek(out, frames_at, SEEK_SET); w32(nframes); fwrite(&secs, 4, 1, out); fseek(out, end, SEEK_SET);
		fprintf(stderr, "%s: %u frames\n", nm, nframes);
	}
	fclose(out);
	fprintf(stderr, "ok: %u textures, %d animations\n", ntex, nanims);
	return 0;
}
