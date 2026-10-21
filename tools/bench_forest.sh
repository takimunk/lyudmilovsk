#!/bin/sh
# Measures what each forest option would gain (docs/benchmark.md, "Forest variants"): the outdoor part of the route
# at Epic, every variant in turn in ONE process and the whole list twice, so a machine whose load drifts (other
# editors, compiles) affects every variant alike; then each variant against "base".
#   tools/bench_forest.sh                     every variant below
#   tools/bench_forest.sh noshadow skyfreeze  base and these
# Env: RES (default 1920x1080), PRESET (default 3 = Epic), ROUNDS (default 2), SEGMENTS (default terrace,forest,overlook).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
variant() {
    case "$1" in
        base)       echo "" ;;
        nolive)     echo "lyud.Bench.LiveHide 1" ;;
        noforest)   echo "lyud.Bench.ForestHide 1" ;;
        notrees)    echo "lyud.Bench.ForestHide 1;lyud.Bench.LiveHide 1" ;;
        noshadow)   echo "lyud.Bench.ForestShadows 0" ;;
        channel1)   echo "lyud.Bench.ForestChannel0 0" ;;
        skyfreeze)  echo "lyud.Bench.SkyFreeze 1" ;;
        cull300)    echo "lyud.Bench.ForestCullM 300" ;;
        nowind)     echo "lyud.Bench.ForestWPODisableM 1" ;;
        minlod1)    echo "lyud.Bench.ForestMinLod 1" ;;
        minlod2)    echo "lyud.Bench.ForestMinLod 2" ;;
        csm)        echo "r.Shadow.Virtual.Enable 0" ;;
        nolumen)    echo "r.DynamicGlobalIlluminationMethod 0;r.ReflectionMethod 2" ;;
        sp67)       echo "r.ScreenPercentage 67" ;;
        *)          echo "?" ;;
    esac
}
ALL="nolive noforest notrees noshadow skyfreeze cull300 nowind minlod1 csm nolumen sp67"
PASSES="base="
for V in ${*:-$ALL}; do
    S="$(variant "$V")"
    [ "$S" = "?" ] && { echo "unknown variant $V"; exit 1; }
    PASSES="$PASSES|$V=$(echo "$S" | tr ' ' '+')"
done
LABEL="forest" EXTRA="-LyudBenchSegments=${SEGMENTS:-terrace,forest,overlook} -LyudBenchPasses=$PASSES -LyudBenchRounds=${ROUNDS:-2} $EXTRA" \
    "$ROOT/tools/bench.sh" "${PRESET:-3}" | grep -E "^bench:|BENCH (start|forests baked|wrote)"
F="$(ls -t "$ROOT"/bench/results/*/*_forest.json 2>/dev/null | head -1)"
[ -n "$F" ] && python3 "$ROOT/tools/bench_report.py" --passes "$F"
