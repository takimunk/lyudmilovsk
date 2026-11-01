#!/bin/sh
# Offscreen play test of the bicycles (test_bikes.py): never opens a window, leaves running editors alone. Waits for the
# UE queue. Env: BIKETEST_ONLY=hero|npc|lobby, BIKETEST_SHOTS=0, EXTRA (more arguments), SOUND=1 (with sound, silent).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_bikes.log"
mkdir -p "$(dirname "$LOG")"
QUIET="$("$ROOT/tools/ue_quiet_args.sh")"
if [ "$SOUND" = "1" ]; then
    # the game's own mix, the speakers silent
    QUIET="$(echo "$QUIET" | sed 's/-nosound//')"
    EXTRA="$EXTRA -dpcvars=au.LinearGainScalarForFinalOutut=0"
fi
"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 $QUIET -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_bikes.py" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: no run takes 25 minutes (it checks the pid is still this test before it kills)
( sleep "${TIMEOUT:-1200}"; ps -p "$PID" -o args= | grep -q test_bikes.py && kill "$PID" 2>/dev/null ) &
DOG=$!
trap 'pkill -P "$DOG" 2>/dev/null; kill "$DOG" 2>/dev/null' EXIT
wait "$PID" || true
grep -E "BIKETEST|BIKEREPORT|HEROBIKE|HEROBONES [a-z]|Bikes:|BIKES " "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' || true
grep -a "Critical error\|Assertion failed" "$LOG" | head -3 || true
