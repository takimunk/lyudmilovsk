"""Twin-cylinder house: two interlocking white plaster drums (the 1920s Moscow avant-garde idea of a house made of
cylinders), 3 storeys, flat roof terrace. Original design in the spirit of Konstantin Melnikov's cylinder-house
experiments and the rationalist "social condenser" houses - not a copy of any building.

Plot 25 x 20 m, max 12 m: footprint ~15 x 17.5 m, height 11.95 m (roof terrace at 9.9 m + stair drum + pergola).

Massing (frame: x along the street, y inland):
  front drum  R 5.3 m  centre (2.0, 6.0)   - cut by a vertical loggia notch (4.8 m wide, 2.2 m deep behind the drum
                                             line) on the street side: 3-storey glazed front (concrete piers),
                                             street door at the bottom, curved balconies with red railings at
                                             floors 1 and 2, the roof slab bridges the notch like a canopy.
  back drum   R 5.5 m  centre (-2.2, 12.6) - the dense "honeycomb": elongated hexagonal windows in a staggered
                                             pattern all round, 6 rows (2 per storey). Hex openings are really cut
                                             into the wall with reveals; window cards sit behind them.
  Drums are 40-gon prisms; the walls exist only where a drum is outside the other one (true interlock, one
  shared floor). Roof: bitumen terrace with railing round the edge, timber deck + pergola over the front drum,
  a round stair head with a red cap on the back drum.
Flats (4): 0-01 ground flat in the back drum (3 beds); 0-02 small ground flat in the front drum (1 bed);
  1-01 maisonette (floors 1-2) in the front drum behind the big glazing (3 beds); 1-02 maisonette (floors 1-2) in the
  back drum (3 beds). One street entrance "main" in the loggia leading to a small lobby (stair window right of the door).
Not enterable (people walk to the door and vanish).
Ideas for later: interior lobby, hex shutters with night glow patterns, planters on the balconies, a shared roof
  garden with a sundial; CT_Plaster variant with a faint pink for the back drum.
Kit notes: windows are placed with B.window() and explicit CCW corners (hex holes are cut by this module itself).
"""
import math

from citykit import prism, triangulate, DOOR_W
from _surrounds import add_surrounds

INFO = {
    "name_ru": "Дом из двух цилиндров", "name_en": "Twin-Cylinder House",
    "style": "constructivism", "use": "residential",
    "inspired_by": "Konstantin Melnikov's 1920s cylinder-house experiments in Moscow (hexagonal window honeycomb), "
                   "rationalist avant-garde of the 1920s",
}

C1, R1 = (2.0, 6.0), 5.3          # front drum
C2, R2 = (-2.2, 12.6), 5.5        # back drum
N = 40
DA = 2 * math.pi / N
ST = 3.3                          # storey height
ZW = 9.55                         # top of the walls; roof slab 9.55..9.9
ZR = 9.9
NW = 2.4                          # notch half width
YC = 2.9                          # notch back wall (chord wall) y
HA, HB, HC = 0.38, 0.13, 0.70     # hexagon: half width at the points, half width of top/bottom, half height
DEPTH = 0.22
INNER = 0.35
# ---- per-plot variant parameters
NS = 3                            # storeys (3 -> 11.95 m; 4 -> 15.25 m: the maisonettes get one more floor)
RAIL = "CT_SteelRed"              # balcony railings and door canopy
CAP = "CT_PlasterRed"             # stair-head cap
DRUM2 = "CT_Plaster"              # plaster of the back (honeycomb) drum
VARIANTS = {
    "P-T1H3": {"NS": 4, "RAIL": "CT_Steel", "CAP": "CT_PlasterBlue", "DRUM2": "CT_PlasterYellow"},    # 40 x 30, max 18 m
    "P-T3H1": {"RAIL": "CT_SteelWhite", "CAP": "CT_PlasterGreen", "DRUM2": "CT_PlasterPink"},          # 25 x 20, max 12 m
}


def _pt(c, R, a):
    return (c[0] + R * math.cos(a), c[1] + R * math.sin(a))


