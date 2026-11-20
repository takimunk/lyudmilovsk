#!/bin/sh
# Builds and runs the offline checks of the records and the town radio (tools/musicsim/musictest.cpp on
# Private/Music/MusicModel.h, docs/music.md) on the real Content/City/music.json. Strict warnings as on the Mac build.
#   tools/musicsim/run.sh              the checks (exit 1 on a failure)
#   tools/musicsim/run.sh show [day]   print a day's programme (ru keys resolved from Content/Localization/music)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/MusicSim"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wno-unused-function -include algorithm \
    -o "$ROOT/Saved/MusicSim/musictest" "$ROOT/tools/musicsim/musictest.cpp"
cd "$ROOT" && exec "$ROOT/Saved/MusicSim/musictest" "$@"
