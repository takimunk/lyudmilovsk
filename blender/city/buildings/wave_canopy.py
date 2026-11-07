"""Wave canopy: a free-form undulating white concrete marquee over a fair / promenade.

An original design in the spirit of 1950s Brazilian park marquees (a single thin sinuous slab on slender round
columns that shelters a public walk). ~120 m of canopy (arc length) follows a meandering curve (PARAMETERS
below), 8-12 m wide, a 0.2-0.3 m slab whose underside rolls between 4.2 and 5.4 m and whose edges curl up;
top 4.5 - 6.4 m. Carried by 28 slender round columns (r 0.14) staggered along both sides. Slab: closed top,
underside and edge band (CT_ConcreteWhite). Floor: a paving ribbon (CT_Paving) the length of the canopy.
Underneath: 5 coloured kiosk boxes (4.0 x 2.8 x 2.9 m, hollow, a back door each, a glazed counter hatch as a
"public" window, wooden shelf outside; spots "stall" inside and "counter" in front) and 12 benches ("bench").
The canopy itself is open: people walk and sit anywhere under it. No flats.

Plot 150 x 70 (fairground), max 8 m. The curve lies on y 13..57, x -52..52.

Ideas for later: hanging string lights, bunting, a stage kiosk, fairground rides in the clearings, glazed
roof lights, uplighting of the underside at night (needs an emissive strip material).
Material requests: none (CT_Plaster* for kiosks, CT_ConcreteWhite for the slab).
"""
import math
import random

from citykit import box, DOOR_W
from _sheet import thick_sheet
import furn_int3 as fu
from furn_int3 import Fr as MFr, lbox, cyl, blob

INFO = {
    "name_ru": "Волнистый навес ярмарки", "name_en": "Wave Canopy",
    "style": "brazilian modernism", "use": "public",
    "inspired_by": "1950s Brazilian park marquees: a free-form concrete slab on slender columns over a promenade - original plan and section",
}

# ---- parameters
X_MIN, X_MAX = -52.0, 52.0
Y_MID, AMP, WAVE_LEN, PHASE = 35.0, 11.0, 78.0, 0.6
HEIGHT0, HEIGHT_AMP, HEIGHT_LEN = 5.0, 0.45, 31.0
N_ARC = 400


def centre_x(x):
    return Y_MID + AMP * math.sin(2 * math.pi * x / WAVE_LEN + PHASE)


def _curve():
    pts = []
    for i in range(N_ARC + 1):
        x = X_MIN + (X_MAX - X_MIN) * i / N_ARC
        pts.append((x, centre_x(x)))
    acc = [0.0]
    for a, b in zip(pts, pts[1:]):
        acc.append(acc[-1] + math.dist(a, b))
    return pts, acc


PTS, ACC = _curve()
LENGTH = ACC[-1]


def at(s):
    """(x, y, tx, ty) of the centreline at arc length s."""
    s = min(max(s, 0.0), LENGTH)
    lo, hi = 0, N_ARC
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if ACC[mid] <= s:
            lo = mid
        else:
            hi = mid
    f = (s - ACC[lo]) / ((ACC[hi] - ACC[lo]) or 1)
    a, b = PTS[lo], PTS[hi]
    dx, dy = b[0] - a[0], b[1] - a[1]
    d = math.hypot(dx, dy)
    return a[0] + dx * f, a[1] + dy * f, dx / d, dy / d


def half_width(s):
    return 5.0 + 1.0 * math.sin(2 * math.pi * s / 37.0 + 2.0) + 0.4 * math.sin(2 * math.pi * s / 17.0)


def z_top(s, t):
    return HEIGHT0 + HEIGHT_AMP * math.sin(2 * math.pi * s / HEIGHT_LEN + 1.0) + 0.3 * math.sin(2 * math.pi * s / 53.0) + 0.55 * t * t


def thick(s, t):
    return 0.30 - 0.11 * t ** 4


def surf(s, t):
    x, y, tx, ty = at(s)
    h = half_width(s)
    return x - ty * t * h, y + tx * t * h