def _ring_edges(notch):
    """Outline of the union of the two drums (CCW) as edges {p0, p1, tag, k, full}."""
    dx, dy = C2[0] - C1[0], C2[1] - C1[1]
    d = math.hypot(dx, dy)
    phi = math.atan2(dy, dx)
    al = math.acos((d * d + R1 * R1 - R2 * R2) / (2 * d * R1))
    be = math.acos((d * d + R2 * R2 - R1 * R1) / (2 * d * R2))
    edges = []

    def arc(c, R, a0, a1, tag, cut=None):
        bps = {a0, a1}
        if cut:
            bps.update(cut)
        k = math.ceil(a0 / DA + 1e-9)
        while k * DA < a1 - 1e-9:
            bps.add(k * DA)
            k += 1
        bl = sorted(bps)
        i = 0
        # drop breakpoints closer than 1.5 deg to a neighbour (not the mandatory ones)
        keep = [bl[0]]
        for a in bl[1:-1]:
            if a - keep[-1] > math.radians(1.5) and bl[-1] - a > math.radians(1.5):
                keep.append(a)
            elif cut and any(abs(a - cc) < 1e-9 for cc in cut):
                keep.append(a)
        keep.append(bl[-1])
        for a, b in zip(keep, keep[1:]):
            mid = (a + b) / 2
            if cut and cut[0] < mid < cut[1]:
                if abs(a - cut[0]) < 1e-9:
                    pa = _pt(c, R, cut[0])
                    pb = _pt(c, R, cut[1])
                    edges.append({"p0": pa, "p1": (pa[0], YC), "tag": "return", "k": 0, "full": False})
                    edges.append({"p0": (pa[0], YC), "p1": (pb[0], YC), "tag": "chord", "k": 0, "full": False})
                    edges.append({"p0": (pb[0], YC), "p1": pb, "tag": "return", "k": 0, "full": False})
                continue
            full = abs(a / DA - round(a / DA)) < 1e-6 and abs(b / DA - round(b / DA)) < 1e-6
            edges.append({"p0": _pt(c, R, a), "p1": _pt(c, R, b), "tag": tag, "k": int(math.floor(mid / DA)) % N,
                          "full": full})

    g = math.asin(NW / R1)
    cut = (1.5 * math.pi - g, 1.5 * math.pi + g) if notch else None
    arc(C1, R1, phi + al, phi - al + 2 * math.pi, "c1", cut)
    phi2 = phi + math.pi
    arc(C2, R2, phi2 + be, phi2 - be + 2 * math.pi, "c2")
    return edges


def _offset_ring(pts, d):
    """CCW ring moved outward by d (mitred)."""
    n = len(pts)
    out = []
    for i in range(n):
        a, b, c = pts[i - 1], pts[i], pts[(i + 1) % n]
        e1 = (b[0] - a[0], b[1] - a[1])
        e2 = (c[0] - b[0], c[1] - b[1])
        l1, l2 = math.hypot(*e1) or 1, math.hypot(*e2) or 1
        n1 = (e1[1] / l1, -e1[0] / l1)
        n2 = (e2[1] / l2, -e2[0] / l2)
        s = (n1[0] + n2[0], n1[1] + n2[1])
        ls = math.hypot(*s) or 1
        s = (s[0] / ls, s[1] / ls)
        cs = max(0.35, s[0] * n1[0] + s[1] * n1[1])
        out.append((b[0] + s[0] * d / cs, b[1] + s[1] * d / cs))
    return out


def _area(p):
    return sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p))) / 2


def _flat(m, mat, poly, z, up=True):
    if _area(poly) < 0:
        poly = poly[::-1]
    for a, b, c in triangulate(poly):
        tri = [poly[a], poly[b], poly[c]]
        if not up:
            tri = tri[::-1]
        m.face(mat, [(x, y, z) for x, y in tri])


class _Wall:
    """A wall line p0 -> p1 (outside on the right): (u, z, depth inward) -> world."""

    def __init__(self, p0, p1):
        self.p0 = p0
        self.L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
        self.d = ((p1[0] - p0[0]) / self.L, (p1[1] - p0[1]) / self.L)
        self.n = (self.d[1], -self.d[0])

    def w(self, u, z, dep=0.0):
        return (self.p0[0] + self.d[0] * u - self.n[0] * dep, self.p0[1] + self.d[1] * u - self.n[1] * dep, z)


def _poly_uz(m, mat, W, poly):
    for a, b, c in triangulate(poly):
        m.face(mat, [W.w(*poly[a]), W.w(*poly[b]), W.w(*poly[c])])


