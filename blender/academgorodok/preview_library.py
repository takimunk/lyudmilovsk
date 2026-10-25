"""Preview renders of the library on the ground floor of Novikova 1 (library.py).

Run headless from the worktree root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/academgorodok/preview_library.py [-- --views=a,b]

Builds the interior mesh, a stand-in shell (the east wall with its glazing and door leaves, slabs,
the ground outside), lights the pendants and reading lamps from library.LIGHTS plus a morning sun
from the east, and renders EEVEE views into renders/academgorodok/library_<view>.png. Materials are
preview stand-ins (flat colours, procedural parquet/tile grids in metre UVs, the picture images,
the house plants' leaf-card atlas); back faces are culled as in Unreal.
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
import common  # noqa: E402
import furniture as F  # noqa: E402
import geom  # noqa: E402
import library as L  # noqa: E402

RENDER_DIR = os.path.join(ROOT, "renders", "academgorodok")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

# view: (camera position, target, focal length mm on a 36 mm sensor) - house frame
VIEWS = {
    # from just inside the entrance, past the issue desk, through the arch into the reading room
    "entrance": ((-0.3, -6.35, 1.65), (6.5, -3.0, 1.55), 18),
    # from the arch end of the hall along the reading tables to the plant-filled windows
    "tables": ((4.3, -2.55, 1.95), (16.0, -6.1, 1.15), 19),
    # the vestibule from beside the arch: issue desk, card catalogues, cloakroom
    "desk": ((2.2, -3.0, 1.7), (-1.6, -4.9, 1.05), 17),
    # close-up of the middle window: sill plants, the reading table's end
    "window": ((11.45, -4.95, 1.75), (12.6, -6.95, 1.5), 22),
}
EXTRA_VIEWS = {
    # down the back aisle between the stacks and the back-wall bookcases
    "stacks": ((17.15, -2.3, 1.7), (4.0, 0.7, 1.6), 20),
}


def hex_rgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def _brick(nt, w, h, mortar, c1, c2, cm, offset=0.5, bias=0.0):
    tc = nt.nodes.new("ShaderNodeTexCoord")
    br = nt.nodes.new("ShaderNodeTexBrick")
    br.offset = offset
    br.offset_frequency = 1
    br.squash = 1.0
    br.inputs["Scale"].default_value = 1.0
    br.inputs["Brick Width"].default_value = w
    br.inputs["Row Height"].default_value = h
    br.inputs["Mortar Size"].default_value = mortar
    br.inputs["Bias"].default_value = bias
    br.inputs["Color1"].default_value = c1
    br.inputs["Color2"].default_value = c2
    br.inputs["Mortar"].default_value = cm
    nt.links.new(tc.outputs["UV"], br.inputs["Vector"])
    return br


def materials():
    hp = L.houseplants()
    table = dict(L.LIBRARY_MATERIALS)
    for k in ("AC_Door", "AC_Frame", "AC_Glass", "AC_Wall", "AC_Trim", "AC_Asphalt", "AC_Plinth"):
        table[k] = common.MATERIALS[k]
    table.update({k: v for k, v in F.FURNITURE_MATERIALS.items() if k.startswith("AC_FPaint_")})
    cards = set(getattr(hp, "CARD_MATERIALS", set())) if hp else set()
    atlas = os.path.join(ROOT, hp.FOLIAGE_ATLAS) if hp else ""
    for name, (hx, rough, _) in table.items():
        mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        b = nt.nodes["Principled BSDF"]
        col = hex_rgb(hx)
        b.inputs["Base Color"].default_value = col
        b.inputs["Roughness"].default_value = rough
        mat.use_backface_culling = True
        if name == "AC_LParquet":
            br = _brick(nt, 0.4, 0.07, 0.002, col, hex_rgb("#77492A"), hex_rgb("#4A3020"))
            nt.links.new(br.outputs["Color"], b.inputs["Base Color"])
        elif name == "AC_LStone":
            br = _brick(nt, 0.15, 0.15, 0.003, col, hex_rgb("#9C4B32"), hex_rgb("#6B5E50"), offset=0.0, bias=-0.4)
            nt.links.new(br.outputs["Color"], b.inputs["Base Color"])
        elif name in ("AC_LBaize", "AC_LCarpet", "AC_LWall", "AC_LCork"):
            noise = nt.nodes.new("ShaderNodeTexNoise")
            noise.inputs["Scale"].default_value = 40.0
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.inputs["Factor"].default_value = 0.12
            mix.inputs["A"].default_value = col
            nt.links.new(noise.outputs["Color"], mix.inputs["B"])
            nt.links.new(mix.outputs["Result"], b.inputs["Base Color"])
        elif name in ("AC_LGlass", "AC_Glass"):
            b.inputs["Alpha"].default_value = 0.1 if name == "AC_LGlass" else 0.06
            mat.surface_render_method = "BLENDED"
            mat.use_backface_culling = False
        elif name in ("AC_LBrass", "AC_LChrome"):
            b.inputs["Metallic"].default_value = 1.0
        elif name == "AC_LOpal":
            b.inputs["Emission Color"].default_value = hex_rgb(L.LIGHT_COLOUR)
            b.inputs["Emission Strength"].default_value = 5.0
        elif name == "AC_LGreenGlass":
            b.inputs["Emission Color"].default_value = hex_rgb("#3E8A50")
            b.inputs["Emission Strength"].default_value = 0.6
        elif name in F.PAINTINGS:
            path = os.path.join(ROOT, F.PAINTINGS[name])
            if os.path.exists(path):
                tx = nt.nodes.new("ShaderNodeTexImage")
                tx.image = bpy.data.images.load(path)
                nt.links.new(nt.nodes.new("ShaderNodeTexCoord").outputs["UV"], tx.inputs["Vector"])
                nt.links.new(tx.outputs["Color"], b.inputs["Base Color"])
        elif name in cards and os.path.exists(atlas):
            tx = nt.nodes.new("ShaderNodeTexImage")
            tx.image = bpy.data.images.load(atlas)
            nt.links.new(nt.nodes.new("ShaderNodeTexCoord").outputs["UV"], tx.inputs["Vector"])
            nt.links.new(tx.outputs["Color"], b.inputs["Base Color"])
            nt.links.new(tx.outputs["Alpha"], b.inputs["Alpha"])
            mat.surface_render_method = "DITHERED"
            mat.use_backface_culling = False


def mesh_object(name, m, coll, smooth=frozenset(), angle=50.0):
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
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0004)
    bm.to_mesh(me)
    bm.free()
    if smooth:
        for p in me.polygons:
            p.use_smooth = me.materials[p.material_index].name in smooth
        if hasattr(me, "set_sharp_from_angle"):
            me.set_sharp_from_angle(angle=math.radians(angle))
    return ob


def shell():
    """Stand-in for what plan.py builds: the east wall outside (with glazing and door leaves), slabs,
    backing boxes behind the inner walls, the ground outside."""
    m = geom.Mesh()
    x0, x1 = -3.0, 18.5
    w = L._Wall((x1, -7.25), (0.0, -1.0))
    holes = [(x1 - cx - wd / 2, x1 - cx + wd / 2, sill, sill + ht) for cx, sill, wd, ht in [L.ENTRANCE, L.HALL_DOOR] + L.SHOP_WINDOWS]
    loc = geom.Mesh()
    geom.wall_with_holes(loc, "AC_Wall", x1 - x0, 0.0, L.ZC + 0.3, holes)
    m.extend(loc.transformed(w))
    for cx, sill, wd, ht in L.SHOP_WINDOWS:
        geom.box(m, "AC_Glass", cx - wd / 2, cx + wd / 2, -7.13, -7.125, sill, sill + ht)
        geom.box(m, "AC_Frame", cx - wd / 2, cx + wd / 2, -7.17, -7.08, sill + ht - 0.62, sill + ht - 0.55)
        geom.box(m, "AC_Frame", cx - 0.045, cx + 0.045, -7.17, -7.08, sill, sill + ht - 0.62)
        geom.box(m, "AC_Frame", cx - wd / 2, cx + wd / 2, -7.17, -7.08, sill + 0.9, sill + 0.97)
        for e in (cx - wd / 2, cx + wd / 2 - 0.09):
            geom.box(m, "AC_Frame", e, e + 0.09, -7.17, -7.08, sill, sill + ht)
        geom.box(m, "AC_Frame", cx - wd / 2, cx + wd / 2, -7.17, -7.08, sill, sill + 0.09)
        geom.box(m, "AC_Frame", cx - wd / 2, cx + wd / 2, -7.17, -7.08, sill + ht - 0.09, sill + ht)
    for cx, sill, wd, ht in (L.ENTRANCE, L.HALL_DOOR):
        for s in (-1, 1):
            a, b = (cx - wd / 2, cx) if s < 0 else (cx, cx + wd / 2)
            geom.box(m, "AC_Door", a, b, -7.1, -7.05, sill, sill + 1.1)
            geom.box(m, "AC_Door", a, b, -7.1, -7.05, sill + ht - 0.5, sill + ht)
            geom.box(m, "AC_Door", a, a + 0.1, -7.1, -7.05, sill + 1.1, sill + ht - 0.5)
            geom.box(m, "AC_Door", b - 0.1, b, -7.1, -7.05, sill + 1.1, sill + ht - 0.5)
            geom.box(m, "AC_Glass", a + 0.1, b - 0.1, -7.08, -7.075, sill + 1.1, sill + ht - 0.5)
    geom.box(m, "AC_Trim", x0, x1, -7.25, 2.0, L.ZC + 0.001, L.ZC + 0.3)
    geom.box(m, "AC_Plinth", x0, x1, -7.25, 2.0, 0.3, L.Z0 - 0.001)
    geom.box(m, "AC_Trim", x0, -2.65, -7.25, 2.0, L.Z0 - 0.01, L.ZC + 0.01)
    geom.box(m, "AC_Trim", 18.2, x1 + 0.2, -7.25, 2.0, L.Z0 - 0.01, L.ZC + 0.01)
    geom.box(m, "AC_Trim", x0, x1, 1.7, 2.2, L.Z0 - 0.01, L.ZC + 0.01)
    geom.box(m, "AC_Trim", -2.44, 2.44, -1.3, 1.5, L.Z0 - 0.01, L.ZC + 0.01)
    m.face("AC_Asphalt", [(-30, -40, 0.0), (40, -40, 0.0), (40, -7.25, 0.0), (-30, -7.25, 0.0)])
    return m


def setup_scene():
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    ee = scene.eevee
    for attr, val in (("taa_render_samples", 32), ("use_raytracing", True), ("use_shadows", True),
                      ("use_gtao", True), ("shadow_pool_size", "2048")):
        if hasattr(ee, attr):
            try:
                setattr(ee, attr, val)
            except Exception:  # noqa: BLE001
                pass
    if hasattr(ee, "use_fast_gi"):
        ee.use_fast_gi = True
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.exposure = float(os.environ.get("LIB_EXPOSURE", "-0.1"))
    world = bpy.data.worlds.new("Sky")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.55, 0.68, 0.9, 1.0)
    bg.inputs["Strength"].default_value = 0.8
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.0
    sun.data.angle = math.radians(1.0)
    sun.rotation_euler = Vector((0.4, 1.0, -0.5)).normalized().to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(sun)
    for i, (x, y, z, hx, lm) in enumerate(L.LIGHTS):
        li = bpy.data.lights.new("Lamp%d" % i, "POINT")
        li.energy = lm / 25.0
        li.color = hex_rgb(hx)[:3]
        li.shadow_soft_size = 0.1
        ob = bpy.data.objects.new("Lamp%d" % i, li)
        below = L.LAMP_R + 0.06 if z > L.Z0 + 2.0 else 0.0     # pendants: just under the globe
        ob.location = (x, y, z - below)
        scene.collection.objects.link(ob)
    for i, (cx, sill, wd, ht) in enumerate(L.SHOP_WINDOWS + [L.ENTRANCE, L.HALL_DOOR]):
        li = bpy.data.lights.new("Win%d" % i, "AREA")
        li.shape = "RECTANGLE"
        li.size, li.size_y = wd, ht
        li.energy = 150.0 if i < 3 else 40.0
        li.color = (0.85, 0.9, 1.0)
        ob = bpy.data.objects.new("Win%d" % i, li)
        ob.location = (cx, -6.95, sill + ht / 2)
        ob.rotation_euler = Vector((0, 1, 0)).to_track_quat("-Z", "Y").to_euler()
        scene.collection.objects.link(ob)
    return scene


def render(scene, names):
    os.makedirs(RENDER_DIR, exist_ok=True)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.sensor_width = 36.0
    cam.data.clip_start = 0.05
    allv = dict(VIEWS, **EXTRA_VIEWS)
    for name in names:
        pos, target, lens = allv[name]
        cam.location = pos
        cam.rotation_euler = (Vector(target) - Vector(pos)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        scene.render.filepath = os.path.join(RENDER_DIR, "library_%s.png" % name)
        bpy.ops.render.render(write_still=True)
        print("LIBRARY render", scene.render.filepath)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    m = L.build()           # also merges houseplants' materials into LIBRARY_MATERIALS
    materials()
    coll = bpy.data.collections.new("Library")
    bpy.context.scene.collection.children.link(coll)
    print("LIBRARY triangles", m.tri_count(), "houseplants", bool(L.houseplants()))
    hp = L.houseplants()
    smooth = set(L.SMOOTH_MATERIALS) | set(getattr(hp, "SMOOTH_MATERIALS", set()) if hp else set())
    mesh_object("Library", m, coll, frozenset(smooth))
    mesh_object("Shell", shell(), coll)
    scene = setup_scene()
    names = list(VIEWS)
    for a in ARGS:
        if a.startswith("--views"):
            names = a.split("=", 1)[1].split(",") if "=" in a else names
    render(scene, names)


main()
