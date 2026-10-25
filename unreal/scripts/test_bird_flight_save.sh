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
def vec(v):
    d = dict((k, float(x)) for k, x in re.findall(r"([XYZ])=(-?[\d.]+)", v))
    return [d.get(k, 0.0) for k in "XYZ"]
def find(kind):
    m = re.search(r"BIRD %s as (\w+) at (V\([^)]*\)), hero at (V\([^)]*\))" % kind, t)
    return (m.group(1), vec(m.group(2)), vec(m.group(3))) if m else None
s, l = find("saving"), find("loaded")
ok = bool(s and l and s[0] == l[0]) and all(abs(a - b) < 5 for a, b in zip(s[1] + s[2], l[1] + l[2]))
print("save/load as a bird: " + ("ok" if ok else "FAIL"))
sys.exit(0 if ok else 1)
PY
