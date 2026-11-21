#!/bin/sh
# Offscreen motion recordings of people, animals and birds (shoot_motion.py) in Lvl_Scene01 at a fixed 1/30 s step:
# never opens a window. Frames, contact sheets, mp4s and numbers go to Saved/Motion/<tag>/.
#   unreal/scripts/shoot_motion.sh [tag]                       all spots (tag default "base")
#   SPOTS=follow,animal unreal/scripts/shoot_motion.sh fix      some spots
#   MOTION_CMDS="lyud.Some.Fix 0" unreal/scripts/shoot_motion.sh off     console commands first (A/B in one binary)
#   DPCVARS="lyud.Anim.Smooth=0,lyud.Anim.Probe=12" (start-up cvars)
#   MOTION_FRAMES=90 (per recording), TIMEOUT=2400 (s), WAIT_UE=<path to wait_ue_queue.sh>, EXTRA=<more UE args>
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TAG="${1:-base}"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
SAVED="$ROOT/unreal/Lyudmilovsk/Saved"
LOG="$SAVED/Logs/shoot_motion_$TAG.log"
OUT="$SAVED/Motion/$TAG"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SAVED"/Screenshots/*/motion_"$TAG"_*.png
rm -rf "$OUT"/frames; mkdir -p "$OUT/frames"
while [ -e /tmp/lyud_dima_playing ]; do sleep 60; done
sh "${WAIT_UE:-$ROOT/tools/wait_ue_queue.sh}" "${WAIT_S:-14400}"
export MOTION_TAG="$TAG"
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 \
    -game -RenderOffscreen -windowed -ResX=960 -ResY=540 -unattended -nosplash -nosound -benchmark -fps=30 \
    -dpcvars=lyud.MainMenuOnStart=0${DPCVARS:+,$DPCVARS} $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_motion.py" > "$LOG.stdout" 2>&1 &
PID=$!
echo "$PID" > "$OUT/ue.pid"
( sleep "${TIMEOUT:-2400}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID"
kill "$DOG" 2>/dev/null
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
# ring buffers: frames dropped before the event
grep -h "MOTION ring drop" "$LOG" | sed 's/^.*ring drop //' | while read -r F; do rm -f "$SAVED"/Screenshots/*/"$F".png; done
for F in "$SAVED"/Screenshots/*/motion_"$TAG"_*.png; do [ -e "$F" ] && mv "$F" "$OUT/frames/"; done
grep -h "MOTION " "$LOG" | sed 's/^.*MOTION /MOTION /' > "$OUT/motion.txt"
grep -v " f[0-9][0-9][0-9][0-9] " "$OUT/motion.txt" | grep -v "ring drop" | head -60
grep -E "Ensure|ensure|Error|Warning" "$LOG" | grep -i -E "People|NpcBody|BodyAnim|LyudBody|Animal|Bird|Anim" | grep -v "MOTION " \
    | sed 's/^\[[^]]*\]\[[^]]*\]//' | sort | uniq -c | sort -rn | head -30 > "$OUT/log_issues.txt"
python3 "$ROOT/tools/motion_sheet.py" "$OUT"
