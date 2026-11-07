#!/bin/sh
# Offscreen check of the restaurant in Lvl_Scene01 (test_restaurant.py; REST_STEPS=outside,sit,menu,order,waiter_walk,serve,eat,clear,bill,night for some,
# REST_LANG=en|ja): prints the RESTTEST lines and copies the screenshots to Saved/RestaurantTest/<n>_<name>.png. Needs
# Content/Scene01, Characters/Hero+Humans, Voice copied from the main checkout (cp -c) and the diner placed
# (import_restaurant.py place). Waits for other offscreen UE runs first. SOUND=1 keeps the sound on.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_restaurant.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/RestaurantTest"
mkdir -p "$(dirname "$LOG")" "$OUT"
sh "$ROOT/tools/wait_ue_queue.sh" 7200
rm -f "$SHOTS"/ScreenShot*.png "$OUT"/*.png
SND="-nosound"; [ "$SOUND" = 1 ] && SND="-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1"
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash $SND -dpcvars=lyud.MainMenuOnStart=0$DPC $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_restaurant.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-2400}"; kill "$PID" 2>/dev/null ) &
DOG=$!
# a fatal start (e.g. no Metal toolchain: a message box, then RequestExit that never returns) ends the run at once
( while kill -0 "$PID" 2>/dev/null; do
      grep -q "RequestExit(1" "$LOG" 2>/dev/null && { echo "RESTTEST fatal start: $(grep -m1 -o 'text: .*' "$LOG")"; kill "$PID"; break; }
      sleep 5
  done ) &
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "RESTTEST|HANDUSE|LogScript.*Error|Error:.*Rest|Ensure|Assert" "$LOG" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
# the shots in the order taken
grep -o "RESTTEST shot [0-9]* [a-z_0-9]*" "$LOG" | while read -r _ _ I NAME; do
    F=$(ls "$SHOTS"/ScreenShot*.png 2>/dev/null | sed -n "$((10#$I + 1))p")
    [ -n "$F" ] && cp "$F" "$OUT/${I}_$NAME.png"
done
ls "$OUT"
