#!/bin/sh
# Headless checks of the player bird's flight model (no engine): glide ratio, cruise, take-off, dive,
# stall, turns, one-wing turns, ridge lift and the flare, for one species of each flight style.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds"
OUT="${TMPDIR:-/tmp}/lyud_bird_flight_test"
clang++ -std=c++17 -O2 -Wall -I"$SRC" "$SRC/BirdFlightModel.cpp" "$ROOT/unreal/scripts/bird_flight/test_model.cpp" -o "$OUT"
"$OUT"
