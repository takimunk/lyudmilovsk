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

from citykit import box, DOOR_W, geom
import interior_kit as kit
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


def _w3(fr, lx, ly, z):
    x, y = fr.p(lx, ly)
    return (x, y, z)


def _face_dir(m, mat, pts, want):
    """A quad whose normal points along `want` (a 3-vector)."""
    n = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[3], pts[0]))
    if sum(n[i] * want[i] for i in range(3)) < 0:
        pts = pts[::-1]
    m.face(mat, pts)


def awning(B, fr, mats, y0=-1.42, reach=0.95, z_top=2.52, z_out=2.18, hw=2.05, n=10, drop=0.17):
    """A striped shop awning over the serving hatch: slanted strips, a valance at the front edge, plain underside."""
    d = B.detail
    for i in range(n):
        x0 = -hw + 2 * hw * i / n
        x1 = x0 + 2 * hw / n
        mat = mats[i % 2]
        top = [_w3(fr, x0, y0, z_top), _w3(fr, x1, y0, z_top), _w3(fr, x1, y0 - reach, z_out), _w3(fr, x0, y0 - reach, z_out)]
        _face_dir(d, mat, top, (0.0, 0.0, 1.0))
        under = [(p[0], p[1], p[2] - 0.012) for p in top]
        _face_dir(d, "CT_PlasterGrey", under, (0.0, 0.0, -1.0))
        fy = y0 - reach
        val = [_w3(fr, x0, fy, z_out), _w3(fr, x1, fy, z_out), _w3(fr, x1, fy, z_out - drop), _w3(fr, x0, fy, z_out - drop)]
        out = fr.p(0, fy - 1.0)
        c0 = fr.p(0, fy)
        _face_dir(d, mat, val, (out[0] - c0[0], out[1] - c0[1], 0.0))
        val_b = [(p[0], p[1], p[2]) for p in val]
        out_in = (c0[0] - out[0], c0[1] - out[1], 0.0)
        _face_dir(d, "CT_PlasterGrey", [(p[0] - 0.004 * (out[0] - c0[0]), p[1] - 0.004 * (out[1] - c0[1]), p[2]) for p in val_b], out_in)
    # two thin tie rods from the roof slab to the awning front corners
    for sx in (-hw + 0.05, hw - 0.05):
        a = _w3(fr, sx, y0 - reach + 0.02, z_out - 0.02)
        b = _w3(fr, sx, y0 - 0.02, z_top + 0.35)
        fu.rod(d, "CT_Steel", a, b, 0.012, caps=False)


def bench(B, s, t, k):
    x, y, tx, ty = at(s)
    h = half_width(s)
    cx, cy = surf(s, t)
    fr = Fr(cx, cy, tx, ty)
    face = -1 if t > 0 else 1                      # sit facing the walk's centre line
    yaw = fr.yaw_dir(0, face)
    bn = kit.bench(B, cx, cy, 0.0, yaw, w=1.6, kind="park")
    px, py = fr.p(0, 0.5 * (-face))
    B.spot("bench%d" % k, "bench", (px, py, 0.0), yaw)
    B.thing("fair_bench_%d" % k, "seat", (cx, cy, 0.45), yaw, height=0.45)
    # a litter bin beside every second bench
    if k % 2 == 0:
        bx, by = fr.p(1.15, 0.0)
        cyl(B.detail, "CT_SteelRed", bx, by, 0.0, 0.62, 0.17, 8, top=False)
        cyl(B.detail, "CT_ConcreteDark", bx, by, 0.62, 0.65, 0.19, 8)


KIOSK_MATS = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterBlue"]
KIOSK_TRADE = ["coffee", "icecream", "press", "toys", "snacks"]
KIOSK_SIGN = {"coffee": "sign_buffet", "icecream": "sign_milk", "press": "sign_books", "toys": "sign_toys",
              "snacks": "sign_bread"}


def kiosk(B, s, side, k):
    x, y, tx, ty = at(s)
    h = half_width(s)
    off = side * (h - 2.4)
    cx, cy = x - ty * off, y + tx * off
    # local +y = away from the centre line
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
    # the serving hatch is left open (no glass): the stall-holder and the goods show from the walk
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
    kiosk_outside(B, fr, k, mat)


