#!/bin/sh
# Builds and runs the passenger checks of the sea railway (tools/railsim/tickettest.cpp on Private/Railway/RailTicketModel.h,
# docs/railway.md "Riding as a passenger"), then the data generator's own check. Strict warnings as on the Mac build.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/RailSim"
"${CXX:-clang++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment \
    -I "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private" -o "$ROOT/Saved/RailSim/tickettest" "$ROOT/tools/railsim/tickettest.cpp"
cd "$ROOT"
RC=0
"$ROOT/Saved/RailSim/tickettest" || RC=1
python3 "$ROOT/tools/rail_passenger/passenger.py" --check > /dev/null || { echo "FAIL tools/rail_passenger/passenger.py output is stale: run it"; RC=1; }
exit $RC
