#!/bin/sh
# Offscreen play test of the seasonal colour grade (shoot_grade.py): never opens a window, leaves running editors alone.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/shoot_grade.log"
mkdir -p "$(dirname "$LOG")"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0,lyud.Birds=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_grade.py" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: no test takes 15 minutes
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "GRADETEST" "$LOG" | sed "s/.*GRADETEST/GRADETEST/" || true
