# Lane d: white-blob investigation and dialogue smoke timing (2026-10-28)

Base: `38261c9`, display `:84`, inside the `omtbuild` container, Mesa llvmpipe.

## Rendering: not reproduced, no renderer change

`SB_SKIP_MOVIES=1 SB_TEST_NEWGAME=1 SB_TEST_SCENE=c1l5s7`, 1920x1080:
initial captures at 300 and 800 buffer flips, scene-ready captures at 8 and 30 flips,
and two successive re-entries from c1l5s4 all show Ray, his chair/towel and the oil item
without a large white blob. `.shots/blob/before.png` and `.shots/blob/after.png` are
investigation captures, **not evidence of a rendering fix**. `later.png`, `reentry.png`,
`ready-before.png`, `asan.png` and corresponding logs provide additional captures.
The scene-ready timing option is the only behavior change in these runs.

Reviewed sb2.exe `0x403e10` BindTextures (named material cookie), `0x4039c0` BuildShape
(per-state material and triangle ordering), and OMT.dll `0x10059a40` DX rasterize
(texture/alpha state). Temporary material tracing in c1l5s7 showed:

| Object | Granny name -> OMT material | Alpha | Canvas |
| --- | --- | --- | --- |
| SpongeBob | sponge -> sponge | 1 | present |
| Ray | Ray -> Ray | 1 | present |
| Both character shadow quads | shadow -> shadow | 0.472222 | present |
| `isl1` oil item | cookingoil -> cookingoil | 1 | present |

There was no unmatched material in this scene. Temporary tracing was removed; the
trace remains in `.shots/blob/trace.log`. No speculative alpha, shadow or binding
change was made. The reported defective frame/revision or additional reproduction
conditions are still needed to identify its object and prove a faithful root fix.

## All-scene scan

`.shots/blob/scan.py` ran a separate spongebob process for each of the 123 scene files,
1920x1080, eight capture flips after a rendered scene with open transition bars.
All 123 exited 0, saved PNGs, and logged readiness for the requested scene.
`.shots/blob/scan/results.json` records the results. The scan extracts exact RGB
255/255/255 runs from 24-bit BMPs, joins vertically overlapping runs (4-connected),
and flags components of at least 10,000 pixels. Inspection of the only flags:

| Scene | Component areas | Interpretation |
| --- | --- | --- |
| c3l2s3 | 15,840; 13,482 | White tiles in the checkerboard floor artwork |
| c3l2s5 | 86,373; 22,815 | White rings in the dartboard artwork |

No other scene met the threshold. This is a short initial-state scan, not a proof
about every animation, interaction, GPU backend or saved state.

## Smoke timing change

`SB_TEST_READY_FRAMES=1` holds the SDL capture counter until StartupMain has drawn a
scene frame after entry with transition bars open. Click replay frame numbers also
start from that ready frame. click-dialogue and hud-item opt in; click-dialogue adds
a required ready marker and retains the Larry event, AAA dialogue and AZA line
assertions. The original screenshot, crash/sanitizer and timeout checks remain.

## Validation

- All desktop targets built with `cmake --build build -j6`; final log `.shots/blob/build-final.log`.
- All sanitizer targets built with ASan/UBSan and frame pointers; final log `.shots/blob/asan-build-final.log`.
- Final c1l5s7 sanitizer run: exit 0, 30 scene-ready capture flips at 1920x1080,
  no ASan/UBSan/crash diagnostics (`.shots/blob/asan-final.log`). Leak detection disabled,
  matching the existing smoke sanitizer configuration.
- `sb_unittest`: 72 checks, 0 failed; shell syntax and `git diff --check` clean.
- Three consecutive smoke runs, each `SUMMARY: 16 passed, 0 failed`, in
  `.shots/smoke-1`, `.shots/smoke-2`, `.shots/smoke-3`; full console logs in `.shots/blob/`.
- No commit made (lead commits).
