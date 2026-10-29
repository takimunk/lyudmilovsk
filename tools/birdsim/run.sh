#!/bin/sh
# Builds and runs the bird life outside the engine (tools/birdsim/birdsim.cpp). Run from the repo root.
#   tools/birdsim/run.sh day [start_hour hours]   -> Saved/BirdSim/day.csv, then python3 tools/birdsim/render_birds.py
#   tools/birdsim/run.sh test
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds"
mkdir -p "$ROOT/Saved/BirdSim"
clang++ -std=c++17 -O2 -Wall -Wno-unused-function -o "$ROOT/Saved/BirdSim/birdsim" "$ROOT/tools/birdsim/birdsim.cpp" "$SRC/BirdSim.cpp" "$SRC/BirdMurmur.cpp" "$SRC/BirdEcology.cpp"
cd "$ROOT" && exec "$ROOT/Saved/BirdSim/birdsim" "$@"
