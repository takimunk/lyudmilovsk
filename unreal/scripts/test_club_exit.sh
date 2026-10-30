#!/bin/sh
# Offscreen walk out of the club through the blast door and back, plus the hall's sphere check and shots
# (test_club_exit.py); never opens a window.
#   unreal/scripts/test_club_exit.sh      CLUBEXIT_WINTER=1 (35 cm forced snow)  CLUBEXIT_TAG=before  CLUBEXIT_SHOTS=0
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_club_exit.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/clubexit_shots"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SHOTS"/HighresScreenshot*.png
CV="lyud.MainMenuOnStart=0"
[ -n "$CLUBEXIT_WINTER" ] && CV="$CV,lyud.Winter.Snow=35,lyud.Winter.Drift=1,lyud.Winter.DriftFrom=250,lyud.Winter.Snowfall=0,lyud.Weather.Rain=0,lyud.Weather.Temp=-6"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=$CV${DPCVARS:+,$DPCVARS} \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_club_exit.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${CLUBEXIT_TIMEOUT:-600}"; kill -9 $PID 2>/dev/null ) &
WD=$!
wait $PID
kill $WD 2>/dev/null
grep -E "CLUBEXIT" "$LOG" | grep -v "LogInit" | sed "s/.*CLUBEXIT //"
# the shots in the order taken, named after the spots
names=$(grep -E "CLUBEXIT shot " "$LOG" | sed "s/.*CLUBEXIT shot //")
i=1
for f in $(ls -tr "$SHOTS"/HighresScreenshot*.png 2>/dev/null); do
    n=$(echo "$names" | sed -n "${i}p")
    cp "$f" "$OUT/${CLUBEXIT_TAG:-run}_${n:-$i}.png"
    i=$((i + 1))
done
