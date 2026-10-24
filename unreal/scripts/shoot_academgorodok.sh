#!/bin/sh
# Offscreen -game shots of the Academgorodok (shoot_academgorodok.py); never opens a window.
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/shoot_academgorodok.log"
mkdir -p "$(dirname "$LOG")"
rm -f "$ROOT"/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor/HighresScreenshot*.png
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0${DPCVARS:+,$DPCVARS} \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/shoot_academgorodok.py" > "$LOG.stdout" 2>&1 || true
grep -E "ACADEMSHOT|Error:" "$LOG" | grep -v "LogInit" | head -40
ls "$ROOT"/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor/
