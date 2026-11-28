"""The three dinghies as separate game parts (docs/yacht_school.md "Meshes (part C)"), from boatspec.py numbers.

Every part's origin is its pivot (boatspec.sockets() "parts", written to sockets.json); at rest each part sits in the boat
frame at that pivot with no rotation. Parts per class C in (Luch, Optimist, Cadet):
  SM_Sail_C_Hull   hull + deck/inside, thwarts, hiking straps, mast step, fittings (cleats, blocks, traveller, painter,
                   fairleads, bailer), buoyancy bags (optimist), shroud plates (cadet)
  SM_Sail_C_Mast   mast (luch: two sections + sleeve stop; cadet: with shrouds, forestay and halyards: the cadet's mast is
                   fixed, its rigging goes with it)
  SM_Sail_C_Boom   boom along -X with the vang (kicker) down to the mast foot, the outhaul, the mainsheet blocks
  SM_Sail_C_Main   the mainsail, double layer (port / starboard faces 1 mm apart, closed at the edges), UV0 picture,
                   UV1 (s, h) and UV2 (chord m, distance from the luff m) for the vertex shader
  SM_Sail_Cadet_Jib, SM_Sail_Optimist_Sprit
  SM_Sail_C_Rudder (stock + blade), SM_Sail_C_Tiller, SM_Sail_C_Ext (extension + universal joint), SM_Sail_C_Board
  SM_Sail_C_<part>_LOD1   lighter copies (import_sailing.py puts them in LOD1 of the part)
  SM_Sail_C_Far    the whole rigged boat in < 300 triangles (the far fleet)
"""
import math

from mathutils import Matrix, Vector

import boatspec as B
from sailkit import Mesh, rz, ry, rx, tr, frame_from

CLASS_NAME = {"luch": "Luch", "optimist": "Optimist", "cadet": "Cadet"}
TINT = (1.0, 1.0, 0.0, 1.0)      # COLOR_0: R tint mask on, G open, B 0
WHITE = (0.0, 1.0, 0.0, 1.0)
SHADE = (0.0, 0.8, 0.0, 1.0)


def u_of(s, x):
    return (x + s["loa"] / 2) / s["loa"]


# ================================================================================================ hull lines
# The hulls are lofted from smooth (monotone cubic) curves through boatspec's keys; the luch has finer lines of its own
# (a fine pointed bow with a raked, rounded stem, the planing run aft). Pivots and sockets stay boatspec's numbers.
LUCH_LINES = {
    "half_beam": [(0.0, 0.57), (0.15, 0.615), (0.40, 0.685), (0.60, 0.668), (0.75, 0.585), (0.85, 0.47), (0.92, 0.33),
                  (0.965, 0.19), (0.99, 0.07), (1.0, 0.008)],
    "keel_z": [(0.0, -0.055), (0.12, -0.10), (0.35, -0.13), (0.60, -0.128), (0.76, -0.10), (0.86, -0.045), (0.93, 0.04),
               (0.975, 0.16), (1.0, 0.335)],
    "fullness": [(0.0, 3.3), (0.5, 2.9), (0.8, 2.35), (1.0, 2.0)],      # superellipse exponent: 2 round .. 3+ flat-bottomed
    "deadrise": [(0.0, 0.008), (0.5, 0.025), (0.8, 0.07), (1.0, 0.11)],
    "flare": [(0.0, 0.03), (0.6, 0.06), (1.0, 0.16)],
}


def pchip(keys, t):
    """Monotone cubic (Fritsch-Carlson) through [(t, v), ...], clamped at the ends."""
    xs = [k[0] for k in keys]
    ys = [k[1] for k in keys]
    n = len(xs)
    if t <= xs[0]:
        return ys[0]
    if t >= xs[-1]:
        return ys[-1]
    h = [xs[i + 1] - xs[i] for i in range(n - 1)]
    d = [(ys[i + 1] - ys[i]) / h[i] for i in range(n - 1)]
    m = [0.0] * n
    m[0], m[-1] = d[0], d[-1]
    for i in range(1, n - 1):
        if d[i - 1] * d[i] <= 0:
            m[i] = 0.0
        else:
            w1, w2 = 2 * h[i] + h[i - 1], h[i] + 2 * h[i - 1]
            m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
    i = max(k for k in range(n - 1) if xs[k] <= t)
    s = (t - xs[i]) / h[i]
    return ((2 * s ** 3 - 3 * s ** 2 + 1) * ys[i] + (s ** 3 - 2 * s ** 2 + s) * h[i] * m[i] +
            (-2 * s ** 3 + 3 * s ** 2) * ys[i + 1] + (s ** 3 - s ** 2) * h[i] * m[i + 1])


def _lines(b, key):
    if b == "luch" and key in LUCH_LINES:
        return LUCH_LINES[key]
    return B.BOATS[b][key]


def hb_u(b, u):
    return pchip(_lines(b, "half_beam"), u)


def sheer_u(b, u):
    return pchip(B.BOATS[b]["sheer_z"], u)


def keel_u(b, u):
    return pchip(_lines(b, "keel_z"), u)


def half_beam(b, x):
    return hb_u(b, u_of(B.BOATS[b], x))


def sheer(b, x):
    return sheer_u(b, u_of(B.BOATS[b], x))


def deck_z(b, x, y=0.0):
    """Deck surface (crowned toward the centreline) of the hull as lofted here."""
    s = B.BOATS[b]
    hb = max(half_beam(b, x), 1e-3)
    return sheer(b, x) + s.get("crown", 0.0) * max(0.0, 1.0 - (y / hb) ** 2)


# ================================================================================================ hull sections
def section(b, x, n_side=None, inset=0.0):
    """Port half-section at x: [(y, z)] from the keel (y = 0) to the sheer; inset > 0 gives the inner skin."""
    s = B.BOATS[b]
    u = u_of(s, x)
    Bh = hb_u(b, u) - inset
    S = sheer_u(b, u)
    K = keel_u(b, u) + inset
    C = Bh * s["chine_frac"]
    Zc = K + s["deadrise"] + s["chine_up"] * (S - K)
    rnd = s["round"]
    pts = [(0.0, K), (C * 0.5, K + s["deadrise"] * 0.5 + (Zc - K - s["deadrise"]) * 0.08)]
    if rnd > 0:
        # bottom -> a rounded bilge (quadratic Bezier at the chine) -> topsides
        p0 = (C * (1 - 0.45 * rnd), K + s["deadrise"] * (1 - 0.45 * rnd) + (Zc - K - s["deadrise"]) * 0.15)
        p1 = (C, Zc)
        p2 = (C + (Bh - C) * 0.5 * rnd, Zc + (S - Zc) * 0.5 * rnd)
        for t in (0.0, 0.25, 0.5, 0.75, 1.0):
            a = (1 - t) ** 2
            bb = 2 * (1 - t) * t
            c = t * t
            pts.append((a * p0[0] + bb * p1[0] + c * p2[0], a * p0[1] + bb * p1[1] + c * p2[1]))
        pts.append(((p2[0] + Bh) / 2 + 0.008, (p2[1] + S) / 2))
    else:
        pts.append((C, Zc))
        for t in (0.33, 0.66):
            pts.append((C + (Bh - C) * t + 0.012 * math.sin(math.pi * t), Zc + (S - Zc) * t))
    pts.append((Bh, S))
    return pts


def ring(b, x, inset=0.0):
    """Full section starboard sheer -> keel -> port sheer as 3D points."""
    half = section(b, x, inset=inset)
    st = [(x, -y, z) for y, z in reversed(half)]
    po = [(x, y, z) for y, z in half[1:]]
    return st + po


def stations(s, n, x0=None, x1=None, extra=()):
    x0 = -s["loa"] / 2 if x0 is None else x0
    x1 = s["loa"] / 2 if x1 is None else x1
    xs = [x0 + (x1 - x0) * (0.5 - 0.5 * math.cos(math.pi * i / (n - 1))) for i in range(n)]
    xs = sorted(set([round(v, 5) for v in xs + [e for e in extra if x0 < e < x1]]))
    return xs


