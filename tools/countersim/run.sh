#!/bin/sh
# Checks the counters' rules outside the engine (docs/counters.md). Run from anywhere.
#   tools/countersim/run.sh   -> paying and change, the hero's place in a line, and town_places.json as counters.py makes it
# The lines themselves (filling from the front, the hero's held spot, the market served) are checked by
# tools/peoplesim/run.sh town.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/Counter"
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -Wshadow -I"$SRC/People" -o "$ROOT/Saved/Counter/countertest" "$ROOT/tools/countersim/countertest.cpp"
"$ROOT/Saved/Counter/countertest"
python3 "$ROOT/tools/town/counters.py" --check
