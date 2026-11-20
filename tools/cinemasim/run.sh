#!/bin/sh
# Builds and runs the Rassvet's newsreel checks outside the engine (tools/cinemasim/cinematest.cpp on
# Private/City/CinemaModel.h, docs/cinema.md), then the data generator's own checks. Strict warnings as on the Mac build.
#   tools/cinemasim/run.sh
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/CinemaSim"
"${CXX:-clang++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment \
    -I "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private" \
    -o "$ROOT/Saved/CinemaSim/cinematest" "$ROOT/tools/cinemasim/cinematest.cpp"
cd "$ROOT"
RC=0
"$ROOT/Saved/CinemaSim/cinematest" || RC=1
python3 "$ROOT/tools/films/films.py" --check > /dev/null || { echo "FAIL tools/films/films.py output is stale: run it"; RC=1; }
exit $RC
