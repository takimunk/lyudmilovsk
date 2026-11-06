#!/bin/sh
# Imports the town's buildings (blender/export/city, built by blender/city/build_city.py) into /Game/City, then places
# them on the town plan's plots in Lvl_Scene01 (unreal/scripts/import_city.py). Headless; this checkout's own editor
# must be closed. One process per step (Interchange). TYPES=capsule_tower,serpentine_slab limits the import.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../Lyudmilovsk/Lyudmilovsk.uproject"
for step in ${STEPS:-import place}; do
    arg="$step"
    [ "$step" = import ] && [ -n "$TYPES" ] && arg="import $TYPES"
    nice -n 19 "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript \
        -script="$HERE/import_city.py $arg" -unattended -nosplash -stdout 2>&1 \
        | grep -E "CITY|Python: Error|Error:|Traceback|  File" | grep -v LogInit || true
done
