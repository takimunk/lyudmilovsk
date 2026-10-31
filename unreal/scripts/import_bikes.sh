#!/bin/sh
# The bicycles' assets into this checkout's project (docs/bikes.md), each importer in a process of its own (Interchange
# skips a second scene import in one process): the parts (blender/bikes/build_bike.py -> design/bikes/game/bike_kit.glb)
# and the sounds (tools/bikesim/bike_sounds.py -> audio/sfx/bikes). Waits for other offscreen games and commandlets
# first (tools/wait_ue_queue.sh); never opens a window.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
QUIET="$("$ROOT/tools/ue_quiet_args.sh")"
"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
"$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_bikes.py" $QUIET -stdout | grep -E "BIKES|Error" | grep -v LogInit || true
IMPORT_ONLY=sfx/bikes/ "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_audio.py" $QUIET -stdout | grep -E "AUDIO|Error" | grep -v LogInit || true
