#!/bin/sh
# Builds and runs the offline checks of the flight home from Europa (tools/spacecraftsim/flighttest.cpp on
# Private/Spacecraft/FlightModel.h, docs/spacecraft.md, tools/spacecraft/MODEL_NOTES.md). Strict warnings as on the Mac build.
#   tools/spacecraftsim/run.sh              the checks (exit 1 on a failure)
#   tools/spacecraftsim/run.sh show [step]  the flight as a table, every [step] seconds (default 30) and at each cue
#   tools/spacecraftsim/run.sh steps        the largest change per 1/60 s of every continuous output (to set the bounds)
#   tools/spacecraftsim/run.sh dump         the model's built-in numbers as flight.json (to start tools/spacecraft/flight.json)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/SpacecraftSim"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wno-unused-function \
    -o "$ROOT/Saved/SpacecraftSim/flighttest" "$ROOT/tools/spacecraftsim/flighttest.cpp"
cd "$ROOT" && exec "$ROOT/Saved/SpacecraftSim/flighttest" "$@"
