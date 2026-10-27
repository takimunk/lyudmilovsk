"""Close-up front pictures (base colour, unlit) and depth of each painted eye of a rigged character, for eye_fit.py.

    Blender -b <export/<id>.blend> -P blender/characters/eye_shots.py -- <id> <out dir>

Writes <out>/<id>_<0|1>.png (0 = the character's left, +X) and <id>_<0|1>_depth.npy (the surface's y in metres,
nan where nothing), each W x H metres round the eye of face_marks.json, PX pixels a metre.
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
W, H, PX = 0.07, 0.04, 10000
args = sys.argv[sys.argv.index("--") + 1:]
cid, out = args[:2]
marks = json.load(open(os.path.join(HERE, cid, "face_marks.json")))
if len(args) > 2 and not args[2].startswith("--"):                                # eyes where eye_fit.py found them in this file's frame
    marks = json.load(open(args[2]))
model = max((o for o in bpy.data.objects if o.type == "MESH" and not o.name.startswith("SK_")), key=lambda o: len(o.data.vertices))   # not the donor
for o in bpy.data.objects:
    if o.type == "MESH" and o != model:
        o.hide_render = True
if model.data.shape_keys:
    for kb in model.data.shape_keys.key_blocks:
        kb.value = 0.0
dg = bpy.context.evaluated_depsgraph_get()
tree = BVHTree.FromObject(model, dg, deform=True)
sc = bpy.context.scene
sc.render.engine = "BLENDER_EEVEE"
sc.render.resolution_x, sc.render.resolution_y = int(W * PX), int(H * PX)
sc.view_settings.view_transform = "Standard"
sc.render.film_transparent = True
mat = model.material_slots[0].material
nt = mat.node_tree
bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
base = next(l.from_node for l in nt.links if l.to_socket == bsdf.inputs["Base Color"])
outn = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
em = nt.nodes.new("ShaderNodeEmission")
nt.links.new(base.outputs["Color"], em.inputs["Color"])
nt.links.new(em.outputs["Emission"], outn.inputs["Surface"])
cam = bpy.data.objects.new("eyecam", bpy.data.cameras.new("eyecam"))
cam.data.type = "ORTHO"
cam.data.ortho_scale = W
sc.collection.objects.link(cam)
sc.camera = cam
mw = model.matrix_world
mwi = mw.inverted()
if "--face" in args:                               # one 0.2 m picture of the whole face, as rig_character's
    W = H = 0.2
    PX = 5120
eyes = marks["eyes"] if "--face" not in args else [{"x": 0.0, "z": (marks["eyes"][0]["z"] + marks["eyes"][1]["z"]) / 2}]
for i, e in enumerate(eyes):
    sc.render.resolution_x, sc.render.resolution_y = int(W * PX), int(H * PX)
    cam.data.ortho_scale = W
    cam.location = (e["x"], -1.0, e["z"])
    cam.rotation_euler = (math.radians(90), 0, 0)
    sc.render.filepath = os.path.join(out, "%s_%d.png" % (cid, i))
    bpy.ops.render.render(write_still=True)
    nx, nz = int(W * PX / 4), int(H * PX / 4)              # depth at a quarter of the picture's resolution
    d = np.full((nz, nx), np.nan, np.float32)
    for r in range(nz):
        for c in range(nx):
            x = e["x"] - W / 2 + (c + 0.5) * W / nx
            z = e["z"] + H / 2 - (r + 0.5) * H / nz
            loc, nor, idx, dist = tree.ray_cast(Vector((x, -2.0, z)), Vector((0, 1, 0)))
            if loc is not None:
                d[r, c] = loc.y
    np.save(os.path.join(out, "%s_%d_depth.npy" % (cid, i)), d)
    print("EYESHOT %s %d at (%.4f, %.4f) mesh %s world==local %s" % (cid, i, e["x"], e["z"], model.name, mw == mw.Identity(4)), flush=True)