class Fr:
    """A local frame: local (lx, ly) -> world; ey = the local +y direction."""
    def __init__(self, cx, cy, ex_x, ex_y):
        self.c = (cx, cy)
        self.ex = (ex_x, ex_y)
        self.ey = (-ex_y, ex_x)

    def p(self, lx, ly):
        return (self.c[0] + self.ex[0] * lx + self.ey[0] * ly, self.c[1] + self.ex[1] * lx + self.ey[1] * ly)

    def yaw(self, dx, dy):                 # yaw (deg) of a world direction vector, 0 = -y, 90 = +x
        return math.degrees(math.atan2(dx, -dy))

    def yaw_dir(self, lx, ly):
        return self.yaw(self.ex[0] * lx + self.ey[0] * ly, self.ex[1] * lx + self.ey[1] * ly)


def rect(fr, x0, x1, y0, y1):
    return [fr.p(x0, y0), fr.p(x1, y0), fr.p(x1, y1), fr.p(x0, y1)]


def bench(B, s, t, k):
    x, y, tx, ty = at(s)
    h = half_width(s)
    cx, cy = surf(s, t)
    fr = Fr(cx, cy, tx, ty)
    B.slab(rect(fr, -0.9, 0.9, -0.22, 0.22), 0.42, 0.48, "CT_Wood")
    for dx in (-0.75, 0.75):
        B.slab(rect(fr, dx - 0.05, dx + 0.05, -0.2, 0.2), 0.0, 0.42, "CT_ConcreteDark")
    face = -1 if t > 0 else 1                      # sit facing the walk's centre line
    px, py = fr.p(0, 0.5 * (-face))
    B.spot("bench%d" % k, "bench", (px, py, 0.0), fr.yaw_dir(0, face))


KIOSK_MATS = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterBlue"]


def kiosk(B, s, side, k):
    x, y, tx, ty = at(s)
    h = half_width(s)
    off = side * (h - 2.4)
    cx, cy = x - ty * off, y + tx * off
    # local +y = away from the centre line
    ex = (tx, ty) if side < 0 else (-tx, -ty)
    # ey = (-ex_y, ex_x): for side>0 want ey = +n=(-ty,tx): ex=(tx,ty); for side<0 want ey=-n: ex=(-tx,-ty)
    ex = (tx, ty) if side > 0 else (-tx, -ty)
    fr = Fr(cx, cy, ex[0], ex[1])
    hw, hd, H = 2.0, 1.4, 2.9
    mat = KIOSK_MATS[k % len(KIOSK_MATS)]
    corners = [(-hw, -hd), (hw, -hd), (hw, hd), (-hw, hd)]     # CCW; edge0 = front (faces local -y)
    wp = [fr.p(*c) for c in corners]
    holes = [[(0.35, 3.65, 1.0, 2.45)], [], [(1.4, 1.4 + DOOR_W + 0.1, 0.0, 2.25)], []]
    F = {}
    for i in range(4):
        F[i] = B.facade(wp[i], wp[(i + 1) % 4], 0.0, H, mat, holes=holes[i], depth=0.2)
    B.facade_window(F[0], 0.35, 3.65, 1.0, 2.45, kind="public", cols=3, rows=1)
    # floor and roof
    B.slab([fr.p(-hw + 0.1, -hd + 0.1), fr.p(hw - 0.1, -hd + 0.1), fr.p(hw - 0.1, hd - 0.1), fr.p(-hw + 0.1, hd - 0.1)],
           0.0, 0.05, "CT_Floor")
    B.slab(rect(fr, -hw - 0.35, hw + 0.35, -hd - 0.45, hd + 0.35), H, H + 0.22, "CT_ConcreteWhite")
    # shelf outside the hatch
    B.slab(rect(fr, -1.7, 1.7, -hd - 0.45, -hd), 1.0, 1.06, "CT_Wood")
    for u in (-1.5, 1.5):
        B.slab(rect(fr, u - 0.04, u + 0.04, -hd - 0.4, -hd - 0.3), 0.0, 1.0, "CT_Steel")
    # door of the stall-holder (back wall, faces local +y)
    dx, dy = fr.ey
    doorp = fr.p(0.0 + (hw - 1.4 - (DOOR_W + 0.1) / 2), hd)
    B.entrance("kiosk%d" % k, (doorp[0], doorp[1], 0.0), fr.yaw(dx, dy), width=DOOR_W, kind="service")
    ip = fr.p(0, -0.2)
    B.spot("stall%d" % k, "stall", (ip[0], ip[1], 0.05), fr.yaw_dir(0, -1))
    cp = fr.p(0, -hd - 1.3)
    B.spot("counter%d" % k, "counter", (cp[0], cp[1], 0.0), fr.yaw_dir(0, 1))
    kiosk_interior(B, fr, k, random.Random(1960 + k))


