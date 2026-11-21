#!/bin/sh
# Builds and runs the bird life outside the engine (tools/birdsim/birdsim.cpp). Run from the repo root.
#   tools/birdsim/run.sh day [start_hour hours]   -> Saved/BirdSim/day.csv, then python3 tools/birdsim/render_birds.py
#   tools/birdsim/run.sh test
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds"
mkdir -p "$ROOT/Saved/BirdSim"
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wno-unused-function -o "${BIRDSIM_BIN:=$ROOT/Saved/BirdSim/birdsim}" "$ROOT/tools/birdsim/birdsim.cpp" "$ROOT/tools/birdsim/corvids_test.cpp" "$SRC/BirdSim.cpp" "$SRC/BirdMurmur.cpp" "$SRC/BirdEcology.cpp" "$SRC/BirdBio.cpp" "$SRC/BirdCorvid.cpp" "$SRC/BirdNest.cpp" "$SRC/BirdVoice.cpp" "$SRC/BirdHabit.cpp" "$ROOT/tools/birdsim/nests_test.cpp" "$ROOT/tools/birdsim/voices_test.cpp" "$ROOT/tools/birdsim/habits_test.cpp" "$ROOT/tools/birdsim/weather_test.cpp"
cd "$ROOT" && exec "$BIRDSIM_BIN" "$@"
