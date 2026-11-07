"""Shared helpers of the restaurant's tableware build (blender/restaurant/tableware.py, docs: design/restaurant/tableware.json).

Reuses the diner's pipeline instead of copying it:
  diner_common (C)      lathe, box, tube, metre UVs, the glTF writer
  diner tableware (D)   loop_stack, dome, cap, ellipsoid, pillow, sweep, spline, extrude, cells, clip_poly, fork, spoon,
                        tine_z / spoon_zin / TINE_X / LOAD_Z (the fork and spoon frames), the bite cutter
The diner's tableware.py runs its build at import, so it is loaded from source without the final main() call.

Slots are this module's RS_* (tableware_mats.py); M(name) is a slot's index. Parts are bmeshes; cut() takes bites out of a
closed part with the EXACT boolean and paints the cut faces; export() writes <OUT>/<name>.glb (+ <name>_Glass.glb).
"""
import math
import os
import random
import sys
import types

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DINER = os.path.join(ROOT, "blender", "diner")
for p in (HERE, DINER):
    if p not in sys.path:
        sys.path.insert(0, p)
import diner_common as C                     # noqa: E402
from tableware_mats import MAT_SPEC          # noqa: E402

OUT = os.path.join(ROOT, "design", "restaurant", "game", "tableware")
PI = math.pi
MATS = list(MAT_SPEC)


def _load_diner_tableware():
    path = os.path.join(DINER, "tableware.py")
    with open(path) as f:
        lines = f.read().rstrip().splitlines()
    assert lines[-1].strip() == "main()", "diner tableware.py no longer ends in main()"
    mod = types.ModuleType("diner_tableware")
    mod.__file__ = path
    exec(compile("\n".join(lines[:-1]) + "\n", path, "exec"), mod.__dict__)
    return mod


D = _load_diner_tableware()


def M(name):
    return MATS.index(name)


# ---- small bmesh tools -------------------------------------------------------------------------------------------------

def new(fn=None, *a, **k):
    bm = bmesh.new()
    if fn:
        fn(bm, *a, **k)
    return bm


def merge(dst, src, free=True):
    """Copies src's faces into dst."""
    vm = {v: dst.verts.new(v.co) for v in src.verts}
    for f in src.faces:
        try:
            dst.faces.new([vm[v] for v in f.verts]).material_index = f.material_index
        except ValueError:
            pass
    if free:
        src.free()
    return dst


def xform(bm, loc=(0, 0, 0), rz=0.0, rx=0.0, ry=0.0, scale=None, mat=None):
    """Rotates (x, then y, then z, about the origin), scales, then moves the bmesh; returns it."""
    m = mat or (Matrix.Translation(loc) @ Matrix.Rotation(rz, 4, "Z") @ Matrix.Rotation(ry, 4, "Y") @ Matrix.Rotation(rx, 4, "X"))
    if scale is not None and mat is None:
        s = scale if isinstance(scale, (tuple, list)) else (scale, scale, scale)
        m = m @ Matrix.Diagonal((s[0], s[1], s[2], 1.0))
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    return bm


def setmat(bm, mi):
    for f in bm.faces:
        f.material_index = mi
    return bm


def paint(bm, fn):
    """fn(centre, normal, material_index) -> material_index, per face."""
    bm.normal_update()
    for f in bm.faces:
        f.material_index = fn(f.calc_center_median(), f.normal, f.material_index)
    return bm


def bbox(bm):
    xs = [v.co for v in bm.verts]
    return (Vector((min(p.x for p in xs), min(p.y for p in xs), min(p.z for p in xs))),
            Vector((max(p.x for p in xs), max(p.y for p in xs), max(p.z for p in xs))))


def tris(bm):
    return sum(len(f.verts) - 2 for f in bm.faces)


def fix_normals(bm):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bm


def lowest_gap(bm, floor):
    return min(v.co.z - floor(v.co.x, v.co.y) for v in bm.verts)


def rest(bm, floor, gap=0.0008):
    """Moves the part up or down so its lowest point is gap over floor(x, y)."""
    d = gap - lowest_gap(bm, floor)
    bmesh.ops.translate(bm, vec=(0, 0, d), verts=bm.verts)
    return bm


def conform(bm, floor, gap=0.0008):
    """Lifts every vertex that sinks below floor(x, y) + gap onto it (soft food squashed into a dish)."""
    for v in bm.verts:
        z = floor(v.co.x, v.co.y) + gap
        if v.co.z < z:
            v.co.z = z
    return bm


