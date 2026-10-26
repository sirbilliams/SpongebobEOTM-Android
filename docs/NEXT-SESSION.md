# Next session

## State (Oct 26, session 2)

- Percent to goal: ~7%. `main` = 6532c65: seed + lane C (app `com.sirbilliams.spongebob`, ISO importer
  `sb_import` (output byte-identical to the reference install), placeholder game screen, crash log, CI).
  First APK on the "latest" release, signed with a one-off debug key: the repo still has no
  NEG_KEYSTORE_B64 / NEG_KEYSTORE_PASSWORD secrets (Bill to add).
- Oct 24 round 1 stopped on the usage limit; resumed Oct 26.
- Known: UBSan flags OMediaGLRenderer's constructor passing its own unset `zbuffer` to the base (OMT 2.5
  quirk, not a use-after-free); lane A to clean up.

## Plan (Bill asked for 5-8 steps, Oct 26)

1. Movies (Bink via minimal LGPL FFmpeg) + the 7 missing Granny functions: lane B, running.
2. OMT.dll 2D/database engine layer + omt1_view tool: lane A (Opus), running.
3. Startup from sb2.exe (exe data at run time, app/display init, scene-switch chain): lane C, running.
4. Main menu; APK 2 = logos + menu on the Thor (35%).
5. Script interpreter + dialogue (Opus).
6. Scene loader + 3D world: first scene c1l0s1.
7. APK 3: first scene playable on the Thor (45%).

Worktrees `/home/claude/wt/{a,b,c}`, game copies `/home/claude/game-{a,b,c}`, branches `backup/agent-{a,b,c}`.
Lead merges into `integ`, builds, smoke-tests (+ ASan/UBSan build `build-asan`), backs up to `backup/lead`.
**Bill (Oct 26): push `main` (= build the release APK) once, at the end of the session limit**, not after every merge.

Done this session: step 1 (Bink/FFmpeg + Granny, on main 594dc97); new launcher icon (tools/icon/make_icon.py,
main efa6c11, first build signed with the NEG_KEYSTORE secrets); lane B's OMT sound manager + input driver
(engine/omt1, touch = absolute pointer, Bink audio through the mixer) merged into integ aa98eef.
Running: A engine 2D layer (Opus), C startup (Sonnet), B script interpreter + dialogue (Opus, step 5).
Desktop deps: SDL2 2.32.10 in /opt/sdl2 (`-DCMAKE_PREFIX_PATH=/opt/sdl2`), GL headers in /usr/local/include.
