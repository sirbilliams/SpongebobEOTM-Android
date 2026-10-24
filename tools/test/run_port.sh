#!/bin/bash
# Real-time capture of the port, the counterpart of tools/wine/run_original.sh (headless runs draw
# thousands of frames a second while voice clips play in real time, so frame-numbered OMT_SHOTS do not
# line up with the cut scenes; this screenshots the display every <interval> seconds instead).
#   tools/test/run_port.sh <negatron> <game dir> <out dir> <seconds> [interval=5] [display=:120] [ENV=value...]
# e.g. ... :120 OMT_FAKE_PAD="5:lefty=-32767" holds the stick forward from the start.
NEG=$(realpath "$1"); GAME=$2; OUT=$(mkdir -p "$3" && cd "$3" && pwd); T=$4; I=${5:-5}; DISP=${6:-:120}
shift $(( $# < 6 ? $# : 6 ))		# the rest are extra environment settings
rm -f "$OUT"/shot-*.png
pgrep -f "Xvfb $DISP " >/dev/null || { Xvfb "$DISP" -screen 0 640x480x24 >/dev/null 2>&1 & sleep 1; }
cd "$GAME" || exit 1
env DISPLAY=$DISP OMT_WINDOW_SIZE=640x480 OMT_FRAMES=2000000000 OMT_SCREENSHOT=/dev/null/never.bmp "$@" "$NEG" > "$OUT/port.log" 2>&1 &
P=$!
for ((t = I; t <= T; t += I)); do sleep "$I"; import -display "$DISP" -window root "$OUT/shot-$t.png" 2>/dev/null; done
kill $P 2>/dev/null; sleep 2; kill -9 $P 2>/dev/null
