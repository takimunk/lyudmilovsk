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
CAB = 2.30                   # the cab's inside, square (the shaft is 2.5 m)
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
        tube(bm, (-0.2, y, 0.5), (-0.26, y, 0.9), 0.022, CH)
        tube(bm, (0.18, y, 0.02), (-0.18, y, 0.02), 0.02, CH)
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
    lathe(bm, [(0.28, 0.0), (0.28, 0.02), (0.06, 0.08), (0.04, 0.6), (0.12, 0.66)], [EN, EN, EN, EN], 20)
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
    lathe(bm, [(1.3, 0.0), (1.3, 0.012)], [MO], 48)
    lathe(bm, [(1.32, 0.0), (1.32, 0.013), (1.22, 0.013)], [AM, AM], 48, cap0=False, cap1=False)
    return bm


def poster():
    bm = bmesh.new()
    w, h = 0.8, 1.2
    # the picture (UV 0..1 over it), a chrome frame round it, a backing board
    v = [bm.verts.new(p) for p in ((0.025, -w / 2, -h / 2), (0.025, w / 2, -h / 2), (0.025, w / 2, h / 2), (0.025, -w / 2, h / 2))]
    f = bm.faces.new(v)
    f.material_index = POSTER
    box(bm, (0.01, 0, 0), (0.02, w + 0.04, h + 0.04), DK)
    for (a, b) in (((-w / 2 - 0.02, -h / 2 - 0.02), (w / 2 + 0.02, -h / 2 - 0.02)), ((w / 2 + 0.02, -h / 2 - 0.02), (w / 2 + 0.02, h / 2 + 0.02)),
                   ((w / 2 + 0.02, h / 2 + 0.02), (-w / 2 - 0.02, h / 2 + 0.02)), ((-w / 2 - 0.02, h / 2 + 0.02), (-w / 2 - 0.02, -h / 2 - 0.02))):
        tube(bm, (0.035, a[0], a[1]), (0.035, b[0], b[1]), 0.015, CH, 6)
    return bm


def sconce():
    bm = bmesh.new()
    tube(bm, (0.0, 0, -0.1), (0.02, 0, 0.1), 0.06, CH, 12)          # the wall plate
    tube(bm, (0.0, 0, 0), (0.12, 0, 0.05), 0.012, CH, 6)
    sphere(bm, (0.18, 0, 0.08), 0.1, GLOW)
    return bm


def cab_floor():
    bm = bmesh.new()
    box(bm, (0, 0, -0.06), (CAB, CAB, 0.12), TZ)
    lathe(bm, [(0.6, 0.0), (0.6, 0.004)], [AM], 32)      # an amber ring inlaid in the terrazzo
    lathe(bm, [(0.5, 0.0), (0.5, 0.005)], [TZ], 32)
    return bm


def cab_ceiling():
    bm = bmesh.new()
    box(bm, (0, 0, CAB_H + 0.06), (CAB, CAB, 0.12), PLATE)
    lathe(bm, [(0.7, CAB_H - 0.02), (0.72, CAB_H), (0.62, CAB_H)], [CH, CH], 32, cap0=False, cap1=False)
    lathe(bm, [(0.6, CAB_H - 0.03), (0.0, CAB_H - 0.03)], [GLOW], 32, cap0=False, cap1=False)
    for k in range(8):                                      # coffers: radial chrome ribs
        a = 2 * math.pi * k / 8
        tube(bm, (0.72 * math.cos(a), 0.72 * math.sin(a), CAB_H - 0.01), (1.1 * math.cos(a), 1.1 * math.sin(a), CAB_H - 0.01), 0.015, CH, 6)
    return bm


def cab_wall():
    """A closed side at +X (the cab's middle at the origin): wood-coloured enamel panels, a mirror band, a handrail."""
    bm = bmesh.new()
    x = CAB / 2
    box(bm, (x - 0.02, 0, CAB_H / 2), (0.04, CAB, CAB_H), PLATE)
    box(bm, (x - 0.045, 0, 0.06), (0.01, CAB, 0.12), CH)                   # kick plate
    box(bm, (x - 0.045, 0, 1.5), (0.01, CAB - 0.5, 0.7), GLS)              # the mirror
    for y in (-CAB / 2 + 0.25, CAB / 2 - 0.25):
        box(bm, (x - 0.045, y, CAB_H / 2), (0.012, 0.03, CAB_H), AM)         # amber pilaster lines
    tube(bm, (x - 0.1, -CAB / 2 + 0.15, 0.92), (x - 0.1, CAB / 2 - 0.15, 0.92), 0.022, CH)
    for y in (-CAB / 2 + 0.2, CAB / 2 - 0.2):
        tube(bm, (x - 0.1, y, 0.92), (x - 0.04, y, 0.92), 0.012, CH, 6)
    return bm


def cab_door():
    """A side with the door at +X: a brass frame round a 1.24 m opening, the floor indicator over it, a button panel."""
    bm = bmesh.new()
    x = CAB / 2
    for y0, y1 in ((-CAB / 2, -0.66), (0.66, CAB / 2)):
        box(bm, (x - 0.02, (y0 + y1) / 2, CAB_H / 2), (0.04, y1 - y0, CAB_H), PLATE)
    box(bm, (x - 0.02, 0, (2.24 + CAB_H) / 2), (0.04, 1.32, CAB_H - 2.24), PLATE)
    for y in (-0.64, 0.64):
        box(bm, (x - 0.05, y, 1.12), (0.03, 0.05, 2.24), AM)
    box(bm, (x - 0.05, 0, 2.25), (0.03, 1.33, 0.05), AM)
    box(bm, (x - 0.045, 0, 2.29), (0.01, 0.3, 0.06), GLOW)                 # the indicator
    box(bm, (x - 0.05, 0.85, 1.15), (0.02, 0.14, 0.4), CH)                 # the button panel
    for k in range(3):
        box(bm, (x - 0.064, 0.85, 1.03 + 0.1 * k), (0.012, 0.045, 0.045), GLOW)
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
