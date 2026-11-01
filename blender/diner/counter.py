"""The diner's counter and back bar (docs/diner.md): the counter (Formica top in a ribbed aluminium edging, fluted stainless front,
mahogany rail, foot rail on brackets, drawers on the staff side), the things on it (napkin dispensers, sugar pourers, salt and
pepper, ketchup and mustard, menus, the cash register, the cake stand), the back bar (cupboards with raised panels, the
griddle and its hood, the twin coffee urns, the pie case, the milkshake mixer, the toaster, the staff's mugs) and the self-service
coffee table at the west end.

  build(parts) adds the geometry to parts["inside"] (big pieces: collision) and parts["small"] (things on tops: no collision)
  and returns {"coffee": {...}} (the coffee table's tap, mug places and where the hero stands; read by ALyudDiner).
"""
import math
import os
import sys

import bmesh  # noqa: F401
from mathutils import Matrix, Vector  # noqa: F401

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import diner_common as C  # noqa: E402,F401
from diner_common import M, bx, box, rbox, tube, lathe, sphere, prism, ensure_uv, own_uv  # noqa: E402,F401
from plan import *  # noqa: E402,F401,F403
from building_util import oq, ybox  # noqa: E402,F401

T = COUNTER_TOP
Z0 = T - 0.001                  # things on the counter stand 1 mm into its top (no coplanar faces)


# ---- helpers ------------------------------------------------------------------------------------------------------------

def rr_path(x0, x1, y0, y1, r, seg=4):
    """A rounded rectangle, counter-clockwise from the right end of its bottom edge."""
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        for k in range(seg + 1):
            a = math.radians(a0 + 90.0 * k / seg)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def path_normals(path):
    n = len(path)
    nm = []
    for i in range(n):
        a, b, c = Vector(path[i - 1]), Vector(path[i]), Vector(path[(i + 1) % n])

        def nr(p, q):
            d = q - p
            d = d.normalized() if d.length > 1e-9 else Vector((1.0, 0.0))
            return Vector((d.y, -d.x))
        v = nr(a, b) + nr(b, c)
        nm.append(v.normalized() if v.length > 1e-9 else nr(b, c))
    return nm


def offset_path(path, o):
    """The path moved o outward along its vertex normals (the same points sweep() puts its rows at)."""
    return [(p[0] + n.x * o, p[1] + n.y * o) for p, n in zip(path, path_normals(path))]


def sweep(bm, path, rows, mi):
    """A profile swept round a closed counter-clockwise plan path: rows [(out, z), ...] bottom to top (out: metres outward from
    the path); faces get UVs in metres: u along the path, v along the profile (the aluminium's ribs vary along v)."""
    ensure_uv(bm)
    n = len(path)
    nm = path_normals(path)
    s = [0.0]
    for i in range(n):
        s.append(s[-1] + (Vector(path[(i + 1) % n]) - Vector(path[i])).length)
    pv = [0.0]
    for (o0, z0), (o1, z1) in zip(rows, rows[1:]):
        pv.append(pv[-1] + math.hypot(o1 - o0, z1 - z0))
    V = [[bm.verts.new((path[i][0] + nm[i].x * o, path[i][1] + nm[i].y * o, z)) for o, z in rows] for i in range(n)]
    for i in range(n):
        j = (i + 1) % n
        for k in range(len(rows) - 1):
            f = bm.faces.new((V[i][k], V[j][k], V[j][k + 1], V[i][k + 1]))
            f.material_index = mi
            own_uv(bm, f, ((s[i], pv[k]), (s[i + 1], pv[k]), (s[i + 1], pv[k + 1]), (s[i], pv[k + 1])))


def flat(bm, poly, z, mi, up=True):
    vs = [bm.verts.new((x, y, z)) for x, y in poly]
    f = bm.faces.new(vs if up else vs[::-1])
    f.material_index = mi
    return f


def extrude(bm, mi, axis, a0, a1, poly):
    """A convex polygon extruded along an axis ('x': poly in (y, z); 'y': poly in (x, z)) from a0 to a1; closed."""
    def P(a, p):
        return (a, p[0], p[1]) if axis == "x" else (p[0], a, p[1])
    lo = [bm.verts.new(P(a0, p)) for p in poly]
    hi = [bm.verts.new(P(a1, p)) for p in poly]
    n = len(poly)
    new = [bm.faces.new((lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i])) for i in range(n)]
    new.append(bm.faces.new(lo[::-1]))
    new.append(bm.faces.new(hi))
    for f in new:
        f.material_index = mi
    bmesh.ops.recalc_face_normals(bm, faces=new)
    return new


def L(bm, prof, mat, c, seg=16, cap0=False, cap1=True, mats=None):
    """lathe with a material name (or mats: a list of material indices per band)."""
    lathe(bm, prof, mats if mats is not None else M(mat), seg=seg, c=c, cap0=cap0, cap1=cap1)


def P(bm, pts, c, seg=16, cap0=False, cap1=True):
    """A lathe from [(r, z, material name of the band that ends at this point), ...] (the first point's name is unused)."""
    lathe(bm, [(p[0], p[1]) for p in pts], [M(p[2]) for p in pts[1:]], seg=seg, c=c, cap0=cap0, cap1=cap1)


def cone(bm, a, b, r0, r1, mi, seg=10, cap_b=True):
    """A tapered cylinder from a (radius r0) to b (radius r1) with a cap at b only (the back is against a surface)."""
    ensure_uv(bm)
    a, b = Vector(a), Vector(b)
    x = (b - a).normalized()
    up = Vector((0, 0, 1)) if abs(x.z) < 0.95 else Vector((1, 0, 0))
    y = up.cross(x).normalized()
    z = x.cross(y)
    ra = [bm.verts.new(a + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * r0) for i in range(seg)]
    rb = [bm.verts.new(b + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * r1) for i in range(seg)]
    Ln = (b - a).length
    ua = math.pi * (r0 + r1) / seg
    for i in range(seg):
        j = (i + 1) % seg
        f = bm.faces.new((ra[i], ra[j], rb[j], rb[i]))
        f.material_index = mi
        own_uv(bm, f, ((ua * i, 0.0), (ua * (i + 1), 0.0), (ua * (i + 1), Ln), (ua * i, Ln)))
    if cap_b:                                                       # a fan from a centre vertex (no sliver triangles)
        cv = bm.verts.new(b)
        for i in range(seg):
            bm.faces.new((cv, rb[i], rb[(i + 1) % seg])).material_index = mi


def rbox_nb(bm, mi, *args, **kw):
    """rbox for things standing on a surface: the faces turned down are dropped (nothing stacks under it)."""
    kw.setdefault("edges", "top")
    new = rbox(bm, mi, *args, **kw)
    bmesh.ops.delete(bm, geom=[f for f in new if f.is_valid and f.normal.z < -0.9], context="FACES_ONLY")


def pull_bar(bm, mi, a, b, off, r=0.006, d=(0.0, -1.0, 0.0)):
    """A bar handle from a to b standing off the face along d on two posts."""
    a, b, d = Vector(a), Vector(b), Vector(d)
    tube(bm, a + d * off, b + d * off, r, mi, seg=6)
    tube(bm, a, a + d * off, r * 0.8, mi, seg=4, caps=False)
    tube(bm, b, b + d * off, r * 0.8, mi, seg=4, caps=False)


