/*  granny_math.h - granny.dll's transform arithmetic (RAD Granny 1.2b), shared by the runtime
    (granny_compat.cpp) and the loader (granny_grn.cpp).  granny.dll runs the x87 in 53-bit mode (its CRT sets
    control word 0x027f), so its expressions behave like C double arithmetic, rounded to float wherever the code
    stores to a float.  Every routine follows the order of operations of the granny.dll function noted, with
    double temporaries and explicit float stores.  Compile without FMA contraction (-ffp-contract=off) and
    with IEEE double (SSE2 / ARM64; not x87).  See docs/granny-replacement.md.                            */
#ifndef GRANNY_MATH_H
#define GRANNY_MATH_H

#include "granny_internal.h"

#include <math.h>
#include <string.h>

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

namespace granny_internal {

inline void local_identity(Local &l)
{
	memset(&l, 0, sizeof l);
	l.q[3] = 1.0f;
	l.ss[0] = l.ss[4] = l.ss[8] = 1.0f;
}

inline float F(D v) { return (float)v; }

/* the normalisation used after sampling, in 0x100220b0 and at the end of 0x10021910:
   one Newton step towards unit length, sum taken as ((x*x + y*y) + z*z) + w*w */
inline void qnorm_a(float q[4])
{
	D x = q[0], y = q[1], z = q[2], w = q[3];
	D s = (3.0 - (((x * x + y * y) + z * z) + w * w)) * 0.5;
	q[0] = F(s * x); q[1] = F(s * y); q[2] = F(s * z); q[3] = F(s * w);
}

/* rotation matrix of a quaternion (0x100185f0; same code inline in 0x100220b0 / 0x10021b90) */
inline void rotmat(const float q[4], float m[9])
{
	D x = q[0], y = q[1], z = q[2], w = q[3];
	D xx = x * x, yy = y * y, zz = z * z, xy = y * x, zw = w * z;
	float XZ = F(z * x), YZ = F(z * y), XW = F(w * x), YW = F(w * y);
	m[0] = F(1.0 - (zz + yy) * 2.0);
	m[1] = F((xy - zw) * 2.0);
	m[2] = F(((D)YW + (D)XZ) * 2.0);
	m[3] = F((zw + xy) * 2.0);
	m[4] = F(1.0 - (zz + xx) * 2.0);
	m[5] = F(((D)YZ - (D)XW) * 2.0);
	m[6] = F(((D)XZ - (D)YW) * 2.0);
	m[7] = F(((D)XW + (D)YZ) * 2.0);
	m[8] = F(1.0 - (yy + xx) * 2.0);
}

/* local -> composed form (0x100220b0): q normalised, M = R(q) * ss, t = pos */
inline void local_to_world(const Local &l, World &w)
{
	float q[4] = { l.q[0], l.q[1], l.q[2], l.q[3] }, R[9];
	qnorm_a(q);
	memcpy(w.q, q, sizeof q);
	{
		/* 0x100220b0 computes the rotation with the same sequence as rotmat but takes x*z as z*x
		   etc.; products commute, so rotmat is exact here */
		D x = q[0], y = q[1], z = q[2], ww = q[3];
		D xx = x * x, yy = y * y, zz = z * z, xy = y * x, zw = ww * z;
		float XZ = F(x * z), YZ = F(y * z), XW = F(x * ww), YW = F(y * ww);
		R[0] = F(1.0 - (zz + yy) * 2.0);
		R[1] = F((xy - zw) * 2.0);
		R[2] = F(((D)YW + (D)XZ) * 2.0);
		R[3] = F((zw + xy) * 2.0);
		R[4] = F(1.0 - (zz + xx) * 2.0);
		R[5] = F(((D)YZ - (D)XW) * 2.0);
		R[6] = F(((D)XZ - (D)YW) * 2.0);
		R[7] = F(((D)XW + (D)YZ) * 2.0);
		R[8] = F(1.0 - (yy + xx) * 2.0);
	}
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			w.M[r * 3 + c] = F(((D)l.ss[c] * R[r * 3 + 0] + (D)l.ss[3 + c] * R[r * 3 + 1]) + (D)l.ss[6 + c] * R[r * 3 + 2]);
	w.t[0] = l.pos[0]; w.t[1] = l.pos[1]; w.t[2] = l.pos[2];
}

/* child := parent o child in composed form (0x100217b0) */
inline void world_compose(World &c, const World &p)
{
	D cx = c.q[0], cy = c.q[1], cz = c.q[2], cw = c.q[3];
	D px = p.q[0], py = p.q[1], pz = p.q[2], pw = p.q[3];
	c.q[0] = F(((cz * py - cy * pz) + cw * px) + cx * pw);
	c.q[1] = F(((cx * pz - cz * px) + cw * py) + cy * pw);
	c.q[2] = F(((cy * px - cx * py) + cw * pz) + cz * pw);
	c.q[3] = F(cw * pw - ((cz * pz + cy * py) + cx * px));
	D tx = c.t[0], ty = c.t[1], tz = c.t[2];
	float t[3];
	for (int r = 0; r < 3; r++)
		t[r] = F((tz * p.M[r * 3 + 2] + ty * p.M[r * 3 + 1]) + tx * p.M[r * 3 + 0]);
	for (int r = 0; r < 3; r++) c.t[r] = F((D)p.t[r] + (D)t[r]);
	float m[9];
	const float *C = c.M;
	for (int r = 0; r < 3; r++)
	{
		const float *P = p.M + r * 3;
		m[r * 3 + 0] = F(((D)P[1] * C[3] + (D)C[6] * P[2]) + (D)P[0] * C[0]);
		m[r * 3 + 1] = F(((D)C[7] * P[2] + (D)P[1] * C[4]) + (D)P[0] * C[1]);
		m[r * 3 + 2] = F(((D)P[1] * C[5] + (D)C[2] * P[0]) + (D)C[8] * P[2]);
	}
	memcpy(c.M, m, sizeof m);
}

/* out = A * B (0x10021e50) */
inline void mat_mul_e50(const float A[9], const float B[9], float out[9])
{
	float o[9];
	for (int r = 0; r < 3; r++)
	{
		const float *a = A + r * 3;
		o[r * 3 + 0] = F(((D)a[0] * B[0] + (D)B[3] * a[1]) + (D)B[6] * a[2]);
		o[r * 3 + 1] = F(((D)a[0] * B[1] + (D)a[2] * B[7]) + (D)a[1] * B[4]);
		o[r * 3 + 2] = F(((D)B[8] * a[2] + (D)B[5] * a[1]) + (D)B[2] * a[0]);
	}
	memcpy(out, o, sizeof o);
}

/* out = A^T * B (0x10021ec0) */
inline void mat_tmul_ec0(const float A[9], const float B[9], float out[9])
{
	float o[9];
	for (int r = 0; r < 3; r++)
	{
		D a0 = A[r], a1 = A[3 + r], a2 = A[6 + r];
		o[r * 3 + 0] = F((a0 * B[0] + (D)B[3] * a1) + (D)B[6] * a2);
		o[r * 3 + 1] = F((a0 * B[1] + a2 * B[7]) + a1 * B[4]);
		o[r * 3 + 2] = F((a2 * B[8] + (D)B[5] * a1) + (D)B[2] * a0);
	}
	memcpy(out, o, sizeof o);
}

/* v = M v (0x10021f30) */
inline void mat_vec_f30(const float M[9], float v[3])
{
	D x = v[0], y = v[1], z = v[2];
	for (int r = 0; r < 3; r++) v[r] = F((z * M[r * 3 + 2] + y * M[r * 3 + 1]) + x * M[r * 3 + 0]);
}

/* A := A o B in decomposed form, B acting as the parent (0x10021b90; used with the per-bone offset of
   a control, with a sequence's placement for the root, and to flatten a bone chain) */
inline void local_compose(Local &A, const Local &B)
{
	D ax = A.q[0], ay = A.q[1], az = A.q[2], aw = A.q[3];
	D bx = B.q[0], by = B.q[1], bz = B.q[2], bw = B.q[3];
	float x = F(((az * by - ay * bz) + ax * bw) + aw * bx);
	float y = F(((ax * bz - az * bx) + ay * bw) + aw * by);
	float z = F(((ay * bx - ax * by) + az * bw) + aw * bz);
	D w = aw * bw - ((ax * bx + az * bz) + ay * by);
	D s = (3.0 - (((w * w + (D)z * z) + (D)y * y) + (D)x * x)) * 0.5;
	A.q[0] = F((D)x * s); A.q[1] = F((D)y * s); A.q[2] = F((D)z * s); A.q[3] = F(w * s);
	float T1[9], T2[9], X[9];
	rotmat(B.q, T1);
	rotmat(A.q, T2);
	const float *S = A.ss;
	for (int r = 0; r < 3; r++)
	{
		const float *t = T2 + r * 3;
		X[r * 3 + 0] = F(((D)t[1] * S[3] + (D)S[6] * t[2]) + (D)t[0] * S[0]);
		X[r * 3 + 1] = F(((D)t[1] * S[4] + (D)t[2] * S[7]) + (D)t[0] * S[1]);
		X[r * 3 + 2] = F(((D)t[1] * S[5] + (D)S[2] * t[0]) + (D)S[8] * t[2]);
	}
	mat_mul_e50(B.ss, X, A.ss);
	mat_tmul_ec0(T2, A.ss, A.ss);
	mat_vec_f30(B.ss, A.pos);
	mat_vec_f30(T1, A.pos);
	for (int i = 0; i < 3; i++) A.pos[i] = F((D)B.pos[i] + (D)A.pos[i]);
}

/* A := blend(A, B, w): lerp position and scale/shear, slerp rotation (0x10021910) */
inline void blend(Local &A, float w, const Local &B)
{
	float omw = F(1.0 - (D)w);
	for (int i = 0; i < 3; i++) A.pos[i] = F((D)F((D)A.pos[i] * omw) + (D)F((D)B.pos[i] * w));
	float ca = omw, cb = w, sign = 1.0f;
	float d = F(((((D)A.q[0] * B.q[0] + (D)A.q[1] * B.q[1]) + (D)A.q[2] * B.q[2]) + (D)A.q[3] * B.q[3]));
	if (d < 0.0f) { sign = -1.0f; d = -d; }
	if (1.0 - (D)d > (D)9.99999974737875e-05f)
	{
		D th = acos((D)d), inv = 1.0 / sin(th);
		ca = F(sin(th * ca) * inv * sign);
		cb = F(sin(th * cb) * inv);
	}
	else
		ca = F((D)sign * ca);
	float q[4];
	for (int i = 0; i < 4; i++) q[i] = F((D)F((D)B.q[i] * cb) + (D)F((D)A.q[i] * ca));
	for (int i = 0; i < 9; i++) A.ss[i] = F((D)w * B.ss[i] + (D)omw * A.ss[i]);
	memcpy(A.q, q, sizeof q);
	qnorm_a(A.q);
}

/* ------------------------------------------------------------------ root motion (0x10026c60) --- */

/* out = A * B, sums in index order (0x100183e0) */
inline void m3_mul(const float A[9], const float B[9], float out[9])
{
	float o[9];
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			o[r * 3 + c] = F(((D)A[r * 3] * B[c] + (D)A[r * 3 + 1] * B[3 + c]) + (D)A[r * 3 + 2] * B[6 + c]);
	memcpy(out, o, sizeof o);
}

inline void m3_transpose(const float m[9], float out[9])     /* 0x10018260 */
{
	float o[9];
	for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) o[c * 3 + r] = m[r * 3 + c];
	memcpy(out, o, sizeof o);
}

