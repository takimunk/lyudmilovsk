"""Helper (not a building type): a perforated cement breeze-block screen (cobogo) with REAL holes as geometry.

cobogo_screen(m, F, u0, u1, z0, z1, ...) fills the rectangle u0..u1 x z0..z1 of the facade frame F (a geom.Facade:
u along the wall, y out of the wall, z up) with square cells; every cell is one block with a hole through it:
alternately a diamond hole and a square hole (16 triangles a block: 8 for the front ring, 8 for the hole's tunnel).
The screen plane is at y = y_front (front face) and y_front - thick (back face); the back face has no ring (it
cannot be seen from outside, and the tunnel walls close the hole). Blocks are not shared: skip(uc, zc) -> True
leaves a cell empty (door openings). Used by cobogo_house and pilotis_curve_slab.
"""
import geom


def _face(m, mat, pts, want):
    n = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[2], pts[0]))
    if sum(n[i] * want[i] for i in range(3)) < 0:
        pts = pts[::-1]
    m.face(mat, pts)


def cobogo_screen(m, F, u0, u1, z0, z1, y_front=0.0, thick=0.14, block=0.6, mat="CT_Cobogo", skip=None, hole=0.31):
    cols = max(1, int(round((u1 - u0) / block)))
    rows = max(1, int(round((z1 - z0) / block)))
    du, dz = (u1 - u0) / cols, (z1 - z0) / rows
    n_out = (F.n[0], F.n[1], 0.0)
    yb = y_front - thick
    count = 0
    for i in range(cols):
        for j in range(rows):
            a, b = u0 + i * du, u0 + (i + 1) * du
            c, d = z0 + j * dz, z0 + (j + 1) * dz
            uc, zc = (a + b) / 2, (c + d) / 2
            if skip and skip(uc, zc):
                continue
            count += 1
            P = lambda u, z, y=y_front: F.to_building((u, y, z))
            A, B, C, D = P(a, c), P(b, c), P(b, d), P(a, d)
            h = hole * min(du, dz)
            if (i + j) % 2 == 0:                     # diamond hole
                Bt, R, T, L = P(uc, zc - h), P(uc + h, zc), P(uc, zc + h), P(uc - h, zc)
                ring = [(A, B, Bt), (B, C, R), (C, D, T), (D, A, L), (A, Bt, L), (B, R, Bt), (C, T, R), (D, L, T)]
                hole_pts = [(uc, zc - h), (uc + h, zc), (uc, zc + h), (uc - h, zc)]
            else:                                    # square hole
                s = h * 0.82
                a1, b1, c1, d1 = P(uc - s, zc - s), P(uc + s, zc - s), P(uc + s, zc + s), P(uc - s, zc + s)
                ring = [(A, B, b1, a1), (B, C, c1, b1), (C, D, d1, c1), (D, A, a1, d1)]
                hole_pts = [(uc - s, zc - s), (uc + s, zc - s), (uc + s, zc + s), (uc - s, zc + s)]
            for tri in ring:
                _face(m, mat, list(tri), n_out)
            ctr = F.to_building((uc, y_front - thick / 2, zc))
            for k in range(4):
                p, q = hole_pts[k], hole_pts[(k + 1) % 4]
                quad = [F.to_building((p[0], y_front, p[1])), F.to_building((q[0], y_front, q[1])),
                        F.to_building((q[0], yb, q[1])), F.to_building((p[0], yb, p[1]))]
                cen = tuple(sum(v[t] for v in quad) / 4 for t in range(3))
                _face(m, mat, quad, tuple(ctr[t] - cen[t] for t in range(3)))
    return count
