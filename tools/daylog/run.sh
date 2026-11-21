#!/bin/sh
# Builds and runs the diary that writes itself outside the engine (tools/daylog/daysim.cpp, docs/diary.md "Days").
# From anywhere.
#   tools/daylog/run.sh                  checks over 60 made-up days (exit 1 on a failure)
#   tools/daylog/run.sh show [lang] [n]  the first n days written out
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/DaySim"
CXX="${CXX:-$(command -v clang++ || command -v g++)}"
"$CXX" -std=c++17 -O2 -Wall -Wshadow -Wno-unused-function -o "$ROOT/Saved/DaySim/daysim" "$ROOT/tools/daylog/daysim.cpp" "$SRC/UI/DayLog.cpp"
cd "$ROOT" && exec "$ROOT/Saved/DaySim/daysim" "$@"
