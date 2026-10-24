/*  granny_compat.h - portable reimplementation of the part of RAD Granny 1.x ("1.2b", granny.dll, 2000)
    that Jimmy Neutron vs. Jimmy Negatron uses.  See docs/granny-replacement.md.

    Same function names, argument lists and handle conventions as granny.dll (handles are 32-bit values
    passed by value as 1-3 dwords), minus the __stdcall decoration.  Differences forced by 64-bit:
      - texture cookies are uintptr_t (the game passes an OMedia3DMaterial*),
      - the out-structures (granny_rendering_state, granny_new_texture) hold native pointers, so the
        game reads them by field name instead of by the 32-bit byte offsets (the offsets are noted).
    The game's .grn files are read directly (granny_grn.cpp); the texture decoder needs the player's granny.dll
    as a data file (it is not loaded or run).                                                           */
#ifndef GRANNY_COMPAT_H
#define GRANNY_COMPAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t granny_uint32;
typedef int32_t granny_result;        /* 0 = GrannyOK */
typedef uintptr_t granny_cookie;

enum
{
	GrannyOK = 0, GrannyUnknownError, GrannyOutOfHandles, GrannyOutOfMemory, GrannyInvalidCall,
	GrannyInvalidPointer, GrannyInvalidHandle, GrannyInvalidIndex, GrannyInvalidFlags, GrannyInvalidString,
	GrannyMismatchedHandles, GrannyMismatchedVersions, GrannyNoTextureFormats, GrannyUnableToReadFromFile,
	GrannyUnableToParseFile, GrannyUnableToFindData, GrannyUnableToWriteToFile, GrannyOperationNotSupported,
	GrannyOperationFailure
};

/* GrannySetAnimationParameter / GrannyGetAnimationParameter ids */
enum
{
	GrannyParamStartTime = 0,        /* global time the control started (set at Add = current time)        */
	GrannyParamEndTime = 1,          /* start + duration * loops / speed (setting it moves the start)        */
	GrannyParamSpeed = 2,            /* local time = (now - start) * speed; keeps the current loop phase     */
	GrannyParamLoopCount = 3,        /* 1 = play once; local time wraps (fmod) while < loops * duration      */
	GrannyParamWeight = 4,           /* weight while fully blended in (default 1)                             */
	GrannyParamWeightBeforeStart = 5,/* weight before start / at the start of blend-in (default 0)           */
	GrannyParamBlendInTime = 6,      /* seconds of linear ramp from param 5 to param 4 after the start       */
	GrannyParamWeightAfterEnd = 7,   /* weight at the end of blend-out and after (default 0)                 */
	GrannyParamBlendOutTime = 8      /* seconds of linear ramp from param 4 to param 7 before the end        */
};

/* LockSequenceForRendering 'present' bits (the game uses 0x3b at load, 0x11 per frame) */
enum
{
	GrannyStreamPositions = 0x01, GrannyStreamColor0 = 0x02, GrannyStreamColor1 = 0x04,
	GrannyStreamTexCoords = 0x08, GrannyStreamNormals = 0x10, GrannyStreamTriangles = 0x20
};

typedef struct granny_stream
{
	granny_uint32 present;   /* +0x00  1 if filled                                        */
	granny_uint32 type;      /* +0x04  6 = float32, 3 = uint16                            */
	granny_uint32 dim;       /* +0x08  components per element (3)                         */
	granny_uint32 count;     /* +0x0c  elements (triangles for the index stream)          */
	granny_uint32 unknown;   /* +0x10  always 0                                           */
	const void *data;        /* +0x14                                                     */
} granny_stream;

typedef struct granny_rendering_state
{
	granny_uint32 lock[2];          /* +0x00 */
	granny_uint32 sequence[2];      /* +0x08 */
	granny_uint32 present;          /* +0x10 bits asked for */
	granny_uint32 texture_pending;  /* +0x14 1 while the texture has not been delivered (no cookie yet) */
	granny_cookie texture_cookie;   /* +0x18 cookie given to GrannyUnlockNewTexture, 0 if none yet */
	granny_uint32 has_transform;    /* +0x1c */
	const float *transform;         /* +0x20 4x4 (identity for skinned meshes) */
	granny_uint32 deformed;         /* +0x24 1: vertices are in world (sequence) space */
	granny_uint32 mesh_cookie;      /* +0x28 always 0 (GrannyLockNextNewMesh is not supported) */
	granny_stream positions;        /* +0x2c float[count][3] */
	granny_stream color0;           /* +0x44 float[count][3] */
	granny_stream color1;           /* +0x5c float[count][3] (same data as color0) */
	granny_stream texcoords;        /* +0x74 float[count][3] (u, v, w) */
	granny_stream normals;          /* +0x8c float[count][3] */
	granny_stream triangles;        /* +0xa4 uint16[count][3] */
	granny_uint32 src_blend;        /* +0xbc 1 (opaque) or 6 (alpha) - D3DBLEND values */
	granny_uint32 dst_blend;        /* +0xc0 0 (opaque) or 7 (alpha) */
	granny_uint32 reserved[2];      /* +0xc4 */
	void *internal;                 /* not in the original */
} granny_rendering_state;

