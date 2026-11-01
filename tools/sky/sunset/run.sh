#!/bin/sh
# Builds and runs the sunset model outside the engine (SunsetModel.cpp). Run from anywhere.
#   tools/sky/sunset/run.sh        a month of evenings and mornings, the weather's variants, and the checks
#   tools/sky/sunset/run.sh test   only the checks
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../../.."
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/Sunset"
clang++ -std=c++17 -O2 -Wall -I "$SRC" -o "$ROOT/Saved/Sunset/sunset" "$HERE/sunset.cpp" "$SRC/Sky/SunsetModel.cpp"
exec "$ROOT/Saved/Sunset/sunset" "$@"
