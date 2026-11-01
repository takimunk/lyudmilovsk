#!/bin/sh
# Offscreen check that nobody in the town is seen twice (test_unique_people.py) in Lvl_Scene01: never opens a window,
# leaves running editors alone, low priority. Exit 1 if anybody was seen twice. Shots: Saved/Screenshots/Mac.
#   unreal/scripts/test_unique_people.sh
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_unique_people.log"
mkdir -p "$(dirname "$LOG")"
rm -f "$ROOT"/unreal/Lyudmilovsk/Saved/Screenshots/Mac/HighresScreenshot*.png
sh "$ROOT/tools/wait_ue_queue.sh"
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 \
    -game -RenderOffscreen -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound \
    -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_unique_people.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-3000}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID"
kill "$DOG" 2>/dev/null
grep -E "UNIQUETEST|PeopleLife:|People:" "$LOG" | sed -E 's/^.*(UNIQUETEST|PeopleLife:|People:)/\1/'
grep -q "UNIQUETEST result PASS" "$LOG"
