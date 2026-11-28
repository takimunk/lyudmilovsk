#!/bin/sh
# The street's day by the weather and the season (Private/People/StreetLifeModel.h, docs/street_life.md) outside the engine.
#   tools/streetsim/run.sh       the checks and a year of the conditions; then tools/town/street_life.py --check
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/StreetSim"
c++ -std=c++17 -O2 -Wall -Wextra -o "$ROOT/Saved/StreetSim/streetsim" "$ROOT/tools/streetsim/streetsim.cpp"
"$ROOT/Saved/StreetSim/streetsim"
python3 "$ROOT/tools/town/street_life.py" --check && echo "street_life.py: up to date"
