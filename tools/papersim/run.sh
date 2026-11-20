#!/bin/sh
# Builds and runs the town paper outside the engine (tools/papersim/papertest.cpp on Private/City/TownPaperModel.h,
# docs/town_paper.md), then the notice boards' dated pages (check_notices.py). Run from anywhere; strict warnings as on the Mac.
#   tools/papersim/run.sh               the checks (exit 1 on a failure)
#   tools/papersim/run.sh show [N] [ru|en|ja]   the first N days' issues as text
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/PaperSim"
CXX="${CXX:-clang++}"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment -o "$ROOT/Saved/PaperSim/papertest" \
    "$ROOT/tools/papersim/papertest.cpp" "$SRC/Festival/FestivalCalendar.cpp"
cd "$ROOT"
if [ "${1:-test}" = "test" ]; then
	RC=0
	"$ROOT/Saved/PaperSim/papertest" test || RC=1
	python3 "$ROOT/tools/papersim/check_notices.py" || RC=1
	python3 "$ROOT/tools/paper/paper.py" --check > /dev/null || { echo "FAIL tools/paper/paper.py output is stale: run it"; RC=1; }
	exit $RC
fi
exec "$ROOT/Saved/PaperSim/papertest" "$@"