KIOSK_TRADE = ["coffee", "icecream", "press", "toys", "snacks"]


def kiosk_interior(B, fr, k, rnd):
    """Furnish a kiosk: shelves with goods on the back and side walls, a counter under the serving window,
    goods on it and a lamp. fr is the kiosk's Fr (local x = ex, local +y = ey)."""
    yk = math.degrees(math.atan2(fr.ex[1], fr.ex[0]))
    z0 = 0.05
    trade = KIOSK_TRADE[(k - 1) % len(KIOSK_TRADE)]
    cx, cy = fr.c
    kf = MFr(cx, cy, yk, z0)                       # u = local x, w = -local y (out of the back wall into the room)
    mats = {"coffee": ["CT_TileWhite", "CT_PlasterRed", "CT_WoodLight", "CT_PlasterYellow"],
            "icecream": ["CT_PlasterPink", "CT_PlasterBlue", "CT_TileWhite", "CT_PlasterYellow"],
            "press": ["CT_PlasterRed", "CT_PlasterBlue", "CT_TileWhite", "CT_PlasterGreen", "CT_PlasterYellow"],
            "toys": ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen", "CT_PlasterPink"],
            "snacks": ["CT_PlasterYellow", "CT_WoodLight", "CT_PlasterRed", "CT_SteelRed", "CT_PlasterGreen"]}[trade]
    kind = {"press": "book", "coffee": "jar", "icecream": "box"}.get(trade, "box")
    # back wall (local y = +1.2): two shelf units either side of the door
    for u0, u1 in ((-1.75, -0.7), (0.7, 1.75)):
        fu.shelving(B, kf.sub(0, -1.2), u0, u1, rnd, depth=0.32, tiers=4, zmax=2.2, z0=0.1, kind=kind, mats=mats)
    # side walls: left faces +local x (u runs +y), right faces -local x (u runs -y)
    fu.shelving(B, kf.sub(-1.8, 0.4, 90), 0.0, 1.1, rnd, depth=0.3, tiers=4, zmax=2.1, z0=0.1, kind="box", mats=mats)
    fu.shelving(B, kf.sub(1.8, -0.7, -90), 0.0, 1.1, rnd, depth=0.3, tiers=4, zmax=2.1, z0=0.1, kind="box", mats=mats)
    # counter under the window (local y -1.2..-0.55, x -1.6..1.6, top 0.95): the stall-holder stands behind it
    cr = MFr(cx, cy, yk, z0)         # u = local x; w = -local y (w > 0 toward the front wall)
    lbox(B.m, "CT_WoodLight", cr, -1.6, 1.6, 0.55, 1.2, 0.0, 0.95)
    lbox(B.detail, "CT_ConcreteDark", cr, -1.6, 1.6, 0.52, 1.22, 0.95, 0.99)
    # goods on the counter, trade by trade
    top = 0.99
    if trade == "coffee":
        for u in (-1.2, -0.6):
            px, py, _ = cr.p(u, 0.85)
            cyl(B.detail, "CT_TileWhite", px, py, top, top + 0.04, 0.2, 8, bottom=True)
            cyl(B.detail, "CT_PlasterYellow", px, py, top + 0.04, top + 0.16, 0.15, 8)
            blob(B.detail, "CT_Glass", px, py, top + 0.2, 0.17, 8)
        lbox(B.detail, "CT_Steel", cr, 0.5, 1.2, 0.7, 1.05, top, top + 0.4, faces="xXyYZ")     # espresso machine
        lbox(B.detail, "CT_SteelRed", cr, 0.55, 1.15, 0.72, 0.8, top + 0.4, top + 0.5, faces="xXyYZ")
        for u in (-0.1, 0.1, 0.3):
            px, py, _ = cr.p(u, 0.8)
            cyl(B.detail, "CT_TileWhite", px, py, top, top + 0.09, 0.04, 6)
    elif trade == "icecream":
        lbox(B.m, "CT_TileWhite", cr, -1.5, 0.1, 0.62, 1.15, top, top + 0.4)           # freezer cabinet with a glass lid
        lbox(B.detail, "CT_Glass", cr, -1.45, 0.05, 0.66, 1.1, top + 0.4, top + 0.46, faces="xXyYZ")
        for u in [-1.35 + 0.18 * i for i in range(8)]:
            lbox(B.detail, rnd.choice(mats), cr, u, u + 0.14, 0.75, 1.0, top + 0.18, top + 0.4, faces="xXyYZ")
        for u in [0.4 + 0.12 * i for i in range(5)]:
            px, py, _ = cr.p(u, 0.9)
            cyl(B.detail, rnd.choice(mats), px, py, top, top + 0.26, 0.04, 5)
    elif trade == "press":
        for u in (-1.3, -0.5, 0.3, 1.0):
            lbox(B.detail, rnd.choice(mats), cr, u, u + 0.45, 0.7, 1.05, top, top + 0.02 + rnd.choice((0.04, 0.08, 0.12)), faces="xXyYZ")
        lbox(B.detail, "CT_Steel", cr, -1.5, -0.7, 1.12, 1.2, top, top + 0.7, faces="xXyYZ")  # a paper rack
        for j in range(4):
            lbox(B.detail, rnd.choice(mats), cr, -1.45 + 0.19 * j, -1.3 + 0.19 * j, 1.0, 1.12, top + 0.15, top + 0.62, faces="xXyYZ")
    elif trade == "toys":
        for j, u in enumerate((-1.2, -0.4, 0.4, 1.1)):
            blob(B.detail, rnd.choice(mats), *cr.p(u, 0.85)[:2], top + 0.2, 0.17, 7)
            lbox(B.detail, rnd.choice(mats), cr, u - 0.14, u + 0.14, 0.7, 1.0, top, top + 0.06, faces="xXyYZ")
    else:
        for u in (-1.2, -0.3, 0.6):
            lbox(B.detail, "CT_WoodLight", cr, u, u + 0.7, 0.65, 1.1, top, top + 0.04, faces="xXyYZ")
            for j in range(4):
                lbox(B.detail, rnd.choice(mats), cr, u + 0.05 + 0.16 * j, u + 0.17 + 0.16 * j, 0.72, 1.0, top + 0.04, top + 0.14, faces="xXyYZ")
    # lamp, a pendant under the roof slab (kiosk H = 2.9)
    lx, ly, _ = kf.p(0.0, -0.1)
    B.lamp((lx, ly, 2.6), lumens=520, colour="#FFD2A0", kind="public", radius=5.5)
    lbox(B.detail, "CT_Steel", MFr(lx, ly), -0.01, 0.01, -0.01, 0.01, 2.68, 2.9, faces="xXyY")


