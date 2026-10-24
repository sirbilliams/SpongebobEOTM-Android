# Next session

## State (Oct 24, session 1: phase 0)

- Phase 0 done: disc unpacked and installed, decompilation inputs for sb2.exe and OMT.dll, OMT.dll class map
  against OMT 2.5, script/dialogue/scene formats and the startup chain (`docs/formats.md`), plan with lanes
  and the percentage scale (`docs/PLAN.md`). **Percent to goal: 5%.**
- Repo holds docs and `tools/decomp` only; no code builds yet, so nothing is on `main`.
  Work is backed up on branch `backup/lead`.
- Round 1 started (Bill's go, Oct 24 02:07). Seed commit 131e21a (`backup/seed`): the neutron2-android tree
  without Negatron's game/src. Worktrees `/home/claude/wt/{a,b,c}`, game copies `/home/claude/game-{a,b,c}`,
  branches `backup/agent-{a,b,c}`: A engine layer (Opus), B Bink/FFmpeg + 7 Granny functions (Sonnet),
  C app id/naming, importer, placeholder game target, crash log, CI (Sonnet).
- Desktop deps built from source: SDL2 2.32.10 in /opt/sdl2 (`-DCMAKE_PREFIX_PATH=/opt/sdl2`), GL headers
  in /usr/local/include (gl4es's gl.h + Khronos glext/EGL/KHR), libGL.so/libEGL.so symlinks. No ALSA.

## Next steps

1. Round 1 (3 agents): A engine layer core (Opus), B Bink + Granny's 7 functions + sound/input (Sonnet),
   C repo base from neutron2-android: app id `com.sirbilliams.spongebob`, importer for this disc, CI,
   crash log (Sonnet). Lead merges via `integ`, builds, smoke-tests, pushes `main`.
2. Then startup + main menu, script VM, scene loader (round 2).
