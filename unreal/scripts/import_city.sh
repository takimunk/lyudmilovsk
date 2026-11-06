#!/bin/sh
# Imports the town's buildings (blender/export/city, built by blender/city/build_city.py) into /Game/City, then places
# them on the town plan's plots in Lvl_Scene01 (unreal/scripts/import_city.py). Headless; this checkout's own editor
# must be closed. One process per step and per type (Interchange). TYPES=capsule_tower,serpentine_slab limits the import.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../Lyudmilovsk/Lyudmilovsk.uproject"
# Interchange imports only the first glb of a process: one process per type
[ -z "$TYPES" ] && TYPES=$(ls "$HERE/../../blender/export/city/" 2>/dev/null | sed -n 's/\.glb$//p' | tr '\n' ',')
run() {
    nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript \
        -script="$HERE/import_city.py $1" -unattended -nosplash -stdout 2>&1 \
        | grep -E "CITY|Python: Error|Error:|Traceback|  File" | grep -v LogInit || true
}
for step in ${STEPS:-import place}; do
    if [ "$step" = import ]; then
        for t in $(echo "$TYPES" | tr ',' ' '); do run "import $t"; done
    else
        run "$step"
    fi
done
