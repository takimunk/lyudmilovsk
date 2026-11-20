#!/bin/sh
# Offscreen checks of the animal friends (test_animal_friends.py; docs/animals.md "Friends"): never opens a window.
#   unreal/scripts/test_animal_friends.sh                      PART=stroke in /Game/Dev/L_AnimalsTest
#   PART=door LEVEL=Lvl_Scene01 unreal/scripts/test_animal_friends.sh    Maru's walk, the club's door, a bench
# Run it through tools/wait_ue_queue.sh on the UE Mac. Shots: Saved/Screenshots/Mac (in step order, FRIENDTEST shot lines).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LEVEL="${LEVEL:-L_AnimalsTest}"
MAP="/Game/Dev/$LEVEL"; [ "$LEVEL" = Lvl_Scene01 ] && MAP=/Game/Scene01/Lvl_Scene01
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_animal_friends_${PART:-stroke}.log"
mkdir -p "$(dirname "$LOG")"
rm -f "$ROOT"/unreal/Lyudmilovsk/Saved/Screenshots/Mac/HighresScreenshot*.png
FRIENDTEST_LEVEL="$LEVEL" FRIENDTEST_PART="${PART:-stroke}" "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" "$MAP" -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound \
    -dpcvars=lyud.MainMenuOnStart=0,lyud.Animals.AnyMap=1,lyud.Birds.AnyMap=1 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_animal_friends.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "FRIENDTEST|AnimalFriends:|AnimalLife:" "$LOG" | sed 's/^.*\(FRIENDTEST\|AnimalFriends:\|AnimalLife:\)/\1/' || true
