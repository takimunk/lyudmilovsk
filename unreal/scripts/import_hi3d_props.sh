#!/bin/sh
# The hi3d town props into this checkout's project (docs/city/hi3d_props.md): optimize them first
# (Blender -b -P blender/props_hi3d/optimize.py), then the textures and materials, then each mesh in a process of its own
# (Interchange skips a second scene import in one process). Waits for other offscreen games and commandlets first.
#   unreal/scripts/import_hi3d_props.sh [<id> ...]     (default: every prop with blender/props_hi3d/game/<id>.glb)
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
QUIET="-unattended -nosplash -nosound"
IDS="$*"
if [ -z "$IDS" ]; then
	IDS=$(cd "$ROOT/blender/props_hi3d/game" && ls *.glb 2>/dev/null | sed 's/\.glb$//')
fi
"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
HI3D_STEP=materials "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_hi3d_props.py" $QUIET -stdout | grep -E "HI3DPROPS|Error" | grep -v LogInit || true
for ID in $IDS; do
	"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
	HI3D_STEP=mesh HI3D_ID=$ID "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_hi3d_props.py" $QUIET -stdout | grep -E "HI3DPROPS|Error" | grep -v LogInit || true
done
