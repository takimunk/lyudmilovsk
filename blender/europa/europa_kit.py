"""The hatch to Europa (docs/europa.md): clean game meshes made here from primitives, no generated models (the project's
rule for things you walk into: real wall thickness, boolean-cut openings, own UVs, own collision).

  /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/europa/europa_kit.py [-- --preview out_dir] [--strict]

  -> design/europa/game/europa_kit.glb           every piece, one mesh object each (SM_EU_*)
  -> unreal/Lyudmilovsk/Content/Europa/kit.json  what the game reads: ladders, lids' hinges, lamps, the console, levels

The pieces, all in one frame (Blender metres; origin = the sea hatch's lid seat on top of its coaming, Z up; UE gets
(x, -y, z) * 100 and ALyudAirlock places them the same way):
  SM_EU_Collar     the hatch's concrete collar on the sea bed with its steel coaming and hinge brackets (also used on
                   Europa's ice); its skirt hides the landscape's hole round the shaft
  SM_EU_Lid        a round hatch lid (clear opening 1.20 m, over the character standard's 1.08), handwheels both sides;
                   its own frame: origin on the hinge axis (along Y), the lid toward +X, top up
  SM_EU_Airlock    under the sea bed: the sea shaft down from the hatch, the round room (4 m across, 2.6 m clear), the
                   shaft toward Europa going up out of its ceiling (capped: the climb leaves the room there), ladders,
                   pipes, ribs, lamps, a drain
  SM_EU_Console    the airlock's computer: a steel desk with a screen (EU_screen: the game draws on it) and a big button;
                   own frame: origin on the floor at its back, facing +X
  SM_EU_Water      the water's surface in the room (a disc the game raises and lowers)
  SM_EU_ShaftB     the shaft under Europa's hatch: the same tube and ladder as the room's Europa shaft, capped below;
                   own frame: origin at its lid seat
  SM_EU_Ground     Europa's ice round the hatch: a 4 km disc on a 1561 km sphere (the horizon falls away as it should),
                   rolling plains, two double ridges, a hole under the collar; UVs: metres (the game tiles the ice)
  SM_EU_IceBlocks  blocks of ice strewn about
  SM_EU_Planet     a unit sphere with equirectangular UVs (the planet over Europa; the game scales it)
Materials: tx_* tile by the metre (tools/europa/europa_textures.py T_EU_<name>_C); EU_* are flat or replaced in the game.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT_GLB = os.path.join(REPO, "design", "europa", "game", "europa_kit.glb")
OUT_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Europa", "kit.json")
TEX = os.path.join(REPO, "textures", "europa")

# --- measures (metres) --------------------------------------------------------------------------------------------
OPEN_R = 0.60          # clear radius of every hatch and shaft (1.20 m across)
TUBE_T = 0.10          # shaft wall
LID_R = 0.80           # lid radius (over the coaming)
LID_T = 0.07
COAMING_T = 0.10       # coaming ring's width over the opening
COAMING_H = 0.15       # coaming over the collar's top
COLLAR_R = 1.60        # collar's outer radius (hides a 2 x 2 m landscape hole: its corners are 1.42 m out)
COLLAR_BOTTOM = -0.95
ROOM_C = (-0.90, 0.0)  # room's axis
ROOM_R = 2.0           # inner radius
FLOOR_Z = -5.80
CEIL_Z = -3.20         # clear height 2.6 m
EUROPA_SHAFT = (-1.80, 0.0)
EUROPA_TOP = -0.90     # the room's Europa shaft ends (capped) here
SHAFT_B_BOTTOM = -3.20
LADDER_OFF = 0.42      # ladder's rung line from a shaft's axis, toward +X
RUNG = 0.30
LADDER_HALF = 0.225
HINGE_X = -(OPEN_R + COAMING_T)     # every lid's hinge on the -X side of its opening
GROUND_R = 4000.0
EUROPA_R = 1_561_000.0
SITE_Z = -0.35         # the ground round a hatch, under its lid seat (as on the sea bed)

PALETTE = {
    "tx_paint": ((0.20, 0.36, 0.33), 0.6, 0.0, 0.0),
    "tx_steel": ((0.32, 0.33, 0.34), 0.45, 0.7, 0.0),
    "tx_concrete": ((0.48, 0.47, 0.45), 0.9, 0.0, 0.0),
    "tx_algae": ((0.18, 0.21, 0.12), 0.75, 0.0, 0.0),
    "tx_ice": ((0.80, 0.85, 0.90), 0.45, 0.0, 0.0),
    "EU_rubber": ((0.04, 0.04, 0.04), 0.9, 0.0, 0.0),
    "EU_yellow": ((0.85, 0.65, 0.08), 0.55, 0.0, 0.0),
    "EU_red": ((0.70, 0.05, 0.04), 0.35, 0.0, 0.0),
    "EU_lamp": ((1.00, 0.86, 0.62), 0.4, 0.0, 6.0),
    "EU_glass": ((0.60, 0.66, 0.66), 0.08, 0.0, 0.0),
    "EU_screen": ((0.02, 0.05, 0.05), 0.15, 0.0, 1.0),
    "EU_water": ((0.10, 0.30, 0.30), 0.05, 0.0, 0.0),
    "EU_planet": ((0.05, 0.12, 0.25), 0.5, 0.0, 0.0),
    "EU_dark": ((0.05, 0.05, 0.05), 0.8, 0.0, 0.0),
}


def srgb_to_lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def material(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    col, rough, metal, emit = PALETTE[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    lin = [srgb_to_lin(c) for c in col]
    b.inputs["Base Color"].default_value = (*lin, 1.0)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit > 0:
        b.inputs["Emission Color"].default_value = (*lin, 1.0)
        b.inputs["Emission Strength"].default_value = emit
    img = os.path.join(TEX, "T_EU_%s_C.png" % name[3:]) if name.startswith("tx_") else None
    if img and os.path.exists(img):
        nt = m.node_tree
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = bpy.data.images.load(img, check_existing=True)
        mp = nt.nodes.new("ShaderNodeMapping")
        tile = TILE.get(name, 1.0)
        mp.inputs["Scale"].default_value = (1 / tile, 1 / tile, 1)
        co = nt.nodes.new("ShaderNodeTexCoord")
        nt.links.new(co.outputs["UV"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], t.inputs["Vector"])
        nt.links.new(t.outputs["Color"], b.inputs["Base Color"])
    return m


# metres per texture tile (the game's material uses the same, kit.json "tiles")
TILE = {"tx_paint": 2.0, "tx_steel": 1.0, "tx_concrete": 2.0, "tx_algae": 1.0, "tx_ice": 16.0}


# --- a small mesh builder: faces per material into one bmesh (metre UVs unless given) ------------------------------
def metre_uv(pts):
    n = Vector((0, 0, 0))
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        n += Vector(((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)))
    ax = max(range(3), key=lambda k: abs(n[k]))
    if ax == 2:
        return [(p.x, p.y) for p in pts]
    if ax == 1:
        return [(p.x, p.z) for p in pts]
    return [(p.y, p.z) for p in pts]


class Builder:
    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.mats = []

    def mi(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def face(self, pts, mat, uv=None):
        vs = [self.bm.verts.new(Vector(p)) for p in pts]
        f = self.bm.faces.new(vs)
        f.material_index = self.mi(mat)
        for lp, t in zip(f.loops, uv or metre_uv([v.co for v in vs])):
            lp[self.uv].uv = t
        return f

    def box(self, mat, c, s, rot=None):
        cx, cy, cz = c
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        R = rot or Matrix.Identity(3)
        def P(x, y, z):
            v = R @ Vector((x, y, z))
            return (cx + v.x, cy + v.y, cz + v.z)
        for pts in ([P(-hx, -hy, -hz), P(-hx, hy, -hz), P(hx, hy, -hz), P(hx, -hy, -hz)],
                    [P(-hx, -hy, hz), P(hx, -hy, hz), P(hx, hy, hz), P(-hx, hy, hz)],
                    [P(-hx, -hy, -hz), P(hx, -hy, -hz), P(hx, -hy, hz), P(-hx, -hy, hz)],
                    [P(hx, hy, -hz), P(-hx, hy, -hz), P(-hx, hy, hz), P(hx, hy, hz)],
                    [P(-hx, hy, -hz), P(-hx, -hy, -hz), P(-hx, -hy, hz), P(-hx, hy, hz)],
                    [P(hx, -hy, -hz), P(hx, hy, -hz), P(hx, hy, hz), P(hx, -hy, hz)]):
            self.face(pts, mat)

    def tube(self, mat, c, r_in, r_out, z0, z1, n=32, inner=True, outer=True, top=None, bottom=None):
        """A pipe of Z from z0 to z1 round c: inner face (looking in), outer face, end rings (mat or None to skip).
        UVs: around in metres (at that radius) x height."""
        cx, cy = c
        def ring(r, z):
            return [Vector((cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n), z)) for i in range(n + 1)]
        for r, face_in, on in ((r_in, True, inner), (r_out, False, outer)):
            if not on:
                continue
            a, b = ring(r, z0), ring(r, z1)
            arc = 2 * math.pi * r / n
            for i in range(n):
                q = [a[i], a[i + 1], b[i + 1], b[i]]
                uv = [(i * arc, z0), ((i + 1) * arc, z0), ((i + 1) * arc, z1), (i * arc, z1)]
                if face_in:
                    q, uv = q[::-1], uv[::-1]
                self.face(q, mat, uv)
        for z, m, up in ((z1, top, True), (z0, bottom, False)):
            if not m:
                continue
            a, b = ring(r_in, z), ring(r_out, z)
            for i in range(n):
                q = [a[i], b[i], b[i + 1], a[i + 1]]
                self.face(q if up else q[::-1], m)

    def disc(self, mat, c, z, r, n=32, up=True, uv_scale=None):
        pts = [Vector((c[0] + r * math.cos(2 * math.pi * i / n), c[1] + r * math.sin(2 * math.pi * i / n), z)) for i in range(n)]
        if not up:
            pts = pts[::-1]
        uv = None
        if uv_scale:
            uv = [((p.x - c[0]) / (2 * r) + 0.5, (p.y - c[1]) / (2 * r) + 0.5) for p in pts]
        return self.face(pts, mat, uv)

    def cyl(self, mat, a, b, r, n=12, caps=True):
        a, b = Vector(a), Vector(b)
        ax = (b - a)
        ln = ax.length
        z = ax / ln
        x = z.orthogonal().normalized()
        y = z.cross(x)
        ra, rb = [], []
        for i in range(n + 1):
            t = 2 * math.pi * i / n
            d = x * math.cos(t) + y * math.sin(t)
            ra.append(a + d * r)
            rb.append(b + d * r)
        arc = 2 * math.pi * r / n
        for i in range(n):
            self.face([ra[i], ra[i + 1], rb[i + 1], rb[i]], mat, [(i * arc, 0), ((i + 1) * arc, 0), ((i + 1) * arc, ln), (i * arc, ln)])
        if caps:
            self.face(list(reversed(ra[:-1])), mat)
            self.face(rb[:-1], mat)

    def torus(self, mat, c, normal, r, t, n=24, m=8):
        c, nz = Vector(c), Vector(normal).normalized()
        x = nz.orthogonal().normalized()
        y = nz.cross(x)
        rings = []
        for i in range(n + 1):
            a = 2 * math.pi * i / n
            d = x * math.cos(a) + y * math.sin(a)
            rings.append([c + d * (r + t * math.cos(2 * math.pi * k / m)) + nz * t * math.sin(2 * math.pi * k / m) for k in range(m + 1)])
        for i in range(n):
            for k in range(m):
                q = [rings[i][k], rings[i + 1][k], rings[i + 1][k + 1], rings[i][k + 1]]
                uv = [(i * 2 * math.pi * r / n, k * 2 * math.pi * t / m), ((i + 1) * 2 * math.pi * r / n, k * 2 * math.pi * t / m),
                      ((i + 1) * 2 * math.pi * r / n, (k + 1) * 2 * math.pi * t / m), (i * 2 * math.pi * r / n, (k + 1) * 2 * math.pi * t / m)]
                self.face(q, mat, uv)

    def to_object(self, name, smooth=False, weld=True):
        me = bpy.data.meshes.new(name)
        if weld:
            bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-5)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(material(m))
        o = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(o)
        for p in me.polygons:
            p.use_smooth = smooth
        return o


def apply_mods(o):
    bpy.context.view_layer.objects.active = o
    for m in list(o.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def metre_uvs(o):
    me = o.data
    uv = me.uv_layers[0] if me.uv_layers else me.uv_layers.new(name="UVMap")
    for p in me.polygons:
        pts = [me.vertices[me.loops[i].vertex_index].co.copy() for i in p.loop_indices]
        for i, t in zip(p.loop_indices, metre_uv(pts)):
            uv.data[i].uv = t


# --- the pieces ---------------------------------------------------------------------------------------------------
def ladder(b, axis, z0, z1, mat="tx_steel", rung_mat="EU_yellow"):
    """Rails and rungs on a shaft's +X side from z0 to z1, with brackets back to the wall every 1.2 m."""
    x = axis[0] + LADDER_OFF
    y0 = axis[1]
    for s in (-1, 1):
        b.box(mat, (x + 0.02, y0 + s * LADDER_HALF, (z0 + z1) / 2), (0.06, 0.03, z1 - z0))
    z = z0 + 0.25
    while z < z1 - 0.05:
        b.cyl(rung_mat, (x, y0 - LADDER_HALF + 0.015, z), (x, y0 + LADDER_HALF - 0.015, z), 0.016, n=8, caps=False)
        z += RUNG
    z = z0 + 0.6
    wall = OPEN_R - 0.005
    while z < z1 - 0.2:
        for s in (-1, 1):
            # a bracket from the rail back to the wall (the wall's at the shaft's radius, a little past the rail line)
            yb = y0 + s * LADDER_HALF
            xw = axis[0] + math.sqrt(max(wall * wall - (yb - y0) ** 2, 0.0))
            b.box(mat, ((x + 0.05 + xw) / 2, yb, z), (xw - x - 0.05, 0.025, 0.04))
        z += 1.2