def kiosk_outside(B, fr, k, mat):
    """The street side of a kiosk: sign over the hatch, striped awning, goods on the shelf, a board on the side wall."""
    trade = KIOSK_TRADE[(k - 1) % len(KIOSK_TRADE)]
    front = fr.yaw_dir(0, -1)
    sx, sy = fr.p(0, -1.4)
    kit.sign_plate(B, sx, sy, 0.0, front, name=KIOSK_SIGN[trade], w=1.9, zc=2.69)
    awning(B, fr, (mat, "CT_TileWhite"))
    top = 1.06
    # goods on the outside shelf (z = 1.06), each trade its own
    def at_(lx, ly=-1.62):
        return fr.p(lx, ly)
    rnd = random.Random(300 + k)
    if trade == "coffee":
        kit.glass(B, *at_(-1.3), top)
        for lx in (-0.9, -0.5, 0.45, 0.85):
            kit.cup(B, *at_(lx), top, yaw=rnd.uniform(0, 90))
        kit.teapot(B, *at_(1.3), top, yaw=front)
        kit.plates(B, *at_(0.0, -1.62), top, n=3)
    elif trade == "icecream":
        for lx in (-1.2, -0.7, -0.2):
            kit.glass(B, *at_(lx), top)
        kit.cup(B, *at_(0.5), top, mat="CT_PlasterPink")
        kit.cup(B, *at_(0.9), top, mat="CT_PlasterYellow")
        kit.bottles(B, *at_(1.35), top, n=2, seed=k)
    elif trade == "press":
        kit.newspaper(B, *at_(-1.1), top, yaw=front + 8, seed=1)
        kit.newspaper(B, *at_(-0.4), top, yaw=front - 6, seed=2)
        kit.books_stack(B, *at_(0.5), top, yaw=front, n=4, seed=3)
        kit.newspaper(B, *at_(1.1), top, yaw=front + 12, seed=4)
        B.thing("press_paper", "take", (*at_(-0.75), top + 0.03), front, item="daily_paper")
    elif trade == "toys":
        kit.globe(B, *at_(-1.2), top, yaw=front)
        kit.books_stack(B, *at_(-0.2), top, yaw=front + 90, n=3, seed=5)
        kit.vase(B, *at_(0.7), top, seed=6)
        kit.cup(B, *at_(1.25), top, mat="CT_PlasterBlue")
    else:
        kit.crate(B, *at_(-1.15), top, yaw=front, fill="CT_PlasterRed", seed=1)
        kit.crate(B, *at_(-0.5), top, yaw=front, fill="CT_PlasterYellow", seed=2)
        kit.kitchen_scales(B, *at_(0.5), top, yaw=front)
        kit.sack(B, *at_(1.2), top)
    # menu board on the side wall facing out along the walk, a clock over the door side
    side_ = 1 if k % 2 else -1
    mx, my = fr.p(side_ * 2.0, -0.35)
    kit.menu_board(B, mx, my, 0.0, fr.yaw_dir(side_, 0), name=("menu_0", "price_0", "menu_1", "price_1", "menu_0")[k - 1],
                   w=0.62, zc=1.65)
    B.thing("kiosk%d_menu" % k, "board", (mx, my, 1.65), fr.yaw_dir(side_, 0), topic="prices")
    # outdoor extras by trade
    if trade == "coffee":
        for sgn in (-1, 1):
            tx_, ty_ = fr.p(sgn * 2.6, -3.1)
            tt = kit.table_round(B, tx_, ty_, 0.0, r=0.36, h=1.0)
            kit.cup(B, tx_, ty_, tt["top"], yaw=sgn * 40)
            for a in (0.0, 180.0):
                ox, oy = fr.p(sgn * 2.6 + (0.6 if a == 0 else -0.6), -3.1)
                st = kit.bar_stool(B, ox, oy, 0.0, 0.0, h=0.72)
            B.thing("coffee_stool_%d" % (sgn + 2), "seat", (*fr.p(sgn * 2.6 + 0.6, -3.1), 0.72), front, height=0.72)
    elif trade == "press":
        for sgn in (-1, 1):
            rx, ry = fr.p(sgn * 2.0 + (-sgn) * 0.0, -0.6)
        mxr, myr = fr.p(-side_ * 2.0, -0.7)
        kit.magazine_rack(B, mxr, myr, 0.0, fr.yaw_dir(-side_, 0), w=0.9, seed=k)
        B.thing("press_rack", "shelf", (mxr, myr, 1.2), fr.yaw_dir(-side_, 0), topic="travel")
    elif trade == "toys":
        dc = kit.display_case(B, *fr.p(-side_ * 2.7, -0.6), 0.0, fr.yaw_dir(-side_, 0), w=1.0, d=0.5, goods="toys", seed=k)
    elif trade == "snacks":
        kit.crate(B, *fr.p(2.55, -0.6), 0.0, fr.yaw_dir(1, 0), fill="CT_PlasterGreen", seed=7)
        kit.crate(B, *fr.p(2.55, -1.2), 0.0, fr.yaw_dir(1, 0), fill="CT_PlasterYellow", seed=8)
        kit.cardboard_box(B, *fr.p(2.55, -0.6), 0.38, fr.yaw_dir(1, 0), seed=9)
    # the hatch counter thing: where the vendor serves
    hx, hy = fr.p(0, -1.55)
    B.thing("kiosk%d_counter" % k, "counter", (hx, hy, 1.1), front, role="vendor", service="shop", trade=trade)


