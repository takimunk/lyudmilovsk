#!/bin/sh
# The hatch to Europa's assets into this checkout's project (docs/europa.md), each importer in a process of its own
# (Interchange skips a second scene import in one process): the kit's meshes, textures and materials
# (blender/europa/europa_kit.py -> design/europa/game/europa_kit.glb, tools/europa/europa_textures.py -> textures/europa),
# its sounds and music (audio/sfx/europa, audio/music/europa_*) and the hero's line on Europa (audio/voice/*/line.hero.europa_*).
# Waits for other offscreen games and commandlets first (tools/wait_ue_queue.sh).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
"$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_europa.py" -unattended -nosplash -stdout | grep -E "EUROPA|Error" | grep -v LogInit || true
IMPORT_ONLY=europa "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_audio.py" -unattended -nosplash -stdout | grep -E "AUDIO|Error" | grep -v LogInit || true
IMPORT_ONLY=europa "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_voice.py" -unattended -nosplash -stdout | grep -E "VOICE|Error" | grep -v LogInit || true