def collar():
    """Concrete collar with a sloped top, the steel coaming, two hinge brackets on -X; algae on what the sea touches."""
    b = Builder()
    n = 40
    top = -COAMING_H
    # the concrete: a sloped top from the coaming out to the rim, the rim's side down into the bed, a skirt below
    r0, r1 = OPEN_R + TUBE_T + 0.005, COLLAR_R
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        def P(r, a, z):
            return Vector((r * math.cos(a), r * math.sin(a), z))
        b.face([P(r0, a0, top), P(r1, a0, top - 0.2), P(r1, a1, top - 0.2), P(r0, a1, top)], "tx_algae")
        b.face([P(r1, a0, top - 0.2), P(r1, a0, COLLAR_BOTTOM), P(r1, a1, COLLAR_BOTTOM), P(r1, a1, top - 0.2)], "tx_concrete",
               [(r1 * a0, top - 0.2), (r1 * a0, COLLAR_BOTTOM), (r1 * a1, COLLAR_BOTTOM), (r1 * a1, top - 0.2)])
    # the coaming: a steel ring round the opening standing COAMING_H over the concrete (its inside is the shaft's)
    b.tube("tx_steel", (0, 0), OPEN_R, OPEN_R + COAMING_T, top, 0.0, n=n, inner=False, top="EU_rubber")
    b.torus("tx_steel", (0, 0, -0.04), (0, 0, 1), OPEN_R + COAMING_T + 0.012, 0.012, n=n, m=6)
    # hinge brackets
    for s in (-1, 1):
        b.box("tx_steel", (HINGE_X - 0.06, s * 0.28, -0.02), (0.14, 0.05, 0.22))
    # dogs (latches) round the rim
    for k in range(6):
        a = 2 * math.pi * (k + 0.5) / 6
        if abs(math.cos(a) + 1) < 0.3:
            continue
        r = OPEN_R + COAMING_T + 0.03
        b.box("tx_steel", (r * math.cos(a), r * math.sin(a), -0.07), (0.06, 0.06, 0.12), Matrix.Rotation(a, 3, "Z"))
    return b.to_object("SM_EU_Collar")


