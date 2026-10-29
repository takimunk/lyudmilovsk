"""Glitch checks for built game meshes (Dima's rule, 2026-10-29: check overlapping surfaces and texture glitches on
every 3D asset): run inside Blender on objects placed as the game places them.

  import kit_lint; problems = kit_lint.lint(objects, name="car")

Finds
  zfight    two faces facing the same way within 2 mm of each other and overlapping (they flicker in the game: a decal
            on its wall, a panel doubled, a box sunk flush into another)
  uv_zero   a textured face whose UVs have no area (the texture smears into a line)
  uv_metre  a tx_* face (metre UVs) whose UV size differs from its real size by more than 1.5x (stretched texture)
  uv_print  an ad_* face whose UV rectangle's proportions differ from the face's by more than 4 % (a squashed print)
Each finding names the objects, materials and a world position to look at.
"""
import math

import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import intersect_point_tri_2d, tessellate_polygon

GAP = 0.002          # m: same-facing faces closer than this flicker
MIN_AREA = 2e-5      # m2: ignore slivers smaller than this


def _faces(objs):
    out = []
    for o in objs:
        me = o.data
        uv = me.uv_layers.active.data if me.uv_layers.active else None
        mw = o.matrix_world
        for p in me.polygons:
            pts = [mw @ me.vertices[v].co for v in p.vertices]
            n = (mw.to_3x3() @ p.normal).normalized()
            area = p.area      # (the objects are placed unscaled)
            mat = me.materials[p.material_index].name if me.materials else ""
            uvs = [uv[i].uv.copy() for i in p.loop_indices] if uv else None
            out.append((o.name, mat, pts, n, area, uvs))
    return out


def _tris(pts):
    """The polygon's own triangles (a fan would leave a concave face, a wall round a window, and overlap its holes)."""
    if len(pts) == 3:
        return [tuple(pts)]
    p3 = [Vector((p.x, p.y, 0.0)) for p in pts]
    return [(pts[a], pts[b], pts[c]) for a, b, c in tessellate_polygon([p3])]


def _inside(p2, tris2):
    return any(intersect_point_tri_2d(p2, *t) for t in tris2)


def _overlap(fa, fb, n):
    """Do two coplanar faces share area? Sample one face's interior and test the samples in the other's triangles."""
    ax = n.orthogonal().normalized()
    ay = n.cross(ax)
    def flat(pts):
        return [Vector((p.dot(ax), p.dot(ay))) for p in pts]
    A, B = flat(fa), flat(fb)
    ta, tb = _tris(A), _tris(B)
    hits = 0
    for t in ta:
        c = (t[0] + t[1] + t[2]) / 3
        for s in (c, (c + t[0]) / 2, (c + t[1]) / 2, (c + t[2]) / 2):
            if _inside(s, tb):
                hits += 1
    for t in tb:
        c = (t[0] + t[1] + t[2]) / 3
        for s in (c, (c + t[0]) / 2, (c + t[1]) / 2, (c + t[2]) / 2):
            if _inside(s, ta):
                hits += 1
    return hits


def lint(objs, name="kit", limit=40):
    F = [f for f in _faces(objs) if f[4] > MIN_AREA]
    problems = []
    # same-facing faces near one another: a BVH of every face, each face looks a few mm along its normal both ways
    verts, polys = [], []
    for f in F:
        k = len(verts)
        verts += [tuple(p) for p in f[2]]
        polys.append(tuple(range(k, k + len(f[2]))))
    bvh = BVHTree.FromPolygons(verts, polys, all_triangles=False, epsilon=0.0)
    seen = set()
    for i, (on, mat, pts, n, area, _) in enumerate(F):
        c = sum(pts, Vector()) / len(pts)
        for j in {r[2] for r in bvh.find_nearest_range(c, max(math.sqrt(area), 0.05) + GAP) if r[2] is not None}:
            if j == i or (min(i, j), max(i, j)) in seen:
                continue
            on2, mat2, pts2, n2, area2, _ = F[j]
            if n.dot(n2) < 0.999:
                continue
            d = abs((pts2[0] - pts[0]).dot(n))
            if d >= GAP - 1e-5:
                continue
            seen.add((min(i, j), max(i, j)))
            if _overlap(pts, pts2, n):
                problems.append(("zfight", "%s/%s and %s/%s %.1f mm apart" % (on, mat, on2, mat2, d * 1000), c))
    for (on, mat, pts, n, area, uvs) in F:
        if not uvs or not (mat.startswith("tx_") or mat.startswith("ad_")):
            continue
        ua = abs(sum((uvs[0] - uvs[i]).cross(uvs[0] - uvs[i + 1]) for i in range(1, len(uvs) - 1)) / 2)
        c = sum(pts, Vector()) / len(pts)
        if ua < 1e-10:
            problems.append(("uv_zero", "%s/%s" % (on, mat), c))
            continue
        if mat.startswith("tx_"):
            r = math.sqrt(ua / area)
            if r > 1.5 or r < 1 / 1.5:
                problems.append(("uv_metre", "%s/%s UV %.2fx its size" % (on, mat, r), c))
        elif len(pts) == 4 and area > 1e-3:
            w3, h3 = (pts[1] - pts[0]).length, (pts[3] - pts[0]).length
            w2, h2 = (uvs[1] - uvs[0]).length, (uvs[3] - uvs[0]).length
            if h3 > 0 and h2 > 0 and abs((w3 / h3) / (w2 / h2) - 1) > 0.04:
                problems.append(("uv_print", "%s/%s face %.3f, print %.3f" % (on, mat, w3 / h3, w2 / h2), c))
    for kind, what, at in sorted(problems, key=lambda q: q[0] != "zfight")[:limit] + [q for q in problems if q[0] != "zfight"][:10]:
        print("KITLINT %s %-8s %s at (%.2f, %.2f, %.2f)" % (name, kind, what, at.x, at.y, at.z))
    kinds = {}
    for k, _, _ in problems:
        kinds[k] = kinds.get(k, 0) + 1
    print("KITLINT %s: %d faces, %s" % (name, len(F), ", ".join("%s %d" % kv for kv in sorted(kinds.items())) or "clean"))
    return problems
