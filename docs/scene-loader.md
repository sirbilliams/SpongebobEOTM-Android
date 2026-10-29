# Scene loader reconstruction

The data portion of sb2.exe 0x437210 is implemented by `game/src/scene/SceneReader.cpp`.
`SceneData.h` contains portable C++98 structs for every streamed payload; `SceneData.cpp`
contains the field descriptors. The desktop target is `sb_scntest`; it has no engine, SDL,
rendering, scene-art database, script, model, sound, or startup dependencies.

## Fidelity and representation

0x437210 reads camera, up to five LITE lights (the task's “tiles”), optional STPT,
two optional GRID chunks, then factory objects until EOF. 0x42a480 consumes each factory tag;
0x429ea0 selects its class. The class reader consumes its own version and payload.
All eight payload field sequences below were checked directly against the executable's
Capstone disassembly (destination offsets, read lengths, and equality-return branches),
and independently against their paired writers. The decompiler loses many arguments and
incorrectly eliminates branches in the camera/light readers; it is insufficient by itself.

`oXXXX` members identify original reader-`this`-relative offsets, **not portable struct offsets**.
The original reader is reached through the serializable subobject: +0xd0 in 2DOB/SPOT,
+0xe0 in CHAR/ITEM/TRIG, +0xf4 in ANIM. Camera and light offsets are relative to their own
objects. Members here are in disk order; `offsetof` descriptors map them into host structs.
No executable bytes, pointers, vtables, or asset strings are embedded in this reconstruction.
Fixed Text32 buffers preserve all 32 bytes and have a bounded string accessor. Floats and
integers are decoded explicitly little-endian, without packed structs or unaligned accesses.
Record offsets and field-presence information survive parsing; absent members have zero storage
and **must be queried with `hasField`**. This is serialized data, not a reconstructed live engine
object with constructor defaults, database links, rendering setup, or post-load mutations.

Version N returns immediately after field N. Zero or values above the known maximum read all
fields, matching the equality tests. Only camera 22/23 is accepted at the top level (port scope).
Camera 23 adds +284; CHAR 32 adds +2a4; ANIM 28 adds +1d4. The original factory also accepts
3DOB, 3LIG, and SCEN; their inherited 0x440c80 reader is empty (tag only, no version).
These empty readers are supported but do not occur in the 123 files.

Port validation: short reads, unexpected camera/light tags, unsupported factory tags, and grids
outside the 30-column capacity fail with a byte offset instead of continuing into unsafe memory.
No unknown-length chunks are skipped. A failed load leaves the caller's Scene unchanged.
The grid's positive fractional extent uses the exe's float/integer comparison (ceiling), with
60 physical rows and 30-byte stride retained; valid square grids fit within the first 30 rows.

## Functions

| Address | Reconstructed data operation | Paired writer |
|---|---|---|
| 0x437210 | scene read order, STPT and both GRID reads | — |
| 0x42a480 | consume tag and call factory reader | — |
| 0x429ea0 | select streamed class | — |
| 0x40ea40 | Camera reader | 0x40ecd0 |
| 0x423a30 | Light reader | 0x423b90 |
| 0x401330 | Object2D reader | 0x4016c0 |
| 0x431bf0 | Character reader | 0x4321a0 |
| 0x4082c0 | Animation reader | 0x406b70 |
| 0x419ef0 | Spot reader | 0x41a0c0 |
| 0x422070 | Item reader | 0x4224d0 |
| 0x440ca0 | Trigger reader | 0x440f90 |
| 0x440c80 | empty inherited reader (3DOB, 3LIG, SCEN) | 0x415450 |

## Fields and unresolved meanings

The tables give every field, in disk order. Index is also the earliest version containing the
field. `u16` denotes preserved bits; many consumers interpret these as signed 16-bit values.
Known position components: camera +174/+178/+17c, 2DOB/ANIM +194/+198/+19c,
CHAR/ITEM +230/+234/+238. SPOT has +194/+198. Text32 at 2DOB/ANIM +174 and
CHAR/ITEM +1e4 identifies the art/model; further usage is outside this data reconstruction.
All remaining offsets listed below have **unassigned semantic names**, even where their broad
role (angle, flag, color, action text, bounds, animation control) is suggested by consumers.
This deliberately includes uncertain labels rather than presenting guesses as format facts.
STPT's final +14 u16 is unresolved (the constructor only initializes the preceding heading);
GRID +8/+12/+16/+20 and the second grid's gameplay role also remain unassigned.

### Camera

| Field/version | Original offset | Disk offset after tag/version | Type | Meaning |
|---|---|---|---|---|
| 1 | +0x174 | 0 | f32 | position component |
| 2 | +0x178 | 4 | f32 | position component |
| 3 | +0x17c | 8 | f32 | position component |
| 4 | +0x180 | 12 | u16 | unknown |
| 5 | +0x182 | 14 | u16 | unknown |
| 6 | +0x184 | 16 | u16 | unknown |
| 7 | +0x188 | 18 | f32 | unknown |
| 8 | +0x18c | 22 | u16 | unknown |
| 9 | +0x18e | 24 | u16 | unknown |
| 10 | +0x190 | 26 | u16 | unknown |
| 11 | +0x192 | 28 | Text32 | unknown |
| 12 | +0x1b2 | 60 | Text32 | unknown |
| 13 | +0x1d2 | 92 | Text32 | unknown |
| 14 | +0x1f2 | 124 | Text32 | unknown |
| 15 | +0x214 | 156 | f32 | unknown |
| 16 | +0x218 | 160 | Text32 | unknown |
| 17 | +0x238 | 192 | u16 | unknown |
| 18 | +0x23a | 194 | u16 | unknown |
| 19 | +0x240 | 196 | f32 | unknown |
| 20 | +0x23c | 200 | u16 | unknown |
| 21 | +0x244 | 202 | Text32 | unknown |
| 22 | +0x264 | 234 | Text32 | unknown |
| 23 | +0x284 | 266 | u16 | unknown |

Full record: 274 bytes including 6-byte tag/version.

### Light

| Field/version | Original offset | Disk offset after tag/version | Type | Meaning |
|---|---|---|---|---|
| 1 | +0x180 | 0 | u16 | unknown |
| 2 | +0x174 | 2 | f32 | unknown |
| 3 | +0x178 | 6 | f32 | unknown |
| 4 | +0x17c | 10 | f32 | unknown |
| 5 | +0x184 | 14 | f32 | unknown |
| 6 | +0x188 | 18 | f32 | unknown |
| 7 | +0x18c | 22 | f32 | unknown |
| 8 | +0x190 | 26 | f32 | unknown |
| 9 | +0x194 | 30 | f32 | unknown |
| 10 | +0x198 | 34 | f32 | unknown |
| 11 | +0x19c | 38 | f32 | unknown |
| 12 | +0x1a0 | 42 | f32 | unknown |

Full record: 52 bytes including 6-byte tag/version.

### Object2D

| Field/version | Original offset | Disk offset after tag/version | Type | Meaning |
|---|---|---|---|---|
| 1 | +0x174 | 0 | Text32 | art/model name |
| 2 | +0x194 | 32 | f32 | position component |
| 3 | +0x198 | 36 | f32 | position component |
| 4 | +0x19c | 40 | f32 | position component |
| 5 | +0x1a0 | 44 | u16 | unknown |
| 6 | +0x020 | 46 | Text32 | unknown |
| 7 | +0x040 | 78 | Text32 | unknown |
| 8 | +0x060 | 110 | Text32 | unknown |
| 9 | +0x120 | 142 | Text32 | unknown |
| 10 | +0x014 | 174 | u16 | unknown |
| 11 | +0x018 | 176 | u16 | unknown |
| 12 | +0x016 | 178 | u16 | unknown |
| 13 | +0x01a | 180 | u16 | unknown |
| 14 | +0x100 | 182 | Text32 | unknown |
| 15 | +0x140 | 214 | Text32 | unknown |
| 16 | +0x16c | 246 | u16 | unknown |
| 17 | +0x16e | 248 | u16 | unknown |

Full record: 256 bytes including 6-byte tag/version.

### Character

| Field/version | Original offset | Disk offset after tag/version | Type | Meaning |
|---|---|---|---|---|
| 1 | +0x1e4 | 0 | Text32 | art/model name |
| 2 | +0x230 | 32 | f32 | position component |
| 3 | +0x234 | 36 | f32 | position component |
| 4 | +0x238 | 40 | f32 | position component |
| 5 | +0x240 | 44 | u16 | unknown |
| 6 | +0x224 | 46 | f32 | unknown |
| 7 | +0x27e | 50 | u16 | unknown |
| 8 | +0x280 | 52 | u16 | unknown |
| 9 | +0x23c | 54 | f32 | unknown |
| 10 | +0x020 | 58 | Text32 | unknown |
| 11 | +0x040 | 90 | Text32 | unknown |
| 12 | +0x060 | 122 | Text32 | unknown |
| 13 | +0x204 | 154 | Text32 | unknown |
| 14 | +0x120 | 186 | Text32 | unknown |
| 15 | +0x014 | 218 | u16 | unknown |
| 16 | +0x018 | 220 | u16 | unknown |
| 17 | +0x016 | 222 | u16 | unknown |
| 18 | +0x01a | 224 | u16 | unknown |
| 19 | +0x2a0 | 226 | u16 | unknown |
| 20 | +0x080 | 228 | Text32 | unknown |
| 21 | +0x0a0 | 260 | Text32 | unknown |
| 22 | +0x0c0 | 292 | Text32 | unknown |
| 23 | +0x0e0 | 324 | Text32 | unknown |
| 24 | +0x100 | 356 | Text32 | unknown |
| 25 | +0x246 | 388 | u16 | unknown |
| 26 | +0x976 | 390 | Text32 | unknown |
| 27 | +0x16c | 422 | u16 | unknown |
| 28 | +0x16e | 424 | u16 | unknown |
| 29 | +0x996 | 426 | Text32 | unknown |
| 30 | +0x9b6 | 458 | Text32 | unknown |
| 31 | +0x9d6 | 490 | Text32 | unknown |
| 32 | +0x2a4 | 522 | u16 | unknown |

Full record: 530 bytes including 6-byte tag/version.

### Animation

| Field/version | Original offset | Disk offset after tag/version | Type | Meaning |
|---|---|---|---|---|
| 1 | +0x174 | 0 | Text32 | art/model name |
| 2 | +0x194 | 32 | f32 | position component |
| 3 | +0x198 | 36 | f32 | position component |
| 4 | +0x19c | 40 | f32 | position component |
| 5 | +0x1a0 | 44 | u16 | unknown |
| 6 | +0x020 | 46 | Text32 | unknown |
| 7 | +0x040 | 78 | Text32 | unknown |
| 8 | +0x060 | 110 | Text32 | unknown |
| 9 | +0x120 | 142 | Text32 | unknown |
| 10 | +0x014 | 174 | u16 | unknown |
| 11 | +0x018 | 176 | u16 | unknown |
| 12 | +0x016 | 178 | u16 | unknown |
| 13 | +0x01a | 180 | u16 | unknown |
| 14 | +0x1b0 | 182 | u16 | unknown |
| 15 | +0x1b2 | 184 | u16 | unknown |
| 16 | +0x1b6 | 186 | u16 | unknown |
| 17 | +0x1b8 | 188 | u16 | unknown |
| 18 | +0x1ba | 190 | u16 | unknown |
| 19 | +0x1bc | 192 | u16 | unknown |
| 20 | +0x1be | 194 | u16 | unknown |
| 21 | +0x140 | 196 | Text32 | unknown |
| 22 | +0x1b4 | 228 | u16 | unknown |
| 23 | +0x1c0 | 230 | u16 | unknown |
| 24 | +0x100 | 232 | Text32 | unknown |
| 25 | +0x1ca | 264 | u16 | unknown |
| 26 | +0x1d0 | 266 | u16 | unknown |
| 27 | +0x1d2 | 268 | u16 | unknown |
| 28 | +0x1d4 | 270 | u16 | unknown |

Full record: 278 bytes including 6-byte tag/version.

### Spot

| Field/version | Original offset | Disk offset after tag/version | Type | Meaning |
|---|---|---|---|---|
| 1 | +0x194 | 0 | f32 | position component |
| 2 | +0x198 | 4 | f32 | position component |
| 3 | +0x020 | 8 | Text32 | unknown |
| 4 | +0x040 | 40 | Text32 | unknown |
| 5 | +0x060 | 72 | Text32 | unknown |
| 6 | +0x120 | 104 | Text32 | unknown |
| 7 | +0x014 | 136 | u16 | unknown |
| 8 | +0x018 | 138 | u16 | unknown |
| 9 | +0x016 | 140 | u16 | unknown |
| 10 | +0x01a | 142 | u16 | unknown |
| 11 | +0x1b0 | 144 | u16 | unknown |
| 12 | +0x140 | 146 | Text32 | unknown |
| 13 | +0x16c | 178 | u16 | unknown |
| 14 | +0x16e | 180 | u16 | unknown |

Full record: 188 bytes including 6-byte tag/version.

### Item

| Field/version | Original offset | Disk offset after tag/version | Type | Meaning |
|---|---|---|---|---|
| 1 | +0x1e4 | 0 | Text32 | art/model name |
| 2 | +0x230 | 32 | f32 | position component |
| 3 | +0x234 | 36 | f32 | position component |
| 4 | +0x238 | 40 | f32 | position component |
| 5 | +0x240 | 44 | u16 | unknown |
| 6 | +0x224 | 46 | f32 | unknown |
| 7 | +0x278 | 50 | u16 | unknown |
| 8 | +0x27a | 52 | u16 | unknown |
| 9 | +0x23c | 54 | f32 | unknown |
| 10 | +0x020 | 58 | Text32 | unknown |
| 11 | +0x040 | 90 | Text32 | unknown |
| 12 | +0x060 | 122 | Text32 | unknown |
| 13 | +0x204 | 154 | Text32 | unknown |
| 14 | +0x120 | 186 | Text32 | unknown |
| 15 | +0x014 | 218 | u16 | unknown |
| 16 | +0x018 | 220 | u16 | unknown |
| 17 | +0x016 | 222 | u16 | unknown |
| 18 | +0x01a | 224 | u16 | unknown |
| 19 | +0x242 | 226 | u16 | unknown |
| 20 | +0x244 | 228 | u16 | unknown |
| 21 | +0x100 | 230 | Text32 | unknown |
| 22 | +0x16c | 262 | u16 | unknown |
| 23 | +0x16e | 264 | u16 | unknown |
| 24 | +0x246 | 266 | u16 | unknown |

Full record: 274 bytes including 6-byte tag/version.

### Trigger

| Field/version | Original offset | Disk offset after tag/version | Type | Meaning |
|---|---|---|---|---|
| 1 | +0x230 | 0 | f32 | unknown |
| 2 | +0x234 | 4 | f32 | unknown |
| 3 | +0x238 | 8 | f32 | unknown |
| 4 | +0x23c | 12 | f32 | unknown |
| 5 | +0x278 | 16 | u16 | unknown |
| 6 | +0x020 | 18 | Text32 | unknown |
| 7 | +0x040 | 50 | Text32 | unknown |
| 8 | +0x060 | 82 | Text32 | unknown |
| 9 | +0x120 | 114 | Text32 | unknown |
| 10 | +0x014 | 146 | u16 | unknown |
| 11 | +0x018 | 148 | u16 | unknown |
| 12 | +0x016 | 150 | u16 | unknown |
| 13 | +0x01a | 152 | u16 | unknown |
| 14 | +0x27a | 154 | u16 | unknown |
| 15 | +0x140 | 156 | Text32 | unknown |
| 16 | +0x27c | 188 | u16 | unknown |
| 17 | +0x080 | 190 | Text32 | unknown |
| 18 | +0x0a0 | 222 | Text32 | unknown |
| 19 | +0x0c0 | 254 | Text32 | unknown |
| 20 | +0x0e0 | 286 | Text32 | unknown |
| 21 | +0x27e | 318 | u16 | unknown |
| 22 | +0x280 | 320 | Text32 | unknown |
| 23 | +0x16c | 352 | u16 | unknown |
| 24 | +0x16e | 354 | u16 | unknown |
| 25 | +0x100 | 356 | Text32 | unknown |

Full record: 394 bytes including 6-byte tag/version.

## Verification (2026-10-28)

Built CMake desktop target with C++98; a separate strict build with
`-std=c++98 -pedantic -Wall -Wextra -Werror` also passes.
ASan+UBSan target in `build-asan` parses all 123 scenes and passes 615 boundary checks
(one-byte truncation and 1–4 trailing zero bytes for each file), including transactional failure.
LeakSanitizer cannot run under this environment's ptrace; ASan/UBSan run with
`ASAN_OPTIONS=detect_leaks=0`. The requested distrobox executable is unavailable here;
both builds use the available host compiler/CMake. No display is opened.

123/123 consume the exact file length. Counts: 615 LITE, 1230 start points, 207 CHAR,
245 2DOB, 353 ANIM, 376 SPOT, 38 ITEM, 144 TRIG. Camera versions: 114 v23, 9 v22;
CHAR: 197 v32, 10 v31; ANIM: 345 v28, 8 v27. All scenes have ten start points and five lights.

Reproduce from worktree root:

```
cmake -S engine -B build
cmake --build build --target sb_scntest -j2
./build/sb_scntest .game/scenes
cmake -S engine -B build-asan -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan --target sb_scntest -j2
ASAN_OPTIONS=detect_leaks=0 ./build-asan/sb_scntest .game/scenes
```

### Per-file results

```
c1l0s1.scn OK bytes=2094/2094 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=3
c1l1s1.scn OK bytes=3320/3320 camera=22 tiles(LITE)=5 starts=10 characters=0 objects2D=7 elements=11
c1l2s1.scn OK bytes=4442/4442 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=11
c1l2s2.scn OK bytes=5618/5618 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=3 elements=15
c1l2s3.scn OK bytes=3620/3620 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=2 elements=10
c1l2s4.scn OK bytes=6494/6494 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=18
c1l2s5.scn OK bytes=3596/3596 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=9
c1l2s6.scn OK bytes=4246/4246 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=2 elements=11
c1l2s7.scn OK bytes=3034/3034 camera=23 tiles(LITE)=5 starts=10 characters=0 objects2D=0 elements=12
c1l3s1.scn OK bytes=5620/5620 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=6 elements=15
c1l3s2.scn OK bytes=2368/2368 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=5
c1l3s3.scn OK bytes=2368/2368 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=5
c1l3s4.scn OK bytes=4566/4566 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=3 elements=12
c1l3s5.scn OK bytes=5128/5128 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=14
c1l3s6.scn OK bytes=3188/3188 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=5
c1l4s1.scn OK bytes=6140/6140 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=1 elements=15
c1l4s2.scn OK bytes=4332/4332 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=11
c1l4s3.scn OK bytes=6368/6368 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=4 elements=20
c1l4s4.scn OK bytes=6180/6180 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=4 elements=18
c1l4s5.scn OK bytes=3006/3006 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=0 elements=7
c1l4s6.scn OK bytes=4442/4442 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=16
c1l4s7.scn OK bytes=1458/1458 camera=23 tiles(LITE)=5 starts=10 characters=0 objects2D=1 elements=3
c1l5s1.scn OK bytes=7868/7868 camera=23 tiles(LITE)=5 starts=10 characters=5 objects2D=2 elements=18
c1l5s2.scn OK bytes=4612/4612 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=11
c1l5s3.scn OK bytes=2812/2812 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=7
c1l5s4.scn OK bytes=3724/3724 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=9
c1l5s5.scn OK bytes=3638/3638 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=8
c1l5s6.scn OK bytes=6294/6294 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=3 elements=14
c1l5s7.scn OK bytes=3180/3180 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=0 elements=8
c1l5s8.scn OK bytes=6530/6530 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=5 elements=19
c1l5s9.scn OK bytes=2590/2590 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=4
c1l6s1.scn OK bytes=5928/5928 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=1 elements=14
c1l6s2.scn OK bytes=3386/3386 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=2 elements=8
c1l6s3.scn OK bytes=4352/4352 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=2 elements=11
c1l6s4.scn OK bytes=3684/3684 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=1 elements=8
c2l0s1.scn OK bytes=2088/2088 camera=22 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=3
c2l1s1.scn OK bytes=3448/3448 camera=22 tiles(LITE)=5 starts=10 characters=0 objects2D=6 elements=10
c2l2s1.scn OK bytes=5652/5652 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=3 elements=14
c2l2s2.scn OK bytes=2800/2800 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=5
c2l2s3.scn OK bytes=5364/5364 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=3 elements=14
c2l2s4.scn OK bytes=5980/5980 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=15
c2l2s5.scn OK bytes=5828/5828 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=1 elements=15
c2l2s6.scn OK bytes=6422/6422 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=3 elements=19
c2l2s7.scn OK bytes=3694/3694 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=10
c2l3s1.scn OK bytes=4986/4986 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=13
c2l3s2.scn OK bytes=4840/4840 camera=23 tiles(LITE)=5 starts=10 characters=4 objects2D=0 elements=9
c2l3s3.scn OK bytes=6302/6302 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=4 elements=17
c2l3s4.scn OK bytes=4418/4418 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=13
c2l3s5.scn OK bytes=2310/2310 camera=23 tiles(LITE)=5 starts=10 characters=0 objects2D=0 elements=5
c2l4s0.scn OK bytes=1988/1988 camera=22 tiles(LITE)=5 starts=10 characters=0 objects2D=1 elements=2
c2l4s1.scn OK bytes=6448/6448 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=7 elements=17
c2l4s2.scn OK bytes=5468/5468 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=3 elements=15
c2l4s3.scn OK bytes=8374/8374 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=6 elements=24
c2l4s4.scn OK bytes=9206/9206 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=7 elements=24
c2l4s5.scn OK bytes=2354/2354 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=2
c2l4s6.scn OK bytes=3848/3848 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=2 elements=9
c2l4s7.scn OK bytes=6536/6536 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=5 elements=17
c2l4s8.scn OK bytes=4680/4680 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=15
c2l4s9.scn OK bytes=5146/5146 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=5 elements=13
c2l5s1.scn OK bytes=3574/3574 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=8
c2l5s2.scn OK bytes=4358/4358 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=11
c2l5s3.scn OK bytes=5664/5664 camera=22 tiles(LITE)=5 starts=10 characters=0 objects2D=9 elements=19
c2l5s4.scn OK bytes=5232/5232 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=4 elements=11
c2l5s5.scn OK bytes=7024/7024 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=3 elements=20
c2l5s6.scn OK bytes=4390/4390 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=14
c2l5s8.scn OK bytes=4036/4036 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=11
c3l0s1.scn OK bytes=2088/2088 camera=22 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=3
c3l1s1.scn OK bytes=3322/3322 camera=23 tiles(LITE)=5 starts=10 characters=0 objects2D=7 elements=11
c3l2s1.scn OK bytes=3580/3580 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=3 elements=9
c3l2s2.scn OK bytes=4664/4664 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=3 elements=13
c3l2s3.scn OK bytes=3462/3462 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=8
c3l2s4.scn OK bytes=5676/5676 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=4 elements=15
c3l2s5.scn OK bytes=2772/2772 camera=23 tiles(LITE)=5 starts=10 characters=0 objects2D=4 elements=8
c3l3s1.scn OK bytes=4726/4726 camera=23 tiles(LITE)=5 starts=10 characters=4 objects2D=2 elements=10
c3l3s2.scn OK bytes=4646/4646 camera=23 tiles(LITE)=5 starts=10 characters=4 objects2D=1 elements=9
c3l3s4.scn OK bytes=5688/5688 camera=23 tiles(LITE)=5 starts=10 characters=5 objects2D=3 elements=12
c3l4s0.scn OK bytes=3376/3376 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=3 elements=9
c3l4s1.scn OK bytes=4946/4946 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=2 elements=13
c3l4s2.scn OK bytes=4174/4174 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=10
c3l4s3.scn OK bytes=4792/4792 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=1 elements=10
c3l4s4.scn OK bytes=3086/3086 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=7
c3l4s5.scn OK bytes=3388/3388 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=8
c3l4s6.scn OK bytes=4014/4014 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=10
c3l4s7.scn OK bytes=3106/3106 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=6
c3l4s8.scn OK bytes=4490/4490 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=0 elements=10
c3l4s9.scn OK bytes=3814/3814 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=9
c3l5s1.scn OK bytes=3974/3974 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=3 elements=10
c3l5s2.scn OK bytes=5148/5148 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=3 elements=16
c3l5s3.scn OK bytes=4924/4924 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=15
c3l6s0.scn OK bytes=4800/4800 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=12
c3l6s1.scn OK bytes=5054/5054 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=13
c3l6s2.scn OK bytes=3350/3350 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=2 elements=8
c3l6s3.scn OK bytes=1240/1240 camera=23 tiles(LITE)=5 starts=10 characters=0 objects2D=0 elements=2
c3l6s4.scn OK bytes=3184/3184 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=0 elements=8
c3l6s5.scn OK bytes=2346/2346 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=4
c4l0s1.scn OK bytes=2088/2088 camera=22 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=3
c4l1s1.scn OK bytes=3066/3066 camera=23 tiles(LITE)=5 starts=10 characters=0 objects2D=6 elements=10
c4l2s1.scn OK bytes=4896/4896 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=3 elements=12
c4l2s2.scn OK bytes=5000/5000 camera=23 tiles(LITE)=5 starts=10 characters=0 objects2D=0 elements=15
c4l2s3.scn OK bytes=3440/3440 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=2 elements=8
c4l2s4.scn OK bytes=2584/2584 camera=22 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=4
c4l3s1.scn OK bytes=5256/5256 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=3 elements=14
c4l3s2.scn OK bytes=5566/5566 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=14
c4l3s3.scn OK bytes=6370/6370 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=0 elements=16
c4l3s4.scn OK bytes=3616/3616 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=0 elements=8
c4l3s5.scn OK bytes=3868/3868 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=8
c4l3s7.scn OK bytes=4032/4032 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=9
c4l3s8.scn OK bytes=4750/4750 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=2 elements=12
c4l3s9.scn OK bytes=3686/3686 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=10
c4l4s0.scn OK bytes=3494/3494 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=8
c4l4s2.scn OK bytes=2950/2950 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=7
c4l4s3.scn OK bytes=5388/5388 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=1 elements=15
c4l4s4.scn OK bytes=5904/5904 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=2 elements=13
c4l4s5.scn OK bytes=3034/3034 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=5
c4l4s6.scn OK bytes=5376/5376 camera=23 tiles(LITE)=5 starts=10 characters=4 objects2D=5 elements=13
c4l4s7.scn OK bytes=10700/10700 camera=23 tiles(LITE)=5 starts=10 characters=5 objects2D=6 elements=31
c4l4s8.scn OK bytes=3104/3104 camera=23 tiles(LITE)=5 starts=10 characters=2 objects2D=0 elements=8
c4l4s9.scn OK bytes=1988/1988 camera=22 tiles(LITE)=5 starts=10 characters=0 objects2D=1 elements=2
c4l5s1.scn OK bytes=3268/3268 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=0 elements=7
c4l5s2.scn OK bytes=8090/8090 camera=23 tiles(LITE)=5 starts=10 characters=5 objects2D=6 elements=21
c4l5s3.scn OK bytes=4018/4018 camera=23 tiles(LITE)=5 starts=10 characters=1 objects2D=1 elements=10
c4l5s4.scn OK bytes=4840/4840 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=2 elements=11
c4l5s5.scn OK bytes=4706/4706 camera=23 tiles(LITE)=5 starts=10 characters=3 objects2D=2 elements=12
TOTAL 123/123 passed, 0 failed; tiles(LITE)=615 starts=1230 characters=207 objects2D=245; boundary checks=615
```