def lid():
    """The lid in its own frame: hinge axis along Y through the origin, the lid toward +X, top up; closed it rests on
    the coaming (its underside at z = 0 of the hatch frame, the hinge 0.05 over it)."""
    b = Builder()
    cx = -HINGE_X        # the lid's centre from the hinge
    zc = -0.05           # the lid's underside under the hinge axis
    n = 40
    # the plate: underside, rim, a slightly domed top
    b.disc("tx_paint", (cx, 0), zc, LID_R, n=n, up=False)
    b.tube("tx_paint", (cx, 0), 0.0, LID_R, zc, zc + LID_T, n=n, inner=False)
    rings = [(LID_R, zc + LID_T), (LID_R * 0.85, zc + LID_T + 0.025), (LID_R * 0.5, zc + LID_T + 0.04)]
    for (ra, za), (rb, zb) in zip(rings, rings[1:]):
        for i in range(n):
            a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
            b.face([(cx + ra * math.cos(a0), ra * math.sin(a0), za), (cx + ra * math.cos(a1), ra * math.sin(a1), za),
                    (cx + rb * math.cos(a1), rb * math.sin(a1), zb), (cx + rb * math.cos(a0), rb * math.sin(a0), zb)], "tx_paint")
    b.disc("tx_paint", (cx, 0), rings[-1][1], rings[-1][0], n=n)
    # the seal ring under it
    b.tube("EU_rubber", (cx, 0), OPEN_R + 0.01, OPEN_R + 0.08, zc - 0.012, zc, n=n, top=None, bottom="EU_rubber")
    # handwheels: top and underside, on a short spindle
    for sgn, z in ((1, rings[-1][1]), (-1, zc - 0.012)):
        zw = z + sgn * 0.09
        b.cyl("tx_steel", (cx, 0, z), (cx, 0, zw + sgn * 0.02), 0.03, n=12)
        b.torus("EU_red" if sgn > 0 else "tx_steel", (cx, 0, zw), (0, 0, 1), 0.24, 0.018, n=28, m=8)
        for k in range(3):
            a = 2 * math.pi * k / 3 + 0.3
            b.cyl("tx_steel", (cx, 0, zw), (cx + 0.23 * math.cos(a), 0.23 * math.sin(a), zw), 0.012, n=6, caps=False)
    # the hinge knuckle and its arms to the plate
    b.cyl("tx_steel", (0, -0.36, 0), (0, 0.36, 0), 0.045, n=14)
    for s in (-1, 1):
        b.box("tx_steel", (0.13, s * 0.2, zc + 0.05), (0.26, 0.06, 0.06))
    return b.to_object("SM_EU_Lid")


