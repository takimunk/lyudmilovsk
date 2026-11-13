"""Renders the hut's preview set into a directory (called by hut_build.py --previews): hut_exterior.png (four views, 2 x 2),
hut_plan.png (top view, roof and ceiling hidden), hut_interior_1..4.png (eye height 1.6 m over the floor; the last with only the
kerosene lamp and the stove's glow lit). A grey 1.75 m box stands in for scale."""
import math
import os

import bpy
from mathutils import Vector

import hut_common as C
import hut_door as D
import hut_interior as I
import hut_preview as V

F = C.F


def _set_emit(matname, strength):
    m = C.mat(matname)
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Emission Strength"].default_value = strength


def _clear_lights():
    for o in list(bpy.data.objects):
        if o.type == "LIGHT":
            bpy.data.objects.remove(o)


def run(main, ceil, roof, interior, door, outdir):
    os.makedirs(outdir, exist_ok=True)
    tmp = "/tmp/hut_prev/"
    os.makedirs(tmp, exist_ok=True)
    V.setup((1280, 800), 48)
    V.world(1.0)
    V.sun()
    V.ground()
    door.location = Vector(D.DOOR_HINGE)
    door.rotation_euler = (0, 0, 0)
    human = V.human(1.6, -5.6, 0.0)

    # ---- four exterior views (door shut)
    shots = []
    for i, (loc, tgt) in enumerate([((-7.5, -9.5, 2.4), (0.0, 0.3, 1.8)), ((8.5, -9.0, 2.4), (0.0, 0.3, 1.8)),
                                   ((9.0, 9.5, 3.4), (0.0, 0.3, 1.8)), ((-9.0, 9.0, 3.2), (0.0, 0.3, 1.8))]):
        p = tmp + "ext%d.png" % i
        V.render(p, loc, tgt, 30)
        shots.append(p)
    V.sheet(shots, os.path.join(outdir, "hut_exterior.png"), cols=2)

    # ---- plan: roof and ceiling hidden, the door open, an orthographic camera
    for o in (roof, ceil):
        o.hide_render = True
    door.rotation_euler = (0, 0, math.radians(-95))
    human.location = (0.35, -0.7, F)
    cam = bpy.data.cameras.new("Plan")
    cam.type = "ORTHO"
    cam.ortho_scale = 9.2
    co = bpy.data.objects.new("Plan", cam)
    bpy.context.scene.collection.objects.link(co)
    co.location = (0.0, 0.2, 14.0)
    co.rotation_euler = (0, 0, 0)
    bpy.context.scene.camera = co
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = 1100, 1000
    bpy.context.scene.render.filepath = os.path.join(outdir, "hut_plan.png")
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(co)
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = 1280, 800
    for o in (roof, ceil):
        o.hide_render = False

    # ---- interior: the door open, the bulb on
    human.location = (0.55, 0.9, F)
    bulb = I.BULB
    pl = V.point((bulb[0], bulb[1], bulb[2] - 0.02), 45, (1.0, 0.85, 0.6), 0.05, "Bulb")
    _set_emit("HUT_LampGlass", 0.0)
    eye = F + 1.6
    views = [((0.5, -1.55, eye), (-0.35, 2.6, 1.3), 17), ((0.7, 3.05, eye), (-0.9, -1.2, 1.3), 17),
             ((-0.95, -1.3, eye), (1.5, 1.6, 1.4), 17)]
    for i, (loc, tgt, lens) in enumerate(views):
        V.render(os.path.join(outdir, "hut_interior_%d.png" % (i + 1)), loc, tgt, lens)

    # ---- the last: door shut, no sun, no sky, no bulb: the kerosene lamp and the stove's glow only
    door.rotation_euler = (0, 0, 0)
    human.hide_render = True
    _clear_lights()
    V.world(0.0)
    lamp = (-1.64, 0.14, I.TABLE_TOP + 0.21)
    V.point(lamp, 14, (1.0, 0.66, 0.30), 0.03, "Kerosene")
    V.point((-1.26, 2.45, F + 0.17), 5, (1.0, 0.40, 0.12), 0.03, "StoveGlow")
    _set_emit("HUT_LampGlass", 1.5)
    _set_emit("HUT_Fire", 25.0)
    V.setup((1280, 800), 96)
    bpy.context.scene.view_settings.exposure = 1.2
    V.render(os.path.join(outdir, "hut_interior_4.png"), (0.9, -1.4, eye), (-1.3, 1.6, 1.2), 17)
