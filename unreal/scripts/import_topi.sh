#!/bin/sh
# Topi's props into this checkout's project (docs/topi.md): build them first (python3 blender/topi/props.py needs `pip install bpy`, or run it
# in Blender -b --python; python3 tools/make_topi_signs.py), then the textures and materials, then each mesh in a process of its own
# (Interchange skips a second scene import in one process). Waits for other offscreen games and commandlets first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
QUIET="-unattended -nosplash -nosound"
for STEP in textures SM_Topi_WarnSign SM_Topi_UralSign SM_Topi_Towel_A SM_Topi_Towel_B SM_Topi_Towel_C SM_Topi_Stake; do
	"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
	TOPI_STEP=$STEP "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_topi.py" $QUIET -stdout | grep -E "TOPI|Error" | grep -v LogInit || true
done
