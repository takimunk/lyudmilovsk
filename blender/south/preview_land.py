"""Preview renders of the land props' GLBs (blender/export/south, manifest_land.json) with the real atlas and the sign
textures, close-ups of every text surface per language, and a contact sheet.

    python3 blender/south/preview_land.py [kind|name ...] [--lang ru] [--close ru,en,ja] [--sheet] [--out dir]
                                          [--res 640] [--samples 24] [--copy path.png]
                                          [--az rad] [--el rad] [--suffix _front] [--no-overview]

The atlas is regenerated from blender/north/northtex.py into Saved/South/atlas/ when that folder is empty (the repo's copy is
an LFS file); text slots M_South_Text_<id> get textures/localized/<lang>/south_<id>.png, as unreal/scripts/import_south.py
should. Overviews: <out>/<name>.png; close-ups: <out>/close_<lang>_<name>_<slot>_<k>.png (an orthographic camera square to
each text face, so a mirrored or stretched texture shows at once); --sheet: <out>/preview_land.png (+ --copy).
"""
import json
import math
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "north"))
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import northkit as K  # noqa: E402
import northtex as T  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
VALUE_OPTS = ("--out", "--res", "--samples", "--lang", "--close", "--copy", "--az", "--el", "--suffix")


def opt(n, d):
    return ARGS[ARGS.index(n) + 1] if n in ARGS else d


EXPORT = os.path.join(K.ROOT, "blender", "export", "south")
MANIFEST = os.path.join(EXPORT, "manifest_land.json")
ATLAS = os.path.join(K.ROOT, "Saved", "South", "atlas")
OUT = opt("--out", os.path.join(K.ROOT, "Saved", "South", "preview"))
RES = int(opt("--res", 640))
SAMPLES = int(opt("--samples", 24))


def ensure_atlas():
    if not os.path.exists(os.path.join(ATLAS, "north_atlas_n.png")):
        T.write(ATLAS)
    K.GEN = ATLAS            # atlas_material() reads K.GEN


def setup_world():
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = SAMPLES
    sc.cycles.device = "CPU"
    try:
        sc.cycles.use_denoising = True
    except Exception:       # noqa: BLE001
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
    so.rotation_euler = (0.9, 0.0, 0.5)
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


def import_glb(path, lang):
    """Imports a GLB (materials without images) and gives the atlas / text slots their textures, as the engine does."""
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
            elif nm.startswith("M_South_Text_"):
                tid = nm[len("M_South_Text_"):]
                p = os.path.join(K.ROOT, "textures", "localized", lang, "south_%s.png" % tid)
                o.material_slots[i].material = K.text_material("PV_%s_%s" % (lang, nm), p, plain=False)
                o.material_slots[i].material["slot"] = nm
    return new


def bounds(objs):
    pts = []
    for o in objs:
        if o.type == "MESH":
            pts += [o.matrix_world @ Vector(c) for c in o.bound_box]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


def camera(loc, target, lens=45, ortho=None, up=Vector((0, 0, 1))):
    cam = bpy.data.cameras.new("c")
    cam.lens = lens
    if ortho:
        cam.type = "ORTHO"
        cam.ortho_scale = ortho
    co = bpy.data.objects.new("c", cam)
    bpy.context.scene.collection.objects.link(co)
    co.location = loc
    co.rotation_euler = (target - loc).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = co
    return co


def render(path, w, h):
    sc = bpy.context.scene
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.filepath = path
    sc.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)


def render_one(glb, outpng, lang="ru", az=-0.6, el=0.32):
    K.reset_scene()
    setup_world()
    ground()
    objs = import_glb(glb, lang)
    mn, mx = bounds(objs)
    c = (mn + mx) / 2
    ext = max((mx - mn).x, (mx - mn).y, (mx - mn).z, 2.0)
    human(mx.x + 0.7, 0.0)
    mid = Vector((c.x + 0.35, c.y, max(c.z, 0.8)))
    d = Vector((math.cos(el) * math.sin(az), -math.cos(el) * math.cos(az), math.sin(el)))
    camera(mid + d * (ext * 1.7 + 1.0), mid, lens=40)
    render(outpng, RES, int(RES * 0.78))


def text_faces(objs):
    """{slot: [(centre, normal, width, height)]}: the text polygons grouped by slot and facing."""
    out = {}
    for o in objs:
        if o.type != "MESH":
            continue
        me = o.data
        groups = {}
        for p in me.polygons:
            mat = o.material_slots[p.material_index].material
            if not mat or "slot" not in mat:
                continue
            nm = mat["slot"]
            n = (o.matrix_world.to_3x3() @ p.normal).normalized()
            lst = groups.setdefault(nm, [])
            for g in lst:
                if g["n"].dot(n) > 0.95:
                    g["polys"].append(p)
                    break
            else:
                lst.append({"n": n, "polys": [p]})
        for nm, lst in groups.items():
            for g in lst:
                vs = [o.matrix_world @ me.vertices[v].co for p in g["polys"] for v in p.vertices]
                n = g["n"]
                right = n.cross(Vector((0, 0, 1))) * -1.0
                right = right.normalized() if right.length > 1e-6 else Vector((1, 0, 0))
                up = right.cross(n) * -1.0
                c = sum(vs, Vector()) / len(vs)
                xs = [(v - c).dot(right) for v in vs]
                zs = [(v - c).dot(up) for v in vs]
                cc = c + right * (max(xs) + min(xs)) / 2 + up * (max(zs) + min(zs)) / 2
                out.setdefault(nm, []).append((cc, n, max(xs) - min(xs), max(zs) - min(zs)))
    return out


