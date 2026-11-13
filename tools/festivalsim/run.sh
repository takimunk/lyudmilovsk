#!/bin/sh
# Builds and runs the festival calendar outside the engine (docs/festivals.md). Run from the repo root.
#   tools/festivalsim/run.sh test | year [YYYY] | at YYYY-MM-DD HH.H
# test: the calendar's checks (festivalsim.cpp, 2026..2060), then the festivals' links to the other data
# (check_festival_data.py: venues, quests, words, dishes, sounds); exits nonzero if either fails.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/FestivalSim"
CXX="${CXX:-clang++}"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wshadow -Wunreachable-code-loop-increment -o "$ROOT/Saved/FestivalSim/festivalsim" "$ROOT/tools/festivalsim/festivalsim.cpp" "$SRC/Festival/FestivalCalendar.cpp"
cd "$ROOT"
if [ "${1:-test}" = "test" ]; then
	RC=0
	"$ROOT/Saved/FestivalSim/festivalsim" test || RC=1
	python3 "$ROOT/tools/festivalsim/check_festival_data.py" || RC=1
	exit $RC
fi
exec "$ROOT/Saved/FestivalSim/festivalsim" "$@"
