"""Preview renders of furnished flat rooms (furniture.py).

Run headless from the worktree root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/academgorodok/preview_furniture.py [-- --views study,dining]

Builds a few sample rooms with a simple shell (walls with door and window holes, reveals, window
units, slabs, a dim vestibule behind each door), furnishes them with furniture.furnish_parts(), lights
them (sky light through the windows, a low sun, the warm lamps from furnish_parts()['lights']) and
renders EEVEE views roughly matched to the museum photos into renders/academgorodok/furniture_<view>.png.
Materials are preview stand-ins: the flat colours of FURNITURE_MATERIALS plus procedural parquet,
tile, wallpaper and damask patterns on the metre UVs.
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
import furniture as F  # noqa: E402

RENDER_DIR = os.path.join(ROOT, "renders", "academgorodok")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
WALL_T = 0.4

SHELL_MATERIALS = {
    "PV_Wall":   ("#D9D3C4", 0.9, "bare plaster behind the finishes, reveals"),
    "PV_Frame":  ("#F2F0EA", 0.5, "white painted window frames, door architraves"),
    "PV_Glass":  ("#C8D8DC", 0.05, "window glass"),
    "PV_Door":   ("#5A3A26", 0.5, "varnished door leaves"),
    "PV_Vest":   ("#B8AE98", 0.9, "vestibule walls behind doors"),
    "PV_Ground": ("#6E7A5A", 1.0, "ground seen from the windows"),
    "PV_Tree":   ("#3F5A34", 1.0, "tree crowns seen from the windows"),
}


def D(side, c, w=1.1, h=2.3):
    return {"side": side, "c": c, "w": w, "h": h}


def W(side, c, w=1.45, sill=0.85, h=2.0):
    return {"side": side, "c": c, "w": w, "sill": sill, "h": h}


# rooms in their local frame (x0 = y0 = 0); OX spreads them out along x in the scene
ROOMS = {
    "study": dict(kind="study", x1=4.6, y1=4.4, h=3.3, seed=2, doors=[D("x1", 0.8)], windows=[W("y1", 2.3)]),
    "living": dict(kind="living", x1=5.6, y1=4.9, h=3.4, seed=13, doors=[D("x0", 1.2, 1.3, 2.4), D("y1", 4.6)],
                   windows=[W("y0", 1.6), W("y0", 4.1)]),
    "bedroom": dict(kind="bedroom", x1=4.5, y1=4.0, h=3.4, seed=4, doors=[D("x1", 0.8)], windows=[W("y1", 2.25)]),
    "kitchen": dict(kind="kitchen", x1=3.6, y1=2.8, h=3.4, seed=10, doors=[D("y0", 0.8)], windows=[W("y1", 2.0, 1.3)]),
    "bath": dict(kind="bath", x1=2.6, y1=2.2, h=3.4, seed=11, doors=[D("y0", 1.9)], windows=[W("y1", 1.0, 0.8, 1.2, 1.0)]),
    "corridor": dict(kind="corridor", x1=6.0, y1=1.5, h=3.4, seed=12,
                     doors=[D("x0", 0.75, 1.3, 2.4), D("y1", 1.7, 1.3, 2.4), D("y0", 4.6), D("x1", 0.75)], windows=[]),
}
OX = {"study": 0.0, "living": 20.0, "bedroom": 40.0, "kitchen": 60.0, "bath": 80.0, "corridor": 100.0}
Z = 0.7

# view: (room, camera position, target (room-local), focal length mm on a 36 mm sensor)
VIEWS = {
    # commons_..._08 / m24_2015_museum_06: the study from the sofa, bookcases behind the desk
    "study": ("study", (4.25, 0.85, 1.55), (0.3, 2.9, 1.15), 17),
    # m24_2016_flat_10 / commons_..._02: dining table under the chandelier, piano and sideboard behind
    "dining": ("living", (5.3, 0.45, 1.65), (1.2, 4.5, 1.25), 17),
    # m24_2016_flat_04: the hall-living room towards the windows
    "hall": ("living", (5.25, 4.35, 1.75), (0.9, 0.5, 1.35), 17),
    "bedroom": ("bedroom", (4.25, 3.7, 1.65), (1.4, 0.4, 0.9), 17),
    "kitchen": ("kitchen", (0.35, 0.3, 1.6), (3.0, 2.6, 1.1), 16),
    "bath": ("bath", (2.4, 0.2, 1.65), (0.6, 1.9, 0.95), 15),
    "corridor": ("corridor", (0.3, 0.75, 1.6), (6.0, 0.75, 1.3), 18),
}


def room_dict(name):
    r = dict(ROOMS[name])
    ox = OX[name]
    r.update(x0=ox, x1=ox + r["x1"], y0=0.0, y1=r["y1"], z=Z, name=name)
    r["doors"] = [dict(d, c=d["c"] + (ox if d["side"] in ("y0", "y1") else 0.0)) for d in r["doors"]]
    r["windows"] = [dict(w, c=w["c"] + (ox if w["side"] in ("y0", "y1") else 0.0)) for w in r["windows"]]
    return r


# --- materials ---------------------------------------------------------------------------------------
def hex_rgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def _brick(nt, w, h, mortar, c1, c2, cm, offset=0.5, bias=0.0):
    tc = nt.nodes.new("ShaderNodeTexCoord")
    br = nt.nodes.new("ShaderNodeTexBrick")
    br.offset = offset
    br.offset_frequency = 1
    br.inputs["Scale"].default_value = 1.0
    br.inputs["Brick Width"].default_value = w
    br.inputs["Row Height"].default_value = h
    br.inputs["Mortar Size"].default_value = mortar
    br.inputs["Mortar Smooth"].default_value = 0.1
    br.inputs["Bias"].default_value = bias
    br.inputs["Color1"].default_value = c1
    br.inputs["Color2"].default_value = c2
    br.inputs["Mortar"].default_value = cm
    nt.links.new(tc.outputs["UV"], br.inputs["Vector"])
    return br, tc


def _pattern(nt, bsdf, col, col2, scale, kind="VORONOI"):
    """Two-colour small pattern on the metre UVs (wallpaper damask, upholstery)."""
    tc = nt.nodes.new("ShaderNodeTexCoord")
    if kind == "VORONOI":
        tx = nt.nodes.new("ShaderNodeTexVoronoi")
        tx.feature = "SMOOTH_F1" if hasattr(tx, "feature") else tx.feature
        tx.inputs["Scale"].default_value = scale
        out = tx.outputs["Distance"]
    else:
        tx = nt.nodes.new("ShaderNodeTexWave")
        tx.inputs["Scale"].default_value = scale
        tx.inputs["Distortion"].default_value = 6.0
        out = tx.outputs["Fac"]
    nt.links.new(tc.outputs["UV"], tx.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.25
    ramp.color_ramp.elements[0].color = col2
    ramp.color_ramp.elements[1].position = 0.45
    ramp.color_ramp.elements[1].color = col
    nt.links.new(out, ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])


def materials():
    table = dict(F.FURNITURE_MATERIALS)
    table.update(SHELL_MATERIALS)
    for name, (hx, rough, _) in table.items():
        mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        b = nt.nodes["Principled BSDF"]
        col = hex_rgb(hx)
        b.inputs["Base Color"].default_value = col
        b.inputs["Roughness"].default_value = rough
        mul = lambda k: [min(1.0, c * k) for c in col[:3]] + [1.0]
        if name == "AC_FParquet":
            br, _ = _brick(nt, 0.4, 0.07, 0.002, col, mul(0.78), hex_rgb("#3A2616"))
            nt.links.new(br.outputs["Color"], b.inputs["Base Color"])
        elif name == "AC_FTileFloor":
            br, _ = _brick(nt, 0.1, 0.1, 0.003, col, mul(0.9), hex_rgb("#6A5A4A"), offset=0.0)
            nt.links.new(br.outputs["Color"], b.inputs["Base Color"])
        elif name == "AC_FTileWall":
            br, _ = _brick(nt, 0.15, 0.15, 0.003, col, mul(0.97), hex_rgb("#8A8780"), offset=0.0)
            nt.links.new(br.outputs["Color"], b.inputs["Base Color"])
        elif name in ("AC_FWallpaperCream", "AC_FWallpaperGreen"):
            _pattern(nt, b, col, mul(0.9), 34.0)
        elif name in ("AC_FDamaskRed", "AC_FPlushGold"):
            _pattern(nt, b, col, hex_rgb("#A8743A") if name == "AC_FDamaskRed" else mul(0.75), 28.0)
        elif name == "AC_FOilcloth":
            br, _ = _brick(nt, 0.05, 0.05, 0.0, col, hex_rgb("#E8EDE4"), col, offset=0.0)
            nt.links.new(br.outputs["Color"], b.inputs["Base Color"])
        elif name in ("AC_FGlass", "PV_Glass"):
            b.inputs["Alpha"].default_value = 0.1 if name == "AC_FGlass" else 0.06
            mat.surface_render_method = "BLENDED"
        elif name == "AC_FLace":
            b.inputs["Alpha"].default_value = 0.7
            mat.surface_render_method = "BLENDED"
        elif name in ("AC_FMirror", "AC_FChrome", "AC_FBrass"):
            b.inputs["Metallic"].default_value = 1.0
        elif "Opal" in name:
            b.inputs["Emission Color"].default_value = hex_rgb(F.LIGHT_WARM)
            b.inputs["Emission Strength"].default_value = 4.0
        mat.use_backface_culling = False


def mesh_object(name, m, coll):
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
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0003)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = False
    return ob


# --- the shell ---------------------------------------------------------------------------------------
def shell(L):
    """Walls (with holes, reveals, window units), slabs, door architraves and vestibules, ground
    and trees outside, for a furniture.Layout."""
    m = geom.Mesh()
    z, h, T = L.z, L.h, WALL_T
    x0, x1, y0, y1 = L.x0, L.x1, L.y0, L.y1
    geom.box(m, "PV_Wall", x0 - T, x1 + T, y0 - T, y1 + T, z - 0.3, z)
    geom.box(m, "PV_Wall", x0 - T, x1 + T, y0 - T, y1 + T, z + h, z + h + 0.3)
    for s in F.SIDES:
        Ls = L.side(s)[6]
        ops = [o for o in L.doors + L.windows if o["side"] == s]
        holes = [(o["u"] - o["w"] / 2 + T, o["u"] + o["w"] / 2 + T, o["sill"], o["sill"] + o["h"]) for o in ops]
        loc = geom.Mesh()
        geom.wall_with_holes(loc, "PV_Wall", Ls + 2 * T, 0.0, h, holes)
        out = geom.Mesh()
        geom.wall_with_holes(out, "PV_Wall", Ls + 2 * T, 0.0, h, holes)
        loc.extend(out.transformed(lambda p: (p[0], -T, p[2])))
        for o in ops:
            a, b = o["u"] - o["w"] / 2 + T, o["u"] + o["w"] / 2 + T
            geom.reveal(loc, "PV_Wall", a, b, o["sill"], o["sill"] + o["h"], T)
            if o in L.windows:
                _window_unit(loc, a, b, o["sill"], o["sill"] + o["h"])
            else:
                _door_frame(loc, a, b, o["h"])
        fr = L.wall_frame(s)
        m.extend(loc.transformed(lambda p: (p[0] - T, p[1], p[2])).transformed(fr))
    # ground and trees outside the windows
    gz = z - 0.7
    m.face("PV_Ground", [(x0 - 60, y0 - 60, gz), (x1 + 60, y0 - 60, gz), (x1 + 60, y1 + 60, gz), (x0 - 60, y1 + 60, gz)])
    for w in L.windows:
        px, py = L.pt(w["side"], w["u"], -9.0)
        for k in (-1, 0, 1):
            qx, qy = L.pt(w["side"], w["u"] + k * 5.0, -9.0 - abs(k) * 2)
            F.sphere(m, "PV_Tree", (qx, qy, z + 4.5 + k * 0.5), 3.2, 10, 7)
            F.tube(m, "PV_Door", (qx, qy, gz), (qx, qy, z + 2.0), 0.25, 0.2, 6)
    return m


def _window_unit(m, a, b, z0, z1):
    """Tripartite white casement with a transom, glass, a sill board (wall-local, v into the room)."""
    v0, v1 = -0.26, -0.2
    fw = 0.07
    geom.box(m, "PV_Frame", a, b, v0, v1, z0, z0 + fw)
    geom.box(m, "PV_Frame", a, b, v0, v1, z1 - fw, z1)
    geom.box(m, "PV_Frame", a, a + fw, v0, v1, z0, z1)
    geom.box(m, "PV_Frame", b - fw, b, v0, v1, z0, z1)
    zt = z1 - 0.5
    geom.box(m, "PV_Frame", a, b, v0, v1, zt - 0.03, zt + 0.03)
    for k in (1, 2):
        x = a + (b - a) * k / 3
        geom.box(m, "PV_Frame", x - 0.03, x + 0.03, v0, v1, z0, zt)
    geom.box(m, "PV_Glass", a, b, v0 + 0.028, v0 + 0.032, z0, z1)
    geom.box(m, "PV_Frame", a - 0.02, b + 0.02, v1, 0.04, z0 - 0.03, z0)


def _door_frame(m, a, b, z1):
    t = 0.09
    geom.box(m, "PV_Frame", a - t, a, 0.0, 0.025, 0.0, z1 + t)
    geom.box(m, "PV_Frame", b, b + t, 0.0, 0.025, 0.0, z1 + t)
    geom.box(m, "PV_Frame", a, b, 0.0, 0.025, z1, z1 + t)
    # a dim vestibule behind the doorway and an open leaf against its side
    T = WALL_T
    geom.box(m, "PV_Vest", a - 0.6, b + 0.6, -T - 1.8, -T - 1.75, 0.0, z1 + 0.4)
    geom.box(m, "PV_Vest", a - 0.65, a - 0.6, -T - 1.8, -T, 0.0, z1 + 0.4)
    geom.box(m, "PV_Vest", b + 0.6, b + 0.65, -T - 1.8, -T, 0.0, z1 + 0.4)
    geom.box(m, "PV_Vest", a - 0.6, b + 0.6, -T - 1.8, -T, z1 + 0.4, z1 + 0.45)
    geom.box(m, "AC_FParquet", a - 0.6, b + 0.6, -T - 1.8, 0.0, -0.01, 0.002)
    geom.box(m, "PV_Door", a - 0.0, a + 0.045, -T - 0.0 - (b - a) * 0.5, -T, 0.02, z1 - 0.02)


# --- scene -------------------------------------------------------------------------------------------
def setup_scene():
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    ee = scene.eevee
    for attr, val in (("taa_render_samples", 24), ("use_raytracing", True), ("use_shadows", True),
                      ("use_gtao", True), ("shadow_pool_size", "1024")):
        if hasattr(ee, attr):
            try:
                setattr(ee, attr, val)
            except Exception:  # noqa: BLE001
                pass
    if hasattr(ee, "ray_tracing_options"):
        try:
            ee.ray_tracing_options.resolution_scale = "2"
        except Exception:  # noqa: BLE001
            pass
    if hasattr(ee, "use_fast_gi"):
        ee.use_fast_gi = True
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.exposure = 0.0
    world = bpy.data.worlds.new("Sky")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.62, 0.72, 0.88, 1.0)
    bg.inputs["Strength"].default_value = 1.0
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    sun.data.angle = math.radians(1.0)
    sun.data.color = (1.0, 0.95, 0.88)
    d = Vector((0.35, 0.9, -0.5)).normalized()        # low morning sun from the south-west
    sun.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(sun)
    return scene


def add_lights(scene, L, lights):
    for i, (x, y, z, hx, lm) in enumerate(lights):
        li = bpy.data.lights.new("Lamp%s%d" % (L.kind, i), "POINT")
        li.energy = lm / 9.0
        li.color = hex_rgb(hx)[:3]
        li.shadow_soft_size = 0.15
        ob = bpy.data.objects.new(li.name, li)
        ob.location = (x, y, z - 0.12)
        scene.collection.objects.link(ob)
    for i, w in enumerate(L.windows):
        li = bpy.data.lights.new("Win%s%d" % (L.kind, i), "AREA")
        li.shape = "RECTANGLE"
        li.size, li.size_y = w["w"], w["h"]
        li.energy = 320.0 * w["w"] * w["h"] / 2.9
        li.color = (0.86, 0.91, 1.0)
        ob = bpy.data.objects.new(li.name, li)
        px, py = L.pt(w["side"], w["u"], -0.3)
        ob.location = (px, py, L.z + w["sill"] + w["h"] / 2)
        nx, ny = L.side(w["side"])[4], L.side(w["side"])[5]
        if w["side"] in ("y0", "y1"):
            ob.rotation_euler = Vector((nx, ny, 0)).to_track_quat("-Z", "Y").to_euler()
        else:
            ob.rotation_euler = Vector((nx, ny, 0)).to_track_quat("-Z", "Z").to_euler()
        scene.collection.objects.link(ob)
    # a faint fill so the corners away from the windows are not black (the photos use flash)
    li = bpy.data.lights.new("Fill%s" % L.kind, "AREA")
    li.shape = "RECTANGLE"
    li.size, li.size_y = (L.x1 - L.x0) * 0.6, (L.y1 - L.y0) * 0.6
    li.energy = 60.0 * (L.x1 - L.x0) * (L.y1 - L.y0) / 20.0
    li.color = (1.0, 0.92, 0.8)
    ob = bpy.data.objects.new(li.name, li)
    ob.location = ((L.x0 + L.x1) / 2, (L.y0 + L.y1) / 2, L.z + L.h - 0.3)
    scene.collection.objects.link(ob)


def render(scene, names, built):
    os.makedirs(RENDER_DIR, exist_ok=True)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.sensor_width = 36.0
    cam.data.clip_start = 0.05
    for name in names:
        rname, pos, target, lens = VIEWS[name]
        L = built[rname]
        p = Vector((L.x0 + pos[0], L.y0 + pos[1], L.z + pos[2]))
        t = Vector((L.x0 + target[0], L.y0 + target[1], L.z + target[2]))
        cam.location = p
        cam.rotation_euler = (t - p).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        scene.render.filepath = os.path.join(RENDER_DIR, "furniture_%s.png" % name)
        bpy.ops.render.render(write_still=True)
        print("FURNITURE render", scene.render.filepath)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    materials()
    names = list(VIEWS)
    for a in ARGS:
        if a.startswith("--views="):
            names = a.split("=", 1)[1].split(",")
    scene = setup_scene()
    built = {}
    for rname in sorted(set(VIEWS[n][0] for n in names)):
        room = room_dict(rname)
        parts = F.furnish_parts(room)
        L = parts["layout"]
        tris = sum(parts[k].tri_count() for k in ("finish", "furniture", "curtains"))
        print("FURNITURE room", rname, room["kind"], "triangles", tris, "items", len(parts["items"]))
        coll = bpy.data.collections.new(rname)
        scene.collection.children.link(coll)
        mesh_object(rname + "_finish", parts["finish"], coll)
        mesh_object(rname + "_furniture", parts["furniture"], coll)
        if parts["curtains"].verts:
            mesh_object(rname + "_curtains", parts["curtains"], coll)
        mesh_object(rname + "_shell", shell(L), coll)
        add_lights(scene, L, parts["lights"])
        built[rname] = L
    render(scene, names, built)


main()
