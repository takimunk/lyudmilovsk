#!/bin/sh
# Builds and runs the weather engine outside Unreal (tools/weather/weathertest.cpp). Run from anywhere.
#   tools/weather/run.sh test                 -> climate checks, exits 1 on failure
#   tools/weather/run.sh climate [seed]       -> per-month table over 10 years
#   tools/weather/run.sh days y m d n [seed]  -> hourly weather for n days (also Saved/Weather/days.csv)
#   tools/weather/run.sh winters [seed]       -> each winter's snow and sea ice
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Weather"
mkdir -p "$ROOT/Saved/Weather"
clang++ -std=c++17 -O2 -Wall -Wno-unused-function -I"$SRC" -o "$ROOT/Saved/Weather/weathertest" "$ROOT/tools/weather/weathertest.cpp" \
	"$SRC/WeatherModel.cpp" "$SRC/WindModel.cpp" "$SRC/RainModel.cpp"
cd "$ROOT" && exec "$ROOT/Saved/Weather/weathertest" "$@"
