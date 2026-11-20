#!/bin/sh
# The town's swappable dressing outside the engine (docs/city_dress.md): the game's C++ condition against the Blender
# side's Python on 20 000 random cases, then every exported set over a game year (never-shown sets are errors).
#   tools/citydress/run.sh [types...]
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/CityDress"
python3 "$ROOT/tools/citydress/gen_cases.py" "$ROOT/Saved/CityDress/cases.txt" 20000
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment -Wno-unused-function -include algorithm \
    -o "$ROOT/Saved/CityDress/dresstest" "$ROOT/tools/citydress/dresstest.cpp"
"$ROOT/Saved/CityDress/dresstest" "$ROOT/Saved/CityDress/cases.txt"
python3 "$ROOT/tools/citydress/year_scan.py" "$@"
