#!/bin/sh
# Offscreen pose check (test_poses.py) in Lvl_Scene01: never opens a window, leaves running editors alone.
#   unreal/scripts/test_poses.sh            POSE_* env as in test_poses.py; SHOTS=<dir> copies the screenshots there
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_poses.log"
SS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
mkdir -p "$(dirname "$LOG")"
rm -f "$SS"/HighresScreenshot*.png
sh "$ROOT/tools/wait_ue_queue.sh"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1280 -ResY=720 -unattended -nosplash -nosound \
    -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_poses.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "POSETEST|LyudBody " "$LOG" | sed 's/^.*\(POSETEST\|LyudBody \)/\1/' || true
if [ -n "$SHOTS" ]; then
    mkdir -p "$SHOTS"
    # the screenshots in order, named after the "shot" lines
    ls -tr "$SS"/HighresScreenshot*.png 2>/dev/null > "$SHOTS/.files" || true
    grep -o "POSETEST shot [A-Za-z0-9_]*" "$LOG" | awk '{print $3}' > "$SHOTS/.names"
    paste -d' ' "$SHOTS/.files" "$SHOTS/.names" | while read F N; do [ -n "$N" ] && cp "$F" "$SHOTS/$N.png"; done
    rm -f "$SHOTS/.files" "$SHOTS/.names"
fi
