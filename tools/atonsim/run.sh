#!/bin/sh
# Builds and runs the checks of the ATON OS lab programs outside the engine (docs/aton_lab.md): the chess engine, the
# text adventure «Ночная вахта» with its data and words, the lab network's model (password, letters, telescope queue,
# save line), the string tables tools/aton/strings.py writes and the terminals' places (tools/aton/terminals.py). Run from anywhere:
#   tools/atonsim/run.sh [test]        everything; exits nonzero if anything fails
#   tools/atonsim/run.sh chess|adventure|labnet|strings|terminals
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/AtonOS/Lab"
DATA="$ROOT/unreal/Lyudmilovsk/Content/AtonOS"
OUT="$ROOT/Saved/AtonSim"
mkdir -p "$OUT"
CXX="${CXX:-clang++}"
FLAGS="-std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -Wshadow -Wunreachable-code-loop-increment"
cd "$ROOT"
WHAT="${1:-test}"
RC=0
want() { [ "$WHAT" = test ] || [ "$WHAT" = "$1" ]; }
if want chess; then
	$CXX $FLAGS -o "$OUT/chess_test" tools/atonsim/chess_test.cpp "$SRC/LabChess.cpp"
	"$OUT/chess_test" || RC=1
fi
if want adventure; then
	$CXX $FLAGS -o "$OUT/adventure_test" tools/atonsim/adventure_test.cpp "$SRC/LabAdventure.cpp"
	"$OUT/adventure_test" "$DATA/adventure.json" tools/aton/strings/adventure.json || RC=1
fi
if want labnet; then
	$CXX $FLAGS -o "$OUT/labnet_test" tools/atonsim/labnet_test.cpp "$SRC/LabNet.cpp"
	"$OUT/labnet_test" "$DATA/lab_mail.json" tools/aton/strings || RC=1
fi
if want strings; then
	python3 tools/aton/strings.py --check || RC=1
fi
if want terminals; then
	python3 tools/aton/terminals.py --check || RC=1
fi
exit $RC
