/*  granny_internal.h - shared between the parts of the granny.dll replacement (not part of the API).
    granny_compat.cpp: the runtime (the 26 functions), granny_grn.cpp: reading .grn files and building what
    granny.dll builds at load time, granny_bink.cpp: the Bink texture decoder, granny_math.h: the DLL's
    transform arithmetic.  See docs/granny-replacement.md.                                                  */
#ifndef GRANNY_INTERNAL_H
#define GRANNY_INTERNAL_H

#include "granny_compat.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace granny_internal {

typedef uint32_t u32;
typedef uint16_t u16;
typedef double D;

/* little-endian reads (the file formats are little-endian; no alignment or byte-order assumptions) */
inline u32 rd32(const unsigned char *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
inline float rdf32(const unsigned char *p)
{
	union { u32 u; float f; } x;
	x.u = rd32(p);
	return x.f;
}

struct Local            /* granny's decomposed transform (16 floats, bone + 0xac) */
{
	float pos[3];
	float q[4];         /* x y z w */
	float ss[9];        /* scale/shear, row-major */
};

struct World            /* granny's composed transform (16 floats, bone + 0xec) */
{
	float q[4];
	float t[3];
	float M[9];         /* = R(q) * scale/shear, row-major, column vectors */
};

/* ------------------------------------------------------------------------------- loaded file --- */

struct Curve
{
	int degree;
	int knots;                    /* knot array used */
	std::vector<float> cps;       /* padded: 3 copies of the first, n points, 2 copies of the last */
};

struct Track
{
	std::string name;
	int kind;                     /* 1 = b-spline, 0 = raw keys */
	std::vector<float> knots[3];  /* padded: k0-3, k0-2, k0-1, n knots, last, last */
	int nknots[3];
	Curve curve[3];               /* position (3), orientation (4), scale/shear (9) */
	std::vector<float> keys;      /* raw: n x { time, pos[3], quat[4], ss[9] } */
};

struct Binding { u32 bone; float bt[3]; float B[9]; };

struct MeshState { u32 texture; u32 src_blend, dst_blend; std::vector<u16> tris; };

struct Mesh
{
	std::vector<Binding> bindings;
	u32 nrender;
	std::vector<float> pos, nrm, uv, col;   /* render order, 3 floats each */
	u32 nsource;
	std::vector<u32> weights;     /* per source vertex: n, n x { binding, f32 bits } */
	std::vector<u32> wofs;        /* offset of each source vertex in weights */
	std::vector<u32> copies;      /* per source vertex: n, n x relative render index */
	std::vector<u32> normal_vertex;
	std::vector<MeshState> states;
	bool rigid() const { return nsource == 0 && bindings.size() < 2; }
};

struct TextureData { u32 w, h, format, bpp, pitch; std::vector<unsigned char> pixels; };

struct BoneDef
{
	std::string name;
	int parent;
	Local rest;
	Local offset;       /* rest o rest^-1 as granny computes it (granny_compat.cpp, track_offset) */
};

struct FileData          /* one opened file (shared by all opens of the same path) */
{
	std::string path;
	int refs;
	bool has_model, has_anim;
	std::vector<BoneDef> bones;
	std::vector<TextureData> textures;
	std::vector<Mesh> meshes;
	D duration;
	std::vector<Track> tracks;    /* knots and control points as in the file (padded by the runtime) */
};

/* --------------------------------------------------------------------------- Bink (granny_bink) --- */

/* tables of granny.dll's Bink decoder, read from the player's granny.dll (granny_grn.cpp) */
struct BinkTables
{
	bool ok;
	const unsigned char *huff[16];  /* per tree: (1 << huff_bits) entries of (code length << 4) | symbol */
	int huff_bits[16];
	int rle_lens[4];
	const unsigned char *patterns;  /* 16 x 64 */
	const unsigned char *scan;      /* 64 */
	const int32_t *intra_quant;     /* 16 x 64, raster order */
};

/* the one key frame of a Bink 'i' file -> 24-bit RGB; false if the data is not something it handles */
bool bink_decode_rgb24(const unsigned char *data, size_t size, const BinkTables &tab, u32 *w, u32 *h,
                       std::vector<unsigned char> &rgb);

/* ------------------------------------------------------------------------------- .grn (granny_grn) --- */

/* the Bink tables from granny.dll (loaded once, looked for near the file 'near' unless grn_set_dll_path
   named it; NULL with the reason in *why if not available) */
const BinkTables *grn_bink_tables(const std::string &near, std::string *why);
void grn_set_dll_path(const char *path);

/* the path with each component matched without regard to case (unchanged if it exists as given) */
std::string grn_find_file(const std::string &path);

/* read a .grn file and build what granny.dll builds from it at GrannyOpenModel / GrannyOpenAnimation.
   Returns 0 and sets *err on failure.  'warn' collects notes about data the DLL's behaviour is not
   known for (none for the game's files). */
FileData *grn_load(const std::string &path, granny_result *err, std::vector<std::string> *warn);

}  /* namespace granny_internal */

#endif
