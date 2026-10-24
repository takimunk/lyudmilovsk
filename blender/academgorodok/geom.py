"""Pure-Python mesh accumulation for the Academgorodok build (no bpy here, so it can be tested
with plain python3).

A Mesh collects triangles/quads per material slot with metre-scale UVs. Facades are built in a
facade frame (u along the wall, y out of the wall, z up) and mapped into the building frame by a
Frame. bpy turns a Mesh into an object in build_academgorodok.py.
"""
import math


class Mesh:
    def __init__(self):
        self.verts = []          # (x, y, z)
        self.uvs = []            # per face-corner, parallel to the corners in self.faces
        self.faces = []          # (material, [vertex indices], [uv per corner])

    def face(self, mat, pts, uvs=None):
        """pts: 3+ points, counter-clockwise seen from the side the face points to."""
        if uvs is None:
            uvs = auto_uv(pts)
        base = len(self.verts)
        self.verts.extend(pts)
        self.faces.append((mat, list(range(base, base + len(pts))), list(uvs)))

    def extend(self, other):
        base = len(self.verts)
        self.verts.extend(other.verts)
        for mat, idx, uv in other.faces:
            self.faces.append((mat, [i + base for i in idx], uv))

    def transformed(self, fn):
        m = Mesh()
        m.verts = [fn(p) for p in self.verts]
        m.faces = [(mat, list(i), list(uv)) for mat, i, uv in self.faces]
        return m

    def tri_count(self):
        return sum(len(i) - 2 for _, i, _ in self.faces)


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    l = math.sqrt(a[0] ** 2 + a[1] ** 2 + a[2] ** 2) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)


def auto_uv(pts):
    """Box projection in metres by the face normal (1 UV unit = 1 m)."""
    n = norm(cross(sub(pts[1], pts[0]), sub(pts[2], pts[0])))
    ax = max(range(3), key=lambda i: abs(n[i]))
    if ax == 2:
        return [(p[0], p[1]) for p in pts]
    if ax == 0:
        return [(p[1] * (1 if n[0] > 0 else -1), p[2]) for p in pts]
    return [(p[0] * (-1 if n[1] > 0 else 1), p[2]) for p in pts]


def box(m, mat, x0, x1, y0, y1, z0, z1, faces="all"):
    """Axis-aligned box; faces: 'all' or a string of letters among xXyYzZ to keep (x = -x side)."""
    P = lambda x, y, z: (x, y, z)
    sides = {
        "x": [P(x0, y1, z0), P(x0, y0, z0), P(x0, y0, z1), P(x0, y1, z1)],
        "X": [P(x1, y0, z0), P(x1, y1, z0), P(x1, y1, z1), P(x1, y0, z1)],
        "y": [P(x0, y0, z0), P(x1, y0, z0), P(x1, y0, z1), P(x0, y0, z1)],
        "Y": [P(x1, y1, z0), P(x0, y1, z0), P(x0, y1, z1), P(x1, y1, z1)],
        "z": [P(x0, y1, z0), P(x1, y1, z0), P(x1, y0, z0), P(x0, y0, z0)],
        "Z": [P(x0, y0, z1), P(x1, y0, z1), P(x1, y1, z1), P(x0, y1, z1)],
    }
    for k, pts in sides.items():
        if faces == "all" or k in faces:
            m.face(mat, pts)


def prism(m, mat, poly, z0, z1, top=True, bottom=False, side_mat=None):
    """Vertical prism over a CCW polygon [(x, y)]."""
    n = len(poly)
    for i in range(n):
        (ax, ay), (bx, by) = poly[i], poly[(i + 1) % n]
        m.face(side_mat or mat, [(ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1)])
    if top:
        m.face(mat, [(x, y, z1) for x, y in poly])
    if bottom:
        m.face(mat, [(x, y, z0) for x, y in reversed(poly)])


