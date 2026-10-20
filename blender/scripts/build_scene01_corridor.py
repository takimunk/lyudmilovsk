"""Scene 01: the long corridor, dressed as a Soviet apartment stairwell (podyezd).

Builds the whole scene from scratch, renders the hero's OS screen texture,
saves blender/scene01_corridor.blend, exports blender/export/scene01_corridor.glb
for Unreal and writes control renders to renders/.

Run headless from the repo root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/scripts/build_scene01_corridor.py
Add `-- --no-render` to skip the control renders.
"""

import math
import os
import sys

import json
import random

import bmesh
import bpy
from mathutils import Euler, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TEX_DIR = os.path.join(ROOT, "textures")
RENDER_DIR = os.path.join(ROOT, "renders")
EXPORT_DIR = os.path.join(ROOT, "blender", "export")
BLEND_PATH = os.path.join(ROOT, "blender", "scene01_corridor.blend")
SCREEN_PNG = os.path.join(TEX_DIR, "hero_os_screen.png")
PODYEZD_DIR = os.path.join(TEX_DIR, "podyezd")   # made by tools/gen_textures_fal.py

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DO_RENDER = "--no-render" not in ARGS

# Corridor dimensions, metres. Corridor runs along +Y.
L = 90.0            # length
W = 3.4             # floor width: walkway, a narrow aisle, chairs, desks
WALL_H = 2.2        # vertical part of the wall
CEIL_Z = 3.6        # flat ceiling strip; the walls slope up to it
TOP_HALF = 0.8      # half width of the flat ceiling strip

# One long row of desks along the right wall, each station with its own desk.
DESK_D = 0.8
TABLE_X = W / 2 - 0.005 - DESK_D / 2  # desk centre line: the desks stand flush against the wall
TABLE_H = 0.75                        # nominal desk height (each desk varies a little)
SEAT_X = TABLE_X - DESK_D / 2 - 0.42
ROW_Y0 = 0.8
WALK_X0, WALK_X1 = -W / 2 + 0.05, -W / 2 + 1.2
WALK_Y0, WALK_Y1 = 0.5, L      # open at the far end: the belt runs on into the distance
HERO_INDEX = 10                 # the hero's place is the 11th on the row, ten computers in
REAL_AFTER_HERO = 4
CLERK_INDEX = 9                 # the office spreadsheet place where the clerk NPC sits             # real, reachable places after the hero's; then the invisible wall


# Admin hall at the start of the corridor (y < 0): squarer, with the exit to the street.
HALL_HW = 3.0       # half width
HALL_Y0 = -7.0      # far wall with the exit door

LAYOUT = {}         # positions Unreal needs, written to blender/export/scene01_layout.json

PANEL_H = 1.5       # oil-painted lower part of the podyezd wall
WARM = (1.0, 0.68, 0.38)
GARLAND_Z = 2.0
GARLAND_COLORS = [(1.0, 0.62, 0.28), (1.0, 0.55, 0.22), (1.0, 0.7, 0.36)]   # warm incandescent only
DUCT_Z = CEIL_Z - 0.38   # ventilation duct under the ceiling
FAN_YS = [9.0, 27.0, 45.0, 63.0, 81.0]


# ---------------------------------------------------------------- helpers

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def set_engine(scene, *names):
    for n in names:
        try:
            scene.render.engine = n
            return n
        except TypeError:
            continue
    return scene.render.engine


def mat(name, color=(0.5, 0.5, 0.5), rough=0.5, metal=0.0, emit=None, strength=0.0,
        transmission=0.0, alpha=1.0):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if emit is not None:
        bsdf.inputs["Emission Color"].default_value = (*emit, 1)
        bsdf.inputs["Emission Strength"].default_value = strength
    if transmission:
        bsdf.inputs["Transmission Weight"].default_value = transmission
    if alpha < 1:
        bsdf.inputs["Alpha"].default_value = alpha
        try:
            m.surface_render_method = "BLENDED"
        except AttributeError:
            pass
    return m


