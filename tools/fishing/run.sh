#!/bin/sh
# Fishing checks outside the engine (docs/fishing.md). Run from anywhere: tools/fishing/run.sh [-v]
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
OUT="$ROOT/Saved/Fishing"
mkdir -p "$OUT"
CXX="${CXX:-clang++}"
"$CXX" -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -I"$SRC/Fishing" -I"$SRC/Sea" -o "$OUT/fishtest" "$ROOT/tools/fishing/fishtest.cpp" "$SRC/Fishing/FishingModel.cpp" "$SRC/Sea/FishModel.cpp" "$SRC/Sea/SeaCurrent.cpp"
exec "$OUT/fishtest" "$@"
