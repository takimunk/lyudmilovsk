#!/bin/sh
# Children's games in the yards outside the engine (tools/yardsim/yardsim.cpp, docs/yard_games.md). Run from anywhere.
#   tools/yardsim/run.sh          the generated data up to date (tools/yard/yard_areas.py --check), the games' rules, the hero,
#                                 then a Monday and a Saturday afternoon of the real town's children (~1 min)
#   tools/yardsim/run.sh rules    only the rules and the hero (seconds)
#   tools/yardsim/run.sh town     only the town's afternoons
# The walk grid (Content/AnimalsData/animal_world.bin) is in LFS: a checkout without it builds it once into
# Saved/PeopleSim/animals (python3 tools/animal_map.py, ~20 s), as tools/peoplesim/run.sh does.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
cd "$ROOT"
mkdir -p "$ROOT/Saved/YardSim"
GRID="$ROOT/unreal/Lyudmilovsk/Content/AnimalsData/animal_world.bin"
if [ "$1" != "rules" ] && [ -z "$LYUD_ANIMALS_DIR" ] && [ "$(head -c 7 "$GRID" 2>/dev/null)" = "version" ]; then
	LYUD_ANIMALS_DIR="$ROOT/Saved/PeopleSim/animals"
	if [ ! -s "$LYUD_ANIMALS_DIR/animal_world.bin" ]; then
		echo "(animal_world.bin is only a pointer: building the walk grid into Saved/PeopleSim/animals)"
		python3 "$ROOT/tools/animal_map.py" --out "$LYUD_ANIMALS_DIR" > /dev/null
	fi
	export LYUD_ANIMALS_DIR
fi
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wshadow -Wno-unused-function -include algorithm -o "$ROOT/Saved/YardSim/yardsim" \
	"$ROOT/tools/yardsim/yardsim.cpp" "$SRC/People/YardGames.cpp" "$SRC/People/PeopleSim.cpp" "$SRC/Animals/AnimalSim.cpp" \
	"$SRC/Animals/AnimalFriendSim.cpp" "$SRC/Railway/RailRun.cpp" "$SRC/Weather/WindModel.cpp" "$SRC/Weather/WeatherModel.cpp" \
	"$SRC/Weather/RainModel.cpp" -I"$SRC/Weather"
set +e
DATA=0
if [ -z "$1" ]; then
	python3 "$ROOT/tools/yard/yard_areas.py" --check > /dev/null
	DATA=$?
	[ $DATA -eq 0 ] && echo "  ok   tools/yard/yard_areas.py --check: the generated files are up to date" || python3 "$ROOT/tools/yard/yard_areas.py" --check
fi
"$ROOT/Saved/YardSim/yardsim" "$@"
SIM=$?
[ $DATA -eq 0 ] && [ $SIM -eq 0 ]
