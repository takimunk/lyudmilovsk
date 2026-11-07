"""Small helpers shared by the constructivist modules (commune_slab, workers_club): walls with a different inner
material, window cards on a wall line, horizontal polygons. Plain Python."""
import math

from citykit import Facade, Mesh, wall_with_holes, reveal, triangulate, geom


def wall(B, p0, p1, z0, z1, mat, holes=(), depth=0.4, mat_in=None, back=True, rev_mat=None):
    """Like B.facade, but the inner face (and the reveals) may have another material (CT_Interior for halls)."""
    F = Facade(p0, p1)
    m = Mesh()
    holes = list(holes)
    wall_with_holes(m, mat, F.length, z0, z1, holes)
    for (u0, u1, h0, h1) in holes:
        reveal(m, rev_mat or mat_in or mat, u0, u1, h0, h1, depth)
    if back:
        inner = Mesh()
        wall_with_holes(inner, mat_in or mat, F.length, z0, z1, holes)
        inner.verts = [(u, -depth, z) for (u, _, z) in inner.verts]
        inner.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in inner.faces]
        m.extend(inner)
    B.m.extend(F.apply(m))
    return F


def win(B, p0, p1, u0, u1, z0, z1, flat=None, kind=None, cols=1, rows=1, inset=0.18, frame=0.05):
    """A window card in the hole (u0..u1, z0..z1) of the wall line p0 -> p1 (outside on the right)."""
    L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
    d = ((p1[0] - p0[0]) / L, (p1[1] - p0[1]) / L)
    P = lambda u, z: (p0[0] + d[0] * u, p0[1] + d[1] * u, z)
    return B.window([P(u0, z0), P(u1, z0), P(u1, z1), P(u0, z1)], flat=flat, kind=kind, cols=cols, rows=rows,
                    inset=inset, frame=frame)


def hface(m, mat, poly, z, up=True):
    """Horizontal polygon (any simple polygon, either winding) facing up or down."""
    area = sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
               for i in range(len(poly)))
    if area < 0:
        poly = poly[::-1]
    for a, b, c in triangulate(poly):
        tri = [poly[a], poly[b], poly[c]]
        if not up:
            tri = tri[::-1]
        m.face(mat, [(x, y, z) for x, y in tri])


def rect(x0, x1, y0, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
