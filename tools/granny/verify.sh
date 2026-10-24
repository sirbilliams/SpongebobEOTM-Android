#!/bin/sh
# Compare game/granny (the granny.dll replacement, reading the .grn files directly) with the real granny.dll:
#  1. load time: grnextract.exe dumps what granny.dll builds from every .grn file (decoded textures, rendering
#     states, render meshes with their bindings, weight / copy lists and triangle lists) and grncheck compares
#     what granny_grn.cpp builds from the same files, bit for bit;
#  2. run time: every script in tools/granny/tests (or the ones named) is run on the DLL under Wine and on
#     granny_compat, and the recorded results are compared (result codes and handles exactly, vertex / bone data
#     as max abs error; 0 = bit-identical).
#
#   tools/granny/verify.sh <game>/grn [work dir] [test...]
#
# granny.dll is taken from the directory above the .grn files (as installed).  Needs i686-w64-mingw32-gcc,
# wine (32-bit prefix: WINEPREFIX, default /root/.wine32), gcc/g++, python3 + numpy.  To run our side on another
# architecture, set CC / CXX to cross compilers and RUN to the emulator, e.g.
#   CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++ RUN="qemu-aarch64 -L /usr/aarch64-linux-gnu"
# (the DLL side always runs on the host).  Exit status 1 on any difference.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
GRN=$(cd "$1" && pwd)
WORK=${2:-${TMPDIR:-/tmp}/grnverify}
[ $# -ge 2 ] && shift 2 || shift $#
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
CC=${CC:-gcc}
CXX=${CXX:-g++}
G="$ROOT/game/granny"
FLAGS="-O2 -ffp-contract=off -fno-fast-math"

i686-w64-mingw32-gcc -O2 -o "$WORK/grntest.exe" "$HERE/grntest.c"
i686-w64-mingw32-gcc -O2 -o "$WORK/grnextract.exe" "$HERE/grnextract.c"
cp "$GRN/../granny.dll" "$WORK/granny.dll"
for f in granny_compat granny_grn granny_bink; do $CXX $FLAGS -c "$G/$f.cpp" -o "$WORK/$f.o"; done
$CC -O2 -DGRNTEST_COMPAT -I"$G" -c "$HERE/grntest.c" -o "$WORK/grntest.o"
$CXX -o "$WORK/grntest" "$WORK/grntest.o" "$WORK/granny_compat.o" "$WORK/granny_grn.o" "$WORK/granny_bink.o" -lm
$CXX $FLAGS -I"$G" -o "$WORK/grncheck" "$HERE/grncheck.cpp" "$WORK/granny_grn.o" "$WORK/granny_bink.o"

winpath() { printf 'Z:%s\n' "$1" | tr / '\\'; }
export WINEPREFIX=${WINEPREFIX:-/root/.wine32} WINEARCH=win32 WINEDEBUG=-all
status=0

# 1. load time
: > "$WORK/extract-list.txt"
for f in $(cd "$GRN" && ls | grep -i '\.grn$'); do printf "model %s\n" "$(winpath "$GRN/$f")" >> "$WORK/extract-list.txt"; done
for attempt in 1 2 3; do  # (one Wine run out of many has been seen to crash inside Wine; a rerun is fine)
	if (cd "$WORK" && wine grnextract.exe "$(winpath "$WORK/dll.gxtr")" -f "$(winpath "$WORK/extract-list.txt")" 2> "$WORK/extract.log"); then break; fi
	[ $attempt = 3 ] && { cat "$WORK/extract.log"; exit 2; }
done
echo "== load time (every .grn file)"
$RUN "$WORK/grncheck" "$WORK/dll.gxtr" "$GRN" "$WORK/granny.dll" > "$WORK/grncheck.txt" || status=1
tail -20 "$WORK/grncheck.txt"

# 2. run time
TESTS="$*"
[ -n "$TESTS" ] || TESTS=$(cd "$HERE/tests" && ls *.txt | sed 's/\.txt$//')
for t in $TESTS; do
	(cd "$WORK" && wine grntest.exe "$(winpath "$HERE/tests/$t.txt")" "$(winpath "$WORK/$t.dll.gtst")" "$(winpath "$GRN")")
	$RUN "$WORK/grntest" "$HERE/tests/$t.txt" "$WORK/$t.compat.gtst" "$GRN"
	echo "== $t"
	python3 "$HERE/grntest.py" cmp "$WORK/$t.dll.gtst" "$WORK/$t.compat.gtst" || status=1
done
exit $status