class Frame:
    """2D placement + yaw (+ optional mirror of the local u axis): local (u, v, z) -> parent (x, y, z)."""

    def __init__(self, ox, oy, yaw, oz=0.0, mirror=False):
        self.ox, self.oy, self.oz, self.yaw, self.mirror = ox, oy, oz, yaw, mirror
        self.c, self.s = math.cos(yaw), math.sin(yaw)

    def __call__(self, p):
        u, v, z = p
        if self.mirror:
            u = -u
        return (self.ox + u * self.c - v * self.s, self.oy + u * self.s + v * self.c, self.oz + z)

    def apply(self, mesh):
        out = mesh.transformed(self)
        if self.mirror:                      # a mirror flips the winding
            out.faces = [(mat, list(reversed(i)), list(reversed(uv))) for mat, i, uv in out.faces]
        return out


class Facade:
    """A straight wall from p0 to p1 (building frame, 2D) whose outside is to the right of p0->p1
    when the footprint is walked counter-clockwise. Local frame: u from 0 at p0 along the wall,
    y out of the wall, z up."""

    def __init__(self, p0, p1):
        self.p0, self.p1 = p0, p1
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        self.length = math.hypot(dx, dy)
        self.d = (dx / self.length, dy / self.length)
        self.n = (self.d[1], -self.d[0])                    # outward
        self.yaw = math.atan2(self.d[1], self.d[0])

    def to_building(self, p):
        u, y, z = p
        return (self.p0[0] + self.d[0] * u + self.n[0] * y, self.p0[1] + self.d[1] * u + self.n[1] * y, z)

    def apply(self, mesh):
        """Facade-local meshes are wound as if (u, y, z) were right-handed; seen from outside, u runs
        to the viewer's right, so the frame is a reflection and the winding is flipped here."""
        out = mesh.transformed(self.to_building)
        out.faces = [(mat, list(reversed(i)), list(reversed(uv))) for mat, i, uv in out.faces]
        return out

    def detail_yaw(self):
        """Yaw for a detail object modelled with +y out of the wall (x then runs along -u)."""
        return math.atan2(self.n[1], self.n[0]) - math.pi / 2


def wall_with_holes(m, mat, length, z0, z1, holes, uv_off=0.0):
    """Wall plane y=0 over u 0..length, z z0..z1 with rectangular holes [(u0, u1, h0, h1)].
    Faces point to +y (outside)."""
    us = sorted(set([0.0, length] + [h[0] for h in holes] + [h[1] for h in holes]))
    zs = sorted(set([z0, z1] + [h[2] for h in holes] + [h[3] for h in holes]))
    us = [u for u in us if 0.0 <= u <= length]
    zs = [z for z in zs if z0 <= z <= z1]
    for i in range(len(us) - 1):
        for j in range(len(zs) - 1):
            cu, cz = (us[i] + us[i + 1]) / 2, (zs[j] + zs[j + 1]) / 2
            if any(h[0] < cu < h[1] and h[2] < cz < h[3] for h in holes):
                continue
            a, b, c, d = us[i], us[i + 1], zs[j], zs[j + 1]
            # outward normal +y: counter-clockwise seen from +y means u decreasing to the right
            m.face(mat, [(b, 0, c), (a, 0, c), (a, 0, d), (b, 0, d)],
                   [(uv_off + length - b, c), (uv_off + length - a, c), (uv_off + length - a, d), (uv_off + length - b, d)])


def reveal(m, mat, u0, u1, z0, z1, depth, arch=False, segs=12):
    """Opening liner from the wall plane (y=0) into the wall (y=-depth). arch: the top is a
    semicircle springing at z1 - (u1-u0)/2."""
    D = -depth
    m0 = len(m.faces)
    if not arch:
        m.face(mat, [(u0, 0, z0), (u1, 0, z0), (u1, D, z0), (u0, D, z0)])          # sill (up)
        m.face(mat, [(u1, 0, z1), (u0, 0, z1), (u0, D, z1), (u1, D, z1)])          # head (down)
        m.face(mat, [(u0, 0, z1), (u0, 0, z0), (u0, D, z0), (u0, D, z1)])          # left jamb (+u)
        m.face(mat, [(u1, 0, z0), (u1, 0, z1), (u1, D, z1), (u1, D, z0)])          # right jamb (-u)
        _flip_from(m, m0)
        return
    r = (u1 - u0) / 2
    zs = z1 - r
    cu = (u0 + u1) / 2
    m.face(mat, [(u0, 0, z0), (u1, 0, z0), (u1, D, z0), (u0, D, z0)])
    m.face(mat, [(u0, 0, zs), (u0, 0, z0), (u0, D, z0), (u0, D, zs)])
    m.face(mat, [(u1, 0, z0), (u1, 0, zs), (u1, D, zs), (u1, D, z0)])
    pts = [(cu + r * math.cos(math.pi * k / segs), zs + r * math.sin(math.pi * k / segs)) for k in range(segs + 1)]
    for (a, az), (b, bz) in zip(pts, pts[1:]):
        m.face(mat, [(a, 0, az), (b, 0, bz), (b, D, bz), (a, D, az)])
    _flip_from(m, m0)


