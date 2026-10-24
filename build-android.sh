#!/bin/bash
# Builds the Neutron vs Negatron Android APK on your own Linux PC with podman (no Android Studio needed).
#
#   bash build-android.sh
#
# The build image (Android SDK + NDK, ~3 GB) is created on first run. The APK ends up in
# ~/Negatron-Android/Neutron-vs-Negatron.apk. Game files are NOT part of the build - see README.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HOME/Negatron-Android"
IMAGE=negatron_android_sdk34_ndk26

step() { printf '\n\033[1;32m==> %s\033[0m\n' "$*"; }
die()  { printf '\n\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

command -v podman >/dev/null || die "podman not found"
command -v git >/dev/null || die "git not found"

if ! podman image exists "localhost/$IMAGE"; then
    step "Building the Android SDK image (first run only)"
    podman build -t "$IMAGE" -f "$ROOT/Containerfile.android" "$ROOT" || die "couldn't build the image"
fi

step "Fetching SDL2 + gl4es (pinned)"
bash "$ROOT/fetch-deps.sh"

step "Building the APK"
podman run --rm -v "$ROOT":/src:Z -v negatron-gradle-cache:/root/.gradle "$IMAGE" \
    bash -c "cd /src/android && chmod +x gradlew && ./gradlew --no-daemon assembleDebug" \
    > "$ROOT/android-build.log" 2>&1 || die "build failed - see $ROOT/android-build.log"

mkdir -p "$OUT"
cp "$ROOT/android/app/build/outputs/apk/debug/app-debug.apk" "$OUT/Neutron-vs-Negatron.apk"
step "Done: $OUT/Neutron-vs-Negatron.apk"
