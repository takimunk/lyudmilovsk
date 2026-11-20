# Offline checks of the instruments (docs/instruments.md): the synth's notes in tune, the tunes, the rhythm game.
#   bash tools/instrumentsim/run.sh            all checks
#   bash tools/instrumentsim/run.sh wav <dir>  also writes every tune played perfectly (piano and guitar) to listen to
set -e
cd "$(dirname "$0")"
EXTRA=""
if c++ -Wunreachable-code-loop-increment -x c++ -fsyntax-only /dev/null 2>/dev/null; then EXTRA="-Wunreachable-code-loop-increment"; fi
c++ -std=c++17 -O2 -Wall -Wshadow $EXTRA -I../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private -include cstring -o /tmp/lyud_instrtest instrtest.cpp
/tmp/lyud_instrtest "$@"
