#!/bin/sh
# Rebuilds /Game/Scene01/Lvl_TerrainPreview (make_terrain_preview_level.py) from scratch. Close this
# checkout's Unreal Editor first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/../Lyudmilovsk"
C="$PROJ/Content"
rm -rf "$C/Scene01/Terrain" "$C/Scene01/Lvl_TerrainPreview.umap" "$C/__ExternalActors__/Scene01/Lvl_TerrainPreview" \
    "$C/__ExternalObjects__/Scene01/Lvl_TerrainPreview"
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ/Lyudmilovsk.uproject" -run=pythonscript \
    -script="$HERE/make_terrain_preview_level.py" -unattended -nosplash -stdout | grep -E "SCENE01|Error" | grep -v LogInit
