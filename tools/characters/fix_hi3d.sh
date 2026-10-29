#!/bin/sh
# The hi3d people fixed in place (Dima, 2026-10-29): face painted from the front picture + smoothed
# (face_project.py), rigged (rig_character.py), real eyes fitted (eye_shots.py + eyes.py), imported (import_character.py).
#   tools/characters/fix_hi3d.sh owner guy_a ...      (logs in Saved/FixHi3d/<id>.log)
cd "$(dirname "$0")/../.."
B=/Applications/Blender.app/Contents/MacOS/Blender
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac"; P=$PWD/unreal/Lyudmilovsk/Lyudmilovsk.uproject
mkdir -p Saved/FixHi3d Saved/EyeShots
for c in "$@"; do
    L=Saved/FixHi3d/$c.log; : > $L
    echo "== $c"
    python3 - "$c" <<'PY'
import json, sys
c = sys.argv[1]; p = "blender/characters/%s/character.json" % c; d = json.load(open(p))
d["model"] = d.get("model", "model").removesuffix("_face")
d["faces"] = max(d.get("faces", 24000), 30000 if d.get("height", 1.7) < 1.5 else 40000)
json.dump(d, open(p, "w"), indent=1, ensure_ascii=False)
PY
    if python3 -c "import json,sys; sys.exit(0 if json.load(open('blender/characters/$c/character.json')).get('face_project',{}).get('skip') else 1)"; then
        echo "  face projection off (face_project.skip: glasses)"
    elif $B -b -P blender/characters/face_project.py -- $c >> $L 2>&1 && grep -q "FACEPROJ wrote" $L; then
        python3 -c "
import json; p='blender/characters/$c/character.json'; d=json.load(open(p)); d['model']=d['model']+'_face'; json.dump(d,open(p,'w'),indent=1,ensure_ascii=False)"
        grep "FACEPROJ model" $L
    else
        echo "  face projection skipped: $(grep -E 'RuntimeError|Error' $L | tail -1)"
    fi
    $B -b -P blender/characters/rig_character.py -- $c >> $L 2>&1; grep "RIG wrote" $L || { echo "  RIG FAILED"; continue; }
    if python3 -c "import json,sys; sys.exit(0 if json.load(open('blender/characters/$c/character.json')).get('face',{}).get('eye_open') else 1)"; then
        $B -b blender/characters/$c/export/$c.blend -P blender/characters/eye_shots.py -- $c Saved/EyeShots >> $L 2>&1
        python3 blender/characters/eyes.py $c Saved/EyeShots 2>&1 | grep "^EYE " | tee -a $L
    fi
    tools/wait_ue_queue.sh 3600 >/dev/null
    rm -f unreal/Lyudmilovsk/Saved/Autosaves/PackageRestoreData.json
    "$UE/UnrealEditor-Cmd" "$P" -run=pythonscript -script="$PWD/unreal/scripts/import_character.py $c" -unattended -nullrhi >> $L 2>&1
    grep -q "CHARACTER done" $L && echo "  imported" || echo "  IMPORT FAILED"
done
