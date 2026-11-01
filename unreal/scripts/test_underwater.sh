#!/bin/sh
# Offscreen play test of the underwater world (test_underwater.py): never opens a window, leaves running editors alone.
# UWTEST_ONLY=a,b for some steps; the screenshots end up in Saved/UnderwaterTest/<step>.png.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_underwater.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/UnderwaterTest"
mkdir -p "$(dirname "$LOG")" "$SHOTS" "$OUT"
STAMP="$ROOT/unreal/Lyudmilovsk/Saved/.uwtest_stamp"
touch "$STAMP"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0,lyud.Birds=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_underwater.py" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: no run takes 20 minutes (it kills only its own process)
( sleep "${TIMEOUT:-1200}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "UWTEST|LyudUnderwater:|Failed to compile|Ensure condition" "$LOG" | sed 's/^.*\(UWTEST\)/\1/' || true
# name the new screenshots after the steps, in order
python3 - "$SHOTS" "$OUT" "$STAMP" "$LOG" <<'PY'
import os, re, shutil, sys
shots, out, stamp, log = sys.argv[1:5]
names = re.findall(r"UWTEST (\w+) eye ", open(log, errors="ignore").read())
new = sorted((f for f in os.listdir(shots) if f.endswith(".png") and os.path.getmtime(os.path.join(shots, f)) > os.path.getmtime(stamp)),
             key=lambda f: os.path.getmtime(os.path.join(shots, f)))
for n, f in zip(names, new):
    shutil.copy(os.path.join(shots, f), os.path.join(out, n + ".png"))
print("UWTEST %d screenshots -> %s" % (min(len(names), len(new)), out))
PY
