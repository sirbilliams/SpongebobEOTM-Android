# Granny 1.2b replacement (`game/granny/`)

A portable reimplementation of the 26 RAD Granny functions that `Neutron2.exe` imports from
`granny.dll` ("1.2b", 2000). It reads the game's `.grn` files directly and builds from them what the DLL
builds at load time (decoded textures, render meshes, skeletons, animation tracks), so nothing has to be
converted and no DLL has to run. The API (names, arguments, handles, error codes, call order) is the DLL's;
see [granny-api.md](granny-api.md) for how the game calls it.

| | |
|---|---|
| runtime | `game/granny/granny_compat.h`, `granny_compat.cpp` (the 26 functions), `granny_math.h` (the DLL's transform arithmetic) |
| loader | `granny_grn.cpp` (.grn files, render meshes, bindings, the DLL's Bink tables), `granny_bink.cpp` (the DLL's Bink texture decoder), `granny_internal.h` |
| build | C++98, no dependencies, CMake target `granny_compat`; x86-64 and arm64, no byte-order, alignment or pointer-size assumptions |
| verification | `tools/granny/verify.sh`: `grnextract.c` + `grncheck.cpp` (load time), `grntest.c` / `grntest.py` / `tests/*.txt` (run time) |

## Status

Every output compared with the real DLL is **bit-identical**, on x86-64 (gcc) and on arm64 (gcc and clang,
run under qemu-aarch64 with glibc), on all 389 `.grn` files of the game:

- **Load time** (`grncheck` against `grnextract`'s dump of the DLL's internal objects): 389 files, 85 decoded
  textures (10 039 296 pixel bytes) + 2 image-less ones, 96 render meshes (24 442 render vertices with
  positions, normals, texture coordinates and colours, 18 245 triangles, 879 bone bindings with their
  inverse rest transforms, weight lists, copy lists, normal-to-vertex maps), the rendering states' textures
  and blend modes: 4 975 compared items, 0 differences.
- **Run time**: `verify.sh` runs 15 scripts on the DLL (Wine) and on the replacement (the sweep alone
  compares 5 449 records with 567 000 skinned vertices) with max error 0: skinned positions and normals,
  texture coordinates, colours, triangle lists, transforms, texture pixels, bone states, animation
  parameters, error codes and which handle words are filled in. Covered:
  - every one of the 389 `.grn` files opened as a model, rendered in its bind pose, and played (looping) on
    its base model (`sweep`); several characters and props side by side (`multi`);
  - one animation at the game's frame rate, the game's own call pattern (lock `0x3b` at load, `0x11` per
    frame, blend-in/out, release, prop attached by bone name) (`basic`, `game`);
  - two and more animations at once with blend weights, end-time moves, release/remove, upper/lower body
    layering (`blend2`); speed changes, a frozen control, finite loop counts, all weight parameters (`speed`);
  - animations on models they were not made for (`cross`); b-spline + keyframe tracks (`rawmotion`);
  - attach, detach, re-attach (`attach3`); textures queued per model instance (`textures1`);
  - 22 100 frames of a looping walk (12 minutes): granny's root-motion accumulation drifts the root's scale
    by +0.042 % over that time and the replacement drifts identically (`drift`);
  - error paths, stale handles, the lock state machine, closing objects that are still in use (`errors`,
    `handles`, `close2`, `notex`).

  The one expected difference is `notex` (4 records, see below).

The only operations whose results could differ between C libraries are `acos`/`sin` in the slerp of blended
rotations and `sqrt`/`fmod` (glibc on x86-64 and arm64 give identical results; Android's bionic was not
available here); everything else is plain IEEE arithmetic or integer code.

What is deliberately different:

- **Handle ids.** The DLL draws ids from a counter advanced by a pseudo-random step that depends on heap
  addresses (`0x10011220`: `*0x1006e244 += table[(ptr >> 2) & 7]`), so they cannot be reproduced; the
  replacement counts up from the version handle. Which words of a handle are set (also after a failed
  open) matches. The version handle is the DLL's, `0x82ca5e2a`.
- **Use after close.** The DLL keeps answering `HasControlExpired`/`GetAnimationParameter` for a control
  whose sequence was closed by reading freed memory; the replacement returns `GrannyInvalidHandle`.
- **Materials without an image** (only in two animation files that carry meshes, `seagullmove.grn`,
  `hermanAttack2.grn`): the DLL queues a format-0 "new texture" with uninitialised width/height, and its
  states' blend modes come from an uninitialised byte (`0x10013003` tests a stack slot that the texture
  lookup at `0x10012b6a` may or may not have filled): seen as 6/7 after other models were loaded and as 1/0
  in a fresh process. The replacement reports width = height = 0 and blend modes 6/7, so `notex` (the file
  opened first) differs in these 4 values.
