"""The sea railway's kit (docs/railway.md): clean game meshes made here from primitives, no generated models (the
project's rule for things you walk into: a real wall thickness, boolean-cut openings, own collision).

  /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/railway/rail_kit.py [-- --preview out_dir]

  -> design/railway/game/rail_kit.glb            every piece, one mesh object each (SM_RL_*)
  -> unreal/Lyudmilovsk/Content/Railway/kit.json  sockets the game reads: seats, doors, lamps, the pantograph's head

The car: a two-car local of a Showa-era Japanese line, 16 m cars, cream over deep green with an orange belt, a flat
cab front with two round headlamps low, red tail lamps over them and a lit destination box under the roof; two
doorways a side (clear 1.20 x 2.15 m, the character standard's 1.08 x 2.10 at least), long moquette benches along the
walls, ceiling fans, a luggage rack, warm lamps. Frame: Blender metres, X along the car (+X its cab end), Y left,
Z up from the rail head (the game puts it on the rails). UE gets (x, -y, z) * 100.

Other pieces: the level crossing's post (crossbuck, two red lamps, the bell), its barrier (box and striped arm), the
halt's shelter and nameboard, the catenary mast, a sleeper, a gangway bellows, a bogie, the pantograph, and the island
(its ground sampled from tools/railway.py's island_height, the same function the line and the people read).
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT_GLB = os.path.join(REPO, "design", "railway", "game", "rail_kit.glb")
OUT_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Railway", "kit.json")
sys.path.insert(0, os.path.join(REPO, "tools"))

# --- the car's measures (metres) --------------------------------------------------------------------------------
L = 16.0              # body length
HALF_W = 1.40         # body half width at the sill
SILL_Z = 1.00         # bottom of the body side over the rail head
FLOOR_Z = 1.15        # floor top (ULyudRailwaySubsystem::FloorOverRail)
EAVE_Z = 3.40
ROOF_Z = 3.95
SKIN = 0.06           # wall thickness
DOOR_X = (-4.55, 4.55)
DOOR_W = 1.20         # clear
DOOR_TOP = FLOOR_Z + 2.15
WIN_Z = (2.02, 2.92)
WINDOWS = [-3.2, -1.95, -0.65, 0.65, 1.95, 3.2, -6.35, 6.35]   # centres along X, each 1.05 wide
WIN_W = 1.05
SEAT_TOP = FLOOR_Z + 0.42
SEAT_D = 0.52

PALETTE = {
    # name: (base colour sRGB 0..1, roughness, metallic, emission strength)
    "RL_cream": ((0.93, 0.88, 0.74), 0.45, 0.0, 0.0),
    "RL_green": ((0.11, 0.31, 0.22), 0.4, 0.0, 0.0),
    "RL_orange": ((0.90, 0.45, 0.12), 0.4, 0.0, 0.0),
    "RL_roof": ((0.36, 0.37, 0.38), 0.7, 0.0, 0.0),
    "RL_under": ((0.10, 0.10, 0.11), 0.8, 0.0, 0.0),
    "RL_black": ((0.03, 0.03, 0.03), 0.6, 0.0, 0.0),
    "RL_chrome": ((0.80, 0.80, 0.82), 0.25, 1.0, 0.0),
    "RL_wall_in": ((0.74, 0.80, 0.72), 0.6, 0.0, 0.0),
    "RL_ceiling": ((0.92, 0.91, 0.86), 0.7, 0.0, 0.0),
    "RL_floor": ((0.36, 0.23, 0.14), 0.7, 0.0, 0.0),
    "RL_seat": ((0.42, 0.07, 0.10), 0.95, 0.0, 0.0),
    "RL_glass": ((0.55, 0.65, 0.66), 0.05, 0.0, 0.0),
    "RL_lamp": ((1.00, 0.80, 0.52), 0.5, 0.0, 4.0),
    "RL_head": ((1.00, 0.96, 0.86), 0.3, 0.0, 6.0),
    "RL_tail": ((1.00, 0.08, 0.04), 0.3, 0.0, 3.0),
    "RL_sign": ((0.96, 0.95, 0.88), 0.5, 0.0, 2.0),
    "RL_yellow": ((0.95, 0.75, 0.05), 0.5, 0.0, 0.0),
    "RL_xlamp": ((0.30, 0.02, 0.02), 0.2, 0.0, 0.0),
    "RL_xglow": ((1.00, 0.05, 0.02), 0.3, 0.0, 8.0),
    "RL_wood": ((0.45, 0.30, 0.18), 0.8, 0.0, 0.0),
    "RL_wood_dark": ((0.22, 0.14, 0.09), 0.85, 0.0, 0.0),
    "RL_tin": ((0.55, 0.20, 0.13), 0.6, 0.2, 0.0),
    "RL_white": ((0.94, 0.94, 0.92), 0.5, 0.0, 0.0),
    "RL_concrete": ((0.62, 0.61, 0.58), 0.9, 0.0, 0.0),
    "RL_steel": ((0.45, 0.46, 0.47), 0.5, 0.8, 0.0),
    "RL_insulator": ((0.55, 0.30, 0.20), 0.2, 0.0, 0.0),
    "RL_sand": ((0.78, 0.70, 0.52), 0.9, 0.0, 0.0),
    "RL_wetsand": ((0.50, 0.44, 0.33), 0.5, 0.0, 0.0),
    "RL_grass": ((0.33, 0.40, 0.18), 0.9, 0.0, 0.0),
    "RL_rock": ((0.42, 0.40, 0.37), 0.85, 0.0, 0.0),
    # the inside's textured surfaces (tools/railway_textures.py grid_b; the game: M_TR_Surface "tx_<name>", metre UVs)
    "tx_moquette": ((0.45, 0.07, 0.10), 0.95, 0.0, 0.0),
    "tx_decora": ((0.66, 0.82, 0.72), 0.4, 0.0, 0.0),
    "tx_floorwood": ((0.40, 0.25, 0.14), 0.6, 0.0, 0.0),
    "tx_grille": ((0.78, 0.76, 0.70), 0.6, 0.0, 0.0),
    "tx_ceiling": ((0.92, 0.91, 0.86), 0.7, 0.0, 0.0),
    "tx_blind": ((0.86, 0.78, 0.60), 0.9, 0.0, 0.0),
    "tx_varnish": ((0.55, 0.30, 0.15), 0.3, 0.0, 0.0),
    "tx_studs": ((0.25, 0.25, 0.25), 0.7, 0.0, 0.0),
    # all the printed matter in one atlas (tools/train_ads.py; the game: the player's language's atlas)
    "ad_atlas": ((1.0, 1.0, 1.0), 0.55, 0.0, 0.0),
}
TEX_DIR = os.path.join(REPO, "design", "railway", "textures")
ATLAS_JSON = os.path.join(REPO, "design", "railway", "ads", "atlas.json")
ATLAS_IMG = os.path.join(REPO, "textures", "localized", "en", "train_atlas.jpg")
with open(ATLAS_JSON) as _f:
    _atlas = json.load(_f)
ATLAS_SIZE = _atlas["size"][0]
PRINTS = _atlas["prints"]


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
    img = None
    if name.startswith("tx_"):
        img = os.path.join(TEX_DIR, "T_RL_%s_C.png" % name[3:])
    elif name == "ad_atlas":
        img = ATLAS_IMG
    if img and os.path.exists(img):
        nt = m.node_tree
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = bpy.data.images.load(img, check_existing=True)
        if name.startswith("tx_"):
            with open(os.path.join(TEX_DIR, "textures.json")) as f:
                tile = json.load(f)["textures"][name[3:]]["tile_m"]
            mp = nt.nodes.new("ShaderNodeMapping")
            mp.inputs["Scale"].default_value = (1 / tile, 1 / tile, 1)
            co = nt.nodes.new("ShaderNodeTexCoord")
            nt.links.new(co.outputs["UV"], mp.inputs["Vector"])
            nt.links.new(mp.outputs["Vector"], t.inputs["Vector"])
        nt.links.new(t.outputs["Color"], b.inputs["Base Color"])
    if name == "RL_glass":
        b.inputs["Alpha"].default_value = 0.35
        m.blend_method = "BLEND" if hasattr(m, "blend_method") else None
    return m


# --- a small mesh builder: faces per material into one bmesh ------------------------------------------------------
def metre_uv(pts):
    """Planar UVs in metres, projected along the face's main axis (the game's textures tile by the metre)."""
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


def metre_uvs(o):
    """The same for a finished object (the body after its booleans)."""
    me = o.data
    uv = me.uv_layers.new(name="UVMap") if not me.uv_layers else me.uv_layers[0]
    for p in me.polygons:
        pts = [me.vertices[me.loops[i].vertex_index].co.copy() for i in p.loop_indices]
        for i, t in zip(p.loop_indices, metre_uv(pts)):
            uv.data[i].uv = t


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

    def card(self, rect, c, r, u, w, h, d, edge="RL_chrome", back=None):
        """A print: a thin closed box (w x h, d deep) whose front (facing r x u) shows the atlas rectangle rect
        (pixels from the top left); back: another print on the back face (a hanging poster), else edge material."""
        c, r, u = Vector(c), Vector(r).normalized(), Vector(u).normalized()
        n = r.cross(u)
        def P(i, j, k):
            return c + r * (i * w / 2) + u * (j * h / 2) + n * (k * d / 2)
        def uvq(rc):
            S = ATLAS_SIZE
            u0, u1 = rc[0] / S, rc[2] / S
            vb, vt = 1 - rc[3] / S, 1 - rc[1] / S
            return [(u0, vb), (u1, vb), (u1, vt), (u0, vt)]
        self.face([P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1)], "ad_atlas", uvq(rect))
        if back:
            # seen from behind its right is -r: the same corners, the picture mirrored back
            self.face([P(1, -1, -1), P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1)], "ad_atlas", uvq(back))
        else:
            self.face([P(1, -1, -1), P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1)], edge)
        self.face([P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1)], edge)
        self.face([P(1, 1, -1), P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1)], edge)
        self.face([P(-1, 1, -1), P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1)], edge)
        self.face([P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1)], edge)

    def box(self, mat, c, s, rot_z=0.0, mats=None):
        """Box at centre c, full size s; mats: optional dict face->mat ('top','bottom','x+','x-','y+','y-')."""
        cx, cy, cz = c
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        R = Matrix.Rotation(rot_z, 3, "Z")
        def P(x, y, z):
            v = R @ Vector((x, y, z))
            return (cx + v.x, cy + v.y, cz + v.z)
        mats = mats or {}
        q = [
            ("bottom", [P(-hx, -hy, -hz), P(-hx, hy, -hz), P(hx, hy, -hz), P(hx, -hy, -hz)]),
            ("top", [P(-hx, -hy, hz), P(hx, -hy, hz), P(hx, hy, hz), P(-hx, hy, hz)]),
            ("y-", [P(-hx, -hy, -hz), P(hx, -hy, -hz), P(hx, -hy, hz), P(-hx, -hy, hz)]),
            ("y+", [P(hx, hy, -hz), P(-hx, hy, -hz), P(-hx, hy, hz), P(hx, hy, hz)]),
            ("x-", [P(-hx, hy, -hz), P(-hx, -hy, -hz), P(-hx, -hy, hz), P(-hx, hy, hz)]),
            ("x+", [P(hx, -hy, -hz), P(hx, hy, -hz), P(hx, hy, hz), P(hx, -hy, hz)]),
        ]
        for k, pts in q:
            self.face(pts, mats.get(k, mat))

    def cyl(self, mat, a, b, r, n=12, caps=True, r2=None, cap_mat=None):
        """A cylinder (or cone with r2) from point a to b."""
        a, b = Vector(a), Vector(b)
        ax = (b - a)
        ln = ax.length
        if ln < 1e-6:
            return
        z = ax / ln
        x = z.orthogonal().normalized()
        y = z.cross(x)
        r2 = r if r2 is None else r2
        ra, rb = [], []
        for i in range(n):
            t = 2 * math.pi * i / n
            d = x * math.cos(t) + y * math.sin(t)
            ra.append(a + d * r)
            rb.append(b + d * r2)
        for i in range(n):
            j = (i + 1) % n
            self.face([ra[i], ra[j], rb[j], rb[i]], mat)
        if caps:
            self.face(list(reversed(ra)), cap_mat or mat)
            self.face(rb, cap_mat or mat)

    def disc(self, mat, c, normal, r, n=16):
        c, nz = Vector(c), Vector(normal).normalized()
        x = nz.orthogonal().normalized()
        y = nz.cross(x)
        self.face([c + (x * math.cos(2 * math.pi * i / n) + y * math.sin(2 * math.pi * i / n)) * r for i in range(n)], mat)

    def torus_ring(self, mat, c, normal, r, t, n=16, m=6):
        c, nz = Vector(c), Vector(normal).normalized()
        x = nz.orthogonal().normalized()
        y = nz.cross(x)
        rings = []
        for i in range(n):
            a = 2 * math.pi * i / n
            d = x * math.cos(a) + y * math.sin(a)
            ring = []
            for k in range(m):
                b = 2 * math.pi * k / m
                ring.append(c + d * (r + t * math.cos(b)) + nz * t * math.sin(b))
            rings.append(ring)
        for i in range(n):
            for k in range(m):
                i2, k2 = (i + 1) % n, (k + 1) % m
                self.face([rings[i][k], rings[i2][k], rings[i2][k2], rings[i][k2]], mat)

    def to_object(self, name):
        me = bpy.data.meshes.new(name)
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-5)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(material(m))
        o = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(o)
        for p in me.polygons:
            p.use_smooth = False
        return o


def obj_from_builder(name, b):
    return b.to_object(name)


def apply_mods(o):
    bpy.context.view_layer.objects.active = o
    for m in list(o.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def boolean_cut(o, cutters):
    """Exact boolean difference of the union of cutters from o; the cutters are removed."""
    for c in cutters:
        m = o.modifiers.new("cut", "BOOLEAN")
        m.operation = "DIFFERENCE"
        m.solver = "EXACT"
        m.object = c
        c.hide_render = True
        apply_mods(o)
    for c in cutters:
        bpy.data.objects.remove(c, do_unlink=True)


def rounded_rect(cx, cz, w, h, r, n=4):
    """A rounded rectangle in (x, z), counterclockwise."""
    pts = []
    for (ox, oz, a0) in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((cx + ox + r * math.cos(a), cz + oz + r * math.sin(a)))
    return pts


def cutter_y(name, poly_xz, y0, y1):
    """A prism along Y from y0 to y1 over a polygon in (x, z)."""
    b = Builder()
    n = len(poly_xz)
    a = [(x, y0, z) for x, z in poly_xz]
    c = [(x, y1, z) for x, z in poly_xz]
    for i in range(n):
        j = (i + 1) % n
        b.face([a[i], a[j], c[j], c[i]], "RL_black")
    b.face(list(reversed(a)), "RL_black")
    b.face(c, "RL_black")
    return b.to_object(name)


def cutter_x(name, poly_yz, x0, x1):
    b = Builder()
    n = len(poly_yz)
    a = [(x0, y, z) for y, z in poly_yz]
    c = [(x1, y, z) for y, z in poly_yz]
    for i in range(n):
        j = (i + 1) % n
        b.face([a[i], a[j], c[j], c[i]], "RL_black")
    b.face(list(reversed(a)), "RL_black")
    b.face(c, "RL_black")
    return b.to_object(name)


# --- the car body ----------------------------------------------------------------------------------------------
def body_profile(n_roof=10):
    """Half of the cross-section's outside, from the sill up over the roof's crown: (y, z)."""
    pts = [(HALF_W - 0.02, SILL_Z), (HALF_W, SILL_Z + 0.25), (HALF_W, 2.0), (HALF_W - 0.03, EAVE_Z - 0.15)]
    # the eave turns into a low roof arc
    for i in range(n_roof + 1):
        t = i / n_roof
        a = math.radians(0 + 90 * t)
        y = (HALF_W - 0.03) * math.cos(a) ** 0.9 if t < 1 else 0.0
        z = EAVE_Z - 0.15 + (ROOF_Z - EAVE_Z + 0.15) * math.sin(a) ** 0.7
        pts.append((max(y, 0.0), z))
    return pts


def car_body():
    half = body_profile()
    # the full ring: right side (y-) up, over the roof, left side (y+) down, closed along the floor
    ring = [(-y, z) for (y, z) in half] + [(y, z) for (y, z) in reversed(half[:-1])]
    # make it counterclockwise seen from +X
    b = Builder()
    n = len(ring)
    xs = [-L / 2, L / 2]
    rings = []
    for x in xs:
        rings.append([(x, y, z) for (y, z) in ring])
    for i in range(n):
        j = (i + 1) % n
        b.face([rings[0][i], rings[0][j], rings[1][j], rings[1][i]], "RL_green")
    # ends: the cab front (+X) and the gangway end (-X), flat
    b.face(rings[1], "RL_green")
    b.face(list(reversed(rings[0])), "RL_green")
    o = b.to_object("SM_RL_Car")
    o.data.materials.append(material("tx_decora"))
    sol = o.modifiers.new("skin", "SOLIDIFY")
    sol.thickness = SKIN
    sol.offset = -1.0
    sol.use_rim = True
    sol.material_offset = 1          # the inner shell: the interior's paint
    sol.material_offset_rim = 0
    apply_mods(o)
    # the paint: split the outside at the belt's edges, then paint by height (green below, the orange belt, cream
    # above, the roof grey, the underside dark); inside, the ceiling lighter than the walls
    for m in ("RL_cream", "RL_orange", "RL_roof", "RL_under", "tx_ceiling"):
        o.data.materials.append(material(m))
    names = [m.name for m in o.data.materials]
    bm = bmesh.new()
    bm.from_mesh(o.data)
    for zc in (1.80, 1.88):
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, zc), plane_no=(0, 0, 1))
    for f in bm.faces:
        c = f.calc_center_median()
        nz = f.normal.z
        if f.material_index == names.index("tx_decora"):
            if c.z > EAVE_Z - 0.2:
                f.material_index = names.index("tx_ceiling")
            continue
        if nz < -0.5:
            f.material_index = names.index("RL_under")
        elif c.z > EAVE_Z - 0.1 and nz > 0.3:
            f.material_index = names.index("RL_roof")
        elif c.z > 1.88:
            f.material_index = names.index("RL_cream")
        elif c.z > 1.80:
            f.material_index = names.index("RL_orange")
        else:
            f.material_index = names.index("RL_green")
    bm.to_mesh(o.data)
    bm.free()

    cutters = []
    # doorways on both sides
    for dx in DOOR_X:
        poly = [(dx - DOOR_W / 2, FLOOR_Z - 0.01), (dx + DOOR_W / 2, FLOOR_Z - 0.01), (dx + DOOR_W / 2, DOOR_TOP), (dx - DOOR_W / 2, DOOR_TOP)]
        cutters.append(cutter_y("cut_door", poly, -HALF_W - 0.3, HALF_W + 0.3))
    for wx in WINDOWS:
        cutters.append(cutter_y("cut_win", rounded_rect(wx, (WIN_Z[0] + WIN_Z[1]) / 2, WIN_W, WIN_Z[1] - WIN_Z[0], 0.09), -HALF_W - 0.3, HALF_W + 0.3))
    # the cab's front windows and the gangway door
    for wy in (-0.62, 0.62):
        poly = [(y, z) for (y, z) in [(wy - 0.42, 2.05), (wy + 0.42, 2.05), (wy + 0.42, 2.95), (wy - 0.42, 2.95)]]
        cutters.append(cutter_x("cut_front", poly, L / 2 - 0.3, L / 2 + 0.3))
    cutters.append(cutter_x("cut_gang", [(-0.45, FLOOR_Z - 0.01), (0.45, FLOOR_Z - 0.01), (0.45, FLOOR_Z + 2.0), (-0.45, FLOOR_Z + 2.0)],
                            -L / 2 - 0.3, -L / 2 + 0.3))
    boolean_cut(o, cutters)
    metre_uvs(o)
    return o


def car_details():
    """Glass, the front's lamps and trim, the rubber seals, the floor, the underframe: separate pieces."""
    glass = Builder()
    for side in (-1, 1):
        y = side * (HALF_W - SKIN / 2)
        for wx in WINDOWS:
            pts = rounded_rect(wx, (WIN_Z[0] + WIN_Z[1]) / 2, WIN_W - 0.02, WIN_Z[1] - WIN_Z[0] - 0.02, 0.08)
            f = [(x, y, z) for x, z in pts]
            glass.face(f if side < 0 else list(reversed(f)), "RL_glass")
    for wy in (-0.62, 0.62):
        x = L / 2 - SKIN / 2
        glass.face([(x, wy - 0.41, 2.06), (x, wy + 0.41, 2.06), (x, wy + 0.41, 2.94), (x, wy - 0.41, 2.94)], "RL_glass")
    g = glass.to_object("SM_RL_CarGlass")

    t = Builder()
    xf = L / 2 + 0.005
    # two round headlamps low on the front, chrome rims; red tail lamps over them; the destination box under the roof
    for yy in (-0.95, 0.95):
        t.cyl("RL_chrome", (xf - 0.02, yy, 1.55), (xf + 0.10, yy, 1.55), 0.17, n=20, caps=False)
        t.disc("RL_head", (xf + 0.09, yy, 1.55), (1, 0, 0), 0.15, n=20)
        t.cyl("RL_chrome", (xf - 0.02, yy, 1.86), (xf + 0.06, yy, 1.86), 0.075, n=14, caps=False)
        t.disc("RL_tail", (xf + 0.055, yy, 1.86), (1, 0, 0), 0.065, n=14)
    t.box("RL_black", (xf + 0.02, 0, 3.22), (0.06, 0.95, 0.24))
    t.box("RL_sign", (xf + 0.054, 0, 3.22), (0.002, 0.85, 0.17))
    # the orange belt across the front and its chrome edge, the window divider
    t.box("RL_chrome", (xf + 0.006, 0, 1.92), (0.012, 2.6, 0.02))
    # wipers, grab rails, a horn, the coupler
    for wy in (-0.62, 0.62):
        t.cyl("RL_black", (xf + 0.03, wy + 0.25, 2.1), (xf + 0.03, wy - 0.15, 2.65), 0.012, n=6)
    for yy in (-1.25, 1.25):
        t.cyl("RL_chrome", (xf + 0.06, yy, 1.6), (xf + 0.06, yy, 2.4), 0.018, n=8)
    t.box("RL_black", (xf + 0.25, 0, 0.88), (0.5, 0.3, 0.25))
    t.box("RL_under", (xf - 0.25, 0, 0.75), (0.5, 2.6, 0.35))
    # the side: rubber surrounds and the window's sash bars, door frames (chrome), the side destination plate
    for side in (-1, 1):
        y = side * (HALF_W + 0.004)
        for wx in WINDOWS:
            t.box("RL_cream", (wx, y, (WIN_Z[0] + WIN_Z[1]) / 2 + 0.02), (0.035, 0.01, WIN_Z[1] - WIN_Z[0] - 0.05))     # a sash bar
            t.box("RL_chrome", (wx, y, WIN_Z[0] - 0.02), (WIN_W + 0.04, 0.012, 0.03))
        for dx in DOOR_X:
            for e in (-1, 1):
                t.box("RL_chrome", (dx + e * (DOOR_W / 2 + 0.02), y, (FLOOR_Z + DOOR_TOP) / 2 - 0.002), (0.03, 0.012, DOOR_TOP - FLOOR_Z - 0.004))
            t.box("RL_chrome", (dx, y, DOOR_TOP + 0.02), (DOOR_W + 0.07, 0.012, 0.03))
            # the step under the door
            t.box("RL_under", (dx, side * (HALF_W - 0.1), SILL_Z - 0.08), (DOOR_W + 0.1, 0.25, 0.04))
        # rain gutter along the eave
        t.box("RL_roof", (0, side * (HALF_W - 0.01), EAVE_Z - 0.12), (L - 0.1, 0.05, 0.04))
    # the roof: running boards and the ventilators (Showa lantern vents)
    for vx in (-5.5, -2.8, 0.0, 2.8, 5.5):
        t.box("RL_roof", (vx, 0, ROOF_Z + 0.08), (0.8, 0.55, 0.16))
        t.box("RL_roof", (vx, 0, ROOF_Z + 0.17), (0.9, 0.62, 0.03))
    tr = t.to_object("SM_RL_CarTrim")

    # the underframe: skirt boxes between the bogies, the air tanks
    u = Builder()
    u.box("RL_under", (0, 0, 0.745), (8.6, 2.2, 0.49))
    for yy in (-0.7, 0.7):
        u.cyl("RL_under", (-3.5, yy, 0.62), (3.5, yy, 0.62), 0.2, n=12)
    u.box("RL_under", (0, 0, SILL_Z - 0.04), (L - 0.4, 2.6, 0.08))
    un = u.to_object("SM_RL_Underframe")
    return [g, tr, un]


