#!/bin/sh
# Builds bird species in Blender (3 at a time) and imports them into this checkout (one Unreal process per step).
#   tools/birds_build.sh [Id ...]     (no ids: every species in Content/BirdsData/species.json)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B=/Applications/Blender.app/Contents/MacOS/Blender
IDS="$*"
[ -z "$IDS" ] && IDS=$(python3 -c "import json;print(' '.join(s['id'] for s in json.load(open('$ROOT/unreal/Lyudmilovsk/Content/BirdsData/species.json'))['species']))")
LOG="$ROOT/blender/birds/export"
mkdir -p "$LOG"
n=0
for s in $IDS; do
    ( "$B" -b --python "$ROOT/blender/birds/build_bird.py" -- "$s" > "$LOG/$s.build.log" 2>&1; grep -E "^SIZE|^TRIS|Error|Traceback" "$LOG/$s.build.log" ) &
    n=$((n + 1)); [ $((n % 3)) = 0 ] && wait
done
wait
[ -n "$BIRDS_NO_IMPORT" ] && exit 0   # Blender only (e.g. while the Unreal Mac is held for others)
while pgrep -f "MacOS/UnrealEditor .*lyudmilovsk-wt/birdspecies" >/dev/null; do sleep 5; done
"$ROOT/unreal/scripts/import_birds.sh" masters $IDS