def tex_mat(name, file, rough=0.8, metal=0.0):
    """Image-textured material (exported to glTF as a base colour texture)."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(PODYEZD_DIR, file))
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def world_uv(obj, tile):
    """Box-projects UVs in world space so `tile` metres = one texture repeat."""
    me = obj.data
    mw = obj.matrix_basis
    if obj.parent:
        mw = obj.parent.matrix_basis @ mw
    uv = me.uv_layers.get("UVMap") or me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        n = (mw.to_3x3() @ poly.normal)
        ax = max(range(3), key=lambda i: abs(n[i]))
        for li in poly.loop_indices:
            co = mw @ me.vertices[me.loops[li].vertex_index].co
            if ax == 0:
                u, v = co.y, co.z
            elif ax == 1:
                u, v = co.x, co.z
            else:
                u, v = co.x, co.y
            uv.data[li].uv = (u / tile, v / tile)
    return obj


def tbox(name, size, loc, material, tile, **kw):
    return world_uv(box(name, size, loc, material, **kw), tile)


def prism(name, profile, y0, y1, material, coll, tile=1.0):
    """Extrudes an (x, z) outline along Y, e.g. the smooth sloped upper walls."""
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    a = [bm.verts.new((x, y0, z)) for x, z in profile]
    b = [bm.verts.new((x, y1, z)) for x, z in profile]
    n = len(profile)
    bm.faces.new(list(reversed(a)))
    bm.faces.new(b)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    me.materials.append(material)
    coll.objects.link(obj)
    return world_uv(obj, tile)


def polyline(name, pts, r, material, coll):
    """A cable: cylinders along a list of points, merged into one mesh."""
    segs = []
    for a, b in zip(pts, pts[1:]):
        d = Vector(b) - Vector(a)
        if d.length < 1e-4:
            continue
        rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_euler()
        segs.append(cyl(name, r, d.length + r, (Vector(a) + Vector(b)) / 2, material, coll=coll, rot=rot, verts=6))
    return join(name, segs, coll)


def sag(a, b, drop, n=8):
    """Points of a cable hanging between a and b."""
    a, b = Vector(a), Vector(b)
    return [a.lerp(b, t / n) - Vector((0, 0, drop * 4 * (t / n) * (1 - t / n))) for t in range(n + 1)]


def world_mat(o):
    m = o.matrix_basis.copy()
    p = o.parent
    while p:
        m = p.matrix_basis @ m
        p = p.parent
    return m


def descendants(o):
    out = []
    for c in o.children:
        out.append(c)
        out.extend(descendants(c))
    return out


def collapse(name, root, coll, extra=()):
    """Merges every mesh under `root` (plus `extra`) into one object and drops the empties."""
    root.name = name + "_root"    # free the name for the merged object
    kids = descendants(root)
    meshes = [o for o in kids if o.type == "MESH"] + [o for o in extra if o]
    empties = [o for o in [root] + kids if o.type == "EMPTY"]
    for o in meshes:
        if o.parent:
            o.matrix_basis = world_mat(o)
            o.parent = None
    obj = join(name, meshes, coll)
    for o in empties:
        bpy.data.objects.remove(o)
    return obj


def join(name, objs, coll):
    """Merges many small objects into one mesh (fewer actors in Unreal)."""
    if not objs:
        return None
    bm = bmesh.new()
    mats = []
    for o in objs:
        tmp = o.data.copy()
        tmp.transform(world_mat(o))
        for m in tmp.materials:
            if m.name not in [x.name for x in mats]:
                mats.append(m)
        offset = {i: [x.name for x in mats].index(m.name) for i, m in enumerate(tmp.materials)}
        for p in tmp.polygons:
            p.material_index = offset.get(p.material_index, 0)
        bm.from_mesh(tmp)
        bpy.data.meshes.remove(tmp)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    for o in objs:
        bpy.data.objects.remove(o)
    obj = bpy.data.objects.new(name, me)
    coll.objects.link(obj)
    return obj


def stone_mat(name, base, alt, axes, brick_w, row_h):
    """Procedural stone blocks; `axes` picks which object coords feed the pattern."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Roughness"].default_value = 0.9
    coord = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    comb = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(coord.outputs["Object"], sep.inputs[0])
    nt.links.new(sep.outputs[axes[0]], comb.inputs[0])
    nt.links.new(sep.outputs[axes[1]], comb.inputs[1])
    brick = nt.nodes.new("ShaderNodeTexBrick")
    brick.inputs["Color1"].default_value = (*base, 1)
    brick.inputs["Color2"].default_value = (*alt, 1)
    brick.inputs["Mortar"].default_value = (0.08, 0.06, 0.045, 1)
    brick.inputs["Scale"].default_value = 1.0
    brick.inputs["Mortar Size"].default_value = 0.012
    brick.inputs["Brick Width"].default_value = brick_w
    brick.inputs["Row Height"].default_value = row_h
    brick.offset = 0.5
    nt.links.new(comb.outputs[0], brick.inputs["Vector"])
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 3.0
    nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 0.35
    nt.links.new(brick.outputs["Color"], mix.inputs["A"])
    nt.links.new(noise.outputs["Color"], mix.inputs["B"])
    nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.4
    nt.links.new(brick.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def _link(obj, parent, coll):
    (coll or bpy.context.scene.collection).objects.link(obj)
    if parent:
        obj.parent = parent
    return obj


def box(name, size, loc, material, parent=None, coll=None, rot=(0, 0, 0), bevel=0.0):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    obj.location = loc
    obj.rotation_euler = rot
    me.materials.append(material)
    if bevel:
        mod = obj.modifiers.new("bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 3
    return _link(obj, parent, coll)


def cyl(name, r, depth, loc, material, parent=None, coll=None, rot=(0, 0, 0), verts=24, r2=None):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r,
                          radius2=r if r2 is None else r2, depth=depth)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new(name, me)
    obj.location = loc
    obj.rotation_euler = rot
    me.materials.append(material)
    return _link(obj, parent, coll)


def plane(name, w, h, loc, material, parent=None, coll=None):
    """Vertical plane facing local -Y by default."""
    me = bpy.data.meshes.new(name)
    hw, hh = w / 2, h / 2
    me.from_pydata([(-hw, 0, -hh), (hw, 0, -hh), (hw, 0, hh), (-hw, 0, hh)], [], [(0, 1, 2, 3)])
    me.uv_layers.new(name="UVMap")
    for i, uv in enumerate([(0, 0), (1, 0), (1, 1), (0, 1)]):
        me.uv_layers[0].data[i].uv = uv
    obj = bpy.data.objects.new(name, me)
    obj.location = loc
    me.materials.append(material)
    return _link(obj, parent, coll)


def empty(name, loc, rot_z=0.0, parent=None, coll=None):
    e = bpy.data.objects.new(name, None)
    e.empty_display_size = 0.2
    e.location = loc
    e.rotation_euler = (0, 0, rot_z)
    return _link(e, parent, coll)


def collection(name):
    c = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(c)
    return c


# ------------------------------------------------- 1. hero OS screen texture

def render_os_screen():
    """Renders the hero's 'basic OS' screen into a PNG via a throwaway scene."""
    scn = bpy.data.scenes.new("OS_Screen")
    if bpy.context.window:
        bpy.context.window.scene = scn
    set_engine(scn, "BLENDER_EEVEE_NEXT", "BLENDER_EEVEE")
    scn.render.resolution_x, scn.render.resolution_y = 1280, 800
    scn.view_settings.view_transform = "Standard"
    scn.render.film_transparent = False
    scn.world = bpy.data.worlds.new("OS_World")
    scn.world.color = (0, 0, 0)

    def flat(name, rgb):
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        em = nt.nodes.new("ShaderNodeEmission")
        em.inputs["Color"].default_value = (*rgb, 1)
        nt.links.new(em.outputs[0], out.inputs[0])
        return m

    def rect(name, x, y, w, h, rgb, z=0.0):
        me = bpy.data.meshes.new(name)
        me.from_pydata([(x, y, z), (x + w, y, z), (x + w, y + h, z), (x, y + h, z)], [], [(0, 1, 2, 3)])
        o = bpy.data.objects.new(name, me)
        me.materials.append(flat(name, rgb))
        scn.collection.objects.link(o)
        return o

    font = None
    for p in ("/System/Library/Fonts/Monaco.ttf", "/System/Library/Fonts/Menlo.ttc"):
        if os.path.exists(p):
            try:
                font = bpy.data.fonts.load(p)
                break
            except RuntimeError:
                pass

    def text(body, x, y, size, rgb, z=0.02):
        cu = bpy.data.curves.new("txt", "FONT")
        cu.body = body
        cu.size = size
        if font:
            cu.font = font
        o = bpy.data.objects.new("txt", cu)
        o.location = (x, y, z)
        cu.materials.append(flat("txt_" + body[:6], rgb))
        scn.collection.objects.link(o)
        return o

    # Canvas is 16 x 10 units, origin bottom-left.
    rect("desk", 0, 0, 16, 10, (0.02, 0.18, 0.20))
    rect("menubar", 0, 9.55, 16, 0.45, (0.80, 0.78, 0.70), 0.01)
    text("АТОН OS   Файл   Правка   Вид", 0.25, 9.66, 0.26, (0.05, 0.05, 0.05))
    text("03:14", 14.9, 9.66, 0.26, (0.05, 0.05, 0.05))
    for i, label in enumerate(["Мой компьютер", "Письма", "Корзина"]):
        y = 8.3 - i * 1.4
        rect("icon%d" % i, 0.55, y, 0.75, 0.75, [(0.85, 0.75, 0.30), (0.95, 0.95, 0.90), (0.55, 0.60, 0.62)][i], 0.01)
        text(label, 0.2, y - 0.35, 0.2, (0.92, 0.95, 0.92))
    # terminal window
    rect("win_shadow", 3.15, 1.35, 11.0, 6.9, (0.0, 0.05, 0.06), 0.005)
    rect("win", 3.0, 1.5, 11.0, 6.9, (0.03, 0.03, 0.035), 0.01)
    rect("win_title", 3.0, 8.0, 11.0, 0.4, (0.80, 0.78, 0.70), 0.012)
    text("терминал", 3.2, 8.09, 0.24, (0.05, 0.05, 0.05))
    green = (0.45, 1.0, 0.55)
    lines = [
        "АТОН OS v1.0  (c) Людмиловск",
        "",
        "> проверка памяти ........ ОК",
        "> проверка памяти ........ ??",
        "> последний вход: никогда",
        "> пользователь не найден",
        "",
        "> ты проснулся?",
        "> _",
    ]
    for i, ln in enumerate(lines):
        text(ln, 3.35, 7.45 - i * 0.62, 0.32, green)
    rect("cursor", 3.95, 7.42 - 8 * 0.62, 0.2, 0.34, green, 0.02)

    cam_data = bpy.data.cameras.new("OS_Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 16
    cam = bpy.data.objects.new("OS_Cam", cam_data)
    cam.location = (8, 5, 5)
    scn.collection.objects.link(cam)
    scn.camera = cam
    scn.render.filepath = SCREEN_PNG
    scn.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True, scene=scn.name)
    return scn


# ---------------------------------------------------------- 2. the corridor

def build_architecture(coll):
    paint = tex_mat("Podyezd_WallPaint", "wall_paint.jpg", 0.35)
    white = tex_mat("Podyezd_Whitewash", "whitewash.jpg", 0.95)
    floor = tex_mat("Podyezd_FloorTiles", "floor_tiles.jpg", 0.6)
    ceil = tex_mat("Podyezd_Ceiling", "ceiling.jpg", 0.95)
    stripe = mat("Podyezd_PaintLine", (0.16, 0.05, 0.03), 0.4)

    tbox("Floor", (W + 1, L, 0.4), (0, L / 2, -0.2), floor, 1.0, coll=coll)
    for side in (-1, 1):
        s_ = "L" if side < 0 else "R"
        x = side * (W / 2 + 0.5)
        # Classic two-tone podyezd wall: oil paint below, whitewash above, a thin dark line between.
        tbox("Wall_%s_Paint" % s_, (1.0, L, PANEL_H), (x, L / 2, PANEL_H / 2), paint, 1.0, coll=coll)
        tbox("Wall_%s" % s_, (1.0, L, WALL_H - PANEL_H), (x, L / 2, (PANEL_H + WALL_H) / 2), white, 1.0, coll=coll)
        box("Wall_%s_Line" % s_, (1.01, L, 0.035), (x, L / 2, PANEL_H), stripe, coll=coll)
        # Smooth sloped upper wall from the top of the vertical wall to the ceiling.
        top_x = TOP_HALF
        outline = [(W / 2, WALL_H), (W / 2 + 1.0, WALL_H), (W / 2 + 1.0, CEIL_Z + 0.6),
                   (top_x - 0.1, CEIL_Z + 0.6), (top_x, CEIL_Z)]
        prism("Slope_%s" % s_, [(side * x, z) for x, z in outline], 0, L, white, coll)
    top_w = 2 * TOP_HALF
    tbox("Ceiling", (top_w + 0.2, L, 0.6), (0, L / 2, CEIL_Z + 0.3), ceil, 2.0, coll=coll)
    # end walls: the far one behind the hero, the near one is a dead end too (no exit yet)
    # The near end opens into the admin hall (build_hall); past the hero an invisible wall
    # stops the player and Unreal repeats this whole stretch into the distance.
    for name, y in ():
        h = CEIL_Z + 0.6
        tbox(name + "_Paint", (W + 1, 1.0, PANEL_H), (0, y, PANEL_H / 2), paint, 1.0, coll=coll)
        tbox(name, (W + 1, 1.0, h - PANEL_H), (0, y, (h + PANEL_H) / 2), white, 1.0, coll=coll)
        box(name + "_Line", (W + 1, 1.01, 0.035), (0, y, PANEL_H), stripe, coll=coll)


def build_garland(coll):
    """Garland of small clear glass bulbs with a glowing filament, sagging along the left wall.
    Every bulb is a light source in Unreal (small radius, no shadows)."""
    wire_m = mat("Garland_Wire", (0.02, 0.05, 0.02), 0.5)
    glass = mat("Garland_Glass", (0.95, 0.92, 0.85), 0.05, transmission=1.0, alpha=0.25)
    socket = mat("Garland_Socket", (0.03, 0.12, 0.04), 0.5)
    filament = mat("Garland_Filament", (1.0, 0.6, 0.25), 0.3, emit=(1.0, 0.55, 0.2), strength=60.0)
    x = -W / 2 + 0.03
    hook, sag_ = 1.6, 0.18
    pts = []
    y = 0.6
    while y <= L - 0.6:
        t = ((y - 0.6) % hook) / hook
        pts.append((x, y, GARLAND_Z - sag_ * 4 * t * (1 - t)))
        y += 0.08
    wires = []
    for a, b in zip(pts, pts[1:]):
        d = Vector(b) - Vector(a)
        mid = (Vector(a) + Vector(b)) / 2
        rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_euler()
        wires.append(cyl("Garland_Wire", 0.003, d.length, mid, wire_m, coll=coll, rot=rot, verts=6))
    join("Garland_Wire", wires, coll)
    glasses, sockets, filaments, lights = [], [], [], []
    for p in pts[::4]:
        bx, by, bz = p[0] + 0.012, p[1], p[2] - 0.045
        sockets.append(cyl("Garland_Socket", 0.006, 0.018, (bx, by, bz + 0.024), socket, coll=coll, verts=8))
        g = uv_sphere("Garland_Bulb", 0.011, (bx, by, bz), glass, coll)
        g.scale = (1.0, 1.0, 1.45)
        glasses.append(g)
        filaments.append(box("Garland_Filament", (0.007, 0.0012, 0.0012), (bx, by, bz), filament, coll=coll))
        lights.append(ue(bx + 0.02, by, bz))
    join("Garland_Bulbs", glasses, coll)
    join("Garland_Sockets", sockets, coll)
    join("Garland_Filaments", filaments, coll)
    LAYOUT["garland_lights"] = lights


def build_ventilation(coll):
    """Galvanised duct along the high ceiling, grilles and slowly turning exhaust fans."""
    zinc = mat("Vent_Galvanised", (0.55, 0.56, 0.57), 0.45, 0.9)
    dark = mat("Vent_Grille", (0.05, 0.05, 0.05), 0.6, 0.5)
    rod = mat("Vent_Rod", (0.2, 0.2, 0.2), 0.5, 0.8)
    parts = [box("Vent_Duct", (0.6, L - 2.0, 0.36), (0, L / 2, DUCT_Z), zinc, coll=coll)]
    for k in range(int(L / 3)):
        y = 1.5 + k * 3.0
        parts.append(box("Vent_Seam", (0.64, 0.04, 0.4), (0, y, DUCT_Z), zinc, coll=coll))
        for dx in (-0.26, 0.26):
            parts.append(cyl("Vent_Hanger", 0.008, CEIL_Z - DUCT_Z - 0.18, (dx, y, (CEIL_Z + DUCT_Z + 0.18) / 2), rod, coll=coll, verts=6))
    for y in range(6, int(L), 9):
        parts.append(box("Vent_Grille", (0.5, 0.4, 0.03), (0, y, DUCT_Z - 0.19), dark, coll=coll))
        for i in range(7):
            parts.append(box("Vent_Slat", (0.46, 0.015, 0.04), (0, y - 0.18 + i * 0.06, DUCT_Z - 0.22), zinc, coll=coll))
    join("Vent_Duct", parts, coll)
    # Outlets: angled diffusers on both sides of the duct, each with a few ribbons that Unreal
    # flutters in the airflow (material wobble) and a hiss of air.
    ribbon = mat("Vent_Ribbon", (0.8, 0.78, 0.7), 0.7)
    outlets, ribbons = [], []
    for k, y in enumerate(range(3, int(L) - 1, 6)):
        side = 1 if k % 2 else -1
        x = side * 0.31
        outlets.append(box("Vent_Outlet", (0.06, 0.45, 0.2), (x + side * 0.03, y, DUCT_Z - 0.05), zinc, coll=coll,
                           rot=(0, math.radians(side * 30), 0)))
        for i in range(6):
            outlets.append(box("Vent_OutletVane", (0.012, 0.42, 0.012), (x + side * 0.065, y, DUCT_Z - 0.13 + i * 0.033),
                               dark, coll=coll, rot=(0, math.radians(side * 30), 0)))
        for i in range(4):
            ribbons.append(plane("Vent_Ribbon", 0.025, 0.32, (x + side * 0.08, y - 0.15 + i * 0.1, DUCT_Z - 0.32),
                                 ribbon, coll=coll))
            ribbons[-1].rotation_euler = (0, 0, math.radians(90))
        LAYOUT.setdefault("vent_outlets", []).append(ue(x + side * 0.1, y, DUCT_Z - 0.15))
    join("Vent_Outlets", outlets, coll)
    join("Vent_Ribbons", ribbons, coll)
    LAYOUT["ribbon_top_z"] = round((DUCT_Z - 0.16) * 100, 1)
    for i, y in enumerate(FAN_YS):
        housing = [cyl("Fan_Ring", 0.3, 0.16, (0, y, DUCT_Z - 0.26), zinc, coll=coll, verts=32),
                   cyl("Fan_Hub", 0.05, 0.1, (0, y, DUCT_Z - 0.34), dark, coll=coll)]
        join("VentFan_Housing_%d" % i, housing, coll)
        # Blades are their own object with the pivot on the axis, so Unreal can spin them.
        root = empty("VentFan_%d" % i, (0, y, DUCT_Z - 0.32), coll=coll)
        blades = []
        for b in range(4):
            a = b * math.pi / 2
            blades.append(box("Blade", (0.24, 0.08, 0.01), (math.cos(a) * 0.14, math.sin(a) * 0.14, 0), dark,
                              root, coll, rot=(math.radians(20), 0, a)))
        bl = join("VentFan_Blades_%d" % i, blades, coll)
        bl.location = (0, 0, 0)
        bl.parent = root
        bl.data.transform(root.matrix_basis.inverted())
    LAYOUT["vent_fans"] = [ue(0, y, DUCT_Z - 0.3) for y in FAN_YS]


def build_podyezd_props(coll):
    """Radiators, a block of mailboxes and an electrical cabinet."""
    rad = tex_mat("Podyezd_RadiatorPaint", "radiator_paint.jpg", 0.5)
    box_m = mat("Mailbox_Paint", (0.18, 0.25, 0.32), 0.45, 0.3)
    door_m = mat("Mailbox_Door", (0.22, 0.3, 0.38), 0.4, 0.3)
    shield = mat("Shield_Grey", (0.32, 0.33, 0.3), 0.5, 0.4)
    pipe = mat("Pipe_Paint", (0.62, 0.58, 0.48), 0.5, 0.2)
    # Cast-iron radiator on the near end wall, under the mailboxes.
    parts = []
    for k in range(12):
        parts.append(tbox("Rad_Section", (0.075, 0.09, 0.55), (0.0 + k * 0.085, HALL_Y0 + 0.1, 0.42), rad, 0.3,
                          coll=coll))
    for z in (0.66, 0.18):
        parts.append(cyl("Rad_Pipe", 0.016, 1.4, (0.47, HALL_Y0 + 0.1, z), pipe, coll=coll,
                         rot=(0, math.radians(90), 0), verts=10))
    join("Radiator", parts, coll)
    # Mailboxes on the hall's far wall, next to the exit.
    parts = []
    for r in range(3):
        for c in range(5):
            x, z = -0.35 + c * 0.42, 1.15 + r * 0.32
            parts.append(box("Mailbox", (0.4, 0.22, 0.3), (x, HALL_Y0 + 0.11, z), box_m, coll=coll))
            parts.append(box("Mailbox_Door", (0.34, 0.01, 0.24), (x, HALL_Y0 + 0.225, z), door_m, coll=coll))
            parts.append(box("Mailbox_Slot", (0.2, 0.012, 0.02), (x, HALL_Y0 + 0.231, z + 0.07), screen_off(),
                             coll=coll))
    join("Mailboxes", parts, coll)
    # Electrical cabinet (shchitok) on the right wall near the hero's end.
    join("ElectricCabinet", [
        box("Cab", (0.12, 0.7, 0.9), (W / 2 - 0.06, L - 8.0, 1.65), shield, coll=coll),
        box("Cab_Lock", (0.02, 0.04, 0.06), (W / 2 - 0.13, L - 7.75, 1.65), pipe, coll=coll)], coll)


# ------------------------------------------------------------ admin hall

def wall_piece(name, x0, x1, y0, y1, z0, z1, coll):
    """A wall block split podyezd-style: oil paint below PANEL_H, whitewash above, a dark line."""
    paint = tex_mat("Podyezd_WallPaint", "wall_paint.jpg", 0.35)
    white = tex_mat("Podyezd_Whitewash", "whitewash.jpg", 0.95)
    line = mat("Podyezd_PaintLine", (0.16, 0.05, 0.03), 0.4)
    cx, cy, sx, sy = (x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0
    if z0 < PANEL_H:
        top = min(z1, PANEL_H)
        tbox(name + "_Paint", (sx, sy, top - z0), (cx, cy, (z0 + top) / 2), paint, 1.0, coll=coll)
        if z1 > PANEL_H:
            box(name + "_Line", (sx + 0.01, sy + 0.01, 0.035), (cx, cy, PANEL_H), line, coll=coll)
    if z1 > PANEL_H:
        bottom = max(z0, PANEL_H)
        tbox(name, (sx, sy, z1 - bottom), (cx, cy, (z1 + bottom) / 2), white, 1.0, coll=coll)


def flip(obj):
    """Turns a plane() around so it faces +Y."""
    obj.rotation_euler = (0, 0, math.pi)
    return obj


def build_hall(coll):
    """Square-ish hall at the start: the admin's booth (a partition with a barred window and a
    curtain, like a stalker trader's corner), a chair with a newspaper and a drinks vending
    machine opposite, and the closed exit to the street."""
    floor = tex_mat("Podyezd_FloorTiles", "floor_tiles.jpg", 0.6)
    ceil = tex_mat("Podyezd_Ceiling", "ceiling.jpg", 0.95)
    white = tex_mat("Podyezd_Whitewash", "whitewash.jpg", 0.95)
    paint = tex_mat("Podyezd_WallPaint", "wall_paint.jpg", 0.35)
    hw, y0, h = HALL_HW, HALL_Y0, CEIL_Z
    tbox("Hall_Floor", (2 * hw + 1, -y0 + 0.5, 0.4), (0, y0 / 2 - 0.25, -0.2), floor, 1.0, coll=coll)
    tbox("Hall_Ceiling", (2 * hw + 1, -y0 + 0.5, 0.4), (0, y0 / 2 - 0.25, h + 0.2), ceil, 2.0, coll=coll)
    wall_piece("Hall_Wall_L", -hw - 0.5, -hw, y0 - 0.5, 0.0, 0, h, coll)
    wall_piece("Hall_Wall_R", hw, hw + 0.5, y0 - 0.5, 0.0, 0, h, coll)
    # Back wall toward the corridor: two pieces around the corridor's sloped opening.
    for side in (-1, 1):
        outline = [(hw + 0.5, 0), (W / 2, 0), (W / 2, WALL_H), (TOP_HALF, CEIL_Z), (hw + 0.5, CEIL_Z)]
        prism("Hall_Back_%s" % ("L" if side < 0 else "R"), [(side * x, z) for x, z in outline], -0.3, 0.0,
              white, coll)
        box("Hall_Back_Paint_%s" % side, (hw + 0.5 - W / 2, 0.02, PANEL_H),
            (side * (W / 2 + hw + 0.5) / 2, -0.31, PANEL_H / 2), paint, coll=coll)
    # Far wall with the exit door opening (x -2.3..-1.3, 2.1 m high).
    dx0, dx1, dh = -2.3, -1.3, 2.1
    wall_piece("Hall_Far_A", -hw - 0.5, dx0, y0 - 0.5, y0, 0, h, coll)
    wall_piece("Hall_Far_B", dx1, hw + 0.5, y0 - 0.5, y0, 0, h, coll)
    wall_piece("Hall_Far_C", dx0, dx1, y0 - 0.5, y0, dh, h, coll)
    door_paint = mat("Door_GreenMetal", (0.12, 0.2, 0.15), 0.4, 0.5)
    frame = mat("Door_Frame", (0.08, 0.08, 0.08), 0.5, 0.6)
    steel = mat("Door_Steel", (0.6, 0.6, 0.62), 0.3, 1.0)
    join("ExitDoor", [
        box("Door_Leaf", (dx1 - dx0 - 0.04, 0.05, dh - 0.03), ((dx0 + dx1) / 2, y0 - 0.1, (dh - 0.03) / 2), door_paint,
            coll=coll, bevel=0.005),
        box("Door_FrameL", (0.05, 0.12, dh), (dx0 + 0.025, y0 - 0.06, dh / 2), frame, coll=coll),
        box("Door_FrameR", (0.05, 0.12, dh), (dx1 - 0.025, y0 - 0.06, dh / 2), frame, coll=coll),
        box("Door_FrameT", (dx1 - dx0, 0.12, 0.05), ((dx0 + dx1) / 2, y0 - 0.06, dh - 0.025), frame, coll=coll),
        box("Door_Handle", (0.14, 0.04, 0.03), (dx1 - 0.15, y0 - 0.055, 1.0), steel, coll=coll),
        box("Door_Lock", (0.05, 0.02, 0.08), (dx1 - 0.15, y0 - 0.07, 1.12), steel, coll=coll),
        box("Door_Bar", (dx1 - dx0 - 0.1, 0.05, 0.06), ((dx0 + dx1) / 2, y0 - 0.02, 1.0), steel, coll=coll),
    ], coll)
    sign = screen_on("exit", "exit_sign.jpg", 2.0)
    join("ExitSign", [box("ExitSign_Box", (0.42, 0.06, 0.16), ((dx0 + dx1) / 2, y0 + 0.03, dh + 0.25),
                          mat("Sign_Case", (0.85, 0.85, 0.85), 0.4), coll=coll),
                      flip(plane("ExitSign_Face", 0.4, 0.14, ((dx0 + dx1) / 2, y0 + 0.061, dh + 0.25), sign,
                                 coll=coll))],
         coll)
    LAYOUT["hall_lights"] = [
        {"pos": ue((dx0 + dx1) / 2, y0 + 0.3, dh + 0.2), "color": [0.2, 1.0, 0.4], "cd": 1.0, "radius": 250},
    ]

    # Bare pendant bulb in the middle of the hall.
    cord = mat("Lamp_Cord", (0.02, 0.02, 0.02), 0.6)
    bulb = mat("Hall_Bulb", (1.0, 0.85, 0.6), 0.2, emit=WARM, strength=25.0)
    join("Hall_Pendant", [cyl("Cord", 0.004, 0.7, (-0.5, -3.5, h - 0.35), cord, coll=coll, verts=6),
                          cyl("Socket", 0.02, 0.06, (-0.5, -3.5, h - 0.72), cord, coll=coll),
                          uv_sphere("Bulb", 0.035, (-0.5, -3.5, h - 0.79), bulb, coll)], coll)
    LAYOUT["hall_lights"].append({"pos": ue(-0.5, -3.5, h - 0.85), "color": [1.0, 0.68, 0.38], "cd": 18.0,
                                  "radius": 900, "shadows": True})

    # ---- the admin's booth on the right: x 1.0..hw, y -5.6..-1.4
    ply = mat("Booth_Plywood", (0.42, 0.3, 0.18), 0.6)
    ply_paint = mat("Booth_PaintBlue", (0.2, 0.3, 0.38), 0.5)
    bars_m = mat("Booth_Bars", (0.1, 0.1, 0.1), 0.4, 0.8)
    glass_m = mat("Booth_Glass", (0.6, 0.7, 0.7), 0.05, transmission=1.0, alpha=0.2)
    curtain_m = mat("Booth_Curtain", (0.35, 0.05, 0.06), 0.9)
    bx0, by0, by1, bh = 1.0, -5.6, -1.4, 2.5
    parts = []
    # side partitions
    for y in (by0, by1):
        parts.append(box("Booth_Side", (hw - bx0, 0.05, bh), ((bx0 + hw) / 2, y, bh / 2), ply_paint, coll=coll))
    # front: counter below, barred window above, header; an entrance gap with a curtain near by1
    gy = by1 - 0.75
    parts.append(box("Booth_Counter", (0.05, gy - by0, 1.05), (bx0, (by0 + gy) / 2, 0.525), ply_paint, coll=coll))
    parts.append(box("Booth_CounterTop", (0.4, gy - by0 + 0.1, 0.04), (bx0 - 0.05, (by0 + gy) / 2, 1.07), ply,
                     coll=coll, bevel=0.005))
    parts.append(box("Booth_Header", (0.05, by1 - by0, 0.3), (bx0, (by0 + by1) / 2, bh - 0.15), ply_paint, coll=coll))
    for y in (by0, gy):
        parts.append(box("Booth_Post", (0.07, 0.07, bh), (bx0, y, bh / 2), ply, coll=coll))
    parts.append(box("Booth_Glass", (0.01, gy - by0, bh - 0.3 - 1.09), (bx0 + 0.02, (by0 + gy) / 2, (1.09 + bh - 0.3) / 2),
                     glass_m, coll=coll))
    k = by0 + 0.12
    while k < gy - 0.05:
        if not (-4.0 < k < -3.45):            # leave a hatch to pass things through
            parts.append(cyl("Booth_Bar", 0.008, bh - 0.3 - 1.09, (bx0 - 0.03, k, (1.09 + bh - 0.3) / 2), bars_m,
                             coll=coll, verts=6))
        k += 0.11
    parts.append(box("Booth_HatchFrame", (0.03, 0.6, 0.03), (bx0 - 0.03, -3.72, 1.55), bars_m, coll=coll))
    # curtain on a rail across the entrance gap
    parts.append(cyl("Curtain_Rail", 0.01, by1 - gy, (bx0, (gy + by1) / 2, 2.15), bars_m, coll=coll,
                     rot=(math.radians(90), 0, 0), verts=8))
    for i in range(9):
        yy = gy + 0.04 + i * (by1 - gy - 0.08) / 8
        parts.append(box("Curtain_Fold", (0.03, 0.1, 1.95), (bx0 + (0.02 if i % 2 else -0.02), yy, 1.16), curtain_m,
                         coll=coll))
    # inside: desk with an old PC, shelves with stuff, a stool, a lamp
    desk_m = mat("Desk_SovietWood", (0.16, 0.08, 0.04), 0.3)
    beige = mat("Plastic_Beige", (0.72, 0.66, 0.53), 0.55)
    parts.append(box("Admin_Desk", (0.7, 1.6, 0.04), (hw - 0.4, -4.3, 0.76), desk_m, coll=coll))
    for yy in (-5.0, -3.6):
        parts.append(box("Admin_DeskLeg", (0.6, 0.04, 0.74), (hw - 0.4, yy, 0.37), desk_m, coll=coll))
    parts.append(box("Admin_CRT", (0.38, 0.36, 0.34), (hw - 0.3, -4.5, 0.95), beige, coll=coll, bevel=0.015))
    parts.append(plane("Admin_Screen", 0.28, 0.21, (hw - 0.49, -4.5, 0.97), screen_on("spreadsheet",
                       "screen_spreadsheet.jpg", 1.1), coll=coll))
    parts[-1].rotation_euler = (0, 0, math.radians(-90))
    parts.append(box("Admin_Keyboard", (0.16, 0.44, 0.03), (hw - 0.62, -4.5, 0.795), beige, coll=coll))
    parts.append(box("Admin_Ledger", (0.25, 0.32, 0.03), (bx0 - 0.02, -4.6, 1.105), mat("Ledger", (0.1, 0.2, 0.35), 0.6),
                     coll=coll, rot=(0, 0, math.radians(8))))
    parts.append(cyl("Admin_Bell", 0.04, 0.03, (bx0 - 0.05, -3.2, 1.105), mat("Brass", (0.7, 0.55, 0.25), 0.3, 1.0),
                     coll=coll, verts=16, r2=0.015))
    shelf = mat("Shelf_Metal", (0.3, 0.3, 0.28), 0.5, 0.6)
    cols = [(0.5, 0.1, 0.05), (0.15, 0.35, 0.12), (0.6, 0.5, 0.2), (0.2, 0.2, 0.45), (0.55, 0.55, 0.5)]
    for lvl in range(4):
        z = 0.5 + lvl * 0.45
        parts.append(box("Shelf", (0.35, 1.4, 0.02), (hw - 0.18, -2.3, z), shelf, coll=coll))
        for j in range(5):
            c = cols[(lvl + j) % len(cols)]
            if (lvl + j) % 3 == 0:
                parts.append(cyl("Jar", 0.05, 0.16, (hw - 0.2, -2.9 + j * 0.28, z + 0.09),
                                 mat("Jar_%d" % ((lvl + j) % 5), c, 0.2, transmission=0.6, alpha=0.7), coll=coll))
            else:
                parts.append(box("Box", (0.22, 0.2, 0.16 + 0.05 * (j % 2)), (hw - 0.2, -2.9 + j * 0.28, z + 0.1),
                                 mat("Crate_%d" % ((lvl + j) % 5), c, 0.8), coll=coll))
    lamp_m = mat("Lamp_GreenShade", (0.08, 0.25, 0.12), 0.35, 0.5)
    lamp_b = mat("Lamp_Bulb", (1.0, 0.85, 0.6), 0.2, emit=WARM, strength=10.0)
    parts += [cyl("Admin_LampBase", 0.07, 0.025, (hw - 0.3, -3.8, 0.79), lamp_m, coll=coll),
              box("Admin_LampArm", (0.015, 0.015, 0.35), (hw - 0.3, -3.8, 0.97), lamp_m, coll=coll),
              cyl("Admin_LampShade", 0.08, 0.1, (hw - 0.38, -3.8, 1.12), lamp_m, coll=coll, r2=0.03),
              cyl("Admin_LampBulb", 0.025, 0.04, (hw - 0.38, -3.8, 1.08), lamp_b, coll=coll)]
    join("AdminBooth", parts, coll)
    c = chair_stool("Admin_Stool", (hw - 0.9, -4.4, 0), math.radians(90), coll)
    collapse("Admin_Stool", c, coll)
    LAYOUT["hall_lights"].append({"pos": ue(hw - 0.38, -3.8, 1.02), "color": [1.0, 0.68, 0.38], "cd": 4.0,
                                  "radius": 350, "shadows": True})

    # ---- opposite: drinks vending machine, a chair with a newspaper
    red = mat("Vending_Red", (0.5, 0.04, 0.03), 0.4, 0.3)
    front = screen_on("vending", "vending_front.jpg", 1.6)
    vx, vy = -hw + 0.4, -3.2
    vend = [box("Vending_Body", (0.75, 0.95, 1.85), (vx, vy, 0.925), red, coll=coll, bevel=0.01),
            plane("Vending_Front", 0.9, 1.75, (vx + 0.38, vy, 0.95), front, coll=coll)]
    vend[1].rotation_euler = (0, 0, math.radians(90))
    join("VendingMachine", vend, coll)
    LAYOUT["hall_lights"].append({"pos": ue(vx + 0.75, vy, 1.2), "color": [0.85, 0.9, 1.0], "cd": 3.0, "radius": 300})
    LAYOUT["vending"] = ue(vx, vy, 1.0)
    c = chair_wooden("Hall_Chair", (-hw + 0.45, -4.7, 0), math.radians(-90), coll)
    paper = mat("Newspaper", (0.8, 0.8, 0.75), 0.9)
    news = tex_mat("Newspaper_Print", "newspaper.jpg", 0.9)
    n1 = plane("Newspaper", 0.3, 0.42, (0, 0, 0), news, coll=coll)
    n1.rotation_euler = (math.radians(-90), 0, math.radians(15))
    n1.location = (-hw + 0.45, -4.7, 0.485)
    n2 = box("Newspaper_Fold", (0.28, 0.4, 0.004), (-hw + 0.47, -4.68, 0.48), paper, coll=coll,
             rot=(0, 0, math.radians(15)))
    collapse("Hall_Chair", c, coll, [n1, n2])


def build_walkway(coll):
    housing = mat("Walkway_Housing", (0.12, 0.12, 0.13), 0.35, 0.8)
    belt = mat("Walkway_Belt", (0.025, 0.025, 0.028), 0.75)
    comb = mat("Walkway_Comb", (0.6, 0.58, 0.5), 0.3, 1.0)
    led = mat("Walkway_LED", (0.0, 0.0, 0.0), 1.0, emit=WARM, strength=2.5)

    cx = (WALK_X0 + WALK_X1) / 2
    w = WALK_X1 - WALK_X0
    ln = WALK_Y1 - WALK_Y0
    cy = (WALK_Y0 + WALK_Y1) / 2
    parent = empty("Walkway", (0, 0, 0), coll=coll)
    box("Walkway_Base", (w + 0.1, ln, 0.12), (cx, cy, 0.06), housing, parent, coll)
    # Plain belt; Unreal gives it a scrolling tread material driven by the conveyor actor.
    box("Walkway_Belt", (w - 0.2, ln, 0.02), (cx, cy, 0.13), belt, parent, coll)
    box("Walkway_Comb", (w - 0.1, 0.5, 0.03), (cx, WALK_Y0 + 0.25, 0.135), comb, parent, coll)
    # No balustrade or handrails: just a low skirt with a warm LED line along both edges.
    LAYOUT["walkway"] = {"x": [round(WALK_X0 * 100, 1), round(WALK_X1 * 100, 1)], "y0": ue(0, WALK_Y0, 0)[1]}
    for x in (WALK_X0, WALK_X1):
        box("Skirt", (0.1, ln, 0.05), (x, cy, 0.145), housing, parent, coll)
        box("Skirt_LED", (0.012, ln, 0.012), (x + (0.06 if x == WALK_X0 else -0.06), cy, 0.15), led, parent, coll)


# ------------------------------------------------------------ chairs
# Built in local space: the sitter faces +Y, the backrest is at -Y.

def chair(name, loc, rot_z, coll, parent=None):
    """Plain black office chair on five castors."""
    fabric = mat("Chair_Fabric", (0.08, 0.08, 0.09), 0.9)
    metal = mat("Chair_Metal", (0.4, 0.4, 0.42), 0.3, 1.0)
    root = empty(name, loc, rot_z, parent, coll)
    box(name + "_Seat", (0.48, 0.46, 0.08), (0, 0, 0.47), fabric, root, coll, bevel=0.02)
    box(name + "_Back", (0.46, 0.07, 0.5), (0, -0.26, 0.8), fabric, root, coll, rot=(math.radians(-8), 0, 0), bevel=0.02)
    _star_base(name, root, coll, metal)
    return root


def _star_base(name, root, coll, metal, post=0.36):
    cyl(name + "_Post", 0.025, post, (0, 0, 0.07 + post / 2), metal, root, coll)
    for i in range(5):
        a = i * 2 * math.pi / 5
        box(name + "_Leg", (0.04, 0.3, 0.03), (math.sin(a) * 0.15, math.cos(a) * 0.15, 0.06), metal, root, coll,
            rot=(0, 0, -a))
        cyl(name + "_Wheel", 0.03, 0.03, (math.sin(a) * 0.29, math.cos(a) * 0.29, 0.03), metal, root, coll,
            rot=(0, math.radians(90), 0), verts=12)


def chair_stool(name, loc, rot_z, coll):
    """Soviet wooden kitchen stool (taburetka)."""
    wood = mat("Stool_Wood", (0.45, 0.3, 0.16), 0.6)
    root = empty(name, loc, rot_z, coll=coll)
    box(name + "_Seat", (0.34, 0.34, 0.035), (0, 0, 0.46), wood, root, coll, bevel=0.006)
    for dx in (-0.13, 0.13):
        for dy in (-0.13, 0.13):
            box(name + "_Leg", (0.035, 0.035, 0.45), (dx, dy, 0.225), wood, root, coll,
                rot=(math.radians(-3 if dy > 0 else 3), math.radians(3 if dx > 0 else -3), 0))
    for dy in (-0.13, 0.13):
        box(name + "_Rung", (0.27, 0.02, 0.03), (0, dy, 0.16), wood, root, coll)
    for dx in (-0.13, 0.13):
        box(name + "_Rung", (0.02, 0.27, 0.03), (dx, 0, 0.12), wood, root, coll)
    return root


def chair_aeron(name, loc, rot_z, coll):
    """Herman Miller style ergonomic chair: graphite frame, dark mesh seat and back."""
    frame = mat("Aeron_Frame", (0.05, 0.05, 0.055), 0.35, 0.2)
    mesh_m = mat("Aeron_Mesh", (0.03, 0.03, 0.035), 0.8)
    alu = mat("Aeron_Alu", (0.7, 0.7, 0.72), 0.25, 1.0)
    root = empty(name, loc, rot_z, coll=coll)
    box(name + "_SeatFrame", (0.5, 0.48, 0.04), (0, 0, 0.47), frame, root, coll, bevel=0.015)
    box(name + "_SeatMesh", (0.44, 0.42, 0.012), (0, 0, 0.495), mesh_m, root, coll)
    back = empty(name + "_BackPivot", (0, -0.22, 0.52), 0, root, coll)
    back.rotation_euler = (math.radians(-12), 0, 0)
    box(name + "_BackFrameL", (0.03, 0.04, 0.6), (-0.23, 0, 0.3), frame, back, coll)
    box(name + "_BackFrameR", (0.03, 0.04, 0.6), (0.23, 0, 0.3), frame, back, coll)
    box(name + "_BackTop", (0.49, 0.04, 0.035), (0, 0, 0.6), frame, back, coll)
    box(name + "_BackMesh", (0.44, 0.012, 0.56), (0, 0, 0.3), mesh_m, back, coll)
    box(name + "_Lumbar", (0.4, 0.03, 0.06), (0, 0.02, 0.18), frame, back, coll)
    for dx in (-0.27, 0.27):
        box(name + "_ArmPost", (0.03, 0.06, 0.2), (dx, -0.08, 0.58), frame, root, coll)
        box(name + "_ArmPad", (0.07, 0.26, 0.025), (dx, -0.02, 0.69), frame, root, coll, bevel=0.01)
    _star_base(name, root, coll, alu, post=0.34)
    return root


def chair_gaming(name, loc, rot_z, coll):
    """Racing-style gaming chair, red and black."""
    black = mat("Gaming_Black", (0.02, 0.02, 0.022), 0.5)
    red = mat("Gaming_Red", (0.55, 0.03, 0.03), 0.45)
    metal = mat("Chair_Metal", (0.4, 0.4, 0.42), 0.3, 1.0)
    root = empty(name, loc, rot_z, coll=coll)
    box(name + "_Seat", (0.52, 0.5, 0.1), (0, 0, 0.48), black, root, coll, bevel=0.03)
    for dx in (-0.24, 0.24):
        box(name + "_SeatWing", (0.06, 0.48, 0.08), (dx, 0, 0.55), red, root, coll, bevel=0.025)
    back = empty(name + "_BackPivot", (0, -0.25, 0.55), 0, root, coll)
    back.rotation_euler = (math.radians(-10), 0, 0)
    box(name + "_Back", (0.5, 0.1, 0.85), (0, 0, 0.42), black, back, coll, bevel=0.03)
    for dx in (-0.22, 0.22):
        box(name + "_BackWing", (0.07, 0.12, 0.6), (dx, 0.03, 0.35), red, back, coll, bevel=0.025)
    box(name + "_Pillow", (0.26, 0.08, 0.12), (0, 0.08, 0.72), red, back, coll, bevel=0.04)
    for dx in (-0.29, 0.29):
        box(name + "_Arm", (0.07, 0.25, 0.03), (dx, -0.02, 0.7), black, root, coll, bevel=0.01)
        box(name + "_ArmPost", (0.04, 0.05, 0.18), (dx, -0.06, 0.6), metal, root, coll)
    _star_base(name, root, coll, metal)
    return root


def chair_wooden(name, loc, rot_z, coll):
    """Old bentwood (Viennese) chair."""
    wood = mat("Bentwood", (0.25, 0.13, 0.06), 0.45)
    root = empty(name, loc, rot_z, coll=coll)
    cyl(name + "_Seat", 0.21, 0.035, (0, 0, 0.46), wood, root, coll, verts=24)
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2
        cyl(name + "_Leg", 0.015, 0.46, (math.cos(a) * 0.16, math.sin(a) * 0.16, 0.23), wood, root, coll, verts=8)
    for dx in (-0.15, 0.15):
        cyl(name + "_BackPost", 0.015, 0.5, (dx, -0.19, 0.7), wood, root, coll, verts=8, rot=(math.radians(-8), 0, 0))
    for z in (0.72, 0.86):
        box(name + "_BackRail", (0.32, 0.02, 0.05), (0, -0.21 - (z - 0.7) * 0.14, z), wood, root, coll)
    cyl(name + "_Ring", 0.14, 0.02, (0, 0, 0.22), wood, root, coll, verts=16)
    return root


def chair_folding(name, loc, rot_z, coll):
    """Grey metal folding chair."""
    metal = mat("Folding_Metal", (0.35, 0.36, 0.37), 0.4, 0.8)
    seat = mat("Folding_Seat", (0.25, 0.26, 0.27), 0.5, 0.5)
    root = empty(name, loc, rot_z, coll=coll)
    box(name + "_Seat", (0.42, 0.4, 0.025), (0, 0.02, 0.45), seat, root, coll, bevel=0.01)
    box(name + "_Back", (0.4, 0.02, 0.2), (0, -0.21, 0.78), seat, root, coll, rot=(math.radians(-8), 0, 0))
    for dx in (-0.2, 0.2):
        box(name + "_LegBack", (0.025, 0.025, 0.9), (dx, -0.17, 0.45), metal, root, coll, rot=(math.radians(-10), 0, 0))
        box(name + "_LegFront", (0.025, 0.025, 0.47), (dx, 0.12, 0.23), metal, root, coll, rot=(math.radians(12), 0, 0))
    return root


CHAIRS = [chair, chair_stool, chair_aeron, chair_gaming, chair_wooden, chair_folding]


# ------------------------------------------------------------ stations
# Each station is built in local space: the device sits at the origin on the
# table top and its screen faces local -Y, where the chair is.

def screen_off():
    return mat("Screen_Off", (0.01, 0.012, 0.014), 0.08, 0.0)


def led(color):
    key = "LED_%.2f_%.2f_%.2f" % color
    return mat(key, (0, 0, 0), 1.0, emit=color, strength=6.0)


def screen_on(name, file, strength=1.2):
    """Glowing screen showing an image; Unreal swaps it for an emissive material by its Screen_On_ prefix."""
    key = "Screen_On_" + name
    m = bpy.data.materials.get(key)
    if m:
        return m
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(PODYEZD_DIR, file))
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Emission Color"])
    bsdf.inputs["Emission Strength"].default_value = strength
    bsdf.inputs["Roughness"].default_value = 0.15
    return m


def crt(root, coll, name, w, h, d, shell, screen=None):
    box(name + "_Front", (w, d * 0.45, h), (0, 0, h / 2), shell, root, coll, bevel=0.015)
    box(name + "_Back", (w * 0.7, d * 0.55, h * 0.75), (0, d * 0.48, h * 0.45), shell, root, coll, bevel=0.03)
    plane(name + "_Screen", w * 0.78, h * 0.72, (0, -d * 0.226, h * 0.52), screen or screen_off(), root, coll)
    box(name + "_LED", (0.012, 0.005, 0.012), (w * 0.4, -d * 0.226, h * 0.08), led((0.9, 0.15, 0.05)), root, coll)


def tower(root, coll, name, material, x=0.45, rgb=None):
    """PC case standing on the floor under the table (table top is local z = 0)."""
    z = -root.location.z
    box(name, (0.2, 0.45, 0.44), (x, 0.05, z + 0.22), material, root, coll, bevel=0.005)
    box(name + "_LED", (0.008, 0.004, 0.008), (x + 0.06, -0.103, z + 0.4), led((0.1, 1.0, 0.2)), root, coll)
    box(name + "_Drive", (0.15, 0.004, 0.04), (x, -0.103, z + 0.34), mat("Plastic_Dark", (0.08, 0.08, 0.08), 0.5),
        root, coll)
    if rgb:
        box(name + "_RGB", (0.004, 0.4, 0.36), (x - 0.102, 0.05, z + 0.22), led(rgb), root, coll)
    return (x, 0.28, z + 0.3)


def cables(root, coll, sources, power_x=-0.35):
    """Cables from the devices over the back edge of the table down to a power strip on the floor."""
    black = mat("Cable", (0.015, 0.015, 0.015), 0.6)
    strip = mat("PowerStrip", (0.85, 0.84, 0.8), 0.5)
    floor = -root.location.z + 0.012
    box("PowerStrip", (0.32, 0.06, 0.04), (power_x, 0.3, floor + 0.008), strip, root, coll)
    box("PowerStrip_Switch", (0.03, 0.02, 0.012), (power_x - 0.12, 0.3, floor + 0.034), led((1.0, 0.35, 0.05)), root, coll)
    objs = []
    for i, (sx, sy, sz) in enumerate(sources):
        plug = (power_x - 0.1 + 0.06 * (i % 4), 0.3, floor + 0.02)
        if sz > -0.1:     # on the table: over the back edge, then hanging down
            edge = (sx + 0.03, DESK_D / 2 - 0.01, 0.0)
            pts = sag((sx, sy + 0.1, sz + 0.02), edge, 0.0, 2) + sag(edge, (sx + 0.08, 0.36, floor), -0.12, 6)[1:]
        else:             # from a tower under the table
            pts = [(sx, sy, sz), (sx, sy + 0.08, floor)]
        pts += sag(pts[-1], plug, 0.0, 2)[1:]
        world = [root.matrix_basis @ Vector(p) for p in pts]
        objs.append(polyline("Cable", world, 0.0045, black, coll))
    return [o for o in objs if o]


def st_sega(root, coll):
    shell = mat("Plastic_Grey", (0.42, 0.42, 0.40), 0.5)
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    crt(root, coll, "SegaCRT", 0.40, 0.36, 0.40, shell, screen_on("16bit", "screen_16bit.jpg"))
    for c in root.children:
        c.location.y += 0.1
    box("MegaDrive", (0.28, 0.21, 0.06), (0.05, -0.22, 0.03), black, root, coll, bevel=0.01)
    cyl("MegaDrive_Ring", 0.07, 0.008, (0.0, -0.22, 0.064), mat("Plastic_Red_Ring", (0.5, 0.05, 0.03), 0.4), root, coll, verts=32)
    box("MegaDrive_Cart", (0.13, 0.02, 0.07), (0.0, -0.21, 0.1), black, root, coll)
    box("Pad", (0.16, 0.08, 0.025), (-0.15, -0.3, 0.013), black, root, coll, bevel=0.012)
    return [(0, 0.3, 0.1), (0.05, -0.12, 0.03)]


def st_ps2(root, coll):
    shell = mat("Plastic_TV_Silver", (0.55, 0.55, 0.56), 0.35, 0.3)
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    crt(root, coll, "TV", 0.62, 0.48, 0.48, shell)
    for c in root.children:
        c.location.y += 0.12
    box("PS2", (0.30, 0.18, 0.078), (0.0, -0.22, 0.039), black, root, coll)
    box("PS2_Stripe", (0.30, 0.004, 0.006), (0.0, -0.311, 0.05), mat("PS2_Stripe", (0.1, 0.1, 0.12), 0.3), root, coll)
    box("PS2_LED", (0.008, 0.004, 0.008), (0.12, -0.312, 0.06), led((0.1, 0.3, 1.0)), root, coll)
    box("DualShock", (0.15, 0.09, 0.03), (0.2, -0.32, 0.015), black, root, coll, bevel=0.015)
    return [(0, 0.35, 0.1), (0.0, -0.13, 0.04)]


def st_gaming(root, coll):
    """Modern gaming setup: two monitors, RGB tower under the table, headset."""
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    for i, (dx, ang, scr) in enumerate([(-0.33, 12, screen_on("modern_game", "screen_modern_game.jpg", 1.4)),
                                         (0.36, -18, screen_off())]):
        m = empty("Monitor_%d" % i, (dx, 0.05, 0), math.radians(ang), root, coll)
        box("Monitor_Panel", (0.62, 0.03, 0.36), (0, 0.03, 0.42), black, m, coll)
        plane("Monitor_Screen", 0.6, 0.34, (0, 0.014, 0.42), scr, m, coll)
        box("Monitor_Neck", (0.05, 0.05, 0.25), (0, 0.08, 0.16), black, m, coll)
        box("Monitor_Foot", (0.25, 0.2, 0.015), (0, 0.05, 0.008), black, m, coll)
    box("Keyboard", (0.44, 0.14, 0.025), (-0.02, -0.25, 0.013), black, root, coll)
    box("Keyboard_Glow", (0.44, 0.14, 0.003), (-0.02, -0.25, 0.0015), led((1.0, 0.2, 0.6)), root, coll)
    box("Mousepad", (0.9, 0.38, 0.004), (0.05, -0.27, 0.002), mat("Mousepad", (0.03, 0.03, 0.04), 0.9), root, coll)
    box("Mouse", (0.06, 0.11, 0.035), (0.33, -0.25, 0.018), black, root, coll, bevel=0.015)
    cyl("Headset_Band", 0.09, 0.03, (-0.5, -0.1, 0.09), black, root, coll, rot=(math.radians(90), 0, 0), verts=16)
    for dx in (-0.09, 0.09):
        cyl("Headset_Cup", 0.045, 0.04, (-0.5 + dx, -0.1, 0.045), black, root, coll, rot=(0, math.radians(90), 0))
    t = tower(root, coll, "GamingTower", black, x=0.5, rgb=(0.6, 0.1, 1.0))
    return [(-0.33, 0.12, 0.1), (0.36, 0.12, 0.1), t]


def st_beige(root, coll):
    beige = mat("Plastic_Beige", (0.72, 0.66, 0.53), 0.55)
    box("Case_Desktop", (0.44, 0.40, 0.15), (0, 0.06, 0.075), beige, root, coll, bevel=0.005)
    box("Floppy", (0.12, 0.005, 0.015), (0.1, -0.142, 0.1), mat("Plastic_Dark", (0.08, 0.08, 0.08), 0.5), root, coll)
    box("Power_LED", (0.008, 0.004, 0.008), (-0.15, -0.142, 0.05), led((0.1, 1.0, 0.2)), root, coll)
    sub = empty("CRT_Mount", (0, 0.04, 0.15), 0, root, coll)
    crt(sub, coll, "BeigeCRT", 0.38, 0.34, 0.40, beige)
    box("Keyboard_Beige", (0.46, 0.17, 0.035), (0, -0.28, 0.018), beige, root, coll, rot=(math.radians(4), 0, 0))
    return [(0, 0.3, 0.1)]


def st_spreadsheet(root, coll):
    """Office PC from the late nineties: CRT monitor with a spreadsheet, tower under the table."""
    beige = mat("Plastic_Beige", (0.72, 0.66, 0.53), 0.55)
    crt(root, coll, "OfficeCRT", 0.42, 0.38, 0.42, beige, screen_on("spreadsheet", "screen_spreadsheet.jpg", 1.1))
    for c in root.children:
        c.location.y += 0.1
    box("Keyboard_Office", (0.46, 0.17, 0.035), (0, -0.26, 0.018), beige, root, coll, rot=(math.radians(4), 0, 0))
    if root.name != "Station_%02d" % CLERK_INDEX:     # the clerk brings his own animated mouse
        box("Mouse_Office", (0.06, 0.1, 0.03), (0.32, -0.26, 0.015), beige, root, coll, bevel=0.012)
    box("Mousepad_Office", (0.22, 0.19, 0.004), (0.32, -0.26, 0.002), mat("Mousepad_Blue", (0.05, 0.12, 0.3), 0.8),
        root, coll)
    for i in range(3):
        box("Papers", (0.21, 0.297, 0.002), (-0.5, -0.1, 0.002 + i * 0.002), mat("Paper", (0.85, 0.84, 0.8), 0.9),
            root, coll, rot=(0, 0, math.radians(8 * i - 6)))
    t = tower(root, coll, "OfficeTower", beige, x=0.45)
    return [(0, 0.35, 0.1), t]


def st_vr(root, coll):
    """VR corner: headset and controllers on the table, two base stations, gaming tower underneath."""
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    white = mat("Plastic_White", (0.85, 0.86, 0.86), 0.35)
    lens = mat("VR_Lens", (0.01, 0.01, 0.02), 0.05, 0.5)
    box("VR_Visor", (0.19, 0.11, 0.1), (0, -0.05, 0.06), white, root, coll, bevel=0.03)
    box("VR_Face", (0.17, 0.02, 0.08), (0, 0.015, 0.06), black, root, coll, bevel=0.01)
    for dx in (-0.035, 0.035):
        cyl("VR_Lens", 0.022, 0.005, (dx, 0.027, 0.06), lens, root, coll, rot=(math.radians(90), 0, 0))
    cyl("VR_Strap", 0.1, 0.03, (0, 0.11, 0.05), black, root, coll, rot=(0, 0, 0), verts=20, r2=0.1)
    for dx in (-0.3, 0.3):
        cyl("VR_Grip", 0.018, 0.12, (dx, -0.2, 0.06), white, root, coll, rot=(math.radians(70), 0, 0), verts=10)
        cyl("VR_Ring", 0.05, 0.015, (dx, -0.24, 0.12), white, root, coll, rot=(math.radians(20), 0, 0), verts=20, r2=0.05)
    for dx in (-0.65, 0.65):
        cyl("BaseStation_Pole", 0.01, 0.4, (dx, 0.3, 0.2), black, root, coll, verts=8)
        box("BaseStation", (0.08, 0.06, 0.08), (dx, 0.28, 0.43), black, root, coll, bevel=0.01,
            rot=(math.radians(-20), 0, math.radians(15 if dx < 0 else -15)))
    box("VR_Laptop_Base", (0.34, 0.24, 0.018), (0.45, -0.05, 0.009), black, root, coll)
    box("VR_Laptop_Lid", (0.34, 0.008, 0.23), (0.45, 0.07, 0.12), black, root, coll, rot=(math.radians(-15), 0, 0))
    t = tower(root, coll, "VRTower", black, x=0.5, rgb=(0.1, 0.7, 1.0))
    return [(0, 0.12, 0.06), (-0.65, 0.3, 0.0), (0.65, 0.3, 0.0), (0.45, 0.07, 0.02), t]


def st_zx(root, coll):
    shell = mat("Plastic_TV_Dark", (0.12, 0.10, 0.09), 0.6)
    crt(root, coll, "SmallTV", 0.36, 0.30, 0.34, shell)
    for c in root.children:
        c.location.y += 0.12
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    box("ZX", (0.31, 0.14, 0.03), (0, -0.24, 0.015), black, root, coll, rot=(math.radians(5), 0, 0))
    for i, col in enumerate([(0.8, 0.05, 0.05), (0.9, 0.6, 0.05), (0.1, 0.6, 0.1), (0.1, 0.3, 0.9)]):
        box("ZX_Stripe", (0.012, 0.05, 0.002), (0.11 + i * 0.014, -0.25, 0.032),
            mat("Stripe_%d" % i, col, 0.4), root, coll, rot=(0, 0, math.radians(-30)))
    box("Cassette", (0.2, 0.13, 0.05), (-0.25, -0.2, 0.025), black, root, coll)
    return [(0, 0.3, 0.1), (0, -0.2, 0.02)]


def st_laptop(root, coll):
    alu = mat("Aluminium", (0.6, 0.6, 0.62), 0.3, 1.0)
    box("Laptop_Base", (0.34, 0.24, 0.018), (0, -0.12, 0.009), alu, root, coll)
    lid = empty("Laptop_Hinge", (0, 0.0, 0.018), 0, root, coll)
    lid.rotation_euler = (math.radians(-15), 0, 0)
    box("Laptop_Lid", (0.34, 0.008, 0.23), (0, 0.004, 0.115), alu, lid, coll)
    plane("Laptop_Screen", 0.31, 0.2, (0, -0.0005, 0.118), screen_off(), lid, coll)
    box("Laptop_LED", (0.006, 0.004, 0.004), (0.15, -0.241, 0.01), led((1.0, 0.6, 0.1)), root, coll)
    return [(0.15, 0.0, 0.01)]


def st_imac(root, coll):
    teal = mat("iMac_Teal", (0.1, 0.55, 0.6), 0.15, transmission=0.6, alpha=0.85)
    white = mat("Plastic_White", (0.85, 0.86, 0.86), 0.35)
    box("iMac_Front", (0.40, 0.18, 0.38), (0, 0.0, 0.25), white, root, coll, bevel=0.04)
    box("iMac_Back", (0.36, 0.26, 0.34), (0, 0.18, 0.25), teal, root, coll, bevel=0.08)
    box("iMac_Foot", (0.18, 0.2, 0.06), (0, 0.06, 0.03), teal, root, coll, bevel=0.02)
    plane("iMac_Screen", 0.28, 0.21, (0, -0.091, 0.28), screen_off(), root, coll)
    box("iMac_Keyboard", (0.35, 0.13, 0.025), (0, -0.28, 0.013), teal, root, coll, bevel=0.01)
    cyl("iMac_Puck", 0.035, 0.02, (0.26, -0.27, 0.01), teal, root, coll)
    return [(0, 0.3, 0.1)]


def st_modern(root, coll):
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    white = mat("Plastic_White", (0.85, 0.86, 0.86), 0.35)
    box("FlatTV", (0.9, 0.04, 0.52), (0, 0.12, 0.34), black, root, coll)
    plane("FlatTV_Screen", 0.88, 0.5, (0, 0.099, 0.34), screen_off(), root, coll)
    box("FlatTV_Stand", (0.35, 0.18, 0.02), (0, 0.12, 0.01), black, root, coll)
    box("PS5", (0.1, 0.26, 0.39), (0.6, 0.05, 0.195), white, root, coll, bevel=0.03)
    box("PS5_Core", (0.07, 0.25, 0.37), (0.6, 0.05, 0.195), black, root, coll)
    box("PS5_LED", (0.07, 0.003, 0.004), (0.6, -0.082, 0.36), led((0.2, 0.5, 1.0)), root, coll)
    box("DualSense", (0.16, 0.1, 0.035), (-0.1, -0.25, 0.018), white, root, coll, bevel=0.015)
    return [(0, 0.14, 0.05), (0.6, 0.18, 0.05)]


STATIONS = [st_sega, st_spreadsheet, st_ps2, st_gaming, st_vr, st_beige, st_zx, st_laptop, st_imac, st_modern]


# ------------------------------------------------------------ desks
# Desk local frame: x along the row, -y toward the chair, the top surface at z = 0
# (the station root sits at desk height), the floor at z = -h.

def desk(root, coll, kind, w, h):
    d = DESK_D
    if kind == "soviet":          # polished dark wood, drawer pedestal on the left
        wood = mat("Desk_SovietWood", (0.16, 0.08, 0.04), 0.3)
        brass = mat("Brass", (0.7, 0.55, 0.25), 0.3, 1.0)
        box("Top", (w, d, 0.03), (0, 0, -0.015), wood, root, coll, bevel=0.004)
        box("Pedestal", (0.4, d - 0.06, h - 0.03), (-w / 2 + 0.22, 0, -h / 2 - 0.015), wood, root, coll)
        for k in range(3):
            z = -0.12 - k * 0.22
            box("Drawer", (0.36, 0.01, 0.18), (-w / 2 + 0.22, -d / 2 + 0.025, z), wood, root, coll, bevel=0.003)
            box("Handle", (0.08, 0.015, 0.012), (-w / 2 + 0.22, -d / 2 + 0.015, z), brass, root, coll)
        for dy in (-1, 1):
            box("Leg", (0.05, 0.05, h - 0.03), (w / 2 - 0.05, dy * (d / 2 - 0.05), -h / 2 - 0.015), wood, root, coll)
        box("Modesty", (w - 0.5, 0.02, 0.35), (0.18, d / 2 - 0.05, -0.2), wood, root, coll)
    elif kind == "white":         # white laminate on thin steel legs
        white = mat("Desk_White", (0.86, 0.86, 0.84), 0.4)
        steel = mat("Desk_Steel", (0.75, 0.75, 0.76), 0.3, 1.0)
        box("Top", (w, d, 0.025), (0, 0, -0.0125), white, root, coll)
        for dx in (-1, 1):
            for dy in (-1, 1):
                cyl("Leg", 0.02, h - 0.025, (dx * (w / 2 - 0.06), dy * (d / 2 - 0.06), -h / 2), steel, root, coll,
                    verts=10)
    elif kind == "gaming":        # black top, RGB edge, T-legs
        black = mat("Desk_Black", (0.02, 0.02, 0.022), 0.35)
        box("Top", (w, d, 0.025), (0, 0, -0.0125), black, root, coll)
        box("RGB_Edge", (w, 0.004, 0.012), (0, -d / 2 - 0.002, -0.012), led((0.7, 0.1, 1.0)), root, coll)
        for dx in (-1, 1):
            box("Leg", (0.06, 0.06, h - 0.05), (dx * (w / 2 - 0.1), 0.1, -h / 2), black, root, coll)
            box("Foot", (0.06, d - 0.1, 0.04), (dx * (w / 2 - 0.1), 0, -h + 0.02), black, root, coll)
        box("Crossbar", (w - 0.2, 0.05, 0.05), (0, 0.1, -h + 0.25), black, root, coll)
    elif kind == "office":        # grey laminate, panel legs, modesty panel
        grey = mat("Desk_OfficeGrey", (0.55, 0.55, 0.52), 0.5)
        frame = mat("Desk_OfficeFrame", (0.22, 0.22, 0.23), 0.4, 0.6)
        box("Top", (w, d, 0.03), (0, 0, -0.015), grey, root, coll, bevel=0.003)
        for dx in (-1, 1):
            box("Panel", (0.025, d - 0.05, h - 0.03), (dx * (w / 2 - 0.02), 0, -h / 2 - 0.015), frame, root, coll)
        box("Modesty", (w - 0.05, 0.02, 0.4), (0, d / 2 - 0.04, -0.25), frame, root, coll)
    elif kind == "school":        # birch top on a green tubular frame
        birch = mat("Desk_Birch", (0.75, 0.6, 0.4), 0.5)
        green = mat("Desk_GreenFrame", (0.12, 0.3, 0.18), 0.45, 0.5)
        box("Top", (w, d, 0.028), (0, 0, -0.014), birch, root, coll, bevel=0.004)
        for dx in (-1, 1):
            for dy in (-1, 1):
                box("Leg", (0.03, 0.03, h - 0.03), (dx * (w / 2 - 0.04), dy * (d / 2 - 0.04), -h / 2 - 0.015), green,
                    root, coll)
            box("Rail", (0.03, d - 0.08, 0.03), (dx * (w / 2 - 0.04), 0, -h + 0.12), green, root, coll)
        box("Shelf", (w - 0.1, d - 0.2, 0.015), (0, 0.05, -0.15), birch, root, coll)
    elif kind == "glass":         # glass top on chrome A-trestles
        glass = mat("Desk_Glass", (0.3, 0.38, 0.38), 0.05, transmission=1.0, alpha=0.35)
        chrome = mat("Chrome", (0.8, 0.8, 0.82), 0.12, 1.0)
        box("Top", (w, d, 0.012), (0, 0, -0.006), glass, root, coll)
        for dx in (-1, 1):
            for dy in (-1, 1):
                box("Trestle", (0.025, 0.025, h + 0.05), (dx * (w / 2 - 0.15), dy * 0.18, -h / 2), chrome, root, coll,
                    rot=(math.radians(dy * 14), 0, 0))
            box("TrestleTop", (0.04, d - 0.15, 0.02), (dx * (w / 2 - 0.15), 0, -0.022), chrome, root, coll)
    else:                         # "pine": plain pine with an X brace
        pine = mat("Desk_Pine", (0.62, 0.45, 0.26), 0.55)
        box("Top", (w, d, 0.035), (0, 0, -0.0175), pine, root, coll, bevel=0.005)
        for dx in (-1, 1):
            for dy in (-1, 1):
                box("Leg", (0.06, 0.06, h - 0.035), (dx * (w / 2 - 0.05), dy * (d / 2 - 0.05), -h / 2 - 0.0175),
                    pine, root, coll)
        brace = math.degrees(math.atan2(h - 0.25, w - 0.1))
        for sgn in (-1, 1):
            box("Brace", (math.hypot(w - 0.1, h - 0.25), 0.03, 0.04), (0, d / 2 - 0.05, -h / 2), pine, root, coll,
                rot=(0, math.radians(sgn * brace), 0))


DESKS = ["soviet", "white", "gaming", "office", "school", "glass", "pine"]
WIDE = {"st_gaming", "st_vr", "st_modern"}


# ------------------------------------------------------------ desk objects

def ext_fan(root, coll, x, y, n):
    """Small desk fan facing the chair; blades are their own object so Unreal can spin them."""
    white = mat("Fan_White", (0.82, 0.83, 0.8), 0.4)
    grille = mat("Fan_Grille", (0.6, 0.62, 0.62), 0.3, 1.0)
    box("Fan_Base", (0.16, 0.12, 0.03), (x, y, 0.015), white, root, coll, bevel=0.01)
    cyl("Fan_Stem", 0.012, 0.2, (x, y, 0.13), white, root, coll, verts=10)
    cyl("Fan_Motor", 0.04, 0.08, (x, y + 0.04, 0.26), white, root, coll, rot=(math.radians(90), 0, 0))
    cyl("Fan_Guard", 0.12, 0.08, (x, y - 0.03, 0.26), grille, root, coll, rot=(math.radians(90), 0, 0), verts=24,
        r2=0.11)
    # Blades: built around local Z, then turned so Z points at the chair (local -Y of the station).
    me_objs = []
    for b in range(3):
        a = b * 2 * math.pi / 3
        me_objs.append(box("FanBlade", (0.1, 0.04, 0.004), (math.cos(a) * 0.055, math.sin(a) * 0.055, 0),
                           mat("Fan_Blade", (0.7, 0.75, 0.8), 0.3, transmission=0.5, alpha=0.6), coll=coll,
                           rot=(math.radians(25), 0, a)))
    bl = join("DeskFan_Blades_%02d" % n, me_objs, coll)
    loc = root.matrix_basis @ Vector((x, y - 0.05, 0.26))
    bl.location = loc
    # local Z -> station -Y -> world -X (stations are turned -90 degrees)
    bl.rotation_euler = (0, math.radians(-90), 0)
    LAYOUT.setdefault("desk_fans", []).append(bl.name)


def ext_rose(root, coll, x, y, n):
    """A single rose in a glass under a purple grow lamp."""
    glass = mat("Vase_Glass", (0.6, 0.7, 0.7), 0.05, transmission=1.0, alpha=0.3)
    stem = mat("Rose_Stem", (0.1, 0.3, 0.08), 0.6)
    petal = mat("Rose_Petal", (0.55, 0.02, 0.06), 0.5)
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    purple = mat("GrowLamp_Purple", (0.0, 0.0, 0.0), 0.5, emit=(0.6, 0.1, 1.0), strength=20.0)
    cyl("Vase", 0.035, 0.18, (x, y, 0.09), glass, root, coll, verts=16, r2=0.03)
    cyl("Stem", 0.004, 0.3, (x, y, 0.25), stem, root, coll, verts=6)
    for k in range(3):
        box("Leaf", (0.05, 0.02, 0.004), (x + 0.02 * (k - 1), y, 0.2 + k * 0.05), stem, root, coll,
            rot=(0, math.radians(30 * (k - 1)), math.radians(60 * k)))
    for k in range(5):
        a = k * 2 * math.pi / 5
        cyl("Petal", 0.022, 0.03, (x + math.cos(a) * 0.012, y + math.sin(a) * 0.012, 0.415), petal, root, coll,
            verts=8, r2=0.012, rot=(math.radians(15) * math.cos(a), math.radians(15) * math.sin(a), 0))
    # clamp lamp: pole, arm, head with a purple LED disc pointing down at the rose
    cyl("GrowLamp_Pole", 0.008, 0.6, (x + 0.15, y + 0.12, 0.3), black, root, coll, verts=8)
    box("GrowLamp_Arm", (0.16, 0.015, 0.015), (x + 0.075, y + 0.06, 0.6), black, root, coll,
        rot=(0, 0, math.radians(38)))
    cyl("GrowLamp_Head", 0.06, 0.03, (x, y, 0.59), black, root, coll)
    cyl("GrowLamp_LED", 0.05, 0.004, (x, y, 0.573), purple, root, coll)
    LAYOUT.setdefault("grow_lights", []).append(ue_from(root, x, y, 0.55))


def ext_desk_lamp(root, coll, x, y, n):
    """Soviet desk lamp with a green metal shade and a warm bulb."""
    green = mat("Lamp_GreenShade", (0.08, 0.25, 0.12), 0.35, 0.5)
    bulb = mat("Lamp_Bulb", (1.0, 0.85, 0.6), 0.2, emit=WARM, strength=10.0)
    cyl("DeskLamp_Base", 0.07, 0.025, (x, y, 0.0125), green, root, coll)
    box("DeskLamp_Arm1", (0.015, 0.015, 0.3), (x, y + 0.04, 0.16), green, root, coll, rot=(math.radians(15), 0, 0))
    box("DeskLamp_Arm2", (0.015, 0.25, 0.015), (x, y - 0.03, 0.31), green, root, coll, rot=(math.radians(-20), 0, 0))
    cyl("DeskLamp_Shade", 0.07, 0.1, (x, y - 0.14, 0.27), green, root, coll, verts=20, r2=0.03,
        rot=(math.radians(-25), 0, 0))
    cyl("DeskLamp_Bulb", 0.025, 0.04, (x, y - 0.14, 0.235), bulb, root, coll, verts=10)
    LAYOUT.setdefault("desk_lights", []).append(ue_from(root, x, y - 0.14, 0.2))


def ext_plant(root, coll, x, y, n):
    pot = mat("Pot_Terracotta", (0.55, 0.25, 0.12), 0.8)
    leaf = mat("Plant_Leaf", (0.08, 0.32, 0.1), 0.6)
    cyl("Pot", 0.06, 0.1, (x, y, 0.05), pot, root, coll, verts=14, r2=0.045)
    if n % 2:
        cyl("Cactus", 0.03, 0.14, (x, y, 0.16), leaf, root, coll, verts=10, r2=0.025)
        cyl("Cactus_Arm", 0.015, 0.06, (x + 0.03, y, 0.18), leaf, root, coll, verts=8)
    else:
        for k in range(7):
            a = k * 2 * math.pi / 7
            box("Leaf", (0.015, 0.008, 0.18), (x + math.cos(a) * 0.03, y + math.sin(a) * 0.03, 0.18), leaf, root, coll,
                rot=(math.sin(a) * 0.4, -math.cos(a) * 0.4, 0))


def ext_books(root, coll, x, y, n):
    cols = [(0.4, 0.05, 0.05), (0.05, 0.15, 0.35), (0.1, 0.3, 0.12), (0.6, 0.5, 0.2), (0.15, 0.12, 0.1)]
    z = 0.0
    for k in range(2 + n % 3):
        th = 0.025 + 0.012 * ((n + k) % 3)
        box("Book", (0.17 + 0.02 * (k % 2), 0.24 - 0.02 * (k % 2), th), (x, y, z + th / 2),
            mat("Book_%d" % ((n + k) % len(cols)), cols[(n + k) % len(cols)], 0.7), root, coll,
            rot=(0, 0, math.radians(7 * (k - 1))))
        z += th


def ext_tea(root, coll, x, y, n):
    """Tea glass in a metal holder (podstakannik) with a spoon."""
    metal = mat("Podstakannik", (0.6, 0.55, 0.45), 0.3, 1.0)
    glass = mat("Tea_Glass", (0.5, 0.25, 0.08), 0.05, transmission=0.8, alpha=0.6)
    cyl("Holder", 0.036, 0.06, (x, y, 0.03), metal, root, coll, verts=16)
    cyl("TeaGlass", 0.033, 0.1, (x, y, 0.08), glass, root, coll, verts=16, r2=0.035)
    box("Handle", (0.01, 0.04, 0.06), (x + 0.045, y, 0.05), metal, root, coll)
    cyl("Spoon", 0.003, 0.15, (x - 0.01, y, 0.12), metal, root, coll, verts=6, rot=(0, math.radians(12), 0))


def ext_trinkets(root, coll, x, y, n):
    """Rubik's cube, an alarm clock and a stack of CDs."""
    cols = [(0.8, 0.05, 0.05), (0.05, 0.5, 0.1), (0.05, 0.15, 0.7), (0.9, 0.8, 0.1), (0.9, 0.9, 0.9), (0.9, 0.4, 0.05)]
    for i in range(2):
        for j in range(2):
            box("Rubik", (0.028, 0.028, 0.056), (x - 0.1 + i * 0.029, y + j * 0.029, 0.028),
                mat("Rubik_%d" % ((i + 2 * j + n) % 6), cols[(i + 2 * j + n) % 6], 0.4), root, coll)
    red = mat("Clock_Red", (0.6, 0.05, 0.04), 0.4, 0.5)
    cyl("Clock", 0.05, 0.035, (x + 0.08, y, 0.06), red, root, coll, rot=(math.radians(90), 0, 0))
    cyl("Clock_Face", 0.042, 0.002, (x + 0.08, y - 0.019, 0.06), mat("Clock_Face", (0.9, 0.88, 0.8), 0.5), root, coll,
        rot=(math.radians(90), 0, 0))
    for k in range(4):
        box("CD_Case", (0.142, 0.125, 0.01), (x, y + 0.14, 0.005 + k * 0.01), mat("CD_Case", (0.7, 0.75, 0.8), 0.1),
            root, coll, rot=(0, 0, math.radians(5 * k)))


EXTRAS = [ext_desk_lamp, ext_plant, ext_books, ext_tea, ext_trinkets]


def build_stations(coll):
    """One long row: every station has its own desk, the desks butt against each other."""
    rng = random.Random(1987)
    kinds = []
    y = ROW_Y0
    i = 0
    roses = 0
    fans = 0
    while True:
        if not kinds:
            kinds = STATIONS[:]
            rng.shuffle(kinds)
        fn = kinds[-1]
        w = rng.choice([1.3, 1.45, 1.6]) if fn.__name__ not in WIDE else rng.choice([1.6, 1.75])
        if i == HERO_INDEX and "hero" not in LAYOUT:
            y = build_hero(coll, y)
            continue
        if y + w > L - 0.8:
            break
        kinds.pop()
        h = rng.uniform(0.73, 0.77)
        cy = y + w / 2
        root = empty("Station_%02d" % i, (TABLE_X, cy, h), math.radians(-90), coll=coll)
        desk(root, coll, rng.choice(DESKS), w, h)
        sources = fn(root, coll) or []
        if i == CLERK_INDEX:
            LAYOUT["clerk"] = {
                "station": ue_from(root, 0, 0, 0),           # desk-top centre
                "station_kind": fn.__name__[3:],
                "desk_height": round(h * 100, 1),
                "chair": ue(SEAT_X, cy, 0.0),
                "chair_yaw": 0.0,                            # sitter faces +X, toward the desk
                "screen": ue_from(root, 0, 0.005, 0.198),     # CRT screen centre (spreadsheet station)
                "screen_size": [32.8, 27.4],
                "screen_normal": [-100.0, 0.0, 0.0],
                "mousepad": ue_from(root, 0.32, -0.26, 0.0),
            }
        wires = cables(root, coll, sources, power_x=rng.uniform(-0.4, 0.2))
        # A few objects at the free back corners of the desk.
        sides = [-1, 1]
        rng.shuffle(sides)
        for side in sides[:rng.choice([0, 1, 1, 2])]:
            ex_x = side * (w / 2 - 0.17)
            if fn.__name__ != "st_vr" and roses < 4 and rng.random() < 0.12:
                ext_rose(root, coll, ex_x, 0.18, i)
                roses += 1
            elif fans < 10 and rng.random() < 0.25:
                ext_fan(root, coll, ex_x, 0.15, i)
                fans += 1
            else:
                rng.choice(EXTRAS)(root, coll, ex_x, 0.15, i)
        trash = litter(coll, rng, root, w) if rng.random() < 0.25 else []
        collapse("Station_%02d_%s" % (i, fn.__name__[3:]), root, coll, wires + trash)
        make = rng.choice(CHAIRS)
        loc = (SEAT_X + rng.uniform(-0.08, 0.1), cy + rng.uniform(-0.15, 0.15), 0)
        yaw = -90 + rng.uniform(-20, 20)
        if i == CLERK_INDEX:                                 # squarely at the desk for the clerk
            loc, yaw = (SEAT_X, cy, 0), -90
        c = make("Chair_%02d" % i, loc, math.radians(yaw), coll)
        collapse("Chair_%02d" % i, c, coll)
        y += w
        i += 1
        if i == HERO_INDEX + REAL_AFTER_HERO:
            # A few real places past the hero; beyond this the corridor is only to look at.
            LAYOUT["invisible_wall_y"] = ue(0, y + 0.05, 0)[1]
    return y


# ------------------------------------------------------------ fast-food litter

def cola_cup(coll, loc, rot, lying=False):
    red = mat("Cup_Red", (0.62, 0.04, 0.04), 0.6)
    white = mat("Cup_White", (0.9, 0.9, 0.88), 0.5)
    straw = mat("Straw", (0.95, 0.95, 0.95), 0.4)
    h = 0.16
    r_ = (math.radians(90), 0, rot) if lying else (0, 0, rot)
    o = cyl("Cup", 0.045, h, (0, 0, 0), red, coll=coll, verts=16, r2=0.035)
    lid = cyl("Cup_Lid", 0.047, 0.01, (0, 0, h / 2 + 0.005), white, coll=coll, verts=16)
    st = cyl("Cup_Straw", 0.004, 0.12, (0.01, 0, h / 2 + 0.05), straw, coll=coll, verts=6, rot=(0, math.radians(8), 0))
    for p in (o, lid, st):
        p.location = Vector(p.location)
    parts = [o, lid, st]
    m = Euler(r_).to_matrix().to_4x4()
    m.translation = Vector(loc) + Vector((0, 0, 0.045 if lying else h / 2))
    for p in parts:
        p.matrix_basis = m @ p.matrix_basis
    return parts


def food_bag(coll, loc, rot):
    kraft = mat("Kraft_Paper", (0.55, 0.4, 0.24), 0.9)
    b = box("FoodBag", (0.15, 0.09, 0.2), (loc[0], loc[1], loc[2] + 0.1), kraft, coll=coll, rot=(0, 0, rot))
    t = box("FoodBag_Fold", (0.15, 0.03, 0.04), (loc[0], loc[1], loc[2] + 0.21), kraft, coll=coll,
            rot=(math.radians(25), 0, rot))
    return [b, t]


def burger_box(coll, loc, rot, open_=False):
    card = mat("Burger_Box", (0.85, 0.75, 0.55), 0.8)
    stripe = mat("Burger_Box_Print", (0.75, 0.18, 0.05), 0.7)
    parts = [box("BurgerBox", (0.13, 0.13, 0.04), (loc[0], loc[1], loc[2] + 0.02), card, coll=coll, rot=(0, 0, rot))]
    if open_:
        dx, dy = math.sin(rot) * 0.07, -math.cos(rot) * 0.07
        parts.append(box("BurgerBox_Lid", (0.13, 0.13, 0.03), (loc[0] - dx, loc[1] - dy, loc[2] + 0.07), card,
                         coll=coll, rot=(math.radians(-70), 0, rot)))
    else:
        parts.append(box("BurgerBox_Lid", (0.132, 0.132, 0.03), (loc[0], loc[1], loc[2] + 0.055), card, coll=coll,
                         rot=(0, 0, rot)))
        parts.append(box("BurgerBox_Print", (0.133, 0.02, 0.005), (loc[0], loc[1], loc[2] + 0.07), stripe, coll=coll,
                         rot=(0, 0, rot)))
    return parts


def fries(coll, loc, rot):
    red = mat("Fries_Box", (0.7, 0.05, 0.04), 0.6)
    yellow = mat("Fries", (0.9, 0.7, 0.2), 0.7)
    parts = [box("FriesBox", (0.09, 0.04, 0.11), (loc[0], loc[1], loc[2] + 0.055), red, coll=coll, rot=(0, 0, rot))]
    for k in range(5):
        parts.append(box("Fry", (0.008, 0.008, 0.08), (loc[0] + (k - 2) * 0.012, loc[1], loc[2] + 0.12), yellow,
                         coll=coll, rot=(math.radians(8 * (k - 2)), 0, rot)))
    return parts


def litter(coll, rng, root, w):
    """A few traces of food on this desk (and maybe one cup on the floor)."""
    makers = [lambda l, r: cola_cup(coll, l, r), lambda l, r: food_bag(coll, l, r),
              lambda l, r: burger_box(coll, l, r, rng.random() < 0.5), lambda l, r: fries(coll, l, r)]
    parts = []
    for _ in range(rng.randint(1, 3)):
        lx = rng.choice((-1, 1)) * rng.uniform(0.3, w / 2 - 0.12)
        loc = root.matrix_basis @ Vector((lx, rng.uniform(-0.3, -0.05), 0.0))
        parts += rng.choice(makers)(loc, rng.uniform(0, 6.28))
    if rng.random() < 0.3:
        loc = (SEAT_X - rng.uniform(0.3, 0.8), root.location.y + rng.uniform(-0.6, 0.6), 0.0)
        parts += cola_cup(coll, loc, rng.uniform(0, 6.28), lying=True)
    return parts


def ue(x, y, z):
    """Blender metres -> Unreal centimetres (Y flips)."""
    return [round(x * 100, 1), round(-y * 100, 1), round(z * 100, 1)]


def ue_from(root, x, y, z):
    p = root.matrix_basis @ Vector((x, y, z))
    return ue(p.x, p.y, p.z)


def build_hero(coll, y0):
    """The hero's mid-2000s PC: a place in the row like the others, switched on. Returns the row end."""
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    beige = mat("Plastic_Beige", (0.72, 0.66, 0.53), 0.55)

    img = bpy.data.images.load(SCREEN_PNG)
    scr = bpy.data.materials.new("Hero_Screen")
    scr.use_nodes = True
    nt = scr.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Emission Color"])
    bsdf.inputs["Emission Strength"].default_value = 1.1
    bsdf.inputs["Roughness"].default_value = 0.15

    w, h = 1.6, 0.75
    hero_y = y0 + w / 2
    root = empty("HeroStation", (TABLE_X, hero_y, h), math.radians(-90), coll=coll)
    desk(root, coll, "soviet", w, h)
    box("Hero_Monitor", (0.46, 0.05, 0.38), (0, 0.12, 0.38), black, root, coll, bevel=0.01)
    plane("Hero_Screen", 0.42, 0.2625 * 1.2, (0, 0.094, 0.39), scr, root, coll)
    box("Hero_Neck", (0.06, 0.06, 0.2), (0, 0.16, 0.1), black, root, coll)
    box("Hero_Foot", (0.24, 0.18, 0.015), (0, 0.13, 0.008), black, root, coll)
    box("Hero_PowerLED", (0.008, 0.004, 0.008), (0.2, 0.094, 0.2), led((0.1, 1.0, 0.2)), root, coll)
    box("Hero_Keyboard", (0.44, 0.15, 0.03), (0, -0.22, 0.015), beige, root, coll, rot=(math.radians(4), 0, 0))
    box("Hero_Mouse", (0.06, 0.1, 0.03), (0.32, -0.22, 0.015), beige, root, coll, bevel=0.012)
    cyl("Hero_Mug", 0.04, 0.1, (-0.55, -0.1, 0.05), mat("Mug", (0.6, 0.1, 0.08), 0.4), root, coll)
    t = tower(root, coll, "Hero_Tower", beige, x=0.5)
    wires = cables(root, coll, [(0, 0.3, 0.1), t], power_x=-0.3)
    LAYOUT["hero"] = {
        "screen": ue_from(root, 0, 0.094, 0.39),
        "screen_normal": [-100.0, 0.0, 0.0],
        "keyboard": ue_from(root, 0, -0.22, 0.03),
        "chair": ue(SEAT_X, hero_y, 0.0),
        "seat_height": 47.0,
        "chair_yaw": 0.0,
        "wake": ue(SEAT_X - 0.65, hero_y - 1.0, 1.0),
    }
    LAYOUT["chunk_length"] = L * 100
    collapse("HeroStation", root, coll, wires)
    c = chair("HeroChair", (SEAT_X, hero_y, 0), math.radians(-90), coll)
    collapse("HeroChair", c, coll)

    # Light the screen casts on the desk and the sleeper (points -X, toward the chair).
    ld = bpy.data.lights.new("Hero_ScreenGlow", "AREA")
    ld.shape = "RECTANGLE"
    ld.size, ld.size_y = 0.42, 0.32
    ld.energy = 6
    ld.color = (0.55, 0.9, 0.8)
    lo = bpy.data.objects.new("Hero_ScreenGlow", ld)
    coll.objects.link(lo)
    lo.location = (TABLE_X + 0.09, hero_y, h + 0.39)
    lo.rotation_euler = (0, math.radians(90), 0)
    return y0 + w


def uv_sphere(name, r, loc, material, coll):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=r)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new(name, me)
    obj.location = loc
    me.materials.append(material)
    return _link(obj, None, coll)