def render_closeups(glb, name, lang):
    K.reset_scene()
    setup_world()
    ground()
    objs = import_glb(glb, lang)
    files = []
    for nm, faces in sorted(text_faces(objs).items()):
        for k, (c, n, w, h) in enumerate(faces):
            aspect = max(0.25, min(8.0, w / max(h, 1e-3)))
            W = 900
            H = max(120, int(W / aspect * 1.25))
            camera(c + n * 2.0, c, ortho=w * 1.12)
            path = os.path.join(OUT, "close_%s_%s_%s_%d.png" % (lang, name, nm[len("M_South_Text_"):], k))
            render(path, W, H)
            files.append(path)
    return files


def sheet(names, langs_close, out):
    from PIL import Image, ImageDraw, ImageFont
    man = {e["file"][:-4]: e for e in json.load(open(MANIFEST))["meshes"]}
    cw, ch = RES // 2, int(RES * 0.78) // 2
    cols = 4
    rows = (len(names) + cols - 1) // cols
    f = ImageFont.load_default(size=14)
    tiles = []
    for n in names:
        im = Image.open(os.path.join(OUT, n + ".png")).convert("RGB").resize((cw, ch))
        d = ImageDraw.Draw(im)
        e = man.get(n, {})
        d.text((6, 4), "%s  %s tris  %s" % (n, e.get("tris", "?"), e.get("collision", "")), fill=(255, 255, 255), font=f,
               stroke_width=2, stroke_fill=(0, 0, 0))
        tiles.append(im)
    close = [p for p in sorted(os.listdir(OUT)) if p.startswith("close_ru_")]
    thumbs = []
    for p in close:
        im = Image.open(os.path.join(OUT, p)).convert("RGB")
        im = im.resize((cw * cols // 3, max(1, int(im.size[1] * (cw * cols // 3) / im.size[0]))))
        thumbs.append(im)
    th_rows = []
    row, rw = [], 0
    for im in thumbs:
        if len(row) == 3:
            th_rows.append(row)
            row = []
        row.append(im)
    if row:
        th_rows.append(row)
    H = rows * ch + sum(max(i.size[1] for i in r) for r in th_rows)
    out_im = Image.new("RGB", (cw * cols, H), (24, 24, 24))
    for i, im in enumerate(tiles):
        out_im.paste(im, ((i % cols) * cw, (i // cols) * ch))
    y = rows * ch
    for r in th_rows:
        for j, im in enumerate(r):
            out_im.paste(im, (j * (cw * cols // 3), y))
        y += max(i.size[1] for i in r)
    out_im.save(out)
    return out


def main():
    skip = set()
    for o in VALUE_OPTS:
        if o in ARGS:
            skip.add(ARGS.index(o) + 1)
    kinds = [a for i, a in enumerate(ARGS) if not a.startswith("--") and i not in skip]
    ensure_atlas()
    man = json.load(open(MANIFEST))["meshes"]
    names = [e["file"][:-4] for e in man if not kinds or e["kind"] in kinds or e["file"][:-4] in kinds]
    os.makedirs(OUT, exist_ok=True)
    lang = opt("--lang", "ru")
    if "--no-overview" not in ARGS:
        for n in names:
            render_one(os.path.join(EXPORT, n + ".glb"), os.path.join(OUT, n + opt("--suffix", "") + ".png"), lang=lang,
                       az=float(opt("--az", -0.6)), el=float(opt("--el", 0.32)))
            print("rendered", n)
    close = opt("--close", "")
    for lg in [c for c in close.split(",") if c]:
        for e in man:
            n = e["file"][:-4]
            if n in names and any(m.startswith("M_South_Text_") for m in e.get("materials", [])):
                for p in render_closeups(os.path.join(EXPORT, n + ".glb"), n, lg):
                    print("close-up", p)
    if "--sheet" in ARGS:
        p = sheet([e["file"][:-4] for e in man], close, os.path.join(OUT, "preview_land.png"))
        print("sheet", p)
        cp = opt("--copy", "")
        if cp:
            os.makedirs(os.path.dirname(cp), exist_ok=True)
            shutil.copyfile(p, cp)
            print("copied", cp)


if __name__ == "__main__":
    main()
