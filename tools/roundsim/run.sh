# Offline checks of the bicycle rounds (docs/rounds.md): the data (tools/rounds/rounds.py --check) and a ride through every
# route on it with the rules of Private/Rounds/RoundsModel.h.
#   bash tools/roundsim/run.sh
set -e
cd "$(dirname "$0")"
python3 ../rounds/rounds.py --check > /dev/null
python3 dump.py > /tmp/lyud_rounds.txt
EXTRA=""
if c++ -Wunreachable-code-loop-increment -x c++ -fsyntax-only /dev/null 2>/dev/null; then EXTRA="-Wunreachable-code-loop-increment"; fi
c++ -std=c++17 -O2 -Wall -Wshadow $EXTRA -I../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private -o /tmp/lyud_roundtest roundtest.cpp
/tmp/lyud_roundtest /tmp/lyud_rounds.txt
