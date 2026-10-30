#!/bin/sh
# Offscreen -game check of the forest floor (shoot_forestfloor.py, docs/forest_floor.md); never opens a window.
#   unreal/scripts/shoot_forestfloor.sh                 autumn: day, close-up, leaves, wet, rime, drought + floor on/off perf
#   FLOOR_MODE=winter unreal/scripts/shoot_forestfloor.sh   forced snow (25 cm, then deep drifts), footprints walk
# PNGs land in Saved/forestfloor_shots/<mode>_<shot>.png; FLOORTEST lines in Saved/Logs/shoot_forestfloor.log.
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
SAVED="$ROOT/unreal/Lyudmilovsk/Saved"
LOG="$SAVED/Logs/shoot_forestfloor.log"
MODE="${FLOOR_MODE:-autumn}"
OUT="$SAVED/forestfloor_shots"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SAVED"/Screenshots/MacEditor/HighresScreenshot*.png
CV="lyud.MainMenuOnStart=0,lyud.Weather.Rain=0,lyud.Weather.Cloud=0.3,lyud.Weather.Fog=0"
[ "$MODE" = "winter" ] && CV="lyud.MainMenuOnStart=0,lyud.Winter.Snow=25,lyud.Winter.Drift=1,lyud.Winter.DriftFrom=250,lyud.Winter.Snowfall=0,lyud.Winter.Age=2,lyud.Winter.Canopy=0.6,lyud.Weather.Rain=0,lyud.Weather.Cloud=0.4,lyud.Weather.Temp=-5,lyud.Weather.Fog=0"
FLOOR_MODE=$MODE "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=$CV${DPCVARS:+,$DPCVARS} $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_forestfloor.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${FLOOR_TIMEOUT:-1200}"; kill -9 $PID 2>/dev/null ) &
WD=$!
wait $PID
kill $WD 2>/dev/null
grep -E "FLOORTEST|Error:.*(Floor|Litter)|Failed to compile Material for|\[SM6\]" "$LOG" | grep -v "LogInit" | head -150
# the shots in the order taken, named after them
NAMES=$(grep -o "FLOORTEST shot [a-z0-9_]*" "$LOG" | awk '{print $3}')
set -- $(ls -tr "$SAVED"/Screenshots/MacEditor/HighresScreenshot*.png 2>/dev/null)
for N in $NAMES; do [ -n "$1" ] && mv "$1" "$OUT/${MODE}_$N.png" && shift; done
ls "$OUT"
