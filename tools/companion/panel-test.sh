#!/bin/bash
# Desktop test of the panel's touch code (the real CompanionView.java on the Java2D shim): tabs switch pages, a tap on an open map place
# sends travel:<target>, a tap on a place that is not open sends nothing (it shows a note), a card dragged onto another sends combine:a:b,
# a card dropped on nothing sends nothing.   Run inside the omtbuild box:  tools/companion/panel-test.sh [out dir]
set -u
here=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$here/../../.shots/pages}; mkdir -p "$OUT"
J=$here/samples/pages.json; FAIL=0
ck() { if [ "$2" = "$3" ]; then echo "PASS  $1"; else echo "FAIL  $1 (got '$2', want '$3')"; FAIL=1; fi; }
r() { SB_JAVA_OPTS="$1" "$here/render.sh" "$J" "$OUT/$2.png" "$3" 2>&1; }
o=$(r "-Dtaps=tab:2;tab:1" tabtap tabtap | grep '^page'); ck "tabs switch to the Map page" "$o" "page 1"
o=$(r "-Dtaps=tab:1;travel:c1l6s1p1" maptap-open maptap | grep -c '^send travel:c1l6s1p1$'); ck "tap on an open place sends travel" "$o" 1
o=$(r "-Dtaps=tab:1;note:Not" maptap-locked maptap | grep -c '^send'); ck "tap on a place that is not open sends nothing" "$o" 0
o=$(r "-Dcards=isp1,ict2" combine combine | grep -c '^send combine:isp1:ict2$'); ck "drag card onto card sends combine" "$o" 1
o=$(r "-Dcards=isp1,isp1" combine-self combine | grep -c '^send combine'); ck "drop on itself sends nothing" "$o" 0
[ $FAIL = 0 ] && echo "PANEL TEST OK" || echo "PANEL TEST FAILED"
exit $FAIL
