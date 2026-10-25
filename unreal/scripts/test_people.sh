#!/bin/sh
# Offscreen checks of the people's life (test_people.py) in Lvl_Scene01: never opens a window, leaves running editors alone.
#   unreal/scripts/test_people.sh                 all parts
#   PEOPLETEST_PART=club unreal/scripts/test_people.sh
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_people.log"
mkdir -p "$(dirname "$LOG")"
rm -f "$ROOT"/unreal/Lyudmilovsk/Saved/Screenshots/Mac/HighresScreenshot*.png
sh "$ROOT/tools/wait_ue_queue.sh"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound \
    -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_people.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "PEOPLETEST|PeopleLife:|People:" "$LOG" | sed 's/^.*\(PEOPLETEST\|PeopleLife:\|People:\)/\1/' || true
