"""Interior of the ground-floor «Гастроном № 7» in Novikova 1 (today the Museum of everyday life of
Soviet scientists), as an early-1950s food shop. Pure Python (no bpy): build() returns a geom.Mesh.

Frame: the Novikova 1 HOUSE frame in metres (the same frame plan.novikova1() builds in):
x along the house (north +), y across it: the square-side (east) outer wall face is y = -7.25, the
yard-side face y = +7.25; walls are 0.5 m thick, so the interior starts at y = -6.75. The ground
floor is at z = 0.7 (plan.floor_z(0)), the ceiling at z = 4.2 (floor above at 4.5).

Spaces (docs/academgorodok/interior_notes.md, section (c)):
  vestibule  x -2.45..2.45, y -6.75..-1.5, ivory 15 cm tiles to 1.6 m, behind the central entrance
  arch       in the partition x 2.45..3.0, y -4.8..-2.4: 2.4 m wide, crown 3.0 m over the floor
  shop hall  x 3.0..18.0,  y -6.75..1.5, light-blue 15 cm tiles to 2.3 m, plaster above
             customers y -6.75..-1.0 | counter (prilavok-vitrina) y -1.0..-0.2 | service aisle |
             back-wall mirror cases y 0.95..1.5 | closed door to the back rooms at x 16.8..17.7
The east wall openings (entrance, hall door, three shop windows) match plan.novikova1(): the outer
facade, the reveals through the wall and the door/window units are built there; this module only
builds from the inner wall face inwards and leaves exactly those holes.

Text: the mesh carries no words. Boards are plain maroon glass; LABELS gives the localization key,
centre and size of each board face so the game can print localized text there
(.claude/skills/localization); LABEL_NORMALS gives the direction each one faces.
"""
import math
import random

from geom import Mesh, box, cross, norm, sub, wall_with_holes

# --- materials ---------------------------------------------------------------------------------------
GASTRONOM_MATERIALS = {
    # name: (base colour sRGB hex, roughness, what it is)
    "AC_GTileBlue":    ("#B9D3DC", 0.15, "15 x 15 cm glazed wall tiles, pale blue, dark grout (hall, column)"),
    "AC_GTileIvory":   ("#E4D8B4", 0.15, "15 x 15 cm glazed wall tiles, ivory, dark grout (vestibule)"),
    "AC_GFloor":       ("#D9D2C6", 0.25, "polished marble-look floor slabs 40 x 40 cm laid diagonally, grey-brown veins"),
    "AC_GPlaster":     ("#EEEADF", 0.9, "lime-white plaster: walls above the tiles, ceiling, cornice, arch"),
    "AC_GPaintWhite":  ("#ECE8DD", 0.5, "white oil-painted joinery: case frames, cornice shelf, consoles, radiators"),
    "AC_GWood":        ("#5A3620", 0.45, "varnished dark oak joinery: counter body, cabinets, cash booth"),
    "AC_GWoodLight":   ("#8A5A33", 0.4, "varnished light wood: counter worktops, shelves"),
    "AC_GMarbleTop":   ("#DCD8D0", 0.2, "white-grey marble slab on the counter's weighing sections"),
    "AC_GGlass":       ("#DDEBEA", 0.03, "display glass (vitrina, shelves, booth panes); transparent"),
    "AC_GMirror":      ("#D8DCDC", 0.02, "silvered mirror backs of the wall cases"),
    "AC_GChrome":      ("#C8CACC", 0.15, "chromed / nickel-plated steel: case uprights, scale pans, trims"),
    "AC_GBrass":       ("#B08D45", 0.3, "brass frames of the glass boards"),
    "AC_GOpal":        ("#FFF6E6", 0.3, "opal glass of the cylinder pendants (emissive when lit)"),
    "AC_GCeramicBlue": ("#2E8FA8", 0.35, "turquoise glazed ceramic lamp cups"),
    "AC_GCord":        ("#3A3632", 0.6, "twisted lamp flex"),
    "AC_GBoard":       ("#5A1418", 0.1, "maroon reverse-painted glass boards (text is printed by the game)"),
    "AC_GScale":       ("#4D8A93", 0.35, "teal enamel paint of the dial scales"),
    "AC_GEnamel":      ("#F4F2EA", 0.2, "white enamel: scale dials, trays, sink"),
    "AC_GTin":         ("#B7B9B4", 0.25, "tinplate lids and cans"),
    "AC_GRed":         ("#A8302A", 0.5, "packaged goods: red labels, tomato juice, caviar tins"),
    "AC_GYellow":      ("#D9A933", 0.5, "packaged goods: yellow labels, butter, apple juice"),
    "AC_GGreen":       ("#4E6E3A", 0.5, "packaged goods: green labels, pickles"),
    "AC_GBlue":        ("#2F4C7E", 0.5, "packaged goods: blue labels, sugar-loaf paper"),
    "AC_GPaper":       ("#E8DFC8", 0.8, "packaged goods: white/kraft paper, sugar, salt, flour packets"),
    "AC_GBrown":       ("#6B4426", 0.6, "packaged goods: tea, coffee, chocolate, kraft bags"),
    "AC_GBottleGreen": ("#2F5A3C", 0.08, "green bottle glass"),
    "AC_GBottleBrown": ("#5A3418", 0.08, "brown bottle glass"),
    "AC_GSausage":     ("#7E3A2C", 0.45, "sausages and hams"),
    "AC_GCheese":      ("#E2B85A", 0.5, "cheese wheels"),
}
# also used from common.MATERIALS: AC_Door (closed door leaves), AC_Frame (door architraves)

# --- key dimensions ----------------------------------------------------------------------------------
Z0 = 0.7                 # ground floor level (plan.floor_z(0))
ZC = 4.2                 # ceiling (floor above at plan.floor_z(1) = 4.5, slab 0.3)
YI = -6.75               # inner face of the east (square-side) wall
HALL = (3.0, 18.0, YI, 1.5)
VEST = (-2.45, 2.45, YI, -1.5)
TILE_HALL = Z0 + 2.3
TILE_VEST = Z0 + 1.6
ARCH_Y = (-4.8, -2.4)    # arch in the partition x 2.45..3.0
ARCH_TOP = Z0 + 3.0      # crown
COUNTER = (5.2, 16.9, -1.0, -0.2)
CASES_X = (3.3, 16.5)    # back-wall cases: 11 bays of 1.2 m
CASE_BAY = 1.2
BACK_DOOR = (16.8, 17.7, Z0, Z0 + 2.1)
KASSA = (3.3, 4.7, -2.2, -0.9)
COLUMN = (10.13, 10.73, -3.6, -3.0)

# East wall openings, as plan.novikova1() cuts them (centre x, sill z, width, height). The brief
# gives the hall door as 1.4 m; plan.py builds it 1.5 m wide, so the hole follows plan.py.
ENTRANCE = (0.0, 0.55, 1.6, 2.6)
HALL_DOOR = (4.35, 0.55, 1.5, 2.65)
SHOP_WINDOWS = [(x, 1.25, 2.0, 2.25) for x in (8.4, 12.45, 16.5)]

# --- pendant lamps: (x, y) of the lamp, bottom of the opal cylinder at LAMP_Z -------------------------
LAMP_Z = Z0 + 2.35
LAMP_XY = [(6.0, -1.55), (8.6, -1.55), (11.2, -1.55), (13.8, -1.55), (16.4, -1.55),
           (7.0, -4.6), (14.0, -4.6), (0.0, -4.0)]
LAMP_R, LAMP_H = 0.15, 0.32
LIGHT_COLOUR = "#FFD9A8"     # ~2900 K incandescent behind opal glass
LIGHT_LUMENS = 1600.0        # one 150 W lamp
LIGHTS = [(x, y, LAMP_Z + LAMP_H * 0.45, LIGHT_COLOUR, LIGHT_LUMENS) for x, y in LAMP_XY]

LABELS = []          # (key, x, y, z, w, h): centre and size of the board face
LABEL_NORMALS = []   # (nx, ny, nz) per LABELS entry: the direction the board face looks


