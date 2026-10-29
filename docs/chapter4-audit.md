# Chapter 4 + ending readiness audit (lane e, base integ 44612ee)

Method as in [chapter2-audit.md](chapter2-audit.md): the real `spongebob` binary, headless (Xvfb :85 in the omtbuild box), 1920x1080
window, fill mode, 400 frames, `SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=<scene> SB_GAME_LOG=1`, screenshot of the last frame
(`.shots/c4/<scene>.png|.log`, git-ignored). Then each of the 55 `scripts/c4/*.sb2` started 60 game frames into its home scene with
`SB_TEST_SCRIPT="60:<name>[@actor]"` (`.shots/c4/script-<name>.png|.log`; the five `*_idle` scripts with the character as actor).
`sb_scripttest`: 52 dialogue files (14 are c4), 0 errors, 168 scripts, 0 parse errors. No game code changed (no new hooks needed).

## Lane c fixes (2026-10-28)

Items **1 (ending)** and **4 (texture)** are implemented. The original observations below describe the pre-fix audit.
Phone and the character idle-script driver are already implemented in integ; they are not open lane c work.

* `Game::movieFinished` reconstructs the game-mode branch of **0x40bcc0**. `mend` starts `mcredits`; credits
  completion leaves the game loop, destroys the scene, and returns to the main menu as requested.
  **Disassembly discrepancy:** vt+0xc0 is **0x40a090**; its +0x254 branch skips the progress clear and switches
  to **c1l2s6p1** (literal 0x4544cc), while its ordinary New Game branch starts `mintro`. Thus returning
  directly to the menu follows the requested task behavior, rather than that final executable restart.
  The port keeps the explicitly requested menu behavior; the corresponding code is marked `// port:`.
  Correction to the original offset note: `param_1[0x95]` is byte **game+0x254**, not game+0x95. That flag
  prevents the progress clear at 0x40a147. Movie unlock progress therefore survives the ending; the last slot
  (`game+0x26c`) is retained, Resume is disabled, and New Game / explicit Load remain available. A new game
  clears progress normally. View Movies still takes its separate single-movie path.
* **0x404070** (character vt+0xf0) looks up a named `3DMa` in the character model's database, then calls
  **0x4053c0** (vt+0xc8) with the replacement material and force=true. The port updates each Granny rendering
  state's texture cookie through Lock/UnlockRenderingStateTexture, then rebuilds the shape's material bindings.
  `SceneScriptHost` now applies `texture` immediately; saved TEXT records reapply after scene/model loading.
* `SB_SKIP_MOVIES=0` now actually enables playback (previously any value skipped movies). The smoke logo
  check now allows 1050 frames because the five actual logo movies total 912 frames.
* Captures/logs: `.shots/c4-fix/` on display :83, 1920x1080. `scoatcheck` belongs in **c4l4s6**, whose CAA
  dialogue is the coat-check exchange; c4l4s4's CAA is the bouncer. `coat-on` runs the real coat script and
  `coat-off` subsequently runs `snojac` to restore the ordinary texture. Both log successful material binding.
* All desktop targets and ASan/UBSan targets build. `sb_unittest`: **72 checks, 0 failed**.
  `tools/test/smoke.sh build .game .shots/smoke`: **SUMMARY: 16 passed, 0 failed**.
  Sanitized ending (movies skipped) and `scoatcheck` runs exit cleanly with no ASan/UBSan diagnostics.
  Full real-time playback (`SB_SKIP_MOVIES=0 SB_TEST_SCENE=c4l5s4 SB_TEST_SCRIPT=60:schap4end`,
  4400 total frames including logos, display :83, 1920x1080) exits **rc 0**: mEND (2146 frames) finishes,
  mCREDITS (882 frames) finishes, then the menu is reached cleanly. `ending-full.png` shows New Game
  available and Resume greyed; last slot 0 is retained. `coat-comparison.png` shows ordinary / black jacket.
  A further ASan/UBSan ending run opens View Movies after completion: `ending-unlocks.png` confirms
  Neptune's Paradise (mend) and Credits available, with unseen chapter movies still locked; rc 0, no diagnostics.

## Summary
* All 28 c4 scenes load, run 400 frames, exit rc 0; no crash, no missing-file message, no error. All 28 have `sceneart/c4*.omt`;
  the 14 scenes with a dialogue table have `dialogue/` (+ `sounds/`, except c4l2s3 which is comments only).
* All 55 c4 scripts start and run; none crashes (rc 0). Only engine TODO: the global `TODO(0x40ceb0)` rasterizer line.
* No garbled models seen (sb, san, fred, shapiro, lisa, carlton, cowfish, bob, timmy, pat/patalt, leon2/leonmop, dude1..3 all drawn
  sensibly). Sky/background ok everywhere (c4l4s9 binocular view is black with the two circles by design, c4l4s0 / c4l4s8 have 4:3 bars).
