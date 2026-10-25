#!/bin/sh
# Offscreen walking test of the Academgorodok (test_academ_walk.py); never opens a window.
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_academ_walk.log"
mkdir -p "$(dirname "$LOG")"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash -nosound -dpcvars=lyud.MainMenuOnStart=0${DPCVARS:+,$DPCVARS} \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_academ_walk.py" > "$LOG.stdout" 2>&1 || true
grep -E "ACADEMWALK" "$LOG" | grep -v "LogInit" | sed "s/.*ACADEMWALK //" | tail -80
