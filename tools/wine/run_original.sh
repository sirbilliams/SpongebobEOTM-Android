#!/bin/bash
# Runs the ORIGINAL Neutron2.exe under Wine, headless, and screenshots it at intervals: the
# reference for what the port should show. Nothing of the game goes into the repo.
#
#   tools/wine/run_original.sh <game dir> <out dir> <seconds> [interval=5] [display=:109]
#
# <game dir> is an installed game (neg_import output: Neutron2.exe, OMT2.dll, omt/, gam/...).
# A run deletes that folder's autosaves, as the port does; use your own install.
# What it sets up (once, in <out dir>):
#   - a Wine prefix with drive D: as a "cdrom" holding an empty NEUTRON2.ico (CheckCD 0x496200
#     only looks for that file on a CD-ROM drive);
#   - an ALSA config whose default PCM is "null": without a sound device DirectSound fails, the
#     game turns sound off and every cut scene camera stops at its first shot (as on Windows);
#   - Xvfb at 640x480x16 (the game sets 640x480x16; a different size or depth makes its display
#     mode change fail and it never draws).
# Screenshots: <out dir>/shot-<seconds>.png (ImageMagick import). The game log is
# <game dir>/dat/debug.log (ErrorLog lines only) and <out dir>/wine.log.
set -u
GAME=$(cd "$1" && pwd); OUT=$(mkdir -p "$2" && cd "$2" && pwd); T=$3; I=${4:-5}; DISP=${5:-:109}
export WINEPREFIX=$OUT/wineprefix WINEDEBUG=${WINEDEBUG:--all} DISPLAY=$DISP
if [ ! -f "$OUT/alsa.conf" ]; then
	cp /usr/share/alsa/alsa.conf "$OUT/alsa.conf"
	printf '\npcm.!default { type null }\n' >> "$OUT/alsa.conf"
fi
export ALSA_CONFIG_PATH=$OUT/alsa.conf
if [ ! -d "$WINEPREFIX/dosdevices" ]; then
	wineboot -i >/dev/null 2>&1
	mkdir -p "$OUT/fakecd" && touch "$OUT/fakecd/NEUTRON2.ico"
	ln -sfn "$OUT/fakecd" "$WINEPREFIX/dosdevices/d:"
	wine reg add 'HKLM\Software\Wine\Drives' /v 'd:' /d cdrom /f >/dev/null 2>&1
	wineserver -k; wineserver -w					# the drive type is read when Wine starts
fi
pgrep -f "Xvfb $DISP " >/dev/null || { Xvfb "$DISP" -screen 0 640x480x16 >/dev/null 2>&1 & sleep 1; }
cd "$GAME" || exit 1
wine Neutron2.exe > "$OUT/wine.log" 2>&1 &
for ((t = I; t <= T; t += I)); do
	sleep "$I"
	import -display "$DISP" -window root "$OUT/shot-$t.png" 2>/dev/null
done
wineserver -k
