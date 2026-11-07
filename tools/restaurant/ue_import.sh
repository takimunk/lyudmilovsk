#!/bin/sh
# The restaurant into Unreal (docs/restaurant.md): the building's kit, the tableware's kit and the sounds each in a commandlet of their
# own (Interchange may skip a second glTF import in one process), then placed into Lvl_Scene01. Low priority, after the UE queue.
#   tools/restaurant/ue_import.sh [building] [tableware] [sounds] [place]      (none: all four)
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
STEPS="${*:-building tableware sounds place}"
run() {
    sh "$ROOT/tools/wait_ue_queue.sh" 7200
    nice -n 19 taskpolicy -b "$CMD" "$PROJ" -run=pythonscript -script="$1" -unattended -nosplash -nopause 2>&1 | grep -E "RESTAURANT|Error|error|Traceback" | grep -v "LogPython: Display" | tail -40
}
for S in $STEPS; do
    case $S in
        building) run "$ROOT/unreal/scripts/import_restaurant.py import kit=restaurant_kit" ;;
        tableware) run "$ROOT/unreal/scripts/import_restaurant.py import kit=tableware_kit" ;;
        sounds) run "$ROOT/unreal/scripts/import_restaurant_sounds.py" ;;
        place) run "$ROOT/unreal/scripts/import_restaurant.py place" ;;
    esac
done
