"""Close-up check renders of the smoking kit with its real textures (the 3D glitch rule: the card on its UVs, seams, the
cigarettes in the pack), before and beside the game's own shots.

  /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/props/smoking/render_kit.py -- <out dir>

Needs export/smoking_kit.blend (build_smoking.py) and textures/localized/<lang>/cig_pack.png (tools/smoking_pack.py). Writes
pack_<lang>.png (lid open, ten in it), pack_ru_nine.png (one gone), pack_ru_back.png and kit.png (cigarette, butt, lighter).
"""
import math
import os
import sys

import bpy
from mathutils import Euler, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else os.path.join(HERE, "export", "renders")
MM = 0.001


def image_material(m, path):
    m.use_nodes = True
    nt = m.node_tree
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = bpy.data.images.load(path, check_existing=True)
    t.interpolation = "Cubic"
    nt.links.new(t.outputs["Color"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.6


def plain(m, rgb, rough=0.6, metal=0.0, emit=0.0):
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit:
        b.inputs["Emission Color"].default_value = (1.0, 0.25, 0.04, 1)
        b.inputs["Emission Strength"].default_value = emit


def setup():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(HERE, "export", "smoking_kit.blend"))
    scn = bpy.context.scene
    scn.render.engine = "CYCLES"
    scn.cycles.device = "CPU"
    scn.cycles.samples = 48
    scn.render.resolution_x, scn.render.resolution_y = 1000, 1000
    scn.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.12, 0.11, 0.1, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
    scn.world = world
    plain(bpy.data.materials["CigPaper"], (0.86, 0.85, 0.82), 0.55)
    image_material(bpy.data.materials["CigFilter"], os.path.join(ROOT, "textures", "smoking", "cig_filter.png"))
    plain(bpy.data.materials["CigTobacco"], (0.22, 0.12, 0.05), 0.9)
    plain(bpy.data.materials["CigEmber"], (0.3, 0.29, 0.28), 0.95, emit=6.0)
    plain(bpy.data.materials["LighterBody"], (0.92, 0.32, 0.02), 0.22)
    plain(bpy.data.materials["LighterMetal"], (0.72, 0.72, 0.74), 0.3, 1.0)
    for name, loc, energy in (("key", (0.25, -0.2, 0.35), 2.5), ("fill", (-0.3, -0.25, 0.1), 0.8), ("rim", (-0.1, 0.3, 0.3), 1.5)):
        L = bpy.data.lights.new(name, "AREA")
        L.energy = energy
        L.size = 0.2
        o = bpy.data.objects.new(name, L)
        o.location = loc
        o.rotation_euler = (Vector((0, 0, 0.045)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        scn.collection.objects.link(o)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.lens = 85
    scn.collection.objects.link(cam)
    scn.camera = cam
    return scn, cam


def aim(cam, loc, target):
    cam.location = loc
    cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()


def main():
    os.makedirs(OUT, exist_ok=True)
    scn, cam = setup()
    body, lid, collar = (bpy.data.objects[n] for n in ("SM_CigPackBody", "SM_CigPackLid", "SM_CigPackCollar"))
    cig = bpy.data.objects["SM_Cigarette"]
    D, W = 17.9 * MM, 42.3 * MM
    pitch, wall, gap, collar_t = 8 * MM, 0.5 * MM, 0.25 * MM, 0.4 * MM
    order = [(0, 2), (0, 1), (0, 3), (0, 0), (0, 4), (1, 2), (1, 1), (1, 3), (1, 0), (1, 4)]
    tens = []
    for i, (row, col) in enumerate(order):
        c = cig.copy()
        c.location = Vector((D / 2 - wall - collar_t - gap - pitch * (row + 0.5), -W / 2 + wall + collar_t + gap + pitch * (col + 0.5), wall + 0.05 * MM))
        scn.collection.objects.link(c)
        tens.append(c)
    # everything else of the kit out of the way
    for n in ("SM_Cigarette", "SM_CigFilter", "SM_CigPaper", "SM_CigTip", "SM_CigButt", "SM_Lighter", "SM_LighterFlame"):
        bpy.data.objects[n].hide_render = True
    lid.rotation_euler = Euler((0, -math.radians(115), 0))   # Blender's Y turn: the front up and over
    pack_mat = bpy.data.materials["Pack"]
    for lang in ("ru", "en", "ja"):
        bpy.data.materials.remove(pack_mat)
        pack_mat = bpy.data.materials.new("Pack")
        image_material(pack_mat, os.path.join(ROOT, "textures", "localized", lang, "cig_pack.png"))
        for o in (body, lid, collar):
            o.data.materials[0] = pack_mat
        aim(cam, (0.30, -0.10, 0.20), (0, 0, 0.05))
        scn.render.filepath = os.path.join(OUT, "pack_%s.png" % lang)
        bpy.ops.render.render(write_still=True)
        if lang == "ru":
            tens[0].hide_render = True
            aim(cam, (0.17, -0.05, 0.21), (0, 0, 0.07))
            scn.render.filepath = os.path.join(OUT, "pack_ru_nine.png")
            bpy.ops.render.render(write_still=True)
            tens[0].hide_render = False
            aim(cam, (-0.30, 0.08, 0.14), (0, 0, 0.045))
            scn.render.filepath = os.path.join(OUT, "pack_ru_back.png")
            bpy.ops.render.render(write_still=True)
    # the loose things: a cigarette burning (parts), the butt, the lighter
    for o in [body, lid, collar] + tens:
        o.hide_render = True
    f, p, t = (bpy.data.objects[n] for n in ("SM_CigFilter", "SM_CigPaper", "SM_CigTip"))
    for o in (f, p, t):
        o.hide_render = False
        o.location = (0, -0.03, 0.01)
    p.location.x = 21 * MM
    p.scale.x = 4.0
    t.location = (21 * MM + 0.04, -0.03, 0.01)
    butt, lighter, flame = (bpy.data.objects[n] for n in ("SM_CigButt", "SM_Lighter", "SM_LighterFlame"))
    for o in (butt, lighter, flame):
        o.hide_render = False
    butt.location = (0.03, 0.02, 0)
    lighter.location = (-0.03, 0.03, 0)
    flame.location = (-0.03, 0.03, 73 * MM)
    flame.scale = (0.55, 0.55, 1.6)
    plain(bpy.data.materials["Flame"], (1, 0.7, 0.3), emit=8.0)
    aim(cam, (0.22, -0.25, 0.2), (0.0, 0.0, 0.02))
    cam.data.lens = 60
    scn.render.filepath = os.path.join(OUT, "kit.png")
    bpy.ops.render.render(write_still=True)


main()
