#!/bin/sh
# Runs a level offscreen (-game, no window) with a Python script and waits for it to quit.
#   tools/play_offscreen.sh <level> <script.py> [label]
# Env: DPC (extra dpcvars, comma separated), LOG (default /tmp/offscreen.log), RES (default 1280x720), plus whatever the
# script reads. The script ends the run with the console command QUIT. Prints the lines of the log containing $GREP
# (default none). Never opens a window; run one at a time.
LEVEL="${1:?level, e.g. /Game/Dev/L_ForestPreview}"
SCRIPT="${2:?python script}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
LOG="${LOG:-/tmp/offscreen.log}"
RES="${RES:-1280x720}"
rm -f "$LOG"
"$UE/Engine/Binaries/Mac/UnrealEditor" "$PROJ" "$LEVEL" -game -RenderOffscreen -ResX="${RES%x*}" -ResY="${RES#*x}" -windowed -unattended -nosplash -nosound \
    -dpcvars=lyud.MainMenuOnStart=0${DPC:+,$DPC} -abslog="$LOG" "-ExecCmds=py $SCRIPT" > "$LOG.stdout" 2>&1 &
PID=$!
wait $PID
[ -n "$GREP" ] && grep -a -E "$GREP" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//'
grep -a "Critical error" "$LOG" | head -2