def kiosk_interior(B, fr, k, rnd):
    """Furnish a kiosk for its trade (floor z 0.05, inner space x +-1.8, y -1.2..1.2; hatch toward local -y, door in the
    back wall near x 0): a counter under the hatch, shelving on the back and side walls, a rug and a stool."""
    z0 = 0.05
    trade = KIOSK_TRADE[(k - 1) % len(KIOSK_TRADE)]
    toward_hatch = fr.yaw_dir(0, -1)
    into_room = fr.yaw_dir(0, 1)
    goods = {"coffee": "jars", "icecream": "boxes", "press": None, "toys": "toys", "snacks": "groceries"}[trade]
    # counter under the hatch (customer side toward the hatch)
    cx_, cy_ = fr.p(0, -0.88)
    cn = kit.shop_counter(B, cx_, cy_, z0, toward_hatch, w=3.4, d=0.65, front=KIOSK_MATS[k % 5], till=False)
    top = cn["top"]
    stand_x, stand_y = fr.p(-0.3, 0.05)
    # trade-specific counter dressing (z = counter top)
    if trade == "coffee":
        kit.samovar(B, *fr.p(-1.2, -0.85), top, yaw=toward_hatch)
        kit.teapot(B, *fr.p(-0.55, -0.85), top, yaw=toward_hatch)
        kit.cup(B, *fr.p(0.1, -0.95), top)
        kit.cup(B, *fr.p(0.35, -0.8), top, mat="CT_PlasterRed")
        kit.plates(B, *fr.p(0.95, -0.9), top, n=3)
        kit.cash_register(B, *fr.p(1.35, -0.8), top, toward_hatch + 180)
    elif trade == "icecream":
        for lx in (-1.3, -0.9, -0.5):
            kit.glass(B, *fr.p(lx, -0.9), top)
        kit.cup(B, *fr.p(0.1, -0.9), top, mat="CT_PlasterPink")
        kit.cup(B, *fr.p(0.5, -0.8), top, mat="CT_PlasterYellow")
        kit.cash_register(B, *fr.p(1.3, -0.8), top, toward_hatch + 180)
    elif trade == "press":
        for j, lx in enumerate((-1.3, -0.85, -0.4, 0.15)):
            kit.newspaper(B, *fr.p(lx, -0.85 + 0.03 * j), top, yaw=toward_hatch + 8 * j, seed=j)
        kit.books_stack(B, *fr.p(0.8, -0.9), top, yaw=toward_hatch, n=3, seed=k)
        kit.cash_register(B, *fr.p(1.35, -0.8), top, toward_hatch + 180)
    elif trade == "toys":
        kit.globe(B, *fr.p(-1.2, -0.85), top)
        kit.vase(B, *fr.p(-0.4, -0.85), top, seed=2)
        kit.books_stack(B, *fr.p(0.4, -0.9), top, yaw=toward_hatch + 20, n=3, seed=2)
        kit.cash_register(B, *fr.p(1.3, -0.8), top, toward_hatch + 180)
    else:
        kit.kitchen_scales(B, *fr.p(-1.1, -0.85), top, yaw=toward_hatch)
        kit.bottles(B, *fr.p(-0.4, -0.9), top, n=3, seed=k)
        kit.crate(B, *fr.p(0.5, -0.9), top, yaw=toward_hatch, fill="CT_PlasterRed", seed=k)
        kit.cash_register(B, *fr.p(1.35, -0.8), top, toward_hatch + 180)
    # shelving: back wall either side of the door, both side walls
    for lx in (-1.2, 1.2):
        bx, by = fr.p(lx, 1.2)
        if trade == "press":
            kit.bookshelf(B, bx, by, z0, toward_hatch, w=1.1, h=2.0, seed=lx, topic="library")
        else:
            kit.shop_shelf(B, bx, by, z0, toward_hatch, w=1.1, h=2.0, goods=goods, seed=k + int(lx * 5))
    for sgn in (-1, 1):
        bx, by = fr.p(sgn * 1.8, 0.1)
        yaw_in = fr.yaw_dir(-sgn, 0)
        if trade == "icecream" and sgn < 0:
            kit.kitchen_unit(B, bx, by, z0, yaw_in, modules="fridge,fridge", wall_cupboards=False)
        elif trade == "press":
            kit.magazine_rack(B, bx, by, z0, yaw_in, w=0.8, seed=k + sgn)
        elif trade == "toys" and sgn > 0:
            kit.display_case(B, bx - 0.0, by, z0, yaw_in, w=1.0, d=0.45, goods="toys", seed=k)
        else:
            kit.shop_shelf(B, bx, by, z0, yaw_in, w=0.9, h=1.9, goods=goods or "boxes", seed=k + 9 * sgn)
    # a rug and a stool for the stall-holder, a clock and a poster
    rx, ry = fr.p(0.2, 0.15)
    kit.rug(B, rx, ry, z0, toward_hatch, w=1.1, d=1.5, seed=k)
    kit.stool(B, *fr.p(-0.95, -0.05), z0, toward_hatch, mat="CT_PlasterBlue", h=0.55)
    kx, ky = fr.p(0.0, -1.2)
    kit.wall_clock(B, *fr.p(1.2, 1.2), z0, toward_hatch, zc=2.35)
    kit.poster(B, *fr.p(-1.2, 1.2), z0, toward_hatch, name=("poster_sun", "kids", "poster_cons", "kids", "poster_birds")[k - 1],
               w=0.5, zc=2.5)
    # lamp, a pendant under the roof slab (kiosk H = 2.9)
    lx, ly, _ = MFr(fr.c[0], fr.c[1], math.degrees(math.atan2(fr.ex[1], fr.ex[0])), z0).p(0.0, -0.1)
    B.lamp((lx, ly, 2.6), lumens=520, colour="#FFD2A0", kind="public", radius=5.5)
    lbox(B.detail, "CT_Steel", MFr(lx, ly), -0.01, 0.01, -0.01, 0.01, 2.68, 2.9, faces="xXyY")


