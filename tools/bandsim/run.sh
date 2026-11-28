#!/bin/sh
# Offline checks of the ship's band (docs/ship.md "The band"; Private/Music/ShipBandModel.h, Content/Music/band_tunes.txt).
#   sh tools/bandsim/run.sh                 = test
#   sh tools/bandsim/run.sh test            every check (parse, forms, ranges, polyphony, levels, determinism, seek, speed)
#   sh tools/bandsim/run.sh wav <dir>       renders every tune (stereo 16-bit wav, 32 kHz) to listen to
#   sh tools/bandsim/run.sh stems <dir> <tune> [soft]   the four instruments dry + the notes played (csv), for analyse.py
#   sh tools/bandsim/run.sh set <set id>    prints the set's programme with its times
# Strict warnings as on the Mac build (the flags of tools/railsim/tickets.sh).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/BandSim"
"${CXX:-clang++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment \
    -I "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private" -o "$ROOT/Saved/BandSim/bandtest" "$ROOT/tools/bandsim/bandtest.cpp"
cd "$ROOT"
"$ROOT/Saved/BandSim/bandtest" "$@"
