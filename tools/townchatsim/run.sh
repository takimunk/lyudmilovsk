#!/bin/sh
# Builds and runs a talk with a townsperson (Private/People/TownChat.cpp, docs/town_talk.md "A talk") on the town's people
# outside the engine (tools/townchatsim/townchatsim.cpp). From the repo root.
#   tools/townchatsim/run.sh               checks: the data and the words, a day of the town (who can be talked to, every
#                                          talk's topics and lines), the variety, the names, the bond, the walk's hold, the cost
#   tools/townchatsim/run.sh talks [lang] [n]   n talks of the day written out as the page would show them (ru, en, ja)
#   tools/townchatsim/run.sh who <key> [lang]   one resident's talk at noon
#   tools/townchatsim/run.sh where [key,key,...] [days]   when and where residents are free for a word, day by day (quest givers)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/TownChatSim"
GRID="$ROOT/unreal/Lyudmilovsk/Content/AnimalsData/animal_world.bin"
# the walk grid is stored in LFS: with only its pointer file here, it is built once into Saved/PeopleSim/animals (as tools/peoplesim/run.sh does)
if [ -z "$LYUD_ANIMALS_DIR" ] && [ "$(head -c 7 "$GRID" 2>/dev/null)" = "version" ]; then
	LYUD_ANIMALS_DIR="$ROOT/Saved/PeopleSim/animals"
	if [ ! -s "$LYUD_ANIMALS_DIR/animal_world.bin" ]; then
		echo "(animal_world.bin is only a pointer: building the walk grid into Saved/PeopleSim/animals)"
		python3 "$ROOT/tools/animal_map.py" --out "$LYUD_ANIMALS_DIR" > /dev/null
	fi
	export LYUD_ANIMALS_DIR
fi
CXX="${CXX:-clang++}"
command -v "$CXX" >/dev/null 2>&1 || CXX=g++
"$CXX" -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wshadow -Wno-unused-function -o "$ROOT/Saved/TownChatSim/townchatsim" \
	"$ROOT/tools/townchatsim/townchatsim.cpp" "$SRC/People/TownChat.cpp" "$SRC/People/TownBarks.cpp" "$SRC/People/Friends.cpp" \
	"$SRC/People/PeopleManners.cpp" "$SRC/People/PeopleSim.cpp" "$SRC/People/PeopleDays.cpp" "$SRC/Animals/AnimalSim.cpp" "$SRC/Animals/AnimalFriendSim.cpp" \
	"$SRC/Animals/AnimalPetsSim.cpp" "$SRC/Railway/RailRun.cpp"
cd "$ROOT" && exec "$ROOT/Saved/TownChatSim/townchatsim" "$@"
