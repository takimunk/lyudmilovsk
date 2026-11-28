#!/bin/sh
# The sailing school's meshes into this checkout's project (docs/yacht_school.md "Meshes (part C)"):
# builds the GLBs and pictures (pip bpy or Blender), then the materials and each kit in a process of its own
# (Interchange takes one scene per process). Waits for other offscreen games and commandlets first.
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
if python3 -c "import bpy" 2>/dev/null; then
  python3 "$ROOT/blender/sailing/build_all.py"
else
  BL="${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}"
  if "$BL" -b --python-expr "import PIL" 2>&1 | grep -q "ModuleNotFoundError"; then
    # Blender's own Python has no Pillow (the pictures and the signs need it): lend it the system Python's packages
    SITE="$(python3 -c "import os, PIL; print(os.path.dirname(os.path.dirname(PIL.__file__)))")" || {
      echo "import_sailing: Pillow is in neither Blender's Python nor python3 (python3 -m pip install pillow)" >&2; exit 1; }
    PYTHONPATH="$SITE${PYTHONPATH:+:$PYTHONPATH}" "$BL" -b --python-use-system-env -P "$ROOT/blender/sailing/build_all.py"
  else
    "$BL" -b -P "$ROOT/blender/sailing/build_all.py"
  fi
fi
"$ROOT/tools/wait_ue_queue.sh" 7200
run() { "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_sailing.py $*" -unattended -nosplash -stdout | grep -E "SAILING|Error" | grep -v LogInit || true; }
run materials
for kit in sailing_boats.glb sailing_gear.glb sailing_house.glb; do
  run import "$kit"
done
