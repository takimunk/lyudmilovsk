"""Renders a posed character at a Station 09 stand-in (glass desk, keyboard, pad, CRT, seat)."""
import os

import bpy
from mathutils import Vector


def _mat(name, rgb, rough=0.5, alpha=1.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Alpha"].default_value = alpha
    return m


def props(S):
    coll = bpy.data.collections.new("StationProxy")
    bpy.context.scene.collection.children.link(coll)

    def box(c, size, m):
        bpy.ops.mesh.primitive_cube_add(size=1, location=tuple(c))
        o = bpy.context.active_object
        o.scale = size
        for cl in o.users_collection:
            cl.objects.unlink(o)
        coll.objects.link(o)
        o.data.materials.append(m)
    box((0, -0.82, S.DESK - 0.006), (1.3, 0.8, 0.012), _mat("P_Glass", (0.3, 0.38, 0.38), 0.05, 0.35))
    box((0, -0.56, S.DESK + 0.018), (0.46, 0.17, 0.035), _mat("P_Kb", (0.1, 0.1, 0.1)))
    box((S.MOUSE.x, S.MOUSE.y, S.DESK + 0.002), (0.22, 0.19, 0.004), _mat("P_Pad", (0.05, 0.12, 0.3), 0.8))
    box((S.MOUSE.x, S.MOUSE.y, S.DESK + 0.018), (0.058, 0.1, 0.032), _mat("P_Mouse", (0.72, 0.66, 0.53)))
    box((0, -0.94, S.DESK + 0.22), (0.42, 0.49, 0.43), _mat("P_Crt", (0.72, 0.7, 0.66)))
    box((0, 0, S.SEAT - 0.0175), (0.42, 0.42, 0.035), _mat("P_Wood", (0.25, 0.13, 0.06), 0.45))


def render(arm, act, prefix, S, frames):
    scn = bpy.context.scene
    props(S)
    scn.render.engine = "BLENDER_EEVEE"
    scn.render.resolution_x = scn.render.resolution_y = 900
    w = bpy.data.worlds.new("W")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.09, 0.085, 0.08, 1)
    scn.world = w
    for nm, loc, e in (("Key", (-0.9, -1.5, 1.9), 120), ("Fill", (1.2, -1.1, 1.3), 40), ("Rim", (0.5, 1.2, 1.9), 80)):
        ld = bpy.data.lights.new(nm, "AREA")
        ld.energy, ld.size = e, 0.9
        lo = bpy.data.objects.new(nm, ld)
        scn.collection.objects.link(lo)
        lo.location = loc
        lo.rotation_euler = (Vector((0, -0.3, 0.9)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    arm.animation_data.action = act
    views = {"front": ((-0.55, -1.55, 1.3), (0, -0.25, 0.9), 30),
             "hands": ((-0.62, -0.95, 1.1), (-0.15, -0.5, 0.82), 38),
             "face": ((-0.25, -0.75, 1.3), (0, -0.12, 1.2), 60),
             "side": ((1.6, -0.4, 1.0), (0, -0.3, 0.75), 32)}
    os.makedirs(os.path.dirname(prefix), exist_ok=True)
    for f in frames:
        scn.frame_set(f + 1)
        for tag, (loc, tgt, lens) in views.items():
            if f and tag not in ("hands", "face"):
                continue
            cam.location = loc
            cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
            cam.data.lens = lens
            scn.render.filepath = "%s_%s_%03d.png" % (prefix, tag, f)
            bpy.ops.render.render(write_still=True)