def airlock():
    """Under the bed: the sea shaft, the room, the Europa shaft and their ladders, in the hatch frame."""
    b = Builder()
    n = 48
    rc = ROOM_C
    # the sea shaft (its top end is the coaming's: the inside continues up to the lid seat)
    b.tube("tx_paint", (0, 0), OPEN_R, OPEN_R + TUBE_T, CEIL_Z, -0.0, n=36, outer=False)
    # the Europa shaft, capped at its top
    b.tube("tx_paint", EUROPA_SHAFT, OPEN_R, OPEN_R + TUBE_T, CEIL_Z, EUROPA_TOP, n=36, outer=False)
    b.disc("tx_steel", EUROPA_SHAFT, EUROPA_TOP, OPEN_R + 0.002, n=36, up=False)
    # its seat ring in the ceiling (the inner lid shuts against it)
    b.tube("EU_rubber", EUROPA_SHAFT, OPEN_R, OPEN_R + 0.08, CEIL_Z - 0.03, CEIL_Z, n=36, inner=True, outer=True,
           bottom="EU_rubber")
    b.tube("EU_rubber", (0, 0), OPEN_R, OPEN_R + 0.08, CEIL_Z - 0.03, CEIL_Z, n=36, inner=True, outer=True, bottom="EU_rubber")
    # the room: wall, floor, a ceiling with the two shafts' holes (an exact boolean: clean rims)
    b.tube("tx_paint", rc, ROOM_R, ROOM_R + 0.25, FLOOR_Z, CEIL_Z, n=n, outer=False)
    b.disc("tx_concrete", rc, FLOOR_Z, ROOM_R + 0.002, n=n)
    room = b.to_object("SM_EU_Airlock")
    ceil = Builder()
    ceil.tube("tx_paint", rc, 0.0, ROOM_R + 0.002, CEIL_Z, CEIL_Z + 0.3, n=n, inner=False, outer=False,
              bottom="tx_paint", top=None)
    ceil_o = ceil.to_object("ceil")
    cutters = []
    for c in ((0, 0), EUROPA_SHAFT):
        bc = Builder()
        bc.tube("tx_paint", c, 0.0, OPEN_R + 0.001, CEIL_Z - 0.5, CEIL_Z + 0.5, n=36, inner=False, top="tx_paint", bottom="tx_paint")
        cutters.append(bc.to_object("cut"))
    # the ceiling disc is a single face: give it thickness so the boolean has a solid to cut
    sol = ceil_o.modifiers.new("solid", "SOLIDIFY")
    sol.thickness = 0.02
    sol.offset = -1.0
    apply_mods(ceil_o)
    for c in cutters:
        m = ceil_o.modifiers.new("cut", "BOOLEAN")
        m.operation = "DIFFERENCE"
        m.solver = "EXACT"
        m.object = c
        apply_mods(ceil_o)
        bpy.data.objects.remove(c, do_unlink=True)
    # keep only the downward faces (the ceiling seen from the room): the rest is buried
    bm = bmesh.new()
    bm.from_mesh(ceil_o.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z > -0.5 or abs(f.calc_center_median().z - CEIL_Z) > 0.005], context="FACES")
    bm.to_mesh(ceil_o.data)
    bm.free()
    metre_uvs(ceil_o)

    d = Builder()
    # ribs up the wall and a belt round it
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        if abs(math.sin(a)) < 0.4 and math.cos(a) < 0:
            pass
        x, y = rc[0] + (ROOM_R - 0.04) * math.cos(a), rc[1] + (ROOM_R - 0.04) * math.sin(a)
        d.box("tx_paint", (x, y, (FLOOR_Z + CEIL_Z) / 2), (0.08, 0.14, CEIL_Z - FLOOR_Z), Matrix.Rotation(a, 3, "Z"))
    d.tube("tx_steel", rc, ROOM_R - 0.06, ROOM_R - 0.002, FLOOR_Z, FLOOR_Z + 0.15, n=n, inner=True, outer=False, top="tx_steel")
    # two pipes from floor to ceiling with a valve wheel, along the wall at +Y
    for k, a in enumerate((math.radians(70), math.radians(110))):
        r = ROOM_R - 0.16
        x, y = rc[0] + r * math.cos(a), rc[1] + r * math.sin(a)
        d.cyl("tx_steel", (x, y, FLOOR_Z), (x, y, CEIL_Z), 0.06, n=12, caps=False)
        for z in (FLOOR_Z + 0.4, CEIL_Z - 0.3):
            d.cyl("tx_steel", (x, y, z), (x, y, z + 0.05), 0.08, n=12)
        if k == 0:
            d.cyl("tx_steel", (x, y, FLOOR_Z + 1.2), (x - 0.18 * math.cos(a), y - 0.18 * math.sin(a), FLOOR_Z + 1.2), 0.02, n=8)
            d.torus("EU_red", (x - 0.2 * math.cos(a), y - 0.2 * math.sin(a), FLOOR_Z + 1.2), (math.cos(a), math.sin(a), 0), 0.13, 0.014)
    # the drain in the floor: a dark grate
    dc = (rc[0] + 0.2, rc[1] + 0.9)
    d.box("EU_dark", (dc[0], dc[1], FLOOR_Z + 0.006), (0.5, 0.5, 0.012))
    for k in range(6):
        d.box("tx_steel", (dc[0] - 0.225 + k * 0.09, dc[1], FLOOR_Z + 0.021), (0.03, 0.48, 0.018))
    # lamps: caged lights on the wall at -Y of the sea shaft and over the console
    lamps = []
    for a in (math.radians(-35), math.radians(150)):
        r = ROOM_R - 0.03
        x, y, z = rc[0] + r * math.cos(a), rc[1] + r * math.sin(a), CEIL_Z - 0.35
        nx, ny = -math.cos(a), -math.sin(a)
        d.box("tx_steel", (x + nx * 0.02, y + ny * 0.02, z), (0.04, 0.22, 0.22), Matrix.Rotation(a, 3, "Z"))
        d.cyl("EU_lamp", (x + nx * 0.04, y + ny * 0.04, z), (x + nx * 0.16, y + ny * 0.16, z), 0.08, n=12)
        for s in (-1, 1):
            px, py = -ny * s * 0.09, nx * s * 0.09
            d.cyl("tx_steel", (x + nx * 0.04 + px, y + ny * 0.04 + py, z), (x + nx * 0.19 + px, y + ny * 0.19 + py, z), 0.008, n=6)
        lamps.append((x + nx * 0.3, y + ny * 0.3, z))
    # ladders: the sea shaft's from the floor to the coaming, the Europa shaft's from the floor to near its cap
    ladder(d, (0, 0), FLOOR_Z, -0.12)
    ladder(d, EUROPA_SHAFT, FLOOR_Z, EUROPA_TOP - 0.1)
    det = d.to_object("details")
    # join
    for o in (ceil_o, det):
        o.select_set(True)
    room.select_set(True)
    bpy.context.view_layer.objects.active = room
    bpy.ops.object.join()
    bpy.ops.object.select_all(action="DESELECT")
    return room, lamps


