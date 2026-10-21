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
import re

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from character_standard import MIN_DOOR_CLEAR_HEIGHT_M, MIN_DOOR_CLEAR_WIDTH_M   # noqa: E402
TEX_DIR = os.path.join(ROOT, "textures")
RENDER_DIR = os.path.join(ROOT, "renders")
EXPORT_DIR = os.path.join(ROOT, "blender", "export")
BLEND_PATH = os.path.join(ROOT, "blender", "scene01_corridor.blend")
SCREEN_PNG = os.path.join(TEX_DIR, "hero_os_screen.png")
PODYEZD_DIR = os.path.join(TEX_DIR, "podyezd")   # made by tools/gen_textures_fal.py
LANG = os.environ.get("LYUD_LANG", "ru")          # textures with words come from textures/localized/<lang> (tools/gen_localized.py)


def tex_path(file):
    """Textures that carry words are generated per language (localization/assets.json); the rest live in podyezd/."""
    for lang in (LANG, "ru"):
        p = os.path.join(TEX_DIR, "localized", lang, file)
        if os.path.exists(p):
            return p
    return os.path.join(PODYEZD_DIR, file)
PROPS_DIR = os.path.join(ROOT, "blender", "props", "export")   # detailed props (blender/props/README.md)

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
    tex.image = bpy.data.images.load(tex_path(file))
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
    # Same textures as the hall, separate materials: Unreal tints the corridor walls dark.
    paint = tex_mat("Corridor_WallPaint", "wall_paint.jpg", 0.35)
    white = tex_mat("Corridor_Wash", "whitewash.jpg", 0.95)
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
        lights.append(ue(bx + 0.02, by, bz))      # all bulbs, to the far end
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
        parts.append(tbox("Rad_Section", (0.075, 0.09, 0.55), (-0.6 + k * 0.085, HALL_Y0 + 0.1, 0.42), rad, 0.3,
                          coll=coll))
    for z in (0.66, 0.18):
        parts.append(cyl("Rad_Pipe", 0.016, 1.4, (-0.13, HALL_Y0 + 0.1, z), pipe, coll=coll,
                         rot=(0, math.radians(90), 0), verts=10))
    join("Radiator", parts, coll)
    # Mailboxes on the hall's far wall, next to the exit.
    parts = []
    for r in range(3):
        for c in range(5):
            x, z = -0.75 + c * 0.38, 1.15 + r * 0.32
            parts.append(box("Mailbox", (0.36, 0.22, 0.3), (x, HALL_Y0 + 0.11, z), box_m, coll=coll))
            parts.append(box("Mailbox_Door", (0.3, 0.01, 0.24), (x, HALL_Y0 + 0.225, z), door_m, coll=coll))
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


def terrain_height(x, d):
    """Ground outside, d metres out from the portal face: the club is dug into a mound on a hilltop
    (6 m of earth over it, flush with the portal top); in front a grassy crown, then the slope falls from 9 m out,
    ~45 m to the sea."""
    mound = 6.0 + min(max(-d, 0.0), 30.0) * 0.08
    if abs(x) <= 6.2:                                # behind the portal / the open ground in front of it
        h = mound if d <= 0.0 else 0.0
    else:
        front = min((abs(x) - 6.2) * 1.1, 4.0)       # the mound's face curves away from the portal
        k = min(1.0, max(0.0, (abs(x) - 12.0) / 25.0))
        mound *= 1 - k * k * (3 - 2 * k)
        t = min(1.0, max(0.0, (d - front) / 7.0))
        h = mound * (1 - t * t * (3 - 2 * t))
    if d > 9.0:                                      # the hill falls away toward the sea
        u = d - 9.0
        h -= 46.0 * (1 - math.exp(-0.9 * u / 46.0))  # steepest at the brow, so the sea shows below it
    if abs(x) > 40.0:
        h -= (abs(x) - 40.0) * 0.12
    n = math.sin(x * 0.21 + d * 0.13) * 0.25 + math.sin(x * 0.07 - d * 0.05) * 0.6
    return h + n * min(1.0, max(0.0, (d - 3.0) / 10.0))


