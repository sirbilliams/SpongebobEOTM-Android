# Chapter 2 readiness audit (lane f, base integ 2cf1640)

Method: the real `spongebob` binary, headless (Xvfb in the omtbuild box), 1920x1080 window, fill mode, 400 frames,
`SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=<scene> SB_GAME_LOG=1`, screenshot of the last frame. Then every one of
the 50 `scripts/c2/*.sb2` scripts started 60 game frames into its home scene with the new test hook
`SB_TEST_SCRIPT="<frame>:<name>"` (desktop only, `StartupMain.cpp`: `game->switchTo(name)` at that game frame). Logs and
screenshots: `.shots/c2/<scene>.png|.log`, `.shots/c2/script-<name>.log` (git-ignored). Screenshots are in fill mode (3x, 360 of
the 480 authored lines, panned), so a character far from the player can be off screen and is then "placed, not seen".
Not done: click-on-character runs (positions need per-scene coordinates; the dialogue tables are covered by
`sb_scripttest`: 52 dialogue files, 0 errors, 168 scripts, 0 parse errors, only c1 `idle3` unknown to the dispatcher).

## Summary
* All 31 chapter 2 scenes load, run 400 frames and exit cleanly (rc 0, no crash, no missing-file message, no error in any log).
  All 31 have a `sceneart/c2*.omt`; the 13 scenes with a dialogue table have `dialogue/` + `sounds/` files.
* The only engine TODO logged is the global `TODO(0x40ceb0) select the 3D scene rasterizer` (once per run, harmless).
* All 50 c2 scripts parse and run; none crashes. Stubbed host calls are hit by: climb (8 scripts), attach (4), sparkle (1),
  blending (1), slots (3), maze (4), plus the no-op blob shadow (1). Dialogue, comments, letterbox, transitions,
  walk/rotate, hide/show, setchar, sound, add, movie and scene switch paths work (see the per-script table).
* Real defects found: **Marlin's model is garbled** (c2l5s5, c2l5s6), the two Weather Wizard mini-games (slots, maze) are stubs,
  ladders (`climb`) do nothing, `attach` (toolbox) does nothing, the `sparkle` HUD effect is missing.
* Comparison point: the map scene c2l1s1 sits on the "Loading..." plaque in the same way c1l1s1 does (not c2 specific).

## Per scene (character lists are the `3D:` placements in the log)
| scene | loads | characters / what is on screen | stubs / TODO hit | notes |
|---|---|---|---|---|
| c2l0s1 | ok | mack, jojo (title card "Chapter 2: Hitting Rock Bottom"), both drawn | none | event sload2 runs, chains to c2l2s7p2 (sb placed at start point 2). Sky/bg ok |
| c2l1s1 | ok | none (map) | none | "Loading..." plaque stays up (same as c1l1s1); no hotspots visible headless |
| c2l2s1 | ok | sb, teller, marty2 all drawn (marty2 on the bench) | none | bg ok |
| c2l2s2 | ok | teller (window) and sb drawn | none | |
| c2l2s3 | ok | sb drawn; leon placed at z=1476 (far, off screen at start) | sflush/sthunder stubs (below) | hover text "Walk Up Road" works |
| c2l2s4 | ok | sb tiny at the bus station; leon placed, not visible | none | bg ok |
| c2l2s5 | ok | sb, leon (tall figure by the sign), can, ikc1 drawn | none | |
| c2l2s6 | ok | sb drawn | none | candy machine |
| c2l2s7 | ok | sb drawn | none | |
| c2l3s1 | ok | sb drawn; bb placed at x=978 (off screen) | none | Rusty Bucket exterior |
| c2l3s2 | ok | sb, mm, bb drawn; tumweed at (-1504, 1442), off screen | event s232unblock ok | parking lot |
| c2l3s3 | ok | sb, mack, iem1 drawn | none | bar interior |
| c2l3s4 | ok | sb drawn (low left), iem1 | none | wall hover text works |
| c2l3s5 | ok | none (barrel close-up, expected) | none | taps; sfill/sempty scripts |
| c2l4s0 | ok | none; 4:3 viewfinder picture on black | none | camera close-up, looks as authored (verify against exe: letterboxed inside the 4:3 frame) |
| c2l4s1 | ok | sb tiny (WSEA TV exterior) | none | |
| c2l4s2 | ok | sb, jojo (flame head), itb1 drawn | none | |
| c2l4s3 | ok | sb, tiff drawn | event shavetool: attach stub | |
| c2l4s4 | ok | sb, garyg drawn | none | |
| c2l4s5 | ok | plankton placed at y=655 (not visible, the commercial script moves/shows him) | event scommercial runs fully (dialogue + voice) | news set background only on frame 400 |
| c2l4s6 | ok | sb drawn | event h247a comment ok | weather machine |
| c2l4s7 | ok | sb drawn; bb (x=829,y=270), mm3 (x=988) off screen right | none | |
| c2l4s8 | ok | sb drawn; plankton placed, not visible | none | WSEA news desk |
| c2l4s9 | ok | mack, mm2, patalt2, iem1 x3, ifm1 x2 drawn ("Meanwhile..." caption) | none | |
| c2l5s1 | ok | sb tiny | none | wizard hat cave |
| c2l5s2 | ok | sb drawn | none | candle; maze entry c2l5s2p2 |
| c2l5s3 | ok | none (slot machine close-up) | event sinitslots: `mini game object`, `slots init 0x436030` stubs | reels render static; spin does nothing |
| c2l5s4 | ok | sb tiny (dark cave) | event H254A ok | maze cave |
| c2l5s5 | ok | sb, patnap (hammock) drawn; **marlin garbled** | event h255d ok | |
| c2l5s6 | ok | sb drawn; **marlin garbled**: jagged triangles, random blue/brown texture | none | see fix list item 1 |
| c2l5s8 | ok | sb drawn | event h251b ok | skull cave |