def wall_y(z):
    """The inner face of the side wall at height z (the body's profile less its skin)."""
    pts = body_profile()
    for (y0, z0), (y1, z1) in zip(pts, pts[1:]):
        if z0 <= z <= z1 and z1 > z0:
            return y0 + (y1 - y0) * (z - z0) / (z1 - z0) - SKIN
    return HALF_W - SKIN


def extrude_x(b, mat, prof, x0, x1, cap=None):
    """A closed prism along X from a (y, z) outline (counterclockwise seen from +X)."""
    n = len(prof)
    A = [(x0, y, z) for (y, z) in prof]
    B = [(x1, y, z) for (y, z) in prof]
    for i in range(n):
        j = (i + 1) % n
        b.face([A[i], A[j], B[j], B[i]], mat)
    b.face(list(reversed(A)), cap or mat)
    b.face(B, cap or mat)


def arc(cy, cz, r, a0, a1, k=4):
    return [(cy + r * math.cos(math.radians(a0 + (a1 - a0) * i / k)), cz + r * math.sin(math.radians(a0 + (a1 - a0) * i / k)))
            for i in range(k + 1)]


# the hanging posters' places (x) and which ads they carry on their two faces; the framed cards over the windows
HANG = [(-6.0, "beach", "aton"), (-2.0, "cable", "starlings"), (2.0, "lectures", "festival"), (6.0, "kiosk", "sprats")]
FRAME_ORDER = ["bakery", "amber", "radio", "bath", "beach", "cable", "aton", "kiosk", "starlings", "lectures", "sprats", "festival"]


