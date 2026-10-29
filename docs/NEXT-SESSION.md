# Next session

## Local workflow

- Repo: `~/Projects/SpongebobEOTM-Android`, branch `integ` (integration). `main` builds the APK on push.
- Lanes: `~/Projects/sb-lanes/<x>` (one worktree per lane, branch `lane/<x>`). Merge `integ` into a lane before starting.
- Private files: `~/Projects/sbeotm-private` (game, decomp, ffmpeg, `briefs/lane-brief.md`, `run-lane.sh` for Codex). Never commit them.
- Build in the `omtbuild` distrobox with `-DFFMPEG_PREFIX=~/Projects/sbeotm-private/ffmpeg`.
- Runs: set `LD_LIBRARY_PATH` for the private ffmpeg. Xvfb runs only inside the box.
- Smoke test: `tools/test/smoke.sh`.

## Next steps (in progress)

1. Chapter 1 playthrough.
2. Chapter 2 playthrough.
3. Chapter 3 and chapter 4 audits.
4. Darts, kick and phone mini-games.
5. Bug sweep.
