#!/bin/sh
# Runs the benchmark route (ULyudBenchmark, docs/benchmark.md) offscreen and files the result under
# bench/results/<machine>/<date>_<commit>[_<label>].json, then prints the summary table.
#   tools/bench.sh                    all four presets (Low, Medium, High, Epic) at 1920x1080
#   tools/bench.sh 3                  Epic only
#   RES=2560x1440 LABEL=noshadow SETUP="lyud.Bench.ForestShadows 0" tools/bench.sh 3
# Env: RES (WxH), LABEL (file suffix and note), SETUP (console commands, ';'-separated, run after the forests bake),
#      MAP (default /Game/Scene01/Lvl_Scene01), EXTRA (more command-line arguments), UE_ROOT.
# Never opens a window and leaves running editors alone.
set -e
PRESETS="${1:-0,1,2,3}"
RES="${RES:-1920x1080}"
MAP="${MAP:-/Game/Scene01/Lvl_Scene01}"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"

COMMIT="$(git -C "$ROOT" rev-parse --short HEAD)"
git -C "$ROOT" diff --quiet HEAD -- unreal/Lyudmilovsk/Source || COMMIT="$COMMIT-dirty"
MACHINE="$(sysctl -n hw.model 2>/dev/null | tr ',' '-' || uname -m)-$(sysctl -n machdep.cpu.brand_string 2>/dev/null | tr -cd 'A-Za-z0-9' || true)"
STAMP="$(date +%Y%m%d-%H%M%S)"
NAME="${STAMP}_${COMMIT}${LABEL:+_$LABEL}"
OUT="$ROOT/bench/results/$MACHINE/$NAME.json"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/bench_$NAME.log"
mkdir -p "$(dirname "$OUT")" "$(dirname "$LOG")"

BUSY=$(pgrep -f "UnrealEditor|UnrealEditor-Cmd|ShaderCompileWorker|clang" | wc -l | tr -d ' ')
if [ "$BUSY" -gt 0 ]; then echo "bench: warning, $BUSY other Unreal/compiler processes are running; numbers will be noisy"; fi
# the editor's -game keeps resolution and scalability in Saved/Config/MacEditor/GameUserSettings.ini: run without
# it (it would override -ResX and the presets) and put it back afterwards
GUS="$ROOT/unreal/Lyudmilovsk/Saved/Config/MacEditor/GameUserSettings.ini"
if [ -f "$GUS" ]; then mv "$GUS" "$GUS.bench-backup"; fi
echo "bench: presets $PRESETS at $RES, $MAP, commit $COMMIT -> $OUT"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" "$MAP" -game -RenderOffscreen \
    -windowed -ResX="${RES%x*}" -ResY="${RES#*x}" -unattended -nosplash -nosound -NoVerifyGC \
    -dpcvars=lyud.MainMenuOnStart=0,r.SetRes=${RES}w -LyudBench -LyudBenchPresets="$PRESETS" -LyudBenchOut="$OUT" \
    -LyudBenchLabel="$COMMIT${LABEL:+ $LABEL}${BUSY:+ busy=$BUSY}" ${SETUP:+"-LyudBenchSetup=$(echo "$SETUP" | tr ' ' '+')"} $EXTRA -abslog="$LOG" > "$LOG.stdout" 2>&1 || true
rm -f "$LOG.stdout" "$GUS"
if [ -f "$GUS.bench-backup" ]; then mv "$GUS.bench-backup" "$GUS"; fi
grep "BENCH" "$LOG" | sed 's/.*BENCH/BENCH/' || true
if [ ! -f "$OUT" ]; then echo "bench: no result, see $LOG"; exit 1; fi
python3 "$ROOT/tools/bench_report.py" "$OUT"
