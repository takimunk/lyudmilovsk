#!/bin/sh
# Builds and runs the rare sky's model outside the engine (RareSkyModel.cpp). Run from anywhere.
#   tools/sky/rare/run.sh        an aurora night at Kp 3..9, a year of flares, halo/rainbow shares
#   tools/sky/rare/run.sh test   only the checks
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../../.."
mkdir -p "$ROOT/Saved/RareSky"
clang++ -std=c++17 -O2 -Wall -o "$ROOT/Saved/RareSky/raresky" "$HERE/rare.cpp" "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sky/RareSkyModel.cpp"
exec "$ROOT/Saved/RareSky/raresky" "$@"
