#!/bin/sh
# Offscreen shots of the player's pigeon (/Game/Dev/L_HeroPigeon, built by make_hero_pigeon_level.py if missing).
# Waits for other Unreal -game runs (Dima may be playing).   tools/pigeon_shoot.sh <out dir>
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
OUT="${1:?out dir}"
while pgrep -f "MacOS/UnrealEditor .*-game" >/dev/null; do sleep 5; done
if [ ! -f "$ROOT/unreal/Lyudmilovsk/Content/Dev/L_HeroPigeon.umap" ] || [ -n "$REBUILD" ]; then
    "$UE" "$PROJ" -run=pythonscript -script="$ROOT/unreal/scripts/make_hero_pigeon_level.py" -unattended -nullrhi -nosplash -stdout 2>&1 \
        | grep -E "BIRDS|Error:|Traceback" | grep -v LogInit
fi
mkdir -p "$OUT"
PIGEON_SHOTS="$OUT" "$UE" "$PROJ" /Game/Dev/L_HeroPigeon -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed \
    -dpcvars=lyud.MainMenuOnStart=0 -abslog="$OUT/shoot.log" "-ExecCmds=py $ROOT/unreal/scripts/shoot_hero_pigeon.py" >/dev/null 2>&1 || true
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
grep -E "PIGEONSHOT|Error:" "$OUT/shoot.log" | head -30
ls "$OUT"