# ================================================================================================ hulls
def hull(b, q=1.0):
    s = B.BOATS[b]
    if b == "luch":
        m = luch_hull(q)
        _fittings(m, b, q)
        return m
    name = "SM_Sail_%s_Hull" % CLASS_NAME[b]
    m = Mesh(name + ("_LOD1" if q < 1 else ""))
    n = max(8, int(40 * q))
    ck = s.get("cockpit")
    extra = []
    if ck:
        extra = [ck["x0"], ck["x1"]]
    if s["deck"] == "foredeck":
        extra = [s["foredeck_x"]]
    xs = stations(s, n, extra=extra)
    tint_hull = b in ("optimist", "cadet")
    m.colour = TINT if tint_hull else WHITE
    rows = [ring(b, x) for x in xs]
    # outer skin (rows run stern -> bow, columns starboard sheer -> port sheer): normals outward
    m.quad_grid(rows, "SL_Hull", flip=False, smooth=True)
    if s["deck"] == "decked":
        _deck_decked(m, b, xs, rows)
    else:
        _open_hull(m, b, xs, rows, q)
    m.smooth_slots = {"SL_Hull", "SL_Wood"}
    m.sharp_angle = 32.0
    m.colour = WHITE
    _fittings(m, b, q)
    _open_details(m, b, xs, q)
    return m


def _open_details(m, b, xs, q):
    """Optimist: straps over the air bags, a mast step and the mast clamp on the thwart, the bow transom's breasthook,
    knees at the stern. Cadet: plank lines and a king plank on the foredeck, coamings along the side decks, the chine
    stringers and frames inside (varnished ply), the jib cleats."""
    s = B.BOATS[b]
    hi = q >= 1
    if b == "optimist":
        for sg in (1, -1):
            c = Vector((-0.15, sg * (half_beam(b, -0.15) - 0.21), sheer(b, -0.15) - 0.13))
            for dx in (-0.32, 0.0, 0.32):
                ring_ = []
                for k in range(11 if hi else 7):
                    a = math.pi * 2 * k / (10 if hi else 6)
                    fx = math.sqrt(max(0.0, 1 - (dx / 0.55) ** 2))
                    ring_.append((c.x + dx, c.y + math.cos(a) * (0.095 * fx + 0.006), c.z + math.sin(a) * (0.11 * fx + 0.006)))
                m.pipe(ring_, 0.006, "SL_Strap", seg=4)
        xb2 = s["loa"] / 2 - 0.25
        z2 = sheer(b, xb2) - 0.16
        for dy in (-0.14, 0.14):
            m.pipe([(xb2 - 0.115, dy, z2 - 0.09), (xb2 - 0.115, dy, z2 + 0.095), (xb2 + 0.115, dy, z2 + 0.095),
                    (xb2 + 0.115, dy, z2 - 0.09)], 0.006, "SL_Strap", seg=4)
        mx = s["mast_x"]
        zk = keel_u(b, u_of(s, mx))
        m.bx("SL_Wood", mx - 0.08, mx + 0.08, -0.06, 0.06, zk + 0.01, zk + 0.06)
        zt = sheer(b, mx) - 0.05
        m.bx("SL_Steel", mx - 0.035, mx + 0.035, -0.04, 0.04, zt + 0.012, zt + 0.018)
        xb = s["loa"] / 2 - 0.03
        m.prism([(xb - 0.10, -0.18), (xb, -0.26), (xb, 0.26), (xb - 0.10, 0.18)], sheer(b, xb) - 0.04, sheer(b, xb) - 0.01, "SL_Wood")
        xt = -s["loa"] / 2 + 0.02
        for sg in (1, -1):
            yy = sg * (half_beam(b, xt) - 0.06)
            m.prism([(xt, yy - 0.012), (xt + 0.14, yy - 0.012), (xt + 0.14, yy + 0.012), (xt, yy + 0.012)],
                    sheer(b, xt) - 0.13, sheer(b, xt) - 0.02, "SL_Wood")
    if b == "cadet":
        fx = s["foredeck_x"]
        xb = s["loa"] / 2 - 0.01
        # plank lines on the foredeck (thin dark seams) and the king plank
        for yy in (-0.33, -0.22, -0.11, 0.11, 0.22, 0.33):
            pts = [(x, yy, deck_z(b, x, yy) + 0.0125) for x in [fx + (xb - fx) * k / 6 for k in range(7)]
                   if abs(yy) < half_beam(b, x) - 0.03]
            if len(pts) >= 2:
                for p0, p1 in zip(pts, pts[1:]):
                    m.beam(p0, p1, 0.004, 0.0012, "SL_Dark")
        m.beam((fx + 0.02, 0, deck_z(b, fx) + 0.0125), (xb - 0.02, 0, deck_z(b, xb - 0.02) + 0.0125), 0.07, 0.003, "SL_Wood")
        # coamings: an upright varnished board along each side deck's inner edge
        sd = s["side_deck"]
        for sg in (1, -1):
            xs2 = [x for x in xs if xs[0] + 0.15 < x < fx - 0.02]
            pts = [(x, sg * (half_beam(b, x) - sd), sheer(b, x) + 0.04) for x in xs2]
            for p0, p1 in zip(pts, pts[1:]):
                m.beam(p0, p1, 0.012, 0.075, "SL_Wood")
        # chine stringers and frames inside
        for sg in (1, -1):
            pts = []
            for x in xs[2:-2]:
                sec = section(b, x, inset=0.02)
                y, z = sec[2]
                pts.append((x, sg * y, z + 0.01))
            m.pipe(pts, 0.012, "SL_Wood", seg=5)
        for x in (-0.95, -0.35, 0.35):
            sec = section(b, x, inset=0.019)          # (its outer face clear of the skin's inner one)
            half = [(x, y, z) for y, z in sec]
            ribs = [(x, -y, z) for _, y, z in reversed(half)] + half[1:]
            for p0, p1 in zip(ribs, ribs[1:]):
                m.beam(p0, p1, 0.02, 0.025, "SL_Wood")
        for sg in (1, -1):
            cleat(m, -0.35, sg * (half_beam(b, -0.35) - sd * 0.5), sheer(b, -0.35) + 0.012, L=0.07)


def _cap(m, pts, slot, reverse=False):
    pts = list(reversed(pts)) if reverse else list(pts)
    return m.face(pts, slot)


