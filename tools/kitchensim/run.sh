#!/bin/sh
# Builds and runs the kitchen model outside the engine (Private/Kitchen/KitchenModel.cpp, docs/kitchen.md). Run from anywhere.
#   tools/kitchensim/run.sh test      the checks
#   tools/kitchensim/run.sh recipes   the recipes as a table
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
WERROR="-Werror"
if [ "$1" = "--no-werror" ]; then WERROR=""; shift; fi
mkdir -p "$ROOT/Saved/Kitchen"
clang++ -std=c++17 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment $WERROR $EXTRA_CXXFLAGS \
  -I"$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Kitchen" \
  -o "$ROOT/Saved/Kitchen/kitchensim" "$HERE/kitchensim.cpp" "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Kitchen/KitchenModel.cpp"
LYUD_ROOT="$ROOT" exec "$ROOT/Saved/Kitchen/kitchensim" "$@"