/* out = M v, sums in index order (0x10018470) */
inline void m3_vec(const float M[9], const float v[3], float out[3])
{
	float o[3];
	for (int r = 0; r < 3; r++) o[r] = F(((D)v[0] * M[r * 3] + (D)v[1] * M[r * 3 + 1]) + (D)v[2] * M[r * 3 + 2]);
	memcpy(out, o, sizeof o);
}

/* 3x3 inverse by cofactors (0x100182b0); a singular matrix is returned unchanged */
inline void m3_inverse(const float m[9], float out[9])
{
	D c0 = (D)m[4] * m[8] - (D)m[7] * m[5];
	float c1 = F((D)m[3] * m[8] - (D)m[6] * m[5]);
	float c2 = F((D)m[7] * m[3] - (D)m[6] * m[4]);
	D det = (c0 * m[0] - (D)c1 * m[1]) + (D)c2 * m[2];
	if (det == 0.0) { memcpy(out, m, 36); return; }
	D id = 1.0 / det;
	float r[9];
	r[0] = F(c0 * id);
	r[3] = F(-((D)c1 * id));
	r[6] = F((D)c2 * id);
	r[1] = F(-(((D)m[1] * m[8] - (D)m[7] * m[2]) * id));
	r[4] = F(((D)m[0] * m[8] - (D)m[6] * m[2]) * id);
	r[7] = F(-(((D)m[7] * m[0] - (D)m[6] * m[1]) * id));
	r[2] = F(((D)m[1] * m[5] - (D)m[2] * m[4]) * id);
	r[5] = F(-(((D)m[0] * m[5] - (D)m[3] * m[2]) * id));
	r[8] = F(((D)m[4] * m[0] - (D)m[3] * m[1]) * id);
	memcpy(out, r, sizeof r);
}

