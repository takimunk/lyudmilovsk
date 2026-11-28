#!/bin/sh
# Builds and runs the beach model outside the engine (Private/Sea/BeachModel.cpp, docs/beach.md). Run from anywhere.
#   tools/beachsim/run.sh test          the checks
#   tools/beachsim/run.sh throws        a table: skips by release quality, flatness and waves
#   tools/beachsim/run.sh shore [storm] what a kilometre of waterline holds on ten days (storm 0..1, default 0 and 1)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
WERROR="-Werror"
if [ "$1" = "--no-werror" ]; then WERROR=""; shift; fi
mkdir -p "$ROOT/Saved/Beach"
clang++ -std=c++17 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment $WERROR $EXTRA_CXXFLAGS \
  -I"$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sea" \
  -o "$ROOT/Saved/Beach/beachsim" "$HERE/beachsim.cpp" "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sea/BeachModel.cpp"
exec "$ROOT/Saved/Beach/beachsim" "$@"
