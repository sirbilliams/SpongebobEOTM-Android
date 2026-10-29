# Chapter 3 readiness audit (lane a, base integ 44612ee)

Method (as for [chapter2-audit.md](chapter2-audit.md)): the real `spongebob` binary, headless (Xvfb :81 in the omtbuild box),
1920x1080 window, `SB_WIDESCREEN=fill`, 400 frames, `SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=<scene> SB_GAME_LOG=1`,
screenshot of the last frame. Then each of the 29 `scripts/c3/*.sb2` started 60 game frames into its home scene with
`SB_TEST_SCRIPT="60:<name>[@actor]"` (no code change, the hook exists). Logs and shots: `.shots/c3/<scene>.png|.log`,
`.shots/c3/script-<name>.log`, `.shots/c3/sheet0..3.png` (contact sheets, git-ignored). Per-script command traces from
`sb_scripttest .game 300 c3/<name>` are in `.shots/c3/st/`. Fill mode shows 360 of 480 lines, so a far character can be
off screen ("placed, not seen"). Not done: click-on-character runs (see c2 audit); the 8 c3 dialogue tables are covered by
`sb_scripttest` (52 dialogue files, 1876 records, 0 errors, 168 scripts, 0 parse errors).

## Summary
* All 29 chapter 3 scenes load, run 400 frames and exit cleanly (rc 0, no crash, no missing-file message, no error in any log).
  All draw their sceneart; no black backdrop (the black area of the contact sheet is just empty cells).
* The only TODO logged is the global `TODO(0x40ceb0) select the 3D scene rasterizer` (once per run, harmless).
* **No garbled model**: sb, sbhelm (bubble helmet), squidbob (SB with Squidward head and the long legs, as authored), squidplay,
  san / sanjelly / sandress, jelly x3, snakey, nelson, stella, patalteat, leon2, ilb1 / igl1 / iot1 (the stopwatch figure) all draw
  with sane meshes and textures. Marlin (c2 only) is fixed in lane c.
* All 29 scripts parse and run, none crashes. Stub host calls hit (all `3D: ...` lines in the logs): **darts** (2 scripts, c3l2s5),
  **attach/hideattach/showattach/detach** (scharm, spitcher), **swapchar** (shelmetswitch), **shock** (sshock), **skip** (sjukebox),
  **save** (sstellawalk), `kickit/goal` not used by any c3 script. Everything else (dialogue, comments, letterbox, transitions,
  walk/walkgrid/rotate, hide/show, setchar, sound, add, movie, scene switches, climb, anim2d, the 9 jelly idle scripts) works.
* Real defects: the dart game is a stub; `swapchar` does not swap sb to sbhelm (the chapter's last act is played with the helmet
  character, which scenes c3l6s0..s5 start with already); the shock 2D effect is missing; attach (clarinet, pitcher/hat) does nothing.
* Comparison point: c3l1s1 (map) sits on the "Loading..." plaque, as c1l1s1 and c2l1s1 do (not c3 specific).