def flutes(bm, xa, xb, y, z0, z1, mi, pitch=0.05, base=0.008, amp=0.005):
    """Vertical flutes on a front facing -y (a wave of 4 columns per pitch), the brushed grain running up them."""
    ensure_uv(bm)
    n = max(1, round((xb - xa) / pitch))
    p = (xb - xa) / n
    cols = []
    for i in range(n):
        for q in range(4):
            cols.append((xa + p * (i + q / 4.0), y - (base + amp * math.sin(2 * math.pi * q / 4.0))))
    cols.append((xb, y - base))
    vb = [bm.verts.new((x, yy, z0)) for x, yy in cols]
    vt = [bm.verts.new((x, yy, z1)) for x, yy in cols]
    for i in range(len(cols) - 1):
        f = bm.faces.new((vb[i], vb[i + 1], vt[i + 1], vt[i]))
        f.material_index = mi
        own_uv(bm, f, ((z0, cols[i][0]), (z0, cols[i + 1][0]), (z1, cols[i + 1][0]), (z1, cols[i][0])))


def raised_panel(bm, mi, x0, x1, y, z0, z1, side=-1, th=0.02, inset=0.07, r=0.004, d=0.006):
    """A cupboard door whose back is on y, facing side (-1: -y, 1: +y): a frame of stiles and rails round a recessed, bevelled
    field. Nothing is stacked closer than 12 mm (the glitch lint): the frame stands th off the wall, the field th - d."""
    ya, yb = (y - th, y) if side < 0 else (y, y + th)
    fr = "y" if side < 0 else "Y"
    bx(bm, mi, x0, x0 + inset, ya, yb, z0, z1, faces=fr + "xXzZ")
    bx(bm, mi, x1 - inset, x1, ya, yb, z0, z1, faces=fr + "xXzZ")
    bx(bm, mi, x0 + inset, x1 - inset, ya, yb, z0, z0 + inset, faces=fr + "zZ")
    bx(bm, mi, x0 + inset, x1 - inset, ya, yb, z1 - inset, z1, faces=fr + "zZ")
    pa, pb = (ya + d, yb - 0.001) if side < 0 else (ya + 0.001, yb - d)
    rbox(bm, mi, x0 + inset - 0.004, x1 - inset + 0.004, pa, pb, z0 + inset - 0.004, z1 - inset + 0.004, r=r, seg=1)


def raised_panel_x(bm, mi, x, y0, y1, z0, z1, th=0.02, inset=0.06, r=0.004, d=0.006):
    """The same on a +x facing wall at x."""
    xa, xb = x, x + th
    bx(bm, mi, xa, xb, y0, y0 + inset, z0, z1, faces="XyYzZ")
    bx(bm, mi, xa, xb, y1 - inset, y1, z0, z1, faces="XyYzZ")
    bx(bm, mi, xa, xb, y0 + inset, y1 - inset, z0, z0 + inset, faces="Xz")
    bx(bm, mi, xa, xb, y0 + inset, y1 - inset, z1 - inset, z1, faces="XZ")
    rbox(bm, mi, xa + 0.001, xb - d, y0 + inset - 0.004, y1 - inset + 0.004, z0 + inset - 0.004, z1 - inset + 0.004, r=r, seg=1)


def tilt_rbox(bm, mi, cx, cy, z0, sx, sy, sz, ax, r=0.002, nb=False):
    """A rounded slab centred on (cx, cy) standing on z0, tipped by ax about the x axis at its foot (nb: no bottom face)."""
    new = rbox(bm, mi, cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2, z0, z0 + sz, r=r, seg=1,
               rot=Matrix.Rotation(ax, 3, "X"), pivot=(cx, cy, z0))
    if nb:
        bmesh.ops.delete(bm, geom=[f for f in new if f.is_valid and f.normal.z < -0.9], context="FACES_ONLY")


# ---- the counter -------------------------------------------------------------------------------------------------------

def counter(bm):
    x0, x1 = COUNTER_X
    y0, y1 = COUNTER_Y
    steel, chrome, mah, dark = M("DN_Steel"), M("DN_Chrome"), M("DN_Mahogany"), M("DN_Dark")
    ensure_uv(bm)
    # the body: a recessed black kick, the stainless walls, chrome belts at the foot and under the rail
    kp = rr_path(x0 + 0.06, x1 - 0.06, y0 + 0.06, y1 - 0.06, 0.06, 4)
    sweep(bm, kp, [(0.0, 0.0), (0.0, 0.125)], dark)
    flat(bm, kp, 0.125, dark, up=False)
    bp = rr_path(x0, x1, y0, y1, 0.12, 5)
    sweep(bm, bp, [(0.0, 0.125), (0.0, 0.885)], steel)
    sweep(bm, bp, [(0.0, 0.12), (0.016, 0.124), (0.016, 0.156), (0.0, 0.16)], chrome)
    sweep(bm, bp, [(0.0, 0.84), (0.016, 0.844), (0.016, 0.876), (0.0, 0.88)], chrome)
    sweep(bm, bp, [(0.0, 0.88), (0.024, 0.884), (0.024, 0.968), (0.006, 0.976)], mah)       # the mahogany rail under the top
    # the customer side: vertical flutes in panels between chrome strips
    xs = [x0 + 0.2 + (x1 - x0 - 0.4) * i / 5.0 for i in range(6)]
    for a, b in zip(xs, xs[1:]):
        flutes(bm, a + 0.016, b - 0.016, y0, 0.14, 0.86, steel)
    for x in xs:
        rbox(bm, chrome, x - 0.015, x + 0.015, y0 - 0.026, y0 - 0.002, 0.14, 0.86, r=0.004, seg=1)
    # the top: Formica inside a ribbed aluminium edging, rounded at the ends
    ox0, ox1, oy0, oy1 = x0 - 0.04, x1 + 0.04, 0.28, 1.02
    flat(bm, rr_path(ox0, ox1, oy0, oy1, 0.16, 5), 0.976, mah, up=False)
    ep = rr_path(ox0 + 0.012, ox1 - 0.012, oy0 + 0.012, oy1 - 0.012, 0.148, 5)
    sweep(bm, ep, [(0.0, 0.976), (0.009, 0.980), (0.012, 0.987), (0.012, T - 0.008), (0.009, T - 0.002), (0.0, T), (-0.006, T)], M("DN_Aluminum"))
    flat(bm, offset_path(ep, -0.006), T, M("DN_Formica"))
    # the foot rail on brackets, with returns at both ends
    zr, yr = 0.20, 0.24
    ra, rb = x0 + 0.2, x1 - 0.2
    tube(bm, (ra, yr, zr), (rb, yr, zr), 0.024, chrome, seg=12, caps=False)
    for xe in (ra, rb):
        sphere(bm, (xe, yr, zr), 0.024, chrome, seg=12, rings=6)
        tube(bm, (xe, yr, zr), (xe, y0 - 0.012, zr), 0.02, chrome, seg=10, caps=False)
    for xp in [ra + (rb - ra) * k / 5.0 for k in range(6)]:
        tube(bm, (xp, yr, zr - 0.03), (xp, y0 - 0.026, zr - 0.03), 0.01, chrome, seg=8, caps=False)
        cone(bm, (xp, y0 - 0.002, zr - 0.03), (xp, y0 - 0.028, zr - 0.03), 0.026, 0.024, chrome, seg=12)
        tube(bm, (xp, yr, zr - 0.03), (xp, yr, zr), 0.01, chrome, seg=8, caps=False)
    # the staff side: drawers over cupboards in stainless
    n = int((x1 - x0 - 0.4) / 0.72)
    w = (x1 - x0 - 0.4) / n
    for i in range(n):
        a = x0 + 0.2 + i * w + 0.01
        b = a + w - 0.02
        cx = (a + b) / 2
        rbox(bm, steel, a, b, y1 + 0.002, y1 + 0.014, 0.62, 0.82, r=0.004, seg=1)
        rbox(bm, steel, a, b, y1 + 0.002, y1 + 0.014, 0.19, 0.60, r=0.004, seg=1)
        pull_bar(bm, chrome, (cx - 0.09, y1 + 0.014, 0.76), (cx + 0.09, y1 + 0.014, 0.76), 0.03, d=(0, 1, 0))
        pull_bar(bm, chrome, (a + 0.06, y1 + 0.014, 0.40), (a + 0.06, y1 + 0.014, 0.52), 0.03, d=(0, 1, 0))


