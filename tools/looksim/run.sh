#!/bin/sh
# Builds and runs the look model outside the engine (Private/Look/LookModel.cpp, docs/look.md). Run from anywhere.
#   tools/looksim/run.sh test      the checks
#   tools/looksim/run.sh weather   what suits which weather, as a table
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
WERROR="-Werror"
if [ "$1" = "--no-werror" ]; then WERROR=""; shift; fi
mkdir -p "$ROOT/Saved/Look"
clang++ -std=c++17 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment $WERROR $EXTRA_CXXFLAGS \
  -I"$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Look" \
  -o "$ROOT/Saved/Look/looksim" "$HERE/looksim.cpp" "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Look/LookModel.cpp"
LYUD_ROOT="$ROOT" exec "$ROOT/Saved/Look/looksim" "$@"
