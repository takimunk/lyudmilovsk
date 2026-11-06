#!/bin/sh
# Long soak (runs like tools/bench.sh, not under taskpolicy, so its numbers compare with bench results): the benchmark route at Epic flown ROUNDS times (default 20, ~33 min) with the game clock at SOAK_SCALE x
# (unreal/scripts/soak_clock.py: day -> night -> day), the process footprint sampled every 60 s into <out>/footprint.tsv,
# SOAK lines (frame ms, object counts) in <out>/game.log, the bench JSON (per-round frame + memory) in <out>/bench.json.
#   tools/soak.sh <out dir>        env ROUNDS, SOAK_SCALE, EXTRA
set -e
OUT="${1:?out dir}"; ROUNDS="${ROUNDS:-20}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
"$ROOT/tools/wait_ue_queue.sh" 7200 --exclusive || exit 1
GUS="$ROOT/unreal/Lyudmilovsk/Saved/Config/MacEditor/GameUserSettings.ini"
[ -f "$GUS" ] && mv "$GUS" "$GUS.soak-backup"
restore() { rm -f "$GUS"; [ -f "$GUS.soak-backup" ] && mv "$GUS.soak-backup" "$GUS"; true; }
trap restore EXIT
"$UE" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -windowed -ResX=1920 -ResY=1080 \
    -unattended -nosplash -NoVerifyGC -dpcvars=lyud.MainMenuOnStart=0,r.SetRes=1920x1080w,au.LinearGainScalarForFinalOutut=0 \
    -LyudBench -LyudBenchPresets=3 -LyudBenchRounds="$ROUNDS" -LyudBenchNoWarmup -LyudBenchOut="$OUT/bench.json" -LyudBenchLabel=soak \
    $EXTRA -abslog="$OUT/game.log" "-ExecCmds=py $ROOT/unreal/scripts/soak_clock.py" > /dev/null 2>&1 &
PID=$!; T=0
printf "t\tfootprint_mb\trss_mb\n" > "$OUT/footprint.tsv"
while kill -0 $PID 2>/dev/null; do
    FP=$(footprint -p $PID 2>/dev/null | awk '/Footprint:/ {for (i=1;i<NF;i++) if ($i=="Footprint:") {v=$(i+1); u=$(i+2)}; if (u=="GB") v*=1024; if (u=="KB") v/=1024; print int(v); exit}')
    RSS=$(ps -o rss= -p $PID | awk '{print int($1/1024)}')
    printf "%s\t%s\t%s\n" $T "$FP" "$RSS" >> "$OUT/footprint.tsv"
    sleep 60; T=$((T + 60))
    if [ $T -gt 5400 ]; then kill $PID 2>/dev/null || true; sleep 20; kill -9 $PID 2>/dev/null || true; fi
done
grep -a "SOAK \|BENCH preset" "$OUT/game.log" | sed 's/^\[[^]]*\]\[[^]]*\]//' > "$OUT/soak.txt" || true
grep -a "Critical error\|Ensure condition" "$OUT/game.log" | head -5
tail -3 "$OUT/footprint.tsv"
