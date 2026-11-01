#!/bin/sh
# Imports fish built by blender/fish/build_fish.py (see import_fish.py). No editor open on this checkout.
#   unreal/scripts/import_fish.sh [Id ...]   (no ids: every blender/fish/export/<Id>.glb)
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
IDS="$*"
[ -z "$IDS" ] && IDS=$(cd "$ROOT/blender/fish/export" && ls *.glb | grep -v _LOD | sed 's/\.glb$//' | tr '\n' ' ')
run() {
    "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_fish.py $*" -unattended -nullrhi -nosplash -stdout 2>&1 \
        | grep -E "FISHIMPORT|Error:|Traceback|  File|Exception" | grep -v LogInit
}
run textures $IDS
for s in $IDS; do run mesh "$s"; done
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
