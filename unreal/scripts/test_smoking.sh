#!/bin/sh
# Offscreen check of the cigarettes in Lvl_Scene01 (test_smoking.py): prints the SMOKETEST lines and the game's SMOKE / HANDUSE
# lines, writes the screenshots to Saved/SmokeTest/<n>_<name>.png (FRAMES=1: the drag and cough frames to Saved/SmokeTest/frames).
# Needs Content/Scene01 and Characters copied from the main checkout (cp -c). Never opens a window or takes the mouse; waits for a
# free slot of the UE queue; the watchdog kills only this run.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_smoking.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/SmokeTest"
mkdir -p "$(dirname "$LOG")" "$OUT/frames"
QUIET=$("$ROOT/tools/ue_quiet_args.sh" 2>/dev/null) || QUIET="-unattended -nosplash -nosound -NoVerifyGC \
-ini:Input:[/Script/Engine.InputSettings]:bCaptureMouseOnLaunch=False \
-ini:Input:[/Script/Engine.InputSettings]:DefaultViewportMouseCaptureMode=NoCapture \
-ini:Input:[/Script/Engine.InputSettings]:DefaultViewportMouseLockMode=DoNotLock"
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
rm -f "$OUT"/*.png "$OUT"/frames/*.png
# a fixed step: the script's timings are game time however slowly the machine renders
FIX="-UseFixedTimeStep -FPS=${FPS:-24}"
SMOKETEST_FRAMES="${FRAMES:-0}" "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game \
    -RenderOffscreen -windowed -ResX=1600 -ResY=900 $QUIET $FIX -dpcvars=lyud.MainMenuOnStart=0$DPC $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_smoking.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-1500}"; ps -p "$PID" -o args= | grep -q test_smoking.py && kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "SMOKETEST|SMOKE |HANDUSE|LogScript.*Error|Critical error" "$LOG" | grep -v "SMOKETEST frame" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
ls "$OUT" | head -60; ls "$OUT/frames" | wc -l
