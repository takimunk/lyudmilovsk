#!/bin/sh
# Builds and runs the telescopes' model outside the engine (Private/Sky/TelescopeModel.cpp). Run from anywhere.
#   tools/telescopesim/run.sh test                  the checks
#   tools/telescopesim/run.sh night 2026 10 20      what a night shows, hour by hour (local time)
#   tools/telescopesim/run.sh preview <dir>         pictures: the planets and the view in the eyepiece (PPM; preview.py makes PNG)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
WERROR="-Werror"
if [ "$1" = "--no-werror" ]; then WERROR=""; shift; fi
mkdir -p "$ROOT/Saved/Telescope"
clang++ -std=c++17 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment $WERROR $EXTRA_CXXFLAGS \
  -I"$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sky" \
  -o "$ROOT/Saved/Telescope/scopesim" "$HERE/scopesim.cpp" "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sky/TelescopeModel.cpp"
exec "$ROOT/Saved/Telescope/scopesim" "$@"
