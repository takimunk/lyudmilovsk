#!/bin/sh
# Tonight's features together (tools/togethersim/togethersim.cpp): a game week with the festivals, the cups, the small jobs,
# the rounds, the allotment and the kites on at once, on the real people's life; then the cross-checks of their data
# (tools/togethersim/check_together.py: one person in two places, the hero's week, money, icons). Run from anywhere.
#   tools/togethersim/run.sh            both
#   tools/togethersim/run.sh brief      both, without the per-day table
#   tools/togethersim/run.sh data       only the data cross-checks (seconds)
# The walk grid (Content/AnimalsData/animal_world.bin) is in LFS: a checkout without it builds it once into
# Saved/PeopleSim/animals (python3 tools/animal_map.py, ~20 s), as tools/peoplesim/run.sh does.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
cd "$ROOT"
if [ "$1" = "data" ]; then
	exec python3 "$ROOT/tools/togethersim/check_together.py"
fi
mkdir -p "$ROOT/Saved/TogetherSim"
GRID="$ROOT/unreal/Lyudmilovsk/Content/AnimalsData/animal_world.bin"
if [ -z "$LYUD_ANIMALS_DIR" ] && [ "$(head -c 7 "$GRID" 2>/dev/null)" = "version" ]; then
	LYUD_ANIMALS_DIR="$ROOT/Saved/PeopleSim/animals"
	if [ ! -s "$LYUD_ANIMALS_DIR/animal_world.bin" ]; then
		echo "(animal_world.bin is only a pointer: building the walk grid into Saved/PeopleSim/animals)"
		python3 "$ROOT/tools/animal_map.py" --out "$LYUD_ANIMALS_DIR" > /dev/null
	fi
	export LYUD_ANIMALS_DIR
fi
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wno-unused-function -include algorithm -o "$ROOT/Saved/TogetherSim/togethersim" \
	"$ROOT/tools/togethersim/togethersim.cpp" "$SRC/People/PeopleSim.cpp" "$SRC/People/PeopleDays.cpp" "$SRC/Animals/AnimalSim.cpp" "$SRC/Animals/AnimalFriendSim.cpp" "$SRC/Animals/AnimalPetsSim.cpp" \
	"$SRC/Railway/RailRun.cpp" "$SRC/Weather/WindModel.cpp" "$SRC/Weather/WeatherModel.cpp" "$SRC/Weather/RainModel.cpp" \
	"$SRC/Festival/FestivalCalendar.cpp" -I"$SRC/Weather"
set +e
"$ROOT/Saved/TogetherSim/togethersim" "$@"
SIM=$?
python3 "$ROOT/tools/togethersim/check_together.py"
DATA=$?
[ $SIM -eq 0 ] && [ $DATA -eq 0 ]
