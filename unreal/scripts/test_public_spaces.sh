#!/bin/sh
# Offscreen -game test of the town's public spaces (test_public_spaces.py): shots day and night, the spots API; never
# opens a window, no sound, lowest priority, waits for a free UE slot (tools/wait_ue_queue.sh). Shots are copied to Saved/CityShots/<name>.png.
#   unreal/scripts/test_public_spaces.sh               (CITY_SHOTS=square,fountain for some only)
#   CITY_AB=1 CITY_EXTRA=-ueexclusive ...     (frame times with and without them in one process, run alone)
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_public_spaces.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/CityShots"
mkdir -p "$(dirname "$LOG")" "$OUT"
if [ -n "$CITY_EXTRA" ]; then "$ROOT/tools/wait_ue_queue.sh" 7200 --exclusive; else "$ROOT/tools/wait_ue_queue.sh" 7200; fi
rm -f "$SHOTS"/HighresScreenshot*.png "$OUT"/*.png
RHI=-RenderOffscreen; [ -n "$CITY_NULLRHI" ] && RHI=-nullrhi    # CITY_NULLRHI=1: checks only, no shots
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game $RHI \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -NoMouseCapture -nosound -dpcvars=lyud.MainMenuOnStart=0${DPCVARS:+,$DPCVARS} \
    -abslog="$LOG" $CITY_EXTRA "-ExecCmds=py $ROOT/unreal/scripts/test_public_spaces.py" > "$LOG.stdout" 2>&1 || true
grep -E "CITY|LyudPublicSpaces|LyudFountain|Error:" "$LOG" | grep -v "LogInit" | head -150
i=0
for f in $(ls "$SHOTS"/HighresScreenshot*.png 2>/dev/null | sort); do
    i=$((i + 1))
    name=$(grep "CITYSHOT" "$LOG" | sed -n "${i}p" | sed -E 's/.*CITYSHOT ([a-z0-9_]+).*/\1/')
    cp "$f" "$OUT/${name:-shot$i}.png"
done
ls "$OUT"
grep -q "CITYTEST done: 0 failed" "$LOG"