def car_interior():
    """A Showa local's inside (docs/railway.md "The inside"): oiled plank floor with studded rubber at the doors, wine
    moquette benches over louvred heater grilles, varnished end screens and window sills, roller blinds, a luggage
    rack of chrome rods on brackets, straps, globe lamps, caged ceiling fans, hanging posters over the aisle, framed
    cards over the racks, the line's map, the car's plates and the notices. Every print is a thin box a few mm off
    its surface (never coplanar with it: tools/kit_lint.py checks)."""
    b = Builder()
    yin = HALF_W - SKIN
    # the floor: planks along the car, studded rubber across each doorway (pieces side by side, never overlapping)
    cuts = sorted([-L / 2 + SKIN] + [dx + e * 0.62 for dx in DOOR_X for e in (-1, 1)] + [L / 2 - SKIN])
    for x0, x1 in zip(cuts, cuts[1:]):
        door = any(abs((x0 + x1) / 2 - dx) < 0.1 for dx in DOOR_X)
        b.box("tx_studs" if door else "tx_floorwood", ((x0 + x1) / 2, 0, FLOOR_Z - 0.05), (x1 - x0, 2 * yin, 0.1))
    for dx in DOOR_X:
        for side in (-1, 1):
            # an aluminium sill plate in the doorway, its top 6 mm proud of the floor
            b.box("RL_chrome", (dx, side * (HALF_W - 0.06), FLOOR_Z - 0.004), (DOOR_W - 0.006, 0.14, 0.02))
    seats = []
    runs = [(-DOOR_X[1] + DOOR_W / 2 + 0.15, DOOR_X[1] - DOOR_W / 2 - 0.15), (DOOR_X[1] + DOOR_W / 2 + 0.15, L / 2 - 1.55),
            (-L / 2 + 0.25, DOOR_X[0] - DOOR_W / 2 - 0.15)]
    wy = wall_y(SEAT_TOP)
    for (x0, x1) in runs:
        for side in (-1, 1):
            s_ = side
            yf = wy - SEAT_D            # the cushion's front (in |y|)
            # the cushion: a slab with a rounded front edge, the pile on top and front
            prof = [(wy - 0.07, SEAT_TOP - 0.15), (yf + 0.02, SEAT_TOP - 0.15)] + arc(yf + 0.06, SEAT_TOP - 0.06, 0.06, 270, 180, 3)[1:] \
                + arc(yf + 0.07, SEAT_TOP - 0.07, 0.07, 180, 90, 4)[1:] + [(wy - 0.07, SEAT_TOP)]
            extrude_x(b, "tx_moquette", [(s_ * y, z) for (y, z) in (prof if s_ < 0 else reversed(prof))], x0, x1)
            # the backrest, leaning back 9 degrees, its top rolled
            t = math.radians(9)
            bz0, bz1 = SEAT_TOP + 0.03, SEAT_TOP + 0.50
            yb0 = wy - 0.12
            back = [(yb0, bz0), (wy - 0.015, bz0), (wy - 0.015 - (bz1 - bz0) * 0.02, bz1 - 0.03)] \
                + arc(yb0 + (bz1 - bz0) * math.tan(t) + 0.035, bz1 - 0.035, 0.035, 60, 180, 3) \
                + [(yb0 + 0.02, bz0 + 0.05)]
            extrude_x(b, "tx_moquette", [(s_ * y, z) for (y, z) in (back if s_ < 0 else reversed(back))], x0 + 0.025, x1 - 0.025)
            # the heater grille under it (louvred steel), a little back from the cushion's front
            b.box("tx_grille", ((x0 + x1) / 2, s_ * (yf + 0.06), (FLOOR_Z + SEAT_TOP - 0.15) / 2), (x1 - x0 - 0.04, 0.025, SEAT_TOP - 0.15 - FLOOR_Z - 0.004))
            b.box("RL_under", ((x0 + x1) / 2, s_ * ((yf + 0.075 + wy) / 2), FLOOR_Z + 0.006), (x1 - x0 - 0.04, wy - yf - 0.09, 0.01))
            # the end screens: varnished boards with a bent chrome rail
            for xe, e in ((x0, -1), (x1, 1)):
                b.box("tx_varnish", (xe + e * 0.017, s_ * (wy - 0.30), FLOOR_Z + 0.50), (0.025, 0.56, 0.96))
                yr = s_ * (wy - 0.66)
                b.cyl("RL_chrome", (xe + e * 0.04, yr, FLOOR_Z - 0.02), (xe + e * 0.04, yr, FLOOR_Z + 0.95), 0.016, n=8)
                b.cyl("RL_chrome", (xe + e * 0.04, yr, FLOOR_Z + 0.95), (xe + e * 0.04, s_ * (wy - 0.026), FLOOR_Z + 0.95), 0.016, n=8)
                b.cyl("RL_chrome", (xe + e * 0.04, yr, FLOOR_Z + 0.95), (xe + e * 0.04, yr, EAVE_Z - 0.2), 0.016, n=8)
            k = int((x1 - x0) / 0.46)
            for i in range(k):
                sx = x0 + (i + 0.5) * (x1 - x0) / k
                seats.append({"x": round(sx, 3), "y": round(s_ * yf, 3), "z": FLOOR_Z, "face_y": -s_})
    # the windows from inside: varnished sills and frames, roller blinds (some pulled part way)
    for side in (-1, 1):
        for i, wx in enumerate(WINDOWS):
            ys = wall_y(WIN_Z[0])
            b.box("tx_varnish", (wx, side * (ys - 0.03), WIN_Z[0] - 0.03), (WIN_W + 0.13, 0.08, 0.03))
            for e in (-1, 1):
                b.box("tx_varnish", (wx + e * (WIN_W / 2 + 0.035), side * (wall_y(2.5) - 0.006), (WIN_Z[0] + WIN_Z[1]) / 2), (0.05, 0.022, WIN_Z[1] - WIN_Z[0] + 0.02))
            b.box("tx_varnish", (wx, side * (wall_y(WIN_Z[1]) - 0.008), WIN_Z[1] + 0.03), (WIN_W + 0.12, 0.026, 0.06))
            # the blind's roll under the top frame, and the blind itself down to a varnished pull bar
            yr = side * (wall_y(WIN_Z[1]) - 0.045)
            b.cyl("tx_blind", (wx - WIN_W / 2 + 0.03, yr, WIN_Z[1] - 0.03), (wx + WIN_W / 2 - 0.03, yr, WIN_Z[1] - 0.03), 0.024, n=10)
            drop = [0.0, 0.35, 0.0, 0.6, 0.15, 0.0, 0.5, 0.0][(i + (3 if side > 0 else 0)) % 8]
            if drop > 0:
                zt, zb = WIN_Z[1] - 0.05, WIN_Z[1] - 0.05 - drop * (WIN_Z[1] - WIN_Z[0] - 0.08)
                yb = side * (wall_y(WIN_Z[1]) + 0.012)
                b.box("tx_blind", (wx, yb, (zt + zb) / 2), (WIN_W - 0.08, 0.004, zt - zb))
                b.cyl("tx_varnish", (wx - WIN_W / 2 + 0.05, yb, zb), (wx + WIN_W / 2 - 0.05, yb, zb), 0.012, n=8)
    # the luggage rack: chrome rods on brackets, over the benches only
    racks = [(-L / 2 + 0.35, DOOR_X[0] - 0.72), (DOOR_X[0] + 0.72, DOOR_X[1] - 0.72), (DOOR_X[1] + 0.72, L / 2 - 1.6)]
    for side in (-1, 1):
        for (x0, x1) in racks:
            zr = 3.05
            yw = wall_y(zr)
            for k in range(6):
                yy = side * (yw - 0.04 - k * 0.055)
                b.cyl("RL_chrome", (x0, yy, zr), (x1, yy, zr), 0.007, n=6)
            b.cyl("RL_chrome", (x0, side * (yw - 0.34), zr + 0.03), (x1, side * (yw - 0.34), zr + 0.03), 0.013, n=8)
            nb = max(2, int((x1 - x0) / 1.1) + 1)
            for k in range(nb):
                bx = x0 + 0.05 + k * (x1 - x0 - 0.1) / (nb - 1)
                b.box("RL_chrome", (bx, side * (yw - 0.17), zr - 0.012), (0.02, 0.36, 0.014))
                b.cyl("RL_chrome", (bx, side * (yw - 0.005), zr - 0.22), (bx, side * (yw - 0.33), zr - 0.01), 0.008, n=6)
    # straps on two rails along the aisle
    for side in (-1, 1):
        b.cyl("RL_chrome", (-L / 2 + 1.0, side * 0.55, EAVE_Z - 0.05), (L / 2 - 1.6, side * 0.55, EAVE_Z - 0.05), 0.014, n=6)
        for i in range(24):
            rx = -L / 2 + 1.3 + i * 0.55
            if any(abs(rx - dx) < 0.3 for dx in DOOR_X):
                continue
            b.box("RL_white", (rx, side * 0.55, EAVE_Z - 0.18), (0.022, 0.006, 0.25))
            b.torus_ring("RL_white", (rx, side * 0.55, EAVE_Z - 0.36), (1, 0, 0), 0.06, 0.01, n=10, m=4)
    # lamps: two rows of globes on chrome collars
    lamps = []
    zc = ROOF_Z - SKIN
    for i in range(6):
        lx = -L / 2 + 1.6 + i * 2.5
        for yy in (-0.62, 0.62):
            b.cyl("RL_chrome", (lx, yy, zc - 0.11), (lx, yy, zc - 0.17), 0.075, n=12)
            b.cyl("RL_lamp", (lx, yy, zc - 0.17), (lx, yy, zc - 0.27), 0.10, n=14, r2=0.07)
            lamps.append([round(lx, 3), yy, ROOF_Z - 0.3])
    # the fans: a motor, three blades and a round cage of rings
    for fx in (-4.0, 0.0, 4.0):
        b.cyl("RL_white", (fx, 0, zc - 0.03), (fx, 0, zc - 0.16), 0.03, n=8)
        b.cyl("RL_white", (fx, 0, zc - 0.16), (fx, 0, zc - 0.26), 0.075, n=12)
        for k in range(3):
            a = 2 * math.pi * k / 3
            b.box("tx_ceiling", (fx + 0.15 * math.cos(a), 0.15 * math.sin(a), zc - 0.24), (0.24, 0.07, 0.008), rot_z=a)
        for rz, rr in ((zc - 0.20, 0.27), (zc - 0.27, 0.27), (zc - 0.31, 0.15)):
            b.torus_ring("RL_chrome", (fx, 0, rz), (0, 0, 1), rr, 0.005, n=20, m=4)
        for k in range(8):
            a = 2 * math.pi * k / 8
            b.cyl("RL_chrome", (fx + 0.27 * math.cos(a), 0.27 * math.sin(a), zc - 0.20), (fx + 0.27 * math.cos(a), 0.27 * math.sin(a), zc - 0.27), 0.004, n=4)
            b.cyl("RL_chrome", (fx + 0.27 * math.cos(a), 0.27 * math.sin(a), zc - 0.27), (fx + 0.15 * math.cos(a), 0.15 * math.sin(a), zc - 0.31), 0.004, n=4)
    # stanchions by the doors
    for dx in DOOR_X:
        for e in (-1, 1):
            for side in (-1, 1):
                b.cyl("RL_chrome", (dx + e * 0.75, side * (yin - 0.45), FLOOR_Z - 0.02), (dx + e * 0.75, side * (yin - 0.45), EAVE_Z - 0.2), 0.016, n=8)
    # the cab: a partition with a door window, the driver's desk and seat on the right (y-)
    xp = L / 2 - 1.45
    b.box("tx_decora", (xp, -0.84, FLOOR_Z + 1.195), (0.04, 0.98, 2.41))
    b.box("tx_decora", (xp, 0.89, FLOOR_Z + 1.195), (0.04, 0.88, 2.41))
    b.box("tx_decora", (xp, 0.0, FLOOR_Z + 2.25), (0.04, 0.8, 0.3))
    b.box("tx_varnish", (xp - 0.025, 0.0, FLOOR_Z + 2.08), (0.01, 0.82, 0.03))
    b.box("RL_green", (L / 2 - 0.45, -0.6, FLOOR_Z + 0.495), (0.6, 1.1, 1.01))
    b.box("RL_black", (L / 2 - 0.4, -0.6, FLOOR_Z + 1.02), (0.45, 1.0, 0.06))
    b.box("tx_moquette", (L / 2 - 1.05, -0.6, FLOOR_Z + 0.5), (0.45, 0.45, 0.1))
    b.cyl("RL_chrome", (L / 2 - 1.05, -0.6, FLOOR_Z - 0.02), (L / 2 - 1.05, -0.6, FLOOR_Z + 0.45), 0.03, n=8)

    # --- print ----------------------------------------------------------------------------------------------
    P = PRINTS
    X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)
    # hanging posters over the aisle, two ads back to back, a chrome clip bar top and bottom, two wires up
    for hx, a1, a2 in HANG:
        w, h = 0.52, 0.52 * 720 / 1008
        zt = zc - 0.24
        b.card(P["hang_" + a1], (hx, 0, zt - h / 2), (0, -1, 0), Z, w, h, 0.004, back=P["hang_" + a2])
        for zz in (zt + 0.006, zt - h - 0.006):
            b.box("RL_chrome", (hx, 0, zz), (0.014, w + 0.02, 0.014))
        for yy in (-0.2, 0.2):
            b.cyl("RL_chrome", (hx, yy, zt + 0.012), (hx, yy, zc + 0.01), 0.0025, n=4)
    # framed cards over the racks, along both walls, one over each window
    k = 0
    for side in (-1, 1):
        for wx in WINDOWS:
            name = FRAME_ORDER[k % len(FRAME_ORDER)]
            k += 1
            w, h = 0.50, 0.50 * 362 / 1008
            zm = 3.075 + h / 2 + 0.02
            yw = wall_y(zm)
            # the frame stands 8 mm off the wall (the wall leans a little: never touching it), the card 3 mm proud
            b.box("RL_chrome", (wx, side * (yw - 0.008 - 0.006), zm), (w + 0.03, 0.012, h + 0.03))
            b.card(P["frame_" + name], (wx, side * (yw - 0.0225), zm), (side, 0, 0), Z, w, h, 0.003)
    # the line's map over the gangway door and over the cab's door; the plates and notices on the end wall
    xe = -L / 2 + SKIN
    b.box("RL_chrome", (xe + 0.012, 0, 3.32), (0.008, 0.95, 0.25))
    b.card(P["route"], (xe + 0.0185, 0, 3.32), (0, 1, 0), Z, 0.92, 0.92 * 256 / 1008, 0.003)
    b.box("RL_chrome", (xp - 0.032, 0, 3.40), (0.008, 0.95, 0.25))
    b.card(P["route"], (xp - 0.0385, 0, 3.40), (0, -1, 0), Z, 0.92, 0.92 * 256 / 1008, 0.003)
    b.card(P["plate_number"], (xe + 0.012, 0.66, 2.95), (0, 1, 0), Z, 0.34, 0.34 * 160 / 512, 0.006, edge="RL_green")
    b.card(P["plate_maker"], (xe + 0.012, -0.66, 2.95), (0, 1, 0), Z, 0.30, 0.30 * 160 / 512, 0.006, edge="RL_wood")
    b.card(P["sticker_no_smoking"], (xe + 0.004, 0.85, 2.55), (0, 1, 0), Z, 0.20, 0.20, 0.0015, edge="RL_white")
    b.card(P["sticker_no_smoking"], (xp - 0.0215, 0.9, 2.55), (0, -1, 0), Z, 0.20, 0.20, 0.0015, edge="RL_white")
    for dx in DOOR_X:
        for side in (-1, 1):
            ex = dx + (0.85 if dx > 0 else -0.85)
            b.card(P["sticker_emergency"], (ex, side * (wall_y(2.45) - 0.006), 2.45), (side, 0, 0), Z, 0.26, 0.13, 0.0015, edge="RL_white")
    # priority seats: the gangway end's benches, a sticker low on the end windows' glass and over the bench
    for side in (-1, 1):
        yg = side * (HALF_W - SKIN / 2 - 0.004)
        b.card(P["sticker_priority"], (-6.35, yg, 2.14), (side, 0, 0), Z, 0.40, 0.40 * 256 / 1008, 0.0015, edge="RL_white")
    o = b.to_object("SM_RL_CarInterior")
    return o, seats, lamps


