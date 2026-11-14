#!/bin/sh
# Plays the game from THIS checkout in a window (standalone -game, no editor UI): the main menu, then Lvl_Scene01.
#   tools/play.sh             windowed 1920x1080
#   tools/play.sh full        fullscreen
#   RES=2560x1440 tools/play.sh
# The C++ module must be built (tools/wait_ubt_queue.sh) and Content/Scene01 rebuilt (unreal/scripts/build_scene01.sh).
# The log goes to unreal/Lyudmilovsk/Saved/Logs/play.log. Quit from the game's menu or with Cmd+Q.
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
RES="${RES:-1920x1080}"
if [ ! -f "$ROOT/unreal/Lyudmilovsk/Binaries/Mac/UnrealEditor.modules" ]; then
    echo "This checkout has no built module yet: run tools/wait_ubt_queue.sh first"; exit 1
fi
if [ ! -f "$ROOT/unreal/Lyudmilovsk/Content/Scene01/Lvl_Scene01.umap" ]; then
    echo "No Lvl_Scene01 here: run unreal/scripts/build_scene01.sh first"; exit 1
fi
MODE="-windowed -ResX=${RES%x*} -ResY=${RES#*x}"
[ "$1" = full ] && MODE="-fullscreen"
mkdir -p "$ROOT/unreal/Lyudmilovsk/Saved/Logs"
echo "Starting Lyudmilovsk from $ROOT (first start compiles shaders for a few minutes)"
exec "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 \
    -game $MODE -nosplash -abslog="$ROOT/unreal/Lyudmilovsk/Saved/Logs/play.log"
