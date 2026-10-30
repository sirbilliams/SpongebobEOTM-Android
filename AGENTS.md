# SpongeBob EOTM Android
Faithful preservation port of the 2002 PC game for Android/AYN Thor.
## Layout
`engine/`: OMT engine reconstruction and CMake build. `game/src/`: function-by-function reconstruction. `android/app/`: Gradle Android project. `docs/`: plans and screenshots. `tools/`: dev utilities and testers. `deps.lock/`: pinned dependencies.
## Build
Desktop: `cmake -S engine -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_PREFIX=<path>`; `cmake --build build -j2 --target spongebob`.
Android: `bash build-android.sh` (requires podman). Produces APK at `~/SpongeBob-Android/SpongeBob-EOTM.apk`.
## Test
Smoke suite: `tools/test/smoke.sh` (16 steps, scene/script/save verification).
Desktop tests: `build/sb_unittest` (72 checks). CI workflow `.github/workflows/android.yml` builds APK on push.
## Rules
Never commit game files, ROMs, or ISOs; app requires user-provided disc image.
Engine is LGPL; respect third-party licenses. No show art in repo (README screenshots are exception).
Build uses minimal FFmpeg for Bink demux/decoders only. Avoid 16-bit GL contexts on Android.
## Release
APK generated via GitHub Actions on every push to `main`. Pre-releases tagged `latest` on the Releases page.
