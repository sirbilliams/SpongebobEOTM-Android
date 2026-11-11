# Agent brief: SpongeBob SquarePants: Employee of the Month (sb2.exe) port. Token budget is tight: be economical.

Project: faithful C++ reconstruction of the 2002 PC game (`sb2.exe`, MSVC 6, AWE Games' OMT 1.x engine
in `OMT.dll`) on our SDL2/gl4es engine port, for Android (AYN Thor). Repo: sirbilliams/SpongebobEOTM-Android.
Template: sirbilliams/neutron2-android (finished port of a later AWE game on OMT 2.x). Read
`docs/PLAN.md` and `docs/formats.md` first, and only the parts of other files you need.

## Inputs (local, git-ignored; paths relative to repo root)
- `.decomp/sb2/`: `sb2.exe`, `raw/_free.c` (r2ghidra, every function, header
  `/* ==== 0xADDR  name  size N ==== */`), `raw/functions.json`, `abi.json` (calling conventions from the
  machine code), `strings.txt`.
- `.decomp/omt/`: `OMT.dll`, `raw/<Class>.c` per class (from the mangled export names; methods
  only reached internally are in `_free.c`), `exports.json` (exports per class + sb2.exe's imports),
  `reach.json`. Historical `classmap.json` is not regenerated.
- OMT 2.5 sources (LGPL, the base of `engine/`): `engine/OMTClasses/`.
- Tools: `r2` + r2ghidra (`r2 -q -c 'af @ A; pdg @ A' file`), Python pefile/capstone/r2pipe.
  When the decompile is unclear, read the machine code (`pd`, capstone); x87 code is often decompiled wrong.
- Game: `.game/` (includes `scenes/`, `music/`, `movies/`, `sounds/`, `cd/sounds/help.omt`).
  Install from your own read-only disc: `tools/install-game.sh /path/to/game.iso .game` (Python 3 + unshield).
  Regenerate raw inputs with `python3 tools/decomp/decompile_pe.py .decomp/sb2/sb2.exe .decomp/sb2/raw`
  (likewise `.decomp/omt/OMT.dll` and `.decomp/omt/raw`). Never stage either local directory.

## Hard rules
- Never commit game files or bytes from the exe/DLLs (no tables, strings or images dumped in bulk).
  Values the code needs are read from the player's own files at run time. Small constants the code uses
  (a literal, a file name) are fine. No leaked source code of any kind; only the retail exe and DLLs.
- Faithful: same logic, same order, same quirks, comments where the original is odd. Every deviation is
  marked `// port:` with the reason. Each function's header comment gives its address.
- Builds for x86-64 Linux and arm64 Android: C++98-compatible style (no `auto`, range-for, `nullptr`,
  `T{...}`), no x86 asm, no Windows APIs, never pointers in int.
- Commit only what builds: `git -c user.name=sirbilliams -c user.email=bravehooves@gmail.com commit`,
  message ending with a blank line and:
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  Claude-Session: <current session URL supplied by Bill>
- Never push without Bill's OK. Don't touch other agents' worktrees, builds, game copies or scratch.

## Desktop workspace (omtbuild distrobox)
- Repo: `~/Projects/SpongebobEOTM-Android`, build: `build/`, installed game: `.game/`.
- Configure if needed: `cmake -S engine -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_PREFIX="$HOME/Projects/sbeotm-private/ffmpeg"`.
  Build: `make -C build -j6`.
- Check: `timeout 900 tools/test/smoke.sh build .game` (manages its own Xvfb and dummy audio),
  then `timeout 120 build/sb_unittest`. Smoke output: `.shots/smoke/summary.txt`.
- Testing economy: build + smoke test before each commit, plus one quick check that your change works.
- Report in <= 12 lines: commits, what works, what's left (with addresses), shared files touched.