def build_outside(coll, dx0, dx1, dh, face_y):
    """Concrete portal around the blast door, the grassy hill, the sea below and a sky dome."""
    conc = tex_mat("Outside_Concrete", "ceiling.jpg", 0.9)
    t = 0.4
    fy = face_y - t / 2
    px0, px1, ph = -6.2, 6.2, 6.0
    o0, o1, oh = dx0 - 0.25, dx1 + 0.25, dh + 0.1    # a little wider than the door, so the leaf swings clear
    tbox("Outside_Portal_L", (o0 - px0, t, ph + 0.5), ((px0 + o0) / 2, fy, ph / 2 - 0.25), conc, 2.0, coll=coll)
    tbox("Outside_Portal_R", (px1 - o1, t, ph + 0.5), ((o1 + px1) / 2, fy, ph / 2 - 0.25), conc, 2.0, coll=coll)
    tbox("Outside_Portal_Top", (o1 - o0, t, ph - oh), ((o0 + o1) / 2, fy, (oh + ph) / 2), conc, 2.0, coll=coll)
    tbox("Outside_Threshold", (o1 - o0, t + 0.1, 0.3), ((o0 + o1) / 2, fy, -0.15), conc, 2.0, coll=coll)
    tbox("Outside_Apron", (6.0, 3.0, 0.3), ((o0 + o1) / 2, face_y - t - 1.5, -0.13), conc, 2.0, coll=coll)
    # reveals between the hall wall and the portal so the doorway has depth, and a lintel
    tbox("Outside_Reveal_L", (0.25, t, oh), (o0 + 0.125, fy, oh / 2), conc, 2.0, coll=coll)
    tbox("Outside_Reveal_R", (0.25, t, oh), (o1 - 0.125, fy, oh / 2), conc, 2.0, coll=coll)
    # ground: a height grid, finer near the door
    grass = tex_mat("Outside_Grass", "grass.jpg", 0.95)
    face = fy - t / 2                                  # Blender y of the portal face
    ds = [-60, -40, -25, -15, -8, -4, -2, -1, -0.3, 0.0, 0.01]
    d = 0.5
    while d < 420:
        ds.append(round(d, 2))
        d += 0.5 if d < 10 else (1.5 if d < 40 else (6 if d < 150 else 20))
    xs = sorted({round(v, 2) for v in [i * 0.5 for i in range(-30, 31)] + [i * 2.0 for i in range(-30, 31)] +
                 [i * 10.0 for i in range(-25, 26)] + [-6.2, 6.2]})
    me = bpy.data.meshes.new("Outside_Ground")
    bm = bmesh.new()
    grid = [[bm.verts.new((x, face - dd, terrain_height(x, dd))) for x in xs] for dd in ds]
    for i in range(len(ds) - 1):
        for j in range(len(xs) - 1):
            if ds[i] == 0.0 and abs(xs[j]) <= 6.2 and abs(xs[j + 1]) <= 6.2:
                continue                             # the portal stands in the drop from the mound
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    uvl = bm.loops.layers.uv.new()
    for f in bm.faces:
        if f.normal.z < 0:
            f.normal_flip()
        for lp in f.loops:
            lp[uvl].uv = (lp.vert.co.x / 3.0, lp.vert.co.y / 3.0)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(grass)
    g = bpy.data.objects.new("Outside_Ground", me)
    coll.objects.link(g)
    # the sea, far below
    sea = tex_mat("Outside_Sea", "sea_water.jpg", 0.08)
    sme = bpy.data.meshes.new("Outside_Sea")
    bm = bmesh.new()
    vs = [bm.verts.new((x, face - y, -43.0)) for x, y in ((-3000, 90), (3000, 90), (3000, 3000), (-3000, 3000))]
    f = bm.faces.new(vs)
    if f.normal.z < 0:
        f.normal_flip()
    uvl = bm.loops.layers.uv.new()
    for lp in f.loops:
        lp[uvl].uv = (lp.vert.co.x / 20.0, lp.vert.co.y / 20.0)
    bm.to_mesh(sme)
    bm.free()
    sme.materials.append(sea)
    so = bpy.data.objects.new("Outside_Sea", sme)
    coll.objects.link(so)
    # sky dome (Unreal renders it as unlit sky): equirect panorama, horizon on the equator
    sky = screen_on("sky", "sky_panorama.jpg", 1.0)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=2800.0,
                                         location=(0.0, face - 40.0, 0.0))
    dome = bpy.context.active_object
    dome.name = "Outside_SkyDome"
    for c in list(dome.users_collection):
        c.objects.unlink(dome)
    coll.objects.link(dome)
    dome.data.materials.append(sky)
    bm = bmesh.new()
    bm.from_mesh(dome.data)
    bmesh.ops.reverse_faces(bm, faces=bm.faces)       # seen from inside
    bm.to_mesh(dome.data)
    bm.free()
    LAYOUT["outside"] = {"portal_face_y": ue(0, face, 0)[1], "center": ue(0, face - 25.0, 0),
                         "sea_z": -4300.0, "starlings_hint": ue(0, face - 120.0, 10.0)}


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
    # The blast door prop is stretched until the doorway, with the leaf swung open, passes the character
    # standard (tools/character_standard.py): frame clear 1.3 x 2.03 m, hinge 0.52 m inside the opening,
    # the open leaf's 0.32 m thickness stands in the way.
    bd_sx = max(1.0, (MIN_DOOR_CLEAR_WIDTH_M + 0.32) / 1.17 * 1.04)
    bd_sz = max(1.0, (MIN_DOOR_CLEAR_HEIGHT_M + 0.03) / 2.03)
    half_open = 0.75 * bd_sx + 0.02
    assert half_open <= 1.15, "blast door too wide for the hall wall: %.2f" % half_open
    dcx_ = -hw + 0.12 + half_open + 0.0
    dx0, dx1, dh = dcx_ - half_open, dcx_ + half_open, 2.3 * bd_sz + 0.02   # frame opening in the far wall
    wall_piece("Hall_Far_A", -hw - 0.5, dx0, y0 - 0.5, y0, 0, h, coll)
    wall_piece("Hall_Far_B", dx1, hw + 0.5, y0 - 0.5, y0, 0, h, coll)
    wall_piece("Hall_Far_C", dx0, dx1, y0 - 0.5, y0, dh, h, coll)
    if have_prop("blastdoor"):
        # hermetic shelter door in the far wall, closed (the street comes later)
        place("blastdoor", floor_at(((dx0 + dx1) / 2, y0 - 0.25, 0), math.pi) @ Matrix.Diagonal((bd_sx, 1.0, bd_sz, 1.0)),
              coll, label="ExitDoor")
        LAYOUT["exit_door"] = {"leaves": ["ExitDoor_Leaf"], "riders": ["ExitDoor_Wheel"], "open_angles": [-100.0],
                               "center": ue((dx0 + dx1) / 2, y0 - 0.25, 1.0)}
    else:
        door_paint = mat("Door_GreenMetal", (0.12, 0.2, 0.15), 0.4, 0.5)
        join("ExitDoor", [box("Door_Leaf", (dx1 - dx0 - 0.04, 0.05, dh - 0.03),
                              ((dx0 + dx1) / 2, y0 - 0.1, (dh - 0.03) / 2), door_paint, coll=coll)], coll)
    build_outside(coll, dx0, dx1, dh, y0 - 0.5)
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
    build_chandelier(coll)
    build_partition(coll)

    # ---- the owner's booth: the whole right side of the hall (x 1.0..hw), closed off by a
    # podyezd-style wall with a counter and two sliding windows, and a doorway with a curtain.
    ply = mat("Booth_Plywood", (0.42, 0.3, 0.18), 0.6)
    frame_m = mat("Booth_WindowFrame", (0.55, 0.42, 0.28), 0.5)
    glass_m = mat("Booth_Glass", (0.6, 0.7, 0.7), 0.05, transmission=1.0, alpha=0.2)
    curtain_m = mat("Booth_Curtain", (0.35, 0.05, 0.06), 0.9)
    rail_m = mat("Booth_Rail", (0.1, 0.1, 0.1), 0.4, 0.8)
    bx0, bt = 1.0, 0.12                      # front wall line and thickness
    by_far, by_near = y0, -0.3               # from the far wall to the back wall of the hall
    win0, win1, wz0, wz1 = -5.9, -1.9, 0.95, 2.1          # big sliding windows along most of the wall
    gb0, gb1 = 2.3, 2.9                                    # decorative glass-block band, full width
    door0, door1, dz = -1.4, -0.6, 2.1
    xa, xb = bx0 - bt / 2, bx0 + bt / 2
    # below the glass-block band: piers, counter wall, the doorway
    wall_piece("Booth_Wall_A", xa, xb, by_far, win0, 0, gb0, coll)
    wall_piece("Booth_Wall_B", xa, xb, win1, door0, 0, gb0, coll)
    wall_piece("Booth_Wall_C", xa, xb, door1, by_near, 0, gb0, coll)
    wall_piece("Booth_Wall_Counter", xa, xb, win0, win1, 0, wz0, coll)
    wall_piece("Booth_Wall_OverWindow", xa, xb, win0, win1, wz1, gb0, coll)
    wall_piece("Booth_Wall_OverDoor", xa, xb, door0, door1, dz, gb0, coll)
    wall_piece("Booth_Wall_Top", xa, xb, by_far, by_near, gb1, h, coll)
    # close the corner where the hall meets the corridor (the corridor starts at x = W/2)
    wall_piece("Booth_Wall_Corner", xa, W / 2 + 0.05, by_near - bt, by_near, 0, h, coll)
    # glass blocks: a band of 19 cm blocks set in the whitewashed upper wall
    gblock = mat("GlassBlock", (0.75, 0.85, 0.85), 0.08, transmission=0.9, alpha=0.55)
    mortar = mat("GlassBlock_Mortar", (0.7, 0.68, 0.62), 0.9)
    blocks = [box("GB_Frame", (bt, by_near - by_far, gb1 - gb0), (bx0, (by_far + by_near) / 2, (gb0 + gb1) / 2), mortar,
                  coll=coll)]
    blocks[0].scale = (0.4, 1.0, 1.0)
    rows = 3
    step = (gb1 - gb0) / rows
    y = by_far + 0.02
    while y + 0.19 <= by_near:
        for r in range(rows):
            blocks.append(box("GlassBlock", (bt + 0.01, 0.18, step - 0.012), (bx0, y + 0.095, gb0 + step * (r + 0.5)),
                              gblock, coll=coll, bevel=0.01))
        y += 0.195
    join("Booth_GlassBlocks", blocks, coll)
    parts = []
    # counter board through the window, outside and in
    parts.append(box("Booth_CounterTop", (0.5, win1 - win0 + 0.2, 0.04), (bx0, (win0 + win1) / 2, wz0 + 0.02), ply,
                     coll=coll, bevel=0.005))
    # window frame and two sliding glass panes on a track; the left one is pushed open
    wcy, ww = (win0 + win1) / 2, win1 - win0
    for z in (wz0 + 0.03, wz1 - 0.02):
        parts.append(box("Window_Track", (0.1, ww, 0.04), (bx0, wcy, z), frame_m, coll=coll))
    for y in (win0 + 0.02, win1 - 0.02):
        parts.append(box("Window_Jamb", (0.1, 0.04, wz1 - wz0), (bx0, y, (wz0 + wz1) / 2), frame_m, coll=coll))
    pane_w, pane_h = ww / 2 + 0.04, wz1 - wz0 - 0.08
    for k, (px, py) in enumerate(((bx0 - 0.02, win1 - pane_w / 2 - 0.03),            # closed, right half
                                  (bx0 + 0.02, win1 - pane_w / 2 - 0.9))):           # slid most of the way over it
        cz = (wz0 + wz1) / 2 + 0.01
        parts.append(box("Pane_Glass", (0.006, pane_w - 0.06, pane_h - 0.06), (px, py, cz), glass_m, coll=coll))
        for dy in (-1, 1):
            parts.append(box("Pane_Stile", (0.03, 0.03, pane_h), (px, py + dy * (pane_w / 2 - 0.015), cz), frame_m,
                             coll=coll))
        for dz_ in (-1, 1):
            parts.append(box("Pane_Rail", (0.03, pane_w, 0.03), (px, py, cz + dz_ * (pane_h / 2 - 0.015)), frame_m,
                             coll=coll))
        parts.append(box("Pane_Handle", (0.02, 0.02, 0.08), (px - 0.02, py - pane_w / 2 + 0.06, cz), rail_m, coll=coll))
    # curtain across the doorway
    parts.append(cyl("Curtain_Rail", 0.01, door1 - door0, (bx0 - 0.08, (door0 + door1) / 2, dz - 0.05), rail_m,
                     coll=coll, rot=(math.radians(90), 0, 0), verts=8))
    for i in range(9):
        yy = door0 + 0.04 + i * (door1 - door0 - 0.08) / 8
        parts.append(box("Curtain_Fold", (0.03, 0.1, dz - 0.1), (bx0 - 0.08 + (0.02 if i % 2 else -0.02), yy,
                                                                    (dz - 0.1) / 2), curtain_m, coll=coll))
    # inside: desk with an old PC behind the window, shelves along the back, a stool, a lamp
    desk_m = mat("Desk_SovietWood", (0.16, 0.08, 0.04), 0.3)
    beige = mat("Plastic_Beige", (0.72, 0.66, 0.53), 0.55)
    dy0 = by_far + 0.8                       # desk tucked into the far corner
    parts.append(box("Admin_Desk", (0.7, 1.6, 0.04), (hw - 0.4, dy0, 0.76), desk_m, coll=coll))
    for yy in (dy0 - 0.7, dy0 + 0.7):
        parts.append(box("Admin_DeskLeg", (0.6, 0.04, 0.74), (hw - 0.4, yy, 0.37), desk_m, coll=coll))
    parts.append(box("Admin_CRT", (0.38, 0.36, 0.34), (hw - 0.3, dy0 - 0.2, 0.95), beige, coll=coll, bevel=0.015))
    parts.append(plane("Admin_Screen", 0.28, 0.21, (hw - 0.495, dy0 - 0.2, 0.97), screen_on("live_glitch",
                       "screen_cctv.jpg", 1.1), coll=coll))
    parts[-1].rotation_euler = (0, 0, math.radians(-90))
    parts.append(box("Admin_Keyboard", (0.16, 0.44, 0.03), (hw - 0.7, dy0 - 0.2, 0.795), beige, coll=coll))
    parts.append(box("Admin_Ledger", (0.25, 0.32, 0.03), (bx0 + 0.1, -3.9, wz0 + 0.055), mat("Ledger", (0.1, 0.2, 0.35), 0.6),
                     coll=coll, rot=(0, 0, math.radians(8))))
    parts.append(cyl("Admin_Bell", 0.04, 0.03, (bx0 - 0.12, -2.9, wz0 + 0.055), mat("Brass", (0.7, 0.55, 0.25), 0.3, 1.0),
                     coll=coll, verts=16, r2=0.015))
    shelf = mat("Shelf_Metal", (0.3, 0.3, 0.28), 0.5, 0.6)
    cols = [(0.5, 0.1, 0.05), (0.15, 0.35, 0.12), (0.6, 0.5, 0.2), (0.2, 0.2, 0.45), (0.55, 0.55, 0.5)]
    for lvl in range(4):
        z = 0.5 + lvl * 0.45
        parts.append(box("Shelf", (0.35, 1.4, 0.02), (hw - 0.18, -2.9, z), shelf, coll=coll))
        for j in range(5):
            c = cols[(lvl + j) % len(cols)]
            if (lvl + j) % 3 == 0:
                parts.append(cyl("Jar", 0.05, 0.16, (hw - 0.2, -3.5 + j * 0.28, z + 0.09),
                                 mat("Jar_%d" % ((lvl + j) % 5), c, 0.2, transmission=0.6, alpha=0.7), coll=coll))
            else:
                parts.append(box("Box", (0.22, 0.2, 0.16 + 0.05 * (j % 2)), (hw - 0.2, -3.5 + j * 0.28, z + 0.1),
                                 mat("Crate_%d" % ((lvl + j) % 5), c, 0.8), coll=coll))
    join("AdminBooth", parts, coll)
    lm = floor_at((hw - 0.3, dy0 + 0.55, 0.78), math.radians(90))
    place("lamp_soviet", lm, coll, label="Admin_Lamp")
    LAYOUT["hall_lights"].append({"pos": ue(*(anchor("lamp_soviet", lm, "Lamp_Bulb") + Vector((0, 0, -0.03)))),
                                  "color": [1.0, 0.68, 0.38], "cd": 4.0, "radius": 350, "shadows": True})
    place("chair_stool", floor_at((hw - 1.3, dy0 - 0.2, 0), math.radians(-90)), coll, label="Admin_Stool")
    # the owner sits facing his monitor (CCTV of the hall) about 95 cm away
    # He gets up when someone stands at the window and comes over to it (Unreal yaw 180 faces the hall).
    LAYOUT["owner"] = {"chair": ue(hw - 1.3, dy0 - 0.2, 0), "sitter_yaw": 0.0,
                       "window": ue(bx0 + 0.42, (win0 + win1) / 2, 0), "window_yaw": 180.0,
                       "zone_center": ue(bx0 - 0.6, (win0 + win1) / 2, 0.9),
                       "zone_extent": [55.0, abs(win1 - win0) * 50, 90.0]}
    # a warm bare bulb under the booth ceiling, now that it is closed in
    bulb = mat("Hall_Bulb", (1.0, 0.85, 0.6), 0.2, emit=WARM, strength=25.0)
    cord = mat("Lamp_Cord", (0.02, 0.02, 0.02), 0.6)
    join("Booth_Pendant", [cyl("Cord", 0.004, 0.5, (2.0, -4.2, h - 0.25), cord, coll=coll, verts=6),
                           uv_sphere("Bulb", 0.03, (2.0, -4.2, h - 0.53), bulb, coll)], coll)
    LAYOUT["hall_lights"].append({"pos": ue(2.0, -4.2, h - 0.6), "color": [1.0, 0.68, 0.38], "cd": 10.0,
                                  "radius": 600, "shadows": True})
    # air humidifier puffing steam (Unreal draws the steam on the cards above it)
    hx, hy = hw - 0.3, -0.75
    hm = floor_at((hx, hy, 0), math.radians(90))
    if have_prop("humidifier"):
        place("humidifier", hm, coll, label="Humidifier")
        nozzle = anchor("humidifier", hm, "humidifier_SteamPoint")
    else:
        hum_body = mat("Humidifier_White", (0.85, 0.87, 0.86), 0.35)
        join("Humidifier", [cyl("Hum_Base", 0.13, 0.12, (hx, hy, 0.06), hum_body, coll=coll, verts=24),
                            cyl("Hum_Nozzle", 0.025, 0.36, (hx, hy, 0.3), hum_body, coll=coll, verts=12)], coll)
        nozzle = Vector((hx, hy, 0.46))
    hx, hy, hz = nozzle
    steam = mat("Humidifier_Steam", (1, 1, 1), 1.0)
    cards = []
    for k in range(3):
        c = plane("Steam_Card", 0.35, 0.8, (hx, hy, hz + 0.4), steam, coll=coll)
        c.rotation_euler = (0, 0, math.radians(60 * k))
        cards.append(c)
    join("Humidifier_Steam", cards, coll)
    LAYOUT["steam"] = {"base_z": round(hz * 100, 1), "top_z": round((hz + 0.8) * 100, 1), "pos": ue(hx, hy, hz)}
    # framed landscapes in gilded baguette frames
    if have_prop("frame_large"):
        # frames hang with their back on a wall at +Y and the picture facing -Y
        place("frame_large", floor_at((hw - 0.045, -4.4, 1.6), math.radians(-90)), coll, label="Painting_0")
        place("frame_medium", floor_at((2.0, by_far + 0.04, 1.75), math.radians(180)), coll, label="Painting_1")
        place("frame_small", floor_at((2.1, by_near - 0.04, 1.7), 0.0), coll, label="Painting_2")
        PAINTINGS = []
    else:
        PAINTINGS = None
    gold = mat("Frame_Gilt", (0.75, 0.55, 0.2), 0.3, 1.0)
    for k, (px, py, rz, w_, h_, img) in ([] if PAINTINGS is not None else enumerate([
            (hw - 0.02, -4.6, 90, 0.9, 0.6, "landscape_1.jpg"),
            (2.0, by_far + 0.02, 0, 0.7, 0.5, "landscape_2.jpg"),
            (2.1, by_near - 0.02, 180, 0.6, 0.8, "landscape_3.jpg")])):
        canvas = tex_mat("Landscape_%d" % k, img, 0.8) if os.path.exists(os.path.join(PODYEZD_DIR, img)) else \
            mat("Landscape_%d" % k, (0.35, 0.4, 0.3), 0.8)
        root = empty("Painting_%d" % k, (px, py, 1.75), math.radians(rz), coll=coll)
        box("Frame_Top", (w_ + 0.1, 0.05, 0.06), (0, 0.025, h_ / 2 + 0.03), gold, root, coll, bevel=0.01)
        box("Frame_Bottom", (w_ + 0.1, 0.05, 0.06), (0, 0.025, -h_ / 2 - 0.03), gold, root, coll, bevel=0.01)
        box("Frame_L", (0.06, 0.05, h_), (-w_ / 2 - 0.03, 0.025, 0), gold, root, coll, bevel=0.01)
        box("Frame_R", (0.06, 0.05, h_), (w_ / 2 + 0.03, 0.025, 0), gold, root, coll, bevel=0.01)
        cv = plane("Canvas", w_, h_, (0, 0.04, 0), canvas, root, coll)
        cv.rotation_euler = (0, 0, math.pi)          # face back out of the frame, into the room
        collapse("Painting_%d" % k, root, coll)

    # ---- opposite: drinks vending machine, a chair with a newspaper
    vx, vy = -hw + 0.4, -2.2
    # front (-Y on the prop) turned to face +X, into the hall
    place("vending_machine", floor_at((vx, vy, 0), math.radians(90)), coll, label="VendingMachine")
    LAYOUT["hall_lights"].append({"pos": ue(vx + 0.75, vy, 1.2), "color": [0.85, 0.9, 1.0], "cd": 3.0, "radius": 300})
    LAYOUT["vending"] = ue(vx, vy, 1.0)
    # wide snack machine
    fx, fy = -hw + 0.42, -3.75
    if have_prop("vending_snacks"):
        place("vending_snacks", floor_at((fx, fy, 0), math.radians(90)), coll, label="FoodVendingMachine")
        LAYOUT["hall_lights"].append({"pos": ue(fx + 0.8, fy, 1.2), "color": [0.85, 0.92, 1.0], "cd": 2.5,
                                      "radius": 300})
    else:
        food_vending_placeholder(coll, fx, fy)
    # a chair with today's newspaper by the exit
    place("chair_bentwood", floor_at((-hw + 0.45, -5.2, 0), math.radians(-90)), coll, label="Hall_Chair")
    paper = mat("Newspaper", (0.8, 0.8, 0.75), 0.9)
    news = tex_mat("Newspaper_Print", "newspaper.jpg", 0.9)
    n1 = plane("Newspaper", 0.3, 0.42, (0, 0, 0), news, coll=coll)
    n1.rotation_euler = (math.radians(-90), 0, math.radians(15))
    n1.location = (-hw + 0.45, -5.2, 0.485)
    n2 = box("Newspaper_Fold", (0.28, 0.4, 0.004), (-hw + 0.47, -5.18, 0.48), paper, coll=coll,
             rot=(0, 0, math.radians(15)))
    join("Hall_Newspaper", [n1, n2], coll)


