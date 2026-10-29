# Startup and main menu reconstruction

Retail addresses refer to the installed sb2.exe (image base 0x400000). No game art,
fonts, executable tables, or captures belong in git. This lane leaves game/src/script alone.

## Startup and display

`game/src/port/StartupMain.cpp` reconstructs the startup sequence from WinMain
0x42a540 -> game entry 0x40e580 -> application constructor 0x4097d0. Database
registration/cache setup, font initialization 0x40d0b0, settings 0x409ed0,
Granny initialization 0x40d2d0, and Bink sound initialization 0x40c2a0 precede
named movie dispatch 0x40a230. Movies open at 0x429b60, advance/present at
0x429c80/0x429ce0, and complete at 0x40bcc0.

The startup now uses **OMT 1.x** Display/ScreenDevice/DoubleCanvas, 2DWorld,
ViewPort, EventManager and WorldClickDispatcher. It does not link the OMT 2.5
world/display/application implementation. The SDL main function invokes the
startup's explicit event/render loop (port adaptation of the application and
periodical loop). `sb_audio` reuses lane B's manager and SDL mixer separately;
its otherwise unused OMT 2.5 sound-engine window association is compiled out
with `OMT1_AUDIO_ONLY`, so UBSan never needs an incompatible OMT 2.5 Window.

Port: SDL/GL replaces DirectDraw; per-user `settings.cfg` replaces the registry
at `Software/THQ/SpongeBob 2/Settings` (`OMT_CONFIG` overrides the path). Defaults
are Music/Sound/Voice 7, Save -1, Text/Shadows 1; `-sw` suppresses shadows.
CD-relative paths use the imported install folder. Software **3D** rasterizer
selection (0x40ceb0) is done by `World3D::rasterizer()` when the first scene starts; the 2D menu needs none.

The five-logo sequence is legal -> THQ -> AWE -> Nick -> SpongeBob. The requested
port milestone opens the menu after it. Retail additionally probes slot0 at
0x40e160 and has save/intro/alternate-scene continuation; that automatic boot
continuation is still outside this menu milestone.

## Font: vtable 0x44dd78

`game/src/ui/GameFont.{h,cpp}` implements the game's font and an OMediaTextFont
adapter for lane A's CCaption/CStringField interfaces. Important correction to
the earlier API note: this retail vtable has **six entries**, ending before
0x44dd90. It is not itself the DLL's text-font vtable with draw at +0x18/+0x1c.
The adapter provides those operations without pretending the original ABIs match.

| Address | Reconstructed operation |
|---|---|
| 0x414820 | Open Font.omt; initialize glyph pools and lookup |
| 0x414910 / 0x414930 | Destruction / release glyph elements and font database |
| 0x414a90 | Hide pooled glyph elements for reuse on the following frame |
| 0x414b00 | Formatted text wrapper; default z=0x2774 |
| 0x414b50 | Formatted text, optional centering, wrapping and returned height |
| 0x414de0 | Draw banked glyph canvases through reusable CanvasElements |
| 0x414fe0 | Measure text using normal-bank glyph widths |

The character-to-ID table at 0x454d44 is read from the player's PE file by
`RetailData`, together with menu label tables. The checked PE reader handles
section virtual addresses and reads little-endian values bytewise. There are
no copied executable tables in source. Glyph variant ID = normal ID + bank*71.
Space advances five pixels; glyphs advance width minus one. CP1252 0x85 maps to
`{`, 0x92 to apostrophe; `*` toggles emphasis without drawing. Measurement
always uses bank zero. Wrapping uses width <631, at most five lines, trailing
spaces after words, and draws preceding lines upwards in 22-pixel steps;
return height is 18 +22*(line count-1).

Port adaptations: captions blit these same glyphs into their canvas; the font
adapter exposes an 18-pixel line height. Unsupported extended bytes are guarded
instead of indexing before the lookup table. Formatted text bounds the original
256-byte vsprintf buffer. Cached canvases hold balanced database locks.

## Main menu and dispatch

`game/src/ui/MainMenu.{h,cpp}` implements the modal controller path: lazy
sb2.omt acquisition at 0x41bbb0, toolbar menu/menut art from 0x41f6a0, modal
opening 0x41c1c0, and dialog construction 0x4242b0. The main menu does **not**
load a menu .scn or sceneart database: its centered bamboo4 canvas, glyphs,
noresume overlay, help pages, sliders, cursor and locked/movie art are in
sb2.omt/Font.omt. Captions start at y=91, spaced by 40 pixels, centered using
retail font measurement. Hover/click rectangles use the strict inequalities
and 18-pixel text height of 0x426e00/0x425aa0. Help arrows use the eight-frame
arrows/arrows2 canvas sequences. Options positions follow 0x427870.

SDL mouse buttons go through `handle_sdl_event` -> mouse broadcaster ->
WorldClickDispatcher -> the world's viewport picker -> MainMenu listener.
SDL motion updates hover selection, matching the controller's polled-position
behavior; it selects font bank one. Quit events terminate the loop, and Escape
returns from a subpage (port keyboard convenience). Right clicks do not activate
items. The desktop-only OMT_MENU_CLICKS replay hook injects SDL events into this
same path; it does not call handlers directly.

