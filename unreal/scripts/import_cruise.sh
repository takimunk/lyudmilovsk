#!/bin/sh
# The cruise's sea into this checkout's project (docs/ship.md), each importer in a process of its own (Interchange
# skips a second scene import in one process): the islands' kit (blender/ship/seascape_kit.py ->
# design/ship/game/seascape_kit.glb, built here when it is missing: the export is not in git) and the ship's sounds
# (tools/ship/ship_sounds.py -> audio/sfx/ship). Waits for other offscreen games and commandlets first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
if [ ! -f "$ROOT/design/ship/game/seascape_kit.glb" ] || [ -n "$REBUILD_KIT" ]; then
    "${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}" -b -P "$ROOT/blender/ship/seascape_kit.py"
fi
[ -f "$ROOT/audio/sfx/ship/ship_horn_01.wav" ] || python3 "$ROOT/tools/ship/ship_sounds.py"
"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
"$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_cruise.py" -unattended -nosplash -stdout | grep -E "CRUISE|Error" | grep -v LogInit || true
IMPORT_ONLY=sfx/ship/ "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_audio.py" -unattended -nosplash -stdout | grep -E "AUDIO|Error" | grep -v LogInit || true
