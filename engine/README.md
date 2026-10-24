# Jimmy Neutron vs. Jimmy Negatron — OMT engine port (SDL2, 64-bit)

The PC game runs on the **Open Media Toolkit (OMT) 2.5**, which is LGPL. This tree is OMT ported
off Windows/DirectX onto **SDL2 + OpenGL**, made 64-bit clean, plus a viewer that loads the
game's original `.omt` data through the real engine.

## Status

| Piece | State |
|---|---|
| OMT core (90 portable files) | Builds on 64-bit Linux, gcc + clang |
| SDL2 platform layer (`sdl/`) | Window, GL context, video engine, events, keyboard/mouse/gamepad HID, file I/O, timer, sound mixer |
| 64-bit | `long`→`int` across OMT, pointer casts → `uintptr_t`, stream ops fixed |
| OMT's own samples | `Cube`, `Teapot`, `Database` build and run unmodified (after `tools/portify.py`) |
| Game data | All 84 `.omt` archives load through `OMediaDataBase` without errors |
| Rendering | Game levels render textured through OMT's GL renderer |
| Sound | Game audio plays through OMT's sound engine → SDL mixer, bit-exact vs source |
| Android | Not built yet (needs NDK + gl4es; no code changes expected beyond build files) |
| Game logic (`Neutron2.exe`) | Not started — next phase (decompile into C++ against this OMT) |

## Layout

```
OMTClasses/        OMT 2.5 engine source, modified (see "Engine changes")
sdl/               New SDL2 platform layer (replaces OS/MSWindows + OS/MacOS)
  OMediaSDLCore.cpp     file paths (case-insensitive, '\\' -> '/'), file streams, memory, timer, clipboard
  OMediaSDLVideo.cpp    engine factory, video engine, single/fullscreen windows, GL context, 640x480 -> any-size scaling
  OMediaSDLInput.cpp    SDL event pump -> OMT events; keyboard/mouse/gamepad as OMT HID devices
  OMediaSDLSound.cpp    OMediaSound (RIFF/WAV), software mixer with resampling, sound engine
  OMediaSDLMain.cpp     main() -> omt_main(); headless screenshot hook
viewer/OMTViewer.cpp  Loads a game .omt and renders it with OMT
samples/           OMT's own samples, portified
tools/portify.py   Makes MSVC-era OMT client code 64-bit/Linux clean (use on decompiled game code too)
```

## Build & run (Linux)

```
sudo apt install build-essential cmake libsdl2-dev libgl-dev zlib1g-dev
mkdir build && cd build && cmake .. && make -j
cd <game folder>/Program_Executable_Files        # the folder containing omt/, gam/, grn/ ...
OMT_DB='omt\level1.omt' OMT_WALK=1 /path/to/build/omt_viewer
```

Viewer env vars: `OMT_DB` archive to load · `OMT_CAM="x,y,z,pitch,yaw"` · `OMT_WALK=1` fly camera
(WASD/arrows, Q/E turn, R/F tilt, Space/Ctrl up/down) · `OMT_LIGHT` sun pitch · `OMT_PLAYSOUND=1`
plays the archive's first sound · `OMT_FULLSCREEN=1` · `OMT_SCREENSHOT=x.bmp OMT_FRAMES=n` capture and exit.

## Engine changes (vs. stock OMT 2.5)

- `Core/OMediaSysDefs.h`, `Core/OMediaBuildSwitch.h`: new `omd_SDL` platform (Linux/Android), DirectX/Sprocket off,
  GL ARB name mapping, `glViewport`/`glScissor` routed through the logical→drawable scaler.
- `long` → `int` everywhere (OMT's file formats and pixel data assume 32-bit `long`); duplicate stream
  operators removed; three pointer→int casts changed to `uintptr_t`.
- **Bug fix** `Stream/Core/OMediaStreamOperators.cpp`: reading an empty Pascal string did `read(buf, -1)` and
  smashed the stack. The game's archives contain empty names, so stock OMT crashes loading them on 64-bit.
- Pre-standard C++ fixes: `= 0L`/`= NULL` pure virtuals, `<list.h>`, `<algo.h>`, a static redeclaration.

## Known issues

- Some walls/houses render darker than expected — lighting/normal convention still to be matched to the game.
- One sound (`soundeffects.omt: bird1`) is MP3-in-WAV; it loads but is skipped (DirectSound couldn't play it raw either).
- Software (OMT) renderer path compiles but is untested; GL is the target.

## Licence

OMT and this port layer are LGPL 2.1 (see OMT's `docs/lgpl.txt`). Game data is not included.
