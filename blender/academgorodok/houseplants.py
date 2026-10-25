"""Procedural house plants in pots for the Academgorodok of Laboratory No. 2 (Moscow, late 1940s-1950s): the
ground-floor library's window sills, stands and floor (docs/academgorodok/refs/dima/shop.jpg: windows full of
plants) and the flats. Pure Python (no bpy): make() returns a geom.Mesh; run this file for the self-test.

API
  KINDS                       {kind: {"cls": "sill"|"floor", "desc": str, ...}}   (17 kinds, see below)
  make(kind, seed=0, size=1.0, stand=None) -> geom.Mesh
        at the origin, the pot's (or stand's) base on z=0, centred on the pot axis; size scales it all.
        stand: None, "iron" (wrought-iron jardiniere ~0.72 m) or "wicker" (woven column ~0.55 m);
        trailing plants (tradescantia, chlorophytum, asparagus) hang lower when stood on one.
  info(kind, size=1.0, stand=None) -> (radius, height)   footprint radius (max horizontal reach from the
        axis, incl. leaves) and total height; an upper bound over seeds (max over seeds 0..7 + 3 %).
  pot_info(kind, size=1.0) -> (radius, height)   the pot alone (for sills: the pot must sit on the board).
  place(m, kind, x, y, z, yaw, seed=0, size=1.0, stand=None)   adds the transformed plant to the Mesh m.
  budget(kind) -> max triangles (sill 1500, floor 5000; +600 with a stand).

Materials: PLANT_MATERIALS {name: (hex sRGB, roughness, description)}, all "AC_P*". Leaves are real geometry
(curved strips with a midrib fold, drooping under gravity, double-sided by a reversed copy 1 mm behind); only
the asparagus fern and the weeping fig's crown use alpha cards (AC_PLeafCards on textures/academ/foliage_houseplants.png, a 2x2 atlas
made by tools/make_academ_houseplants.py; FOLIAGE_ATLAS / CARD_CELLS). SMOOTH_MATERIALS should be smooth-shaded
(merge doubles at < 0.5 mm first: a leaf's back copy sits 1 mm off so it does not weld to the front);
CARD_MATERIALS are alpha-masked. Back faces of leaves are separate faces, so leaf materials can stay one-sided.

Conventions as geom.py: faces counter-clockwise seen from the side they face; leaf UVs are 0..1 across the
blade (u across, v along) except the cards, which map into their atlas cell.
"""
import math
import random

from geom import Frame, Mesh, cross, norm
from furniture import lathe, tube, face_n, sphere

# --- materials ---------------------------------------------------------------------------------------
PLANT_MATERIALS = {
    # leaves
    "AC_PLeafGlossy":    ("#1E3A1C", 0.25, "dark glossy leaves: rubber plant, aspidistra, lemon, dieffenbachia margins"),
    "AC_PLeafGreen":     ("#35622A", 0.45, "mid green leaves: monstera, ficus benjamina, palm, geranium, begonia"),
    "AC_PLeafLight":     ("#5E8E34", 0.50, "fresh yellow-green leaves: new growth, spider plant, tradescantia, palm"),
    "AC_PLeafGreyGreen": ("#405A3E", 0.45, "dark grey-green: snake plant bands, dracaena"),
    "AC_PLeafBand":      ("#66805C", 0.45, "pale grey-green cross-bands of the snake plant"),
    "AC_PLeafPale":      ("#C4D196", 0.50, "cream-green variegation: spider plant stripe, dieffenbachia centre, dracaena stripe"),
    "AC_PLeafYellowEdge":("#C9B14A", 0.45, "yellow margins of the snake plant (laurentii)"),
    "AC_PLeafVelvet":    ("#2B4626", 0.85, "dark velvety leaves of the African violet, zonal geranium (matte)"),
    "AC_PLeafSilver":    ("#9DB3A0", 0.40, "silver-green stripes of tradescantia zebrina"),
    "AC_PLeafPurple":    ("#5B2850", 0.55, "purple leaf undersides: tradescantia zebrina, some violets, begonia"),
    "AC_PLeafBronze":    ("#4C3424", 0.40, "bronze-red begonia leaves"),
    "AC_PSucculent":     ("#5C7E54", 0.60, "grey-green succulent: aloe leaves"),
    "AC_PCactus":        ("#3C6436", 0.55, "cactus body"),
    "AC_PSpine":         ("#E9E3C8", 0.70, "cactus areoles and spines, aloe teeth"),
    "AC_PLeafCards":     ("#3C6A2C", 0.50, "alpha leaf cards: asparagus fern sprays, weeping-fig twigs (foliage_houseplants.png, masked)"),
    # stems, wood, soil
    "AC_PStem":          ("#5E7032", 0.60, "green stems and petioles, palm rachis"),
    "AC_PBark":          ("#4E4231", 0.80, "woody trunks and branches: ficus, lemon, dracaena canes, aloe stem"),
    "AC_PSheath":        ("#A44A38", 0.40, "red-brown leaf sheath of the rubber plant's growing tip"),
    "AC_PRoot":          ("#7A6448", 0.80, "aerial roots of the monstera"),
    "AC_PSoil":          ("#2C2219", 0.95, "dark potting soil"),
    # pots and stands
    "AC_PTerracotta":    ("#AE5A3A", 0.85, "unglazed terracotta flower pots and saucers"),
    "AC_PCeramicWhite":  ("#ECE9E0", 0.15, "white glazed ceramic cachepot"),
    "AC_PCeramicGreen":  ("#355E46", 0.15, "green glazed (majolica) cachepot"),
    "AC_PCeramicBrown":  ("#64391F", 0.15, "brown glazed ceramic pot"),
    "AC_PEnamel":        ("#E4DECA", 0.25, "cream enamelled tin pot (a re-used enamel pan / bucket)"),
    "AC_PEnamelRim":     ("#2C4876", 0.25, "dark blue enamel rim band of the tin pot"),
    "AC_PTubWood":       ("#6E4B2A", 0.70, "staved wooden plant tub (palm, lemon)"),
    "AC_PWicker":        ("#B48C56", 0.80, "woven wicker plant stand (rattan)"),
    "AC_PIron":          ("#1E1D1C", 0.55, "black wrought iron: plant stand, tub hoops"),
    # flowers and fruit
    "AC_PFlowerRed":     ("#C81E1A", 0.60, "scarlet geranium (pelargonium) florets"),
    "AC_PFlowerPink":    ("#E2789A", 0.60, "pink begonia flowers, cactus flower"),
    "AC_PFlowerViolet":  ("#5B2F92", 0.70, "African violet flowers"),
    "AC_PFlowerWhite":   ("#F2EEE2", 0.60, "white flowers: lemon blossom, spider-plant flowers"),
    "AC_PFlowerYellow":  ("#E6BE2A", 0.50, "yellow stamens of the violet and begonia"),
    "AC_PLemon":         ("#E2C232", 0.35, "ripe lemons"),
    "AC_PLemonGreen":    ("#7E9A3A", 0.40, "unripe green lemons"),
}

FOLIAGE_ATLAS = "textures/academ/foliage_houseplants.png"
# atlas cells (u0, v0, u1, v1) in UV space (v up; the spray's stem end is at v0)
CARD_CELLS = {"sprengeri": (0.0, 0.5, 0.5, 1.0), "setaceus": (0.5, 0.5, 1.0, 1.0),
              "benjamina_a": (0.0, 0.0, 0.5, 0.5), "benjamina_b": (0.5, 0.0, 1.0, 0.5)}
CARD_MATERIALS = {"AC_PLeafCards"}
SMOOTH_MATERIALS = {k for k in PLANT_MATERIALS if k.startswith("AC_PLeaf") and k != "AC_PLeafCards"} | {
    "AC_PSucculent", "AC_PCactus", "AC_PStem", "AC_PBark", "AC_PSheath", "AC_PRoot", "AC_PLemon", "AC_PLemonGreen",
    "AC_PCeramicWhite", "AC_PCeramicGreen", "AC_PCeramicBrown", "AC_PTerracotta", "AC_PEnamel"}
SMOOTH_ANGLE = 60.0

GL, GR, LT, GG = "AC_PLeafGlossy", "AC_PLeafGreen", "AC_PLeafLight", "AC_PLeafGreyGreen"
BAND, PALE, YEL, VEL = "AC_PLeafBand", "AC_PLeafPale", "AC_PLeafYellowEdge", "AC_PLeafVelvet"
SIL, PUR, BRZ, SUC, CAC, SPN = "AC_PLeafSilver", "AC_PLeafPurple", "AC_PLeafBronze", "AC_PSucculent", "AC_PCactus", "AC_PSpine"
CARDS, STEM, BARK, SHEATH, ROOT, SOIL = "AC_PLeafCards", "AC_PStem", "AC_PBark", "AC_PSheath", "AC_PRoot", "AC_PSoil"
TERRA, CWHITE, CGREEN, CBROWN = "AC_PTerracotta", "AC_PCeramicWhite", "AC_PCeramicGreen", "AC_PCeramicBrown"
ENAM, ENRIM, TUB, WICK, IRON = "AC_PEnamel", "AC_PEnamelRim", "AC_PTubWood", "AC_PWicker", "AC_PIron"
FRED, FPINK, FVIO, FWHITE, FYEL, LEMON, LEMONG = ("AC_PFlowerRed", "AC_PFlowerPink", "AC_PFlowerViolet", "AC_PFlowerWhite",
                                                  "AC_PFlowerYellow", "AC_PLemon", "AC_PLemonGreen")

