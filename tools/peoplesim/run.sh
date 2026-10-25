#!/bin/sh
# Builds and runs the people's life outside the engine (tools/peoplesim/peoplesim.cpp). Run from the repo root.
#   tools/peoplesim/run.sh test                  behaviour checks (a small made-up town, then the real map if built)
#   tools/peoplesim/run.sh day [start_hour hours] -> Saved/PeopleSim/day.csv + acts.txt (python3 tools/peoplesim/render_people.py)
#   tools/peoplesim/run.sh trace <key> [start_hour hours]
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/PeopleSim"
clang++ -std=c++17 -O2 -Wall -Wno-unused-function -o "$ROOT/Saved/PeopleSim/peoplesim" "$ROOT/tools/peoplesim/peoplesim.cpp" "$SRC/People/PeopleSim.cpp" "$SRC/Animals/AnimalSim.cpp"
cd "$ROOT" && exec "$ROOT/Saved/PeopleSim/peoplesim" "$@"
