#!/bin/sh
# Builds and runs the kite's lift model outside the engine (tools/kitesim/kitetest.cpp). Run from anywhere.
#   tools/kitesim/run.sh            -> checks (calm, breeze, gale, storm, snag, the line's sag), exits 1 on failure
#   tools/kitesim/run.sh fly 7 [box] -> a trace of a kite flown 2 minutes in a 7 m/s wind
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/Kite"
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -I"$SRC/Kite" -I"$SRC/Weather" -o "$ROOT/Saved/Kite/kitetest" "$ROOT/tools/kitesim/kitetest.cpp" "$SRC/Kite/KiteModel.cpp" "$SRC/Weather/WindModel.cpp" "$SRC/Weather/WeatherModel.cpp" "$SRC/Weather/RainModel.cpp"
cd "$ROOT" && exec "$ROOT/Saved/Kite/kitetest" "$@"
