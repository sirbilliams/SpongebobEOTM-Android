/* xftest: the coordinate-system functions on real files (the game's own call pattern, 0x403810): xftest model.grn [animation.grn]
   prints the tool system, the recommended transform for "mirror x", and the first state of the model before/after. */
#include "granny_compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void pm(const char *n, const float *m, int c) { printf("%s:", n); for (int i = 0; i < c; i++) printf(" %.4f", m[i]); printf("\n"); }
int main(int argc, char **argv)
{
	granny_uint32 ver, mod[2], seq[2], lock[2], anim[2];
	if (getenv("GRANNY_DLL")) GrannyCompatSetDllPath(getenv("GRANNY_DLL"));
	if (GrannyOpenVersion("1.2b", "win32", "10-4-2000", "(C) Copyright 1999-2000 RAD Game Tools, Inc.  All Rights Reserved.", &ver)) return 1;
	GrannyAllowTextureFormat(ver, 4);
	if (GrannyOpenModel(ver, argv[1], mod)) { printf("open model failed %d\n", GrannyGetLastError()); return 1; }
	float to[3], ta[9];
	printf("get %d\n", GrannyGetToolCoordinateSystemForModel(mod[0], mod[1], to, ta));
	pm("origin", to, 3); pm("axes", ta, 9);
	float wo[3] = { 0, 0, 0 }, wa[9] = { -ta[0], 0, 0, 0, 0, ta[5], 0, ta[7], 0 };
	float T[3], M[9];
	printf("rec %d\n", GrannyRecommendTransform(ver, to, ta, wo, wa, T, M));
	pm("T", T, 3); pm("M", M, 9);
	GrannyOpenSequence(mod[0], mod[1], seq);
	float before[3] = {0}; unsigned tri0[3] = {0};
	for (int pass = 0; pass < 2; pass++)
	{
		GrannyLockSequenceForRendering(seq[0], seq[1], 0x3b, lock);
		granny_uint32 n; GrannyGetRenderingStatesLeft(lock[0], lock[1], &n);
		granny_rendering_state st;
		GrannyLockNextRenderingState(lock[0], lock[1], &st);
		const float *p = (const float *)st.positions.data, *nr = (const float *)st.normals.data;
		const unsigned short *tr = (const unsigned short *)st.triangles.data;
		printf("pass %d: states %u verts %u pos[5] %.3f %.3f %.3f nrm[5] %.3f %.3f %.3f tri0 %u %u %u\n", pass, n, st.positions.count,
		       p[15], p[16], p[17], nr[15], nr[16], nr[17], tr[0], tr[1], tr[2]);
		GrannyUnlockRenderingState(&st);
		GrannyUnlockRendering(lock[0], lock[1]);
		if (pass == 0) {
			printf("xform %d\n", GrannyTransformModel(mod[0], mod[1], T, M));
			granny_uint32 s2[2]; GrannyOpenSequence(mod[0], mod[1], s2); seq[0] = s2[0]; seq[1] = s2[1];
		}
	}
	if (argc > 2)
	{
		if (GrannyOpenAnimation(ver, argv[2], anim)) { printf("open anim failed\n"); return 1; }
		float ao[3], aa[9]; printf("get anim %d\n", GrannyGetToolCoordinateSystemForAnimation(anim[0], anim[1], ao, aa)); pm("aaxes", aa, 9);
		printf("xform anim %d\n", GrannyTransformAnimation(anim[0], anim[1], T, M));
	}
	printf("bad: %d %d %d\n", GrannyTransformModel(ver, 999, T, M), GrannyTransformModel(ver, mod[1], 0, M), GrannyRecommendTransform(1, to, ta, wo, wa, T, M));
	GrannyClose(ver);
	return 0;
}
