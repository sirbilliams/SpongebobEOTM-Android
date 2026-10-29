#!/bin/sh
# Desktop check of the bottom-screen companion panel: renders the real CompanionView.java (with a small
# Java2D stand-in for the android.graphics / org.json calls it makes, tools/companion/shim) from a snapshot.
# Usage: tools/companion/render.sh <snap.json|-> <out.png> [pad|pad:tap|pad:drag|pad:hold|page:map|page:talk|page:goals[:pad]|combine|combine:mid|maptap|tabtap]      (- = "no game yet"; samples/inventory.json shows item pictures; pad = the Touchpad layout)
set -e
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../android/app/src/main/java/com/sirbilliams/spongebob/CompanionView.java
build=${TMPDIR:-/tmp}/sb-companion-render
rm -rf "$build"; mkdir -p "$build"
javac -nowarn -d "$build" $(find "$here/shim" -name '*.java') "$src" "${src%CompanionView.java}TouchTestView.java"
# stand-in item pictures for samples whose "icons" is @ICONS@ (a real snapshot from the game names the game's own picture folder)
java -Djava.awt.headless=true ${SB_JAVA_OPTS:-} -cp "$build" com.sirbilliams.spongebob.SampleIcons "$build/icons"
snap=$1
if [ "$snap" != "-" ] && grep -q '@ICONS@' "$snap"; then snap=$build/snap.json; sed "s#@ICONS@#$build/icons#" "$1" > "$snap"; fi
SB_PANEL_CLOCK=${SB_PANEL_CLOCK:-1500} java -Djava.awt.headless=true ${SB_JAVA_OPTS:-} -cp "$build" com.sirbilliams.spongebob.RenderPanel "$snap" "$2" ${3:-}
