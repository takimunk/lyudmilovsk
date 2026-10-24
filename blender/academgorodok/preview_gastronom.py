"""Preview renders of the Gastronom No. 7 interior (gastronom.py).

Run headless from the worktree root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/academgorodok/preview_gastronom.py [-- --views a,b]

Builds the interior mesh, a simple shell (the east wall with its shop windows, door and entrance,
slabs, the ground outside), lights the pendants from gastronom.LIGHTS plus east-morning daylight,
and renders EEVEE views matched roughly to the museum photos into renders/academgorodok/gastronom_<view>.png.
Materials here are preview stand-ins (flat colours + procedural tile/slab grids in metre UVs).
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
import geom  # noqa: E402
import gastronom as G  # noqa: E402

RENDER_DIR = os.path.join(ROOT, "renders", "academgorodok")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

# view: (camera position, target, focal length mm on a 36 mm sensor) - house frame
VIEWS = {
    # commons_..._09 / _10: from the hall's south end along the counter, cases on the left
    "hall": ((4.0, -5.2, 2.25), (13.5, 0.2, 2.15), 20),
    # m24_2015_museum_09: the service aisle behind the counter, cases on the right
    "service": ((17.65, 0.3, 2.1), (4.0, 0.0, 1.85), 18),
    # m24_2016_shop_03: from the vestibule through the arch
    "arch": ((-2.1, -4.3, 2.2), (10.0, -3.4, 2.3), 22),
    # m24_2015_museum_05: from the north end back towards the arch and the windows
    "north": ((17.6, -6.3, 2.5), (4.5, -1.8, 1.9), 18),
    # the vestibule's entrance from inside: threshold, reveal liners, the doors
    "entrance": ((1.8, -2.6, 2.2), (-0.3, -7.25, 1.5), 22),
}


def hex_rgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def _uv_brick(nt, size_w, size_h, mortar, c1, c2, cm, offset=0.0):
    tc = nt.nodes.new("ShaderNodeTexCoord")
    br = nt.nodes.new("ShaderNodeTexBrick")
    br.offset = offset
    br.offset_frequency = 1
    br.squash = 1.0
    br.inputs["Scale"].default_value = 1.0
    br.inputs["Brick Width"].default_value = size_w
    br.inputs["Row Height"].default_value = size_h
    br.inputs["Mortar Size"].default_value = mortar
    br.inputs["Mortar Smooth"].default_value = 0.1
    br.inputs["Bias"].default_value = 0.0
    br.inputs["Color1"].default_value = c1
    br.inputs["Color2"].default_value = c2
    br.inputs["Mortar"].default_value = cm
    nt.links.new(tc.outputs["UV"], br.inputs["Vector"])
    return br, tc


def materials():
    table = dict(G.GASTRONOM_MATERIALS)
    for k in ("AC_Door", "AC_Frame", "AC_Glass", "AC_Wall", "AC_Trim", "AC_Asphalt", "AC_Plinth"):
        table[k] = common.MATERIALS[k]
    for name, (hx, rough, _) in table.items():
        mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        b = nt.nodes["Principled BSDF"]
        col = hex_rgb(hx)
        b.inputs["Base Color"].default_value = col
        b.inputs["Roughness"].default_value = rough
        if name in ("AC_GTileBlue", "AC_GTileIvory"):
            c2 = [min(1.0, c * 1.05) for c in col[:3]] + [1.0]
            br, _ = _uv_brick(nt, 0.15, 0.15, 0.004, col, c2, hex_rgb("#5C5A55"))
            br.inputs["Bias"].default_value = -0.3
            nt.links.new(br.outputs["Color"], b.inputs["Base Color"])
        elif name == "AC_GFloor":
            br, tc = _uv_brick(nt, 0.4, 0.4, 0.003, col, hex_rgb("#C9BCAA"), hex_rgb("#8C847A"))
            br.inputs["Bias"].default_value = 0.0
            wave = nt.nodes.new("ShaderNodeTexWave")
            wave.inputs["Scale"].default_value = 1.2
            wave.inputs["Distortion"].default_value = 4.0
            wave.inputs["Detail"].default_value = 3.0
            nt.links.new(tc.outputs["UV"], wave.inputs["Vector"])
            ramp = nt.nodes.new("ShaderNodeValToRGB")
            ramp.color_ramp.elements[0].position = 0.7
            ramp.color_ramp.elements[0].color = (1, 1, 1, 1)
            ramp.color_ramp.elements[1].position = 0.9
            ramp.color_ramp.elements[1].color = hex_rgb("#8A7E70")
            nt.links.new(wave.outputs["Fac"], ramp.inputs["Fac"])
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.blend_type = "MULTIPLY"
            mix.inputs["Factor"].default_value = 0.6
            nt.links.new(br.outputs["Color"], mix.inputs["A"])
            nt.links.new(ramp.outputs["Color"], mix.inputs["B"])
            nt.links.new(mix.outputs["Result"], b.inputs["Base Color"])
        elif name in ("AC_GGlass", "AC_Glass"):
            b.inputs["Alpha"].default_value = 0.12 if name == "AC_GGlass" else 0.08
            b.inputs["IOR"].default_value = 1.5
            mat.surface_render_method = "BLENDED"
            mat.use_backface_culling = False
        elif name in ("AC_GMirror", "AC_GChrome", "AC_GBrass", "AC_GTin"):
            b.inputs["Metallic"].default_value = 1.0
        elif name == "AC_GOpal":
            b.inputs["Emission Color"].default_value = hex_rgb(G.LIGHT_COLOUR)
            b.inputs["Emission Strength"].default_value = 6.0
        elif name in ("AC_GBottleGreen", "AC_GBottleBrown"):
            b.inputs["Alpha"].default_value = 0.75
            mat.surface_render_method = "BLENDED"


def mesh_object(name, m, coll):
    """geom.Mesh -> bpy object with one material slot per used material name."""
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
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    bm.to_mesh(me)
    bm.free()
    return ob


def shell():
    """Stand-in for what plan.py builds: the east wall (outer face, reveals, glazing, closed door
    leaves), the slabs, the ground outside."""
    m = geom.Mesh()
    x0, x1 = -3.0, 18.5
    w = G._Wall((x1, -7.25), (0.0, -1.0))           # outer face looking east (-y); u = x1 - x
    holes = []
    for cx, sill, wd, ht in [G.ENTRANCE, G.HALL_DOOR] + G.SHOP_WINDOWS:
        holes.append((x1 - cx - wd / 2, x1 - cx + wd / 2, sill, sill + ht))
    loc = geom.Mesh()
    geom.wall_with_holes(loc, "AC_Wall", x1 - x0, 0.0, G.ZC + 0.3, holes)
    # (the reveal liners through the wall come from gastronom.opening_liners)
    m.extend(loc.transformed(w))
    # glazing / door leaves
    for cx, sill, wd, ht in G.SHOP_WINDOWS:
        geom.box(m, "AC_Glass", cx - wd / 2, cx + wd / 2, -7.13, -7.125, sill, sill + ht)
        geom.box(m, "AC_Frame", cx - wd / 2, cx + wd / 2, -7.17, -7.08, sill + ht - 0.62, sill + ht - 0.55)
        for e in (cx - wd / 2, cx + wd / 2 - 0.09):
            geom.box(m, "AC_Frame", e, e + 0.09, -7.17, -7.08, sill, sill + ht)
        geom.box(m, "AC_Frame", cx - wd / 2, cx + wd / 2, -7.17, -7.08, sill, sill + 0.09)
        geom.box(m, "AC_Frame", cx - wd / 2, cx + wd / 2, -7.17, -7.08, sill + ht - 0.09, sill + ht)
    for cx, sill, wd, ht in (G.ENTRANCE, G.HALL_DOOR):
        for s in (-1, 1):
            a, b = (cx - wd / 2, cx) if s < 0 else (cx, cx + wd / 2)
            geom.box(m, "AC_Door", a, b, -7.1, -7.05, sill, sill + 1.1)
            geom.box(m, "AC_Door", a, b, -7.1, -7.05, sill + ht - 0.5, sill + ht)
            geom.box(m, "AC_Door", a, a + 0.1, -7.1, -7.05, sill + 1.1, sill + ht - 0.5)
            geom.box(m, "AC_Door", b - 0.1, b, -7.1, -7.05, sill + 1.1, sill + ht - 0.5)
            geom.box(m, "AC_Glass", a + 0.1, b - 0.1, -7.08, -7.075, sill + 1.1, sill + ht - 0.5)
        geom.box(m, "AC_Trim", cx - wd / 2, cx + wd / 2, -7.25, -6.75, sill - 0.02, G.Z0, faces="Z")
    # slabs (close the partition top and everything above/below)
    geom.box(m, "AC_Trim", x0, x1, -7.25, 2.0, G.ZC + 0.001, G.ZC + 0.3)
    geom.box(m, "AC_Plinth", x0, x1, -7.25, 2.0, 0.3, G.Z0 - 0.001)
    # thick backing behind the inner walls so nothing leaks
    geom.box(m, "AC_Trim", x0, -2.46, -7.25, 2.0, G.Z0 - 0.01, G.ZC + 0.01)
    geom.box(m, "AC_Trim", 18.01, x1, -7.25, 2.0, G.Z0 - 0.01, G.ZC + 0.01)
    geom.box(m, "AC_Trim", x0, x1, 1.51, 2.0, G.Z0 - 0.01, G.ZC + 0.01)
    geom.box(m, "AC_Trim", -2.44, 2.44, -1.49, 1.5, G.Z0 - 0.01, G.ZC + 0.01)
    # ground outside
    m.face("AC_Asphalt", [(-30, -40, 0.0), (40, -40, 0.0), (40, -7.25, 0.0), (-30, -7.25, 0.0)])
    return m


def setup_scene():
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    ee = scene.eevee
    for attr, val in (("taa_render_samples", 24), ("use_raytracing", True), ("use_shadows", True),
                      ("use_gtao", True), ("shadow_pool_size", "2048")):
        if hasattr(ee, attr):
            setattr(ee, attr, val)
    if hasattr(ee, "ray_tracing_options"):
        try:
            ee.ray_tracing_options.resolution_scale = "2"
        except Exception:  # noqa: BLE001
            pass
    if hasattr(ee, "use_fast_gi"):
        ee.use_fast_gi = True
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.exposure = -0.4
    world = bpy.data.worlds.new("Sky")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.55, 0.68, 0.9, 1.0)
    bg.inputs["Strength"].default_value = 0.8
    # morning sun from the east (-y), low: light falls through the shop windows onto the floor
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.5
    sun.data.angle = math.radians(1.0)
    d = Vector((0.35, 1.0, -0.55)).normalized()
    sun.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(sun)
    # pendant lamps
    for i, (x, y, z, hx, lm) in enumerate(G.LIGHTS):
        li = bpy.data.lights.new("Lamp%d" % i, "POINT")
        li.energy = lm / 28.0                 # rough lumen -> preview watt
        li.color = hex_rgb(hx)[:3]
        li.shadow_soft_size = 0.12
        ob = bpy.data.objects.new("Lamp%d" % i, li)
        ob.location = (x, y, z - 0.25)        # below the opal body so it lights the room, not its own shell
        scene.collection.objects.link(ob)
    # soft fill through the windows (sky light from the square)
    for i, (cx, sill, wd, ht) in enumerate(G.SHOP_WINDOWS):
        li = bpy.data.lights.new("Win%d" % i, "AREA")
        li.shape = "RECTANGLE"
        li.size, li.size_y = wd, ht
        li.energy = 120.0
        li.color = (0.85, 0.9, 1.0)
        ob = bpy.data.objects.new("Win%d" % i, li)
        ob.location = (cx, -6.8, sill + ht / 2)
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
    for name in names:
        pos, target, lens = VIEWS[name]
        cam.location = pos
        cam.rotation_euler = (Vector(target) - Vector(pos)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        scene.render.filepath = os.path.join(RENDER_DIR, "gastronom_%s.png" % name)
        bpy.ops.render.render(write_still=True)
        print("GASTRONOM render", scene.render.filepath)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    materials()
    coll = bpy.data.collections.new("Gastronom")
    bpy.context.scene.collection.children.link(coll)
    m = G.build()
    print("GASTRONOM triangles", m.tri_count())
    mesh_object("Gastronom7", m, coll)
    mesh_object("Shell", shell(), coll)
    scene = setup_scene()
    names = list(VIEWS)
    for a in ARGS:
        if a.startswith("--views"):
            names = a.split("=", 1)[1].split(",") if "=" in a else names
    render(scene, names)


main()
