#!/bin/sh
# Builds and runs the townsfolk's characters and manners outside the engine (tools/mannersim/mannersim.cpp,
# docs/town_manners.md). From the repo root.
#   tools/mannersim/run.sh            checks: manners.json reads clean, the personas' spread, a day of the town in four
#                                     weathers (mild, a shower, frost, a warm day), what people notice, the cost
#   tools/mannersim/run.sh day        the mild day hour by hour: how many do what
#   tools/mannersim/run.sh who [key]  personas: everybody's (a summary) or one person's
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/MannerSim"
clang++ -std=c++17 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment -Wno-unused-function -o "$ROOT/Saved/MannerSim/mannersim" \
	"$ROOT/tools/mannersim/mannersim.cpp" "$SRC/People/PeopleManners.cpp" "$SRC/People/PeopleSim.cpp" "$SRC/Animals/AnimalSim.cpp" "$SRC/Railway/RailRun.cpp"
cd "$ROOT" && exec "$ROOT/Saved/MannerSim/mannersim" "$@"