# --- helpers -----------------------------------------------------------------------------------------
def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def face_n(m, mat, pts, n, uvs=None):
    """Adds a face, reversing it if needed so its normal points along n."""
    # drop repeated corners (degenerate quads become triangles), then Newell's normal
    keep, kuv = [], []
    for i, p in enumerate(pts):
        if not keep or max(abs(p[k] - keep[-1][k]) for k in range(3)) > 1e-9:
            keep.append(p)
            kuv.append(uvs[i] if uvs else None)
    if len(keep) > 1 and max(abs(keep[0][k] - keep[-1][k]) for k in range(3)) <= 1e-9:
        keep.pop()
        kuv.pop()
    if len(keep) < 3:
        return
    pts, uvs = keep, (kuv if uvs else None)
    fn = [0.0, 0.0, 0.0]
    for i, a in enumerate(pts):
        b = pts[(i + 1) % len(pts)]
        fn[0] += (a[1] - b[1]) * (a[2] + b[2])
        fn[1] += (a[2] - b[2]) * (a[0] + b[0])
        fn[2] += (a[0] - b[0]) * (a[1] + b[1])
    if _dot(fn, n) < 0:
        pts = list(reversed(pts))
        uvs = list(reversed(uvs)) if uvs else None
    m.face(mat, pts, uvs)


def bx(m, mat, x0, x1, y0, y1, z0, z1, faces="all"):
    box(m, mat, min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1), min(z0, z1), max(z0, z1), faces)


