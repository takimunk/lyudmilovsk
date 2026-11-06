#!/bin/sh
# What each graphics setting at one level costs or saves (docs/graphics.md): the whole route at Epic with every variant
# in turn in ONE process and the whole list twice (docs/benchmark.md, "Forest variants" for why), then each variant
# against "base" (the level as Config/DefaultScalability.ini has it).
#   tools/bench_gfx.sh                       every variant below
#   tools/bench_gfx.sh tsr100 wind0          base and these
#   PRESET=2 tools/bench_gfx.sh              the same at High
# Env: PRESET (default 3 = Epic; one level per run: a pass's console values stay set for the rest of the process),
#      ROUNDS (default 2), SEGMENTS (default: the whole route), RES (default 1920x1080; 3024x1964 is a fullscreen Retina).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
variant() {
    case "$1" in
        base)          echo "" ;;
        tsr100)        echo "r.TSR.History.ScreenPercentage 100" ;;
        tsr150)        echo "r.TSR.History.ScreenPercentage 150" ;;
        sp67)          echo "r.ScreenPercentage 67" ;;
        sp100)         echo "r.ScreenPercentage 100" ;;
        wind0)         echo "lyud.Forest.WindDistance 0" ;;
        wind60)        echo "lyud.Forest.WindDistance 60" ;;
        wind250)       echo "lyud.Forest.WindDistance 250" ;;
        fdist70)       echo "lyud.Forest.DistanceScale 0.7" ;;
        noforestshadow) echo "lyud.Forest.Shadows 0" ;;
        nolive)        echo "lyud.Forest.MaxLive 0" ;;
        vsmbias10)     echo "r.Shadow.Virtual.ResolutionLodBiasDirectional -1;r.Shadow.Virtual.ResolutionLodBiasDirectionalMoving -1" ;;
        vsmbias05)     echo "r.Shadow.Virtual.ResolutionLodBiasDirectional -0.5;r.Shadow.Virtual.ResolutionLodBiasDirectionalMoving -0.5" ;;
        smrt4)         echo "r.Shadow.Virtual.SMRT.RayCountDirectional 4" ;;
        shadowlod2)    echo "r.Shadow.LODDistanceFactor 2" ;;
        lodscale125)   echo "r.StaticMeshLODDistanceScale 1.25" ;;
        vdist08)       echo "r.ViewDistanceScale 0.8" ;;
        clouds075)     echo "lyud.Sky.CloudSamples 0.75" ;;
        clouds05)      echo "lyud.Sky.CloudSamples 0.5" ;;
        grass085)      echo "lyud.GrassDensity 0.85" ;;
        floor075)      echo "lyud.ForestFloor.Density 0.75" ;;
        sea1)          echo "lyud.Sea.Quality 1" ;;
        birds8)        echo "lyud.Birds.Bodies 8;lyud.Birds.DrawDistance 550" ;;
        shafts0)       echo "r.LightShaftQuality 0" ;;
        *)             echo "?" ;;
    esac
}
ALL="tsr100 sp67 wind0 wind250 fdist70 vsmbias10 smrt4 shadowlod2 lodscale125 clouds075 grass085 sea1"
PASSES="base="
for V in ${*:-$ALL}; do
    S="$(variant "$V")"
    [ "$S" = "?" ] && { echo "unknown variant $V"; exit 1; }
    PASSES="$PASSES|$V=$(echo "$S" | tr ' ' '+')"
done
LABEL="gfx" EXTRA="${SEGMENTS:+-LyudBenchSegments=$SEGMENTS} -LyudBenchPasses=$PASSES -LyudBenchRounds=${ROUNDS:-2} $EXTRA" \
    "$ROOT/tools/bench.sh" "${PRESET:-3}" | grep -E "^bench:|BENCH (start|forests baked|wrote)"
F="$(ls -t "$ROOT"/bench/results/*/*_gfx.json 2>/dev/null | head -1)"
[ -n "$F" ] && python3 "$ROOT/tools/bench_report.py" --passes "$F"