# ---- things on the counter ----------------------------------------------------------------------------------------------

def napkins(bm, cx, cy):
    """A chrome tapered napkin dispenser, the stack of napkins showing at both ends."""
    z = Z0
    chrome, paper = M("DN_Chrome"), M("DN_Paper")
    rbox_nb(bm, chrome, cx - 0.066, cx + 0.066, cy - 0.04, cy + 0.04, z, z + 0.008, r=0.003, seg=1)
    for k in range(6):                                    # the folded edges at the ends: a stack of slices
        zk = z + 0.008 + k * 0.019
        bx(bm, paper, cx - (0.0585 if k % 2 else 0.055), cx + (0.0585 if k % 2 else 0.055), cy - 0.03, cy + 0.03, zk, zk + 0.019,
           faces="xXZ" if k == 0 else ("xXzZ" if k < 5 else "xXz"))
    a = math.radians(5)
    tilt_rbox(bm, chrome, cx, cy - 0.034, z + 0.008, 0.1, 0.0045, 0.118, a, r=0.0, nb=True)
    tilt_rbox(bm, chrome, cx, cy + 0.034, z + 0.008, 0.1, 0.0045, 0.118, -a, r=0.0, nb=True)
    rbox(bm, chrome, cx - 0.058, cx + 0.058, cy - 0.036, cy + 0.036, z + 0.122, z + 0.136, r=0.005, seg=1)


def sugar(bm, cx, cy, z=None, seg=12):
    """A glass sugar pourer: a glass jar, sugar in it, a chrome screw cap with a flap spout."""
    z = Z0 if z is None else z
    chrome = M("DN_Chrome")
    L(bm, [(0.028, z), (0.036, z + 0.004), (0.039, z + 0.02), (0.038, z + 0.07), (0.033, z + 0.092), (0.03, z + 0.098)], "DN_Glass", (cx, cy, 0), seg)
    L(bm, [(0.0, z + 0.006), (0.034, z + 0.006), (0.0345, z + 0.07), (0.0, z + 0.07)], "DN_China", (cx, cy, 0), seg, cap1=True)
    P(bm, [(0.034, z + 0.09, "DN_Chrome"), (0.0375, z + 0.093, "DN_Chrome"), (0.0375, z + 0.098, "DN_Chrome"), (0.0345, z + 0.102, "DN_Chrome"),
           (0.0375, z + 0.106, "DN_Chrome"), (0.0375, z + 0.112, "DN_Chrome"), (0.03, z + 0.12, "DN_Chrome"), (0.0, z + 0.126, "DN_Chrome")],
      (cx, cy, 0), seg)
    tube(bm, (cx + 0.012, cy, z + 0.12), (cx + 0.03, cy, z + 0.14), 0.011, chrome, seg=8, r1=0.008)
    rbox(bm, chrome, cx + 0.028, cx + 0.04, cy - 0.013, cy + 0.013, z + 0.139, z + 0.142, r=0.0012, seg=1,
         rot=Matrix.Rotation(-math.radians(55), 3, "Y"), pivot=(cx + 0.029, cy, z + 0.14))


def shaker(bm, cx, cy, salt=True, seg=10):
    z = Z0
    fill = "DN_China" if salt else "DN_Dark"
    L(bm, [(0.02, z), (0.025, z + 0.004), (0.026, z + 0.02), (0.021, z + 0.036), (0.023, z + 0.056), (0.022, z + 0.066)], "DN_Glass", (cx, cy, 0), seg)
    L(bm, [(0.0, z + 0.006), (0.0235, z + 0.006), (0.0225, z + 0.03), (0.0, z + 0.03)], fill, (cx, cy, 0), seg, cap1=True)
    P(bm, [(0.0225, z + 0.062, "DN_Chrome"), (0.0245, z + 0.064, "DN_Chrome"), (0.0245, z + 0.07, "DN_Chrome"), (0.021, z + 0.078, "DN_Chrome"),
           (0.011, z + 0.086, "DN_Chrome"), (0.0, z + 0.088, "DN_Chrome")], (cx, cy, 0), seg)
    holes = 4 if salt else 1
    for k in range(holes):
        a = 2 * math.pi * k / holes
        rr = 0.009 if holes > 1 else 0.0
        sphere(bm, (cx + rr * math.cos(a), cy + rr * math.sin(a), z + 0.0875), 0.0025, M("DN_Dark"), seg=4, rings=3)


def squeeze(bm, cx, cy, red=True, seg=12):
    """A ketchup (red) or mustard (yellow) squeeze bottle with a ridged nozzle and a label band in its surface."""
    z = Z0
    b = "DN_RedPlastic" if red else "DN_YellowPlastic"
    lab = "DN_Paper" if red else "DN_EnamelRed"
    P(bm, [(0.0, z, b), (0.025, z, b), (0.031, z + 0.008, b), (0.033, z + 0.02, b), (0.033, z + 0.04, b), (0.033, z + 0.094, lab), (0.033, z + 0.1, b),
           (0.031, z + 0.125, b), (0.02, z + 0.145, b), (0.015, z + 0.152, b), (0.0175, z + 0.155, b), (0.0175, z + 0.162, b), (0.016, z + 0.164, b),
           (0.0165, z + 0.167, b), (0.0165, z + 0.172, b), (0.011, z + 0.186, b), (0.0045, z + 0.2, b), (0.0, z + 0.202, b)], (cx, cy, 0), seg, cap0=True)


def menu_stand(bm, cx, cy):
    """Three menus (the first red) leaning in a chrome holder."""
    z = Z0
    chrome, cream, red = M("DN_Chrome"), M("DN_EnamelCream"), M("DN_EnamelRed")
    rbox_nb(bm, chrome, cx - 0.085, cx + 0.085, cy - 0.04, cy + 0.04, z, z + 0.012, r=0.004, seg=1)
    for k, dy in enumerate((-0.016, 0.0, 0.016)):
        tilt_rbox(bm, red if k == 0 else cream, cx, cy + dy, z + 0.012, 0.13, 0.005, 0.2 - 0.004 * k, math.radians(-11), r=0.0012, nb=True)
    for sx in (-0.07, 0.07):
        tube(bm, (cx + sx, cy - 0.034, z + 0.012), (cx + sx, cy - 0.034, z + 0.06), 0.004, chrome, seg=6)
        tube(bm, (cx + sx, cy - 0.034, z + 0.06), (cx + sx, cy + 0.03, z + 0.06), 0.004, chrome, seg=6)


def tab(bm, mi_paper, mi_dark, x, yc, z, h, hw=0.025, hy=0.0235):
    """A pop-up price tab: a white plate standing on z, a dark numeral bar pair in its faces (built as strips: no stacked faces)."""
    bx(bm, mi_paper, x - hw, x + hw, yc - hy, yc + hy, z, z + h, faces="xXzZ")
    cuts = [(0.0, 0.012, mi_paper), (0.012, 0.026, mi_dark), (0.026, h - 0.028, mi_paper), (h - 0.028, h - 0.014, mi_dark), (h - 0.014, h, mi_paper)]
    for sy in (-1, 1):
        for a, b, m in cuts:
            oq(bm, [(x - hw, yc + sy * hy, z + a), (x + hw, yc + sy * hy, z + a), (x + hw, yc + sy * hy, z + b), (x - hw, yc + sy * hy, z + b)], m, (0, sy, 0))


