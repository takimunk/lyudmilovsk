#!/bin/sh
# The North outside the engine (docs/north.md). Run from anywhere.
#   tools/north_sim/run.sh ground  -> fresh terrain inputs (tools/terrain.py; KEEP_INPUTS=1 reuses them), the terrain
#                                     generator on them, Saved/North/north_ground.png (layers + relief of the North)
#                                     and Saved/North/terrain_out.bin (copy it to unreal/Lyudmilovsk/Saved/World for
#                                     ground_z without Unreal)
#   tools/north_sim/run.sh data    -> rewrites Content/North/north.json and Content/Dev/teleports/north.json
#   tools/north_sim/run.sh model   -> checks the game side's plain rules (Public/North/NorthModel.h)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
OUT="$ROOT/Saved/North"
mkdir -p "$OUT"
case "$1" in
ground)
    clang++ -std=c++17 -O2 -Wall -Wshadow -I"$SRC/Terrain" -o "$OUT/northgen" "$ROOT/tools/north_sim/northgen.cpp" "$SRC/Terrain/TerrainGen.cpp"
    [ -n "$KEEP_INPUTS" ] || python3 -c "import sys; sys.path.insert(0, '$ROOT/tools'); import terrain; terrain.write_inputs('$OUT')"
    "$OUT/northgen" "$OUT" "$OUT" "${2:-2000}"
    python3 -c "
from PIL import Image
Image.open('$OUT/north_ground.ppm').save('$OUT/north_ground.png')
print('$OUT/north_ground.png')"
    ;;
data)
    python3 "$ROOT/tools/north.py"
    ;;
model)
    # the game side's plain rules (Public/North/NorthModel.h): wake ranges, mud bubbles, the camp fire, vent columns
    clang++ -std=c++17 -O2 -Wall -Wextra -Wshadow -Werror -I"$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Public/North" -o "$OUT/north_model" "$ROOT/tools/north_sim/model_check.cpp"
    "$OUT/north_model"
    ;;
*) echo "usage: $0 ground|data|model"; exit 2 ;;
esac
