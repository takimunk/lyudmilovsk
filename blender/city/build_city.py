"""Builds the town buildings of Lyudmilovsk (blender/city/buildings/<type>.py) into game meshes.

    blender -b -P blender/city/build_city.py -- [types,comma,separated | all] [--no-render] [--samples N] [--res WxH]
    (or with pip's bpy:  python3 blender/city/build_city.py -- capsule_tower)

Writes per type:
    blender/export/city/<type>.glb    SM_<type> (shell, collision), SM_<type>_Detail (no collision), SM_<type>_Windows
                                      (window cards: UV0 over the pane, UV1 = the light texel)
    blender/export/city/<type>.json   footprint, entrances, spots, flats, windows (the life thread's contract), tris
    renders/city/<type>_day.jpg, <type>_night.jpg   preview renders (unless --no-render)
and prints a lint (triangles, UV stretch, degenerate faces, coplanar fights fixed).
"""
import importlib
import json
import math
import os
import random
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "buildings"))
import citykit  # noqa: E402
import citymats  # noqa: E402
import geom  # noqa: E402

EXPORT = os.path.join(ROOT, "blender", "export", "city")
RENDERS = os.path.join(ROOT, "renders", "city")
TEX_DIRS = [os.path.join(ROOT, "textures", "city"), os.path.join(ROOT, "textures", "academ")]
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def opt(name, default):
    if name in ARGS:
        return ARGS[ARGS.index(name) + 1]
    return default


DO_RENDER = "--no-render" not in ARGS
SAMPLES = int(opt("--samples", 48))
RES = tuple(int(v) for v in opt("--res", "1280x800").split("x"))
TEXTURED = {k for k, v in citymats.MATERIALS.items() if v[2]}


def all_types():
    d = os.path.join(HERE, "buildings")
    return sorted(f[:-3] for f in os.listdir(d) if f.endswith(".py") and not f.startswith("_"))


# ------------------------------------------------------------------------------------------------ lint
def lint(B):
    out = []
    moved = geom.separate_coplanar(B.m)
    moved += geom.separate_coplanar(B.detail)
    if moved:
        out.append("coplanar faces lifted: %d" % moved)
    for label, m in (("shell", B.m), ("detail", B.detail), ("windows", B.win)):
        bad_uv = degenerate = 0
        for mat, idx, uv in m.faces:
            pts = [m.verts[i] for i in idx]
            a3 = 0.0
            a2 = 0.0
            for k in range(1, len(pts) - 1):
                c = geom.cross(geom.sub(pts[k], pts[0]), geom.sub(pts[k + 1], pts[0]))
                a3 += math.sqrt(c[0] ** 2 + c[1] ** 2 + c[2] ** 2) / 2
                a2 += abs((uv[k][0] - uv[0][0]) * (uv[k + 1][1] - uv[0][1]) - (uv[k + 1][0] - uv[0][0]) * (uv[k][1] - uv[0][1])) / 2
            if a3 < 1e-6:
                degenerate += 1
                continue
            if mat in TEXTURED and a3 > 0.01 and not (0.6 < a2 / a3 < 1.6):
                bad_uv += 1
        out.append("%s: %d tris, %d faces stretched UV, %d degenerate" % (label, m.tri_count(), bad_uv, degenerate))
    flats = [f for f in B.flats.values()]
    nowin = [f["flat"] for f in flats if not f["windows"]]
    out.append("windows %d (flat %d), flats %d%s, entrances %d" % (
        len(B.windows), sum(1 for w in B.windows if w["flat"]), len(flats),
        (" WITHOUT windows: " + ",".join(nowin)) if nowin else "", len(B.entrances)))
    if len(B.windows) > citykit.LIGHT_TEX ** 2:
        out.append("ERROR: more windows than light slots")
    return out


# ------------------------------------------------------------------------------------------------ bpy
def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


_MATS = {}


def _tex(name, suffix):
    for d in TEX_DIRS:
        p = os.path.join(d, "%s_%s.jpg" % (name, suffix))
        if os.path.exists(p):
            return p
    return None