def console():
    """The airlock's computer: a steel desk against the wall, a slanted panel with the screen and a big button."""
    b = Builder()
    W, D, H = 0.72, 0.46, 0.80
    b.box("tx_paint", (D / 2, 0, (H + 0.03) / 2), (D, W, H - 0.03))
    # the slanted panel: from the front edge at H up to the back at H + 0.32
    s0, s1 = (D, H), (0.04, H + 0.32)
    for y in (-W / 2, W / 2):
        sgn = 1 if y > 0 else -1
        pts = [(s0[0], y, H), (0.0, y, H), (0.0, y, s1[1] + 0.02), (s1[0], y, s1[1] + 0.02)]
        b.face(pts if sgn > 0 else pts[::-1], "tx_paint")
    b.face([(0.0, -W / 2, H), (0.0, W / 2, H), (0.0, W / 2, s1[1] + 0.02), (0.0, -W / 2, s1[1] + 0.02)], "tx_paint")
    b.face([(0.0, -W / 2, s1[1] + 0.02), (0.0, W / 2, s1[1] + 0.02), (s1[0], W / 2, s1[1] + 0.02), (s1[0], -W / 2, s1[1] + 0.02)], "tx_paint")
    face = [(s0[0], -W / 2, H), (s1[0], -W / 2, s1[1] + 0.02), (s1[0], W / 2, s1[1] + 0.02), (s0[0], W / 2, H)]
    b.face(face, "tx_steel")
    # the screen: a recessed quad on the panel (4 mm proud of nothing: it sits in a bezel 6 mm over the panel)
    tdir = (Vector(s1) - Vector(s0)).normalized()
    nrm = Vector((-tdir[1], tdir[0]))          # the panel's normal in XZ (up and toward +X)
    if nrm[1] < 0:
        nrm = -nrm
    def on_panel(t, y, h):
        p = Vector(s0) + tdir * t + nrm * h
        return (p[0], y, p[1])
    L = (Vector(s1) - Vector(s0)).length
    bz = 0.006
    t0, t1, yw = 0.10, L - 0.05, 0.25
    for (a, b2) in (((t0 - 0.02, -yw - 0.02), (t1 + 0.02, yw + 0.02)),):
        pass
    # bezel box
    b.face([on_panel(t0 - 0.02, -yw - 0.02, bz), on_panel(t0 - 0.02, yw + 0.02, bz), on_panel(t1 + 0.02, yw + 0.02, bz), on_panel(t1 + 0.02, -yw - 0.02, bz)][::-1], "EU_dark")
    for (ta, ya), (tb, yb) in (((t0 - 0.02, -yw - 0.02), (t0 - 0.02, yw + 0.02)), ((t0 - 0.02, yw + 0.02), (t1 + 0.02, yw + 0.02)),
                               ((t1 + 0.02, yw + 0.02), (t1 + 0.02, -yw - 0.02)), ((t1 + 0.02, -yw - 0.02), (t0 - 0.02, -yw - 0.02))):
        b.face([on_panel(ta, ya, 0.0), on_panel(tb, yb, 0.0), on_panel(tb, yb, bz), on_panel(ta, ya, bz)][::-1], "EU_dark")
    # the screen itself 2 mm over the bezel, UVs 0..1 (the game draws on it)
    q = [on_panel(t0, -yw, bz + 0.002), on_panel(t1, -yw, bz + 0.002), on_panel(t1, yw, bz + 0.002), on_panel(t0, yw, bz + 0.002)]
    b.face(q[::-1], "EU_screen", [(1, 1), (1, 0), (0, 0), (0, 1)])
    # the big button on the desk's front lip, and its collar
    bx, bzz = D - 0.05, H - 0.08
    b.cyl("tx_steel", (D, 0.26, bzz), (D + 0.03, 0.26, bzz), 0.06, n=20)
    b.cyl("EU_red", (D + 0.03, 0.26, bzz), (D + 0.07, 0.26, bzz), 0.045, n=20)
    # feet
    for y in (-W / 2 + 0.05, W / 2 - 0.05):
        b.box("tx_steel", (D / 2, y, 0.015), (D + 0.04, 0.06, 0.03))
    _ = bx
    o = b.to_object("SM_EU_Console")
    panel = {"screen": [list(on_panel((t0 + t1) / 2, 0.0, bz)), list(nrm) ], "button": [D + 0.07, 0.26, bzz], "size": [W, D, H + 0.34]}
    return o, panel


