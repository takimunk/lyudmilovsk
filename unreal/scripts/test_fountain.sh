#!/bin/sh
# Offscreen -game test of the Academgorodok fountain (test_fountain.py): water, season, sound; never opens a window and
# the speakers stay silent (au.LinearGainScalarForFinalOutut 0). Shots are copied to Saved/FountainShots/<name>.png.
#   unreal/scripts/test_fountain.sh               (FOUNTAIN_SHOTS=front,close for some only)
#   FOUNTAIN_EXTRA=-ueexclusive ...               (an A/B: tools/wait_ue_queue.sh starts nothing else meanwhile)
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_fountain.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/FountainShots"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SHOTS"/HighresScreenshot*.png "$OUT"/*.png
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0${DPCVARS:+,$DPCVARS} \
    -abslog="$LOG" $FOUNTAIN_EXTRA "-ExecCmds=py $ROOT/unreal/scripts/test_fountain.py" > "$LOG.stdout" 2>&1 || true
grep -E "FOUNTAIN|LyudFountain|Error:" "$LOG" | grep -v "LogInit" | head -80
i=0
for f in $(ls "$SHOTS"/HighresScreenshot*.png 2>/dev/null | sort); do
    i=$((i + 1))
    name=$(grep "FOUNTAINSHOT" "$LOG" | sed -n "${i}p" | sed -E 's/.*FOUNTAINSHOT ([a-z0-9_]+).*/\1/')
    cp "$f" "$OUT/${name:-shot$i}.png"
done
ls "$OUT"
grep -q "FOUNTAINTEST done: 0 failed" "$LOG"
