# Third-party software

The app is built from this repository plus three pinned libraries that `fetch-deps.sh` downloads at
build time (versions in `deps.lock`). Each licence below was copied from the library's own source at
the pinned commit. The full texts ship inside the APK (`assets/licenses/`) and are in this repository
at [`android/app/src/main/assets/licenses/`](android/app/src/main/assets/licenses/).

| Component | Version | Licence | Used for |
|---|---|---|---|
| [Open Media Toolkit (OMT)](#open-media-toolkit-omt-25) | 2.5, ported (`engine/`) | LGPL 2.1 or later | the game's engine |
| [SDL2](#sdl2) | 2.32.10 (`5d24957`) | zlib | window, OpenGL context, input, sound, Android glue |
| [HIDAPI](#hidapi-inside-sdl2) (inside SDL2) | as in SDL 2.32.10 | BSD-style (chosen of its three) | gamepads over USB / Bluetooth |
| [gl4es](#gl4es) | `a025615` | MIT | the engine's OpenGL 1.x on OpenGL ES 2 |
| [unshield](#unshield) | 1.5.1 (`871c2bf`) | MIT | installing the game from the CD's InstallShield cabinet |

zlib is not bundled: the app uses Android's own `libz` (the desktop build, the system's). The app's
text uses Android's system fonts; no font files are included.

## Open Media Toolkit (OMT) 2.5

Copyright Yves Schmid 1996-2003 (garagecube.com). Licensed under the GNU Lesser General Public
License, version 2.1 or (at your option) any later version: [`engine/LICENSE-LGPL.txt`](engine/LICENSE-LGPL.txt)
(also `assets/licenses/OMT-LGPL-2.1.txt` in the APK).

The engine in `engine/OMTClasses/` is OMT 2.5 modified for this port (moved from Windows / DirectX
onto SDL2, made 64-bit and ARM clean); `engine/README.md` lists the changes, and every file keeps
its original notice. The complete source of the app, including this modified OMT and the scripts to
build it (`README.md`, "Building"), is in this repository, so the engine can be changed and the app
rebuilt and relinked against it, as the LGPL asks.

## SDL2

Simple DirectMedia Layer, copyright (C) 1997-2025 Sam Lantinga. zlib licence:
`assets/licenses/SDL2-zlib.txt`. Unmodified; its Java glue is copied into the app by `fetch-deps.sh`.

## HIDAPI (inside SDL2)

Copyright (c) 2010, Alan Ott, Signal 11 Software. HIDAPI offers three licences (GPL 3, BSD-style, its
original licence); SDL ships it and this app uses it under the BSD-style licence:
`assets/licenses/HIDAPI-BSD.txt`.

## gl4es

Copyright (c) 2016-2018 Sebastien Chevalier, (c) 2013-2016 Ryan Hileman. MIT licence:
`assets/licenses/gl4es-MIT.txt`. Unmodified.

## unshield

Copyright (c) 2003 David Eriksson. MIT licence: `assets/licenses/unshield-MIT.txt`. Unmodified.

## Not included

- **The game.** No game files, and no data or code bytes from `Neutron2.exe` or its DLLs, are in this
  repository or the APK. The player installs their own copy from the game's CD image; the
  reconstructed code reads the values it needs from that copy at run time. *Jimmy Neutron* and all
  of the game's content belong to their owners (Nickelodeon / Viacom, THQ).
- **RAD Game Tools' Granny.** `game/granny/` is our own reimplementation of the few Granny 1.x
  functions the game calls; it reads the game's `.grn` files, and the player's own `granny.dll` as a
  data file (it is never loaded or run).
