#!/bin/sh
# Europa's ground and sky into this checkout's project (docs/europa.md): tools/europa/moon_terrain.py's
# design/europa/game/moon_terrain.glb and Content/Europa/moon.json -> /Game/Europa/Moon, the planet's textures and the
# materials (unreal/scripts/import_moon.py). Low priority; waits for a free UE slot (tools/wait_ue_queue.sh).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
nice -n 19 taskpolicy -b "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_moon.py" -unattended -nosplash -stdout | grep -E "MOON|EUROPA|Error|error" | grep -v LogInit || true
