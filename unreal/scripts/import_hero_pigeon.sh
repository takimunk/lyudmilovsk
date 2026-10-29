#!/bin/sh
# The player's feral pigeon (blender/birds/hero_pigeon): textures, then the mesh, one Unreal process each.
# No editor open on this checkout. See import_hero_pigeon.py.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
for step in textures mesh; do
    # the old body's file goes before the mesh step: deleting it from Unreal leaves it loaded and blocks the new name
    [ "$step" = mesh ] && rm -f "$ROOT/unreal/Lyudmilovsk/Content/Birds/FeralPigeon/SK_FeralPigeon_Hero.uasset"
    "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_hero_pigeon.py $step" -unattended -nullrhi -nosplash -stdout 2>&1 \
        | grep -E "BIRDS|Error:|Traceback|  File|Exception" | grep -v LogInit
done
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
