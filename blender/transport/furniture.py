"""The furniture of the stations' halls and lifts (Dima, 2026-10-26: the interiors are bare), our own clean models in the
1960s space-age language of the vestibules: SM_TR_Furn_<Item>.glb in design/transport/game, metre UVs.

  Blender -b -P blender/transport/furniture.py

Items (origin on the floor at the item's middle, its back to -X: it stands with its back to a wall, facing +X):
  Bench       a long bench, amber leatherette on a chrome frame
  EggChair    an egg chair: an ivory shell, an amber cushion, on a swivel foot
  Table       a tulip table, terrazzo top on a white stem
  Planter     a terrazzo planter with soil, for a live plant (AProceduralPlant) set on its top at PLANTER_TOP
  Lamp        a floor lamp: three white globes on a chrome stem (the game adds the light)
  Bin         a waste bin, ivory with an amber band
  Rug         a round rug (2.6 m), its pattern from the mosaic surface
  Poster      a frame for a poster (0.8 x 1.2 m), its picture on slot TR_Poster (the game sets the texture), hung by its
              middle at its origin
  Sconce      a wall lamp (glow), hung at its origin
  LiftCab*    the lift's cab (Dima: a new cab): Floor, Ceiling, Wall (a closed side: panels, a mirror, a handrail) and
              DoorFrame (a side with a door), each for the cab's 230 cm square (the game turns them to the sides)
"""
import bmesh
import bpy
import math
import os
from mathutils import Matrix, Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "design", "transport", "game")
MATS = ["TR_Tx_Enamel", "TR_Tx_Amber", "TR_Chrome", "TR_Terrazzo", "TR_Seat", "TR_Tx_Dark", "TR_Tx_Mosaic",
        "TR_Glow", "TR_Poster", "TR_Glass", "TR_Tx_Plate", "TR_Tx_Steel"]
COL = {"TR_Tx_Enamel": (0.9, 0.87, 0.8), "TR_Tx_Amber": (0.85, 0.45, 0.1), "TR_Chrome": (0.8, 0.8, 0.82),
       "TR_Terrazzo": (0.62, 0.58, 0.52), "TR_Seat": (0.75, 0.38, 0.12), "TR_Tx_Dark": (0.2, 0.18, 0.16),
       "TR_Tx_Mosaic": (0.3, 0.35, 0.6), "TR_Glow": (1, 0.85, 0.6), "TR_Poster": (0.5, 0.5, 0.5),
       "TR_Glass": (0.5, 0.6, 0.65), "TR_Tx_Plate": (0.85, 0.8, 0.7), "TR_Tx_Steel": (0.6, 0.62, 0.65)}
EN, AM, CH, TZ, SEAT, DK, MO, GLOW, POSTER, GLS, PLATE, STEEL = range(len(MATS))
PLANTER_TOP = 0.5
CAB = 2.30                   # (the old square cab's inside; the shaft is 2.5 m)
CAB_H = 2.32


def box(bm, c, size, mi, rot=None):
    r = rot or Matrix.Identity(3)
    h = Vector(size) / 2
    v = [bm.verts.new(Vector(c) + r @ Vector((sx * h.x, sy * h.y, sz * h.z))) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
    for ids in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
        bm.faces.new([v[i] for i in ids]).material_index = mi


def tube(bm, a, b, r, mi, seg=10):
    a, b = Vector(a), Vector(b)
    d = b - a
    x = d.normalized()
    up = Vector((0, 0, 1)) if abs(x.z) < 0.95 else Vector((1, 0, 0))
    y = up.cross(x).normalized()
    z = x.cross(y)
    ra = [bm.verts.new(a + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * r) for i in range(seg)]
    rb = [bm.verts.new(b + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * r) for i in range(seg)]
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((ra[i], ra[j], rb[j], rb[i])).material_index = mi
    bm.faces.new(ra[::-1]).material_index = mi
    bm.faces.new(rb).material_index = mi


def lathe(bm, prof, mats, seg=24, c=(0, 0, 0), cap0=True, cap1=True):
    c = Vector(c)
    rings = [[bm.verts.new(c + Vector((r * math.cos(2 * math.pi * i / seg), r * math.sin(2 * math.pi * i / seg), z))) for i in range(seg)]
             for r, z in prof]
    for k in range(len(rings) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i])).material_index = mats[min(k, len(mats) - 1)]
    if cap0:
        bm.faces.new(rings[0][::-1]).material_index = mats[0]
    if cap1:
        bm.faces.new(rings[-1]).material_index = mats[-1]


