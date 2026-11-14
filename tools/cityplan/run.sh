#!/bin/bash
# The town's offline checks (docs/city_plan.md, docs/city/public_spaces.md). Run from anywhere:
#   bash tools/cityplan/run.sh          the C++ (CityKit meshes, window schedules) and the plan/data checks
#   bash tools/cityplan/run.sh -v       every problem listed, not the first few
# The C++ is the game's own (Private/City/CityKit.cpp, LyudCityWindows.cpp, LyudCityBuilding.cpp), cut out by
# extract.py and compiled against shim/CoreMinimal.h; nothing Unreal is needed. (-Wno-unused-*: CityKit.cpp has
# helpers and parameters the game compiles without -Wextra.)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/Saved/CityPlan"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$OUT"
python3 "$ROOT/tools/cityplan/extract.py" "$OUT"
CXX="${CXX:-clang++}"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-loop-increment -Wno-unused-parameter -Wno-unused-function -I "$ROOT/tools/cityplan/shim" -I "$SRC/City" -I "$OUT" \
	-o "$OUT/cityplan" "$ROOT/tools/cityplan/cityplan.cpp" "$OUT/CityKit_free.cpp"
FAIL=0
"$OUT/cityplan" "$OUT" || FAIL=1
python3 "$ROOT/tools/cityplan/check_plan.py" "$@" || FAIL=1
if [ "$FAIL" = 0 ]; then echo "cityplan: all ok"; else echo "cityplan: FAILED"; fi
exit $FAIL
