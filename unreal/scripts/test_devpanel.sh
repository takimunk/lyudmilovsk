#!/bin/sh
# Offscreen play test of the F7 panel's weather section, languages, H keys list and F10 menu (test_devpanel.py):
# never opens a window, leaves running editors alone. Prints the DEVTEST/DEVPANEL lines and fails on a missed click,
# a button not found, or a preset or check box that did not change the weather.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_devpanel.log"
mkdir -p "$(dirname "$LOG")"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1920 -ResY=1080 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_devpanel.py ${WARMUP:-45}" > "$LOG.stdout" 2>&1 &
PID=$!
# a watchdog: no test takes 15 minutes
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "DEVTEST|DEVPANEL" "$LOG" | sed -E "s/.*(DEVTEST|DEVPANEL)/\1/" || true
python3 "$ROOT/unreal/scripts/check_devpanel_log.py" "$LOG"
