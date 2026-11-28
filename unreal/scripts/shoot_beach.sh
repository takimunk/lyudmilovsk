#!/bin/sh
# Offscreen check of the beach at play (shoot_beach.py), through the UE queue: never opens a window, leaves running
# editors alone. Screenshots: the newest files in Saved/Screenshots/Mac*.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/shoot_beach.log"
mkdir -p "$(dirname "$LOG")"
"$ROOT/tools/wait_ue_queue.sh" 7200 
nice -n 19 "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_beach.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-1500}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "BEACHSHOT|BEACH " "$LOG" | sed "s/.*\(BEACH\)/\1/" || true
