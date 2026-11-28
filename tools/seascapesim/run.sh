#!/bin/sh
# Builds and runs the checks of the cruise's seascape outside the engine (tools/seascapesim/seascapesim.cpp on
# Private/Cruise/SeascapeMath.h, docs/ship.md): the chart -> stage placing, "hull down", visibility, the far level,
# the lighthouse's beam, the wake's rates, the waves' drift phase, the ship's bells; then a whole voyage of
# Private/Cruise/CruiseModel.h (Content/Cruise/cruise.json) against the kit's pieces (Content/Cruise/seascape.json):
# the ship never inside a rock, the anchorage in Long Island's bay, every island seen. Strict warnings as on the Mac.
#   tools/seascapesim/run.sh            the checks
#   tools/seascapesim/run.sh table      + each island: nearest approach, when, which side, how long in sight
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/SeascapeSim"
"${CXX:-clang++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment \
    -I "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private" -o "$ROOT/Saved/SeascapeSim/seascapesim" "$ROOT/tools/seascapesim/seascapesim.cpp"
cd "$ROOT"
exec "$ROOT/Saved/SeascapeSim/seascapesim" "$@"
