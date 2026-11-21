"""Preview renders of the props_marks GLBs with the real atlas (as the engine shows them), a 1.8 m capsule for scale.

    python3 blender/south/preview_marks.py [name ...] [--sheet] [--res 720] [--samples 16] [--views 34,front,close]

Writes Saved/South/preview/<name>_<view>.png and, with --sheet, /mnt/project-files/south/preview_marks.png. The atlas is
regenerated from blender/north/northtex.py into Saved/South/atlas/ when missing (the repo's PNGs are LFS pointers here).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "north"))
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import northkit as K  # noqa: E402
import northtex as T  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
EXPORT = os.path.join(ROOT, "blender", "export", "south")
OUT = os.path.join(ROOT, "Saved", "South", "preview")
ATLAS = os.path.join(ROOT, "Saved", "South", "atlas")
SHEET = "/mnt/project-files/south/preview_marks.png"
ARGS = sys.argv[1:]


def opt(n, d):
    return ARGS[ARGS.index(n) + 1] if n in ARGS else d


RES = int(opt("--res", 720))
SAMPLES = int(opt("--samples", 16))
VIEWS = opt("--views", "34,front,close").split(",")

# per mesh: capsule position, close-up focus point and distance, 3/4 azimuth
SHOTS = {
    "lighthouse": {"human": (1.2, -4.3, 0.0), "focus": (0.0, 0.0, 21.6), "dist": 7.5, "az": -0.6, "close_az": -0.5, "close_el": 0.05,
                   "focus2": (1.6, -2.8, 1.3), "dist2": 5.5},
    "bunker": {"human": (-1.6, -2.6, 0.0), "focus": (1.6, -1.4, 1.0), "dist": 3.6, "az": -0.65, "close_az": -0.55, "close_el": 0.3},
    "shipwreck": {"human": (0.4, -0.2, 0.0), "focus": (2.5, 0.0, 0.8), "dist": 4.5, "az": -0.9, "close_az": -0.5, "close_el": 0.35,
                  "ground": (0.62, 0.55, 0.40)},
    "erratic_v0": {"human": (2.9, -1.4, 0.0), "focus": (-0.6, -1.0, 2.0), "dist": 3.8, "az": -0.6, "close_az": -0.3, "close_el": 0.5},
    "erratic_v1": {"human": (0.0, -0.2, 0.0), "focus": (0.0, -1.2, 1.1), "dist": 3.4, "az": -0.5, "close_az": 0.0, "close_el": 0.15},
}


def atlas_material():
    if "PV_Atlas" in bpy.data.materials:
        return bpy.data.materials["PV_Atlas"]
    if not os.path.exists(os.path.join(ATLAS, "north_atlas.png")):
        T.write(ATLAS)
    m = bpy.data.materials.new("PV_Atlas")
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.85
    bsdf.inputs["Specular IOR Level"].default_value = 0.25
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(ATLAS, "north_atlas.png"))
    tex.interpolation = "Closest" if "--closest" in ARGS else "Linear"
    nm = nt.nodes.new("ShaderNodeTexImage")
    nm.image = bpy.data.images.load(os.path.join(ATLAS, "north_atlas_n.png"))
    nm.image.colorspace_settings.name = "Non-Color"
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nmap.inputs["Strength"].default_value = 0.8
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(nm.outputs["Color"], nmap.inputs["Color"])
    nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return m


def setup_world():
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
    so.rotation_euler = (0.85, 0.0, 0.6)
    sc.collection.objects.link(so)


def flat_mat(name, rgb):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = rgb + (1,)
    m.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 1.0
    return m


def ground(size=60.0, rgb=(0.20, 0.30, 0.10)):
    me = bpy.data.meshes.new("g")
    h = size / 2
    me.from_pydata([(-h, -h, 0), (h, -h, 0), (h, h, 0), (-h, h, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("ground", me)
    me.materials.append(flat_mat("grass", rgb))
    bpy.context.scene.collection.objects.link(ob)


def capsule(x, y, z):
    """A 1.8 m capsule (r 0.22), the hero's stand-in."""
    import bmesh
    bm = bmesh.new()
    r, h = 0.22, 1.8
    seg, rings = 16, 6
    verts = []
    prof = []
    for i in range(rings + 1):
        a = -math.pi / 2 + math.pi / 2 * i / rings
        prof.append((r * math.cos(a), r + r * math.sin(a)))
    for i in range(rings + 1):
        a = math.pi / 2 * i / rings
        prof.append((r * math.cos(a), h - r + r * math.sin(a)))
    for (pr, pz) in prof:
        verts.append([bm.verts.new((x + pr * math.cos(2 * math.pi * k / seg), y + pr * math.sin(2 * math.pi * k / seg), z + pz)) for k in range(seg)])
    for j in range(len(verts) - 1):
        for k in range(seg):
            k2 = (k + 1) % seg
            try:
                bm.faces.new((verts[j][k], verts[j][k2], verts[j + 1][k2], verts[j + 1][k]))
            except ValueError:
                pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    me = bpy.data.meshes.new("cap")
    bm.to_mesh(me)
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("capsule", me)
    me.materials.append(flat_mat("capsule", (0.75, 0.35, 0.25)))
    bpy.context.scene.collection.objects.link(ob)


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.type != "MESH":
            continue
        for i, s in enumerate(o.material_slots):
            nm = s.material.name.split(".")[0] if s.material else ""
            if nm == "M_North_Atlas":
                o.material_slots[i].material = atlas_material()
    return new