def sphere(bm, c, r, mi, seg=16, rings=8):
    prof = [(r * math.sin(math.pi * k / rings) + 1e-3, -r * math.cos(math.pi * k / rings)) for k in range(rings + 1)]
    lathe(bm, prof, [mi], seg, c, False, False)


# --- the items -----------------------------------------------------------------------------------------------------

def bench():
    bm = bmesh.new()
    L = 1.8
    box(bm, (0, 0, 0.42), (0.48, L, 0.1), SEAT)
    box(bm, (-0.22, 0, 0.72), (0.08, L, 0.42), SEAT, Matrix.Rotation(math.radians(-12), 3, "Y"))
    for y in (-L / 2 + 0.15, L / 2 - 0.15):
        tube(bm, (0.18, y, 0.0), (0.18, y, 0.37), 0.025, CH)
        tube(bm, (-0.18, y, 0.0), (-0.2, y, 0.5), 0.025, CH)
        tube(bm, (-0.21, y, 0.5), (-0.33, y, 0.9), 0.022, CH)          # behind the back, not along its face
        tube(bm, (0.18, y, 0.035), (-0.18, y, 0.035), 0.02, CH)
    tube(bm, (0.0, -L / 2 + 0.15, 0.36), (0.0, L / 2 - 0.15, 0.36), 0.02, CH)
    return bm


def egg_chair():
    bm = bmesh.new()
    # the shell: an egg opened to the front (+X), its back high
    seg, rings = 24, 12
    verts = []
    for k in range(rings + 1):
        t = k / rings
        z = 0.35 + 0.95 * t
        r = 0.48 * math.sin(math.pi * (0.12 + 0.8 * t)) + 0.05
        row = []
        for i in range(seg + 1):
            a = math.radians(70 + (360 - 140) * i / seg)          # the front 140 degrees open
            row.append((Vector((r * math.cos(a), r * math.sin(a), z)), Vector((max(0.01, r - 0.05) * math.cos(a), max(0.01, r - 0.05) * math.sin(a), z))))
        verts.append(row)
    vo = [[bm.verts.new(p[0]) for p in row] for row in verts]
    vi = [[bm.verts.new(p[1]) for p in row] for row in verts]
    for k in range(rings):
        for i in range(seg):
            bm.faces.new((vo[k][i], vo[k][i + 1], vo[k + 1][i + 1], vo[k + 1][i])).material_index = EN
            bm.faces.new((vi[k][i + 1], vi[k][i], vi[k + 1][i], vi[k + 1][i + 1])).material_index = SEAT
    for k in range(rings):
        for i in (0, seg):
            bm.faces.new((vo[k][i], vi[k][i], vi[k + 1][i], vo[k + 1][i])).material_index = EN
    for i in range(seg):
        bm.faces.new((vo[-1][i], vi[-1][i], vi[-1][i + 1], vo[-1][i + 1])).material_index = EN
    # the seat cushion and the foot
    lathe(bm, [(0.38, 0.38), (0.4, 0.45), (0.36, 0.5)], [SEAT, SEAT], 20, (0.06, 0, 0))
    lathe(bm, [(0.32, 0.0), (0.32, 0.03), (0.05, 0.05), (0.05, 0.36)], [CH, CH, CH], 16)
    # the egg is centred on its foot; turned so its back is -X
    return bm, Matrix.Rotation(math.radians(180), 4, "Z")


def table():
    bm = bmesh.new()
    lathe(bm, [(0.28, 0.0), (0.28, 0.02), (0.06, 0.08), (0.04, 0.6), (0.12, 0.64)], [EN, EN, EN, EN], 20, cap1=False)
    lathe(bm, [(0.42, 0.66), (0.42, 0.7)], [TZ], 24)
    return bm


def planter():
    bm = bmesh.new()
    lathe(bm, [(0.24, 0.0), (0.3, 0.06), (0.36, 0.45), (0.38, 0.5), (0.34, 0.5)], [TZ, TZ, TZ, AM], 24, cap1=False)
    lathe(bm, [(0.34, 0.5), (0.34, PLANTER_TOP - 0.06)], [TZ], 24, cap0=False, cap1=False)
    lathe(bm, [(0.34, PLANTER_TOP - 0.06), (0.0, PLANTER_TOP - 0.06)], [DK], 24, cap0=False, cap1=False)
    return bm