def material(slot, night=False):
    key = (slot, night)
    if key in _MATS:
        return _MATS[key]
    hx, rough, tex, tile, _ = citymats.MATERIALS.get(slot, ("#FF00FF", 0.5, None, 1.0, "?"))
    mat = bpy.data.materials.new(slot)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    col = tuple(int(hx[i:i + 2], 16) / 255.0 for i in (1, 3, 5))
    lin = tuple(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in col) + (1.0,)
    bsdf.inputs["Base Color"].default_value = lin
    bsdf.inputs["Roughness"].default_value = rough
    if "Steel" in slot or slot == "CT_Aluminium":
        bsdf.inputs["Metallic"].default_value = 0.6 if slot == "CT_Aluminium" else 0.2
    d = _tex(tex, "D") if tex else None
    if d:
        uv = nt.nodes.new("ShaderNodeUVMap")
        uv.uv_map = "UV0"
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (1 / tile, 1 / tile, 1)
        img = nt.nodes.new("ShaderNodeTexImage")
        img.image = bpy.data.images.load(d, check_existing=True)
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs[6].default_value = tuple(min(1.0, c * 2.2) for c in lin[:3]) + (1,)
        nt.links.new(uv.outputs["UV"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], img.inputs["Vector"])
        # the academ textures are coloured: desaturate to a grain and tint by the slot colour
        bw = nt.nodes.new("ShaderNodeRGBToBW")
        nt.links.new(img.outputs["Color"], bw.inputs["Color"])
        nt.links.new(bw.outputs["Val"], mix.inputs[6 + 1] if False else mix.inputs[7])
        nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    if slot == "CT_Window":
        bsdf.inputs["Roughness"].default_value = 0.04
        bsdf.inputs["Metallic"].default_value = 0.0
        attr = nt.nodes.new("ShaderNodeAttribute")
        attr.attribute_name = "lit"
        attr.attribute_type = "GEOMETRY"
        em = nt.nodes.new("ShaderNodeMath")
        em.operation = "MULTIPLY"
        em.inputs[1].default_value = 6.0 if night else 0.0
        nt.links.new(attr.outputs["Fac"], em.inputs[0])
        bsdf.inputs["Emission Color"].default_value = (1.0, 0.62, 0.30, 1)
        nt.links.new(em.outputs[0], bsdf.inputs["Emission Strength"])
        # round windows (UV0 x in 2..3): cut the circle, as the game shader does
        uvn = nt.nodes.new("ShaderNodeUVMap")
        uvn.uv_map = "UV0"
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(uvn.outputs["UV"], sep.inputs[0])
        fx = nt.nodes.new("ShaderNodeMath"); fx.operation = "FRACT"
        nt.links.new(sep.outputs[0], fx.inputs[0])
        cmb = nt.nodes.new("ShaderNodeCombineXYZ")
        nt.links.new(fx.outputs[0], cmb.inputs[0]); nt.links.new(sep.outputs[1], cmb.inputs[1])
        dist = nt.nodes.new("ShaderNodeVectorMath"); dist.operation = "DISTANCE"
        dist.inputs[1].default_value = (0.5, 0.5, 0.0)
        nt.links.new(cmb.outputs[0], dist.inputs[0])
        inside = nt.nodes.new("ShaderNodeMath"); inside.operation = "LESS_THAN"; inside.inputs[1].default_value = 0.5
        nt.links.new(dist.outputs["Value"], inside.inputs[0])
        isround = nt.nodes.new("ShaderNodeMath"); isround.operation = "GREATER_THAN"; isround.inputs[1].default_value = 1.5
        nt.links.new(sep.outputs[0], isround.inputs[0])
        notr = nt.nodes.new("ShaderNodeMath"); notr.operation = "SUBTRACT"; notr.inputs[0].default_value = 1.0
        nt.links.new(isround.outputs[0], notr.inputs[1])
        alpha = nt.nodes.new("ShaderNodeMath"); alpha.operation = "MAXIMUM"
        nt.links.new(inside.outputs[0], alpha.inputs[0]); nt.links.new(notr.outputs[0], alpha.inputs[1])
        nt.links.new(alpha.outputs[0], bsdf.inputs["Alpha"])
    if slot == "CT_Neon" and night:
        bsdf.inputs["Emission Color"].default_value = lin
        bsdf.inputs["Emission Strength"].default_value = 8.0
    _MATS[key] = mat
    return mat


def to_object(name, m, slots=None, lit=None, night=False):
    """geom.Mesh -> a Blender object; slots: per face light slot (UV1 = its texel); lit: set of lit slots."""
    me = bpy.data.meshes.new(name)
    verts, faces = [], []
    for _, idx, _ in m.faces:
        faces.append([len(verts) + k for k in range(len(idx))])
        verts.extend(m.verts[i] for i in idx)
    me.from_pydata(verts, [], faces)
    mats = sorted({mt for mt, _, _ in m.faces})
    for mt in mats:
        me.materials.append(material(mt, night))
    mi = {mt: i for i, mt in enumerate(mats)}
    uv0 = me.uv_layers.new(name="UV0")
    uv1 = me.uv_layers.new(name="UV1") if slots is not None else None
    litattr = me.attributes.new("lit", "FLOAT", "FACE") if slots is not None else None
    for p, (mt, idx, uv) in zip(me.polygons, m.faces):
        p.material_index = mi[mt]
        p.use_smooth = False
        for k, li in enumerate(p.loop_indices):
            uv0.data[li].uv = uv[k]
        if uv1 is not None:
            s = slots[p.index]
            t = citykit.slot_uv(s)
            for li in p.loop_indices:
                uv1.data[li].uv = t
            litattr.data[p.index].value = 1.0 if (lit and s in lit) else 0.0
    me.validate()
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def export(type_id, obs):
    os.makedirs(EXPORT, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in obs:
        o.select_set(True)
    path = os.path.join(EXPORT, type_id + ".glb")
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_yup=True,
                              export_apply=False, export_materials="PLACEHOLDER", export_attributes=False)
    return path


