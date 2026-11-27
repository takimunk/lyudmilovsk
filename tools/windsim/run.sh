#!/bin/sh
# Builds and runs the local wind (Private/Weather/WindField.cpp: shifts, lee and lift over the map's relief, the gust
# ahead, Beaufort and outlooks) outside the engine (tools/windsim/windtest.cpp). Run from anywhere. docs/wind.md.
#   tools/windsim/run.sh                       -> checks, one line each, "N passed, M failed", exits 1 on failure
#   tools/windsim/run.sh map 270 8 [10]        -> Saved/Wind/exposure_*.pgm and updraft_*.ppm over the real map
#                                                 (wind from 270 at 8 m/s, 10 m over the ground)
#   tools/windsim/run.sh at 120 -300 2 [16.5]  -> the wind at map X 120, Y -300, 2 m up, at 16:30 of the scene's day
#   tools/windsim/run.sh line -300 90 8 [2]    -> a cut along X at map Y -300, wind from 90 at 8 m/s, 2 m up
#   tools/windsim/run.sh day 2026 10 20        -> the day hour by hour: regime, shift, Beaufort, the outlook
#   tools/windsim/run.sh spell 3 8 [y m d]     -> the daylight spells of 3..8 m/s over two weeks
# The real map's relief is Saved/Wind/terrain.bin, made by terrain.py from tools/world_map.py when missing or older
# than its sources; without python3 or the map the checks run on the built-in coast alone.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Weather"
OUT="$ROOT/Saved/Wind"
mkdir -p "$OUT"
BIN="$OUT/terrain.bin"
STALE=0
[ -f "$BIN" ] || STALE=1
for f in "$ROOT/tools/windsim/terrain.py" "$ROOT/tools/world_map.py" "$ROOT/tools/city_plan.py" "$ROOT/tools/north.py" \
	"$ROOT/tools/south.py" "$ROOT/tools/east.py" "$ROOT/unreal/Lyudmilovsk/Content/City/city_plan.json"; do
	[ "$STALE" = 0 ] && [ -f "$f" ] && [ "$f" -nt "$BIN" ] && STALE=1
done
if [ "$STALE" = 1 ]; then
	if ! { command -v python3 > /dev/null 2>&1 && python3 "$ROOT/tools/windsim/terrain.py" "$BIN.tmp" && mv -f "$BIN.tmp" "$BIN"; }; then
		echo "windsim: could not build $BIN (python3 or tools/world_map.py missing?): going on without the real map"
		rm -f "$BIN" "$BIN.tmp"
	fi
fi
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -I"$SRC" -o "$OUT/windtest" "$ROOT/tools/windsim/windtest.cpp" "$SRC/WindField.cpp" "$SRC/WindModel.cpp" "$SRC/WeatherModel.cpp" "$SRC/RainModel.cpp"
cd "$ROOT" && exec "$OUT/windtest" "$@"
