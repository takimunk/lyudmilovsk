#!/bin/sh
# Builds and runs the offline checks of the yacht school's sound mixing (tools/sailsoundsim/soundtest.cpp on
# Private/Sailing/SailSound.h, docs/yacht_school.md "Sounds"), strict warnings as on the Mac. Also checks that every pool
# the header names has its sidecars in audio/sfx/sailing (tools/sailing/sounds.py writes them).
#   tools/sailsoundsim/run.sh     the checks (exit 1 on a failure)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/SailSoundSim"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment \
    -I"$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sailing" \
    -o "$ROOT/Saved/SailSoundSim/soundtest" "$ROOT/tools/sailsoundsim/soundtest.cpp"
cd "$ROOT" && exec "$ROOT/Saved/SailSoundSim/soundtest" "$@"
