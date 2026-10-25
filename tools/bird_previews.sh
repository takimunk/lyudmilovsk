#!/bin/sh
# Blender previews of bird species (blender/birds/build_bird.py --previews --no-export), 3 at a time, then a comparison
# sheet per species next to a reference photo (refs dir: <Id>.jpg, not in the repo).
#   tools/bird_previews.sh <refs dir> <out dir> Id Id ...
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B=/Applications/Blender.app/Contents/MacOS/Blender
REFS="$1"; OUT="$2"; shift 2
mkdir -p "$OUT"
n=0
for s in "$@"; do
    ( "$B" -b --python "$ROOT/blender/birds/build_bird.py" -- "$s" --previews --no-export > "$OUT/$s.log" 2>&1
      P="$ROOT/blender/birds/export/$s/previews"
      "$B" -b --python "$ROOT/blender/birds/contact_sheet.py" -- "$OUT/cmp_$s.png" 3 400 "$REFS/$s.jpg" "$P/${s}_side.png" "$P/${s}_34.png" \
          "$P/${s}_fly_top.png" "$P/${s}_glide_below.png" "$P/${s}_gait.png" > /dev/null 2>&1
      grep -E "SIZE|Error|Traceback" "$OUT/$s.log" ) &
    n=$((n + 1))
    [ $((n % 3)) = 0 ] && wait
done
wait
