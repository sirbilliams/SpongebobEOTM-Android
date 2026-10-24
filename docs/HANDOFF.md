# Handoff: where the project stands and how to continue

Read this file, then `docs/PLAN.md` and `docs/formats.md`.

## The project

Bill (GitHub `sirbilliams`) wants **SpongeBob SquarePants: Employee of the Month** (THQ / AWE Games, 2002,
`sb2.exe` on AWE's OMT 1.x engine `OMT.dll`) running as a native Android app on his **AYN Thor** (arm64,
dual screen 1920x1080 top + 1240x1080 bottom touch, gamepad). Faithful decompile-and-recompile; the player
installs from their own disc; the repo ships no game files.

- Repo: **github.com/sirbilliams/SpongebobEOTM-Android** (public; the session tool names it
  `sirbilliams/spongebobeotm-android`). Push to `main` only after a build + smoke test.
- Template: **sirbilliams/neutron2-android** (finished Negatron port: OMT 2.x on SDL2/gl4es, Granny 1.2b
  replacement, Android app, importer, CI, signing). Method docs: sirbilliams/neutron1-android `docs/`.
- Git identity: `git -c user.name=sirbilliams -c user.email=bravehooves@gmail.com commit`, message ending
  with the session's attribution lines.

### Bill's standing rules
- Never commit game files or bytes copied from the exe/DLLs; values are read from the player's own files.
  No show logos, characters or art in the repo or launcher.
- No leaked source code. Only the retail exe and DLLs.
- Faithful; every deviation marked `// port:` with the reason.
- ASan/UBSan desktop build before sending an APK. Send APKs and screenshots when there is something to see.
- Short status notes; "check in" / "percent to goal" = a brief answer (scale in `docs/PLAN.md`).
- Agents: Sonnet for porting lanes, Opus for engine core/renderer/VM design/debugging; at most 3 at once;
  own worktree, build dir, game copy, Xvfb display; reports <= 12 lines; push to `backup/agent-<x>`;
  merge via an `integ` branch.
- Blocked here (ask Bill): pushing tags, deleting branches, Actions secrets.

## Setting up a workspace (cloud session, Ubuntu 24.04)

Package registries (PyPI, the Ubuntu archive) were **blocked** in the Oct 24 session; GitHub works. So
everything was built from GitHub sources:
- `gh` works for the REST API once the repos are attached (`add_repo`); the env token alone is invalid.
- Game files: attach `sirbilliams/negatron-gamefiles` (read), then
  `gh api -H "Accept: application/octet-stream" repos/sirbilliams/negatron-gamefiles/releases/assets/665262492 > sb.iso`
  (sha256 starts `f55a47087b4bc2c8`, 729,520,128 bytes); OMT 2.5: asset 660762708 (`OMTFullV250.zip`).
- ISO: `python3 -I` + pycdlib (Joliet). `unshield` built from github.com/twogood/unshield (cmake).
  `unshield -d out x data1.cab` gives `Program_Executable_Files/` (the game) and `Txt_Files/`.
- Master install `/home/claude/game` = `Program_Executable_Files/` + `movies/` (the CD's `Movies/`) +
  `cd/sounds/help.omt`.
- Python libs: pefile, pycdlib, r2pipe copied from their GitHub sources into site-packages; capstone 6.0
  built with cmake (`-DCAPSTONE_BUILD_SHARED_LIBS=ON`, target `capstone_shared`) and its
  `bindings/python/capstone` copied in with `lib/libcapstone.so*`. unicorn not built yet (not needed so far).
- radare2 6.2.4 (`--prefix=/opt/r2`) and r2ghidra `c1287cc`: run `make -C subprojects` first (fetches
  ghidra-native and zlib), and pass `PKG_CONFIG_PATH=/opt/r2/lib/pkgconfig PATH=/opt/r2/bin:$PATH
  LD_LIBRARY_PATH=/opt/r2/lib` to configure **and** make. Wrappers `/usr/local/bin/r2` and
  `/usr/local/bin/radare2` (r2pipe looks for `radare2`) set `LD_LIBRARY_PATH`.
- Background jobs: use the tool's background mode; `&`/nohup jobs are killed after the turn ends.

## Decompilation inputs (derived from the exe/DLL; never in this repo)

Work folder `/home/claude/decomp/{sb2,omt}`. Backup: `decomp/sb-decomp-inputs.tar.gz` in
sirbilliams/negatron-gamefiles. To regenerate:
1. `python3 -I tools/decomp/decompile_pe.py sb2.exe raw/` and the same for `OMT.dll`. It seeds functions
   `aaa` misses (call targets, exports, then gap filling: about half of sb2.exe's code is reached only
   through function pointers) and groups OMT.dll methods per class from the mangled export names.
   sb2.exe: 1082 functions (~10 min); OMT.dll: 4422 functions (~1 h).
2. `python3 -I tools/decomp/omt_exports.py OMT.dll sb2.exe exports.json`.
3. `python3 -I tools/decomp/omt_reach.py OMT.dll exports.json reach.json` (lower bound of used OMT code).
4. `python3 -I <neutron1>/tools/decomp/abi.py sb2.exe raw/functions.json abi.json`.
5. `rtti.py` finds nothing useful: neither binary has RTTI for its own classes.

## Status

See `docs/NEXT-SESSION.md`.
