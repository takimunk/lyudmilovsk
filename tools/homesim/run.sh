#!/bin/sh
# Builds and runs the checks of the hero's room outside the engine (tools/homesim/hometest.cpp on
# Private/Home/HomeModel.h and the real Content/Home/home.json, docs/hero_room.md). Run from anywhere; strict warnings
# as on the Mac build.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/HomeSim"
python3 "$ROOT/tools/home/home.py" --check
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment -Wno-unused-function $EXTRA_CXXFLAGS \
    -o "$ROOT/Saved/HomeSim/hometest" "$ROOT/tools/homesim/hometest.cpp"
cd "$ROOT" && exec "$ROOT/Saved/HomeSim/hometest" "$@"
