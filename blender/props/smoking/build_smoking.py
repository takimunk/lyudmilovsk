"""The smoking kit for the game (docs/smoking.md): a flip-top pack of ten, the cigarette (whole, and in three parts for
burning in the hand), the butt, a disposable lighter with its flame. Clean game geometry built here, no generated models.

  /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/props/smoking/build_smoking.py

-> blender/props/smoking/export/smoking_kit.glb (one mesh object per game asset, no images; unreal/scripts/import_smoking.py
makes /Game/Smoking/SM_*), export/uv_*.png (UV layouts to check) and LINT lines (overlapping faces, bad UVs; exit code 1 if any).

Units: metres here, cm after the import; Blender (x, y, z) is UE (x, -y, z). Frames (what the game code relies on):
  SM_CigPackBody   origin at the middle of the bottom; front (the warning) faces +X, width along Y, height +Z
  SM_CigPackLid    origin on the hinge (back top edge of the body's cut), so a turn about Y opens it (negative pitch)
  SM_Cigarette     the whole one standing in the pack: origin at the bottom (tobacco end), filter up (+Z)
  SM_CigFilter     origin at the mouth end, along +X, FILTER long
  SM_CigPaper      a 1 cm open tube along +X from the origin: scaled in X to the paper left
  SM_CigTip        the burning end, origin where the paper ends, TIP long along +X (ember and ash, material slot CigEmber)
  SM_CigButt       lying butt: origin at its middle on the ground, along +X
  SM_Lighter       origin at the middle of the bottom, front +X, height +Z
  SM_LighterFlame  origin at the flame's foot, up +Z (unit height 1 cm: scaled by the game)
The pack's atlas (material slot Pack) is laid out by PACK_UV below; tools/smoking_pack.py prints the art and the warning into it.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(HERE, "export")
MM = 0.001

# the pack: 2 rows of 5 cigarettes, cardboard WALL thick
CIG_R = 3.9 * MM
CIG_L = 84.0 * MM
FILTER = 21.0 * MM
TIP = 4.0 * MM
PITCH = 8.0 * MM             # centres of neighbouring cigarettes (0.2 mm apart)
WALL = 0.5 * MM
GAP = 0.25 * MM              # air between the cigarettes and the inside of the box
COLLAR = 0.4 * MM            # the inner card collar behind the front and the sides
IN_W = 5 * PITCH + 2 * GAP + 2 * COLLAR     # inside along Y
IN_D = 2 * PITCH + 2 * GAP + COLLAR         # inside along X (the collar only at the front)
W = IN_W + 2 * WALL
D = IN_D + 2 * WALL
H = CIG_L + 2 * WALL + 3.0 * MM
CUT_FRONT = 60.0 * MM        # the lid's cut: low at the front, high at the back
CUT_BACK = 68.0 * MM
SEAM = 0.3 * MM              # between the body and the lid where they meet
COLLAR_TOP = CUT_FRONT + 9.0 * MM
SEGS = 12

# the lighter
L_D, L_W, L_BODY, L_HOOD = 11.0 * MM, 24.0 * MM, 62.0 * MM, 11.0 * MM

# atlas regions (Blender UV, v up): name -> (u0, v0, u1, v1); 1 mm = K in UV
K = 0.0079
PACK_UV = {
    "front": (0.008, 0.008, 0.008 + W / MM * K, 0.008 + H / MM * K),
    "back": (0.350, 0.008, 0.350 + W / MM * K, 0.008 + H / MM * K),
    "side_r": (0.692, 0.008, 0.692 + D / MM * K, 0.008 + H / MM * K),     # +Y side
    "side_l": (0.841, 0.008, 0.841 + D / MM * K, 0.008 + H / MM * K),     # -Y side
    "top": (0.008, 0.720, 0.008 + W / MM * K, 0.720 + D / MM * K),
    "bottom": (0.350, 0.720, 0.350 + W / MM * K, 0.720 + D / MM * K),
    "inner": (0.700, 0.720, 0.990, 0.990),
}

LINTS = []


def lint(msg):
    LINTS.append(msg)
    print("LINT " + msg)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, color):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1.0)
    return m


def new_object(name, bm, mats):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    for p in me.polygons:
        p.use_smooth = False
    return o


def box_faces(bm, lo, hi):
    """An axis-aligned box (outward faces) into bm."""
    v = [bm.verts.new((x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    idx = lambda i, j, k: v[i * 4 + j * 2 + k]
    quads = [
        (idx(1, 0, 0), idx(1, 1, 0), idx(1, 1, 1), idx(1, 0, 1)),   # +X
        (idx(0, 0, 0), idx(0, 0, 1), idx(0, 1, 1), idx(0, 1, 0)),   # -X
        (idx(0, 1, 0), idx(0, 1, 1), idx(1, 1, 1), idx(1, 1, 0)),   # +Y
        (idx(0, 0, 0), idx(1, 0, 0), idx(1, 0, 1), idx(0, 0, 1)),   # -Y
        (idx(0, 0, 1), idx(1, 0, 1), idx(1, 1, 1), idx(0, 1, 1)),   # +Z
        (idx(0, 0, 0), idx(0, 1, 0), idx(1, 1, 0), idx(1, 0, 0)),   # -Z
    ]
    return [bm.faces.new(q) for q in quads]


def mesh_object_from_box(name, lo, hi):
    bm = bmesh.new()
    box_faces(bm, lo, hi)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    return o


def boolean(target, cutter, op="DIFFERENCE"):
    mod = target.modifiers.new("b", "BOOLEAN")
    mod.operation = op
    mod.solver = "EXACT"
    mod.object = cutter
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cutter)


def cut_z(x):
    """Height of the lid's cut at depth x (front +D/2 low, back -D/2 high)."""
    t = (x + D / 2) / D
    return CUT_BACK + (CUT_FRONT - CUT_BACK) * t


