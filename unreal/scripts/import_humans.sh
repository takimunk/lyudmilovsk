#!/bin/sh
# Imports MakeHuman characters into /Game/Characters/Humans, one editor process per character.
#   unreal/scripts/import_humans.sh zoomer clerk owner ...   (no names: every blender/humans/export/human_*.glb)
# The editor must not be open on this checkout.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
NAMES="$*"
[ -z "$NAMES" ] && NAMES=$(ls "$ROOT/blender/humans/export" | sed -n 's/^human_\(.*\)\.glb$/\1/p')
FLAG=""
for n in $NAMES; do
    "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_humans.py $n $FLAG" -unattended -nullrhi -nosplash -stdout 2>&1 \
        | grep -E "HUMANS|Error:" | grep -v LogInit
    FLAG="--keep-masters"
done
