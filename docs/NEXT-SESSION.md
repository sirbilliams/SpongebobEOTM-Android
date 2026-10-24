# Next session

## State (Oct 24, session 1: phase 0)

- Phase 0 done: disc unpacked and installed, decompilation inputs for sb2.exe and OMT.dll, OMT.dll class map
  against OMT 2.5, script/dialogue/scene formats and the startup chain (`docs/formats.md`), plan with lanes
  and the percentage scale (`docs/PLAN.md`). **Percent to goal: 5%.**
- Repo holds docs and `tools/decomp` only; no code builds yet, so nothing is on `main`.
  Work is backed up on branch `backup/lead`.
- Waiting for Bill's go before starting agents (round 1 lanes A/B/C in `docs/PLAN.md`).

## Next steps

1. Round 1 (3 agents): A engine layer core (Opus), B Bink + Granny's 7 functions + sound/input (Sonnet),
   C repo base from neutron2-android: app id `com.sirbilliams.spongebob`, importer for this disc, CI,
   crash log (Sonnet). Lead merges via `integ`, builds, smoke-tests, pushes `main`.
2. Then startup + main menu, script VM, scene loader (round 2).
