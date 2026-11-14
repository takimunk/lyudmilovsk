#!/bin/sh
# Imports the manner props (blender/humans/props.py -> Saved/ClipSources/props/*.glb) one UnrealEditor-Cmd per prop:
# Interchange brings in only the first glb of a process. Waits for a UE slot (tools/wait_ue_queue.sh), runs at nice 19.
#   unreal/scripts/import_props.sh [name ...]      (default: all 14)
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOGS="$ROOT/unreal/Lyudmilovsk/Saved/Logs"
mkdir -p "$LOGS"
NAMES="${*:-phone cigarette camera umbrella rod brush binoculars newspaper book knitting ice_cream cup bag easel}"
for N in $NAMES; do
    sh "$ROOT/tools/wait_ue_queue.sh"
    PROPS="$N" nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_props.py" -unattended -nullrhi -abslog="$LOGS/import_props_$N.log" > /dev/null 2>&1
    grep -h "PROPS" "$LOGS/import_props_$N.log" | grep -v LogInit | sed 's/^.*PROPS/PROPS/'
done
ls "$ROOT/unreal/Lyudmilovsk/Content/Characters/Humans/Props/"
