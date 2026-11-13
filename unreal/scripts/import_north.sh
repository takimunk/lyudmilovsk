#!/bin/sh
# The North into this checkout's project (docs/north.md "Game side"), each step in a process of its own (Interchange imports
# only the first glb of a process): its sounds (tools/north_sounds.py -> audio/sfx/north), its materials, every glb of
# blender/export/north listed in manifest.json / manifest_hut.json / far.json (the far ones made by tools/north_far.py when
# missing), then the placing in Lvl_Scene01. Headless; this checkout's own editor must be closed. Waits for other offscreen
# games and commandlets first (tools/wait_ue_queue.sh).
#   STEPS="sounds materials import place" (default all)   FILES="tent_dome_v0.glb hut.glb" limits the import
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../.."
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
EXPORT="$ROOT/blender/export/north"
sh "${WAIT:-$ROOT/tools/wait_ue_queue.sh}" 7200
run() {
    nice -n 19 taskpolicy -b "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_north.py $1" -unattended -nosplash -stdout 2>&1 \
        | grep -E "NORTH|Python: Error|Error:|Traceback|  File" | grep -v LogInit || true
}
for step in ${STEPS:-sounds materials import place}; do
    case "$step" in
    sounds)
        python3 "$ROOT/tools/north_sounds.py"
        IMPORT_ONLY=sfx/north "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_audio.py" -unattended -nosplash -stdout 2>&1 \
            | grep -E "AUDIO|Error" | grep -v LogInit || true
        ;;
    import)
        # the far backdrop first (numpy + PIL), so its files are listed
        [ -f "$EXPORT/far.json" ] || [ ! -f "$ROOT/tools/north_far.py" ] || python3 "$ROOT/tools/north_far.py" || echo "NORTH tools/north_far.py failed"
        if [ -z "$FILES" ]; then
            FILES=$(python3 -c "
import json, os, sys
d = sys.argv[1]
out = set()
for m in ('manifest.json', 'manifest_hut.json', 'far.json'):
    p = os.path.join(d, m)
    if os.path.exists(p):
        j = json.load(open(p))
        for r in (j if isinstance(j, list) else j.get('meshes', [])):
            if r.get('file'):
                out.add(os.path.basename(r['file']))
print(' '.join(sorted(out)))" "$EXPORT")
        fi
        [ -n "$FILES" ] || echo "NORTH nothing to import yet (no manifests in blender/export/north)"
        for f in $FILES; do run "import $f"; done
        ;;
    *)
        run "$step"
        ;;
    esac
done
