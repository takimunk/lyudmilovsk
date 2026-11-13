"""Preview renders of the built GLBs (what ships), contact sheets and the camp layout.

    python3 blender/north/preview_north.py [kind|name ...] [--sheet] [--camp] [--out dir] [--sheet-dir dir] [--res 640] [--samples 24]
Renders each GLB with Cycles (CPU), neutral sky light, a grass plane and a 1.75 m grey box for scale, to
<out>/<name>.png; --sheet assembles contact sheets props_sheet_N.png; --camp renders camp_layout.png from tools/north.py.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import northkit as K  # noqa: E402

ARGS = sys.argv[1:]


def opt(n, d):
    return ARGS[ARGS.index(n) + 1] if n in ARGS else d


OUT = opt("--out", "/tmp/claude-0/-home-user/b5d14c29-0649-5973-937a-0628d5493640/scratchpad/prev")
RES = int(opt("--res", 640))
SAMPLES = int(opt("--samples", 24))
VALUE_OPTS = ("--out", "--res", "--samples", "--sheet-dir", "--az", "--el")


def setup_world(sun_rot=(0.9, 0.0, 0.7)):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = SAMPLES
    sc.cycles.device = "CPU"
    try:
        sc.cycles.use_denoising = True
    except Exception:
        pass
    sc.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.62, 0.72, 0.86, 1)
    bg.inputs[1].default_value = 1.0
    sc.world = w
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 3.2
    sun.angle = math.radians(5)
    so = bpy.data.objects.new("sun", sun)
    so.rotation_euler = sun_rot
    sc.collection.objects.link(so)


def ground(size=40.0):
    me = bpy.data.meshes.new("g")
    h = size / 2
    me.from_pydata([(-h, -h, 0), (h, -h, 0), (h, h, 0), (-h, h, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("ground", me)
    m = bpy.data.materials.new("grass")
    m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.20, 0.30, 0.10, 1)
    m.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 1.0
    me.materials.append(m)
    bpy.context.scene.collection.objects.link(ob)


def human(x, y):
    me = bpy.data.meshes.new("h")
    verts = [(x + a * 0.25, y + b * 0.15, c * 1.75) for a in (-1, 1) for b in (-1, 1) for c in (0, 1)]
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    me.from_pydata(verts, [], faces)
    ob = bpy.data.objects.new("human", me)
    m = bpy.data.materials.new("grey")
    m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.5, 0.5, 0.5, 1)
    me.materials.append(m)
    bpy.context.scene.collection.objects.link(ob)


def import_glb(path):
    """Imports a GLB (materials come without images) and gives the atlas / text slots their textures, as the engine does."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.type != "MESH":
            continue
        for i, s in enumerate(o.material_slots):
            nm = s.material.name.split(".")[0] if s.material else ""
            if nm == "M_North_Atlas":
                o.material_slots[i].material = K.atlas_material("PV_Atlas", plain=False)
            elif nm.startswith("M_North_Text_"):
                tid = nm[len("M_North_Text_"):]
                o.material_slots[i].material = K.text_material("PV_" + nm, os.path.join(K.ROOT, "textures", "localized", "ru", "north_%s.png" % tid), plain=False)
    return new


def bounds(objs):
    pts = []
    for o in objs:
        if o.type == "MESH":
            pts += [o.matrix_world @ Vector(c) for c in o.bound_box]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


def camera(center, dist, az, el, lens=45):
    cam = bpy.data.cameras.new("c")
    cam.lens = lens
    co = bpy.data.objects.new("c", cam)
    bpy.context.scene.collection.objects.link(co)
    d = Vector((math.cos(el) * math.sin(az), -math.cos(el) * math.cos(az), math.sin(el)))
    co.location = center + d * dist
    co.rotation_euler = (center - co.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = co
    return co


def render(path, w, h):
    sc = bpy.context.scene
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.filepath = path
    sc.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)


