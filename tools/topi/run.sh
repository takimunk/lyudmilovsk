#!/bin/sh
# Builds and runs Topi's model outside the engine (tools/topi/topitest.cpp). Run from anywhere.
#   tools/topi/run.sh test      -> checks, exits 1 on failure
#   tools/topi/run.sh map       -> Saved/Topi/topi_near.png (1 m per pixel, the path in red), topi_far.png (10 m per pixel)
#   tools/topi/run.sh profile   -> heights along the path
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Topi"
mkdir -p "$ROOT/Saved/Topi"
clang++ -std=c++17 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment -I"$SRC" -o "$ROOT/Saved/Topi/topitest" "$ROOT/tools/topi/topitest.cpp" "$SRC/TopiModel.cpp"
cd "$ROOT"
"$ROOT/Saved/Topi/topitest" "$@"
if [ "$1" = "map" ]; then
	python3 -c "
from PIL import Image
for n in ('near', 'far'):
    Image.open('Saved/Topi/topi_%s.ppm' % n).save('Saved/Topi/topi_%s.png' % n)
"
fi
