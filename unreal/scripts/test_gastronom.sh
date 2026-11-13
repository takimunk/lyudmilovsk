#!/bin/sh
# Offscreen check of the Gastronom No. 7 pavilion (test_gastronom.py; GASTRO_STEPS=hall,people,trade for some): prints the
# GASTROTEST / SHOPSCROLL lines and copies the screenshots to Saved/GastroTest/<TAG or steps>/<n>_<name>.png. Needs Content/Scene01 and
# Characters/Hero+Humans copied from the main checkout (cp -c). Never opens a window or takes the mouse; waits for a free slot of
# the UE queue; the watchdog kills only this run.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_gastronom.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/GastroTest/${TAG:-${GASTRO_STEPS:-all}}"
mkdir -p "$(dirname "$LOG")" "$OUT"
QUIET=$("$ROOT/tools/ue_quiet_args.sh")
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
rm -f "$SHOTS"/ScreenShot*.png "$OUT"/*.png
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game \
    -RenderOffscreen -windowed -ResX=${RES_X:-1600} -ResY=${RES_Y:-900} $QUIET -dpcvars=lyud.MainMenuOnStart=0$DPC $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_gastronom.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-1200}"; ps -p "$PID" -o args= | grep -q test_gastronom.py && kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "GASTROTEST|SHOPSCROLL|People:|LogScript.*Error|Critical error" "$LOG" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
# the shots in the order taken
grep -o "GASTROTEST shot [0-9]* [a-z_0-9]*" "$LOG" | while read -r _ _ I NAME; do
    F=$(ls -tr "$SHOTS"/ScreenShot*.png 2>/dev/null | sed -n "$((10#$I + 1))p")
    [ -n "$F" ] && cp "$F" "$OUT/${I}_$NAME.png"
done
ls "$OUT"