typedef struct granny_new_texture
{
	granny_uint32 version;          /* +0x00 */
	granny_uint32 reserved[3];      /* +0x04 internal pointers in the original */
	granny_uint32 format;           /* +0x10 4 = 24-bit RGB */
	granny_uint32 width;            /* +0x14 */
	granny_uint32 height;           /* +0x18 */
	granny_uint32 bytes_per_pixel;  /* +0x1c */
	granny_uint32 pitch;            /* +0x20 */
	granny_uint32 size;             /* +0x24 */
	const unsigned char *pixels;    /* +0x28 */
	void *internal;                 /* not in the original */
} granny_new_texture;

/* transform state (GrannyGetBoneState, a debugging aid: the game does not call it).  With present 0x1ff it
   reports the bone's local transform; the 3x3 matrices are written column-major, as by the DLL */
typedef struct granny_transform_state
{
	granny_uint32 sequence[2], bone, present, reserved[3], parent;
	float position[3], rotation[9], quaternion[4], scale_shear[9], combined[9];
} granny_transform_state;

/* the 26 functions the game imports */
granny_result GrannyOpenVersion(const char *version, const char *platform, const char *date,
                                const char *copyright, granny_uint32 *out_version);
granny_result GrannyAllowTextureFormat(granny_uint32 version, granny_uint32 format);
granny_result GrannyOpenModel(granny_uint32 version, const char *file, granny_uint32 out_model[2]);
granny_result GrannyOpenSequence(granny_uint32 m0, granny_uint32 m1, granny_uint32 out_sequence[2]);
granny_result GrannyOpenAnimation(granny_uint32 version, const char *file, granny_uint32 out_animation[2]);
granny_result GrannyAddControlledAnimation(granny_uint32 s0, granny_uint32 s1, granny_uint32 a0, granny_uint32 a1,
                                           granny_uint32 out_control[3]);
granny_result GrannySetAnimationParameter(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2,
                                          granny_uint32 param, double value);
granny_result GrannyHasControlExpired(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2, granny_uint32 *expired);
granny_result GrannyRemoveControlledAnimation(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2);
granny_result GrannyReleaseControlledAnimation(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2);
granny_result GrannyAdvanceTime(granny_uint32 version, double seconds);
granny_result GrannyLockSequenceForRendering(granny_uint32 s0, granny_uint32 s1, granny_uint32 present,
                                             granny_uint32 out_lock[2]);
granny_result GrannyGetRenderingStatesLeft(granny_uint32 l0, granny_uint32 l1, granny_uint32 *count);
granny_result GrannyLockNextRenderingState(granny_uint32 l0, granny_uint32 l1, granny_rendering_state *state);
granny_result GrannyUnlockRenderingState(granny_rendering_state *state);
granny_result GrannyUnlockRendering(granny_uint32 l0, granny_uint32 l1);
granny_result GrannyGetNewTexturesLeft(granny_uint32 version, granny_uint32 *count);
granny_result GrannyLockNextNewTexture(granny_uint32 version, granny_new_texture *texture);
granny_result GrannyUnlockNewTexture(granny_new_texture *texture, granny_cookie cookie);
granny_result GrannyGetBoneIndexFromName(granny_uint32 s0, granny_uint32 s1, const char *name, granny_uint32 *index);
granny_result GrannyAttachSequence(granny_uint32 s0, granny_uint32 s1, granny_uint32 bone,
                                   granny_uint32 p0, granny_uint32 p1, granny_uint32 parent_bone,
                                   granny_uint32 dof0, granny_uint32 dof1);
granny_result GrannyDetachSequence(granny_uint32 s0, granny_uint32 s1);
granny_result GrannyCloseSequence(granny_uint32 s0, granny_uint32 s1);
granny_result GrannyCloseAnimation(granny_uint32 a0, granny_uint32 a1);
granny_result GrannyCloseModel(granny_uint32 m0, granny_uint32 m1);
granny_result GrannyClose(granny_uint32 version);

/* extras (exported by granny.dll too; used by the tests) */
granny_result GrannyGetAnimationParameter(granny_uint32 c0, granny_uint32 c1, granny_uint32 c2,
                                          granny_uint32 param, double *value);
granny_result GrannyGetCurrentTime(granny_uint32 version, double *seconds);
granny_result GrannyGetBoneCount(granny_uint32 s0, granny_uint32 s1, granny_uint32 *count);
granny_result GrannyGetBoneState(granny_uint32 s0, granny_uint32 s1, granny_uint32 bone, granny_uint32 present,
                                 granny_transform_state *state);
granny_result GrannyGetLastError(void);

/* not in granny.dll: where the .grn files are looked up (default: the path given, relative to the current
   directory; names are matched without regard to case) */
void GrannyCompatSetDataDirectory(const char *dir);
/* not in granny.dll: the player's granny.dll, whose Bink tables the texture decoder reads (default: next to the
   directory of the first .grn file opened, then in it, then in the current directory).  Call before opening. */
void GrannyCompatSetDllPath(const char *path);
/* not in granny.dll (port: HD texture pack): "<model>_<n>" for a texture between GrannyLockNextNewTexture and
   GrannyUnlockNewTexture: <model> = the .grn file name without directory and extension, n = the texture's
   number in that file.  Empty when unknown.  The string lives until the next call. */
const char *GrannyCompatTextureName(const granny_new_texture *texture);

#ifdef __cplusplus
}
#endif
#endif
