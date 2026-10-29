#!/bin/sh
# Rebuilds the sky's materials (M_LyudSky with the rare sky, M_LyudSkyOptics, M_LyudLightning) and the Sky/Sea actors of
# Lvl_Scene01 headless (unreal/scripts/import_sky.py). No editor may have this checkout open.
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
. "$(dirname "$0")/ue_slot.sh"
mkdir -p "$ROOT/Saved/RareSky"
LOG="$ROOT/Saved/RareSky/rebuild_sky.log"
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject" -run=pythonscript \
	-script="$ROOT/unreal/scripts/import_sky.py" -unattended -nosplash -nullrhi > "$LOG" 2>&1
grep -E "SCENE01 SKY|Error" "$LOG" | grep -v "LogPython: Error: *$" | head -30