def skin(bm, rings, matfn, closed=False):
    """Skins rings (each a list of points, or one point = an apex); matfn(band, seg) -> material index.
    Going outward a band faces down, going inward it faces up (as D.loop_stack)."""
    vr = [[bm.verts.new(p) for p in r] for r in rings]
    n = max(len(r) for r in vr)
    segs = list(zip(vr, vr[1:])) + ([(vr[-1], vr[0])] if closed else [])
    for k, (a, b) in enumerate(segs):
        for i in range(n):
            j = (i + 1) % n
            if len(a) == 1 and len(b) == 1:
                continue
            if len(a) == 1:
                f = (a[0], b[j], b[i])
            elif len(b) == 1:
                f = (a[i], a[j], b[0])
            else:
                f = (a[i], a[j], b[j], b[i])
            bm.faces.new(f).material_index = matfn(k, i)
    return bm


def ring_tube(bm, path, r, mi, seg=6, closed=True):
    """A tube round a closed planar-ish path (list of points): rings in the plane of the path's outward normal and z."""
    n = len(path)
    rings = []
    for i in range(n):
        p = Vector(path[i])
        t = (Vector(path[(i + 1) % n]) - Vector(path[i - 1])).normalized()
        out = t.cross(Vector((0, 0, 1))).normalized()
        if out.length < 0.5:
            out = Vector((1, 0, 0))
        up = out.cross(t).normalized()
        rings.append([tuple(p + r * (math.cos(2 * PI * s / seg) * out + math.sin(2 * PI * s / seg) * up)) for s in range(seg)])
    D.loop_stack(bm, rings, mi, closed=closed)
    return bm


def torus(bm, c, R, r, mi, seg=14, tseg=5, tilt=(0.0, 0.0)):
    t = new()
    path = [(R * math.cos(2 * PI * i / seg), R * math.sin(2 * PI * i / seg), 0.0) for i in range(seg)]
    ring_tube(t, path, r, mi, tseg)
    xform(t, loc=c, rx=tilt[0], ry=tilt[1])
    merge(bm, t)
    return bm


