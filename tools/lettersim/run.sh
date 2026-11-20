#!/bin/sh
# Builds and runs the town's post outside the engine (tools/lettersim/lettersim.cpp on Private/Letters/LettersModel.cpp).
# From anywhere:
#   tools/lettersim/run.sh                 the checks (delivery by the calendar, replies, thanks, invitations, the pen
#                                          friend's chain, the nudge, save and load, a month's jump, the words in ru/en/ja)
#   tools/lettersim/run.sh story [lang]    the whole correspondence of a simulated autumn in one language (ru, en, ja)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/LetterSim"
clang++ -std=c++17 -O1 $EXTRA_CXXFLAGS -Wall -Wshadow -Wunreachable-code-loop-increment -Werror -Wno-unused-function -I"$SRC" \
	-o "$ROOT/Saved/LetterSim/lettersim" "$ROOT/tools/lettersim/lettersim.cpp" "$SRC/Letters/LettersModel.cpp"
cd "$ROOT" && exec "$ROOT/Saved/LetterSim/lettersim" "$@"
