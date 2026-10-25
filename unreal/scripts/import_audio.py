"""Imports the generated sounds and music (tools/gen_sounds.py) into /Game/Audio.

  audio/sfx/<group>/<id>_<nn>.wav  ->  /Game/Audio/<Group>/S_<id>_<nn>   (looping when its sidecar says loop)
  audio/music/<id>.wav             ->  /Game/Audio/Music/MU_<id>         (<id> = <mood>_<n>, the music's mood)

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_audio.py -unattended -nosplash
"""
import glob
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
tasks, loops = [], []


def add(path, dest, name):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = dest
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)


for path in sorted(glob.glob(os.path.join(ROOT, "audio", "sfx", "*", "*.wav"))):
    group = os.path.basename(os.path.dirname(path)).capitalize()
    name = "S_" + os.path.basename(path)[:-4]
    add(path, "/Game/Audio/" + group, name)
    side = path[:-4] + ".json"
    if os.path.exists(side) and json.load(open(side, encoding="utf-8")).get("loop"):
        loops.append("/Game/Audio/%s/%s" % (group, name))
for path in sorted(glob.glob(os.path.join(ROOT, "audio", "music", "*.wav"))):
    add(path, "/Game/Audio/Music", "MU_" + os.path.basename(path)[:-4])
tools.import_asset_tasks(tasks)
for p in loops:
    w = eal.load_asset(p)
    if w:
        w.set_editor_property("looping", True)
        eal.save_asset(p)
# music streams from disk instead of sitting in memory
for p in eal.list_assets("/Game/Audio/Music", recursive=False):
    w = eal.load_asset(p)
    if isinstance(w, unreal.SoundWave):
        try:
            w.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
            eal.save_asset(p.split(".")[0])
        except Exception as e:
            unreal.log_warning("AUDIO could not set streaming on %s: %s" % (p, e))
unreal.log_warning("AUDIO imported %d sounds (%d loops)" % (len(tasks), len(loops)))