def build_work_lights(coll):
    """Bright temporary lights for layout renders; hidden in the final file."""
    for i, y in enumerate(range(4, int(L), 8)):
        ld = bpy.data.lights.new("Work_%d" % i, "POINT")
        ld.energy = 300
        ld.color = (1.0, 0.85, 0.65)
        ld.shadow_soft_size = 0.8
        o = bpy.data.objects.new(ld.name, ld)
        o.location = (0.2, y, CEIL_Z - 1.2)
        coll.objects.link(o)


def add_camera(scn, name, loc, rot_deg, lens=24, dof_target=None, fstop=2.0):
    cd = bpy.data.cameras.new(name)
    cd.lens = lens
    cam = bpy.data.objects.new(name, cd)
    cam.location = loc
    cam.rotation_euler = Euler([math.radians(a) for a in rot_deg])
    scn.collection.objects.link(cam)
    if dof_target is not None:
        cd.dof.use_dof = True
        cd.dof.focus_object = dof_target
        cd.dof.aperture_fstop = fstop
    return cam


def render(scn, cam, path, res=(1600, 900)):
    scn.camera = cam
    scn.render.resolution_x, scn.render.resolution_y = res
    scn.render.filepath = path
    bpy.ops.render.render(write_still=True, scene=scn.name)


