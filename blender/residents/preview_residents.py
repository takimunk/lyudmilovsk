"""Preview renders of built residents: a lineup (front and side) and their faces, shaded like the game shades them.

    Blender -b -P blender/residents/preview_residents.py -- [--models R_a,R_b|--limit N] [--out renders/residents]
                                                            [--name lineup] [--faces]

Imports blender/residents/export/<Model>.glb side by side and gives the four sections node materials that do what
the game's M_Resident_* do (docs/residents.md): atlas texel T, tint (UV1.xy, UV2.x), amount (UV2.y, Cloth: x T.a):
colour = lerp(T, T * 2 * tint, amount). Used to check the looks, the fit of the clothes and the atlas UVs (RULE 3D).
"""
import argparse
import json
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
EXPORT = os.path.join(HERE, "export")
ATLASES = os.path.join(HERE, "atlases")


def material(section, image, uv_names):
    m = bpy.data.materials.new("prev_" + section)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    N = nt.nodes.new
    out = N("ShaderNodeOutputMaterial")
    bsdf = N("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = {"Skin": 0.5, "Cloth": 0.85, "Hair": 0.6, "Eyes": 0.2}[section]
    uv0 = N("ShaderNodeUVMap")
    uv0.uv_map = uv_names[0]
    tex = N("ShaderNodeTexImage")
    tex.image = image
    tex.interpolation = "Linear"
    nt.links.new(uv0.outputs["UV"], tex.inputs["Vector"])
    rg = N("ShaderNodeUVMap")
    rg.uv_map = uv_names[1]
    ba = N("ShaderNodeUVMap")
    ba.uv_map = uv_names[2]
    srg = N("ShaderNodeSeparateXYZ")
    sba = N("ShaderNodeSeparateXYZ")
    nt.links.new(rg.outputs["UV"], srg.inputs[0])
    nt.links.new(ba.outputs["UV"], sba.inputs[0])
    tint = N("ShaderNodeCombineXYZ")
    nt.links.new(srg.outputs[0], tint.inputs[0])
    nt.links.new(srg.outputs[1], tint.inputs[1])
    nt.links.new(sba.outputs[0], tint.inputs[2])
    two = N("ShaderNodeVectorMath")
    two.operation = "SCALE"
    two.inputs["Scale"].default_value = 2.0
    nt.links.new(tint.outputs[0], two.inputs[0])
    mul = N("ShaderNodeVectorMath")
    mul.operation = "MULTIPLY"
    nt.links.new(tex.outputs["Color"], mul.inputs[0])
    nt.links.new(two.outputs[0], mul.inputs[1])
    amt = sba.outputs[1]
    if section == "Cloth":
        am = N("ShaderNodeMath")
        am.operation = "MULTIPLY"
        nt.links.new(sba.outputs[1], am.inputs[0])
        nt.links.new(tex.outputs["Alpha"], am.inputs[1])
        amt = am.outputs[0]
    mix = N("ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(amt, mix.inputs[0])
    nt.links.new(tex.outputs["Color"], mix.inputs[6])
    nt.links.new(mul.outputs[0], mix.inputs[7])
    nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    if section == "Hair":
        nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
        try:
            m.surface_render_method = "DITHERED"
        except Exception:
            pass
    if section == "Skin":
        bsdf.inputs["Subsurface Weight"].default_value = 0.15
        bsdf.inputs["Subsurface Radius"].default_value = (0.4, 0.15, 0.08)
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    m.use_backface_culling = False
    return m


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def setup_scene(width, height):
    sc = bpy.context.scene
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            sc.render.engine = eng
            break
        except TypeError:
            continue
    sc.render.resolution_x, sc.render.resolution_y = width, height
    sc.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.6, 0.66, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.9
    sc.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    sc.collection.objects.link(sun)
    fill = bpy.data.objects.new("fill", bpy.data.lights.new("fill", "SUN"))
    fill.data.energy = 1.0
    fill.rotation_euler = (math.radians(70), 0, math.radians(-140))
    sc.collection.objects.link(fill)
    floor = bpy.data.meshes.new("floor")
    floor.from_pydata([(-50, -50, 0), (50, -50, 0), (50, 50, 0), (-50, 50, 0)], [], [(0, 1, 2, 3)])
    fo = bpy.data.objects.new("floor", floor)
    fm = bpy.data.materials.new("floor")
    fm.use_nodes = True
    fm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.35, 0.34, 0.32, 1)
    floor.materials.append(fm)
    sc.collection.objects.link(fo)
    return sc


def camera(sc, loc, rot, ortho=None, lens=50):
    cam = bpy.data.objects.get("cam")
    if cam is None:
        cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
        sc.collection.objects.link(cam)
    cam.location = loc
    cam.rotation_euler = rot
    if ortho:
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = ortho
    else:
        cam.data.type = "PERSP"
        cam.data.lens = lens
    sc.camera = cam
    return cam


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", default="")
    ap.add_argument("--limit", type=int, default=8)
    ap.add_argument("--export", default=EXPORT)
    ap.add_argument("--atlases", default=ATLASES)
    ap.add_argument("--out", default=os.path.join(ROOT, "renders", "residents"))
    ap.add_argument("--name", default="lineup")
    ap.add_argument("--faces", action="store_true")
    ap.add_argument("--side", action="store_true")
    ap.add_argument("--res", type=int, default=1)
    ap.add_argument("--fast", action="store_true", help="Workbench, textures without tints: geometry checks")
    ap.add_argument("--samples", type=int, default=12)
    ap.add_argument("--rows", type=int, default=1, help="a crowd: rows going back, seen from a little above")
    ap.add_argument("--random", type=int, default=0, help="pick this many models at random (seeded)")
    a = ap.parse_args(argv)
    models = [m for m in a.models.split(",") if m] or sorted(f[:-4] for f in os.listdir(a.export) if f.endswith(".glb"))
    if a.random:
        import random
        random.Random(7).shuffle(models)
        models = models[:a.random]
    elif not a.models:
        models = models[:a.limit]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    with open(os.path.join(a.atlases, "layout.json"), encoding="utf-8") as f:
        layout = json.load(f)
    images = {s: bpy.data.images.load(os.path.join(a.atlases, sec["file"])) for s, sec in layout["sections"].items()}
    for s, im in images.items():
        im.colorspace_settings.name = "sRGB"
        im.alpha_mode = "CHANNEL_PACKED"
    n = len(models)
    gap = 0.75
    rows = max(1, a.rows)
    cols = (n + rows - 1) // rows
    if rows > 1:
        sc = setup_scene(1920 * a.res, 1080 * a.res)
    else:
        sc = setup_scene(max(640, int(260 * n)) * a.res, 900 * a.res)
    try:
        sc.eevee.taa_render_samples = a.samples
    except Exception:
        pass
    if a.fast:
        sc.render.engine = "BLENDER_WORKBENCH"
        sc.display.shading.light = "STUDIO"
        sc.display.shading.color_type = "TEXTURE"
    mats = {}
    heads = []
    for i, m in enumerate(models):
        objs = import_glb(os.path.join(a.export, m + ".glb"))
        if rows > 1:
            r_, c_ = i // cols, i % cols
            x = (c_ - (cols - 1) / 2) * gap + (0.37 if r_ % 2 else 0.0)
            y = r_ * 1.1
        else:
            x, y = (i - (n - 1) / 2) * gap, 0.0
        roots = [o for o in objs if o.parent is None]
        for r in roots:
            r.location.x += x
            r.location.y += y
        for o in objs:
            if o.type != "MESH":
                continue
            uvn = [l.name for l in o.data.uv_layers]
            for slot in o.material_slots:
                base = slot.material.name.split(".")[0] if slot.material else "Skin"
                key = (base, tuple(uvn))
                if key not in mats:
                    mats[key] = material(base, images[base], uvn)
                slot.material = mats[key]
        bpy.context.view_layer.update()
        arm = [o for o in objs if o.type == "ARMATURE"]
        if arm:
            hb = arm[0].pose.bones.get("head")
            if hb:
                heads.append(arm[0].matrix_world @ hb.head)
    os.makedirs(a.out, exist_ok=True)
    width = (cols if rows > 1 else n) * gap + 0.4
    aspect = sc.render.resolution_x / sc.render.resolution_y
    ortho = max(width, 2.1 * aspect) if aspect >= 1 else 2.1
    if rows > 1:
        # a crowd photo: perspective, a little above eye height, the rows going back
        hfov = 2 * math.atan(18.0 / 40.0)
        dist = (width / 2) / math.tan(hfov / 2) * 1.02
        camera(sc, (0, -dist, 1.05 + dist * math.tan(math.radians(9))), (math.radians(81), 0, 0), lens=40)
    else:
        # front lineup (the glTF import faces the people to -Y)
        camera(sc, (0, -12, 0.98), (math.radians(90), 0, 0), ortho=ortho)
    sc.render.filepath = os.path.join(a.out, a.name + "_front.png")
    bpy.ops.render.render(write_still=True)
    if a.side:
        camera(sc, (12, 0, 0.98), (math.radians(90), 0, math.radians(90)), ortho=ortho)
        sc.render.filepath = os.path.join(a.out, a.name + "_side.png")
        bpy.ops.render.render(write_still=True)
    if a.faces and heads:
        sc.render.resolution_x, sc.render.resolution_y = 520 * a.res, 600 * a.res
        for i, h in enumerate(heads):
            camera(sc, (h.x + 0.12, h.y - 0.75, h.z + 0.07), (math.radians(87), 0, math.radians(9)), lens=85)
            sc.render.filepath = os.path.join(a.out, "%s_face_%02d.png" % (a.name, i))
            bpy.ops.render.render(write_still=True)
    print("preview:", n, "models ->", a.out)


if __name__ == "__main__":
    main()
