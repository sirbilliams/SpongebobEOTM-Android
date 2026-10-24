#!/bin/bash
# Fetches the pinned SDL2, gl4es and unshield (InstallShield cabinet reader, MIT) sources into android/app/jni/ and SDL's Java glue into the app.
# Safe to re-run; already-fetched sources at the right commit are kept.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$ROOT/deps.lock"
JNI="$ROOT/android/app/jni"

fetch() {   # repo ref expected-commit dest
    local repo=$1 ref=$2 want=$3 dest=$4
    if [ -d "$dest/.git" ] && [ "$(git -C "$dest" rev-parse HEAD)" = "$want" ]; then
        echo "ok   $(basename "$dest") @ ${want:0:10}"; return
    fi
    rm -rf "$dest"
    git init -q "$dest"
    git -C "$dest" remote add origin "$repo"
    git -C "$dest" fetch -q --depth 1 origin "$ref"
    git -C "$dest" checkout -q FETCH_HEAD
    local got; got="$(git -C "$dest" rev-parse HEAD)"
    [ "$got" = "$want" ] || { echo "ERROR: $repo $ref is $got, expected $want" >&2; exit 1; }
    echo "got  $(basename "$dest") @ ${want:0:10}"
}

fetch "$SDL_REPO"   "$SDL_REF"       "$SDL_COMMIT"   "$JNI/SDL"
fetch "$GL4ES_REPO" "$GL4ES_COMMIT"  "$GL4ES_COMMIT" "$JNI/gl4es"
fetch "$UNSHIELD_REPO" "$UNSHIELD_REF" "$UNSHIELD_COMMIT" "$JNI/unshield"

# SDL's Java side (SDLActivity etc.) must match the native SDL version
JAVA="$ROOT/android/app/src/main/java/org/libsdl/app"
rm -rf "$JAVA"; mkdir -p "$JAVA"
cp "$JNI/SDL/android-project/app/src/main/java/org/libsdl/app/"*.java "$JAVA/"
echo "ok   SDL Java glue -> android/app/src/main/java/org/libsdl/app"