def lamp():
    bm = bmesh.new()
    lathe(bm, [(0.2, 0.0), (0.2, 0.03), (0.03, 0.05)], [CH, CH], 16)
    tube(bm, (0, 0, 0.04), (0, 0, 1.55), 0.018, CH)
    for k, (dx, dy, z) in enumerate(((0.18, 0, 1.75), (-0.09, 0.16, 1.62), (-0.09, -0.16, 1.88))):
        tube(bm, (0, 0, z - 0.25), (dx * 0.7, dy * 0.7, z - 0.1), 0.012, CH, 6)
        sphere(bm, (dx, dy, z), 0.13, GLOW)
    return bm


def bin_():
    bm = bmesh.new()
    lathe(bm, [(0.18, 0.0), (0.2, 0.55), (0.2, 0.62)], [EN, AM], 20)
    lathe(bm, [(0.2, 0.62), (0.21, 0.66), (0.08, 0.7)], [CH, CH], 20, cap0=False)
    return bm


def rug():
    bm = bmesh.new()
    # the pile, and a braided amber rim standing 1.5 cm over it (a rim lying on the pile flickers)
    lathe(bm, [(1.2, 0.0), (1.2, 0.015)], [MO], 48)
    lathe(bm, [(1.22, 0.015), (1.22, 0.03), (1.32, 0.03), (1.32, 0.0)], [AM, AM, AM], 48, cap0=False, cap1=False)
    return bm


def poster():
    bm = bmesh.new()
    w, h = 0.8, 1.2
    # the picture (UV 0..1 over it), a chrome frame round it, a backing board
    # the picture 1.5 cm off its backing board (closer flickers)
    v = [bm.verts.new(p) for p in ((0.035, -w / 2, -h / 2), (0.035, w / 2, -h / 2), (0.035, w / 2, h / 2), (0.035, -w / 2, h / 2))]
    f = bm.faces.new(v)
    f.material_index = POSTER
    box(bm, (0.01, 0, 0), (0.02, w + 0.04, h + 0.04), DK)
    for (a, b) in (((-w / 2 - 0.02, -h / 2 - 0.02), (w / 2 + 0.02, -h / 2 - 0.02)), ((w / 2 + 0.02, -h / 2 - 0.02), (w / 2 + 0.02, h / 2 + 0.02)),
                   ((w / 2 + 0.02, h / 2 + 0.02), (-w / 2 - 0.02, h / 2 + 0.02)), ((-w / 2 - 0.02, h / 2 + 0.02), (-w / 2 - 0.02, -h / 2 - 0.02))):
        tube(bm, (0.04, a[0], a[1]), (0.04, b[0], b[1]), 0.015, CH, 6)
    return bm


def sconce():
    bm = bmesh.new()
    tube(bm, (0.0, 0, -0.1), (0.02, 0, 0.1), 0.06, CH, 12)          # the wall plate
    tube(bm, (0.0, 0, 0), (0.12, 0, 0.05), 0.012, CH, 6)
    sphere(bm, (0.18, 0, 0.08), 0.1, GLOW)
    return bm


# --- the lift's cab: a round capsule (Dima: the square box looked grim). Inside radius CAB_R; the game turns the wall
# pieces to its four sides: a quarter of the round wall each (CabWall closed, CabDoor with the door's opening).

CAB_R = 1.12
SEG_Q = 12            # segments per quarter


def arc_wall(bm, a0, a1, z0, z1, r_in, r_out, mi_out, mi_in, mi_top=None, seg=SEG_Q):
    """A curved wall from azimuth a0 to a1 (radians), z0..z1, between r_in and r_out, closed."""
    ang = [a0 + (a1 - a0) * k / seg for k in range(seg + 1)]
    def ring(r, z):
        return [bm.verts.new((r * math.cos(a), r * math.sin(a), z)) for a in ang]
    ob, ot, ib, it = ring(r_out, z0), ring(r_out, z1), ring(r_in, z0), ring(r_in, z1)
    for k in range(seg):
        bm.faces.new((ob[k], ob[k + 1], ot[k + 1], ot[k])).material_index = mi_out
        bm.faces.new((ib[k + 1], ib[k], it[k], it[k + 1])).material_index = mi_in
        bm.faces.new((ot[k], ot[k + 1], it[k + 1], it[k])).material_index = mi_in if mi_top is None else mi_top
        bm.faces.new((ob[k + 1], ob[k], ib[k], ib[k + 1])).material_index = mi_in if mi_top is None else mi_top
    for (o0, o1, i0, i1) in ((ob[0], ot[0], ib[0], it[0]), (ob[-1], ot[-1], ib[-1], it[-1])):
        bm.faces.new((i0, i1, o1, o0)).material_index = mi_in


