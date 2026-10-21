#!/bin/sh
# Rebuilds Lvl_Scene01 from the .glb. Close the Unreal Editor first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../Lyudmilovsk"
rm -rf "$PROJ/Content/Scene01" "$PROJ/Content/__ExternalActors__/Scene01" "$PROJ/Content/__ExternalObjects__/Scene01"
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_scene01.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
# the ground (tools/terrain.py: a landscape) and then the outdoor map standing on it (tools/world_map.py),
# each in its own process
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_terrain.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_world.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
# the sky (ALyudSky: sun, moon, atmosphere, clouds, stars) and the Baltic sea, replacing the fixed sun and sea plane
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_sky.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
