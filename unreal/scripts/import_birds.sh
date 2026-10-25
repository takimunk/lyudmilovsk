#!/bin/sh
# Imports bird species built by blender/birds/build_bird.py (see import_birds.py). No editor open on this checkout.
#   unreal/scripts/import_birds.sh [masters] Starling Rook ...
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
run() {
    "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_birds.py $*" -unattended -nullrhi -nosplash -stdout 2>&1 \
        | grep -E "BIRDS|Error:|Traceback|  File|Exception" | grep -v LogInit
}
for s in "$@"; do
    if [ "$s" = masters ]; then run masters; else run textures "$s"; run import "$s"; fi
done
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