def cab_floor():
    bm = bmesh.new()
    # a round terrazzo floor with an amber compass ring set into it (its own ring of the floor, not laid on it)
    lathe(bm, [(CAB_R + 0.06, -0.12), (CAB_R + 0.06, 0.0), (0.62, 0.0)], [CH, TZ], 48, cap0=True, cap1=False)
    lathe(bm, [(0.62, 0.0), (0.5, 0.0)], [AM], 48, cap0=False, cap1=False)
    lathe(bm, [(0.5, 0.0), (0.0, 0.0)], [TZ], 48, cap0=False, cap1=False)
    return bm


def cab_ceiling():
    bm = bmesh.new()
    # a low dome over the cab, a glowing ring round its middle, a chrome boss in the centre
    prof = [(CAB_R + 0.06, CAB_H), (CAB_R - 0.05, CAB_H + 0.02), (0.95, CAB_H + 0.12), (0.62, CAB_H + 0.2)]
    lathe(bm, prof, [EN, EN, EN], 48, cap0=False, cap1=False)
    lathe(bm, [(0.62, CAB_H + 0.2), (0.62, CAB_H + 0.17), (0.4, CAB_H + 0.17), (0.4, CAB_H + 0.24)], [CH, GLOW, CH], 48, cap0=False, cap1=False)
    lathe(bm, [(0.4, CAB_H + 0.24), (0.0, CAB_H + 0.27)], [EN], 48, cap0=False, cap1=False)
    lathe(bm, [(0.1, CAB_H + 0.2), (0.12, CAB_H + 0.24)], [CH], 16, cap0=True, cap1=False)
    # the outer shell over it (seen from the shaft)
    lathe(bm, [(CAB_R + 0.06, CAB_H + 0.32), (0.0, CAB_H + 0.36)], [EN], 48, cap0=False, cap1=False)
    lathe(bm, [(CAB_R + 0.06, CAB_H), (CAB_R + 0.06, CAB_H + 0.32)], [AM], 48, cap0=False, cap1=False)
    return bm


def _wall_layers(bm, a0, a1, mullions=(True, True)):
    """The rounded wall's bands from the floor up: an enamel dado, an amber belt, a glass band to see the shaft and
    the stair through, an enamel frieze; a chrome handrail at 0.92 m."""
    r_in, r_out = CAB_R, CAB_R + 0.06
    arc_wall(bm, a0, a1, 0.0, 0.95, r_in, r_out, EN, PLATE)
    arc_wall(bm, a0, a1, 0.95, 1.05, r_in, r_out + 0.01, AM, AM)
    arc_wall(bm, a0, a1, 1.05, 2.05, r_in + 0.02, r_in + 0.04, GLS, GLS)
    arc_wall(bm, a0, a1, 2.05, CAB_H, r_in, r_out, EN, PLATE)
    # mullions at the ends of the glass band
    for a, on in zip((a0, a1), mullions):
        if not on:
            continue                       # (the door's jamb stands there)
        c = Vector(((r_in + 0.03) * math.cos(a), (r_in + 0.03) * math.sin(a), 1.55))
        box(bm, c, (0.05, 0.05, 1.04), CH, Matrix.Rotation(a, 3, "Z"))      # into the bands over and under the glass
    # the handrail on brackets
    ang = [a0 + (a1 - a0) * k / SEG_Q for k in range(SEG_Q + 1)]
    pts = [Vector(((r_in - 0.07) * math.cos(a), (r_in - 0.07) * math.sin(a), 0.92)) for a in ang]
    for p, q in zip(pts, pts[1:]):
        tube(bm, p, q, 0.02, CH, 8)
    for a in (a0 + (a1 - a0) * 0.2, a0 + (a1 - a0) * 0.8):
        tube(bm, ((r_in - 0.07) * math.cos(a), (r_in - 0.07) * math.sin(a), 0.92), ((r_in - 0.005) * math.cos(a), (r_in - 0.005) * math.sin(a), 0.92), 0.012, CH, 6)


def cab_wall():
    """A closed quarter, facing +X (from -45 to +45 degrees)."""
    bm = bmesh.new()
    _wall_layers(bm, math.radians(-45), math.radians(45))
    return bm


