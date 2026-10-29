#!/bin/sh
# Render the actual Android Canvas launcher using the companion's Java2D graphics shim.
# Usage: tools/launcher/render.sh [output-directory]
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build=$(mktemp -d /tmp/sb-launcher-render.XXXXXX)
trap 'rm -rf "$build"' EXIT
out=${1:-"$here/../../.shots"}
mkdir -p "$out"
javac -d "$build" $(find "$here/../companion/shim/android/graphics" -name '*.java' ! -name Typeface.java) "$here/shim/android/graphics/Typeface.java" \
 "$here/../companion/shim/android/content/Context.java" \
 "$here/../companion/shim/android/os/SystemClock.java" \
 "$here"/shim/android/view/*.java "$here"/shim/com/sirbilliams/spongebob/*.java \
 "$here/../../android/app/src/main/java/com/sirbilliams/spongebob/LauncherView.java"
java -Djava.awt.headless=true -cp "$build" com.sirbilliams.spongebob.VerifyLauncher
for page in main play import settings test about; do
 java -Djava.awt.headless=true -cp "$build" com.sirbilliams.spongebob.RenderLauncher "$page" "$out/launcher-$page.png"
done
