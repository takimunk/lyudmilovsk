#!/bin/sh
# The nest and chick kits into this checkout's project (docs/bird_nests.md): build them first
# (blender/birds/nests/build_nests.py, blender/birds/chicks/build_chicks.py), then the textures and the master material, then
# each kit in a process of its own (Interchange skips a second scene import in one process). Waits for other offscreen games
# and commandlets first (tools/wait_ue_queue.sh).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
for STEP in textures nests chicks; do
	"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
	NESTS_STEP=$STEP "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_nests.py" -unattended -nosplash -stdout | grep -E "NESTS|Error" | grep -v LogInit || true
done