def _smooth01(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def _luch_half_skin(x, nb):
    """Port half of the luch's skin at x, keel -> lip: [(y, z)] and the index of each kind of point.
    Superellipse section (flat run aft, rounder and V'd forward, flared topsides), a boot stripe band at a fixed depth
    under the gunwale (sweeps up to the stem with the sheer), and a rolled gunwale lip."""
    b = "luch"
    s = B.BOATS[b]
    u = u_of(s, x)
    hb = hb_u(b, u)
    S = sheer_u(b, u)
    K = keel_u(b, u)
    n = pchip(LUCH_LINES["fullness"], u)
    dr = pchip(LUCH_LINES["deadrise"], u)
    fl = pchip(LUCH_LINES["flare"], u)
    L = 0.35 + 0.65 * min(1.0, hb / 0.12)           # lip size (smaller toward the stem)
    Zt = S - 0.028 * L
    Hs = max(Zt - K, 0.004)
    ds = [min(0.165, 0.62 * Hs), min(0.135, 0.52 * Hs), min(0.095, 0.38 * Hs), min(0.045, 0.2 * Hs)]

    def th_of(d):
        return math.acos(min(1.0, max(0.0, d / Hs)) ** (n / 2.0))

    th_top = th_of(ds[0])
    ths = [th_top * (i / nb) ** 0.85 for i in range(nb)] + [th_of(d) for d in ds] + [math.pi / 2]
    pts = []
    for th in ths:
        c, sn = max(0.0, math.cos(th)), max(0.0, math.sin(th))
        y = hb * sn ** (2.0 / n)
        z = Zt - Hs * c ** (2.0 / n)
        fh = (z - K) / Hs
        y *= 1.0 - fl * (1.0 - fh) ** 1.5
        z += dr * (y / max(hb, 1e-4)) * c ** 0.7
        pts.append((y, z))
    lip = [(hb + 0.009 * L, S - 0.019 * L), (hb + 0.0135 * L, S - 0.008 * L), (hb + 0.010 * L, S + 0.003 * L),
           (hb - 0.004 * L, S + 0.008 * L)]
    kinds = {"stripe": nb, "top": nb + 4, "lip0": nb + 5}
    return pts + lip, kinds, hb, L


def _luch_deck_half(x, hb, L, a):
    """Port half of the deck at x from the lip's inner edge to the centreline: [(y, z)], blending (a 0..1) from the plain
    crowned deck into the cockpit section: rounded edge, sloped wall, flat floor, a recessed centre trough."""
    b = "luch"
    s = B.BOATS[b]
    ck = s["cockpit"]
    hw, fz = ck["half_w"], ck["floor_z"]
    crown = s["crown"]
    hbi = max(hb - 0.02 * L, hb * 0.3)

    def dk(y):
        return sheer(b, x) + crown * max(0.0, 1.0 - (y / max(hb, 1e-3)) ** 2)
    # (y in the cockpit, z in the cockpit as (kind, offset)), and the plain-deck fraction of hbi
    ck_pts = [(hbi, ("d", 0.0)), (None, ("d", 0.0)), (None, ("d", 0.0)), (hw + 0.10, ("d", 0.0)),
              (hw + 0.035, ("d", 0.0)), (hw + 0.02, ("d", -0.004)), (hw + 0.006, ("d", -0.018)), (hw, ("d", -0.040)),
              (hw - 0.035, ("f", 0.025)), (hw - 0.05, ("f", 0.006)), (hw - 0.07, ("f", 0.0)), (0.10, ("f", 0.0)),
              (0.085, ("f", -0.004)), (0.072, ("f", -0.028)), (0.06, ("f", -0.035)), (0.0, ("f", -0.035))]
    ck_pts[1] = (hbi - (hbi - (hw + 0.10)) / 3.0, ("d", 0.0))
    ck_pts[2] = (hbi - 2.0 * (hbi - (hw + 0.10)) / 3.0, ("d", 0.0))
    ref = 0.64
    out = []
    for k, (yc, (kind, off)) in enumerate(ck_pts):
        if k == 0:
            yp = hbi
        elif k in (1, 2):
            yp = hbi * (1.0 - 0.1 * k)
        else:
            yp = hbi * min(0.75, yc / ref) if yc > 0 else 0.0
            yp = min(yp, hbi * (0.79 - 0.001 * k))
        y = (1 - a) * yp + a * yc
        zd = dk(y)
        zc = zd + off if kind == "d" else fz + off
        out.append((y, (1 - a) * zd + a * zc))
    return out


def luch_hull(q=1.0):
    """The luch (a Laser-type single-hander) lofted as one closed tube per station: starboard lip -> keel -> port lip ->
    deck (with the cockpit) back to the starboard lip; the transom and the stem tip capped. Smooth shading, sharp edges
    by angle (gunwale lip, transom, cockpit edges). Hull outside takes the tint (COLOR_0.R), the deck stays pale with
    non-skid areas, a navy boot stripe sweeps up to the stem."""
    b = "luch"
    s = B.BOATS[b]
    ck = s["cockpit"]
    m = Mesh("SM_Sail_Luch_Hull" + ("_LOD1" if q < 1 else ""))
    nb = 8 if q >= 1 else 4
    n_st = 30 if q >= 1 else 13
    ramp = 0.15
    nr = 5 if q >= 1 else 2
    extra = [ck["x0"] + ramp * k / nr for k in range(nr + 1)] + [ck["x1"] - ramp * k / nr for k in range(nr + 1)]
    xs = stations(s, n_st, extra=extra)
    xs = [x for x in xs if all(abs(x - e) > 0.012 or x == e for e in extra)]
    rings, meta = [], []
    for x in xs:
        a = _smooth01((x - ck["x0"]) / ramp) * _smooth01((ck["x1"] - x) / ramp)
        half, kinds, hb, L = _luch_half_skin(x, nb)
        deck = _luch_deck_half(x, hb, L, a)
        skin = [(x, -y, z) for y, z in reversed(half)] + [(x, y, z) for y, z in half[1:]]
        dk = [(x, y, z) for y, z in deck] + [(x, -y, z) for y, z in reversed(deck[:-1])]
        rings.append(skin + dk)
        meta.append((a, x))
    nh = len(half)                     # points per half skin (keel = index 0)
    nd = len(deck)
    nsk = 2 * nh - 1
    N = len(rings[0])
    V_ = [[m.bm.verts.new(Vector(p)) for p in r] for r in rings]

    def kind_of(j, a, x):
        """(slot, colour) of the face from ring point j to j + 1."""
        if j < nsk - 1:
            t = (j - (nh - 1)) if j >= nh - 1 else (nh - 2 - j)     # skin segment index from the keel
            if t == kinds["stripe"]:
                return "SL_Stripe", WHITE
            if t >= kinds["top"]:
                return "SL_Hull", WHITE                             # the rolled lip: white gelcoat
            return "SL_Hull", TINT
        if j == nsk - 1 or j == N - 1:
            return "SL_HullDeck", WHITE                             # lip -> deck edge
        k = j - nsk                                                 # deck segment index (port half, then starboard)
        t = k if k < nd - 1 else (2 * nd - 3 - k)
        in_ck = ck["x0"] - 0.01 <= x <= ck["x1"] + 0.35
        if a > 0.6 and t in (10, 14):
            return "SL_NonSkid", WHITE                              # cockpit floor and the trough
        if in_ck and t in (0, 1, 2):
            return "SL_NonSkid", WHITE                              # the side decks where the helm hikes
        return "SL_HullDeck", WHITE

    faces = []
    for i in range(len(xs) - 1):
        a = min(meta[i][0], meta[i + 1][0])
        xm = 0.5 * (meta[i][1] + meta[i + 1][1])
        for j in range(N):
            jj = (j + 1) % N
            slot, col = kind_of(j, a, xm)
            f = m.face([V_[i][j], V_[i][jj], V_[i + 1][jj], V_[i + 1][j]], slot, col=col, smooth=True)
            if f is not None:
                faces.append(f)
    m.colour = WHITE
    faces.append(m.face(list(V_[0]), "SL_Hull", smooth=False))
    faces.append(m.face(list(V_[-1]), "SL_Hull", smooth=False))
    import bmesh
    bmesh.ops.recalc_face_normals(m.bm, faces=[f for f in faces if f is not None])
    m.smooth_slots = {"SL_Hull", "SL_HullDeck", "SL_NonSkid", "SL_Stripe"}
    m.sharp_angle = 40.0
    m.colour = WHITE
    # transom details: the bailer's flap, the drain plug, the moulded rudder-head recess plate
    xt = xs[0]
    m.bx("SL_Black", xt - 0.012, xt + 0.002, -0.06, 0.06, 0.03, 0.075)
    m.lathe([(0.0, 0.0), (0.018, 0.0), (0.018, 0.012), (0.012, 0.018), (0.0, 0.018)], "SL_Black", seg=10 if q >= 1 else 6,
            c=(xt + 0.001, 0.30, 0.10), axis=Matrix(((0, 0, -1), (0, 1, 0), (1, 0, 0))).transposed())
    return m


def _open_hull(m, b, xs, rows, q):
    """Optimist / Cadet: an open hull with an inner skin, a gunwale rim and thick transoms; cadet foredeck and side
    decks as plates."""
    s = B.BOATS[b]
    t = 0.014
    xi0, xi1 = xs[0] + t, xs[-1] - t
    xi = [min(max(x, xi0), xi1) for x in xs]
    xi[0], xi[-1] = xi0, xi1
    inner = []
    for x in xi:
        r = ring(b, x, inset=t)
        # keep the sheer height of the outer skin (the rim is flat)
        sz = sheer(b, x)
        r[0] = (r[0][0], r[0][1], sz)
        r[-1] = (r[-1][0], r[-1][1], sz)
        inner.append(r)
    m.colour = WHITE
    m.quad_grid(inner, "SL_Wood" if b == "cadet" else "SL_Hull", flip=True, smooth=True)
    # transom and bow transom: outer cap (section + straight top) and inner cap, joined by the rim
    _cap(m, rows[0], "SL_Hull", reverse=False)       # stern, facing -X
    _cap(m, inner[0], "SL_Wood" if b == "cadet" else "SL_Hull", reverse=True)       # inner face of the stern transom, facing +X
    _cap(m, rows[-1], "SL_Hull", reverse=True)       # bow transom, facing +X
    _cap(m, inner[-1], "SL_Wood" if b == "cadet" else "SL_Hull", reverse=False)
    # rim: port side, starboard side, and across both transom tops
    m.colour = WHITE
    n = len(xs)
    for k in range(n - 1):
        for side in (-1, 0):
            o0, o1 = rows[k][side], rows[k + 1][side]
            i0, i1 = inner[k][side], inner[k + 1][side]
            quad = [o0, o1, i1, i0] if side == -1 else [o0, i0, i1, o1]
            m.face(quad, "SL_Wood")
    for k, sgn in ((0, 1), (n - 1, -1)):
        o_st, o_po = rows[k][0], rows[k][-1]
        i_st, i_po = inner[k][0], inner[k][-1]
        quad = [o_st, o_po, i_po, i_st] if sgn > 0 else [o_st, i_st, i_po, o_po]
        m.face(quad, "SL_Wood")
    # wooden gunwale capping and rubbing strake outside along the sheer
    sheer_p = [(x, half_beam(b, x) + 0.012, sheer(b, x) - 0.03) for x in xs[1:-1]]
    m.pipe(sheer_p, 0.016, "SL_Rubber", seg=6)
    m.pipe([(x, -y, z) for x, y, z in sheer_p], 0.016, "SL_Rubber", seg=6)
    if s["deck"] == "foredeck":
        # cadet foredeck (plywood, varnished) and narrow side decks
        fx = s["foredeck_x"]
        xb = s["loa"] / 2 - 0.005
        pts = []
        steps = 6
        for k in range(steps + 1):
            x = fx + (xb - fx) * k / steps
            pts.append((x, half_beam(b, x) - 0.005, sheer(b, x)))
        poly2 = [(p[0], p[1]) for p in pts] + [(p[0], -p[1]) for p in reversed(pts)]
        _plate_follow(m, b, poly2, 0.012, "SL_WoodDeck")
        sd = s["side_deck"]
        for sg in (1, -1):
            xs2 = [x for x in xs if xs[0] + 0.05 < x < fx]
            outer = [(x, sg * (half_beam(b, x) - 0.005)) for x in xs2]
            inn = [(x, sg * (half_beam(b, x) - sd)) for x in reversed(xs2)]
            poly = outer + inn
            if sg < 0:
                poly = poly[::-1]
            _plate_follow(m, b, poly, 0.012, "SL_WoodDeck")
        # breakwater on the foredeck
        m.beam((fx + 0.02, -0.45, sheer(b, fx) + 0.02), (fx + 0.30, 0.0, sheer(b, fx) + 0.05), 0.02, 0.06, "SL_Wood")
        m.beam((fx + 0.30, 0.0, sheer(b, fx) + 0.05), (fx + 0.02, 0.45, sheer(b, fx) + 0.02), 0.02, 0.06, "SL_Wood")
    # floor: duckboards (slatted floor boards) along the bottom
    for dy in (-0.16, -0.05, 0.06, 0.17):
        x0, x1 = (-0.75, 0.25) if b == "optimist" else (-1.25, 0.45)
        zf = max(keel_u(b, u_of(s, x0)), keel_u(b, u_of(s, x1))) + 0.035
        m.bx("SL_WoodRaw", x0, x1, dy - 0.035, dy + 0.035, zf - 0.012, zf + 0.012)


def _plate_follow(m, b, poly, t, slot, tint=False):
    """A plate whose top follows the deck height (crowned) over a polygon [(x, y)] (counter-clockwise from above)."""
    old = m.colour
    m.colour = TINT if tint else WHITE
    top = [(x, y, deck_z(b, x, y) + t) for x, y in poly]
    bot = [(x, y, deck_z(b, x, y) - 0.001) for x, y in poly]
    m.face(top, slot)
    m.face(bot[::-1], slot)
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        m.face([bot[i], bot[j], top[j], top[i]], slot)
    m.colour = old


def cleat(m, x, y, z, yaw=0.0, L=0.10):
    R = rz(yaw)
    for c, sz in (((0, 0, 0.012), (L, 0.022, 0.024)), ((0, 0, 0.030), (L * 1.3, 0.016, 0.012))):
        m.box(R @ Vector(c) + Vector((x, y, z)), sz, "SL_Black", rot=R.to_3x3())


def block(m, c, r=0.03, axis=(0, 1, 0), slot="SL_Black"):
    """A pulley block: a sheave disc with cheeks (lathe about the axis) and a shackle above."""
    x, y, zz = frame_from(axis)
    M = Matrix((y, zz, x)).transposed()
    m.lathe([(0.0, -0.012), (r, -0.012), (r, 0.012), (0.0, 0.012)], slot, seg=10, c=c, axis=M)
    m.tube(Vector(c) + Vector((0, 0, r * 0.9)), Vector(c) + Vector((0, 0, r * 1.6)), 0.005, "SL_Steel", seg=5)


def torus(m, c, R, r, slot, seg=14, rs=6, M=None):
    M = M or Matrix.Identity(3)
    pts = []
    for i in range(seg + 1):
        a = 2 * math.pi * i / seg
        pts.append(Vector(c) + M @ Vector((math.cos(a) * R, math.sin(a) * R, 0)))
    m.pipe(pts[:-1] + [pts[0]], r, slot, seg=rs, caps=True)


def strap(m, pts, w=0.045, t=0.006, slot="SL_Strap"):
    for a, b_ in zip(pts, pts[1:]):
        m.beam(a, b_, w, t, slot)


def _fittings(m, b, q):
    s = B.BOATS[b]
    mx = s["mast_x"]
    dz = deck_z(b, mx)
    hi = q >= 1
    # mast step / partner collar
    rm = s["mast_d"] / 2
    m.lathe([(rm + 0.02, dz - 0.01), (rm + 0.02, dz + 0.012), (rm + 0.006, dz + 0.02), (rm + 0.006, dz - 0.01),
             (rm + 0.02, dz - 0.01)], "SL_Black", seg=16 if hi else 8, c=(mx, 0, 0), cap0=False, cap1=False)
    # board slot rim
    bd = s["board"]
    if bd["kind"] == "dagger":
        zt = deck_z(b, bd["x"]) if s["deck"] == "decked" else keel_u(b, u_of(s, bd["x"])) + 0.30
        if s["deck"] != "decked":
            # the daggerboard case: a box from the bottom up to the thwart height
            zb = keel_u(b, u_of(s, bd["x"]))
            xa, xb_ = bd["x"] - bd["chord"] / 2 - 0.02, bd["x"] + bd["chord"] / 2 + 0.02
            m.bx("SL_WoodRaw", xa, xb_, bd["thick"] / 2 + 0.003, bd["thick"] / 2 + 0.018, zb, zt)
            m.bx("SL_WoodRaw", xa, xb_, -(bd["thick"] / 2 + 0.018), -(bd["thick"] / 2 + 0.003), zb, zt)
        else:
            w, L = bd["thick"] / 2 + 0.015, bd["chord"] / 2 + 0.015
            for sg in (1, -1):
                m.bx("SL_Black", bd["x"] - L, bd["x"] + L, sg * w - 0.006, sg * w + 0.006, zt - 0.02, zt + 0.012)
                m.bx("SL_Black", bd["x"] + sg * L - 0.006, bd["x"] + sg * L + 0.006, -w, w, zt - 0.02, zt + 0.012)
    else:
        # cadet centreboard case: a long box on the centreline
        x0, x1 = bd["x"] - 0.12, bd["x"] + 0.62
        zb = keel_u(b, 0.55)
        m.bx("SL_Wood", x0, x1, -0.03, 0.03, zb, sheer(b, 0) - 0.02)
        m.bx("SL_Wood", x0 - 0.02, x1 + 0.02, -0.04, 0.04, sheer(b, 0) - 0.04, sheer(b, 0) - 0.01)
    # pintles / gudgeons on the transom
    rud = s["rudder"]
    xt = -s["loa"] / 2
    for zz in (0.02, 0.20):
        m.bx("SL_Steel", xt - 0.035, xt, -0.02, 0.02, zz, zz + 0.03)
    # hiking strap(s)
    def strap_line(p0, p1, y):
        z0 = p0[2]
        pts = [(p0[0], y, z0), (p0[0] + 0.08, y, z0 + 0.10), ((p0[0] + p1[0]) / 2, y, z0 + 0.16),
               (p1[0] - 0.08, y, z0 + 0.10), (p1[0], y, z0)]
        strap(m, pts)
    p0, p1 = s["strap"]
    strap_line(p0, p1, p0[1])
    if b == "cadet":
        strap_line(p0, p1, -p0[1])
    # mainsheet ratchet block on its spring
    rx_, ry_, rz_ = s["ratchet"]
    rz0 = s["cockpit"]["floor_z"] - 0.036 if b == "luch" else rz_
    m.tube((rx_, ry_, rz0), (rx_, ry_, rz_ + 0.12), 0.012, "SL_Black", seg=8)
    block(m, (rx_, ry_, rz_ + 0.16), 0.035, axis=(0, 1, 0))
    # bow eye / painter: eye bolt and a coiled painter on the foredeck (luch) or tied to the mast thwart
    xb = s["loa"] / 2 - 0.05
    if hi:
        torus(m, (xb, 0, sheer(b, xb) + 0.012), 0.016, 0.004, "SL_Steel", seg=10, rs=4)
    if b == "luch":
        cx = xb - 0.55
        if hi:
            for k in range(3):
                torus(m, (cx, 0.18, deck_z(b, cx, 0.18) + 0.01 + k * 0.008), 0.11 - k * 0.012, 0.006, "SL_Rope", seg=18, rs=4)
            m.pipe([(xb, 0, sheer(b, xb) + 0.015), (cx + 0.11, 0.12, deck_z(b, cx + 0.1, 0.12) + 0.01)], 0.006, "SL_Rope", seg=4)
        # traveller: rope bridle across the aft deck between two fairleads; its block
        tv = s["traveller"]
        xt2 = tv["x"]
        zt = deck_z(b, xt2, tv["half_w"])
        for sg in (1, -1):
            m.bx("SL_Black", xt2 - 0.03, xt2 + 0.03, sg * tv["half_w"] - 0.02, sg * tv["half_w"] + 0.02, zt - 0.01, zt + 0.025)
        m.pipe([(xt2, -tv["half_w"], zt + 0.02), (xt2, -0.1, zt + 0.10), (xt2, 0.1, zt + 0.10), (xt2, tv["half_w"], zt + 0.02)], 0.005, "SL_Rope", seg=4)
        block(m, (xt2, 0.0, zt + 0.13), 0.025, axis=(1, 0, 0))
        # control-line cleats either side of the daggerboard, fairleads at the mast
        for sg in (1, -1):
            cleat(m, bd["x"] - 0.30, sg * 0.20, deck_z(b, bd["x"] - 0.3, 0.2), yaw=0.0)
            cleat(m, -0.6, sg * 0.42, deck_z(b, -0.6, 0.42), yaw=0.0, L=0.08)
        # self-bailer in the cockpit floor (aft)
        ck = s["cockpit"]
        m.bx("SL_Black", ck["x0"] + 0.20, ck["x0"] + 0.34, -0.045, 0.045, ck["floor_z"] - 0.037, ck["floor_z"] - 0.027)
        # deck hand-hold at the transom corners (rope loops)
        for sg in (1, -1):
            y = sg * 0.45
            m.pipe([(xt + 0.10, y, deck_z(b, xt + 0.1, y)), (xt + 0.14, y, deck_z(b, xt + 0.1, y) + 0.05),
                    (xt + 0.22, y, deck_z(b, xt + 0.2, y) + 0.05), (xt + 0.26, y, deck_z(b, xt + 0.26, y))], 0.005, "SL_Rope", seg=4)
    else:
        # thwarts (the optimist has a mast thwart and a centre thwart; the cadet a centre thwart)
        for key in ("thwart_x", "mast_thwart_x"):
            if key in s:
                x = s[key]
                z = sheer(b, x) - 0.05
                hb = half_beam(b, x) - 0.02
                w = 0.20 if key == "thwart_x" else 0.24
                m.bx("SL_Wood", x - w / 2, x + w / 2, -hb, hb, z - 0.012, z + 0.012)
                # knees down to the inner skin
                for sg in (1, -1):
                    m.bx("SL_Wood", x - 0.012, x + 0.012, sg * (hb - 0.06) - 0.03, sg * (hb - 0.06) + 0.03, z - 0.14, z - 0.012)
        # cleats on the gunwale for the sheets / the painter at the bow transom
        for sg in (1, -1):
            x = -0.2
            cleat(m, x, sg * (half_beam(b, x) - 0.03), sheer(b, x), L=0.08)
        if hi:
            m.pipe([(xb, 0, sheer(b, xb) + 0.015), (xb - 0.05, 0, sheer(b, xb) + 0.05), (xb - 0.25, 0.08, sheer(b, xb - 0.25) - 0.02)], 0.006, "SL_Rope", seg=4)
            # a bailer (scoop) on a lanyard on the floor
            bx0 = -0.25 if b == "optimist" else -0.8
            zb = keel_u(b, u_of(s, bx0)) + 0.06
            m.lathe([(0.0, 0.0), (0.06, 0.005), (0.075, 0.07), (0.07, 0.075)], "SL_Yellow", seg=10, c=(bx0, -0.22, zb), cap1=False)
            m.beam((bx0, -0.22, zb + 0.06), (bx0 + 0.10, -0.22, zb + 0.09), 0.025, 0.02, "SL_Yellow")
    if b == "optimist":
        # buoyancy bags: two along the sides, one in the bow
        for sg in (1, -1):
            c = (-0.15, sg * (half_beam(b, -0.15) - 0.21), sheer(b, -0.15) - 0.13)
            ellipsoid(m, c, (0.55, 0.095, 0.11), "SL_Buoyancy", seg=12 if hi else 6, rings=8 if hi else 4)
        xb2 = s["loa"] / 2 - 0.25
        m.box((xb2, 0, sheer(b, xb2) - 0.16), (0.22, 0.48, 0.18), "SL_Buoyancy")
    if b == "cadet":
        # shroud chainplates
        sh = s["shrouds"]
        for sg in (1, -1):
            m.bx("SL_Steel", sh["x"] - 0.01, sh["x"] + 0.01, sg * sh["y"] - 0.004, sg * sh["y"] + 0.004, sh["z"] - 0.05, sh["z"] + 0.05)
        fs = s["forestay"]["bow"]
        m.bx("SL_Steel", fs[0] - 0.02, fs[0] + 0.01, -0.006, 0.006, fs[2] - 0.01, fs[2] + 0.03)
        # jib fairleads on the side decks
        for sg in (1, -1):
            block(m, (-0.05, sg * 0.50, sheer(b, -0.05) + 0.03), 0.02, axis=(1, 0, 0))


def ellipsoid(m, c, ax, slot, seg=12, rings=8):
    """An ellipsoid with semi-axes ax = (x, y, z), long along X (a lathe turned about X)."""
    prof = [(math.sin(math.pi * k / rings) if 0 < k < rings else 0.0, -ax[0] * math.cos(math.pi * k / rings)) for k in range(rings + 1)]
    M = Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).transposed()
    m.lathe(prof, slot, seg, c, False, False, sx=ax[1], sy=ax[2], axis=M)