def arch_ring(bm, mi, yc, ht, outer, inner):
    """A frame ring (outer and inner convex polygons of equal length in (x, z)), thickness 2*ht centred on yc."""
    n = len(outer)
    for i in range(n):
        j = (i + 1) % n
        for sy in (-1, 1):
            y = yc + sy * ht
            oq(bm, [(outer[i][0], y, outer[i][1]), (outer[j][0], y, outer[j][1]), (inner[j][0], y, inner[j][1]), (inner[i][0], y, inner[i][1])], mi, (0, sy, 0))
        for poly, sgn in ((outer, 1), (inner, -1)):
            dx, dz = poly[j][0] - poly[i][0], poly[j][1] - poly[i][1]
            oq(bm, [(poly[i][0], yc - ht, poly[i][1]), (poly[j][0], yc - ht, poly[j][1]), (poly[j][0], yc + ht, poly[j][1]), (poly[i][0], yc + ht, poly[i][1])],
               mi, (sgn * dz, 0, -sgn * dx))


def register(bm, rx, ry):
    """A National-style brass-and-chrome cash register: keys facing the staff (+y), a pop-up display, the drawer open."""
    z = Z0
    brass, chrome, dark, bake, china, paper = M("DN_Brass"), M("DN_Chrome"), M("DN_Dark"), M("DN_Bakelite"), M("DN_China"), M("DN_Paper")
    rbox_nb(bm, chrome, rx - 0.19, rx + 0.19, ry - 0.21, ry + 0.21, z, z + 0.016, r=0.004, seg=1)
    rbox_nb(bm, brass, rx - 0.175, rx + 0.175, ry - 0.2, ry + 0.2, z + 0.03, z + 0.175, r=0.008, seg=1)
    for sx in (-1, 1):                                                  # chrome ribs on the sides
        for k in range(4):
            yy = ry - 0.12 + 0.08 * k
            xa, xb = (rx + 0.175, rx + 0.19) if sx > 0 else (rx - 0.19, rx - 0.175)
            rbox(bm, chrome, xa, xb, yy - 0.012, yy + 0.012, z + 0.06, z + 0.15, r=0.003, seg=1)
    # the cash drawer, pulled out toward the staff
    yf = ry + 0.2
    rbox(bm, brass, rx - 0.16, rx + 0.16, yf + 0.058, yf + 0.074, z + 0.035, z + 0.14, r=0.005, seg=1)
    rbox(bm, chrome, rx - 0.05, rx + 0.05, yf + 0.074, yf + 0.092, z + 0.07, z + 0.1, r=0.004, seg=1)
    bx(bm, dark, rx - 0.15, rx + 0.15, yf, yf + 0.058, z + 0.05, z + 0.056, faces="xXyYZ")
    for sx in (-0.15, 0.15):
        bx(bm, dark, rx + sx - 0.003, rx + sx + 0.003, yf, yf + 0.058, z + 0.056, z + 0.085, faces="xXyYZ")
    for sx in (-0.075, 0.0, 0.075):
        bx(bm, dark, rx + sx - 0.002, rx + sx + 0.002, yf, yf + 0.058, z + 0.056, z + 0.08, faces="xXyYZ")
    for k, sx in enumerate((-0.11, -0.04, 0.035, 0.11)):               # coin stacks
        tube(bm, (rx + sx, yf + 0.03, z + 0.056), (rx + sx, yf + 0.03, z + 0.056 + 0.014 + 0.007 * (k % 2)), 0.011, brass if k % 2 else chrome, seg=8)
    # the keyboard deck: a sloping block (its top dark) with five columns of keys
    p0 = Vector((rx, ry + 0.19, z + 0.2))
    d = Vector((0.0, -0.866, 0.5))
    n = Vector((0.0, 0.5, 0.866))
    tip = p0 + d * 0.2425
    new = extrude(bm, brass, "x", rx - 0.16, rx + 0.16, [(ry - 0.1, z + 0.175), (ry + 0.19, z + 0.175), (p0.y, p0.z), (tip.y, tip.z), (ry - 0.1, tip.z)])
    for f in new:
        if f.normal.dot(n) > 0.99:
            f.material_index = dark
    for r_ in range(4):
        for c_ in range(5):
            base = p0 + d * (0.035 + 0.045 * r_) + Vector((-0.1 + 0.05 * c_, 0, 0))
            tube(bm, base, base + n * 0.014, 0.0105, china if (r_ + c_) % 4 else bake, seg=6)
    # the upper housing and the arched display frame, tabs popping up on both sides
    rbox(bm, brass, rx - 0.175, rx + 0.175, ry - 0.2, ry - 0.08, z + 0.175, z + 0.34, r=0.008, seg=1)

    def arch(w, h0, hh, k_=12):
        return [(rx - w, z + h0), (rx + w, z + h0), (rx + w, z + 0.46)] + \
               [(rx + w * math.cos(math.pi * k / k_), z + 0.46 + hh * math.sin(math.pi * k / k_)) for k in range(1, k_)] + [(rx - w, z + 0.46)]
    yc = ry - 0.13
    arch_ring(bm, brass, yc, 0.018, arch(0.15, 0.34, 0.125), arch(0.12, 0.365, 0.095))
    extrude(bm, dark, "y", yc - 0.001, yc + 0.001, arch(0.12, 0.365, 0.095))
    for k, tx in enumerate((-0.06, 0.0, 0.06)):
        tab(bm, paper, dark, rx + tx, yc, z + 0.4, 0.072 + 0.012 * (k % 2))
    for sx in (-0.16, 0.16):
        tube(bm, (rx + sx, yc, z + 0.335), (rx + sx, yc, z + 0.352), 0.008, chrome, seg=6, caps=False)
        sphere(bm, (rx + sx, yc, z + 0.364), 0.017, brass, seg=10, rings=5)
    sphere(bm, (rx, yc, z + 0.598), 0.014, chrome, seg=8, rings=4)
    # the crank on the right side
    tube(bm, (rx + 0.19, ry - 0.02, z + 0.12), (rx + 0.23, ry - 0.02, z + 0.12), 0.007, chrome, seg=6)
    tube(bm, (rx + 0.23, ry - 0.02, z + 0.12), (rx + 0.23, ry + 0.01, z + 0.16), 0.007, chrome, seg=6)
    sphere(bm, (rx + 0.23, ry + 0.01, z + 0.165), 0.013, bake, seg=8, rings=4)