def build_partition(coll):
    """Wall across the corridor mouth with a wide double door (open by default; Unreal swings the
    leaves shut with E) and a glass-block panel beside it."""
    white = tex_mat("Podyezd_Whitewash", "whitewash.jpg", 0.95)
    paint = tex_mat("Podyezd_WallPaint", "wall_paint.jpg", 0.35)
    t = 0.12
    dcx, dw, dh = -0.4, 2.1, 2.37            # door_double: 2.1 m wide frame, 1.8 m clear opening
    x0, x1 = dcx - dw / 2, dcx + dw / 2
    gx0, gx1, gh = x1, W / 2 + 0.01, WALL_H  # glass-block panel to the right of the door, under the slope
    outline_l = [(-W / 2 - 0.01, 0), (x0, 0), (x0, CEIL_Z), (-TOP_HALF, CEIL_Z), (-W / 2, WALL_H)]
    prism("Partition_L", outline_l, -t, 0.0, white, coll)
    tbox("Partition_Paint_L", (abs(x0 + W / 2), t + 0.02, PANEL_H), ((x0 - W / 2) / 2, -t / 2, PANEL_H / 2), paint, 1.0,
         coll=coll)
    prism("Partition_R", [(gx0, gh), (W / 2, gh), (TOP_HALF, CEIL_Z), (gx0, CEIL_Z)], -t, 0.0, white, coll)
    prism("Partition_Top", [(x0, dh), (x1, dh), (x1, CEIL_Z), (x0, CEIL_Z)], -t, 0.0, white, coll)
    m = floor_at((dcx, -t / 2, 0), 0.0)
    if have_prop("door_double"):
        # leaves stand open toward the hall (-Y): LeafL turns negative, LeafR positive about Z
        place("door_double", m, coll, label="PartitionDoor", hinge_angles={"LeafL": -95.0, "LeafR": 95.0})
    if have_prop("glassblock_panel"):
        place("glassblock_panel", floor_at(((gx0 + gx1) / 2, -t / 2, 0), 0.0) @ Matrix.Diagonal(
            ((gx1 - gx0) / 1.30, 1.0, gh / 2.44, 1.0)), coll, label="Partition_GlassBlocks")
    # Unreal yaw flips sign against Blender's Z rotation
    LAYOUT["door"] = {"leaves": ["PartitionDoor_LeafL", "PartitionDoor_LeafR"], "open_angles": [95.0, -95.0],
                      "center": ue(dcx, -t / 2, 1.0)}