GOLD = math.radians(137.508)
BACK_OFF = 0.001          # the back copy of a leaf lies this far behind the front
BUDGET = {"sill": 1500, "floor": 5000}
STAND_EXTRA = 600


# --- vector helpers -------------------------------------------------------------------------------------
def _ad(a, b, s=1.0):
    return (a[0] + b[0] * s, a[1] + b[1] * s, a[2] + b[2] * s)


def _sc(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def _lerp(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def _dist(a, b):
    return math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2)


def az_el(v):
    """Azimuth and elevation of a direction."""
    h = math.hypot(v[0], v[1])
    return math.atan2(v[1], v[0]), math.atan2(v[2], h)


def dir_of(az, el):
    return (math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))


# --- the blade: a midrib under gravity ------------------------------------------------------------------
class Blade:
    """A midrib from base: azimuth az, initial elevation el (radians above horizontal), arc length L.
    Gravity bends it down by droop radians from base to tip (as t**power); sway turns the azimuth along the
    length; roll tilts the blade about the midrib (twist adds roll towards the tip)."""

    def __init__(self, base, az, el, L, droop=0.0, power=1.6, n=10, roll=0.0, twist=0.0, sway=0.0):
        self.L, self.n = L, n
        self.C, self.D, self.S, self.N = [base], [], [], []
        for i in range(n + 1):
            t = i / n
            e = max(-1.5, min(1.5, el - droop * t ** power))
            a = az + sway * t
            d = dir_of(a, e)
            s0 = (math.sin(a), -math.cos(a), 0.0)
            n0 = cross(s0, d)
            r = roll + twist * t
            s = _ad(_sc(s0, math.cos(r)), n0, math.sin(r))
            self.D.append(d)
            self.S.append(s)
            self.N.append(cross(s, d))
        for i in range(1, n + 1):
            self.C.append(_ad(self.C[-1], _lerp(self.D[i - 1], self.D[i], 0.5), L / n))

    def at(self, t):
        f = max(0.0, min(1.0, t)) * self.n
        i = min(int(f), self.n - 1)
        w = f - i
        return (_lerp(self.C[i], self.C[i + 1], w), norm(_lerp(self.D[i], self.D[i + 1], w)),
                norm(_lerp(self.S[i], self.S[i + 1], w)), norm(_lerp(self.N[i], self.N[i + 1], w)))

    def pt(self, t):
        return self.at(t)[0]

    def tip(self):
        return self.C[-1]


# width profiles: x 0..1 from the blade's base to its tip -> half-width fraction
SHAPES = {
    "elliptic": lambda x: math.sin(math.pi * min(1.0, x)) ** 0.75,
    "ovate": lambda x: math.sin(math.pi * min(1.0, x) ** 0.75) ** 0.8,
    "lanceolate": lambda x: math.sin(math.pi * min(1.0, x) ** 0.6) ** 0.9,
    "obovate": lambda x: math.sin(math.pi * min(1.0, x) ** 1.4) ** 0.8,
    "linear": lambda x: (0.55 + 0.45 * min(1.0, x / 0.2)) * (1.0 - min(1.0, x) ** 3),
    "sword": lambda x: (0.5 + 0.5 * min(1.0, x / 0.25)) * (1.0 - min(1.0, x) ** 5) ** 0.7,
    "strap": lambda x: (0.4 + 0.6 * min(1.0, x / 0.15)) * (1.0 - min(1.0, x) ** 6) ** 0.6,
    "round": lambda x: 0.35 + 0.65 * math.sqrt(max(0.0, 1.0 - (1.6 * x - 0.6) ** 2)) if x < 0.95 else 0.35 * (1 - x) / 0.05 + 0.0,
    "cordate": lambda x: (0.55 + 0.45 * math.sin(math.pi * min(1.0, x / 0.7) * 0.5)) * (1.0 - max(0.0, (x - 0.35) / 0.65) ** 1.6) ** 0.8,
    "monstera": lambda x: (0.72 + 0.28 * math.sin(0.5 * math.pi * min(1.0, x / 0.4))) * (1.0 - max(0.0, (x - 0.45) / 0.55) ** 2.2) ** 0.6,
    "const": lambda x: 1.0,
}


def leaf(m, bl, wmax, shape="elliptic", t0=0.0, segs=4, cols=(-1.0, 0.0, 1.0), mats=GR, back=None, cup=0.0, fold=0.0,
         notch=0.0, asym=0.0, cells=None, uvrect=None, wave=0.0):
    """A leaf blade on Blade bl from t0 to 1 (the part before t0 is the petiole). wmax: maximum half-width (m).
    cols: lateral stations (-1 left edge .. 1 right edge); mats: one material, a list per strip, or a function
    (segment, strip) -> material. cup: edges up (>0) or down (<0), quadratic; fold: V fold along the midrib;
    notch: base lobes pulled back (cordate); asym: right half wider by (1 + asym), left narrower; wave: edge
    undulation. cells: explicit [(x, u) x4] quads instead of the grid. uvrect: atlas cell for card leaves.
    back: material of the back side (default the front's). Returns the triangle count."""
    fn = SHAPES[shape] if isinstance(shape, str) else shape
    Ll = bl.L * (1.0 - t0)

    def P(x, u):
        t = t0 + (1.0 - t0) * x
        C, D, S, N = bl.at(t)
        uu = u * (1.0 + asym) if u > 0 else u * (1.0 - asym)
        w = wmax * fn(x)
        up = cup * u * u * w + fold * abs(u) * w + wave * w * abs(u) * math.sin(x * 19.0 + u * 3.0)
        bk = notch * Ll * abs(u) * (1.0 - min(1.0, x)) ** 4
        return _ad(_ad(_ad(C, S, uu * w), N, up), D, -bk), N

    if cells is None:
        cells = []
        for k in range(segs):
            xa, xb = k / segs, (k + 1) / segs
            for j in range(len(cols) - 1):
                cells.append(((k, j), [(xa, cols[j]), (xa, cols[j + 1]), (xb, cols[j + 1]), (xb, cols[j])]))
    tris = 0
    for key, corners in cells:
        pts, nrm, uvs = [], [], []
        for x, u in corners:
            p, n = P(x, u)
            if pts and _dist(p, pts[-1]) < 1e-7:
                continue
            pts.append(p)
            nrm.append(n)
            if uvrect:
                u0, v0, u1, v1 = uvrect
                uvs.append((u0 + (u1 - u0) * (u + 1) / 2, v0 + (v1 - v0) * x))
            else:
                uvs.append(((u + 1) / 2, x))
        if len(pts) > 3 and _dist(pts[0], pts[-1]) < 1e-7:
            pts, nrm, uvs = pts[:-1], nrm[:-1], uvs[:-1]
        if len(pts) < 3:
            continue
        mat = mats(*key) if callable(mats) else (mats[key[1]] if isinstance(mats, (list, tuple)) else mats)
        m.face(mat, pts, uvs)
        bpts = [_ad(p, n, -BACK_OFF) for p, n in zip(pts, nrm)]
        m.face(back or mat, list(reversed(bpts)), list(reversed(uvs)))
        tris += 2 * (len(pts) - 2)
    return tris


def rod(m, mat, pts, r0, r1=None, seg=4):
    """A tapering rod through the points (stems, branches, cords)."""
    r1 = r0 if r1 is None else r1
    n = len(pts) - 1
    for i in range(n):
        a, b = pts[i], pts[i + 1]
        if _dist(a, b) < 1e-6:
            continue
        tube(m, mat, a, b, r0 + (r1 - r0) * i / n, r0 + (r1 - r0) * (i + 1) / n, seg)


def blade_rod(m, mat, bl, t1, r0, r1=None, steps=3, seg=4, t0=0.0):
    rod(m, mat, [bl.pt(t0 + (t1 - t0) * k / steps) for k in range(steps + 1)], r0, r1, seg)


def stem_leaf(m, base, az, el, droop, petiole, L, wmax, mat=GR, stem_mat=STEM, pr=0.003, psteps=2, pseg=3, **kw):
    """A petiole of length `petiole` (a thin rod) running into a blade of length L, both on one bent midrib."""
    tot = petiole + L
    bl = Blade(base, az, el, tot, droop, kw.pop("power", 1.6), kw.pop("n", 10), kw.pop("roll", 0.0),
               kw.pop("twist", 0.0), kw.pop("sway", 0.0))
    t0 = petiole / tot
    if petiole > 1e-4:
        blade_rod(m, stem_mat, bl, t0 + 0.02, pr, pr * 0.8, psteps, pseg)
    leaf(m, bl, wmax, t0=t0, mats=mat, **kw)
    return bl