def cake_stand(bm, cx, cy):
    """A chrome pedestal cake stand with a layer cake under a glass dome."""
    z = Z0
    c = (cx, cy, 0)
    P(bm, [(0.065, z, "DN_Chrome"), (0.07, z + 0.006, "DN_Chrome"), (0.055, z + 0.014, "DN_Chrome"), (0.02, z + 0.03, "DN_Chrome"), (0.016, z + 0.06, "DN_Chrome"),
           (0.016, z + 0.1035, "DN_Chrome")], c, 16)
    zc = z + 0.118
    P(bm, [(0.0, z + 0.104, "DN_China"), (0.135, z + 0.104, "DN_China"), (0.15, z + 0.11, "DN_China"), (0.152, z + 0.118, "DN_ChinaRim"),
           (0.14, zc, "DN_China"), (0.0, zc, "DN_China")], c, 20, cap1=False)
    P(bm, [(0.095, zc, "DN_Crust"), (0.097, zc + 0.014, "DN_Crust"), (0.095, zc + 0.027, "DN_Crust"), (0.098, zc + 0.033, "DN_EnamelCream"),
           (0.095, zc + 0.036, "DN_EnamelCream"), (0.097, zc + 0.05, "DN_Crust"), (0.095, zc + 0.063, "DN_Crust"), (0.1, zc + 0.066, "DN_EnamelCream"),
           (0.1, zc + 0.073, "DN_EnamelCream"), (0.09, zc + 0.078, "DN_EnamelCream"), (0.0, zc + 0.08, "DN_EnamelCream")], c, 16)
    for k in range(8):
        a = 2 * math.pi * k / 8
        sphere(bm, (cx + 0.07 * math.cos(a), cy + 0.07 * math.sin(a), zc + 0.09), 0.0095, M("DN_Berry"), seg=6, rings=4)
    L(bm, [(0.146, zc + 0.005), (0.146, zc + 0.08), (0.142, zc + 0.15), (0.12, zc + 0.205), (0.08, zc + 0.236), (0.035, zc + 0.25), (0.0, zc + 0.253)],
      "DN_Glass", c, 20)
    tube(bm, (cx, cy, zc + 0.252), (cx, cy, zc + 0.262), 0.007, M("DN_Chrome"), seg=8, caps=False)
    sphere(bm, (cx, cy, zc + 0.272), 0.016, M("DN_Chrome"), seg=8, rings=5)


def counter_items(bm):
    """Napkins, sugar, salt and pepper, ketchup and mustard, menus at every second stool pair; the register at the east end;
    a cake under a dome in the middle."""
    x1 = COUNTER_X[1]
    for i, x in enumerate(STOOLS[::2]):
        xx = x + 0.36
        if xx > x1 - 0.55:
            break
        napkins(bm, xx - 0.02, 0.72)
        sugar(bm, xx + 0.1, 0.6)
        shaker(bm, xx - 0.095, 0.585, True)
        shaker(bm, xx - 0.05, 0.585, False)
        if i % 2 == 0:
            squeeze(bm, xx + 0.1, 0.77, True)
            squeeze(bm, xx + 0.15, 0.755, False)
        else:
            menu_stand(bm, xx + 0.1, 0.77)
    cake_stand(bm, 1.41, 0.66)
    register(bm, x1 - 0.32, 0.64)


# ---- the urn (the back bar's and the coffee table's) --------------------------------------------------------------------

def urn(bm, x, y, z, face, seg=18):
    """A polished stainless coffee urn standing on z, its faucet facing face (-1: -y, 1: +y, 2: +x). Returns the spout's outlet
    point (x, y, z)."""
    dx, dy = {-1: (0.0, -1.0), 1: (0.0, 1.0), 2: (1.0, 0.0)}[face]
    dv = Vector((dx, dy, 0.0))
    sv = Vector((-dy, dx, 0.0))
    Pt = Vector((x, y, 0.0))
    chrome, brass, bake = M("DN_Chrome"), M("DN_Brass"), M("DN_Bakelite")

    def at(r, h):
        return Pt + dv * r + Vector((0.0, 0.0, z + h))
    S, Cr = "DN_Steel", "DN_Chrome"
    P(bm, [(0.13, z, Cr), (0.15, z + 0.008, Cr), (0.152, z + 0.03, Cr), (0.14, z + 0.07, Cr), (0.12, z + 0.11, Cr),               # the foot
           (0.15, z + 0.125, S), (0.166, z + 0.158, S), (0.1745, z + 0.164, Cr), (0.1745, z + 0.176, Cr), (0.168, z + 0.182, Cr),   # belly, a bead
           (0.176, z + 0.26, S), (0.178, z + 0.34, S), (0.172, z + 0.44, S), (0.1665, z + 0.49, S),
           (0.1745, z + 0.496, Cr), (0.1745, z + 0.508, Cr), (0.16, z + 0.514, Cr), (0.148, z + 0.56, S),                           # a bead, the shoulder
           (0.156, z + 0.565, Cr), (0.158, z + 0.58, Cr), (0.15, z + 0.592, Cr),                                                 # the lid's rim
           (0.138, z + 0.615, S), (0.105, z + 0.645, S), (0.06, z + 0.668, S), (0.02, z + 0.68, S), (0.014, z + 0.686, Cr), (0.011, z + 0.7, Cr)],
      (x, y, 0), seg, cap1=False)
    sphere(bm, (x, y, z + 0.722), 0.026, bake, seg=10, rings=6)
    # the gauge glass beside the faucet: a glass tube in brass collars, coffee in it
    gd = dv * math.cos(0.75) + sv * math.sin(0.75)
    g = Pt + gd * 0.188
    tube(bm, (g.x, g.y, z + 0.2), (g.x, g.y, z + 0.47), 0.0085, M("DN_Glass"), seg=8)
    tube(bm, (g.x, g.y, z + 0.205), (g.x, g.y, z + 0.37), 0.0058, M("DN_Coffee"), seg=6)
    b = Pt + gd * 0.17
    for zz in (0.19, 0.47):
        tube(bm, (g.x, g.y, z + zz), (g.x, g.y, z + zz + 0.02), 0.0125, brass, seg=8)
        tube(bm, (g.x, g.y, z + zz + 0.01), (b.x, b.y, z + zz + 0.01), 0.006, chrome, seg=5, caps=False)
    # the faucet: a brass boss, a chrome valve, the spout curving down, a black lever on top
    tube(bm, at(0.166, 0.24), at(0.19, 0.24), 0.026, brass, seg=12)
    tube(bm, at(0.19, 0.24), at(0.225, 0.24), 0.017, chrome, seg=10)
    tube(bm, at(0.205, 0.24), at(0.205, 0.262), 0.011, chrome, seg=8)
    tube(bm, at(0.225, 0.24), at(0.25, 0.24), 0.0115, chrome, seg=8, caps=False)
    sphere(bm, tuple(at(0.25, 0.24)), 0.0115, chrome, seg=8, rings=4)
    tube(bm, at(0.25, 0.24), at(0.25, 0.2), 0.0105, chrome, seg=8)
    tube(bm, at(0.205, 0.262), at(0.185, 0.31), 0.006, bake, seg=6)
    sphere(bm, tuple(at(0.183, 0.316)), 0.016, bake, seg=8, rings=4)
    o = at(0.25, 0.2)
    return (o.x, o.y, o.z)


# ---- the back bar -------------------------------------------------------------------------------------------------------

def mug_up(bm, x, y, z, yaw=0.0):
    """A diner mug standing upside down on z, its handle toward yaw."""
    L(bm, [(0.037, z), (0.0385, z + 0.01), (0.04, z + 0.04), (0.0415, z + 0.076), (0.04, z + 0.086), (0.0, z + 0.0865)], None, (x, y, 0), 10,
      mats=[M("DN_ChinaRim"), M("DN_China")])
    ca, sa = math.cos(yaw), math.sin(yaw)

    def pt(r, h):
        return (x + ca * r, y + sa * r, z + h)
    pts = [pt(0.0405, 0.07), pt(0.058, 0.066), pt(0.062, 0.042), pt(0.051, 0.019), pt(0.0395, 0.016)]
    for a, b in zip(pts, pts[1:]):
        tube(bm, a, b, 0.0055, M("DN_China"), seg=4, caps=False)


def pie(bm, x, y, z, berries=False):
    L(bm, [(0.085, z), (0.11, z + 0.035)], "DN_Chrome", (x, y, 0), 16, cap1=False)
    L(bm, [(0.11, z + 0.035), (0.113, z + 0.04), (0.103, z + 0.055), (0.07, z + 0.066), (0.0, z + 0.07)], "DN_Crust", (x, y, 0), 16)
    if berries:
        for k in range(5):
            a = 2 * math.pi * k / 5
            sphere(bm, (x + 0.05 * math.cos(a), y + 0.05 * math.sin(a), z + 0.068), 0.012, M("DN_PieFilling"), seg=6, rings=3)


