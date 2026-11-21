#!/bin/sh
# Builds and runs the checks of the film camera outside the engine (docs/photo.md): the camera's model (the lens and
# sharpness, the film of 36 frames, the studio and the darkroom, the album, the stamps, the save line) and the string
# tables tools/photo/strings.py writes. Run from anywhere:
#   tools/photosim/run.sh [test]        everything; exits nonzero if anything fails
#   tools/photosim/run.sh model|strings
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Photo"
OUT="$ROOT/Saved/PhotoSim"
mkdir -p "$OUT"
CXX="${CXX:-clang++}"
FLAGS="-std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -Wshadow -Wunreachable-code-loop-increment"
cd "$ROOT"
WHAT="${1:-test}"
RC=0
want() { [ "$WHAT" = test ] || [ "$WHAT" = "$1" ]; }
if want model; then
	$CXX $FLAGS -o "$OUT/photo_test" tools/photosim/photo_test.cpp "$SRC/PhotoModel.cpp"
	"$OUT/photo_test" || RC=1
	$CXX $FLAGS -o "$OUT/contest_test" tools/photosim/contest_test.cpp "$SRC/PhotoContestModel.cpp" "$SRC/PhotoModel.cpp"
	"$OUT/contest_test" || RC=1
fi
if want strings; then
	# the words are written by tools/photo/strings.py; a tree without it has nothing to check here
	if [ -f tools/photo/strings.py ]; then
		python3 tools/photo/strings.py --check || RC=1
	fi
fi
exit $RC