* **The game never ends**: `schap4end` ends with `movie mEnd` and nothing follows; the port has no mend -> mcredits -> menu chain (see End).
* **Real defects**: (1) the global `H000A..X` comments are never found (`comment 'H000I' not found` in snojac, `H000F` in sshorts and for
  every failed item use / unknown click in all chapters); (2) `texture` (c4 coat swap `sb sponge` / `spongetux`) is a stub; (3) the phone
  keypad (`phone` command) is a stub; (4) end chain missing.
* Note: c4l3s8 / c4l3s9 show a large SpongeBob (igc1/inc1, inside Patrick's rock? "SpongeBob" hover) with pink/purple eyes; looks like
  the authored 3D prop texture, to confirm against the original. c4l5s5 walls have red outlines (sceneart), also to confirm.

## Per scene (character lists are the `3D:` placements in the log)
| scene | loads | characters on screen | stubs / TODO hit | notes |
|---|---|---|---|---|
| c4l0s1 | ok | fred, shapiro (title "Chapter 4: Bottom's Up!") | none | event sload4 |
| c4l1s1 | ok | none (map) | none | "Loading..." plaque, as c1l1s1 / c2l1s1 |
| c4l2s1 | ok | sb, san (idle script ssan_idle421 starts and walks) | none | |
| c4l2s2 | ok | none (phone close-up, keypad 1..9 hotspots) | none | needs phone game |
| c4l2s3 | ok | sb | none; event snojac | `snojac` texture stub |
| c4l2s4 | ok | carlton, cowfishtaunt | none | event smeanwhile4, voice 424aaa plays |
| c4l3s1 | ok | sb(wet), inc1, inc2 | none; snojac | fountain, scoco/sgrow/spplate/swateron |
| c4l3s2 | ok | sbwet | none | Neptune gate |
| c4l3s3 | ok | carlton, sb | none | comment h432e |
| c4l3s4 | ok | carlton, sb (scarlton_idle434 runs) | none | |
| c4l3s5 | ok | sb | none | comment h433d |
| c4l3s7 | ok | cowfish (Cowfish Craig), sb | none | |
| c4l3s8 | ok | igc1, inc1, sb | none | event sindy |
| c4l3s9 | ok | igc1, inc1, sb | none | |
| c4l4s0 | ok | fred, sb | none | voices 440aaa/aab play (narration) |
| c4l4s2 | ok | sb | none | comment h442C |
| c4l4s3 | ok | sb, timmy (idle443 runs) | none | comment h441b |
| c4l4s4 | ok | bob, sb | none; snojac | bouncer / coat check |
| c4l4s5 | ok | sb | none; snojac | |
| c4l4s6 | ok | fred, lisa, sb, shapiro (idle446) | none | |
| c4l4s7 | ok | dude1..3, fred, sb (idle447) | none | |
| c4l4s8 | ok | fred, sbsit | none | swaiter dialogue (3 choices) |
| c4l4s9 | ok | none (binocular view) | none | |
| c4l5s1 | ok | sb | none; snojac | intercom |
| c4l5s2 | ok | dude1/2, pat, patalt, sb | none | comment h452c |
| c4l5s3 | ok | sb | none | |
| c4l5s4 | ok | pat, patalt, sb | none | end scene |
| c4l5s5 | ok | ipp2, leon2, leonmop, sb | none | |

## Per script (run in its home scene; "stub" = a `      3D:` line in the log)
| script | home scene | result |
|---|---|---|
| s444up/down, s447up/down | c4l4s4, c4l4s7 | climb works (fixed in c2) |
| sbillboard, sbouncer, scarlton, scoco1/2, sgrow, sindy, sintercom, sinvite, sjacket, skout, sleonard, sload4, slook, smeanwhile4, spickupcoco2, spplate, sswap, stakeshorts, stanks, stony, stransition2/4, stubon, svideo4, swaiter, swateron, sbinoc, sexitbinoc | various | run clean: dialogue, comment, letterbox, transition, walk/rotate, hide/show, setchar, add, sound, anim2d, block/unblock, switch all work |
| scarlton_idle434, sfred_idle446/447, ssan_idle421, stimmy_idle443 | c4l3s4, c4l4s6, c4l4s7, c4l2s1, c4l4s3 | run with `@actor`; the port still has no idle driver (see c2 audit) |
| snojac | c4l2s3 | **stub `sb texture sponge`**; then `comment H000I` **not found** (global table) |
| scoatcheck | c4l4s4 | dialogue CAA runs; `texture SB spongetux` / `undo snojac` follow the dialogue (stub) |
| sshorts | c4l5s2 | `comment H000F` **not found** (global table); also `hide/show/setchar/transition` ok |
| snum1..9 | c4l2s2 | **stub `phone key n` (0x436640)** |
| sphone1 | c4l2s2 | **stub `phone reset` (0x436610)** |
| sphone2 | c4l2s2 | comment + sound ok |
| sphone3, sphone4 | c4l2s1, c4l2s2 | **stub `phone key 10`** (sphone3 also `talk` / `switch`) |
| schap4end | c4l5s4 | comment h454e, `transition close`, `movie mEnd`; the scene then keeps running (see End) |

## Commands / ScriptHost calls only c4 (or c4 + one other chapter) uses
c4-only commands: `phone` (snum1..9, sphone1..4), `undo` (snojac, scoatcheck). c4 + other: `texture` (c1), `block` (c1), `control` /
`unblock` (c2), `exitif` (c1). Other c4 commands (`narrate`, `startanim`, `move`, `climb`, `anim2d`, `walkgrid`) are implemented and
were exercised. Missing host calls hit: `actorSetTexture` (vt+0xf0, SceneScriptHost.cpp:217, stub), `miniPhoneKey` / `miniPhoneReset`
(:328-329, stubs; exe 0x436640 / 0x436610, mini game object ctor 0x4354d0, `phone` handler 0x440774). Attach/detach, kick, goal stubs are not used by c4.

## Phone mini-game entry (a Sonnet lane builds it)
Scene c4l2s2 (phone close-up, keypad hotspots 1..9) runs `snum<n>` -> `phone n` (ScriptRunner.cpp:625, handler 0x440774 ->
`miniPhoneKey(n)` -> exe 0x436640); `sphone1` -> `phone` reset (0x436610); `sphone3` / `sphone4` send `phone 10` (the dial / hang up key)
from c4l2s1 / c4l2s2. Shares the 0x5c-byte mini game object with slots/darts (`game+0x12c`).

## The end: original audit (fixed by lane c above)
* Chain in the data: c4l5s4 event -> `schap4end`: `comment h454e`, `wait`, `transition close`, `movie mEnd`. Nothing follows in the script.
* Exe (0x40bcc0, "movie finished" handler): in game mode, if the finished movie is `mend` (str 0x454690) it calls vt+0xb0 (`switchTo`)
  `"mcredits"`; if it is `mcredits` (0x454684) it sets `game+0x95 = 1` and calls vt+0xc0 (end the game, back to the main menu).
  View Movies (0x424d60 table, case 6 mend / 0xb mcredits) plays one movie then returns (vt+0xc8).
* Port: `Game::switchTo` plays the movie through `movieHook`; `StartupMain.cpp` game loop (lines ~512-517) only continues `game->chained`.
  No mend -> mcredits and no mcredits -> menu. Run with 2500 frames: after `movie mEnd` the c4l5s4 scene keeps running (fidgets) on the
  closed transition; the player is stuck. With movies on, mEnd (178 s) plays and then the same hang.
* View Movies: movie 11 (Credits) is always unlocked (MainMenu.cpp:189) and mCREDITS.bik (73 s, 640x480) / mEND.bik open and decode; movie 6 (mEnd)
  unlocks with `schap4end` seen. Menu path is fine; only the in-game chain is missing.
* Movies in `movies/`: mEND, mCREDITS and mCHAPTER4 all present.

## Prioritised fix list (lane-sized)
1. **FIXED — End chain (lane c)** - in the port's movie-finished path (`StartupMain.cpp` game loop / `Game`), port 0x40bcc0: after `mend` start
   `mcredits`; after `mcredits` end the game and return to the main menu (`game+0x95`, vt+0xc0). Also the c4 movie `schap4end` runs with
   `transition close` first (already ok). Verify by a real-time run and `SB_TEST_SCRIPT=60:schap4end`.
2. **Global comments H000A..X (small, all chapters)** - exe 0x410450 (loads `dialogue/comments.sb2`, rows whose id starts `H000`, kept in
   the comment list besides the chapter/level rows; 0x4106d0 is the per-scene load). Port `CommentTable::load` (Dialogue.cpp:156) only keeps
   `id[1]==chapter && id[2]==level`, so `H000F` "I don't wanna do that" (every refused item / unknown click, Game.cpp:312, Hud.cpp:276,
   Scene.cpp:479) and c4's `H000I` (coat comment) are missing. Needs the voice lookup too (Game.cpp:357).
3. **Phone mini-game (Sonnet lane, in progress)** - 0x4354d0 / 0x436610 / 0x436640 / 0x440774 (+ update hook 0x4355a0 / 0x435830);
   the c4l2s2 keypad scripts and the sphone result branch (sphone3 `talk`, `switch`).
4. **FIXED — `texture` (actorSetTexture, vt+0xf0, lane c)** - small/medium. c4 uses it for the coat swap (`snojac` sponge, `scoatcheck` spongetux, c1 for
   other swaps); the character's texture must be swapped (named texture of the model) and persist (`GameState::setTexture` exists for saves).
5. **Idle-script driver** (char update 0x42f1a0, flag +0x1962) - already listed in the c2 audit; c4 has five idle scripts (san 421, carlton 434,
   timmy 443, fred 446/447) that run only through the test hook.
6. **Verify in real play** - map hotspots (c4l1s1 "Loading..." plaque), the c4l3s8/c4l3s9 SpongeBob prop colours, c4l5s5 red wall outlines,
   c4l4s9 binoculars (script `sbinoc`/`slook` interaction), shadow blob drawing, click-per-character runs of the 14 c4 dialogue tables.