def water():
    b = Builder()
    b.disc("EU_water", (0, 0), 0.0, ROOM_R - 0.004, n=48, uv_scale=True)
    b.disc("EU_water", (0, 0), 0.0, ROOM_R - 0.004, n=48, up=False, uv_scale=True)
    return b.to_object("SM_EU_Water", weld=False)


def shaft_b():
    b = Builder()
    b.tube("tx_paint", (0, 0), OPEN_R, OPEN_R + TUBE_T, SHAFT_B_BOTTOM, 0.0, n=36, outer=False)
    b.disc("tx_steel", (0, 0), SHAFT_B_BOTTOM, OPEN_R + 0.002, n=36)
    ladder(b, (0, 0), SHAFT_B_BOTTOM, -0.12)
    return b.to_object("SM_EU_ShaftB")


# --- Europa's ice ------------------------------------------------------------------------------------------------
def _hash(i, j, k=0):
    h = (i * 73856093) ^ (j * 19349663) ^ (k * 83492791)
    h = (h ^ (h >> 13)) * 0x5bd1e995 & 0xffffffff
    return ((h ^ (h >> 15)) & 0xffffff) / float(0xffffff)


def vnoise2(x, y, seed):
    i, j = math.floor(x), math.floor(y)
    fx, fy = x - i, y - j
    ux, uy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = _hash(i, j, seed), _hash(i + 1, j, seed)
    c, d = _hash(i, j + 1, seed), _hash(i + 1, j + 1, seed)
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy


def fbm2(x, y, seed, octaves=5):
    s, a, t = 0.0, 1.0, 0.0
    for o in range(octaves):
        s += a * vnoise2(x, y, seed + o * 31)
        t += a
        x, y, a = x * 2.03, y * 2.03, a * 0.5
    return s / t


# two double ridges (Europa's commonest landform): centre line (a point and a heading), crest height, spacing
RIDGES = [((-60.0, 40.0), math.radians(28), 2.6, 18.0), ((380.0, -900.0), math.radians(-62), 4.5, 30.0)]


def ice_height(x, y):
    r = math.hypot(x, y)
    h = 0.0
    # rolling plains: long swells and hummocks
    h += 6.0 * (fbm2(x / 700.0, y / 700.0, 11, 4) - 0.5)
    h += 1.2 * (fbm2(x / 90.0, y / 90.0, 23, 4) - 0.5)
    h += 0.25 * (fbm2(x / 12.0, y / 12.0, 37, 3) - 0.5)
    for (px, py), hd, ht, sp in RIDGES:
        dx, dy = x - px, y - py
        along = dx * math.cos(hd) + dy * math.sin(hd)
        across = -dx * math.sin(hd) + dy * math.cos(hd)
        across += 8.0 * (fbm2(along / 300.0, 0.5, 51, 3) - 0.5) * 2
        crest = ht * (math.exp(-((abs(across) - sp * 0.5) / (sp * 0.28)) ** 2))
        trough = -0.5 * ht * math.exp(-(across / (sp * 0.18)) ** 2)
        h += (crest + trough) * (0.7 + 0.6 * fbm2(along / 220.0, 3.3, 61, 2))
    # the hatch's apron: flat round the collar, blending out by 9 m
    k = min(max((r - 3.0) / 6.0, 0.0), 1.0)
    k = k * k * (3 - 2 * k)
    h = SITE_Z + (h - h_at_site()) * k
    # Europa's curve: the ground falls away from the hatch, so the horizon is where it is on a 1561 km moon
    return h - r * r / (2 * EUROPA_R)


_site = None


def h_at_site():
    global _site
    if _site is None:
        _site = 0.0
        x = y = 0.0
        _site = 6.0 * (fbm2(x / 700.0, y / 700.0, 11, 4) - 0.5) + 1.2 * (fbm2(x / 90.0, y / 90.0, 23, 4) - 0.5) \
            + 0.25 * (fbm2(x / 12.0, y / 12.0, 37, 3) - 0.5)
    return _site


