# Marlin / installed model audit (lane c)

Marlin's defect was in texture-map parsing, not skinning or index width. The old
loader read only the first `0e02` under a `0e00`, but collected **all** descendants'
`0e04` UV layers. Marlin has two mesh records under one map: his body (822
triangles) and book (40 triangles). Each has one UV layer. The loader treated the
body as having two layers, read its four-dword triangle/UV records with a
seven-dword stride, and lost the book's UV records entirely. This scrambled the
texture and needlessly split render vertices at false UV seams.

The fix retains the hierarchy:

- `0e00` selects a model (1-based).
- Each `0e02` supplies a **model mesh slot** (0-based) and **material** (1-based).
  A mesh slot indexes the model's `0c03` list, whose value then selects a `0601`
  geometry. Geometry order and model order differ in Leon, Fred/SpongeBob,
  Mermaid Man with mug, and others.
- Layer and per-triangle UV lists belong to that `0e02` alone. The mesh uses its
  referenced material's colour, texture and blend state. Referenced materials
  are queued once in first-use order.

The bitmap object's parsed name now accompanies the runtime texture so the OMT
wrapper can bind the correct database material. Raw byte scanning for bitmap
extensions could not associate several textures with their meshes. The wrapper
checks `object_exists` before `get_object`: the latter silently creates an empty
material for an absent name and previously prevented its fallback from running.
The existing first-material fallback remains for unnamed/missing materials.

The sequential thumbnail review exposed a third defect in the GL binding path:
`OMedia3DGLRasterizer` cached textures by `OMediaGfxPixels*` for the rasterizer's
entire lifetime. After a model database was destroyed, another bitmap could
reuse that address with the same dimensions and version. It then displayed the
previous bitmap. The process-wide game rasterizer also survives scene changes.
The separate repeat-wrapped 3D GL copy now belongs to `OMediaGfxPixels`, alongside
its clamp-wrapped 2D copy; `omgl_forget` deletes both when the bitmap is destroyed.
The final sheet was reviewed again after this fix and has correct burger, mug,
item and character textures without dependence on preceding loads.

Retail granny.dll machine-code checks: `0x100359e0` parses `0e02` and its own
layers / triangle data; `0x10035cc0` parses the model's `0e00` list;
`0x10038b90` returns the mesh slot; `0x100358e0` returns the material reference;
`0x10023e50`, specifically `0x1002415b..0x10024183`, matches the mesh slot and
looks up that material. The OMT wrapper implements sb2.exe `0x4039c0` BuildShape,
`0x403e10` BindTextures and the pose-copy path associated with `0x405620`.
No new game function or transform arithmetic was reconstructed.

Marlin now has 2 meshes, 779 render vertices, 2,586 indices (862 triangles),
52 bones, 39 bindings, at most 2 influences per source vertex, and 1 material.
The old loader produced 1,550 render vertices and a missing-UV warning. Expanding
each old/new index into its bind-position triangle stream and comparing the
result with `cmp` gives **identical bytes** (`.shots/marlin/compare.log`): the
apparent jagged geometry was the texture, not displaced triangles. Other
characters such as Mack and Fred/SpongeBob have 3 influences and already use the
variable-length skinning path correctly. All installed models fit u16 indices.

## Reproduce

Inside omtbuild, from this worktree (the agent environment is already in it):

```
cmake -S engine -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_PREFIX=$HOME/Projects/sbeotm-private/ffmpeg
make -C build -j6
mkdir -p .shots/models
DISPLAY=:83 SDL_AUDIODRIVER=dummy OMT_WINDOW_SIZE=640x480 build/omt1_models_test "$PWD/.game" "$PWD/.shots/models" > .shots/models/audit.log 2>&1
python3 tools/test/model-sheet.py .shots/models
SB_SMOKE_DISPLAY=:83 tools/test/smoke.sh build .game .shots/smoke
```

`omt1_models_test` recursively reads every `.grn` and reports meshes, vertices,
indices, bones, bindings, maximum influences, materials and animation tracks.
It checks index/binding ranges, UV coverage, finite positions/UVs, the OMT
material/canvas bindings and finite sampled poses. It uses the engine's actual
Granny wrapper, skinning, OMT shape and GL renderer to capture each mesh-bearing
file, sampling an idle where available. The three Patrick animation files that
also embed meshes are played on Patrick's normal model/material database.
Animation-only files are reported with zero meshes rather than mistaken for
character models. Captures are framed independently by their sampled bounds.
The target produces `contact.bmp` and `thumbnails.tsv`; the optional Python /
ImageMagick helper writes a labelled `contact.png`. No game files/art are bundled.

## Verification

- Full native build, all targets: passed.
- `.shots/models/audit.log`: **674 files, 128 mesh-bearing files, 556 files with
  animation tracks, 0 failures** (125 model files plus 3 embedded-mesh animations).
  Includes 31 model files with multiple meshes. Sheet: `.shots/models/contact.png`.
- Real game, 640x480, 400 frames, `SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1
  SB_TEST_SCENE=c2l5s5` / `c2l5s6`: exit 0, visually checked Marlin and book.
  `.shots/marlin/c2l5s5.png`, `.shots/marlin/c2l5s6.png`.
- `SUMMARY: 16 passed, 0 failed` in `.shots/smoke/summary.txt`.
- ASan/UBSan (`halt_on_error=1`, leak detection disabled for engine process
  globals): both real-game scenes, 400 frames each, and the complete model
  catalogue passed without reports. `.shots/marlin/*-asan.log|png`,
  `.shots/models-asan/audit.log`.

The catalogue separately reports pre-existing embedded texture decoder limits:
Leon's alpha Bink can texture and two raw Patrick-eating images are not decoded
by granny_bink. **Rendering uses the model .omt canvases**, so those images do
not need the embedded decoder and do not block material binding or thumbnails.
NaN normals authored into ikp2/ikp3/ikp6 and Ray are preserved and reported;
positions and UVs remain finite. Neither decoder expansion nor alteration of
these authored normals is part of this fix. Wine/DLL execution is unavailable
in this environment; the old Neutron corpus bit-identity suite was not rerun.
