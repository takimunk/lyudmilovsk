#!/bin/sh
# Builds and runs the yacht school's fleet outside the engine (tools/sailfleetsim/fleettest.cpp): the integration test of
# the sail physics and helmsmen (Private/Sailing/SailModel.*) with the school's rules (SailSchoolModel.h) through the
# fleet's brain (SailFleetModel.h) on the real Content/City/sailing.json, in the game's own gust field. Run from anywhere.
#   tools/sailfleetsim/run.sh [test]          -> every check, exits 1 on failure
#   tools/sailfleetsim/run.sh quick           -> fewer wind directions
#   tools/sailfleetsim/run.sh session kids 4 250 [drillA drillB]   -> a traced session: group, wind m/s, from degrees
#   tools/sailfleetsim/run.sh race 6 250 [wl|tri]                  -> a traced race of the 12 cadets and luches
# FLEET_DEBUG=1 prints every contact and race event.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
OUT="$ROOT/Saved/SailFleetSim"
mkdir -p "$OUT"
FLAGS="-std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -Wshadow -Wunreachable-code-loop-increment -Werror"
clang++ $FLAGS -I"$SRC/Sailing" -I"$SRC/Weather" -o "$OUT/fleettest" "$ROOT/tools/sailfleetsim/fleettest.cpp" \
    "$SRC/Sailing/SailModel.cpp" "$SRC/Weather/WindModel.cpp" "$SRC/Weather/WeatherModel.cpp" "$SRC/Weather/RainModel.cpp"
cd "$ROOT" && exec "$OUT/fleettest" "$@"