Sky/background: every scene draws its pre-drawn sceneart (no black or missing backdrops; the black bars of c2l4s0 are the authored
4:3 picture). No scene is without its player character except the ones with no 3D at all (c2l1s1, c2l3s5, c2l4s0, c2l4s5, c2l5s3).

## Per script (run in its home scene; "stub" = a `3D:` stub line in the log)
| script | scene | result |
|---|---|---|
| s232unblock, sbrady, scamera, scandy, sdrapes, sempty, sempty2, sfill, sfill2, sjojo_idle242, sload2, smack, smack2, smeanwhile3/3b, smegaon, smegascreen, splank, sslip, stransition3/3b, stumweed_idle232, swand (walk part), sleon, scommercial | various | run, no stub |
| s243up/down, s244up/down, s244up2/down2, s255up/down | c2l4s3, c2l4s4, c2l5s5 | ~~climb stub~~ **fixed** (lane f, see "Fixed in lane f" below) |
| shavetool, stoolbox | c2l4s2.. | **attach stub** `itb2 Bone01 Bip01 R Hand`; `stoolbox` also skipif |
| sflush | c2l2s3 | ~~`blending off` stub~~ fixed (game flag), `shadow off` stores the flag (blob shadows not drawn), sound toilet ok |
| sthunder, sthunder2 | c2l2s3 | ~~`shake` timer only~~ fixed: the 2D objects and the background really shake (see below) |
| sinitslots, sspinslots, sreturn | c2l5s3 | mini game object + slots init/spin are stubs (4 stub lines each) |
| smaze0..3 | c2l5s4 | with no mini game object `maze` does `gameSwitch("c2l5s2p2")` (exe behaviour); the `0x436510` branch is a stub |
| swinner | c2l5s3 | 2 stubs (slots) |
| swand | c2l4s3 | `attach`, `sparkle 590 270` (now drawn), `terrain off`, `setdoor` are reached after a long walk |
| sgaryg_idle | c2l4s4 | **not a bug**: the earlier "no such animation" came from running it with sb as the actor. With garyg as the actor (`SB_TEST_SCRIPT="60:sgaryg_idle@garyg"`) walkgrid, rotate, idle2, emofixtie, buttons, sounds and the loop run for 1300 frames with no error |
| schap2end | c2l4s6 | `movie mChapter3 c3l0s1` then scene c3l0s1 + sload3: chapter hand-over works |

## Script commands / ScriptHost calls c2 uses that c1 does not
Commands: `anim2d`, `blending`, `climb`, `control`, `maze`, `run`, `script`, `setdoor`, `shadow`, `shake`, `slots`, `sparkle`,
`terrain`, `unblock`, `unlock` (c1 uses `block`, `darts`, `exitif`, `goal`, `idle3`, `kickit`, `skip`, `texture`, which c2 does not).
Host calls and state (SceneScriptHost.cpp) and their status:
| call | status |
|---|---|
| `actorClimb` (char vt+0x154, 0x4342a0) | fixed (lane f) |
| `actorAttach` / `actorDetach` / `actorHideAttach` / `actorShowAttach` (vt+0x12c / 0x130, 0x30) | stubs; c2 needs them for the toolbox (`itb2`), `swand` |
| `sceneSparkle` (HUD object 0x457334, 0x41ebe0) | fixed (lane f): Hud::sparkle |
| `gameSetBlending` (game+0x251) | fixed (lane f): Game::blending gates the animation blend time |
| `actorShadow` | stores `Actor3D::shadowOn` (char +0x30c show / hide); blob shadows are still not drawn |
| `miniGameCreate/Exists`, `miniSlotsInit/Spin`, `miniMaze` | stubs (see below) |
| `sceneShake` (scene vt+0x15c, 0x43d130) | fixed (lane f): ten 50 ms steps of 2D object / background jitter |
| `sceneAnim2D`, `sceneSetDoor`, `sceneBlock(unblock)`, `actorRun`, `uiUnlock`, `script` (SB_TEST_SCRIPT path) | implemented |
Handler addresses in the dispatcher (0x43e110 step): run 0x43e2b5, blending 0x43e5cf, terrain 0x43e6da, shake 0x43e8c7,
climb 0x43ebb5, sparkle 0x43ec47, unblock 0x43ed67, setdoor 0x43fbed, anim2d 0x43ff4d, slots 0x4400b6, maze 0x4401a3.

