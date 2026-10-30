#!/bin/sh
# Offscreen check of the bird feeder and the grocery's feeds (test_feeder.py) in /Game/Dev/L_FeederTest with the real bird
# life: prints the FEEDERTEST and FEEDER lines and copies the screenshots to Saved/FeederTest/<n>_<name>.png.
# Needs the test level (make_tame_test_level.py, then make_feeder_test_level.py) and Characters/Hero copied from the main checkout (cp -c).
# FEED=feed_grain, SLOMO=8, MINUTES=20 (real minutes the tray is watched), AGE=0 change the run. Waits for other offscreen UE runs first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_feeder.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/FeederTest"
mkdir -p "$(dirname "$LOG")" "$OUT"
sh "$ROOT/tools/wait_ue_queue.sh" 7200
rm -f "$SHOTS"/ScreenShot*.png "$OUT"/*.png
export FEED SLOMO MINUTES AGE
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Dev/L_FeederTest -game -RenderOffscreen \
    -windowed -ResX=1280 -ResY=720 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0,lyud.Birds.AnyMap=1$DPC $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_feeder.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-2400}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "FEEDERTEST|FEEDER |Feeder:|LogScript.*Error|Python: Error|Traceback" "$LOG" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
# the shots in the order taken
grep -o "FEEDERTEST shot [0-9]* [a-z_0-9]*" "$LOG" | while read -r _ _ I NAME; do
    F=$(ls "$SHOTS"/ScreenShot*.png 2>/dev/null | sed -n "$((10#$I + 1))p")
    [ -n "$F" ] && cp "$F" "$OUT/${I}_$NAME.png"
done
ls "$OUT"
