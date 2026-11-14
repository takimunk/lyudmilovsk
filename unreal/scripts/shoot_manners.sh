#!/bin/sh
# Offscreen close-ups of the townsfolk's manners (shoot_manners.py) in Lvl_Scene01: never opens a window.
#   unreal/scripts/shoot_manners.sh            all manners -> Saved/MannerShots/manner_<name>.png
#   MANNERS=smoke,umbrella unreal/scripts/shoot_manners.sh
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/shoot_manners.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/Saved/MannerShots"
mkdir -p "$(dirname "$LOG")" "$OUT"
rm -f "$SHOTS"/manner_*.png
sh "$ROOT/tools/wait_ue_queue.sh"
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 \
    -game -RenderOffscreen -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound \
    -dpcvars=lyud.MainMenuOnStart=0 $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_manners.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-1200}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID"
kill "$DOG" 2>/dev/null
for F in "$SHOTS"/manner_*.png; do [ -e "$F" ] && mv "$F" "$OUT/"; done
grep -h "MANNERSHOT" "$LOG" | grep -v LogInit | sed 's/^.*MANNERSHOT/MANNERSHOT/'
grep -E "Ensure|ensure|Error" "$LOG" | grep -i -E "People|Manner|Gesture|TownTalk|NpcBody|BodyAnim" | head -20
