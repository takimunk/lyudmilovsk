"""Furniture and room finishes for the flats of the Academgorodok of Laboratory No. 2 (Moscow,
1945-48; nuclear physicists' flats of the late 1940s-1950s). Pure Python (no bpy): every builder
returns a geom.Mesh, so it can be tested with plain python3 (run this file for the self-test).

Sources: docs/academgorodok/interior_notes.md (b) flat finishes, (d) the museum's furniture list and
sizes; photos in docs/academgorodok/refs/interior/ (m24_2016_flat_04/05/07/09/10,
commons_Museum_of_everyday_life_of_soviet_scientists_02/03/04/07/08, aif_2019_02,
m24_2015_museum_02/06).

Props (PROPS below): built at the origin, floor at z = 0, front facing -y, centred in x (and in y).
Ceiling fittings (chandelier, ceiling_rose, pendant, ceiling_globe) hang from z = 0 downwards.
Wall-hung props (pictures, clocks, wall cupboard, mirror) have their back at +y and their bottom
at z = 0; the layout lifts them.

furnish(room) -> geom.Mesh in the HOUSE frame. room = {
    "kind": "living" | "bedroom" | "study" | "kitchen" | "bath" | "corridor" | "storage",
    "x0", "x1", "y0", "y1": floor rectangle (inner faces of the walls), "z": floor level,
    "h": clear height, "seed": int,
    "doors":   [{"side": "x0"|"x1"|"y0"|"y1", "c": centre, "w": clear width, "h": height (opt., 2.3)}],
    "windows": [{"side": ..., "c": centre, "w": width, "sill": sill over the floor, "h": height}]}
"c" is the house-frame coordinate of the opening centre along that wall (x for the y0/y1 walls,
y for the x0/x1 walls). furnish_parts(room) returns the same content split up (finish, furniture,
curtains), plus the lights, the placed items and the clear zones; furnish() merges the meshes.

Rules kept by the layout (and asserted by the self-test):
  * nothing below DOOR_BOX_H inside a DOOR_BOX_D deep x (door width + 2 * DOOR_BOX_SIDE) box in
    front of each door on the room side (finish layers lie within FINISH_T of the walls/floor);
  * nothing taller than the sill within WINDOW_ZONE_D in front of a window, except the curtains;
  * a ROUTE_W wide route joins every door (and the first window) through the furniture.
"""
import math
import random

from geom import Frame, Mesh, box, cross, norm, sub, wall_with_holes

# --- materials ---------------------------------------------------------------------------------------
FURNITURE_MATERIALS = {
    # name: (base colour sRGB hex, roughness, what it is)
    "AC_FWalnut":        ("#3E2516", 0.30, "dark polished walnut / mahogany-stained furniture, skirting boards"),
    "AC_FOak":           ("#8A5A2E", 0.40, "honey-varnished oak: sectional bookcases, wardrobes, clock cases"),
    "AC_FBeech":         ("#B07A45", 0.45, "light varnished beech: dining chairs, kitchen table, stools, skis"),
    "AC_FLeather":       ("#4A2A1A", 0.45, "dark brown buttoned leather: club armchair, suitcases, cases"),
    "AC_FLeatherGreen":  ("#2F4A32", 0.50, "green leather inlay of the writing desk top"),
    "AC_FDamaskRed":     ("#8E2A22", 0.85, "red-gold damask upholstery: sofa, armchairs, dining chair seats"),
    "AC_FPlushGold":     ("#A8783A", 0.85, "mustard-gold plush: sofa variant, lamp shades, radio cloth"),
    "AC_FLinen":         ("#EEEBE2", 0.90, "white linen: sheets, pillows, tablecloths, towels, rug fringes"),
    "AC_FLace":          ("#F3EFE4", 0.90, "lace and tulle: window tulle, pillow covers, bed valance, runners (translucent)"),
    "AC_FCurtainGreen":  ("#4F5B3A", 0.90, "heavy olive-green portiere curtains"),
    "AC_FCurtainWine":   ("#6A2329", 0.90, "heavy wine-red portiere curtains"),
    "AC_FWool":          ("#8C3A30", 0.95, "red check wool: blankets, chenille table cloth, coats"),
    "AC_FBrass":         ("#B08D45", 0.30, "brass: chandelier, knobs, curtain rods, frames, samovar (metallic)"),
    "AC_FChrome":        ("#C8CACC", 0.15, "nickel / chrome: taps, pipes, lens frame, stays (metallic)"),
    "AC_FOpalGlass":     ("#FFF6E6", 0.30, "opal glass of lamp globes and bowls (emissive when lit)"),
    "AC_FGreenGlass":    ("#2E6B3E", 0.10, "green cased glass / enamel desk lamp shades"),
    "AC_FEnamelWhite":   ("#F2F0E8", 0.20, "white enamel / white-painted wood: stove, sink, kitchen dresser, cupboards"),
    "AC_FEnamelCream":   ("#E8DFC4", 0.25, "cream enamel / paint: radiators, tub outside, pots, water heaters"),
    "AC_FCastIron":      ("#2E2C2A", 0.60, "black cast iron: stove grates, bed frames, cistern, legs, pipes"),
    "AC_FPorcelain":     ("#F7F6F2", 0.10, "glazed porcelain: dishes, washbasin, toilet, piano keys"),
    "AC_FParquet":       ("#9A6A3A", 0.35, "waxed oak herringbone parquet; boards 7 x 40 cm along the UV u axis, UVs turned +-45 deg in strips"),
    "AC_FTileFloor":     ("#B08A6A", 0.40, "metlakh floor tiles 10 x 10 cm (kitchen, bath), tile skirting"),
    "AC_FTileWall":      ("#E9E6DC", 0.15, "white glazed wall tiles 15 x 15 cm: splashbacks behind stove, sink, tub"),
    "AC_FWallpaperCream":("#E6D8B0", 0.85, "cream damask wallpaper with a small pattern"),
    "AC_FWallpaperGreen":("#C9D1A8", 0.85, "light green damask wallpaper"),
    "AC_FCeiling":       ("#F0EDE4", 0.95, "lime-white: ceilings, cornices, ceiling roses, frieze, walls above the dado"),
    "AC_FDadoPaint":     ("#8FA394", 0.35, "green-grey oil paint dado (kitchen, bath, corridor, storeroom)"),
    "AC_FBookRed":       ("#7A2420", 0.60, "book spines: maroon"),
    "AC_FBookBlue":      ("#24344F", 0.60, "book spines: dark blue (also globes, rug borders)"),
    "AC_FBookGreen":     ("#2F4A35", 0.60, "book spines: dark green (also leaves, stems)"),
    "AC_FBookBrown":     ("#6B4A2C", 0.60, "book spines: brown (also boxes, cardboard)"),
    "AC_FBookCream":     ("#D8CBA8", 0.70, "book spines: cream paperbacks"),
    "AC_FRugRed":        ("#7E2A24", 0.95, "carpet field: red (also flowers)"),
    "AC_FRugBlue":       ("#2B3550", 0.95, "carpet border / field: dark blue"),
    "AC_FMirror":        ("#D8DCDC", 0.02, "silvered mirror glass (metallic)"),
    "AC_FGlass":         ("#DDEBEA", 0.03, "clear glass: bookcase doors, shelves, jars, TV lens (transparent)"),
    "AC_FPaper":         ("#E9E2CC", 0.80, "paper: newspapers, photos, dials, labels, passe-partouts"),
    "AC_FBlackLacquer":  ("#121110", 0.15, "black polish / carbolite: piano, telephone, typewriter, photo prints"),
    "AC_FCanvas":        ("#6B5A3A", 0.70, "dark varnished oil paintings"),
    "AC_FOilcloth":      ("#C9D7C4", 0.35, "pale green checked oilcloth on the kitchen table"),
}

WAL, OAK, BEECH = "AC_FWalnut", "AC_FOak", "AC_FBeech"
LEA, LEAG, DAM, PLUSH = "AC_FLeather", "AC_FLeatherGreen", "AC_FDamaskRed", "AC_FPlushGold"
LIN, LACE, CURG, CURW, WOOL = "AC_FLinen", "AC_FLace", "AC_FCurtainGreen", "AC_FCurtainWine", "AC_FWool"
BRASS, CHROME, OPAL, GGL = "AC_FBrass", "AC_FChrome", "AC_FOpalGlass", "AC_FGreenGlass"
ENW, ENC, IRON, PORC = "AC_FEnamelWhite", "AC_FEnamelCream", "AC_FCastIron", "AC_FPorcelain"
PARQ, TFL, TWALL = "AC_FParquet", "AC_FTileFloor", "AC_FTileWall"
WPC, WPG, CEIL, DADO = "AC_FWallpaperCream", "AC_FWallpaperGreen", "AC_FCeiling", "AC_FDadoPaint"
BRED, BBLUE, BGREEN, BBROWN, BCREAM = "AC_FBookRed", "AC_FBookBlue", "AC_FBookGreen", "AC_FBookBrown", "AC_FBookCream"
RUGR, RUGB, MIR, GLASS, PAPER = "AC_FRugRed", "AC_FRugBlue", "AC_FMirror", "AC_FGlass", "AC_FPaper"
BLACK, CANVAS, OILC = "AC_FBlackLacquer", "AC_FCanvas", "AC_FOilcloth"
BOOKS = [BBROWN, BBROWN, BRED, BRED, BBLUE, BBLUE, BGREEN, BCREAM]

CURTAIN_MATS = {CURG, CURW, LACE}          # materials of the curtain part (exempt from window zones)

# --- clear-zone rules ----------------------------------------------------------------------------------
DOOR_BOX_D = 1.2         # depth of the clear box in front of a door
DOOR_BOX_SIDE = 0.2      # the box is the door width + 2 x this
DOOR_BOX_H = 2.3         # ... and reaches this high over the floor (chandeliers hang above)
DOOR_H = 2.3             # door opening height when a door has no "h"
WINDOW_ZONE_D = 0.7      # nothing taller than the sill this deep in front of a window
ROUTE_W = 0.8            # walking route width between the doors
ROUTE_HEAD = 1.9         # things lower than this (and above 5 cm) block the route
FINISH_T = 0.035         # finish layers stay this close to the walls / floor
WALL_OFF = 0.005         # wallpaper / paint layer off the wall face
CORNICE = 0.25           # cornice height and projection
TRI_BUDGET = 12000

LIGHT_WARM = "#FFD2A0"   # ~2700 K incandescent behind opal glass


# --- geometry helpers -------------------------------------------------------------------------------
def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _newell(pts):
    nx = ny = nz = 0.0
    n = len(pts)
    for i in range(n):
        a, b = pts[i], pts[(i + 1) % n]
        nx += (a[1] - b[1]) * (a[2] + b[2])
        ny += (a[2] - b[2]) * (a[0] + b[0])
        nz += (a[0] - b[0]) * (a[1] + b[1])
    return (nx, ny, nz)


def face_n(m, mat, pts, n, uvs=None):
    """Adds a face, reversed if needed so that its normal points along n."""
    if _dot(_newell(pts), n) < 0:
        pts = list(reversed(pts))
        uvs = list(reversed(uvs)) if uvs else None
    m.face(mat, pts, uvs)


def _centroid(pts):
    k = len(pts)
    return (sum(p[0] for p in pts) / k, sum(p[1] for p in pts) / k, sum(p[2] for p in pts) / k)


def face_out(m, mat, pts, c, uvs=None):
    """Face oriented away from the point c (for convex solids)."""
    q = _centroid(pts)
    face_n(m, mat, pts, (q[0] - c[0], q[1] - c[1], q[2] - c[2]), uvs)


def bx(m, mat, x0, x1, y0, y1, z0, z1, faces="all"):
    x0, x1 = min(x0, x1), max(x0, x1)
    y0, y1 = min(y0, y1), max(y0, y1)
    z0, z1 = min(z0, z1), max(z0, z1)
    if x1 - x0 < 1e-5 or y1 - y0 < 1e-5 or z1 - z0 < 1e-5:
        return
    box(m, mat, x0, x1, y0, y1, z0, z1, faces)


def bxc(m, mat, cx, cy, w, d, z0, z1, faces="all"):
    bx(m, mat, cx - w / 2, cx + w / 2, cy - d / 2, cy + d / 2, z0, z1, faces)


def cbox(m, mat, x0, x1, y0, y1, z0, z1, r, skip="", inward=False):
    """Box with chamfered edges and corners (r); skip: letters of xXyYzZ faces to leave out;
    inward: faces point into the box (a bowl's inside)."""
    lo, hi = (x0, y0, z0), (x1, y1, z1)
    r = max(1e-4, min(r, (x1 - x0) * 0.49, (y1 - y0) * 0.49, (z1 - z0) * 0.49))
    ilo, ihi = (x0 + r, y0 + r, z0 + r), (x1 - r, y1 - r, z1 - r)
    c = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)

    def P(spec):
        return tuple((hi[i] if s > 0 else lo[i]) if o else (ihi[i] if s > 0 else ilo[i])
                     for i, (s, o) in enumerate(spec))

    def add(pts):
        if inward:
            q = _centroid(pts)
            face_n(m, mat, pts, (c[0] - q[0], c[1] - q[1], c[2] - q[2]))
        else:
            face_out(m, mat, pts, c)

    letters = {(0, -1): "x", (0, 1): "X", (1, -1): "y", (1, 1): "Y", (2, -1): "z", (2, 1): "Z"}
    for a in range(3):
        o1, o2 = [i for i in range(3) if i != a]
        for s in (-1, 1):
            if letters[(a, s)] in skip:
                continue
            pts = []
            for sb, sc in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                spec = [None] * 3
                spec[a], spec[o1], spec[o2] = (s, True), (sb, False), (sc, False)
                pts.append(P(spec))
            add(pts)
    for a, b in ((0, 1), (0, 2), (1, 2)):
        cax = 3 - a - b
        for sa in (-1, 1):
            for sb in (-1, 1):
                pts = []
                for (oa, ob, sc) in ((True, False, -1), (True, False, 1), (False, True, 1), (False, True, -1)):
                    spec = [None] * 3
                    spec[a], spec[b], spec[cax] = (sa, oa), (sb, ob), (sc, False)
                    pts.append(P(spec))
                add(pts)
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                pts = [P([(sx, True), (sy, False), (sz, False)]), P([(sx, False), (sy, True), (sz, False)]),
                       P([(sx, False), (sy, False), (sz, True)])]
                add(pts)


def obox(m, mat, c, ax, ay, az, hx, hy, hz, faces="all"):
    """Oriented box: centre c, unit axes ax/ay/az, half sizes."""
    def P(i, j, k):
        return tuple(c[t] + i * hx * ax[t] + j * hy * ay[t] + k * hz * az[t] for t in range(3))
    sides = {"x": [P(-1, j, k) for j, k in ((-1, -1), (1, -1), (1, 1), (-1, 1))],
             "X": [P(1, j, k) for j, k in ((-1, -1), (1, -1), (1, 1), (-1, 1))],
             "y": [P(i, -1, k) for i, k in ((-1, -1), (1, -1), (1, 1), (-1, 1))],
             "Y": [P(i, 1, k) for i, k in ((-1, -1), (1, -1), (1, 1), (-1, 1))],
             "z": [P(i, j, -1) for i, j in ((-1, -1), (1, -1), (1, 1), (-1, 1))],
             "Z": [P(i, j, 1) for i, j in ((-1, -1), (1, -1), (1, 1), (-1, 1))]}
    for k, pts in sides.items():
        if faces == "all" or k in faces:
            face_out(m, mat, pts, c)


def yawbox(m, mat, cx, cy, w, d, z0, z1, yaw, faces="all"):
    c, s = math.cos(yaw), math.sin(yaw)
    obox(m, mat, (cx, cy, (z0 + z1) / 2), (c, s, 0), (-s, c, 0), (0, 0, 1), w / 2, d / 2, (z1 - z0) / 2, faces)