def build_chandelier(coll):
    """Crystal chandelier in the middle of the hall (stand-in until the detailed prop arrives)."""
    h = CEIL_Z
    cx, cy = -1.0, HALL_Y0 / 2 - 0.15       # middle of the free part of the hall (the owner's booth takes x 1..3)
    if have_prop("chandelier"):
        m = floor_at((cx, cy, h), 0.0)
        place("chandelier", m, coll, label="Chandelier")
        for k in range(0, 24, 4):             # a few of its candle bulbs actually light the room
            p = anchor("chandelier", m, "chandelier_Light_%02d" % k)
            LAYOUT["hall_lights"].append({"pos": ue(*p), "color": [1.0, 0.75, 0.5], "cd": 2.0, "radius": 350})
    else:
        brass = mat("Brass", (0.7, 0.55, 0.25), 0.3, 1.0)
        crystal = mat("Crystal", (0.95, 0.97, 1.0), 0.02, transmission=1.0, alpha=0.35)
        candle = mat("Chandelier_Bulb", (1.0, 0.85, 0.6), 0.2, emit=WARM, strength=18.0)
        parts = [cyl("Ch_Rod", 0.012, 0.5, (cx, cy, h - 0.25), brass, coll=coll, verts=8),
                 cyl("Ch_Ring", 0.38, 0.03, (cx, cy, h - 0.75), brass, coll=coll, verts=32),
                 cyl("Ch_Ring", 0.2, 0.03, (cx, cy, h - 0.6), brass, coll=coll, verts=24)]
        for k in range(10):
            a = k * 2 * math.pi / 10
            px, py = cx + math.cos(a) * 0.38, cy + math.sin(a) * 0.38
            parts.append(cyl("Ch_Candle", 0.012, 0.06, (px, py, h - 0.7), candle, coll=coll, verts=8))
            for j in range(3):
                parts.append(cyl("Ch_Drop", 0.015, 0.06, (px + math.cos(a) * 0.04 * j, py + math.sin(a) * 0.04 * j,
                                                         h - 0.82 - 0.07 * j), crystal, coll=coll, verts=6, r2=0.003))
        for k in range(16):
            a = k * 2 * math.pi / 16
            parts.append(cyl("Ch_Drop", 0.012, 0.09, (cx + math.cos(a) * 0.2, cy + math.sin(a) * 0.2, h - 0.68),
                             crystal, coll=coll, verts=6, r2=0.002))
        join("Chandelier", parts, coll)
    LAYOUT["hall_lights"].append({"pos": ue(cx, cy, h - 0.6), "color": [1.0, 0.72, 0.45], "cd": 30.0,
                                  "radius": 1100, "shadows": True})


