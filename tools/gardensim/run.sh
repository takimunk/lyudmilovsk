#!/bin/sh
# Builds and runs the allotment's rules outside Unreal over the game's own weather (tools/gardensim/gardensim.cpp,
# docs/allotment.md). Run from anywhere.
#   tools/gardensim/run.sh                        -> the checks, exits 1 on a failure
#   tools/gardensim/run.sh season Y M D DAYS [crop] -> a well kept bed's diary from that date
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/Garden"
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wshadow -Werror -Wno-unused-function -I"$SRC/Garden" -I"$SRC/Weather" -I"$SRC/People" \
	-o "$ROOT/Saved/Garden/gardensim" "$ROOT/tools/gardensim/gardensim.cpp" "$SRC/Garden/GardenModel.cpp" \
	"$SRC/Weather/WeatherModel.cpp" "$SRC/Weather/WindModel.cpp" "$SRC/Weather/RainModel.cpp"
cd "$ROOT" && LYUD_ROOT="$ROOT" exec "$ROOT/Saved/Garden/gardensim" "$@"
