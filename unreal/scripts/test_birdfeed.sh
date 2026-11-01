#!/bin/sh
# Offscreen play test of birds coming to the feed, flying round the player, real perches and far birds (test_birdfeed.py):
# never opens a window, leaves running editors alone. Waits for a free UE slot (tools/wait_ue_queue.sh), runs at low priority.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_birdfeed.log"
mkdir -p "$(dirname "$LOG")"
sh "$ROOT/tools/wait_ue_queue.sh" || true
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1280 -ResY=720 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_birdfeed.py" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog on our own process only: the test takes ~12 minutes
( sleep "${TIMEOUT:-1500}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "BIRDFEED|BIRDPERCH" "$LOG" | sed 's/.*\(BIRDFEED\|BIRDPERCH\)/\1/' || true
