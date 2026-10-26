"""The cable car's pylons as our own models (Dima, 2026-10-26: the hi3d pylons, design/transport/gen/pylon_*.glb, only as
the reference), one mesh per whole metre of height so the game picks the one a pylon needs and sinks the extra into
the ground (each stands on a pad with a skirt). docs/transport.md, "Pylons".

  Blender -b -P blender/transport/pylons.py [-- line|corner]

SM_TR_PylonLine_H<nn> (nn = 8..46 m): the rocket mast. A pad, four swept fins, a tapered shaft with amber bands and a
  ladder up it, a collar, a crossarm across the line and on each end a battery of four sheaves; the ropes ride on the
  sheaves' tops at z = nn m, ROPE_Y off the axis (the two tracks 9 m apart); a spire with a red beacon.
SM_TR_PylonCorner_H<nn> (nn = 8, 10, .. 46 m): the lattice A-frame of a corner. Four splayed legs braced in bays, a
  crossarm with a curved deflection shoe of sheaves on each end (the rope turns over it at z = nn m), a golden star.
Frame (Blender, metres; Unreal (x, -y, z) * 100): the origin on the ground at the axis; +X along the line, the
crossarm along Y. Clean meshes with metre UVs: TR_Tx_<Plate|Amber|Steel|Dark|Concrete> (the game's tiled surfaces),
TR_Glow_Red (the beacon), TR_Glow (the star). Writes design/transport/game/SM_TR_Pylon*_H*.glb and
Content/Transport/pylons.json.
"""
import bmesh
import bpy
import json
import math
import os
import sys
from mathutils import Matrix, Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "design", "transport", "game")
JSON = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Transport", "pylons.json")

ROPE_Y = 4.5                 # the tracks are 9 m apart
LINE_H = range(8, 47)        # m
CORNER_H = range(8, 47, 2)
SKIRT = 2.2                  # the pad goes this far into the ground (a pylon sinks up to its step into it)
MATS = ["TR_Tx_Plate", "TR_Tx_Amber", "TR_Tx_Steel", "TR_Tx_Dark", "TR_Tx_Concrete", "TR_Glow_Red", "TR_Glow"]
COL = {"TR_Tx_Plate": (0.9, 0.87, 0.8), "TR_Tx_Amber": (0.85, 0.45, 0.1), "TR_Tx_Steel": (0.7, 0.72, 0.75),
       "TR_Tx_Dark": (0.2, 0.21, 0.23), "TR_Tx_Concrete": (0.55, 0.54, 0.5), "TR_Glow_Red": (1, 0.1, 0.05),
       "TR_Glow": (1, 0.75, 0.3)}
PLATE, AMBER, STEEL, DARK, CONC, RED, GLOW = range(len(MATS))


def log(*a):
    print("PYLONS", *a, flush=True)


# --- primitives ----------------------------------------------------------------------------------------------------