def door_leaf():
    """One sliding leaf (half the doorway: two leaves part in the middle), its outside in the body's paint."""
    b = Builder()
    w, h = DOOR_W / 2 + 0.02, DOOR_TOP - FLOOR_Z + 0.03      # shut, the two meet edge to edge (game: LeafX 0.31)
    # the leaf: frame along X (its width), Z up; the outside +Y
    b.box("RL_cream", (0, 0, h / 2), (w, 0.04, h), mats={"y-": "tx_decora"})
    # the paint bands, 0.5 mm proud of the outside, short of the ends (and of the rubber)
    b.box("RL_green", (-0.012, 0.0225, 0.35), (w - 0.028, 0.004, 0.7))
    b.box("RL_orange", (-0.012, 0.0225, 0.73), (w - 0.028, 0.004, 0.06))
    # its window and the rubber of the meeting edge, both standing proud of the leaf on both faces
    b.box("RL_glass", (0, 0.0, 1.5), (w - 0.18, 0.047, 0.85))
    b.box("RL_black", (w / 2 - 0.012, 0, h / 2), (0.02, 0.044, h - 0.01))
    # inside (y-): the fingers sticker by the meeting edge, "don't lean" on the glass
    b.card(PRINTS["sticker_fingers"], (w / 2 - 0.14, -0.0215, 0.92), (1, 0, 0), (0, 0, 1), 0.15, 0.15, 0.002, edge="RL_white")
    b.card(PRINTS["sticker_lean"], (0, -0.0265, 1.17), (1, 0, 0), (0, 0, 1), 0.40, 0.40 * 160 / 1008, 0.0015, edge="RL_white")
    return b.to_object("SM_RL_DoorLeaf")


