#!/bin/sh
# Imports the Academgorodok (blender/export/academgorodok) into /Game/Academgorodok, then places it in
# Lvl_Scene01 (unreal/scripts/import_academgorodok.py). Headless; leaves running editors alone, but this
# checkout's own editor must be closed. One process per step (Interchange).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../Lyudmilovsk/Lyudmilovsk.uproject"
for step in ${STEPS:-import place}; do
    "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript \
        -script="$HERE/import_academgorodok.py $step" -unattended -nosplash -stdout 2>&1 \
        | grep -E "ACADEM|Python: Error|Error:|Traceback|  File" | grep -v LogInit || true
done