def build_posters(coll):
    """ATON computer club posters (the Eye of Horus logo) taped to the whitewash: two designs, over the desks
    on the right wall, on the aisle wall opposite and in the hall."""
    pw, ph = 0.55, 0.73
    mats = [tex_mat("Poster_Aton_1", "poster_aton_1.jpg", 0.7), tex_mat("Poster_Aton_2", "poster_aton_2.jpg", 0.7)]
    spots = [  # (x, y, z centre, wall faces +x (1) or -x (-1), design)
        (W / 2 - 0.012, 3.1, 1.74, -1, 0), (W / 2 - 0.012, 9.6, 1.74, -1, 1), (W / 2 - 0.012, 18.7, 1.74, -1, 0),
        (W / 2 - 0.012, 21.6, 1.74, -1, 1),
        (-W / 2 + 0.012, 1.6, 1.55, 1, 1), (-W / 2 + 0.012, 12.2, 1.55, 1, 0), (-W / 2 + 0.012, 21.0, 1.55, 1, 1),
        (-HALL_HW + 0.012, -2.2, 2.6, 1, 0), (-HALL_HW + 0.012, -3.75, 2.6, 1, 1),     # above the two vending machines
    ]
    for k, (x, y, z, face, design) in enumerate(spots):
        # plane() faces -Y: a quarter turn about Z points it along +X or -X
        pl = plane("Poster_Aton_%d" % k, pw, ph, (x, y, z), mats[design], coll=coll)
        pl.rotation_euler = (0, 0, face * math.pi / 2)
        POSTERS.append(pl)


POSTERS = []


def check_posters():
    """Reports every poster that overlaps another poster or anything else standing against its wall."""
    bpy.context.view_layer.update()
    corners = lambda o: [o.matrix_world @ Vector(c) for c in o.bound_box]

    def box_of(o):
        cs = corners(o)
        return [min(c[i] for c in cs) for i in range(3)], [max(c[i] for c in cs) for i in range(3)]

    bad = 0
    for pl in POSTERS:
        lo, hi = box_of(pl)
        for o in bpy.data.objects:
            if o is pl or o.type != "MESH" or o.hide_render:
                continue
            if any(t in o.name for t in ("Wall", "Slope", "Floor", "Ceiling", "Partition_L", "Partition_R", "Partition_Top", "Hall_Back", "Hall_Far", "Booth_Wall", "Outside")):
                continue
            lo2, hi2 = box_of(o)
            if all(lo[i] < hi2[i] - 0.01 and hi[i] > lo2[i] + 0.01 for i in range(3)) or \
                    (o.name.startswith("Poster") and all(lo[i] <= hi2[i] and hi[i] >= lo2[i] for i in range(3))):
                print("POSTER OVERLAP:", pl.name, "with", o.name)
                bad += 1
    print("posters checked: %d, overlaps: %d" % (len(POSTERS), bad))


def food_vending_placeholder(coll, fx, fy):
    """Blocky stand-in for the snack machine, used if blender/props has no vending_snacks yet."""
    food_body = mat("FoodVending_Body", (0.15, 0.17, 0.2), 0.4, 0.4)
    food_glass = mat("FoodVending_Glass", (0.6, 0.7, 0.75), 0.05, transmission=1.0, alpha=0.2)
    food_lit = mat("FoodVending_Light", (0.9, 0.95, 1.0), 0.3, emit=(0.85, 0.92, 1.0), strength=3.0)
    fparts = [box("Food_Body", (0.8, 1.3, 1.85), (fx, fy, 0.925), food_body, coll=coll, bevel=0.01),
              box("Food_Window", (0.02, 0.95, 1.3), (fx + 0.41, fy - 0.12, 1.1), food_glass, coll=coll),
              box("Food_Back", (0.02, 0.95, 1.3), (fx + 0.1, fy - 0.12, 1.1), food_lit, coll=coll),
              box("Food_Panel", (0.03, 0.25, 0.6), (fx + 0.41, fy + 0.5, 1.2), mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4),
                  coll=coll),
              box("Food_Flap", (0.03, 0.8, 0.2), (fx + 0.41, fy - 0.1, 0.3), mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4),
                  coll=coll)]
    snack_cols = [(0.8, 0.1, 0.05), (0.9, 0.7, 0.1), (0.1, 0.4, 0.8), (0.1, 0.6, 0.2), (0.6, 0.2, 0.6)]
    for r in range(5):
        z = 0.55 + r * 0.25
        fparts.append(box("Food_Shelf", (0.3, 0.95, 0.01), (fx + 0.25, fy - 0.12, z), food_body, coll=coll))
        for cidx in range(6):
            fparts.append(box("Snack", (0.06, 0.12, 0.15), (fx + 0.33, fy - 0.52 + cidx * 0.16, z + 0.08),
                              mat("Snack_%d" % ((r + cidx) % 5), snack_cols[(r + cidx) % 5], 0.5), coll=coll))
    join("FoodVendingMachine", fparts, coll)
    LAYOUT["hall_lights"].append({"pos": ue(fx + 0.8, fy, 1.2), "color": [0.85, 0.92, 1.0], "cd": 2.5, "radius": 300})


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


# ------------------------------------------------------------ prop library
# Each detailed prop is imported once; its parts are merged into one mesh (plus a separate
# screen mesh) and every placement is a linked copy, so Unreal imports one static mesh per
# prop and reuses it everywhere, including the repeated corridor in the distance.

PROPS = {}
PROP_COUNT = {}
PLACED_SCREENS = []

