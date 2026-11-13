#!/bin/bash
# The town's offline checks (docs/city_plan.md, docs/city/public_spaces.md). Run from anywhere:
#   bash tools/citysim/run.sh          the C++ (CityKit meshes, window schedules) and the plan/data checks
#   bash tools/citysim/run.sh -v       every problem listed, not the first few
# The C++ is the game's own (Private/City/CityKit.cpp, LyudCityWindows.cpp, LyudCityBuilding.cpp), cut out by
# extract.py and compiled against shim/CoreMinimal.h; nothing Unreal is needed. (-Wno-unused-*: CityKit.cpp has
# helpers and parameters the game compiles without -Wextra.)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/Saved/CitySim"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$OUT"
python3 "$ROOT/tools/citysim/extract.py" "$OUT"
CXX="${CXX:-clang++}"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-loop-increment -Wno-unused-parameter -Wno-unused-function -I "$ROOT/tools/citysim/shim" -I "$SRC/City" -I "$OUT" \
	-o "$OUT/citysim" "$ROOT/tools/citysim/citysim.cpp" "$OUT/CityKit_free.cpp"
FAIL=0
"$OUT/citysim" "$OUT" || FAIL=1
python3 "$ROOT/tools/citysim/check_plan.py" "$@" || FAIL=1
if [ "$FAIL" = 0 ]; then echo "citysim: all ok"; else echo "citysim: FAILED"; fi
exit $FAIL
