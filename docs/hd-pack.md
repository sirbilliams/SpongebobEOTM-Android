# HD background pack

An optional pack of upscaled scene backgrounds and sky canvases, so the 4:3 picture is sharper at 1920x1080. It is
made from the player's **own** installed game by `tools/hd/sb_hdtool`, lives in the game folder as `hd/` and is never
part of this repository (it is the game's art; `*.omt` and the exports are git-ignored / kept outside the tree).
Modelled on Negatron's HD texture pack (`neutron2-Android/docs/hd-pack.md`): bounded memory, PNG validation,
fallback to the original on any error, staged install with rollback.

## Making a pack (desktop)

```sh
# inside the omtbuild box (or any desktop build): build/sb_hdtool
sb_hdtool export <game dir> <export dir> [c1]        # backgrounds + skies of sceneart/*.omt as PNGs + manifest.tsv
sb_hdtool pack <export dir> pack.zip --scale 2       # built-in 2x Lanczos (a plain resample, no new detail: for testing)
sb_hdtool verify <pack dir>                           # CRC / size / sha256 against manifest.tsv
```

Upscaling is pluggable, in this order: `--upscaled <dir>` (the same names, upscaled by a batch run you did yourself),
`--upscaler "<command>"` (run once per picture; `{in}`, `{out}`, `{scale}` are replaced by quoted paths and the
factor), else the built-in Lanczos. A result of another size is resampled to original x scale, and the transparency of
the original is re-applied (colour-keyed backgrounds have transparent sky areas; AI upscalers mangle alpha).

Real-ESRGAN (ncnn-vulkan, runs on any Vulkan GPU, no Python): download the release from
`github.com/xinntao/Real-ESRGAN-ncnn-vulkan`, unpack it somewhere, then

```sh
sb_hdtool pack exp1 pack.zip --scale 4 --upscaler "$HOME/realesrgan/realesrgan-ncnn-vulkan -i {in} -o {out} -s {scale} -n realesrgan-x4plus"
```

(models `realesrgan-x4plus` / `realesrgan-x4plus-anime`; for `--scale 2` the tool shrinks the 4x result, or use the
2x model). Any other tool works the same way (waifu2x, chaiNNer, ...): wrap it in a script that takes `{in} {out}`.
`--scale` is 1 to 4. A 640x480 background at 4x is 2560x1920 (19.7 MB of GPU memory), so a whole chapter fits the
default budget; 2x is enough for 1080p.

Nothing exported or upscaled is ever committed: keep export and pack folders outside the repo (or under a git-ignored
path such as `.shots/`).

## Layout

`hd/sceneart/<scene>/<canvas>.png` stands for canvas `<canvas>` (lower case) of `sceneart/<scene>.omt`: every
`background*` canvas of a scene and `sceneart/c<N>skies/sky0<i>.png` for the chapter's skies. Each picture is a whole
multiple (1x to 4x) of the original canvas, same aspect; any PNG colour type works. `hd/manifest.tsv` (tab separated,
header first):

| column | meaning |
|---|---|
| file | `sceneart/<scene>/<canvas>.png` |
| scene, canvas | the .omt stem and canvas name |
| orig_w, orig_h | the original canvas size (the engine rejects a mismatch) |
| scale | integer factor of this picture; the largest scale in the pack sets the screen factor |
| canvas_w, canvas_h | the area covered, in original pixels (= orig for a plain upscale) |
| offset_x, offset_y | where that area starts, in original pixels (0, 0 for a plain upscale) |
| sha256 | of the PNG (provenance; `sb_hdtool verify` checks it) |

## In the engine (engine/sdl/SbHd.cpp, OMediaSDLGfx.cpp)

- `hd/` is scanned once at start (`SB_HD_DIR` overrides the folder). **`SB_HD=on|off`**: off ignores the pack, on logs
  when there is none, unset = on if the pack has pictures.
- The scene loader (`GameScene::canvasNamed`) and the sky getter (`Game::skyCanvas`) call `omgl_hd_attach` for the
  `background*` / `sky*` canvases. That decodes the picture (CRC, IHDR, size a whole multiple of the canvas, working
  memory estimated before inflating) and uploads one linear-filtered texture. The memory world keeps its size and pixels,
  so every coordinate the game uses is unchanged; blits take the HD texture instead of the world's own, with the same
  normalised UVs. If the game draws into the canvas afterwards, the picture is dropped and the original shows.
- **The screen surface is rendered at the pack's scale** (the pack's largest scale, at most 3; `SB_HD_SCALE=1..4` overrides; limited by `GL_MAX_TEXTURE_SIZE`):
  logical 640x480 coordinates, a 1280x960 (2x) framebuffer. 2D sprites and the HUD are drawn as before (just scaled),
  the 3D characters render at the higher resolution (sharper), the picture is presented linearly filtered. Readbacks
  (`omgl_surface_read`, tests) sample the logical pixel grid, so nothing else changes. Without a pack the factor is 1
  and nothing differs from before.
