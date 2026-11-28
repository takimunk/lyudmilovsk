#!/bin/sh
# The descent craft «Ласточка» into this checkout's project (docs/spacecraft.md), each importer in a process of its own
# (Interchange skips a second scene import in one process): the textures, materials and the kit's meshes
# (tools/spacecraft/textures.py, island_map.py, labels.py, sleeves.py -> textures/spacecraft;
# blender/spacecraft/spacecraft_kit.py -> design/spacecraft/game/spacecraft_kit.glb) and the fetched records
# (audio/music/spacecraft) by import_spacecraft.py, then its sounds (audio/sfx/spacecraft, tools/spacecraft/sounds.py) and
# its voice lines (audio/voice/*/line.flightauto.*, line.hero.flight_*). IMPORT_SPACECRAFT_ONLY=materials rebuilds the textures and materials
# only. Waits for other offscreen games and commandlets first (tools/wait_ue_queue.sh).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
# the lint first: a shader that does not parse here would only fail an hour into the queue
python3 "$HERE/import_spacecraft.py"
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
"$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_spacecraft.py" -unattended -nosplash -stdout | grep -E "SPACECRAFT|Error" | grep -v LogInit || true
if [ "$IMPORT_SPACECRAFT_ONLY" = "materials" ]; then exit 0; fi
IMPORT_ONLY=spacecraft "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_audio.py" -unattended -nosplash -stdout | grep -E "AUDIO|Error" | grep -v LogInit || true
IMPORT_ONLY=line.flightauto.,line.hero.flight_ "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_voice.py" -unattended -nosplash -stdout | grep -E "VOICE|Error" | grep -v LogInit || true
