#!/bin/sh
# Builds and runs the yacht school's sail physics outside the engine (tools/sailsim/sailtest.cpp), in the game's own
# wind (Private/Weather/WindModel.cpp: the calendar's regime and the gust field) and sea (WaveModel.cpp). Run from anywhere.
#   tools/sailsim/run.sh [test]                 -> every check, exits 1 on failure
#   tools/sailsim/run.sh polar                  -> the polar table of each class at 3, 6 and 10 m/s
#   tools/sailsim/run.sh sail luch 6 [skill] [wl|tri] [fromdeg] [gustiness]
#                                               -> a trace of one boat round a course, a line every 5 s
#   tools/sailsim/run.sh session 2027 6 12 15   -> a 2-hour training session in that day's calendar wind
#   tools/sailsim/run.sh quick                  -> the same checks on fewer wind directions, seeds and days (half a minute)
#   tools/sailsim/run.sh hold luch 6 90 [assist] [sheet] [hike] [secs]   -> tuning: one boat held at a true wind angle
#   tools/sailsim/run.sh irons luch [wind]      -> tuning: head to wind and backing out
# SAIL_DEBUG=1 prints every contact between boats in the fleet and session checks.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/Sail"
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -I"$SRC/Sailing" -I"$SRC/Weather" -o "$ROOT/Saved/Sail/sailtest" "$ROOT/tools/sailsim/sailtest.cpp" "$SRC/Sailing/SailModel.cpp" "$SRC/Weather/WindModel.cpp" "$SRC/Weather/WaveModel.cpp" "$SRC/Weather/WeatherModel.cpp" "$SRC/Weather/RainModel.cpp"
cd "$ROOT" && exec "$ROOT/Saved/Sail/sailtest" "$@"