def render_one(glb, outpng, az=-0.6, el=0.38, with_human=True):
    K.reset_scene()
    setup_world()
    ground()
    objs = import_glb(glb)
    mn, mx = bounds(objs)
    c = (mn + mx) / 2
    ext = max((mx - mn).x, (mx - mn).y, (mx - mn).z, 2.2)
    if with_human:
        human(mx.x + 0.7, 0.0)
    mid = Vector((c.x + (0.35 if with_human else 0), c.y, max(c.z, 0.85)))
    camera(mid, ext * 1.9 + 1.2, az, el, lens=40)
    render(outpng, RES, int(RES * 0.78))


def sheets(names, outdir):
    from PIL import Image, ImageDraw, ImageFont
    man = {e["file"][:-4]: e for e in json.load(open(os.path.join(K.EXPORT, "manifest.json")))["meshes"]}
    per = 6
    files = []
    f = ImageFont.load_default(size=18)
    cw, ch = RES, int(RES * 0.78)
    for si in range(0, len(names), per):
        group = names[si:si + per]
        sheet = Image.new("RGB", (cw * 3, ch * 2), (30, 30, 30))
        d = ImageDraw.Draw(sheet)
        for i, n in enumerate(group):
            im = Image.open(os.path.join(OUT, n + ".png")).convert("RGB")
            sheet.paste(im, ((i % 3) * cw, (i // 3) * ch))
            e = man.get(n, {})
            d.text(((i % 3) * cw + 8, (i // 3) * ch + 6), "%s  %s tris" % (n, e.get("tris", "?")), fill=(255, 255, 255), font=f,
                   stroke_width=2, stroke_fill=(0, 0, 0))
        p = os.path.join(outdir, "props_sheet_%d.png" % (si // per + 1))
        sheet.save(p)
        files.append(p)
    return files


def camp_layout(outpng):
    sys.path.insert(0, os.path.join(K.ROOT, "tools"))
    import north
    K.reset_scene()
    setup_world(sun_rot=(0.85, 0.0, 0.5))
    ground(120.0)
    man = {(e["kind"], e["variant"]): e for e in json.load(open(os.path.join(K.EXPORT, "manifest.json")))["meshes"]}
    cx, cy = north.CAMP
    for o in north.camp():
        e = man.get((o["kind"], o.get("variant", 0))) or man.get((o["kind"], 0))
        if not e:
            continue
        objs = import_glb(os.path.join(K.EXPORT, e["file"]))
        yaw = math.radians(o["yaw"])
        rzv = math.atan2(-math.cos(yaw), -math.sin(yaw))     # Blender world: x = east (map -X), y = north
        root = bpy.data.objects.new("root", None)
        bpy.context.scene.collection.objects.link(root)
        root.location = (-(o["x"] - cx), o["y"] - cy, 0)
        root.rotation_euler = (0, 0, rzv)
        for ob in objs:
            if ob.parent is None:
                ob.parent = root
    human(0.0, -3.0)
    camera(Vector((2.0, 4.0, 1.0)), 72, 0.0, 0.62, lens=40)
    render(outpng, 1600, 1000)


def main():
    skip = set()
    for o in VALUE_OPTS:
        if o in ARGS:
            skip.add(ARGS.index(o) + 1)
    kinds = [a for i, a in enumerate(ARGS) if not a.startswith("--") and i not in skip]
    man = json.load(open(os.path.join(K.EXPORT, "manifest.json")))["meshes"]
    names = [e["file"][:-4] for e in man if not kinds or e["kind"] in kinds or e["file"][:-4] in kinds]
    os.makedirs(OUT, exist_ok=True)
    az = float(opt("--az", -0.6))
    el = float(opt("--el", 0.38))
    if "--sheet" not in ARGS and "--camp" not in ARGS:
        for n in names:
            render_one(os.path.join(K.EXPORT, n + ".glb"), os.path.join(OUT, n + ".png"), az=az, el=el)
            print("rendered", n)
    if "--sheet" in ARGS:
        for p in sheets([e["file"][:-4] for e in man], opt("--sheet-dir", OUT)):
            print("sheet", p)
    if "--camp" in ARGS:
        camp_layout(os.path.join(opt("--sheet-dir", OUT), "camp_layout.png"))


if __name__ == "__main__":
    main()
