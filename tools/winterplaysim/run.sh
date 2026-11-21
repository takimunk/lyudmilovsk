#!/bin/sh
# Builds and runs the offline checks of the winter play's rules (tools/winterplaysim/wintertest.cpp on
# Private/Winter/WinterPlayModel.h, docs/winter_play.md) on the real Content/City/winter_play.json. Strict warnings as on the Mac.
#   tools/winterplaysim/run.sh     the checks (exit 1 on a failure)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/WinterPlaySim"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wno-unused-function \
    -o "$ROOT/Saved/WinterPlaySim/wintertest" "$ROOT/tools/winterplaysim/wintertest.cpp"
cd "$ROOT" && exec "$ROOT/Saved/WinterPlaySim/wintertest" "$@"
