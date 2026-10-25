#!/bin/sh
# Offscreen play test of the bird life (test_birds.py): never opens a window, leaves running editors alone.
# Then draws the simulated day it dumped: Saved/BirdSim/birds_day_map.png.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_birds.log"
mkdir -p "$(dirname "$LOG")"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash ${NOSOUND--nosound} -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_birds.py" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: no test takes 15 minutes
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "BIRDTEST|BirdLife:" "$LOG" | sed 's/.*\(BIRDTEST\|BirdLife:\)/\1/' || true
CSV="$ROOT/unreal/Lyudmilovsk/Saved/BirdSim/day.csv"
if [ -f "$CSV" ]; then python3 "$ROOT/tools/birdsim/render_birds.py" "$CSV" "$ROOT/unreal/Lyudmilovsk/Saved/BirdSim"; fi