- Not implemented (the game never does it): attaching by a child bone other than 0 (an approximate
  inverse is used), degrees of freedom other than `0x1ff`, texture formats other than 4, Bink blocks that
  refer to a previous frame (a texture is one key frame; none of the game's textures has one).

## Files

The runtime maps a file name passed to `GrannyOpenModel`/`GrannyOpenAnimation` (`\` or `/`, any case: each
path component is matched without regard to case, as the game's names do not always match the installed
files) to the path relative to the current directory - the game passes `grn\<name>.grn` and runs in the game
folder - or to `<dir>/<base name>` after `GrannyCompatSetDataDirectory(dir)`. Files are shared between opens.

The texture decoder needs the Bink tables of the player's own `granny.dll`, which is read as a data file (it
is never loaded or run; the bytes are not in this source tree): from `GrannyCompatSetDllPath(path)`, else
`<directory of the first .grn file>/../granny.dll` (the installed layout: `grn/` next to `granny.dll`), then
that directory, then the current directory. The tables are checked by crc32: Huffman lookup tables
`0x1005f848` (16 tables, addresses at `0x1006eda0`, index widths `0x1005fc20`), run lengths `0x1005fc30`,
run-block patterns `0x1005f440`, DCT scan order `0x1006f05c`, intra quantisers `0x10060018`. Without a usable
DLL the models load with image-less textures (a message is printed once).

## Reading a .grn file (what granny.dll builds at GrannyOpenModel)

Layout and node tags: see the comments in `granny_grn.cpp` (and `tools/granny/grnfile.py`). The model is the
first `0c00`; its skeleton (`0c08`, 1-based) gives the bones (`0506`: parent, position, quaternion,
scale/shear), named by the tool nodes of `0c02`. Animation tracks (`1204`) are named by their tool node.

**Textures.** One "new texture" per texture map (`0e00`) whose material exists; its image is the texture
(`0d03`, 1-based) of that material: `0301 > 0303 { w, h, format, Bink file }`. The Bink file is revision `i`,
one key frame; granny.dll decodes it with the Bink library linked into it (BinkOpen `0x1003c440`, BinkDoFrame
`0x1003db40` with the plane decoder `0x10043480`, BinkCopyToBuffer `0x1003d590`): Huffman-coded bundles of
block types, colours, patterns, runs and DCs read a block row ahead, 8x8 and scaled 16x16 blocks (run,
intra DCT, fill, pattern, raw), the integer IDCT of `0x10049b30` (dequantise `c * q >> 11`, rows rounded
`(x + 0x7f) >> 8`, stored as bytes without clamping). The chroma planes follow at the offset in the frame's
first word (V, then U). RGB24 comes from the MMX blitter `0x1004a7e0` with the tables of `0x10046880`:
`Y' = (sat(Y - 16) * 4 * 0x4a85) >> 16`, `R = Y' + Vr[V]`, `G = Y' + Ug[U] + Vg[V]`, `B = Y' + Ub[U]`,
clamped, chroma by nearest sample, with `Vr = (V-128)*0xcc4b/32768`, `Ug = (128-U)*0x3225/32768`,
`Vg = (128-V)*0x680f/32768`, `Ub = (U-128)*0x10235/32768` (C division). Bytes R, G, B, rows top down.

**Render meshes**, one per mesh of the model (`0c03`, in the model's order) with one rendering state each:
- Render vertices: the first ones are the file's vertices (positions) in order, each taking the normal and
  texture-layer values of the first triangle corner that uses it; a later corner of the same position with a
  different normal or layer value (compared as floats, so NaN normals never match) reuses an earlier
  duplicate with the same values or appends a new render vertex. Triangles in file order, corners in order;
  the state's triangle list is the corners' render vertices. A position no triangle uses stays all zero.
- The texture map of a mesh (`0e02`: mesh index) lists per triangle one index per corner and layer (`0e06`)
  into the uv layers (`0804 > 0803`) named by `0e04`. With one layer it gives the texture coordinates; with
  two the first is the vertex colour and the second the texture coordinates. Otherwise the colour is the
  material's (`0d02`) or white.
- Skinned meshes (`0702`): the weight list is the file's, per position `{n, n x {binding, weight}}`; the
  copy list gives each position's duplicates (render index - position); each render normal uses the weights
  of its position.
- Bindings (`0c0a`, built at `0x100247a3..0x1002494e`): the bone's composed rest transform `{q, t, M}`
  (computed like a sequence's bind pose), with `t` negated (`bt`) and `M` inverted by cofactors in place
  (determinant `0x10018fd0` and `1/det` in double; a singular `M` is kept).
- The state uses texture 0 (blend modes 1/0; 6/7 for an image-less one, see above).

## Semantics (from `granny.dll`'s machine code)

**Arithmetic.** granny.dll's CRT sets the x87 control word to `0x027f` (53-bit precision), so its
expressions are C `double` arithmetic rounded to `float` where the code stores a float. Each routine in
`granny_compat.cpp` names the DLL function it follows and keeps its operation order, temporaries and
rounding points. Compile with `-ffp-contract=off` and SSE2/ARM64 doubles (the CMake target does).

**Transforms.** A bone's local transform is decomposed: position, quaternion, 3×3 scale/shear `S`
(meaning `x -> pos + R(q) S x`). World (composed) form: quaternion, translation, `M = R(q) S` (`0x100220b0`,
after one Newton normalisation step of q), composed with the parent's (`0x100217b0`).

**Time.** `GrannyAdvanceTime` adds `(double)(float)seconds` to the global clock. Then, for each control
in creation order: if it is alive at the old time (`active && end > old`) it is applied; a released
control that is not alive at the new time is deleted. Then every sequence (parent first) recomputes its
world transforms.

**Controls.** Defaults at `Add`: start = now, speed 1, loops 1, weights: before 0, hold 1, after 0, blend
times 0. `total = duration / speed * loops`, `end = start + total`; `HasControlExpired` = `now > end`.
Parameters (0–8, the ids in `granny_compat.h`): 0 start, 1 end (moves the start), 2 speed (rescales the
start so the current phase is kept; `0x10011e2e`), 3 loop count, 4/5/7 weights (stored as float), 6/8
blend-in/out seconds. Weight at the new time: before start → p5; ramp p5→p4 over the blend-in; p4; ramp
p4→p7 over the last blend-out seconds; p7 after the end.
Applying a control samples at the local time of the **old** clock: `local = (old - start) * speed`;
if `loops != 1` and `local/duration < loops` it wraps with `fmod`; clamped to the duration; frozen while
speed ≤ 1e-4.

**Tracks.** Tracks 1.. drive the bone of the same name (a later track wins). Sampling: b-splines of
degree 0–3 over knot arrays padded like granny's (3 knots before, 2 after; first control point ×3, last
×2), knot search from index 3, quaternion normalised once; keyframe tracks take the first key with
time ≥ t. The sample is composed with a per-bone offset — `rest ∘ rest⁻¹` computed the DLL's way
(`0x10031080` looks both transforms up in the same table), an identity up to rounding noise
(~1e-6 in position, ~1e-7 in scale) that is part of every pose — then written (weight 1) or blended
(`0x10021910`: lerp position and scale/shear, slerp rotation above 1e-4).

**Root motion.** Track 0 does not set the root: granny's default mode (sequence flag `+0x6d` = 1, never
changed by the game) moves the root by the track's change between the previous and the current local
time, `delta = S(t0)⁻¹ ∘ S(t1)` (`0x10026c60`; with a wrap-around part when `t1` passes the duration), and
`root = blend(root, w, root ∘ delta)`. With the game's identity root tracks this is rounding noise that
accumulates frame after frame, exactly as in the DLL.

**Skinning** (`0x100298e0`, `0x100291d0`, `0x10029710`, `0x10029880`): per binding `L = M_bone · B`,
`T = L · bt + t_bone`; positions per source vertex as a weighted sum accumulated in double and rounded once,
then copied to their duplicate render vertices; normals per render vertex with their source vertex's
weights, then normalised. Rigid meshes (no weight list, < 2 bindings) use binding 0. Deformed vertices are
in sequence space, so the state's transform is the identity.

**Rendering locks.** One lock at a time; the lock handle is (version, sequence id). Bits: 1 positions,
2 colour0 (always returned), 4 colour1, 8 texture coordinates, 0x10 normals, 0x20 triangles. A state's
texture cookie is 0 and `texture_pending` 1 until its texture was delivered through
`GrannyUnlockNewTexture`. Each `GrannyOpenModel` queues its own textures, even for the same file twice.

**Attach.** `GrannyAttachSequence(child, 0, parent, bone, 0x1ff, 0x1ff)`: an attached child is first
detached; the child's root becomes the placement `(-0, quat(-0,-0,-0,1), I)` under the parent bone
(effective at the next `AdvanceTime`). Detach flattens the chain into the root.

**Lifetimes.** Closing a model only invalidates its handle while sequences still use it (they render on,
textures included); closing an animation lets its live controls play on. Closing a sequence detaches its
children and deletes its controls.

## For `C3DGrannyObj`

- Include `granny_compat.h`, link `granny_compat`. Call the functions exactly as the original does;
  handles stay 32-bit words (`granny_uint32[2]` / `[3]`) and can live in the same object fields.
- `g_granny` is opened in `CAweReal::CAweReal` (`game/src/AweReal.cpp`, done).
- Read rendering states and new textures through the structs' **field names**
  (`granny_rendering_state`, `granny_new_texture`), not the 32-bit offsets — pointers are 64-bit here. The
  texture cookie is a `granny_cookie` (`uintptr_t`): pass the `OMedia3DMaterial*`.
- Per frame (`0x41e2cb`): lock with `0x11`, and for each state copy `positions` and `normals`
  (`count` × 3 floats) into the OMT shape's 16-byte vertices as `(-y, z, x)`; `0x3b` at load
  (`0x41d7d3`, `0x41dcb2`) also yields texture coordinates and triangles.
- Starting an animation (`0x41d5df`): `Add`, then param 2 = the object's speed (float at `+0x4f0`),
  param 3 = 9999 (looping) or 1, params 6 / 8 = blend time or 0.
- Pass the file names as the game does (`grn\<name>.grn`, relative to the game folder); `granny.dll` must be
  in the game folder (the importer installs it) for the textures.

## Verifying

```sh
tools/granny/verify.sh <game>/grn [work dir] [test...]
```

Builds `grnextract.exe` and `grntest.exe` (DLL, Wine: `i686-w64-mingw32-gcc`, a 32-bit prefix in
`WINEPREFIX`) and the native `grncheck` and `grntest`, then
1. dumps what granny.dll builds from every `.grn` file (`grnextract`, a `.gxtr`: decoded textures, the bind-pose
   rendering states, and the internal mesh object of each state, reached from the version object
   `*(*(0x100721fc) + 0x10)`: `mesh = *((V[0x194] & ~3) + 0x0c)` while a state is locked) and compares it
   with what `granny_grn.cpp` builds, bit for bit (`grncheck`);
2. runs each `tools/granny/tests/<test>.txt` on both and prints the comparison (`grntest.py`).

Exit status 1 on a mismatch. Script commands are listed in `grntest.c` (`model`, `seq`, `anim`, `add`,
`param`, `advance`, `advancen`, `render`, `textures`, `bones`, `attach`, `close*`, raw `lock`/`left`/`unlock`,
…; `~` stands for a space in names). For another architecture set `CC`/`CXX` to cross compilers and `RUN` to
the emulator, e.g. `CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++ RUN="qemu-aarch64 -L /usr/aarch64-linux-gnu"`
(apt: `gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-user`; clang works the same with
`--target=aarch64-linux-gnu`). Dumps and records must not be committed (`.gitignore`: `*.gxtr`, `*.gtst`).