def pie_case(bm, small, px, py):
    """A glass pie case with a chrome frame and two shelves of pies (the pies go to the small mesh)."""
    z = BACKBAR_TOP
    w, d, h = 0.62, 0.36, 0.4
    chrome = M("DN_Chrome")
    bx(bm, M("DN_EnamelCream"), px - w / 2 - 0.01, px + w / 2 + 0.01, py - d / 2 - 0.01, py + d / 2 + 0.01, z, z + 0.055, faces="xXyY")
    rbox(bm, chrome, px - w / 2 - 0.012, px + w / 2 + 0.012, py - d / 2 - 0.012, py + d / 2 + 0.012, z + 0.055, z + 0.067, r=0.004, seg=1)
    zb = z + 0.067
    bx(bm, M("DN_Glass"), px - w / 2, px + w / 2, py - d / 2, py + d / 2, zb, zb + h, faces="xXyYZ")
    rbox(bm, chrome, px - w / 2 - 0.012, px + w / 2 + 0.012, py - d / 2 - 0.012, py + d / 2 + 0.012, zb + h, zb + h + 0.02, r=0.006, seg=2)
    for sx in (-1, 1):
        for sy in (-1, 1):
            tube(bm, (px + sx * (w / 2 + 0.002), py + sy * (d / 2 + 0.002), zb), (px + sx * (w / 2 + 0.002), py + sy * (d / 2 + 0.002), zb + h), 0.008, chrome,
                 seg=6, caps=False)
    zs = zb + 0.2
    rbox(bm, chrome, px - w / 2 + 0.01, px + w / 2 - 0.01, py - d / 2 + 0.01, py + d / 2 - 0.01, zs, zs + 0.008, r=0.002, seg=1)
    for zz in (zb + 0.002, zs + 0.008):
        for k in range(2):
            pie(small, px - 0.15 + 0.3 * k, py, zz, berries=(k == 1))


def mixer(bm, x, y, z):
    """A Hamilton Beach triple-spindle milkshake mixer: chrome base and column, a cream motor head, three spindles over
    stainless cups."""
    chrome, steel, cream, bake = M("DN_Chrome"), M("DN_Steel"), M("DN_EnamelCream"), M("DN_Bakelite")
    rbox_nb(bm, chrome, x - 0.24, x + 0.24, y - 0.12, y + 0.14, z, z + 0.028, r=0.008, seg=1)
    for k in range(3):
        cx = x - 0.14 + 0.14 * k
        L(bm, [(0.06, z + 0.028), (0.058, z + 0.034), (0.045, z + 0.04)], "DN_Chrome", (cx, y - 0.025, 0), 14, cap1=False)
        L(bm, [(0.032, z + 0.04), (0.046, z + 0.06), (0.052, z + 0.2), (0.054, z + 0.215), (0.049, z + 0.214), (0.044, z + 0.06), (0.0, z + 0.052)],
          "DN_Steel", (cx, y - 0.025, 0), 16, cap0=True, cap1=False)
        tube(bm, (cx, y - 0.025, z + 0.16), (cx, y - 0.025, z + 0.46), 0.0055, chrome, seg=6, caps=False)
        L(bm, [(0.0, z + 0.14), (0.026, z + 0.15), (0.03, z + 0.16), (0.0, z + 0.165)], "DN_Chrome", (cx, y - 0.025, 0), 10)
    tube(bm, (x, y + 0.1, z + 0.028), (x, y + 0.1, z + 0.5), 0.032, steel, seg=12)
    rbox(bm, cream, x - 0.22, x + 0.22, y - 0.08, y + 0.15, z + 0.46, z + 0.6, r=0.035, seg=2)
    rbox(bm, chrome, x - 0.23, x + 0.23, y - 0.095, y - 0.07, z + 0.5, z + 0.545, r=0.004, seg=1)
    for sx in (-0.1, 0.1):
        cone(bm, (x + sx, y - 0.094, z + 0.522), (x + sx, y - 0.125, z + 0.522), 0.015, 0.012, bake, seg=10)
    tube(bm, (x + 0.2, y + 0.15, z + 0.56), (x + 0.25, y + 0.15, z + 0.64), 0.006, chrome, seg=6)
    sphere(bm, (x + 0.254, y + 0.15, z + 0.65), 0.014, bake, seg=8, rings=4)


def toaster(bm, x, y, z):
    chrome, bake, dark = M("DN_Chrome"), M("DN_Bakelite"), M("DN_Dark")
    rbox_nb(bm, bake, x - 0.15, x + 0.15, y - 0.09, y + 0.09, z, z + 0.018, r=0.006, seg=1)
    new = rbox(bm, chrome, x - 0.14, x + 0.14, y - 0.08, y + 0.08, z + 0.03, z + 0.2, r=0.03, seg=2)
    top = max(new, key=lambda f: f.normal.z * 10 + f.calc_area())     # the flat top: replaced by strips with two dark slots
    ax, ay = 0.11, 0.05
    bmesh.ops.delete(bm, geom=[f for f in new if f.is_valid and f.normal.z < -0.9], context="FACES_ONLY")
    bmesh.ops.delete(bm, geom=[top], context="FACES_ONLY")
    ys = [y - ay, y - 0.043, y - 0.027, y + 0.027, y + 0.043, y + ay]
    ys = [y - ay, y - 0.045, y - 0.025, y - 0.0, y + 0.025, y + 0.045, y + ay]
    for k in range(len(ys) - 1):
        oq(bm, [(x - ax, ys[k], z + 0.2), (x + ax, ys[k], z + 0.2), (x + ax, ys[k + 1], z + 0.2), (x - ax, ys[k + 1], z + 0.2)],
           dark if k in (1, 4) else chrome, (0, 0, 1))
    for sx, (xa, xb) in ((-1, (x - 0.153, x - 0.136)), (1, (x + 0.136, x + 0.153))):
        rbox(bm, bake, xa, xb, y - 0.055, y + 0.055, z + 0.06, z + 0.16, r=0.004, seg=1)
    cone(bm, (x - 0.1, y - 0.08, z + 0.09), (x - 0.1, y - 0.105, z + 0.09), 0.006, 0.006, chrome, seg=6)
    sphere(bm, (x - 0.1, y - 0.11, z + 0.09), 0.012, bake, seg=8, rings=4)


