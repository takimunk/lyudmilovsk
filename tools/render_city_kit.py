"""Renders the public spaces' kit meshes outside the game (docs/city/public_spaces.md), for checking shapes, seams, faces
turned inside out and stretched patterns while the game can't render:

  1. the game writes them: a -game -nullrhi run with -dpcvars=lyud.City.ExportKit=<dir> (every CityKit mesh -> <name>.obj)
  2. /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/render_city_kit.py -- <dir> <out dir> [name filter]

Each mesh: vertex colours (linear) as base colour, the OBJ's material groups: "glow" emissive, "pat<k>_<tile>_r<rough>"
roughness (and a joint grid for slabs/setts drawn from UV so their seams show), a sun and a sky, two views (3/4 front and
3/4 back) so back faces and holes show; a contact sheet of all of them.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SRC = argv[0]
OUT = argv[1]
FILTER = argv[2] if len(argv) > 2 else ""
os.makedirs(OUT, exist_ok=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in
                                                                      bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 960, 640
    sc.render.film_transparent = False
    w = bpy.data.worlds.new("w")
    sc.world = w
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.72, 0.85, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 0.8
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(35))
    sc.collection.objects.link(sun)
    g = bpy.data.objects.new("ground", bpy.data.meshes.new("ground"))
    bm = g.data
    bm.from_pydata([(-200, -200, -0.005), (200, -200, -0.005), (200, 200, -0.005), (-200, 200, -0.005)], [], [(0, 1, 2, 3)])
    gm = bpy.data.materials.new("ground")
    gm.use_nodes = True
    gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.25, 0.27, 0.22, 1)
    bm.materials.append(gm)
    sc.collection.objects.link(g)
    return sc


def material(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    attr = nt.nodes.new("ShaderNodeVertexColor")
    if name == "glow":
        bsdf.inputs["Emission Color" if "Emission Color" in bsdf.inputs else "Emission"].default_value = (1.0, 0.72, 0.42, 1)
        bsdf.inputs["Emission Strength"].default_value = 3.0
    rough = 0.7
    pat, tile = 0, 0.0
    if name.startswith("pat"):
        parts = name[3:].split("_")
        pat, tile = int(parts[0]), float(parts[1])
        rough = float(parts[2][1:]) if len(parts) > 2 else 0.7
    bsdf.inputs["Roughness"].default_value = rough
    if pat in (1, 2, 4) and tile > 0:
        # a joint grid from UV (metres): darker lines where the slabs/setts/courses meet
        uv = nt.nodes.new("ShaderNodeUVMap")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(uv.outputs[0], sep.inputs[0])
        cols = []
        for axis, size in ((0, tile * (1.6 if pat == 2 else 2.5 if pat == 4 else 1.0)), (1, tile)):
            div = nt.nodes.new("ShaderNodeMath"); div.operation = "DIVIDE"; div.inputs[1].default_value = size
            nt.links.new(sep.outputs[axis], div.inputs[0])
            fr = nt.nodes.new("ShaderNodeMath"); fr.operation = "FRACT"
            nt.links.new(div.outputs[0], fr.inputs[0])
            ge = nt.nodes.new("ShaderNodeMath"); ge.operation = "PINGPONG"; ge.inputs[1].default_value = 0.5
            nt.links.new(fr.outputs[0], ge.inputs[0])
            lt = nt.nodes.new("ShaderNodeMath"); lt.operation = "LESS_THAN"; lt.inputs[1].default_value = 0.012 / size
            nt.links.new(ge.outputs[0], lt.inputs[0])
            cols.append(lt)
        mx = nt.nodes.new("ShaderNodeMath"); mx.operation = "MAXIMUM"
        nt.links.new(cols[0].outputs[0], mx.inputs[0]); nt.links.new(cols[1].outputs[0], mx.inputs[1])
        mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"
        nt.links.new(mx.outputs[0], mix.inputs[0])
        nt.links.new(attr.outputs["Color"], mix.inputs[6])
        mix.inputs[7].default_value = (0.08, 0.08, 0.08, 1)
        nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    else:
        nt.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def render_one(path, name):
    sc = reset()
    bpy.ops.wm.obj_import(filepath=path, forward_axis="Y", up_axis="Z")
    obs = [o for o in sc.objects if o.type == "MESH" and o.name != "ground"]
    if not obs:
        return []
    ob = obs[0]
    for i, slot in enumerate(ob.material_slots):
        nm = slot.material.name.split(".")[0] if slot.material else "pat0_0.00_r0.70"
        slot.material = material(nm)
    ob.data.shade_flat() if hasattr(ob.data, "shade_flat") else None
    # frame it
    bb = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    lo = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    hi = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    c = (lo + hi) / 2
    r = max((hi - lo).length / 2, 0.3)
    sc.objects["ground"].location.z = lo.z
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.lens = 40
    sc.collection.objects.link(cam)
    sc.camera = cam
    shots = []
    for tag, az, el in (("front", 35, 22), ("back", 215, 30)):
        a, e = math.radians(az), math.radians(el)
        d = r * 2.6
        cam.location = c + Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))) * d
        cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
        out = os.path.join(OUT, "%s_%s.png" % (name, tag))
        sc.render.filepath = out
        bpy.ops.render.render(write_still=True)
        shots.append(out)
    return shots


def sheet(paths):
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        return
    from PIL import Image, ImageDraw
    w, h = 480, 320
    cols = 4
    rows = (len(paths) + cols - 1) // cols
    im = Image.new("RGB", (w * cols, h * rows), (30, 30, 30))
    dr = ImageDraw.Draw(im)
    for i, p in enumerate(paths):
        t = Image.open(p).resize((w, h))
        im.paste(t, ((i % cols) * w, (i // cols) * h))
        dr.text(((i % cols) * w + 6, (i // cols) * h + 6), os.path.basename(p)[:-4], fill=(255, 255, 0))
    im.save(os.path.join(OUT, "_sheet.png"))


allshots = []
seen_skins = set()
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".obj") or (FILTER and FILTER not in f) or f.startswith("SM_CityBlockCollision"):
        continue
    if f.startswith("SM_CityBlocks_"):
        skin = f[len("SM_CityBlocks_"):].split("_")[0]
        if skin in seen_skins:
            continue                               # the paving cells: one sample per skin
        seen_skins.add(skin)
    allshots += render_one(os.path.join(SRC, f), f[:-4])
sheet(allshots)
print("RENDERED %d images to %s" % (len(allshots), OUT))
