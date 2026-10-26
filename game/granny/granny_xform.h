/*  granny_xform.h - the coordinate-system transforms of granny.dll (RAD Granny 1.2b): GrannyRecommendTransform
    (0x100053a0 -> 0x100137f0), GrannyTransformModel (0x10004ee0 -> 0x10013460) and GrannyTransformAnimation
    (0x10005140 -> 0x10013670 -> 0x10027320).  sb2.exe calls them at 0x403810 (models) and 0x4048xx (animations) to turn the
    art tool's units and axes (3D Studio MAX: 39.37 units per metre, Z up) into the game's.
    Same rules as granny_math.h: double arithmetic, float stores where the DLL stores to a float.      */
#ifndef GRANNY_XFORM_H
#define GRANNY_XFORM_H

#include "granny_math.h"

namespace granny_internal {

/* granny's "version" object holds two matrix-order flags (0x240 and 0x244), set to 1 by GrannyOpenVersion
   (0x10015284); the game imports nothing that changes them.  1 = the 3x3 matrices that cross the API are
   transposed relative to the row-major ones used inside (0x244 for the matrix RecommendTransform returns, 0x240
   for the ones handed in). */
static const int XFORM_IN_TRANSPOSED = 1;
static const int XFORM_OUT_TRANSPOSED = 1;

/* a 3x3 argument as the DLL copies it in (0x10013460 etc.): transposed when the flag says so */
inline void xform_in(const float *src, float m[9])
{
	float t[9];
	memcpy(t, src, sizeof t);
	if (XFORM_IN_TRANSPOSED == 1) m3_transpose(t, m); else memcpy(m, t, sizeof t);
}

/* GrannyRecommendTransform (0x100137f0): the transform taking the tool's coordinate system (origin, axes) onto
   the wanted one.  out_t = wanted origin - M * tool origin; M = wanted axes * inverse(tool axes). */
inline void xform_recommend(const float tool_origin[3], const float tool_axes[9],
                            const float want_origin[3], const float want_axes[9],
                            float out_t[3], float out_m[9])
{
	float A[9], B[9], inv[9], M[9], v[3];
	memcpy(A, tool_axes, sizeof A);
	memcpy(B, want_axes, sizeof B);
	if (XFORM_IN_TRANSPOSED == 1) { m3_transpose(A, A); m3_transpose(B, B); }
	m3_inverse(A, inv);                                   /* 0x100182b0 */
	m3_mul(B, inv, M);                                    /* the loop at 0x10013946 */
	m3_vec(M, tool_origin, v);                            /* 0x10018470 */
	float t[3];
	for (int i = 0; i < 3; i++) t[i] = F((D)want_origin[i] - (D)v[i]);   /* 0x100184d0 */
	memcpy(out_t, t, sizeof t);
	if (XFORM_OUT_TRANSPOSED == 0) memcpy(out_m, M, sizeof M);          /* 0x10016070 */
	else m3_transpose(M, out_m);
}

/* the three operations of 0x10032170 on one decomposed transform / control point */

/* 0x10031ca0: position := M * position + T */
inline void xform_pos(const float T[3], const float M[9], float p[3])
{
	float tmp[3];
	m3_vec(M, p, tmp);
	for (int i = 0; i < 3; i++) p[i] = F((D)tmp[i] + (D)T[i]);
}

/* 0x100321b0: quaternion (x y z w) of a rotation matrix (trace branch or largest diagonal), all in double */
inline void xform_mat_to_quat(const float m[9], float q[4])
{
	float r[4] = { 0, 0, 0, 0 };
	D t = 0.0;
	for (int i = 0; i < 3; i++) t += m[i * 4];
	if (t > 0.0)
	{
		D s = sqrt(t + 1.0);
		r[3] = F(0.5 * s);
		D inv = 0.5 / s;
		r[0] = F(((D)m[7] - m[5]) * inv);
		r[1] = F(((D)m[2] - m[6]) * inv);
		r[2] = F(((D)m[3] - m[1]) * inv);
	}
	else
	{
		u32 i = 0;
		if (m[4] > m[0]) i = 1;
		if (m[8] > m[i * 4]) i = 2;
		u32 j = (i + 1) % 3, k = (i + 2) % 3;
		D s = sqrt(((D)m[i * 4] - ((D)m[k * 4] + m[j * 4])) + 1.0);
		r[i] = F(0.5 * s);
		D inv = 0.5 / s;
		r[j] = F(((D)m[i * 3 + j] + m[j * 3 + i]) * inv);
		r[k] = F(((D)m[i + k * 3] + m[i * 3 + k]) * inv);
		r[3] = F(((D)m[k * 3 + j] - m[j * 3 + k]) * inv);
	}
	memcpy(q, r, sizeof r);
}

/* 0x10031da0: quaternion q := quaternion of (M * R(q/|q|) * M^-1), scaled back by |q| */
inline void xform_quat(const float M[9], const float Minv[9], float q[4])
{
	float e[4] = { q[0], q[1], q[2], q[3] };
	float norm = F(sqrt((((D)e[0] * e[0] + (D)e[1] * e[1]) + (D)e[2] * e[2]) + (D)e[3] * e[3]));   /* 0x10019530 */
	for (int i = 0; i < 4; i++) e[i] = F((1.0 / (D)norm) * (D)e[i]);
	float R[9], T1[9], T2[9];
	rotmat(e, R);
	m3_mul(M, R, T1);
	m3_mul(T1, Minv, T2);
	xform_mat_to_quat(T2, e);
	for (int i = 0; i < 4; i++) e[i] = F((D)norm * (D)e[i]);
	memcpy(q, e, sizeof e);
}

/* 0x10031ff0: scale/shear := M * ss * M^-1 */
inline void xform_ss(const float M[9], const float Minv[9], float ss[9])
{
	float T1[9];
	m3_mul(M, ss, T1);
	m3_mul(T1, Minv, ss);
}

/* 0x10032170 (a bone's local transform, a raw key) */
inline void xform_local(const float T[3], const float M[9], const float Minv[9], float pos[3], float q[4], float ss[9])
{
	xform_pos(T, M, pos);
	xform_quat(M, Minv, q);
	xform_ss(M, Minv, ss);
}

/* GrannyTransformModel, one mesh (0x100293e0).  Positions and normals are the mesh's render vertices (3 floats each). */
inline void xform_mesh(Mesh &m, const float T[3], const float M[9])
{
	float Minv[9], NT[9];
	mat_inverse(M, Minv);                                  /* 0x100293e0's inline inverse */
	m3_transpose(Minv, NT);
	for (size_t b = 0; b < m.bindings.size(); b++)         /* the 0x7c-byte bindings: bt := M bt, B := M B M^-1 */
	{
		Binding &bd = m.bindings[b];
		float v[3], t1[9];
		m3_vec(M, bd.bt, v);
		memcpy(bd.bt, v, sizeof v);
		m3_mul(M, bd.B, t1);
		m3_mul(t1, Minv, bd.B);
	}
	for (size_t i = 0; i + 3 <= m.pos.size(); i += 3)      /* position := M p + T (0x10029600) */
	{
		float v[3];
		m3_vec(M, &m.pos[i], v);
		for (int k = 0; k < 3; k++) m.pos[i + k] = F((D)v[k] + (D)T[k]);
	}
	for (size_t i = 0; i + 3 <= m.nrm.size(); i += 3)      /* normal := M^-T n (not renormalised) */
	{
		float v[3];
		m3_vec(NT, &m.nrm[i], v);
		memcpy(&m.nrm[i], v, sizeof v);
	}
	D det = ((D)M[4] * M[8] - (D)M[7] * M[5]) * M[0] - ((D)M[3] * M[8] - (D)M[6] * M[5]) * M[1]
	      + ((D)M[7] * M[3] - (D)M[6] * M[4]) * M[2];      /* 0x10018fd0 */
	if (det < 0.0)                                         /* a mirroring transform: flip the winding */
		for (size_t s = 0; s < m.states.size(); s++)
		{
			std::vector<u16> &t = m.states[s].tris;
			for (size_t i = 0; i + 2 <= t.size(); i += 3) { u16 a = t[i]; t[i] = t[i + 1]; t[i + 1] = a; }
		}
}

/* GrannyTransformModel's bones (0x1002a600): each bone's local transform */
inline void xform_bone(Local &l, const float T[3], const float M[9])
{
	float Minv[9];
	mat_inverse(M, Minv);
	xform_local(T, M, Minv, l.pos, l.q, l.ss);
}

/* GrannyTransformAnimation, one track (0x100204a0 b-spline, 0x1001f220 raw keys) */
inline void xform_track(Track &t, const float T[3], const float M[9])
{
	float Minv[9];
	mat_inverse(M, Minv);
	if (t.kind == 1)
	{
		std::vector<float> &p = t.curve[0].cps, &q = t.curve[1].cps, &s = t.curve[2].cps;
		for (size_t i = 0; i + 3 <= p.size(); i += 3) xform_pos(T, M, &p[i]);
		D prev[4] = { 0, 0, 0, 0 };
		for (size_t i = 0; i + 4 <= q.size(); i += 4)
		{
			xform_quat(M, Minv, &q[i]);
			/* keep neighbouring orientations in the same hemisphere */
			if (((prev[1] * q[i + 1] + prev[2] * q[i + 2]) + prev[3] * q[i + 3]) + prev[0] * q[i] < 0.0)
				for (int k = 0; k < 4; k++) q[i + k] = -q[i + k];
			for (int k = 0; k < 4; k++) prev[k] = q[i + k];
		}
		for (size_t i = 0; i + 9 <= s.size(); i += 9) xform_ss(M, Minv, &s[i]);
	}
	else
		for (size_t i = 0; i + 17 <= t.keys.size(); i += 17) xform_local(T, M, Minv, &t.keys[i + 1], &t.keys[i + 4], &t.keys[i + 8]);
}

}  /* namespace granny_internal */

#endif
