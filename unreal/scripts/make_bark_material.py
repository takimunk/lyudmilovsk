"""Rebuilds only /Game/Plants/M_PlantBark (the bark half of make_plant_materials.py, which stays the one source of truth).

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import os

_src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_plant_materials.py")).read()
exec(compile(_src[:_src.index("# --- leaves")], "make_plant_materials.py (bark)", "exec"))
unreal.log_warning("BARK material saved")
