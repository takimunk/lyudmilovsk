#!/bin/sh
# Offscreen shots of the townsfolk's new activities (shoot_residents.py) in Lvl_Scene01: never opens a window.
#   unreal/scripts/shoot_residents.sh                 -> Saved/Shots/residents/res_<scene>_<n>.png
#   SCENES=allot,cinema unreal/scripts/shoot_residents.sh
#   UE_QUEUE=<path to a wait_ue_queue.sh> (default tools/wait_ue_queue.sh)
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/shoot_residents.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/Saved/Shots/residents"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SHOTS"/res_*.png
sh "${UE_QUEUE:-$ROOT/tools/wait_ue_queue.sh}"
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 \
    -game -RenderOffscreen -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound \
    -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_residents.py" > "$LOG.stdout" 2>&1 &
PID=$!
echo "$PID" > "$LOG.pid"
( sleep "${TIMEOUT:-1500}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID"
kill "$DOG" 2>/dev/null
for F in "$SHOTS"/res_*.png; do [ -e "$F" ] && mv "$F" "$OUT/"; done
grep -h "RESSHOT" "$LOG" | grep -v LogInit | sed 's/^.*RESSHOT/RESSHOT/'
grep -E "Ensure|ensure|Error" "$LOG" | grep -i -E "People|Manner|Gesture|NpcBody|BodyAnim|Prop" | head -20