/* inverse of a decomposed transform as 0x10026c60 computes it inline:
   q' = conj q, ss' = R(q')^T ss^-1 R(q'), pos' = R(q') ss' (-pos) */
inline void root_inverse(const Local &a, Local &o)
{
	float cq[4] = { -a.q[0], -a.q[1], -a.q[2], a.q[3] };
	float Rc[9], Rct[9], inv[9], M[9], neg[3], v[3];
	rotmat(cq, Rc);
	m3_inverse(a.ss, inv);
	m3_transpose(Rc, Rct);
	m3_mul(Rct, inv, M);
	m3_mul(M, Rc, M);
	for (int i = 0; i < 3; i++) neg[i] = -a.pos[i];
	m3_vec(M, neg, v);
	m3_vec(Rc, v, o.pos);
	memcpy(o.q, cq, sizeof cq);
	memcpy(o.ss, M, sizeof M);
}

/* out = X o Y (Y applied first) in decomposed form.  round_ww selects 0x100280c0, which rounds
   wX*wY to float before subtracting the dot product; the inline copy in 0x10026c60 (quaternion
   product 0x100186d0) does not */
inline void root_compose(const Local &X, const Local &Y, Local &out, bool round_ww)
{
	const float *vx = X.q, *vy = Y.q;
	float wx = X.q[3], wy = Y.q[3];
	float p1[3], p2[3], c[3], q[4];
	for (int i = 0; i < 3; i++) { p1[i] = F((D)wy * vx[i]); p2[i] = F((D)wx * vy[i]); }
	c[0] = F((D)vy[2] * vx[1] - (D)vx[2] * vy[1]);
	c[1] = F((D)vx[2] * vy[0] - (D)vy[2] * vx[0]);
	c[2] = F((D)vy[1] * vx[0] - (D)vy[0] * vx[1]);
	for (int i = 0; i < 3; i++) q[i] = F((D)F((D)c[i] + p2[i]) + p1[i]);
	D dot = ((D)vx[0] * vy[0] + (D)vx[1] * vy[1]) + (D)vx[2] * vy[2];
	q[3] = round_ww ? F((D)F((D)wy * wx) - dot) : F((D)wy * wx - dot);
	float RX[9], RY[9], RYt[9], S[9], v[3], pos[3];
	rotmat(X.q, RX);
	rotmat(Y.q, RY);
	m3_transpose(RY, RYt);
	m3_mul(RYt, X.ss, S);
	m3_mul(S, RY, S);
	m3_mul(S, Y.ss, S);
	m3_vec(X.ss, Y.pos, v);
	m3_vec(RX, v, v);
	for (int i = 0; i < 3; i++) pos[i] = F((D)v[i] + X.pos[i]);
	memcpy(out.pos, pos, sizeof pos);
	memcpy(out.q, q, sizeof q);
	memcpy(out.ss, S, sizeof S);
}