def griddle(bm, small):
    gx0, gx1 = GRILL_X
    y0, y1 = BACKBAR_Y, YI_B
    bt = BACKBAR_TOP
    steel, chrome, bake, dark, iron = M("DN_Steel"), M("DN_Chrome"), M("DN_Bakelite"), M("DN_Dark"), M("DN_CastIron")
    a, b = gx0 + 0.002, gx1 - 0.002
    bx(bm, dark, a, b, y0 + 0.06, y1 - 0.03, 0.0, 0.1, faces="xXyYz")
    rbox(bm, steel, a, b, y0 - 0.02, y1 - 0.03, 0.1, bt, r=0.01, seg=2, edges="vert")
    # the control panel with knobs, over two cupboard doors
    for k in range(5):
        cx = a + 0.14 + (b - a - 0.28) * k / 4.0
        cone(bm, (cx, y0 - 0.02, 0.84), (cx, y0 - 0.06, 0.84), 0.024, 0.018, bake, seg=10)
    mid = (a + b) / 2
    raised_panel(bm, steel, a + 0.05, mid - 0.02, y0 - 0.02, 0.16, 0.72, th=0.02, inset=0.05)
    raised_panel(bm, steel, mid + 0.02, b - 0.05, y0 - 0.02, 0.16, 0.72, th=0.02, inset=0.05)
    pull_bar(bm, chrome, (mid - 0.1, y0 - 0.04, 0.5), (mid - 0.1, y0 - 0.04, 0.62), 0.03)
    pull_bar(bm, chrome, (mid + 0.1, y0 - 0.04, 0.5), (mid + 0.1, y0 - 0.04, 0.62), 0.03)
    # the plate: a chrome front lip, the grease trough, the cast-iron plate, splash guards
    rbox_nb(bm, chrome, a - 0.004, b + 0.004, y0 - 0.026, y0 - 0.004, bt - 0.002, bt + 0.03, r=0.004, seg=1)
    bx(bm, dark, a + 0.02, b - 0.02, y0 - 0.004, y0 + 0.07, bt - 0.001, bt + 0.012, faces="xXyYZ")
    bx(bm, iron, a + 0.02, b - 0.02, y0 + 0.07, y1 - 0.09, bt + 0.002, bt + 0.036, faces="xXyY")
    xs = [a + 0.02, a + 0.2, a + 0.58, a + 0.62, a + 0.98, a + 1.0, b - 0.02]       # the top in strips: seasoned (dark) and scoured zones
    for k in range(len(xs) - 1):
        oq(bm, [(xs[k], y0 + 0.07, bt + 0.036), (xs[k + 1], y0 + 0.07, bt + 0.036), (xs[k + 1], y1 - 0.09, bt + 0.036), (xs[k], y1 - 0.09, bt + 0.036)],
           dark if k in (1, 4) else iron, (0, 0, 1))
    rbox(bm, steel, a, b, y1 - 0.09, y1 - 0.04, bt + 0.03, bt + 0.3, r=0.006, seg=1)
    for xe in (a, b - 0.05):
        rbox(bm, steel, xe, xe + 0.05, y0 + 0.07, y1 - 0.09, bt + 0.03, bt + 0.14, r=0.006, seg=1)
    # a spatula and patties on the plate
    box(small, (a + 0.35, y0 + 0.26, bt + 0.0385), (0.12, 0.09, 0.003), steel)
    tube(small, (a + 0.41, y0 + 0.26, bt + 0.04), (a + 0.55, y0 + 0.26, bt + 0.06), 0.006, bake, seg=6)
    for k in range(4):
        L(small, [(0.05, bt + 0.0366), (0.052, bt + 0.045), (0.045, bt + 0.052), (0.0, bt + 0.053)], "DN_Patty", (a + 0.6 + 0.14 * k, y0 + 0.3 + 0.06 * (k % 2), 0), 12)


def hood(bm):
    """The griddle's stainless hood: a sloping canopy against the wall with a front lip and louvred filters under it."""
    gx0, gx1 = GRILL_X
    steel, chrome, dark = M("DN_Steel"), M("DN_Chrome"), M("DN_Dark")
    a, b = gx0 - 0.1, gx1 + 0.1
    yw = YI_B
    extrude(bm, steel, "x", a, b, [(yw - 0.68, 1.78), (yw - 0.01, 1.78), (yw - 0.01, 2.32), (yw - 0.25, 2.32), (yw - 0.68, 1.97)])
    rbox(bm, chrome, a - 0.006, b + 0.006, yw - 0.7, yw - 0.675, 1.775, 1.795, r=0.004, seg=1)
    nf = 4
    fw = (b - a - 0.1) / nf
    for k in range(nf):
        fa = a + 0.05 + k * fw + 0.01
        fb = fa + fw - 0.02
        bx(bm, chrome, fa, fb, yw - 0.6, yw - 0.12, 1.765, 1.782, faces="xXyY")
        ys = [yw - 0.6 + 0.0685 * j for j in range(8)]            # the underside in louvre strips: chrome and dark slits
        for j in range(7):
            oq(bm, [(fa, ys[j], 1.765), (fb, ys[j], 1.765), (fb, ys[j + 1], 1.765), (fa, ys[j + 1], 1.765)], dark if j % 2 else chrome, (0, 0, -1))
    tube(bm, (a + 0.05, yw - 0.64, 1.765), (b - 0.05, yw - 0.64, 1.765), 0.014, M("DN_Opal"), seg=8)


def back_bar(bm, small):
    x0, x1 = BACKBAR_X
    y0, y1 = BACKBAR_Y, YI_B
    gx0, gx1 = GRILL_X
    steel, chrome, dark, mah = M("DN_Steel"), M("DN_Chrome"), M("DN_Dark"), M("DN_Mahogany")
    for sa, sb in ((x0, gx0 - 0.002), (gx1 + 0.002, x1)):
        bx(bm, dark, sa, sb, y0 + 0.06, y1, 0.0, 0.1, faces="xXyz")
        bx(bm, steel, sa, sb, y0, y1, 0.1, 0.889, faces="xXyz")
        rbox(bm, steel, sa - (0.02 if sa == x0 else 0.0), sb + (0.02 if sb == x1 else 0.0), y0 - 0.03, y1, 0.889, BACKBAR_TOP, r=0.008, seg=1, edges="top")
        n = max(1, round((sb - sa) / 0.78))
        w = (sb - sa) / n
        for i in range(n):                                      # cupboard bays: a raised-panel door under a drawer
            a = sa + i * w + 0.015
            b = sa + (i + 1) * w - 0.015
            cx = (a + b) / 2
            raised_panel(bm, steel, a, b, y0, 0.12, 0.66, th=0.02, inset=0.07)
            raised_panel(bm, steel, a, b, y0, 0.685, 0.865, th=0.02, inset=0.045)
            pull_bar(bm, chrome, (b - 0.035, y0 - 0.02, 0.34), (b - 0.035, y0 - 0.02, 0.5), 0.028)
            pull_bar(bm, chrome, (cx - 0.09, y0 - 0.02, 0.775), (cx + 0.09, y0 - 0.02, 0.775), 0.028)
    # the stainless backsplash, and the mahogany shelf with the staff's mugs
    bx(bm, steel, x0, x1, y1 - 0.016, y1, BACKBAR_TOP, 1.4, faces="yxX")
    for sa, sb in ((x0, -4.5), (-3.4, x1)):
        rbox(bm, mah, sa, sb, y1 - 0.2, y1 - 0.016, 1.4, 1.43, r=0.006, seg=1, edges="top")
    for k in range(6):
        mug_up(small, -3.25 + 0.115 * k, y1 - 0.1, 1.43, yaw=math.radians(-60 + 25 * k))
    for k in range(5):
        mug_up(small, 2.8 + 0.115 * k, y1 - 0.1, 1.43, yaw=math.radians(-40 + 30 * k))
    for k in range(5):
        mug_up(small, 6.1 + 0.115 * k, y1 - 0.1, 1.43, yaw=math.radians(-70 + 30 * k))
    # the twin urns on a stand with a drip shelf
    ux0, ux1 = URNS_X
    uy = y1 - 0.3
    sy0 = uy - 0.34
    top = BACKBAR_TOP + 0.1
    bx(bm, steel, ux0 - 0.28, ux1 + 0.28, sy0, y1 - 0.06, BACKBAR_TOP, top, faces="xXY")
    rbox_nb(bm, chrome, ux0 - 0.28, ux1 + 0.28, sy0, y1 - 0.06, top, top + 0.008, r=0.004, seg=1)
    xa, xb = ux0 - 0.28, ux1 + 0.28                              # the front: steel with rows of dark vent slots, all in one plane
    zs = (BACKBAR_TOP, BACKBAR_TOP + 0.03, BACKBAR_TOP + 0.075, top)
    cuts = [xa]
    for xc in URNS_X:
        cuts += [xc - 0.09 + j * 0.06 + o for j in range(4) for o in (0.0, 0.015)]
    cuts.append(xb)
    for za, zb, mm in ((zs[0], zs[1], steel), (zs[2], zs[3], steel)):
        oq(bm, [(xa, sy0, za), (xb, sy0, za), (xb, sy0, zb), (xa, sy0, zb)], mm, (0, -1, 0))
    for k in range(len(cuts) - 1):
        slot = k % 2 == 1
        oq(bm, [(cuts[k], sy0, zs[1]), (cuts[k + 1], sy0, zs[1]), (cuts[k + 1], sy0, zs[2]), (cuts[k], sy0, zs[2])], dark if slot else steel, (0, -1, 0))
    for x in URNS_X:
        urn(bm, x, uy, top + 0.008, -1)
    pie_case(bm, small, -2.9, y1 - 0.3)
    mixer(small, -1.95, y1 - 0.38, BACKBAR_TOP)
    toaster(small, -1.1, y1 - 0.3, BACKBAR_TOP)
    griddle(bm, small)
    hood(bm)