def ground():
    """A polar grid: rings 0.5 m apart near the hatch growing to ~90 m at the rim, 256 segments; a hole under the
    collar (r 1.1 m)."""
    seg = 256
    radii = [1.1]
    r = 1.1
    while r < GROUND_R:
        r += max(0.5, r * 0.022)
        radii.append(min(r, GROUND_R))
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    rows = []
    for r in radii:
        row = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            x, y = r * math.cos(a), r * math.sin(a)
            row.append(bm.verts.new((x, y, ice_height(x, y))))
        rows.append(row)
    m = 0
    for k in range(len(rows) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            f = bm.faces.new((rows[k][i], rows[k][j], rows[k + 1][j], rows[k + 1][i]))
            f.material_index = m
            for lp in f.loops:
                lp[uvl].uv = (lp.vert.co.x, lp.vert.co.y)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.z < 0:
            f.normal_flip()
    me = bpy.data.meshes.new("SM_EU_Ground")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(material("tx_ice"))
    o = bpy.data.objects.new("SM_EU_Ground", me)
    bpy.context.scene.collection.objects.link(o)
    for p in me.polygons:
        p.use_smooth = True
    print("EUKIT ground: %d rings, %d faces" % (len(radii), len(me.polygons)))
    return o


def ice_blocks():
    """Blocks of ice: rough faceted lumps sunk into the ground, a few near the hatch, more along the ridges."""
    rnd = random.Random(1610)
    b = Builder()
    spots = [(6.5, 3.0, 0.7), (-5.0, 7.5, 1.1), (9.0, -6.0, 0.5), (-12.0, -9.0, 1.6), (22.0, 14.0, 2.2)]
    for _ in range(26):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(25, 400)
        spots.append((r * math.cos(a), r * math.sin(a), rnd.uniform(0.8, 4.5)))
    for (px, py), hd, ht, sp in RIDGES:
        for t in range(-6, 7):
            if rnd.random() < 0.6:
                s = sp * 0.5 * rnd.choice((-1, 1))
                x = px + t * 45 * math.cos(hd) - s * math.sin(hd)
                y = py + t * 45 * math.sin(hd) + s * math.cos(hd)
                spots.append((x, y, rnd.uniform(1.5, 4.0)))
    for x, y, size in spots:
        if math.hypot(x, y) < 4.5:
            continue
        z = ice_height(x, y)
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=1.0)
        sx, sy, sz = size * rnd.uniform(0.7, 1.3), size * rnd.uniform(0.7, 1.3), size * rnd.uniform(0.35, 0.8)
        rot = Matrix.Rotation(rnd.uniform(0, 6.3), 3, "Z") @ Matrix.Rotation(rnd.uniform(-0.3, 0.3), 3, "X")
        pts = []
        for v in bm.verts:
            p = v.co * (1 + rnd.uniform(-0.18, 0.18))
            p = rot @ Vector((p.x * sx, p.y * sy, p.z * sz))
            pts.append(p)
        for f in bm.faces:
            b.face([(x + pts[v.index].x, y + pts[v.index].y, z - sz * 0.3 + pts[v.index].z) for v in f.verts], "tx_ice")
        bm.free()
    o = b.to_object("SM_EU_IceBlocks", weld=False)
    return o


def planet():
    """A unit sphere, 128 x 64, equirectangular UVs (u = (lon + 180) / 360 with lon 0 toward +X, v = latitude up)."""
    nu, nv = 128, 64
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    grid = []
    for j in range(nv + 1):
        lat = -math.pi / 2 + math.pi * j / nv
        row = []
        for i in range(nu + 1):
            lon = -math.pi + 2 * math.pi * i / nu
            row.append(bm.verts.new((math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat))))
        grid.append(row)
    for j in range(nv):
        for i in range(nu):
            vs = [grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]]
            uvs = [(i / nu, j / nv), ((i + 1) / nu, j / nv), ((i + 1) / nu, (j + 1) / nv), (i / nu, (j + 1) / nv)]
            if j == 0:
                vs, uvs = [vs[0], vs[2], vs[3]], [((i + 0.5) / nu, 0.0), uvs[2], uvs[3]]
            elif j == nv - 1:
                vs, uvs = [vs[0], vs[1], vs[2]], [uvs[0], uvs[1], ((i + 0.5) / nu, 1.0)]
            f = bm.faces.new(vs)
            for lp, t in zip(f.loops, uvs):
                lp[uvl].uv = t
    me = bpy.data.meshes.new("SM_EU_Planet")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(material("EU_planet"))
    o = bpy.data.objects.new("SM_EU_Planet", me)
    bpy.context.scene.collection.objects.link(o)
    for p in me.polygons:
        p.use_smooth = True
    return o


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def place(o, loc=(0, 0, 0), rot=None, own=False):
    c = o.copy()
    if own:
        c.data = o.data.copy()
    c.location = loc
    if rot is not None:
        c.rotation_euler = rot
    bpy.context.scene.collection.objects.link(c)
    return c


