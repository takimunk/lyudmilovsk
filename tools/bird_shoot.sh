#!/bin/sh
# Offscreen shots of /Game/Dev/L_BirdSpecies (unreal/scripts/shoot_bird_species.py). Waits for other Unreal -game / Blender runs.
#   tools/bird_shoot.sh <out dir> [camera prefix]
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
OUT="${1:?out dir}"
while pgrep -f "UnrealEditor.*-game" >/dev/null; do sleep 5; done
mkdir -p "$OUT"
BIRD_SHOTS="$OUT" BIRD_CAMS="${2:-}" "$UE" "$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject" /Game/Dev/L_BirdSpecies -game -RenderOffscreen \
    -ResX=1280 -ResY=720 -windowed -dpcvars=lyud.MainMenuOnStart=0 -abslog="$OUT/shoot.log" \
    "-ExecCmds=py $ROOT/unreal/scripts/shoot_bird_species.py" >/dev/null 2>&1 || true
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
grep -E "BIRDSHOT|Error:" "$OUT/shoot.log" | head -20
ls "$OUT"
