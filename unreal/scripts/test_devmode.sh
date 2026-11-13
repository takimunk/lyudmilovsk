#!/bin/sh
# Offscreen play test of developer mode (test_devmode.py): Dev.* commands, teleports, a quest played through, every
# F7 tab. Never opens a window, leaves running editors alone. Fails on a jump that did not land, a quest that got stuck,
# a tab button not found (check_devmode_log.py).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_devmode.log"
mkdir -p "$(dirname "$LOG")"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1920 -ResY=1080 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_devmode.py ${WARMUP:-45}" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: no test takes 15 minutes
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "DEVTEST|DEVPANEL|DEV |DEVTP|QUESTTEST" "$LOG" | cat || true
python3 "$ROOT/unreal/scripts/check_devmode_log.py" "$LOG"