def cab_door():
    """A quarter with the door's opening (1.32 m wide, 2.24 m high) facing +X: the rounded wall beside it, a curved
    amber frame round it, the floor indicator and the buttons."""
    bm = bmesh.new()
    half = math.asin(0.66 / CAB_R)
    _wall_layers(bm, math.radians(-45), -half, (True, False))
    _wall_layers(bm, half, math.radians(45), (False, True))
    # over the opening: the frieze and an amber lintel
    arc_wall(bm, -half, half, 2.24, CAB_H, CAB_R, CAB_R + 0.06, EN, PLATE, seg=6)
    arc_wall(bm, -half - 0.04, half + 0.04, 2.2, 2.24, CAB_R - 0.03, CAB_R + 0.08, AM, AM, seg=6)     # its ends in the walls
    for a in (-half, half):
        # the jambs up to the lintel's underside, their feet sunk 2 cm into the floor
        c = Vector(((CAB_R + 0.025) * math.cos(a), (CAB_R + 0.025) * math.sin(a), 1.09))
        box(bm, c, (0.11, 0.06, 2.22), AM, Matrix.Rotation(a, 3, "Z"))
    # the indicator over the door, the buttons on a chrome plate beside it (inside)
    a = 0.0
    box(bm, Vector((CAB_R - 0.04, 0, 2.28)), (0.012, 0.3, 0.05), GLOW)
    b = half + math.radians(10)
    c = Vector(((CAB_R - 0.03) * math.cos(b), (CAB_R - 0.03) * math.sin(b), 1.15))
    box(bm, c, (0.02, 0.14, 0.4), CH, Matrix.Rotation(b, 3, "Z"))
    for k in range(3):
        box(bm, c + Vector((-0.02 * math.cos(b), -0.02 * math.sin(b), -0.1 + 0.1 * k)), (0.012, 0.045, 0.045), GLOW, Matrix.Rotation(b, 3, "Z"))
    return bm


# --- output --------------------------------------------------------------------------------------------------------

def metre_uv(me):
    uv = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        n = poly.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = (co.x, co.y) if ax == 2 else ((co.x, co.z) if ax == 1 else (co.y, co.z))
        if poly.material_index == POSTER:
            # the picture: 0..1 over the poster's face (it faces +X: u along +Y, v up)
            for li in poly.loop_indices:
                co = me.vertices[me.loops[li].vertex_index].co
                uv.data[li].uv = (co.y / 0.8 + 0.5, co.z / 1.2 + 0.5)


def export(made, name, mats=None, col=None, out=None):
    """mats / col: another material list and its colours (lobby_props.py); out: another folder."""
    mats = mats or MATS
    col = col or COL
    bm, m = made if isinstance(made, tuple) else (made, None)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    if m is not None:
        me.transform(m)
    for k in mats:
        mat = bpy.data.materials.get(k) or bpy.data.materials.new(k)
        mat.diffuse_color = col[k] + (1,)
        me.materials.append(mat)
    me.update()
    metre_uv(me)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    if name.startswith("SM_TR_Furn_LiftCab"):
        # the cab's glass band as a mesh of its own (<name>_Glass): the game keeps glass out of shadows and distance
        # fields, so light comes through it
        gpath = os.path.join(out or OUT, name + "_Glass.glb")
        if os.path.isfile(gpath):
            os.remove(gpath)
        gi = [i for i, m_ in enumerate(me.materials) if m_.name.startswith("TR_Glass")]
        if any(p.material_index in gi for p in me.polygons):
            g = o.copy()
            g.data = me.copy()
            bpy.context.scene.collection.objects.link(g)
            for ob, keep in ((g, True), (o, False)):
                b2 = bmesh.new()
                b2.from_mesh(ob.data)
                bmesh.ops.delete(b2, geom=[f for f in b2.faces if (f.material_index in gi) != keep], context="FACES")
                b2.to_mesh(ob.data)
                b2.free()
            g.name = g.data.name = name + "_Glass"
            bpy.ops.object.select_all(action="DESELECT")
            g.select_set(True)
            bpy.context.view_layer.objects.active = g
            bpy.ops.export_scene.gltf(filepath=gpath, use_selection=True, export_format="GLB", export_yup=True, export_apply=True)
            bpy.data.objects.remove(g)
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.gltf(filepath=os.path.join(out or OUT, name + ".glb"), use_selection=True, export_format="GLB",
                              export_yup=True, export_apply=True)
    n = sum(len(p.vertices) - 2 for p in me.polygons)
    bpy.data.objects.remove(o)
    bpy.data.meshes.remove(me)
    print("FURNITURE", name, n, flush=True)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    for name, f in (("Bench", bench), ("EggChair", egg_chair), ("Table", table), ("Planter", planter), ("Lamp", lamp),
                    ("Bin", bin_), ("Rug", rug), ("Poster", poster), ("Sconce", sconce),
                    ("LiftCabFloor", cab_floor), ("LiftCabCeiling", cab_ceiling), ("LiftCabWall", cab_wall),
                    ("LiftCabDoor", cab_door)):
        export(f(), "SM_TR_Furn_" + name)


if __name__ == "__main__":
    main()
