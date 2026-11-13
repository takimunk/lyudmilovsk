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

import interior_kit as kit
from citykit import prism, triangulate, DOOR_W
from _surrounds import add_surrounds
import _homekit as hk

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
CEIL = 3.15                       # ceiling of the ground floor (hex windows end at 3.12)
ALT = "tank"                      # studio corner: "tank" (aquarium + workbench) | "wheel" (potter's wheel)
SEED = 1
VARIANTS = {
    "P-T1H3": {"NS": 4, "RAIL": "CT_Steel", "CAP": "CT_PlasterBlue", "DRUM2": "CT_PlasterYellow",
               "ALT": "tank", "SEED": 1},                                                             # 40 x 30, max 18 m
    "P-T3H1": {"RAIL": "CT_SteelWhite", "CAP": "CT_PlasterGreen", "DRUM2": "CT_PlasterPink",
               "ALT": "wheel", "SEED": 5},                                                            # 25 x 20, max 12 m
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
        if zc + HC <= CEIL:      # ground floor: the hexagon shines into the room (frame ring + opal pane)
            Ho, Hg = _hex(uc, zc, 1.12), _hex(uc, zc, 0.97)
            dd = INNER + 0.006
            for i in range(6):
                a, b = Ho[i], Ho[(i + 1) % 6]
                a2, b2 = Hg[i], Hg[(i + 1) % 6]
                B.detail.face("CT_FrameWhite", [W.w(b[0], b[1], dd), W.w(a[0], a[1], dd), W.w(a2[0], a2[1], dd),
                                                W.w(b2[0], b2[1], dd)])
            B.detail.face("CT_Opal", [W.w(p[0], p[1], dd + 0.002) for p in reversed(Hg)])
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
    _interior(B, outline_cut)
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


# ================================================================================================ interiors
def _clip(poly, p, n):
    """Part of the polygon on the side of the line (point p, normal n) where (q - p).n >= 0."""
    out = []
    for i in range(len(poly)):
        a, b = poly[i], poly[(i + 1) % len(poly)]
        da = (a[0] - p[0]) * n[0] + (a[1] - p[1]) * n[1]
        db = (b[0] - p[0]) * n[0] + (b[1] - p[1]) * n[1]
        if da >= 0:
            out.append(a)
        if (da >= 0) != (db >= 0):
            t = da / (da - db)
            out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    return out


def _interior(B, outline_cut):
    """Ground floor: the loggia door leads into a round lobby (mailboxes, boards, concierge, a waiting corner, a pram
    and a bike); a plaster partition along the waist of the two drums has a doorway into the painter's studio that
    fills the back drum (easels, a day bed behind a screen, a kitchenette, a record player, a corner for a hobby).
    The hexagonal windows shine into both rooms. Ceiling 3.15, everything above stays closed."""
    f = lambda fn, x, y, yaw=0.0, z=0.0, **kw: fn(B, x, y, z, yaw, **kw)
    ring = _offset_ring(outline_cut, -(INNER + 0.01))
    inside = hk.poly_test(ring)
    H = hk.Home(B, lambda x, y: inside(x, y, 0.02))
    # ---- chord frame: M middle of the waist chord, U along it, V toward the studio (back drum)
    dx, dy = C2[0] - C1[0], C2[1] - C1[1]
    d = math.hypot(dx, dy)
    a = (d * d + R1 * R1 - R2 * R2) / (2 * d)
    h = math.sqrt(R1 * R1 - a * a)
    ex, ey = dx / d, dy / d
    M = (C1[0] + a * ex, C1[1] + a * ey)
    U = (-ey, ex)                                    # chord direction (west -> east means -U .. +U; sign below)
    if U[0] < 0:
        U = (-U[0], -U[1])
    V = (-U[1], U[0])
    if (C2[0] - M[0]) * V[0] + (C2[1] - M[1]) * V[1] < 0:
        V = (-V[0], -V[1])

    def S(u, v):
        return (M[0] + u * U[0] + v * V[0], M[1] + u * U[1] + v * V[1])

    def Y(fu, fv):                                   # kit yaw of a piece facing the local direction (fu, fv)
        return hk.yaw_facing(fu * U[0] + fv * V[0], fu * U[1] + fv * V[1])

    def Yphi(phi):
        return Y(math.cos(math.radians(phi)), math.sin(math.radians(phi)))

    def Yin(phi):                                    # a wall piece at polar angle phi faces the centre
        return Yphi(phi + 180.0)

    CS = S(0.0, math.hypot(C2[0] - M[0], C2[1] - M[1]))     # studio centre (= C2)
    cu, cv = 0.0, math.hypot(C2[0] - M[0], C2[1] - M[1])
    SR = R2 - INNER - 0.015

    def wall_s(phi, back=0.0):                       # (x, y, yaw) on the studio wall at polar angle phi (about C2)
        p = S(cu + (SR - back) * math.cos(math.radians(phi)), cv + (SR - back) * math.sin(math.radians(phi)))
        return p[0], p[1], Yin(phi)

    def free_s(phi, rr, face_phi):                   # a free piece at radius rr facing the polar direction face_phi
        p = S(cu + rr * math.cos(math.radians(phi)), cv + rr * math.sin(math.radians(phi)))
        return p[0], p[1], Yphi(face_phi)

    LR = R1 - INNER - 0.015

    def wall_l(theta, back=0.0):                     # on the lobby wall at angle theta about C1
        x = C1[0] + (LR - back) * math.cos(math.radians(theta))
        y = C1[1] + (LR - back) * math.sin(math.radians(theta))
        return x, y, hk.yaw_facing(-math.cos(math.radians(theta)), -math.sin(math.radians(theta)))

    # ---- floors, ceiling, partition ------------------------------------------------------------------------------
    lobby_poly = _clip(ring, M, (-V[0], -V[1]))
    studio_poly = _clip(ring, M, V)
    _flat(B.detail, "CT_Stone", lobby_poly, 0.004, True)
    _flat(B.detail, "CT_Floor", studio_poly, 0.004, True)
    big = _offset_ring(outline_cut, -0.2)
    prism(B.m, "CT_Plaster", big, CEIL, CEIL + 0.15, top=True, bottom=False)
    _flat(B.m, "CT_Interior", big, CEIL, False)
    L = 2 * h
    TP = 0.16
    dw = 0.55                                         # half width of the doorway

    def rect(u0, u1):
        pts = [S(u0, -TP / 2), S(u1, -TP / 2), S(u1, TP / 2), S(u0, TP / 2)]
        return pts
    prism(B.m, "CT_Plaster", rect(-L / 2 + 0.1, -dw), 0.0, CEIL, top=False, bottom=False)
    prism(B.m, "CT_Plaster", rect(dw, L / 2 - 0.1), 0.0, CEIL, top=False, bottom=False)
    prism(B.m, "CT_Plaster", rect(-dw, dw), 2.15, CEIL, top=False, bottom=False)
    for u in (-dw - 0.04, dw + 0.04):                 # door jambs
        prism(B.detail, "CT_FrameWhite", [S(u - 0.04, -TP / 2 - 0.02), S(u + 0.04, -TP / 2 - 0.02),
                                         S(u + 0.04, TP / 2 + 0.02), S(u - 0.04, TP / 2 + 0.02)], 0.0, 2.15,
              top=False, bottom=False)
    # skirting along the partition on both sides is skipped on purpose: plain plaster

    # ================================================================ LOBBY
    # waiting corner (west): sofa under the window pocket, coffee table, armchair, lamp, rug
    f(kit.rug, -0.45, 5.55, 90.0, w=1.9, d=2.5, name="rug_3")
    f(kit.rug, 2.0, 4.3, 0.0, w=1.3, d=0.9, name="rug_6")
    sofa = f(kit.sofa, -2.1, 5.7, 90.0, seats=2, mat="CT_PlasterGreen")
    ct = f(kit.coffee_table, -0.85, 5.55, 90.0, w=0.9, d=0.5)
    f(kit.newspaper, -0.85, 5.45, 0.0, z=0.42, cellname="news_1")
    f(kit.cup, -0.8, 5.7, 0.0, z=0.42)
    arm = f(kit.armchair, 0.0, 4.8, 180.0, mat="CT_PlasterRed")
    f(kit.floor_lamp, -2.15, 4.35, 0.0)
    f(kit.plant, -1.3, 3.95, 0.0, kind="monstera", pot="CT_PlasterBlue", h=1.0, seed=3)
    rt = f(kit.table, -2.25, 7.0, 90.0, w=0.5, d=0.4, h=0.7)
    f(kit.radio, -2.25, 7.0, 90.0, z=0.7)
    # concierge's corner by the door (east)
    dk = f(kit.office_desk, 4.5, 4.55, 0.0, w=1.3, d=0.65, typewriter_on=False, papers=True, phone=True, seed=2)
    cc = f(kit.chair, 4.5, 5.25, 0.0, kind="wood", mat="CT_PlasterYellow")
    f(kit.table_lamp, 4.0, 4.55, 0.0, z=0.75, shade="CT_PlasterGreen")
    f(kit.cup, 4.95, 4.45, 0.0, z=0.75)
    f(kit.hat_stand, 6.05, 4.1, 0.0)
    # east wall: mailboxes, the house boards, the clock, a bench
    x, y, yw = wall_l(-12)
    f(hk.mailboxes, x, y, yw, cols=5, rows=3)
    x, y, yw = wall_l(14)
    f(kit.notice_board, x, y, yw, w=1.0, zc=1.55, seed=21)
    f(kit.wall_clock, x, y, yw, zc=2.45, face="clock_1")
    x, y, yw = wall_l(34)
    bn = f(kit.bench, x - 0.35 * math.cos(math.radians(34)), y - 0.35 * math.sin(math.radians(34)), yw, w=1.2, kind="park")
    f(kit.picture, x, y, yw, name="painting_sea_night", w=0.8, zc=1.7)
    x, y, yw = wall_l(54)
    f(kit.notice_board, x, y, yw, w=0.9, zc=1.55, seed=22)
    f(kit.sign_plate, x, y, yw, name="sign_quiet", w=0.5, zc=2.3)
    x, y, yw = wall_l(72)
    sh = f(kit.bookshelf, x, y, yw, w=0.9, h=1.7, levels=4, topic="local", seed=9)
    f(kit.fire_extinguisher, *wall_l(-26)[:2], wall_l(-26)[2])
    # pram and bike by the door, plants
    pr = hk.pram(B, 6.0, 5.6, 100.0, mat="CT_PlasterRed")
    hk.bike(B, 6.45, 6.7, 0.0)
    f(kit.plant, 5.25, 3.65, 0.0, kind="ficus", pot="CT_PlasterYellow", h=1.3, seed=4)
    f(kit.plant, 1.2, 10.3, 0.0, kind="fern", pot="CT_PlasterRed", h=0.9, seed=6)
    # west pocket wall: pictures and a poster
    x, y, yw = wall_l(190)
    f(kit.picture, x, y, yw, name="painting_lighthouse", w=0.7, zc=1.75)
    x, y, yw = wall_l(205)
    f(kit.poster, x, y, yw, name="poster_cons_2", w=0.6, zc=1.6)
    x, y, yw = wall_l(168)
    f(kit.radiator, x, y, yw, w=1.0)
    # lights: two pendants (the building's only lobby lamps)
    kit.pendant(B, 1.6, 6.0, CEIL, drop=0.5, light=True, lumens=600, colour="#FFE2B8")
    kit.pendant(B, 4.0, 8.5, CEIL, drop=0.5, light=True, lumens=500, colour="#FFE2B8")

    B.spot("lobby_bench", "bench", bn["seats"][0][:2] + (0.0,), bn["seats"][0][3])
    B.spot("lobby_desk", "seat", cc["seats"][0][:2] + (0.0,), cc["seats"][0][3])
    H.seat("lobby_bench_seat", bn)
    H.seat("lobby_sofa_seat", sofa, 1)
    H.seat("lobby_armchair", arm)
    H.seat("lobby_desk_chair", cc)
    B.thing("lobby_board_lost", "board", wall_l(14)[:2] + (1.55,), wall_l(14)[2], topic="lost")
    B.thing("lobby_board_club", "board", wall_l(54)[:2] + (1.55,), wall_l(54)[2], topic="club")
    B.thing("lobby_picture_night_sea", "board", wall_l(34)[:2] + (1.7,), wall_l(34)[2], topic="painting")
    B.thing("lobby_picture_lighthouse", "board", wall_l(190)[:2] + (1.75,), wall_l(190)[2], topic="painting")
    B.thing("lobby_swap_shelf", "shelf", wall_l(72)[:2] + (1.3,), wall_l(72)[2], topic="local")
    B.thing("lobby_radio", "machine", (-2.25, 7.0, 0.95), 90.0, what="jukebox")
    B.thing("lobby_paper", "take", (-0.85, 5.6, 0.45), 0.0, item="daily_paper")

    # ================================================================ STUDIO
    # kitchenette on the far wall
    x, y, yw = wall_s(90, back=0.2)
    ku = f(kit.kitchen_unit, x, y, yw, modules="cupboard,sink,stove,fridge")
    f(kit.samovar, *S(cu - 0.9, cv + SR - 0.55)[:2], 0.0, z=0.9)
    tb = free_s(55, 2.3, 235)
    kt = f(kit.table, tb[0], tb[1], tb[2], w=0.9, d=0.7)
    ch1 = f(kit.chair, *free_s(55, 2.3 + 0.7, 235 + 180)[:2], free_s(55, 2.3 + 0.7, 235 + 180)[2], kind="wood", mat="CT_PlasterBlue")
    f(kit.teapot, tb[0], tb[1], 0.0, z=0.75)
    f(kit.cup, tb[0] + 0.2, tb[1] + 0.1, 0.0, z=0.75)
    # easels in the light of the hexagons, a posing chair and a still life
    e1 = free_s(150, 2.1, -60)
    ea = f(kit.easel, e1[0], e1[1], Yphi(-60), canvas="painting_dunes")
    e2 = free_s(205, 2.4, -20)
    eb = f(kit.easel, e2[0], e2[1], Yphi(-20), canvas="canvas_blank")
    pj = free_s(190, 3.0, 0)
    f(kit.paint_jars, pj[0], pj[1], 0.0, z=0.0, n=5, seed=3)
    st = free_s(130, 1.5, 0)
    stool = f(kit.stool, st[0], st[1], 0.0, h=0.5)
    # drafting table and the art shelf
    dtp = free_s(235, 3.9, 235 + 180)
    dt = f(kit.drafting_table, dtp[0], dtp[1], Yphi(235 + 180))
    x, y, yw = wall_s(250)
    sh2 = f(kit.bookshelf, x, y, yw, w=1.0, h=1.9, levels=5, topic="art", seed=SEED + 3)
    x, y, yw = wall_s(214)
    ex = f(kit.notice_board, x, y, yw, w=1.0, zc=1.6, seed=SEED + 30)
    f(kit.picture, *wall_s(228)[:2], wall_s(228)[2], name="painting_portrait", w=0.7, zc=1.8)
    x, y, yw = wall_s(180)
    f(kit.picture, x, y, yw, name="painting_pines", w=0.9, zc=1.7)
    f(kit.picture, *wall_s(168)[:2], wall_s(168)[2], name="painting_abstract", w=0.7, zc=1.8)
    # day bed with a screen (east), wardrobe, record corner
    bx, by, byw = wall_s(18, back=1.15)
    bed = f(kit.bed, bx, by, byw, size="single", blanket="CT_PlasterYellow")
    sd = free_s(2, 3.0, 180)
    f(kit.screen_divider, sd[0], sd[1], Yphi(180 - 90))
    bt = free_s(34, 3.8, 214)
    f(kit.bedside_table, bt[0], bt[1], Yphi(214), lamp=True)
    x, y, yw = wall_s(48)
    f(kit.wardrobe, x, y, yw, w=1.0)
    x, y, yw = wall_s(112)
    rs = f(kit.table, *free_s(116, SR - 0.3, 296)[:2], Yphi(296), w=0.9, d=0.4, h=0.6)
    rp = f(kit.record_player, rs["pos"][0], rs["pos"][1], Yphi(296), z=0.6)
    arm2 = f(kit.armchair, *free_s(116, SR - 1.3, 296 + 180)[:2], Yphi(296 + 180), mat="CT_PlasterRed")
    f(kit.rug, *S(cu, cv)[:2], 0.0, w=2.6, d=2.6, name="rug_round_0", round=True)
    f(kit.floor_lamp, *free_s(130, SR - 0.6, 0)[:2], 0.0, shade="CT_PlasterYellow")
    f(kit.plant, *free_s(262, SR - 0.9, 0)[:2], 0.0, kind="ficus", pot="CT_PlasterRed", h=1.1, seed=8)
    f(kit.wall_clock, *wall_s(100)[:2], wall_s(100)[2], zc=2.55, face="clock_3")
    # hobby corner by the door (west side): variant
    if ALT == "wheel":
        w0 = free_s(-10, 3.6, 90)
        wh = f(kit.potters_wheel, w0[0], w0[1], Yphi(90))
    else:
        w0 = free_s(-10, 3.9, 90)
        wh = f(kit.aquarium, w0[0], w0[1], Yphi(90), w=1.0)
    kit.pendant(B, *S(cu, cv)[:2], CEIL, drop=0.6, light=True, lumens=700, colour="#FFE2B8")
    kit.pendant(B, *free_s(150, 2.6, 0)[:2], CEIL, drop=0.5, light=True, lumens=500, colour="#FFE2B8")
    kit.pendant(B, *free_s(35, 2.6, 0)[:2], CEIL, drop=0.5, light=True, lumens=400, colour="#FFE2B8")
    # ---- studio things
    H.seat("studio_armchair", arm2)
    H.seat("studio_kitchen_chair", ch1)
    H.seat("studio_stool", stool)
    B.thing("studio_daybed", "bed", bed["pos"][:2] + (0.5,), bed["yaw"], height=0.5, rest="private")
    B.thing("studio_easel_dunes", "craft", ea["pos"][:2] + (1.1,), ea["yaw"], what="easel")
    B.thing("studio_easel_blank", "craft", eb["pos"][:2] + (1.1,), eb["yaw"], what="easel")
    B.thing("studio_drafting_table", "craft", dt["pos"][:2] + (0.95,), dt["yaw"], what="easel")
    B.thing("studio_art_shelf", "shelf", sh2["pos"][:2] + (1.3,), sh2["yaw"], topic="art")
    B.thing("studio_board_exhibit", "board", ex["pos"][:2] + (1.6,), ex["yaw"], topic="exhibit")
    B.thing("studio_picture_portrait", "board", wall_s(228)[:2] + (1.8,), wall_s(228)[2], topic="painting")
    B.thing("studio_picture_pines", "board", wall_s(180)[:2] + (1.7,), wall_s(180)[2], topic="painting")
    B.thing("studio_picture_abstract", "board", wall_s(168)[:2] + (1.8,), wall_s(168)[2], topic="painting")
    B.thing("studio_record_player", "machine", rp["pos"][:2] + (0.65,), rp["yaw"], what="record_player")
    if ALT == "wheel":
        B.thing("studio_potters_wheel", "craft", wh["pos"][:2] + (0.6,), wh["yaw"], what="potters_wheel")
    else:
        B.thing("studio_aquarium", "screen", wh["pos"][:2] + (1.0,), wh["yaw"], what="aquarium")
    B.spot("studio_easel", "work", ea["front"][:2] + (0.0,), (ea["yaw"] + 0.0) % 360)
    H.check("twin")
