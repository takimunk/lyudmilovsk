#!/bin/sh
# Builds and runs the kite's lift model outside the engine (tools/kitesim/kitetest.cpp). Run from anywhere.
#   tools/kitesim/run.sh            -> checks (the five kinds in calm, breeze, gale and storm; the stunt kite's window,
#                                      loops and twists; lee, lift and rough air; launches; tricks; a snag got
#                                      down; the hum), exits 1 on failure
#   tools/kitesim/run.sh fly 7 [box|delta|dragon|stunt] -> a trace of a kite flown 2 minutes in a 7 m/s wind
#   tools/kitesim/run.sh trace N    -> the checks, with their Nth stunt flight printed four times a second
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/Kite"
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -I"$SRC/Kite" -I"$SRC/Weather" -o "$ROOT/Saved/Kite/kitetest" "$ROOT/tools/kitesim/kitetest.cpp" "$SRC/Kite/KiteModel.cpp" "$SRC/Weather/WindModel.cpp" "$SRC/Weather/WeatherModel.cpp" "$SRC/Weather/RainModel.cpp"
cd "$ROOT" && exec "$ROOT/Saved/Kite/kitetest" "$@"