def _basis(a):
    a = norm(a)
    t = (1.0, 0.0, 0.0) if abs(a[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = norm(cross(a, t))
    v = cross(a, u)
    return a, u, v


def tube(m, mat, c0, c1, r0, r1=None, seg=6, cap0=None, cap1=None):
    """Frustum from c0 (radius r0) to c1 (r1); cap0/cap1: material of the end caps or None."""
    r1 = r0 if r1 is None else r1
    a, u, v = _basis(sub(c1, c0))
    ring = []
    for k in range(seg):
        t = 2 * math.pi * (k + 0.5) / seg
        ring.append(tuple(u[i] * math.cos(t) + v[i] * math.sin(t) for i in range(3)))
    p0 = [tuple(c0[i] + d[i] * r0 for i in range(3)) for d in ring]
    p1 = [tuple(c1[i] + d[i] * r1 for i in range(3)) for d in ring]
    for k in range(seg):
        j = (k + 1) % seg
        mid = tuple(ring[k][i] + ring[j][i] for i in range(3))
        if r1 < 1e-6:
            face_n(m, mat, [p0[k], p0[j], c1], mid)
        elif r0 < 1e-6:
            face_n(m, mat, [c0, p1[j], p1[k]], mid)
        else:
            face_n(m, mat, [p0[k], p0[j], p1[j], p1[k]], mid)
    if cap0 and r0 > 1e-6:
        face_n(m, cap0, p0, (-a[0], -a[1], -a[2]))
    if cap1 and r1 > 1e-6:
        face_n(m, cap1, p1, a)


def chain(m, mat, pts, r, seg=5, closed=False):
    """A bent rod through the points."""
    q = list(pts) + ([pts[0]] if closed else [])
    for a, b in zip(q, q[1:]):
        tube(m, mat, a, b, r, r, seg)


def lathe(m, mat, prof, seg=12, c=(0.0, 0.0, 0.0), sx=1.0, sy=1.0, mats=None, phase=0.0):
    """Surface of revolution about the vertical axis through c. prof: [(r, z)], traversed so that
    the solid lies to the right (bottom centre -> out -> up -> in to the top centre): normals out."""
    rings = []
    for r, z in prof:
        rings.append([(c[0] + r * sx * math.cos(phase + 2 * math.pi * j / seg),
                       c[1] + r * sy * math.sin(phase + 2 * math.pi * j / seg), c[2] + z) for j in range(seg)])
    for k in range(len(prof) - 1):
        (ra, za), (rb, zb) = prof[k], prof[k + 1]
        mk = mats[k] if mats else mat
        if ra < 1e-6 and rb < 1e-6:
            continue
        if abs(ra - rb) < 1e-9 and abs(za - zb) < 1e-9:
            continue
        A, B = rings[k], rings[k + 1]
        for j in range(seg):
            jj = (j + 1) % seg
            if ra < 1e-6:
                m.face(mk, [(c[0], c[1], c[2] + za), B[jj], B[j]])
            elif rb < 1e-6:
                m.face(mk, [A[j], A[jj], (c[0], c[1], c[2] + zb)])
            else:
                m.face(mk, [A[j], A[jj], B[jj], B[j]])


def sphere(m, mat, c, r, seg=8, rings=5, sz=1.0):
    prof = [(r * math.sin(math.pi * k / rings), -r * sz * math.cos(math.pi * k / rings)) for k in range(rings + 1)]
    lathe(m, mat, prof, seg, c)


def prism_y(m, mat, poly, y0, y1, front=True, back=True, sides=True):
    """Polygon [(x, z)] (convex) extruded along y from y0 (front) to y1."""
    c = (sum(p[0] for p in poly) / len(poly), (y0 + y1) / 2, sum(p[1] for p in poly) / len(poly))
    if front:
        face_out(m, mat, [(x, y0, z) for x, z in poly], c)
    if back:
        face_out(m, mat, [(x, y1, z) for x, z in poly], c)
    if sides:
        n = len(poly)
        for i in range(n):
            (ax, az), (bx_, bz) = poly[i], poly[(i + 1) % n]
            face_out(m, mat, [(ax, y0, az), (bx_, y0, bz), (bx_, y1, bz), (ax, y1, az)], c)


def disc_y(m, mat, cx, y, cz, r, seg=10):
    """A flat disc facing -y."""
    face_n(m, mat, [(cx + r * math.cos(2 * math.pi * k / seg), y, cz + r * math.sin(2 * math.pi * k / seg))
                    for k in range(seg)], (0, -1, 0))


def arc(cx, cz, r, a0, a1, n):
    return [(cx + r * math.cos(a0 + (a1 - a0) * k / n), cz + r * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]


def bbox(m):
    if not m.verts:
        return (0, 0, 0, 0, 0, 0)
    xs = [v[0] for v in m.verts]
    ys = [v[1] for v in m.verts]
    zs = [v[2] for v in m.verts]
    return (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))


def moved(part, dx=0.0, dy=0.0, dz=0.0, yaw=0.0):
    return Frame(dx, dy, yaw, dz).apply(part)


def add(m, part, dx=0.0, dy=0.0, dz=0.0, yaw=0.0):
    m.extend(moved(part, dx, dy, dz, yaw))


def rot_x90(part):
    """Turns a mesh built around +z so that +z points to -y (proper rotation about x)."""
    return part.transformed(lambda p: (p[0], -p[2], p[1]))


def _rng(rnd, seed=0):
    return rnd if rnd is not None else random.Random(seed)


# --- small clutter ----------------------------------------------------------------------------------
def books_row(m, rnd, x0, x1, yf, yb, z, hmax, mats=None):
    """A shelf of books standing between x0 and x1, spines at the front (yf)."""
    mats = mats or BOOKS
    x = x0 + rnd.uniform(0.0, 0.03)
    while x < x1 - 0.03:
        r = rnd.random()
        if r < 0.07:                                   # a gap
            x += rnd.uniform(0.04, 0.12)
            continue
        if r < 0.14 and x1 - x > 0.25:                 # a flat stack
            w = rnd.uniform(0.17, 0.24)
            zz = z
            for _ in range(rnd.randint(2, 4)):
                t = rnd.uniform(0.02, 0.045)
                bx(m, rnd.choice(mats), x, x + w, yf + 0.015, yf + 0.015 + (yb - yf) * 0.75, zz, zz + t, faces="yxXZ")
                zz += t
            x += w + 0.01
            continue
        n = rnd.randint(3, 10) if rnd.random() < 0.4 else rnd.randint(1, 4)   # sets of collected works
        bw = rnd.uniform(0.022, 0.045)
        w = min(n * bw, x1 - x)
        hh = hmax * rnd.uniform(0.62, 0.98)
        dd = (yb - yf) * rnd.uniform(0.6, 0.92)
        bx(m, rnd.choice(mats), x, x + w, yf + 0.01, yf + 0.01 + dd, z, z + hh, faces="yxXZ")
        x += w + 0.003


def books_stack(rnd=None, n=None):
    rnd = _rng(rnd, 3)
    m = Mesh()
    n = n or rnd.randint(2, 5)
    z = 0.0
    for _ in range(n):
        w, d, t = rnd.uniform(0.16, 0.26), rnd.uniform(0.12, 0.18), rnd.uniform(0.02, 0.05)
        yawbox(m, rnd.choice(BOOKS), 0, 0, w, d, z, z + t, rnd.uniform(-0.25, 0.25))
        z += t
    return m


def vase(rnd=None, flowers=True):
    rnd = _rng(rnd, 5)
    m = Mesh()
    mat = rnd.choice([GLASS, PORC, RUGR, BRASS])
    h = rnd.uniform(0.2, 0.32)
    lathe(m, mat, [(0, 0), (0.045, 0), (0.06, h * 0.25), (0.05, h * 0.6), (0.03, h * 0.85), (0.045, h), (0.0, h)], 8)
    if flowers:
        for k in range(rnd.randint(3, 6)):
            a = rnd.uniform(0, 2 * math.pi)
            r = rnd.uniform(0.03, 0.12)
            top = (r * math.cos(a), r * math.sin(a), h + rnd.uniform(0.12, 0.3))
            tube(m, BGREEN, (0, 0, h - 0.02), top, 0.004, 0.004, 3)
            sphere(m, rnd.choice([RUGR, RUGR, LIN, PLUSH]), top, rnd.uniform(0.025, 0.04), 4, 3)
    return m


def candlestick():
    m = Mesh()
    lathe(m, BRASS, [(0, 0), (0.05, 0), (0.05, 0.01), (0.015, 0.04), (0.012, 0.2), (0.025, 0.21), (0, 0.23)], 6)
    tube(m, LIN, (0, 0, 0.23), (0, 0, 0.32), 0.01, 0.01, 5, cap1=LIN)
    return m


def samovar():
    """Brass samovar on a tray with a porcelain teapot on its crown."""
    m = Mesh()
    lathe(m, BRASS, [(0, 0), (0.2, 0), (0.21, 0.008), (0.0, 0.008)], 10)              # tray
    bxc(m, BRASS, 0, 0, 0.17, 0.17, 0.008, 0.04)
    lathe(m, BRASS, [(0, 0.04), (0.08, 0.04), (0.05, 0.08), (0.13, 0.16), (0.15, 0.26),
                     (0.13, 0.36), (0.07, 0.38), (0.045, 0.42), (0.045, 0.46), (0.0, 0.46)], 10)
    lathe(m, PORC, [(0, 0.46), (0.045, 0.46), (0.07, 0.5), (0.065, 0.55), (0.012, 0.6), (0, 0.6)], 8)
    tube(m, PORC, (0.0, -0.06, 0.52), (0.0, -0.11, 0.56), 0.012, 0.008, 5)
    for s in (-1, 1):
        chain(m, BLACK, [(s * 0.14, 0, 0.34), (s * 0.2, 0, 0.33), (s * 0.2, 0, 0.27), (s * 0.15, 0, 0.26)], 0.01, 4)
    tube(m, BRASS, (0, -0.13, 0.15), (0, -0.2, 0.14), 0.012, 0.01, 5)
    tube(m, BRASS, (0, -0.2, 0.14), (0, -0.2, 0.1), 0.01, 0.008, 5)
    return m


def kettle(color=ENC):
    m = Mesh()
    lathe(m, color, [(0, 0), (0.1, 0), (0.11, 0.04), (0.1, 0.12), (0.06, 0.16), (0.03, 0.17), (0, 0.18)], 10)
    tube(m, color, (0, -0.08, 0.06), (0, -0.16, 0.14), 0.015, 0.008, 5)
    chain(m, BLACK, [(-0.06, 0, 0.16), (-0.05, 0, 0.24), (0.05, 0, 0.24), (0.06, 0, 0.16)], 0.008, 4)
    return m


def pot(color=ENW, r=0.11, h=0.14):
    m = Mesh()
    lathe(m, color, [(0, 0), (r, 0), (r, h), (r + 0.01, h), (0.015, h + 0.02), (0.02, h + 0.04), (0, h + 0.04)], 8)
    for s in (-1, 1):
        bxc(m, color, s * (r + 0.02), 0, 0.03, 0.03, h - 0.03, h - 0.01)
    return m


def jar(rnd, r=0.05, h=0.15):
    m = Mesh()
    lathe(m, GLASS, [(r, h * 0.6), (r, h * 0.85), (r * 0.7, h)], 6)
    lathe(m, rnd.choice([RUGR, PLUSH, BGREEN, BBROWN]), [(0, 0), (r, 0), (r, h * 0.6), (0, h * 0.6)], 6)
    lathe(m, CHROME, [(r * 0.7, h), (r * 0.75, h + 0.015), (0, h + 0.015)], 6)
    return m


def plate_stack(n=6, r=0.12):
    m = Mesh()
    lathe(m, PORC, [(0, 0), (r * 0.6, 0), (r, n * 0.012), (r * 0.6, n * 0.012 - 0.004), (0, n * 0.012 - 0.004)], 12)
    return m


def cup(r=0.04):
    m = Mesh()
    lathe(m, PORC, [(0, 0), (r * 1.6, 0), (r * 1.5, 0.008), (r * 0.6, 0.008), (r, 0.06), (r * 0.9, 0.06), (0, 0.025)], 6)
    return m


def telephone():
    """Black carbolite desk telephone with a dial and handset."""
    m = Mesh()
    cbox(m, BLACK, -0.09, 0.09, -0.11, 0.11, 0.0, 0.075, 0.02)
    prism_y(m, BLACK, [(-0.08, 0.07), (0.08, 0.07), (0.07, 0.105), (-0.07, 0.105)], -0.05, 0.1)
    face_n(m, PAPER, [(-0.045, -0.1, 0.02), (0.045, -0.1, 0.02), (0.045, -0.06, 0.07), (-0.045, -0.06, 0.07)], (0, -1, 0.8))
    e1, e2 = (1.0, 0.0, 0.0), (0.0, 0.6, 0.8)          # the dial lies on the sloping front
    dc = (0.0, -0.075, 0.05)
    face_n(m, CHROME, [tuple(dc[i] + 0.042 * (math.cos(a) * e1[i] + math.sin(a) * e2[i]) + 0.002 * (0, -0.8, 0.6)[i] for i in range(3))
                       for a in [2 * math.pi * k / 10 for k in range(10)]], (0, -0.8, 0.6))
    tube(m, BLACK, (-0.1, 0.02, 0.13), (0.1, 0.02, 0.13), 0.016, 0.016, 6)
    for s in (-1, 1):
        sphere(m, BLACK, (s * 0.1, 0.02, 0.12), 0.03, 6, 4, 0.7)
    return m


def inkstand():
    m = Mesh()
    cbox(m, WAL, -0.15, 0.15, -0.09, 0.09, 0.0, 0.025, 0.008)
    for s in (-1, 1):
        bxc(m, GLASS, s * 0.08, 0.0, 0.06, 0.06, 0.025, 0.08)
        lathe(m, BRASS, [(0, 0.08), (0.02, 0.08), (0.02, 0.09), (0, 0.095)], 6, (s * 0.08, 0.0, 0.0))
    tube(m, BLACK, (0.0, 0.03, 0.03), (0.02, 0.0, 0.2), 0.004, 0.003, 4)
    return m


def blotter():
    m = Mesh()
    lathe(m, WAL, [(0, 0), (0.06, 0), (0.06, 0.04), (0.05, 0.05), (0, 0.05)], 8, sx=2.4)
    tube(m, BRASS, (0, 0, 0.05), (0, 0, 0.1), 0.012, 0.008, 6, cap1=BRASS)
    return m


def typewriter():
    m = Mesh()
    cbox(m, BLACK, -0.17, 0.17, -0.16, 0.16, 0.0, 0.05, 0.015)
    prism_y(m, BLACK, [(-0.16, 0.05), (0.16, 0.05), (0.16, 0.13), (-0.16, 0.13)], -0.02, 0.16)
    for r in range(4):
        z = 0.06 + r * 0.018
        y = -0.15 + r * 0.032
        for k in range(10):
            x = -0.13 + k * 0.029 + (r % 2) * 0.012
            bx(m, PAPER, x, x + 0.016, y, y + 0.016, z, z + 0.008, faces="Zy")
    tube(m, BLACK, (-0.21, 0.1, 0.165), (0.21, 0.1, 0.165), 0.028, 0.028, 8, BLACK, BLACK)
    for s in (-1, 1):
        sphere(m, CHROME, (s * 0.22, 0.1, 0.165), 0.02, 6, 4)
    face_n(m, PAPER, [(-0.11, 0.12, 0.19), (0.11, 0.12, 0.19), (0.11, 0.15, 0.4), (-0.11, 0.15, 0.4)], (0, -1, 0.15))
    face_n(m, PAPER, [(-0.11, 0.12, 0.19), (-0.11, 0.15, 0.4), (0.11, 0.15, 0.4), (0.11, 0.12, 0.19)], (0, 1, -0.15))
    return m


def gramophone():
    """Soviet portable gramophone (patefon) with its lid open."""
    m = Mesh()
    cbox(m, BBLUE, -0.16, 0.16, -0.18, 0.18, 0.0, 0.12, 0.01)
    bx(m, BBLUE, -0.16, 0.16, 0.16, 0.18, 0.12, 0.45)
    lathe(m, BLACK, [(0, 0.12), (0.15, 0.12), (0.15, 0.13), (0, 0.13)], 14)
    lathe(m, RUGR, [(0, 0.13), (0.045, 0.13), (0.045, 0.132), (0, 0.132)], 8)
    chain(m, CHROME, [(0.13, 0.12, 0.14), (0.12, 0.0, 0.15), (0.06, -0.06, 0.145)], 0.008, 4)
    tube(m, CHROME, (0.16, -0.08, 0.06), (0.22, -0.08, 0.06), 0.006, 0.006, 4)
    return m


def mantel_clock(rnd=None):
    m = Mesh()
    poly = [(-0.17, 0.0), (0.17, 0.0), (0.17, 0.14)] + [(x, 0.14 + z) for x, z in arc(0, 0, 0.17, 0.0, math.pi, 6)[1:-1]] + [(-0.17, 0.14)]
    prism_y(m, WAL, [(x, z) for x, z in poly], -0.06, 0.06)
    disc_y(m, PAPER, 0, -0.062, 0.15, 0.08, 10)
    r = Mesh()
    lathe(r, BRASS, [(0.08, 0), (0.09, 0), (0.09, 0.01), (0.08, 0.01)], 10)
    m.extend(rot_x90(r).transformed(lambda p: (p[0], p[1] - 0.06, p[2] + 0.15)))
    return m


def picture(rnd=None, w=0.6, h=0.45, kind="oil"):
    """Framed picture, back at y=+0.015, bottom at z=0. kind: oil | photo | print | portrait | rug."""
    rnd = _rng(rnd, 11)
    m = Mesh()
    if kind == "rug":                                  # wall carpet
        bx(m, rnd.choice([RUGR, RUGB]), -w / 2, w / 2, -0.005, 0.015, 0.0, h)
        rug_pattern(m, rnd, -w / 2, w / 2, 0.0, h, -0.0055, plane="xz")
        return m
    fr = 0.06 if kind == "oil" else 0.035
    fmat = BRASS if kind == "oil" else rnd.choice([OAK, WAL, BLACK])
    d0, d1 = -0.015, 0.015
    bx(m, fmat, -w / 2, w / 2, d0, d1, 0, fr)
    bx(m, fmat, -w / 2, w / 2, d0, d1, h - fr, h)
    bx(m, fmat, -w / 2, -w / 2 + fr, d0, d1, fr, h - fr)
    bx(m, fmat, w / 2 - fr, w / 2, d0, d1, fr, h - fr)
    y = d0 + 0.008
    iw, ih = w / 2 - fr, h - fr
    if kind == "oil":
        face_n(m, CANVAS, [(-iw, y, fr), (iw, y, fr), (iw, y, ih), (-iw, y, ih)], (0, -1, 0))
        for _ in range(rnd.randint(3, 6)):                 # a still life of flowers: a few blobs
            cx, cz, r = rnd.uniform(-iw * 0.5, iw * 0.5), rnd.uniform(fr + (ih - fr) * 0.35, ih - 0.05), rnd.uniform(0.03, 0.07)
            disc_y(m, rnd.choice([RUGR, PLUSH, LIN, BGREEN]), cx, y - 0.001, cz, r, 6)
        bxc(m, BRASS, 0, y, 0.08, 0.002, fr + 0.03, fr + (ih - fr) * 0.4, faces="y")
    else:
        face_n(m, PAPER, [(-iw, y, fr), (iw, y, fr), (iw, y, ih), (-iw, y, ih)], (0, -1, 0))
        ex, ez = iw * (0.55 if kind != "print" else 0.8), (ih - fr) * (0.32 if kind != "print" else 0.4)
        zc = (fr + ih) / 2
        mat = BLACK if kind in ("photo", "portrait") else rnd.choice([BBLUE, BGREEN, CANVAS])
        face_n(m, mat, [(-ex, y - 0.001, zc - ez), (ex, y - 0.001, zc - ez), (ex, y - 0.001, zc + ez), (-ex, y - 0.001, zc + ez)], (0, -1, 0))
        if kind == "portrait":
            disc_y(m, PAPER, 0, y - 0.002, zc + ez * 0.25, ex * 0.35, 8)
    return m


def photo_frame(rnd=None):
    """Small standing photo frame for chests and shelves."""
    rnd = _rng(rnd, 13)
    m = Mesh()
    w, h = rnd.uniform(0.1, 0.16), rnd.uniform(0.14, 0.2)
    obox(m, rnd.choice([OAK, WAL, BRASS]), (0, 0, h / 2), (1, 0, 0), (0, 0.96, 0.28), (0, -0.28, 0.96), w / 2, 0.006, h / 2)
    obox(m, BLACK, (0, -0.007, h / 2 + 0.003), (1, 0, 0), (0, 0.96, 0.28), (0, -0.28, 0.96), w / 2 - 0.015, 0.002, h / 2 - 0.02, faces="y")
    return m


def wall_clock():
    """Pendulum wall clock, back at +y, bottom at z=0 (0.3 x 0.13 x 0.7)."""
    m = Mesh()
    poly = [(-0.15, 0.0), (0.15, 0.0), (0.15, 0.55)] + [(x, 0.55 + z * 0.6) for x, z in arc(0, 0, 0.15, 0.0, math.pi, 6)[1:-1]] + [(-0.15, 0.55)]
    prism_y(m, OAK, poly, -0.065, 0.065)
    disc_y(m, PAPER, 0, -0.066, 0.53, 0.11, 12)
    bx(m, BLACK, -0.004, 0.004, -0.068, -0.067, 0.53, 0.62, faces="y")
    bx(m, BLACK, -0.004, 0.06, -0.068, -0.067, 0.526, 0.534, faces="y")
    face_n(m, GLASS, [(-0.11, -0.066, 0.06), (0.11, -0.066, 0.06), (0.11, -0.066, 0.38), (-0.11, -0.066, 0.38)], (0, -1, 0))
    bx(m, BRASS, -0.004, 0.004, -0.05, -0.045, 0.14, 0.4, faces="y")
    disc_y(m, BRASS, 0, -0.052, 0.14, 0.045, 10)
    return m


def globe():
    m = Mesh()
    lathe(m, BLACK, [(0, 0), (0.1, 0), (0.08, 0.03), (0.02, 0.05), (0.02, 0.12), (0, 0.12)], 10)
    sphere(m, RUGB, (0, 0, 0.3), 0.16, 10, 6)
    pts = [(0.175 * math.sin(a), 0.0, 0.3 - 0.175 * math.cos(a)) for a in [math.pi * k / 8 for k in range(9)]]
    chain(m, BRASS, [(p[0] * math.cos(0.4), p[0] * math.sin(0.4), p[2]) for p in pts], 0.006, 4)
    return m


def suitcase(rnd=None, w=0.6, d=0.2, h=0.4, flat=True):
    rnd = _rng(rnd, 17)
    m = Mesh()
    mat = rnd.choice([LEA, BBROWN, BCREAM, LEA])
    if flat:                                            # lying flat
        cbox(m, mat, -w / 2, w / 2, -h / 2, h / 2, 0, d, 0.015)
        for s in (-1, 1):
            bxc(m, BRASS, s * w * 0.3, -h / 2, 0.04, 0.01, d * 0.5, d * 0.75)
    else:
        cbox(m, mat, -w / 2, w / 2, -d / 2, d / 2, 0, h, 0.015)
        chain(m, BLACK, [(-0.06, 0, h), (-0.05, 0, h + 0.04), (0.05, 0, h + 0.04), (0.06, 0, h)], 0.01, 4)
    return m


def on_top(m, rnd, x0, x1, y, z, kinds, density=0.7):
    """Lines small things up on a surface (x0..x1, centre line y, top z)."""
    x = x0 + rnd.uniform(0.02, 0.12)
    while x < x1 - 0.1:
        k = rnd.choice(kinds)
        item, w = {
            "vase": (lambda: (vase(rnd), 0.2)),
            "clock": (lambda: (mantel_clock(rnd), 0.36)),
            "photo": (lambda: (photo_frame(rnd), 0.18)),
            "books": (lambda: (books_stack(rnd), 0.3)),
            "candle": (lambda: (candlestick(), 0.12)),
            "samovar": (lambda: (samovar(), 0.44)),
            "globe": (lambda: (globe(), 0.36)),
            "case": (lambda: (suitcase(rnd, rnd.uniform(0.4, 0.6), 0.18, 0.3), 0.6)),
            "box": (lambda: (_box_prop(rnd), 0.3)),
            "plates": (lambda: (plate_stack(rnd.randint(3, 8)), 0.26)),
            "cup": (lambda: (cup(), 0.12)),
            "jar": (lambda: (jar(rnd), 0.12)),
            "pot": (lambda: (pot(rnd.choice([ENW, ENC])), 0.28)),
            "radio": (lambda: (radio_set(rnd), 0.5)),
        }[k]()
        if x + w > x1:
            break
        if rnd.random() < density:
            add(m, item, x + w / 2, y, z)
        x += w + rnd.uniform(0.02, 0.15)


def _box_prop(rnd):
    m = Mesh()
    w, d, h = rnd.uniform(0.18, 0.3), rnd.uniform(0.15, 0.22), rnd.uniform(0.08, 0.2)
    cbox(m, rnd.choice([BBROWN, WAL, LEA, BCREAM]), -w / 2, w / 2, -d / 2, d / 2, 0, h, 0.008)
    return m


# --- seating ------------------------------------------------------------------------------------------
def bentwood_chair(rnd=None):
    """Thonet-type bentwood chair (0.42 x 0.5 x 0.88), dark stained beech."""
    m = Mesh()
    mat = WAL
    lathe(m, mat, [(0, 0.43), (0.2, 0.43), (0.21, 0.45), (0.2, 0.465), (0, 0.465)], 12, (0, -0.02, 0))
    for sx, sy in ((-1, -1), (1, -1)):
        tube(m, mat, (sx * 0.14, -0.16, 0.44), (sx * 0.2, -0.22, 0.0), 0.014, 0.013, 5)
    for sx in (-1, 1):
        chain(m, mat, [(sx * 0.2, 0.2, 0.0), (sx * 0.15, 0.14, 0.44), (sx * 0.15, 0.2, 0.7)], 0.014, 5)
    hoop = [(0.15 * math.cos(a), 0.2 + 0.03 * math.sin(a), 0.7 + 0.16 * math.sin(a)) for a in [math.pi * k / 8 for k in range(9)]]
    chain(m, mat, hoop, 0.012, 5)
    inner = [(0.09 * math.cos(a), 0.19, 0.58 + 0.12 * math.sin(a)) for a in [math.pi * k / 6 for k in range(7)]]
    chain(m, mat, [(-0.15, 0.19, 0.58)] + inner + [(0.15, 0.19, 0.58)], 0.009, 4)
    ring = [(0.18 * math.cos(a), -0.02 + 0.18 * math.sin(a), 0.2) for a in [2 * math.pi * k / 8 for k in range(8)]]
    chain(m, mat, ring, 0.008, 4, closed=True)
    return m


def dining_chair(rnd=None, uph=DAM, wood=BEECH):
    """Light-wood dining chair with upholstered seat and back panel (0.45 x 0.5 x 0.95)."""
    m = Mesh()
    w, d = 0.44, 0.46
    for sx in (-1, 1):
        bxc(m, wood, sx * (w / 2 - 0.02), -d / 2 + 0.02, 0.035, 0.035, 0.0, 0.44)
        obox(m, wood, (sx * (w / 2 - 0.02), d / 2 - 0.03, 0.47), (1, 0, 0), (0, 0.995, -0.1), (0, 0.1, 0.995), 0.018, 0.02, 0.47)
    bx(m, wood, -w / 2, w / 2, -d / 2, d / 2, 0.38, 0.44)
    cbox(m, uph, -w / 2 + 0.01, w / 2 - 0.01, -d / 2 + 0.01, d / 2 - 0.02, 0.44, 0.5, 0.02)
    bx(m, wood, -w / 2 + 0.02, w / 2 - 0.02, d / 2 - 0.0, d / 2 + 0.06, 0.88, 0.93)
    cbox(m, uph, -w / 2 + 0.04, w / 2 - 0.04, d / 2 - 0.01, d / 2 + 0.06, 0.56, 0.88, 0.015)
    for sx in (-1, 1):
        bx(m, wood, sx * (w / 2 - 0.035), sx * (w / 2 - 0.005), -d / 2 + 0.03, d / 2 - 0.03, 0.12, 0.15)
    return m


def leather_armchair(rnd=None):
    """Buttoned leather club armchair (0.66 x 0.66 x 0.95)."""
    m = Mesh()
    w, d, h = 0.66, 0.66, 0.95
    for sx in (-1, 1):
        for sy in (-1, 1):
            bxc(m, WAL, sx * (w / 2 - 0.05), sy * (d / 2 - 0.05), 0.05, 0.05, 0.0, 0.1)
    cbox(m, LEA, -w / 2 + 0.1, w / 2 - 0.1, -d / 2 + 0.02, d / 2 - 0.12, 0.1, 0.45, 0.05)
    for sx in (-1, 1):
        cbox(m, LEA, sx * (w / 2 - 0.12), sx * w / 2, -d / 2, d / 2, 0.1, 0.62, 0.04)
        tube(m, LEA, (sx * (w / 2 - 0.06), -d / 2 + 0.0, 0.62), (sx * (w / 2 - 0.06), d / 2, 0.62), 0.065, 0.065, 8, LEA, LEA)
    cbox(m, LEA, -w / 2 + 0.08, w / 2 - 0.08, d / 2 - 0.17, d / 2, 0.4, h, 0.06)
    tube(m, LEA, (-w / 2 + 0.08, d / 2 - 0.09, h - 0.04), (w / 2 - 0.08, d / 2 - 0.09, h - 0.04), 0.08, 0.08, 8, LEA, LEA)
    yb = d / 2 - 0.172
    for i in range(4):
        for j in range(3):
            x, z = -0.15 + i * 0.1 + (j % 2) * 0.05, 0.56 + j * 0.11
            if abs(x) < 0.2:
                face_n(m, BLACK, [(x - 0.012, yb, z - 0.012), (x + 0.012, yb, z - 0.012), (x + 0.012, yb, z + 0.012), (x - 0.012, yb, z + 0.012)], (0, -1, 0))
    return m


def armchair(rnd=None, uph=DAM):
    """Upholstered 1950s armchair with high rounded back and wooden arm fronts (0.75 x 0.8 x 1.0)."""
    m = Mesh()
    w, d, h = 0.74, 0.78, 1.0
    for sx in (-1, 1):
        for sy in (-1, 1):
            lathe(m, WAL, [(0, 0), (0.025, 0), (0.035, 0.1), (0, 0.1)], 6, (sx * (w / 2 - 0.06), sy * (d / 2 - 0.06), 0))
    cbox(m, uph, -w / 2 + 0.14, w / 2 - 0.14, -d / 2 + 0.03, d / 2 - 0.15, 0.1, 0.46, 0.06)
    for sx in (-1, 1):
        cbox(m, uph, sx * (w / 2 - 0.15), sx * w / 2, -d / 2 + 0.02, d / 2, 0.1, 0.66, 0.06)
        bx(m, WAL, sx * (w / 2 - 0.13), sx * (w / 2 - 0.02), -d / 2 + 0.01, -d / 2 + 0.03, 0.12, 0.62)
    cbox(m, uph, -w / 2 + 0.06, w / 2 - 0.06, d / 2 - 0.2, d / 2, 0.42, h - 0.06, 0.08)
    tube(m, uph, (-w / 2 + 0.12, d / 2 - 0.1, h - 0.08), (w / 2 - 0.12, d / 2 - 0.1, h - 0.08), 0.085, 0.085, 8, uph, uph)
    return m


def sofa(rnd=None, w=1.9, d=0.85, uph=DAM):
    """Divan with bolsters (валики) at the ends and a high back with a shaped top (1.9 x 0.85 x 0.95)."""
    rnd = _rng(rnd, 19)
    m = Mesh()
    for sx in (-1, 1):
        bx(m, WAL, sx * (w / 2 - 0.05), sx * w / 2, -d / 2, d / 2, 0.0, 0.55)               # wooden ends
        cbox(m, WAL, sx * (w / 2 - 0.06), sx * (w / 2 + 0.01), -d / 2 - 0.01, d / 2, 0.55, 0.6, 0.015)
    bx(m, WAL, -w / 2 + 0.05, w / 2 - 0.05, -d / 2 + 0.04, d / 2, 0.06, 0.2)
    cbox(m, uph, -w / 2 + 0.05, w / 2 - 0.05, -d / 2 + 0.02, d / 2 - 0.15, 0.2, 0.44, 0.05)
    # high back with a shaped top
    poly = [(-w / 2 + 0.05, 0.42), (w / 2 - 0.05, 0.42), (w / 2 - 0.05, 0.86), (w * 0.25, 0.92), (0, 0.98), (-w * 0.25, 0.92), (-w / 2 + 0.05, 0.86)]
    prism_y(m, uph, poly, d / 2 - 0.16, d / 2)
    for k in range(3):
        x0 = -w / 2 + 0.07 + k * (w - 0.14) / 3
        cbox(m, uph, x0 + 0.01, x0 + (w - 0.14) / 3 - 0.01, d / 2 - 0.3, d / 2 - 0.15, 0.44, 0.8, 0.06)
    for sx in (-1, 1):                                  # bolsters
        tube(m, uph, (sx * (w / 2 - 0.15), -d / 2 + 0.03, 0.55), (sx * (w / 2 - 0.15), d / 2 - 0.16, 0.55), 0.1, 0.1, 10, uph, uph)
    if rnd.random() < 0.6:                              # a cushion and a plaid
        cbox(m, rnd.choice([PLUSH, BGREEN, RUGR]), w * 0.12, w * 0.12 + 0.36, d / 2 - 0.4, d / 2 - 0.28, 0.44, 0.76, 0.05)
        cbox(m, WOOL, -w / 2 + 0.3, -w / 2 + 0.75, -d / 2 + 0.05, d / 2 - 0.32, 0.44, 0.5, 0.02)
    return m


def piano_stool():
    m = Mesh()
    lathe(m, BLACK, [(0, 0.46), (0.17, 0.46), (0.18, 0.5), (0.0, 0.52)], 12)
    lathe(m, DAM, [(0.16, 0.52), (0.17, 0.52), (0.0, 0.535)], 12)
    lathe(m, BLACK, [(0, 0.3), (0.05, 0.3), (0.04, 0.46), (0, 0.46)], 8)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        tube(m, BLACK, (0.05 * math.cos(a), 0.05 * math.sin(a), 0.36), (0.18 * math.cos(a), 0.18 * math.sin(a), 0.0), 0.016, 0.014, 5)
    return m


def stool(mat=BEECH):
    m = Mesh()
    bx(m, mat, -0.17, 0.17, -0.17, 0.17, 0.42, 0.45)
    for sx in (-1, 1):
        for sy in (-1, 1):
            bxc(m, mat, sx * 0.13, sy * 0.13, 0.035, 0.035, 0.0, 0.42)
    for sx in (-1, 1):
        bx(m, mat, sx * 0.13 - 0.01, sx * 0.13 + 0.01, -0.13, 0.13, 0.12, 0.15)
        bx(m, mat, -0.13, 0.13, sx * 0.13 - 0.01, sx * 0.13 + 0.01, 0.2, 0.23)
    return m


# --- tables ------------------------------------------------------------------------------------------
def writing_desk(rnd=None, w=1.6, d=0.85, h=0.78):
    """Pedestal writing desk, dark polished wood with a green leather top (1.6 x 0.85 x 0.78)."""
    m = Mesh()
    zt = h - 0.04
    pw = 0.45
    for s in (-1, 1):
        a, b = sorted((s * (w / 2 - 0.03), s * (w / 2 - 0.03 - pw)))
        bx(m, WAL, a, b, -d / 2 + 0.03, d / 2 - 0.03, 0.04, zt)
        bx(m, WAL, a + 0.02, b - 0.02, -d / 2 + 0.05, d / 2 - 0.05, 0.0, 0.04)
        nd = 4 if s < 0 else 3
        dh = (zt - 0.08) / nd
        for k in range(nd):
            z0, z1 = 0.07 + k * dh + 0.01, 0.07 + (k + 1) * dh - 0.01
            bx(m, WAL, a + 0.025, b - 0.025, -d / 2 + 0.015, -d / 2 + 0.03, z0, z1, faces="yxXzZ")
            bxc(m, BRASS, (a + b) / 2, -d / 2 + 0.008, 0.06, 0.014, (z0 + z1) / 2 - 0.008, (z0 + z1) / 2 + 0.008)
        bx(m, WAL, a + 0.03, b - 0.03, d / 2 - 0.03, d / 2 - 0.015, 0.1, zt - 0.05, faces="Y")   # visitor-side panels
    bx(m, WAL, -w / 2 + pw + 0.03, w / 2 - pw - 0.03, -d / 2 + 0.03, d / 2 - 0.03, zt - 0.1, zt)
    bx(m, WAL, -w / 2 + pw + 0.06, w / 2 - pw - 0.06, -d / 2 + 0.015, -d / 2 + 0.03, zt - 0.09, zt - 0.015, faces="yxXzZ")
    bx(m, WAL, -w / 2 + pw + 0.03, w / 2 - pw - 0.03, d / 2 - 0.06, d / 2 - 0.04, 0.2, zt - 0.1)          # modesty panel
    bx(m, WAL, -w / 2 + pw + 0.03, w / 2 - pw - 0.03, d / 2 - 0.4, d / 2 - 0.06, 0.22, 0.24)              # visitor shelf
    cbox(m, WAL, -w / 2, w / 2, -d / 2, d / 2, zt, h, 0.01)
    bx(m, LEAG, -w / 2 + 0.07, w / 2 - 0.07, -d / 2 + 0.07, d / 2 - 0.07, h, h + 0.002, faces="Z")
    return m


def desk_lamp(kind="banker"):
    """Desk lamp: banker's lamp with a green glass shade, or a green enamel dome lamp."""
    m = Mesh()
    if kind == "banker":
        lathe(m, BRASS, [(0, 0), (0.09, 0), (0.09, 0.02), (0.03, 0.04), (0, 0.04)], 10, sx=1.0, sy=0.7)
        tube(m, BRASS, (0, 0.02, 0.04), (0, 0.02, 0.34), 0.011, 0.011, 6)
        tube(m, BRASS, (0, 0.02, 0.34), (0, -0.03, 0.36), 0.009, 0.009, 5)
        pts = arc(0, 0, 0.09, 0.15, math.pi - 0.15, 6)
        cy, cz = -0.03, 0.31
        for (y0, z0), (y1, z1) in zip(pts, pts[1:]):
            q = [(-0.14, cy + y0, cz + z0 * 0.6), (0.14, cy + y0, cz + z0 * 0.6), (0.14, cy + y1, cz + z1 * 0.6), (-0.14, cy + y1, cz + z1 * 0.6)]
            mid = (0, (y0 + y1) / 2, (z0 + z1) / 2)
            face_n(m, GGL, q, mid)
            face_n(m, OPAL, q, (-mid[0], -mid[1], -mid[2]))
        for s in (-1, 1):
            face_n(m, GGL, [(s * 0.14, cy + y, cz + z * 0.6) for y, z in pts], (s, 0, 0))
        tube(m, BRASS, (0.05, -0.02, 0.05), (0.05, -0.02, 0.0), 0.004, 0.004, 4)
    else:
        lathe(m, GGL, [(0, 0), (0.1, 0), (0.1, 0.02), (0.02, 0.035), (0, 0.035)], 12)
        chain(m, CHROME, [(0, 0.03, 0.03), (0, 0.06, 0.3), (0, -0.05, 0.42)], 0.008, 5)
        d = Mesh()
        lathe(d, GGL, [(0.17, 0), (0.15, 0.05), (0.08, 0.1), (0.02, 0.12), (0, 0.12)], 14)
        lathe(d, OPAL, [(0, 0.005), (0.16, 0.005), (0.0, 0.006)], 12)
        m.extend(d.transformed(lambda p: (p[0], p[1] - 0.1, p[2] + 0.3)))
    return m


def desk_set(rnd=None, chair="leather", w=1.6, d=0.85):
    """Writing desk + chair (at -y) + lamp, telephone, inkstand, blotter, books and papers."""
    rnd = _rng(rnd, 23)
    m = writing_desk(rnd, w, d)
    k = w / 1.6
    add(m, leather_armchair(rnd) if chair == "leather" else bentwood_chair(rnd), 0.0, -d / 2 - 0.2, 0.0, math.pi)
    side = rnd.choice([-1, 1])
    add(m, desk_lamp(rnd.choice(["banker", "banker", "dome"])), side * 0.55 * k, 0.22, 0.782)
    add(m, telephone(), -side * 0.6 * k, 0.18, 0.782, rnd.uniform(-0.3, 0.3))
    add(m, inkstand(), 0.0, -0.18, 0.782)
    add(m, blotter(), 0.25, -0.22, 0.782, 0.2)
    add(m, books_stack(rnd, 3), -side * 0.42 * k, -0.15, 0.782)
    bx(m, LEAG, -0.22, 0.22, 0.05, 0.32, 0.782, 0.797)                                   # leather folder
    for k in range(rnd.randint(2, 4)):
        yawbox(m, PAPER, rnd.uniform(-0.35, 0.35), rnd.uniform(-0.25, 0.1), 0.21, 0.297, 0.783 + 0.0015 * k, 0.784 + 0.0015 * k,
               rnd.uniform(-0.5, 0.5), faces="Z")
    return m


def oval_table(w=1.8, d=1.1, h=0.76, cloth=True, oval=True):
    m = Mesh()
    sx, sy = w / 2, d / 2
    if oval:
        lathe(m, WAL, [(0, h - 0.04), (1.0, h - 0.04), (1.0, h), (0, h)], 32, sx=sx, sy=sy)
    else:
        cbox(m, WAL, -sx, sx, -sy, sy, h - 0.04, h, 0.01)
    for px, py in ((-sx * 0.6, -sy * 0.55), (sx * 0.6, -sy * 0.55), (-sx * 0.6, sy * 0.55), (sx * 0.6, sy * 0.55)):
        lathe(m, WAL, [(0, 0), (0.035, 0), (0.025, 0.15), (0.045, 0.3), (0.03, 0.55), (0.035, h - 0.04), (0, h - 0.04)], 6, (px, py, 0))
    if cloth:
        if oval:
            lathe(m, LIN, [(1.0 + 0.2 / sx, h - 0.3), (1.0 + 0.05 / sx, h - 0.06), (1.0 + 0.01 / sx, h + 0.004), (0, h + 0.006)], 32, sx=sx, sy=sy)
        else:
            cbox(m, LIN, -sx - 0.02, sx + 0.02, -sy - 0.02, sy + 0.02, h - 0.24, h + 0.006, 0.01, skip="z")
    return m


def dining_set(rnd=None, w=1.8, d=1.1, chairs=6, oval=True):
    """Dining table with a white cloth and chairs around it (red upholstered light-wood chairs)."""
    rnd = _rng(rnd, 29)
    m = oval_table(w, d, oval=oval)
    per_side = max(0, (chairs - 2) // 2) if chairs >= 4 else chairs // 2
    ends = chairs - 2 * per_side
    for s in (-1, 1):
        for k in range(per_side):
            x = (k - (per_side - 1) / 2) * min(0.62, (w - 0.3) / max(1, per_side))
            add(m, dining_chair(rnd), x + rnd.uniform(-0.03, 0.03), s * (d / 2 + 0.06 + rnd.uniform(0, 0.08)), 0,
                (0.0 if s > 0 else math.pi) + rnd.uniform(-0.08, 0.08))
    for k, s in enumerate((-1, 1)[:ends]):
        add(m, dining_chair(rnd), s * (w / 2 + 0.06 + rnd.uniform(0, 0.06)), 0.0, 0, -s * math.pi / 2 + rnd.uniform(-0.08, 0.08))
    # on the table: a vase or the samovar, maybe papers
    r = rnd.random()
    if r < 0.4:
        add(m, vase(rnd), rnd.uniform(-0.2, 0.2), 0.0, 0.766)
    elif r < 0.7:
        add(m, samovar(), 0.0, 0.0, 0.766)
        for k in range(rnd.randint(2, 4)):
            add(m, cup(), (k - 1.5) * 0.25, -d * 0.3, 0.766)
    else:
        add(m, books_stack(rnd, 3), -0.3, 0.1, 0.766)
        for k in range(3):
            yawbox(m, PAPER, rnd.uniform(-0.5, 0.5), rnd.uniform(-0.25, 0.25), 0.42, 0.3, 0.767 + 0.002 * k, 0.768 + 0.002 * k, rnd.uniform(-0.6, 0.6), faces="Z")
    return m


def round_table(rnd=None, r=0.45, cloth=WOOL, chairs=0):
    """Round side/card table with a heavy chenille cloth (⌀0.9 x 0.75)."""
    rnd = _rng(rnd, 31)
    m = Mesh()
    h = 0.75
    lathe(m, WAL, [(0, h - 0.035), (r, h - 0.035), (r, h), (0, h)], 16)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        tube(m, WAL, (r * 0.55 * math.cos(a), r * 0.55 * math.sin(a), h - 0.04), (r * 0.65 * math.cos(a), r * 0.65 * math.sin(a), 0.0), 0.025, 0.018, 5)
    if cloth:
        lathe(m, cloth, [(r + 0.17, 0.32), (r + 0.06, h - 0.05), (r + 0.01, h + 0.004), (0, h + 0.006)], 16)
        lathe(m, LACE, [(r * 0.55, h + 0.007), (0, h + 0.008)], 10)
    for k in range(chairs):
        a = math.pi * 2 * k / chairs + math.pi / 4
        add(m, dining_chair(rnd, wood=rnd.choice([BEECH, WAL])), (r + 0.25) * math.cos(a), (r + 0.25) * math.sin(a), 0, a - math.pi / 2)
    if rnd.random() < 0.5:
        add(m, books_stack(rnd), 0.05, 0.05, h + 0.008)
    else:
        add(m, vase(rnd), 0, 0, h + 0.008)
    return m


def small_table(rnd=None, w=0.55, d=0.4, h=0.62, mat=WAL, shelf=True):
    m = Mesh()
    cbox(m, mat, -w / 2, w / 2, -d / 2, d / 2, h - 0.03, h, 0.008)
    for sx in (-1, 1):
        for sy in (-1, 1):
            tube(m, mat, (sx * (w / 2 - 0.05), sy * (d / 2 - 0.05), h - 0.03), (sx * (w / 2 - 0.03), sy * (d / 2 - 0.03), 0.0), 0.018, 0.012, 5)
    bx(m, mat, -w / 2 + 0.04, w / 2 - 0.04, -d / 2 + 0.04, d / 2 - 0.04, h - 0.1, h - 0.03, faces="xXyY")
    if shelf:
        bx(m, mat, -w / 2 + 0.05, w / 2 - 0.05, -d / 2 + 0.05, d / 2 - 0.05, 0.2, 0.22)
    return m


def kitchen_table(rnd=None, w=1.0, d=0.62, h=0.75, stools=2):
    """Kitchen table with a drawer and an oilcloth cover, stools tucked in at the front."""
    rnd = _rng(rnd, 37)
    m = Mesh()
    for sx in (-1, 1):
        for sy in (-1, 1):
            bxc(m, BEECH, sx * (w / 2 - 0.05), sy * (d / 2 - 0.05), 0.045, 0.045, 0.0, h - 0.03)
    bx(m, BEECH, -w / 2 + 0.03, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, h - 0.15, h - 0.03, faces="xXyY")
    bx(m, BEECH, -0.2, 0.2, -d / 2 + 0.015, -d / 2 + 0.03, h - 0.14, h - 0.04, faces="yxXzZ")
    bxc(m, BRASS, 0, -d / 2 + 0.008, 0.05, 0.015, h - 0.1, h - 0.085)
    bx(m, BEECH, -w / 2, w / 2, -d / 2, d / 2, h - 0.03, h - 0.005)
    cbox(m, OILC, -w / 2 - 0.015, w / 2 + 0.015, -d / 2 - 0.015, d / 2 + 0.015, h - 0.08, h, 0.004, skip="z")
    for k in range(stools):
        add(m, stool(rnd.choice([BEECH, ENW])), (k - (stools - 1) / 2) * 0.45, -d / 2 + 0.12 - rnd.uniform(0, 0.12), 0, rnd.uniform(-0.2, 0.2))
    r = rnd.random()
    if r < 0.4:
        add(m, samovar(), w * 0.2, 0.08, h)
    elif r < 0.7:
        add(m, kettle(rnd.choice([ENC, ENW])), w * 0.25, 0.1, h)
        add(m, cup(), -0.1, -0.05, h)
    else:
        add(m, pot(ENW, 0.12, 0.12), -w * 0.2, 0.05, h)
    return m


# --- cabinets ----------------------------------------------------------------------------------------
def sectional_bookcase(rnd=None, w=0.9, d=0.38, sections=5, mat=None):
    """Stacking glazed sectional bookcase (0.9 x 0.38 x ~1.95): plinth, 5-6 sections with lifting
    glass doors, a cornice; books inside."""
    rnd = _rng(rnd, 41)
    mat = mat or rnd.choice([OAK, OAK, WAL])
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -d / 2, d / 2
    pl, sh, top = 0.1, 0.34, 0.06
    H = pl + sections * sh + top
    bx(m, mat, x0 + 0.01, x1 - 0.01, y0 + 0.02, y1, 0, pl)
    bx(m, mat, x0, x0 + 0.02, y0, y1, pl, H - top)
    bx(m, mat, x1 - 0.02, x1, y0, y1, pl, H - top)
    bx(m, mat, x0 + 0.02, x1 - 0.02, y1 - 0.012, y1, pl, H - top, faces="yY")
    for k in range(sections):
        zb = pl + k * sh
        bx(m, mat, x0, x1, y0, y1 - 0.012, zb, zb + 0.025, faces="yZz")
        bx(m, mat, x0 + 0.02, x1 - 0.02, y0, y0 + 0.022, zb + sh - 0.055, zb + sh, faces="yzZ")
        bx(m, mat, x0 + 0.02, x1 - 0.02, y0, y0 + 0.018, zb + 0.025, zb + 0.045, faces="yZ")
        face_n(m, GLASS, [(x0 + 0.02, y0 + 0.012, zb + 0.045), (x1 - 0.02, y0 + 0.012, zb + 0.045),
                          (x1 - 0.02, y0 + 0.012, zb + sh - 0.055), (x0 + 0.02, y0 + 0.012, zb + sh - 0.055)], (0, -1, 0))
        if k == sections - 1 and rnd.random() < 0.3:
            on_top(m, rnd, x0 + 0.03, x1 - 0.03, 0.0, zb + 0.025, ["photo", "box", "cup", "clock"])
        else:
            books_row(m, rnd, x0 + 0.03, x1 - 0.03, y0 + 0.03, y1 - 0.02, zb + 0.025, sh - 0.085)
    cbox(m, mat, x0 - 0.015, x1 + 0.015, y0 - 0.015, y1, H - top, H, 0.01)
    return m


def two_door_bookcase(rnd=None, w=1.1, d=0.45, h=1.9, mat=None):
    """Two-door glazed bookcase with rounded front corners (1950s), books and keepsakes inside."""
    rnd = _rng(rnd, 43)
    mat = mat or rnd.choice([WAL, OAK])
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -d / 2, d / 2
    r = 0.06
    bx(m, mat, x0 + 0.02, x1 - 0.02, y0 + 0.03, y1, 0, 0.08)
    bx(m, mat, x0, x0 + 0.02, y0 + r, y1, 0.08, h)
    bx(m, mat, x1 - 0.02, x1, y0 + r, y1, 0.08, h)
    for s in (-1, 1):                                       # rounded front corners
        cx = s * (w / 2 - r)
        pts = arc(cx, y0 + r, r, (0 if s > 0 else math.pi), -math.pi / 2, 3)
        for (ax, ay), (bxx, by) in zip(pts, pts[1:]):
            face_out(m, mat, [(ax, ay, 0.08), (bxx, by, 0.08), (bxx, by, h), (ax, ay, h)], (cx, y0 + r, (h + 0.08) / 2))
    bx(m, mat, x0 + r, x1 - r, y0, y0 + 0.03, 0.08, 0.5, faces="y")
    bx(m, mat, x0 + r, x1 - r, y0, y0 + 0.03, h - 0.12, h, faces="y")
    cbox(m, mat, x0 - 0.01, x1 + 0.01, y0 - 0.01, y1, h, h + 0.04, 0.012)
    bx(m, mat, x0 + 0.02, x1 - 0.02, y1 - 0.012, y1, 0.08, h, faces="yY")
    bx(m, mat, -0.008, 0.008, y0, y0 + 0.03, 0.5, h - 0.12, faces="yxX")
    face_n(m, GLASS, [(x0 + r, y0 + 0.012, 0.5), (x1 - r, y0 + 0.012, 0.5), (x1 - r, y0 + 0.012, h - 0.12), (x0 + r, y0 + 0.012, h - 0.12)], (0, -1, 0))
    for s in (-1, 1):
        bxc(m, BRASS, s * 0.03, y0 - 0.005, 0.015, 0.012, 1.0, 1.05)
        bx(m, mat, s * 0.02, s * (w / 2 - r), y0 - 0.005, y0 + 0.0, 0.15, 0.43, faces="yZz")
    n = 4
    sh = (h - 0.12 - 0.5) / n
    for k in range(n):
        zb = 0.5 + k * sh
        bx(m, mat, x0 + 0.02, x1 - 0.02, y0 + 0.04, y1 - 0.012, zb, zb + 0.02, faces="yZz")
        if rnd.random() < 0.7:
            books_row(m, rnd, x0 + 0.04, x1 - 0.04, y0 + 0.05, y1 - 0.02, zb + 0.02, sh - 0.04)
        else:
            on_top(m, rnd, x0 + 0.04, x1 - 0.04, 0.02, zb + 0.02, ["photo", "cup", "vase", "box", "plates"])
    if rnd.random() < 0.6:
        on_top(m, rnd, x0 + 0.02, x1 - 0.02, 0.0, h + 0.04, ["globe", "clock", "box", "vase", "photo", "radio"], 0.5)
    return m


def sideboard(rnd=None, w=1.6, d=0.5, h=1.9, mat=None, clutter=True):
    """Буфет: lower cabinet with drawers and doors, an open niche, a glazed upper cabinet with
    dishes, a cornice."""
    rnd = _rng(rnd, 47)
    mat = mat or rnd.choice([WAL, WAL, OAK])
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -d / 2, d / 2
    zl = 0.88
    bx(m, mat, x0 + 0.02, x1 - 0.02, y0 + 0.03, y1, 0, 0.1)
    bx(m, mat, x0, x1, y0 + 0.01, y1, 0.1, zl - 0.03)
    cbox(m, mat, x0 - 0.02, x1 + 0.02, y0 - 0.01, y1, zl - 0.03, zl, 0.01)
    for k in range(2):                                     # drawers
        a = x0 + 0.03 + k * (w - 0.06) / 2
        b = a + (w - 0.06) / 2 - 0.02
        bx(m, mat, a, b, y0 - 0.005, y0 + 0.01, zl - 0.2, zl - 0.06, faces="yxXzZ")
        bxc(m, BRASS, (a + b) / 2, y0 - 0.01, 0.08, 0.012, zl - 0.14, zl - 0.12)
    for k in range(3 if w > 1.3 else 2):                   # doors
        nn = 3 if w > 1.3 else 2
        a = x0 + 0.03 + k * (w - 0.06) / nn
        b = a + (w - 0.06) / nn - 0.02
        bx(m, mat, a, b, y0 - 0.005, y0 + 0.01, 0.14, zl - 0.24, faces="yxXzZ")
        bx(m, mat, a + 0.05, b - 0.05, y0 - 0.012, y0 - 0.005, 0.2, zl - 0.3, faces="yxXzZ")
        bxc(m, BRASS, b - 0.04 if k % 2 == 0 else a + 0.04, y0 - 0.012, 0.015, 0.012, 0.45, 0.5)
    # niche and upper glazed cabinet
    du = 0.34
    yu0 = y1 - du
    zn = zl + 0.32
    face_n(m, MIR if rnd.random() < 0.5 else mat, [(x0 + 0.02, y1 - 0.01, zl), (x1 - 0.02, y1 - 0.01, zl), (x1 - 0.02, y1 - 0.01, zn), (x0 + 0.02, y1 - 0.01, zn)], (0, -1, 0))
    for s in (-1, 1):
        bx(m, mat, s * (w / 2 - 0.08), s * (w / 2 - 0.02), yu0, y1, zl, zn)              # niche posts
    bx(m, mat, x0, x1, yu0, y1, zn, zn + 0.03)
    bx(m, mat, x0, x0 + 0.03, yu0, y1, zn, h - 0.06)
    bx(m, mat, x1 - 0.03, x1, yu0, y1, zn, h - 0.06)
    bx(m, mat, x0 + 0.03, x1 - 0.03, y1 - 0.012, y1, zn, h - 0.06, faces="yY")
    cbox(m, mat, x0 - 0.03, x1 + 0.03, yu0 - 0.03, y1, h - 0.06, h, 0.015)
    wb = (w - 0.06) / 3
    for k in range(3):
        a, b = x0 + 0.03 + k * wb, x0 + 0.03 + (k + 1) * wb
        bx(m, mat, a, a + 0.03, yu0 - 0.01, yu0 + 0.01, zn + 0.03, h - 0.06, faces="yxX")
        bx(m, mat, b - 0.03, b, yu0 - 0.01, yu0 + 0.01, zn + 0.03, h - 0.06, faces="yxX")
        bx(m, mat, a, b, yu0 - 0.01, yu0 + 0.01, h - 0.12, h - 0.06, faces="yz")
        bx(m, mat, a, b, yu0 - 0.01, yu0 + 0.01, zn + 0.03, zn + 0.08, faces="yZ")
        if k == 1 or rnd.random() < 0.5:
            face_n(m, GLASS, [(a + 0.03, yu0, zn + 0.08), (b - 0.03, yu0, zn + 0.08), (b - 0.03, yu0, h - 0.12), (a + 0.03, yu0, h - 0.12)], (0, -1, 0))
        else:
            face_n(m, mat, [(a + 0.03, yu0, zn + 0.08), (b - 0.03, yu0, zn + 0.08), (b - 0.03, yu0, h - 0.12), (a + 0.03, yu0, h - 0.12)], (0, -1, 0))
    zs = zn + 0.03 + (h - 0.06 - zn - 0.03) / 2
    bx(m, GLASS, x0 + 0.03, x1 - 0.03, yu0 + 0.02, y1 - 0.012, zs - 0.006, zs, faces="Z")
    for zz in (zn + 0.03, zs):
        x = x0 + 0.1
        while x < x1 - 0.12:
            if rnd.random() < 0.45:                                  # a plate standing up
                p = Mesh()
                lathe(p, PORC, [(0, 0), (0.11, 0.0), (0.11, 0.012), (0, 0.012)], 8)
                m.extend(rot_x90(p).transformed(lambda q, x=x, zz=zz: (q[0] + x, q[1] + y1 - 0.03, q[2] + zz + 0.11)))
                x += 0.07
            else:
                add(m, cup() if rnd.random() < 0.7 else vase(rnd, False), x, (yu0 + y1) / 2, zz)
                x += 0.17
    if clutter:
        on_top(m, rnd, x0 + 0.1, x1 - 0.1, (yu0 + y1) / 2 - 0.15, zl, ["vase", "samovar", "candle", "photo", "clock", "cup"], 0.6)
        on_top(m, rnd, x0 + 0.05, x1 - 0.05, (yu0 + y1) / 2, h, ["vase", "box", "case", "clock", "photo"], 0.5)
    return m


def kitchen_dresser(rnd=None, w=0.95, d=0.48, h=1.85):
    """White painted kitchen dresser with a glazed top (museum: 0.9 x 0.45 x 1.8)."""
    rnd = _rng(rnd, 53)
    m = sideboard(rnd, w, d, h, mat=ENW, clutter=False)
    on_top(m, rnd, -w / 2 + 0.1, w / 2 - 0.1, -0.05, 0.88, ["jar", "jar", "pot", "cup", "plates"], 0.8)
    return m


def wardrobe(rnd=None, w=1.2, d=0.6, h=2.1, mirror=True):
    """Two-door wardrobe, one door with a mirror (1.2 x 0.6 x 2.1); suitcases on top sometimes."""
    rnd = _rng(rnd, 59)
    mat = rnd.choice([WAL, OAK, WAL])
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -d / 2, d / 2
    bx(m, mat, x0 + 0.02, x1 - 0.02, y0 + 0.03, y1, 0, 0.1)
    bx(m, mat, x0, x1, y0 + 0.02, y1, 0.1, h - 0.07)
    cbox(m, mat, x0 - 0.03, x1 + 0.03, y0 - 0.03, y1, h - 0.07, h, 0.015)
    side = rnd.choice([-1, 1])
    for s in (-1, 1):
        a, b = (x0 + 0.02, -0.005) if s < 0 else (0.005, x1 - 0.02)
        bx(m, mat, a, b, y0, y0 + 0.02, 0.12, h - 0.1, faces="yxXzZ")
        if mirror and s == side:
            face_n(m, MIR, [(a + 0.07, y0 - 0.002, 0.45), (b - 0.07, y0 - 0.002, 0.45), (b - 0.07, y0 - 0.002, h - 0.25), (a + 0.07, y0 - 0.002, h - 0.25)], (0, -1, 0))
        else:
            bx(m, mat, a + 0.07, b - 0.07, y0 - 0.01, y0, 0.3, h - 0.3, faces="yxXzZ")
        hx = b - 0.05 if s < 0 else a + 0.05
        bxc(m, BRASS, hx, y0 - 0.01, 0.015, 0.02, 1.05, 1.12)
    bx(m, mat, x0 + 0.04, x1 - 0.04, y0, y0 + 0.02, 0.12, 0.1 + 0.0, faces="y")
    if rnd.random() < 0.6:
        z = h
        for k in range(rnd.randint(1, 3)):
            sw = rnd.uniform(0.5, 0.75)
            add(m, suitcase(rnd, min(sw, w - 0.1), rnd.uniform(0.15, 0.22), d - 0.15), rnd.uniform(-0.1, 0.1), 0.02, z)
            z += 0.2
            if z > h + 0.45:
                break
    return m


def chest_of_drawers(rnd=None, w=1.0, d=0.5, h=0.9):
    """Комод with four drawers, a lace runner and keepsakes on top."""
    rnd = _rng(rnd, 61)
    mat = rnd.choice([WAL, OAK])
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -d / 2, d / 2
    bx(m, mat, x0 + 0.02, x1 - 0.02, y0 + 0.03, y1, 0, 0.08)
    bx(m, mat, x0, x1, y0 + 0.015, y1, 0.08, h - 0.03)
    cbox(m, mat, x0 - 0.02, x1 + 0.02, y0 - 0.01, y1, h - 0.03, h, 0.01)
    n = 4
    dh = (h - 0.13) / n
    for k in range(n):
        z0, z1 = 0.1 + k * dh + 0.008, 0.1 + (k + 1) * dh - 0.008
        bx(m, mat, x0 + 0.03, x1 - 0.03, y0, y0 + 0.016, z0, z1, faces="yxXzZ")
        for s in (-1, 1):
            bxc(m, BRASS, s * w * 0.25, y0 - 0.006, 0.07, 0.012, (z0 + z1) / 2 - 0.007, (z0 + z1) / 2 + 0.007)
    bx(m, LACE, -w * 0.38, w * 0.38, y0 + 0.05, y1 - 0.08, h, h + 0.002, faces="Z")
    face_n(m, LACE, [(-w * 0.38, y0 + 0.05, h + 0.001), (w * 0.38, y0 + 0.05, h + 0.001), (w * 0.38, y0 - 0.012, h - 0.12), (-w * 0.38, y0 - 0.012, h - 0.12)], (0, -1, 0.1))
    on_top(m, rnd, x0 + 0.05, x1 - 0.05, 0.05, h + 0.002, ["vase", "clock", "photo", "photo", "candle", "box"], 0.75)
    return m


def nightstand(rnd=None, w=0.45, d=0.4, h=0.62, top=None):
    rnd = _rng(rnd, 67)
    m = Mesh()
    bx(m, WAL, -w / 2, w / 2, -d / 2, d / 2, 0.05, h - 0.02)
    for sx in (-1, 1):
        for sy in (-1, 1):
            bxc(m, WAL, sx * (w / 2 - 0.03), sy * (d / 2 - 0.03), 0.04, 0.04, 0.0, 0.05)
    cbox(m, WAL, -w / 2 - 0.01, w / 2 + 0.01, -d / 2 - 0.01, d / 2, h - 0.025, h, 0.008)
    bx(m, WAL, -w / 2 + 0.02, w / 2 - 0.02, -d / 2 - 0.012, -d / 2, h - 0.15, h - 0.04, faces="yxXzZ")
    bx(m, WAL, -w / 2 + 0.02, w / 2 - 0.02, -d / 2 - 0.012, -d / 2, 0.08, h - 0.17, faces="yxXzZ")
    bxc(m, BRASS, 0, -d / 2 - 0.018, 0.05, 0.012, h - 0.1, h - 0.088)
    top = top or rnd.choice(["lamp", "clock", "books", "lamp"])
    if top == "lamp":
        add(m, table_lamp(rnd), 0.0, 0.0, h)
    elif top == "clock":
        add(m, mantel_clock(rnd).transformed(lambda p: (p[0] * 0.6, p[1] * 0.6, p[2] * 0.6)), 0.0, 0.02, h)
    else:
        add(m, books_stack(rnd, 2), 0.0, 0.0, h)
    return m


def trumo(rnd=None, w=1.0, d=0.42, h=0.72):
    """Трюмо: low cabinet with a tall centre mirror and two angled wings, scent bottles on top."""
    rnd = _rng(rnd, 71)
    mat = rnd.choice([WAL, OAK])
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -d / 2, d / 2
    bx(m, mat, x0 + 0.02, x1 - 0.02, y0 + 0.03, y1, 0, 0.08)
    bx(m, mat, x0, x1, y0 + 0.01, y1, 0.08, h - 0.03)
    cbox(m, mat, x0 - 0.02, x1 + 0.02, y0 - 0.01, y1, h - 0.03, h, 0.01)
    for k in range(3):
        a = x0 + 0.03 + k * (w - 0.06) / 3
        b = a + (w - 0.06) / 3 - 0.015
        bx(m, mat, a, b, y0 - 0.005, y0 + 0.01, h - 0.15, h - 0.05, faces="yxXzZ")
        bxc(m, BRASS, (a + b) / 2, y0 - 0.01, 0.05, 0.01, h - 0.105, h - 0.095)
    bx(m, mat, -0.18, 0.18, y0 - 0.005, y0 + 0.01, 0.12, h - 0.18, faces="yxXzZ")
    yc = y1 - 0.06
    bx(m, mat, -0.28, 0.28, yc - 0.015, yc + 0.015, h, h + 1.0)
    face_n(m, MIR, [(-0.25, yc - 0.016, h + 0.04), (0.25, yc - 0.016, h + 0.04), (0.25, yc - 0.016, h + 0.96), (-0.25, yc - 0.016, h + 0.96)], (0, -1, 0))
    for s in (-1, 1):
        ax = (math.cos(math.radians(30)), -math.sin(math.radians(30)), 0) if s > 0 else (math.cos(math.radians(30)), math.sin(math.radians(30)), 0)
        hx = 0.13
        cx = s * 0.28 + s * hx * ax[0]
        cy = yc + (hx * ax[1] if s > 0 else -hx * ax[1])
        ay = (-ax[1], ax[0], 0)
        obox(m, mat, (cx, cy, h + 0.45), ax, ay, (0, 0, 1), hx, 0.012, 0.38)
        inw = (-ay[0], -ay[1], 0) if ay[1] > 0 else ay
        pts = [(cx + ax[0] * sx_ * (hx - 0.02) + inw[0] * 0.013, cy + ax[1] * sx_ * (hx - 0.02) + inw[1] * 0.013, h + 0.45 + sz_ * 0.35)
               for sx_, sz_ in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        face_n(m, MIR, pts, inw)
    for k in range(rnd.randint(2, 4)):
        lathe(m, GLASS, [(0, 0), (0.025, 0), (0.025, 0.08), (0.008, 0.1), (0.008, 0.12), (0, 0.12)], 6, (rnd.uniform(-0.35, 0.35), rnd.uniform(-0.12, 0.0), h))
    bx(m, LACE, -w * 0.4, w * 0.4, y0 + 0.03, yc - 0.03, h, h + 0.002, faces="Z")
    return m


def bed(rnd=None, w=1.4, L=2.0, iron=None):
    """Bed, head at +y: iron (black bars, brass knobs) or wooden; white linen, a coverlet, a lace
    valance (подзор) and a 'pyramid' of pillows under a lace cover."""
    rnd = _rng(rnd, 73)
    iron = rnd.random() < 0.5 if iron is None else iron
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -L / 2, L / 2
    zm = 0.58
    if iron:
        for yy, hh in ((y1 - 0.02, 1.2), (y0 + 0.02, 0.9)):
            for s in (-1, 1):
                tube(m, IRON, (s * (w / 2 - 0.02), yy, 0.0), (s * (w / 2 - 0.02), yy, hh), 0.02, 0.02, 6)
                sphere(m, BRASS, (s * (w / 2 - 0.02), yy, hh + 0.03), 0.035, 6, 4)
            tube(m, IRON, (-w / 2, yy, hh - 0.05), (w / 2, yy, hh - 0.05), 0.016, 0.016, 5)
            tube(m, BRASS, (-w / 2, yy, hh - 0.12), (w / 2, yy, hh - 0.12), 0.012, 0.012, 5)
            tube(m, IRON, (-w / 2, yy, 0.55), (w / 2, yy, 0.55), 0.016, 0.016, 5)
            n = int(w / 0.14)
            for k in range(1, n):
                x = -w / 2 + k * w / n
                tube(m, IRON, (x, yy, 0.55), (x, yy, hh - 0.05), 0.008, 0.008, 4)
        for s in (-1, 1):
            bx(m, IRON, s * (w / 2 - 0.03), s * (w / 2 - 0.0), y0 + 0.02, y1 - 0.02, 0.36, 0.42)
    else:
        mat = rnd.choice([WAL, OAK])
        for yy, hh in ((y1 - 0.03, 1.15), (y0 + 0.03, 0.8)):
            bx(m, mat, x0, x1, yy - 0.03, yy + 0.03, 0.0, hh - 0.08)
            cbox(m, mat, x0 - 0.02, x1 + 0.02, yy - 0.045, yy + 0.045, hh - 0.08, hh, 0.02)
            bx(m, mat, x0 + 0.08, x1 - 0.08, yy - 0.04 if yy < 0 else yy + 0.03, yy - 0.03 if yy < 0 else yy + 0.04, 0.3, hh - 0.16)
        for s in (-1, 1):
            bx(m, mat, s * (w / 2 - 0.03), s * w / 2, y0 + 0.06, y1 - 0.06, 0.28, 0.45)
    cbox(m, LIN, x0 + 0.03, x1 - 0.03, y0 + 0.06, y1 - 0.06, 0.42, zm, 0.04)
    cover = rnd.choice([LIN, LIN, WOOL, PLUSH])
    cbox(m, cover, x0 + 0.015, x1 - 0.015, y0 + 0.05, y1 - 0.5, zm - 0.05, zm + 0.04, 0.03)
    for s in (-1, 1):                                    # lace valance on the sides and the foot
        face_n(m, LACE, [(s * (w / 2 - 0.005), y0 + 0.06, 0.06), (s * (w / 2 - 0.005), y1 - 0.06, 0.06),
                         (s * (w / 2 - 0.005), y1 - 0.06, 0.44), (s * (w / 2 - 0.005), y0 + 0.06, 0.44)], (s, 0, 0))
    face_n(m, LACE, [(x0 + 0.02, y0 + 0.055, 0.06), (x1 - 0.02, y0 + 0.055, 0.06), (x1 - 0.02, y0 + 0.055, 0.44), (x0 + 0.02, y0 + 0.055, 0.44)], (0, -1, 0))
    npw = 2 if w > 1.1 else 1
    for k in range(npw):
        xc = 0 if npw == 1 else (k - 0.5) * (w / 2)
        pw = min(0.65, w / npw - 0.06)
        z = zm
        for j, sc in enumerate((1.0, 0.85, 0.7)[:rnd.randint(2, 3)]):
            obox(m, LIN, (xc, y1 - 0.25 + j * 0.03, z + 0.1 * sc), (1, 0, 0), (0, 0.94, 0.34), (0, -0.34, 0.94), pw / 2 * sc, 0.08 * sc, 0.22 * sc)
            z += 0.17 * sc
        obox(m, LACE, (xc, y1 - 0.34, z - 0.02), (1, 0, 0), (0, 0.94, 0.34), (0, -0.34, 0.94), pw / 2 * 0.72, 0.003, 0.14, faces="y")
    return m


def bed_group(rnd=None, w=1.4, L=2.0):
    rnd = _rng(rnd, 79)
    m = bed(rnd, w, L)
    for s in (-1, 1):
        add(m, nightstand(rnd), s * (w / 2 + 0.27), L / 2 - 0.22, 0)
    return m


def piano(rnd=None):
    """Upright piano, black polish (1.45 x 0.6 x 1.3) with a stool in front, candlesticks and
    keepsakes on the lid."""
    rnd = _rng(rnd, 83)
    m = Mesh()
    w, d, h = 1.45, 0.6, 1.3
    yb = d / 2
    yf = yb - 0.32                                   # front of the cabinet above the keys
    bx(m, BLACK, -w / 2, w / 2, yf, yb, 0.0, h)
    cbox(m, BLACK, -w / 2 - 0.02, w / 2 + 0.02, yf - 0.03, yb, h - 0.03, h + 0.01, 0.01)
    for s in (-1, 1):                                # cheek pieces and front legs
        bx(m, BLACK, s * (w / 2 - 0.05), s * w / 2, -d / 2 + 0.03, yf, 0.62, 0.8)
        tube(m, BLACK, (s * (w / 2 - 0.08), -d / 2 + 0.08, 0.0), (s * (w / 2 - 0.08), -d / 2 + 0.08, 0.62), 0.03, 0.04, 6, cap1=BLACK)
    bx(m, BLACK, -w / 2 + 0.05, w / 2 - 0.05, -d / 2 + 0.03, yf, 0.62, 0.68)          # key bed
    bx(m, PORC, -w / 2 + 0.06, w / 2 - 0.06, -d / 2 + 0.03, yf - 0.02, 0.68, 0.7)       # white keys
    kw = (w - 0.12) / 52
    for k in range(52):
        if k % 7 in (2, 6):
            continue
        x = -w / 2 + 0.06 + (k + 1) * kw
        bx(m, BLACK, x - kw * 0.3, x + kw * 0.3, -d / 2 + 0.1, yf - 0.02, 0.7, 0.715, faces="yZxX")
    bx(m, BLACK, -w / 2 + 0.05, w / 2 - 0.05, yf - 0.06, yf, 0.72, 0.84)               # fallboard
    bx(m, WAL, -0.45, 0.45, yf - 0.01, yf, 0.86, 1.15, faces="y")                       # inlay panel
    bx(m, BLACK, -0.35, 0.35, yf - 0.04, yf, 0.85, 0.88)                                 # music desk ledge
    face_n(m, PAPER, [(-0.18, yf - 0.03, 0.88), (0.18, yf - 0.03, 0.88), (0.18, yf - 0.005, 1.13), (-0.18, yf - 0.005, 1.13)], (0, -1, 0.1))
    for s in (-1, 1):
        chain(m, BRASS, [(s * 0.5, yf, 1.0), (s * 0.55, yf - 0.1, 0.98), (s * 0.58, yf - 0.16, 1.02)], 0.008, 4)
        lathe(m, BRASS, [(0, 0), (0.03, 0), (0.02, 0.03), (0, 0.03)], 6, (s * 0.58, yf - 0.16, 1.02))
        tube(m, LIN, (s * 0.58, yf - 0.16, 1.05), (s * 0.58, yf - 0.16, 1.15), 0.01, 0.01, 5, cap1=LIN)
    on_top(m, rnd, -w / 2 + 0.05, w / 2 - 0.05, (yf + yb) / 2, h + 0.01, ["vase", "candle", "photo", "clock", "vase", "candle"], 0.75)
    add(m, piano_stool(), rnd.uniform(-0.2, 0.2), -d / 2 - 0.32, 0)
    return m


def radio_set(rnd=None):
    """Table radio receiver (wooden case, cloth front, dial, knobs), 0.5 x 0.28 x 0.33."""
    rnd = _rng(rnd, 89)
    m = Mesh()
    w, d, h = 0.5, 0.28, 0.33
    cbox(m, rnd.choice([WAL, OAK]), -w / 2, w / 2, -d / 2, d / 2, 0.0, h, 0.04)
    face_n(m, PLUSH, [(-w / 2 + 0.04, -d / 2 - 0.001, 0.08), (0.06, -d / 2 - 0.001, 0.08), (0.06, -d / 2 - 0.001, h - 0.04), (-w / 2 + 0.04, -d / 2 - 0.001, h - 0.04)], (0, -1, 0))
    face_n(m, PAPER, [(0.09, -d / 2 - 0.001, 0.12), (w / 2 - 0.04, -d / 2 - 0.001, 0.12), (w / 2 - 0.04, -d / 2 - 0.001, h - 0.05), (0.09, -d / 2 - 0.001, h - 0.05)], (0, -1, 0))
    for k in range(3):
        tube(m, BLACK, (0.11 + k * 0.05, -d / 2, 0.06), (0.11 + k * 0.05, -d / 2 - 0.025, 0.06), 0.015, 0.015, 6, cap1=BLACK)
    return m


def radio_on_stand(rnd=None):
    rnd = _rng(rnd, 97)
    m = small_table(rnd, 0.58, 0.4, 0.62)
    if rnd.random() < 0.5:
        add(m, radio_set(rnd), 0.0, 0.0, 0.62)
    else:
        add(m, gramophone(), 0.0, 0.0, 0.62)
    if rnd.random() < 0.5:
        lathe(m, LACE, [(0, 0.6205), (0.22, 0.6205)], 10, sx=1.0, sy=0.75)
    return m


def tv_kvn(rnd=None):
    """KVN-49 television (small walnut case, a tiny screen and a water lens in front) on a stand."""
    rnd = _rng(rnd, 101)
    m = small_table(rnd, 0.55, 0.45, 0.66)
    z = 0.66
    cbox(m, WAL, -0.19, 0.19, -0.2, 0.2, z, z + 0.4, 0.02)
    face_n(m, BLACK, [(-0.08, -0.201, z + 0.2), (0.08, -0.201, z + 0.2), (0.08, -0.201, z + 0.32), (-0.08, -0.201, z + 0.32)], (0, -1, 0))
    face_n(m, PAPER, [(-0.15, -0.201, z + 0.05), (0.15, -0.201, z + 0.05), (0.15, -0.201, z + 0.1), (-0.15, -0.201, z + 0.1)], (0, -1, 0))
    for k in range(3):
        tube(m, BLACK, (-0.1 + k * 0.1, -0.2, z + 0.14), (-0.1 + k * 0.1, -0.225, z + 0.14), 0.014, 0.014, 6, cap1=BLACK)
    lens = Mesh()
    lathe(lens, GLASS, [(0, 0), (0.08, 0.005), (0.12, 0.03), (0.12, 0.06), (0.08, 0.085), (0, 0.09)], 12)
    lathe(lens, CHROME, [(0.115, 0.025), (0.13, 0.025), (0.13, 0.065), (0.115, 0.065)], 12)
    m.extend(rot_x90(lens).transformed(lambda p: (p[0], p[1] - 0.26, p[2] + z + 0.26)))
    for s in (-1, 1):
        tube(m, CHROME, (s * 0.12, -0.2, z + 0.12), (s * 0.12, -0.3, z + 0.18), 0.006, 0.006, 4)
    tube(m, CHROME, (0.05, 0.1, z + 0.4), (-0.15, 0.15, z + 0.75), 0.004, 0.003, 3)
    tube(m, CHROME, (0.05, 0.1, z + 0.4), (0.25, 0.15, z + 0.75), 0.004, 0.003, 3)
    return m


def radiogram(rnd=None):
    """Floor radiogram cabinet on splayed legs (0.85 x 0.42 x 0.85), cloth front, lid on top."""
    rnd = _rng(rnd, 103)
    m = Mesh()
    w, d = 0.85, 0.42
    for sx in (-1, 1):
        for sy in (-1, 1):
            tube(m, WAL, (sx * (w / 2 - 0.06), sy * (d / 2 - 0.06), 0.2), (sx * (w / 2 - 0.02), sy * (d / 2 - 0.02), 0.0), 0.02, 0.012, 5)
    cbox(m, WAL, -w / 2, w / 2, -d / 2, d / 2, 0.2, 0.85, 0.02)
    face_n(m, PLUSH, [(-w / 2 + 0.05, -d / 2 - 0.001, 0.26), (w / 2 - 0.05, -d / 2 - 0.001, 0.26), (w / 2 - 0.05, -d / 2 - 0.001, 0.62), (-w / 2 + 0.05, -d / 2 - 0.001, 0.62)], (0, -1, 0))
    face_n(m, PAPER, [(-w / 2 + 0.05, -d / 2 - 0.001, 0.66), (w / 2 - 0.2, -d / 2 - 0.001, 0.66), (w / 2 - 0.2, -d / 2 - 0.001, 0.78), (-w / 2 + 0.05, -d / 2 - 0.001, 0.78)], (0, -1, 0))
    for k in range(3):
        tube(m, BLACK, (w / 2 - 0.16 + k * 0.05, -d / 2, 0.72), (w / 2 - 0.16 + k * 0.05, -d / 2 - 0.025, 0.72), 0.015, 0.015, 6, cap1=BLACK)
    add(m, vase(rnd) if rnd.random() < 0.5 else books_stack(rnd, 2), -0.2, 0.0, 0.85)
    return m


def sewing_machine(rnd=None):
    """Treadle sewing machine: cast-iron frame, wooden top, black machine head."""
    m = Mesh()
    w, d, h = 0.88, 0.44, 0.75
    for s in (-1, 1):
        x = s * (w / 2 - 0.08)
        chain(m, IRON, [(x, -d / 2 + 0.04, 0.0), (x, -0.02, 0.4), (x, -d / 2 + 0.06, h - 0.03)], 0.016, 4)
        chain(m, IRON, [(x, d / 2 - 0.04, 0.0), (x, 0.02, 0.4), (x, d / 2 - 0.06, h - 0.03)], 0.016, 4)
        tube(m, IRON, (x, -d / 2 + 0.04, 0.01), (x, d / 2 - 0.04, 0.01), 0.015, 0.015, 4)
    tube(m, IRON, (-w / 2 + 0.08, 0, 0.4), (w / 2 - 0.08, 0, 0.4), 0.012, 0.012, 4)
    wh = Mesh()
    lathe(wh, IRON, [(0.16, 0), (0.18, 0), (0.18, 0.02), (0.16, 0.02)], 12)
    m.extend(wh.transformed(lambda p: (w / 2 - 0.12 + p[2], p[0], 0.4 + p[1])))
    yawbox(m, IRON, 0, -0.05, 0.36, 0.2, 0.06, 0.09, 0.0)
    cbox(m, OAK, -w / 2, w / 2, -d / 2, d / 2, h - 0.03, h, 0.008)
    bx(m, OAK, -w / 2 + 0.05, w / 2 - 0.05, -d / 2 + 0.02, d / 2 - 0.02, h - 0.15, h - 0.03, faces="xXyY")
    cbox(m, BLACK, -0.18, 0.2, -0.05, 0.05, h, h + 0.05, 0.01)
    cbox(m, BLACK, 0.1, 0.2, -0.04, 0.04, h + 0.05, h + 0.27, 0.02)
    cbox(m, BLACK, -0.2, 0.2, -0.035, 0.035, h + 0.2, h + 0.28, 0.025)
    cbox(m, BLACK, -0.2, -0.13, -0.03, 0.03, h + 0.1, h + 0.28, 0.015)
    wh2 = Mesh()
    lathe(wh2, CHROME, [(0, 0), (0.07, 0), (0.07, 0.02), (0, 0.02)], 10)
    m.extend(wh2.transformed(lambda p: (0.2 + p[2], p[0], h + 0.2 + p[1])))
    return m


# --- lighting ----------------------------------------------------------------------------------------
def ceiling_rose(r=0.35):
    """Moulded plaster ceiling rose, hanging from z=0."""
    m = Mesh()
    k = r / 0.35
    prof = [(0, -0.075), (0.06, -0.075), (0.08, -0.06), (0.13, -0.05), (0.15, -0.035), (0.2, -0.032), (0.23, -0.02),
            (0.3, -0.016), (0.33, -0.006), (0.35, 0.0)]
    lathe(m, CEIL, [(rr * k, z) for rr, z in prof], 20)
    return m


def chandelier(rnd=None, arms=5, drop=0.9, style="globe"):
    """Brass chandelier with opal glass globes (style 'globe') or frosted tulips ('tulip'),
    hanging from z=0 down to z=-drop."""
    m = Mesh()
    lathe(m, BRASS, [(0, -0.08), (0.06, -0.08), (0.075, -0.03), (0.05, 0.0), (0, 0.0)], 10)
    zc = -drop + 0.14
    tube(m, BRASS, (0, 0, -0.08), (0, 0, zc + 0.05), 0.008, 0.008, 5)
    for zz in (-0.25, zc + 0.12):
        if zz > zc + 0.05 and zz < -0.08:
            sphere(m, BRASS, (0, 0, zz), 0.025, 6, 4)
    lathe(m, BRASS, [(0, zc - 0.14), (0.04, zc - 0.13), (0.1, zc - 0.07), (0.13, zc - 0.02), (0.12, zc), (0.04, zc + 0.04), (0, zc + 0.05)], 12)
    lathe(m, OPAL, [(0, zc - 0.135), (0.035, zc - 0.13), (0.0, zc - 0.128)], 10)
    R = 0.3 if style == "globe" else 0.27
    ring = [(R * math.cos(2 * math.pi * k / 14), R * math.sin(2 * math.pi * k / 14), zc - 0.04) for k in range(14)]
    if style == "globe":
        chain(m, BRASS, ring, 0.008, 4, closed=True)
    for k in range(arms):
        a = 2 * math.pi * k / arms
        ca, sa = math.cos(a), math.sin(a)
        path = [(0.1, zc - 0.03), (0.22, zc - 0.08), (R, zc - 0.04), (R + 0.04, zc + 0.04), (R + 0.04, zc + 0.08)]
        chain(m, BRASS, [(r * ca, r * sa, z) for r, z in path], 0.011, 5)
        cx, cy, cz = (R + 0.04) * ca, (R + 0.04) * sa, zc + 0.08
        lathe(m, BRASS, [(0, 0), (0.025, 0), (0.045, 0.035), (0.04, 0.04), (0, 0.02)], 8, (cx, cy, cz))
        if style == "globe":
            sphere(m, OPAL, (cx, cy, cz + 0.03 + 0.075), 0.075, 8, 5)
        else:
            lathe(m, OPAL, [(0, 0.0), (0.03, 0.01), (0.07, 0.07), (0.075, 0.13), (0.06, 0.13), (0.0, 0.05)], 8, (cx, cy, cz + 0.02))
    return m


def pendant(drop=0.8):
    """Opal glass cylinder pendant with a chrome band on a flex (kitchen, corridor)."""
    m = Mesh()
    lathe(m, CHROME, [(0, -0.04), (0.05, -0.04), (0.05, 0.0), (0, 0.0)], 8)
    tube(m, IRON, (0, 0, -0.04), (0, 0, -drop + 0.3), 0.004, 0.004, 4)
    z = -drop
    lathe(m, OPAL, [(0, z), (0.14, z + 0.01), (0.15, z + 0.05), (0.15, z + 0.26), (0.11, z + 0.3), (0, z + 0.3)], 12)
    lathe(m, CHROME, [(0.152, z + 0.04), (0.158, z + 0.04), (0.158, z + 0.08), (0.152, z + 0.08)], 12)
    return m


def ceiling_globe(r=0.16):
    """Opal half-globe ceiling light (bath, storeroom), hanging from z=0."""
    m = Mesh()
    lathe(m, CHROME, [(0, -0.03), (r + 0.02, -0.03), (r + 0.02, 0.0), (0, 0.0)], 10)
    prof = [(r * math.sin(math.pi / 2 * k / 4), -0.03 - r * 0.9 * math.cos(math.pi / 2 * k / 4)) for k in range(5)]
    lathe(m, OPAL, prof, 10)
    return m


def floor_lamp(rnd=None):
    """Торшер: turned wooden pole on a round base, a pleated silk shade (1.65 m)."""
    rnd = _rng(rnd, 107)
    m = Mesh()
    lathe(m, WAL, [(0, 0), (0.19, 0), (0.19, 0.025), (0.12, 0.05), (0.03, 0.07), (0, 0.07)], 12)
    tube(m, WAL, (0, 0, 0.07), (0, 0, 1.42), 0.016, 0.016, 6)
    for z in (0.5, 1.0):
        sphere(m, WAL, (0, 0, z), 0.025, 6, 4)
    sphere(m, OPAL, (0, 0, 1.43), 0.045, 6, 4)
    mat = rnd.choice([PLUSH, PLUSH, LIN, RUGR])
    lathe(m, mat, [(0.28, 1.3), (0.16, 1.62)], 12)
    lathe(m, mat, [(0.16, 1.62), (0.28, 1.3)], 12)
    return m


def table_lamp(rnd=None):
    m = Mesh()
    lathe(m, rnd.choice([PORC, BRASS]) if rnd else PORC, [(0, 0), (0.07, 0), (0.07, 0.02), (0.05, 0.1), (0.015, 0.22), (0, 0.22)], 10)
    mat = rnd.choice([PLUSH, LIN]) if rnd else PLUSH
    lathe(m, mat, [(0.14, 0.2), (0.08, 0.36)], 10)
    lathe(m, mat, [(0.08, 0.36), (0.14, 0.2)], 10)
    sphere(m, OPAL, (0, 0, 0.25), 0.03, 6, 3)
    return m


# --- soft furnishing --------------------------------------------------------------------------------
def rug_pattern(m, rnd, a0, a1, b0, b1, off, plane="xy", cols=None):
    """Border, inner line and a centre medallion on a rug (in the xy plane at z=off, or the xz
    plane at y=off for a wall carpet)."""
    cols = cols or rnd.sample([RUGR, RUGB, BCREAM], 3)
    w, d = a1 - a0, b1 - b0
    bw = min(w, d) * 0.1

    def P(a, b, e=0.0):
        return (a, b, off + e) if plane == "xy" else (a, off - e, b)

    n = (0, 0, 1) if plane == "xy" else (0, -1, 0)

    def quad(mat, x0, x1, y0, y1, e):
        face_n(m, mat, [P(x0, y0, e), P(x1, y0, e), P(x1, y1, e), P(x0, y1, e)], n)

    quad(cols[1], a0 + bw * 0.3, a1 - bw * 0.3, b0 + bw * 0.3, b0 + bw * 1.3, 0.0005)
    quad(cols[1], a0 + bw * 0.3, a1 - bw * 0.3, b1 - bw * 1.3, b1 - bw * 0.3, 0.0005)
    quad(cols[1], a0 + bw * 0.3, a0 + bw * 1.3, b0 + bw * 1.3, b1 - bw * 1.3, 0.0005)
    quad(cols[1], a1 - bw * 1.3, a1 - bw * 0.3, b0 + bw * 1.3, b1 - bw * 1.3, 0.0005)
    cx, cy = (a0 + a1) / 2, (b0 + b1) / 2
    rx, ry = w * 0.22, d * 0.22
    face_n(m, cols[2], [P(cx - rx, cy, 0.001), P(cx, cy - ry, 0.001), P(cx + rx, cy, 0.001), P(cx, cy + ry, 0.001)], n)
    face_n(m, cols[1], [P(cx - rx * 0.5, cy, 0.0015), P(cx, cy - ry * 0.5, 0.0015), P(cx + rx * 0.5, cy, 0.0015), P(cx, cy + ry * 0.5, 0.0015)], n)
    for sx in (-1, 1):
        for sy in (-1, 1):
            qx, qy = cx + sx * (w / 2 - bw * 2.4), cy + sy * (d / 2 - bw * 2.4)
            r = bw * 0.7
            face_n(m, cols[2], [P(qx - r, qy, 0.001), P(qx, qy - r, 0.001), P(qx + r, qy, 0.001), P(qx, qy + r, 0.001)], n)


def rug(rnd=None, w=2.5, d=1.8):
    """Pile carpet: red or blue field, border, medallion, linen fringes at the short ends."""
    rnd = _rng(rnd, 109)
    m = Mesh()
    cols = rnd.sample([RUGR, RUGB, BCREAM], 3) if rnd.random() < 0.3 else [RUGR, RUGB, BCREAM]
    bx(m, cols[0], -w / 2, w / 2, -d / 2, d / 2, 0.003, 0.01, faces="ZxXyY")
    rug_pattern(m, rnd, -w / 2, w / 2, -d / 2, d / 2, 0.0101, cols=cols)
    for s in (-1, 1):
        face_n(m, LIN, [(s * w / 2, -d / 2 + 0.03, 0.004), (s * (w / 2 + 0.06), -d / 2 + 0.03, 0.004),
                        (s * (w / 2 + 0.06), d / 2 - 0.03, 0.004), (s * w / 2, d / 2 - 0.03, 0.004)], (0, 0, 1))
    return m


def runner(rnd=None, L=3.0, w=0.7):
    """Corridor runner (дорожка): a long strip with two border stripes."""
    rnd = _rng(rnd, 113)
    m = Mesh()
    a, b = rnd.choice([(RUGR, RUGB), (RUGB, RUGR), (RUGR, BCREAM)])
    bx(m, a, -L / 2, L / 2, -w / 2, w / 2, 0.003, 0.008, faces="ZxXyY")
    for s in (-1, 1):
        face_n(m, b, [(-L / 2 + 0.03, s * w * 0.36 - 0.03, 0.0085), (L / 2 - 0.03, s * w * 0.36 - 0.03, 0.0085),
                      (L / 2 - 0.03, s * w * 0.36 + 0.03, 0.0085), (-L / 2 + 0.03, s * w * 0.36 + 0.03, 0.0085)], (0, 0, 1))
    return m


def _fold_strip(m, mat, xa, xb, ya, yb, z0, z1, folds, both=True):
    """A pleated vertical curtain strip from x=xa to xb, zig-zagging between y=ya and y=yb."""
    pts = [(xa + (xb - xa) * k / (2 * folds), ya if k % 2 == 0 else yb) for k in range(2 * folds + 1)]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        q = [(x0, y0, z0), (x1, y1, z0), (x1, y1, z1), (x0, y0, z1)]
        face_n(m, mat, q, (0, -1, 0))
        if both:
            face_n(m, mat, list(reversed(q)), (0, 1, 0))


def curtains(w, h, sill=0.85, rnd=None, mat=None, pelmet=None, ext_neg=0.45, ext_pos=0.45):
    """Pair of heavy curtains on a brass rod plus tulle, for a window w wide and h high whose sill
    is `sill` over the floor. Wall plane at y=0 (room at -y), centred on the window, floor z=0.
    ext_neg/ext_pos: free wall beyond the window edges (narrower drapes near a corner)."""
    rnd = _rng(rnd, 127)
    mat = mat or rnd.choice([CURG, CURW])
    pelmet = rnd.random() < 0.4 if pelmet is None else pelmet
    m = curtain_rod_tulle(w, h, sill, mat, pelmet, ext_neg, ext_pos)
    m.extend(drape(w, h, sill, -1, mat, ext_neg))
    m.extend(drape(w, h, sill, 1, mat, ext_pos))
    return m


def drape(w, h, sill, side, mat, ext=0.45):
    """One heavy curtain beside the window (side -1/+1 along local x); ext: room beyond the window
    edge on that side (the drape is narrowed or dropped near a corner)."""
    d = Mesh()
    zr = sill + h + 0.14
    outer = min(0.38, ext - 0.03)
    if outer < 0.08:
        return d
    _fold_strip(d, mat, side * (w / 2 + outer), side * (w / 2 - 0.08), -0.11, -0.18, 0.015, zr - 0.02, 5)
    return d


def curtain_rod_tulle(w, h, sill, mat, pelmet, ext_neg=0.45, ext_pos=0.45):
    """Brass rod (with an optional pelmet) and the tulle; ext_neg/ext_pos: room beyond the window
    edges along -x / +x."""
    m = Mesh()
    zr = sill + h + 0.14
    a, b = -w / 2 - max(0.0, min(0.42, ext_neg - 0.03)), w / 2 + max(0.0, min(0.42, ext_pos - 0.03))
    tube(m, BRASS, (a, -0.13, zr), (b, -0.13, zr), 0.013, 0.013, 6)
    for x, s in ((a, -1), (b, 1)):
        if (s < 0 and ext_neg > 0.12) or (s > 0 and ext_pos > 0.12):
            sphere(m, BRASS, (x + s * 0.0, -0.13, zr), 0.03, 6, 4)
            bxc(m, BRASS, x - s * 0.12, -0.065, 0.02, 0.13, zr - 0.01, zr + 0.01)
    if pelmet:
        bx(m, mat, a, b, -0.2, -0.0, zr + 0.02, zr + 0.06, faces="yzZxX")
        _fold_strip(m, mat, a, b, -0.2, -0.215, zr - 0.22, zr + 0.06, 14, both=False)
    _fold_strip(m, LACE, -w / 2 - min(0.06, max(0.0, ext_neg - 0.01)), w / 2 + min(0.06, max(0.0, ext_pos - 0.01)),
                -0.06, -0.09, sill + 0.03, zr - 0.02, 12)
    return m


def radiator(w=1.0, h=0.58):
    """Cast-iron sectional radiator (MS-140 type), painted cream; back at +y, 0.16 deep, standing
    on its pipe stubs; bottom at z=0 (it is lifted 0.12 m by the layout)."""
    m = Mesh()
    n = max(4, int(w / 0.093))
    sw = w / n
    for k in range(n):
        x = -w / 2 + k * sw
        bx(m, ENC, x + 0.008, x + sw - 0.008, -0.08, 0.06, 0.0, h, faces="yzZxX")
    for z in (0.04, h - 0.06):
        bx(m, ENC, -w / 2, w / 2, -0.03, 0.03, z, z + 0.03, faces="yzZ")
    tube(m, ENC, (-w / 2 - 0.02, 0.03, 0.05), (-w / 2 - 0.02, 0.03, -0.12), 0.014, 0.014, 5)
    tube(m, ENC, (-w / 2 - 0.02, 0.03, 0.05), (-w / 2 + 0.02, 0.03, 0.05), 0.014, 0.014, 5)
    tube(m, ENC, (w / 2 + 0.02, 0.03, h - 0.05), (w / 2 - 0.02, 0.03, h - 0.05), 0.014, 0.014, 5)
    tube(m, ENC, (w / 2 + 0.02, 0.03, h - 0.05), (w / 2 + 0.02, 0.03, h + 0.15), 0.014, 0.014, 5)
    return m


# --- kitchen -----------------------------------------------------------------------------------------
def gas_stove(rnd=None, w=0.6, d=0.55, h=0.85):
    """1950s four-burner gas cooker in white enamel with an oven, on short legs; a kettle or pot."""
    rnd = _rng(rnd, 131)
    m = Mesh()
    for sx in (-1, 1):
        for sy in (-1, 1):
            bxc(m, IRON, sx * (w / 2 - 0.04), sy * (d / 2 - 0.04), 0.035, 0.035, 0.0, 0.08)
    cbox(m, ENW, -w / 2, w / 2, -d / 2, d / 2, 0.08, h - 0.02, 0.015)
    bx(m, ENW, -w / 2, w / 2, -d / 2, d / 2, h - 0.03, h)
    cbox(m, ENW, -w / 2 + 0.05, w / 2 - 0.05, -d / 2 - 0.02, -d / 2 + 0.005, 0.16, 0.6, 0.012)
    tube(m, CHROME, (-w / 2 + 0.1, -d / 2 - 0.05, 0.57), (w / 2 - 0.1, -d / 2 - 0.05, 0.57), 0.01, 0.01, 6)
    for s in (-1, 1):
        tube(m, CHROME, (s * (w / 2 - 0.1), -d / 2 - 0.02, 0.57), (s * (w / 2 - 0.1), -d / 2 - 0.05, 0.57), 0.008, 0.008, 4)
    bx(m, IRON, -w / 2 + 0.03, w / 2 - 0.03, -d / 2 - 0.001, -d / 2, 0.66, 0.72, faces="y")
    for k in range(4):
        x = -w / 2 + 0.11 + k * (w - 0.22) / 3
        tube(m, BLACK, (x, -d / 2, 0.69), (x, -d / 2 - 0.03, 0.69), 0.016, 0.016, 6, cap1=BLACK)
    for bxp, byp in ((-0.14, -0.12), (0.14, -0.12), (-0.14, 0.13), (0.14, 0.13)):
        lathe(m, IRON, [(0, h), (0.05, h), (0.05, h + 0.02), (0.0, h + 0.022)], 8, (bxp, byp, 0))
        bxc(m, IRON, bxp, byp, 0.2, 0.016, h + 0.02, h + 0.035)
        bxc(m, IRON, bxp, byp, 0.016, 0.2, h + 0.02, h + 0.035)
    bx(m, ENW, -w / 2, w / 2, d / 2 - 0.02, d / 2, h, h + 0.12)
    r = rnd.random()
    if r < 0.5:
        add(m, kettle(rnd.choice([ENC, ENW, RUGB])), -0.14, -0.12, h + 0.035)
    elif r < 0.8:
        add(m, pot(rnd.choice([ENW, ENC])), 0.14, 0.13, h + 0.035)
    return m


def gas_heater():
    """Wall-mounted gas water heater (колонка) with flue and pipes, in the floor frame: body
    at z 1.25..1.95, back at +y."""
    m = Mesh()
    cbox(m, ENW, -0.17, 0.17, -0.12, 0.12, 1.25, 1.95, 0.04)
    disc_y(m, BLACK, 0.0, -0.121, 1.45, 0.03, 8)
    tube(m, BLACK, (0.08, -0.12, 1.36), (0.08, -0.15, 1.36), 0.018, 0.018, 6, cap1=BLACK)
    tube(m, CHROME, (0, 0, 1.95), (0, 0, 2.6), 0.06, 0.06, 8)
    tube(m, CHROME, (0, 0, 2.6), (0, 0.13, 2.66), 0.06, 0.06, 8)
    for x in (-0.1, 0.1):
        tube(m, CHROME, (x, 0.0, 1.25), (x, 0.0, 1.05), 0.01, 0.01, 4)
        tube(m, CHROME, (x, 0.0, 1.05), (x, 0.12, 1.05), 0.01, 0.01, 4)
    return m


def sink(rnd=None, w=0.6, d=0.45, heater=True):
    """Enamelled cast-iron sink on iron legs (rim at 0.85), wall tap; with a gas water heater
    mounted on the wall beside it."""
    rnd = _rng(rnd, 137)
    m = Mesh()
    zr = 0.85
    cbox(m, ENW, -w / 2, w / 2, -d / 2, d / 2, zr - 0.2, zr, 0.02, skip="Z")
    cbox(m, ENW, -w / 2 + 0.04, w / 2 - 0.04, -d / 2 + 0.04, d / 2 - 0.04, zr - 0.18, zr + 0.05, 0.03, skip="Z", inward=True)
    for a, b, c, e in ((-w / 2, w / 2, -d / 2, -d / 2 + 0.04), (-w / 2, w / 2, d / 2 - 0.04, d / 2),
                       (-w / 2, -w / 2 + 0.04, -d / 2 + 0.04, d / 2 - 0.04), (w / 2 - 0.04, w / 2, -d / 2 + 0.04, d / 2 - 0.04)):
        face_n(m, ENW, [(a, c, zr), (b, c, zr), (b, e, zr), (a, e, zr)], (0, 0, 1))
    bx(m, ENW, -w / 2, w / 2, d / 2 - 0.02, d / 2, zr, zr + 0.25)
    for s in (-1, 1):
        tube(m, IRON, (s * (w / 2 - 0.06), -d / 2 + 0.06, 0.0), (s * (w / 2 - 0.06), -d / 2 + 0.06, zr - 0.2), 0.018, 0.018, 6)
        bx(m, IRON, s * (w / 2 - 0.08), s * (w / 2 - 0.04), -d / 2 + 0.06, d / 2, zr - 0.24, zr - 0.2)
    chain(m, IRON, [(0, 0, zr - 0.18), (0, 0, 0.42), (0, d / 2 - 0.04, 0.42), (0, d / 2 - 0.01, 0.0)], 0.022, 5)
    chain(m, CHROME, [(0, d / 2, 1.12), (0, d / 2 - 0.1, 1.12), (0, d / 2 - 0.18, 1.06), (0, d / 2 - 0.2, 0.98)], 0.012, 6)
    bxc(m, CHROME, 0, d / 2 - 0.05, 0.12, 0.012, 1.15, 1.162)
    if heater:
        add(m, gas_heater(), 0.0, d / 2 - 0.12, 0.08)
    return m


def wall_cupboard(rnd=None, w=0.8, d=0.3, h=0.62):
    """White painted wall cupboard with two glazed doors and dishes; bottom at z=0 (hung at 1.45)."""
    rnd = _rng(rnd, 139)
    m = Mesh()
    x0, x1, y0, y1 = -w / 2, w / 2, -d / 2, d / 2
    bx(m, ENW, x0, x0 + 0.02, y0, y1, 0, h)
    bx(m, ENW, x1 - 0.02, x1, y0, y1, 0, h)
    bx(m, ENW, x0, x1, y0, y1, 0, 0.02)
    cbox(m, ENW, x0 - 0.02, x1 + 0.02, y0 - 0.02, y1, h, h + 0.04, 0.01)
    bx(m, ENW, x0 + 0.02, x1 - 0.02, y1 - 0.01, y1, 0.02, h, faces="y")
    bx(m, ENW, x0 + 0.02, x1 - 0.02, y0 + 0.02, y1 - 0.01, h / 2, h / 2 + 0.015, faces="zZy")
    for s in (-1, 1):
        a, b = (x0, 0.0) if s < 0 else (0.0, x1)
        bx(m, ENW, a, b, y0 - 0.02, y0, 0.0, 0.05, faces="yzZxX")
        bx(m, ENW, a, b, y0 - 0.02, y0, h - 0.05, h, faces="yzZxX")
        bx(m, ENW, a, a + 0.05, y0 - 0.02, y0, 0.05, h - 0.05, faces="yxX")
        bx(m, ENW, b - 0.05, b, y0 - 0.02, y0, 0.05, h - 0.05, faces="yxX")
        face_n(m, GLASS, [(a + 0.05, y0 - 0.01, 0.05), (b - 0.05, y0 - 0.01, 0.05), (b - 0.05, y0 - 0.01, h - 0.05), (a + 0.05, y0 - 0.01, h - 0.05)], (0, -1, 0))
        bxc(m, CHROME, -s * 0.0 + (0.03 if s < 0 else -0.03) * -1, y0 - 0.03, 0.015, 0.015, h / 2 - 0.03, h / 2 + 0.03)
    on_top(m, rnd, x0 + 0.04, x1 - 0.04, 0.0, 0.02, ["plates", "cup", "cup", "jar"], 0.9)
    on_top(m, rnd, x0 + 0.04, x1 - 0.04, 0.0, h / 2 + 0.015, ["jar", "cup", "plates", "jar"], 0.9)
    return m


def wall_shelf(rnd=None, w=0.8):
    """Kitchen wall shelf with lettered jars (no text) and enamel cans; bottom at z=0."""
    rnd = _rng(rnd, 149)
    m = Mesh()
    bx(m, ENW, -w / 2, w / 2, -0.11, 0.11, 0.0, 0.02)
    for s in (-1, 1):
        prism_y(m, ENW, [(s * w / 2 - 0.01, 0.0), (s * w / 2 + 0.01, 0.0), (s * w / 2 + 0.01, -0.15)], -0.1, 0.11)
    on_top(m, rnd, -w / 2 + 0.03, w / 2 - 0.03, 0.0, 0.02, ["jar", "jar", "cup", "jar", "pot"], 0.95)
    return m


# --- bathroom ----------------------------------------------------------------------------------------
def bathtub(rnd=None, L=1.7, d=0.75, h=0.6):
    """Cast-iron tub on short feet (no claws), white enamel inside, painted outside; mixer and
    shower on the wall behind (+y)."""
    m = Mesh()
    x0, x1, y0, y1 = -L / 2, L / 2, -d / 2, d / 2
    for sx in (-1, 1):
        for sy in (-1, 1):
            bxc(m, IRON, sx * (L / 2 - 0.12), sy * (d / 2 - 0.1), 0.08, 0.06, 0.0, 0.1)
    cbox(m, ENC, x0 + 0.02, x1 - 0.02, y0 + 0.02, y1 - 0.02, 0.08, h - 0.03, 0.08, skip="Z")
    bx(m, ENW, x0, x1, y0, y1, h - 0.05, h, faces="xXyY")
    cbox(m, ENW, x0 + 0.06, x1 - 0.06, y0 + 0.06, y1 - 0.06, 0.12, h + 0.02, 0.12, skip="Z", inward=True)
    i0, i1, j0, j1 = x0 + 0.06, x1 - 0.06, y0 + 0.06, y1 - 0.06
    for a, b, c, e in ((x0, x1, y0, j0), (x0, x1, j1, y1), (x0, i0, j0, j1), (i1, x1, j0, j1)):   # the rolled rim
        face_n(m, ENW, [(a, c, h), (b, c, h), (b, e, h), (a, e, h)], (0, 0, 1))
    return m


def tub_fittings(shower=True):
    """Tub mixer with a hand shower on a riser, wall-mounted (back at +y, floor frame; centred on
    the mixer)."""
    m = Mesh()
    zb = 0.95
    bx(m, CHROME, -0.15, 0.15, -0.0, 0.03, zb - 0.02, zb + 0.02)
    tube(m, CHROME, (0.0, 0.0, zb), (0.0, -0.15, zb - 0.05), 0.012, 0.01, 5)
    for s in (-1, 1):
        tube(m, CHROME, (s * 0.12, 0.0, zb + 0.02), (s * 0.12, 0.0, zb + 0.06), 0.015, 0.015, 6, cap1=CHROME)
    if shower:
        chain(m, CHROME, [(0.12, -0.01, zb), (0.2, -0.11, 0.68), (0.3, -0.12, 0.72), (0.3, -0.0, 1.7)], 0.008, 4)
        tube(m, CHROME, (0.3, 0.0, 1.7), (0.3, -0.1, 1.78), 0.02, 0.04, 6, cap1=CHROME)
    return m


def titan():
    """Wood-fired bath water heater («титан»): cast-iron firebox, cream cylinder, flue into the
    wall at +y, a chrome tap; ⌀0.42 x 1.8."""
    m = Mesh()
    lathe(m, IRON, [(0, 0), (0.23, 0), (0.23, 0.5), (0.2, 0.52), (0, 0.52)], 12)
    bx(m, BLACK, -0.1, 0.1, -0.235, -0.22, 0.15, 0.32)
    bxc(m, BRASS, 0, -0.24, 0.04, 0.015, 0.23, 0.25)
    lathe(m, ENC, [(0, 0.52), (0.2, 0.52), (0.2, 1.62), (0.17, 1.72), (0.07, 1.78), (0, 1.78)], 12)
    tube(m, IRON, (0, 0, 1.75), (0, 0, 2.55), 0.06, 0.06, 8)
    tube(m, IRON, (0, 0, 2.55), (0, 0.32, 2.62), 0.06, 0.06, 8)
    chain(m, CHROME, [(0.18, -0.08, 0.9), (0.3, -0.08, 0.9), (0.3, -0.08, 0.82)], 0.012, 5)
    return m


def washbasin(rnd=None, w=0.55, d=0.42):
    """Porcelain washbasin on a pedestal, two taps; mirror and glass shelf above (floor frame)."""
    m = Mesh()
    zr = 0.82
    y0, y1 = -d / 2, d / 2
    cbox(m, PORC, -w / 2, w / 2, y0, y1, zr - 0.16, zr, 0.05, skip="Z")
    cbox(m, PORC, -w / 2 + 0.05, w / 2 - 0.05, y0 + 0.05, y1 - 0.09, zr - 0.14, zr + 0.05, 0.06, skip="Z", inward=True)
    for a, b, c, e in ((-w / 2, w / 2, y0, y0 + 0.05), (-w / 2, w / 2, y1 - 0.09, y1),
                       (-w / 2, -w / 2 + 0.05, y0 + 0.05, y1 - 0.09), (w / 2 - 0.05, w / 2, y0 + 0.05, y1 - 0.09)):
        face_n(m, PORC, [(a, c, zr), (b, c, zr), (b, e, zr), (a, e, zr)], (0, 0, 1))
    lathe(m, PORC, [(0, 0), (0.12, 0), (0.09, 0.05), (0.08, 0.4), (0.1, zr - 0.14), (0, zr - 0.14)], 10, (0, 0.03, 0), sy=0.8)
    for s in (-1, 1):
        tube(m, CHROME, (s * 0.1, y1 - 0.05, zr), (s * 0.1, y1 - 0.05, zr + 0.09), 0.012, 0.012, 6)
        tube(m, CHROME, (s * 0.1, y1 - 0.05, zr + 0.09), (s * 0.1, y1 - 0.12, zr + 0.08), 0.01, 0.008, 5)
        bxc(m, CHROME, s * 0.1, y1 - 0.05, 0.06, 0.012, zr + 0.1, zr + 0.11)
    bx(m, WAL, -0.3, 0.3, y1 - 0.02, y1, 1.15, 1.75)
    face_n(m, MIR, [(-0.27, y1 - 0.021, 1.18), (0.27, y1 - 0.021, 1.18), (0.27, y1 - 0.021, 1.72), (-0.27, y1 - 0.021, 1.72)], (0, -1, 0))
    bx(m, GLASS, -0.25, 0.25, y1 - 0.14, y1, 1.08, 1.09)
    lathe(m, GLASS, [(0, 1.09), (0.03, 1.09), (0.035, 1.19), (0.0, 1.11)], 8, (-0.15, y1 - 0.07, 0))
    return m


def toilet(rnd=None):
    """Porcelain toilet with a black seat and a high cast-iron cistern with pull chain (floor
    frame, back at +y; footprint 0.42 x 0.65)."""
    m = Mesh()
    lathe(m, PORC, [(0, 0), (0.12, 0), (0.1, 0.1), (0.13, 0.27), (0.19, 0.38), (0.19, 0.4), (0.15, 0.4), (0.1, 0.25), (0.0, 0.22)],
          12, (0, -0.06, 0), sy=1.3)
    lathe(m, BLACK, [(0.14, 0.4), (0.2, 0.4), (0.2, 0.42), (0.14, 0.42), (0.14, 0.4)], 12, (0, -0.06, 0), sy=1.3)
    bxc(m, PORC, 0, 0.2, 0.18, 0.1, 0.3, 0.42)
    bx(m, IRON, -0.22, 0.22, 0.12, 0.32, 1.85, 2.08)
    for s in (-1, 1):
        bxc(m, IRON, s * 0.16, 0.27, 0.03, 0.1, 1.78, 1.85)
    chain(m, CHROME, [(0.0, 0.22, 1.85), (0.0, 0.22, 0.6), (0.0, 0.2, 0.42)], 0.018, 6)
    tube(m, IRON, (0.2, 0.18, 1.88), (0.24, 0.1, 1.9), 0.008, 0.008, 4)
    tube(m, CHROME, (0.24, 0.1, 1.9), (0.24, 0.1, 1.35), 0.003, 0.003, 3)
    lathe(m, PORC, [(0, 0), (0.02, 0.01), (0.015, 0.08), (0, 0.09)], 6, (0.24, 0.1, 1.27))
    return m


def towel_rail(rnd=None, w=0.6):
    """Chrome towel rail with a towel; back at +y, bottom z=0."""
    m = Mesh()
    tube(m, CHROME, (-w / 2, -0.08, 0.1), (w / 2, -0.08, 0.1), 0.012, 0.012, 6)
    for s in (-1, 1):
        tube(m, CHROME, (s * w / 2, -0.08, 0.1), (s * w / 2, 0.02, 0.1), 0.01, 0.01, 5)
    _fold_strip(m, LIN, -w * 0.3, w * 0.3, -0.09, -0.07, -0.35, 0.11, 3, both=True)
    return m


def basin_stool(rnd=None):
    m = stool(ENW)
    lathe(m, ENW, [(0, 0.45), (0.12, 0.45), (0.2, 0.55), (0.19, 0.555), (0.1, 0.47), (0, 0.47)], 12)
    return m


# --- hall, storeroom ---------------------------------------------------------------------------------
def coat_rack(rnd=None, w=1.0):
    """Wall coat rack: board with hooks, a hat shelf above, coats and a hat; depth 0.32."""
    rnd = _rng(rnd, 151)
    m = Mesh()
    bx(m, WAL, -w / 2, w / 2, 0.13, 0.16, 1.55, 1.7)
    bx(m, WAL, -w / 2, w / 2, -0.12, 0.16, 1.85, 1.87)
    for s in (-1, 1):
        prism_y(m, WAL, [(s * w / 2 - 0.01, 1.85), (s * w / 2 + 0.01, 1.85), (s * w / 2 + 0.01, 1.7)], -0.1, 0.13)
    n = max(3, int(w / 0.22))
    for k in range(n):
        x = -w / 2 + (k + 0.5) * w / n
        chain(m, BRASS, [(x, 0.13, 1.62), (x, 0.06, 1.62), (x, 0.03, 1.68)], 0.007, 4)
        if rnd.random() < 0.55:
            mat = rnd.choice([WOOL, BBLUE, LEA, BBROWN, BGREEN])
            poly = [(x - 0.2, 0.8), (x + 0.2, 0.8), (x + 0.17, 1.5), (x + 0.1, 1.6), (x - 0.1, 1.6), (x - 0.17, 1.5)]
            prism_y(m, mat, poly, -0.07, 0.1)
            prism_y(m, mat, [(x - 0.06, 1.58), (x + 0.06, 1.58), (x + 0.04, 1.66), (x - 0.04, 1.66)], -0.02, 0.08)
    if rnd.random() < 0.7:
        lathe(m, rnd.choice([BLACK, BBROWN, BBLUE]), [(0, 0), (0.18, 0), (0.18, 0.01), (0.1, 0.02), (0.1, 0.12), (0, 0.13)], 10, (rnd.uniform(-0.2, 0.2), 0.0, 1.87))
    return m


def hall_mirror(rnd=None):
    """Hall console table with a framed mirror above and a telephone (0.8 x 0.36)."""
    rnd = _rng(rnd, 157)
    m = small_table(rnd, 0.8, 0.36, 0.8, WAL, shelf=True)
    bx(m, WAL, -0.33, 0.33, 0.15, 0.18, 0.95, 1.95)
    face_n(m, MIR, [(-0.29, 0.149, 0.99), (0.29, 0.149, 0.99), (0.29, 0.149, 1.91), (-0.29, 0.149, 1.91)], (0, -1, 0))
    add(m, telephone(), 0.2, 0.0, 0.8, 0.3)
    if rnd.random() < 0.6:
        add(m, books_stack(rnd, 2), -0.2, 0.0, 0.8)
    return m


def trunk(rnd=None, w=0.9, d=0.5, h=0.5):
    """Сундук: wooden chest with iron bands."""
    rnd = _rng(rnd, 163)
    m = Mesh()
    mat = rnd.choice([WAL, BGREEN, BBROWN])
    cbox(m, mat, -w / 2, w / 2, -d / 2, d / 2, 0.02, h, 0.04)
    for x in (-w * 0.3, 0, w * 0.3):
        cbox(m, IRON, x - 0.025, x + 0.025, -d / 2 - 0.005, d / 2 + 0.0, 0.0, h + 0.005, 0.02)
    bxc(m, BRASS, 0, -d / 2 - 0.01, 0.06, 0.02, h - 0.12, h - 0.04)
    if rnd.random() < 0.5:
        add(m, suitcase(rnd, 0.55, 0.16, 0.36), 0.0, 0.0, h)
    return m


def storage_rack(rnd=None, w=0.9, d=0.36, h=1.9):
    """Open storeroom shelves: jars, boxes, enamel pots, a suitcase."""
    rnd = _rng(rnd, 167)
    m = Mesh()
    for sx in (-1, 1):
        for sy in (-1, 1):
            bxc(m, BEECH, sx * (w / 2 - 0.02), sy * (d / 2 - 0.02), 0.04, 0.04, 0.0, h)
    n = 5
    for k in range(n):
        z = 0.1 + k * (h - 0.12) / (n - 1)
        bx(m, BEECH, -w / 2, w / 2, -d / 2, d / 2, z, z + 0.02)
        if k < n - 1:
            on_top(m, rnd, -w / 2 + 0.03, w / 2 - 0.03, 0.0, z + 0.02, ["jar", "jar", "box", "pot", "box", "plates"], 0.85)
        else:
            on_top(m, rnd, -w / 2 + 0.03, w / 2 - 0.03, 0.0, z + 0.02, ["case", "box"], 0.9)
    return m


def suitcase_stack(rnd=None):
    rnd = _rng(rnd, 173)
    m = Mesh()
    z = 0.0
    for k in range(rnd.randint(3, 5)):
        sw = rnd.uniform(0.55, 0.8) - k * 0.03
        sh = rnd.uniform(0.15, 0.22)
        add(m, suitcase(rnd, sw, sh, 0.4 - k * 0.02), rnd.uniform(-0.04, 0.04), 0.0, z, rnd.uniform(-0.06, 0.06))
        z += sh
    return m


def skis(rnd=None):
    """A pair of wooden skis and poles leaning on the wall (back at +y)."""
    m = Mesh()
    t = math.radians(10)
    ay = (0, math.sin(t), math.cos(t))
    for k, x in enumerate((-0.06, 0.06)):
        obox(m, BEECH, (x, 0.18 - 1.0 * math.sin(t), 1.0 * math.cos(t)), (1, 0, 0), (0, math.cos(t), -math.sin(t)), ay, 0.035, 0.012, 1.0)
    for x in (-0.18, 0.18):
        tube(m, OAK, (x, 0.18 - 1.5 * math.sin(t), 0.0), (x, 0.2, 1.45), 0.01, 0.01, 4)
    return m


def sledge(rnd=None):
    """Child's sledge with coloured slats, hung on the wall (back at +y, bottom z=0)."""
    m = Mesh()
    for k, mat in enumerate([BRED, BBLUE, PLUSH, BGREEN, BRED, BCREAM]):
        z = 0.08 + k * 0.07
        bx(m, mat, -0.4, 0.4, -0.04, 0.0, z, z + 0.05)
    for s in (-1, 1):
        chain(m, CHROME, [(-0.45, -0.05, s * 0.0 + 0.08 + (0.35 if s > 0 else 0.0)), (0.4, -0.05, 0.08 + (0.35 if s > 0 else 0.0)),
                          (0.48, -0.03, 0.12 + (0.35 if s > 0 else 0.0))], 0.01, 4)
    return m


# --- props table (for tests and the game) ------------------------------------------------------------
PROPS = {
    "writing_desk": writing_desk, "leather_armchair": leather_armchair, "bentwood_chair": bentwood_chair,
    "dining_chair": dining_chair, "armchair": armchair, "sofa": sofa,
    "sectional_bookcase": sectional_bookcase, "two_door_bookcase": two_door_bookcase, "sideboard": sideboard,
    "kitchen_dresser": kitchen_dresser, "wardrobe": wardrobe, "chest_of_drawers": chest_of_drawers,
    "nightstand": nightstand, "trumo": trumo, "bed": bed, "bed_group": bed_group,
    "oval_table": lambda rnd=None: oval_table(), "dining_set": dining_set, "round_table": round_table, "small_table": small_table,
    "desk_set": desk_set, "piano": piano, "piano_stool": lambda rnd=None: piano_stool(),
    "radio_set": radio_set, "radio_on_stand": radio_on_stand, "radiogram": radiogram, "tv_kvn": tv_kvn,
    "sewing_machine": sewing_machine, "floor_lamp": floor_lamp, "desk_lamp": lambda rnd=None: desk_lamp("banker"),
    "desk_lamp_dome": lambda rnd=None: desk_lamp("dome"), "table_lamp": table_lamp,
    "chandelier": chandelier, "chandelier_tulip": lambda rnd=None: chandelier(rnd, 5, 0.9, "tulip"),
    "ceiling_rose": lambda rnd=None: ceiling_rose(), "pendant": lambda rnd=None: pendant(),
    "ceiling_globe": lambda rnd=None: ceiling_globe(), "rug": rug, "runner": runner,
    "curtains": lambda rnd=None: curtains(1.45, 2.0, 0.85, rnd),
    "curtain_rod_tulle": lambda rnd=None: curtain_rod_tulle(1.45, 2.0, 0.85, CURG, True),
    "radiator": lambda rnd=None: radiator(),
    "gas_stove": gas_stove, "kitchen_table": kitchen_table, "wall_cupboard": wall_cupboard, "wall_shelf": wall_shelf,
    "tub_fittings": lambda rnd=None: tub_fittings(), "sink": sink, "gas_heater": lambda rnd=None: gas_heater(), "bathtub": bathtub, "titan": lambda rnd=None: titan(),
    "washbasin": washbasin, "toilet": toilet, "towel_rail": towel_rail, "basin_stool": basin_stool,
    "coat_rack": coat_rack, "hall_mirror": hall_mirror, "trunk": trunk, "storage_rack": storage_rack,
    "suitcase_stack": suitcase_stack, "skis": skis, "sledge": sledge, "stool": lambda rnd=None: stool(),
    "books_stack": books_stack, "vase": vase, "samovar": lambda rnd=None: samovar(), "typewriter": lambda rnd=None: typewriter(),
    "telephone": lambda rnd=None: telephone(), "gramophone": lambda rnd=None: gramophone(),
    "picture": picture, "photo_frame": photo_frame, "wall_clock": lambda rnd=None: wall_clock(),
    "mantel_clock": mantel_clock, "globe": lambda rnd=None: globe(), "kettle": lambda rnd=None: kettle(),
    "candlestick": lambda rnd=None: candlestick(), "suitcase": suitcase,
}
CEILING_PROPS = {"chandelier", "chandelier_tulip", "ceiling_rose", "pendant", "ceiling_globe"}


# --- room layout -------------------------------------------------------------------------------------
SIDES = ("x0", "x1", "y0", "y1")
YAW = {"y1": 0.0, "y0": math.pi, "x0": math.pi / 2, "x1": -math.pi / 2}
KIND_SALT = {"living": 1, "bedroom": 2, "study": 3, "kitchen": 4, "bath": 5, "corridor": 6, "storage": 7}


def _ov(a, b, eps=1e-4):
    return (a[0] < b[1] - eps and a[1] > b[0] + eps and a[2] < b[3] - eps and a[3] > b[2] + eps
            and a[4] < b[5] - eps and a[5] > b[4] + eps)


def _rot_bbox(bb, yaw):
    c, s = round(math.cos(yaw)), round(math.sin(yaw))
    xs, ys = [], []
    for x in (bb[0], bb[1]):
        for y in (bb[2], bb[3]):
            xs.append(x * c - y * s)
            ys.append(x * s + y * c)
    return (min(xs), max(xs), min(ys), max(ys), bb[4], bb[5])


class Layout:
    """Places props in one room: wall-backed, free-standing and wall-hung, keeping the door
    boxes, window zones and a walking route clear."""

    CELL = 0.05

    def __init__(self, room):
        self.room = room
        self.kind = room["kind"]
        self.x0, self.x1, self.y0, self.y1 = room["x0"], room["x1"], room["y0"], room["y1"]
        self.z = room.get("z", 0.0)
        self.h = room.get("h", 3.4)
        self.seed = room.get("seed", 0)
        self.rnd = random.Random(self.seed * 1000 + KIND_SALT.get(self.kind, 0))
        self.doors = [self._opening(d, True) for d in room.get("doors", [])]
        self.windows = [self._opening(w, False) for w in room.get("windows", [])]
        self.finish, self.furniture, self.curtain_mesh = Mesh(), Mesh(), Mesh()
        self.items = []                    # (name, aabb)
        self.rugs = []
        self.lights = []                   # (x, y, z, colour hex, lumens)
        self.door_boxes = []
        for d in self.doors:
            r = self.rect(d["side"], d["u"] - d["w"] / 2 - DOOR_BOX_SIDE, d["u"] + d["w"] / 2 + DOOR_BOX_SIDE, 0.0, DOOR_BOX_D)
            r = (max(r[0], self.x0), min(r[1], self.x1), max(r[2], self.y0), min(r[3], self.y1))
            self.door_boxes.append(r + (self.z, self.z + DOOR_BOX_H))
        self.window_zones = []             # (aabb) things over the sill must stay out
        for w in self.windows:
            r = self.rect(w["side"], w["u"] - w["w"] / 2 - 0.05, w["u"] + w["w"] / 2 + 0.05, 0.0, WINDOW_ZONE_D)
            self.window_zones.append(r + (self.z + w["sill"], self.z + self.h))
            for s in ((-1, 1) if self.kind not in ("bath", "storage", "kitchen") else ()):   # drapes beside the window
                a, b = sorted((w["u"] + s * (w["w"] / 2 - 0.12), w["u"] + s * (w["w"] / 2 + 0.42)))
                self.window_zones.append(self.rect(w["side"], a, b, 0.0, 0.3) + (self.z, self.z + self.h))
        self._grid_init()

    # -- frames
    def side(self, s):
        x0, x1, y0, y1 = self.x0, self.x1, self.y0, self.y1
        return {"y0": (x0, y0, 1.0, 0.0, 0.0, 1.0, x1 - x0), "y1": (x1, y1, -1.0, 0.0, 0.0, -1.0, x1 - x0),
                "x0": (x0, y1, 0.0, -1.0, 1.0, 0.0, y1 - y0), "x1": (x1, y0, 0.0, 1.0, -1.0, 0.0, y1 - y0)}[s]

    def u_of(self, s, c):
        return {"y0": c - self.x0, "y1": self.x1 - c, "x0": self.y1 - c, "x1": c - self.y0}[s]

    def pt(self, s, u, v):
        ox, oy, dx, dy, nx, ny, _ = self.side(s)
        return (ox + u * dx + v * nx, oy + u * dy + v * ny)

    def rect(self, s, u0, u1, v0, v1):
        a, b = self.pt(s, u0, v0), self.pt(s, u1, v1)
        return (min(a[0], b[0]), max(a[0], b[0]), min(a[1], b[1]), max(a[1], b[1]))

    def wall_frame(self, s):
        """(u, v, z) wall-local (v into the room) -> house; a proper rotation."""
        ox, oy, dx, dy, nx, ny, _ = self.side(s)
        z = self.z
        return lambda p: (ox + p[0] * dx + p[1] * nx, oy + p[0] * dy + p[1] * ny, z + p[2])

    def _opening(self, d, door):
        s = d["side"]
        o = {"side": s, "c": d["c"], "w": d["w"], "u": self.u_of(s, d["c"])}
        if door:
            o["h"], o["sill"] = d.get("h", DOOR_H), 0.0
        else:
            o["h"], o["sill"] = d["h"], d.get("sill", 0.85)
        return o

    # -- walking route grid
    def _grid_init(self):
        cs = self.CELL
        self.nx = max(1, int(math.ceil((self.x1 - self.x0) / cs)))
        self.ny = max(1, int(math.ceil((self.y1 - self.y0) / cs)))
        self.block = bytearray(self.nx * self.ny)
        cl = ROUTE_W / 2
        for j in range(self.ny):
            cy = self.y0 + (j + 0.5) * cs
            for i in range(self.nx):
                cx = self.x0 + (i + 0.5) * cs
                if cx - self.x0 < cl or self.x1 - cx < cl or cy - self.y0 < cl or self.y1 - cy < cl:
                    self.block[j * self.nx + i] = 1
        self.targets = []
        for d in self.doors:
            t = self._free_cell(self.pt(d["side"], d["u"], 0.6))
            if t is not None:
                self.targets.append(t)
        if self.windows and self.kind in ("living", "study", "bedroom", "kitchen"):
            w = self.windows[0]
            t = self._free_cell(self.pt(w["side"], w["u"], 0.95))
            if t is not None:
                self.targets.append(t)
        self.route_on = len(self.targets) >= 2 and self._route_ok(None)
        if not self.route_on and len(self.targets) > 2:          # the window may be unreachable in an odd room
            self.targets = self.targets[:len(self.doors)]
            self.route_on = len(self.targets) >= 2 and self._route_ok(None)

    def _cell(self, p):
        i = int((p[0] - self.x0) / self.CELL)
        j = int((p[1] - self.y0) / self.CELL)
        return min(max(i, 0), self.nx - 1), min(max(j, 0), self.ny - 1)

    def _free_cell(self, p):
        i0, j0 = self._cell(p)
        best = None
        for r in range(0, 8):
            for j in range(j0 - r, j0 + r + 1):
                for i in range(i0 - r, i0 + r + 1):
                    if 0 <= i < self.nx and 0 <= j < self.ny and not self.block[j * self.nx + i]:
                        dd = (i - i0) ** 2 + (j - j0) ** 2
                        if best is None or dd < best[0]:
                            best = (dd, i, j)
            if best:
                return best[1], best[2]
        return None

    def _cells_of(self, aabb):
        cl, cs = ROUTE_W / 2, self.CELL
        i0 = max(0, int(math.floor((aabb[0] - cl - self.x0) / cs - 0.5)))
        i1 = min(self.nx - 1, int(math.ceil((aabb[1] + cl - self.x0) / cs - 0.5)))
        j0 = max(0, int(math.floor((aabb[2] - cl - self.y0) / cs - 0.5)))
        j1 = min(self.ny - 1, int(math.ceil((aabb[3] + cl - self.y0) / cs - 0.5)))
        out = []
        for j in range(j0, j1 + 1):
            cy = self.y0 + (j + 0.5) * cs
            if not (aabb[2] - cl < cy < aabb[3] + cl):
                continue
            for i in range(i0, i1 + 1):
                cx = self.x0 + (i + 0.5) * cs
                if aabb[0] - cl < cx < aabb[1] + cl:
                    out.append(j * self.nx + i)
        return out

    def _blocks_route(self, aabb):
        if aabb[4] > self.z + 1.0 and min(aabb[1] - aabb[0], aabb[3] - aabb[2]) < 0.06:
            return False                   # pictures, clocks and other thin things on the walls
        return aabb[4] < self.z + ROUTE_HEAD and aabb[5] > self.z + 0.05

    def _route_ok(self, extra):
        if len(self.targets) < 2:
            return True
        ex = set(self._cells_of(extra)) if extra is not None and self._blocks_route(extra) else set()
        nx = self.nx
        start = self.targets[0][1] * nx + self.targets[0][0]
        if start in ex or self.block[start]:
            return False
        seen = bytearray(nx * self.ny)
        seen[start] = 1
        stack = [start]
        while stack:
            c = stack.pop()
            i, j = c % nx, c // nx
            for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                a, b = i + di, j + dj
                if 0 <= a < nx and 0 <= b < self.ny:
                    k = b * nx + a
                    if not seen[k] and not self.block[k] and k not in ex:
                        seen[k] = 1
                        stack.append(k)
        return all(seen[j * nx + i] for i, j in self.targets[1:])

    # -- checks and commits
    def ok(self, aabb, route=True, windows=True, margin=0.02):
        e = 1e-6
        if aabb[0] < self.x0 - e or aabb[1] > self.x1 + e or aabb[2] < self.y0 - e or aabb[3] > self.y1 + e:
            return False
        if aabb[5] > self.z + self.h - CORNICE * 0.6 and (aabb[0] < self.x0 + CORNICE or aabb[1] > self.x1 - CORNICE
                                                          or aabb[2] < self.y0 + CORNICE or aabb[3] > self.y1 - CORNICE):
            return False
        for db in self.door_boxes:
            if _ov(aabb, db):
                return False
        if windows:
            for wz in self.window_zones:
                if _ov(aabb, wz):
                    return False
        big = (aabb[0] - margin, aabb[1] + margin, aabb[2] - margin, aabb[3] + margin, aabb[4], aabb[5])
        for _, it in self.items:
            if _ov(big, it):
                return False
        if route and self.route_on and self._blocks_route(aabb) and not self._route_ok(aabb):
            return False
        return True

    def commit(self, name, part, frame_args, aabb, target=None):
        zf = self.z
        placed = Frame(*frame_args).apply(part)
        placed.verts = [(p[0], p[1], zf) if zf - 0.01 < p[2] < zf else p for p in placed.verts]   # splayed legs
        (self.furniture if target is None else target).extend(placed)
        self.items.append((name, aabb))
        if self._blocks_route(aabb):
            for k in self._cells_of(aabb):
                self.block[k] = 1

    def _sides(self, mode):
        if isinstance(mode, (list, tuple)):
            return list(mode)
        rnd = self.rnd

        def score(s):
            L = self.side(s)[6]
            nwin = sum(1 for w in self.windows if w["side"] == s)
            ndoor = sum(1 for d in self.doors if d["side"] == s)
            if mode == "nowin":
                return (nwin > 0) * 10 - L + rnd.uniform(0, 1.5)
            if mode == "long":
                return -L + rnd.uniform(0, 0.8)
            if mode == "win":
                return (nwin == 0) * 10 - L + rnd.uniform(0, 1.0)
            if mode == "nodoor":
                return (ndoor > 0) * 5 + (nwin > 0) * 5 - L + rnd.uniform(0, 1.0)
            return rnd.uniform(0, 3)
        return sorted(SIDES, key=score)

    def wall(self, part, name, sides="any", pref="centre", gap=0.02, zoff=0.0, route=True, windows=True, step=0.05, margin=0.02,
             target=None):
        """Places part with its back (+y) against a wall. Returns {'side', 'u', 'aabb'} or None."""
        bb = bbox(part)
        rnd = self.rnd
        for s in self._sides(sides):
            ox, oy, dx, dy, nx, ny, L = self.side(s)
            yaw = YAW[s]
            rb = _rot_bbox(bb, yaw)
            wa = (rb[1] - rb[0]) if s in ("y0", "y1") else (rb[3] - rb[2])
            if wa > L - 0.02:
                continue
            n = int((L - wa) / step) + 1
            us = [wa / 2 + 0.01 + k * (L - wa - 0.02) / max(1, n - 1) for k in range(n)] if n > 1 else [L / 2]
            if isinstance(pref, (int, float)):
                key = lambda u: abs(u - pref) + rnd.uniform(0, 0.05)
            elif pref == "centre":
                key = lambda u: abs(u - L / 2) + rnd.uniform(0, 0.35)
            elif pref == "corner":
                key = lambda u: min(u - wa / 2, L - wa / 2 - u) + rnd.uniform(0, 0.35)
            else:
                key = lambda u: rnd.uniform(0, 1)
            for u in sorted(us, key=key):
                px, py = ox + u * dx + gap * nx, oy + u * dy + gap * ny
                if s == "y0":
                    tx, ty = px - (rb[0] + rb[1]) / 2, py - rb[2]
                elif s == "y1":
                    tx, ty = px - (rb[0] + rb[1]) / 2, py - rb[3]
                elif s == "x0":
                    tx, ty = px - rb[0], py - (rb[2] + rb[3]) / 2
                else:
                    tx, ty = px - rb[1], py - (rb[2] + rb[3]) / 2
                aabb = (rb[0] + tx, rb[1] + tx, rb[2] + ty, rb[3] + ty, self.z + zoff + rb[4], self.z + zoff + rb[5])
                if self.ok(aabb, route, windows, margin):
                    self.commit(name, part, (tx, ty, yaw, self.z + zoff), aabb, target)
                    return {"side": s, "u": u, "aabb": aabb, "wa": wa}
        return None

    def free(self, part, name, target, yaws=None, radius=3.0, step=0.1, route=True):
        """Places part free-standing with its centre as close to target (x, y) as the rules allow."""
        bb = bbox(part)
        rnd = self.rnd
        yaws = yaws if yaws is not None else [0.0, math.pi / 2, math.pi, -math.pi / 2]
        cands = []
        nxs, nys = int((self.x1 - self.x0) / step), int((self.y1 - self.y0) / step)
        for i in range(nxs + 1):
            for j in range(nys + 1):
                x, y = self.x0 + i * step, self.y0 + j * step
                dd = math.hypot(x - target[0], y - target[1])
                if dd <= radius:
                    cands.append((dd + rnd.uniform(0, 0.15), x, y))
        cands.sort()
        for _, x, y in cands[:600]:
            for yaw in yaws:
                rb = _rot_bbox(bb, yaw)
                tx, ty = x - (rb[0] + rb[1]) / 2, y - (rb[2] + rb[3]) / 2
                aabb = (rb[0] + tx, rb[1] + tx, rb[2] + ty, rb[3] + ty, self.z + rb[4], self.z + rb[5])
                if self.ok(aabb, route):
                    self.commit(name, part, (tx, ty, yaw, self.z), aabb)
                    return {"x": x, "y": y, "yaw": yaw, "aabb": aabb}
        return None

    def rug(self, cx, cy, w, d, margin=0.12):
        """A rug centred near (cx, cy), shrunk until it keeps out of the door boxes and other rugs."""
        for k in range(10):
            sc = 1.0 - 0.08 * k
            ww, dd = w * sc, d * sc
            a = (max(cx - ww / 2, self.x0 + margin), min(cx + ww / 2, self.x1 - margin),
                 max(cy - dd / 2, self.y0 + margin), min(cy + dd / 2, self.y1 - margin), self.z, self.z + 0.012)
            if a[1] - a[0] < 0.9 or a[3] - a[2] < 0.6:
                return None
            ext = (a[0] - 0.07, a[1] + 0.07, a[2], a[3], a[4], a[5])     # fringes
            if any(_ov(ext, db) for db in self.door_boxes) or any(_ov(ext, r) for r in self.rugs):
                continue
            part = rug(self.rnd, a[1] - a[0], a[3] - a[2])
            self.furniture.extend(moved(part, (a[0] + a[1]) / 2, (a[2] + a[3]) / 2, self.z))
            self.rugs.append(ext)
            return a
        return None

    # -- fixtures
    def ceiling_light(self, kind, x=None, y=None):
        x = (self.x0 + self.x1) / 2 if x is None else x
        y = (self.y0 + self.y1) / 2 if y is None else y
        zc = self.z + self.h
        if kind in ("chandelier", "tulip"):
            drop = max(0.5, min(0.95, self.h - DOOR_BOX_H - 0.25))
            arms = self.rnd.choice([5, 5, 3]) if kind == "chandelier" else 5
            part = chandelier(self.rnd, arms, drop, "globe" if kind == "chandelier" else "tulip")
            self.finish.extend(moved(ceiling_rose(), x, y, zc - 0.002))
            lm = 600.0 * arms / 5 + 400.0
            lz = zc - drop + 0.2
        elif kind == "pendant":
            drop = max(0.4, min(0.8, self.h - DOOR_BOX_H - 0.3))
            part = pendant(drop)
            lm, lz = 900.0, zc - drop + 0.15
        else:
            part = ceiling_globe()
            lm, lz = 700.0, zc - 0.15
        bb = bbox(part)
        aabb = (bb[0] + x, bb[1] + x, bb[2] + y, bb[3] + y, bb[4] + zc, bb[5] + zc)
        if any(_ov(aabb, db) for db in self.door_boxes):
            return None
        self.commit("light_" + kind, part, (x, y, 0.0, zc), aabb)
        self.lights.append((x, y, lz, LIGHT_WARM, lm))
        return aabb

    def windows_fixtures(self):
        rnd = self.rnd
        mat = rnd.choice([CURG, CURW])
        pelmet = rnd.random() < 0.35
        for w in self.windows:
            s, u = w["side"], w["u"]
            if self.kind not in ("storage", "bath"):        # the bath is warmed by its water heater
                rw = min(w["w"] * 0.7, 1.1)
                rh = max(0.3, min(0.58, w["sill"] - 0.22))
                part = radiator(rw, rh)
                fr = self.wall_frame(s)
                loc = moved(part, 0.0, 0.0, 0.12)
                # radiator local: front -y; in wall-local coords the room is +v: rotate 180 deg about z
                loc = loc.transformed(lambda p: (u - p[0], 0.1 - p[1], p[2]))
                hm = loc.transformed(fr)
                a = bbox(hm)
                if not any(_ov(a, db) for db in self.door_boxes):
                    self.furniture.extend(hm)
                    self.items.append(("radiator", a))
                    if self._blocks_route(a):
                        for k in self._cells_of(a):
                            self.block[k] = 1
            if self.kind in ("bath", "storage", "kitchen"):
                cm = Mesh()
                Ls = self.side(s)[6]
                ep, en = min(0.1, max(0.0, u - w["w"] / 2 - 0.02)), min(0.1, max(0.0, Ls - u - w["w"] / 2 - 0.02))
                _fold_strip(cm, LACE, -w["w"] / 2 - min(en, 0.06), w["w"] / 2 + min(ep, 0.06), -0.06, -0.09, w["sill"] + (0.03 if self.kind == "kitchen" else w["h"] * 0.5), w["sill"] + w["h"] + 0.12, 8)
                tube(cm, CHROME, (-w["w"] / 2 - en, -0.075, w["sill"] + w["h"] + 0.13), (w["w"] / 2 + ep, -0.075, w["sill"] + w["h"] + 0.13), 0.008, 0.008, 4)
                pieces = [cm]
            else:
                Ls = self.side(s)[6]
                e_pos = u - w["w"] / 2            # local +x runs to decreasing u
                e_neg = Ls - (u + w["w"] / 2)
                pieces = [curtain_rod_tulle(w["w"], w["h"], w["sill"], mat, pelmet, e_neg, e_pos)]
                for sd, e in ((-1, e_neg), (1, e_pos)):
                    pieces.append(drape(w["w"], w["h"], w["sill"], sd, mat, e))
            fr = self.wall_frame(s)
            for p in pieces:
                # curtain local: wall at y=0, room at -y -> wall-local v = -y, u = window u - x
                hm = p.transformed(lambda q: (u - q[0], -q[1], q[2])).transformed(fr)
                a = bbox(hm)
                if any(_ov(a, db) for db in self.door_boxes):
                    continue
                self.curtain_mesh.extend(hm)

    # -- finishes
    def finishes(self):
        k = self.kind
        rnd = self.rnd
        z, h = self.z, self.h
        x0, x1, y0, y1 = self.x0, self.x1, self.y0, self.y1
        f = self.finish
        # floor
        if k in ("kitchen", "bath"):
            f.face(TFL, [(x0, y0, z + 0.002), (x1, y0, z + 0.002), (x1, y1, z + 0.002), (x0, y1, z + 0.002)],
                   [(x0, y0), (x1, y0), (x1, y1), (x0, y1)])
        else:
            sw = 0.4 * math.sqrt(0.5)              # strip width: one board length at 45 degrees
            n = max(1, int(math.ceil((x1 - x0) / sw)))
            c45 = math.sqrt(0.5)
            for i in range(n):
                a, b = x0 + i * (x1 - x0) / n, x0 + (i + 1) * (x1 - x0) / n
                sg = 1 if i % 2 == 0 else -1
                uv = lambda x, y: ((x * c45 + sg * y * c45), (-sg * x * c45 + y * c45))
                pts = [(a, y0, z + 0.002), (b, y0, z + 0.002), (b, y1, z + 0.002), (a, y1, z + 0.002)]
                f.face(PARQ, pts, [uv(p[0], p[1]) for p in pts])
        # ceiling
        zc = z + h - 0.002
        f.face(CEIL, [(x0, y1, zc), (x1, y1, zc), (x1, y0, zc), (x0, y0, zc)])
        # walls: (material, z from, z to) bands
        paper = rnd.choice([WPC, WPC, WPG])
        top = h - CORNICE + 0.01
        if k in ("living", "bedroom", "study"):
            bands = [(paper, 0.0, h - 0.45), (CEIL, h - 0.45, top)]
        elif k == "corridor":
            bands = [(DADO, 0.0, 1.4), (paper, 1.4, h - 0.45), (CEIL, h - 0.45, top)]
        else:
            bands = [(DADO, 0.0, 1.5), (CEIL, 1.5, top)]
        for s in SIDES:
            L = self.side(s)[6]
            holes = [(o["u"] - o["w"] / 2, o["u"] + o["w"] / 2, o["sill"], o["sill"] + o["h"])
                     for o in self.doors + self.windows if o["side"] == s]
            loc = Mesh()
            for mat, za, zb in bands:
                wall_with_holes(loc, mat, L, za, zb, holes)
            fr = self.wall_frame(s)
            f.extend(loc.transformed(lambda p: (p[0], p[1] + WALL_OFF, p[2])).transformed(fr))
            # skirting, a moulding under the frieze, a dado rail
            sk = Mesh()
            skm = TFL if k in ("kitchen", "bath") else WAL
            ends = (0.021, L - 0.021) if s in ("x0", "x1") else (0.0, L)
            for a, b in self._runs(ends, [(hh[0], hh[1]) for hh in holes if hh[2] < 0.12]):
                bx(sk, skm, a, b, 0.0, 0.022, 0.0, 0.12, faces="YZxX")
                bx(sk, skm, a, b, 0.0, 0.03, 0.0, 0.03, faces="YZ")
            if k in ("living", "bedroom", "study", "corridor"):
                zm = h - 0.45
                for a, b in self._runs(ends, [(hh[0], hh[1]) for hh in holes if hh[3] > zm - 0.04]):
                    bx(sk, CEIL, a, b, 0.0, 0.022, zm - 0.02, zm + 0.015, faces="YzZxX")
            if k in ("kitchen", "bath", "corridor", "storage"):
                zd = 1.5 if k != "corridor" else 1.4
                for a, b in self._runs(ends, [(hh[0], hh[1]) for hh in holes if hh[3] > zd - 0.03 and hh[2] < zd + 0.03]):
                    bx(sk, DADO, a, b, 0.0, 0.012, zd - 0.015, zd + 0.015, faces="YzZxX")
            f.extend(sk.transformed(lambda p: (p[0], p[1] + WALL_OFF, p[2])).transformed(fr))
        self._cornice()

    @staticmethod
    def _runs(ends, cuts):
        a0, b0 = ends
        segs = [(a0, b0)]
        for c0, c1 in cuts:
            nxt = []
            for a, b in segs:
                if c1 <= a or c0 >= b:
                    nxt.append((a, b))
                    continue
                if c0 > a + 0.01:
                    nxt.append((a, c0))
                if c1 < b - 0.01:
                    nxt.append((c1, b))
            segs = nxt
        return [(a, b) for a, b in segs if b - a > 0.02]

    def _cornice(self):
        x0, x1, y0, y1 = self.x0, self.x1, self.y0, self.y1
        zt = self.z + self.h - 0.002
        prof = [(WALL_OFF, 0.25), (0.02, 0.25), (0.02, 0.225), (0.04, 0.205), (0.04, 0.175), (0.085, 0.135), (0.105, 0.135),
                (0.105, 0.105), (0.15, 0.075), (0.17, 0.075), (0.17, 0.045), (0.215, 0.028), (0.25, 0.0)]
        if self.kind == "storage":
            prof = [(WALL_OFF, 0.08), (0.03, 0.08), (0.08, 0.0)]
        centre = ((x0 + x1) / 2, (y0 + y1) / 2, zt - 1.5)

        def ring(d, dz):
            z = zt - dz
            return [(x0 + d, y0 + d, z), (x1 - d, y0 + d, z), (x1 - d, y1 - d, z), (x0 + d, y1 - d, z)]

        rings = [ring(d, dz) for d, dz in prof]
        for ra, rb in zip(rings, rings[1:]):
            for k in range(4):
                j = (k + 1) % 4
                pts = [ra[k], ra[j], rb[j], rb[k]]
                c = _centroid(pts)
                face_n(self.finish, CEIL, pts, (centre[0] - c[0], centre[1] - c[1], centre[2] - c[2]))

    def splash(self, side, u0, u1, z0, z1):
        """White wall tiles behind a stove/sink/tub on wall `side` (u0..u1, z0..z1 over the floor)."""
        L = self.side(side)[6]
        u0, u1 = max(0.0, u0), min(L, u1)
        if u1 - u0 < 0.05:
            return
        holes = [(o["u"] - o["w"] / 2 - u0, o["u"] + o["w"] / 2 - u0, o["sill"], o["sill"] + o["h"])
                 for o in self.doors + self.windows if o["side"] == side]
        loc = Mesh()
        wall_with_holes(loc, TWALL, u1 - u0, z0, z1, holes)
        fr = self.wall_frame(side)
        self.finish.extend(loc.transformed(lambda p: (p[0] + u0, p[1] + WALL_OFF + 0.004, p[2])).transformed(fr))

    def hang(self, part, name, side, u, zoff, gap=0.006):
        return self.wall(part, name, [side], u, gap=gap, zoff=zoff, margin=0.0)

    def pictures(self, n, kinds=("oil", "photo", "print", "portrait"), zoff=(1.45, 1.75)):
        rnd = self.rnd
        for _ in range(n):
            kind = rnd.choice(kinds)
            w, hh = (rnd.uniform(0.5, 0.9), rnd.uniform(0.4, 0.7)) if kind == "oil" else (rnd.uniform(0.25, 0.45), rnd.uniform(0.3, 0.5))
            self.wall(picture(rnd, w, hh, kind), "picture", "any", "random", gap=0.006, zoff=rnd.uniform(*zoff), route=False, margin=0.05)

    def above(self, p, part, name, dz=0.3, zmin=1.2):
        """Hangs a wall prop above a placed wall item p (centred on it)."""
        if not p or "side" not in p:
            return None
        top = p["aabb"][5] - self.z
        return self.hang(part, name, p["side"], p["u"], max(zmin, top + dz))


# --- room recipes ------------------------------------------------------------------------------------
def _living(L):
    rnd = L.rnd
    area = (L.x1 - L.x0) * (L.y1 - L.y0)
    uph = rnd.choice([DAM, DAM, PLUSH])
    L.wall(sideboard(rnd), "sideboard", "nowin", "centre") or L.wall(sideboard(rnd, 1.2), "sideboard", "nowin", "centre")
    ps = L.wall(sofa(rnd, 1.9, 0.85, uph), "sofa", "nowin", "centre") or L.wall(sofa(rnd, 1.6, 0.8, uph), "sofa", "any", "centre")
    L.above(ps, picture(rnd, rnd.uniform(0.8, 1.1), rnd.uniform(0.55, 0.75), "oil"), "picture", 0.35)
    cx, cy = (L.x0 + L.x1) / 2, (L.y0 + L.y1) / 2
    tgt = (cx + rnd.uniform(-0.4, 0.4), cy + rnd.uniform(-0.4, 0.4))
    t, dining = None, False
    if area > 18 and rnd.random() < 0.8:
        oval = rnd.random() < 0.7
        for tw, n in ((1.8, 6), (1.5, 6), (1.5, 4), (1.2, 4)):
            if tw > 1.6 and area < 24:
                continue
            t = L.free(dining_set(rnd, tw, 1.05 if tw > 1.3 else 0.9, n, oval), "dining_set", tgt)
            if t:
                dining = True
                break
    if not t:
        for r, n in ((0.5, 4), (0.45, 3), (0.45, 2), (0.4, 0)):
            t = L.free(round_table(rnd, r, rnd.choice([WOOL, RUGR]), n), "round_table", tgt)
            if t:
                break
    focus = None
    if t:
        a = t["aabb"]
        focus = ((a[0] + a[1]) / 2, (a[2] + a[3]) / 2)
        if dining:
            L.rug(focus[0], focus[1], a[1] - a[0] + 0.3, a[3] - a[2] + 0.3)
        else:
            L.rug(focus[0], focus[1], 2.4, 1.8)
    if rnd.random() < 0.65:
        pp = L.wall(piano(rnd), "piano", "nowin", "corner")
        L.above(pp, picture(rnd, 0.7, 0.5, rnd.choice(["oil", "portrait"])), "picture", 0.45)
    L.wall(two_door_bookcase(rnd), "bookcase", "any", "corner")
    for k in range(rnd.choice([1, 2, 2])):
        L.wall(armchair(rnd, uph), "armchair", "any", "corner")
    if dining and area > 22 and rnd.random() < 0.6:
        L.wall(round_table(rnd, 0.45, rnd.choice([WOOL, RUGR, BGREEN]), 2), "round_table", "any", "corner")
    r = rnd.random()
    L.wall(tv_kvn(rnd) if r < 0.35 else (radiogram(rnd) if r < 0.6 else radio_on_stand(rnd)), "radio", "any", "corner")
    if rnd.random() < 0.4:
        L.wall(sewing_machine(rnd), "sewing_machine", "any", "random")
    L.wall(floor_lamp(rnd), "floor_lamp", "any", "corner", gap=0.1)
    L.wall(wall_clock(), "wall_clock", "any", "random", gap=0.006, zoff=1.6, route=False)
    L.pictures(rnd.randint(2, 3))
    L.ceiling_light("chandelier", *(focus or (cx, cy)))


def _study(L):
    rnd = L.rnd
    area = (L.x1 - L.x0) * (L.y1 - L.y0)
    door_sides = [d["side"] for d in L.doors]
    opp = {"x0": "x1", "x1": "x0", "y0": "y1", "y1": "y0"}
    cand = [opp[s] for s in door_sides if not any(w["side"] == opp[s] for w in L.windows)]
    sides = L._sides("nowin")
    bs = cand[0] if cand else sides[0]
    mat = rnd.choice([OAK, OAK, WAL])
    L_bs = L.side(bs)[6]
    first = L.wall(sectional_bookcase(rnd, sections=rnd.choice([5, 5, 6]), mat=mat), "bookcase", [bs], "centre")
    if first:
        u0 = first["u"]
        for k, du in enumerate((0.93, -0.93, 1.86, -1.86)):
            if 0.5 < u0 + du < L_bs - 0.5:
                L.wall(sectional_bookcase(rnd, sections=rnd.choice([5, 6]), mat=mat), "bookcase", [bs], u0 + du)
        nb, ny_ = L.side(bs)[4], L.side(bs)[5]
        px, py = L.pt(bs, u0, 1.45)
        t = L.free(desk_set(rnd, rnd.choice(["leather", "leather", "bentwood"])), "desk", (px, py),
                   yaws=[YAW[bs] + math.pi], radius=1.2)
    else:
        t = None
    if not t:
        for dw, dd in ((1.6, 0.85), (1.3, 0.75), (1.1, 0.65)):
            t = L.wall(desk_set(rnd, "bentwood", dw, dd), "desk", "any", "centre") or \
                L.free(desk_set(rnd, "bentwood", dw, dd), "desk", ((L.x0 + L.x1) / 2, (L.y0 + L.y1) / 2))
            if t:
                break
    if t:
        a = t["aabb"]
        L.rug((a[0] + a[1]) / 2, (a[2] + a[3]) / 2, 2.2, 1.7)
        dl = [i for i in L.items if i[0] == "desk"]
        if dl:
            L.lights.append(((a[0] + a[1]) / 2, (a[2] + a[3]) / 2, L.z + 1.05, LIGHT_WARM, 350.0))
    L.wall(two_door_bookcase(rnd), "bookcase", "nowin", "corner")
    for _ in range(2):
        L.wall(sectional_bookcase(rnd, mat=mat), "bookcase", "nowin", "corner")
    pa = L.wall(leather_armchair(rnd), "armchair", "any", "corner")
    L.wall(floor_lamp(rnd), "floor_lamp", "any", "corner", gap=0.1)
    tt = small_table(rnd, 0.7, 0.45, 0.72, WAL, False)
    add(tt, typewriter(), 0.0, 0.0, 0.72)
    L.wall(tt, "typewriter_table", "any", "random")
    if area > 15:
        L.wall(sofa(rnd, 1.8, 0.8, rnd.choice([DAM, PLUSH, LEA])), "sofa", "nowin", "centre")
    L.wall(globe().transformed(lambda p: (p[0] * 1.6, p[1] * 1.6, p[2] * 1.6)), "globe", "any", "corner", gap=0.1)
    L.wall(wall_clock(), "wall_clock", "any", "random", gap=0.006, zoff=1.6, route=False)
    L.pictures(rnd.randint(2, 4), ("portrait", "portrait", "photo", "print"), (1.5, 2.1))
    L.ceiling_light(rnd.choice(["chandelier", "tulip"]))


def _bedroom(L):
    rnd = L.rnd
    area = (L.x1 - L.x0) * (L.y1 - L.y0)
    w = 1.4 if area > 12 else 0.9
    pb = L.wall(bed_group(rnd, w, 2.0), "bed", "nowin", "centre") or L.wall(bed(rnd, w, 2.0), "bed", "any", "centre") \
        or L.wall(bed(rnd, 0.9, 1.9), "bed", "any", "corner")
    if pb:
        if rnd.random() < 0.4:
            L.above(pb, picture(rnd, min(1.6, pb["wa"] - 0.2), 1.0, "rug"), "wall_rug", 0.1, 1.25)
        else:
            L.above(pb, picture(rnd, 0.6, 0.45, rnd.choice(["oil", "print"])), "picture", 0.35)
        a = pb["aabb"]
        s = pb["side"]
        fx, fy = L.pt(s, pb["u"], (a[1] - a[0] if s in ("x0", "x1") else a[3] - a[2]) + 0.45)
        L.rug(fx, fy, 1.8, 1.0)
    L.wall(wardrobe(rnd), "wardrobe", "nowin", "corner")
    pc = L.wall(chest_of_drawers(rnd), "chest", "any", "centre")
    L.above(pc, picture(rnd, 0.5, 0.4, rnd.choice(["photo", "portrait", "print"])), "picture", 0.4)
    L.wall(trumo(rnd), "trumo", "any", "random")
    L.wall(bentwood_chair(rnd), "chair", "any", "corner")
    if rnd.random() < 0.4:
        L.wall(sewing_machine(rnd), "sewing_machine", "any", "random")
    elif rnd.random() < 0.5:
        L.wall(armchair(rnd, rnd.choice([DAM, PLUSH])), "armchair", "any", "corner")
    L.pictures(rnd.randint(1, 2), ("photo", "print", "oil"))
    L.ceiling_light(rnd.choice(["chandelier", "tulip"]))


def _kitchen(L):
    rnd = L.rnd
    ps = L.wall(gas_stove(rnd), "stove", "nowin", "corner")
    if ps:
        L.splash(ps["side"], ps["u"] - 0.4, ps["u"] + 0.4, 0.85, 1.55)
        du = 0.95 * rnd.choice([-1, 1])
        pk = L.wall(sink(rnd), "sink", [ps["side"]], ps["u"] + du) or L.wall(sink(rnd), "sink", "any", "corner")
    else:
        pk = L.wall(sink(rnd), "sink", "any", "corner")
    if pk:
        L.splash(pk["side"], pk["u"] - 0.36, pk["u"] + 0.36, 0.85, 1.55)
    pt_ = L.wall(kitchen_table(rnd), "table", "any", "centre") or L.wall(kitchen_table(rnd, 0.8, 0.55, stools=1), "table", "any", "centre") \
        or L.wall(kitchen_table(rnd, 0.7, 0.5, stools=0), "table", "any", "corner") \
        or L.free(kitchen_table(rnd, 0.8, 0.55, stools=1), "table", ((L.x0 + L.x1) / 2, (L.y0 + L.y1) / 2))
    L.wall(kitchen_dresser(rnd), "dresser", "nowin", "corner")
    if pt_:
        L.above(pt_, wall_cupboard(rnd) if rnd.random() < 0.6 else wall_shelf(rnd), "cupboard", 0.55, 1.45)
    L.wall(wall_cupboard(rnd), "cupboard", "any", "random", gap=0.006, zoff=1.5, route=True)
    if rnd.random() < 0.5:
        L.wall(wall_shelf(rnd, 0.7), "shelf", "any", "random", gap=0.006, zoff=1.6)
    L.wall(stool(ENW), "stool", "any", "corner")
    L.pictures(1, ("print",), (1.6, 1.8))
    L.ceiling_light("pendant")


def _bath(L):
    rnd = L.rnd
    pt_ = None
    for Lt, dt in ((1.7, 0.75), (1.5, 0.7)):
        pt_ = L.wall(bathtub(rnd, Lt, dt), "tub", "long", "corner")
        if pt_:
            break
    if pt_:
        L.splash(pt_["side"], pt_["u"] - pt_["wa"] / 2, pt_["u"] + pt_["wa"] / 2, 0.6, 1.5)
        for du in (pt_["wa"] / 2 - 0.35, -pt_["wa"] / 2 + 0.35):
            if L.hang(tub_fittings(), "tub_fittings", pt_["side"], pt_["u"] + du, 0.0) or \
                    L.hang(tub_fittings(False), "tub_fittings", pt_["side"], pt_["u"] + du, 0.0):
                break
        for du in (pt_["wa"] / 2 + 0.27, -pt_["wa"] / 2 - 0.27):
            if L.wall(titan(), "titan", [pt_["side"]], pt_["u"] + du, gap=0.08):
                break
        L.above(pt_, towel_rail(rnd), "towel_rail", 0.95, 1.5)
    L.wall(toilet(rnd), "toilet", "any", "corner")
    pb = L.wall(washbasin(rnd), "washbasin", "any", "centre") or L.wall(washbasin(rnd, 0.48, 0.38), "washbasin", "any", "corner")
    if pb:
        L.splash(pb["side"], pb["u"] - 0.35, pb["u"] + 0.35, 0.82, 1.15)
    L.wall(basin_stool(rnd), "stool", "any", "corner")
    L.wall(towel_rail(rnd, 0.5), "towel_rail", "any", "random", gap=0.006, zoff=1.3, route=False)
    L.ceiling_light("globe")


def _corridor(L):
    rnd = L.rnd
    L.wall(coat_rack(rnd, rnd.choice([0.9, 1.1, 1.3])), "coat_rack", "long", "corner")
    L.wall(hall_mirror(rnd), "hall_mirror", "long", "centre")
    L.wall(trunk(rnd), "trunk", "any", "corner")
    L.wall(bentwood_chair(rnd), "chair", "any", "random")
    if rnd.random() < 0.5:
        L.wall(two_door_bookcase(rnd, 1.0, 0.4), "bookcase", "long", "random")
    L.pictures(rnd.randint(1, 3), ("print", "photo", "oil"))
    # runners along the long axis between the door boxes
    along_x = (L.x1 - L.x0) >= (L.y1 - L.y0)
    a0, a1 = (L.x0, L.x1) if along_x else (L.y0, L.y1)
    cuts = sorted(((db[0], db[1]) if along_x else (db[2], db[3])) for db in L.door_boxes)
    for a, b in Layout._runs((a0 + 0.15, a1 - 0.15), [(c0 - 0.1, c1 + 0.1) for c0, c1 in cuts]):
        if b - a > 0.9:
            c = (a + b) / 2
            wdt = min(0.7, ((L.y1 - L.y0) if along_x else (L.x1 - L.x0)) - 0.5)
            if wdt > 0.4:
                if along_x:
                    L.rug(c, (L.y0 + L.y1) / 2, b - a, wdt, margin=0.1)
                else:
                    L.rug((L.x0 + L.x1) / 2, c, wdt, b - a, margin=0.1)
    n = max(1, int(((a1 - a0) + 0.5) / 3.0))
    for k in range(n):
        c = a0 + (k + 0.5) * (a1 - a0) / n
        if along_x:
            L.ceiling_light("pendant", c, (L.y0 + L.y1) / 2)
        else:
            L.ceiling_light("pendant", (L.x0 + L.x1) / 2, c)


def _storage(L):
    rnd = L.rnd
    for _ in range(2):
        L.wall(storage_rack(rnd, rnd.choice([0.8, 0.9, 1.0])), "rack", "any", "corner", margin=0.01) or \
            L.wall(storage_rack(rnd, 0.6, 0.3), "rack", "any", "corner", margin=0.01)
    L.wall(suitcase_stack(rnd), "suitcases", "any", "corner")
    L.wall(skis(rnd), "skis", "any", "corner")
    L.wall(trunk(rnd), "trunk", "any", "random")
    L.wall(sledge(rnd), "sledge", "any", "random", gap=0.006, zoff=1.3, route=False)
    L.ceiling_light("globe")


RECIPES = {"living": _living, "study": _study, "bedroom": _bedroom, "kitchen": _kitchen, "bath": _bath,
           "corridor": _corridor, "storage": _storage}


def furnish_parts(room):
    """Furnishes a room. Returns {'finish', 'furniture', 'curtains': Mesh (house frame), 'lights':
    [(x, y, z, colour hex, lumens)], 'items': [(name, aabb)], 'door_boxes', 'window_zones', 'layout'}."""
    L = Layout(room)
    L.finishes()
    L.windows_fixtures()
    RECIPES[room["kind"]](L)
    return {"finish": L.finish, "furniture": L.furniture, "curtains": L.curtain_mesh, "lights": L.lights,
            "items": L.items, "door_boxes": L.door_boxes, "window_zones": L.window_zones, "layout": L}


def furnish(room):
    """The room's finishes, furniture and curtains as one geom.Mesh in the house frame."""
    p = furnish_parts(room)
    m = Mesh()
    m.extend(p["finish"])
    m.extend(p["furniture"])
    m.extend(p["curtains"])
    return m


# --- self-test ---------------------------------------------------------------------------------------
def sample_rooms():
    """Rooms of each kind in a couple of sizes with doors on different sides (house frame)."""
    W = lambda side, c, w=1.45, sill=0.85, h=2.0: {"side": side, "c": c, "w": w, "sill": sill, "h": h}
    D = lambda side, c, w=1.1: {"side": side, "c": c, "w": w}
    R = []
    R.append({"name": "living_big", "kind": "living", "x0": 0.0, "x1": 5.6, "y0": 0.0, "y1": 4.9, "z": 3.8, "h": 3.4, "seed": 1,
              "doors": [D("x0", 1.2, 1.3), D("y1", 4.6)], "windows": [W("y0", 1.6), W("y0", 4.1)]})
    R.append({"name": "living_big2", "kind": "living", "x0": 0.0, "x1": 5.6, "y0": 0.0, "y1": 4.9, "z": 3.8, "h": 3.4, "seed": 2,
              "doors": [D("x0", 1.2, 1.3), D("y1", 4.6)], "windows": [W("y0", 1.6), W("y0", 4.1)]})
    R.append({"name": "living_small", "kind": "living", "x0": 10.0, "x1": 14.2, "y0": -2.0, "y1": 2.0, "z": 0.7, "h": 3.4, "seed": 5,
              "doors": [D("x1", 0.0, 1.25)], "windows": [W("x0", 0.0)]})
    R.append({"name": "study", "kind": "study", "x0": 0.0, "x1": 4.0, "y0": 0.0, "y1": 3.6, "z": 0.7, "h": 3.4, "seed": 3,
              "doors": [D("y0", 3.2)], "windows": [W("x0", 1.8)]})
    R.append({"name": "study_big", "kind": "study", "x0": 0.0, "x1": 4.6, "y0": 0.0, "y1": 4.4, "z": 0.7, "h": 3.3, "seed": 8,
              "doors": [D("x1", 0.8)], "windows": [W("y1", 2.3)]})
    R.append({"name": "bedroom", "kind": "bedroom", "x0": 0.0, "x1": 4.5, "y0": 0.0, "y1": 4.0, "z": 0.7, "h": 3.4, "seed": 4,
              "doors": [D("x1", 0.8)], "windows": [W("y1", 2.25)]})
    R.append({"name": "bedroom_small", "kind": "bedroom", "x0": 0.0, "x1": 3.4, "y0": 0.0, "y1": 3.2, "z": 0.7, "h": 3.4, "seed": 9,
              "doors": [D("y0", 2.6)], "windows": [W("y1", 1.7)]})
    R.append({"name": "kitchen", "kind": "kitchen", "x0": 0.0, "x1": 3.0, "y0": 0.0, "y1": 3.0, "z": 0.7, "h": 3.4, "seed": 6,
              "doors": [D("x1", 0.75)], "windows": [W("x0", 1.5, 1.3)]})
    R.append({"name": "kitchen2", "kind": "kitchen", "x0": 0.0, "x1": 3.6, "y0": 0.0, "y1": 2.8, "z": 0.7, "h": 3.4, "seed": 10,
              "doors": [D("y0", 0.8)], "windows": [W("y1", 2.0, 1.3)]})
    R.append({"name": "bath", "kind": "bath", "x0": 0.0, "x1": 2.4, "y0": 0.0, "y1": 2.0, "z": 0.7, "h": 3.4, "seed": 7,
              "doors": [D("x1", 1.3)], "windows": []})
    R.append({"name": "bath2", "kind": "bath", "x0": 0.0, "x1": 2.6, "y0": 0.0, "y1": 2.2, "z": 0.7, "h": 3.4, "seed": 11,
              "doors": [D("y0", 1.9)], "windows": [W("y1", 1.0, 0.8, 1.2, 1.0)]})
    R.append({"name": "corridor", "kind": "corridor", "x0": 0.0, "x1": 6.0, "y0": 0.0, "y1": 1.5, "z": 0.7, "h": 3.4, "seed": 12,
              "doors": [D("x0", 0.75), D("y0", 1.6), D("y0", 4.4), D("y1", 3.0, 1.3)], "windows": []})
    R.append({"name": "corridor_v", "kind": "corridor", "x0": 0.0, "x1": 1.6, "y0": 0.0, "y1": 4.5, "z": 0.7, "h": 3.4, "seed": 13,
              "doors": [D("y0", 0.8), D("x1", 3.2)], "windows": []})
    R.append({"name": "storage", "kind": "storage", "x0": 0.0, "x1": 1.8, "y0": 0.0, "y1": 2.4, "z": 0.7, "h": 3.4, "seed": 14,
              "doors": [D("x1", 1.8)], "windows": []})
    return R


def random_rooms(seed, n):
    """n random rooms (each kind in turn) with 1-3 doors and 0-2 windows on random walls."""
    rnd = random.Random(seed)
    kinds = list(RECIPES)
    sizes = {"living": (4.0, 6.5, 3.8, 5.5), "bedroom": (3.2, 5.0, 3.0, 4.6), "study": (3.2, 4.8, 3.0, 4.4),
             "kitchen": (2.6, 3.8, 2.4, 3.4), "bath": (2.2, 3.0, 1.8, 2.4), "corridor": (4.0, 7.0, 1.3, 1.8),
             "storage": (1.4, 2.2, 1.2, 2.4)}
    out = []
    for k in range(n):
        kind = kinds[k % len(kinds)]
        a0, a1, b0, b1 = sizes[kind]
        W, D = rnd.uniform(a0, a1), rnd.uniform(b0, b1)
        if rnd.random() < 0.5 and kind != "corridor":
            W, D = D, W
        x0, y0 = rnd.uniform(-20, 20), rnd.uniform(-20, 20)
        room = {"name": "rnd%d_%s" % (k, kind), "kind": kind, "x0": x0, "x1": x0 + W, "y0": y0, "y1": y0 + D,
                "z": rnd.choice([0.7, 4.5]), "h": rnd.uniform(3.2, 3.5), "seed": rnd.randint(0, 999), "doors": [], "windows": []}
        used = []

        def put(side, w):
            L = (W if side in ("y0", "y1") else D)
            for _ in range(20):
                u = rnd.uniform(w / 2 + 0.15, L - w / 2 - 0.15) if L > w + 0.3 else L / 2
                if all(not (s == side and abs(u - uu) < (w + ww) / 2 + 0.3) for s, uu, ww in used):
                    used.append((side, u, w))
                    base = x0 if side in ("y0", "y1") else y0
                    return base + u
            return None

        for _ in range(rnd.randint(1, 3 if kind in ("corridor", "living") else 2)):
            side, w = rnd.choice(SIDES), rnd.choice([1.1, 1.1, 1.3])
            c = put(side, w)
            if c is not None:
                room["doors"].append({"side": side, "c": c, "w": w})
        if kind not in ("corridor", "storage"):
            for _ in range(rnd.randint(0 if kind == "bath" else 1, 2)):
                side = rnd.choice(SIDES)
                w = 0.8 if kind == "bath" else rnd.choice([1.3, 1.45])
                c = put(side, w)
                if c is not None:
                    room["windows"].append({"side": side, "c": c, "w": w, "sill": 1.2 if kind == "bath" else 0.85,
                                            "h": 1.0 if kind == "bath" else 2.0})
        out.append(room)
    return out


def _tri_boxes(mesh):
    for mat, idx, _ in mesh.faces:
        P = [mesh.verts[i] for i in idx]
        for k in range(1, len(P) - 1):
            t = (P[0], P[k], P[k + 1])
            yield mat, (min(p[0] for p in t), max(p[0] for p in t), min(p[1] for p in t), max(p[1] for p in t),
                        min(p[2] for p in t), max(p[2] for p in t))


def check_room(room, parts=None, verbose=True):
    """Asserts the clear-zone rules on a furnished room; returns its triangle count."""
    p = parts or furnish_parts(room)
    L = p["layout"]
    name = room.get("name", room["kind"])
    # door boxes: nothing of the furniture or the curtains
    for db in L.door_boxes:
        for part in ("furniture", "curtains"):
            for mat, tb in _tri_boxes(p[part]):
                assert not _ov(tb, db), "%s: %s %s triangle %s in door box %s" % (name, part, mat, tb, db)
        for mat, tb in _tri_boxes(p["finish"]):          # finish layers hug the walls / floor
            if _ov(tb, db):
                near = (tb[1] < L.x0 + FINISH_T or tb[0] > L.x1 - FINISH_T or tb[3] < L.y0 + FINISH_T or tb[2] > L.y1 - FINISH_T
                        or tb[5] < L.z + FINISH_T)
                assert near, "%s: finish %s triangle %s in door box %s" % (name, mat, tb, db)
    # windows: nothing over the sill in front of a window except the curtains
    for w in L.windows:
        r = L.rect(w["side"], w["u"] - w["w"] / 2, w["u"] + w["w"] / 2, 0.0, WINDOW_ZONE_D)
        zone = r + (L.z + w["sill"] + 0.001, L.z + L.h)
        for mat, tb in _tri_boxes(p["furniture"]):
            assert not _ov(tb, zone), "%s: %s triangle %s over the sill in front of a window" % (name, mat, tb)
    # route: rasterise the furniture triangles that block (independent of the layout's item boxes)
    if L.route_on:
        chk = Layout(room)
        for mat, tb in _tri_boxes(p["furniture"]):
            if chk._blocks_route(tb):
                for k in chk._cells_of(tb):
                    chk.block[k] = 1
        assert chk._route_ok(None), "%s: no %.1f m route between the doors" % (name, ROUTE_W)
    # inside the room
    for part in ("furniture", "curtains", "finish"):
        if not p[part].verts:
            continue
        b = bbox(p[part])
        assert b[0] >= L.x0 - 1e-6 and b[1] <= L.x1 + 1e-6 and b[2] >= L.y0 - 1e-6 and b[3] <= L.y1 + 1e-6, (name, part, b)
        assert b[4] >= L.z - 1e-6 and b[5] <= L.z + L.h + 1e-6, (name, part, b)
    tris = p["finish"].tri_count() + p["furniture"].tri_count() + p["curtains"].tri_count()
    assert tris <= TRI_BUDGET, "%s: %d triangles > %d" % (name, tris, TRI_BUDGET)
    mats = set(f[0] for part in ("finish", "furniture", "curtains") for f in p[part].faces)
    assert mats <= set(FURNITURE_MATERIALS), (name, mats - set(FURNITURE_MATERIALS))
    if verbose:
        names = sorted(set(n for n, _ in p["items"]))
        print("  %-14s %-9s %5d tris  %2d items  route %s  %s" % (name, room["kind"], tris, len(p["items"]),
                                                                  "on " if L.route_on else "off", ", ".join(names)))
    return tris


def self_test():
    assert len(FURNITURE_MATERIALS) <= 40, len(FURNITURE_MATERIALS)
    assert all(k.startswith("AC_F") for k in FURNITURE_MATERIALS)
    assert any("Opal" in k for k in FURNITURE_MATERIALS)
    print("props:")
    for name, fn in PROPS.items():
        m = fn(random.Random(1))
        b = bbox(m)
        t = m.tri_count()
        assert t > 0, name
        assert t <= 3500, (name, t)
        mats = set(f[0] for f in m.faces)
        assert mats <= set(FURNITURE_MATERIALS), (name, mats - set(FURNITURE_MATERIALS))
        for mat, idx, uv in m.faces:
            assert len(idx) >= 3 and len(uv) == len(idx), name
            n = _newell([m.verts[i] for i in idx])
            assert _dot(n, n) > 1e-16, (name, "degenerate face", mat)
        if name in CEILING_PROPS:
            assert abs(b[5]) < 1e-6, (name, b)
        elif name not in ("gas_heater", "wall_shelf", "towel_rail", "radiator"):
            assert b[4] > -0.006, (name, b)            # splayed legs dip a few mm; the layout clamps them
        assert abs((b[0] + b[1]) / 2) < 0.35, (name, b)
        print("  %-20s %5d tris  %.2f x %.2f x %.2f m" % (name, t, b[1] - b[0], b[3] - b[2], b[5] - b[4]))
    print("rooms:")
    per_kind = {}
    sigs = {}
    for room in sample_rooms():
        p = furnish_parts(room)
        t = check_room(room, p)
        per_kind.setdefault(room["kind"], []).append(t)
        sigs[room["name"]] = (p["furniture"].tri_count(), tuple(sorted(n for n, _ in p["items"])))
        needed = {"living": {"sofa", "sideboard"}, "study": {"desk", "bookcase"}, "bedroom": {"bed", "wardrobe"},
                  "kitchen": {"stove", "sink"}, "bath": {"washbasin", "toilet"}, "corridor": {"coat_rack"},
                  "storage": {"rack"}}[room["kind"]]
        got = set(n for n, _ in p["items"])
        assert needed <= got, (room["name"], needed - got)
    assert sigs["living_big"] != sigs["living_big2"], "two living rooms came out identical"
    # random rooms: the rules must hold whatever the plan
    n = 0
    for room in random_rooms(42, 49):
        check_room(room, verbose=False)
        n += 1
    print("random rooms checked:", n)
    # determinism
    a, b = furnish(sample_rooms()[0]), furnish(sample_rooms()[0])
    assert a.verts == b.verts
    print("triangles per room kind:", {k: (min(v), max(v)) for k, v in per_kind.items()})
    print("furniture self-test OK")


if __name__ == "__main__":
    self_test()
