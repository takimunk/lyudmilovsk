#!/bin/sh
# Builds and runs the sea railway's timetable outside the engine (tools/railsim/railsim.cpp). Run from anywhere.
#   tools/railsim/run.sh test     checks on Content/Railway/railway_built.json
#   tools/railsim/run.sh table    where the train is every 30 s of a period
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/RailSim"
clang++ -std=c++17 -O2 -Wall -o "$ROOT/Saved/RailSim/railsim" "$ROOT/tools/railsim/railsim.cpp" "$SRC/Railway/RailRun.cpp"
[ -f "$ROOT/unreal/Lyudmilovsk/Content/Railway/railway_built.json" ] || python3 "$ROOT/tools/railway.py" > /dev/null
cd "$ROOT" && exec "$ROOT/Saved/RailSim/railsim" "$@"