# Visitors (the humans thread's characters) at reachable places: station -> (person, screen content).
# Their chairs are set square to the desk and their screens show live content (animated in Unreal).
VISITORS = {
    0: ("Guy_b", "video"),         # laptop, leaning back watching a video
    1: ("Streamer", "racing"),     # PS2 racing game; chat on a second screen, webcam on the TV
    4: ("Girl_a", "zx"),           # ZX Spectrum game on the keyboard computer
    5: ("Guy_a", "modern_game"),   # gaming PC, shooter
    6: ("Kid_boy", "platformer"),  # Mega Drive platformer on the TV, gamepad in hand
    7: ("Kid_girl", "chat"),       # iMac, chin in hand, reading a chat
    8: ("Girl_b", "feed"),         # beige PC, social feed
    # past the hero, in front of the invisible wall: more people so the wall looks like a crowd, not a barrier
    11: ("Guy_b", "feed"),
    12: ("Girl_a", "video"),
    13: ("Guy_a", "chat"),
}
LIVE_SCREENS = {"video": "screen_video.jpg", "cctv": "screen_cctv.jpg", "feed": "screen_feed.jpg", "chat": "screen_chat.jpg", "racing": "screen_racing.jpg",
                "zx": "screen_zx.jpg", "platformer": "screen_platformer.jpg",
                "modern_game": "screen_modern_game.jpg"}
ANCHOR_MATERIALS = ("Lamp_Bulb", "GrowLamp_Purple")   # where lights go, measured on the prop itself


def prop(name):
    """Imports blender/props/export/<name>.glb once. Returns {"body", "Screen", "Blades": mesh or None,
    "anchors": {material: local centre}}."""
    if name in PROPS:
        return PROPS[name]
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(PROPS_DIR, name + ".glb"))
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:                       # the importer suffixes shared materials (.001); reuse the first
        if o.type != "MESH":
            continue
        for i, m in enumerate(o.data.materials):
            base = re.sub(r"\.\d{3}$", "", m.name) if m else None
            if m and base != m.name and base in bpy.data.materials:
                o.data.materials[i] = bpy.data.materials[base]
    empties = [o for o in new if o.type == "EMPTY"]
    meshes = [o for o in new if o.type == "MESH"]
    # which top-level node each mesh hangs under (hinged parts are found by it), before unparenting
    TOPS = {}
    for o in meshes:
        top = o
        while top.parent is not None and top.parent.parent is not None:
            top = top.parent
        TOPS[o] = top
    top_pivots = {t: world_mat(t).translation.copy() for t in set(TOPS.values())}
    for o in meshes:
        o.matrix_basis = world_mat(o)
        o.parent = None
    anchors = {}
    for o in meshes:
        for slot, m in enumerate(o.data.materials):
            if m and m.name in ANCHOR_MATERIALS:
                vs = {v for p in o.data.polygons if p.material_index == slot for v in p.vertices}
                if vs:
                    pts = [o.matrix_basis @ o.data.vertices[v].co for v in vs]
                    anchors[m.name] = sum(pts, Vector()) / len(pts)
    for e in empties:
        anchors[e.name] = world_mat(e).translation.copy()
    out = {"anchors": anchors, "pivots": {}}
    groups = {"Screen": [o for o in meshes if "Screen" in o.name],
              "Blades": [o for o in meshes if "Blades" in o.name]}
    # Hinged parts: everything under a top-level node named <prop>_Leaf*/_Wheel stays separate and
    # keeps that node's position as its pivot (doors swing about it in Unreal).
    tops = TOPS
    for o in meshes:
        top = tops[o]
        key = top.name[len(name) + 1:] if top.name.startswith(name + "_") else ""
        if key.startswith("Leaf") or key.startswith("Wheel"):
            groups.setdefault(key, []).append(o)
            out["pivots"][key] = top_pivots[top]
    hinged = {o for k, v in groups.items() if k not in ("Screen", "Blades") for o in v}
    groups["body"] = [o for o in meshes if o not in groups["Screen"] and o not in groups["Blades"] and o not in hinged]
    scratch = bpy.context.scene.collection
    for key, part in groups.items():
        if not part:
            out[key] = None
            continue
        merged = join(name if key == "body" else "%s_%s" % (name, key), part, scratch)
        if key in out["pivots"]:                  # mesh relative to its hinge
            merged.data.transform(Matrix.Translation(-out["pivots"][key]))
        if key == "Blades":                       # origin on the hub (centroid of the blades), so Unreal spins them about it
            hub = sum((v.co for v in merged.data.vertices), Vector()) / len(merged.data.vertices)
            merged.data.transform(Matrix.Translation(-hub))
            out["blades_pivot"] = hub
        merged.data.use_fake_user = True
        out[key] = merged.data
        bpy.data.objects.remove(merged)
    for o in empties:
        if o.name in bpy.data.objects:
            bpy.data.objects.remove(o)
    PROPS[name] = out
    return out


def place(name, matrix, coll, label=None, screen_mat=None, blades_label=None, hinge_angles=None):
    """Puts a linked copy of a prop at `matrix`; returns the body object."""
    p = prop(name)
    n = PROP_COUNT[name] = PROP_COUNT.get(name, 0) + 1
    label = label or "%s_%03d" % (name, n)
    obj = bpy.data.objects.new(label, p["body"])
    obj.matrix_basis = matrix
    coll.objects.link(obj)
    if p["Screen"]:
        so = bpy.data.objects.new(label + "_Screen", p["Screen"])
        so.matrix_basis = matrix
        coll.objects.link(so)
        PLACED_SCREENS.append(so)
        if screen_mat:
            so.material_slots[0].link = "OBJECT"
            so.material_slots[0].material = screen_mat
    if p["Blades"]:
        bo = bpy.data.objects.new(blades_label or label + "_Blades", p["Blades"])
        bo.matrix_basis = matrix @ Matrix.Translation(p["blades_pivot"])
        coll.objects.link(bo)
    for key, piv in p["pivots"].items():                 # hinged parts, placed at their pivot
        part = bpy.data.objects.new("%s_%s" % (label, key), p[key])
        swing = (hinge_angles or {}).get(key, 0.0)
        part.matrix_basis = matrix @ Matrix.Translation(piv) @ Matrix.Rotation(math.radians(swing), 4, "Z")
        coll.objects.link(part)
    return obj


def have_prop(name):
    return os.path.exists(os.path.join(PROPS_DIR, name + ".glb"))


def anchor(name, matrix, material):
    """World position of a material's centre (e.g. a lamp's bulb) on a placed prop."""
    return matrix @ prop(name)["anchors"][material]


def at(root, x, y, z, rot_deg=0.0, scale=(1, 1, 1)):
    """Matrix for something placed in a station's local frame."""
    return (root.matrix_basis @ Matrix.Translation((x, y, z)) @ Matrix.Rotation(math.radians(rot_deg), 4, "Z")
            @ Matrix.Diagonal((*scale, 1.0)))


def floor_at(loc, yaw_rad):
    return Matrix.Translation(loc) @ Matrix.Rotation(yaw_rad, 4, "Z")


# Chair props, in the same order as the old procedural set (the shuffled layout stays the same).
CHAIRS = ["chair_office", "chair_stool", "chair_aeron", "chair_gaming", "chair_bentwood", "chair_folding"]


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
    tex.image = bpy.data.images.load(tex_path(file))
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


def tower(root, coll, kind, x=0.45):
    """PC case on the floor under the desk (station root sits at desk height)."""
    place("pc_tower_" + kind, at(root, x, 0.05, -root.location.z), coll)
    return (x, 0.28, -root.location.z + 0.3)


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


def kb(name, root):
    """Full keyboards (real keycap legends) near the hero, lite ones elsewhere."""
    i = int(root.name.split("_")[1]) if root.name.startswith("Station_") else HERO_INDEX
    lite = name + "_lite"
    if abs(i - HERO_INDEX) > 3 and os.path.exists(os.path.join(PROPS_DIR, lite + ".glb")):
        return lite
    return name


def st_sega(root, coll):
    """Small TV with a 16-bit game, Mega Drive with a cartridge, pad."""
    place("tv_small", at(root, 0, 0.12, 0), coll, screen_mat=screen_on("16bit", "screen_16bit.jpg"))
    place("console_megadrive", at(root, 0.05, -0.2, 0, 5), coll)
    place("pad_mega", at(root, -0.32, -0.28, 0, 15), coll)
    return [(0, 0.29, 0.1), (0.05, -0.1, 0.03)]


