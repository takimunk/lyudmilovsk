"""Runs a Blender script with pip bpy (the cloud has no Blender), as `Blender -b -P script.py -- args` would:

    python3 tools/residents/bpyrun.py blender/residents/build_residents.py -- --shard 0/4
"""
import runpy
import sys

script = sys.argv[1]
sys.argv = ["blender", "-b", "-P", script] + sys.argv[2:]
import bpy  # noqa: E402,F401  (the module must load before the script's own imports)
runpy.run_path(script, run_name="__main__")