def gangway():
    b = Builder()
    for i in range(7):
        b.box("RL_black", (0, 0, FLOOR_Z + 1.0), (0.11, 1.15 + (0.06 if i % 2 else 0), 2.15 + (0.06 if i % 2 else 0)))
        b.bm.verts.ensure_lookup_table()
        for v in b.bm.verts[-8:]:
            v.co.x += -0.42 + i * 0.14
    b.box("RL_floor", (0, 0, FLOOR_Z - 0.03), (1.0, 0.9, 0.06))
    return b.to_object("SM_RL_Gangway")


def bogie():
    b = Builder()
    # the frame and the bolster, two wheelsets, springs, brake shoes
    for yy in (-0.84, 0.84):
        b.box("RL_under", (0, yy, 0.55), (2.6, 0.16, 0.24))
        for xx in (-1.05, 1.05):
            b.cyl("RL_under", (xx, yy, 0.43), (xx, yy * 1.06, 0.43), 0.25, n=16)
            b.cyl("RL_black", (xx, yy * 1.06, 0.43), (xx, yy * 1.10, 0.43), 0.12, n=10)
            b.cyl("RL_steel", (xx - 0.2, yy, 0.62), (xx - 0.2, yy, 0.82), 0.07, n=8)
    for xx in (-1.05, 1.05):
        b.cyl("RL_steel", (xx, -0.72, 0.43), (xx, 0.72, 0.43), 0.43, n=24, caps=True)    # the wheels' rims as one drum
        b.cyl("RL_under", (xx, -0.95, 0.43), (xx, 0.95, 0.43), 0.08, n=10)
    b.box("RL_under", (0, 0, 0.68), (0.5, 2.0, 0.2))
    return b.to_object("SM_RL_Bogie")


