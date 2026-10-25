#!/bin/sh
# The cable-car metro's assets into this checkout's project, each importer in a process of its own (Interchange skips a
# second scene import in one process): the meshes and materials, its sounds, its announcements. Waits for other
# offscreen games and commandlets first (tools/wait_ue_queue.sh). docs/transport.md.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
"$ROOT/tools/wait_ue_queue.sh" 7200
"$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_transport.py" -unattended -nosplash -stdout | grep -E "TRANSPORT|Error" | grep -v LogInit || true
IMPORT_ONLY=sfx/transport/ "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_audio.py" -unattended -nosplash -stdout | grep -E "AUDIO|Error" | grep -v LogInit || true
IMPORT_ONLY=line.transport. "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_voice.py" -unattended -nosplash -stdout | grep -E "VOICE|Error" | grep -v LogInit || true