def string_lights(B, rnd):
    """Festoons of bulbs under the canopy slab along two lines (t = +-0.32): lamps (kind lobby) every ~8 m, and a
    sagging cable with small CT_Neon beads between the supports every 3.5 m."""
    for li, t in enumerate((0.32, -0.32)):
        s = 3.0 + 4.0 * li
        k = 0
        while s < LENGTH - 3.0:
            x, y = surf(s, t)
            zu = z_top(s, t) - thick(s, t) - 0.04
            pts = []
            # a catenary from here to the next support (3.5 m along s)
            n_seg = 6
            ds = 3.5
            for i in range(n_seg + 1):
                ss = s + ds * i / n_seg
                if ss > LENGTH - 2.0:
                    break
                px, py = surf(ss, t)
                sag = 0.34 * 4 * (i / n_seg) * (1 - i / n_seg)
                pts.append((px, py, z_top(ss, t) - thick(ss, t) - 0.06 - sag))
            for a, b in zip(pts, pts[1:]):
                fu.rod(B.detail, "CT_Steel", a, b, 0.012, caps=False)
            for (px, py, pz) in pts[1:-1] if len(pts) > 2 else []:
                blob(B.detail, "CT_Opal", px, py, pz - 0.06, 0.05, 5)
            if k % 2 == 0 and pts:
                px, py, pz = pts[len(pts) // 2]
                B.lamp((px, py, pz - 0.12), lumens=170, colour="#FFD9A0", kind="lobby", radius=6.0, fixture=False)
                blob(B.detail, "CT_Opal", px, py, pz - 0.12, 0.09, 6)
            s += ds
            k += 1


def build(B):
    # canopy slab
    def f(u, v):
        s, t = u * LENGTH, 2 * v - 1
        x, y = surf(s, t)
        return (x, y, z_top(s, t))
    nu = int(LENGTH)
    thick_sheet(B.m, lambda u, v: f(u, v), nu, 10, lambda u, v: thick(u * LENGTH, 2 * v - 1),
                "CT_ConcreteWhite", "CT_ConcreteWhite", "CT_ConcreteWhite")
    # paving ribbon under it
    def fl(u, v):
        s, t = u * LENGTH, 2 * v - 1
        x, y = surf(s, t * 1.0)
        tx, ty = at(s)[2:]
        w = half_width(s) + 1.4
        xc, yc = at(s)[:2]
        return (xc - ty * t * w, yc + tx * t * w, 0.0)
    thick_sheet(B.m, fl, nu // 2, 8, lambda u, v: 0.3, "CT_Paving", "CT_ConcreteDark", "CT_ConcreteDark")
    # kiosks first (columns avoid them)
    kiosks = [(20.0, 1), (46.0, -1), (66.0, 1), (92.0, -1), (108.0, 1)]
    for k, (s, side) in enumerate(kiosks, 1):
        kiosk(B, s, side, k)
    # columns
    n = 0
    s = 4.0
    i = 0
    while s < LENGTH - 2.0:
        for side in (1, -1):
            if (i + (side > 0)) % 3 == 2 and False:
                continue
            ss = s + (3.5 if side < 0 else 0)
            if ss > LENGTH - 2:
                continue
            t = 0.62 * side
            if any(abs(ss - ks) < 4.2 and ksd == side for ks, ksd in kiosks):
                continue
            x, y = surf(ss, t)
            zu = z_top(ss, t) - thick(ss, t)
            B.column_round(x, y, 0.0, zu + 0.05, 0.14, "CT_ConcreteWhite", segs=10)
            n += 1
        s += 7.0
        i += 1
    string_lights(B, None)
    # benches
    k = 0
    for ss, t in [(12, -0.35), (30, 0.4), (38, -0.4), (56, 0.35), (60, -0.35), (79, -0.4), (84, 0.4), (100, 0.35), (116, -0.4), (8, 0.45)]:
        if ss < LENGTH - 3 and not any(abs(ss - ks) < 4.0 and ksd * t > 0 for ks, ksd in kiosks):
            k += 1
            bench(B, ss, t, k)
    # footprint: the canopy outline
    left = [surf(LENGTH * i / 60, 1.0) for i in range(61)]
    right = [surf(LENGTH * i / 60, -1.0) for i in range(61)]
    B.footprint = right + left[::-1]
    B.height = max(z_top(LENGTH * i / 200, 1.0) for i in range(201)) + 0.2
    B.note("canopy %.0f m long (arc), %d columns, %d kiosks, %d benches" % (LENGTH, n, len(kiosks), k))
