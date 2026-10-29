#!/bin/sh
# Desktop check of the bottom-screen companion panel: renders the real CompanionView.java (with a small
# Java2D stand-in for the android.graphics / org.json calls it makes, tools/companion/shim) from a snapshot.
# Usage: tools/companion/render.sh <snap.json|-> <out.png>      (- = "no game yet")
set -e
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../android/app/src/main/java/com/sirbilliams/spongebob/CompanionView.java
build=${TMPDIR:-/tmp}/sb-companion-render
rm -rf "$build"; mkdir -p "$build"
javac -nowarn -d "$build" $(find "$here/shim" -name '*.java') "$src"
SB_PANEL_CLOCK=${SB_PANEL_CLOCK:-1500} java -Djava.awt.headless=true -cp "$build" com.sirbilliams.spongebob.RenderPanel "$1" "$2"
