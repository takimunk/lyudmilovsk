#!/bin/sh
# Builds and runs the townsfolk's lines on the town's people outside the engine (tools/barksim/barksim.cpp). From the repo root.
#   tools/barksim/run.sh              checks (the file, the words in ru/en/ja, every set fits somebody, most have a line)
#   tools/barksim/run.sh day [lang]   a day of the town: samples of what is said, hour by hour (ru, en, ja)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/BarkSim"
clang++ -std=c++17 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment -Wno-unused-function -o "$ROOT/Saved/BarkSim/barksim" \
	"$ROOT/tools/barksim/barksim.cpp" "$SRC/People/TownBarks.cpp" "$SRC/People/PeopleSim.cpp" "$SRC/Animals/AnimalSim.cpp" "$SRC/Railway/RailRun.cpp"
cd "$ROOT" && exec "$ROOT/Saved/BarkSim/barksim" "$@"
