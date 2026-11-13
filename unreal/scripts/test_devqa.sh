#!/bin/sh
# Offscreen QA of developer mode (test_devqa.py): every quest played through by its real events, every teleport point
# visited with a check of the ground under the feet. Fails on a STUCK quest or a point that did not land
# (check_devqa_log.py; CHECK_ARGS=--allow-teleport-problems only lists the points). WAIT=s per point, GROUP=one group.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_devqa.log"
mkdir -p "$(dirname "$LOG")"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1920 -ResY=1080 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_devqa.py ${WARMUP:-45} ${WAIT:-2.5} ${GROUP:-all}" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: the walk is about WAIT s a point; an hour is plenty
( sleep "${TIMEOUT:-3600}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "DEVTEST|DEVQA" "$LOG" | cat || true
python3 "$ROOT/unreal/scripts/check_devqa_log.py" "$LOG" $CHECK_ARGS
