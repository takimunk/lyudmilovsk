#!/bin/sh
# Rebuilds Lvl_Scene01 from the .glb. Close the Unreal Editor first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../Lyudmilovsk"
rm -rf "$PROJ/Content/Scene01" "$PROJ/Content/__ExternalActors__/Scene01" "$PROJ/Content/__ExternalObjects__/Scene01"
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_scene01.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
# the outdoor map (tools/world_map.py) into the saved level, in its own process
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_world.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
