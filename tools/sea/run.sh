#!/bin/sh
# Builds and runs the wind and wave models outside the engine (tools/sea/seatest.cpp). Run from anywhere.
#   tools/sea/run.sh test             -> checks, prints a summary, exits 1 on failure
#   tools/sea/run.sh days [y m d n]   -> Saved/Sea/wind_days.csv (hourly wind for n days)
#   tools/sea/run.sh field [speed]    -> Saved/Sea/sea_<speed>.pgm (height field 200 m square)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Weather"
mkdir -p "$ROOT/Saved/Sea"
clang++ -std=c++17 -O2 -Wall -I"$SRC" -o "$ROOT/Saved/Sea/seatest" "$ROOT/tools/sea/seatest.cpp" "$SRC/WindModel.cpp" "$SRC/WaveModel.cpp"
cd "$ROOT" && exec "$ROOT/Saved/Sea/seatest" "$@"
