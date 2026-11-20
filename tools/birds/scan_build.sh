#!/bin/sh
# One bird species from its hi3d scan to the export Unreal imports (blender/birds/export/<Id>/), one Blender at a time:
#   tools/birds/scan_build.sh [--no-prep] Id ...
# scan_prep (body cut + LODs, wing photo, raw copy) -> build_bird.py in scan-rig mode (export, previews, raw-scan
# previews, wing blend previews, lint) -> renders/birdmodels/<Id>_old_raw_new.jpg. Waits while free memory is low.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
B=/Applications/Blender.app/Contents/MacOS/Blender
LOCK="${BIRD_LOCK:-}"
PREP=1
[ "$1" = "--no-prep" ] && { PREP=0; shift; }
mkdir -p blender/birds/scan_raw/logs
for s in "$@"; do
  while [ "$(memory_pressure | tail -1 | grep -oE '[0-9]+')" -lt 12 ]; do sleep 20; done
  if [ $PREP = 1 ]; then
    $LOCK nice -n 19 $B -b --python tools/birds/scan_prep.py -- $s > blender/birds/scan_raw/logs/prep_$s.log 2>&1
  fi
  BIRD_SCAN_CARDS=none BIRD_SCAN_MODE=S $LOCK nice -n 19 $B -b --python blender/birds/build_bird.py -- $s --scan --previews --all-views --head --raw --blend --lint \
      > blender/birds/scan_raw/logs/${s}_final.log 2>&1
  mkdir -p blender/birds/scan_raw/$s
  ln -sfn "$ROOT/blender/birds/export/$s" blender/birds/scan_raw/$s/S
  python3 tools/birds/compare.py $s > /dev/null
  echo "$s $(grep -E '^TRIS|Traceback|Error:' blender/birds/scan_raw/logs/${s}_final.log | head -2 | tr '\n' ' ') $(grep -cE 'LINT.*(FAIL|overlap)' blender/birds/scan_raw/logs/${s}_final.log) lint"
done