# -------------------------------------------------------------------- main

def main():
    os.makedirs(TEX_DIR, exist_ok=True)
    os.makedirs(RENDER_DIR, exist_ok=True)
    os.makedirs(EXPORT_DIR, exist_ok=True)
    reset()

    os_scene = render_os_screen()

    scn = bpy.data.scenes.new("Scene01_Corridor")
    if bpy.context.window:
        bpy.context.window.scene = scn
    engine = set_engine(scn, "BLENDER_EEVEE_NEXT", "BLENDER_EEVEE")
    print("render engine:", engine)
    scn.render.image_settings.file_format = "PNG"
    try:
        scn.view_settings.view_transform = "AgX"
        scn.view_settings.look = "AgX - Punchy"
    except TypeError:
        pass
    scn.unit_settings.system = "METRIC"
    world = bpy.data.worlds.new("Darkness")
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs["Color"].default_value = (0.002, 0.002, 0.003, 1)
    bg.inputs["Strength"].default_value = 1.0
    scn.world = world

    arch = collection("Architecture")
    walk = collection("Walkway")
    furn = collection("Table_And_Stations")
    hero = collection("Hero")
    work = collection("WorkLights_LayoutOnly")

    build_architecture(arch)
    build_garland(arch)
    build_ventilation(arch)
    build_podyezd_props(arch)
    build_hall(arch)
    build_walkway(walk)
    build_stations(furn)
    build_work_lights(work)

    screen = bpy.data.objects["HeroStation"]
    # First person, slumped in the hero's chair, just woken up, looking at the screen.
    hero_y = -LAYOUT["hero"]["chair"][1] / 100
    cam_wake = add_camera(scn, "Cam_WakeUp_FPV", (SEAT_X - 0.05, hero_y + 0.15, 1.05), (88, 0, -98),
                          lens=22, dof_target=screen, fstop=1.8)
    cam_corr = add_camera(scn, "Cam_Layout_Corridor", (-0.3, 1.0, 1.7), (88, 0, -4), lens=20)
    cam_tbl = add_camera(scn, "Cam_Layout_Stations", (0.2, 30.0, 1.5), (80, 0, -35), lens=24)
    scn.camera = cam_wake

    if DO_RENDER:
        work.hide_render = True
        render(scn, cam_wake, os.path.join(RENDER_DIR, "scene01_wakeup_fpv.png"))
        work.hide_render = False
        render(scn, cam_corr, os.path.join(RENDER_DIR, "scene01_layout_corridor.png"))
        render(scn, cam_tbl, os.path.join(RENDER_DIR, "scene01_layout_stations.png"))
    work.hide_render = True
    work.hide_viewport = True

    bpy.data.scenes.remove(os_scene)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)

    with open(os.path.join(EXPORT_DIR, "scene01_layout.json"), "w") as f:
        json.dump(LAYOUT, f, indent=1)

    # glTF for Unreal (Interchange imports .glb natively). Work lights stay out.
    bpy.ops.export_scene.gltf(filepath=os.path.join(EXPORT_DIR, "scene01_corridor.glb"),
                              export_format="GLB", use_visible=True, export_cameras=True,
                              export_lights=True, export_apply=True)
    print("done:", BLEND_PATH)


main()
