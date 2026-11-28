#!/bin/sh
# Builds and runs the checks of the motor ship's cruise outside the engine (tools/cruisesim/cruisesim.cpp on
# Private/Cruise/CruiseModel.h, docs/ship.md), then the data generator's own check. Strict warnings as on the Mac build.
#   tools/cruisesim/run.sh                        the checks on Content/Cruise/cruise.json
#   tools/cruisesim/run.sh table                  the route, the legs' speeds and where the ship is at every event
#   tools/cruisesim/run.sh at 2026-10-24 10:30    the ship at a date and time
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/CruiseSim"
"${CXX:-clang++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment \
    -I "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private" -o "$ROOT/Saved/CruiseSim/cruisesim" "$ROOT/tools/cruisesim/cruisesim.cpp"
cd "$ROOT"
if [ -n "$1" ] && [ "$1" != "test" ]; then
    exec "$ROOT/Saved/CruiseSim/cruisesim" "$@"
fi
RC=0
"$ROOT/Saved/CruiseSim/cruisesim" test || RC=1
python3 "$ROOT/tools/ship/cruise.py" --check > /dev/null || { echo "FAIL tools/ship/cruise.py output is stale: run it"; RC=1; }
exit $RC
