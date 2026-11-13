#!/bin/sh
# Offscreen check of the quests talk variants, .again lines and remarks in Lvl_Scene01 (test_quest_depth.py, docs/quests.md): prints the QUESTDEPTH and QUESTTEST lines and copies the
# screenshots to Saved/QuestDepthTest/<n>_<name>.png. Needs Content/Scene01 and Characters copied from the main checkout (cp -c).
# Waits for other offscreen UE runs first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_quest_depth.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/QuestDepthTest"
mkdir -p "$(dirname "$LOG")" "$OUT"
sh "$ROOT/tools/wait_ue_queue.sh" 7200
rm -f "$SHOTS"/ScreenShot*.png "$OUT"/*.png
SND="-nosound"; [ "$SOUND" = 1 ] && SND="-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash $SND -dpcvars=lyud.MainMenuOnStart=0$DPC $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_quest_depth.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-2400}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "QUESTDEPTH|NpcDialogue: (line|talking)|remark|pickups, |QUESTTEST|QuestDirector:|LyudQuests:|FEEDER|LogScript.*Error" "$LOG" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
# the shots in the order taken
N=0
grep -o "QUESTDEPTH shot [0-9]* [a-z_0-9]*" "$LOG" | while read -r _ _ I NAME; do
    F=$(ls "$SHOTS"/ScreenShot*.png 2>/dev/null | sed -n "$((10#$I + 1))p")
    [ -n "$F" ] && cp "$F" "$OUT/${I}_$NAME.png"
done
ls "$OUT"
# the director's count, then the static checks
grep -q "QuestDirector: 1 pickups, 2 listeners, 14 remarks" "$LOG" && echo "DEPTH director counts ok" || echo "DEPTH director counts WRONG: $(grep -m1 'QuestDirector: [0-9]* pickups' "$LOG")"
python3 "$ROOT/tools/check_quest_talk.py" | tail -3
python3 "$ROOT/tools/check_devdata.py" | tail -3
