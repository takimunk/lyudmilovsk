#!/bin/sh
# Underwater checks outside the engine (docs/underwater.md). Run from anywhere.
#   tools/underwater/run.sh bed    -> writes fresh terrain inputs to Saved/Underwater/bed (tools/terrain.py), runs the
#                                     terrain generator on them and draws the sea bed: bed_depth.png, bed_stone.png
#   tools/underwater/run.sh test   -> the current, fish and weed models' checks (underwatertest.cpp)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
OUT="$ROOT/Saved/Underwater"
mkdir -p "$OUT/bed"
case "$1" in
bed)
    clang++ -std=c++17 -O2 -Wall -I"$SRC/Terrain" -o "$OUT/bedgen" "$ROOT/tools/underwater/bedgen.cpp" "$SRC/Terrain/TerrainGen.cpp"
    [ -n "$KEEP_INPUTS" ] || python3 -c "import sys; sys.path.insert(0, '$ROOT/tools'); import terrain; terrain.write_inputs('$OUT/bed')"
    "$OUT/bedgen" "$OUT/bed" "$OUT/bed"
    python3 -c "
from PIL import Image
for n in ('bed_depth', 'bed_stone'):
    Image.open('$OUT/bed/%s.pgm' % n).save('$OUT/bed/%s.png' % n)
print('$OUT/bed/bed_depth.png')"
    ;;
test)
    clang++ -std=c++17 -O2 -Wall -I"$SRC/Sea" -o "$OUT/underwatertest" "$ROOT/tools/underwater/underwatertest.cpp" "$SRC/Sea/SeaCurrent.cpp" "$SRC/Sea/FishModel.cpp"
    exec "$OUT/underwatertest" "${@:2}"
    ;;
*) echo "usage: $0 bed|test"; exit 2 ;;
esac
