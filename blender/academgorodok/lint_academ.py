"""Mesh lint for the Academgorodok parts (Dima's rule: check overlaps and UVs before delivery):
coplanar overlapping faces facing the same way (z-fighting, within 2 mm), zero-area faces, and faces
whose UVs are missing or stretched. Runs inside Blender on the built objects:

    build_academgorodok.py -- --no-render --lint     (lint after the build, before the export)

lint(parts) prints "ACADEM lint <part>: ..." lines and returns the number of z-fighting pairs.
"""
import math
from collections import defaultdict

PLANE_TOL = 0.002        # m: faces this close to one plane are coplanar
MIN_OVERLAP = 2e-4       # m^2: smaller overlaps are edge contacts
CELL = 0.5               # m: 2D grid for the pair search


def _clip(subject, clipper):
    """Sutherland-Hodgman: subject polygon clipped by a convex clipper (both CCW lists of (x, y))."""
    out = subject
    n = len(clipper)
    for i in range(n):
        a, b = clipper[i], clipper[(i + 1) % n]
        inp, out = out, []
        if not inp:
            break
        def inside(p):
            return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]) >= -1e-9
        def cut(p, q):
            dx, dy = q[0] - p[0], q[1] - p[1]
            den = (b[0] - a[0]) * dy - (b[1] - a[1]) * dx
            if abs(den) < 1e-15:
                return q
            t = ((b[0] - a[0]) * (a[1] - p[1]) - (b[1] - a[1]) * (a[0] - p[0])) / den
            return (p[0] + t * dx, p[1] + t * dy)
        for j in range(len(inp)):
            p, q = inp[j - 1], inp[j]
            if inside(q):
                if not inside(p):
                    out.append(cut(p, q))
                out.append(q)
            elif inside(p):
                out.append(cut(p, q))
    return out


def _area2(poly):
    return sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
               for i in range(len(poly))) / 2.0


def _basis(n):
    a = (1.0, 0.0, 0.0) if abs(n[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = (n[1] * a[2] - n[2] * a[1], n[2] * a[0] - n[0] * a[2], n[0] * a[1] - n[1] * a[0])
    lu = math.sqrt(sum(c * c for c in u))
    u = tuple(c / lu for c in u)
    v = (n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2], n[0] * u[1] - n[1] * u[0])
    return u, v


def lint_object(ob, max_examples=6):
    """(z-fight pairs, zero-area faces, bad-UV faces, examples) of one Blender mesh object."""
    me = ob.data
    mw = ob.matrix_world
    mats = [s.material.name if s.material else "?" for s in ob.material_slots]
    uv = me.uv_layers.active.data if me.uv_layers.active else None
    verts = [mw @ v.co for v in me.vertices]
    buckets = defaultdict(list)
    zero = bad_uv = 0
    faces = []
    for p in me.polygons:
        pts = [verts[i] for i in p.vertices]
        n = (mw.to_3x3() @ p.normal).normalized()
        area = p.area
        if area < 1e-7:
            zero += 1
            continue
        if uv is not None:
            c = [uv[li].uv for li in p.loop_indices]
            ua = abs(sum(c[i].x * c[(i + 1) % len(c)].y - c[(i + 1) % len(c)].x * c[i].y for i in range(len(c))) / 2)
            if area > 1e-3 and ua < 1e-9:
                bad_uv += 1
        d = n.dot(pts[0])
        key = (round(n.x * 40), round(n.y * 40), round(n.z * 40), round(d / PLANE_TOL))
        faces.append((p.index, n, d, pts, mats[p.material_index] if p.material_index < len(mats) else "?"))
        buckets[key].append(len(faces) - 1)
    pairs = 0
    examples = []
    by_pair = defaultdict(lambda: [0, 0.0, None])
    seen = set()
    for key, idx in buckets.items():
        # the plane's neighbours within the tolerance share the search
        cand = list(idx)
        for dd in (-1, 1):
            cand += buckets.get(key[:3] + (key[3] + dd,), [])
        if len(cand) < 2:
            continue
        n = faces[idx[0]][1]
        u, v = _basis(n)
        grid = defaultdict(list)
        poly2 = {}
        for fi in cand:
            pts = faces[fi][3]
            q = [(p.dot(u), p.dot(v)) for p in pts]
            if _area2(q) < 0:
                q = q[::-1]
            poly2[fi] = q
            xs, ys = [a for a, _ in q], [b for _, b in q]
            for gx in range(int(math.floor(min(xs) / CELL)), int(math.floor(max(xs) / CELL)) + 1):
                for gy in range(int(math.floor(min(ys) / CELL)), int(math.floor(max(ys) / CELL)) + 1):
                    grid[(gx, gy)].append(fi)
        for cell in grid.values():
            for i in range(len(cell)):
                for j in range(i + 1, len(cell)):
                    a, b = cell[i], cell[j]
                    if a == b or (min(a, b), max(a, b)) in seen:
                        continue
                    seen.add((min(a, b), max(a, b)))
                    if faces[a][1].dot(faces[b][1]) < 0.999 or abs(faces[a][2] - faces[b][2]) > PLANE_TOL:
                        continue
                    # really in one plane: every corner of each within the tolerance of the other's plane
                    # (not two faces crossing at a slight angle)
                    na, da, nb, db = faces[a][1], faces[a][2], faces[b][1], faces[b][2]
                    if any(abs(na.dot(p) - da) > PLANE_TOL for p in faces[b][3]) or \
                            any(abs(nb.dot(p) - db) > PLANE_TOL for p in faces[a][3]):
                        continue
                    ov = _clip(poly2[a], poly2[b])
                    if len(ov) >= 3 and abs(_area2(ov)) > MIN_OVERLAP:
                        pairs += 1
                        bp = by_pair[tuple(sorted((faces[a][4], faces[b][4])))]
                        bp[0] += 1
                        bp[1] += abs(_area2(ov))
                        if bp[2] is None:
                            c = faces[a][3][0]
                            bp[2] = (round(c.x, 2), round(c.y, 2), round(c.z, 2))
                        if len(examples) < max_examples:
                            c = faces[a][3][0]
                            examples.append("%s/%s at (%.2f, %.2f, %.2f) %.4f m2" % (
                                faces[a][4], faces[b][4], c.x, c.y, c.z, abs(_area2(ov))))
    return pairs, zero, bad_uv, examples, by_pair


def lint(parts):
    total = 0
    for name, ob in parts.items():
        pairs, zero, bad_uv, ex, by_pair = lint_object(ob)
        total += pairs
        print("ACADEM lint %s: %d z-fight pairs, %d zero-area, %d without UVs" % (name, pairs, zero, bad_uv))
        for (ma, mb), (cnt, area, at) in sorted(by_pair.items(), key=lambda kv: -kv[1][1])[:12]:
            print("ACADEM lint   %s / %s: %d pairs, %.2f m2, e.g. at %s" % (ma, mb, cnt, area, at))
    print("ACADEM lint total z-fight pairs: %d" % total)
    return total