| Item | Retail dispatch / implemented result |
|---|---|
| Help | 0x425580 -> 0x427640; five help canvases, previous/next, Cancel |
| Options | 0x425580 -> 0x427870; three 0..9 sliders, dialogue text/shadows On/Off, persisted settings |
| Save Game | 0x427ef0; preview frame; TODO(0x428740): scene/VM serialization |
| Load Game | 0x428bb0; empty-slot presentation; TODO(0x429120): restore scene/VM state |
| View Movies | 0x4246a0 / 0x424bb0; THQ, SpongeBob, Intro, Credits initially available; locked rows for others |
| New Game | 0x425605 -> confirmation -> 0x4254b0 -> 0x4248b0 -> app 0x40a090; plays mintro, parses c1l0s1 with lane D's loader |
| Resume Game | 0x4248b0; no scene to resume during this startup menu |
| Quit Game | 0x4255ef -> confirmation -> 0x4254b0 -> app 0x40a070; clean teardown |

New Game/Quit confirmations use Yes/No with their retail positions. The Quit
prompt preserves the positioning quirk: measure "Exit Game?", draw "Quit Game?".
No restores the menu through 0x425520. Logs identify each item's handler.

Remaining runtime boundaries are explicit TODOs: instantiate the first 3D scene
and script runner at 0x40a230 after the parsed c1l0s1 data; save screenshots,
metadata browsing and restoration at 0x428740/0x429120 require live scene state;
additional movie unlocks at 0x424d60 require script variables. The current empty
save UI does not deserialize existing player saves. Menu audio effects and the
inventory/map toolbar beyond its acquired menu art are not reconstructed here.
Returning to the menu after a scene TODO is a port fallback, not playable gameplay.

## Help / Options (lane c)

Options (0x425aa0 key handler, 0x4256b0, 0x40c3b0): rows Music / Sound / Voice (0..9), Dialogue Text, 3D Shadows, Ok;
keys M S V D 3 O select a row, Up/Down move, Left/Right change, Enter on Ok leaves; pad d-pad = arrows, B = back.
Every change applies live (0x40c3b0): channel 3 = Music, 4 = Voice, others (not 0) = Sound, volume 6*level+201 (0 for
level 0), logged as `0x40c3b0: mixer ...`; it is saved to settings.cfg (the registry's Music/Sound/Voice/Save/Text/
Shadows) and read back at start. Sound/Voice changes play a sample (ibt1 / H000L) when a game's banks are open
(0x4256b0). Text Off hides the caption of a voiced dialogue line (0x410a60/0x4128d0); Shadows is kept on the game
(`optShadows`) until the 3D scene draws blob shadows. Help: Left/Right page, Enter leaves. Escape, F10 and Android
Back leave a screen. The menu stays a 4:3 640x480 canvas in fill/panels/stretch (checked at 1920x1080).

## Validation (2026-10-28)

- C++98 RelWithDebInfo and ASan/UBSan builds of spongebob pass.
- The shell is already inside the build distro; distrobox itself is not on PATH.
  Existing local FFmpeg 7 headers/libraries at `/tmp/c-ffmpeg` were used because
  the pinned SDK is absent. No dependency binaries are committed.
- Full 912-frame logo sequence completes, then the menu displays and Quit/Yes
  exits 0. `.shots/logos-menu.log` and PNGs at frames 20/125/225/350/450/925.
- `tools/startup/smoke.py`: eight actual SDL menu clicks, hover, help navigation,
  option changes, confirmation No/Yes, Intro, first-scene parse, clean quit.
  `.shots/menu-replay.log`, `.shots/new-game.log`, `.shots/smoke.log`; captures
  include the main panel, Options, Help, movie list and confirmations.
- `sb_font_test`: empty text, spacing, glyph overlap, smart-character mapping,
  emphasis markers, pool reuse across frames/banks, wrapping/height and clipping.
  ASan/UBSan pass, `.shots/font.log`.
- The shared RLE reader's unaligned unit load is now memcpy (`// port:`), fixing
  the menu canvas UBSan report from OMediaRLEPacker.cpp.
- Current sandbox cannot bind Xvfb's listening sockets on :83; host launch is
  unavailable too. Tests explicitly unset WAYLAND_DISPLAY and use SDL's offscreen
  driver (headless llvmpipe), with DISPLAY=:83 retained. **A real :83 Xvfb run is
  still unverified in this session.** No final verification uses the real desktop.
- ASan/UBSan report no memory-access or undefined-behavior errors. LSan's exit
  scan cannot attach threads in this sandbox and fails with its ptrace diagnostic;
  use detect_leaks=0 here. Leak freedom is not certified.
- No commit: lane brief directs Codex to leave commits to the lead.

Reproduce (inside the build distro):

```
cmake -S engine -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DFFMPEG_PREFIX=/tmp/c-ffmpeg \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build-asan -j6 --target spongebob sb_font_test
LD_LIBRARY_PATH=/tmp/c-ffmpeg/lib python3 tools/startup/smoke.py --offscreen --full-logos
```

Once Xvfb :83 is available, omit `--offscreen`. Captures remain in ignored `.shots/`.
