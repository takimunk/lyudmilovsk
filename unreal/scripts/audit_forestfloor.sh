#!/bin/sh
# Offscreen -game check that the forest floor keeps out of the town, the Academgorodok, yards, paths and buildings
# (audit_forestfloor.py, docs/forest_floor.md); never opens a window. Winter, forced snow.
# Shots in Saved/forestfloor_shots/audit_<before|after>_<view>.png, FLOORAUDIT lines, then tools/forestfloor_audit.py
# on the occupancy files (must print 0 for after.ffa).
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
SAVED="$ROOT/unreal/Lyudmilovsk/Saved"
LOG="$SAVED/Logs/audit_forestfloor.log"
OUT="$SAVED/forestfloor_shots"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SAVED"/Screenshots/MacEditor/HighresScreenshot*.png "$SAVED"/ForestFloorAudit/*.ffa
CV="lyud.MainMenuOnStart=0,lyud.ForestFloor.Guard=${FLOOR_BEFORE:--1},lyud.Winter.Snow=25,lyud.Winter.Drift=1,lyud.Winter.DriftFrom=250,lyud.Winter.Snowfall=0,lyud.Winter.Age=2,lyud.Winter.Canopy=0.6,lyud.Weather.Rain=0,lyud.Weather.Cloud=0.4,lyud.Weather.Temp=-5,lyud.Weather.Fog=0"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=$CV${DPCVARS:+,$DPCVARS} $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/audit_forestfloor.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${FLOOR_TIMEOUT:-2400}"; kill -9 $PID 2>/dev/null ) &
WD=$!
wait $PID
kill $WD 2>/dev/null
grep -E "FLOORAUDIT|ForestFloor: sites|Error:.*(Floor|Litter)" "$LOG" | grep -v "LogInit" | head -80
NAMES=$(grep -o "FLOORAUDIT shot [a-z0-9_]*" "$LOG" | awk '{print $3}')
set -- $(ls -tr "$SAVED"/Screenshots/MacEditor/HighresScreenshot*.png 2>/dev/null)
for N in $NAMES; do [ -n "$1" ] && mv "$1" "$OUT/audit_$N.png" && shift; done
ls "$OUT" | grep audit_
ls "$SAVED"/ForestFloorAudit/*.ffa >/dev/null 2>&1 && python3 "$ROOT/tools/forestfloor_audit.py" "$SAVED"/ForestFloorAudit/*.ffa
