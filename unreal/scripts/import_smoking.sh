#!/bin/sh
# The smoking kit into this checkout's project (docs/smoking.md): build the meshes first (blender/props/smoking/build_smoking.py)
# and the pictures (tools/smoking_pack.py), then textures + materials, then the meshes, each in a process of its own (Interchange
# skips a second scene import in one process). Waits for a free slot of the UE queue first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
QUIET=$("$ROOT/tools/ue_quiet_args.sh" 2>/dev/null || echo "-unattended -nosplash -nosound")
for STEP in textures meshes; do
	"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
	SMOKING_STEP=$STEP "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_smoking.py" $QUIET -stdout | grep -E "SMOKING|Error" | grep -v LogInit || true
done