def split_by_plane(obj, keep_above, offset):
    """Cuts obj along the slanted lid plane (moved by offset along Z) and keeps the part above or below."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    # the plane through (D/2, *, CUT_FRONT) and (-D/2, *, CUT_BACK)
    co = Vector((0.0, 0.0, (CUT_FRONT + CUT_BACK) / 2 + offset))
    no = Vector((CUT_BACK - CUT_FRONT, 0.0, D)).normalized()
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    res = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no, clear_inner=keep_above, clear_outer=not keep_above)
    # the cut goes through the wall: an outer and an inner loop, filled as the ring between them (not two lids on each other)
    edges = [e for e in res["geom_cut"] if isinstance(e, bmesh.types.BMEdge)]
    bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=edges)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()


def uv_pack(obj, inner_scale=1.0):
    """Planar UVs per face into the pack atlas: outer faces into their panel, everything inside (and the rims) into 'inner'."""
    me = obj.data
    uvl = me.uv_layers.new(name="UV")
    eps = 0.02 * MM
    for p in me.polygons:
        n = p.normal
        c = obj.matrix_world @ p.center
        outer = None
        if abs(c.x - D / 2) < eps and n.x > 0.9:
            outer = "front"
        elif abs(c.x + D / 2) < eps and n.x < -0.9:
            outer = "back"
        elif abs(c.y - W / 2) < eps and n.y > 0.9:
            outer = "side_r"
        elif abs(c.y + W / 2) < eps and n.y < -0.9:
            outer = "side_l"
        elif abs(c.z - H) < eps and n.z > 0.9:
            outer = "top"
        elif abs(c.z) < eps and n.z < -0.9:
            outer = "bottom"
        for li in p.loop_indices:
            co = obj.matrix_world @ me.vertices[me.loops[li].vertex_index].co
            if outer == "front":       # seen from +X: right is +Y
                s, t = (co.y + W / 2) / W, co.z / H
            elif outer == "back":      # seen from -X: right is -Y
                s, t = (W / 2 - co.y) / W, co.z / H
            elif outer == "side_r":    # seen from +Y: right is -X
                s, t = (D / 2 - co.x) / D, co.z / H
            elif outer == "side_l":    # seen from -Y: right is +X
                s, t = (co.x + D / 2) / D, co.z / H
            elif outer == "top":       # from above, the front at the bottom of the picture
                s, t = (co.y + W / 2) / W, (D / 2 - co.x) / D
            elif outer == "bottom":
                s, t = (co.y + W / 2) / W, (co.x + D / 2) / D
            else:
                # the inside: a plain card, projected along the face's main axis
                a = max(range(3), key=lambda i: abs(n[i]))
                size = (D, W, H)
                u_, v_ = [(co[i] + (0 if i == 2 else size[i] / 2)) / size[i] for i in range(3) if i != a]
                s, t = u_, v_
            r = PACK_UV[outer or "inner"]
            uvl.data[li].uv = (r[0] + (r[2] - r[0]) * min(max(s, 0), 1), r[1] + (r[3] - r[1]) * min(max(t, 0), 1))


def build_pack(mats):
    # an open-topped box with walls, then the lid cut off along the slanted plane
    shell = mesh_object_from_box("shell", (-D / 2, -W / 2, 0.0), (D / 2, W / 2, H))
    hollow = mesh_object_from_box("hollow", (-D / 2 + WALL, -W / 2 + WALL, WALL), (D / 2 - WALL, W / 2 - WALL, H - WALL))
    boolean(shell, hollow)
    body = shell
    body.name = "SM_CigPackBody"
    body.data.name = body.name
    lid = body.copy()
    lid.data = body.data.copy()
    lid.name = lid.data.name = "SM_CigPackLid"
    bpy.context.scene.collection.objects.link(lid)
    split_by_plane(body, keep_above=False, offset=-SEAM / 2)
    split_by_plane(lid, keep_above=True, offset=SEAM / 2)
    # the inner collar: a U of card behind the front and along the sides, standing above the cut (held by the lid when closed)
    col = mesh_object_from_box("collar", (D / 2 - WALL - COLLAR, -W / 2 + WALL, 20 * MM), (D / 2 - WALL, W / 2 - WALL, COLLAR_TOP))
    for side in (-1, 1):
        y0 = side * (W / 2 - WALL)
        y1 = side * (W / 2 - WALL - COLLAR)
        part = mesh_object_from_box("collar_s", (D / 2 - WALL - 9 * MM, min(y0, y1), 20 * MM), (D / 2 - WALL - COLLAR, max(y0, y1), COLLAR_TOP - 2 * MM))
        boolean(col, part, "UNION")
    col.name = col.data.name = "SM_CigPackCollar"
    for o in (body, lid, col):
        o.data.materials.clear()
        o.data.materials.append(mats["Pack"])
        uv_pack(o)
    # the lid's origin on the hinge: the back top edge of the body's cut
    hinge = Vector((-D / 2, 0.0, CUT_BACK))
    lid.data.transform(__import__("mathutils").Matrix.Translation(-hinge))
    lid.location = hinge
    return [body, lid, col]


def tube(bm, x0, x1, r, cap0, cap1, mat_side, mat0=None, mat1=None, axis="X", rings=None):
    """A cylinder of SEGS sides from x0 to x1 along axis; extra rings at the given coordinates (material changes)."""
    xs = sorted(set([x0, x1] + list(rings or [])))
    loops = []
    for x in xs:
        ring = []
        for i in range(SEGS):
            a = 2 * math.pi * i / SEGS
            p = (x, r * math.cos(a), r * math.sin(a)) if axis == "X" else (r * math.cos(a), r * math.sin(a), x)
            ring.append(bm.verts.new(p))
        loops.append(ring)
    faces = []
    for k in range(len(xs) - 1):
        a, b = loops[k], loops[k + 1]
        for i in range(SEGS):
            j = (i + 1) % SEGS
            f = bm.faces.new((a[i], a[j], b[j], b[i]) if axis == "X" else (a[i], b[i], b[j], a[j]))
            f.material_index = mat_side(0.5 * (xs[k] + xs[k + 1]))
            faces.append(f)
    if cap0:
        f = bm.faces.new(list(reversed(loops[0])) if axis == "X" else loops[0][::-1])
        f.material_index = mat0
    if cap1:
        f = bm.faces.new(loops[-1] if axis == "X" else loops[-1][:])
        f.material_index = mat1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return loops


def uv_cylinders(obj, length, start=0.0, flip=False):
    """Around the cylinder u, along it v (0..1 over length from start, or back from start + length when flip: the filter's tipping
    line, the picture's top, always lies where the filter meets the paper); caps planar in the corner."""
    me = obj.data
    uvl = me.uv_layers.new(name="UV")
    for p in me.polygons:
        side = abs(p.normal.x) < 0.5 if obj.get("axis", "X") == "X" else abs(p.normal.z) < 0.5
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            if obj.get("axis", "X") == "X":
                along, a, b = co.x, co.y, co.z
            else:
                along, a, b = co.z, co.x, co.y
            if side:
                ang = math.atan2(b, a) / (2 * math.pi) % 1.0
                # the seam: faces that wrap from the last vertex to the first take u = 1, not 0
                cx = math.atan2(p.center.z if obj.get("axis", "X") == "X" else p.center.y,
                                p.center.y if obj.get("axis", "X") == "X" else p.center.x) / (2 * math.pi) % 1.0
                if cx > 0.75 and ang < 0.25:
                    ang += 1.0
                v = (along - start) / length
                uvl.data[li].uv = (ang, 1.0 - v if flip else v)
            else:
                uvl.data[li].uv = (0.5 + a / (4 * CIG_R), 0.5 + b / (4 * CIG_R))


def build_cigarettes(mats):
    out = []
    order = ["CigFilter", "CigPaper", "CigTobacco", "CigEmber"]
    ms = [mats[n] for n in order]
    F, P, T, E = 0, 1, 2, 3
    # the whole one in the pack: tobacco end at the bottom, the filter up
    bm = bmesh.new()
    tube(bm, 0.0, CIG_L, CIG_R, True, True, lambda z: P if z < CIG_L - FILTER else F, T, F, axis="Z", rings=[CIG_L - FILTER])
    o = new_object("SM_Cigarette", bm, ms)
    o["axis"] = "Z"
    uv_cylinders(o, FILTER, start=CIG_L - FILTER, flip=True)
    out.append(o)
    # in the hand, three parts along +X from the mouth
    bm = bmesh.new()
    tube(bm, 0.0, FILTER, CIG_R, True, False, lambda x: F, F)
    o = new_object("SM_CigFilter", bm, ms)
    uv_cylinders(o, FILTER)
    out.append(o)
    bm = bmesh.new()
    tube(bm, 0.0, 0.01, CIG_R, False, False, lambda x: P)
    o = new_object("SM_CigPaper", bm, ms)
    uv_cylinders(o, 0.01)
    out.append(o)
    # the burning end: a short closed cylinder, its far end rounded a little (ash), all ember material
    bm = bmesh.new()
    loops = tube(bm, 0.0, TIP * 0.75, CIG_R * 0.995, False, False, lambda x: E, rings=[])
    # a low dome: one fan from the last ring to the tip point
    tipv = bm.verts.new((TIP, 0.0, 0.0))
    for i in range(SEGS):
        j = (i + 1) % SEGS
        f = bm.faces.new((loops[-1][i], loops[-1][j], tipv))
        f.material_index = E
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = new_object("SM_CigTip", bm, ms)
    uv_cylinders(o, TIP)
    out.append(o)
    # the butt: the filter and a short charred stub, bent a little, lying on the ground (origin at its middle, under it)
    bm = bmesh.new()
    stub = 7.0 * MM
    tube(bm, 0.0, FILTER + stub, CIG_R, True, True, lambda x: F if x < FILTER else P, F, E, rings=[FILTER, FILTER + stub * 0.5])
    for v in bm.verts:
        x = v.co.x
        if x > FILTER:
            k = (x - FILTER) / stub
            v.co.z += -0.6 * MM * k
            v.co.y *= 1.0 - 0.18 * k        # squashed where it was put out
        v.co.x -= (FILTER + stub) / 2
        v.co.z += CIG_R * 0.97
    o = new_object("SM_CigButt", bm, ms)
    uv_cylinders(o, FILTER, start=-(FILTER + stub) / 2)
    out.append(o)
    return out


def rounded_box(bm, lo, hi, r, mat):
    """A box with its vertical edges bevelled (a plastic lighter's body)."""
    faces = box_faces(bm, lo, hi)
    for f in faces:
        f.material_index = mat
    vertical = [e for e in bm.edges if abs(e.verts[0].co.x - e.verts[1].co.x) < 1e-9 and abs(e.verts[0].co.y - e.verts[1].co.y) < 1e-9]
    bmesh.ops.bevel(bm, geom=vertical, offset=r, segments=3, affect="EDGES", profile=0.5)


def build_lighter(mats):
    ms = [mats["LighterBody"], mats["LighterMetal"], mats["Flame"]]
    bm = bmesh.new()
    rounded_box(bm, (-L_D / 2, -L_W / 2, 0.0), (L_D / 2, L_W / 2, L_BODY), 4.0 * MM, 0)
    # the metal hood over the top, a little narrower, open where the flame comes out
    top = bmesh.new()
    rounded_box(top, (-L_D / 2 + 0.4 * MM, -L_W / 2 + 0.4 * MM, L_BODY + 0.0003), (L_D / 2 - 0.4 * MM, L_W / 2 - 0.4 * MM, L_BODY + L_HOOD), 3.0 * MM, 1)
    me = bpy.data.meshes.new("tmp")
    top.to_mesh(me)
    top.free()
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)
    # the spark wheel behind the flame hole, across Y
    whl = bmesh.new()
    tube(whl, -3.5 * MM, 3.5 * MM, 3.0 * MM, True, True, lambda x: 1, 1, 1)
    for v in whl.verts:
        x, y, z = v.co
        v.co = Vector((z - 2.0 * MM, x, y + L_BODY + L_HOOD + 1.0 * MM))
    me = bpy.data.meshes.new("tmp")
    whl.to_mesh(me)
    whl.free()
    for p in me.polygons:
        p.material_index = 1
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)
    o = new_object("SM_Lighter", bm, ms)
    me = o.data
    uvl = me.uv_layers.new(name="UV")
    for p in me.polygons:
        a = max(range(3), key=lambda i: abs(p.normal[i]))
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            u_, v_ = [co[i] for i in range(3) if i != a]
            uvl.data[li].uv = (u_ / (L_W * 1.1) + 0.5, v_ / (L_BODY + L_HOOD))
    # the flame: a teardrop of unit height (1 cm) on its foot
    bm = bmesh.new()
    prof = [(0.0, 0.0), (0.12, 0.10), (0.20, 0.25), (0.22, 0.42), (0.17, 0.62), (0.09, 0.82), (0.0, 1.0)]
    rings = []
    for (r, z) in prof[1:-1]:
        rings.append([bm.verts.new((r * 0.01 * math.cos(2 * math.pi * i / 8), r * 0.01 * math.sin(2 * math.pi * i / 8), z * 0.01)) for i in range(8)])
    bot, tip = bm.verts.new((0, 0, 0)), bm.verts.new((0, 0, 0.01))
    for i in range(8):
        j = (i + 1) % 8
        bm.faces.new((rings[0][j], rings[0][i], bot))
        bm.faces.new((rings[-1][i], rings[-1][j], tip))
        for k in range(len(rings) - 1):
            bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    f = new_object("SM_LighterFlame", bm, [ms[2]])
    me = f.data
    uvl = me.uv_layers.new(name="UV")
    for p in me.polygons:
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            uvl.data[li].uv = (math.atan2(co.y, co.x) / (2 * math.pi) % 1.0, co.z / 0.01)
    return [o, f]


