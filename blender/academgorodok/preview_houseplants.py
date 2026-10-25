"""Preview renders of the house plants (houseplants.py).

Run headless from the worktree root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/academgorodok/preview_houseplants.py [-- --views sill,floor]

Lines the kinds up on a window sill (with a window and its light behind) and on the floor in front of it,
lights them and renders EEVEE views into renders/academgorodok/houseplants_<view>.png:
  sill    the small kinds on the sill board, seen from the room
  floor   the floor kinds in a row
  stands  trailing kinds on the iron and wicker stands, a few sizes
  seeds   three seeds of every sill kind (variety check)
  window  a library window crowded with plants, seen from outside (cf. refs/dima/shop.jpg)
Materials: the flat colours of PLANT_MATERIALS; AC_PLeafCards uses the alpha atlas; leaf materials cull back
faces (each leaf carries a reversed back copy), SMOOTH_MATERIALS are smooth-shaded.
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import geom  # noqa: E402
import houseplants as HP  # noqa: E402

RENDER_DIR = os.path.join(ROOT, "renders", "academgorodok")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
VIEWS_ON = None
if "--views" in ARGS:
    VIEWS_ON = set(ARGS[ARGS.index("--views") + 1].split(","))

SCENE_MATERIALS = {
    "PV_Wall":   ("#D8CDB0", 0.9, "plaster wall"),
    "PV_Facade": ("#E3C98E", 0.9, "ochre stucco facade"),
    "PV_Frame":  ("#E8E2CC", 0.5, "painted window frames, sill board"),
    "PV_Glass":  ("#C8D8DC", 0.05, "glass"),
    "PV_Floor":  ("#8A6038", 0.45, "floor boards"),
    "PV_Sky":    ("#BFD3E6", 1.0, "backdrop"),
    "PV_Dark":   ("#3A3530", 0.9, "dim room interior"),
}


def hex_rgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def materials():
    table = dict(HP.PLANT_MATERIALS)
    table.update(SCENE_MATERIALS)
    atlas = os.path.join(ROOT, HP.FOLIAGE_ATLAS)
    for name, (hx, rough, _) in table.items():
        mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        b = nt.nodes["Principled BSDF"]
        b.inputs["Base Color"].default_value = hex_rgb(hx)
        b.inputs["Roughness"].default_value = rough
        if name in HP.CARD_MATERIALS and os.path.exists(atlas):
            tx = nt.nodes.new("ShaderNodeTexImage")
            tx.image = bpy.data.images.load(atlas)
            nt.links.new(nt.nodes.new("ShaderNodeTexCoord").outputs["UV"], tx.inputs["Vector"])
            nt.links.new(tx.outputs["Color"], b.inputs["Base Color"])
            nt.links.new(tx.outputs["Alpha"], b.inputs["Alpha"])
            mat.surface_render_method = "DITHERED"
        if name.startswith("AC_PLeaf") or name.startswith("AC_PFlower"):
            # thin leaves let some light through
            if "Subsurface Weight" in b.inputs:
                pass
        if name == "PV_Glass":
            b.inputs["Alpha"].default_value = 0.08
            mat.surface_render_method = "BLENDED"
        if name in ("AC_PIron",):
            b.inputs["Metallic"].default_value = 0.6
        mat.use_backface_culling = name.startswith("AC_P") and name not in HP.CARD_MATERIALS


def mesh_object(name, m, coll, loc=(0, 0, 0)):
    mats = sorted(set(f[0] for f in m.faces))
    me = bpy.data.meshes.new(name)
    me.from_pydata(m.verts, [], [f[1] for f in m.faces])
    uv = me.uv_layers.new(name="UVMap")
    for poly, (mat, idx, uvs) in zip(me.polygons, m.faces):
        poly.material_index = mats.index(mat)
        for k, loop in enumerate(poly.loop_indices):
            uv.data[loop].uv = uvs[k]
    for mn in mats:
        me.materials.append(bpy.data.materials[mn])
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0003)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = me.materials[p.material_index].name in HP.SMOOTH_MATERIALS
    if hasattr(me, "set_sharp_from_angle"):
        me.set_sharp_from_angle(angle=math.radians(HP.SMOOTH_ANGLE))
    ob = bpy.data.objects.new(name, me)
    ob.location = loc
    coll.objects.link(ob)
    return ob


def label(coll, text, x, y, z, size=0.035):
    cu = bpy.data.curves.new("L_" + text, "FONT")
    cu.body = text
    cu.size = size
    cu.align_x = "CENTER"
    ob = bpy.data.objects.new("L_" + text, cu)
    ob.location = (x, y, z)
    ob.rotation_euler = (math.radians(90), 0, 0)
    ob.data.materials.append(bpy.data.materials["PV_Dark"])
    coll.objects.link(ob)


def setup_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    materials()
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    ee = scene.eevee
    for attr, val in (("taa_render_samples", 16), ("use_shadows", True), ("use_gtao", True), ("use_raytracing", False)):
        if hasattr(ee, attr):
            try:
                setattr(ee, attr, val)
            except Exception:  # noqa: BLE001
                pass
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
    scene.view_settings.view_transform = "AgX"
    for look in ("AgX - Punchy", "Punchy"):
        try:
            scene.view_settings.look = look
            break
        except Exception:  # noqa: BLE001
            pass
    world = bpy.data.worlds.new("Sky")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.6, 0.68, 0.8, 1.0)
    bg.inputs["Strength"].default_value = 0.35
    return scene


def light(scene, name, kind, loc, target, energy, size=1.0, color=(1, 1, 1)):
    li = bpy.data.lights.new(name, kind)
    li.energy = energy
    li.color = color
    if kind == "AREA":
        li.size = size
    if kind == "SUN":
        li.angle = math.radians(2.0)
    ob = bpy.data.objects.new(name, li)
    ob.location = loc
    ob.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(ob)


def shoot(scene, name, pos, target, lens):
    cam = scene.camera
    if cam is None:
        cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
        scene.collection.objects.link(cam)
        scene.camera = cam
        cam.data.sensor_width = 36.0
        cam.data.clip_start = 0.02
    cam.location = pos
    cam.rotation_euler = (Vector(target) - Vector(pos)).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = lens
    os.makedirs(RENDER_DIR, exist_ok=True)
    scene.render.filepath = os.path.join(RENDER_DIR, "houseplants_%s.png" % name)
    bpy.ops.render.render(write_still=True)
    print("HOUSEPLANTS render", scene.render.filepath)


SILL_Z = 0.85
SILL_KINDS = [k for k, v in HP.KINDS.items() if v["cls"] == "sill"]
FLOOR_KINDS = [k for k, v in HP.KINDS.items() if v["cls"] == "floor"]


def room(m, x0, x1, wall_y=0.0, win=True):
    """Floor, a back wall (at +y) with a long window band and a sill board."""
    geom.box(m, "PV_Floor", x0 - 2, x1 + 2, -6, wall_y, -0.02, 0.0)
    if win:
        geom.box(m, "PV_Wall", x0 - 2, x1 + 2, wall_y, wall_y + 0.4, 0.0, SILL_Z - 0.03)
        geom.box(m, "PV_Wall", x0 - 2, x1 + 2, wall_y, wall_y + 0.4, 2.6, 3.4)
        geom.box(m, "PV_Frame", x0 - 2, x1 + 2, wall_y - 0.12, wall_y + 0.25, SILL_Z - 0.03, SILL_Z)
        k = x0 - 2
        while k < x1 + 2:
            geom.box(m, "PV_Frame", k, k + 0.07, wall_y + 0.2, wall_y + 0.27, SILL_Z, 2.6)
            k += 1.0
        geom.box(m, "PV_Sky", x0 - 8, x1 + 8, wall_y + 6, wall_y + 6.1, -2, 8)
    else:
        geom.box(m, "PV_Wall", x0 - 2, x1 + 2, wall_y, wall_y + 0.2, 0.0, 3.4)


def row(m, kinds, x0, gap, y, z, seed=0, stand=None, labels=None, coll=None):
    x = x0
    out = []
    for k in kinds:
        r, h = HP.info(k, 1.0, stand)
        r = min(r, 0.55)
        x += r
        HP.place(m, k, x, y, z, 0.3, seed, 1.0, stand)
        if labels is not None:
            labels.append((k, x, y - 0.2 - (0.1 if z > 0.1 else 0.3), z + 0.01))
        out.append((k, x))
        x += r + gap
    return x


def group(scene, kinds, name, z=0.0, seed=0, stand=None, sizes=None):
    """A row of kinds on a sill board (z > 0) or the floor in front of a plain wall, framed close."""
    m = geom.Mesh()
    x = 0.0
    tops = []
    labels = []
    for i, k in enumerate(kinds):
        st = stand[i] if isinstance(stand, (list, tuple)) else stand
        sz = sizes[i] if sizes else 1.0
        r, h = HP.info(k, sz, st)
        w = max(0.14, min(r * 0.75, 0.45))
        x += w
        HP.place(m, k, x, 0.0, z, 0.4, seed + (i if kinds.count(k) > 1 else 0), sz, st)
        labels.append((k if not st else "%s / %s" % (k, st), x))
        tops.append(h)
        x += w + 0.04
    geom.box(m, "PV_Floor", -3, x + 3, -8, 1.0, -0.02, 0.0)
    geom.box(m, "PV_Wall", -3, x + 3, 1.0, 1.1, 0.0, 4.0)
    if z > 0:
        geom.box(m, "PV_Frame", -0.3, x + 0.3, -0.35, 0.35, z - 0.04, z)
        geom.box(m, "PV_Wall", -0.3, x + 0.3, -0.3, 0.35, 0.0, z - 0.04)
    mesh_object(name, m, scene.collection)
    for t, lx in labels:
        label(scene.collection, t, lx, -0.42 if z > 0 else -0.6, z + 0.005 if z > 0 else 0.005, 0.03 if z > 0 else 0.045)
    H = max(tops)
    cz = z + H * 0.5
    dist = max(x * 0.95, H * 1.6) + 0.3
    light(scene, "Key", "AREA", (x * 0.3, -dist * 0.8, z + H + 1.2), (x / 2, 0, cz), 60 * dist * dist, 2.5, (1.0, 0.95, 0.88))
    light(scene, "Fill", "AREA", (x * 1.1, -dist * 0.6, z + 0.5), (x / 2, 0, cz), 15 * dist * dist, 3.0, (0.85, 0.9, 1.0))
    light(scene, "Sun", "SUN", (0, 0, 5), (0.5, 0.6, 3.0), 1.5, color=(1.0, 0.93, 0.82))
    shoot(scene, name, (x / 2, -dist, cz + H * 0.25 + 0.1), (x / 2, 0.0, cz), 32)


GROUPS = {
    "sill_a": ["sansevieria", "aspidistra", "chlorophytum", "tradescantia", "geranium"],
    "sill_b": ["begonia", "african_violet", "asparagus", "aloe", "cactus"],
    "floor_a": ["ficus_elastica", "ficus_benjamina", "monstera", "palm"],
    "floor_b": ["dieffenbachia", "dracaena", "lemon"],
}


def main():
    views = VIEWS_ON or set(GROUPS) | {"stands", "seeds", "window"}
    if "--solo" in ARGS:                      # debugging: one kind, three seeds, close up
        k = ARGS[ARGS.index("--solo") + 1]
        scene = setup_scene()
        group(scene, [k, k, k], "solo_" + k, 0.0, seed=0)
        return
    for g, kinds in GROUPS.items():
        if g in views:
            scene = setup_scene()
            group(scene, kinds, g, 0.85 if g.startswith("sill") else 0.0)
    if "stands" in views:
        scene = setup_scene()
        group(scene, ["tradescantia", "chlorophytum", "asparagus", "begonia", "geranium", "tradescantia"], "stands", 0.0, seed=1,
              stand=["iron", "wicker", "iron", "wicker", "iron", "wicker"], sizes=[1.0, 1.1, 1.1, 1.0, 1.0, 1.2])
    if "seeds" in views:
        scene = setup_scene()
        m = geom.Mesh()
        xe = 0.0
        for j, seed in enumerate((1, 2, 4)):
            xe = max(xe, row(m, SILL_KINDS, 0.0, 0.0, -0.5 * j, 0.0, seed))
        room(m, 0.0, xe, win=False)
        mesh_object("seeds", m, scene.collection)
        light(scene, "Key", "AREA", (xe / 2 - 1.0, -3.0, 3.0), (xe / 2, -0.5, 0.2), 1200, 4.0, (1.0, 0.93, 0.85))
        light(scene, "Sun", "SUN", (0, 0, 5), (0.5, -1.0, 4.0), 2.5)
        shoot(scene, "seeds", (xe / 2, -xe * 0.62, 1.9), (xe / 2, -0.5, 0.15), 32)
    if "window" in views:
        scene = setup_scene()
        coll = scene.collection
        m = geom.Mesh()
        # facade (y=0 plane, outside at -y) with a big shop-style window 2.4 x 2.0 over a 0.9 m sill
        W0, W1, S, T = 0.0, 2.4, 0.9, 2.9
        geom.box(m, "PV_Facade", -1.5, W0, 0.0, 0.5, 0.0, 4.0)
        geom.box(m, "PV_Facade", W1, 4.0, 0.0, 0.5, 0.0, 4.0)
        geom.box(m, "PV_Facade", W0, W1, 0.0, 0.5, 0.0, S)
        geom.box(m, "PV_Facade", W0, W1, 0.0, 0.5, T, 4.0)
        for bx in ((W0 - 0.16, W0, S - 0.16, T + 0.16), (W1, W1 + 0.16, S - 0.16, T + 0.16),
                   (W0, W1, S - 0.16, S), (W0, W1, T, T + 0.16)):          # white stucco surround
            geom.box(m, "PV_Frame", bx[0], bx[1], -0.03, 0.0, bx[2], bx[3])
        for xx in (W0, W1 / 2 - 0.03, W1 - 0.06):
            geom.box(m, "PV_Frame", xx, xx + 0.06, 0.26, 0.32, S, T)
        for zz in (S, 1.85, T - 0.45, T - 0.06):
            geom.box(m, "PV_Frame", W0, W1, 0.26, 0.32, zz, zz + 0.06)
        geom.box(m, "PV_Glass", W0, W1, 0.28, 0.285, S, T)
        geom.box(m, "PV_Frame", W0 - 0.05, W1 + 0.05, 0.3, 0.75, S - 0.03, S)          # inner sill board
        geom.box(m, "PV_Floor", -1.5, 4.0, 0.5, 6.0, -0.02, 0.0)
        geom.box(m, "PV_Dark", -1.5, 4.0, 4.0, 4.1, 0.0, 4.0)
        geom.box(m, "PV_Floor", -3, 6, -6, 0.0, -0.32, -0.3)
        # on the sill: a crowd of small plants; behind it, floor plants rising above the sill
        sill = [("geranium", 0.18), ("aloe", 0.45), ("cactus", 0.66), ("african_violet", 0.82), ("sansevieria", 1.05),
                ("begonia", 1.35), ("asparagus", 1.6), ("geranium", 1.85), ("chlorophytum", 2.15)]
        for i, (k, x) in enumerate(sill):
            HP.place(m, k, x, 0.5, S, i * 0.7, i + 3, 0.95)
        floor = [("palm", 0.4, 1.25), ("dracaena", 1.25, 1.2), ("ficus_benjamina", 2.0, 1.3), ("monstera", 1.6, 1.9),
                 ("ficus_elastica", 0.9, 1.9)]
        for i, (k, x, y) in enumerate(floor):
            HP.place(m, k, x, y, 0.0, i * 1.1, i + 7, 1.0)
        mesh_object("window", m, coll)
        light(scene, "Sun", "SUN", (0, 0, 5), (0.6, 1.0, 3.6), 3.0, color=(1.0, 0.92, 0.8))
        light(scene, "Room", "AREA", (1.2, 3.0, 2.8), (1.2, 0.5, 1.0), 700, 3.0, (1.0, 0.9, 0.75))
        shoot(scene, "window", (0.6, -4.2, 1.4), (1.2, 0.6, 1.7), 32)
        shoot(scene, "window_inside", (1.2, 3.8, 1.6), (1.2, 0.3, 1.2), 26)


main()
