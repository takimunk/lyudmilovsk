#!/bin/sh
# Offscreen play test of swimming and diving (test_swim.py): never opens a window, leaves running editors alone. Waits for the UE queue.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_swim.log"
mkdir -p "$(dirname "$LOG")"
"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 $("$ROOT/tools/ue_quiet_args.sh") -dpcvars=lyud.MainMenuOnStart=0,lyud.Birds=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_swim.py" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: no run takes 20 minutes
# (it checks that the pid is still this test before it kills, and goes with its sleep when the test ends)
( sleep "${TIMEOUT:-1200}"; ps -p "$PID" -o args= | grep -q test_swim.py && kill "$PID" 2>/dev/null ) &
DOG=$!
trap 'pkill -P "$DOG" 2>/dev/null; kill "$DOG" 2>/dev/null' EXIT
wait "$PID" || true
grep -E "SWIMTEST|Swim:" "$LOG" | sed 's/.*\(SWIMTEST\|Swim:\)/\1/' || true