def st_ps2(root, coll):
    place("tv_silver", at(root, 0, 0.12, 0), coll)
    place("console_ps2", at(root, 0, -0.25, 0), coll)
    place("pad_dualshock", at(root, 0.32, -0.3, 0, -20), coll)
    return [(0, 0.36, 0.1), (0.0, -0.16, 0.04)]


def st_gaming(root, coll):
    """Two monitors, RGB keyboard and mouse, RGB tower under the table, headset."""
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    place("monitor_flat", at(root, -0.33, 0.12, 0, 12), coll,
          screen_mat=screen_on("modern_game", "screen_modern_game.jpg", 1.4))
    place("monitor_flat", at(root, 0.36, 0.12, 0, -18), coll)
    place(kb("keyboard_gaming", root), at(root, -0.02, -0.24, 0), coll)
    place("mouse_rgb", at(root, 0.34, -0.24, 0), coll)
    box("Mousepad", (0.9, 0.38, 0.004), (0.05, -0.27, 0.002), mat("Mousepad", (0.03, 0.03, 0.04), 0.9), root, coll)
    cyl("Headset_Band", 0.09, 0.03, (-0.55, -0.1, 0.09), black, root, coll, rot=(math.radians(90), 0, 0), verts=16)
    for dx in (-0.09, 0.09):
        cyl("Headset_Cup", 0.045, 0.04, (-0.55 + dx, -0.1, 0.045), black, root, coll, rot=(0, math.radians(90), 0))
    t = tower(root, coll, "rgb", x=0.5)
    return [(-0.33, 0.2, 0.1), (0.36, 0.2, 0.1), t]


def st_beige(root, coll):
    """Nineties desktop case with a 14\" CRT on top, Model M keyboard, ball mouse."""
    place("pc_desktop_beige", at(root, 0, 0.08, 0), coll)
    place("crt_beige14", at(root, 0, 0.08, 0.14), coll)
    place("keyboard_modelm", at(root, 0, -0.27, 0), coll)
    place("mouse_ball", at(root, 0.33, -0.27, 0), coll)
    return [(0, 0.29, 0.1)]


def st_spreadsheet(root, coll):
    """Late-nineties office PC: 17\" CRT with a spreadsheet, tower under the table."""
    place("crt_grey17", at(root, 0, 0.1, 0), coll, screen_mat=screen_on("spreadsheet", "screen_spreadsheet.jpg", 1.1))
    place(kb("keyboard_modern", root), at(root, 0, -0.26, 0), coll)
    box("Mousepad_Office", (0.22, 0.19, 0.004), (0.32, -0.26, 0.002), mat("Mousepad_Blue", (0.05, 0.12, 0.3), 0.8),
        root, coll)
    if root.name != "Station_%02d" % CLERK_INDEX:     # the clerk brings his own animated mouse
        place("mouse_ball", at(root, 0.32, -0.26, 0.004), coll)
    for i in range(3):
        box("Papers", (0.21, 0.297, 0.002), (-0.5, -0.1, 0.002 + i * 0.002), mat("Paper", (0.85, 0.84, 0.8), 0.9),
            root, coll, rot=(0, 0, math.radians(8 * i - 6)))
    t = tower(root, coll, "beige", x=0.45)
    return [(0, 0.32, 0.1), t]


def st_vr(root, coll):
    """VR corner: headset and controllers on the table, two base stations, gaming tower underneath."""
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    white = mat("Plastic_White", (0.85, 0.86, 0.86), 0.35)
    lens = mat("VR_Lens", (0.01, 0.01, 0.02), 0.05, 0.5)
    box("VR_Visor", (0.19, 0.11, 0.1), (0, -0.05, 0.06), white, root, coll, bevel=0.03)
    box("VR_Face", (0.17, 0.02, 0.08), (0, 0.015, 0.06), black, root, coll, bevel=0.01)
    for dx in (-0.035, 0.035):
        cyl("VR_Lens", 0.022, 0.005, (dx, 0.027, 0.06), lens, root, coll, rot=(math.radians(90), 0, 0))
    cyl("VR_Strap", 0.1, 0.03, (0, 0.11, 0.05), black, root, coll, verts=20, r2=0.1)
    for dx in (-0.3, 0.3):
        cyl("VR_Grip", 0.018, 0.12, (dx, -0.2, 0.06), white, root, coll, rot=(math.radians(70), 0, 0), verts=10)
        cyl("VR_Ring", 0.05, 0.015, (dx, -0.24, 0.12), white, root, coll, rot=(math.radians(20), 0, 0), verts=20, r2=0.05)
    for dx in (-0.65, 0.65):
        cyl("BaseStation_Pole", 0.01, 0.4, (dx, 0.3, 0.2), black, root, coll, verts=8)
        box("BaseStation", (0.08, 0.06, 0.08), (dx, 0.28, 0.43), black, root, coll, bevel=0.01,
            rot=(math.radians(-20), 0, math.radians(15 if dx < 0 else -15)))
    place("laptop_black", at(root, 0.45, -0.05, 0), coll)
    t = tower(root, coll, "black", x=0.5)
    return [(0, 0.12, 0.06), (-0.65, 0.3, 0.0), (0.65, 0.3, 0.0), (0.45, 0.07, 0.02), t]


def st_zx(root, coll):
    """ZX-style home computer (the keyboard is the computer) on a small TV, cassette."""
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    place("tv_small", at(root, 0, 0.12, 0), coll)
    place("keyboard_zx", at(root, 0, -0.24, 0), coll)
    box("Cassette", (0.2, 0.13, 0.05), (-0.3, -0.2, 0.025), black, root, coll)
    return [(0, 0.29, 0.1), (0, -0.17, 0.02)]


def st_laptop(root, coll):
    place("laptop_silver", at(root, 0, -0.1, 0), coll)
    return [(0.15, 0.02, 0.01)]


def st_imac(root, coll):
    teal = mat("iMac_Teal", (0.1, 0.55, 0.6), 0.15, transmission=0.6, alpha=0.85)
    place("imac_g3", at(root, 0, 0.05, 0), coll)
    place(kb("keyboard_imac", root), at(root, 0, -0.28, 0), coll)
    cyl("iMac_Puck", 0.035, 0.02, (0.27, -0.27, 0.01), teal, root, coll)
    return [(0, 0.25, 0.1)]


def st_modern(root, coll):
    place("monitor_flat", at(root, 0, 0.12, 0), coll)
    place("console_ps5", at(root, 0.55, 0.1, 0), coll)
    place("pad_dualsense", at(root, -0.1, -0.25, 0, 10), coll)
    return [(0, 0.2, 0.05), (0.55, 0.23, 0.05)]


def st_mac(root, coll):
    """Classic all-in-one Macintosh with its keyboard and one-button mouse."""
    place("mac_classic", at(root, 0, 0.1, 0), coll)
    place(kb("keyboard_macext", root), at(root, 0, -0.25, 0), coll)
    place("mouse_mac", at(root, 0.3, -0.25, 0), coll)
    return [(0, 0.24, 0.1)]


STATIONS = [st_sega, st_spreadsheet, st_ps2, st_gaming, st_vr, st_beige, st_zx, st_laptop, st_imac, st_modern]
EXTRA_STATIONS = {3: st_mac, 17: st_mac, 29: st_mac}   # a few classic Macs dropped into the row


# ------------------------------------------------------------ desks
# Desk local frame: x along the row, -y toward the chair, the top surface at z = 0
# (the station root sits at desk height), the floor at z = -h.