def floret(m, mat, c, nrm, r, k=5, rot=0.0, cup=0.25):
    """A flat k-sided flower (pentagon) facing nrm, slightly cupped, double-sided."""
    nrm = norm(nrm)
    a = (1.0, 0.0, 0.0) if abs(nrm[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = norm(cross(nrm, a))
    v = cross(nrm, u)
    pts = []
    for i in range(k):
        t = rot + 2 * math.pi * i / k
        pts.append(_ad(_ad(_ad(c, u, r * math.cos(t)), v, r * math.sin(t)), nrm, r * cup))
    face_n(m, mat, pts, nrm, [(0.5 + 0.5 * math.cos(rot + 2 * math.pi * i / k), 0.5 + 0.5 * math.sin(rot + 2 * math.pi * i / k)) for i in range(k)])
    face_n(m, mat, [_ad(p, nrm, -BACK_OFF) for p in pts], _sc(nrm, -1))


def ellipsoid(m, mat, c, r, h, seg=8, rings=5, axis=None):
    """A lemon-like ellipsoid with a pointed nose along axis (default up)."""
    part = Mesh()
    prof = []
    for k in range(rings + 1):
        a = math.pi * k / rings
        rr = r * math.sin(a) * (1.0 - 0.15 * (k / rings) ** 4)
        prof.append((rr, -h * math.cos(a)))
    prof[-1] = (0.0, h * 1.12)
    prof[0] = (0.0, -h * 1.08)
    lathe(part, mat, prof, seg)
    if axis is None:
        m.extend(part.transformed(lambda p: _ad(c, p)))
        return
    ax = norm(axis)
    t = (1.0, 0.0, 0.0) if abs(ax[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = norm(cross(t, ax))
    v = cross(ax, u)
    m.extend(part.transformed(lambda p: _ad(_ad(_ad(c, u, p[0]), v, p[1]), ax, p[2])))


# --- pots -----------------------------------------------------------------------------------------------
POT_STYLES = ("terracotta", "glazed_white", "glazed_green", "glazed_brown", "enamel", "tub")


def pot(m, style, r, h, seg=12):
    """A pot of top outer radius r and height h standing on z=0 (with its saucer for terracotta).
    Returns (soil level, inner radius at the soil)."""
    if style == "terracotta":
        z0 = 0.009
        lathe(m, TERRA, [(0, 0), (0.80 * r, 0), (0.86 * r, 0.004), (0.94 * r, 0.012), (0.90 * r, z0), (0, z0)], seg)
        hs = h * 0.86
        prof = [(0, z0), (0.70 * r, z0), (0.72 * r, z0 + 0.008), (0.90 * r, z0 + h * 0.80), (r, z0 + h * 0.81),
                (r, z0 + h), (r - 0.011, z0 + h), (0.86 * r, z0 + hs), (0, z0 + hs)]
        lathe(m, TERRA, prof, seg, mats=[TERRA] * 7 + [SOIL])
        return z0 + hs, 0.86 * r
    if style.startswith("glazed"):
        mat = {"glazed_white": CWHITE, "glazed_green": CGREEN, "glazed_brown": CBROWN}[style]
        hs = h * 0.87
        prof = [(0, 0), (0.58 * r, 0), (0.60 * r, 0.02 * h), (0.55 * r, 0.05 * h), (0.86 * r, 0.30 * h), (r, 0.58 * h),
                (0.94 * r, 0.86 * h), (0.90 * r, 0.95 * h), (0.95 * r, h), (0.88 * r, h), (0.84 * r, hs), (0, hs)]
        lathe(m, mat, prof, seg, mats=[mat] * 10 + [SOIL])
        return hs, 0.84 * r
    if style == "enamel":
        hs = h * 0.88
        prof = [(0, 0), (0.95 * r, 0), (0.97 * r, 0.01), (0.97 * r, 0.86 * h), (0.97 * r, 0.9 * h), (r, h), (0.95 * r, h),
                (0.94 * r, hs), (0, hs)]
        lathe(m, ENAM, prof, seg, mats=[ENAM, ENAM, ENAM, ENRIM, ENRIM, ENRIM, ENAM, SOIL])
        return hs, 0.94 * r
    if style == "tub":                       # staved wooden tub with two iron hoops, on four little feet
        s8 = 8
        hs = h * 0.9
        f = 0.03
        lathe(m, TUB, [(0, f), (0.80 * r, f), (r, f + h), (0.92 * r, f + h), (0.9 * r, f + hs), (0, f + hs)], s8,
              mats=[TUB, TUB, TUB, TUB, SOIL], phase=math.pi / s8)
        for zz in (0.18, 0.8):
            rr = 0.80 * r + 0.2 * r * zz
            lathe(m, IRON, [(rr + 0.004, f + h * zz - 0.012), (rr + 0.006, f + h * zz + 0.012)], s8, phase=math.pi / s8)
            lathe(m, IRON, [(rr + 0.006, f + h * zz + 0.012), (rr, f + h * zz + 0.012)], s8, phase=math.pi / s8)
        for k in range(4):
            a = math.pi / 4 + k * math.pi / 2
            x, y = 0.7 * r * math.cos(a), 0.7 * r * math.sin(a)
            tube(m, TUB, (x, y, 0.0), (x, y, f + 0.005), 0.025, 0.025, 4, cap0=TUB)
        return f + hs, 0.9 * r
    raise ValueError(style)


def _stand(m, kind, rnd):
    """Plant stand under the pot; returns its top height."""
    if kind == "iron":                # three bowed legs with scrolled feet, a ring between them, a round top
        H = 0.72
        lathe(m, IRON, [(0, H - 0.012), (0.16, H - 0.012), (0.16, H), (0, H)], 12)
        legr = lambda z: 0.15 + 0.05 * (1 - z / H) + 0.02 * math.sin(math.pi * z / H)
        for k in range(3):
            a = 2 * math.pi * k / 3
            ca, sa = math.cos(a), math.sin(a)
            pts = [(legr(z) * ca, legr(z) * sa, z) for z in [H - 0.01 - (H - 0.06) * i / 6 for i in range(7)]]
            r0 = legr(0.05)
            for q in range(5):                                       # the scroll foot
                ang = math.pi * q / 4
                rr = r0 + 0.03 * (1 - math.cos(ang))
                pts.append((rr * ca, rr * sa, 0.035 + 0.025 * math.sin(ang) * (1 if q < 3 else 0.6) + 0.0))
            pts = [(p[0], p[1], max(0.009, p[2])) for p in pts]
            rod(m, IRON, pts, 0.008, 0.007, 4)
        zr = 0.3
        r = legr(zr)
        lathe(m, IRON, [(r - 0.004, zr), (r + 0.004, zr), (r + 0.004, zr + 0.01), (r - 0.004, zr + 0.01), (r - 0.004, zr)], 12)
        return H
    if kind == "wicker":              # a woven column, waisted, with a plaited top rim
        H = 0.55
        prof = [(0, 0), (0.17, 0), (0.17, 0.03), (0.12, 0.2), (0.105, 0.32), (0.12, 0.44), (0.16, 0.52),
                (0.17, H), (0, H)]
        lathe(m, WICK, prof, 12)
        for zz in (0.03, 0.52):              # plaited rims
            lathe(m, WICK, [(0.175, zz - 0.012), (0.188, zz), (0.175, zz + 0.012)], 12)
        for zz in (0.12, 0.26, 0.38):        # woven bands
            r = 0.105 + 0.065 * abs(zz - 0.32) / 0.3 + 0.004
            lathe(m, WICK, [(r, zz - 0.01), (r + 0.008, zz), (r, zz + 0.01)], 12)
        return H
    raise ValueError(kind)


def _soil_pt(rnd, rs, zs, frac=0.8):
    a = rnd.uniform(0, 2 * math.pi)
    r = rs * frac * math.sqrt(rnd.random())
    return (r * math.cos(a), r * math.sin(a), zs)


# --- the plants (each builds at size 1; ctx: rnd, soil z, inner radius, pot r, z floor for trailers) ---------
class Ctx:
    def __init__(self, rnd, zs, rs, pr, ph, zmin):
        self.rnd, self.zs, self.rs, self.pr, self.ph, self.zmin = rnd, zs, rs, pr, ph, zmin


def _ficus_elastica(m, c):
    rnd = c.rnd
    H = rnd.uniform(0.7, 1.05)
    lean = (rnd.uniform(-0.06, 0.06), rnd.uniform(-0.06, 0.06))
    sp = lambda t: (lean[0] * t * t, lean[1] * t * t, c.zs + H * t)
    rod(m, BARK, [sp(k / 5) for k in range(6)], 0.015, 0.009, 5)
    n = rnd.randint(15, 19)
    a0 = rnd.uniform(0, 6.3)
    for i in range(n):
        t = 0.22 + 0.76 * i / (n - 1)
        b = sp(t)
        L = rnd.uniform(0.24, 0.31) * (1.0 - 0.4 * max(0.0, t - 0.75) / 0.25)
        el = math.radians(rnd.uniform(25, 45) + 25 * t ** 2)
        droop = math.radians(rnd.uniform(25, 45))
        stem_leaf(m, b, a0 + i * GOLD + rnd.uniform(-0.2, 0.2), el, droop, 0.05, L, L * rnd.uniform(0.28, 0.32), GL,
                  stem_mat=STEM, pr=0.004, segs=5, shape="elliptic", cup=-0.06, fold=0.03, roll=rnd.uniform(-0.5, 0.5),
                  back=GR, power=1.8)
    top = sp(1.0)
    tube(m, SHEATH, top, _ad(top, (lean[0] * 0.5, lean[1] * 0.5, 0.1)), 0.008, 0.0, 5)


def _ficus_benjamina(m, c):
    """Weeping fig: braided trunks, arching branches hung with twig cards (dense) and some loose leaves."""
    rnd = c.rnd
    H = rnd.uniform(1.2, 1.5)
    ntr = rnd.randint(2, 3)
    for k in range(ntr):                              # a few braided trunks
        ph = 2 * math.pi * k / ntr
        pts = [(0.014 * math.cos(ph + 4 * t), 0.014 * math.sin(ph + 4 * t), c.zs + 0.72 * H * t) for t in [i / 6 for i in range(7)]]
        rod(m, BARK, pts, 0.012, 0.008, 4)
    nb = rnd.randint(13, 16)
    a0 = rnd.uniform(0, 6.3)
    cell = ["benjamina_a", "benjamina_b"]
    for i in range(nb):
        t = i / (nb - 1)
        base = (0.0, 0.0, c.zs + H * (0.42 + 0.4 * t))
        az = a0 + i * GOLD
        el = math.radians(15 + 50 * t)
        reach = (0.5 - 0.25 * t) * rnd.uniform(0.85, 1.1)
        br = Blade(base, az, el, reach, math.radians(rnd.uniform(40, 70)), 1.3, 6)
        blade_rod(m, BARK, br, 1.0, 0.006, 0.002, 3, 3)
        for j in range(6):                            # pendulous twig sprays along the branch
            tb = 0.2 + 0.8 * j / 5
            p0 = br.pt(tb)
            tw_az, tw_el, tw_L = az + rnd.uniform(-1.4, 1.4), math.radians(rnd.uniform(-5, 35)), rnd.uniform(0.2, 0.28)
            tw_dr, tw_roll = math.radians(rnd.uniform(50, 90)), rnd.uniform(-0.8, 0.8)
            tw = Blade(p0, tw_az, tw_el, tw_L, tw_dr, 1.2, 3, roll=tw_roll)
            for rr in (0.0, 1.57):          # crossed pair
                tw.__init__(p0, tw_az, tw_el, tw_L, tw_dr, 1.2, 3, roll=tw_roll + rr)
                leaf(m, tw, rnd.uniform(0.05, 0.06), "const", 0.0, 3, (-1.0, 1.0), CARDS, uvrect=CARD_CELLS[rnd.choice(cell)],
                     cup=0.2)
        for q in range(8):                            # loose leaves for depth
            lp = br.pt(rnd.uniform(0.2, 1.0))
            L = rnd.uniform(0.06, 0.08)
            bl = Blade(lp, rnd.uniform(0, 6.3), math.radians(rnd.uniform(-40, 10)), L, math.radians(25), 1.5, 3,
                       roll=rnd.uniform(-0.5, 0.5))
            leaf(m, bl, L * 0.2, "lanceolate", 0.0, 2, (-1.0, 0.0, 1.0), GR if rnd.random() < 0.6 else GL, fold=0.12, back=LT)
    for q in range(rnd.randint(10, 14)):              # the crown top
        a = rnd.uniform(0, 6.3)
        r = rnd.uniform(0.0, 0.1)
        p0 = (r * math.cos(a), r * math.sin(a), c.zs + H * rnd.uniform(0.85, 0.97))
        tw = Blade(p0, a, math.radians(rnd.uniform(30, 70)), rnd.uniform(0.18, 0.24), math.radians(rnd.uniform(60, 100)), 1.2, 3,
                   roll=rnd.uniform(-0.8, 0.8))
        leaf(m, tw, 0.055, "const", 0.0, 3, (-1.0, 1.0), CARDS, uvrect=CARD_CELLS[rnd.choice(cell)], cup=0.2)


def _monstera(m, c):
    rnd = c.rnd
    n = rnd.randint(8, 10)
    adult = True             # juvenile (unsplit) leaves: the two youngest
    a0 = rnd.uniform(0, 6.3)
    for i in range(n):
        az = a0 + i * GOLD + rnd.uniform(-0.3, 0.3)
        base = _soil_pt(rnd, c.rs, c.zs, 0.4)
        young = i >= n - 2
        pet = rnd.uniform(0.38, 0.68) * (0.75 if young else 1.0)
        L = (rnd.uniform(0.34, 0.46) if adult else rnd.uniform(0.2, 0.28)) * (0.7 if young else 1.0)
        # petiole: rises and leans out; the blade hangs from its tip at an angle (the 'knee' of the geniculum)
        pb = Blade(base, az, math.radians(rnd.uniform(70, 86)), pet, math.radians(rnd.uniform(25, 50)), 1.6, 6)
        rod(m, STEM, pb.C[::2], 0.01, 0.007, 4)
        bl = Blade(pb.tip(), az + rnd.uniform(-0.3, 0.3), math.radians(rnd.uniform(-10, 20)), L, math.radians(rnd.uniform(40, 70)),
                   1.4, 8, roll=rnd.uniform(-0.35, 0.35))
        w = L * rnd.uniform(0.47, 0.52)
        cols = (-1.0, -0.66, -0.3, 0.0, 0.3, 0.66, 1.0)
        segs = 9
        slit = adult and not young
        cells = []
        for k in range(segs):
            xa, xb = k / segs, (k + 1) / segs
            for j in range(len(cols) - 1):
                ua, ub = cols[j], cols[j + 1]
                outer = slit and abs(ua) >= 0.29 and abs(ub) >= 0.29 and 0 < k < segs - 1
                if outer:            # pinnate slits: each lobe is a little narrower and swept towards the tip
                    g = 0.22 / segs
                    f = lambda u: (0.8 / segs) * ((abs(u) - 0.3) / 0.7) ** 1.5
                    if abs(ua) < 0.7 and abs(ub) < 0.7 and k in (2, 5) and rnd.random() < 0.6:
                        continue     # a fenestration (hole) near the midrib
                    cells.append(((k, j), [(xa + g + f(ua), ua), (xa + g + f(ub), ub), (min(1.0, xb + f(ub)), ub),
                                           (min(1.0, xb + f(ua)), ua)]))
                else:
                    cells.append(((k, j), [(xa, ua), (xa, ub), (xb, ub), (xb, ua)]))
        leaf(m, bl, w, "monstera", 0.0, cells=cells, mats=LT if young else GR, cup=-0.12, notch=0.14, back=LT)
    for k in range(rnd.randint(1, 3)):                 # aerial roots
        a = rnd.uniform(0, 6.3)
        p0 = (0.03 * math.cos(a), 0.03 * math.sin(a), c.zs + rnd.uniform(0.12, 0.25))
        p1 = (c.rs * 0.7 * math.cos(a), c.rs * 0.7 * math.sin(a), c.zs + 0.08)
        p2 = (c.rs * 0.75 * math.cos(a + 0.2), c.rs * 0.75 * math.sin(a + 0.2), c.zs)
        rod(m, ROOT, [p0, p1, p2], 0.005, 0.004, 3)


def _sansevieria(m, c):
    rnd = c.rnd
    n = rnd.randint(6, 9)
    laur = rnd.random() < 0.55
    a0 = rnd.uniform(0, 6.3)
    for i in range(n):
        base = _soil_pt(rnd, c.rs, c.zs - 0.01, 0.6)
        az = a0 + i * GOLD
        L = rnd.uniform(0.42, 0.7) * (0.75 if i > n - 3 else 1.0)
        el = math.radians(rnd.uniform(76, 88))
        bl = Blade(base, az, el, L, math.radians(rnd.uniform(4, 16)), 1.5, 8, roll=rnd.uniform(-1.6, 1.6),
                   twist=rnd.uniform(-0.6, 0.6))
        w = rnd.uniform(0.028, 0.038)
        cols = (-1.0, -0.84, 0.84, 1.0) if laur else (-1.0, 0.0, 1.0)
        bands = [rnd.random() < 0.4 for _ in range(14)]

        def mt(k, j, bands=bands):
            if laur and j != 1:
                return YEL
            return BAND if bands[k] else GG
        leaf(m, bl, w, "sword", 0.0, 12, cols, mt, fold=0.22, cup=0.0)


def _aspidistra(m, c):
    rnd = c.rnd
    n = rnd.randint(10, 13)
    a0 = rnd.uniform(0, 6.3)
    for i in range(n):
        base = _soil_pt(rnd, c.rs, c.zs, 0.5)
        az = a0 + i * GOLD
        pet = rnd.uniform(0.1, 0.17)
        L = rnd.uniform(0.34, 0.48)
        el = math.radians(rnd.uniform(72, 86))
        stem_leaf(m, base, az, el, math.radians(rnd.uniform(60, 95)), pet, L, L * rnd.uniform(0.13, 0.16), GL,
                  stem_mat=STEM, pr=0.0035, segs=6, shape="elliptic", fold=0.14, cup=-0.05, power=2.0, n=12,
                  roll=rnd.uniform(-0.3, 0.3), back=GR)


def _trail(c, rnd, start, az, el, length, n, rmin):
    """A trailing shoot: rises from the rim, arches over and hangs; does not pass through the pot, stops at zmin
    (then runs along the shelf)."""
    pts = [start]
    d_el = el
    n = max(n, int(length / 0.075))
    step = length / n
    for i in range(n):
        d_el = max(-1.45, d_el - rnd.uniform(4.0, 6.5) * step)
        p = pts[-1]
        q = (p[0] + math.cos(d_el) * math.cos(az) * step, p[1] + math.cos(d_el) * math.sin(az) * step, p[2] + math.sin(d_el) * step)
        rq = math.hypot(q[0], q[1])
        if q[2] < c.ph + 0.01 and rq < rmin:
            s = rmin / max(rq, 1e-6)
            q = (q[0] * s, q[1] * s, q[2])
        if q[2] < c.zmin + 0.008:
            q = (q[0] + math.cos(az) * step * 0.7, q[1] + math.sin(az) * step * 0.7, c.zmin + 0.008)
        pts.append(q)
        az += rnd.uniform(-0.15, 0.15)
    return pts


def _chlorophytum(m, c):
    rnd = c.rnd
    n = rnd.randint(22, 26)
    vit = rnd.random() < 0.6
    cols = (-1.0, -0.3, 0.3, 1.0)
    mats = [GR, PALE, GR] if vit else [PALE, LT, PALE]
    a0 = rnd.uniform(0, 6.3)
    for i in range(n):
        base = _soil_pt(rnd, c.rs, c.zs, 0.25)
        az = a0 + i * GOLD
        L = rnd.uniform(0.25, 0.38)
        bl = Blade(base, az, math.radians(rnd.uniform(35, 80)), L, math.radians(rnd.uniform(90, 130)), 1.3, 6,
                   roll=rnd.uniform(-0.3, 0.3))
        leaf(m, bl, rnd.uniform(0.011, 0.014), "linear", 0.0, 3, cols, mats, fold=0.12)
    # stolons (runners) with plantlets and a few white flowers
    for k in range(rnd.randint(2, 4)):
        az = rnd.uniform(0, 6.3)
        pts = _trail(c, rnd, (0.0, 0.0, c.zs + 0.04), az, math.radians(55), rnd.uniform(0.35, 0.55) + (0.25 if c.zmin < 0 else 0.0), 6,
                     c.pr + 0.02)
        rod(m, STEM, pts[:-1:2] + [pts[-1]] if len(pts) > 7 else pts, 0.0018, 0.0015, 3)
        tip = pts[-1]
        for q in range(5):
            L = rnd.uniform(0.05, 0.085)
            bl = Blade(tip, az + q * GOLD, math.radians(rnd.uniform(20, 60)), L, math.radians(70), 1.4, 3)
            leaf(m, bl, 0.006, "linear", 0.0, 2, (-1.0, 0.0, 1.0), mats[0], fold=0.25)
        p = pts[len(pts) // 2]
        floret(m, FWHITE, _ad(p, (0, 0, 0.01)), (0, 0, 1), 0.008, rot=rnd.uniform(0, 1))


def _tradescantia(m, c):
    rnd = c.rnd
    zebrina = rnd.random() < 0.6
    top, back = (SIL, PUR) if zebrina else (LT, GR)
    stripe = GR if zebrina else LT
    n = rnd.randint(9, 11)
    long = 0.45 if c.zmin < 0 else 0.0
    for i in range(n):
        az = 2 * math.pi * i / n + rnd.uniform(-0.2, 0.2)
        r0 = c.rs * 0.75
        start = (r0 * math.cos(az), r0 * math.sin(az), c.zs + 0.01)
        if i % 4 == 0:            # a short upright shoot on top
            pts = [start, _ad(start, (0.02 * math.cos(az), 0.02 * math.sin(az), 0.06)), _ad(start, (0.05 * math.cos(az), 0.05 * math.sin(az), 0.1))]
        else:
            pts = _trail(c, rnd, start, az, math.radians(rnd.uniform(25, 50)), rnd.uniform(0.28, 0.42) + long, 5, c.pr + 0.015)
        rod(m, STEM if not zebrina else PUR, pts[:-1:3] + [pts[-1]] if len(pts) > 7 else pts, 0.0022, 0.0018, 3)
        # leaves alternating along the shoot, held out sideways and up
        nl = (8 + int(10 * long)) if len(pts) > 3 else 4
        segL = [_dist(a, b) for a, b in zip(pts, pts[1:])]
        tot = sum(segL)
        for q in range(nl):
            s = tot * (q + 0.6) / nl
            j = 0
            while j < len(segL) - 1 and s > segL[j]:
                s -= segL[j]
                j += 1
            p = _lerp(pts[j], pts[j + 1], min(1.0, s / max(segL[j], 1e-6)))
            daz, _ = az_el(norm((pts[j + 1][0] - pts[j][0] + 1e-9, pts[j + 1][1] - pts[j][1], 0.0)))
            side = 1 if q % 2 else -1
            L = rnd.uniform(0.045, 0.06)
            bl = Blade(p, daz + side * rnd.uniform(0.9, 1.4), math.radians(rnd.uniform(5, 30)), L, math.radians(30), 1.5, 3,
                       roll=-side * 0.3)
            leaf(m, bl, L * 0.38, "ovate", 0.0, 2, (-1.0, -0.35, 0.35, 1.0), [top, stripe, top], back=back, fold=0.15)


def _geranium(m, c):
    rnd = c.rnd
    ns = rnd.randint(4, 5)
    tops = []
    for k in range(ns):
        az = 2 * math.pi * k / ns + rnd.uniform(-0.4, 0.4)
        b = _soil_pt(rnd, c.rs, c.zs, 0.3)
        st = Blade(b, az, math.radians(rnd.uniform(55, 75)), rnd.uniform(0.12, 0.2), math.radians(10), 1.0, 4)
        blade_rod(m, STEM, st, 1.0, 0.007, 0.005, 3, 4)
        tops.append((st, az))
    zonal = rnd.random() < 0.6
    nl = rnd.randint(14, 17)
    for i in range(nl):
        st, az = tops[i % ns]
        t = rnd.uniform(0.2, 1.0)
        p = st.pt(t)
        laz = az + rnd.uniform(-1.3, 1.3)
        L = rnd.uniform(0.075, 0.1)
        pet = rnd.uniform(0.04, 0.08)

        def mt(k, j, zonal=zonal):
            return VEL if (zonal and k == 2) else GR
        stem_leaf(m, p, laz, math.radians(rnd.uniform(15, 45)), math.radians(rnd.uniform(20, 40)), pet, L, L * 0.62, mt,
                  stem_mat=STEM, pr=0.0022, segs=4, cols=(-1.0, 0.0, 1.0), shape="round", notch=0.25, cup=0.12, wave=0.12,
                  n=6, back=LT)
    for k in range(rnd.randint(3, 5)):                      # flower umbels on long peduncles
        st, az = tops[k % ns]
        p = st.pt(rnd.uniform(0.7, 1.0))
        ped = Blade(p, az + rnd.uniform(-0.8, 0.8), math.radians(rnd.uniform(60, 85)), rnd.uniform(0.09, 0.14),
                    math.radians(15), 1.0, 3)
        blade_rod(m, STEM, ped, 1.0, 0.002, 0.0018, 2, 3)
        cen = _ad(ped.tip(), (0, 0, 0.02))
        nf = rnd.randint(12, 15)
        for f in range(nf):
            th = f * GOLD
            ph = math.acos(1 - (f + 0.5) / nf * 1.15)
            dirv = (math.sin(ph) * math.cos(th), math.sin(ph) * math.sin(th), math.cos(ph))
            floret(m, FRED, _ad(cen, dirv, 0.03), dirv, 0.016, rot=rnd.uniform(0, 1.2), cup=0.25)


def _begonia(m, c):
    rnd = c.rnd
    bronze = rnd.random() < 0.4
    lm = BRZ if bronze else GR
    ns = rnd.randint(6, 8)
    tops = []
    for k in range(ns):
        az = 2 * math.pi * k / ns + rnd.uniform(-0.3, 0.3)
        b = _soil_pt(rnd, c.rs, c.zs, 0.5)
        st = Blade(b, az, math.radians(rnd.uniform(50, 75)), rnd.uniform(0.07, 0.13), math.radians(20), 1.0, 3)
        blade_rod(m, STEM if not bronze else SHEATH, st, 1.0, 0.004, 0.003, 2, 3)
        tops.append((st, az))
        for q in range(3):
            p = st.pt(0.35 + 0.33 * q)
            L = rnd.uniform(0.065, 0.09)
            stem_leaf(m, p, az + rnd.uniform(-1.2, 1.2), math.radians(rnd.uniform(0, 30)), math.radians(rnd.uniform(25, 45)),
                      0.015, L, L * 0.36, lm, stem_mat=STEM, pr=0.002, psteps=1, segs=4, shape="ovate", asym=0.35,
                      notch=0.12, cup=-0.06, fold=0.06, n=6, back=PUR if bronze else LT)
    for k in range(rnd.randint(11, 14)):                    # pink flowers in loose clusters above the leaves
        st, az = tops[k % ns]
        cen = _ad(st.tip(), (rnd.uniform(-0.035, 0.035), rnd.uniform(-0.035, 0.035), rnd.uniform(0.015, 0.05)))
        nrm = norm((rnd.uniform(-0.6, 0.6), rnd.uniform(-0.6, 0.6), 1.0))
        rod(m, STEM, [st.tip(), cen], 0.0015, 0.0015, 3)
        floret(m, FPINK, cen, nrm, 0.017, 5, rnd.uniform(0, 1), cup=0.35)
        floret(m, FPINK, _ad(cen, nrm, 0.004), nrm, 0.011, 5, rnd.uniform(0, 1), cup=0.4)
        floret(m, FYEL, _ad(cen, nrm, 0.007), nrm, 0.003, 3)


def _violet(m, c):
    rnd = c.rnd
    purple_back = rnd.random() < 0.5
    nl = rnd.randint(12, 15)
    a0 = rnd.uniform(0, 6.3)
    for i in range(nl):
        whorl = i / nl
        az = a0 + i * GOLD
        L = rnd.uniform(0.05, 0.065) * (1.0 - 0.3 * whorl)
        pet = rnd.uniform(0.045, 0.065) * (1.0 - 0.5 * whorl)
        stem_leaf(m, (0.0, 0.0, c.zs + 0.005), az, math.radians(18 + 30 * whorl), math.radians(rnd.uniform(15, 35)), pet, L,
                  L * 0.42, VEL, stem_mat=STEM, pr=0.0022, psteps=2, segs=4, shape="round", notch=0.12, cup=-0.1, n=6,
                  back=PUR if purple_back else GR)
    col = rnd.choice([FVIO, FVIO, FPINK, FWHITE])
    for k in range(rnd.randint(7, 11)):
        a = rnd.uniform(0, 6.3)
        r = rnd.uniform(0.0, 0.035)
        cen = (r * math.cos(a), r * math.sin(a), c.zs + rnd.uniform(0.06, 0.08))
        rod(m, STEM, [(0.0, 0.0, c.zs), cen], 0.0012, 0.0012, 3)
        nrm = norm((r * math.cos(a) * 8, r * math.sin(a) * 8, 1.0))
        floret(m, col, cen, nrm, 0.014, 5, rnd.uniform(0, 1), cup=0.15)
        floret(m, FYEL, _ad(cen, nrm, 0.003), nrm, 0.0035, 3)


def _asparagus(m, c):
    """Asparagus fern: arching stems carrying alpha sprays (sprengeri: needle plumes; setaceus: flat lace)."""
    rnd = c.rnd
    sp = rnd.random() < 0.6
    cells = ["sprengeri"] if sp else ["setaceus"]
    n = rnd.randint(11, 14)
    long = 0.35 if c.zmin < 0 else 0.0
    for i in range(n):
        az = i * GOLD + rnd.uniform(-0.2, 0.2)
        b = _soil_pt(rnd, c.rs, c.zs, 0.4)
        L = rnd.uniform(0.3, 0.5) + (long * rnd.random() if sp else 0.0)
        el = math.radians(rnd.uniform(45, 85))
        bl = Blade(b, az, el, L, math.radians(rnd.uniform(90, 140) if sp else rnd.uniform(40, 80)), 1.3, 8)
        # keep it out of the pot wall / above the shelf
        ok = all(not (p[2] < c.ph + 0.01 and math.hypot(p[0], p[1]) < c.pr + 0.01) and p[2] > c.zmin + 0.01 for p in bl.C)
        if not ok:
            bl = Blade(b, az, math.radians(70), L * 0.8, math.radians(70), 1.3, 8)
        blade_rod(m, STEM, bl, 1.0, 0.0022, 0.0012, 3, 3)
        k = 4 if sp else 3
        for q in range(k):
            ta = 0.15 + 0.85 * q / k
            tb = min(1.0, ta + 1.0 / k + 0.08)
            # a card along this stretch of the stem, rolled so the sprays face out and up
            sub = Blade(bl.pt(ta), *az_el(bl.at(ta)[1]), L * (tb - ta) * 1.15, math.radians(25), 1.3, 2,
                        roll=rnd.uniform(-0.6, 0.6))
            leaf(m, sub, (0.035 if sp else 0.065) * rnd.uniform(0.85, 1.1), "const", 0.0, 2, (-1.0, 1.0), CARDS,
                 uvrect=CARD_CELLS[rnd.choice(cells)], cup=0.15)


def _palm(m, c):
    """Kentia palm (Howea forsteriana) in a tub: arching fronds with drooping leaflets."""
    rnd = c.rnd
    n = rnd.randint(7, 9)
    a0 = rnd.uniform(0, 6.3)
    for i in range(n):
        az = a0 + i * GOLD
        b = _soil_pt(rnd, c.rs, c.zs, 0.3)
        pet = rnd.uniform(0.35, 0.65)
        rach = rnd.uniform(0.6, 0.85)
        tot = pet + rach
        el = math.radians(rnd.uniform(62, 84))
        bl = Blade(b, az, el, tot, math.radians(rnd.uniform(70, 105)), 2.2, 12, roll=rnd.uniform(-0.2, 0.2))
        blade_rod(m, STEM, bl, 1.0, 0.007, 0.002, 5, 3)
        t0 = pet / tot
        np_ = rnd.randint(13, 15)
        for q in range(np_):
            x = (q + 0.5) / np_
            t = t0 + (1 - t0) * x
            C, D, S, N = bl.at(t)
            lL = 0.32 * math.sin(math.pi * (0.15 + 0.8 * x)) * rnd.uniform(0.9, 1.1) + 0.06
            for side in (-1, 1):
                dv = norm(_ad(_ad(_sc(S, side * 0.85), D, 0.5), N, 0.15))
                laz, lel = az_el(dv)
                lb = Blade(C, laz, lel, lL, math.radians(rnd.uniform(40, 75)), 1.3, 3)
                leaf(m, lb, 0.013, "linear", 0.0, 2, (-1.0, 0.0, 1.0), GR if q % 3 else LT, fold=0.35, back=LT)


def _aloe(m, c):
    """Candelabra aloe ('stoletnik', A. arborescens): a short stem with a rosette of thick toothed leaves."""
    rnd = c.rnd
    sh = rnd.choice([0.0, 0.02, 0.04, 0.1])
    top = (0.0, 0.0, c.zs + sh)
    if sh > 0:
        rod(m, BARK, [(0, 0, c.zs - 0.01), top], 0.014, 0.012, 6)
    n = rnd.randint(15, 18)
    a0 = rnd.uniform(0, 6.3)
    for i in range(n):
        t = i / n
        az = a0 + i * GOLD
        L = rnd.uniform(0.2, 0.28) * (1.0 - 0.5 * t)
        el = math.radians(35 + 50 * t + rnd.uniform(-8, 8))
        bl = Blade(_ad(top, (0, 0, 0.02 * t)), az, el, L, math.radians(rnd.uniform(30, 55) * (1 - t * 0.5)), 1.3, 5)
        w = L * rnd.uniform(0.075, 0.09) + 0.004
        _thick_leaf(m, bl, w, SUC, SPN)


def _thick_leaf(m, bl, w, mat, tooth=None, segs=5):
    """A fleshy leaf of triangular-ish section (flat top, keel below), tapering to a point; teeth on its edges."""
    secs = []
    for k in range(segs + 1):
        x = k / segs
        C, D, S, N = bl.at(x)
        ww = w * (1.0 - x ** 1.5) * (0.85 + 0.15 * min(1.0, x * 4))
        secs.append((C, [_ad(C, S, -ww), _ad(C, N, ww * 0.25), _ad(C, S, ww), _ad(C, N, -ww * 0.75)], N, S, ww))
    for k in range(segs):
        ca, ra, _, _, _ = secs[k]
        cb, rb, _, _, _ = secs[k + 1]
        cc = _ad(_lerp(ca, cb, 0.5), _lerp(secs[k][2], secs[k + 1][2], 0.5), -w * 0.2)
        for j in range(4):
            jj = (j + 1) % 4
            pts = [ra[j], ra[jj], rb[jj], rb[j]]
            if k == segs - 1:
                pts = [ra[j], ra[jj], rb[jj]]
            q = [sum(p[i] for p in pts) / len(pts) for i in range(3)]
            face_n(m, mat, pts, (q[0] - cc[0], q[1] - cc[1], q[2] - cc[2]))
    c0, r0, n0, _, _ = secs[0]
    face_n(m, mat, r0, _sc(bl.D[0], -1))
    if tooth:
        for k in range(1, segs - 1):
            C, ring, N, S, ww = secs[k]
            D = bl.at(k / segs)[1]
            for side, e in ((-1, ring[0]), (1, ring[2])):
                tp = _ad(_ad(e, S, side * 0.006), D, 0.004)
                a, b = _ad(e, D, -0.004), _ad(e, D, 0.004)
                face_n(m, tooth, [a, b, tp], N)
                face_n(m, tooth, [_ad(a, N, -BACK_OFF), _ad(b, N, -BACK_OFF), _ad(tp, N, -BACK_OFF)], _sc(N, -1))


def _ribbed(m, mat, base, prof, ribs, depth=0.12, spn=None, rnd=None, seg_per_rib=2):
    """A ribbed cactus body: lathe profile [(r, z)] (bottom centre .. top centre) with `ribs` ridges."""
    ns = ribs * seg_per_rib
    rings = []
    for r, z in prof:
        ring = []
        for j in range(ns):
            a = 2 * math.pi * j / ns
            rr = r * (1.0 - depth * (0.5 - 0.5 * math.cos(ribs * a)))
            ring.append((base[0] + rr * math.cos(a), base[1] + rr * math.sin(a), base[2] + z))
        rings.append(ring)
    for k in range(len(prof) - 1):
        A, B = rings[k], rings[k + 1]
        zc = base[2] + (prof[k][1] + prof[k + 1][1]) / 2
        for j in range(ns):
            jj = (j + 1) % ns
            pts = [A[j], A[jj], B[jj], B[j]]
            if prof[k][0] < 1e-6:
                pts = [A[j], B[jj], B[j]]
            elif prof[k + 1][0] < 1e-6:
                pts = [A[j], A[jj], B[j]]
            q = [sum(p[i] for p in pts) / len(pts) for i in range(3)]
            face_n(m, mat, pts, (q[0] - base[0], q[1] - base[1], q[2] - zc))
    if spn:                                   # areoles: tiny pale tufts on the rib crests
        for k in range(1, len(prof) - 1):
            r, z = prof[k]
            for j in range(ribs):
                a = 2 * math.pi * j / ribs + (math.pi / ribs if k % 2 else 0) * 0.3
                p = (base[0] + r * 1.01 * math.cos(a), base[1] + r * 1.01 * math.sin(a), base[2] + z)
                out = (math.cos(a), math.sin(a), 0.3)
                floret(m, spn, p, out, 0.0028 + 0.001 * rnd.random(), 3, rnd.uniform(0, 2), cup=1.2)


def _cactus(m, c):
    rnd = c.rnd
    col = rnd.random() < 0.45
    ribs = rnd.randint(8, 12)
    base = (0.0, 0.0, c.zs - 0.01)
    if col:      # a columnar cereus, sometimes with a side arm
        H = rnd.uniform(0.18, 0.3)
        r = c.rs * 0.55
        prof = [(0.0, 0.0), (r * 0.9, 0.0), (r, 0.03), (r, H * 0.5), (r * 0.95, H * 0.85), (r * 0.6, H * 0.97), (0.0, H)]
        _ribbed(m, CAC, base, prof, ribs, 0.14, SPN, rnd)
        if rnd.random() < 0.6:
            a = rnd.uniform(0, 6.3)
            rr = r * 0.6
            b2 = (r * 0.9 * math.cos(a), r * 0.9 * math.sin(a), c.zs + H * 0.35)
            prof2 = [(0.0, 0.0), (rr, 0.01), (rr, H * 0.3), (rr * 0.6, H * 0.38), (0.0, H * 0.4)]
            _ribbed(m, CAC, b2, prof2, 8, 0.14, None, rnd)
    else:        # a globular echinopsis with pups, maybe flowering
        r = c.rs * rnd.uniform(0.7, 0.85)
        H = r * 1.6
        prof = [(0.0, 0.0), (r * 0.7, 0.0), (r * 0.95, H * 0.25), (r, H * 0.5), (r * 0.85, H * 0.78), (r * 0.45, H * 0.95), (0.0, H)]
        _ribbed(m, CAC, base, prof, ribs, 0.16, SPN, rnd)
        for k in range(rnd.randint(0, 2)):
            a = rnd.uniform(0, 6.3)
            rr = r * 0.32
            b2 = ((r + rr * 0.3) * math.cos(a), (r + rr * 0.3) * math.sin(a), c.zs - 0.005)
            _ribbed(m, CAC, b2, [(0.0, 0.0), (rr, rr * 0.6), (rr * 0.6, rr * 1.4), (0.0, rr * 1.6)], 8, 0.16, None, rnd)
        if rnd.random() < 0.5:                       # the long pink trumpet of an echinopsis
            a = rnd.uniform(0, 6.3)
            p0 = (r * 0.5 * math.cos(a), r * 0.5 * math.sin(a), c.zs + H * 0.9)
            ax = norm((math.cos(a) * 0.5, math.sin(a) * 0.5, 1.0))
            fl = Mesh()
            lathe(fl, STEM, [(0.0, 0.0), (0.006, 0.0), (0.006, 0.07)], 6)
            lathe(fl, FPINK, [(0.006, 0.07), (0.025, 0.11), (0.04, 0.13), (0.018, 0.115), (0.0, 0.11)], 8)
            t = (1.0, 0.0, 0.0) if abs(ax[0]) < 0.9 else (0.0, 1.0, 0.0)
            u = norm(cross(t, ax))
            v = cross(ax, u)
            m.extend(fl.transformed(lambda p: _ad(_ad(_ad(p0, u, p[0]), v, p[1]), ax, p[2])))


def _dieffenbachia(m, c):
    rnd = c.rnd
    ncane = rnd.randint(1, 3)
    for k in range(ncane):
        b = _soil_pt(rnd, c.rs, c.zs, 0.4) if ncane > 1 else (0.0, 0.0, c.zs)
        H = rnd.uniform(0.4, 0.85) * (1.0 - 0.25 * k)
        az = rnd.uniform(0, 6.3)
        lean = math.radians(rnd.uniform(75, 88))
        cane = Blade(b, az, lean, H, math.radians(5), 1.0, 4)
        blade_rod(m, STEM, cane, 1.0, 0.016, 0.013, 4, 6)
        nl = rnd.randint(7, 9)
        for i in range(nl):
            t = 0.45 + 0.55 * i / nl
            p = cane.pt(t)
            L = rnd.uniform(0.3, 0.4) * (0.8 if i > nl - 3 else 1.0)
            pet = rnd.uniform(0.1, 0.17)
            pal = [rnd.random() < 0.55 for _ in range(14)]

            def mt(s, j, pal=pal):
                if j in (0, 4):
                    return GL
                if j == 2:
                    return GR
                return PALE if pal[s * 2 + (j == 3)] else LT
            stem_leaf(m, p, az + i * GOLD, math.radians(rnd.uniform(35, 60) + 15 * t), math.radians(rnd.uniform(45, 65)), pet, L,
                      L * 0.25, mt, stem_mat=STEM, pr=0.005, segs=6, cols=(-1.0, -0.6, -0.1, 0.1, 0.6, 1.0), shape="elliptic",
                      cup=-0.1, fold=0.08, n=10, roll=rnd.uniform(-0.25, 0.25), back=GR)


def _dracaena(m, c):
    """Dragon tree (Dracaena fragrans 'Massangeana' / D. marginata): bare canes with tufts of arching straps."""
    rnd = c.rnd
    ncane = rnd.randint(2, 3)
    fr = rnd.random() < 0.5
    for k in range(ncane):
        b = _soil_pt(rnd, c.rs, c.zs, 0.35)
        H = rnd.uniform(0.6, 1.25) * (1.0 - 0.28 * k)
        cane = Blade(b, rnd.uniform(0, 6.3), math.radians(rnd.uniform(78, 88)), H, math.radians(rnd.uniform(-8, 8)), 1.0, 4)
        blade_rod(m, BARK, cane, 1.0, 0.018 if fr else 0.012, 0.014 if fr else 0.009, 4, 5)
        top = cane.tip()
        nl = rnd.randint(14, 18) if fr else rnd.randint(20, 26)
        for i in range(nl):
            t = i / nl
            az = i * GOLD
            L = (rnd.uniform(0.35, 0.55) if fr else rnd.uniform(0.25, 0.38)) * (1.0 - 0.3 * t)
            bl = Blade(_ad(top, (0, 0, 0.06 * t)), az, math.radians(10 + 70 * t), L, math.radians(rnd.uniform(40, 80) * (1 - 0.5 * t)),
                       1.6, 6, roll=rnd.uniform(-0.4, 0.4))
            if fr:
                leaf(m, bl, rnd.uniform(0.028, 0.035), "strap", 0.0, 5, (-1.0, -0.2, 0.2, 1.0), [GG, PALE, GG], fold=0.12, back=GR)
            else:
                leaf(m, bl, rnd.uniform(0.007, 0.009), "linear", 0.0, 4, (-1.0, 0.0, 1.0), GG, fold=0.25, back=SHEATH)


def _lemon(m, c):
    """Pot lemon (the Pavlovo lemon of Soviet windows): a small tree in a tub with glossy leaves and fruit."""
    rnd = c.rnd
    H = rnd.uniform(0.22, 0.32)
    trunk = Blade((0.0, 0.0, c.zs - 0.01), rnd.uniform(0, 6.3), math.radians(rnd.uniform(82, 88)), H, 0.0, 1.0, 3)
    blade_rod(m, BARK, trunk, 1.0, 0.018, 0.014, 3, 5)
    shoots = []
    a0 = rnd.uniform(0, 6.3)
    ns = rnd.randint(3, 4)
    for k in range(ns):                                        # scaffold branches, then side shoots
        az = a0 + 2 * math.pi * k / ns + rnd.uniform(-0.3, 0.3)
        sc = Blade(trunk.pt(rnd.uniform(0.85, 1.0)), az, math.radians(rnd.uniform(40, 60)), rnd.uniform(0.22, 0.3),
                   math.radians(rnd.uniform(5, 20)), 1.2, 4)
        blade_rod(m, BARK, sc, 1.0, 0.011, 0.006, 2, 4)
        shoots.append(sc)
        for q in range(3):
            p = sc.pt(0.4 + 0.3 * q)
            sh = Blade(p, az + rnd.uniform(-1.2, 1.2), math.radians(rnd.uniform(15, 65)), rnd.uniform(0.14, 0.22),
                       math.radians(rnd.uniform(10, 35)), 1.2, 3)
            blade_rod(m, BARK, sh, 1.0, 0.005, 0.0025, 2, 3)
            shoots.append(sh)
    for sh in shoots:
        for q in range(10):
            t = 0.15 + 0.85 * q / 9
            p = sh.pt(t)
            L = rnd.uniform(0.075, 0.105)
            bl = Blade(p, rnd.uniform(0, 6.3), math.radians(rnd.uniform(-5, 45)), L, math.radians(rnd.uniform(10, 30)), 1.5, 3,
                       roll=rnd.uniform(-0.4, 0.4))
            leaf(m, bl, L * 0.24, "elliptic", 0.0, 3, (-1.0, 0.0, 1.0), GL if rnd.random() < 0.7 else GR, cup=-0.1, fold=0.1,
                 back=LT)
    for k in range(rnd.randint(3, 6)):
        sh = rnd.choice(shoots[1:])
        p = sh.pt(rnd.uniform(0.3, 0.8))
        r = rnd.uniform(0.028, 0.036)
        cen = _ad(p, (0, 0, -r * 1.5))
        rod(m, BARK, [p, _ad(cen, (0, 0, r))], 0.0015, 0.0015, 3)
        ellipsoid(m, LEMON if rnd.random() < 0.7 else LEMONG, cen, r, r * 1.3, 8, 5)
    for k in range(rnd.randint(0, 4)):
        sh = rnd.choice(shoots)
        floret(m, FWHITE, _ad(sh.tip(), (0, 0, 0.01)), (0, 0, 1), 0.012, 5, rnd.uniform(0, 1), cup=0.3)


# kind: builder, class, pot styles (seeded pick), pot radius, pot height, description
KINDS = {
    "ficus_elastica": dict(fn=_ficus_elastica, cls="floor", pots=("terracotta", "glazed_brown", "glazed_green", "enamel"), pr=0.15, ph=0.22,
                           desc="rubber plant: one stem, big dark glossy elliptic leaves, red sheath at the tip"),
    "ficus_benjamina": dict(fn=_ficus_benjamina, cls="floor", pots=("glazed_white", "terracotta", "glazed_brown"), pr=0.16, ph=0.22,
                            desc="weeping fig: braided trunks, drooping twigs dense with small pointed leaves"),
    "monstera": dict(fn=_monstera, cls="floor", pots=("tub", "glazed_green", "glazed_brown"), pr=0.2, ph=0.26,
                     desc="Swiss cheese plant: heart-shaped split leaves on long petioles, aerial roots"),
    "sansevieria": dict(fn=_sansevieria, cls="sill", pots=("terracotta", "glazed_white", "enamel", "glazed_brown"), pr=0.1, ph=0.13,
                        desc="snake plant ('teshchin yazyk'): upright banded swords, sometimes yellow-edged"),
    "aspidistra": dict(fn=_aspidistra, cls="sill", pots=("terracotta", "glazed_green", "enamel"), pr=0.12, ph=0.15,
                       desc="cast-iron plant: dark arching lanceolate leaves on stalks straight from the soil"),
    "chlorophytum": dict(fn=_chlorophytum, cls="sill", pots=("terracotta", "glazed_white", "enamel"), pr=0.085, ph=0.1,
                         desc="spider plant: arching striped straps, runners with plantlets"),
    "tradescantia": dict(fn=_tradescantia, cls="sill", pots=("terracotta", "glazed_white", "enamel"), pr=0.08, ph=0.09,
                         desc="wandering Jew: trailing shoots with striped silver/purple (zebrina) or green leaves"),
    "geranium": dict(fn=_geranium, cls="sill", pots=("terracotta", "terracotta", "enamel"), pr=0.085, ph=0.1,
                     desc="zonal pelargonium: round scalloped leaves with a dark zone, scarlet umbels"),
    "begonia": dict(fn=_begonia, cls="sill", pots=("terracotta", "glazed_white", "glazed_green"), pr=0.08, ph=0.09,
                    desc="flowering begonia: lopsided glossy (or bronze) leaves, pink double flowers"),
    "african_violet": dict(fn=_violet, cls="sill", pots=("terracotta", "glazed_white", "glazed_green"), pr=0.055, ph=0.065,
                           desc="uzambara violet: flat rosette of round velvet leaves, violet flowers in the centre"),
    "asparagus": dict(fn=_asparagus, cls="sill", pots=("terracotta", "glazed_white", "glazed_green"), pr=0.085, ph=0.1,
                      desc="asparagus fern: arching stems of needle plumes (sprengeri) or flat lace (setaceus)"),
    "palm": dict(fn=_palm, cls="floor", pots=("tub", "tub", "terracotta"), pr=0.21, ph=0.28,
                 desc="kentia palm in a tub: long arching fronds with drooping leaflets"),
    "aloe": dict(fn=_aloe, cls="sill", pots=("terracotta", "terracotta", "enamel", "glazed_brown"), pr=0.08, ph=0.1,
                 desc="'stoletnik' (Aloe arborescens): short stem, rosette of fleshy toothed grey-green leaves"),
    "cactus": dict(fn=_cactus, cls="sill", pots=("terracotta", "terracotta", "glazed_white"), pr=0.06, ph=0.07,
                   desc="cactus: ribbed globe with pups (maybe a pink trumpet flower) or a column with an arm"),
    "dieffenbachia": dict(fn=_dieffenbachia, cls="floor", pots=("glazed_white", "terracotta", "glazed_green"), pr=0.15, ph=0.2,
                          desc="dumb cane: thick canes, big oblong leaves with cream-blotched centres"),
    "dracaena": dict(fn=_dracaena, cls="floor", pots=("glazed_white", "terracotta", "glazed_brown"), pr=0.14, ph=0.22,
                     desc="dragon tree: bare canes topped by tufts of arching straps (striped fragrans or thin marginata)"),
    "lemon": dict(fn=_lemon, cls="floor", pots=("tub", "tub", "glazed_brown", "terracotta"), pr=0.17, ph=0.22,
                  desc="pot lemon tree in a tub: small crown of glossy leaves, lemons and white blossom"),
}


def _rng(kind, seed):
    return random.Random("%s:%d" % (kind, seed))


def make(kind, seed=0, size=1.0, stand=None):
    """The plant `kind` in its pot (optionally on a stand) at the origin, base on z=0; a geom.Mesh."""
    spec = KINDS[kind]
    rnd = _rng(kind, seed)
    m = Mesh()
    zb = _stand(m, stand, rnd) if stand else 0.0
    style = rnd.choice(spec["pots"])
    pr = spec["pr"] * rnd.uniform(0.95, 1.05)
    ph = spec["ph"] * rnd.uniform(0.95, 1.05)
    pm = Mesh()
    seg = 16 if spec["cls"] == "floor" else 10
    zs, rs = pot(pm, style, pr, ph, seg)
    m.extend(pm.transformed(lambda p: (p[0], p[1], p[2] + zb)))
    plant = Mesh()
    top = ph + (0.009 if style == "terracotta" else 0.03 if style == "tub" else 0.0)
    spec["fn"](plant, Ctx(rnd, zs, rs, pr, top, -zb))
    m.extend(plant.transformed(lambda p: (p[0], p[1], p[2] + zb)))
    if size != 1.0:
        m = m.transformed(lambda p: (p[0] * size, p[1] * size, p[2] * size))
    return m


_INFO = {}


def _extent(m):
    r = max(math.hypot(v[0], v[1]) for v in m.verts)
    return r, max(v[2] for v in m.verts)


def info(kind, size=1.0, stand=None):
    """(footprint radius, height): an upper bound over seeds (max over seeds 0..7, plus 3 %)."""
    key = (kind, stand)
    if key not in _INFO:
        ex = [_extent(make(kind, s, 1.0, stand)) for s in range(8)]
        _INFO[key] = (max(e[0] for e in ex) * 1.03, max(e[1] for e in ex) * 1.03)
    r, h = _INFO[key]
    return r * size, h * size


def pot_info(kind, size=1.0):
    """(radius, height) of the pot alone (upper bound; saucers/tub feet included)."""
    spec = KINDS[kind]
    return spec["pr"] * 1.05 * size, (spec["ph"] * 1.05 + 0.03) * size


def budget(kind, stand=None):
    return BUDGET[KINDS[kind]["cls"]] + (STAND_EXTRA if stand else 0)


def place(m, kind, x, y, z, yaw, seed=0, size=1.0, stand=None):
    """Adds the plant at (x, y, z) turned by yaw (radians) to the Mesh m; returns m."""
    m.extend(Frame(x, y, yaw, z).apply(make(kind, seed, size, stand)))
    return m


# --- self-test ------------------------------------------------------------------------------------------
def _selftest():
    import time
    # a flat leaf pointing +x faces up (+z) on its front side
    t = Mesh()
    leaf(t, Blade((0, 0, 0), 0.0, 0.0, 0.1), 0.02, "elliptic", 0.0, 3, (-1.0, 0.0, 1.0), GR)
    a, b, c_ = [t.verts[i] for i in t.faces[0][1][:3]]
    n = cross((b[0] - a[0], b[1] - a[1], b[2] - a[2]), (c_[0] - a[0], c_[1] - a[1], c_[2] - a[2]))
    assert n[2] > 0, "leaf front must face up"
    used = set()
    bad = []
    t0 = time.time()
    print("%-16s %-5s %6s %6s %6s  %5s %5s" % ("kind", "class", "s0", "s1", "s2", "r", "h"))
    for kind, spec in KINDS.items():
        cnt = []
        for seed in range(3):
            for stand in (None, "iron", "wicker") if seed == 0 else (None,):
                m = make(kind, seed, 1.0, stand)
                tc = m.tri_count()
                zmin = min(v[2] for v in m.verts)
                if stand is None:
                    cnt.append(tc)
                if tc > budget(kind, stand):
                    bad.append("%s seed %d stand %s: %d tris > %d" % (kind, seed, stand, tc, budget(kind, stand)))
                if zmin < -1e-6:
                    bad.append("%s seed %d stand %s: below z=0 (%.4f)" % (kind, seed, stand, zmin))
                for mat, idx, uv in m.faces:
                    used.add(mat)
                    if mat not in PLANT_MATERIALS:
                        bad.append("%s: unknown material %s" % (kind, mat))
                        break
                    if len(idx) != len(uv):
                        bad.append("%s: uv count" % kind)
                        break
        r, h = info(kind)
        for seed in range(3, 6):
            er, eh = _extent(make(kind, seed))
            if er > r * 1.12 or eh > h * 1.12:
                bad.append("%s seed %d exceeds info: %.2f/%.2f vs %.2f/%.2f" % (kind, seed, er, eh, r, h))
        print("%-16s %-5s %6d %6d %6d  %5.2f %5.2f" % (kind, spec["cls"], cnt[0], cnt[1], cnt[2], r, h))
    # place() and size
    m = Mesh()
    place(m, "geranium", 1.0, 2.0, 0.9, 0.5, 3, 1.2)
    assert abs(min(v[2] for v in m.verts) - 0.9) < 1e-6
    unused = set(PLANT_MATERIALS) - used
    print("materials unused:", sorted(unused) or "none")
    print("kinds: %d, %.1f s" % (len(KINDS), time.time() - t0))
    if bad:
        print("FAIL")
        for b in bad:
            print("  ", b)
        raise SystemExit(1)
    print("houseplants self-test OK")


if __name__ == "__main__":
    _selftest()
