"""Imports the restaurant's procedural sounds (tools/restaurant/sounds.py) into /Game/Audio/Restaurant.

  audio/sfx/restaurant/rest_<kind>_<nn>.wav  ->  /Game/Audio/Restaurant/S_rest_<kind>_<nn>   (looping when its sidecar says "loop")

The same naming as unreal/scripts/import_audio.py (S_<pool>_<nn> under /Game/Audio/<Group>), so ULyudSoundscapeSubsystem's
effect pools rest_plate ... rest_coins find them. The music loop streams from disk (load on demand) like the other music.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_restaurant_sounds.py -unattended -nosplash
  (or: IMPORT_ONLY=sfx/restaurant ... import_audio.py does the same job)
"""
import glob
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEST = "/Game/Audio/Restaurant"
eal = unreal.EditorAssetLibrary
tasks, loops, streamed = [], [], []

for path in sorted(glob.glob(os.path.join(ROOT, "audio", "sfx", "restaurant", "*.wav"))):
    name = "S_" + os.path.basename(path)[:-4]
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = DEST
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)
    side = path[:-4] + ".json"
    meta = json.load(open(side, encoding="utf-8")) if os.path.exists(side) else {}
    if meta.get("loop"):
        loops.append("%s/%s" % (DEST, name))
    if meta.get("pool") == "rest_music":
        streamed.append("%s/%s" % (DEST, name))
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for p in loops:
    w = eal.load_asset(p)
    if w:
        w.set_editor_property("looping", True)
        if p in streamed:
            try:
                w.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
            except Exception as e:
                unreal.log_warning("RESTAURANT could not set streaming on %s: %s" % (p, e))
        eal.save_asset(p)
unreal.log_warning("RESTAURANT sounds imported %d (%d loops)" % (len(tasks), len(loops)))
