# Agent brief: SpongeBob SquarePants: Employee of the Month (sb2.exe) port. Token budget is tight: be economical.

Project: faithful C++ reconstruction of the 2002 PC game (`sb2.exe`, MSVC 6, AWE Games' OMT 1.x engine
in `OMT.dll`) on our SDL2/gl4es engine port, for Android (AYN Thor). Repo: sirbilliams/SpongebobEOTM-Android.
Template: sirbilliams/neutron2-android (finished port of a later AWE game on OMT 2.x). Read
`docs/PLAN.md` and `docs/formats.md` first, and only the parts of other files you need.

## Inputs (read-only, shared)
- `/home/claude/decomp/sb2/`: `sb2.exe`, `raw/_free.c` (r2ghidra, every function, header
  `/* ==== 0xADDR  name  size N ==== */`), `raw/functions.json`, `abi.json` (calling conventions from the
  machine code), `strings.txt`.
- `/home/claude/decomp/omt/`: `OMT.dll`, `raw/<Class>.c` per class (from the mangled export names; methods
  only reached internally are in `_free.c`), `exports.json` (exports per class + sb2.exe's imports),
  `classmap.json` (class, in OMT 2.5?, in neutron2 engine?, sb2 imports, exports), `reach.json`.
- OMT 2.5 sources (LGPL, the base of `engine/`): `/home/claude/omt25/OMT/sources/OMTClasses`.
- neutron2-android checkout (reference, don't edit): `/home/claude/ref/neutron2-android`.
- Tools: `r2` (radare2 6.2.4 + r2ghidra; `r2 -q -c 'af @ A; pdg @ A' file`), python pefile/capstone/r2pipe.
  When the decompile is unclear, read the machine code (`pd`, capstone); x87 code is often decompiled wrong.
- Game: master install `/home/claude/game` (don't modify); the ISO `/home/claude/dl/sb.iso`.

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
  Claude-Session: https://claude.ai/code/session_014SPomGh3mhrgLMGv2VW9o9
- After each commit: `git push -f origin HEAD:backup/agent-<x>`. Never push main or other branches.
- Don't touch other agents' worktrees, build dirs, game copies or scratch. Machine: 2 CPUs, 7 GB:
  `make -j2`, never more; one build at a time.

## Your workspace (<x> = your letter)
- Worktree `/home/claude/wt/<x>` (branch `agent-<x>`). Build dir `<worktree>/build-<x>`. Game copy
  `/home/claude/game-<x>`. Scratch `/home/claude/scratch-<x>`. Xvfb display `:9<n>` (A=1, B=2, C=3;
  start it yourself: `Xvfb :91 -screen 0 1024x768x24 &`).
- Desktop build (no system SDL2 here; it's in /opt/sdl2, GL headers in /usr/local/include):
  `cmake -S engine -B build-<x> -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=/opt/sdl2 && make -C build-<x> -j2 <target>`
  Run with `LD_LIBRARY_PATH=/opt/sdl2/lib`. No ALSA: use `SDL_AUDIODRIVER=dummy`, or `SDL_AUDIODRIVER=disk
  SDL_DISKAUDIOFILE=out.raw` to capture the mixed audio for checking.
- The tree is seeded from neutron2-android: `engine/` is OMT 2.5 on SDL2/gl4es, `game/granny` the Granny
  1.2b replacement, `android/` and `.github/` the app and CI (still Negatron-named until lane C renames them).
  There is no `game/src` yet.
- Headless run from the game copy: `DISPLAY=:9<n> OMT_WINDOW_SIZE=640x480 OMT_FRAMES=<n>
  OMT_SCREENSHOT=x.bmp timeout 60 <build>/<binary>` (`OMT_SCREENSHOT` makes it exit). Success = "CLEAN EXIT".
- Testing economy: build + smoke test before each commit, plus one quick check that your change works.
- Report in <= 12 lines: commits, what works, what's left (with addresses), shared files touched.