## Per scene (character lists are the `3D:` placements in the log)
| scene | loads | characters / what is on screen | stubs / TODO hit | notes |
|---|---|---|---|---|
| c3l0s1 | ok | san, squid, sb placed (title scene; only sb in the fill view) | none | sload3 runs, chains to c3l2s1p1; sky/bg ok |
| c3l1s1 | ok | none (map) | none | "Loading..." plaque stays up (as c1/c2) |
| c3l2s1 | ok | sb drawn (road in front of the diner) | none | bg ok |
| c3l2s2 | ok | sb tiny at the right edge | none | diner exterior, sky ok |
| c3l2s3 | ok | sb drawn close up (jukebox) | sjukebox `skip` stub | hover "Jukebox Buttons" works |
| c3l2s4 | ok | sb, patalteat (Patrick on the bench) drawn; ilb1, stella placed (off the view) | none | |
| c3l2s5 | ok | none (dartboard 2D, "Throw Dart" hover) | darts stub (sinitdarts / sthrow) | the mini game is a stub |
| c3l3s1 | ok | sb, 3 jellies drawn (jelly shadows ok) | none | jelly idle scripts + walks run |
| c3l3s2 | ok | sb, 3 jellies drawn | none | event senterdream runs (comment h344d) |
| c3l3s4 | ok | sb, sanjelly (Sandy in the suit, drawn), jellies | ssanwalk ok | |
| c3l4s0 | ok | nelson placed; TV swirl 2D | none | snelson: 4 TV shots + talk wait for the player |
| c3l4s1 | ok | sb tiny, houses road | none | |
| c3l4s2 | ok | sb drawn (Squidward's house, TV) | none | |
| c3l4s3 | ok | sb, squidon / squidoff (Squidward in bed) drawn | none | spitcher / sleavedream / sdream chains |
| c3l4s4 | ok | squidbob drawn small (dream street) | none | caption h344d shown |
| c3l4s5 | ok | squidbob drawn (stairs, angry statues) | none | s345up: climb + switch to c3l4s6p1 work |
| c3l4s6 | ok | squidbob far (stage hall) | none | h346a comment ok |
| c3l4s7 | ok | squidbob + squidplay (music stand) drawn | none | sdream ok |
| c3l4s8 | ok | sb (lying/tilted, bird's eye), snakey in bowl | scharm attach stub | |
| c3l4s9 | ok | sb, snakey, iwh1 (water helmet) | sstrike / spickupwh ok | |
| c3l5s1 | ok | sb | none | |
| c3l5s2 | ok | sb, nelson drawn (clothes stall, stall shop) | none | |
| c3l5s3 | ok | sb, igl1, leon2 drawn | none | |
| c3l6s0 | ok | sb drawn small, sbhelm placed | shelmetswitch swapchar stub | |
| c3l6s1 | ok | sbhelm, sandress (Sandy at the tree) drawn | none | h361c comment, door sound ok |
| c3l6s2 | ok | sbhelm in the bubble | none | |
| c3l6s3 | ok | iot1 (stopwatch figure) drawn | none | |
| c3l6s4 | ok | sbhelm, sandress drawn | none | schap3end: movie mChapter4 c4l0S1 |
| c3l6s5 | ok | sbhelm drawn | none | |

Sky/background: every scene draws its sceneart; no scene is without its player character except those with no 3D at all
(c3l1s1, c3l2s5, c3l4s0 shows nelson only, c3l6s3 shows only iot1).

## Per script (run in its home scene; "stub" = a `3D:` stub line in the log)
| script | home scene | result |
|---|---|---|
| sload3 | c3l0s1 | ok, ends `switch c3l2s1p1` |
| schap3end | c3l6s4 | ok (waits on `talk`; then movie mChapter4 -> c4l0S1) |
| s345up | c3l4s5 | ok (`climb 135`, `walk`, switch c3l4s6p1, comment h346a) |
| sdream, senterdream, sleavedream, snelson, spickupwh, ssanwalk, ssandydome, sstellawalk, svideo3, sjelly* | various | ok; those with `talk`/`wait`/`unlock` stop for the player, as the exe |
| sstellawalk | c3l2s4 | `save` stub (progress position + scene id) |
| sjukebox | c3l2s3 | `skip` stub (scene vt+0x10c cutscene skip); `anim2d 323RecordTurnOver` ok |
| scharm | c3l4s8 | **attach stub** `icl2 Bone01 Bip01 L Hand` (clarinet); detach |
| spitcher | c3l4s3 | **attach/hideattach/showattach stubs** `igl2 Bone01 Hat` |
| shelmetswitch | c3l6s0 | **swapchar stub** (`hide SB`, `show SBHelm` work) |
| sinitdarts, sthrow | c3l2s5 | **darts stub** (`darts 0` / `darts 1`) |
| sshock | c3l3s2 | **shock stub**; `anim2d elec` finds the object only in c3l3s1/s2/s4 (run it there; in c3l4s9 it reports "no 2D animation") |
| sstrike | c3l4s9 | ok with sb as actor (comment h349a); run as snakey it logs "no such animation" (not a bug, the actor was wrong) |
| sjelly/2/3_idle331/332/334 (9) | c3l3s1/s2/s4 | ok with the jelly as actor: walk, idle1, sound (the scene driver also starts them: log "idle script ... started") |

## Script commands / ScriptHost calls c3 uses that c1 and c2 do not
Commands: `hideattach`, `showattach`, `save`, `shock`, `swapchar` (c3 also uses `darts` like c1, `video` and `startanim` like c2).
Kick mini game: no c3 script or scene uses `kickit` / `goal`; the entry is `ScriptRunner` `kickit` (0x440213) ->
`actorKick` (vt+0x16c), `goal` (vt+0x174 / 0x188), all still stubs; a Codex lane is building it (noted only, not touched here).
| call | status |
|---|---|
| `miniDarts` (0x4409de, 0x436990 / 0x4369c0) | stub, c3l2s5 |
| `actorAttach` / `actorDetach` / `actorHideAttach` / `actorShowAttach` (vt+0x12c / 0x130, 0x43e755 / 0x43e7f7) | stubs (as c2 toolbox) |
| `swapPlayerHelmet` (0x43fced) | stub |
| `shockEffect` (0x43ffda) | stub |
| `gameSkip` (0x43f062, scene vt+0x10c) | stub |
| `progressSave` (0x43f1a7) | stub |
| `actorKick`, `actorGoal`, `actorGoalMissed`, `miniPhone*`, `actorSetTexture` | stubs, not hit by c3 |

## Prioritised fix list (lane-sized)
1. **Darts mini game (c3l2s5)** - medium. 0x436990 / 0x4369c0 (`darts 0/1`), `sinitdarts` / `sthrow` flow, board 2D hit zones
   and score ring values (the board with 25/20/15/10/5 is already drawn), the "Throw Dart" hover; needs the mini game object
   (shared with slots/maze/phone) and the win condition that advances the chapter.
2. **Attach family (scharm clarinet, spitcher hat/pitcher, c2 toolbox)** - small/medium. vt+0x12c / 0x130 (0x43e755, 0x43e7f7),
   bones `Bone01 Bip01 L Hand`, `Bone01 Hat`; one lane also fixes c2.
3. **`swapchar` (shelmetswitch, 0x43fced)** - small. sb <-> sbhelm player swap, so the finale uses the helmet character and its
   walkgrid / texture state; check the 3D objects `SB`, `SBHelm` hide/show interplay.
4. **`shock` effect (sshock, 0x43ffda)** - tiny. 2D object `elec` at the player's screen position, `anim2d elec`.
5. **`skip` (0x43f062) and `save` (0x43f1a7)** - small. Cutscene skip (scene vt+0x10c) for sjukebox / video scenes; progress
   record of the actor position + scene id (sstellawalk).
6. **Kick mini game** - in progress elsewhere (kickit 0x440213, actorKick vt+0x16c, goal vt+0x174 / 0x188).
7. **Verify in real play** - c3l1s1 map hotspots, the click-driven dialogue tables (c3l2s4, c3l3s4, c3l4s0, c3l4s7, c3l5s2,
   c3l5s3, c3l6s1, c3l6s4), c3l4s3 -> c3l4s4 dream chain (sdream / spitcher / senterdream), the c3l6 finale with the helmet
   character, snelson TV sequence, c3l4s8 low camera foreshortening of sb in fill mode.