/* the per-track offset of a control (0x10031080 called from 0x1002ac50): B o A^-1 where granny looks
   both A and B up by the track's name in the same table, the sequence's model skeleton - so it is the
   bone's rest transform composed with its own inverse: an identity up to rounding noise (positions
   ~1e-6, scale/shear ~1e-7) that granny composes onto every sample of every track 1.. */
inline void track_offset(const Local &b, const Local &a, Local &out)
{
	Local inv;
	root_inverse(a, inv);
	root_compose(b, inv, out, false);
}

/* inverse of a 3x3 by cofactors (as 0x10024853 / 0x1002a600) - only used for attaching by a child bone
   other than 0, which the game never does */
inline void mat_inverse(const float m[9], float o[9])
{
	D det = ((D)m[4] * m[8] - (D)m[7] * m[5]) * m[0] - ((D)m[3] * m[8] - (D)m[6] * m[5]) * m[1]
	      + ((D)m[7] * m[3] - (D)m[6] * m[4]) * m[2];
	if (det == 0.0) { memcpy(o, m, 36); return; }
	D id = 1.0 / det;
	float r[9];
	r[0] = F(((D)m[8] * m[4] - (D)m[5] * m[7]) * id);
	r[3] = F(-(((D)m[8] * m[3] - (D)m[6] * m[5]) * id));
	r[6] = F(((D)m[7] * m[3] - (D)m[6] * m[4]) * id);
	r[1] = F(-(((D)m[8] * m[1] - (D)m[7] * m[2]) * id));
	r[4] = F(((D)m[8] * m[0] - (D)m[2] * m[6]) * id);
	r[7] = F(-(((D)m[7] * m[0] - (D)m[1] * m[6]) * id));
	r[2] = F(((D)m[5] * m[1] - (D)m[2] * m[4]) * id);
	r[5] = F(-(((D)m[5] * m[0] - (D)m[2] * m[3]) * id));
	r[8] = F(((D)m[0] * m[4] - (D)m[1] * m[3]) * id);
	memcpy(o, r, sizeof r);
}

/* inverse of a decomposed transform (approximately what 0x10021f70 + 0x1001c140 compute) */
inline void local_inverse(const Local &a, Local &o)
{
	World w;
	local_to_world(a, w);
	float Mi[9];
	mat_inverse(w.M, Mi);
	o.q[0] = -w.q[0]; o.q[1] = -w.q[1]; o.q[2] = -w.q[2]; o.q[3] = w.q[3];
	float R[9], Rt[9];
	rotmat(o.q, R);
	for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) Rt[r * 3 + c] = R[c * 3 + r];
	/* ss = R(o.q)^T * M^-1 */
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			o.ss[r * 3 + c] = F((D)Rt[r * 3] * Mi[c] + (D)Rt[r * 3 + 1] * Mi[3 + c] + (D)Rt[r * 3 + 2] * Mi[6 + c]);
	for (int r = 0; r < 3; r++)
		o.pos[r] = F(-((D)Mi[r * 3] * w.t[0] + (D)Mi[r * 3 + 1] * w.t[1] + (D)Mi[r * 3 + 2] * w.t[2]));
}

}  /* namespace granny_internal */

#endif
