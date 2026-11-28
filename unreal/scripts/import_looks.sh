#!/bin/sh
# Imports the hero's haircuts and cap (blender/hero/build_looks.py -> blender/hero/export/looks/*.glb, docs/look.md) one
# UnrealEditor-Cmd per mesh: Interchange brings in only the first glb of a process. Waits for a UE slot
# (tools/wait_ue_queue.sh), runs at nice 19.
#   unreal/scripts/import_looks.sh [name ...]      (default: all four)
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOGS="$ROOT/unreal/Lyudmilovsk/Saved/Logs"
mkdir -p "$LOGS"
NAMES="${*:-SM_HeroHair_crop SM_HeroHair_short SM_HeroHair_parted SM_HeroCap}"
for N in $NAMES; do
    sh "$ROOT/tools/wait_ue_queue.sh"
    LOOKS="$N" nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_looks.py" -unattended -nullrhi -abslog="$LOGS/import_looks_$N.log" > /dev/null 2>&1
    grep -h "LOOKS" "$LOGS/import_looks_$N.log" | grep -v LogInit | sed 's/^.*LOOKS/LOOKS/'
done
ls "$ROOT/unreal/Lyudmilovsk/Content/Characters/Hero/Look/"
