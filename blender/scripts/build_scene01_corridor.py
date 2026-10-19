"""Scene 01: the pyramid corridor, dressed as a Soviet apartment stairwell (podyezd).

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
W = 7.0             # floor width
WALL_H = 2.4        # vertical part of the wall
COURSES = 7         # corbel courses above it
COURSE_H = 0.8
STEP = 0.25         # each course steps inward this much per side
CEIL_Z = WALL_H + COURSES * COURSE_H

TABLE_X = 1.55      # table centre line
TABLE_W = 0.9
TABLE_H = 0.75
TABLE_Y0, TABLE_Y1 = 3.0, L - 6.0
SEAT_X = 0.55
WALK_X0, WALK_X1 = -3.05, -1.35
WALK_Y0, WALK_Y1 = 4.0, L - 8.0

HERO_DESK_Y = L - 3.0
HERO_CHAIR_Y = L - 2.1
HERO_X = 1.0

PANEL_H = 1.5       # oil-painted lower part of the podyezd wall
WARM = (1.0, 0.68, 0.38)
GARLAND_Z = 2.2
GARLAND_COLORS = [(1.0, 0.12, 0.05), (1.0, 0.75, 0.1), (0.1, 0.9, 0.2), (0.15, 0.35, 1.0), (1.0, 0.4, 0.05)]
DUCT_Z = 7.45       # ventilation duct under the 8 m ceiling
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


def join(name, objs, coll):
    """Merges many small objects into one mesh (fewer actors in Unreal)."""
    if not objs:
        return None
    bm = bmesh.new()
    mats = []
    for o in objs:
        tmp = o.data.copy()
        mw = o.matrix_basis if not o.parent else o.parent.matrix_basis @ o.matrix_basis
        tmp.transform(mw)
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
    text("ЛЮДОС   Файл   Правка   Вид", 0.25, 9.66, 0.26, (0.05, 0.05, 0.05))
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
        "ЛЮДОС v0.1  (c) Людмиловск",
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
        for i in range(COURSES):
            inner = W / 2 - (i + 1) * STEP
            z = WALL_H + i * COURSE_H + COURSE_H / 2
            thick = 1.0 + (i + 1) * STEP
            tbox("Course_%s_%d" % (s_, i), (thick, L, COURSE_H),
                 (side * (inner + thick / 2), L / 2, z), white, 1.0, coll=coll)
    top_w = W - 2 * COURSES * STEP
    tbox("Ceiling", (top_w + 0.2, L, 0.6), (0, L / 2, CEIL_Z + 0.3), ceil, 2.0, coll=coll)
    # end walls: the far one behind the hero, the near one is a dead end too (no exit yet)
    for name, y in (("EndWall_Far", L + 0.5), ("EndWall_Near", -0.5)):
        h = CEIL_Z + 0.6
        tbox(name + "_Paint", (W + 1, 1.0, PANEL_H), (0, y, PANEL_H / 2), paint, 1.0, coll=coll)
        tbox(name, (W + 1, 1.0, h - PANEL_H), (0, y, (h + PANEL_H) / 2), white, 1.0, coll=coll)
        box(name + "_Line", (W + 1, 1.01, 0.035), (0, y, PANEL_H), stripe, coll=coll)


def build_garland(coll):
    """Soviet New Year garland sagging along the right wall above the table."""
    wire_m = mat("Garland_Wire", (0.02, 0.05, 0.02), 0.5)
    bulb_ms = [mat("Garland_Bulb_%d" % i, c, 0.3, emit=c, strength=12.0) for i, c in enumerate(GARLAND_COLORS)]
    x = W / 2 - 0.03
    hook, sag, step = 1.6, 0.18, 0.32
    pts = []
    y = 0.6
    while y <= L - 0.6:
        t = ((y - 0.6) % hook) / hook
        pts.append((x, y, GARLAND_Z - sag * 4 * t * (1 - t)))
        y += 0.08
    wires = []
    for a, b in zip(pts, pts[1:]):
        d = Vector(b) - Vector(a)
        mid = (Vector(a) + Vector(b)) / 2
        rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_euler()
        wires.append(cyl("Garland_Wire", 0.004, d.length, mid, wire_m, coll=coll, rot=rot, verts=6))
    join("Garland_Wire", wires, coll)
    bulbs = [[] for _ in GARLAND_COLORS]
    for i, p in enumerate(pts[::4]):
        k = i % len(GARLAND_COLORS)
        bulbs[k].append(cyl("Garland_Bulb", 0.014, 0.05, (p[0] - 0.015, p[1], p[2] - 0.03),
                            bulb_ms[k], coll=coll, verts=8, r2=0.006))
    for k, objs in enumerate(bulbs):
        join("Garland_Bulbs_%d" % k, objs, coll)


def build_ventilation(coll):
    """Galvanised duct along the high ceiling, grilles and slowly turning exhaust fans."""
    zinc = mat("Vent_Galvanised", (0.55, 0.56, 0.57), 0.45, 0.9)
    dark = mat("Vent_Grille", (0.05, 0.05, 0.05), 0.6, 0.5)
    rod = mat("Vent_Rod", (0.2, 0.2, 0.2), 0.5, 0.8)
    parts = [box("Vent_Duct", (0.8, L - 2.0, 0.5), (0, L / 2, DUCT_Z), zinc, coll=coll)]
    for k in range(int(L / 3)):
        y = 1.5 + k * 3.0
        parts.append(box("Vent_Seam", (0.84, 0.04, 0.54), (0, y, DUCT_Z), zinc, coll=coll))
        for dx in (-0.35, 0.35):
            parts.append(cyl("Vent_Hanger", 0.008, CEIL_Z - DUCT_Z - 0.25, (dx, y, (CEIL_Z + DUCT_Z + 0.25) / 2), rod, coll=coll, verts=6))
    for y in range(6, int(L), 9):
        parts.append(box("Vent_Grille", (0.6, 0.4, 0.03), (0, y, DUCT_Z - 0.26), dark, coll=coll))
        for i in range(7):
            parts.append(box("Vent_Slat", (0.56, 0.015, 0.04), (0, y - 0.18 + i * 0.06, DUCT_Z - 0.29), zinc, coll=coll))
    join("Vent_Duct", parts, coll)
    for i, y in enumerate(FAN_YS):
        housing = [cyl("Fan_Ring", 0.42, 0.25, (0, y, DUCT_Z - 0.38), zinc, coll=coll, verts=32),
                   cyl("Fan_Hub", 0.06, 0.12, (0, y, DUCT_Z - 0.5), dark, coll=coll)]
        join("VentFan_Housing_%d" % i, housing, coll)
        # Blades are their own object with the pivot on the axis, so Unreal can spin them.
        root = empty("VentFan_%d" % i, (0, y, DUCT_Z - 0.46), coll=coll)
        blades = []
        for b in range(4):
            a = b * math.pi / 2
            blades.append(box("Blade", (0.34, 0.1, 0.01), (math.cos(a) * 0.2, math.sin(a) * 0.2, 0), dark,
                              root, coll, rot=(math.radians(20), 0, a)))
        bl = join("VentFan_Blades_%d" % i, blades, coll)
        bl.location = (0, 0, 0)
        bl.parent = root
        bl.data.transform(root.matrix_basis.inverted())


def build_podyezd_props(coll):
    """Radiators, a block of mailboxes and an electrical cabinet."""
    rad = tex_mat("Podyezd_RadiatorPaint", "radiator_paint.jpg", 0.5)
    box_m = mat("Mailbox_Paint", (0.18, 0.25, 0.32), 0.45, 0.3)
    door_m = mat("Mailbox_Door", (0.22, 0.3, 0.38), 0.4, 0.3)
    shield = mat("Shield_Grey", (0.32, 0.33, 0.3), 0.5, 0.4)
    pipe = mat("Pipe_Paint", (0.62, 0.58, 0.48), 0.5, 0.2)
    # Cast-iron radiators in the gap between the left wall and the walkway.
    for n, y in enumerate(range(8, int(L) - 4, 10)):
        parts = []
        for k in range(10):
            parts.append(tbox("Rad_Section", (0.09, 0.075, 0.55), (-W / 2 + 0.1, y + k * 0.085, 0.42), rad, 0.3,
                              coll=coll))
        parts.append(cyl("Rad_Pipe", 0.016, 1.2, (-W / 2 + 0.1, y + 0.4, 0.66), pipe, coll=coll,
                         rot=(math.radians(90), 0, 0), verts=10))
        parts.append(cyl("Rad_Pipe", 0.016, 1.2, (-W / 2 + 0.1, y + 0.4, 0.18), pipe, coll=coll,
                         rot=(math.radians(90), 0, 0), verts=10))
        join("Radiator_%02d" % n, parts, coll)
    # Mailboxes on the near dead-end wall, facing the hero down the corridor.
    parts = []
    for r in range(3):
        for c in range(5):
            x, z = -1.0 + c * 0.42, 1.15 + r * 0.32
            parts.append(box("Mailbox", (0.4, 0.22, 0.3), (x, 0.11, z), box_m, coll=coll))
            parts.append(box("Mailbox_Door", (0.34, 0.01, 0.24), (x, 0.225, z), door_m, coll=coll))
            parts.append(box("Mailbox_Slot", (0.2, 0.012, 0.02), (x, 0.231, z + 0.07), screen_off(), coll=coll))
    join("Mailboxes", parts, coll)
    # Electrical cabinet (shchitok) on the right wall near the hero's end.
    join("ElectricCabinet", [
        box("Cab", (0.12, 0.7, 0.9), (W / 2 - 0.06, L - 8.0, 1.6), shield, coll=coll),
        box("Cab_Lock", (0.02, 0.04, 0.06), (W / 2 - 0.13, L - 7.75, 1.6), pipe, coll=coll)], coll)


def build_walkway(coll):
    housing = mat("Walkway_Housing", (0.12, 0.12, 0.13), 0.35, 0.8)
    belt = mat("Walkway_Belt", (0.025, 0.025, 0.028), 0.75)
    comb = mat("Walkway_Comb", (0.6, 0.58, 0.5), 0.3, 1.0)
    glass = mat("Walkway_Glass", (0.6, 0.75, 0.8), 0.05, transmission=1.0, alpha=0.25)
    rail = mat("Walkway_Rail", (0.02, 0.02, 0.02), 0.4)
    led = mat("Walkway_LED", (0.0, 0.0, 0.0), 1.0, emit=(0.25, 0.55, 1.0), strength=2.5)

    cx = (WALK_X0 + WALK_X1) / 2
    w = WALK_X1 - WALK_X0
    ln = WALK_Y1 - WALK_Y0
    cy = (WALK_Y0 + WALK_Y1) / 2
    parent = empty("Walkway", (0, 0, 0), coll=coll)
    box("Walkway_Base", (w + 0.3, ln + 1.2, 0.12), (cx, cy, 0.06), housing, parent, coll)
    box("Walkway_Belt", (w - 0.2, ln, 0.02), (cx, cy, 0.13), belt, parent, coll)
    # belt treads so motion would read
    tread = mat("Walkway_Tread", (0.06, 0.06, 0.065), 0.6)
    y = WALK_Y0 + 0.2
    while y < WALK_Y1:
        box("Tread", (w - 0.25, 0.04, 0.012), (cx, y, 0.146), tread, parent, coll)
        y += 0.4
    for yy in (WALK_Y0 - 0.4, WALK_Y1 + 0.4):
        box("Walkway_Comb", (w - 0.1, 0.5, 0.03), (cx, yy, 0.135), comb, parent, coll)
    for x in (WALK_X0, WALK_X1):
        box("Balustrade", (0.03, ln, 0.85), (x, cy, 0.12 + 0.45), glass, parent, coll)
        box("Skirt", (0.1, ln, 0.12), (x, cy, 0.18), housing, parent, coll)
        box("Skirt_LED", (0.012, ln, 0.02), (x + (0.06 if x == WALK_X0 else -0.06), cy, 0.2), led, parent, coll)
        box("Handrail", (0.09, ln + 0.6, 0.05), (x, cy, 1.0), rail, parent, coll)
        for yy in (WALK_Y0 - 0.3, WALK_Y1 + 0.3):
            cyl("Handrail_End", 0.45, 0.09, (x, yy, 0.56), rail, parent, coll, rot=(0, math.radians(90), 0), verts=32)


def chair(name, loc, rot_z, coll, parent=None):
    fabric = mat("Chair_Fabric", (0.08, 0.08, 0.09), 0.9)
    metal = mat("Chair_Metal", (0.4, 0.4, 0.42), 0.3, 1.0)
    root = empty(name, loc, rot_z, parent, coll)
    # local front is -Y: the sitter faces +Y
    box(name + "_Seat", (0.48, 0.46, 0.08), (0, 0, 0.47), fabric, root, coll, bevel=0.02)
    box(name + "_Back", (0.46, 0.07, 0.5), (0, -0.26, 0.8), fabric, root, coll, rot=(math.radians(-8), 0, 0), bevel=0.02)
    cyl(name + "_Post", 0.025, 0.36, (0, 0, 0.25), metal, root, coll)
    for i in range(5):
        a = i * 2 * math.pi / 5
        box(name + "_Leg", (0.04, 0.3, 0.03), (math.sin(a) * 0.15, math.cos(a) * 0.15, 0.06), metal, root, coll,
            rot=(0, 0, -a))
        cyl(name + "_Wheel", 0.03, 0.03, (math.sin(a) * 0.29, math.cos(a) * 0.29, 0.03), metal, root, coll,
            rot=(0, math.radians(90), 0), verts=12)
    return root


def build_table(coll):
    top = mat("Table_Top", (0.28, 0.17, 0.10), 0.45)
    leg = mat("Table_Leg", (0.05, 0.05, 0.05), 0.4, 0.8)
    ln = TABLE_Y1 - TABLE_Y0
    box("Table_Top", (TABLE_W, ln, 0.05), (TABLE_X, (TABLE_Y0 + TABLE_Y1) / 2, TABLE_H - 0.025), top, coll=coll)
    y = TABLE_Y0 + 0.2
    while y <= TABLE_Y1 - 0.1:
        for dx in (-TABLE_W / 2 + 0.06, TABLE_W / 2 - 0.06):
            box("Table_Leg", (0.05, 0.05, TABLE_H - 0.05), (TABLE_X + dx, y, (TABLE_H - 0.05) / 2), leg, coll=coll)
        y += 3.0


# ------------------------------------------------------------ stations
# Each station is built in local space: the device sits at the origin on the
# table top and its screen faces local -Y, where the chair is.

def screen_off():
    return mat("Screen_Off", (0.01, 0.012, 0.014), 0.08, 0.0)


def led(color):
    key = "LED_%.2f_%.2f_%.2f" % color
    return mat(key, (0, 0, 0), 1.0, emit=color, strength=6.0)


def crt(root, coll, name, w, h, d, shell):
    box(name + "_Front", (w, d * 0.45, h), (0, 0, h / 2), shell, root, coll, bevel=0.015)
    box(name + "_Back", (w * 0.7, d * 0.55, h * 0.75), (0, d * 0.48, h * 0.45), shell, root, coll, bevel=0.03)
    plane(name + "_Screen", w * 0.78, h * 0.72, (0, -d * 0.226, h * 0.52), screen_off(), root, coll)
    box(name + "_LED", (0.012, 0.005, 0.012), (w * 0.4, -d * 0.226, h * 0.08), led((0.9, 0.15, 0.05)), root, coll)


def st_sega(root, coll):
    shell = mat("Plastic_Grey", (0.42, 0.42, 0.40), 0.5)
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    crt(root, coll, "SegaCRT", 0.40, 0.36, 0.40, shell)
    for c in root.children:
        c.location.y += 0.1
    box("MegaDrive", (0.28, 0.21, 0.06), (0.05, -0.22, 0.03), black, root, coll, bevel=0.01)
    cyl("MegaDrive_Ring", 0.07, 0.008, (0.0, -0.22, 0.064), mat("Plastic_Red_Ring", (0.5, 0.05, 0.03), 0.4), root, coll, verts=32)
    box("MegaDrive_Cart", (0.13, 0.02, 0.07), (0.0, -0.21, 0.1), black, root, coll)
    box("Pad", (0.16, 0.08, 0.025), (-0.15, -0.3, 0.013), black, root, coll, bevel=0.012)


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


def st_gaming(root, coll):
    black = mat("Plastic_Black", (0.02, 0.02, 0.02), 0.4)
    glass = mat("Case_Glass", (0.2, 0.2, 0.25), 0.05, transmission=1.0, alpha=0.3)
    # monitor
    box("Monitor_Panel", (0.62, 0.03, 0.36), (0, 0.08, 0.42), black, root, coll)
    plane("Monitor_Screen", 0.6, 0.34, (0, 0.064, 0.42), screen_off(), root, coll)
    box("Monitor_Neck", (0.05, 0.05, 0.25), (0, 0.13, 0.16), black, root, coll)
    box("Monitor_Foot", (0.25, 0.2, 0.015), (0, 0.1, 0.008), black, root, coll)
    # tower with RGB
    box("Tower", (0.22, 0.46, 0.48), (0.48, 0.05, 0.24), black, root, coll)
    box("Tower_Glass", (0.005, 0.42, 0.42), (0.368, 0.05, 0.25), glass, root, coll)
    for i, col in enumerate([(1.0, 0.1, 0.8), (0.1, 0.8, 1.0), (0.6, 0.1, 1.0)]):
        cyl("Fan_%d" % i, 0.055, 0.02, (0.48, -0.181, 0.1 + i * 0.13), led(col), root, coll,
            rot=(math.radians(90), 0, 0), verts=24)
    box("RGB_Strip", (0.004, 0.40, 0.012), (0.372, 0.05, 0.46), led((0.2, 1.0, 0.6)), root, coll)
    # keyboard + mouse
    box("Keyboard", (0.44, 0.14, 0.025), (-0.02, -0.25, 0.013), black, root, coll)
    box("Keyboard_Glow", (0.44, 0.14, 0.003), (-0.02, -0.25, 0.0015), led((1.0, 0.2, 0.6)), root, coll)
    box("Mouse", (0.06, 0.11, 0.035), (0.3, -0.25, 0.018), black, root, coll, bevel=0.015)


def st_beige(root, coll):
    beige = mat("Plastic_Beige", (0.72, 0.66, 0.53), 0.55)
    box("Case_Desktop", (0.44, 0.40, 0.15), (0, 0.06, 0.075), beige, root, coll, bevel=0.005)
    box("Floppy", (0.12, 0.005, 0.015), (0.1, -0.142, 0.1), mat("Plastic_Dark", (0.08, 0.08, 0.08), 0.5), root, coll)
    box("Power_LED", (0.008, 0.004, 0.008), (-0.15, -0.142, 0.05), led((0.1, 1.0, 0.2)), root, coll)
    sub = empty("CRT_Mount", (0, 0.04, 0.15), 0, root, coll)
    crt(sub, coll, "BeigeCRT", 0.38, 0.34, 0.40, beige)
    box("Keyboard_Beige", (0.46, 0.17, 0.035), (0, -0.28, 0.018), beige, root, coll, rot=(math.radians(4), 0, 0))


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


def st_laptop(root, coll):
    alu = mat("Aluminium", (0.6, 0.6, 0.62), 0.3, 1.0)
    box("Laptop_Base", (0.34, 0.24, 0.018), (0, -0.12, 0.009), alu, root, coll)
    lid = empty("Laptop_Hinge", (0, 0.0, 0.018), 0, root, coll)
    lid.rotation_euler = (math.radians(-15), 0, 0)
    box("Laptop_Lid", (0.34, 0.008, 0.23), (0, 0.004, 0.115), alu, lid, coll)
    plane("Laptop_Screen", 0.31, 0.2, (0, -0.0005, 0.118), screen_off(), lid, coll)
    box("Laptop_LED", (0.006, 0.004, 0.004), (0.15, -0.241, 0.01), led((1.0, 0.6, 0.1)), root, coll)


def st_imac(root, coll):
    teal = mat("iMac_Teal", (0.1, 0.55, 0.6), 0.15, transmission=0.6, alpha=0.85)
    white = mat("Plastic_White", (0.85, 0.86, 0.86), 0.35)
    box("iMac_Front", (0.40, 0.18, 0.38), (0, 0.0, 0.25), white, root, coll, bevel=0.04)
    box("iMac_Back", (0.36, 0.26, 0.34), (0, 0.18, 0.25), teal, root, coll, bevel=0.08)
    box("iMac_Foot", (0.18, 0.2, 0.06), (0, 0.06, 0.03), teal, root, coll, bevel=0.02)
    plane("iMac_Screen", 0.28, 0.21, (0, -0.091, 0.28), screen_off(), root, coll)
    box("iMac_Keyboard", (0.35, 0.13, 0.025), (0, -0.28, 0.013), teal, root, coll, bevel=0.01)
    cyl("iMac_Puck", 0.035, 0.02, (0.26, -0.27, 0.01), teal, root, coll)


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


STATIONS = [st_sega, st_ps2, st_gaming, st_beige, st_zx, st_laptop, st_imac, st_modern]


def build_stations(coll):
    spacing = 3.5
    y = TABLE_Y0 + 1.5
    i = 0
    while y < TABLE_Y1 - 1.0:
        fn = STATIONS[i % len(STATIONS)]
        root = empty("Station_%02d_%s" % (i, fn.__name__[3:]), (TABLE_X + 0.05, y, TABLE_H), math.radians(-90), coll=coll)
        fn(root, coll)
        chair("Chair_%02d" % i, (SEAT_X, y, 0), math.radians(-90), coll)
        y += spacing
        i += 1


def build_hero(coll):
    wood = mat("Hero_Desk", (0.22, 0.13, 0.07), 0.5)
    leg = mat("Table_Leg", (0.05, 0.05, 0.05), 0.4, 0.8)
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

    # The hero sits at the very end, facing back down the corridor (-Y).
    root = empty("HeroStation", (HERO_X, HERO_DESK_Y, 0), math.radians(180), coll=coll)
    box("HeroDesk_Top", (1.6, 0.8, 0.05), (0, 0, 0.75), wood, root, coll)
    for dx in (-0.75, 0.75):
        for dy in (-0.35, 0.35):
            box("HeroDesk_Leg", (0.05, 0.05, 0.73), (dx, dy, 0.365), leg, root, coll)
    dev = empty("HeroPC", (0, 0, 0.775), 0, root, coll)
    # A mid-2000s home PC: matte black 4:3 LCD, beige tower, worn keyboard.
    box("Hero_Monitor", (0.46, 0.05, 0.38), (0, 0.12, 0.38), black, dev, coll, bevel=0.01)
    plane("Hero_Screen", 0.42, 0.2625 * 1.2, (0, 0.094, 0.39), scr, dev, coll)
    box("Hero_Neck", (0.06, 0.06, 0.2), (0, 0.16, 0.1), black, dev, coll)
    box("Hero_Foot", (0.24, 0.18, 0.015), (0, 0.13, 0.008), black, dev, coll)
    box("Hero_PowerLED", (0.008, 0.004, 0.008), (0.2, 0.094, 0.2), led((0.1, 1.0, 0.2)), dev, coll)
    box("Hero_Tower", (0.2, 0.44, 0.42), (0.55, 0.05, -0.565), beige, dev, coll, bevel=0.005)
    box("Hero_TowerLED", (0.008, 0.004, 0.008), (0.55, -0.172, -0.415), led((0.1, 1.0, 0.2)), dev, coll)
    box("Hero_Keyboard", (0.44, 0.15, 0.03), (0, -0.22, 0.015), beige, dev, coll, rot=(math.radians(4), 0, 0))
    box("Hero_Mouse", (0.06, 0.1, 0.03), (0.32, -0.22, 0.015), beige, dev, coll, bevel=0.012)
    cyl("Hero_Mug", 0.04, 0.1, (-0.55, -0.1, 0.05), mat("Mug", (0.6, 0.1, 0.08), 0.4), dev, coll)
    chair("HeroChair", (HERO_X, HERO_CHAIR_Y, 0), math.radians(180), coll)

    # Light the screen casts on the desk and the sleeper.
    ld = bpy.data.lights.new("Hero_ScreenGlow", "AREA")
    ld.shape = "RECTANGLE"
    ld.size, ld.size_y = 0.42, 0.32
    ld.energy = 6
    ld.color = (0.55, 0.9, 0.8)
    lo = bpy.data.objects.new("Hero_ScreenGlow", ld)
    coll.objects.link(lo)
    lo.location = (HERO_X, HERO_DESK_Y + 0.05, 0.775 + 0.39)
    lo.rotation_euler = (math.radians(-90), 0, 0)  # pointing +Y, toward the chair
    return root


def build_lights(coll):
    """Warm incandescent globes on long cords from the high ceiling, plus their point lights."""
    glass = mat("Lamp_Globe", (1.0, 0.85, 0.6), 0.2, emit=WARM, strength=8.0)
    cord = mat("Lamp_Cord", (0.02, 0.02, 0.02), 0.6)
    for i, y in enumerate(range(8, int(L), 12)):
        z = 3.9
        join("Lamp_%d" % i, [
            cyl("Lamp_Cord", 0.005, CEIL_Z - z - 0.12, (0, y, (CEIL_Z + z + 0.12) / 2), cord, coll=coll, verts=6),
            uv_sphere("Lamp_Globe", 0.13, (0, y, z), glass, coll)], coll)
        ld = bpy.data.lights.new("Corridor_Fill_%d" % i, "POINT")
        ld.energy = 120
        ld.color = WARM
        ld.shadow_soft_size = 0.13
        o = bpy.data.objects.new(ld.name, ld)
        o.location = (0, y, z - 0.2)
        coll.objects.link(o)


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
        ld.energy = 900
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
    lights = collection("Lights")
    work = collection("WorkLights_LayoutOnly")

    build_architecture(arch)
    build_garland(arch)
    build_ventilation(arch)
    build_podyezd_props(arch)
    build_walkway(walk)
    build_table(furn)
    build_stations(furn)
    build_hero(hero)
    build_lights(lights)
    build_work_lights(work)

    screen = bpy.data.objects["Hero_Screen"]
    # First person, slumped in the hero's chair, just woken up, looking at the screen.
    cam_wake = add_camera(scn, "Cam_WakeUp_FPV", (HERO_X + 0.38, HERO_CHAIR_Y + 0.15, 1.02), (91, 4, 183),
                          lens=22, dof_target=screen, fstop=1.8)
    cam_corr = add_camera(scn, "Cam_Layout_Corridor", (-0.6, 1.0, 1.7), (88, 0, -4), lens=20)
    cam_tbl = add_camera(scn, "Cam_Layout_Stations", (0.1, 2.0, 1.9), (76, 0, -22), lens=28)
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

    # glTF for Unreal (Interchange imports .glb natively). Work lights stay out.
    bpy.ops.export_scene.gltf(filepath=os.path.join(EXPORT_DIR, "scene01_corridor.glb"),
                              export_format="GLB", use_visible=True, export_cameras=True,
                              export_lights=True, export_apply=True)
    print("done:", BLEND_PATH)


main()
