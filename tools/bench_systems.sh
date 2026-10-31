#!/bin/sh
# What each live system costs: the whole route at Epic with each system switched off in turn, in ONE process,
# the list twice (docs/benchmark.md, "Forest variants" for why one process), then each pass against "base".
#   tools/bench_systems.sh                  every system below
#   tools/bench_systems.sh nobirds nopeople base and these
# Env: PRESET (default 3 = Epic), ROUNDS (default 2), SEGMENTS (default: the whole route).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
variant() {
    case "$1" in
        base)        echo "" ;;
        nobirds)     echo "lyud.Birds 0" ;;
        nopeople)    echo "lyud.People 0" ;;
        noanimals)   echo "lyud.Animals 0" ;;
        notransport) echo "lyud.Transport 0" ;;
        norailway)   echo "lyud.Railway 0" ;;
        nosea)       echo "lyud.Sea 0" ;;
        noacadem)    echo "lyud.Academ 0" ;;
        nofloor)     echo "lyud.ForestFloor 0" ;;
        nosound)     echo "lyud.Sound 0" ;;
        novines)     echo "lyud.Vines 0" ;;
        noforest)    echo "lyud.Bench.ForestHide 1;lyud.Bench.LiveHide 1" ;;
        nolumen)     echo "r.DynamicGlobalIlluminationMethod 0;r.ReflectionMethod 2" ;;
        cardfreeze)  echo "r.LumenScene.SurfaceCache.Freeze 1" ;;
        captures64)  echo "r.LumenScene.SurfaceCache.CardCapturesPerFrame 64" ;;
        lumenlow)    echo "sg.GlobalIlluminationQuality 2;sg.ReflectionQuality 2" ;;
        shadowhigh)  echo "sg.ShadowQuality 2" ;;
        sp75)        echo "r.ScreenPercentage 75" ;;
        *)           echo "?" ;;
    esac
}
ALL="nobirds nopeople noanimals notransport norailway nosea noacadem nofloor nosound noforest nolumen"
PASSES="base="
for V in ${*:-$ALL}; do
    S="$(variant "$V")"
    [ "$S" = "?" ] && { echo "unknown variant $V"; exit 1; }
    PASSES="$PASSES|$V=$(echo "$S" | tr ' ' '+')"
done
LABEL="systems" EXTRA="${SEGMENTS:+-LyudBenchSegments=$SEGMENTS} -LyudBenchPasses=$PASSES -LyudBenchRounds=${ROUNDS:-2} $EXTRA" \
    "$ROOT/tools/bench.sh" "${PRESET:-3}" | grep -E "^bench:|BENCH (start|forests baked|wrote)"
F="$(ls -t "$ROOT"/bench/results/*/*_systems.json 2>/dev/null | head -1)"
[ -n "$F" ] && python3 "$ROOT/tools/bench_report.py" --passes "$F"
