"""Quick EEVEE turntable-style stills of a character for review (renders/humans/*.png)."""
import math

import bpy
from mathutils import Vector


def _look_at(cam, target):
    d = Vector(target) - cam.location
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()


def setup_stage():
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 900, 1200
    sc.view_settings.view_transform = "AgX"
    w = bpy.data.worlds.get("preview") or bpy.data.worlds.new("preview")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.18, 0.17, 0.16, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 0.35
    sc.world = w
    for name, loc, energy, size in [("key", (1.6, -2.2, 2.6), 380, 1.5), ("fill", (-2.2, -1.4, 1.6), 110, 2.0),
                                    ("rim", (0.5, 2.5, 2.4), 220, 1.0)]:
        if name in bpy.data.objects:
            continue
        ld = bpy.data.lights.new(name, "AREA")
        ld.energy, ld.size = energy, size
        lo = bpy.data.objects.new(name, ld)
        sc.collection.objects.link(lo)
        lo.location = loc
        _look_at(lo, (0, 0, 1.0))
    if "cam" not in bpy.data.objects:
        cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
        sc.collection.objects.link(cam)
        sc.camera = cam
    return sc.camera


def shot(path, eye, target, lens=50):
    cam = setup_stage()
    cam.data.lens = lens
    cam.location = eye
    _look_at(cam, target)
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def standard_shots(prefix, height=1.75, seated=False):
    h = height
    tz = h * (0.36 if seated else 0.52)
    shot(prefix + "_front.png", (0.9, -3.6, tz + 0.1), (0, 0, tz), lens=50)
    shot(prefix + "_side.png", (3.7, -0.4, tz + 0.1), (0, 0, tz), lens=50)
    fz = h * (0.68 if seated else 0.93)
    shot(prefix + "_face.png", (0.25, -0.9, fz + 0.03), (0, 0, fz), lens=85)