def pantograph():
    """A diamond pantograph on its frame; origin on the roof, head 1.35 m up (the wire at 5.3 m over the rail)."""
    b = Builder()
    b.box("RL_roof", (0, 0, 0.06), (1.3, 1.4, 0.06))
    for yy in (-0.55, 0.55):
        b.cyl("RL_insulator", (0, yy, 0.0), (0, yy, 0.12), 0.06, n=10)
    up = 1.35
    for yy in (-0.45, 0.45):
        b.cyl("RL_steel", (-0.6, yy, 0.12), (0, yy, up / 2 + 0.06), 0.025, n=6)
        b.cyl("RL_steel", (0.6, yy, 0.12), (0, yy, up / 2 + 0.06), 0.025, n=6)
        b.cyl("RL_steel", (0, yy, up / 2 + 0.06), (-0.45, yy, up - 0.06), 0.02, n=6)
        b.cyl("RL_steel", (0, yy, up / 2 + 0.06), (0.45, yy, up - 0.06), 0.02, n=6)
    b.cyl("RL_steel", (-0.45, -0.5, up - 0.06), (-0.45, 0.5, up - 0.06), 0.02, n=6)
    b.cyl("RL_steel", (0.45, -0.5, up - 0.06), (0.45, 0.5, up - 0.06), 0.02, n=6)
    b.box("RL_steel", (0, 0, up - 0.02), (0.06, 1.9, 0.04))
    b.box("RL_steel", (0.12, 0, up - 0.02), (0.06, 1.9, 0.04))
    return b.to_object("SM_RL_Pantograph")