## Mini-games in chapter 2 (docs/PLAN.md lists darts, slots, maze, kick)
Only **slots** and **maze** are chapter 2. Darts (`c3/sinitdarts`, `c3/sthrow`; `c1/scarwash` also sends `darts 0`), kick (`c1/skick`
`kickit`) and the phone keypad (`c4/snum*`) belong to other chapters. All share ONE 0x5c-byte mini game object
(ctor 0x4354d0, stored at `game+0x12c` i.e. `(*0x457310)[0x4b]`; created on demand by `slots`/`darts`/`phone`).
| game | entry from script | exe functions | state fields (object +off) |
|---|---|---|---|
| slots (c2l5s3, lights `253light1..3`) | `slots init` 0x436030, `slots spin` 0x436090 (the script waits) | 0x435d70 pick 3 target reel symbols (rand % 5, distinct), 0x435e10 force a win (every 3rd spin / 4th try: `rand % 3 == 1 \|\| count == 4`), 0x435e70(reel) start reel, 0x435f40 reel update/stop, 0x4368f0 per-spin refresh | targets +0xc/+0xe/+0x10, reel running flags +0x3c/+0x3e/+0x40, spin count +0x42 |
| maze (c2l5s2 / c2l5s4 cave, `smaze0..3`) | `maze n` 0x4401a3 -> 0x436510(n) | 0x436510: checks `n+5 == target[step]`, step +0x12; wrong -> reset and `c2l5s2p2`; step 3 -> `c2l5s5p1`; else `c2l5s4p4/p3/p2/p1` by n; 0x436250 (drip/archway objects `254drip1/2`, `254archa01`, scene-side update) | step +0x12, armed +0x14, failed +0x16 |
| (other chapters, for the same object) darts 0x436990 / 0x4369c0 / 0x436b70..0x436f30 ("Darts01..05" objects), phone 0x436610 / 0x436640 | | | |
The mini game object also has an update hook run from the scene frame (0x4355a0 / 0x435830, 1.3 KB, touches `0x457350` = the
timer object): that is where the per-frame behaviour lives and why a stub object gives static reels.

## Fixed in lane f (chapter 2 fixes 4, 6, 7, 8 of the list below)
* **climb** (char vt+0x154 = 0x4342a0, class vtable 0x44e358): arms the next walk, it does not move by itself. It sets
  `char+0x3f8 = 1` (climbing), `+0x3f4 = y + arg` (target y) and `+0x3f9 = running`. The next `walk` (0x434650) then skips the
  grid route (straight line to x,z), has no rotate phase, plays `down` (target <= y) or `up` at speed 1 (+1 when running) and
  stores the horizontal distance in `+0x3fc`. Each frame (0x42f1a0) y moves by `(target - y) * (1 - rem / prev)` (so y follows the
  horizontal progress), and at arrival y = target and the flag clears. Port: `World3D::climb`, `Actor3D::climbing/climbTarget/climbPrev`,
  `gridWalk` and `update`. Verified at 1920x1080: s243up lifts SpongeBob from the floor to the upper landing of the stairs, s243down
  brings him back (`.shots/f/up-sheet.png`, `dn-sheet.png`). The other six scripts use the same path (values 220, 305).
* **sparkle** (0x41ebe0 stores 3000 ms and x,y at HUD +0x124/+0x128/+0x12c; effect in 0x41e3d0 / 0x41ec90 / 0x41ec20): for 3 s,
  every frame the live sparkles age (gone at 250 ms) and, when more than 10 ms went by, one `sparkle1..3` canvas of sb2.omt is made
  at a random quadrant offset 10..41 px from the point (z 10200), at most 10 alive. Port: `Hud::sparkle/updateSparkle/spawnSparkle`.
  Seen in the swand run (`.shots/f/sp-sheet.png`).
