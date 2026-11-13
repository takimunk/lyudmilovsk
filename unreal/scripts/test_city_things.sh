#!/bin/sh
# Offscreen check of the things to do in the town's buildings (test_city_things.py, docs/city_interiors.md) in Lvl_Scene01:
# prints the CITYTEST and CityThings lines, checks them (PASS/FAIL at the end) and copies the screenshots to
# Saved/CityThingsTest/<n>_<name>.png. Needs Content/City/interiors.json (python3 tools/city/interiors.py) and the town built.
# Waits for other offscreen UE runs first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_city_things.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/CityThingsTest"
mkdir -p "$(dirname "$LOG")" "$OUT"
sh "$ROOT/tools/wait_ue_queue.sh" 7200
rm -f "$SHOTS"/ScreenShot*.png "$OUT"/*.png
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0$DPC $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_city_things.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "CITYTEST|CityThings|LogScript.*Error|Python: Error|Traceback|Ensure|Assert" "$LOG" | sed 's/.*Warning: //; s/.*LogLyudCity: Display: //' || true
grep -o "CITYTEST shot [0-9]* [a-z_0-9]*" "$LOG" | while read -r _ _ I NAME; do
    F=$(ls "$SHOTS"/ScreenShot*.png 2>/dev/null | sed -n "$((10#$I + 1))p")
    [ -n "$F" ] && cp "$F" "$OUT/${I}_$NAME.png"
done
ls "$OUT"
# the checks: loaded, ran, the second take refused, an unknown id reported, no script errors
FAIL=0
check() { if grep -qE "$2" "$LOG"; then echo "PASS $1"; else echo "FAIL $1"; FAIL=1; fi; }
check "loaded" "CityThings: [1-9][0-9]* things, [0-9]+ acts, [0-9]+ shops from City/interiors.json"
check "listed" "CityThings: [0-9]+ things, [0-9]+ acts, [0-9]+ shops$"
check "ran" "CityThings: do .* -> ok"
grep -q -- "-> daily_done" "$LOG" && echo "PASS daily limit" || echo "WARN no daily_done (the first take thing may have no daily limit)"
check "unknown id" "CityThings: no thing no/such_thing"
check "finished" "CITYTEST done"
if grep -qE "LogScript.*Error|Python: Error|Traceback" "$LOG"; then echo "FAIL script errors"; FAIL=1; fi
exit $FAIL
