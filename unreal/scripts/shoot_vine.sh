#!/bin/sh
# Offscreen -game shots of the climbing vines on Novikova 1's courtyard face (shoot_vine.py) at several
# seasons and ages; never opens a window. Shots are copied to Saved/VineShots/<name>.png.
#   unreal/scripts/shoot_vine.sh                (VINE_SHOTS=s_oct,n_winter for some only)
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/shoot_vine.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/VineShots"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SHOTS"/HighresScreenshot*.png "$OUT"/*.png
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0${DPCVARS:+,$DPCVARS} \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_vine.py" > "$LOG.stdout" 2>&1 || true
grep -E "VINESHOT|Vine |LogVine|Error:" "$LOG" | grep -v "LogInit" | head -60
# HighResShot numbers its files in order: name them after the shots
i=0
for f in $(ls "$SHOTS"/HighresScreenshot*.png 2>/dev/null | sort); do
    i=$((i + 1))
    name=$(grep "VINESHOT" "$LOG" | sed -n "${i}p" | sed -E 's/.*VINESHOT ([a-z0-9_]+).*/\1/')
    cp "$f" "$OUT/${name:-shot$i}.png"
done
ls "$OUT"
