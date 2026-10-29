# Darts reconstruction (lane c)

The retail darts board is **c3l2s5**, not a chapter 1 scene. Chapter 1's
`scarwash` calls `darts 0`, which initializes/resets the shared object; chapter 3's
`sinitdarts` does the same on entering the board. Its Throw Dart SPOT dispatches
`sthrow`, whose sole command is `darts 1`. No replacement scene or bundled art is
needed: the scene loader loads the installed `sceneart/c3l2s5.omt` background,
Darts animation and Darts01/03/05 landed sprites.

## Executable behavior

- Dispatcher 0x43e110, darts branch 0x4409de: create the persistent 0x4354d0
  object if absent; argument 0 calls 0x436990, argument 1 calls 0x4369c0.
- 0x436990 resets throw count, bullseye gate, coordinates, pending flag, score
  and last hit; plays `ready` on shared mini-game sound channel 7. High score survives.
- 0x4369c0 ignores throws during flight or while the bullseye gate is set.
  After three landed throws the next click hides Darts and all three landed
  sprites and clears round state. That click does not throw or play `ready`.
- Otherwise read the input manager pointer; subtract (47,66), then add independent
  `rand()%50-25` offsets, X first then Y. A third random draw chooses dart1..3.
  Place Darts at (X,Y,Y-640), show it and start its authored animation.
- Darts branch of update 0x435830 waits until that animation stops. Score the
  impact at (X+47,Y+66), increment score and throw count, hide Darts and show
  Darts01, Darts03 or Darts05 at the same position/depth. A 50-point hit gates
  throws until the dart sound ends, then plays `yipee` and clears the gate.
- 0x436b70 tests six nested ellipses in order, returning 50/25/20/15/10/5,
  otherwise zero. Bounds (left,top,right,bottom): (302,226,336,260),
  (275,201,360,285), (234,162,400,323), (200,125,433,360),
  (161,90,475,398), (133,60,496,424).
- 0x4355a0 raises the session high score and renders Your Score at (20,435),
  High Score right-aligned at (620,435), centered values at Y=457. Positive
  hit text lasts 800 ms and moves from (X+27,Y+46) by (-0.05,+0.1) pixels/ms.

**There is no darts win/loss threshold, result script, prize or win/lose callback
in these functions.** A scoring round and an all-miss round are the faithful
substitutes for the requested win/loss tests. Do not invent a chapter 1 gate or
reuse slots' `swinner`. Script callbacks remain the authored SPOT -> sthrow path.

## Implementation and limits

Changes are confined to MiniGame.h/.cpp and the miniDarts forwarding method in
SceneScriptHost.cpp. The existing SDL/OMT mouse event path supplies aim and invokes
the SPOT, also used by touch and the pad cursor. Existing chapter 2 state and logic
are unchanged. Code is C++98; all art, glyphs and sounds load from player files.

Port differences: Win32 GDI PtInRegion is replaced by a pixel-centered analytic
ellipse test; exact GDI boundary rasterization can differ at ring edges. Existing
TextBlock draws the labels/numbers; its highlighted bullseye uses the port's
caption highlight. Random draws use the port's existing libc rand implementation,
not the MSVCRT random sequence. High score survives rounds and scene changes in
the shared object; save-file serialization of it is not added here.

The existing default widescreen fill mode crops the top/bottom of this 4:3 board,
including the score labels. Set SB_WIDESCREEN=off for the complete authored board;
changing global presentation policy is outside this lane's small integration edits.

## Verification

Headless :83, 1920x1080, SB_SKIP_MOVIES=1. Container entry was attempted but
failed: distrobox has no available container manager. Builds and runs use the
current Ubuntu environment with the private FFmpeg prefix instead.

`make -C build -j6` builds all targets after reconfiguring CMake (the stale build
cache did not yet include MiniGame.cpp). `build-asan` uses address,undefined on C
and C++, with frame pointers; leak detection disabled for the graphics/audio stack.

Scoring replay: three real board clicks produce 50+25+25=100, including `yipee`;
fourth click clears sprites and score, retaining high score 100. ASan/UBSan replay
is `.shots/darts/asan-play.log`; PNG sequence `asan-play-*.png` includes the full
board with score labels and the reset. `.shots/darts/play-*.png` also records the
default fill presentation. Miss replay uses (140,70), within the authored SPOT
rectangle but outside the outer ellipse even with the random spread.

Reproduce from `.game` (replace build-asan with build for regular binary):

```
DISPLAY=:83 SDL_AUDIODRIVER=dummy SB_WIDESCREEN=off OMT_WINDOW_SIZE=1920x1080 \
OMT_FRAMES=900 OMT_SHOTS=100,200,250,450,650,850 \
OMT_SCREENSHOT=../.shots/darts/asan-play.bmp SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 \
SB_TEST_SCENE=c3l2s5 SB_GAME_LOG=1 SB_TEST_SCRIPT=60:sinitdarts \
SB_TEST_GAME_CLICKS=150:319:243,350:319:243,550:319:243,750:319:243 \
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
LD_LIBRARY_PATH=$HOME/Projects/sbeotm-private/ffmpeg/lib ../build-asan/spongebob
```

Miss replay: OMT_FRAMES=740, clicks 150:140:70,350:140:70,550:140:70.

Both sanitizer replays exit 0 with no AddressSanitizer or UBSan diagnostics.
The miss log records three landed throws, each hit=0 and total=0. The scoring
PNG at frame 850 shows an empty board, current score 0, high score 100 after reset.

Final regression: `SUMMARY: 16 passed, 0 failed  out=/home/bill/Projects/sb-lanes/c/.shots/smoke`.
An earlier smoke run overlapped a relink and failed four launches with exit 126;
the final run above started after builds completed. All-target build and
`git diff --check` pass. No commit: the lane brief reserves commits for the lead.
