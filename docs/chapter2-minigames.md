# Chapter 2 slots and maze (lane d)

Implementation: `game/src/game/MiniGame.{h,cpp}`. `Game` owns the object across scene changes;
`SceneScriptHost` forwards the existing slots/maze commands. No new input interface or artwork:
the Handle ANIM and the maze TRIGs use the SDL/OMT mouse event path (also used by touch and pad cursor).
All canvases, animation frames and sounds come from the installed game's scene/sound databases.

## Executable reconstruction

- 0x4354d0: chapter 2 state starts with targets -1, step/count 0, no armed clue, no failure,
  no running reels and no pending result. Other chapters' phone/darts remain outside this work.
- 0x435d70: choose three distinct symbols in [0,4], clear armed flag.
- 0x435e10: a winning clue is [6 or 7, 8 or 5, opposite of first (7 or 6)]. Arm it,
  reset maze step and spin count. These are direction symbols, not three identical pictures.
- 0x435e70 / 0x435f40: show/hide `symbol0<target+1>`; reel positions (143,150), (289,149), (430,150).
- 0x436030: reset spin count, stop mini-game sound, choose initial symbols only when unset,
  restore symbols for stopped reels. In particular `sreturn` preserves the maze clue.
- 0x436090: refuse a second simultaneous spin, increment count, hide lights and old symbols;
  win when rand()%3 == 1 or count == 4, otherwise select distinct losing symbols.
  Start the shipped `spin`, `spin2`, `spin3` animations continuously.
- Slots branch of 0x435830: advance timer only in c2l5s3; stop reels after strictly >1000,
  >2000, >3000 ms, checked third-to-first. Stop sound pans left/centre/right (0x40c360).
  Third stop wakes the waiting script. After the runner and channel 7 finish, dispatch `swinner`
  with `sb5` and the three light animations, or play a random `sb1..4` on loss.
- 0x4367a0 / 0x4368f0: mini-game sound uses channel 7, replaces/stops the previous mini-game sound.
- 0x436510: require armed clue and direction+5 == target[step]. Wrong: reset step, set failure,
  switch c2l5s2p2. Correct: advance step, third exits c2l5s5p1 and resets step. Earlier correct
  choices 0/1/2/3 enter c2l5s4 at p4/p3/p2/p1 respectively.
- 0x436250: at step 1 use `background2`, drip2 and archb01/02; otherwise use `background`,
  drip1 and archa01/02. Scene entry to c2l5s4 creates the object if absent (0x437cf0).
  Failure return to c2l5s2 clears the failure and dispatches h252d. The dispatcher still sends
  `maze` without an object directly to c2l5s2p2.
- 0x4355a0 is the phone/darts text renderer; there is no slots/maze overlay to reconstruct there.

Small scene integration fixes, marked in the source:

- 0x43a310 checks TRIG rectangles before actors. The previous port skipped every TRIG in picking;
  `MiniGame::pickMazeDoor` supplies this first pass for the maze, used by both click and hover.
- 0x43ad10 / 0x438fa0 / 0x439110 compare names using case-insensitive 0x44b2d0. Scene find/show/hide
  now do so too: the shipped `sdrapes` requests `252drapes`, authored as `252Drapes`. Without this,
  the win/return script chain cannot complete.

## Verification

All tests headless on :84, 1920x1080, SB_SKIP_MOVIES=1. The distrobox command is unavailable in
this lane's environment; builds/tests ran directly with the available host SDL/FFmpeg dependencies.
`make -C build -j6` builds all targets. Sanitizer build uses address,undefined on C and C++, with
-fno-omit-frame-pointer; runs disable leak detection for the existing graphics/audio stack.

`.shots/maze/complete.log` and `complete-*.png` record a handle click, each reel stopping,
`swinner`, the real sdrapes/sreturn round trip, then three mouse clicks reaching c2l5s5p1.
The startup hook selects c2l5s4 after the completed slots return (avoids a long manual walk).
Reproduce from `.game` with the built executable and FFmpeg library path:

```
DISPLAY=:84 SDL_AUDIODRIVER=dummy OMT_WINDOW_SIZE=1920x1080 \
OMT_FRAMES=2100 OMT_SHOTS=60,200,350,500,900,1200,1500,1650,1850,2050 \
OMT_SCREENSHOT=../.shots/maze/complete.bmp SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 \
SB_TEST_SCENE=c2l5s3 SB_GAME_LOG=1 SB_TEST_SCRIPT=1450:c2l5s4 \
SB_TEST_GAME_CLICKS=90:595:180,1550:20:240,1730:320:465,1910:620:240 \
../build/spongebob
```

This run's clue is 2,0,1. It is observed in the log/art, not forced by a test-only winning path.
`wrong.log` / `wrong*.png`: SB_TEST_SCENE=c2l5s4 SB_TEST_SCRIPT=90:smaze0, returns with h252d.
`no-object.log` / `no-object.png`: SB_TEST_SCENE=c2l5s2 SB_TEST_SCRIPT=90:smaze3,
returns to c2l5s2p2 without creating a mini-game. `asan-complete.log` repeats the mouse journey
under ASan/UBSan. Slots-only `play*.png` show the reel sequence; `retry.log` exercises another spin.

Final mouse replay and ASan/UBSan replay both exit 0 and log all three correct maze steps and
`Switch to scene c2l5s5p1`. No sanitizer diagnostics were emitted (leak detection disabled as above).

Final regression: `SUMMARY: 16 passed, 0 failed  out=/home/bill/Projects/sb-lanes/d/.shots/smoke`.
All-target build and `git diff --check` pass. No commit (lane brief reserves commits for the lead).