def _squash_last(m, nverts_hint):
    """Scales the buoyancy spheres just made into long bags (scales verts of SL_Buoyancy faces about their centres)."""
    import bmesh
    bm = m.bm
    idx = m.mi("SL_Buoyancy")
    faces = [f for f in bm.faces if f.material_index == idx]
    verts = {v for f in faces for v in f.verts}
    # two islands: port and starboard
    for sg in (1, -1):
        vs = [v for v in verts if (v.co.y > 0) == (sg > 0)]
        if not vs:
            continue
        c = sum((v.co for v in vs), Vector()) / len(vs)
        for v in vs:
            d = v.co - c
            v.co = c + Vector((d.x * 2.3, d.y * 0.26, d.z * 0.34))


# ================================================================================================ spars and foils
def mast(b, q=1.0):
    s = B.BOATS[b]
    m = Mesh("SM_Sail_%s_Mast%s" % (CLASS_NAME[b], "_LOD1" if q < 1 else ""))
    seg = 16 if q >= 1 else 8
    z0 = s["mast_foot_z"]
    top = s["mast_top_z"] - z0
    r0, r1 = s["mast_d"] / 2, s["mast_d_top"] / 2
    jz = (s["mast_joint_z"] - z0) if s["mast_joint_z"] else None
    if jz:
        prof = [(0.0, 0.0), (r0, 0.0), (r0, jz - 0.02), (r0 * 0.94, jz), (r1, jz + 0.01), (r1, top * 0.93), (r1 * 0.8, top - 0.01), (0.0, top)]
    else:
        prof = [(0.0, 0.0), (r0, 0.0), (r0, top * 0.55), (r1, top - 0.04), (r1 * 0.8, top), (0.0, top)]
    m.lathe(prof, "SL_Alu", seg=seg, smooth=True)
    m.lathe([(r1 + 0.002, top - 0.10), (r1 + 0.004, top - 0.10), (r1 + 0.004, top + 0.004), (0.0, top + 0.012)], "SL_Black", seg=seg, cap0=False)
    gz = s["gooseneck_z"] - z0
    # gooseneck fitting and vang fitting
    m.lathe([(r0 + 0.002, gz - 0.04), (r0 + 0.008, gz - 0.04), (r0 + 0.008, gz + 0.03), (r0 + 0.002, gz + 0.03)], "SL_Black", seg=seg, cap0=False, cap1=False)
    dz = deck_z(b, s["mast_x"]) - z0
    m.bx("SL_Steel", -r0 - 0.025, -r0, -0.012, 0.012, dz + 0.08, dz + 0.12)
    if b == "luch":
        # sleeve stop cleat and the cunningham's cleat on the lower section
        cleat(m, r0 + 0.01, 0, gz + 0.25, yaw=math.pi / 2, L=0.06)
    if b == "cadet":
        hz = s["forestay"]["hound_z"] - z0
        sh = s["shrouds"]
        mx = s["mast_x"]
        # shrouds and forestay (stainless wire) and the halyards down the mast
        for sg in (1, -1):
            m.pipe([(0, 0, hz), (sh["x"] - mx, sg * sh["y"], sh["z"] - z0 + 0.05)], 0.0025, "SL_Steel", seg=4)
        fs = s["forestay"]["bow"]
        m.pipe([(0, 0, hz), (fs[0] - mx, 0, fs[2] - z0 + 0.02)], 0.0025, "SL_Steel", seg=4)
        m.pipe([(r0 + 0.004, 0.006, top - 0.05), (r0 + 0.004, 0.006, dz + 0.2)], 0.003, "SL_RopeBlue", seg=4)
        m.pipe([(r0 + 0.004, -0.006, hz), (r0 + 0.004, -0.006, dz + 0.2)], 0.003, "SL_RopeRed", seg=4)
        m.bx("SL_Steel", -0.012, 0.012, -0.03, 0.03, hz - 0.01, hz + 0.01)
        for sg in (1, -1):
            cleat(m, 0.0, sg * (r0 + 0.012), dz + 0.35, yaw=math.pi / 2, L=0.06)
    if b == "optimist":
        # the sprit's snotter and the halyard
        sz = s["sprit"]["snotter_z"] - z0
        m.lathe([(r0 + 0.002, sz - 0.02), (r0 + 0.01, sz - 0.02), (r0 + 0.01, sz + 0.02), (r0 + 0.002, sz + 0.02)], "SL_RopeRed", seg=8, cap0=False, cap1=False)
    m.closed = False
    return m


