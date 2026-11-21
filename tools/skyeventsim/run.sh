#!/bin/sh
# Builds and runs the sky events' model outside the engine (Private/Sky/SkyEventsModel.cpp, docs/sky_events.md).
#   tools/skyeventsim/run.sh test                     the checks
#   tools/skyeventsim/run.sh year [YYYY MM DD [days]]  what the sky does from a date (default: the game's first year)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
WERROR="-Werror"
if [ "$1" = "--no-werror" ]; then WERROR=""; shift; fi
mkdir -p "$ROOT/Saved/SkyEvents"
SKY="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Sky"
clang++ -std=c++17 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment $WERROR $EXTRA_CXXFLAGS -I"$SKY" \
  -o "$ROOT/Saved/SkyEvents/skyeventsim" "$HERE/skyeventsim.cpp" "$SKY/SkyEventsModel.cpp" "$SKY/TelescopeModel.cpp"
exec "$ROOT/Saved/SkyEvents/skyeventsim" "$@"