# --- the crossing, the halt, the line ------------------------------------------------------------------------------
def crossing_post():
    """The crossing's warning post: striped pole, the yellow crossbuck, a black board with two red lamps under hoods,
    the bell on top; faces +X (the road). Origin at the ground."""
    b = Builder()
    # the pole, black and yellow bands near the ground
    for i in range(8):
        z0 = i * 0.25
        b.cyl("RL_yellow" if i % 2 == 0 else "RL_black", (0, 0, z0), (0, 0, z0 + 0.25), 0.07, n=12, caps=False)
    b.cyl("RL_white", (0, 0, 2.0), (0, 0, 4.0), 0.065, n=12)
    b.box("RL_concrete", (0, 0, -0.1), (0.5, 0.5, 0.3))
    # the crossbuck (an X of two yellow boards with black edges)
    for a in (math.radians(35), math.radians(-35)):
        R = Matrix.Rotation(a, 3, "X")
        for (mat, s, dx) in (("RL_black", (0.03, 1.32, 0.24), 0.0), ("RL_yellow", (0.035, 1.25, 0.17), 0.006)):
            hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
            cs = [Vector((x, y, z)) for x in (-hx, hx) for y in (-hy, hy) for z in (-hz, hz)]
            pts = [R @ c + Vector((0.09 + dx, 0, 3.55)) for c in cs]
            idx = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
            for f in idx:
                b.face([pts[k] for k in f], mat)
    # the board with two lamps (one lamp each way: front +X and back -X)
    b.box("RL_black", (0, 0, 2.95), (0.04, 1.05, 0.45))
    lamps = []
    for yy in (-0.33, 0.33):
        for fx in (1, -1):
            b.cyl("RL_black", (fx * 0.02, yy, 2.95), (fx * 0.2, yy, 3.02), 0.15, n=16, caps=False)     # the hood
            b.disc("RL_xlamp", (fx * 0.035, yy, 2.95), (fx, 0, 0), 0.13, n=18)
            lamps.append([round(fx * 0.04, 3), yy, 2.95, fx])
        b.cyl("RL_black", (0, yy, 2.95), (0, yy * 0.2, 2.95), 0.02, n=6)
    # the direction arrows' box and the bell (a small dome)
    b.box("RL_black", (0, 0, 2.55), (0.06, 0.55, 0.2))
    b.cyl("RL_black", (0, 0, 4.0), (0, 0, 4.12), 0.16, n=14, r2=0.06)
    o = b.to_object("SM_RL_XingPost")
    # the lamps' glowing faces (the game blinks them): a mesh of their own
    g = Builder()
    for (x, y, z, fx) in lamps:
        g.disc("RL_xglow", (x + fx * 0.004, y, z), (fx, 0, 0), 0.125, n=18)
    gl = g.to_object("SM_RL_XingGlow")
    return [o, gl], lamps


def crossing_gate():
    """The barrier's mechanism box (origin on the ground) and its arm (origin on the hinge, the arm along +Y, black and
    yellow stripes, a red reflector)."""
    b = Builder()
    b.box("RL_yellow", (0, 0, 0.55), (0.35, 0.35, 1.1))
    b.box("RL_black", (0, 0, 1.12), (0.4, 0.4, 0.05))
    b.box("RL_concrete", (0, 0, -0.1), (0.6, 0.6, 0.3))
    b.cyl("RL_black", (0, 0, 0.85), (0.25, 0, 0.85), 0.08, n=12)
    box = b.to_object("SM_RL_XingGateBox")
    a = Builder()
    n = 10
    ln = 4.6
    for i in range(n):
        y0 = 0.25 + i * (ln / n)
        a.box("RL_yellow" if i % 2 == 0 else "RL_black", (0, y0 + ln / n / 2, 0), (0.06, ln / n, 0.09))
    a.box("RL_black", (0, 0.05, 0), (0.12, 0.5, 0.18))   # the counterweight's side
    a.box("RL_under", (0, -0.25, 0), (0.116, 0.3, 0.25))
    a.cyl("RL_tail", (0.035, ln - 0.3, 0), (0.05, ln - 0.3, 0), 0.04, n=10)
    arm = a.to_object("SM_RL_XingArm")
    return [box, arm]


def shelter():
    """The halt's little wooden shelter: back wall, two side walls, a tin roof leaning back, a bench. Origin on the deck,
    open toward +Y (the track), 3.2 m long (X), 1.5 m deep."""
    b = Builder()
    w, d, h = 3.2, 1.5, 2.35
    for x in (-w / 2, w / 2):
        b.box("RL_wood", (x, -d / 2, h / 2), (0.08, d, h))
    b.box("RL_wood", (0, -d + 0.04, h / 2), (w - 0.08, 0.08, h))
    for i in range(12):
        b.box("RL_wood_dark", (-w / 2 + 0.13 + i * (w - 0.2) / 11, -d + 0.09, h / 2), (0.02, 0.01, h - 0.1))
    roof = Matrix.Rotation(math.radians(-8), 3, "X")
    for i in range(9):
        cx = -w / 2 - 0.15 + i * (w + 0.3) / 8.5
        b.box("RL_tin", (cx, -d / 2 + 0.1, h + 0.12), (0.4, d + 0.5, 0.03))
    b.box("RL_wood_dark", (0, -d / 2 + 0.1, h + 0.06), (w + 0.2, d + 0.3, 0.08))
    # the bench
    b.box("RL_wood", (0, -d + 0.45, 0.43), (w - 0.4, 0.42, 0.05))
    for x in (-w / 2 + 0.4, 0, w / 2 - 0.4):
        b.box("RL_wood_dark", (x, -d + 0.45, 0.2), (0.06, 0.38, 0.4))
    b.box("RL_wood", (0, -d + 0.2, 0.75), (w - 0.4, 0.04, 0.3))
    return b.to_object("SM_RL_Shelter")


def nameboard():
    """A Japanese halt's nameboard: a white board (2.2 x 0.75 m) on two posts; the names go on it in the game (a
    widget per language). Origin on the deck, the board's face toward +Y."""
    b = Builder()
    for x in (-0.95, 0.95):
        b.cyl("RL_white", (x, 0, 0), (x, 0, 2.1), 0.045, n=10)
    b.box("RL_white", (0, 0, 1.75), (2.3, 0.05, 0.8))
    b.box("RL_black", (0, 0.027, 1.4), (2.2, 0.002, 0.012))
    return b.to_object("SM_RL_NameBoard")


def mast():
    """A catenary mast: a tapering concrete pole (origin at the rail head's level beside the track, 3.5 m sunk into
    the bank), a steel cantilever reaching over the track (-Y) with insulators and the contact wire's holder at
    5.3 m over the rail."""
    b = Builder()
    b.cyl("RL_concrete", (0, 0, -3.5), (0, 0, 7.2), 0.17, n=10, r2=0.11)
    b.cyl("RL_steel", (0, 0, 6.6), (0, -3.2, 6.25), 0.035, n=6)
    b.cyl("RL_steel", (0, 0, 5.75), (0, -3.2, 5.75), 0.04, n=6)
    b.cyl("RL_steel", (0, -0.25, 6.55), (0, -0.25, 5.75), 0.025, n=6)
    for yy in (-0.25, -3.15):
        b.cyl("RL_insulator", (0, yy, 5.85), (0, yy, 5.6), 0.05, n=8)
    b.cyl("RL_steel", (0, -2.9, 5.75), (0, -2.9, 5.32), 0.012, n=6)
    return b.to_object("SM_RL_Mast")


def sleeper():
    b = Builder()
    b.box("RL_wood_dark", (0, 0, -0.07), (0.22, 2.1, 0.14))
    return b.to_object("SM_RL_Sleeper")