def desk(root, coll, kind, w, h):
    """Detailed desk prop of this kind and width, stretched to this station's height."""
    place("desk_%s_%d" % (kind, round(w * 100)), at(root, 0, 0, 0, scale=(1, 1, h / 0.75)), coll)


DESKS = ["soviet", "white", "gaming", "office", "school", "glass", "pine"]
WIDE = {"st_gaming", "st_vr", "st_modern"}


# ------------------------------------------------------------ desk objects

def ext_fan(root, coll, x, y, n):
    """Desk fan blowing at the chair; the blades are their own actor so Unreal can spin them."""
    place("fan_desk", at(root, x, y, 0, rng_turn(n, 25)), coll, blades_label="DeskFan_Blades_%02d" % n)
    LAYOUT.setdefault("desk_fans", []).append("DeskFan_Blades_%02d" % n)


def ext_rose(root, coll, x, y, n):
    """A rose in a vase under a purple grow lamp (its head hangs 18 cm off the pole)."""
    place("rose_vase", at(root, x, y, 0), coll)
    m = at(root, x + 0.18, y, 0)
    place("lamp_grow", m, coll)
    LAYOUT.setdefault("grow_lights", []).append(ue(*(anchor("lamp_grow", m, "GrowLamp_Purple") + Vector((0, 0, -0.04)))))


def ext_desk_lamp(root, coll, x, y, n):
    m = at(root, x, y + 0.05, 0, rng_turn(n, 20))
    place("lamp_soviet", m, coll)
    LAYOUT.setdefault("desk_lights", []).append(ue(*(anchor("lamp_soviet", m, "Lamp_Bulb") + Vector((0, 0, -0.03)))))


def ext_plant(root, coll, x, y, n):
    place("plant_cactus" if n % 2 else "plant_leafy", at(root, x, y, 0, rng_turn(n, 180)), coll)


def ext_books(root, coll, x, y, n):
    place("books_%d" % (2 + n % 3), at(root, x, y, 0, rng_turn(n, 12)), coll)


def ext_tea(root, coll, x, y, n):
    place("tea_glass", at(root, x, y - 0.05, 0, rng_turn(n, 60)), coll)


def ext_trinkets(root, coll, x, y, n):
    place("rubik_cube", at(root, x - 0.08, y - 0.05, 0, rng_turn(n, 40)), coll)
    place("alarm_clock", at(root, x + 0.06, y, 0, rng_turn(n + 1, 20)), coll)
    place("cd_stack", at(root, x, y + 0.1, 0, rng_turn(n + 2, 15)), coll)


def rng_turn(n, spread):
    """Stable little rotation per station, without touching the layout's random stream."""
    return ((n * 37) % 100 / 100.0 - 0.5) * 2 * spread


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
        desk_kind = rng.choice(DESKS)
        if i in EXTRA_STATIONS and fn.__name__ not in WIDE and i != CLERK_INDEX:
            fn = EXTRA_STATIONS[i]
            root.name = "Station_%02d" % i
        PLACED_SCREENS.clear()
        sources = fn(root, coll) or []
        if i in VISITORS:
            person, content = VISITORS[i]
            if PLACED_SCREENS:
                sc = PLACED_SCREENS[0]
                sc.material_slots[0].link = "OBJECT"
                sc.material_slots[0].material = screen_on("live_" + content, LIVE_SCREENS[content], 1.2)
            LAYOUT.setdefault("visitors", []).append({"index": i, "person": person, "kind": fn.__name__[3:]})
            if person == "Streamer":
                # chat on a small second screen to her right (station +x), webcam on top of the TV
                PLACED_SCREENS.clear()
                place("monitor_flat", at(root, 0.48, 0.12, 0, -25, scale=(0.6, 0.6, 0.6)), coll)
                if PLACED_SCREENS:
                    sc2 = PLACED_SCREENS[0]
                    sc2.material_slots[0].link = "OBJECT"
                    sc2.material_slots[0].material = screen_on("live_chat", LIVE_SCREENS["chat"], 1.2)
                cam_m = mat("Webcam", (0.05, 0.05, 0.06), 0.3)
                box("Webcam", (0.09, 0.05, 0.05), (0.0, 0.0, 0.505), cam_m, root, coll, bevel=0.01)
                box("Webcam_Lens", (0.025, 0.006, 0.025), (0.0, -0.028, 0.505), led((0.2, 0.3, 1.0)), root, coll)
        # The desk goes in after the devices: some stations nudge all their children back.
        desk(root, coll, desk_kind, w, h)
        if i == CLERK_INDEX:
            LAYOUT["clerk"] = {
                "station": ue_from(root, 0, 0, 0),           # desk-top centre
                "station_kind": fn.__name__[3:],
                "desk_height": round(h * 100, 1),
                "chair": ue(SEAT_X, cy, 0.0),
                "chair_yaw": 0.0,                            # sitter faces +X, toward the desk
                "screen": ue_from(root, 0, -0.126, 0.251),    # CRT screen centre (crt_grey17)
                "screen_size": [33.6, 31.2],
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
        global NO_LITTER
        NO_LITTER = i == CLERK_INDEX         # keep the clerk's mousepad clear (same random draws)
        trash = litter(coll, rng, root, w) if rng.random() < 0.25 else []
        NO_LITTER = False
        collapse("Station_%02d_%s" % (i, fn.__name__[3:]), root, coll, wires + trash)
        chair_kind = rng.choice(CHAIRS)
        loc = (SEAT_X + rng.uniform(-0.08, 0.1), cy + rng.uniform(-0.15, 0.15), 0)
        yaw = -90 + rng.uniform(-20, 20)
        if i == CLERK_INDEX or i in VISITORS:               # squarely at the desk for people who sit here
            loc, yaw = (SEAT_X, cy, 0), -90
        place(chair_kind, floor_at(loc, math.radians(yaw)), coll, label="Chair_%02d" % i)
        # Unreal yaw the sitter faces (0 = +X, toward the desk); Blender rotations flip with Y.
        LAYOUT.setdefault("stations", []).append({
            "index": i, "kind": fn.__name__[3:], "chair": ue(*loc), "chair_kind": chair_kind,
            "desk": ue(TABLE_X, cy, h),
            "sitter_yaw": round(-(yaw + 90), 1), "reachable": "invisible_wall_y" not in LAYOUT})
        y += w
        i += 1
        if i == HERO_INDEX + REAL_AFTER_HERO:
            # A few real places past the hero; beyond this the corridor is only to look at.
            LAYOUT["invisible_wall_y"] = ue(0, y + 0.05, 0)[1]
    return y


# ------------------------------------------------------------ fast-food litter

NO_LITTER = False


def drop(name, coll, loc, rot):
    if not NO_LITTER:
        place(name, floor_at(loc, rot), coll)
    return []


def cola_cup(coll, loc, rot, lying=False):
    return drop("cup_cola_lying" if lying else "cup_cola", coll, loc, rot)


def food_bag(coll, loc, rot):
    return drop("food_bag", coll, loc, rot)


def burger_box(coll, loc, rot, open_=False):
    return drop("burger_box_open" if open_ else "burger_box_closed", coll, loc, rot)


def fries(coll, loc, rot):
    return drop("fries", coll, loc, rot)


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
    place(kb("keyboard_modern", root), at(root, 0, -0.22, 0), coll, label="Hero_Keyboard")
    place("mouse_optical", at(root, 0.32, -0.22, 0), coll, label="Hero_Mouse")
    cyl("Hero_Mug", 0.04, 0.1, (-0.55, -0.1, 0.05), mat("Mug", (0.6, 0.1, 0.08), 0.4), root, coll)
    t = tower(root, coll, "beige", x=0.5)
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
    place("chair_office", floor_at((SEAT_X, hero_y, 0), math.radians(-90)), coll, label="HeroChair")

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
    build_posters(arch)
    build_walkway(walk)
    build_stations(furn)
    build_work_lights(work)
    check_posters()

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
