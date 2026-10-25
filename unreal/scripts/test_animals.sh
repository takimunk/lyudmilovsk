#!/bin/sh
# Offscreen checks of the cats and dogs (test_animals.py): never opens a window, leaves running editors alone.
#   unreal/scripts/test_animals.sh            the test level (/Game/Dev/L_AnimalsTest, built by make_animals_test_level.py)
#   LEVEL=Lvl_Scene01 unreal/scripts/test_animals.sh
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LEVEL="${LEVEL:-L_AnimalsTest}"
MAP="/Game/Dev/$LEVEL"; [ "$LEVEL" = Lvl_Scene01 ] && MAP=/Game/Scene01/Lvl_Scene01
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_animals.log"
mkdir -p "$(dirname "$LOG")"
rm -f "$ROOT"/unreal/Lyudmilovsk/Saved/Screenshots/Mac/HighresScreenshot*.png
ANIMTEST_LEVEL="$LEVEL" "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" "$MAP" -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound \
    -dpcvars=lyud.MainMenuOnStart=0,lyud.Animals.AnyMap=1,lyud.Birds.AnyMap=1 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_animals.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "ANIMTEST|AnimalLife:|Animals?:" "$LOG" | sed 's/^.*\(ANIMTEST\|AnimalLife:\|Animal:\|Animals:\)/\1/' || true