- **Memory budget**: `SB_HD_BUDGET_MB` (default 256) bounds the HD textures; the decode of a picture must also fit what is
  left. Past it the rest stays original (logged once per picture). Textures are released with their canvas (scene
  change), which returns their bytes. The log shows each picture's size and, at exit, `HD: N pictures used, M fell back,
  peak X MB`.
- **Any error falls back to the original**: missing file, not a PNG, bad chunk CRC, truncated, wrong size or
  manifest mismatch, over the budget, over `GL_MAX_TEXTURE_SIZE`. One log line each (`HD: <key> <reason> - using the original`).

## Android install / remove (`HdPack.java`)

`android/app/src/main/java/com/sirbilliams/spongebob/HdPack.java` is self-contained (staging in `hd.import`, the old pack
kept as `hd.previous` until the swap succeeds and restored if the final move fails, recovery of an interrupted swap on the
next `count`, zip-slip and duplicate names rejected, 256 MB per picture, free-space check against the expanded bytes,
non-PNG files left out). **Launcher hook for the lead** (LauncherView is not touched by this lane):

```java
File data = <the game data folder, the one holding sceneart/>;   // same as the importer's target
int n = HdPack.count(data);                                      // 0 = no pack: item shows "install", else "installed (n)"
// ACTION_OPEN_DOCUMENT (application/zip) -> uri, then on a worker thread:
HdPack.Result r = HdPack.install(context, uri, data, sizeOrMinus1, permille -> ui.post(() -> setProgress(permille)));
toast(r.message());                                              // r.error != null on failure, the old pack is kept
// "remove":  HdPack.remove(data);
```

Relaunch the game after installing (the pack is read at scene load; the screen factor is chosen when the window opens).
`SB_HD` has no UI yet: a launcher setting can set the env var (or delete the pack).

## Keeping the door open for true 16:9

The manifest already carries `canvas_w/h` and `offset_x/y` in original pixels. A later outpainted pack would ship e.g.
`canvas_w=853, canvas_h=480, offset_x=-106` (the original 640x480 picture sits at x=106 inside it). The loader recognises
this and today **skips such a picture with a log line** (`wider-than-original canvas, not supported by this build`), so a
wide pack cannot corrupt a 4:3 run. To use it, three pieces are needed (none built yet):

1. The background element is drawn at `(offset_x, offset_y)` with size `canvas_w x canvas_h` (original units; the HD texture
   covers it, UVs 0..1) instead of at (0, 0), and sky tiles likewise.
2. The camera/viewport: the logical screen stays 640x480 for the game logic (hit rectangles, 3D camera focal, script
   coordinates), but the present step shows `canvas_w` wide: `omgl_present`'s existing "fill"/"panels" widescreen modes
   (`omgl_widescreen_scene`) are the place: instead of black or cropped sides the extra picture on each side is visible. The
   3D world needs its viewport widened to the same area (scissor and projection extents) and the pan/scroll scenes (c2l4s0,
   c4l4s9: background offset) must shift the wide picture with the same offset.
3. Objects authored at the 4:3 edges (2DOB art, characters walking off screen) pop in/out at x=0 / x=640; scenes need a
   check, and a pack can mark a scene as unsuitable by simply not providing its picture.

## Verified (lane i)

Chapter 1 export (38 pictures) + built-in 2x pack; c1l2s6 and c1l4s1 at 1920x1080, HD off/on (screenshots in `.shots/hd/`,
not committed); the memory budget log (`SB_HD_BUDGET_MB=6` falls back with a log line, default shows 8.5 of 256 MB used); a
corrupt PNG (flipped byte: bad chunk CRC; truncated file) falls back cleanly; `sb_hdtool verify`; `HdPack.install/count/remove`
on the pack zip with a stubbed Android API.
