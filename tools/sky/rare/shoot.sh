#!/bin/sh
# Offscreen screenshots of the rare sky (unreal/scripts/shoot_rare.py) with a watchdog; waits for a UE slot (ue_slot.sh).
#   tools/sky/rare/shoot.sh <tag> [RARE_SHOTS=aurora,halo ...]   -> Saved/RareSky/<tag>/NN_<shot>.png + <tag>.log
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
P="$ROOT/unreal/Lyudmilovsk"
TAG=$1; shift
for kv in "$@"; do export "$kv"; done
OUT="$ROOT/Saved/RareSky/$TAG"
SHOTDIR="$P/Saved/Screenshots/MacEditor"
mkdir -p "$OUT" "$SHOTDIR"
. "$(dirname "$0")/ue_slot.sh"
MARK="$OUT/.start"; touch "$MARK"
LOG="$ROOT/Saved/RareSky/$TAG.log"
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor" "$P/Lyudmilovsk.uproject" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
	-ResX=1600 -ResY=900 -windowed -dpcvars=lyud.MainMenuOnStart=0 -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_rare.py" \
	-unattended -nosplash >/dev/null 2>&1 &
PID=$!
T=0; MAX=${WATCHDOG:-1500}
while kill -0 $PID 2>/dev/null; do sleep 5; T=$((T+5)); [ $T -ge $MAX ] && { echo "watchdog: killing after $T s"; kill $PID; sleep 5; kill -9 $PID 2>/dev/null; }; done
grep "RARE " "$LOG" | grep -v "calendar now\|script error" | sed 's/.*RARE //' | cut -d' ' -f1 > "$OUT/names.txt"
i=0
for f in $(find "$SHOTDIR" -name '*.png' -newer "$MARK" | sort); do
	i=$((i+1)); n=$(sed -n "${i}p" "$OUT/names.txt"); cp "$f" "$OUT/$(printf %02d $((i-1)))_$n.png"
done
echo "done $TAG: $i shots in $T s"
grep -E "RARE (script error|calendar)|Error:.*([Ss]hader|[Mm]aterial)|LyudRareSky:" "$LOG" | head -20
