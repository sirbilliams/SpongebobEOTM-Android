# Plan: SpongeBob SquarePants: Employee of the Month (sb2.exe) for Android

Phase 0 findings (Oct 24) and the plan for the rest. Formats: `docs/formats.md`.

## What the game is made of

| Part | Size | Approach |
|---|---|---|
| `sb2.exe` (MSVC 6, Aug 2002) | 1082 functions, no RTTI game classes | Reconstruct as C++ in `game/src`, function by function. |
| Scripts | 168 `.sb2` command lists (c1 34, c2 50, c3 29, c4 55), ~60 commands | Interpreter = sb2.exe's dispatcher 0x43e110 (a strcmp chain, not bytecode). |
| Dialogue | 52 `.sb2` tables (1876 lines) | sb2.exe's dialogue code. |
| Scenes | 123 `.scn` (versions 22/23) | sb2.exe's loader 0x437210 (OMT stream: camera, tiles, start points, grids, characters, 2D objects). |
| `OMT.dll` (OMT 1.x, Aug 2002) | 1728 exports, 182 classes, 4422 functions | sb2.exe imports 208 of its functions from 54 classes. Static reach from those: at least 572 functions / 139 KB in 102 classes (plus classes created by the database class registry). |
| `granny.dll` 1.2b | sb2.exe uses 33 functions | neutron2-android's `game/granny` has 26; 7 are new (`GetToolCoordinateSystemForModel/Animation`, `RecommendTransform`, `TransformModel/Animation`, `Lock/UnlockRenderingStateTexture`): decompile them from granny.dll. |
| Movies | 15 Bink rev `i`, 640x480, one audio track | Minimal LGPL FFmpeg (bink demuxer, bink video + bink audio decoders only), linked dynamically. |

### OMT.dll vs OMT 2.5 (our LGPL base)

66 of the 182 classes exist in OMT 2.5 (and in neutron2-android's `engine/`); 116 don't. Of the 54
classes sb2.exe calls directly, 25 are missing from OMT 2.5: OMedia2DElement, 2DAnim, 2DAnimFrame,
2DElementSorter, 2DWorld, 3DElement, 3DLight, 3DNavigator, 3DRasterizerDesc, 3DWorld, AutoMove,
AutoRotate, BoxElement, CCaption, CStringField, DXSurfaceWorld, DirectX, Display, DoubleCanvas,
InputManager, ScreenDevice, SoundManager, VPElement, WinDefaultMsgHandler, WorldClickDispatcher.

Not reached from sb2.exe (skip unless something proves otherwise): networking (TCP/UDP/IPX/modem/serial),
palettes, GUI widgets (CButton, CSlider, CPropScroller, TextList), tilemaps, particle emitter, 3D
sprites, height grid, converters (ASE/DXF/GIF).

Rule for the engine layer: a class shared with OMT 2.5 is used from our port only after its methods are
checked against OMT.dll's code (OMT.dll is older; any difference is ported from OMT.dll). Missing classes
are rebuilt from OMT.dll's decompile. DirectX/Win32 back ends map onto the SDL2/gl4es layer (`// port:`).

## Layers in order

1. **Repo base** (from neutron2-android): engine port, SDL layer, importer, Android app, CI, signing,
   crash backtrace to `omt-log.txt`. App id `com.sirbilliams.spongebob`. Importer adapted to this disc:
   `data1.cab` group `Program_Executable_Files` + `Movies/` + `Sounds/help.omt`.
2. **Engine layer** `engine/omt1/`: the OMT.dll classes sb2.exe needs: elements and worlds (2D and 3D),
   viewport, rasterizer on GL, canvases, 2D animation, navigator/lights, auto-move/rotate, the database
   and file streams (sb2.omt, sceneart/*.omt, Font.omt), sound manager, input, mouse cursor, captions.
3. **Granny** (copy + 7 functions) and **Bink playback** (FFmpeg).
4. **Startup**: sb2.exe's WinMain/app/display init, the movie chain, the main menu.
   Desktop target: logo movies play, the main menu shows.
5. **Script interpreter + scene loader + dialogue**, faithful to sb2.exe.
6. **Chapters** c1 → c4 until the game plays through to the end movie and credits; saves and minigames
   (darts, slots, maze, kick).
7. **Thor**: touch on the top screen as the mouse, gamepad as mouse/hotkeys, then the port features
   (widescreen, companion panel, saves export/import, lid suspend, casting).

## Agent lanes (max 3 at once, 2 CPUs: make -j2)

Round 1 (after your go):
- **A (Opus)**: engine layer core: element/world/2D/3D worlds, viewport, rasterizer → GL, canvases.
- **B (Sonnet)**: Bink via FFmpeg; Granny's 7 functions (bit-exact against granny.dll's code); OMT sound
  manager and input on SDL.
- **C (Sonnet)**: repo base from neutron2-android: app id, importer for this disc, CI/APK, crash log,
  launcher without show art.

Round 2: startup + main menu (Sonnet, Opus if it stalls), script VM design (Opus), scene loader (Sonnet).
Round 3+: commands and chapter lanes (Sonnet), one chapter per lane.

## First APK

Installs the game from the ISO, plays the legal/THQ/AWE/Nick/SpongeBob logo movies with sound, and shows
the main menu (mouse/touch works on it). Before that, an early APK of just the importer + movies is
possible if useful.

## Percentage to goal

| Milestone | Done at |
|---|---|
| Phase 0: inputs, maps, formats, plan | 5% |
| Repo base, importer, CI APK | 12% |
| Engine layer + Granny + Bink: logo movies + main menu on desktop | 30% |
| Same on the Thor (first APK) | 35% |
| Script VM, scene loader, dialogue: first scene playable | 45% |
| Chapter 1 plays through | 55% |
| Chapter 2 | 65% |
| Chapter 3 | 73% |
| Chapter 4, end movie, credits | 82% |
| Full playthrough on the Thor, saves, minigames, bug pass | 90% |
| Port features (launcher art, widescreen, companion panel, saves export, lid suspend, casting) | 100% |
