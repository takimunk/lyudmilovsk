#!/bin/sh
# Builds and runs the red dwarf's activity model outside the engine (SunActivityModel.cpp). Run from anywhere.
#   tools/sky/sun/run.sh        ten real minutes of the limb at noon, a game day of regions and flares, and the checks
#   tools/sky/sun/run.sh test   only the checks
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../../.."
mkdir -p "$ROOT/Saved/Sun"
clang++ -std=c++17 -O2 -Wall -o "$ROOT/Saved/Sun/sunactivity" "$HERE/sun.cpp" "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sky/SunActivityModel.cpp"
exec "$ROOT/Saved/Sun/sunactivity" "$@"
