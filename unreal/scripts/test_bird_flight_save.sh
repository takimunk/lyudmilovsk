#!/bin/sh
# Headless (no rendering) check that a game saved as a bird loads back as that bird, with the hero where he stood:
# Bird.Script savetest flies a Rook, saves, loads; the log must say where it saved and where it loaded.
#   unreal/scripts/test_bird_flight_save.sh [map]
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
LOG="${TMPDIR:-/tmp}/lyud_bird_save.log"
while pgrep -f "UnrealEditor.*-game" >/dev/null; do sleep 5; done
"$UE" "$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject" "${1:-/Game/Scene01/Lvl_Scene01}" -game -nullrhi -dpcvars=lyud.MainMenuOnStart=0 \
    -abslog="$LOG" "-ExecCmds=Bird.Script savetest:Rook warm=15" >/dev/null 2>&1 || true
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
grep -E "BIRD (saving|loaded)" "$LOG" | sed 's/^.*BIRD/BIRD/'
python3 - "$LOG" <<'PY'
import re, sys
t = open(sys.argv[1], errors="replace").read()
s = re.search(r"BIRD saving as (\w+) at X=(\S+) Y=(\S+) Z=(\S+), hero at X=(\S+) Y=(\S+) Z=(\S+)", t)
l = re.search(r"BIRD loaded as (\w+) at X=(\S+) Y=(\S+) Z=(\S+), hero at X=(\S+) Y=(\S+) Z=(\S+)", t)
ok = bool(s and l and s.group(1) == l.group(1))
if ok:
    a = [float(v) for v in s.groups()[1:]]; b = [float(v) for v in l.groups()[1:]]
    ok = all(abs(x - y) < 5 for x, y in zip(a, b))
print("save/load as a bird: " + ("ok" if ok else "FAIL"))
sys.exit(0 if ok else 1)
PY