def _hex(uc, zc, k=1.0):
    a, b, c = HA * k, HB * k, HC * k
    return [(uc - a, zc), (uc - b, zc - c), (uc + b, zc - c), (uc + a, zc), (uc + b, zc + c), (uc - b, zc + c)]


def _panel(B, p0, p1, z0, z1, centres, mat="CT_Plaster", flat_of=None, kind="flat"):
    """A cylinder facet with hexagonal holes at the heights `centres` (centred in the facet) and cards behind."""
    W = _Wall(p0, p1)
    L = W.L
    m = B.m
    uc = L / 2
    zs = sorted(centres)
    zcur = z0
    for zc in zs:
        zb, zt = zc - HC, zc + HC
        if zb > zcur + 1e-6:
            _poly_uz(m, mat, W, [(0, zcur), (L, zcur), (L, zb), (0, zb)])
        _poly_uz(m, mat, W, [(0, zb), (uc - HB, zb), (uc - HA, zc), (uc - HB, zt), (0, zt)])
        _poly_uz(m, mat, W, [(uc + HB, zb), (L, zb), (L, zt), (uc + HB, zt), (uc + HA, zc)])
        zcur = zt
        H = _hex(uc, zc)
        for i in range(6):
            a, b = H[i], H[(i + 1) % 6]
            m.face("CT_Plaster", [W.w(*a), W.w(*b), W.w(b[0], b[1], DEPTH), W.w(a[0], a[1], DEPTH)])
        # white frame ring in the hole, card behind
        Hi = _hex(uc, zc, 0.88)
        for i in range(6):
            a, b = H[i], H[(i + 1) % 6]
            a2, b2 = Hi[i], Hi[(i + 1) % 6]
            B.detail.face("CT_FrameWhite", [W.w(a[0], a[1], 0.17), W.w(b[0], b[1], 0.17),
                                            W.w(b2[0], b2[1], 0.17), W.w(a2[0], a2[1], 0.17)])
        card = [W.w(uc - HA, zc - HC), W.w(uc + HA, zc - HC), W.w(uc + HA, zc + HC), W.w(uc - HA, zc + HC)]
        fid = flat_of(zc) if flat_of else None
        B.window(card, flat=fid, kind=kind, inset=0.20, frame=0)
    if z1 > zcur + 1e-6:
        _poly_uz(m, mat, W, [(0, zcur), (L, zcur), (L, z1), (0, z1)])
    # inner sheet (the shell is closed; nobody sees it)
    q = [W.w(L, z0, INNER), W.w(0, z0, INNER), W.w(0, z1, INNER), W.w(L, z1, INNER)]
    m.face("CT_Interior", q)


