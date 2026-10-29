#!/bin/bash
# Desktop test of the companion panel's game side (docs/companion.md): the map travel rule, the dialogue history, the
# drag-combine.  Needs the installed game; run inside the omtbuild box:  tools/companion/test.sh <build dir> <game dir> [out dir]
# Env: SB_SMOKE_DISPLAY (default :84, its own Xvfb), FFMPEG_LIB.  Exit 0 only if every check passed.
set -u
REPO=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=$(realpath "$1"); GAME=$(realpath "$2"); OUT=${3:-$REPO/.shots/companion}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
DISP=${SB_SMOKE_DISPLAY:-:84}
XPID=""
if [ ! -e "/tmp/.X11-unix/X${DISP#:}" ]; then Xvfb "$DISP" -screen 0 1920x1080x24 >/dev/null 2>&1 & XPID=$!; sleep 2; fi
trap '[ -n "$XPID" ] && kill $XPID 2>/dev/null' EXIT
export LD_LIBRARY_PATH="${FFMPEG_LIB:-$HOME/Projects/sbeotm-private/ffmpeg/lib}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
FAIL=0
check() { if grep -qE "$2" "$OUT/$1.log"; then echo "PASS  $1: $3"; else echo "FAIL  $1: $3"; FAIL=1; fi; }
run() {   # run <name> <frames> [ENV=value ...]
  local n=$1 fr=$2; shift 2
  ( cd "$GAME" && env DISPLAY=$DISP SDL_AUDIODRIVER=dummy OMT_DATA="$GAME" OMT_WINDOW_SIZE=640x480 OMT_FRAMES=$fr SB_SKIP_MOVIES=1 SB_GAME_LOG=1 \
      SB_TEST_NEWGAME=1 SB_COMPANION_DUMP="$OUT/$n.json" "$@" timeout 240 "$BUILD/spongebob" ) > "$OUT/$n.log" 2>&1
}
# 1. dialogue history (Larry's talk); the map button is dead while the conversation is open
run talk 1600 SB_TEST_READY_FRAMES=1 SB_TEST_SCENE=c1l5s4 SB_TEST_GAME_CLICKS=150:30:30,330:160:32,520:427:267 "SB_TEST_COMPANION=1500:travel:c1l2s6p1"
check talk 'companion: talk 1 \[' 'dialogue history filled after a conversation'
check talk 'companion: talk 2 \[' 'the next line too'
check talk 'companion: travel c1l2s6p1 refused \(busy' 'no travel while a conversation is open'
grep -q '"talk":\[{"i":1' "$OUT/talk.json" && echo "PASS  talk: snapshot has the history" || { echo "FAIL  talk: snapshot"; FAIL=1; }
# 2. the map: Goo Lagoon is not open yet (164caa) -> refused; the Krusty Krab is always open -> travels
run map 600 SB_TEST_READY_FRAMES=1 SB_TEST_SCENE=c1l5s4 "SB_TEST_COMPANION=100:travel:c1l5s1p1,140:travel:c1l1s1p1,200:travel:c1l2s6p1"
check map 'companion: travel c1l5s1p1 refused \(the map does not offer it yet\)' 'locked area refused'
check map 'companion: travel c1l1s1p1 refused \(not on the map\)' 'a target that is not on the map refused'
check map 'companion: travel c1l2s6p1$' 'open area travels'
check map 'scene c1l2s6 entered' 'and the scene was entered'
grep -q '"map":\[' "$OUT/map.json" && grep -q '"goals":{' "$OUT/map.json" && echo "PASS  map: snapshot has map and goals" || { echo "FAIL  map: snapshot fields"; FAIL=1; }
# 3. drag-combine: Patty + Bun make the Krabby Patty; that + the Urchin Chip pie is refused with the game's own comment
run combine 600 SB_TEST_READY_FRAMES=1 SB_TEST_SCENE=c1l5s4 "SB_TEST_ITEMS=ikp1:Patty,ikp2:Bun,iuc1:Urchin" \
    "SB_TEST_COMPANION=200:combine:ikp1:ikp2,260:combine:ikp3:iuc1"
check combine 'HUD: ikp1 \+ ikp2 -> ikp3' 'combine by drag goes through Hud::combine'
check combine 'companion: combine ikp3 \+ iuc1 -> refused: .+' 'a refusal shows the game comment'
check combine 'comment H000F' 'the refusal is the game comment H000F'
grep -ah 'companion: combine' "$OUT/combine.log"
[ $FAIL = 0 ] && echo "COMPANION TEST OK" || echo "COMPANION TEST FAILED"
exit $FAIL
