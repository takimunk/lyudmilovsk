#!/bin/sh
# Builds and runs the snow and sea-ice model outside the engine (tools/winter/wintertest.cpp). Run from anywhere.
#   tools/winter/run.sh test     -> checks on a made-up valley with a house, a path and a coast; exits 1 on failure
#   tools/winter/run.sh perf     -> timings on a grid the size of the game's map
#   tools/winter/run.sh images   -> Saved/Winter/*.pgm (depth, factor, ice) of the test world
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Winter"
mkdir -p "$ROOT/Saved/Winter"
clang++ -std=c++17 -O2 -Wall -pthread -I"$SRC" -o "$ROOT/Saved/Winter/wintertest" "$ROOT/tools/winter/wintertest.cpp" "$SRC/SnowModel.cpp"
cd "$ROOT" && exec "$ROOT/Saved/Winter/wintertest" "$@"
