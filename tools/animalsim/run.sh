#!/bin/sh
# Builds and runs the animal life outside the engine (tools/animalsim/animalsim.cpp). Run from the repo root.
#   tools/animalsim/run.sh day [start_hour hours]   -> Saved/AnimalSim/day.csv, then python3 tools/animalsim/render_animals.py
#   tools/animalsim/run.sh test
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Animals"
mkdir -p "$ROOT/Saved/AnimalSim"
clang++ -std=c++17 -O2 -Wall -Wno-unused-function -o "$ROOT/Saved/AnimalSim/animalsim" "$ROOT/tools/animalsim/animalsim.cpp" "$SRC/AnimalSim.cpp"
cd "$ROOT" && exec "$ROOT/Saved/AnimalSim/animalsim" "$@"