# ---- the self-service coffee table --------------------------------------------------------------------------------------

def coffee_station(bm, small):
    x0, x1, y0, y1 = STATION
    mah, chrome, dark = M("DN_Mahogany"), M("DN_Chrome"), M("DN_Dark")
    tz = STATION_TOP
    ensure_uv(bm)
    # the cabinet: mahogany, a dark kick, two raised-panel doors on the +x side
    kp = rr_path(x0, x1 - 0.05, y0 + 0.05, y1 - 0.05, 0.03, 3)
    sweep(bm, kp, [(0.0, 0.0), (0.0, 0.1)], dark)
    flat(bm, kp, 0.1, dark, up=False)
    sweep(bm, rr_path(x0, x1, y0, y1, 0.03, 3), [(0.0, 0.1), (0.0, tz - 0.045)], mah)
    ym = (y0 + y1) / 2
    for a, b in ((y0 + 0.04, ym - 0.015), (ym + 0.015, y1 - 0.04)):
        raised_panel_x(bm, mah, x1, a, b, 0.14, tz - 0.1, th=0.02, inset=0.06)
    for yy in (ym - 0.07, ym + 0.07):
        cone(bm, (x1 + 0.02, yy, 0.55), (x1 + 0.044, yy, 0.55), 0.014, 0.011, M("DN_Bakelite"), seg=10)
    # the Formica top in its ribbed aluminium edging
    ox0, ox1, oy0, oy1 = x0, x1 + 0.04, y0 - 0.03, y1 + 0.03
    flat(bm, rr_path(ox0, ox1, oy0, oy1, 0.05, 4), tz - 0.045, mah, up=False)
    ep = rr_path(ox0 + 0.012, ox1 - 0.012, oy0 + 0.012, oy1 - 0.012, 0.038, 4)
    sweep(bm, ep, [(0.0, tz - 0.045), (0.009, tz - 0.041), (0.012, tz - 0.034), (0.012, tz - 0.008), (0.009, tz - 0.002), (0.0, tz), (-0.006, tz)], M("DN_Aluminum"))
    flat(bm, offset_path(ep, -0.006), tz, M("DN_Formica"))
    # the urn, the drip tray of chrome bars under its spout
    ux, uy = URN_SELF
    tap = urn(bm, ux, uy, tz, 2, seg=16)
    tx, ty = tap[0], tap[1]
    z = tz - 0.001
    bx(bm, chrome, tx - 0.095, tx + 0.095, ty - 0.12, ty + 0.12, z, z + 0.016, faces="xXyY")      # the tray: its top in strips, bars and slots
    xs = [tx - 0.095, tx - 0.083] + [tx - 0.083 + 0.0332 * k for k in range(1, 6)] + [tx + 0.083, tx + 0.095]
    xs = sorted(set([tx - 0.095, tx - 0.083] + [tx - 0.083 + 0.0166 * k for k in range(1, 10)] + [tx + 0.083, tx + 0.095]))
    for k in range(len(xs) - 1):
        slot = 1 <= k <= len(xs) - 3 and k % 2 == 0
        oq(bm, [(xs[k], ty - 0.12, z + 0.016), (xs[k + 1], ty - 0.12, z + 0.016), (xs[k + 1], ty + 0.12, z + 0.016), (xs[k], ty + 0.12, z + 0.016)],
           dark if slot else chrome, (0, 0, 1))
    # a tray of upside-down mugs, saucers, a sugar pourer, a creamer, a spoon cup
    mx, my = -6.85, -1.33
    rbox(small, M("DN_Steel"), mx - 0.17, mx + 0.17, my - 0.21, my + 0.21, tz - 0.001, tz + 0.012, r=0.004, seg=1)
    yaws = (60, 120, 90, 80, 100, 70)
    for i in range(2):
        for j in range(3):
            mug_up(small, mx - 0.07 + i * 0.14, my - 0.12 + j * 0.12, tz + 0.012, yaw=math.radians(yaws[i * 3 + j] + (0 if i else 180)))
    zk = tz - 0.001                                                 # three saucers in a stack, one lathe (a stepped rim)
    pr = [(0.045, zk)]
    for k in range(3):
        pr += [(0.07, zk + 0.016 * k + 0.008), (0.075, zk + 0.016 * k + 0.0125), (0.075, zk + 0.016 * (k + 1)), (0.05, zk + 0.016 * (k + 1))]
    pr = pr[:-1] + [(0.07, zk + 0.016 * 3 - 0.001), (0.0, zk + 0.016 * 3 - 0.001)]
    L(small, pr, None, (-7.09, -1.04, 0), 14, mats=[M("DN_China")])
    sugar(small, -6.95, -0.5, z=tz - 0.001)
    cx, cy = -6.78, -0.52                                           # a creamer: a little china pitcher with a lip and a handle
    L(small, [(0.0, tz - 0.001), (0.03, tz - 0.001), (0.036, tz + 0.02), (0.034, tz + 0.06), (0.04, tz + 0.075), (0.032, tz + 0.077)], None, (cx, cy, 0), 12,
      cap0=True, cap1=False, mats=[M("DN_China")])
    pts = ((cx + 0.035, cy, tz + 0.065), (cx + 0.055, cy, tz + 0.055), (cx + 0.052, cy, tz + 0.03), (cx + 0.034, cy, tz + 0.022))
    for a, b in zip(pts, pts[1:]):
        tube(small, a, b, 0.005, M("DN_China"), seg=4, caps=False)
    L(small, [(0.03, tz - 0.001), (0.036, tz + 0.03), (0.04, tz + 0.07), (0.037, tz + 0.07), (0.033, tz + 0.012), (0.0, tz + 0.012)], "DN_Steel", (-6.72, -1.02, 0), 12, cap0=True, cap1=False)
    for k in range(4):                                              # spoons
        a = 2 * math.pi * k / 4 + 0.4
        tube(small, (-6.72 + 0.008 * math.cos(a), -1.02 + 0.008 * math.sin(a), tz + 0.04),
             (-6.72 + 0.026 * math.cos(a), -1.02 + 0.026 * math.sin(a), tz + 0.115), 0.003, M("DN_Chrome"), seg=4)
    return {"tap": [round(v, 4) for v in tap], "mug_under_tap": [round(tap[0], 4), round(tap[1], 4), STATION_TOP + 0.015],
            "mugs": [mx, my, STATION_TOP + 0.012], "stand": [x1 + 0.45, (y0 + y1) / 2, 0.0]}


def build(parts):
    counter(parts["inside"])
    counter_items(parts["small"])
    back_bar(parts["inside"], parts["small"])
    return {"coffee": coffee_station(parts["inside"], parts["small"])}