def island():
    """The island's ground (map metres -> the island's own frame: origin at its centre at sea level, X west, Y north
    in map terms; the game places it), from tools/railway.py's island_height, 2 m grid; sand, wet sand, turf, rock by
    height and slope."""
    import railway as RW
    src = RW.load()
    isl = src["island"]
    sea = -43.0
    cx, cy = isl["x"], isl["y"]
    rx, ry = isl["rx"] * 1.3, isl["ry"] * 1.3
    step = 1.25
    nx, ny = int(2 * rx / step) + 1, int(2 * ry / step) + 1
    H = {}
    for j in range(ny):
        for i in range(nx):
            x, y = cx - rx + i * step, cy - ry + j * step
            h = RW.island_height(src, x, y)
            H[(i, j)] = (h if h is not None else sea - 3.5) - sea
    bm = bmesh.new()
    V = {}
    for (i, j), h in H.items():
        # map (x west, y north) -> Blender (x, -y) as the rest of the project (UE gets x, -(-y)) = map y
        V[(i, j)] = bm.verts.new(((-rx + i * step), -(-ry + j * step), h))
    mats = ["RL_sand", "RL_wetsand", "RL_grass", "RL_rock"]
    for j in range(ny - 1):
        for i in range(nx - 1):
            hs = [H[(i, j)], H[(i + 1, j)], H[(i + 1, j + 1)], H[(i, j + 1)]]
            if max(hs) < -3.0:
                continue
            f = bm.faces.new([V[(i, j)], V[(i, j + 1)], V[(i + 1, j + 1)], V[(i + 1, j)]])
            hm = sum(hs) / 4
            slope = (max(hs) - min(hs)) / step
            # the seed's rocky west knoll and steep bits are rock, the crown turf, the rim wet and dry sand
            k = 3 if slope > 0.55 else (2 if hm > 1.9 else (0 if hm > 0.2 else 1))
            f.material_index = k
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.z < 0:          # an open sheet: make sure it faces the sky
            f.normal_flip()
    # UVs in metres over the plan (the game tiles its textures by the metre)
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        for lp in f.loops:
            lp[uv].uv = (lp.vert.co.x, lp.vert.co.y)
    me = bpy.data.meshes.new("SM_RL_Island")
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(material(m))
    for p in me.polygons:
        p.use_smooth = True
    o = bpy.data.objects.new("SM_RL_Island", me)
    bpy.context.scene.collection.objects.link(o)
    return o


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def preview(objs, out_dir):
    """Quick EEVEE renders of the car from outside and inside (for docs and checking)."""
    os.makedirs(out_dir, exist_ok=True)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE_NEXT"
    scn.render.resolution_x, scn.render.resolution_y = 1280, 720
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.7, 0.8, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
    scn.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
    scn.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    shown = {o.name for o in objs}
    for o in bpy.data.objects:
        if o.type == "MESH":
            o.hide_render = o.name not in shown
    def shot(name, loc, look, lens=30):
        cam.location = loc
        d = Vector(look) - Vector(loc)
        cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        scn.render.filepath = os.path.join(out_dir, name + ".png")
        bpy.ops.render.render(write_still=True)
    shot("car_front", (14.0, -7.0, 2.6), (6.0, 0.0, 2.0))
    shot("car_side", (0.0, -14.0, 2.4), (0.0, 0.0, 2.2), lens=24)
    shot("car_inside", (-6.5, 0.3, FLOOR_Z + 1.55), (4.0, -0.2, FLOOR_Z + 0.9), lens=18)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    prev = argv[argv.index("--preview") + 1] if "--preview" in argv else None
    clear()
    car = car_body()
    details = car_details()
    inside, seats, lamps = car_interior()
    leaf = door_leaf()
    gang = gangway()
    bog = bogie()
    pan = pantograph()
    xpost, xlamps = crossing_post()
    xgate = crossing_gate()
    shel = shelter()
    nb = nameboard()
    ms = mast()
    sl = sleeper()
    isl = island()
    # park the pieces apart (each keeps its own origin; the export uses their local frames)
    objs = [car, *details, inside, leaf, gang, bog, pan, *xpost, *xgate, shel, nb, ms, sl, isl]
    if prev:
        # a preview train: the bogies and the pantograph in place
        b1 = bog.copy(); b1.location = (5.6, 0, 0); scn = bpy.context.scene; scn.collection.objects.link(b1)
        b2 = bog.copy(); b2.location = (-5.6, 0, 0); scn.collection.objects.link(b2)
        p1 = pan.copy(); p1.location = (2.8, 0, ROOF_Z); scn.collection.objects.link(p1)
        lv = []
        for dx in DOOR_X:
            for side in (-1, 1):
                for e in (-1, 1):
                    l = leaf.copy(); l.location = (dx + e * DOOR_W / 4, side * (HALF_W - 0.03), FLOOR_Z)
                    l.rotation_euler = (0, 0, 0 if side > 0 else math.pi)
                    scn.collection.objects.link(l); lv.append(l)
        preview([car, *details, inside, b1, b2, p1] + lv[:4], prev)
        for o in [b1, b2, p1] + lv:
            bpy.data.objects.remove(o, do_unlink=True)
    # glitch checks (blender/railway/kit_lint.py): the car as the game assembles it (body, glass, trim, inside, the
    # door leaves shut in their doorways), then every other piece on its own
    sys.path.insert(0, HERE)
    import kit_lint
    scn = bpy.context.scene
    shut = []
    for dx in DOOR_X:
        for side in (-1, 1):
            for e in (-1, 1):
                l = leaf.copy(); l.location = (dx + e * 0.31, side * (HALF_W - 0.03), FLOOR_Z)
                l.rotation_euler = (0, 0, 0 if side > 0 else math.pi)
                scn.collection.objects.link(l); shut.append(l)
    bpy.context.view_layer.update()
    problems = kit_lint.lint([car, *details, inside] + shut, "car")
    for o in shut:
        bpy.data.objects.remove(o, do_unlink=True)
    for o in [gang, bog, pan, *xpost, *xgate, shel, nb, ms, sl]:
        problems += kit_lint.lint([o], o.name)
    if "--strict" in argv and problems:
        raise SystemExit("RAILKIT: %d glitch findings (KITLINT lines above)" % len(problems))
    os.makedirs(os.path.dirname(OUT_GLB), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.location = (0, 0, 0)
        o.select_set(True)
    # no images in the file: the game puts its own textures on the tx_* and ad_* slots (the preview's are only for looking)
    bpy.ops.export_scene.gltf(filepath=OUT_GLB, use_selection=True, export_format="GLB", export_yup=True, export_apply=True,
                              export_image_format="NONE")
    kit = {
        "_doc": "Made by blender/railway/rail_kit.py; metres, the car's frame (X along it, +X its cab end, Y left, Z up from the rail head).",
        "car": {"length": L, "half_width": HALF_W, "floor": FLOOR_Z, "roof": ROOF_Z, "sill": SILL_Z,
                "doors": [{"x": dx, "width": DOOR_W, "top": DOOR_TOP} for dx in DOOR_X],
                "bogies": [-5.6, 5.6], "pantograph": [2.8, 0.0, ROOF_Z], "pantograph_up": 1.35,
                "headlamps": [[L / 2 + 0.1, -0.95, 1.55], [L / 2 + 0.1, 0.95, 1.55]],
                "taillamps": [[L / 2 + 0.06, -0.95, 1.86], [L / 2 + 0.06, 0.95, 1.86]],
                "sign": [L / 2 + 0.06, 0.0, 3.22],
                "lamps": lamps, "seats": seats, "seat_height": 0.42},
        "crossing": {"lamps": xlamps, "arm_hinge": [0.0, 0.25, 0.85], "arm_length": 4.85},
        "pieces": [o.name for o in objs],
    }
    with open(OUT_JSON, "w") as f:
        json.dump(kit, f, indent=1)
    print("RAILKIT: %d pieces, %d seats, %d lamps -> %s" % (len(objs), len(seats), len(lamps), OUT_GLB))
    for o in objs:
        print("RAILKIT  %-20s %6d tris" % (o.name, sum(len(p.vertices) - 2 for p in o.data.polygons)))


main()
