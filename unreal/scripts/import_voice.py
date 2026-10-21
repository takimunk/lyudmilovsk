"""Imports audio/voice/<lang>/<line key>.wav as /Game/Voice/<lang>/V_<key, dots as underscores> (tools/gen_voice.py makes them).

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_voice.py -unattended -nosplash
"""
import glob
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for path in sorted(glob.glob(os.path.join(ROOT, "audio", "voice", "*", "*.wav"))):
    lang = os.path.basename(os.path.dirname(path))
    key = os.path.basename(path)[:-4].replace(".", "_")
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = "/Game/Voice/" + lang
    t.destination_name = "V_" + key
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)
tools.import_asset_tasks(tasks)
unreal.log_warning("VOICE imported %d lines" % len(tasks))
