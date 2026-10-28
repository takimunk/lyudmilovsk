#!/bin/sh
# The sea railway's assets into this checkout's project (docs/railway.md), each importer in a process of its own
# (Interchange skips a second scene import in one process): the kit's meshes (blender/railway/rail_kit.py ->
# design/railway/game/rail_kit.glb) and its sounds (tools/railsim/rail_sounds.py -> audio/sfx/railway). Waits for
# other offscreen games and commandlets first (tools/wait_ue_queue.sh).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
python3 "$ROOT/tools/railway.py" > /dev/null
"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
"$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_railway.py" -unattended -nosplash -stdout | grep -E "RAILWAY|Error" | grep -v LogInit || true
IMPORT_ONLY=sfx/railway/ "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_audio.py" -unattended -nosplash -stdout | grep -E "AUDIO|Error" | grep -v LogInit || true
