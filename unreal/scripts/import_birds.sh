#!/bin/sh
# Imports bird species built by blender/birds/build_bird.py (see import_birds.py). No editor open on this checkout.
#   unreal/scripts/import_birds.sh [masters] Starling Rook ...
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
run() {   # one Unreal process per step, in the UE queue, quiet (no Dock focus, no mouse capture)
    "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200 > /dev/null
    "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -run=pythonscript \
        -script="$ROOT/unreal/scripts/import_birds.py $*" -nullrhi $("$ROOT/tools/ue_quiet_args.sh") -stdout 2>&1 \
        | grep -E "BIRDS|Error:|Traceback|  File|Exception" | grep -v LogInit
}
for s in "$@"; do
    case "$s" in
        masters) run masters ;;
        hero:*)   # hero:<Id>: the player's detailed body (the old one's file removed first: deleted in Unreal it stays on disk)
            rm -f "$ROOT/unreal/Lyudmilovsk/Content/Birds/${s#hero:}/SK_${s#hero:}_Hero.uasset"
            run herotextures "${s#hero:}"; run hero "${s#hero:}" ;;
        *) run textures "$s"; run import "$s" ;;
    esac
done
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
# every species' gloss hue from blender/birds/species_art.py
[ "$#" -gt 0 ] && run hues "$(echo "$@" | sed 's/masters//; s/hero:[A-Za-z]*//g')"
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
