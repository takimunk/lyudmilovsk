#!/bin/sh
# Offscreen check of the hatch to Europa in Lvl_Scene01 (test_europa.py): prints the EUROPATEST lines and copies the
# screenshots to Saved/EuropaTest/<n>_<name>.png. Needs Content/Scene01 (built with the hatch's landscape hole:
# tools/terrain.py europa_holes), Characters/Hero+Humans, Voice copied from the main checkout (cp -c), and the Europa
# assets imported (unreal/scripts/import_europa.sh). Waits for other offscreen UE runs first. SOUND=1 keeps the sound on
# (silent output).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_europa.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/EuropaTest"
mkdir -p "$(dirname "$LOG")" "$OUT"
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
rm -f "$SHOTS"/ScreenShot*.png "$OUT"/*.png
SND="-nosound"; [ "$SOUND" = 1 ] && SND="-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1 -ini:Engine:[ConsoleVariables]:au.LinearGainScalarForFinalOutut=0"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash $SND -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_europa.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "EUROPATEST|EUROPA |Europa:|LogScript.*Error|Error:.*Europa" "$LOG" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
N=0
grep -o "EUROPATEST shot [0-9]* [a-z_0-9]*" "$LOG" | while read -r _ _ I NAME; do
    F=$(ls "$SHOTS"/ScreenShot*.png 2>/dev/null | sed -n "$((10#$I + 1))p")
    [ -n "$F" ] && cp "$F" "$OUT/${I}_$NAME.png"
done
ls "$OUT"
