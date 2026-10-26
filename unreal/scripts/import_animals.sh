#!/bin/sh
# Imports the cats and dogs built by blender/animals/build_animal.py (see import_animals.py). No editor open on this checkout.
#   unreal/scripts/import_animals.sh [masters] Cat Laika Shepherd Mutt Maru
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
run() {
    "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_animals.py $*" -unattended -nullrhi -nosplash -stdout 2>&1 \
        | grep -E "ANIMALS|Error:|Traceback|  File|Exception" | grep -v LogInit || true
}
for s in "$@"; do
    if [ "$s" = masters ]; then run masters; else run textures "$s"; run import "$s"; fi
    if [ "$s" != masters ] && [ -f "$ROOT/blender/animals/export/$s/${s}_Collar.glb" ]; then run collar "$s" Collar; run collar "$s" Tag; fi
done
python3 "$ROOT/unreal/scripts/import_animals.py" bodies
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