# ------------------------------------------------------------------------------------------------ render
def render(type_id, B, night):
    scn = bpy.context.scene
    scn.render.engine = "CYCLES"
    scn.cycles.device = "CPU"
    scn.cycles.samples = SAMPLES
    scn.cycles.use_denoising = True
    scn.render.resolution_x, scn.render.resolution_y = RES
    scn.render.image_settings.file_format = "JPEG"
    scn.render.image_settings.quality = 88
    scn.view_settings.view_transform = "AgX"
    scn.view_settings.exposure = 0.0 if night else -1.6
    world = bpy.data.worlds.new("w")
    scn.world = world
    world.use_nodes = True
    sky = world.node_tree.nodes.new("ShaderNodeTexSky")
    try:
        sky.sky_type = "NISHITA"
    except TypeError:
        pass
    try:
        sky.sun_elevation = math.radians(-4 if night else 28)
        sky.sun_rotation = math.radians(215)
    except AttributeError:
        pass
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Strength"].default_value = 0.35 if night else 0.9
    world.node_tree.links.new(sky.outputs["Color"], bg.inputs["Color"])
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 0.0 if night else 3.2
    sun.data.angle = math.radians(1.0)
    sun.rotation_euler = (math.radians(62), 0, math.radians(35))
    scn.collection.objects.link(sun)
    if night:
        moon = bpy.data.objects.new("moon", bpy.data.lights.new("moon", "SUN"))
        moon.data.energy = 0.08
        moon.data.color = (0.6, 0.7, 1.0)
        moon.rotation_euler = (math.radians(50), 0, math.radians(-120))
        scn.collection.objects.link(moon)
    # ground
    me = bpy.data.meshes.new("ground")
    s = 400
    me.from_pydata([(-s, -s, -0.02), (s, -s, -0.02), (s, s, -0.02), (-s, s, -0.02)], [], [[0, 1, 2, 3]])
    g = bpy.data.objects.new("ground", me)
    me.materials.append(material("CT_Paving"))
    scn.collection.objects.link(g)
    # camera: 3/4 from the street side, framing the bounding box
    xs = [v[0] for v in B.m.verts] or [0]
    ys = [v[1] for v in B.m.verts] or [0]
    zs = [v[2] for v in B.m.verts] or [0]
    c = Vector(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, (min(zs) + max(zs)) / 2))
    r = max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)) * 0.5 + 2
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.lens = 32
    d = Vector((-0.55, -1.0, 0.30)).normalized()
    cam.location = c + d * r * 3.1
    cam.location.z = max(cam.location.z, 1.7)
    look = (c - cam.location).normalized()
    cam.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()
    scn.collection.objects.link(cam)
    scn.camera = cam
    os.makedirs(RENDERS, exist_ok=True)
    scn.render.filepath = os.path.join(RENDERS, "%s_%s.jpg" % (type_id, "night" if night else "day"))
    bpy.ops.render.render(write_still=True)
    return scn.render.filepath


def build_one(type_id):
    mod = importlib.import_module(type_id)
    importlib.reload(mod)
    B = citykit.Building(type_id)
    mod.build(B)
    report = lint(B)
    info = getattr(mod, "INFO", {})
    os.makedirs(EXPORT, exist_ok=True)
    with open(os.path.join(EXPORT, type_id + ".json"), "w") as f:
        json.dump(B.data(info), f, indent=1, ensure_ascii=False)
    results = {}
    for night in ([False, True] if DO_RENDER else [False]):
        clear_scene()
        _MATS.clear()
        rnd = random.Random(7)
        lit = {w["slot"] for w in B.windows if rnd.random() < (0.45 if w["flat"] else 0.8)} if night else set()
        obs = [to_object("SM_" + type_id, B.m, night=night)]
        if B.detail.faces:
            obs.append(to_object("SM_%s_Detail" % type_id, B.detail, night=night))
        if B.win.faces:
            obs.append(to_object("SM_%s_Windows" % type_id, B.win, slots=B.win_slot, lit=lit, night=night))
        if not night:
            results["glb"] = export(type_id, obs)
        if DO_RENDER:
            results["night" if night else "day"] = render(type_id, B, night)
    print("CITY %s" % type_id)
    for line in report:
        print("CITY   " + line)
    for k, v in results.items():
        print("CITY   %s %s" % (k, os.path.relpath(v, ROOT)))
    return report


def main():
    names = [a for a in ARGS if not a.startswith("--") and not (ARGS.index(a) > 0 and ARGS[ARGS.index(a) - 1] in ("--samples", "--res"))]
    types = all_types() if not names or names[0] == "all" else names[0].split(",")
    for t in types:
        build_one(t)


if __name__ == "__main__":
    main()
