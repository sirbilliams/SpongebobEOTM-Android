#!/bin/bash
# Regression smoke test for the port. Needs the installed game, so it never runs in CI (CI builds and runs
# the game-file-free sb_unittest only). Run it inside the omtbuild distrobox.
#   tools/test/smoke.sh [--asan] [--update-baseline] <build dir> <game dir> [out dir]
# --asan             use <repo>/build-asan (configured and built here if missing) and fail on sanitizer reports
# --update-baseline  rewrite tools/test/todo-baseline.txt from this run's TODO( log lines
# Env: SB_SMOKE_DISPLAY (default :85, its own Xvfb, started here if absent), FFMPEG_LIB (default
#      $HOME/Projects/sbeotm-private/ffmpeg/lib), SB_SMOKE_TIMEOUT per game run in seconds (default 240).
# Output: <out dir>/ (default <repo>/.shots/smoke): <step>.log, <step>.png, contact.png, todo.txt, summary.txt.
# Exit 0 only if every step passed.
set -u
ASAN=0; UPD=0
while [ $# -gt 0 ]; do case "$1" in --asan) ASAN=1; shift;; --update-baseline) UPD=1; shift;; *) break;; esac; done
[ $# -ge 2 ] || { sed -n 2,11p "$0"; exit 2; }
REPO=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=$(realpath "$1"); GAME=$(realpath "$2"); OUT=${3:-$REPO/.shots/smoke}
if [ $ASAN = 1 ]; then
  BUILD=$REPO/build-asan
  if [ ! -x "$BUILD/spongebob" ]; then
    cmake -S "$REPO/engine" -B "$BUILD" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFMPEG_PREFIX="${FFMPEG_PREFIX:-$HOME/Projects/sbeotm-private/ffmpeg}" \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_C_FLAGS="-fsanitize=address,undefined" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" >/dev/null && make -C "$BUILD" -j6 >/dev/null 2>&1
  fi
  export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1
fi
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd); rm -f "$OUT"/*.log "$OUT"/*.png "$OUT"/*.bmp "$OUT"/todo.txt
DISP=${SB_SMOKE_DISPLAY:-:85}; TMO=${SB_SMOKE_TIMEOUT:-240}
FFLIB=${FFMPEG_LIB:-$HOME/Projects/sbeotm-private/ffmpeg/lib}
BASE=$REPO/tools/test/todo-baseline.txt
PASS=0; FAIL=0; FAILED=""
XPID=""
if [ ! -e "/tmp/.X11-unix/X${DISP#:}" ]; then
  Xvfb "$DISP" -screen 0 1920x1080x24 >/dev/null 2>&1 & XPID=$!; sleep 2
fi
cleanup() { [ -n "$XPID" ] && kill $XPID 2>/dev/null; }
trap cleanup EXIT

result() {   # result <name> <ok 0|1> <detail>
  if [ "$2" = 0 ]; then PASS=$((PASS+1)); printf 'PASS  %-16s %s\n' "$1" "$3"
  else FAIL=$((FAIL+1)); FAILED="$FAILED $1"; printf 'FAIL  %-16s %s\n' "$1" "$3"; fi
}

# ---- tool steps (no display)
t=$("$BUILD/sb_scntest" "$GAME/scenes" 2>&1 | tail -1); echo "$t" > "$OUT/scntest.log"
if echo "$t" | grep -q '^TOTAL 123/123 passed, 0 failed'; then result scntest 0 "123/123 scenes"; else result scntest 1 "$t"; fi

"$BUILD/sb_scripttest" "$GAME" > "$OUT/scripttest.log" 2>&1; rc=$?
if [ $rc = 0 ] && grep -q '^RESULT: OK' "$OUT/scripttest.log" && grep -q 'parse errors 0' "$OUT/scripttest.log"; then result scripttest 0 "0 errors"
else result scripttest 1 "rc=$rc $(grep -E 'RESULT|parse errors' "$OUT/scripttest.log" | tr '\n' ' ')"; fi

mkdir -p "$OUT/scenetest"
DISPLAY=$DISP SDL_AUDIODRIVER=dummy OMT_WINDOW_SIZE=640x480 timeout "$TMO" "$BUILD/sb_scenetest" "$GAME" 450 "$OUT/scenetest" > "$OUT/scenetest.log" 2>&1; rc=$?
shots=$(ls "$OUT/scenetest" | wc -l)
if [ $rc = 0 ] && ! grep -q '\*\*\* CRASH' "$OUT/scenetest.log" && [ "$shots" -ge 3 ] && grep -q '^stub calls' "$OUT/scenetest.log"; then result scenetest 0 "450 frames, $shots shots"
else result scenetest 1 "rc=$rc shots=$shots"; fi
for b in "$OUT"/scenetest/*.bmp; do [ -e "$b" ] && convert "$b" "${b%.bmp}.png" && rm -f "$b"; done

# ---- the real game
export LD_LIBRARY_PATH="$FFLIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
t=$(cd "$GAME" && SB_TEST_SAVEFMT=1 OMT_DATA="$GAME" DISPLAY=$DISP SDL_AUDIODRIVER=dummy timeout 60 "$BUILD/spongebob" 2>&1); rc=$?
echo "$t" > "$OUT/savefmt.log"
if [ $rc = 0 ] && echo "$t" | grep -q 'selftest: round trip OK'; then result savefmt 0 "round trip OK"; else result savefmt 1 "rc=$rc"; fi

# game <name> <frames> <expect regex|-> [ENV=value ...]: 1920x1080 run, screenshot at the last frame
game() {
  local n=$1 fr=$2 exp=$3; shift 3
  ( cd "$GAME" && env DISPLAY=$DISP SDL_AUDIODRIVER=dummy OMT_DATA="$GAME" OMT_WINDOW_SIZE=1920x1080 OMT_FRAMES=$fr OMT_SCREENSHOT="$OUT/$n.bmp" \
      SB_SKIP_MOVIES=1 SB_GAME_LOG=1 "$@" timeout "$TMO" "$BUILD/spongebob" ) > "$OUT/$n.log" 2>&1
  local rc=$? why=""
  [ $rc = 0 ] || why="exit $rc;"
  grep -qE '\*\*\* CRASH|\*\*\* end of backtrace|AddressSanitizer|runtime error:' "$OUT/$n.log" && why="$why crash/sanitizer report;"
  [ -s "$OUT/$n.bmp" ] || why="$why no screenshot;"
  local e
  for e in $(echo "$exp" | tr '|' '\n' | sed 's/ /\\x20/g'); do
    [ "$e" = - ] && continue
    grep -qE "$(printf '%b' "$e")" "$OUT/$n.log" || why="$why missing log line '$(printf '%b' "$e")';"
  done
  [ -s "$OUT/$n.bmp" ] && convert "$OUT/$n.bmp" "$OUT/$n.png" && rm -f "$OUT/$n.bmp"
  grep -h 'TODO(' "$OUT/$n.log" >> "$OUT/todo.all"
  if [ -z "$why" ]; then result "$n" 0 "$fr frames"; else result "$n" 1 "$why"; fi
}
rm -f "$OUT/todo.all"; : > "$OUT/todo.all"
SH="SB_TEST_NEWGAME=1"
game logos-menu    700 'sb2\.omt menu dialog|main menu: clean exit' SB_SKIP_MOVIES=0
game newgame-c1l0s1 400 'scene c1l0s1 entered' $SH
game scene-c1l2s6   300 'scene c1l2s6 entered' $SH SB_TEST_SCENE=c1l2s6
game click-dialogue 800 'event .AAA. \(Larry The Lobster\)|dialogue AAA|line AZA' $SH SB_TEST_SCENE=c1l5s4 SB_TEST_GAME_CLICKS=150:30:30,330:160:32,520:427:267
game hud-item       800 "HUD: ict2 used on 'Larry The Lobster': accepted|dialogue BAA" $SH SB_TEST_SCENE=c1l5s4 \
     "SB_TEST_ITEMS=isp1:Shovel and Pail,ict2:Urchin Chip Pie" SB_TEST_GAME_CLICKS=150:30:30,330:160:32,520:427:267
for m in fill stretch panels fit; do game "wide-$m" 300 'scene c1l2s6 entered' $SH SB_TEST_SCENE=c1l2s6 SB_WIDESCREEN=$m; done
game pad            300 'scene c1l2s6 entered' $SH SB_TEST_SCENE=c1l2s6 "SB_TEST_PAD=40:rx=0.8,ry=0;120:rx=0,r3=1;200:r3=0"
game touch          300 'scene c1l2s6 entered' $SH SB_TEST_SCENE=c1l2s6 "SB_TEST_TOUCH=60:d:300:300,70:m:400:300,80:u:400:300"

# ---- TODO( lines vs the baseline
sort -u "$OUT/todo.all" > "$OUT/todo.txt"; rm -f "$OUT/todo.all"
if [ $UPD = 1 ]; then cp "$OUT/todo.txt" "$BASE"; result todo-baseline 0 "baseline rewritten ($(wc -l < "$BASE") lines)"
else
  new=$(comm -13 <(sort -u "$BASE" 2>/dev/null) "$OUT/todo.txt")
  if [ -z "$new" ]; then result todo-baseline 0 "no new TODO( lines"; else result todo-baseline 1 "new: $(echo "$new" | head -3 | tr '\n' '|')"; fi
fi

# ---- contact sheet of every screenshot
shots=$(ls "$OUT"/*.png "$OUT"/scenetest/*.png 2>/dev/null | grep -v contact.png)
[ -n "$shots" ] && montage -label '%t' $shots -tile 5x -geometry 384x216+4+4 -pointsize 14 "$OUT/contact.png" 2>/dev/null

echo "SUMMARY: $PASS passed, $FAIL failed${FAILED:+ (}${FAILED# }${FAILED:+)}  out=$OUT" | tee "$OUT/summary.txt"
[ $FAIL = 0 ]