def boom(b, q=1.0):
    s = B.BOATS[b]
    m = Mesh("SM_Sail_%s_Boom%s" % (CLASS_NAME[b], "_LOD1" if q < 1 else ""))
    seg = 12 if q >= 1 else 6
    r = s["boom_d"] / 2
    r_mast = s["mast_d"] / 2
    L = s["boom_len"]
    # the boom along -X from the mast's aft face, axis on the gooseneck line (origin on the mast axis)
    m.lathe([(0.0, 0.0), (r, 0.0), (r, L - r_mast - 0.06), (0.0, L - r_mast - 0.06)], "SL_Alu", seg=seg,
            c=(-r_mast - 0.02, 0, 0), axis=Matrix(((0, 0, 1), (0, 1, 0), (-1, 0, 0))).transposed())
    # gooseneck jaw and end fitting
    m.bx("SL_Black", -r_mast - 0.03, -r_mast + 0.005, -0.02, 0.02, -0.025, 0.025)
    m.box((-L + 0.03, 0, 0), (0.07, r * 2 + 0.01, r * 2 + 0.01), "SL_Black")
    # mainsheet blocks under the boom (end and mid) and the outhaul line
    if q >= 1:
        block(m, (-L + 0.08, 0, -r - 0.04), 0.025, axis=(0, 1, 0))
        block(m, (s["boom_block"], 0, -r - 0.04), 0.025, axis=(0, 1, 0))
        m.pipe([(-L + 0.02, 0.0, r + 0.004), (-0.25, 0.0, r + 0.006)], 0.003, "SL_Rope", seg=4)
    # vang (kicker): two blocks and a strop from the boom down to the mast foot (deck)
    dz = deck_z(b, s["mast_x"]) + 0.10 - s["gooseneck_z"]
    if b != "optimist":
        a = (-0.42 if b == "luch" else -0.36, 0.0, -r - 0.01)
        c = (-r_mast - 0.02, 0.0, dz)
        m.pipe([a, c], 0.006, "SL_Rope", seg=4, smooth=False)
        if q >= 1:
            block(m, (a[0] * 0.75 + c[0] * 0.25, 0.0, a[2] * 0.75 + c[2] * 0.25), 0.022, axis=(0, 1, 0))
            block(m, (a[0] * 0.25 + c[0] * 0.75, 0.0, a[2] * 0.25 + c[2] * 0.75), 0.022, axis=(0, 1, 0))
    m.closed = False
    return m


