# Testing

Game files are never in the repo, so CI can only build and run what needs none of them. Everything else is
the local smoke test.

## CI (`.github/workflows/android.yml`, desktop job `linux`)
Builds every target (`cmake --build build`) and runs `build/sb_unittest`
(`engine/tests/sb_unittest.cpp`): the script command table, the tab-delimited record tokenizer
(`ScriptRunner::parseRecords`, field cutting, the 50-line limit) and `loadText` on a synthetic script string.
Add pure-logic checks there; anything that reads `.game` belongs in the smoke test.

## Local smoke test
Run inside the `omtbuild` distrobox, with a full build (`make -C build -j6`) and the installed game:

    tools/test/smoke.sh [--asan] [--update-baseline] <build dir> <game dir> [out dir]
    distrobox enter omtbuild -- bash -lc 'cd <repo> && tools/test/smoke.sh build .game'

It prints `PASS`/`FAIL` per step and a `SUMMARY:` line; exit 0 only if everything passed (about 2 minutes).
Output goes to `<out dir>` (default `.shots/smoke`, git-ignored): `<step>.log`, `<step>.png`, `contact.png`
(a contact sheet of all screenshots), `todo.txt`, `summary.txt`.

Steps:

| step | what |
|---|---|
| scntest | `sb_scntest`: all 123 scenes parse, `TOTAL 123/123` |
| scripttest | `sb_scripttest`: every `.sb2` and dialogue table, 0 errors |
| scenetest | `sb_scenetest` 450 frames at 640x480 (scene runtime, dialogue, no crash, shots written) |
| savefmt | `SB_TEST_SAVEFMT=1 spongebob`: save-file round trip |
| logos-menu | `spongebob` at 1920x1080 with `SB_SKIP_MOVIES=0` (the only run that plays the logo movies) up to the menu |
| newgame-c1l0s1 / scene-c1l2s6 | New Game, then `SB_TEST_SCENE=c1l2s6` |
| click-dialogue | click on Larry (c1l5s4) starts dialogue AAA |
| hud-item | `SB_TEST_ITEMS` + clicks: the pie is used on Larry, dialogue BAA |
| wide-fill / stretch / panels / fit | `SB_WIDESCREEN=<mode>` once each |
| pad, touch | `SB_TEST_PAD` / `SB_TEST_TOUCH` hooks (`OMediaEventManager.cpp`) |
| touchpad | `SB_TEST_TOUCHPAD="frame:dx,dy,btn;..."`: the bottom-screen touchpad's relative cursor path (docs/bottom-touch.md); a click on Larry (c1l5s4) starts dialogue AAA |
| todo-baseline | no `TODO(` log line that is not in `tools/test/todo-baseline.txt` |

Every game run (own Xvfb display `:85`, started if absent, `SB_SMOKE_DISPLAY` to change; `SDL_AUDIODRIVER=dummy`;
`LD_LIBRARY_PATH=$FFMPEG_LIB`, default `$HOME/Projects/sbeotm-private/ffmpeg/lib`) must exit 0, print no
`*** CRASH` backtrace or sanitizer report, save its screenshot and print its expected log lines. The pad and
touch hooks log nothing, so those two only prove the run survives them (look at their screenshots).

When a `TODO(` line disappears or a new one is intended, run once with `--update-baseline` and commit the file.
`--asan` runs the same steps with `<repo>/build-asan` (ASan + UBSan, built on first use); `runtime error:` and
`AddressSanitizer` lines fail the step.

The dialogue and HUD click replays use `SB_TEST_READY_FRAMES=1`: both the capture counter and click
schedule start after a scene has entered, its transition bars are open, and its first frame has been
drawn. Startup/loading buffer flips cannot exhaust the screenshot budget before the clicks run.
The dialogue step also requires the `test: scene c1l5s4 ready` marker, in addition to the original
Larry event, dialogue AAA and line AZA assertions. The external per-run timeout still bounds startup.

## Collected-item regression (issue #2)

`sb_unittest` covers scene visibility with collected/uncollected inventory pickups,
empty/true/false authored conditions, case-insensitive progress names, an empty
inventory bar after consumption, new-game reset, and unchanged non-pickup behavior.
`sb_save_unittest` needs no game assets: it verifies the existing save format round
trip and a collected item restored from disk with an empty inventory bar.

Before releasing, use the Android launcher and a legally installed game:

1. Export existing saves in the launcher, then install the candidate APK over v1.0
   without uninstalling. Confirm the launcher still finds the game and saves.
2. Play / Resume. Collect the Dirty Plate (`ikp4`) in the Krusty Krab dining room
   (`c1l2s1`). Walk to Other Side and back to Hungry Customer. Check the plate is
   absent both visually and from REVEAL/click targets, and exists only once in Pocket.
3. Save and reload, then quit/relaunch and Resume: the plate stays absent.
4. Use it on the Sink; revisit the dining room. It must remain absent after use.
   Repeat with Plain Patty and Fixin's after combining them.
5. Load a pre-pickup save or start a new game: uncollected items must be available.
   Check a script-controlled item and the playerless oxygen-tank scene (`c3l6s3`).

The issue report names no item, scene, or save. Unit/save tests establish the code
path, but do not replace this gameplay check or prove every item is affected.
