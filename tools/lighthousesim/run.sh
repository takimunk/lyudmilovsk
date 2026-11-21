#!/bin/sh
# Builds and runs the Mayak lighthouse's rules outside the engine (Private/South/LighthouseModel.*). Run from anywhere.
#   tools/lighthousesim/run.sh test    -> checks, exits 1 on failure
#   tools/lighthousesim/run.sh night   -> the lamp, the beam and the horn through a day of a few weathers
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/South"
mkdir -p "$ROOT/Saved/LighthouseSim"
c++ -std=c++17 -O2 -Wall -Wshadow -I"$SRC" -o "$ROOT/Saved/LighthouseSim/lhtest" "$ROOT/tools/lighthousesim/lhtest.cpp" "$SRC/LighthouseModel.cpp"
cd "$ROOT"
"$ROOT/Saved/LighthouseSim/lhtest" "${@:-test}"
