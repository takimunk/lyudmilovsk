#!/bin/sh
# Offscreen -game shots of the placed hi3d props (shoot_hi3d_props.py) plus ground / collision / ceiling probes; never
# opens a window, no sound, lowest priority, waits for a free UE slot. Shots go to Saved/Hi3dShots/<item id>.png.
#   unreal/scripts/shoot_hi3d_props.sh                 (HI3D_SHOTS=sundial_square,muse_P-CLUB2 for some only)
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/shoot_hi3d_props.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/Hi3dShots"
mkdir -p "$(dirname "$LOG")" "$OUT"
"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
rm -f "$SHOTS"/HighresScreenshot*.png "$OUT"/*.png
nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -NoMouseCapture -nosound -dpcvars=lyud.MainMenuOnStart=0${DPCVARS:+,$DPCVARS} \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_hi3d_props.py" > "$LOG.stdout" 2>&1 || true
grep -E "HI3D|LyudPublicSpaces.*hi3d|Error:" "$LOG" | grep -v "LogInit" | head -120
i=0
for f in $(ls "$SHOTS"/HighresScreenshot*.png 2>/dev/null | sort); do
    i=$((i + 1))
    name=$(grep "HI3DSHOT" "$LOG" | sed -n "${i}p" | sed -E 's/.*HI3DSHOT ([A-Za-z0-9_-]+).*/\1/')
    cp "$f" "$OUT/${name:-shot$i}.png"
done
ls "$OUT"
grep -q "HI3DTEST done" "$LOG" && ! grep -q "HI3DTEST FAIL" "$LOG"
