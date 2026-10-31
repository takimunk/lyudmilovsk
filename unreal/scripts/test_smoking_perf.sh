#!/bin/sh
# Offscreen check of the cigarettes in Lvl_Scene01 (test_smoking_perf.py): prints the SMOKETEST lines and the game's SMOKE / HANDUSE
# lines, copies the screenshots to Saved/SmokeTest/<n>_<name>.png (FRAMES=1: the drag and cough frames to Saved/SmokeTest/frames).
# Needs Content/Scene01 and Characters copied from the main checkout (cp -c). Never opens a window or takes the mouse; waits for a
# free slot of the UE queue; the watchdog kills only this run.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_smoking_perf.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/SmokeTest"
mkdir -p "$(dirname "$LOG")" "$OUT/frames"
QUIET=$("$ROOT/tools/ue_quiet_args.sh" 2>/dev/null) || QUIET="-unattended -nosplash -nosound -NoVerifyGC \
-ini:Input:[/Script/Engine.InputSettings]:bCaptureMouseOnLaunch=False \
-ini:Input:[/Script/Engine.InputSettings]:DefaultViewportMouseCaptureMode=NoCapture \
-ini:Input:[/Script/Engine.InputSettings]:DefaultViewportMouseLockMode=DoNotLock"
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200 --exclusive
rm -f "$LOG.rss"
FIX=""; [ "$FRAMES" = 1 ] && FIX="-UseFixedTimeStep -FPS=24"
SMOKETEST_FRAMES="${FRAMES:-0}" "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game \
    -RenderOffscreen -ueexclusive -windowed -ResX=1600 -ResY=900 $QUIET $FIX -dpcvars=lyud.MainMenuOnStart=0$DPC $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_smoking_perf.py" > "$LOG.stdout" 2>&1 &
PID=$!
( while ps -p "$PID" >/dev/null; do echo "$(date +%s) $(ps -o rss= -p "$PID")" >> "$LOG.rss"; sleep 5; done ) &
( sleep "${TIMEOUT:-1500}"; ps -p "$PID" -o args= | grep -q test_smoking_perf.py && kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "SMOKEPERF|SMOKE state|LogScript.*Error|Critical error" "$LOG" | grep -v "shot [0-9]* frame" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
