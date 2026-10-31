#!/bin/sh
# The street food kit into this checkout's project (docs/food.md): build it first (python3 blender/food/food_atlas.py, then
# Blender -b --python blender/food/build_food.py and build_stalls.py), then the textures and the master material, then each
# kit in a process of its own (Interchange skips a second scene import in one process), then the eating sounds
# (audio/sfx/food, tools/food_sounds.py) and the item icons. Waits for other offscreen games and commandlets first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
QUIET="-unattended -nosplash -nosound"
for STEP in textures food stalls; do
	"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
	FOOD_STEP=$STEP "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_food.py" $QUIET -stdout | grep -E "FOOD|Error" | grep -v LogInit || true
done
"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
IMPORT_ONLY=sfx/food "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_audio.py" $QUIET -stdout | grep -E "AUDIO|Food|Error" | grep -v LogInit || true
# the item icons and the diary's sketches (tools/food_icons.py, tools/ui_diary_icons.py); both re-save every icon: revert the
# unchanged .uassets before committing
for S in import_hall_assets.py import_diary_assets.py; do
	"${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
	"$CMD" "$PROJ" -run=pythonscript -script="$HERE/$S" $QUIET -stdout | grep -E "Error" | grep -v LogInit || true
done