def _flip_from(m, start):
    """The liners above are listed the wrong way round (they would face into the wall): flip them."""
    m.faces[start:] = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in m.faces[start:]]


def arch_spandrels(m, mat, u0, u1, z1, segs=12):
    """Fills the wall between a semicircular arch and the square top of its hole (plane y=0)."""
    r = (u1 - u0) / 2
    zs = z1 - r
    cu = (u0 + u1) / 2
    pts = [(cu + r * math.cos(math.pi * k / segs), zs + r * math.sin(math.pi * k / segs)) for k in range(segs + 1)]
    half = segs // 2
    m0 = len(m.faces)
    right = pts[:half + 1]            # from (u1, zs) up to the crown
    left = pts[half:]                 # from the crown to (u0, zs)
    for (a, az), (b, bz) in zip(right, right[1:]):
        m.face(mat, [(u1, 0, z1), (b, 0, bz), (a, 0, az)])
    for (a, az), (b, bz) in zip(left, left[1:]):
        m.face(mat, [(u0, 0, z1), (b, 0, bz), (a, 0, az)])
    _flip_from(m, m0)


def hip_roof(m, mat, soffit_mat, fascia_mat, x0, x1, y0, y1, z, pitch, over, fascia_h=0.25, gable_x=None):
    """Hipped roof over the rectangle [x0,x1]x[y0,y1] (wall lines) with eaves at z, pitch radians,
    overhang `over`. Returns the ridge height. gable_x: None, or a set of 'x'/'X' ends that get a
    gable (vertical end) instead of a hip."""
    gable_x = gable_x or set()
    X0, X1, Y0, Y1 = x0 - over, x1 + over, y0 - over, y1 + over
    ze = z + fascia_h
    hw = (Y1 - Y0) / 2
    t = math.tan(pitch)
    zr = ze + hw * t
    ym = (Y0 + Y1) / 2
    rx0 = X0 if "x" in gable_x else min(X0 + hw, (X0 + X1) / 2)
    rx1 = X1 if "X" in gable_x else max(X1 - hw, (X0 + X1) / 2)
    m.face(mat, [(X0, Y0, ze), (X1, Y0, ze), (rx1, ym, zr), (rx0, ym, zr)])          # -y slope
    m.face(mat, [(X1, Y1, ze), (X0, Y1, ze), (rx0, ym, zr), (rx1, ym, zr)])          # +y slope
    if "X" in gable_x:
        m.face(fascia_mat, [(X1, Y0, ze), (X1, Y1, ze), (X1, ym, zr)])
    else:
        m.face(mat, [(X1, Y0, ze), (X1, Y1, ze), (rx1, ym, zr)])
    if "x" in gable_x:
        m.face(fascia_mat, [(X0, Y1, ze), (X0, Y0, ze), (X0, ym, zr)])
    else:
        m.face(mat, [(X0, Y1, ze), (X0, Y0, ze), (rx0, ym, zr)])
    # fascia band and the flat soffit under the overhang
    box(m, fascia_mat, X0, X1, Y0, Y1, z, ze, faces="xXyY")
    m.face(soffit_mat, [(X0, Y1, z), (X1, Y1, z), (X1, Y0, z), (X0, Y0, z)])
    return zr


def roof_z(x, y, x0, x1, y0, y1, z, pitch, over, fascia_h=0.25):
    """Height of a hip roof surface (as built by hip_roof) over (x, y); for placing chimneys."""
    X0, X1, Y0, Y1 = x0 - over, x1 + over, y0 - over, y1 + over
    d = min(x - X0, X1 - x, y - Y0, Y1 - y)
    return z + fascia_h + max(0.0, d) * math.tan(pitch)
