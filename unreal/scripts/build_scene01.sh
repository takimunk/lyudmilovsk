#!/bin/sh
# Rebuilds Lvl_Scene01 from the .glb. Close the Unreal Editor first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../Lyudmilovsk"
rm -rf "$PROJ/Content/Scene01" "$PROJ/Content/__ExternalActors__/Scene01" "$PROJ/Content/__ExternalObjects__/Scene01"
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_scene01.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
# the sea railway's line first: its tunnel mouth cuts the landscape (terrain.railway_holes reads railway_built.json)
python3 "$HERE/../../tools/railway.py" > /dev/null || echo "railway.py failed"
# the ground (tools/terrain.py: a landscape) and then the outdoor map standing on it (tools/world_map.py),
# each in its own process
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_terrain.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
# the cable car follows the new ground: lifts, stairs and pylons from terrain.ground_z (docs/transport.md)
python3 "$HERE/../../tools/transport.py" > /dev/null || echo "transport.py failed"
# and the railway on it (the game builds the line from railway_built.json at play: docs/railway.md)
python3 "$HERE/../../tools/railway.py" > /dev/null || echo "railway.py failed"
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_world.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
# the sky (ALyudSky: sun, moon, atmosphere, clouds, stars) and the Baltic sea, replacing the fixed sun and sea plane
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/import_sky.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
