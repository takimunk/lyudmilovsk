#!/bin/sh
# Imports finished birds into this checkout one at a time, each waiting for an offscreen Unreal slot
# (tools/wait_ue_queue.sh). hero:<Id> imports only the hero body.
#   tools/birds/import_queue.sh Rook hero:Rook ...
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
mkdir -p blender/birds/scan_raw/logs
for s in "$@"; do
  while [ "$(memory_pressure | tail -1 | grep -oE '[0-9]+')" -lt 15 ]; do sleep 30; done
  tools/wait_ue_queue.sh 7200 || { echo "$s: no UE slot"; continue; }
  n=$(echo "$s" | tr ':' '_')
  nice -n 10 unreal/scripts/import_birds.sh "$s" > blender/birds/scan_raw/logs/import_$n.log 2>&1
  echo "$s: $(grep -E 'BIRDS' blender/birds/scan_raw/logs/import_$n.log | tail -2 | tr '\n' ' ') $(grep -cE 'Error|Traceback' blender/birds/scan_raw/logs/import_$n.log) err"
done