def preview(out_dir, pieces, lamps):
    """EEVEE renders: the hatch on its bed, the room from the door of the sea shaft, the console, Europa's ice."""
    os.makedirs(out_dir, exist_ok=True)
    scn = bpy.context.scene
    names = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scn.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in names else "BLENDER_EEVEE_NEXT"
    scn.render.resolution_x, scn.render.resolution_y = 1280, 720
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.25, 0.3, 0.35, 1)
    scn.world = world
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    temp = []
    for p in lamps:
        L = bpy.data.objects.new("lamp", bpy.data.lights.new("lamp", "POINT"))
        L.data.energy = 120
        L.data.color = (1.0, 0.85, 0.65)
        L.location = p
        scn.collection.objects.link(L)
        temp.append(L)
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(40), 0, math.radians(30))
    scn.collection.objects.link(sun)
    temp.append(sun)
    def shot(name, loc, look, lens=24, show=()):
        for o in bpy.data.objects:
            if o.type == "MESH":
                o.hide_render = o.name not in show
        cam.location = loc
        cam.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        scn.render.filepath = os.path.join(out_dir, name + ".png")
        bpy.ops.render.render(write_still=True)
    lid_open = place(pieces["lid"], (HINGE_X, 0, 0.05), (0, math.radians(-110), 0))
    shot("hatch", (2.6, -2.2, 1.3), (0, 0, -0.1), 28, {pieces["collar"].name, lid_open.name})
    lid_in = place(pieces["lid"], (EUROPA_SHAFT[0] + HINGE_X, 0, CEIL_Z - 0.05), (math.pi, math.radians(-90), 0))
    cons = place(pieces["console"], (ROOM_C[0], -ROOM_R + 0.06, FLOOR_Z), (0, 0, math.radians(90)))
    wat = place(pieces["water"], (ROOM_C[0], 0, FLOOR_Z + 0.9))
    show = {pieces["airlock"].name, lid_in.name, cons.name, wat.name}
    shot("room", (ROOM_C[0] + 1.3, 1.2, FLOOR_Z + 1.6), (ROOM_C[0] - 1.0, -0.8, FLOOR_Z + 0.9), 16, show)
    shot("room_up", (ROOM_C[0] + 0.2, -0.3, FLOOR_Z + 1.6), (EUROPA_SHAFT[0], 0, CEIL_Z), 16, show)
    shot("console", (ROOM_C[0] + 0.3, -0.4, FLOOR_Z + 1.6), (ROOM_C[0], -ROOM_R + 0.3, FLOOR_Z + 0.9), 24, show)
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.0, 0.0, 0.0, 1)
    pl = place(pieces["planet"], (-3000, 0, 2500))
    pl.scale = (900, 900, 900)
    shot("europa", (12, 6, 1.7), (-60, 0, 25), 18, {pieces["ground"].name, pieces["blocks"].name, pieces["collar"].name, pl.name})
    for o in (lid_open, lid_in, cons, wat, pl, *temp):
        bpy.data.objects.remove(o, do_unlink=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    prev = argv[argv.index("--preview") + 1] if "--preview" in argv else None
    clear()
    pieces = {}
    pieces["collar"] = collar()
    pieces["lid"] = lid()
    pieces["airlock"], lamps = airlock()
    pieces["console"], panel = console()
    pieces["water"] = water()
    pieces["shaft_b"] = shaft_b()
    pieces["ground"] = ground()
    pieces["blocks"] = ice_blocks()
    pieces["planet"] = planet()
    if prev:
        preview(prev, pieces, lamps)
    # glitch checks (blender/railway/kit_lint.py): the installation as the game assembles it (collar, room, shut lids,
    # console in place), then the other pieces on their own
    sys.path.insert(0, os.path.join(REPO, "blender", "railway"))
    import kit_lint
    sea_lid = place(pieces["lid"], (HINGE_X, 0, 0.05), own=True)
    in_lid = place(pieces["lid"], (EUROPA_SHAFT[0] + HINGE_X, 0, CEIL_Z - 0.05), (math.pi, 0, 0), own=True)
    cons = place(pieces["console"], (ROOM_C[0], -ROOM_R + 0.06, FLOOR_Z), (0, 0, math.radians(90)), own=True)
    bpy.context.view_layer.update()
    for o in (sea_lid, in_lid, cons):
        bpy.context.view_layer.objects.active = o
        o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.select_all(action="DESELECT")
    problems = kit_lint.lint([pieces["collar"], pieces["airlock"], sea_lid, in_lid, cons], "airlock")
    for o in (sea_lid, in_lid, cons):
        bpy.data.objects.remove(o, do_unlink=True)
    problems += kit_lint.lint([pieces["shaft_b"], pieces["collar"]], "shaft_b")
    problems += kit_lint.lint([pieces["lid"]], "lid")
    problems += kit_lint.lint([pieces["console"]], "console")
    problems += kit_lint.lint([pieces["blocks"]], "blocks")
    if "--strict" in argv and problems:
        raise SystemExit("EUKIT: %d glitch findings (KITLINT lines above)" % len(problems))
    os.makedirs(os.path.dirname(OUT_GLB), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in pieces.values():
        o.location = (0, 0, 0)
        o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=OUT_GLB, use_selection=True, export_format="GLB", export_yup=True, export_apply=True,
                              export_image_format="NONE")
    def ue(p):
        return [round(p[0] * 100, 2), round(-p[1] * 100, 2), round(p[2] * 100, 2)]
    kit = {
        "_doc": "Made by blender/europa/europa_kit.py. UE centimetres in the airlock's frame (origin = the sea hatch's lid "
                "seat, X toward the ladders); the actor ALyudAirlock reads it.",
        "open_radius": OPEN_R * 100, "lid_radius": LID_R * 100,
        "sea_hinge": ue((HINGE_X, 0, 0.05)),
        "inner_hinge": ue((EUROPA_SHAFT[0] + HINGE_X, 0, CEIL_Z - 0.05)),
        "sea_shaft": ue((0, 0, 0)), "europa_shaft": ue((EUROPA_SHAFT[0], 0, 0)),
        "europa_top": EUROPA_TOP * 100, "shaft_b_bottom": SHAFT_B_BOTTOM * 100,
        "ladder_offset": LADDER_OFF * 100,
        "floor": FLOOR_Z * 100, "ceiling": CEIL_Z * 100,
        "room_center": ue((ROOM_C[0], ROOM_C[1], 0)), "room_radius": ROOM_R * 100,
        "console": {"at": ue((ROOM_C[0], -ROOM_R + 0.06, FLOOR_Z)), "yaw": -90.0,
                    "screen": [round(v * 100, 2) for v in panel["screen"][0]], "button": [round(v * 100, 2) for v in panel["button"]]},
        "lamps": [ue(p) for p in lamps],
        "collar_bottom": COLLAR_BOTTOM * 100, "site_z": SITE_Z * 100,
        "ground_radius": GROUND_R * 100, "europa_radius_m": EUROPA_R,
        "tiles": {k: v * 100 for k, v in TILE.items()},
    }
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w") as f:
        json.dump(kit, f, indent=1)
        f.write("\n")
    print("EUKIT wrote %s and %s, %d lint findings" % (OUT_GLB, OUT_JSON, len(problems)))


main()
