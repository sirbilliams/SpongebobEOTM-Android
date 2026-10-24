/*  granny_compat.cpp - portable reimplementation of the RAD Granny 1.x ("1.2b", 2000) functions used by
    Jimmy Neutron vs. Jimmy Negatron.  Reference: granny.dll's machine code; see docs/granny-replacement.md.

    Arithmetic: granny.dll runs the x87 in 53-bit mode (its CRT sets control word 0x027f), so its
    expressions behave like C double arithmetic, rounded to float wherever the code stores to a float.
    Every routine below follows the order of operations of the corresponding granny.dll function (its
    address is noted) with double temporaries and explicit float stores, which makes the results
    bit-identical (tools/granny/verify.sh).  This file must be compiled without FMA contraction
    (-ffp-contract=off) and with IEEE double (SSE2 / ARM64; not x87).

    The transform arithmetic is in granny_math.h; the .grn files are read by granny_grn.cpp.          */
#include "granny_compat.h"
#include "granny_internal.h"
#include "granny_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

namespace {

using namespace granny_internal;

/* ----------------------------------------------------------------------------------- b-splines --- */

/* 0x1001fa50 / 0x1001fd10 / 0x10020020: evaluate around knot k (pointer to knot i) and control point c */
static void bspline_eval(int degree, float t, const float *k, const float *c, int dim, float *out)
{
	switch (degree)
	{
	case 0:
		for (int i = 0; i < dim; i++) out[i] = c[i];
		return;
	case 1:
	{
		D f = ((D)t - k[-1]) / ((D)k[0] - k[-1]), g = 1.0 - f;
		for (int i = 0; i < dim; i++) out[i] = F(g * c[i - dim] + f * c[i]);
		return;
	}
	case 2:
	{
		float d1 = F((D)t - k[-1]);
		float f3 = F((D)d1 / ((D)k[0] - k[-1]));
		D f2 = ((D)t - k[-2]) / ((D)k[0] - k[-2]);
		float s2 = F(((D)f3 + f2) - f2 * f3);
		D q = (D)d1 / ((D)k[1] - k[-1]) * f3;
		D c1 = (D)s2 - q, c2 = 1.0 - (D)s2;
		for (int i = 0; i < dim; i++)
			out[i] = F((c2 * c[i - 2 * dim] + c1 * c[i - dim]) + q * c[i]);
		return;
	}
	default:
	{
		/* cubic: not used by the game's files; same formula as granny's case 3 */
		D t0 = t, k0 = k[0], km1 = k[-1], km2 = k[-2], km3 = k[-3];
		float a = F(t0 - km1), b = F(t0 - km2);
		float f9 = F(a / (k0 - km1)), f8 = F(b / (k0 - km2)), f4 = F(a / ((D)k[1] - km1));
		float f6 = F((t0 - km3) / (k0 - km3)), f1 = F(b / ((D)k[1] - km2)), f5 = F(a / ((D)k[2] - km1));
		D p3 = (1.0 - f8) * (1.0 - f9), p2 = (D)f4 * f9, c7 = (1.0 - f6) * p3;
		D p4 = (1.0 - f4) * f9 + (1.0 - f9) * f8;
		D c3 = p3 * f6 + (1.0 - f1) * p4, c1 = p4 * f1 + (1.0 - f5) * p2, c0 = p2 * f5;
		for (int i = 0; i < dim; i++)
			out[i] = F(((c0 * c[i] + c1 * c[i - dim]) + c3 * c[i - 2 * dim]) + c7 * c[i - 3 * dim]);
		return;
	}
	}
}

static const int curve_dim[3] = { 3, 4, 9 };

/* bspline_motion::sample (0x1001f8c0) / raw_motion::sample (0x1001f160) */
static bool sample_track(const Track &tr, D t, Local &out)
{
	if (tr.kind == 0)
	{
		size_t n = tr.keys.size() / 17, i = 0;
		if (!n) return false;
		while (i < n && !((D)tr.keys[i * 17] >= t)) i++;
		if (i == n) i--;
		const float *k = &tr.keys[i * 17];
		memcpy(out.pos, k + 1, 12); memcpy(out.q, k + 4, 16); memcpy(out.ss, k + 8, 36);
		return true;
	}
	bool any = false;
	float *dst[3] = { out.pos, out.q, out.ss };
	for (int c = 0; c < 3; c++)
	{
		const Curve &cv = tr.curve[c];
		int n = tr.nknots[cv.knots];
		if (!n) continue;
		const std::vector<float> &K = tr.knots[cv.knots];
		int count = n + 5, i = 3, lim = count - 3;
		if (lim > 3)
			while (i < lim && (D)K[i] < t) i++;
		bspline_eval(cv.degree, F(t), &K[i], &cv.cps[i * curve_dim[c]], curve_dim[c], dst[c]);
		if (c == 1) qnorm_a(out.q);
		any = true;
	}
	return any;
}

/* --------------------------------------------------------------------------------- data files --- */

static std::map<std::string, FileData *> g_files;
static std::string g_datadir;

/* the file the game names: '\' or '/' separated, any case (the game's names do not always match the case
   of the installed files), relative to the current directory or, after GrannyCompatSetDataDirectory, its
   base name in that directory */
static std::string grn_path(const char *file)
{
	std::string s(file);
	for (size_t i = 0; i < s.size(); i++) if (s[i] == '\\') s[i] = '/';
	if (!g_datadir.empty())
	{
		size_t slash = s.rfind('/');
		s = g_datadir + "/" + (slash == std::string::npos ? s : s.substr(slash + 1));
	}
	return grn_find_file(s);
}

static void pad_track(Track &t)
{
	for (int a = 0; a < 3; a++)
	{
		std::vector<float> &k = t.knots[a];
		int n = t.nknots[a];
		if (!n) continue;
		std::vector<float> p(n + 5);
		for (int i = 0; i < n; i++) p[i + 3] = k[i];
		D v = (D)p[3] - 1.0; p[2] = F(v); v -= 1.0; p[1] = F(v); v -= 1.0; p[0] = F(v);
		p[n + 3] = p[n + 2]; p[n + 4] = p[n + 2];
		k.swap(p);
	}
	for (int c = 0; c < 3; c++)
	{
		Curve &cv = t.curve[c];
		int n = t.nknots[cv.knots], dim = curve_dim[c];
		if (!n) continue;
		std::vector<float> p((n + 5) * dim);
		for (int i = 0; i < n * dim; i++) p[3 * dim + i] = cv.cps[i];
		for (int j = 0; j < dim; j++)
		{
			p[j] = p[dim + j] = p[2 * dim + j] = p[3 * dim + j];
			p[(n + 3) * dim + j] = p[(n + 4) * dim + j] = p[(n + 2) * dim + j];
		}
		cv.cps.swap(p);
	}
}

/* files are shared between opens (the DLL reads the file again for every open, with the same result) */
static FileData *load_file(const char *file, granny_result *err)
{
	std::string path = grn_path(file);
	std::map<std::string, FileData *>::iterator it = g_files.find(path);
	if (it != g_files.end()) { it->second->refs++; return it->second; }
	FileData *g = grn_load(path, err, 0);
	if (!g) return 0;
	g->path = path;
	g->refs = 1;
	for (size_t i = 0; i < g->bones.size(); i++) track_offset(g->bones[i].rest, g->bones[i].rest, g->bones[i].offset);
	for (size_t i = 0; i < g->tracks.size(); i++) if (g->tracks[i].kind == 1) pad_track(g->tracks[i]);
	g_files[path] = g;
	return g;
}

static void release_file(FileData *g)
{
	if (g && --g->refs == 0) { g_files.erase(g->path); delete g; }
}

/* --------------------------------------------------------------------------------- the runtime --- */

static const u32 VERSION_HANDLE = 0x82ca5e2a;

struct Model;
struct Sequence;
struct Animation;

struct TextureInst { Model *model; u32 index; granny_cookie cookie; bool delivered; };

/* models and animations are reference counted like in the DLL: closing one that a sequence (model)
   or a control (animation) still uses only invalidates its handle */
struct Model
{
	u32 id;
	FileData *file;
	std::vector<TextureInst *> textures;
	u32 refs;           /* open sequences */
	bool closed;
};

struct Animation { u32 id; FileData *file; u32 refs; bool closed; };

struct Control
{
	u32 id;
	Sequence *seq;
	Animation *anim;
	std::vector<int> track_bone;         /* sequence bone driven by each track (track 0: the root), -1: none */
	D start, speed, loops, blendin, blendout, frozen_local, duration;
	float w_hold, w_before, w_after, priority;
	bool active, released;
};

struct Sequence
{
	u32 id;
	Model *model;
	std::vector<Local> local;
	std::vector<World> world;
	Local placement;
	Sequence *parent;
	u32 parent_bone;
	bool enabled;
	u32 frame;
};

struct Lock
{
	Sequence *seq;
	u32 present;
	u32 next;                                   /* index of the next state */
	std::vector<std::pair<u32, u32> > states;   /* (mesh, state in mesh) */
	bool state_locked;
	std::vector<std::vector<float> > pos, nrm;  /* deformed, per mesh */
	std::vector<bool> done;
};

static bool g_open;
static u32 g_next_id;
static D g_clock;
static u32 g_frame;
static granny_result g_last_error;
static std::vector<Model *> g_models;
static std::vector<Animation *> g_anims;
static std::vector<Sequence *> g_seqs;
static std::vector<Control *> g_controls;       /* the clock's list: creation order */
static std::vector<TextureInst *> g_new_textures;
static Lock *g_lock;
static const float g_identity4[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

static granny_result ret(granny_result r) { g_last_error = r; return r; }

template <class T> static T *find_id(std::vector<T *> &v, u32 id)
{
	for (size_t i = 0; i < v.size(); i++) if (v[i]->id == id) return v[i];
	return 0;
}

static Model *find_model(u32 h0, u32 h1)
{
	Model *m = g_open && h0 == VERSION_HANDLE ? find_id(g_models, h1) : 0;
	return m && !m->closed ? m : 0;
}
static Animation *find_anim(u32 h0, u32 h1)
{
	Animation *a = g_open && h0 == VERSION_HANDLE ? find_id(g_anims, h1) : 0;
	return a && !a->closed ? a : 0;
}
static Sequence *find_seq(u32 h0, u32 h1) { return g_open && h0 == VERSION_HANDLE ? find_id(g_seqs, h1) : 0; }
static Control *find_ctrl(u32 c0, u32 c1, u32 c2)
{
	if (!g_open || c0 != VERSION_HANDLE) return 0;
	Control *c = find_id(g_controls, c2);
	return c && !c->released && c->seq->id == c1 ? c : 0;
}

static const FileData *seq_file(const Sequence *s) { return s->model->file; }

/* total length (0x10022600), end time (0x10022620) */
static D ctrl_total(const Control *c) { return c->duration / c->speed * c->loops; }
static D ctrl_end(const Control *c) { return ctrl_total(c) + c->start; }
static bool ctrl_alive(const Control *c, D t) { return c->active && ctrl_end(c) > t; }

/* weight at global time t (0x100229f0) */
static float ctrl_weight(const Control *c, D t)
{
	D total = ctrl_total(c), out_start = total - c->blendout, in = c->blendin;
	D rel = t - c->start;
	if (rel < 0.0) return c->w_before;
	if (rel < in) return F((rel - 0.0) / (in - 0.0) * ((D)c->w_hold - (D)c->w_before) + (D)c->w_before);
	if (rel < out_start) return c->w_hold;
	if (rel < total) return F((rel - out_start) / (total - out_start) * ((D)c->w_after - (D)c->w_hold) + (D)c->w_hold);
	return c->w_after;
}

/* root motion between two local times (0x10026c60): the root track's change from t0 to t1,
   delta = S(t0)^-1 o S(t1); when t1 runs past the end of the animation the part past the end
   wraps around: delta = (S(0)^-1 o S(t1 - duration)) o S(t0)^-1 o S(duration) */
static bool root_delta(const Control *c, D t0, D t1, Local &out)
{
	const Track &tr = c->anim->file->tracks[0];
	local_identity(out);
	D dur = c->duration, t2 = t1;
	if (t1 > dur)
	{
		D t3 = t1 - dur;
		t2 = dur;
		Local C, Dl;
		local_identity(C);
		local_identity(Dl);
		if (sample_track(tr, 0.0, C) && sample_track(tr, t3, Dl))
		{
			Local Ci;
			root_inverse(C, Ci);
			root_compose(out, Ci, out, false);
			root_compose(out, Dl, out, false);
		}
	}
	Local A, B;
	local_identity(A);
	local_identity(B);
	if (sample_track(tr, t0, A) && sample_track(tr, t2, B))
	{
		Local Ai;
		root_inverse(A, Ai);
		root_compose(out, Ai, out, true);
		root_compose(out, B, out, true);
	}
	return true;
}

/* rst_affector::affect (0x1002aae0) with 0x10026b50: tracks 1.. drive the bones of the same name, in
   track order (a later track for the same bone wins); track 0 moves the root by the root motion
   between the previous and the current local time (granny's default "accumulate" mode, sequence
   flag +0x6d = 1, which the game never changes) */
static void ctrl_affect(Control *c, D local_time, D old_local, D new_local, float w)
{
	Sequence *s = c->seq;
	if (!s->enabled) return;
	const std::vector<Track> &tracks = c->anim->file->tracks;
	for (size_t i = 1; i < tracks.size(); i++)
	{
		int b = c->track_bone[i];
		if (b < 0) continue;
		Local X;
		local_identity(X);
		if (!sample_track(tracks[i], local_time, X)) continue;
		local_compose(X, seq_file(s)->bones[b].offset);
		if (w == 1.0f) s->local[b] = X;
		else blend(s->local[b], w, X);
	}
	if (!tracks.empty() && !s->local.empty())
	{
		Local X;
		if (root_delta(c, old_local, new_local, X))
		{
			local_compose(X, s->local[0]);
			blend(s->local[0], w, X);
		}
	}
}

/* animation_affector::update (0x10022770) */
static void ctrl_update(Control *c, D old_t, D new_t)
{
	if (!c->active) return;
	D dur = c->duration;
	D local = (old_t - c->start) * c->speed;                 /* 0x10022910 */
	if (!(c->loops == 1.0) && local / dur < c->loops)       /* 0x10022930 */
		local = fmod(local, dur);
	/* root motion interval (0x10022980): from the wrapped old local time, unclamped and not frozen */
	D old_local = local, new_local = (new_t - old_t) * c->speed + local;
	if (local > dur) local = dur;
	if (c->speed > 0.0001) c->frozen_local = local;
	else local = c->frozen_local;
	ctrl_affect(c, local, old_local, new_local, ctrl_weight(c, new_t));
}

static void bone_world(Sequence *s, size_t i)
{
	local_to_world(s->local[i], s->world[i]);
	if (i == 0)
	{
		if (s->parent) world_compose(s->world[0], s->parent->world[s->parent_bone]);
	}
	else
		world_compose(s->world[i], s->world[seq_file(s)->bones[i].parent]);
}

/* 0x1001c5d0 */
static void seq_update(Sequence *s)
{
	if (!s->enabled || s->frame == g_frame) return;
	s->frame = g_frame;
	if (s->parent) seq_update(s->parent);
	for (size_t i = 0; i < s->local.size(); i++) bone_world(s, i);
}

static void seq_compute_all(Sequence *s)
{
	for (size_t i = 0; i < s->local.size(); i++) bone_world(s, i);
}

/* decomposed transform of bone i relative to the top of its chain (0x1002a5a0) */
static void flatten(Sequence *s, u32 i, Local &out)
{
	out = s->local[i];
	for (;;)
	{
		Sequence *ps = s;
		int pi;
		if (i == 0) { if (!s->parent) break; ps = s->parent; pi = (int)s->parent_bone; }
		else pi = seq_file(s)->bones[i].parent;
		local_compose(out, ps->local[pi]);
		s = ps; i = (u32)pi;
	}
}

static void seq_detach(Sequence *s)
{
	if (!s->parent) return;
	Local l;
	flatten(s, 0, l);
	s->local[0] = l;
	s->placement = l;
	s->parent = 0;
}

static void free_model(Model *m)
{
	for (size_t k = 0; k < m->textures.size(); k++)
	{
		for (size_t i = 0; i < g_new_textures.size(); i++)
			if (g_new_textures[i] == m->textures[k]) { g_new_textures.erase(g_new_textures.begin() + (long)i); break; }
		delete m->textures[k];
	}
	for (size_t i = 0; i < g_models.size(); i++) if (g_models[i] == m) { g_models.erase(g_models.begin() + (long)i); break; }
	release_file(m->file);
	delete m;
}

static void free_anim(Animation *a)
{
	for (size_t i = 0; i < g_anims.size(); i++) if (g_anims[i] == a) { g_anims.erase(g_anims.begin() + (long)i); break; }
	release_file(a->file);
	delete a;
}

static void destroy_control(Control *c)
{
	for (size_t i = 0; i < g_controls.size(); i++)
		if (g_controls[i] == c) { g_controls.erase(g_controls.begin() + (long)i); break; }
	Animation *a = c->anim;
	delete c;
	if (--a->refs == 0 && a->closed) free_anim(a);
}

/* ---------------------------------------------------------------------------------- skinning --- */

static void skin_mesh(const Sequence *s, const Mesh &m, bool want_pos, bool want_nrm,
                      std::vector<float> &out_pos, std::vector<float> &out_nrm)
{
	size_t nb = m.bindings.size();
	std::vector<float> L(nb * 9), T(nb * 3);
	for (size_t b = 0; b < nb; b++)               /* composite per binding (0x100298e0) */
	{
		const World &w = s->world[m.bindings[b].bone];
		const float *Bm = m.bindings[b].B, *bt = m.bindings[b].bt;
		float *l = &L[b * 9];
		for (int r = 0; r < 3; r++)
		{
			const float *Mr = w.M + r * 3;
			l[r * 3 + 0] = F(((D)Mr[1] * Bm[3] + (D)Bm[6] * Mr[2]) + (D)Mr[0] * Bm[0]);
			l[r * 3 + 1] = F(((D)Bm[7] * Mr[2] + (D)Mr[0] * Bm[1]) + (D)Mr[1] * Bm[4]);
			l[r * 3 + 2] = F(((D)Mr[1] * Bm[5] + (D)Bm[8] * Mr[2]) + (D)Mr[0] * Bm[2]);
		}
		for (int r = 0; r < 3; r++)
			T[b * 3 + r] = F(((D)bt[2] * l[r * 3 + 2] + (D)bt[1] * l[r * 3 + 1]) + (D)bt[0] * l[r * 3 + 0]);
		for (int r = 0; r < 3; r++) T[b * 3 + r] = F((D)w.t[r] + (D)T[b * 3 + r]);
	}
	size_t n = m.nrender;
	if (want_pos)
	{
		out_pos.assign(n * 3, 0.0f);
		if (m.rigid())
		{
			if (nb)                                  /* 0x100292e0 */
				for (size_t v = 0; v < n; v++)
				{
					D x = m.pos[v * 3], y = m.pos[v * 3 + 1], z = m.pos[v * 3 + 2];
					for (int r = 0; r < 3; r++)
						out_pos[v * 3 + r] = F(((x * L[r * 3] + y * L[r * 3 + 1]) + z * L[r * 3 + 2]) + (D)T[r]);
				}
		}
		else
		{
			for (u32 v = 0; v < m.nsource; v++)     /* 0x100291d0 */
			{
				D x = m.pos[v * 3], y = m.pos[v * 3 + 1], z = m.pos[v * 3 + 2], acc[3] = { 0.0, 0.0, 0.0 };
				const u32 *wl = &m.weights[m.wofs[v]];
				for (u32 k = 0; k < wl[0]; k++)
				{
					u32 b = wl[1 + 2 * k];
					float wt;
					memcpy(&wt, &wl[2 + 2 * k], 4);
					if (b >= nb) continue;
					const float *l = &L[b * 9];
					for (int r = 0; r < 3; r++)
						acc[r] = acc[r] + ((((x * l[r * 3] + y * l[r * 3 + 1]) + z * l[r * 3 + 2]) + (D)T[b * 3 + r]) * (D)wt);
				}
				for (int r = 0; r < 3; r++) out_pos[v * 3 + r] = F(acc[r]);
			}
			u32 p = 0;                               /* 0x10029830 */
			for (u32 v = 0; v < m.nsource && p < m.copies.size(); v++)
			{
				u32 cnt = m.copies[p++];
				for (u32 k = 0; k < cnt && p < m.copies.size(); k++)
				{
					u32 dst = v + m.copies[p++];
					if (dst < n) memcpy(&out_pos[dst * 3], &out_pos[v * 3], 12);
				}
			}
		}
	}
	if (want_nrm)
	{
		out_nrm.assign(n * 3, 0.0f);
		if (m.rigid())
		{
			if (nb)                                  /* 0x10029360 */
				for (size_t v = 0; v < n; v++)
				{
					D x = m.nrm[v * 3], y = m.nrm[v * 3 + 1], z = m.nrm[v * 3 + 2];
					for (int r = 0; r < 3; r++) out_nrm[v * 3 + r] = F((x * L[r * 3] + y * L[r * 3 + 1]) + z * L[r * 3 + 2]);
				}
		}
		else
		{
			for (size_t v = 0; v < m.normal_vertex.size() && v < n; v++)   /* 0x10029710 */
			{
				u32 sv = m.normal_vertex[v];
				D x = m.nrm[v * 3], y = m.nrm[v * 3 + 1], z = m.nrm[v * 3 + 2], acc[3] = { 0.0, 0.0, 0.0 };
				if (sv < m.nsource)
				{
					const u32 *wl = &m.weights[m.wofs[sv]];
					for (u32 k = 0; k < wl[0]; k++)
					{
						u32 b = wl[1 + 2 * k];
						float wt;
						memcpy(&wt, &wl[2 + 2 * k], 4);
						if (b >= nb) continue;
						const float *l = &L[b * 9];
						for (int r = 0; r < 3; r++)
							acc[r] = acc[r] + (((x * l[r * 3] + y * l[r * 3 + 1]) + z * l[r * 3 + 2]) * (D)wt);
					}
				}
				for (int r = 0; r < 3; r++) out_nrm[v * 3 + r] = F(acc[r]);
			}
		}
		for (size_t v = 0; v < n; v++)               /* 0x10029880 */
		{
			D x = out_nrm[v * 3], y = out_nrm[v * 3 + 1], z = out_nrm[v * 3 + 2];
			D inv = 1.0 / sqrt((y * y + x * x) + z * z);
			out_nrm[v * 3] = F(inv * x); out_nrm[v * 3 + 1] = F(inv * y); out_nrm[v * 3 + 2] = F(inv * z);
		}
	}
}

static void set_stream(granny_stream &s, u32 present, u32 type, u32 count, const void *data)
{
	s.present = present; s.type = type; s.dim = 3; s.count = count; s.unknown = 0; s.data = present ? data : 0;
}

} /* namespace */

/* ----------------------------------------------------------------------------------- the API --- */

extern "C" {

void GrannyCompatSetDataDirectory(const char *dir) { g_datadir = dir ? dir : ""; }

void GrannyCompatSetDllPath(const char *path) { grn_set_dll_path(path); }

granny_result GrannyGetLastError(void) { return g_last_error; }

granny_result GrannyOpenVersion(const char *version, const char *platform, const char *date,
                                const char *copyright, granny_uint32 *out_version)
{
	(void)platform; (void)date; (void)copyright;
	if (!out_version) return ret(GrannyInvalidPointer);
	if (!version || strcmp(version, "1.2b")) return ret(GrannyMismatchedVersions);
	if (g_open) GrannyClose(VERSION_HANDLE);
	g_open = true;
	g_next_id = VERSION_HANDLE + 1;
	g_clock = 0.0;
	g_frame = 1;
	*out_version = VERSION_HANDLE;
	return ret(GrannyOK);
}

granny_result GrannyAllowTextureFormat(granny_uint32 version, granny_uint32 format)
{
	if (!g_open || version != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	return ret(format == 4 ? GrannyOK : GrannyOperationNotSupported);
}

granny_result GrannyOpenModel(granny_uint32 version, const char *file, granny_uint32 out_model[2])
{
	if (!g_open || version != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (!out_model) return ret(GrannyInvalidPointer);
	out_model[0] = version;     /* the DLL fills in the first word even when the open fails */
	if (!file) return ret(GrannyInvalidString);
	granny_result err = GrannyOK;
	FileData *g = load_file(file, &err);
	if (!g) return ret(err);
	if (!g->has_model) { release_file(g); return ret(GrannyUnableToFindData); }
	Model *m = new Model;
	m->id = g_next_id++;
	m->file = g;
	m->refs = 0;
	m->closed = false;
	for (u32 i = 0; i < g->textures.size(); i++)
	{
		TextureInst *t = new TextureInst;
		t->model = m; t->index = i; t->cookie = 0; t->delivered = false;
		m->textures.push_back(t);
		g_new_textures.push_back(t);
	}
	g_models.push_back(m);
	out_model[0] = VERSION_HANDLE; out_model[1] = m->id;
	return ret(GrannyOK);
}

granny_result GrannyOpenAnimation(granny_uint32 version, const char *file, granny_uint32 out_animation[2])
{
	if (!g_open || version != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (!out_animation) return ret(GrannyInvalidPointer);
	out_animation[0] = version;
	if (!file) return ret(GrannyInvalidString);
	granny_result err = GrannyOK;
	FileData *g = load_file(file, &err);
	if (!g) return ret(err);
	if (!g->has_anim) { release_file(g); return ret(GrannyUnableToFindData); }
	Animation *a = new Animation;
	a->id = g_next_id++;
	a->file = g;
	a->refs = 0;
	a->closed = false;
	g_anims.push_back(a);
	out_animation[0] = VERSION_HANDLE; out_animation[1] = a->id;
	return ret(GrannyOK);
}

granny_result GrannyOpenSequence(granny_uint32 m0, granny_uint32 m1, granny_uint32 out_sequence[2])
{
	if (!g_open || m0 != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (!out_sequence) return ret(GrannyInvalidPointer);
	out_sequence[0] = m0;
	Model *m = find_model(m0, m1);
	if (!m) return ret(GrannyInvalidHandle);
	Sequence *s = new Sequence;
	s->id = g_next_id++;
	s->model = m;
	m->refs++;
	size_t nb = m->file->bones.size();
	s->local.resize(nb);
	s->world.resize(nb);
	for (size_t i = 0; i < nb; i++) s->local[i] = m->file->bones[i].rest;
	local_identity(s->placement);
	s->parent = 0; s->parent_bone = 0; s->enabled = true; s->frame = 0;
	seq_compute_all(s);
	g_seqs.push_back(s);
	out_sequence[0] = VERSION_HANDLE; out_sequence[1] = s->id;
	return ret(GrannyOK);
}

granny_result GrannyAddControlledAnimation(granny_uint32 s0, granny_uint32 s1, granny_uint32 a0, granny_uint32 a1,
                                           granny_uint32 out_control[3])
{
	/* checking order of 0x10006530 / 0x10011220 */
	if (!g_open || s0 != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (a0 != s0) return ret(GrannyMismatchedHandles);
	if (!out_control) return ret(GrannyInvalidPointer);
	out_control[0] = s0;
	Sequence *s = find_seq(s0, s1);
	Animation *a = find_anim(a0, a1);
	if (!s || !a) return ret(GrannyInvalidHandle);
	Control *c = new Control;
	c->id = g_next_id++;
	c->seq = s; c->anim = a;
	a->refs++;
	const FileData *mf = seq_file(s);
	const std::vector<Track> &tracks = a->file->tracks;
	c->track_bone.assign(tracks.size(), -1);
	for (size_t k = 1; k < tracks.size(); k++)
		for (size_t i = 0; i < mf->bones.size(); i++)
			if (tracks[k].name == mf->bones[i].name) { c->track_bone[k] = (int)i; break; }
	if (!tracks.empty()) c->track_bone[0] = 0;
	c->start = g_clock; c->speed = 1.0; c->loops = 1.0; c->blendin = 0.0; c->blendout = 0.0;
	c->frozen_local = 0.0; c->duration = a->file->duration;
	c->w_hold = 1.0f; c->w_before = 0.0f; c->w_after = 0.0f; c->priority = 0.0f;
	c->active = true; c->released = false;
	g_controls.push_back(c);
	out_control[0] = VERSION_HANDLE; out_control[1] = s->id; out_control[2] = c->id;
	return ret(GrannyOK);
}

granny_result GrannySetAnimationParameter(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2,
                                          granny_uint32 param, double v)
{
	Control *c = find_ctrl(c0, c1, c2);
	if (!c) return ret(GrannyInvalidHandle);
	switch (param)
	{
	case 0: c->start = v; break;
	case 1: c->start = (v - ctrl_end(c)) + c->start; break;
	case 2:                                                  /* 0x10011e2e: keep the current phase */
	{
		D old_speed = c->speed, old_start = c->start, old_total = ctrl_total(c), now = g_clock;
		c->speed = v;
		bool rescale = old_total > 1e-05 && old_speed > 1e-05;
		if (v > 1e-05)
		{
			D total = ctrl_total(c);
			if (rescale) c->start = (now - old_start) / old_total * (old_total - total) + old_start;
			else c->start = c->start - ((c->frozen_local / c->speed + c->start) - now);
		}
		break;
	}
	case 3: c->loops = v; break;
	case 4: c->w_hold = (float)v; break;
	case 5: c->w_before = (float)v; break;
	case 6: c->blendin = v; break;
	case 7: c->w_after = (float)v; break;
	case 8: c->blendout = v; break;
	default: return ret(GrannyInvalidIndex);
	}
	return ret(GrannyOK);
}

granny_result GrannyGetAnimationParameter(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2,
                                          granny_uint32 param, double *v)
{
	Control *c = find_ctrl(c0, c1, c2);
	if (!c) return ret(GrannyInvalidHandle);
	if (!v) return ret(GrannyInvalidPointer);
	switch (param)
	{
	case 0: *v = c->start; break;
	case 1: *v = ctrl_end(c); break;
	case 2: *v = c->speed; break;
	case 3: *v = c->loops; break;
	case 4: *v = c->w_hold; break;
	case 5: *v = c->w_before; break;
	case 6: *v = c->blendin; break;
	case 7: *v = c->w_after; break;
	case 8: *v = c->blendout; break;
	default: return ret(GrannyInvalidIndex);
	}
	return ret(GrannyOK);
}

granny_result GrannyHasControlExpired(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2, granny_uint32 *expired)
{
	Control *c = find_ctrl(c0, c1, c2);
	if (!c) return ret(GrannyInvalidHandle);
	if (!expired) return ret(GrannyInvalidPointer);
	*expired = g_clock > ctrl_end(c) ? 1 : 0;
	return ret(GrannyOK);
}

granny_result GrannyRemoveControlledAnimation(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2)
{
	Control *c = find_ctrl(c0, c1, c2);
	if (!c) return ret(GrannyInvalidHandle);
	destroy_control(c);
	return ret(GrannyOK);
}

granny_result GrannyReleaseControlledAnimation(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2)
{
	Control *c = find_ctrl(c0, c1, c2);
	if (!c) return ret(GrannyInvalidHandle);
	c->released = true;          /* plays on, deleted once it is no longer alive */
	return ret(GrannyOK);
}

granny_result GrannyAdvanceTime(granny_uint32 version, double seconds)
{
	if (!g_open || version != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	D old_t = g_clock;
	g_clock = old_t + (D)(float)seconds;                     /* the core takes a float */
	std::vector<Control *> list(g_controls);                 /* 0x1001b830 */
	for (size_t i = 0; i < list.size(); i++)
	{
		Control *c = list[i];
		if (ctrl_alive(c, old_t)) ctrl_update(c, old_t, g_clock);
		if (!ctrl_alive(c, g_clock) && c->released) destroy_control(c);
	}
	g_frame++;                                               /* 0x1001b3c0 */
	for (size_t i = 0; i < g_seqs.size(); i++) seq_update(g_seqs[i]);
	return ret(GrannyOK);
}

granny_result GrannyGetCurrentTime(granny_uint32 version, double *seconds)
{
	if (!g_open || version != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (!seconds) return ret(GrannyInvalidPointer);
	*seconds = g_clock;
	return ret(GrannyOK);
}

granny_result GrannyLockSequenceForRendering(granny_uint32 s0, granny_uint32 s1, granny_uint32 present,
                                             granny_uint32 out_lock[2])
{
	Sequence *s = find_seq(s0, s1);
	if (!s) return ret(GrannyInvalidHandle);
	if (present & ~0x3fu) return ret(GrannyInvalidFlags);
	if (!out_lock) return ret(GrannyInvalidPointer);
	if (g_lock) return ret(GrannyInvalidCall);
	Lock *l = new Lock;
	l->seq = s; l->present = present; l->next = 0; l->state_locked = false;
	const FileData *f = seq_file(s);
	for (u32 m = 0; m < f->meshes.size(); m++)
		for (u32 k = 0; k < f->meshes[m].states.size(); k++) l->states.push_back(std::make_pair(m, k));
	l->pos.resize(f->meshes.size()); l->nrm.resize(f->meshes.size()); l->done.assign(f->meshes.size(), false);
	g_lock = l;
	out_lock[0] = VERSION_HANDLE; out_lock[1] = s->id;
	return ret(GrannyOK);
}

/* a lock handle is (version, sequence id); there is at most one lock at a time (0x10012a70) */
static granny_result check_lock(u32 l0, u32 l1)
{
	if (!find_seq(l0, l1)) return GrannyInvalidHandle;
	if (!g_lock) return GrannyInvalidCall;
	if (g_lock->seq->id != l1) return GrannyMismatchedHandles;
	return GrannyOK;
}

granny_result GrannyGetRenderingStatesLeft(granny_uint32 l0, granny_uint32 l1, granny_uint32 *count)
{
	if (!g_open || l0 != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (!count) return ret(GrannyInvalidPointer);
	granny_result e = check_lock(l0, l1);
	if (e) return ret(e);
	*count = (u32)g_lock->states.size() - g_lock->next;
	return ret(GrannyOK);
}

granny_result GrannyLockNextRenderingState(granny_uint32 l0, granny_uint32 l1, granny_rendering_state *st)
{
	if (!g_open || l0 != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (!st) return ret(GrannyInvalidPointer);
	granny_result e = check_lock(l0, l1);
	if (e) return ret(e);
	Lock *l = g_lock;
	if (l->state_locked || l->next >= l->states.size()) return ret(GrannyInvalidCall);
	u32 mi = l->states[l->next].first, si = l->states[l->next].second;
	l->next++;
	l->state_locked = true;
	const FileData *f = seq_file(l->seq);
	const Mesh &m = f->meshes[mi];
	const MeshState &ms = m.states[si];
	bool want_pos = (l->present & GrannyStreamPositions) != 0, want_nrm = (l->present & GrannyStreamNormals) != 0;
	if (!l->done[mi])
	{
		skin_mesh(l->seq, m, want_pos, want_nrm, l->pos[mi], l->nrm[mi]);
		l->done[mi] = true;
	}
	memset(st, 0, sizeof *st);
	st->lock[0] = l0; st->lock[1] = l1;
	st->sequence[0] = VERSION_HANDLE; st->sequence[1] = l->seq->id;
	st->present = l->present;
	{
		const TextureInst *t = ms.texture < l->seq->model->textures.size() ? l->seq->model->textures[ms.texture] : 0;
		st->texture_pending = t && t->delivered ? 0 : 1;
		st->texture_cookie = t && t->delivered ? t->cookie : 0;
	}
	st->has_transform = 1;
	st->transform = g_identity4;
	st->deformed = 1;
	st->mesh_cookie = 0;
	set_stream(st->positions, want_pos ? 1u : 0u, 6, m.nrender, l->pos[mi].empty() ? 0 : &l->pos[mi][0]);
	set_stream(st->color0, 1, 6, m.nrender, m.col.empty() ? 0 : &m.col[0]);
	set_stream(st->color1, (l->present & GrannyStreamColor1) ? 1u : 0u, 6, m.nrender, m.col.empty() ? 0 : &m.col[0]);
	set_stream(st->texcoords, (l->present & GrannyStreamTexCoords) ? 1u : 0u, 6, m.nrender, m.uv.empty() ? 0 : &m.uv[0]);
	set_stream(st->normals, want_nrm ? 1u : 0u, 6, m.nrender, l->nrm[mi].empty() ? 0 : &l->nrm[mi][0]);
	set_stream(st->triangles, (l->present & GrannyStreamTriangles) ? 1u : 0u, 3, (u32)(ms.tris.size() / 3),
	           ms.tris.empty() ? 0 : &ms.tris[0]);
	st->src_blend = ms.src_blend; st->dst_blend = ms.dst_blend;
	st->internal = l;
	return ret(GrannyOK);
}

granny_result GrannyUnlockRenderingState(granny_rendering_state *st)
{
	if (!st) return ret(GrannyInvalidPointer);
	if (!g_lock || st->internal != g_lock || !g_lock->state_locked) return ret(GrannyInvalidCall);
	g_lock->state_locked = false;
	st->internal = 0;
	return ret(GrannyOK);
}

granny_result GrannyUnlockRendering(granny_uint32 l0, granny_uint32 l1)
{
	if (!g_open || l0 != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	granny_result e = check_lock(l0, l1);           /* 0x10012950 */
	if (e) return ret(e);
	if (g_lock->state_locked) return ret(GrannyInvalidCall);
	delete g_lock;
	g_lock = 0;
	return ret(GrannyOK);
}

granny_result GrannyGetNewTexturesLeft(granny_uint32 version, granny_uint32 *count)
{
	if (!g_open || version != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (!count) return ret(GrannyInvalidPointer);
	*count = (u32)g_new_textures.size();
	return ret(GrannyOK);
}

granny_result GrannyLockNextNewTexture(granny_uint32 version, granny_new_texture *t)
{
	if (!g_open || version != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (!t) return ret(GrannyInvalidPointer);
	if (g_new_textures.empty()) return ret(GrannyInvalidCall);
	TextureInst *ti = g_new_textures.front();
	g_new_textures.erase(g_new_textures.begin());
	const TextureData &d = ti->model->file->textures[ti->index];
	memset(t, 0, sizeof *t);
	t->version = VERSION_HANDLE;
	t->format = d.format; t->width = d.w; t->height = d.h; t->bytes_per_pixel = d.bpp; t->pitch = d.pitch;
	t->size = (u32)d.pixels.size();
	t->pixels = d.pixels.empty() ? 0 : &d.pixels[0];
	t->internal = ti;
	return ret(GrannyOK);
}

granny_result GrannyUnlockNewTexture(granny_new_texture *t, granny_cookie cookie)
{
	if (!t || !t->internal) return ret(GrannyInvalidPointer);
	TextureInst *ti = (TextureInst *)t->internal;
	bool live = false;
	for (size_t i = 0; i < g_models.size() && !live; i++)
		for (size_t k = 0; k < g_models[i]->textures.size(); k++) if (g_models[i]->textures[k] == ti) live = true;
	if (!live) return ret(GrannyInvalidPointer);
	ti->cookie = cookie;
	ti->delivered = true;
	t->internal = 0;
	return ret(GrannyOK);
}

const char *GrannyCompatTextureName(const granny_new_texture *t)
{
	static std::string name;
	name.clear();
	if (!t || !t->internal) return name.c_str();
	const TextureInst *ti = (const TextureInst *)t->internal;
	if (!ti->model || !ti->model->file) return name.c_str();
	std::string b = ti->model->file->path;
	size_t s = b.find_last_of("/\\");
	if (s != std::string::npos) b = b.substr(s + 1);
	size_t d = b.find_last_of('.');
	if (d != std::string::npos) b.resize(d);
	char n[16];
	snprintf(n, sizeof n, "_%u", (unsigned)ti->index);
	name = b + n;
	return name.c_str();
}

granny_result GrannyGetBoneCount(granny_uint32 s0, granny_uint32 s1, granny_uint32 *count)
{
	Sequence *s = find_seq(s0, s1);
	if (!s) return ret(GrannyInvalidHandle);
	if (!count) return ret(GrannyInvalidPointer);
	*count = (u32)s->local.size();
	return ret(GrannyOK);
}

granny_result GrannyGetBoneIndexFromName(granny_uint32 s0, granny_uint32 s1, const char *name, granny_uint32 *index)
{
	Sequence *s = find_seq(s0, s1);
	if (!s) return ret(GrannyInvalidHandle);
	if (!index) return ret(GrannyInvalidPointer);
	*index = 0;
	if (!name) return ret(GrannyInvalidString);
	const FileData *f = seq_file(s);
	for (u32 i = 0; i < f->bones.size(); i++)
		if (f->bones[i].name == name) { *index = i; return ret(GrannyOK); }
	return ret(GrannyInvalidString);
}

granny_result GrannyGetBoneState(granny_uint32 s0, granny_uint32 s1, granny_uint32 bone, granny_uint32 present,
                                 granny_transform_state *st)
{
	Sequence *s = find_seq(s0, s1);
	if (!s) return ret(GrannyInvalidHandle);
	if (present & ~0x1ffu) return ret(GrannyInvalidFlags);
	if (bone >= s->local.size()) return ret(GrannyInvalidIndex);
	if (!st) return ret(GrannyInvalidPointer);
	memset(st, 0, sizeof *st);
	st->sequence[0] = s0; st->sequence[1] = s1; st->bone = bone; st->present = present;
	st->parent = bone ? (u32)seq_file(s)->bones[bone].parent : 0;
	/* the DLL (0x100166c0) reports the local transform and writes its 3x3 matrices transposed
	   (column-major); combined = rotation * scale/shear (0x100183e0) */
	const Local &l = s->local[bone];
	float R[9], C[9];
	rotmat(l.q, R);
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			C[r * 3 + c] = F(((D)R[r * 3] * l.ss[c] + (D)R[r * 3 + 1] * l.ss[3 + c]) + (D)R[r * 3 + 2] * l.ss[6 + c]);
	memcpy(st->position, l.pos, 12);
	memcpy(st->quaternion, l.q, 16);
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
		{
			st->rotation[c * 3 + r] = R[r * 3 + c];
			st->scale_shear[c * 3 + r] = l.ss[r * 3 + c];
			st->combined[c * 3 + r] = C[r * 3 + c];
		}
	return ret(GrannyOK);
}

granny_result GrannyAttachSequence(granny_uint32 s0, granny_uint32 s1, granny_uint32 bone,
                                   granny_uint32 p0, granny_uint32 p1, granny_uint32 parent_bone,
                                   granny_uint32 dof0, granny_uint32 dof1)
{
	/* checking order of 0x10009fb0 / 0x10010bd0: an attached child is detached before the bone
	   indices and the attachment itself are checked, so a failed re-attach leaves it detached.
	   The degrees of freedom are only range-checked: the game always passes 0x1ff, 0x1ff */
	if (!g_open || s0 != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	if (p0 != s0) return ret(GrannyMismatchedHandles);
	if ((dof0 | dof1) & ~0x1ffu) return ret(GrannyInvalidFlags);
	Sequence *s = find_seq(s0, s1);
	Sequence *p = find_seq(p0, p1);
	if (!s || !p) return ret(GrannyInvalidHandle);
	if (s->parent) seq_detach(s);
	if (bone >= s->local.size() || parent_bone >= p->local.size()) return ret(GrannyInvalidIndex);
	for (Sequence *q = p; q; q = q->parent) if (q == s) return ret(GrannyOperationFailure);
	/* 0x1001c140: the root becomes the inverse of the attach bone's transform within the sequence */
	Local inv;
	if (bone == 0)
	{
		local_identity(inv);
		inv.pos[0] = inv.pos[1] = inv.pos[2] = -0.0f;
		inv.q[0] = inv.q[1] = inv.q[2] = -0.0f;
	}
	else
	{
		local_identity(s->local[0]);
		Local l;
		flatten(s, bone, l);
		local_inverse(l, inv);
	}
	s->local[0] = inv;
	s->placement = inv;
	s->parent = p;
	s->parent_bone = parent_bone;
	return ret(GrannyOK);
}

granny_result GrannyDetachSequence(granny_uint32 s0, granny_uint32 s1)
{
	Sequence *s = find_seq(s0, s1);
	if (!s) return ret(GrannyInvalidHandle);
	if (!s->parent) return ret(GrannyInvalidCall);
	seq_detach(s);
	return ret(GrannyOK);
}

granny_result GrannyCloseSequence(granny_uint32 s0, granny_uint32 s1)
{
	Sequence *s = find_seq(s0, s1);
	if (!s) return ret(GrannyInvalidHandle);
	if (g_lock && g_lock->seq == s) return ret(GrannyInvalidCall);
	for (size_t i = 0; i < g_seqs.size(); i++) if (g_seqs[i]->parent == s) seq_detach(g_seqs[i]);
	for (size_t i = g_controls.size(); i-- > 0;) if (g_controls[i]->seq == s) destroy_control(g_controls[i]);
	for (size_t i = 0; i < g_seqs.size(); i++) if (g_seqs[i] == s) { g_seqs.erase(g_seqs.begin() + (long)i); break; }
	Model *m = s->model;
	delete s;
	if (--m->refs == 0 && m->closed) free_model(m);
	return ret(GrannyOK);
}

granny_result GrannyCloseAnimation(granny_uint32 a0, granny_uint32 a1)
{
	Animation *a = find_anim(a0, a1);
	if (!a) return ret(GrannyInvalidHandle);
	a->closed = true;                   /* live controls keep playing it */
	if (a->refs == 0) free_anim(a);
	return ret(GrannyOK);
}

granny_result GrannyCloseModel(granny_uint32 m0, granny_uint32 m1)
{
	Model *m = find_model(m0, m1);
	if (!m) return ret(GrannyInvalidHandle);
	m->closed = true;                   /* open sequences (and their textures) stay usable */
	if (m->refs == 0) free_model(m);
	return ret(GrannyOK);
}

granny_result GrannyClose(granny_uint32 version)
{
	if (!g_open || version != VERSION_HANDLE) return ret(GrannyInvalidHandle);
	delete g_lock; g_lock = 0;
	while (!g_controls.empty()) destroy_control(g_controls.back());
	while (!g_seqs.empty()) { delete g_seqs.back(); g_seqs.pop_back(); }
	while (!g_anims.empty()) { release_file(g_anims.back()->file); delete g_anims.back(); g_anims.pop_back(); }
	while (!g_models.empty())
	{
		Model *m = g_models.back();
		for (size_t k = 0; k < m->textures.size(); k++) delete m->textures[k];
		release_file(m->file); delete m; g_models.pop_back();
	}
	g_new_textures.clear();
	g_open = false;
	return ret(GrannyOK);
}

} /* extern "C" */
