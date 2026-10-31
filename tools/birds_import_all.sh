#!/bin/sh
# Imports every built bird (blender/birds/export/<Id>: the species and its hero body) into this checkout, two
# species at a time, never more than two heavy Unreal processes on the Mac counting other checkouts' runs.
#   tools/birds_import_all.sh [Id ...]      (no ids: every species with an export)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IDS="$*"
[ -z "$IDS" ] && IDS=$(cd "$ROOT/blender/birds/export" && for d in */; do [ -f "$d${d%/}.glb" ] && echo "${d%/}"; done)
LOG="$ROOT/blender/birds/export/import_logs"
mkdir -p "$LOG"
slot() {   # wait until fewer than two heavy Unreal runs (game, commandlets) are going on this Mac
    while [ "$(pgrep -f 'UnrealEditor.*(-game|-run=)' | wc -l)" -ge 2 ]; do sleep 5; done
}
one() {
    S="$1"
    slot; "$ROOT/unreal/scripts/import_birds.sh" "$S" > "$LOG/$S.log" 2>&1
    if [ -f "$ROOT/blender/birds/export/$S/${S}_Hero.glb" ] && [ "$S" != FeralPigeon ]; then
        slot; "$ROOT/unreal/scripts/import_birds.sh" "hero:$S" >> "$LOG/$S.log" 2>&1
    fi
    grep -E "BIRDS|Error|Traceback" "$LOG/$S.log" | grep -v LogInit | tail -4 | sed "s/^/$S: /"
}
set -- $IDS
while [ $# -gt 0 ]; do
    one "$1" &
    sleep 20
    [ $# -gt 1 ] && one "$2"
    wait
    shift; [ $# -gt 0 ] && shift
done
