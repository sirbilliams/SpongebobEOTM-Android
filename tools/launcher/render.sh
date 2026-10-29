#!/bin/sh
# Render the actual Android Canvas launcher with a Java2D graphics shim (tools/companion's, plus tools/launcher/shim overrides).
# Usage: tools/launcher/render.sh [output-directory]   (default .shots/launcher2)
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build=$(mktemp -d /tmp/sb-launcher-render.XXXXXX)
trap 'rm -rf "$build"' EXIT
out=${1:-"$here/../../.shots/launcher2"}
mkdir -p "$out"
own=$(cd "$here/shim/android/graphics" && ls *.java)
companion=""
for f in "$here"/../companion/shim/android/graphics/*.java; do
 case " $(echo $own) " in *" $(basename "$f") "*) ;; *) companion="$companion $f";; esac
done
javac -nowarn -d "$build" $companion "$here"/shim/android/graphics/*.java \
 "$here/../companion/shim/android/content/Context.java" \
 "$here"/shim/android/os/*.java "$here"/shim/android/view/*.java "$here"/shim/com/sirbilliams/spongebob/*.java \
 "$here/../../android/app/src/main/java/com/sirbilliams/spongebob/LauncherView.java"
timeout 120 java -Djava.awt.headless=true -cp "$build" com.sirbilliams.spongebob.VerifyLauncher
timeout 300 java -Djava.awt.headless=true -cp "$build" com.sirbilliams.spongebob.RenderLauncher "$out"
