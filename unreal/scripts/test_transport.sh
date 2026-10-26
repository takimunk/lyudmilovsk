#!/bin/sh
# Offscreen check of the cable-car metro in Lvl_Scene01 (test_transport.py): prints the TRANSPORTTEST lines and copies the
# screenshots to Saved/TransportTest/<n>_<name>.png. Needs Content/Scene01, Characters/Hero+Humans, Voice copied from the
# main checkout (cp -c). Waits for other offscreen UE runs first. SOUND=1 keeps the sound on (silent output).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_transport.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/TransportTest"
mkdir -p "$(dirname "$LOG")" "$OUT"
# the network resolved against this checkout's terrain (not committed)
[ -f "$ROOT/unreal/Lyudmilovsk/Content/Transport/network_built.json" ] || python3 "$ROOT/tools/transport.py" > /dev/null
sh "$ROOT/tools/wait_ue_queue.sh" 7200
rm -f "$SHOTS"/ScreenShot*.png "$OUT"/*.png
SND="-nosound"; [ "$SOUND" = 1 ] && SND="-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash $SND -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_transport.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-1200}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "TRANSPORTTEST|Transport:|LogScript.*Error|Error:.*Transport" "$LOG" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
# the shots in the order taken
N=0
grep -o "TRANSPORTTEST shot [0-9]* [a-z_0-9]*" "$LOG" | while read -r _ _ I NAME; do
    F=$(ls "$SHOTS"/ScreenShot*.png 2>/dev/null | sed -n "$((10#$I + 1))p")
    [ -n "$F" ] && cp "$F" "$OUT/${I}_$NAME.png"
done
ls "$OUT"
