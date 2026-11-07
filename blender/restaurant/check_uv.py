"""UV check of the restaurant's meshes: no NaNs, no zero-area UV faces, metre scale (box-projected UVs: a face's UV area over
its 3D area lies in [1/3, 1] - its normal's largest component squared... at least 0.33).
  Blender -b -P blender/restaurant/check_uv.py [-- glob ...]     (default design/restaurant/game/SM_Rest_*.glb)"""
import glob
import math
import os
import sys

import bpy

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def tri_area2(a, b, c):
    return abs((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])) / 2


def check(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    bad_nan = bad_zero = bad_scale = faces = 0
    lo, hi = 1e9, 0.0
    for o in bpy.context.scene.objects:
        if o.type != "MESH":
            continue
        me = o.data
        uv = me.uv_layers.active
        if uv is None:
            print("UV %s: no UV layer" % os.path.basename(path))
            return 1
        me.calc_loop_triangles()
        for t in me.loop_triangles:
            faces += 1
            us = [tuple(uv.data[li].uv) for li in t.loops]
            if any(math.isnan(x) or math.isinf(x) for u in us for x in u):
                bad_nan += 1
                continue
            a3 = t.area
            if a3 < 1e-8:
                continue
            a2 = tri_area2(*us)
            if a2 < 1e-9:
                bad_zero += 1
                continue
            r = a2 / a3
            lo, hi = min(lo, r), max(hi, r)
            if r < 0.33 - 1e-3 or r > 1.0 + 1e-3:
                bad_scale += 1
    print("UV %s: %d tris, NaN %d, zero-area %d, off-metre %d, uv/3d area ratio %.3f..%.3f"
          % (os.path.basename(path)[:-4], faces, bad_nan, bad_zero, bad_scale, lo, hi), flush=True)
    return bad_nan + bad_zero + bad_scale


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
pats = argv or [os.path.join(ROOT, "design", "restaurant", "game", "SM_Rest_*.glb")]
paths = sorted(p for pat in pats for p in glob.glob(pat if os.path.isabs(pat) else os.path.join(ROOT, pat)))
bad = sum(1 for p in paths if check(p) > 0)
print("UV done: %d of %d meshes with problems" % (bad, len(paths)), flush=True)
sys.exit(1 if bad else 0)
