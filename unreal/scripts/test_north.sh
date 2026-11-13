#!/bin/sh
# Offscreen play test of the North (test_north.py): never opens a window, leaves running editors alone, waits its turn
# (tools/wait_ue_queue.sh). NORTHTEST_ONLY=day_north_camp,night_camp picks steps; NORTHTEST_WAIT=8 seconds per step.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_north.log"
mkdir -p "$(dirname "$LOG")"
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 3600
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0,lyud.Birds=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_north.py" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: no test takes 20 minutes
( sleep "${TIMEOUT:-1200}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "NORTHTEST|NORTH " "$LOG" | sed "s/.*NORTHTEST/NORTHTEST/" || true