def build(B):
    global ZW, ZR
    ZW = NS * ST - 0.35
    ZR = ZW + 0.35
    full = _ring_edges(False)
    cut = _ring_edges(True)
    outline_full = [e["p0"] for e in full]
    outline_cut = [e["p0"] for e in cut]

    # ---- plinth (also the loggia floor) and the roof slab
    plinth = _offset_ring(outline_full, 0.15)
    prism(B.m, "CT_ConcreteDark", plinth, -0.6, 0.0, top=False, bottom=False)
    _flat(B.m, "CT_Paving", plinth, 0.0, True)
    roof = _offset_ring(outline_full, 0.10)
    prism(B.m, "CT_Plaster", roof, ZW, ZR, top=False, bottom=False)
    _flat(B.m, "CT_Plaster", roof, ZW, False)
    _flat(B.m, "CT_Roof", roof, ZR, True)

    def flat_c2(zc):
        return "0-01" if zc < ST else "1-02"

    def flat_c1(zc):
        return "0-02" if zc < ST else "1-01"

    rows = []
    for s in range(NS):
        rows += [s * ST + 0.92, s * ST + 2.42]
    for e in cut:
        p0, p1 = e["p0"], e["p1"]
        if e["tag"] in ("c1", "c2"):
            cen = []
            if e["full"]:
                for r, zc in enumerate(rows):
                    if e["tag"] == "c2" and (e["k"] + r) % 2 == 0:
                        cen.append(zc)
                    elif e["tag"] == "c1" and e["k"] % 2 == 0 and r % 2 == 0:
                        cen.append(zc)
            fo = flat_c2 if e["tag"] == "c2" else flat_c1
            _panel(B, p0, p1, 0.0, ZW, cen, mat=DRUM2 if e["tag"] == "c2" else "CT_Plaster", flat_of=fo)
        elif e["tag"] == "chord":
            _chord(B, p0, p1)
        else:   # returns: plain, thick
            B.facade(p0, p1, 0.0, ZW, "CT_Plaster", holes=[], depth=0.35, back=True)

    # ---- string courses (thin projecting bands) at the floors
    for e in cut:
        if e["tag"] not in ("c1", "c2"):
            continue
        W = _Wall(e["p0"], e["p1"])
        for zb in [ST * i for i in range(1, NS)]:
            z0, z1, o = zb - 0.07, zb + 0.07, 0.05
            B.detail.face("CT_PlasterGrey", [W.w(0, z0, -o), W.w(W.L, z0, -o), W.w(W.L, z1, -o), W.w(0, z1, -o)])
            B.detail.face("CT_PlasterGrey", [W.w(0, z1, -o), W.w(W.L, z1, -o), W.w(W.L, z1, 0), W.w(0, z1, 0)])
            B.detail.face("CT_PlasterGrey", [W.w(0, z0, 0), W.w(W.L, z0, 0), W.w(W.L, z0, -o), W.w(0, z0, -o)])

    # ---- balconies with curved fronts in the loggia
    cx = C1[0]
    ya = C1[1] - math.sqrt(R1 * R1 - NW * NW)
    g = math.asin(NW / R1)
    arcpts = [_pt(C1, R1, 1.5 * math.pi + g - 2 * g * i / 8) for i in range(9)]       # right to left
    bal = [(cx - NW, YC), (cx + NW, YC)] + arcpts
    for zt in [ST * i for i in range(1, NS)]:
        prism(B.m, "CT_Plaster", bal if _area(bal) > 0 else bal[::-1], zt - 0.25, zt, top=False, bottom=False)
        _flat(B.m, "CT_Concrete", bal, zt, True)
        _flat(B.m, "CT_Plaster", bal, zt - 0.25, False)
        inner = [(x + (cx - x) * 0.0, y) for x, y in arcpts]
        rl = [(x, y - 0.0) for x, y in arcpts[::-1]]
        rl = [(cx + (x - cx) * 0.97, C1[1] + (y - C1[1]) * 0.97) for x, y in rl]
        B.railing([(cx - NW + 0.1, YC - 0.1)] + rl[:1] + rl[1:-1] + [(cx + NW - 0.1, rl[-1][1])][:0], zt, mat=RAIL)
        B.railing(rl, zt, mat=RAIL)

    # ---- door canopy (red) and loggia ceiling light strip
    prism(B.m, RAIL, [(cx - 1.2, YC - 1.0), (cx + 1.2, YC - 1.0), (cx + 1.2, YC), (cx - 1.2, YC)], 2.75, 2.9,
          top=True, bottom=True)
    B.entrance("main", (cx, YC, 0.0), 0.0, width=1.4, height=2.5, kind="street", leaves=1)

    # ---- roof terrace: railing, timber deck, pergola, stair head
    inner_roof = _offset_ring(outline_full, -0.12)
    B.railing(inner_roof + [inner_roof[0]], ZR, h=1.05, mat="CT_Steel", post=1.3)
    deck_c = (C1[0], C1[1] - 0.6)
    deck = [(deck_c[0] + 3.3 * math.cos(2 * math.pi * k / 24), deck_c[1] + 3.3 * math.sin(2 * math.pi * k / 24))
            for k in range(24)]
    prism(B.m, "CT_Wood", deck, ZR, ZR + 0.08, top=True, bottom=False)
    for px in (-2.4, 2.4):
        for py in (-1.8, 1.8):
            B.detail.face  # noqa
            from citykit import box
            box(B.detail, "CT_Steel", deck_c[0] + px - 0.06, deck_c[0] + px + 0.06, deck_c[1] + py - 0.06,
                deck_c[1] + py + 0.06, ZR + 0.08, ZR + 2.0)
    from citykit import box
    for py in (-1.8, 1.8):
        box(B.detail, "CT_Steel", deck_c[0] - 2.7, deck_c[0] + 2.7, deck_c[1] + py - 0.08, deck_c[1] + py + 0.08,
            ZR + 2.0, ZR + 2.16)
    for i in range(9):
        x = deck_c[0] - 2.5 + i * 0.625
        box(B.detail, "CT_Wood", x - 0.04, x + 0.04, deck_c[1] - 2.0, deck_c[1] + 2.0, ZR + 2.16, ZR + 2.22)
    # stair head: a round drum with a red cap on the back drum
    sc = (C2[0] - 1.2, C2[1] + 1.5)
    sr = 2.1
    drum = [(sc[0] + sr * math.cos(2 * math.pi * k / 20), sc[1] + sr * math.sin(2 * math.pi * k / 20)) for k in range(20)]
    prism(B.m, "CT_Plaster", drum, ZR, ZR + 1.8, top=False)
    cap = [(sc[0] + (sr + 0.25) * math.cos(2 * math.pi * k / 20), sc[1] + (sr + 0.25) * math.sin(2 * math.pi * k / 20))
           for k in range(20)]
    prism(B.m, CAP, cap, ZR + 1.8, ZR + 2.05, top=True, bottom=True)
    for k in (-2, 0, 2):    # three stair windows facing the street side
        a = -math.pi / 2 + k * 0.42
        p0 = (sc[0] + sr * math.cos(a - 0.15), sc[1] + sr * math.sin(a - 0.15))
        p1 = (sc[0] + sr * math.cos(a + 0.15), sc[1] + sr * math.sin(a + 0.15))
        # drum facet normal ~ radial; CCW from outside = from p1 side: use the chord direction (CCW around the drum)
        B.window([(p0[0], p0[1], ZR + 0.5), (p1[0], p1[1], ZR + 0.5), (p1[0], p1[1], ZR + 1.5), (p0[0], p0[1], ZR + 1.5)],
                 kind="stair", inset=0.05, frame=0.04)
    B.footprint = _offset_ring(outline_full, 0.15)
    B.height = ZR + 2.05
    B.note("hex honeycomb on the back drum; loggia with curved balconies on the front drum")
    add_surrounds(B, seed=2)


