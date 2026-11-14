#!/bin/sh
# Builds and runs the people's life outside the engine (tools/peoplesim/peoplesim.cpp). Run from the repo root.
#   tools/peoplesim/run.sh test                  behaviour checks (a small made-up town, then the real map if built)
#   tools/peoplesim/run.sh town                  the town (town_places.json, townsfolk.json: docs/town_life.md): a day and a night
#   tools/peoplesim/run.sh week                  the town over a simulated week: invariants (docs/town_life.md, Tests)
#   tools/peoplesim/run.sh day [start_hour hours] -> Saved/PeopleSim/day.csv + acts.txt (python3 tools/peoplesim/render_people.py)
#   tools/peoplesim/run.sh trace <key> [start_hour hours]
#   tools/peoplesim/run.sh summary [start_hour hours [weekday]]   a day of the Academgorodok's residents, one line each
# The sea railway's timetable (Content/Railway/railway_built.json, python3 tools/railway.py) drives the train rides.
# The walk grid (Content/AnimalsData/animal_world.bin) is stored in LFS: a checkout without LFS has only its pointer
# file, and then the grid is built once into Saved/PeopleSim/animals (python3 tools/animal_map.py, ~20 s).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/PeopleSim"
GRID="$ROOT/unreal/Lyudmilovsk/Content/AnimalsData/animal_world.bin"
if [ -z "$LYUD_ANIMALS_DIR" ] && [ "$(head -c 7 "$GRID" 2>/dev/null)" = "version" ]; then
	LYUD_ANIMALS_DIR="$ROOT/Saved/PeopleSim/animals"
	if [ ! -s "$LYUD_ANIMALS_DIR/animal_world.bin" ]; then
		echo "(animal_world.bin is only a pointer: building the walk grid into Saved/PeopleSim/animals)"
		python3 "$ROOT/tools/animal_map.py" --out "$LYUD_ANIMALS_DIR" > /dev/null
	fi
	export LYUD_ANIMALS_DIR
fi
clang++ -std=c++17 -O2 -Wall -Wno-unused-function -o "$ROOT/Saved/PeopleSim/peoplesim" "$ROOT/tools/peoplesim/peoplesim.cpp" "$SRC/People/PeopleSim.cpp" "$SRC/Animals/AnimalSim.cpp" "$SRC/Railway/RailRun.cpp"
cd "$ROOT"
if [ "$1" = "week" ]; then
	# the generated data first (check_town_data.py), then the week; both run, either failing fails
	set +e
	python3 "$ROOT/tools/peoplesim/check_town_data.py"
	DATA=$?
	"$ROOT/Saved/PeopleSim/peoplesim" "$@"
	SIM=$?
	[ $DATA -eq 0 ] && [ $SIM -eq 0 ]
	exit $?
fi
exec "$ROOT/Saved/PeopleSim/peoplesim" "$@"