def bounds(objs):
    pts = []
    for o in objs:
        if o.type == "MESH":
            pts += [o.matrix_world @ Vector(c) for c in o.bound_box]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


def camera(center, dist, az, el, lens=40):
    cam = bpy.data.cameras.new("c")
    cam.lens = lens
    cam.clip_start = 0.05
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


def render_views(name):
    sh = SHOTS.get(name, {"human": (2.0, 0.0, 0.0), "focus": (0, 0, 1), "dist": 3.0, "az": -0.6, "close_az": -0.4, "close_el": 0.3})
    out = []
    for view in VIEWS:
        K.reset_scene()
        setup_world()
        ground(rgb=sh.get("ground", (0.20, 0.30, 0.10)))
        objs = import_glb(os.path.join(EXPORT, name + ".glb"))
        mn, mx = bounds(objs)
        mn.z = max(mn.z, 0.0)
        c = (mn + mx) / 2
        ext = max((mx - mn).x, (mx - mn).y, (mx - mn).z, 2.0)
        capsule(*sh["human"])
        if view == "34":
            camera(c, ext * 1.75 + 1.0, sh["az"], 0.36)
        elif view == "front":
            camera(Vector((c.x, c.y, max(c.z, 0.9))), ext * 1.75 + 1.0, 0.0, 0.1)
        elif view == "back":
            camera(Vector((c.x, c.y, max(c.z, 0.9))), ext * 1.75 + 1.0, math.pi, 0.25)
        elif view == "inside":
            ins = sh.get("inside", ((0, 0, 1.5), 2.0, math.pi, 0.2))
            camera(Vector(ins[0]), ins[1], ins[2], ins[3], lens=24)
        elif view == "top":
            camera(c, ext * 1.6 + 1.0, 0.3, 1.2)
        elif view == "close2":
            camera(Vector(sh["focus2"]), sh["dist2"], sh["close_az"], 0.2, lens=35)
        else:
            camera(Vector(sh["focus"]), sh["dist"], sh["close_az"], sh["close_el"], lens=35)
        p = os.path.join(OUT, "%s_%s.png" % (name, view))
        render(p, RES, int(RES * 0.75))
        out.append(p)
        print("rendered", p)
    return out


def sheet(names):
    from PIL import Image, ImageDraw, ImageFont
    man = {e["file"][:-4]: e for e in json.load(open(os.path.join(EXPORT, "manifest_marks.json")))["meshes"]}
    cw, ch = 600, 450
    views = ["34", "front", "close"]
    im = Image.new("RGB", (cw * len(views), ch * len(names)), (30, 30, 30))
    d = ImageDraw.Draw(im)
    f = ImageFont.load_default(size=18)
    for r, n in enumerate(names):
        for c, v in enumerate(views):
            p = os.path.join(OUT, "%s_%s.png" % (n, v))
            if os.path.exists(p):
                im.paste(Image.open(p).convert("RGB").resize((cw, ch)), (c * cw, r * ch))
        e = man.get(n, {})
        d.text((8, r * ch + 6), "%s  %s tris  %s  collision %s" % (n, e.get("tris", "?"), e.get("size", ""), e.get("collision", "")),
               fill=(255, 255, 255), font=f, stroke_width=2, stroke_fill=(0, 0, 0))
    p = SHEET
    os.makedirs(os.path.dirname(p), exist_ok=True)
    im.save(p)
    print("sheet", p)


def main():
    skip = {ARGS.index(o) + 1 for o in ("--res", "--samples", "--views") if o in ARGS}
    names = [a for i, a in enumerate(ARGS) if not a.startswith("--") and i not in skip]
    man = json.load(open(os.path.join(EXPORT, "manifest_marks.json")))["meshes"]
    alln = [e["file"][:-4] for e in man]
    os.makedirs(OUT, exist_ok=True)
    if "--sheet-only" not in ARGS:
        for n in (names or alln):
            render_views(n)
    if "lighthouse" in (names or alln) and "--sheet-only" not in ARGS and "--views" not in ARGS:
        VIEWS[:] = ["close2"]
        render_views("lighthouse")
    if "--sheet" in ARGS or "--sheet-only" in ARGS:
        sheet(alln)


if __name__ == "__main__":
    main()