* **blending** (game+0x251, 1 at start): only gates `GrannySetAnimationParameter(6 and 8, 0.4)` (blend in / out) in the character's
  play-animation (0x4046d0). Port: `Game::blending`; `World3D::setAnim` blends in over 0.4 s when it is set and not at all when off
  (was a fixed 0.15 s). The old control is still dropped instead of blending out (granny replacement, noted in OMedia3DGrannyModel.cpp).
* **shadow on/off** (0x434410 / 0x434430): shows / hides the blob shadow object `char+0x30c`. Port: the flag is stored
  (`Actor3D::shadowOn`), no blob shadow is drawn yet.
* **shake** (0x43d130 + the scene update at 0x43afc0): it was only a 1 s timer. The exe runs ten 50 ms steps; the first moves every
  2D object (2DOB, ANIM) to its place + a random -5..4 px in x and y, the following steps also move the background (z -10000) by
  its own random offset, the last puts all back and wakes the script (the `shake` command waits). The 3D characters do not shake.
  Port: `GameScene::shake/shakeStep/shakeObjects`, `place()` adds the offset. Measured on sthunder: ~20 frames of frame-to-frame
  change, then identical frames (everything restored).
* **sgaryg_idle**: not a bug (see the table). Finding: the port has no driver for the idle scripts (`s%s_idle`, `s%s_idle%d%d%d`,
  started from the character update 0x42f1a0 with the character as actor, runner flag +0x1962), so these scripts only run via the
  test hook (`SB_TEST_SCRIPT="frame:name@actor"`).
* Engine: `OMediaGfxWorld.cpp` read a 2-byte value at an odd address while unpacking a canvas (UBSan); now `memcpy`.
* Test hook: `SB_TEST_SCRIPT` takes `frame:name[@actor][,frame:name...]` (the actor is made the active character first).

## Prioritised fix list (lane-sized)
1. **Marlin model garbled (c2l5s5, c2l5s6)** - small. `models/people/marlin_model.grn/.omt` renders as jagged triangles with
   wrong colours; every other c2 character is fine. Suspect: a mesh/vertex variant the Granny replacement does not handle (docs/granny-replacement.md
   mesh/weight section), a bone-count or index-width case, or the texture binding. Compare against `patnap`/`garyg`; needed to finish
   the chapter (Marlin is the boss of l5). Owner: Granny/3D lane.
2. **Mini game object + slots (c2l5s3)** - medium. Reconstruct ctor 0x4354d0, 0x435d70, 0x435e10, 0x435e70, 0x435f40, 0x4368f0,
   0x436030, 0x436090 and the update 0x4355a0/0x435830; `miniGameExists` must report the object; `slots spin` must wait and
   `swinner` follow. Needs reel scroll (2D/3D objects `253light1..3`, the reels) and a win/lose branch.
3. **Maze (c2l5s2 -> c2l5s4 -> c2l5s5)** - small after 2 (same object). 0x436510 (sequence check, step counter, the five scene
   switches) and the scene-side 0x436250 drip/arch state; unblock/`block` grid already works.
4. DONE (lane f) **`climb` (8 scripts: ladders in c2l4s3, c2l4s4, c2l5s5)** - small. Character vt+0x154 (the base class default is the no-op at
   0x415450; find the character override); the walk part of the same scripts works, climb should move the actor in y by the
   argument over time. Blocks the TV-station and the wizard stairs, so it is on the critical path.
5. **`attach`/`detach`/hideattach/showattach (toolbox itb2 in shavetool/stoolbox/sjojo/swand)** - small/medium. Bones copied to
   `char+0xa76/+0xa96`, attach object `char+0xad8`; needs a 3D object parented to a named bone of the granny skeleton
   (`Bip01 R Hand`). Handler addresses 0x43e755 (hide/show), 0x43e7f7 (attach), vt+0x12c / 0x130.
6. DONE (lane f; shadow = flag only) **`sparkle` HUD effect (swand) and `blending`/`shadow` (sflush)** - small. `sceneSparkle` 0x41ebe0 on HUD object 0x457334;
   `game+0x251` blending flag (what it gates: check readers of +0x251); blob shadows are a no-op by design.
7. DONE (lane f) **Shake visual (sthunder, sthunder2)** - tiny. `Scene::shake` only sets a timer; verify the camera offset (0x43d130).
8. **Verify in real play (no code defect seen, needs eyes/controls)** - c2l1s1 map hotspots (the "Loading..." plaque), idle scripts
   (sgaryg_idle needs its `emofixtie` animation checked with garyg as the actor, stumweed_idle232 `run`), c2l4s5/c2l4s8 Plankton
   shown only via script, the c2l3s4 / c2l4s2 low-left sb foreshortening in fill mode, `terrain off`/`setdoor` in swand after the walk.
9. **Test hook follow-up** - `SB_TEST_SCRIPT` exists now; a click-per-character run (positions per scene) is still to do and
   would exercise the 13 dialogue tables on actual clicks.
