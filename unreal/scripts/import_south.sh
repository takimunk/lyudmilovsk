#!/bin/sh
# The Aton coast (docs/south.md) into this checkout's project, each step in a process of its own (Interchange imports only
# the first glb of a process): the text materials, every glb of blender/export/south listed in its manifest_*.json, then
# the placing in Lvl_Scene01. Headless; this checkout's own editor must be closed. The North's atlas material must exist
# (unreal/scripts/import_north.sh, STEPS=materials). Waits for other offscreen games and commandlets first.
#   STEPS="build signs materials import place" (default: materials import place)   FILES="belvedere.glb" limits the import
#   build: python3 tools/south.py and the props with Blender (BLENDER=/Applications/Blender.app/Contents/MacOS/Blender)
#   signs: python3 tools/south_signs.py (the boards' words as textures)
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
EXPORT="$ROOT/blender/export/south"
BLENDER="${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}"
run() {
    sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
    nice -n 19 "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_south.py $1" -unattended -nosplash -stdout 2>&1 \
        | grep -E "SOUTH|NORTH|Python: Error|Error:|Traceback|  File" | grep -v LogInit || true
}
for step in ${STEPS:-materials import place}; do
    case "$step" in
    build)
        python3 "$ROOT/tools/south.py"
        "$BLENDER" -b --python "$ROOT/blender/south/build_south.py" -- 2>&1 | grep -vE "^(Blender|Read|Saved)" || true
        ;;
    signs)
        python3 "$ROOT/tools/south_signs.py"
        ;;
    import)
        if [ -z "$FILES" ]; then
            FILES=$(python3 -c "
import glob, json, os, sys
out = set()
for p in glob.glob(os.path.join(sys.argv[1], 'manifest_*.json')):
    for r in json.load(open(p)).get('meshes', []):
        if r.get('file'):
            out.add(os.path.basename(r['file']))
print(' '.join(sorted(out)))" "$EXPORT")
        fi
        [ -n "$FILES" ] || echo "SOUTH nothing to import yet (no manifests in blender/export/south)"
        for f in $FILES; do run "import $f"; done
        ;;
    *)
        run "$step"
        ;;
    esac
done