def _basis(axis):
    a = norm(axis)
    t = (1.0, 0.0, 0.0) if abs(a[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = norm(cross(a, t))
    v = cross(a, u)
    return a, u, v


def tube(m, mat, c0, c1, r0, r1, seg=8, cap0=None, cap1=None, phase=0.0):
    """Frustum from c0 (radius r0) to c1 (radius r1); cap0/cap1: material of the end caps or None."""
    a, u, v = _basis(sub(c1, c0))
    ring = []
    for k in range(seg):
        t = phase + 2 * math.pi * k / seg
        d = (u[0] * math.cos(t) + v[0] * math.sin(t), u[1] * math.cos(t) + v[1] * math.sin(t),
             u[2] * math.cos(t) + v[2] * math.sin(t))
        ring.append(d)
    p0 = [(c0[0] + d[0] * r0, c0[1] + d[1] * r0, c0[2] + d[2] * r0) for d in ring]
    p1 = [(c1[0] + d[0] * r1, c1[1] + d[1] * r1, c1[2] + d[2] * r1) for d in ring]
    circ = 2 * math.pi * max(r0, r1)
    L = math.sqrt(sum((c1[i] - c0[i]) ** 2 for i in range(3)))
    for k in range(seg):
        j = (k + 1) % seg
        mid = ((ring[k][0] + ring[j][0]) / 2, (ring[k][1] + ring[j][1]) / 2, (ring[k][2] + ring[j][2]) / 2)
        su0, su1 = circ * k / seg, circ * (k + 1) / seg
        if r1 <= 1e-6:
            face_n(m, mat, [p0[k], p0[j], c1], mid, [(su0, 0), (su1, 0), ((su0 + su1) / 2, L)])
        elif r0 <= 1e-6:
            face_n(m, mat, [c0, p1[j], p1[k]], mid, [((su0 + su1) / 2, 0), (su1, L), (su0, L)])
        else:
            face_n(m, mat, [p0[k], p0[j], p1[j], p1[k]], mid, [(su0, 0), (su1, 0), (su1, L), (su0, L)])
    if cap0 and r0 > 1e-6:
        face_n(m, cap0, p0, (-a[0], -a[1], -a[2]))
    if cap1 and r1 > 1e-6:
        face_n(m, cap1, p1, a)


def cyl(m, mat, x, y, z0, z1, r, seg=8, top=None, bottom=None, r_top=None):
    tube(m, mat, (x, y, z0), (x, y, z1), r, r if r_top is None else r_top, seg, cap0=bottom, cap1=top,
         phase=math.pi / seg)


class _Wall:
    """Local wall frame: u along the wall, v = the wall's inward normal (into the room), z up.
    Built so that (u, v, z) -> (x, y, z) is a proper rotation: face windings carry over."""

    def __init__(self, origin, n):
        self.o, self.n = origin, n
        self.d = (n[1], -n[0])

    def __call__(self, p):
        u, v, z = p
        return (self.o[0] + u * self.d[0] + v * self.n[0], self.o[1] + u * self.d[1] + v * self.n[1], z)

    def at(self, u, v=0.0):
        return self((u, v, 0.0))[:2]


def tiled_wall(m, w, length, holes, tile_top, tile_mat, uv_off=0.0):
    """Wall face (z Z0..ZC) with holes [(u0, u1, z0, z1)]: tiles up to tile_top, plaster above."""
    loc = Mesh()
    wall_with_holes(loc, tile_mat, length, Z0, tile_top, holes, uv_off)
    wall_with_holes(loc, "AC_GPlaster", length, tile_top, ZC, holes, uv_off)
    m.extend(loc.transformed(w))


def _arch_pts(u0, u1, ztop, segs=12):
    r = (u1 - u0) / 2
    zs, cu = ztop - r, (u0 + u1) / 2
    # from u1 (springing) over the crown to u0
    return [(cu + r * math.cos(math.pi * k / segs), zs + r * math.sin(math.pi * k / segs)) for k in range(segs + 1)], zs


def arch_spandrel(m, w, u0, u1, ztop, zhole, tile_top, tile_mat, segs=12):
    """Fills the wall between a round arch (u0..u1, crown ztop) and the hole's square top zhole,
    in vertical strips split at the tile line. Faces look along +v (into the room)."""
    pts, _ = _arch_pts(u0, u1, ztop, segs)
    loc = Mesh()
    for (a, az), (b, bz) in zip(pts, pts[1:]):
        lo, hi = (b, a), (bz, az)     # b < a: strip from u=b to u=a
        ua, ub = lo
        za, zb = hi
        # lower (tiled) part
        if min(za, zb) < tile_top:
            ta, tb = max(za, tile_top), max(zb, tile_top)
            face_n(loc, tile_mat, [(ua, 0, za), (ub, 0, zb), (ub, 0, tb), (ua, 0, ta)], (0, 1, 0),
                   [(ua, za), (ub, zb), (ub, tb), (ua, ta)])
        ta, tb = max(za, tile_top), max(zb, tile_top)
        face_n(loc, "AC_GPlaster", [(ua, 0, ta), (ub, 0, tb), (ub, 0, zhole), (ua, 0, zhole)], (0, 1, 0),
               [(ua, ta), (ub, tb), (ub, zhole), (ua, zhole)])
    m.extend(loc.transformed(w))


def arch_reveal(m, w, u0, u1, ztop, depth, tile_top, segs=12):
    """Liner of the round-arched passage from the wall face (v=0) through the wall (v=-depth)."""
    pts, zs = _arch_pts(u0, u1, ztop, segs)
    loc = Mesh()
    D = -depth
    cu = (u0 + u1) / 2
    for u, n in ((u0, (1, 0, 0)), (u1, (-1, 0, 0))):
        face_n(loc, "AC_GTileBlue", [(u, 0, Z0), (u, D, Z0), (u, D, tile_top), (u, 0, tile_top)], n)
        face_n(loc, "AC_GPlaster", [(u, 0, tile_top), (u, D, tile_top), (u, D, zs), (u, 0, zs)], n)
    for (a, az), (b, bz) in zip(pts, pts[1:]):
        mid = ((a + b) / 2, 0, (az + bz) / 2)
        inward = (cu - mid[0], 0, zs - mid[2])
        face_n(loc, "AC_GPlaster", [(a, 0, az), (b, 0, bz), (b, D, bz), (a, D, az)], inward)
    m.extend(loc.transformed(w))


def door_in_wall(m, w, u0, u1, z1, leaves=1, glazed=False, recess=0.08):
    """A closed door in a hole (u0..u1, Z0..z1) of the wall w: reveal, leaf (AC_Door) set `recess`
    behind the face, painted architrave (AC_Frame) around the hole."""
    loc = Mesh()
    D = -recess
    face_n(loc, "AC_GPlaster", [(u0, 0, Z0), (u0, D, Z0), (u0, D, z1), (u0, 0, z1)], (1, 0, 0))
    face_n(loc, "AC_GPlaster", [(u1, 0, Z0), (u1, D, Z0), (u1, D, z1), (u1, 0, z1)], (-1, 0, 0))
    face_n(loc, "AC_GPlaster", [(u0, 0, z1), (u1, 0, z1), (u1, D, z1), (u0, D, z1)], (0, 0, -1))
    face_n(loc, "AC_Door", [(u0, D, Z0), (u1, D, Z0), (u1, D, z1), (u0, D, z1)], (0, 1, 0))
    face_n(loc, "AC_GFloor", [(u0, 0, Z0), (u1, 0, Z0), (u1, D, Z0), (u0, D, Z0)], (0, 0, 1))      # floor in the recess
    lw = (u1 - u0) / leaves
    for k in range(leaves):
        a, b = u0 + k * lw, u0 + (k + 1) * lw
        # fielded panels: a low and a tall panel per leaf (or glass above)
        mz = Z0 + 0.95
        bx(loc, "AC_Door", a + 0.1, b - 0.1, D, D + 0.02, Z0 + 0.15, mz - 0.1, faces="yXxzZY")
        if glazed:
            bx(loc, "AC_GGlass", a + 0.12, b - 0.12, D - 0.005, D + 0.005, mz + 0.05, z1 - 0.15)
        else:
            bx(loc, "AC_Door", a + 0.1, b - 0.1, D, D + 0.02, mz + 0.05, z1 - 0.15, faces="yXxzZY")
        hx = b - 0.08 if k == 0 else a + 0.08
        bx(loc, "AC_GBrass", hx - 0.015, hx + 0.015, D, D + 0.06, Z0 + 1.0, Z0 + 1.06)
    # architrave
    t, p = 0.09, 0.025
    bx(loc, "AC_Frame", u0 - t, u0, 0, p, Z0, z1 + t)
    bx(loc, "AC_Frame", u1, u1 + t, 0, p, Z0, z1 + t)
    bx(loc, "AC_Frame", u0, u1, 0, p, z1, z1 + t)
    m.extend(loc.transformed(w))


def cornice(m, x0, x1, y0, y1):
    """Profiled plaster cornice round a room rectangle plus the flat ceiling inside it."""
    prof = [(0.0, 0.32), (0.025, 0.32), (0.025, 0.28), (0.05, 0.26), (0.05, 0.22), (0.10, 0.17),
            (0.13, 0.17), (0.13, 0.13), (0.19, 0.09), (0.22, 0.09), (0.22, 0.05), (0.27, 0.035),
            (0.30, 0.0)]
    centre = ((x0 + x1) / 2, (y0 + y1) / 2, ZC - 1.5)

    def ring(d, dz):
        z = ZC - dz
        return [(x0 + d, y0 + d, z), (x1 - d, y0 + d, z), (x1 - d, y1 - d, z), (x0 + d, y1 - d, z)]

    rings = [ring(d, dz) for d, dz in prof]
    for ra, rb in zip(rings, rings[1:]):
        for k in range(4):
            j = (k + 1) % 4
            pts = [ra[k], ra[j], rb[j], rb[k]]
            if abs(ra[k][0] - rb[k][0]) < 1e-9 and abs(ra[k][2] - rb[k][2]) < 1e-9:
                continue
            c = [sum(p[i] for p in pts) / 4 for i in range(3)]
            face_n(m, "AC_GPlaster", pts, (centre[0] - c[0], centre[1] - c[1], centre[2] - c[2]))
    d = prof[-1][0]
    face_n(m, "AC_GPlaster", [(x0 + d, y0 + d, ZC), (x1 - d, y0 + d, ZC), (x1 - d, y1 - d, ZC), (x0 + d, y1 - d, ZC)],
           (0, 0, -1))


def floor(m, x0, x1, y0, y1):
    pts = [(x0, y0, Z0), (x1, y0, Z0), (x1, y1, Z0), (x0, y1, Z0)]
    s = math.sqrt(0.5)
    uvs = [((x + y) * s, (y - x) * s) for x, y, _ in pts]   # slabs on the diagonal
    face_n(m, "AC_GFloor", pts, (0, 0, 1), uvs)


def board(m, key, x, y, z, w, h, n, frame=0.03):
    """Maroon glass board on a wall facing n (unit along x or y): centre (x, y, z) on the wall face."""
    nx, ny = n
    t = 0.02
    ux, uy = -ny, nx                    # along the wall
    def P(a, b, c):
        return (x + ux * a + nx * b, y + uy * a + ny * b, z + c)
    pts = [P(-w / 2, t, -h / 2), P(w / 2, t, -h / 2), P(w / 2, t, h / 2), P(-w / 2, t, h / 2)]
    face_n(m, "AC_GBoard", pts, (nx, ny, 0))
    # brass frame bars
    for a0, a1, c0, c1 in ((-w / 2 - frame, w / 2 + frame, h / 2, h / 2 + frame),
                           (-w / 2 - frame, w / 2 + frame, -h / 2 - frame, -h / 2),
                           (-w / 2 - frame, -w / 2, -h / 2, h / 2), (w / 2, w / 2 + frame, -h / 2, h / 2)):
        p0, p1 = P(a0, 0, c0), P(a1, t + 0.008, c1)
        bx(m, "AC_GBrass", p0[0], p1[0], p0[1], p1[1], p0[2], p1[2])
    # board edges
    for a, nn in ((-w / 2, (-ux, -uy, 0)), (w / 2, (ux, uy, 0))):
        face_n(m, "AC_GBoard", [P(a, 0, -h / 2), P(a, t, -h / 2), P(a, t, h / 2), P(a, 0, h / 2)], nn)
    if key:
        c = P(0, t + 0.001, 0)
        LABELS.append((key, round(c[0], 4), round(c[1], 4), round(c[2], 4), w, h))
        LABEL_NORMALS.append((nx, ny, 0.0))


# --- goods ---------------------------------------------------------------------------------------------
LABEL_MATS = ["AC_GRed", "AC_GYellow", "AC_GGreen", "AC_GBlue", "AC_GPaper", "AC_GBrown"]


def bottle(m, rnd, x, y, z, r=0.034, h=0.30, mat=None):
    mat = mat or rnd.choice(["AC_GBottleGreen", "AC_GBottleGreen", "AC_GBottleBrown", "AC_GGlass"])
    hb = h * 0.62
    cyl(m, mat, x, y, z, z + hb, r, seg=6, bottom=None)
    tube(m, mat, (x, y, z + hb), (x, y, z + hb + h * 0.14), r, r * 0.35, 6, phase=math.pi / 6)
    cyl(m, mat, x, y, z + hb + h * 0.14, z + h, r * 0.35, seg=4, top=rnd.choice(["AC_GTin", "AC_GRed", "AC_GYellow"]))
    if rnd.random() < 0.7:                                      # paper label
        cyl(m, rnd.choice(LABEL_MATS), x, y, z + hb * 0.25, z + hb * 0.7, r * 1.04, seg=6)


def goods_row(m, rnd, x0, x1, y, z, depth=0.25, tall=0.32, kinds=None):
    """A row of packaged goods on a shelf at height z between x0 and x1, centred on y."""
    kinds = kinds or ["tin", "tin", "tins", "jar", "bottle", "box", "box", "packet"]
    x = x0 + 0.03
    while True:
        k = rnd.choice(kinds)
        if k == "tin":
            r = rnd.uniform(0.035, 0.05)
            h = rnd.uniform(0.05, 0.11)
            if x + 2 * r > x1:
                break
            lab = rnd.choice(LABEL_MATS)
            for j in range(rnd.choice((1, 2, 2, 3))):
                if z + h * (j + 1) - z > tall:
                    break
                cyl(m, lab, x + r, y, z + h * j, z + h * (j + 1) - 0.003, r, seg=8, top="AC_GTin")
            x += 2 * r + 0.012
        elif k == "tins":                                 # flat fish tins in a stack
            w, d, h = 0.11, 0.07, 0.03
            if x + w > x1:
                break
            lab = rnd.choice(["AC_GRed", "AC_GBlue", "AC_GTin", "AC_GYellow"])
            n = rnd.randint(3, 6)
            for j in range(n):
                if (j + 1) * h > tall:
                    break
                bx(m, lab, x, x + w, y - d / 2, y + d / 2, z + j * h, z + (j + 1) * h - 0.002, faces="xXyYZ")
            x += w + 0.015
        elif k == "jar":
            r, h = 0.048, rnd.uniform(0.11, 0.17)
            if x + 2 * r > x1:
                break
            cyl(m, rnd.choice(["AC_GRed", "AC_GGreen", "AC_GBrown", "AC_GYellow"]), x + r, y, z, z + h, r, seg=8)
            cyl(m, rnd.choice(["AC_GTin", "AC_GRed"]), x + r, y, z + h, z + h + 0.015, r * 0.92, seg=8, top="AC_GTin")
            x += 2 * r + 0.012
        elif k == "bottle":
            r = 0.034
            if x + 2 * r > x1:
                break
            bottle(m, rnd, x + r, y, z, r, min(tall, rnd.uniform(0.26, 0.33)))
            x += 2 * r + 0.012
        elif k == "box":
            w, d, h = rnd.uniform(0.07, 0.12), rnd.uniform(0.04, 0.08), rnd.uniform(0.1, min(0.2, tall))
            if x + w > x1:
                break
            bx(m, rnd.choice(LABEL_MATS), x, x + w, y - d / 2, y + d / 2, z, z + h, faces="xXyYZ")
            x += w + 0.01
        elif k == "packet":                               # tea / coffee packets in a little stack
            w, d, h = 0.06, 0.045, 0.09
            if x + 3 * w > x1:
                break
            lab = rnd.choice(["AC_GBrown", "AC_GGreen", "AC_GYellow", "AC_GRed"])
            for j in range(3):
                bx(m, lab, x + j * w, x + (j + 1) * w - 0.004, y - d / 2, y + d / 2, z, z + h, faces="xXyYZ")
            x += 3 * w + 0.015


def three_litre_jar(m, x, y, z, fill):
    r, h = 0.085, 0.24
    cyl(m, "AC_GGlass", x, y, z, z + h, r, seg=10, top=None)
    cyl(m, fill, x, y, z + 0.005, z + h * 0.85, r * 0.94, seg=10, top=fill)
    tube(m, "AC_GGlass", (x, y, z + h), (x, y, z + h + 0.04), r, r * 0.6, 10)
    cyl(m, "AC_GTin", x, y, z + h + 0.04, z + h + 0.06, r * 0.62, seg=10, top="AC_GTin")


def sugar_loaf(m, x, y, z):
    """Sugar loaf (сахарная голова) half-wrapped in blue paper."""
    tube(m, "AC_GBlue", (x, y, z), (x, y, z + 0.22), 0.085, 0.045, 10, cap0="AC_GBlue")
    tube(m, "AC_GPaper", (x, y, z + 0.22), (x, y, z + 0.34), 0.045, 0.0, 10)


def juice_cones(m, x, y, z, fills=("AC_GRed", "AC_GYellow", "AC_GSausage")):
    """Stand with three glass juice cones (inverted cones with taps) on the counter."""
    n = len(fills)
    span = 0.32 * n
    bx(m, "AC_GChrome", x - span / 2, x + span / 2, y - 0.14, y + 0.14, z, z + 0.04)          # base tray
    for k, fill in enumerate(fills):
        cx = x - span / 2 + 0.16 + k * 0.32
        cyl(m, "AC_GChrome", cx, y + 0.155, z + 0.04, z + 0.845, 0.012, seg=4)              # post
        tube(m, "AC_GGlass", (cx, y, z + 0.30), (cx, y, z + 0.82), 0.02, 0.14, 10, cap1="AC_GGlass")
        tube(m, fill, (cx, y, z + 0.33), (cx, y, z + 0.70), 0.025, 0.105, 10, cap1=fill)
        tube(m, "AC_GChrome", (cx, y, z + 0.82), (cx, y, z + 0.845), 0.145, 0.14, 10, cap1="AC_GChrome")
        cyl(m, "AC_GChrome", cx, y, z + 0.22, z + 0.30, 0.012, seg=6)                       # tap
        tube(m, "AC_GChrome", (cx, y, z + 0.25), (cx, y - 0.06, z + 0.25), 0.008, 0.008, 4)
        cyl(m, "AC_GGlass", cx, y - 0.02, z + 0.04, z + 0.14, 0.032, seg=8, bottom=None)  # glass under it


def dial_scale(m, x, y, z):
    """Mechanical counter scale with a round twin dial (both faces read: seller and customer)."""
    bx(m, "AC_GScale", x - 0.24, x + 0.24, y - 0.15, y + 0.15, z, z + 0.11)
    bx(m, "AC_GScale", x - 0.05, x + 0.05, y - 0.05, y + 0.05, z + 0.11, z + 0.30)
    tube(m, "AC_GScale", (x, y - 0.055, z + 0.46), (x, y + 0.055, z + 0.46), 0.175, 0.175, 14)
    for s in (-1, 1):
        tube(m, "AC_GEnamel", (x, y + s * 0.055, z + 0.46), (x, y + s * 0.065, z + 0.46), 0.15, 0.15, 14,
             cap1="AC_GEnamel")
        tube(m, "AC_GCord", (x, y + s * 0.066, z + 0.46), (x + 0.1, y + s * 0.067, z + 0.52),
             0.004, 0.004, 3)                                             # pointer
    cyl(m, "AC_GChrome", x - 0.12, y, z + 0.11, z + 0.13, 0.13, seg=14, top="AC_GChrome")     # pan
    cyl(m, "AC_GChrome", x + 0.15, y, z + 0.11, z + 0.125, 0.07, seg=10, top="AC_GChrome")    # weight plate


# --- parts -----------------------------------------------------------------------------------------------
def walls(m):
    hx0, hx1, hy0, hy1 = HALL
    vx0, vx1, vy0, vy1 = VEST
    # hall, east wall (window wall), looking +y
    w = _Wall((hx0, YI), (0.0, 1.0))
    holes = []
    for cx, sill, wd, ht in [HALL_DOOR] + SHOP_WINDOWS:
        holes.append((cx - wd / 2 - hx0, cx + wd / 2 - hx0, sill, sill + ht))
    tiled_wall(m, w, hx1 - hx0, holes, TILE_HALL, "AC_GTileBlue")
    # window boards (sills) on the inner face: painted board under each shop window
    for cx, sill, wd, ht in SHOP_WINDOWS:
        bx(m, "AC_GPaintWhite", cx - wd / 2 - 0.05, cx + wd / 2 + 0.05, YI - 0.2, YI + 0.06, sill - 0.03, sill + 0.006)
        # cast-iron radiator under the window (only 0.55 m of wall: a low one)
        bx(m, "AC_GPaintWhite", cx - 0.7, cx + 0.7, YI + 0.02, YI + 0.12, Z0 + 0.1, sill - 0.12, faces="xXYZ")
        for k in range(12):
            fx = cx - 0.66 + k * 0.12
            bx(m, "AC_GPaintWhite", fx, fx + 0.06, YI + 0.12, YI + 0.15, Z0 + 0.12, sill - 0.14, faces="xXYZ")
    # hall, north wall
    w = _Wall((hx1, hy0), (-1.0, 0.0))
    tiled_wall(m, w, hy1 - hy0, [], TILE_HALL, "AC_GTileBlue")
    # hall, back wall (y = 1.5), with the closed door to the back rooms
    w = _Wall((hx1, hy1), (0.0, -1.0))
    du0, du1 = hx1 - BACK_DOOR[1], hx1 - BACK_DOOR[0]
    tiled_wall(m, w, hx1 - hx0, [(du0, du1, Z0, BACK_DOOR[3])], TILE_HALL, "AC_GTileBlue")
    door_in_wall(m, w, du0, du1, BACK_DOOR[3], leaves=1)
    # hall, south wall (x = 3.0) with the arch
    w = _Wall((hx0, hy1), (1.0, 0.0))
    a0, a1 = hy1 - ARCH_Y[1], hy1 - ARCH_Y[0]
    tiled_wall(m, w, hy1 - hy0, [(a0, a1, Z0, ARCH_TOP)], TILE_HALL, "AC_GTileBlue")
    arch_spandrel(m, w, a0, a1, ARCH_TOP, ARCH_TOP, TILE_HALL, "AC_GTileBlue")
    arch_reveal(m, w, a0, a1, ARCH_TOP, hx0 - vx1, TILE_VEST)

    # vestibule: east wall with the entrance
    w = _Wall((vx0, YI), (0.0, 1.0))
    cx, sill, wd, ht = ENTRANCE
    tiled_wall(m, w, vx1 - vx0, [(cx - wd / 2 - vx0, cx + wd / 2 - vx0, sill, sill + ht)], TILE_VEST, "AC_GTileIvory")
    # vestibule: north wall (x = 2.45) with the arch
    w = _Wall((vx1, vy0), (-1.0, 0.0))
    b0, b1 = ARCH_Y[0] - vy0, ARCH_Y[1] - vy0
    tiled_wall(m, w, vy1 - vy0, [(b0, b1, Z0, ARCH_TOP)], TILE_VEST, "AC_GTileIvory")
    arch_spandrel(m, w, b0, b1, ARCH_TOP, ARCH_TOP, TILE_VEST, "AC_GTileIvory")
    # vestibule: back wall (y = -1.5)
    w = _Wall((vx1, vy1), (0.0, -1.0))
    tiled_wall(m, w, vx1 - vx0, [], TILE_VEST, "AC_GTileIvory")
    # vestibule: south wall (x = -2.45) with a closed glazed double door (the hairdresser's)
    w = _Wall((vx0, vy1), (1.0, 0.0))
    d0, d1 = vy1 - (-3.0), vy1 - (-4.3)
    tiled_wall(m, w, vy1 - vy0, [(d0, d1, Z0, Z0 + 2.4)], TILE_VEST, "AC_GTileIvory")
    door_in_wall(m, w, d0, d1, Z0 + 2.4, leaves=2, glazed=True)

    # floors, ceilings with cornices, the arch threshold
    floor(m, hx0, hx1, hy0, hy1)
    floor(m, vx0, vx1, vy0, vy1)
    floor(m, vx1, hx0, ARCH_Y[0], ARCH_Y[1])
    cornice(m, hx0, hx1, hy0, hy1)
    cornice(m, vx0, vx1, vy0, vy1)


def column(m):
    x0, x1, y0, y1 = COLUMN
    bx(m, "AC_GTileBlue", x0, x1, y0, y1, Z0, TILE_HALL, faces="xXyY")
    bx(m, "AC_GPlaster", x0, x1, y0, y1, TILE_HALL, ZC - 0.3, faces="xXyY")
    # simple capital: two plaster steps under the ceiling
    bx(m, "AC_GPlaster", x0 - 0.04, x1 + 0.04, y0 - 0.04, y1 + 0.04, ZC - 0.3, ZC - 0.18, faces="xXyYz")
    bx(m, "AC_GPlaster", x0 - 0.09, x1 + 0.09, y0 - 0.09, y1 + 0.09, ZC - 0.18, ZC, faces="xXyYz")
    cx = (x0 + x1) / 2
    board(m, "sign.gastronom.sales_limits", cx, y0, Z0 + 1.75, 0.45, 0.7, (0, -1))
    board(m, "sign.gastronom.pay_at_cashier", x1, (y0 + y1) / 2, Z0 + 1.55, 0.36, 0.26, (1, 0))


def counter(m, rnd):
    """1950s prilavok-vitrina: oak body, glass display cases alternating with marble weighing tops."""
    x0, x1, yf, yb = COUNTER
    n = 6
    mw = (x1 - x0) / n
    kinds = ["vitrina", "scale", "vitrina", "vitrina", "scale", "juice"]
    zt = Z0 + 0.72                     # top of the body
    for k in range(n):
        a, b = x0 + k * mw, x0 + (k + 1) * mw
        # plinth, body, panelled front
        bx(m, "AC_GWood", a + 0.02, b - 0.02, yf + 0.06, yb, Z0, Z0 + 0.1, faces="xXyYZ")
        bx(m, "AC_GWood", a, b, yf, yb, Z0 + 0.1, zt, faces="xXyYZ" if k in (0, n - 1) else "yYZ")
        for p in range(2):
            pa, pb = a + 0.1 + p * (mw - 0.1) / 2, a + (p + 1) * (mw - 0.1) / 2
            bx(m, "AC_GWoodLight", pa, pb, yf - 0.018, yf, Z0 + 0.2, zt - 0.1, faces="xXyzZ")
        bx(m, "AC_GChrome", a, b, yf - 0.01, yf + 0.01, zt - 0.03, zt, faces="yzZ")     # nickel edge
        if kinds[k] == "vitrina":
            vitrina(m, rnd, a, b, yf, yb, zt)
        else:
            bx(m, "AC_GMarbleTop", a - 0.005, b + 0.005, yf - 0.03, yb + 0.01, zt, zt + 0.03, faces="xXyYZ")
            if kinds[k] == "scale":
                dial_scale(m, (a + b) / 2, (yf + yb) / 2 + 0.05, zt + 0.03)
                # wrapping paper roll and a few weights
                tube(m, "AC_GPaper", (a + 0.15, yb - 0.1, zt + 0.09), (a + 0.6, yb - 0.1, zt + 0.09), 0.06, 0.06, 8,
                     cap0="AC_GPaper", cap1="AC_GPaper")
                for j, r in enumerate((0.03, 0.025, 0.02)):
                    cyl(m, "AC_GCord", b - 0.3 + j * 0.08, yb - 0.12, zt + 0.03,
                        zt + 0.03 + r * 2, r, seg=6, top="AC_GCord")
            else:
                juice_cones(m, (a + b) / 2, (yf + yb) / 2 + 0.05, zt + 0.03)
    # customer-side foot rail on brackets
    bx(m, "AC_GChrome", x0, x1, yf - 0.16, yf - 0.13, Z0 + 0.12, Z0 + 0.15)
    for k in range(n + 1):
        xx = min(max(x0 + k * mw, x0 + 0.05), x1 - 0.05)
        bx(m, "AC_GChrome", xx - 0.015, xx + 0.015, yf - 0.16, yf, Z0 + 0.12, Z0 + 0.15, faces="xXzZ")


def vitrina(m, rnd, a, b, yf, yb, zt):
    """Glass display case on the front part of a counter module: sloping front glass, glass top,
    sliding glass at the back for the seller; goods on a white enamel tray inside."""
    g = 0.006
    ytop = yf + 0.1
    yback = yb - 0.32
    zg = Z0 + 1.0                      # 1.0 m over the floor
    # tray
    bx(m, "AC_GEnamel", a + 0.03, b - 0.03, yf + 0.02, yback, zt, zt + 0.01, faces="Z")
    # sloping front glass (thin double-sided quad pair)
    p = [(a + 0.02, yf + 0.01, zt), (b - 0.02, yf + 0.01, zt), (b - 0.02, ytop, zg), (a + 0.02, ytop, zg)]
    nf = norm(cross(sub(p[1], p[0]), sub(p[3], p[0])))
    face_n(m, "AC_GGlass", p, (-nf[0], -nf[1], -nf[2]) if nf[1] > 0 else nf)
    face_n(m, "AC_GGlass", [(q[0] + nf[0] * g, q[1] + nf[1] * g, q[2] + nf[2] * g) for q in p],
           nf if nf[1] > 0 else (-nf[0], -nf[1], -nf[2]))
    bx(m, "AC_GGlass", a + 0.02, b - 0.02, ytop, yback, zg - g, zg)                       # top
    bx(m, "AC_GGlass", a + 0.02, b - 0.02, yback - g, yback, zt + 0.06, zg - g)            # back slider
    for xx in (a + 0.01, b - 0.02):                                                      # ends
        face_n(m, "AC_GGlass", [(xx, yf + 0.01, zt), (xx, yback, zt), (xx, yback, zg), (xx, ytop, zg)], (-1, 0, 0))
        face_n(m, "AC_GGlass", [(xx + 0.01, yf + 0.01, zt), (xx + 0.01, yback, zt), (xx + 0.01, yback, zg),
                                (xx + 0.01, ytop, zg)], (1, 0, 0))
    # nickel frame: corner posts and the top front edge
    for xx in (a + 0.005, b - 0.025):
        tube(m, "AC_GChrome", (xx + 0.01, yf + 0.01, zt), (xx + 0.01, ytop, zg), 0.008, 0.008, 4)
        bx(m, "AC_GChrome", xx, xx + 0.02, yback - 0.02, yback, zt, zg)
    bx(m, "AC_GChrome", a, b, ytop - 0.01, ytop + 0.01, zg - 0.015, zg + 0.005)
    bx(m, "AC_GChrome", a, b, yback - 0.02, yback + 0.005, zg - 0.015, zg + 0.005)
    # seller's worktop behind
    bx(m, "AC_GWoodLight", a, b, yback, yb + 0.01, zt, zt + 0.03, faces="xXYZ")
    # goods inside: sausages, cheese wheels, butter blocks, tins
    z = zt + 0.01
    x = a + 0.08
    kind = rnd.choice(["deli", "deli", "cheese", "sweets"])
    while x < b - 0.2:
        if kind == "deli":
            L = rnd.uniform(0.25, 0.4)
            r = rnd.uniform(0.03, 0.05)
            yy = rnd.uniform(yf + 0.12, yback - 0.08)
            tube(m, "AC_GSausage", (x, yy, z + r), (min(x + L, b - 0.08), yy + rnd.uniform(-0.05, 0.05), z + r), r, r, 6,
                 cap0="AC_GSausage", cap1="AC_GSausage")
            x += 0.09
            if rnd.random() < 0.3:
                cyl(m, "AC_GCheese", x + 0.1, (yf + yback) / 2, z, z + 0.08, 0.1, seg=10, top="AC_GCheese")
                x += 0.22
        elif kind == "cheese":
            r = rnd.uniform(0.08, 0.13)
            cyl(m, "AC_GCheese", x + r, (yf + yback) / 2 + 0.03, z, z + rnd.uniform(0.06, 0.1), r, seg=10, top="AC_GCheese")
            x += 2 * r + 0.05
            if rnd.random() < 0.5:
                bx(m, "AC_GYellow", x, x + 0.14, yf + 0.15, yf + 0.25, z, z + 0.06)       # butter block
                x += 0.18
        else:
            w = rnd.uniform(0.12, 0.2)
            bx(m, rnd.choice(["AC_GRed", "AC_GBlue", "AC_GYellow", "AC_GBrown", "AC_GPaper"]),
               x, x + w, yf + 0.12, yback - 0.05, z, z + rnd.uniform(0.03, 0.07), faces="xXyYZ")
            x += w + 0.03


def back_cases(m, rnd):
    """Built-in wall cases along the back wall: oak lower cupboards with a worktop, mirror-backed
    open glass shelves on nickel uprights in white-painted frames, a white cornice shelf with
    bottles on top."""
    yw = HALL[3]
    x0, x1 = CASES_X
    n = int(round((x1 - x0) / CASE_BAY))
    zc = Z0 + 0.9                         # cupboard top
    zg0, zg1 = zc + 0.03, zc + 0.03 + 1.5  # glass part
    yd, yu = yw - 0.55, yw - 0.36         # front of the cupboards / of the upper cases
    for k in range(n):
        a, b = x0 + k * CASE_BAY, x0 + (k + 1) * CASE_BAY
        bx(m, "AC_GWood", a + 0.02, b - 0.02, yd + 0.05, yw, Z0, Z0 + 0.1, faces="xXyZ")
        bx(m, "AC_GWood", a, b, yd, yw, Z0 + 0.1, zc, faces="yxXZ" if k in (0, n - 1) else "yZ")
        for p in range(2):                                            # two doors with knobs
            pa, pb = a + 0.04 + p * 0.57, a + 0.04 + p * 0.57 + 0.55
            bx(m, "AC_GWoodLight", pa, pb, yd - 0.015, yd, Z0 + 0.15, zc - 0.06, faces="xXyzZ")
            kx = pb - 0.05 if p == 0 else pa + 0.05
            bx(m, "AC_GBrass", kx - 0.012, kx + 0.012, yd - 0.04, yd - 0.015, zc - 0.25, zc - 0.21)
        bx(m, "AC_GWoodLight", a, b, yd - 0.02, yw, zc, zc + 0.03, faces="xXyZ" if k in (0, n - 1) else "yZ")
        # mirror back
        face_n(m, "AC_GMirror", [(a, yw - 0.01, zg0), (b, yw - 0.01, zg0), (b, yw - 0.01, zg1), (a, yw - 0.01, zg1)],
               (0, -1, 0))
        # case top (inside) and side frames
        bx(m, "AC_GPaintWhite", a, b, yu, yw, zg1, zg1 + 0.12, faces="yz")
        for xx in (a, b - 0.07):
            bx(m, "AC_GPaintWhite", xx, xx + 0.07, yu - 0.03, yw, zg0, zg1 + 0.12, faces="xXyZ")
        # nickel uprights and glass shelves
        for xx in (a + 0.12, b - 0.12):
            cyl(m, "AC_GChrome", xx, yu + 0.03, zg0, zg1, 0.011, seg=6)
        rows = (zg0, zg0 + 0.42, zg0 + 0.82, zg0 + 1.18)
        for j, zs in enumerate(rows):
            if j > 0:
                bx(m, "AC_GGlass", a + 0.07, b - 0.07, yu, yw - 0.02, zs - 0.008, zs)
            tall = (rows[j + 1] - zs - 0.03) if j + 1 < len(rows) else zg1 - zs - 0.03
            if j == 0:
                # worktop: 3-litre jars, sugar loaves, bigger jars
                xx = a + 0.15
                while xx < b - 0.2:
                    t = rnd.random()
                    if t < 0.45:
                        three_litre_jar(m, xx, yw - 0.2, zs, rnd.choice(["AC_GRed", "AC_GYellow", "AC_GGreen", "AC_GBrown"]))
                        xx += 0.21
                    elif t < 0.65:
                        sugar_loaf(m, xx, yw - 0.2, zs)
                        xx += 0.2
                    else:
                        goods_row(m, rnd, xx - 0.05, min(xx + 0.3, b - 0.1), yw - 0.2, zs, tall=tall,
                                  kinds=["box", "packet", "tins", "jar"])
                        xx += 0.33
            else:
                goods_row(m, rnd, a + 0.08, b - 0.08, yw - 0.18, zs, tall=min(tall, 0.33))
    # white frame round the run, head beam and cornice shelf with consoles
    bx(m, "AC_GPaintWhite", x0 - 0.1, x1 + 0.1, yu - 0.05, yw, zg1 + 0.12, zg1 + 0.32, faces="xXyz")
    zs = zg1 + 0.32
    bx(m, "AC_GPaintWhite", x0 - 0.15, x1 + 0.15, yw - 0.5, yw, zs, zs + 0.05, faces="xXyzZ")
    for k in range(0, n + 1, 2):
        xx = x0 + k * CASE_BAY
        bx(m, "AC_GPaintWhite", xx - 0.04, xx + 0.04, yw - 0.16, yu - 0.05, zg1 - 0.15, zg1 + 0.12, faces="xXyz")
        bx(m, "AC_GPaintWhite", xx - 0.03, xx + 0.03, yw - 0.45, yu - 0.05, zs - 0.18, zs, faces="xXyz")
    for xx in (x0 - 0.1, x1 + 0.02):
        bx(m, "AC_GPaintWhite", xx, xx + 0.08, yu - 0.06, yw, Z0, zg1 + 0.32, faces="xXyZ")
    # bottles and big jars on the cornice shelf
    x = x0 - 0.05
    while x < x1 + 0.05:
        if rnd.random() < 0.12:
            three_litre_jar(m, x + 0.09, yw - 0.25, zs + 0.05, rnd.choice(["AC_GGlass", "AC_GGreen"]))
            x += 0.2
        else:
            bottle(m, rnd, x + 0.04, yw - 0.25, zs + 0.05, 0.036, rnd.uniform(0.28, 0.36))
            x += rnd.uniform(0.09, 0.13)


def service(m):
    """Service aisle: deep ceramic sink and a work table at the north end, behind the counter."""
    hx1 = HALL[1]
    # sink against the north wall
    sx0, sx1, sy0, sy1 = hx1 - 0.6, hx1, -0.15, 0.55
    bx(m, "AC_GEnamel", sx0, sx1, sy0, sy1, Z0 + 0.55, Z0 + 0.85, faces="xyYZ")
    bx(m, "AC_GChrome", sx0 + 0.08, sx0 + 0.12, sy0 + 0.1, sy1 - 0.1, Z0, Z0 + 0.55, faces="xXyY")
    cyl(m, "AC_GChrome", hx1 - 0.08, (sy0 + sy1) / 2, Z0 + 0.85, Z0 + 1.15, 0.012, seg=6)
    tube(m, "AC_GChrome", (hx1 - 0.08, (sy0 + sy1) / 2, Z0 + 1.15), (hx1 - 0.25, (sy0 + sy1) / 2, Z0 + 1.1),
         0.012, 0.01, 6)


def kassa(m):
    """Cashier's booth: oak lower panels, glazed upper part with a small pay window, a cornice and
    the till inside."""
    x0, x1, y0, y1 = KASSA
    zl, zh = Z0 + 1.1, Z0 + 2.15
    bx(m, "AC_GWood", x0, x1, y0, y1, Z0, zl, faces="xXyYZ")
    for (a, b) in ((x0 + 0.08, (x0 + x1) / 2 - 0.04), ((x0 + x1) / 2 + 0.04, x1 - 0.08)):
        bx(m, "AC_GWoodLight", a, b, y0 - 0.015, y0, Z0 + 0.15, zl - 0.12, faces="xXyzZ")
    # ledge for coins on the customer side
    bx(m, "AC_GWoodLight", x0 + 0.1, x1 - 0.1, y0 - 0.2, y0, zl, zl + 0.03)
    # corner posts
    for xx in (x0, x1 - 0.06):
        for yy in (y0, y1 - 0.06):
            bx(m, "AC_GWood", xx, xx + 0.06, yy, yy + 0.06, zl, zh, faces="xXyY")
    # glass panes (front has a pay window cut out at the bottom middle)
    g = 0.006
    pw0, pw1 = (x0 + x1) / 2 - 0.15, (x0 + x1) / 2 + 0.15
    bx(m, "AC_GGlass", x0 + 0.06, pw0, y0 + 0.02, y0 + 0.02 + g, zl, zh)
    bx(m, "AC_GGlass", pw1, x1 - 0.06, y0 + 0.02, y0 + 0.02 + g, zl, zh)
    bx(m, "AC_GGlass", pw0, pw1, y0 + 0.02, y0 + 0.02 + g, zl + 0.2, zh)
    bx(m, "AC_GWood", pw0 - 0.02, pw1 + 0.02, y0, y0 + 0.04, zl + 0.2, zl + 0.24)
    bx(m, "AC_GGlass", x0 + 0.02, x0 + 0.02 + g, y0 + 0.06, y1 - 0.06, zl, zh)
    bx(m, "AC_GGlass", x1 - 0.02 - g, x1 - 0.02, y0 + 0.06, y1 - 0.06, zl, zh)
    # back: a door (closed) below the glass
    bx(m, "AC_GGlass", x0 + 0.06, x1 - 0.06, y1 - 0.02 - g, y1 - 0.02, zl + 0.3, zh)
    bx(m, "AC_GWood", x0 + 0.06, x1 - 0.06, y1 - 0.06, y1 - 0.02, zl, zl + 0.3)
    # roof with a small cornice and the sign board
    bx(m, "AC_GWood", x0 - 0.03, x1 + 0.03, y0 - 0.03, y1 + 0.03, zh, zh + 0.06)
    bx(m, "AC_GWood", x0 - 0.06, x1 + 0.06, y0 - 0.06, y1 + 0.06, zh + 0.06, zh + 0.1)
    board(m, "sign.gastronom.cashier", (x0 + x1) / 2, y0 - 0.06, zh + 0.32, 0.9, 0.3, (0, -1), frame=0.02)
    bx(m, "AC_GWood", (x0 + x1) / 2 - 0.47, (x0 + x1) / 2 + 0.47, y0 - 0.06, y0 - 0.04, zh + 0.1, zh + 0.15)
    # inside: shelf with the till, a stool
    bx(m, "AC_GWoodLight", x0 + 0.06, x1 - 0.06, y0 + 0.03, y0 + 0.5, zl - 0.02, zl + 0.01, faces="yZ")
    cx = (x0 + x1) / 2
    bx(m, "AC_GChrome", cx - 0.2, cx + 0.2, y0 + 0.12, y0 + 0.45, zl + 0.01, zl + 0.18)
    p = [(cx - 0.2, y0 + 0.12, zl + 0.18), (cx + 0.2, y0 + 0.12, zl + 0.18), (cx + 0.2, y0 + 0.45, zl + 0.36),
         (cx - 0.2, y0 + 0.45, zl + 0.36)]
    face_n(m, "AC_GChrome", p, (0, -1, 1))
    for s in (-1, 1):
        face_n(m, "AC_GChrome", [(cx + s * 0.2, y0 + 0.12, zl + 0.18), (cx + s * 0.2, y0 + 0.45, zl + 0.18),
                                 (cx + s * 0.2, y0 + 0.45, zl + 0.36)], (s, 0, 0))
    bx(m, "AC_GChrome", cx - 0.2, cx + 0.2, y0 + 0.45, y0 + 0.47, zl + 0.18, zl + 0.36, faces="Y")
    for j in range(4):                                 # keys
        bx(m, "AC_GEnamel", cx - 0.15 + j * 0.08, cx - 0.1 + j * 0.08, y0 + 0.2, y0 + 0.25, zl + 0.22 + 0.0,
           zl + 0.25, faces="xXyYZ")
    cyl(m, "AC_GWood", cx, y1 - 0.35, Z0, Z0 + 0.65, 0.17, seg=10, top="AC_GWoodLight")


def lamps(m):
    for x, y in LAMP_XY:
        z0 = LAMP_Z
        cyl(m, "AC_GPaintWhite", x, y, ZC - 0.04, ZC, 0.06, seg=10, bottom="AC_GPaintWhite")   # ceiling rose
        bx(m, "AC_GCord", x - 0.004, x + 0.004, y - 0.004, y + 0.004, z0 + LAMP_H + 0.12, ZC - 0.04, faces="xXyY")
        tube(m, "AC_GCeramicBlue", (x, y, z0 + LAMP_H + 0.12), (x, y, z0 + LAMP_H - 0.005), 0.025, 0.075, 12,
             cap0="AC_GCeramicBlue")
        # opal cylinder, slightly rounded at the bottom
        tube(m, "AC_GOpal", (x, y, z0 + 0.04), (x, y, z0 + LAMP_H), LAMP_R, LAMP_R * 0.93, 16, cap1="AC_GOpal")
        tube(m, "AC_GOpal", (x, y, z0), (x, y, z0 + 0.04), LAMP_R * 0.82, LAMP_R, 16, cap0="AC_GOpal")


def decorations(m):
    hx0, hx1, hy0, hy1 = HALL
    vx0, vx1, vy0, vy1 = VEST
    # price lists on the north wall (facing -x), over the customers' area
    for k, yy in enumerate((-5.2, -4.3, -3.4)):
        board(m, "sign.gastronom.price_list", hx1, yy, Z0 + 1.75, 0.5, 0.75, (-1, 0))
    # south wall of the hall, between the arch and the window wall (facing +x)
    board(m, "sign.gastronom.rules", hx0, -5.8, Z0 + 1.7, 0.45, 0.7, (1, 0))
    # vestibule: the shop's name board on the back wall, opening hours by the arch
    board(m, "sign.gastronom.name", 0.0, vy1, Z0 + 2.55, 1.8, 0.45, (0, -1))
    board(m, "sign.gastronom.hours", vx1, -5.6, Z0 + 1.55, 0.4, 0.55, (-1, 0))
    # a wall clock over the arch (hall side)
    tube(m, "AC_GWood", (hx0, (ARCH_Y[0] + ARCH_Y[1]) / 2, ARCH_TOP + 0.3),
         (hx0 + 0.06, (ARCH_Y[0] + ARCH_Y[1]) / 2, ARCH_TOP + 0.3), 0.17, 0.17, 16, cap1="AC_GWood")
    tube(m, "AC_GEnamel", (hx0 + 0.06, (ARCH_Y[0] + ARCH_Y[1]) / 2, ARCH_TOP + 0.3),
         (hx0 + 0.065, (ARCH_Y[0] + ARCH_Y[1]) / 2, ARCH_TOP + 0.3), 0.14, 0.14, 16, cap1="AC_GEnamel")
    # vestibule bench along the back wall
    bx(m, "AC_GWood", -1.6, 1.6, vy1 - 0.4, vy1 - 0.02, Z0 + 0.42, Z0 + 0.46)
    for xx in (-1.5, 1.42):
        bx(m, "AC_GWood", xx, xx + 0.08, vy1 - 0.38, vy1 - 0.04, Z0, Z0 + 0.42, faces="xXyY")


YO = -7.25              # outer face of the east wall
DOOR_LEAF_Y = -7.02     # the facade's door leaves sit at about y = -7.13: the threshold stops short of them


def opening_liners(m):
    """Inward-facing liners of the east-wall openings through the wall thickness (y -7.25..-6.75),
    flush with the hole edges. (plan.build_facade's own reveals face away from the opening, so from
    inside they are culled in Unreal.) Doors also get a stone threshold flush with the floor: top at
    Z0 from the inner face to DOOR_LEAF_Y, a riser down to the door sill (z 0.55) there."""
    e = 0.0
    for (cx, sill, wd, ht), is_door in [(ENTRANCE, True), (HALL_DOOR, True)] + [(w, False) for w in SHOP_WINDOWS]:
        u0, u1, z0, z1 = cx - wd / 2 + e, cx + wd / 2 - e, sill, sill + ht
        face_n(m, "AC_Trim", [(u0, YO, z0), (u0, YI, z0), (u0, YI, z1), (u0, YO, z1)], (1, 0, 0))
        face_n(m, "AC_Trim", [(u1, YO, z0), (u1, YI, z0), (u1, YI, z1), (u1, YO, z1)], (-1, 0, 0))
        face_n(m, "AC_Trim", [(u0, YO, z1 - e), (u1, YO, z1 - e), (u1, YI, z1 - e), (u0, YI, z1 - e)], (0, 0, -1))
        if is_door:
            face_n(m, "AC_Trim", [(u0, YO, z0 + e), (u1, YO, z0 + e), (u1, DOOR_LEAF_Y, z0 + e), (u0, DOOR_LEAF_Y, z0 + e)],
                   (0, 0, 1))
            face_n(m, "AC_GFloor", [(u0, DOOR_LEAF_Y, Z0), (u1, DOOR_LEAF_Y, Z0), (u1, YI, Z0), (u0, YI, Z0)], (0, 0, 1))
            face_n(m, "AC_GFloor", [(u0, DOOR_LEAF_Y, z0), (u1, DOOR_LEAF_Y, z0), (u1, DOOR_LEAF_Y, Z0),
                                        (u0, DOOR_LEAF_Y, Z0)], (0, -1, 0))
        else:   # full-depth sill; the inner window board (top 6 mm higher) covers its last 0.2 m
            face_n(m, "AC_Trim", [(u0, YO, z0), (u1, YO, z0), (u1, YI, z0), (u0, YI, z0)], (0, 0, 1))


def backs(m, t=0.15):
    """Outward-facing plaster backs of the interior walls that border spaces this module does not
    build (vestibule south and west walls, hall back wall, hall north end), set t into the wall, so
    neither room can be seen into from a neighbouring unbuilt space. Full rectangles, no holes."""
    hx0, hx1, hy0, hy1 = HALL
    vx0, vx1, vy0, vy1 = VEST
    for pts, n in (
        ([(vx0 - t, vy0, Z0), (vx0 - t, vy1, Z0), (vx0 - t, vy1, ZC), (vx0 - t, vy0, ZC)], (-1, 0, 0)),
        ([(vx0, vy1 + t, Z0), (vx1, vy1 + t, Z0), (vx1, vy1 + t, ZC), (vx0, vy1 + t, ZC)], (0, 1, 0)),
        ([(vx1, vy1 + t, Z0), (hx0, vy1 + t, Z0), (hx0, vy1 + t, ZC), (vx1, vy1 + t, ZC)], (0, 1, 0)),
        ([(hx0 - t, vy1 + t, Z0), (hx0 - t, hy1 + t, Z0), (hx0 - t, hy1 + t, ZC), (hx0 - t, vy1 + t, ZC)], (-1, 0, 0)),
        ([(hx0 - t, hy1 + t, Z0), (hx1 + t, hy1 + t, Z0), (hx1 + t, hy1 + t, ZC), (hx0 - t, hy1 + t, ZC)], (0, 1, 0)),
        ([(hx1 + t, hy0, Z0), (hx1 + t, hy1 + t, Z0), (hx1 + t, hy1 + t, ZC), (hx1 + t, hy0, ZC)], (1, 0, 0)),
    ):
        face_n(m, "AC_GPlaster", pts, n)


def build(with_backs=True):
    """Returns the interior as a geom.Mesh in the Novikova 1 house frame (metres). Also fills LABELS."""
    del LABELS[:]
    del LABEL_NORMALS[:]
    rnd = random.Random(7)
    m = Mesh()
    walls(m)
    opening_liners(m)
    if with_backs:
        backs(m)
    column(m)
    counter(m, rnd)
    back_cases(m, rnd)
    service(m)
    kassa(m)
    lamps(m)
    decorations(m)
    return m


def clearances():
    """Clear width/height (m) of the passages the player uses: the entrance and the arch."""
    return {
        "entrance": (ENTRANCE[2], ENTRANCE[1] + ENTRANCE[3] - Z0),
        "hall_door": (HALL_DOOR[2], HALL_DOOR[1] + HALL_DOOR[3] - Z0),
        "arch": (ARCH_Y[1] - ARCH_Y[0], ARCH_TOP - Z0),
        "counter_end_gap": (HALL[1] - COUNTER[1], ZC - Z0),
    }


# --- self-check: is the interior closed when seen from inside? -------------------------------------
ENCLOSURE = {"AC_GTileBlue", "AC_GTileIvory", "AC_GPlaster", "AC_GFloor", "AC_Door", "AC_Trim"}
AUDIT_POINTS = ([(0.0, -4.0, 2.3), (-2.0, -6.4, 1.0), (2.0, -2.0, 3.9), (0.0, -6.6, 0.9), (2.7, -3.6, 2.0),
                 (4.0, -6.2, 1.2), (10.0, -5.0, 2.3), (17.6, -6.4, 3.95), (15.0, -2.0, 1.0), (3.4, -4.0, 3.9),
                 (17.5, 0.3, 1.5), (8.0, 0.4, 1.2), (3.3, 0.5, 2.5), (12.0, -6.6, 0.8), (17.25, 1.45, 1.5),
                 (-2.4, -3.6, 1.5)]
                # a grid over vestibule, hall and service aisle (below the cornice, which reaches 0.3 m out)
                + [(x, y, z) for x in (-2.2, 0.0, 2.2, 3.3, 6.0, 9.0, 12.0, 15.0, 17.8) for y in (-6.6, -4.0, -1.7)
                   for z in (0.75, 2.0, 3.8) if not (x < 2.45 and y > -1.5)]
                + [(x, 0.3, z) for x in (3.2, 9.0, 17.8) for z in (0.75, 2.0, 3.8)])


def _tris(mesh, mats):
    out = []
    for mat, idx, _ in mesh.faces:
        if mats is not None and mat not in mats:
            continue
        P = [mesh.verts[i] for i in idx]
        for k in range(1, len(P) - 1):
            a, b, c = P[0], P[k], P[k + 1]
            e1, e2 = sub(b, a), sub(c, a)
            out.append((a, e1, e2, cross(e1, e2), mat))
    return out


def _openings():
    """East-wall openings as (x0, x1, z0, z1): a ray that leaves through one of these is a view out."""
    return [(cx - w / 2, cx + w / 2, s, s + h) for cx, s, w, h in [ENTRANCE, HALL_DOOR] + SHOP_WINDOWS]


def audit(mesh=None, n_dirs=500, points=None, verbose=True):
    """Casts rays from interior points in n_dirs directions against the room enclosure (walls,
    floors, ceilings, cornices, doors, liners: ENCLOSURE materials; props are ignored since they
    stand inside). A ray is fine if the first face it hits faces the ray's origin, or if it leaves
    through an east-wall opening (crosses y = -7.25 inside a door/window rectangle). Otherwise it is
    reported: 'escape' (hits nothing) or 'backface' (first hit faces away: culled in Unreal, so
    the player would look through it). Returns {'rays', 'escape', 'backface', 'opening', 'problems'}."""
    mesh = mesh or build()
    tris = _tris(mesh, ENCLOSURE)
    ops = _openings()
    pts = points or AUDIT_POINTS
    dirs = []
    ga = math.pi * (3 - math.sqrt(5))
    for i in range(n_dirs):
        z = 1 - 2 * (i + 0.5) / n_dirs
        r = math.sqrt(1 - z * z)
        dirs.append((r * math.cos(ga * i), r * math.sin(ga * i), z))
    res = {"rays": 0, "escape": 0, "backface": 0, "opening": 0, "problems": []}

    def cast(o, d):
        best, hit = 1e9, None
        for a, e1, e2, n, mat in tris:
            p = cross(d, e2)
            det = _dot(e1, p)
            if abs(det) < 1e-12:
                continue
            inv = 1.0 / det
            s = sub(o, a)
            u = _dot(s, p) * inv
            if u < 0.0 or u > 1.0:
                continue
            q = cross(s, e1)
            v = _dot(d, q) * inv
            if v < 0.0 or u + v > 1.0:
                continue
            t = _dot(e2, q) * inv
            if 1e-6 < t < best:
                best, hit = t, (n, mat)
        if d[1] < -1e-9:                    # out through an east-wall opening before the first hit?
            t_out = (YO - o[1]) / d[1]
            if t_out < best:
                x, z = o[0] + d[0] * t_out, o[2] + d[2] * t_out
                if any(x0 <= x <= x1 and z0 <= z <= z1 for x0, x1, z0, z1 in ops):
                    return "opening", None
        if hit is None:
            return "escape", None
        if _dot(hit[0], d) > 0:
            return "backface", (tuple(round(o[k] + d[k] * best, 3) for k in range(3)), hit[1])
        return "ok", None

    for o in pts:
        for d in dirs:
            res["rays"] += 1
            kind, info = cast(o, d)
            if kind in ("escape", "backface"):
                # a ray that grazes a shared edge exactly is not a hole: retry with two tiny jitters
                retry = [cast(o, norm((d[0] + j[0], d[1] + j[1], d[2] + j[2])))[0]
                         for j in ((1e-4, -7e-5, 3e-5), (-6e-5, 1e-4, -8e-5))]
                if any(r in ("ok", "opening") for r in retry):
                    kind = "ok"
            if kind == "opening":
                res["opening"] += 1
            elif kind != "ok":
                res[kind] += 1
                res["problems"].append((kind, o, tuple(round(c, 3) for c in d), info))
    if verbose:
        print("audit: %d rays, %d out through openings, %d escape, %d backface" %
              (res["rays"], res["opening"], res["escape"], res["backface"]))
        for p in res["problems"][:20]:
            print("  ", p)
    return res


if __name__ == "__main__":
    import collections
    mesh = build()
    print("triangles", mesh.tri_count(), "faces", len(mesh.faces), "verts", len(mesh.verts))
    print(collections.Counter(f[0] for f in mesh.faces).most_common())
    print("labels", len(LABELS))
    used = set(f[0] for f in mesh.faces)
    print("unknown materials", sorted(u for u in used if u not in GASTRONOM_MATERIALS))
    print(clearances())
    import sys
    if "--audit" in sys.argv:
        audit(mesh, n_dirs=600)
