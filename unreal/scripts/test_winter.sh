#!/bin/sh
# Offscreen -game check of the winter (test_winter.py): deep drifted snow and a frozen sea forced by cvars; never opens a window.
#   unreal/scripts/test_winter.sh            WINTER_SHOTS=door,beach_ice  EXTRA="-ini:..."  DPCVARS=lyud.Winter.Snow=10
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_winter.log"
mkdir -p "$(dirname "$LOG")"
rm -f "$ROOT"/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor/HighresScreenshot*.png
CV="lyud.MainMenuOnStart=0,lyud.Winter.Snow=35,lyud.Winter.Drift=1,lyud.Winter.DriftFrom=250,lyud.Winter.Snowfall=0,lyud.Winter.Age=1,lyud.Winter.Canopy=0.8,lyud.Winter.Ice=1,lyud.Winter.IceCm=25,lyud.Weather.Rain=0,lyud.Weather.Cloud=0.35,lyud.Weather.Temp=-6"
# WINTER_REAL=1 WINTER_DATE=2027-01-12-11: the weather's own winter on that day, nothing forced
[ -n "$WINTER_REAL" ] && CV="lyud.MainMenuOnStart=0"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=$CV${DPCVARS:+,$DPCVARS} $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_winter.py" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: an offscreen game that hangs never quits by itself
( sleep "${WINTER_TIMEOUT:-900}"; kill -9 $PID 2>/dev/null ) &
WD=$!
wait $PID
kill $WD 2>/dev/null
grep -E "WINTERTEST|LyudWinter|Error:.*(Winter|Snow|Ice)|failed to compile" "$LOG" | grep -v "LogInit" | head -60
ls "$ROOT"/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor/
