#!/bin/sh
# Offscreen check of the diary's birds section and calling a befriended bird (test_bird_diary.py): never opens a
# window, leaves running editors alone. Screenshots land in Saved/shots/<label>.png (repo root).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_bird_diary.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/Saved/shots"
mkdir -p "$(dirname "$LOG")" "$SHOTS" "$OUT"
MARK="$OUT/.mark"; touch "$MARK"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_bird_diary.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-1200}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "BIRDDIARY|BIRDBOOK|BirdBook:|Tame:|Bird flight" "$LOG" || true
# name the screenshots by the SHOT labels, in order
python3 - "$LOG" "$SHOTS" "$OUT" "$MARK" <<'PY'
import glob, os, re, shutil, sys
log, shots, out, mark = sys.argv[1:5]
labels = re.findall(r"BIRDDIARY SHOT (\S+)", open(log, errors="replace").read())
files = sorted((f for f in glob.glob(os.path.join(shots, "*.png")) if os.path.getmtime(f) > os.path.getmtime(mark)), key=os.path.getmtime)
for lab, f in zip(labels, files):
    shutil.copy(f, os.path.join(out, lab + ".png"))
    print("shot", os.path.join(out, lab + ".png"))
if len(labels) != len(files):
    print("shots: %d labels, %d files" % (len(labels), len(files)))
PY