def ribbon(bm, pts, nrms, w, t, mi):
    """A flat strip along pts (tagliatelle): width w across, thickness t along nrms (the flat side's normal at each point)."""
    P = [Vector(p) for p in pts]
    n = len(P)
    rings = []
    for i in range(n):
        T = (P[min(i + 1, n - 1)] - P[max(i - 1, 0)]).normalized()
        N = Vector(nrms[i])
        N = (N - T * N.dot(T)).normalized()
        B = T.cross(N).normalized()
        rings.append([tuple(P[i] + B * (sx * w / 2) + N * (sy * t / 2)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
    D.loop_stack(bm, rings, mi, bottom=True, top=True)
    return bm


def sphere_blob(bm, c, R, mi, seg=14, rings=8, noise=None, sz=1.0):
    """A UV sphere, poles on z; noise(theta, phi) -> radius factor."""
    rr = [[(c[0], c[1], c[2] - R * sz * (noise(0, PI) if noise else 1))]]
    for k in range(1, rings):
        ph = PI - PI * k / rings
        ring = []
        for i in range(seg):
            th = 2 * PI * i / seg
            f = noise(th, ph) if noise else 1.0
            ring.append((c[0] + R * f * math.sin(ph) * math.cos(th), c[1] + R * f * math.sin(ph) * math.sin(th),
                         c[2] + R * sz * f * math.cos(ph)))
        rr.append(ring)
    rr.append([(c[0], c[1], c[2] + R * sz * (noise(0, 0) if noise else 1))])
    D.loop_stack(bm, rr, mi)
    return bm


def prism_layers(bm, poly, zs, side_mat, top_mi, bot_mi, zb=None):
    """An upright prism over a counter-clockwise outline with its walls split at the heights zs (a layered cake);
    side_mat(edge_index, band) -> material. zb(x, y): base height added to every level. Caps: fans round the centroid."""
    zb = zb or (lambda x, y: 0.0)
    n = len(poly)
    lv = [[bm.verts.new((x, y, zb(x, y) + z)) for x, y in poly] for z in zs]
    for b in range(len(zs) - 1):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((lv[b][i], lv[b][j], lv[b + 1][j], lv[b + 1][i])).material_index = side_mat(i, b)
    cx, cy = sum(p[0] for p in poly) / n, sum(p[1] for p in poly) / n
    c0 = bm.verts.new((cx, cy, zb(cx, cy) + zs[0]))
    c1 = bm.verts.new((cx, cy, zb(cx, cy) + zs[-1]))
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((c0, lv[0][j], lv[0][i])).material_index = bot_mi
        bm.faces.new((c1, lv[-1][i], lv[-1][j])).material_index = top_mi
    return bm


def slab_grid(bm, nx, ny, fpos, ztop, zbot, matfn):
    """A slab over a parametric grid: fpos(u, v) -> (x, y) for u, v in 0..1, top ztop(x, y), bottom zbot(x, y);
    matfn(i, j, top) -> material; walls round the edge."""
    T = [[None] * (ny + 1) for _ in range(nx + 1)]
    B = [[None] * (ny + 1) for _ in range(nx + 1)]
    for i in range(nx + 1):
        for j in range(ny + 1):
            x, y = fpos(i / nx, j / ny)
            T[i][j] = bm.verts.new((x, y, ztop(x, y)))
            B[i][j] = bm.verts.new((x, y, zbot(x, y)))
    for i in range(nx):
        for j in range(ny):
            bm.faces.new((T[i][j], T[i + 1][j], T[i + 1][j + 1], T[i][j + 1])).material_index = matfn(i, j, True)
            bm.faces.new((B[i][j], B[i][j + 1], B[i + 1][j + 1], B[i + 1][j])).material_index = matfn(i, j, False)
    edge = [(i, 0) for i in range(nx)] + [(nx, j) for j in range(ny)] + [(i, ny) for i in range(nx, 0, -1)] + [(0, j) for j in range(ny, 0, -1)]
    for k in range(len(edge)):
        (i0, j0), (i1, j1) = edge[k], edge[(k + 1) % len(edge)]
        bm.faces.new((B[i0][j0], B[i1][j1], T[i1][j1], T[i0][j0])).material_index = matfn(min(i0, nx - 1), min(j0, ny - 1), False)
    return bm


def sprinkle(bm_target, targets, pts, make, seed=0, z_top=0.3):
    """Drops garnish onto the food: for each (x, y) in pts a ray from z_top straight down onto the bmeshes targets;
    where it hits, make(bm_target, hit_point, normal, rng) adds a piece. Misses (eaten parts) add nothing."""
    acc = new()
    for t in targets:
        merge(acc, t.copy())
    if not acc.faces:
        acc.free()
        return 0
    bvh = BVHTree.FromBMesh(acc)
    rng = random.Random(seed)
    n = 0
    for x, y in pts:
        r = random.Random(rng.random())
        hit = bvh.ray_cast(Vector((x, y, z_top)), Vector((0, 0, -1)))
        if hit[0] is not None:
            make(bm_target, hit[0], hit[1], r)
            n += 1
    acc.free()
    return n


# ---- the boolean bites -------------------------------------------------------------------------------------------------

def _mats_for(me):
    for k in MATS:
        me.materials.append(bpy.data.materials.get(k) or bpy.data.materials.new(k))


def _obj(bm, name, coll=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    _mats_for(me)
    o = bpy.data.objects.new(name, me)
    (coll or bpy.context.scene.collection).objects.link(o)
    return o


def _overlap(a, b):
    return all(a[0][i] <= b[1][i] and b[0][i] <= a[1][i] for i in range(3))


def _convex(f):
    n = f.normal
    vs = [v.co for v in f.verts]
    m = len(vs)
    for i in range(m):
        if (vs[(i + 1) % m] - vs[i]).cross(vs[(i + 2) % m] - vs[(i + 1) % m]).dot(n) < -1e-12:
            return False
    return True


def fan_ngons(bm):
    """N-gons (cut faces) become fans round their middle (convex) or beauty triangles, so no stray slivers."""
    bm.normal_update()
    big = [f for f in bm.faces if len(f.verts) > 4]
    conv = [f for f in big if _convex(f)]
    rest_ = [f for f in big if not _convex(f)]
    if conv:
        bmesh.ops.poke(bm, faces=conv)
    if rest_:
        bmesh.ops.triangulate(bm, faces=rest_, quad_method="BEAUTY", ngon_method="BEAUTY")
    return bm


def cut(part, cutters, inner=None):
    """part minus the closed cutter bmeshes (EXACT boolean). Faces on the cut get material inner (default: the part's
    commonest). Returns a new bmesh (part is freed); empty when the bite took everything."""
    if not cutters or not part.faces:
        return part
    pb = bbox(part)
    use = [c for c in cutters if _overlap(bbox(c), pb)]
    if not use:
        return part
    counts = {}
    for f in part.faces:
        counts[f.material_index] = counts.get(f.material_index, 0) + 1
    mi_in = inner if inner is not None else max(counts, key=counts.get)
    coll = bpy.data.collections.new("cutters")
    bpy.context.scene.collection.children.link(coll)
    cobjs = [_obj(c.copy(), "cut", coll) for c in use]
    ob = _obj(part, "part")
    md = ob.modifiers.new("b", "BOOLEAN")
    md.operation, md.solver, md.operand_type, md.collection = "DIFFERENCE", "EXACT", "COLLECTION", coll
    md.use_self = True
    dg = bpy.context.evaluated_depsgraph_get()
    me2 = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    out = bmesh.new()
    out.from_mesh(me2)
    acc = new()
    for c in use:
        merge(acc, c.copy())
    bvh = BVHTree.FromBMesh(acc)
    out.normal_update()
    for f in out.faces:
        loc, nor, idx, dist = bvh.find_nearest(f.calc_center_median())
        if dist is not None and dist < 3e-5:
            f.material_index = mi_in
    acc.free()
    for o in [ob] + cobjs:
        me = o.data
        bpy.data.objects.remove(o)
        bpy.data.meshes.remove(me)
    bpy.data.meshes.remove(me2)
    bpy.data.collections.remove(coll)
    part.free()
    bmesh.ops.remove_doubles(out, verts=out.verts, dist=1e-6)
    fan_ngons(out)
    return out


def ball_cutter(c, r, sz=1.0, seg=14, rings=8):
    return sphere_blob(new(), c, r, 0, seg, rings, sz=sz)


def bite_cutter(x, y, r, z0=-0.02, z1=0.25, seed=0):
    """An upright toothed cylinder (the diner's bite shape)."""
    rr = random.Random(seed)
    n = 22
    pts = [(x + r * (1 + (0.07 if i % 2 else -0.04) + rr.uniform(-0.03, 0.03)) * math.cos(2 * PI * i / n),
            y + r * (1 + (0.07 if i % 2 else -0.04) + rr.uniform(-0.03, 0.03)) * math.sin(2 * PI * i / n)) for i in range(n)]
    bm = new()
    D.extrude(bm, pts, z0, z1, 0, 0, 0)
    return bm


def box_cutter(c, size, rz=0.0, ry=0.0, rx=0.0):
    bm = new()
    C.box(bm, (0, 0, 0), size, 0)
    return xform(bm, loc=c, rz=rz, ry=ry, rx=rx)


def halfspace_cutter(x0, rz=0.0, size=0.6, ry=0.0):
    """Removes everything at x > x0 (in a frame turned rz about z)."""
    bm = new()
    C.box(bm, (x0 + size / 2, 0, 0), (size, size, size), 0)
    return xform(bm, rz=rz, ry=ry)


def drop_bottom(bm, zmax):
    """Deletes the downward faces under zmax (a part's hidden underside, once its bites are taken)."""
    bm.normal_update()
    dead = [f for f in bm.faces if f.normal.z < -0.9 and f.calc_center_median().z < zmax]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    return bm


def cull_inside(bm, inside):
    """Deletes faces whose middle is inside(p) (bites out of thin open sheets, where a boolean can't)."""
    dead = [f for f in bm.faces if inside(f.calc_center_median())]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    return bm


# ---- vessels: inner surfaces for fitting food and for the checks -------------------------------------------------------

class Vessel:
    """The food-side surface of a turned (or oval) vessel: prof [(r, z)] from the centre outwards with z not decreasing,
    in the vessel's frame (origin at the bottom centre); r_max: the farthest food may reach; aspect: y scale of an oval."""

    def __init__(self, name, prof, fill_z, r_max, aspect=1.0):
        self.name, self.prof, self.fill_z, self.r_max, self.aspect = name, prof, fill_z, r_max, aspect

    def rho(self, x, y):
        return math.hypot(x, y / self.aspect)

    def z_in(self, r):
        return D.interp(self.prof, r)

    def r_at(self, z):
        """Inner radius at height z (the outermost profile point at or below z)."""
        best = 0.0
        for (r0, z0), (r1, z1) in zip(self.prof, self.prof[1:]):
            if z1 <= z:
                best = max(best, r1)
            elif z0 <= z < z1:
                best = max(best, r0 + (r1 - r0) * (z - z0) / (z1 - z0))
        return best

    def floor(self, x, y):
        """Inner surface height under (x, y) in the food frame (origin at the inner floor's centre)."""
        return self.z_in(self.rho(x, y)) - self.fill_z

    def check(self, bm, tol=0.0003):
        """(worst sink below the inner surface m, count of sinking verts, verts beyond r_max)."""
        worst, n, out = 0.0, 0, 0
        for v in bm.verts:
            x, y, z = v.co
            r = self.rho(x, y)
            if r > self.r_max:
                out += 1
                continue
            d = self.z_in(r) - self.fill_z - z
            if d > tol:
                n += 1
                worst = max(worst, d)
        return worst, n, out


def fill_xy(fill_prof, vessel, margin=0.0008, steps=10):
    """Scale table for a liquid column fill_prof [(r, h)] (h from the vessel's inner floor): at level t the game scales the
    fill by t in Z and by xy(t) in X and Y, so no ring of the column pokes through the wall: [[t, xy], ...]."""
    out = []
    for s in range(steps + 1):
        t = s / steps
        k = 1.0
        if t > 0:
            for r, h in fill_prof:
                if r < 1e-4:
                    continue
                R = vessel.r_at(vessel.fill_z + t * h) - margin
                k = min(k, R / r)
        else:
            r0, h0 = fill_prof[0]
            k = min(1.0, (vessel.r_at(vessel.fill_z) - margin) / max(r0, 1e-4))
        out.append([round(t, 3), round(max(0.05, k), 4)])
    return out


def fill_k(table, t):
    return D.interp([(a, b) for a, b in table], t)


# ---- export ------------------------------------------------------------------------------------------------------------

PICTURE = {"RS_T_MenuCover"}


def _object(bm, name):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    _mats_for(me)
    me.update()
    C.metre_uv(me)
    uv = me.uv_layers[0]
    pic = {i for i, m in enumerate(me.materials) if m.name in PICTURE}
    pic = {i for i in pic if any(p.material_index == i for p in me.polygons)}
    if pic:                                  # the picture: 0..1 over its faces' extent, u along x, v along y (seen from above)
        pv = [me.vertices[me.loops[li].vertex_index].co for p in me.polygons if p.material_index in pic for li in p.loop_indices]
        x0, x1 = min(p.x for p in pv), max(p.x for p in pv)
        y0, y1 = min(p.y for p in pv), max(p.y for p in pv)
        for p in me.polygons:
            if p.material_index in pic:
                for li in p.loop_indices:
                    co = me.vertices[me.loops[li].vertex_index].co
                    uv.data[li].uv = ((co.x - x0) / (x1 - x0), (co.y - y0) / (y1 - y0))
    used = sorted({p.material_index for p in me.polygons})
    keep = [me.materials[i] for i in used]
    remap = {old: k for k, old in enumerate(used)}
    idx = [remap[p.material_index] for p in me.polygons]
    me.materials.clear()
    for m in keep:
        me.materials.append(m)
    for p, i in zip(me.polygons, idx):
        p.material_index = i
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    return o


SMOOTH = {k for k in MATS if k.startswith("RS_F_")} | {"RS_T_China", "RS_T_Amber", "RS_T_ChinaBlack", "RS_T_Glaze",
                                                       "RS_T_GlazeRim", "RS_T_Steel", "RS_T_Glass", "RS_T_GlassBase",
                                                       "RS_T_Wicker", "RS_T_WickerDark", "RS_T_Lacquer", "RS_T_Bamboo",
                                                       "RS_T_Linen", "RS_T_LinenStripe", "RS_T_Gold"}
FLAT = set()                                   # mesh names shaded flat throughout (the faceted glass)
TRIS = {}


def export(bm, name, out=None, crease=None):
    """Writes <out>/<name>.glb, and <name>_Glass.glb with the RS_T_Glass faces; frees bm; returns the triangle count."""
    out = out or OUT
    os.makedirs(out, exist_ok=True)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bm.normal_update()
    D.crease(bm, crease or (72 if name.startswith("SM_RF_") else 38))
    gi = M("RS_T_Glass")
    glass = [f for f in bm.faces if f.material_index == gi]
    if glass:
        g = bm.copy()
        bmesh.ops.delete(g, geom=[f for f in g.faces if f.material_index != gi], context="FACES")
        og = _object(g, name + "_Glass")
        g.free()
        for p in og.data.polygons:
            p.use_smooth = name + "_Glass" not in FLAT
        TRIS[name + "_Glass"] = sum(len(p.vertices) - 2 for p in og.data.polygons)
        C._write(og, os.path.join(out, name + "_Glass.glb"))
        me = og.data
        bpy.data.objects.remove(og)
        bpy.data.meshes.remove(me)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index == gi], context="FACES")
    o = _object(bm, name)
    bm.free()
    for p in o.data.polygons:
        m = o.data.materials[p.material_index]
        p.use_smooth = bool(m and m.name in SMOOTH) and name not in FLAT
    t = sum(len(p.vertices) - 2 for p in o.data.polygons)
    TRIS[name] = t
    C._write(o, os.path.join(out, name + ".glb"))
    me = o.data
    bpy.data.objects.remove(o)
    bpy.data.meshes.remove(me)
    print("RS", name, t, "tris", "+glass" if glass else "", flush=True)
    return t
