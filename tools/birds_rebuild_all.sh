#!/bin/sh
# Rebuilds every bird species in Blender, the species body and its first-person hero body, linted, with previews,
# 3 Blender processes at a time (no Unreal). Logs in blender/birds/export/<Id>.{build,hero}.log.
#   tools/birds_rebuild_all.sh [Id ...]
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B=/Applications/Blender.app/Contents/MacOS/Blender
IDS="$*"
[ -z "$IDS" ] && IDS=$(python3 -c "import json;print(' '.join(s['id'] for s in json.load(open('$ROOT/unreal/Lyudmilovsk/Content/BirdsData/species.json'))['species']))")
LOG="$ROOT/blender/birds/export"
mkdir -p "$LOG"
one() {
    "$B" -b --python "$ROOT/blender/birds/build_bird.py" -- "$1" --lint --previews > "$LOG/$1.build.log" 2>&1
    # the pigeon's hero body is a generated model (blender/birds/hero_pigeon), not this generator's
    [ "$1" != FeralPigeon ] && "$B" -b --python "$ROOT/blender/birds/build_bird.py" -- "$1" --hero --lint --previews > "$LOG/$1.hero.log" 2>&1
    echo "$1: $(grep -E '^SIZE' "$LOG/$1.build.log" | cut -c1-60) $(grep -cE 'Error|Traceback' "$LOG/$1.build.log" "$LOG/$1.hero.log" 2>/dev/null | tr '\n' ' ')"
}
n=0
for s in $IDS; do
    one "$s" &
    n=$((n + 1)); [ $((n % 3)) = 0 ] && wait
done
wait
