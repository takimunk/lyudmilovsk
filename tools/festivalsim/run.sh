#!/bin/sh
# Builds and runs the festival calendar outside the engine (docs/festivals.md). Run from the repo root.
#   tools/festivalsim/run.sh test | year [YYYY] | at YYYY-MM-DD HH.H
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/FestivalSim"
CXX="${CXX:-clang++}"
"$CXX" -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -Wshadow -o "$ROOT/Saved/FestivalSim/festivalsim" "$ROOT/tools/festivalsim/festivalsim.cpp" "$SRC/Festival/FestivalCalendar.cpp"
cd "$ROOT" && exec "$ROOT/Saved/FestivalSim/festivalsim" "$@"
