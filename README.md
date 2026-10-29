# SpongeBob SquarePants: Employee of the Month for Android

A free, open-source fan preservation port of the 2002 PC game *SpongeBob SquarePants: Employee of the
Month* (THQ / AWE Games) to Android, made for the AYN Thor handheld.

It is a faithful reconstruction of the game's own code, decompiled from the retail executable and engine
DLL, running on an SDL2 / OpenGL ES port of the engine. **No game files are included**: you install the game
from your own original CD (an `.iso` image of it), and the app reads everything it needs from your copy.

Not affiliated with or endorsed by THQ, AWE Games, Nickelodeon or the rights holders.

## Status

About 55% of the way to the goal (see `docs/PLAN.md` for the milestones).

- Done: start-up, the logo movies (with sound), and the main menu, on desktop and in the Android build.
- Done: the chapter 1 opening is playable, with 3D characters, dialogue, the inventory bar, saves and walking.
- In progress: playing chapter 1 through to its end. Chapters 2 to 4, the end movie and credits, and the
  minigames are not done yet.

## Features

- Dual-screen companion panel on the AYN Thor's bottom screen. The game stays on the top screen, and the
  panel shows the game state and lets you tap dialogue choices and items. It is drawn in code, with no show art.
- Widescreen setting with four modes: Fill (default, scales to the width and pans vertically), Side panels
  (4:3 picture with code-drawn side panels), Stretch, and Off.
- Touch controls: one finger acts as the mouse. The right stick moves a cursor, and R3 or A is the left
  click. The right-stick cursor can be turned off in the launcher.
- Save games: the game's own save and load screens, plus quick save and resume.

## Installing

1. Download `SpongeBob-EOTM.apk` from the **latest** pre-release on the
   [Releases page](https://github.com/sirbilliams/SpongebobEOTM-Android/releases/tag/latest). It is rebuilt
   on every push to `main`.
2. Install it on your Android device.
3. Start the app and import your own disc image (`.iso`) of the game when it asks. The app does not
   download or include the game.

## Building

**Desktop** (for development and testing), in an Ubuntu 24.04 container with the build tools and FFmpeg:

```
cmake -S engine -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_PREFIX=<ffmpeg-prefix>
cmake --build build -j2 --target spongebob
```

`<ffmpeg-prefix>` is the folder of your minimal FFmpeg build (the Bink demuxer and decoders are needed).

**Android:** run `bash build-android.sh`. It needs `podman` and builds the SDK and NDK image on the first run
(about 3 GB). The APK ends up in `~/SpongeBob-Android/SpongeBob-EOTM.apk`. The GitHub workflow in
`.github/workflows/android.yml` builds the same APK with the same SDK and NDK versions.

Dependencies are pinned in `deps.lock` and fetched by `fetch-deps.sh`.

## Credits and licences

The engine is a port of the Open Media Toolkit (LGPL). The other third-party components (SDL2, gl4es,
FFmpeg, and others) and their licences are listed in [`THIRD_PARTY.md`](THIRD_PARTY.md).
