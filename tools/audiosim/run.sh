#!/bin/sh
# Builds and runs the sound and weather models outside the engine (tools/audiosim). Run from the repo root.
#   tools/audiosim/run.sh weather            the weather's climate per month and the days round the scene's start
#   tools/audiosim/run.sh render <voice> ... renders a procedural voice to Saved/AudioSim/<name>.wav (see audiosim.cpp)
#   tools/audiosim/run.sh test               checks the voices (levels, loops, no clicks or NaNs)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/AudioSim"
clang++ -std=c++17 -O2 -Wall -Wno-unused-function -o "$ROOT/Saved/AudioSim/audiosim" "$ROOT/tools/audiosim/audiosim.cpp" "$ROOT/tools/audiosim/weather.cpp" \
	"$SRC/Weather/RainModel.cpp" "$SRC/Weather/WindModel.cpp" "$SRC/Weather/WeatherModel.cpp" "$SRC/Audio/AmbienceDSP.cpp"
cd "$ROOT" && exec "$ROOT/Saved/AudioSim/audiosim" "$@"