def fair_board(B, s, t, tid, topic, posters=False):
    """A free-standing board on two wooden posts beside the walk; faces the centre line, both sides dressed."""
    cx, cy = surf(s, t)
    x, y, tx, ty = at(s)
    fr = Fr(cx, cy, tx, ty)
    face = -1 if t > 0 else 1
    yaw = fr.yaw_dir(0, face)
    for u in (-0.95, 0.95):
        B.slab(rect(fr, u - 0.06, u + 0.06, -0.06, 0.06), 0.0, 2.5, "CT_Wood")
    B.slab(rect(fr, -1.1, 1.1, -0.04, 0.04), 0.95, 2.4, "CT_WoodLight")
    B.slab(rect(fr, -1.15, 1.15, -0.07, 0.07), 2.4, 2.46, "CT_Wood")
    fx, fy = fr.p(0, face * 0.04)
    bx, by = fr.p(0, -face * 0.04)
    if posters:
        for u, nm, w_ in ((-0.55, "cinema", 0.62), (0.15, "poster_sun", 0.6), (0.78, "travel", 0.5)):
            px, py = fr.p(u, face * 0.04)
            kit.poster(B, px, py, 0.0, yaw, name=nm, w=w_, zc=1.62, seed=int(u * 10))
        kit.sign_plate(B, fx, fy, 0.0, yaw, name="sign_cinema", w=1.0, zc=2.2)
        kit.notice_board(B, bx, by, 0.0, yaw + 180, w=1.7, zc=1.65, seed=5)
    else:
        kit.notice_board(B, fx, fy, 0.0, yaw, w=1.7, zc=1.65, seed=3)
        kit.poster(B, bx, by, 0.0, yaw + 180, name="poster_cons", w=0.6, zc=1.65, seed=2)
        kit.poster(B, *fr.p(0.65, -face * 0.04), 0.0, yaw + 180, name="kids", w=0.5, zc=1.65, seed=3)
    B.thing(tid, "board", (fx, fy, 1.65), yaw, topic=topic)


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
    fair_board(B, 52.0, -0.5, "fair_board", "fair")
    fair_board(B, 100.0, 0.5, "fair_programme", "cinema", posters=True)
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
    for m_ in kit.check(B):
        print("KIT:", m_)
    B.note("canopy %.0f m long (arc), %d columns, %d kiosks, %d benches" % (LENGTH, n, len(kiosks), k))