def foil_loft(m, slot, z_top, z_bot, chord_f, x_le_f, thick, n_rows=10, n_pts=12, round_tip=True):
    """A foil blade: sections in XY at heights z (z_top .. z_bot); chord_f(t) and x_le_f(t) with t 0 top .. 1 bottom;
    leading edge toward +X? No: the leading edge is at x_le (larger x), the trailing edge at x_le - chord."""
    rows = []
    for i in range(n_rows + 1):
        t = i / n_rows
        z = z_top + (z_bot - z_top) * t
        c = chord_f(t)
        xl = x_le_f(t)
        ring_ = []
        for k in range(n_pts):
            a = 2 * math.pi * k / n_pts
            # cosine-spaced around: x along the chord, y the thickness (NACA-ish 00xx)
            xc = 0.5 + 0.5 * math.cos(a)            # 1 = leading edge (a = 0) ... 0 = trailing edge
            yt = 5 * thick * (0.2969 * math.sqrt(max(xc, 0)) - 0.126 * xc - 0.3516 * xc ** 2 + 0.2843 * xc ** 3 - 0.1036 * xc ** 4)
            yt = yt * c + 0.0012
            # mirror: xc from leading (1) to trailing (0); 'xc' measured from the trailing edge here
            x = xl - c * (1 - xc)
            y = yt * (1 if math.sin(a) >= 0 else -1) * abs(math.sin(a)) ** 0.0
            ring_.append((x, y if k <= n_pts // 2 else -abs(y), z))
        rows.append(ring_)
    # ensure ring order is counter-clockwise seen from above (+Z) for an outward-facing loft downward
    V_, faces = m.quad_grid(rows, slot, closed_u=True, smooth=True)
    m.face([V_[0][j] for j in range(n_pts)], slot)                  # top cap (normal +Z)
    m.face([V_[-1][j] for j in reversed(range(n_pts))], slot)       # bottom cap
    return V_


def rudder(b, q=1.0):
    s = B.BOATS[b]
    rd = s["rudder"]
    m = Mesh("SM_Sail_%s_Rudder%s" % (CLASS_NAME[b], "_LOD1" if q < 1 else ""))
    blade = "SL_Foil" if b == "luch" else "SL_FoilWood"
    c = rd["chord"]
    zt, zb = 0.02, rd["z_bot"]
    foil_loft(m, blade, zt, zb, lambda t: c * (1 - 0.35 * max(0, (t - 0.75) / 0.25) ** 2), lambda t: -0.03 - 0.03 * t,
              0.11, n_rows=10 if q >= 1 else 4, n_pts=12 if q >= 1 else 6)
    # stock (rudder head): a box cheek pair from the waterline to above the deck, the tiller's socket on top
    zs = rd["tiller_z"] + 0.03
    m.bx("SL_Alu" if b == "luch" else "SL_FoilWood", -0.20, -0.005, -0.032, 0.032, zt - 0.06, zs)
    m.bx("SL_Black", -0.01, 0.025, -0.02, 0.02, 0.0, 0.05)     # pintle straps
    m.bx("SL_Black", -0.01, 0.025, -0.02, 0.02, 0.18, 0.23)
    m.tube((0.01, 0, -0.03), (0.01, 0, 0.26), 0.006, "SL_Steel", seg=6)
    m.closed = False
    return m


def tiller(b, q=1.0):
    s = B.BOATS[b]
    rd = s["rudder"]
    m = Mesh("SM_Sail_%s_Tiller%s" % (CLASS_NAME[b], "_LOD1" if q < 1 else ""))
    L = rd["tiller_len"]
    slot = "SL_Alu" if b == "luch" else "SL_Wood"
    m.beam((-0.15, 0, 0.0), (L, 0, 0.0), 0.032, 0.040, slot)
    m.beam((L * 0.55, 0, 0.0), (L - 0.02, 0, 0.0), 0.036, 0.044, "SL_Black")       # grip
    m.closed = False
    return m


def extension(b, q=1.0):
    s = B.BOATS[b]
    rd = s["rudder"]
    m = Mesh("SM_Sail_%s_Ext%s" % (CLASS_NAME[b], "_LOD1" if q < 1 else ""))
    L = rd["ext_len"]
    seg = 10 if q >= 1 else 5
    m.lathe([(0.012, 0.0), (0.02, 0.0), (0.02, 0.06), (0.012, 0.07)], "SL_Black", seg=seg, axis=Matrix(((0, 0, 1), (0, 1, 0), (-1, 0, 0))), cap1=False)
    m.tube((0.07, 0, 0), (L, 0, 0), 0.0125, "SL_Alu", seg=seg)
    m.tube((L - 0.12, 0, 0), (L, 0, 0), 0.016, "SL_Black", seg=seg)
    m.closed = False
    return m


def board(b, q=1.0):
    s = B.BOATS[b]
    bd = s["board"]
    m = Mesh("SM_Sail_%s_Board%s" % (CLASS_NAME[b], "_LOD1" if q < 1 else ""))
    slot = "SL_Foil" if b == "luch" else "SL_FoilWood"
    c = bd["chord"]
    if bd["kind"] == "dagger":
        zt, zb = bd["top_z"], bd["top_z"] - bd["len"]
        foil_loft(m, slot, zt, zb, lambda t: c * (1 - 0.25 * max(0, (t - 0.8) / 0.2) ** 2), lambda t: c / 2,
                  bd["thick"] / c * 1.3, n_rows=8 if q >= 1 else 3, n_pts=12 if q >= 1 else 6)
        # the handle strap at the top and the stopper
        m.pipe([(c / 2 - 0.04, 0, zt), (c / 2 - 0.06, 0, zt + 0.06), (-c / 2 + 0.06, 0, zt + 0.06), (-c / 2 + 0.04, 0, zt)], 0.008, "SL_Strap", seg=4)
        m.bx("SL_Black", -c / 2 - 0.02, c / 2 + 0.02, -0.02, 0.02, zt - 0.10, zt - 0.08)
    else:
        # centreboard in its own frame: origin at the pivot bolt; the board hangs down and forward of it
        px, pz = bd["pivot"]
        L = bd["len"]
        foil_loft(m, slot, 0.05, -L, lambda t: c * (1 - 0.3 * max(0, (t - 0.8) / 0.2) ** 2), lambda t: 0.06 + 0.10 * t,
                  bd["thick"] / c * 1.3, n_rows=8 if q >= 1 else 3, n_pts=12 if q >= 1 else 6)
        m.tube((0, -0.03, 0), (0, 0.03, 0), 0.01, "SL_Steel", seg=8, up=(0, 0, 1))
    m.closed = False
    return m


# ================================================================================================ sails
def sail(b, which="main", q=1.0):
    """The flat sail on a grid (s across from the luff, h up), two layers 1 mm apart closed round the edges."""
    s = B.BOATS[b]
    cname = CLASS_NAME[b]
    nm = "SM_Sail_%s_%s%s" % (cname, "Main" if which == "main" else "Jib", "_LOD1" if q < 1 else "")
    m = Mesh(nm, uv1=True)
    m.uv2 = m.bm.loops.layers.uv.new("UV2")
    ns = max(4, int((12 if which == "main" else 8) * q))
    nh = max(5, int((22 if b != "optimist" else 14) * q * (0.7 if which == "jib" else 1)))
    if which == "main":
        ox, oz = 0.0, s["gooseneck_z"]
        luff_dx = 0.0 if b == "luch" else -(s["mast_d"] / 2 + 0.006)
    else:
        ox, oz = s["jib"]["tack"]
        luff_dx = 0.0
    # spacing: denser near the luff and the foot (the belly's shape lives there)
    S_ = [(k / ns) ** 1.15 for k in range(ns + 1)]
    H_ = [(k / nh) for k in range(nh + 1)]

    def P(i, j, y):
        h, sv = H_[i], S_[j]
        x, z = B.sail_point(b, sv, h, which)
        return (x - ox + luff_dx * (1 - sv), y, z - oz)

    def uv0(i, j):
        h, sv = H_[i], S_[j % (ns + 1)]
        return B.sail_uv(b, B.sail_point(b, sv, h, which), which)

    def uv1(i, j):
        return (S_[j % (ns + 1)], H_[i])

    def chord(i):
        a = B.sail_point(b, 0.0, H_[i], which)
        c = B.sail_point(b, 1.0, H_[i], which)
        return math.hypot(a[0] - c[0], a[1] - c[1])

    t = 0.0005
    stb = [[P(i, j, -t) for j in range(ns + 1)] for i in range(nh + 1)]
    prt = [[P(i, j, t) for j in range(ns + 1)] for i in range(nh + 1)]
    # starboard face looks to -Y: with rows up (z) and columns aft (-x), (i,j)->(i,j+1)->(i+1,j+1) turns... pick by test
    Vs, fs = m.quad_grid(stb, "SL_Sail", uvf=uv0, uv1f=uv1, colf=lambda i, j: (0.0, 1.0, 1.0, 1.0), flip=False)
    Vp, fp = m.quad_grid(prt, "SL_Sail", uvf=uv0, uv1f=uv1, colf=lambda i, j: (0.0, 1.0, 0.0, 1.0), flip=True)
    _fix_side(fs, Vector((0, -1, 0)))
    _fix_side(fp, Vector((0, 1, 0)))
    # uv2 = (chord m, distance from the luff m)
    for f in fs + fp:
        if f is None:
            continue
        for l in f.loops:
            sv, hv = l[m.uv1].uv
            i = min(range(nh + 1), key=lambda k: abs(H_[k] - hv))
            cl = chord(i)
            l[m.uv2].uv = (cl, sv * cl)
    # closing strip round the edge (luff, leech, foot, head): UVs as the edge's own
    border = ([(0, j) for j in range(ns + 1)] + [(i, ns) for i in range(1, nh + 1)] +
              [(nh, j) for j in range(ns - 1, -1, -1)] + [(i, 0) for i in range(nh - 1, 0, -1)])
    for k in range(len(border)):
        a, c = border[k], border[(k + 1) % len(border)]
        q4 = [Vs[a[0]][a[1]], Vs[c[0]][c[1]], Vp[c[0]][c[1]], Vp[a[0]][a[1]]]
        f = m.face(q4, "SL_Sail", uvs=[uv0(*a), uv0(*c), uv0(*c), uv0(*a)], uv1s=[uv1(*a), uv1(*c), uv1(*c), uv1(*a)],
                   col=(0.0, 1.0, 0.5, 1.0))
        if f is not None:
            # outward = away from the sail's centre
            cen = Vector(P(nh // 2, ns // 2, 0))
            if f.normal.dot(f.calc_center_median() - cen) < 0:
                f.normal_flip()
            for l in f.loops:
                sv, hv = l[m.uv1].uv
                l[m.uv2].uv = (chord(min(range(nh + 1), key=lambda kk: abs(H_[kk] - hv))), 0.0)
    # the luch's sleeve (around the mast) and battens (stiffer strips: geometry ribs in the pockets, both sides)
    if b == "luch" and which == "main":
        rr = s["mast_d"] / 2 + 0.004
        z0 = B.sail_point(b, 0, 0)[1] - oz
        z1 = B.sail_point(b, 0, 1)[1] - oz - 0.02
        segs = 10 if q >= 1 else 6
        u_sl = 0.02
        rows = []
        for k in range(nh + 1):
            z = z0 + (z1 - z0) * k / nh
            rows.append([(rr * math.cos(2 * math.pi * a / segs), rr * math.sin(2 * math.pi * a / segs), z) for a in range(segs)])
        x0_, x1_, zz0, zz1 = B.sail_box(b)
        Vsl, fsl = m.quad_grid(rows, "SL_Sail", closed_u=True,
                               uvf=lambda i, j: (0.004 + 0.025 * j / segs, (z0 + (z1 - z0) * i / nh + oz - zz0) / (zz1 - zz0)),
                               uv1f=lambda i, j: (0.0, H_[i]), colf=lambda i, j: (0.0, 1.0, 0.5, 1.0), smooth=True)
        for f in fsl:
            for l in f.loops:
                l[m.uv2].uv = (0.0, 0.0)
        m.face([Vsl[0][j] for j in reversed(range(segs))], "SL_Sail", uvs=[(0.01, 0.002)] * segs, col=(0.0, 1.0, 0.5, 1.0))
        m.face([Vsl[-1][j] for j in range(segs)], "SL_Sail", uvs=[(0.01, 0.998)] * segs, col=(0.0, 1.0, 0.5, 1.0))
    m.closed = True
    return m


def _fix_side(faces, want):
    for f in faces:
        if f is not None and f.normal.dot(want) < 0:
            f.normal_flip()


def sprit(b, q=1.0):
    s = B.BOATS[b]
    m = Mesh("SM_Sail_Optimist_Sprit%s" % ("_LOD1" if q < 1 else ""))
    pk = s["main"]["peak"]
    sz = s["sprit"]["snotter_z"]
    end = (pk[0] + 0.03, 0.03, pk[1] - sz - 0.03)
    m.tube((0.03, 0.03, 0.0), end, s["sprit"]["d"] / 2, "SL_Wood" if False else "SL_Alu", seg=10 if q >= 1 else 5)
    m.box(end, (0.05, 0.05, 0.05), "SL_Black")
    m.closed = False
    return m


# ================================================================================================ far boat
def far(b):
    """The whole boat in < 300 triangles: hull (6 x 5 sections), mast, boom, sail(s) as single-sided pairs."""
    s = B.BOATS[b]
    m = Mesh("SM_Sail_%s_Far" % CLASS_NAME[b], uv1=False)
    xs = stations(s, 6)
    rows = []
    for x in xs:
        sec = section(b, x)
        pick = [sec[0], sec[len(sec) // 2], sec[-1]]
        rows.append([(x, -y, z) for y, z in reversed(pick)] + [(x, y, z) for y, z in pick[1:]])
    m.colour = TINT
    m.quad_grid(rows, "SL_Hull")
    m.colour = WHITE
    # deck as one strip across each station pair (flat)
    for k in range(len(xs) - 1):
        a, c = rows[k], rows[k + 1]
        m.face([a[0], c[0], c[-1], a[-1]][::1] if True else None, "SL_HullDeck")
    m.face(rows[0], "SL_Hull")
    m.face(list(reversed(rows[-1])), "SL_Hull")
    mx = s["mast_x"]
    m.tube((mx, 0, deck_z(b, mx)), (mx, 0, s["mast_top_z"]), s["mast_d"] / 2, "SL_Alu", seg=4, smooth=False)
    gz = s["gooseneck_z"]
    m.tube((mx, 0, gz), (mx - s["boom_len"], 0, gz), s["boom_d"] / 2, "SL_Alu", seg=4, smooth=False)
    # sails: flat polygons on the centreline, both faces
    for which in (["main", "jib"] if b == "cadet" else ["main"]):
        pts = []
        for sv, hv in ((0, 0), (1, 0), (1, 0.5), (1, 1), (0, 1), (0, 0.5)):
            x, z = B.sail_point(b, sv, hv, which)
            pts.append(((mx + x) if which == "main" else x, 0.0, z))
        uvs = [B.sail_uv(b, B.sail_point(b, sv, hv, which), which) for sv, hv in ((0, 0), (1, 0), (1, 0.5), (1, 1), (0, 1), (0, 0.5))]
        m.face([(p[0], -0.002, p[2]) for p in pts], "SL_Sail" if which == "main" else "SL_White", uvs=uvs if which == "main" else None, col=(0, 1, 1, 1))
        m.face([(p[0], 0.002, p[2]) for p in reversed(pts)], "SL_Sail" if which == "main" else "SL_White", uvs=list(reversed(uvs)) if which == "main" else None, col=(0, 1, 0, 1))
    m.bx("SL_Foil" if b == "luch" else "SL_FoilWood", s["rudder"]["x"] - s["rudder"]["chord"], s["rudder"]["x"], -0.01, 0.01, s["rudder"]["z_bot"], 0.25)
    m.closed = False
    return m


def _variants(cname, part):
    """The sail's cloth variants: material instance per look (the mesh's default is "new"; the engine swaps the SL_Sail
    slot's material per boat). Pictures: sail_<class>[_old|_patched].png; alpha = light through the cloth."""
    return {v: "MI_SL_Sail_%s_%s%s" % (cname, part, suf) for v, suf in (("new", ""), ("old", "_Old"), ("patched", "_Patched"))}


def build(reg, b, kit="sailing_boats"):
    """All parts of one class (LOD0 + LOD1) and its far boat into the registry."""
    cname = CLASS_NAME[b]
    parts = [("Hull", hull), ("Mast", mast), ("Boom", boom), ("Rudder", rudder), ("Tiller", tiller), ("Ext", extension),
             ("Board", board)]
    for nm, fn in parts:
        reg.add(fn(b, 1.0), kit, closed=(nm == "Hull" and b == "luch"))
        reg.add(fn(b, 0.45), kit, closed=False, lod_of="SM_Sail_%s_%s" % (cname, nm))
    marks = B.SAIL_MARKS[b]
    x0, x1, z0, z1 = B.sail_box(b)
    nb = {}
    for side in ("num_stbd", "num_port"):
        sv, hv = marks[side]
        u, v = B.sail_uv(b, B.sail_point(b, sv, hv))
        hh = marks["num_h"] * (B.sail_point(b, 0, 1)[1] - B.sail_point(b, 0, 0)[1]) / (z1 - z0)
        ww = hh * 3 * 0.62 * (z1 - z0) / (x1 - x0)
        nb[side] = [round(u - ww / 2, 4), round(v - hh / 2, 4), round(u + ww / 2, 4), round(v + hh / 2, 4)]
    extra = {"sail_picture": "sail_%s.png" % b, "number_boxes_uv": nb,
             "sail_area_m2": round(B.sail_area(b), 2), "cloth_variants": _variants(cname, "Main")}
    reg.add(sail(b, "main", 1.0), kit, closed=True, extra=extra)
    reg.add(sail(b, "main", 0.5), kit, closed=True, lod_of="SM_Sail_%s_Main" % cname)
    if b == "cadet":
        reg.add(sail(b, "jib", 1.0), kit, closed=True, extra={"sail_picture": "sail_cadet_jib.png",
                                                               "sail_area_m2": round(B.sail_area(b, "jib"), 2),
                                                               "cloth_variants": _variants(cname, "Jib")})
        reg.add(sail(b, "jib", 0.5), kit, closed=True, lod_of="SM_Sail_Cadet_Jib")
    if b == "optimist":
        reg.add(sprit(b, 1.0), kit, closed=False)
        reg.add(sprit(b, 0.45), kit, closed=False, lod_of="SM_Sail_Optimist_Sprit")
    reg.add(far(b), kit, closed=False, extra={"note": "whole boat, far fleet"})