def _chord(B, p0, p1):
    L = p1[0] - p0[0]
    cx = C1[0]
    u = lambda x: x - p0[0]
    holes = []
    wins = []          # (u0, u1, z0, z1, flat, kind, cols, rows)
    # ground: door, flat window on the left, lobby window on the right
    dw = 0.7
    holes.append((u(cx) - dw, u(cx) + dw, 0.0, 2.5))
    wins.append((0.35, u(cx) - dw - 0.3, 0.9, 2.5, "0-02", "flat", 1, 1))
    wins.append((u(cx) + dw + 0.3, L - 0.35, 0.9, 2.5, None, "stair", 1, 1))
    pier = 0.3
    bw = (L - 4 * pier) / 3
    for si, (z0, z1) in enumerate([(i * ST, (i + 1) * ST - (0.4 if i == 1 else 0.5)) for i in range(1, NS)]):
        for b in range(3):
            a = pier + b * (bw + pier)
            wins.append((a, a + bw, z0, z1, "1-01", "flat", 2, 1))
    for w in wins:
        holes.append((w[0], w[1], w[2], w[3]))
    B.facade(p0, p1, 0.0, ZW, "CT_Plaster", holes=holes, depth=0.35, back=True)
    for u0, u1, z0, z1, fid, kind, cols, rows in wins:
        B.window([(p0[0] + u0, p0[1], z0), (p0[0] + u1, p0[1], z0), (p0[0] + u1, p0[1], z1), (p0[0] + u0, p0[1], z1)],
                 flat=fid, kind=kind, cols=cols, rows=rows, inset=0.18, frame=0.05)
    # flats
    B.flat("0-01", 0, beds=3, door=(cx - 1.0, YC + 1.5, 0.0), entrance="main")
    B.flat("0-02", 0, beds=1, door=(cx + 1.0, YC + 1.5, 0.0), entrance="main")
    B.flat("1-01", 1, beds=3, door=(cx, YC + 2.5, ST), entrance="main")
    B.flat("1-02", 1, beds=3, door=(cx - 2.0, YC + 3.5, ST), entrance="main")
