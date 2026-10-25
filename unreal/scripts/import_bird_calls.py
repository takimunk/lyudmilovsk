"""Imports audio/birds/<Id>/S_<Id>_<call>.wav (tools/birdsim/bird_calls.py) as /Game/Birds/<Id>/Sounds/S_<Id>_<call>.

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_bird_calls.py -unattended -nosplash
"""
import glob
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for path in sorted(glob.glob(os.path.join(ROOT, "audio", "birds", "*", "S_*.wav"))):
    sp = os.path.basename(os.path.dirname(path))
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = "/Game/Birds/%s/Sounds" % sp
    t.destination_name = os.path.basename(path)[:-4]
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)
tools.import_asset_tasks(tasks)
unreal.log_warning("BIRDCALLS imported %d calls" % len(tasks))
