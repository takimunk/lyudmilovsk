"""Helper (not a building type; the leading underscore hides it from build_city): closed thin sheets over a
parametric grid. Used by shell_market and wave_canopy."""
import math

from citykit import geom


def _cross_z(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def thick_sheet(m, f, nu, nv, thick, mat_top, mat_bot=None, mat_edge=None, edges="uUvV", flip=False):
    """A closed thin sheet. f(u, v) -> (x, y, z_top) for u, v in 0..1; thick(u, v) -> thickness measured straight
    down; the grid is nu x nv quads. The top faces up, the bottom down, `edges` (letters u=0, U=1, v=0, V=1)
    get an edge band facing outwards. Faces CCW from outside."""
    mat_bot = mat_bot or mat_top
    mat_edge = mat_edge or mat_top
    T = [[f(i / nu, j / nv) for j in range(nv + 1)] for i in range(nu + 1)]
    Bt = [[(p[0], p[1], p[2] - thick(i / nu, j / nv)) for j, p in enumerate(row)] for i, row in enumerate(T)]
    # orientation of the parametrisation seen from above
    c = _cross_z(T[0][0], T[nu][0], T[nu][nv]) + _cross_z(T[0][0], T[nu][nv], T[0][nv])
    up_ccw = (c > 0) != flip
    for i in range(nu):
        for j in range(nv):
            q = [T[i][j], T[i + 1][j], T[i + 1][j + 1], T[i][j + 1]]
            b = [Bt[i][j], Bt[i + 1][j], Bt[i + 1][j + 1], Bt[i][j + 1]]
            if up_ccw:
                m.face(mat_top, q)
                m.face(mat_bot, b[::-1])
            else:
                m.face(mat_top, q[::-1])
                m.face(mat_bot, b)

    def edge(pa, pb, ba, bb, inner_mid):
        quad = [pa, pb, bb, ba]
        n = geom.cross(geom.sub(pb, pa), geom.sub(ba, pa))
        mid = tuple((pa[k] + pb[k]) / 2 for k in range(3))
        out = geom.sub(mid, inner_mid)
        # quad normal: (pb-pa) x (ba-pa)
        if sum(n[k] * out[k] for k in range(3)) < 0:
            quad = [pa, ba, bb, pb]
        m.face(mat_edge, quad)

    if "u" in edges:
        for j in range(nv):
            im = tuple((T[1][j][k] + T[1][j + 1][k]) / 2 for k in range(3))
            edge(T[0][j], T[0][j + 1], Bt[0][j], Bt[0][j + 1], im)
    if "U" in edges:
        for j in range(nv):
            im = tuple((T[nu - 1][j][k] + T[nu - 1][j + 1][k]) / 2 for k in range(3))
            edge(T[nu][j], T[nu][j + 1], Bt[nu][j], Bt[nu][j + 1], im)
    if "v" in edges:
        for i in range(nu):
            im = tuple((T[i][1][k] + T[i + 1][1][k]) / 2 for k in range(3))
            edge(T[i][0], T[i + 1][0], Bt[i][0], Bt[i + 1][0], im)
    if "V" in edges:
        for i in range(nu):
            im = tuple((T[i][nv - 1][k] + T[i + 1][nv - 1][k]) / 2 for k in range(3))
            edge(T[i][nv], T[i + 1][nv], Bt[i][nv], Bt[i + 1][nv], im)
    return T, Bt
