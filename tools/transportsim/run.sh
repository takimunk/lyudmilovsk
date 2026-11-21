#!/bin/sh
# Builds and runs the cable-car metro outside the engine (tools/transportsim/transportsim.cpp). Run from anywhere.
#   tools/transportsim/run.sh test        behaviour checks on Content/Transport/network_built.json
#   tools/transportsim/run.sh town        the stations' day: station_life.json's trips, the rota, the wind stop
#   tools/transportsim/run.sh board       Aton as the club's front door: the departures board, the counter's line, the
#                                         usual, the lost and found, the travel card (docs/front_door.md)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private"
mkdir -p "$ROOT/Saved/TransportSim"
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wno-unused-function -o "$ROOT/Saved/TransportSim/transportsim" "$ROOT/tools/transportsim/transportsim.cpp" "$SRC/Transport/CablewaySim.cpp" "$SRC/Transport/StationLife.cpp" "$SRC/Transport/FrontDoor.cpp"
# network_built.json is made per checkout from its terrain (not committed)
[ -f "$ROOT/unreal/Lyudmilovsk/Content/Transport/network_built.json" ] || python3 "$ROOT/tools/transport.py" > /dev/null
cd "$ROOT" && exec "$ROOT/Saved/TransportSim/transportsim" "$@"
