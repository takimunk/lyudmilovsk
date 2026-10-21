#!/bin/sh
# Imports the first-person hero with its idle and walk (see import_hero.py).
# The editor must not be open on this checkout.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
# a clean re-import: Interchange can't replace the assets of a previous run in one process
[ "${1:-import}" = import ] && rm -rf "$ROOT/unreal/Lyudmilovsk/Content/Characters/Hero"
for step in ${*:-import}; do
    "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_hero.py $step" -unattended -nullrhi -nosplash -stdout 2>&1 \
        | grep -E "HERO|Error:|Traceback|  File" | grep -v LogInit
done
