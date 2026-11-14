#!/bin/sh
# Builds and runs the checks of the town's interiors outside the engine (tools/citysim/citytest.cpp on
# Private/City/CityThingsModel.h, docs/city_interiors.md). Run from anywhere; strict warnings as on the Mac build.
#   tools/citysim/run.sh            the fixture, then Content/City/interiors.json if it has been built
#   tools/citysim/run.sh <file>     the fixture, then that interiors file
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/CitySim"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment -Wno-unused-function -include algorithm \
    -o "$ROOT/Saved/CitySim/citytest" "$ROOT/tools/citysim/citytest.cpp"
cd "$ROOT" && exec "$ROOT/Saved/CitySim/citytest" "$@"