def box(bm, c, size, mi, rot=None):
    r = rot or Matrix.Identity(3)
    h = Vector(size) / 2
    v = [bm.verts.new(c + r @ Vector((sx * h.x, sy * h.y, sz * h.z))) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
    for ids in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
        bm.faces.new([v[i] for i in ids]).material_index = mi


def beam(bm, a, b, w, h, mi):
    d = b - a
    if d.length < 1e-4:
        return
    x = d.normalized()
    up = Vector((0, 0, 1)) if abs(x.z) < 0.95 else Vector((1, 0, 0))
    y = up.cross(x).normalized()
    z = x.cross(y)
    box(bm, (a + b) / 2, (d.length, w, h), mi, Matrix((x, y, z)).transposed())


def tube(bm, a, b, r, mi, seg=8):
    """A round bar from a to b."""
    d = b - a
    if d.length < 1e-4:
        return
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


def lathe(bm, prof, mats, seg=32, cap0=True, cap1=True):
    """A round body turned from prof [(r, z)], each ring band in mats[i] (one fewer than the points)."""
    rings = [[bm.verts.new((r * math.cos(2 * math.pi * i / seg), r * math.sin(2 * math.pi * i / seg), z)) for i in range(seg)]
             for r, z in prof]
    for k in range(len(rings) - 1):
        A, B = rings[k], rings[k + 1]
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new((A[i], A[j], B[j], B[i])).material_index = mats[k]
    if cap0:
        bm.faces.new(rings[0][::-1]).material_index = mats[0]
    if cap1:
        bm.faces.new(rings[-1]).material_index = mats[-1]


def wheel(bm, c, r, w, mi, mi_rim, seg=16):
    """A sheave turning about Y (the rope runs along X over its top): a disc with a dark groove rim."""
    def ring(rr, y):
        return [bm.verts.new(c + Vector((rr * math.cos(2 * math.pi * i / seg), y, rr * math.sin(2 * math.pi * i / seg)))) for i in range(seg)]
    a, b = ring(r, -w / 2), ring(r, w / 2)
    ai, bi = ring(r * 0.35, -w / 2 - 0.02), ring(r * 0.35, w / 2 + 0.02)
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((a[j], a[i], b[i], b[j])).material_index = mi_rim
        bm.faces.new((a[i], a[j], ai[j], ai[i])).material_index = mi
        bm.faces.new((b[j], b[i], bi[i], bi[j])).material_index = mi
    bm.faces.new(ai).material_index = mi
    bm.faces.new(bi[::-1]).material_index = mi


def fin(bm, ang, pts, t, mi, mi_edge):
    """A plate in the vertical plane at azimuth ang, its outline pts [(r, z)], t thick; the outer edge amber."""
    d = Vector((math.cos(ang), math.sin(ang), 0))
    u = Vector((-d.y, d.x, 0))
    n = len(pts)
    A = [bm.verts.new(d * r + Vector((0, 0, z)) - u * t / 2) for r, z in pts]
    B = [bm.verts.new(d * r + Vector((0, 0, z)) + u * t / 2) for r, z in pts]
    bm.faces.new(A[::-1]).material_index = mi
    bm.faces.new(B).material_index = mi
    for i in range(n):
        j = (i + 1) % n
        edge = pts[i][0] > 0.6 and pts[j][0] > 0.6
        bm.faces.new((A[i], A[j], B[j], B[i])).material_index = mi_edge if edge else mi


def pad(bm, r, top, mi, sides=24):
    """The concrete pad: its top at `top`, a skirt SKIRT into the ground."""
    lathe(bm, [(r, -SKIRT), (r, top)], [mi], seg=sides)


# --- the line pylon ------------------------------------------------------------------------------------------------

def line_pylon(H):
    bm = bmesh.new()
    r0 = 0.62 + 0.011 * H          # the shaft at its foot and its top
    r1 = 0.30 + 0.004 * H
    head = H - 0.6                 # the crossarm's top: the sheave batteries sit on its ends
    collar0 = head - 1.5
    pad(bm, r0 + 1.9, 0.3, CONC)
    rad = lambda z: r0 + (r1 - r0) * (z / collar0) + 0.05 * math.sin(math.pi * z / collar0)
    # the shaft: ivory plate, amber bands (two over the fins, one under the head), a darker plinth ring
    marks = [0.3, 0.9, 4.6, 5.3, 6.1, 6.8, collar0 - 3.0, collar0 - 2.4, collar0]
    marks = sorted(set(m for m in marks if 0.3 <= m <= collar0))
    zs = sorted(set(marks + [0.3 + (collar0 - 0.3) * k / 10 for k in range(11)]))
    mats = []
    for a, b in zip(zs, zs[1:]):
        m = (a + b) / 2
        band = 0.3 <= m < 0.9 and DARK or (4.6 <= m < 5.3 or 6.1 <= m < 6.8 or collar0 - 3.0 <= m < collar0 - 2.4) and AMBER or PLATE
        mats.append(band)
    lathe(bm, [(rad(z) + (0.015 if mats[min(i, len(mats) - 1)] == AMBER else 0), z) for i, z in enumerate(zs)], mats)
    # four swept fins (the rocket's), between the line and across it
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        top = 5.4
        out = r0 + 2.2
        pts = [(rad(0.35) - 0.05, 0.3), (out, 0.3), (out, 0.75)]
        for i in range(1, 9):
            f = i / 8
            pts.append((out - (out - rad(top) + 0.05) * (1 - (1 - f) ** 2), 0.75 + (top - 0.75) * f))
        pts.append((rad(top) - 0.05, top))
        fin(bm, a, pts, 0.16, PLATE, AMBER)
        # a dark shoe under each fin
        d = Vector((math.cos(a), math.sin(a), 0))
        box(bm, d * (out - 0.25) + Vector((0, 0, 0.36)), (0.7, 0.36, 0.12), DARK, Matrix.Rotation(a, 3, "Z"))
    # the ladder up the side facing -X (along the line, out of the ropes' way), with rest hoops every 6 m
    zl = 1.2
    rails = []
    while zl < collar0 - 0.2:
        z2 = min(zl + 1.5, collar0 - 0.2)
        for s in (-0.22, 0.22):
            a_ = Vector((-(rad(zl) + 0.22), s, zl))
            b_ = Vector((-(rad(z2) + 0.22), s, z2))
            tube(bm, a_, b_, 0.025, STEEL, 6)
        zl = z2
    z = 1.5
    while z < collar0 - 0.3:
        tube(bm, Vector((-(rad(z) + 0.22), -0.22, z)), Vector((-(rad(z) + 0.22), 0.22, z)), 0.018, STEEL, 6)
        z += 0.32
    # the head: a collar, the crossarm (swept up a little at the ends), the sheave batteries, a walkway grid
    lathe(bm, [(r1 + 0.05, collar0), (r1 + 0.32, collar0 + 0.4), (r1 + 0.32, collar0 + 0.9), (r1 + 0.1, head + 0.1)],
          [PLATE, AMBER, PLATE])
    for sy in (-1, 1):
        a_ = Vector((0, 0, head - 0.3))
        b_ = Vector((0, sy * (ROPE_Y + 0.35), head - 0.22))
        beam(bm, a_, b_, 0.42, 0.44, PLATE)
        beam(bm, Vector((0, 0, head - 1.1)), Vector((0, sy * (ROPE_Y - 0.7), head - 0.45)), 0.2, 0.18, AMBER)
        # on the arm's end a saddle bracket and the rocker; four sheaves along the line, the rope on their tops at H
        hy = sy * ROPE_Y
        box(bm, Vector((0, hy, head + 0.02)), (0.55, 0.3, 0.12), DARK)
        beam(bm, Vector((-1.15, hy - sy * 0.12, H - 0.42)), Vector((1.15, hy - sy * 0.12, H - 0.42)), 0.08, 0.22, DARK)
        beam(bm, Vector((-1.15, hy + sy * 0.12, H - 0.42)), Vector((1.15, hy + sy * 0.12, H - 0.42)), 0.08, 0.22, DARK)
        box(bm, Vector((0, hy, (head + 0.08 + H - 0.53) / 2)), (0.3, 0.3, H - 0.53 - head - 0.08), STEEL)
        for x in (-0.9, -0.3, 0.3, 0.9):
            wheel(bm, Vector((x, hy, H - 0.24)), 0.24, 0.1, STEEL, DARK)
            tube(bm, Vector((x, hy - 0.17, H - 0.24)), Vector((x, hy + 0.17, H - 0.24)), 0.03, STEEL, 6)
    # the spire and the beacon
    lathe(bm, [(r1 * 0.6, head + 0.1), (0.08, H + 0.9)], [PLATE], seg=12)
    lathe(bm, [(0.12, H + 0.9), (0.14, H + 1.05), (0.1, H + 1.25)], [RED, RED], seg=12)
    return bm


# --- the corner pylon ----------------------------------------------------------------------------------------------

def corner_pylon(H):
    bm = bmesh.new()
    top = H - 1.4                   # the legs meet the crossarm
    b0, b1 = 2.6 + 0.04 * H, 0.75   # half the square at the foot and at the top
    leg = lambda sx, sy, z: Vector((sx * (b0 + (b1 - b0) * z / top), sy * (b0 + (b1 - b0) * z / top), z))
    for sx in (-1, 1):
        for sy in (-1, 1):
            pad_c = leg(sx, sy, 0)
            bm2 = bm
            lathe_at(bm2, pad_c, 0.75, 0.35, CONC)
            box(bm, pad_c + Vector((0, 0, 0.42)), (0.8, 0.8, 0.14), DARK)
            # the leg in bands: amber at its foot and its top
            zs = [0.45, 1.6] + [1.6 + (top - 2.4) * k / 6 for k in range(1, 7)] + [top]
            for k, (za, zb) in enumerate(zip(zs, zs[1:])):
                tube(bm, leg(sx, sy, za), leg(sx, sy, zb), 0.2, AMBER if k in (0, len(zs) - 2) else PLATE, 10)
    # bays: a ring and an X on each face
    nb = max(2, int(round(top / 3.2)))
    for k in range(nb):
        za, zb = 1.6 + (top - 1.6) * k / nb, 1.6 + (top - 1.6) * (k + 1) / nb
        for (ax, ay), (bx, by_) in (((1, 1), (1, -1)), ((1, -1), (-1, -1)), ((-1, -1), (-1, 1)), ((-1, 1), (1, 1))):
            tube(bm, leg(ax, ay, zb), leg(bx, by_, zb), 0.09, PLATE, 8)
            tube(bm, leg(ax, ay, za), leg(bx, by_, zb), 0.07, PLATE, 6)
            tube(bm, leg(bx, by_, za), leg(ax, ay, zb), 0.07, PLATE, 6)
    # the crossarm; on each end a shoe of sheaves lying flat that the rope bends round in plan (built for a turn to
    # the left, +Y: the rope bears on the shoe's outer side, -Y; the game mirrors it for a right turn), at z = H
    arm_z = H - 0.75
    beam(bm, Vector((0, -(ROPE_Y + 0.9), arm_z)), Vector((0, ROPE_Y + 0.9, arm_z)), 0.7, 0.5, PLATE)
    beam(bm, Vector((0, -(ROPE_Y - 0.5), arm_z - 0.45)), Vector((0, ROPE_Y - 0.5, arm_z - 0.45)), 0.25, 0.2, AMBER)
    beam(bm, Vector((0, 0, top)), Vector((0, 0, arm_z - 0.25)), 0.5, 0.5, PLATE)
    Rs = 7.0
    for sy in (-1, 1):
        hy = sy * ROPE_Y
        cen = Vector((0, hy + Rs + 0.32, H))        # the arc's centre on the turn's inside
        arc = [math.radians(-9 + 3 * i) for i in range(7)]
        pts = [cen + Vector((Rs * math.sin(a), -Rs * math.cos(a), 0)) for a in arc]
        for p_ in pts:
            hwheel(bm, p_ + Vector((0, 0.32, 0)), 0.3, 0.12, STEEL, DARK)
        for p_, q_ in zip(pts, pts[1:]):
            beam(bm, p_ + Vector((0, 0.32, -0.16)), q_ + Vector((0, 0.32, -0.16)), 0.5, 0.08, DARK)
        box(bm, Vector((0, hy + 0.35, (arm_z + 0.25 + H - 0.2) / 2)), (2.6, 0.5, (H - 0.2) - (arm_z + 0.25)), STEEL)
    # the golden star on its staff
    tube(bm, Vector((0, 0, arm_z + 0.25)), Vector((0, 0, H + 1.6)), 0.07, PLATE, 8)
    star(bm, Vector((0, 0, H + 2.1)), 0.7, 0.25, 8, GLOW)
    return bm


def hwheel(bm, c, r, w, mi, mi_rim, seg=16):
    """A sheave lying flat (turning about Z): the rope bears on its rim at the wheel's middle height."""
    def ring(rr, z):
        return [bm.verts.new(c + Vector((rr * math.cos(2 * math.pi * i / seg), rr * math.sin(2 * math.pi * i / seg), z))) for i in range(seg)]
    a, b = ring(r, -w / 2), ring(r, w / 2)
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((a[i], a[j], b[j], b[i])).material_index = mi_rim
    bm.faces.new(a[::-1]).material_index = mi
    bm.faces.new(b).material_index = mi


def lathe_at(bm, c, r, h, mi):
    seg = 16
    a = [bm.verts.new(c + Vector((r * math.cos(2 * math.pi * i / seg), r * math.sin(2 * math.pi * i / seg), -SKIRT))) for i in range(seg)]
    b = [bm.verts.new(c + Vector((r * math.cos(2 * math.pi * i / seg), r * math.sin(2 * math.pi * i / seg), h))) for i in range(seg)]
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((a[i], a[j], b[j], b[i])).material_index = mi
    bm.faces.new(a[::-1]).material_index = mi
    bm.faces.new(b).material_index = mi


def star(bm, c, r_out, r_in, n, mi, t=0.12):
    """A flat n-pointed star facing +-X (seen along the line), t thick."""
    pts = []
    for i in range(2 * n):
        r = r_out if i % 2 == 0 else r_in
        a = math.pi / 2 + math.pi * i / n
        pts.append((r * math.cos(a), r * math.sin(a)))
    A = [bm.verts.new(c + Vector((-t / 2, x, y))) for x, y in pts]
    B = [bm.verts.new(c + Vector((t / 2, x, y))) for x, y in pts]
    ca, cb = bm.verts.new(c + Vector((-t, 0, 0))), bm.verts.new(c + Vector((t, 0, 0)))
    m = len(pts)
    for i in range(m):
        j = (i + 1) % m
        bm.faces.new((ca, A[j], A[i])).material_index = mi
        bm.faces.new((cb, B[i], B[j])).material_index = mi
        bm.faces.new((A[i], A[j], B[j], B[i])).material_index = mi


# --- output --------------------------------------------------------------------------------------------------------

def metre_uv(me):
    uv = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        n = poly.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = (co.x, co.y) if ax == 2 else ((co.x, co.z) if ax == 1 else (co.y, co.z))


def export(bm, name):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for k in MATS:
        m = bpy.data.materials.get(k) or bpy.data.materials.new(k)
        m.diffuse_color = COL[k] + (1,)
        me.materials.append(m)
    me.update()
    metre_uv(me)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, name + ".glb"), use_selection=True, export_format="GLB",
                              export_yup=True, export_apply=True)
    n = sum(len(p.vertices) - 2 for p in me.polygons)
    bpy.data.objects.remove(o)
    bpy.data.meshes.remove(me)
    return n


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["line", "corner"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for f in os.listdir(OUT) if os.path.isdir(OUT) else []:
        if f.startswith(("SM_TR_PylonLine", "SM_TR_PylonCorner")) and f.endswith(".glb"):
            if ("line" in argv and f.startswith("SM_TR_PylonLine")) or ("corner" in argv and f.startswith("SM_TR_PylonCorner")):
                os.remove(os.path.join(OUT, f))       # the old hi3d pylon parts go too
    data = json.load(open(JSON)) if os.path.isfile(JSON) else {}
    if "line" in argv:
        tr = {}
        for H in LINE_H:
            tr[H] = export(line_pylon(H), "SM_TR_PylonLine_H%02d" % H)
        data["line"] = {"mesh": "SM_TR_PylonLine_H%02d", "heights": [LINE_H[0], LINE_H[-1], 1], "rope_y": ROPE_Y * 100,
                        "skirt": SKIRT * 100, "tris": [min(tr.values()), max(tr.values())]}
        log("line", len(tr), "meshes", min(tr.values()), "..", max(tr.values()), "triangles")
    if "corner" in argv:
        tr = {}
        for H in CORNER_H:
            tr[H] = export(corner_pylon(H), "SM_TR_PylonCorner_H%02d" % H)
        data["corner"] = {"mesh": "SM_TR_PylonCorner_H%02d", "heights": [CORNER_H[0], CORNER_H[-1], 2],
                          "rope_y": ROPE_Y * 100, "skirt": SKIRT * 100, "shoe_deg": 18.0,
                          "turn": "built for a left turn (+Y); mirror Y for a right one",
                          "tris": [min(tr.values()), max(tr.values())]}
        log("corner", len(tr), "meshes", min(tr.values()), "..", max(tr.values()), "triangles")
    data["_doc"] = ("Made by blender/transport/pylons.py. Per kind the mesh name pattern for a whole height in m (ceil of "
                    "the pylon's height, the extra sunk into the ground, at most a step), heights [min, max, step], the "
                    "ropes' offset from the axis across the line (UE cm), the pad's skirt. Mesh frame: origin on the "
                    "ground at the axis, +X along the line, the rope on the sheaves' tops at z = the height.")
    json.dump(data, open(JSON, "w"), indent=1, sort_keys=True)


main()
