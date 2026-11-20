#!/bin/sh
# Offscreen look at the Big Sun day (shoot_sun_day.py) in Lvl_Scene01 through the UE queue: never opens a window.
#   unreal/scripts/shoot_sun_day.sh                      -> Saved/SunDayShots/sunday_*.png, log Saved/Logs/shoot_sun_day.log
#   SUNDAY_ONLY=seaside unreal/scripts/shoot_sun_day.sh
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/shoot_sun_day.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="${OUT:-$ROOT/Saved/SunDayShots}"
QUEUE="${UE_QUEUE:-$ROOT/tools/wait_ue_queue.sh}"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SHOTS"/sunday_*.png
sh "$QUEUE" ${QUEUE_ARGS:-} || exit 1
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 \
    -game -RenderOffscreen -windowed -ResX=1600 -ResY=900 $("$ROOT/tools/ue_quiet_args.sh") \
    -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_sun_day.py" > "$LOG.stdout" 2>&1 &
PID=$!
echo "$PID" > "$OUT/ue.pid"
( sleep "${TIMEOUT:-1500}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID"
kill "$DOG" 2>/dev/null
for F in "$SHOTS"/sunday_*.png; do [ -e "$F" ] && mv "$F" "$OUT/"; done
grep -a -h "SUNDAYSHOT\|Festivals:\|FLUFF\|Fluff:\|FESTIVAL\|sun_day" "$LOG" | grep -v LogInit | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -150
grep -a -E "Ensure|ensure|Critical error|Fatal" "$LOG" | head -10
