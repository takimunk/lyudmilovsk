#!/bin/sh
# Offscreen door checks: builds /Game/Dev/L_DoorTest headless if it is missing (REBUILD=1 forces it), then plays it
# without a window (test_doors.py) and prints the DOORTEST lines. Waits for other offscreen UE runs first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_doors.log"
mkdir -p "$(dirname "$LOG")"
sh "$ROOT/tools/wait_ue_queue.sh"
if [ "$REBUILD" = 1 ] || [ ! -f "$ROOT/unreal/Lyudmilovsk/Content/Dev/L_DoorTest.umap" ]; then
    "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript -script="$ROOT/unreal/scripts/make_door_test_level.py" \
        -unattended -nosplash -abslog="$LOG.level" > /dev/null 2>&1 || true
    grep -E "DOORLEVEL|DOOR test|Error" "$LOG.level" | sed 's/.*Warning: //' || true
fi
rm -f "$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor/door_"*.png
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Dev/L_DoorTest -game -RenderOffscreen \
    -windowed -ResX=1280 -ResY=720 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_doors.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-600}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "DOORTEST|DOOR |Door .* (locked|below)" "$LOG" | grep -v "DOORTEST run" | sed 's/.*Warning: //; s/.*LogTemp: //' || true
ls "$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor/"door_*.png 2>/dev/null || true
