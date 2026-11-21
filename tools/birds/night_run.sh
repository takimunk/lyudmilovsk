#!/bin/sh
# The whole bird set from cached scans: Blender (bodies, then hero bodies) in one loop, the Unreal import in another
# that takes each species (then its hero) as soon as its build is newer than the start.
#   tools/birds/night_run.sh blender|import
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
SP="Rook HerringGull FeralPigeon HouseSparrow Starling GreatTit Magpie Peregrine Blackbird Robin HoodedCrow Jackdaw WoodPigeon BlackHeadedGull CommonGull Bullfinch TreeSparrow CarrionCrow Raven BarnSwallow SandMartin WhiteWagtail SongThrush Fieldfare Waxwing Jay Nutcracker Sparrowhawk Cormorant Goldfinch Chaffinch WhiteTailedEagle GoldenEagle BlackBrowedAlbatross"
HEROES="HerringGull Rook FeralPigeon HouseSparrow Starling GreatTit Magpie Peregrine Blackbird Robin HoodedCrow Jackdaw WoodPigeon BlackHeadedGull CommonGull Bullfinch TreeSparrow CarrionCrow BarnSwallow SandMartin WhiteWagtail SongThrush Fieldfare Waxwing Jay Nutcracker Sparrowhawk Cormorant Goldfinch Chaffinch"
T0=$(cat "${T0_FILE:?}")
B=/Applications/Blender.app/Contents/MacOS/Blender
L="${BIRD_LOCK:-}"
newer() { [ -f "$1" ] && [ $(stat -f %m "$1") -gt $T0 ] && grep -q "Blender quit" "$1"; }
if [ "$1" = blender ]; then
  for s in $SP; do
    newer blender/birds/scan_raw/logs/${s}_final.log || BIRD_FAST=1 BIRD_LOCK="$L" tools/birds/scan_build.sh $s
  done
  for s in $HEROES; do
    while [ "$(memory_pressure | tail -1 | grep -oE '[0-9]+')" -lt 12 ]; do sleep 20; done
    BIRD_SCAN_CARDS=none BIRD_SCAN_MODE=S $L nice -n 19 $B -b --python blender/birds/build_bird.py -- $s --hero --scan --previews > blender/birds/scan_raw/logs/${s}_hero.log 2>&1
    echo "$s hero $(grep -E '^TRIS|Traceback|Error:' blender/birds/scan_raw/logs/${s}_hero.log | head -2 | tr '\n' ' ')"
  done
else
  for s in $SP; do
    until newer blender/birds/scan_raw/logs/${s}_final.log; do sleep 20; done
    tools/birds/import_queue.sh $s
  done
  for s in $HEROES; do
    until newer blender/birds/scan_raw/logs/${s}_hero.log; do sleep 20; done
    tools/birds/import_queue.sh hero:$s
  done
fi
