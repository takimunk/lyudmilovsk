#!/bin/sh
# Offscreen play test of the crow family (test_corvids.py, docs/corvids.md): never opens a window, leaves running editors
# alone, waits its turn for the Mac (tools/wait_ue_queue.sh). CORVIDTEST lines; shots in Saved/Screenshots/MacEditor.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_corvids.log"
mkdir -p "$(dirname "$LOG")"
"$ROOT/tools/wait_ue_queue.sh" 7200
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash ${NOSOUND--nosound} -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_corvids.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-1200}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "CORVIDTEST|CORVIDS|BirdLife:" "$LOG" | sed 's/.*\(CORVIDTEST\|CORVIDS\|BirdLife:\)/\1/' || true
