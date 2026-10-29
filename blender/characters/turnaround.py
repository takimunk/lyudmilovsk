"""A generated person's model as Dima looks at it before anything else is done with it: front, side, back, the other
side, the face and both hands close, and the reference views beside the renders (design/people/<key>/gen.json).

  /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/characters/turnaround.py -- <key> [out_dir]

reads blender/characters/<key>/gen/model.glb (hi3d's raw model, local) and gen/front.png, back.png (the 4-view
reference), writes <out_dir>/<key>_{front,left,back,right,face,hand_l,hand_r}.png and <key>_sheet.jpg (default
_shots/residents). Prints the model's size, triangles and textures (the game budget is 24k triangles, 2k maps).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    key = argv[0]
    out = argv[1] if len(argv) > 1 else os.path.join(REPO, "_shots", "residents")
    os.makedirs(out, exist_ok=True)
    gen = os.path.join(HERE, key, "gen")
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(gen, "model.glb"))
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes)
    lo = Vector((1e9, 1e9, 1e9))
    hi = -lo
    for o in meshes:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
    size = hi - lo
    texs = sorted({(i.name, i.size[0], i.size[1]) for i in bpy.data.images if i.size[0]})
    print("TURNAROUND %s: %d meshes, %d triangles, size %.2f x %.2f x %.2f, textures %s" % (key, len(meshes), tris, size.x, size.y, size.z, texs))
    # scale to the person's real height is the rig's job later; here: frame whatever came
    h = size.z
    mid = (lo + hi) * 0.5
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE_NEXT"
    sc.render.resolution_x, sc.render.resolution_y = 1000, 1000
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.62, 0.64, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 1.1
    sc.world = w
    for rot, e in (((50, 0, 30), 2.6), ((60, 0, 200), 1.0)):
        sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
        sun.data.energy = e
        sun.rotation_euler = tuple(math.radians(a) for a in rot)
        sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam

    def shot(name, yaw, target, scale, z=None):
        """Ortho camera round the vertical axis: yaw 0 looks from -Y (glTF's +Z front comes in as -Y)."""
        a = math.radians(yaw)
        d = max(size.x, size.y) * 2 + 4
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = scale
        cam.location = (target.x + d * math.sin(a), target.y - d * math.cos(a), target.z if z is None else z)
        cam.rotation_euler = (math.radians(90), 0, a)
        sc.render.filepath = os.path.join(out, "%s_%s.png" % (key, name))
        bpy.ops.render.render(write_still=True)

    body = Vector((mid.x, mid.y, lo.z + h * 0.5))
    shot("front", 0, body, h * 1.08)
    shot("left", 90, body, h * 1.08)
    shot("back", 180, body, h * 1.08)
    shot("right", 270, body, h * 1.08)
    head = Vector((mid.x, mid.y, lo.z + h * 0.9))
    shot("face", 0, head, h * 0.17)
    shot("face_side", 90, head, h * 0.17)
    # the hands: the T-pose's ends of the span, found on the vertices (the tips of the fingers along X)
    vs = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
    for side, sign in (("hand_l", -1), ("hand_r", 1)):
        tip = min(vs, key=lambda v: v.x) if sign < 0 else max(vs, key=lambda v: v.x)
        near = [v for v in vs if abs(v.x - tip.x) < h * 0.12]
        c = Vector((tip.x - sign * h * 0.05, sum(v.y for v in near) / len(near), sum(v.z for v in near) / len(near)))
        shot(side, 0, c, h * 0.13)
        # from above: the palm down, the fingers' backs
        d = h * 3
        cam.data.ortho_scale = h * 0.13
        cam.location = (c.x, c.y, c.z + d)
        cam.rotation_euler = (0, 0, 0)
        sc.render.filepath = os.path.join(out, "%s_%s_top.png" % (key, side))
        bpy.ops.render.render(write_still=True)
    print("TURNAROUND renders -> %s" % out)


main()
