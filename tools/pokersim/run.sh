#!/bin/sh
# Builds and runs the offline checks of the ship's poker (tools/pokersim/pokertest.cpp on Private/Cruise/PokerModel.h and the
# strings of Content/Localization/poker; docs/ship.md "Poker"). Strict warnings as on the Mac build. Run from anywhere.
#   tools/pokersim/run.sh [test]                   every check
#   tools/pokersim/run.sh play [seed] [hands] [ru|en|ja]    a few hands printed (a scripted hero at the roubles table)
#   tools/pokersim/run.sh sharp [seed] [hands] [lang]       the same in the quest's marked-cards mode
#   tools/pokersim/run.sh stats                    the regulars' styles and what their tells are worth
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/PokerSim"
"${CXX:-clang++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -Wunreachable-code-loop-increment $EXTRA_CXXFLAGS \
    -I "$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private" -o "$ROOT/Saved/PokerSim/pokertest" "$ROOT/tools/pokersim/pokertest.cpp"
cd "$ROOT"
if [ "${1:-test}" != test ]; then
    exec "$ROOT/Saved/PokerSim/pokertest" "$@"
fi
RC=0
python3 "$ROOT/tools/pokersim/poker_strings.py" --check || RC=1
"$ROOT/Saved/PokerSim/pokertest" test || RC=1
exit $RC