# ------------------------------------------------------------------ checks

def lint_overlaps(objs, gap=0.12 * MM):
    """Faces lying on other faces (z-fighting): parallel, facing the same way, within gap, not sharing a vertex."""
    bm = bmesh.new()
    for o in objs:
        me = o.data.copy()
        me.transform(o.matrix_world)
        bm.from_mesh(me)
        bpy.data.meshes.remove(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
    bm.faces.ensure_lookup_table()
    bm.normal_update()
    tree = BVHTree.FromBMesh(bm)
    bad = 0
    for f in bm.faces:
        if f.calc_area() < 1e-9:
            lint("zero-area face in %s" % ",".join(o.name for o in objs))
            bad += 1
            continue
        c = f.calc_center_median()
        mine = set(v.index for v in f.verts)
        for (loc, nor, idx, dist) in tree.find_nearest_range(c, gap):
            if idx == f.index:
                continue
            g = bm.faces[idx]
            if mine & set(v.index for v in g.verts):
                continue
            if g.normal.dot(f.normal) > 0.98 and abs((c - g.calc_center_median()).dot(f.normal)) < gap:
                # the point must lie inside the other face, not only near its edge
                if bmesh.geometry.intersect_face_point(g, c):
                    bad += 1
                    if bad <= 4:
                        print("  overlap at %.2f %.2f %.2f mm, normal %.2f %.2f %.2f" % (c.x / MM, c.y / MM, c.z / MM, *f.normal))
                    break
    bm.free()
    return bad


def lint_uvs(o):
    me = o.data
    if not me.uv_layers:
        lint("%s: no UVs" % o.name)
        return
    uvl = me.uv_layers[0]
    bad = 0
    for p in me.polygons:
        uvs = [Vector(uvl.data[li].uv) for li in p.loop_indices]
        area = 0.0
        for i in range(1, len(uvs) - 1):
            area += abs((uvs[i] - uvs[0]).cross(uvs[i + 1] - uvs[0])) / 2
        if area < 1e-10 and p.area > 1e-8:
            bad += 1
            print("  flat UV on %s at %s normal %s" % (o.name, tuple(round(c / MM, 2) for c in p.center), tuple(round(c, 2) for c in p.normal)))
        if any(u.x < -0.001 or u.x > 1.001 or u.y < -0.001 or u.y > 1.001 for u in uvs) and o.name.startswith("SM_CigPack"):
            bad += 1
    if bad:
        lint("%s: %d faces with zero-area or out-of-atlas UVs" % (o.name, bad))


def uv_dump(objs):
    """The UV polygons of the pack's parts (export/pack_uv.json): tools/smoking_pack.py --uv draws them over the atlas."""
    import json
    out = {}
    for o in objs:
        uvl = o.data.uv_layers[0]
        out[o.name] = [[list(uvl.data[li].uv) for li in p.loop_indices] for p in o.data.polygons]
    with open(os.path.join(EXPORT, "pack_uv.json"), "w") as f:
        json.dump(out, f)


def main():
    reset()
    os.makedirs(EXPORT, exist_ok=True)
    mats = {n: material(n, c) for n, c in (
        ("Pack", (0.8, 0.75, 0.6)), ("CigFilter", (0.8, 0.5, 0.2)), ("CigPaper", (0.95, 0.95, 0.93)),
        ("CigTobacco", (0.45, 0.3, 0.15)), ("CigEmber", (0.4, 0.38, 0.36)),
        ("LighterBody", (0.9, 0.4, 0.05)), ("LighterMetal", (0.7, 0.7, 0.72)), ("Flame", (1.0, 0.7, 0.3)))}
    pack = build_pack(mats)
    cigs = build_cigarettes(mats)
    lighter = build_lighter(mats)
    # the ten in the pack, as the game puts them (UE: SmokingKit::CigaretteSlot): for the overlap check only
    probes = []
    for row in range(2):
        for col in range(5):
            p = cigs[0].copy()
            p.data = cigs[0].data.copy()
            p.location = Vector((D / 2 - WALL - COLLAR - GAP - PITCH * (row + 0.5), -W / 2 + WALL + COLLAR + GAP + PITCH * (col + 0.5), WALL + 0.05 * MM))
            bpy.context.scene.collection.objects.link(p)
            probes.append(p)
    # checks: no faces on faces, inside a part and between the parts that sit together
    paper, tip = cigs[2].copy(), cigs[3].copy()
    for p_ in (paper, tip):
        p_.data = p_.data.copy()
        bpy.context.scene.collection.objects.link(p_)
    paper.location.x, paper.scale.x = FILTER, (CIG_L - FILTER - TIP) / 0.01
    tip.location.x = CIG_L - TIP
    probes_hand = [paper, tip]
    bpy.context.view_layer.update()
    for group in ([pack[0]], [pack[1]], [pack[2]], pack, pack + probes, [cigs[1]] + probes_hand, [cigs[4]], lighter[:1]):
        n = lint_overlaps(group)
        if n:
            lint("%d overlapping faces in %s" % (n, "+".join(sorted(set(o.name.split(".")[0] for o in group)))))
    # the cigarettes must not cut into the walls or the collar: their distance to the box's inside
    inside_y = W / 2 - WALL - COLLAR
    inside_x_front, inside_x_back = D / 2 - WALL - COLLAR, -D / 2 + WALL
    for p in probes:
        x, y = p.location.x, p.location.y
        if x + CIG_R > inside_x_front or x - CIG_R < inside_x_back or abs(y) + CIG_R > inside_y:
            lint("cigarette at %.4f %.4f cuts into the pack" % (x, y))
    if CIG_L + WALL > H - WALL:
        lint("cigarettes taller than the inside")
    for p in probes + probes_hand:
        bpy.data.objects.remove(p)
    for o in pack + cigs + lighter:
        lint_uvs(o)
    uv_dump(pack)
    print("PACK outer W %.1f D %.1f H %.1f mm, cut %.0f..%.0f mm" % (W / MM, D / MM, H / MM, CUT_FRONT / MM, CUT_BACK / MM))
    for o in pack + cigs + lighter:
        print("MESH %s: %d tris" % (o.name, sum(len(p.vertices) - 2 for p in o.data.polygons)))
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=os.path.join(EXPORT, "smoking_kit.glb"), export_format="GLB", use_selection=True,
                              export_apply=True, export_cameras=False, export_lights=False, export_materials="EXPORT")
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(EXPORT, "smoking_kit.blend"), compress=True)
    print("LINT total %d" % len(LINTS))
    sys.exit(1 if LINTS else 0)


main()
