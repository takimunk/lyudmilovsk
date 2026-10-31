#!/bin/sh
# Plays Lvl_Scene01 offscreen in the checkout given (default: this one) and samples the process memory every
# 5 s (phys_footprint as Activity Monitor's "Memory", and RSS) into <out>/footprint.tsv, memreports beside it.
# MODE=pie plays in the editor instead (probe_pie.py: editor idle, then Play In Editor, as Dima plays).
#   [MODE=pie] unreal/scripts/probe_memory.sh <out dir> [checkout root]
OUT="${1:?out dir}"; ROOT="${2:-$(cd "$(dirname "$0")/../.." && pwd)}"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ "$MODE" = pie ]; then ARGS="-unattended -nosplash -gameslotprobe"; SCRIPT="$HERE/probe_pie.py"  # -game* so tools/wait_ue_queue.sh counts it
else ARGS="-game -ResX=1280 -ResY=720 -windowed -nosound -dpcvars=lyud.MainMenuOnStart=0"; SCRIPT="$HERE/probe_memory.py"; fi
mkdir -p "$OUT"
"$(dirname "$0")/../../tools/wait_ue_queue.sh" 7200 || exit 1
"$UE" "$PROJ" /Game/Scene01/Lvl_Scene01 -RenderOffscreen $ARGS -abslog="$OUT/game.log" "-ExecCmds=py $SCRIPT" > /dev/null 2>&1 &
PID=$!
T=0
printf "t\tfootprint_mb\trss_mb\n" > "$OUT/footprint.tsv"
while kill -0 $PID 2>/dev/null; do
    FP=$(footprint -p $PID 2>/dev/null | awk '/Footprint:/ {for (i=1;i<NF;i++) if ($i=="Footprint:") {v=$(i+1); u=$(i+2)}; if (u=="GB") v*=1024; if (u=="KB") v/=1024; print int(v); exit}')
    RSS=$(ps -o rss= -p $PID | awk '{print int($1/1024)}')
    printf "%s\t%s\t%s\n" $T "$FP" "$RSS" >> "$OUT/footprint.tsv"
    sleep 5; T=$((T + 5))
    [ $T -gt 900 ] && kill $PID
done
cp -c "$ROOT"/unreal/Lyudmilovsk/Saved/Profiling/MemReports/*/*_p*_*.memreport "$OUT/" 2>/dev/null
tail -3 "$OUT/footprint.tsv"
