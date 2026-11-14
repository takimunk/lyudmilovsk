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


# Atlas goods labels are label_NN = GOODS[NN % 30] (tools/city/atlas.py): the tins on a stall's shelf carry what that
# trade really sells (the kit's shop_shelf picks a label at random: a coffee kiosk with SPRATS on its shelf).
LABELS = {
    "coffee": (5, 6, 9, 11, 29, 10, 13, 14),            # TEA COFFEE SUGAR HONEY COCOA JAM BISCUITS SWEETS
    "icecream": (23, 24, 4, 21, 22, 9, 29, 14),         # MILK KEFIR CONDENSED MILK LEMONADE KVASS SUGAR COCOA SWEETS
    "snacks": (25, 26, 12, 8, 27, 28, 2, 3, 0, 1, 13),  # PICKLES MUSHROOMS GROATS SALT PEPPER CAVIAR PEAS TOMATOES SPRATS SPRATLETS BISCUITS
    "toys": (14, 13, 10),                               # SWEETS BISCUITS JAM (the toys themselves are plain painted)
}
TOY_MATS = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterPink", "CT_PlasterGreen", "CT_TileWhite"]


def trade_shelf(B, x, y, yaw, w=1.1, h=2.0, d=0.34, trade="coffee", seed=0, z0=0.05, levels=4):
    """A light wooden shelf unit [wall] standing out of the wall toward `yaw`, goods by trade: tins and jars whose labels
    are the trade's own (LABELS), or painted toys. About a third of the kit's shop_shelf in triangles."""
    d_ = B.detail
    a_ = math.radians(yaw)
    F = MFr(x + d * math.sin(a_), y - d * math.cos(a_), yaw, z0)           # (x, y) is on the wall: the unit stands out from it
    rnd = random.Random(seed * 31 + len(trade))
    lbox(d_, "CT_WoodLight", F, -w / 2, w / 2, -d, -d + 0.02, 0.0, h)
    for sg in (-1, 1):
        u0 = sg * w / 2
        lbox(d_, "CT_Wood", F, min(u0, u0 - sg * 0.03), max(u0, u0 - sg * 0.03), -d, -0.01, 0.0, h)
    labs = LABELS[trade]
    pitch = (h - 0.28) / levels
    for k in range(levels):
        zb = 0.22 + pitch * k
        lbox(d_, "CT_Wood", F, -w / 2, w / 2, -d, -0.01, zb - 0.025, zb)
        u = -w / 2 + 0.07
        while u < w / 2 - 0.1:
            room = pitch - 0.04
            if trade == "toys":
                if rnd.random() < 0.55:
                    r = rnd.uniform(0.035, 0.06)
                    px, py, _ = F.p(u + r, -0.17)
                    blob(d_, rnd.choice(TOY_MATS), px, py, F.z + zb + r, r, 5)
                    u += 2 * r + 0.035
                else:
                    ww = rnd.uniform(0.07, 0.12)
                    hh = min(room, rnd.uniform(0.07, 0.15))
                    lbox(d_, rnd.choice(TOY_MATS), F, u, u + ww, -0.2, -0.1, zb, zb + hh)
                    lbox(d_, "CT_TileWhite", F, u + 0.01, u + ww - 0.01, -0.1, -0.098, zb + 0.015, zb + hh * 0.5)
                    u += ww + 0.03
            else:
                ww = rnd.uniform(0.09, 0.125)
                hh = min(room, rnd.uniform(0.13, 0.2))
                if u + ww > w / 2 - 0.05:
                    break
                lbox(d_, "CT_TileWhite", F, u, u + ww, -0.2, -0.1, zb, zb + hh)
                px, py, _ = F.p(u + ww / 2, -0.1)
                nn = labs[rnd.randrange(len(labs))] + 30 * rnd.randrange(2)
                kit.poster(B, px, py, F.z, yaw, name="label_%02d" % nn, w=ww * 0.95, h=hh * 0.85, zc=zb + hh / 2)
                u += ww + 0.02


def toy_case(B, x, y, yaw, w=1.0, d=0.5, seed=0, z0=0.0):
    """A counter-high open case of plain painted toys (the kit's display_case puts random goods labels on toy boxes)."""
    d_ = B.detail
    F = MFr(x, y, yaw, z0)
    rnd = random.Random(seed + 77)
    lbox(B.m, "CT_Wood", F, -w / 2, w / 2, -d / 2, d / 2, 0.0, 0.78)
    lbox(d_, "CT_WoodLight", F, -w / 2 - 0.02, w / 2 + 0.02, -d / 2 - 0.02, d / 2 + 0.02, 0.78, 0.82)
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        lbox(d_, "CT_Wood", F, sx * (w / 2 - 0.02) - 0.015, sx * (w / 2 - 0.02) + 0.015, sy * (d / 2 - 0.02) - 0.015,
             sy * (d / 2 - 0.02) + 0.015, 0.82, 1.3)
    lbox(d_, "CT_WoodLight", F, -w / 2, w / 2, -d / 2, d / 2, 1.28, 1.3)
    lbox(d_, "CT_WoodLight", F, -w / 2 + 0.02, w / 2 - 0.02, -d / 2 + 0.03, d / 2 - 0.03, 1.04, 1.06)
    for zb in (0.82, 1.06):
        u = -w / 2 + 0.08
        while u < w / 2 - 0.1:
            r = rnd.uniform(0.04, 0.065)
            px, py, pz = F.p(u + r, rnd.uniform(-0.1, 0.1), zb)
            if rnd.random() < 0.5:
                blob(d_, rnd.choice(TOY_MATS), px, py, pz + r, r, 5)
            else:
                lbox(d_, rnd.choice(TOY_MATS), F, u, u + 2 * r, -r, r, zb, zb + 1.6 * r)
            u += 2 * r + 0.05


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
        B.thing("ring_toss", "play", (*at_(1.35), top + 0.15), front, what="ring_toss")       # rings over the bottles
    elif trade == "press":
        kit.newspaper(B, *at_(-1.1), top, yaw=front + 8, seed=1)
        kit.newspaper(B, *at_(-0.4), top, yaw=front - 6, seed=2)
        kit.books_stack(B, *at_(0.5), top, yaw=front, n=4, seed=3)
        B.thing("lottery_tickets", "play", (*at_(0.5), top + 0.1), front, what="lottery")      # the lottery tickets
        kit.newspaper(B, *at_(1.1), top, yaw=front + 12, seed=4)
        B.thing("press_paper", "take", (*at_(-0.75), top + 0.03), front, item="daily_paper")
    elif trade == "toys":
        kit.globe(B, *at_(-1.2), top, yaw=front)
        B.thing("fortune_globe", "machine", (*at_(-1.2), top + 0.2), front, what="fortune")    # the fortune-telling globe
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
        toy_case(B, *fr.p(-side_ * 2.7, -0.6), fr.yaw_dir(-side_, 0), w=1.0, d=0.5, seed=k)
        B.thing("shooting_gallery", "play", (*fr.p(-side_ * 2.7, -0.6), 0.9), fr.yaw_dir(-side_, 0),
                what="shooting_gallery")                                                        # the prizes' case
    elif trade == "snacks":
        kit.crate(B, *fr.p(2.55, -0.6), 0.0, fr.yaw_dir(1, 0), fill="CT_PlasterGreen", seed=7)
        kit.crate(B, *fr.p(2.55, -1.2), 0.0, fr.yaw_dir(1, 0), fill="CT_PlasterYellow", seed=8)
        kit.cardboard_box(B, *fr.p(2.55, -0.6), 0.38, fr.yaw_dir(1, 0), seed=9)
        B.thing("strongman", "sport", (*fr.p(2.55, -0.9), 0.6), fr.yaw_dir(1, 0), what="strongman")  # by the crates
    kiosk_extras(B, fr, k, trade, side_, front)
    # the hatch counter thing: where the vendor serves
    hx, hy = fr.p(0, -1.55)
    B.thing("kiosk%d_counter" % k, "counter", (hx, hy, 1.1), front, role="vendor", service="shop", trade=trade)


def kiosk_extras(B, fr, k, trade, side_, front):
    """Second pass: one more thing by each kiosk's side that tells its trade (all in B.detail, none on the walk's line)."""
    d = B.detail
    K = MFr(fr.c[0], fr.c[1], front)               # K.p(u, w): u = fr's lx, w toward the street front
    if trade == "coffee":                          # a milk churn and a crate of bottles, a sack of coffee beans
        x, y, _ = K.p(-side_ * 2.5, 0.7)
        cyl(d, "CT_Aluminium", x, y, 0.0, 0.58, 0.16, 8, top=False)
        cyl(d, "CT_Aluminium", x, y, 0.58, 0.66, 0.11, 8)
        blob(d, "CT_Aluminium", x, y, 0.69, 0.05, 5)
        kit.crate(B, *K.p(-side_ * 2.5, 1.25)[:2], 0.0, front + 90, w=0.5, d=0.34, h=0.3, fill="CT_PlasterBlue", seed=k)
        kit.sack(B, *K.p(-side_ * 2.5, 0.15)[:2], 0.0)
    elif trade == "icecream":                      # the freezer chest on the pavement, a cone on a stick over the hatch
        kit.ice_cream_chest(B, *K.p(-side_ * 2.75, 0.5)[:2], 0.0, K.yaw + 90 * (-side_))
        x, y, _ = K.p(-side_ * 1.55, 1.2)
        _bar(d, "CT_Steel", K, (-side_ * 1.55, 1.2, 1.06), (-side_ * 1.55, 1.2, 2.3), 0.012)
        for j in range(6):                         # a cone: stripes of wafer and a ball of pink on top
            a0, a1 = 2 * math.pi * j / 6, 2 * math.pi * (j + 1) / 6
            pa = K.p(-side_ * 1.55 + 0.1 * math.cos(a0), 1.2 + 0.1 * math.sin(a0), 2.3)
            pb = K.p(-side_ * 1.55 + 0.1 * math.cos(a1), 1.2 + 0.1 * math.sin(a1), 2.3)
            d.face("CT_PlasterYellow", [pa, pb, K.p(-side_ * 1.55, 1.2, 2.12)])
            d.face("CT_PlasterYellow", [pb, pa, K.p(-side_ * 1.55, 1.2, 2.12)])
        blob(d, "CT_PlasterPink", x, y, 2.38, 0.11, 6)
    elif trade == "press":                         # a postcard spinner and newspapers pegged on a wire
        x, y, _ = K.p(side_ * 2.6, 1.4)
        cyl(d, "CT_Steel", x, y, 0.0, 1.35, 0.015, 5, top=False)
        cyl(d, "CT_Steel", x, y, 0.0, 0.03, 0.17, 8)
        for j in range(8):
            a = 2 * math.pi * j / 8
            px, py, _ = K.p(side_ * 2.6 + 0.1 * math.cos(a), 1.4 + 0.1 * math.sin(a))
            kit.poster(B, px, py, 0.0, K.yaw + 90 - math.degrees(a), name="travel", w=0.12, h=0.17, zc=0.95 + 0.2 * (j % 2), seed=j + k)
        blob(d, "CT_Brass", x, y, 1.38, 0.04, 4)
        fu.rod(d, "CT_Steel", K.p(side_ * 1.45, 1.75, 1.9), K.p(side_ * 2.4, 1.75, 1.9), 0.008, caps=False)
        for j in range(4):
            lbox(d, "CT_Paper", K, side_ * (1.55 + 0.22 * j) - 0.07, side_ * (1.55 + 0.22 * j) + 0.07, 1.74, 1.76, 1.5, 1.88)
    elif trade == "toys":                          # a bunch of balloons tied to the shelf and a bucket of pinwheels
        base = K.p(-side_ * 2.2, 1.72, 1.06)
        for j in range(7):
            bx = -side_ * 2.2 + 0.16 * math.cos(j * 2.4)
            by = 1.72 + 0.16 * math.sin(j * 2.4)
            bz = 1.95 + 0.11 * (j % 3) + 0.05 * j / 7
            fu.rod(d, "CT_Steel", base, K.p(bx, by, bz - 0.14), 0.004, caps=False)
            blob(d, TOY_MATS[j % 5], *K.p(bx, by)[:2], bz, 0.12, 6)
        x, y, _ = K.p(side_ * 2.5, 1.7)
        cyl(d, "CT_SteelRed", x, y, 0.0, 0.34, 0.13, 8, top=False)
        for j in range(4):
            sx, sy, _ = K.p(side_ * 2.5 + 0.06 * (j - 1.5), 1.7 + 0.04 * (j % 2))
            _bar(d, "CT_Wood", K, (side_ * 2.5 + 0.06 * (j - 1.5), 1.7 + 0.04 * (j % 2), 0.3), (side_ * 2.5 + 0.06 * (j - 1.5), 1.7 + 0.04 * (j % 2), 0.78 + 0.05 * j), 0.012)
            for a in range(4):
                ang = math.pi / 2 * a + 0.4 * j
                _tri(d, TOY_MATS[(a + j) % 4], K.p(side_ * 2.5 + 0.06 * (j - 1.5), 1.7 + 0.04 * (j % 2), 0.78 + 0.05 * j),
                     K.p(side_ * 2.5 + 0.06 * (j - 1.5) + 0.07 * math.cos(ang), 1.7 + 0.04 * (j % 2) + 0.07 * math.sin(ang), 0.84 + 0.05 * j),
                     K.p(side_ * 2.5 + 0.06 * (j - 1.5) + 0.07 * math.cos(ang + 1.0), 1.7 + 0.04 * (j % 2) + 0.07 * math.sin(ang + 1.0), 0.84 + 0.05 * j))
    else:                                          # the corn steamer on its stand, a sack of seeds
        kit.dumpling_pot(B, *K.p(-side_ * 2.6, 0.6)[:2], 0.0, K.yaw + 90 * (-side_), steamers=2, lid=False)
        kit.sack(B, *K.p(-side_ * 2.5, 1.5)[:2], 0.0)


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
            trade_shelf(B, bx, by, toward_hatch, w=1.1, h=2.0, trade=trade, seed=k + int(lx * 5), z0=z0)
    for sgn in (-1, 1):
        bx, by = fr.p(sgn * 1.8, 0.1)
        yaw_in = fr.yaw_dir(-sgn, 0)
        if trade == "icecream" and sgn < 0:
            kit.kitchen_unit(B, bx, by, z0, yaw_in, modules="fridge,fridge", wall_cupboards=False)
        elif trade == "press":
            kit.magazine_rack(B, bx, by, z0, yaw_in, w=0.8, seed=k + sgn)
        elif trade == "toys" and sgn > 0:
            toy_case(B, bx, by, yaw_in, w=1.0, d=0.45, seed=k, z0=z0)
        else:
            trade_shelf(B, bx, by, yaw_in, w=0.9, h=1.9, trade=trade, seed=k + 9 * sgn, z0=z0)
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
        for u, nm, w_ in ((-0.55, "cinema", 0.62), (0.08, "chart_timetable_cable", 0.55), (0.65, "travel", 0.45)):
            px, py = fr.p(u, face * 0.04)
            kit.poster(B, px, py, 0.0, yaw, name=nm, w=w_, zc=1.62, seed=int(u * 10))
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
                # one real light per three big bulbs (the game lights the nearest 24 lamps of a building): wider, brighter
                if k % 6 == 0:
                    B.lamp((px, py, pz - 0.12), lumens=480, colour="#FFD9A0", kind="lobby", radius=11.0, fixture=False)
                blob(B.detail, "CT_Opal", px, py, pz - 0.12, 0.09, 6)
            s += ds
            k += 1


# ======================================================================================================== fair games
FLAGS = ["CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterPink", "CT_PlasterGreen", "CT_TileWhite"]
LAMP_GAMES = []


def _gframe(s, t, face):
    """A game's frame: gf.p(u, w, z), u along the walk, w toward the customers (face +1: toward the walk's +t side)."""
    cx, cy = surf(s, t)
    x, y, tx, ty = at(s)
    fr = Fr(cx, cy, tx, ty)
    yaw = fr.yaw_dir(0, face)
    return MFr(cx, cy, yaw), yaw


def _wp(gf, u, w, z=0.0):
    return gf.p(u, w, z)


def _bar(m, mat, gf, a, b, wd=0.03, caps=True):
    fu.rod(m, mat, gf.p(*a), gf.p(*b), wd, caps=caps)


def _tri(m, mat, a, b, c, both=True):
    n = geom.cross(geom.sub(b, a), geom.sub(c, a))
    m.face(mat, [a, b, c])
    if both:
        m.face(mat, [a, c, b])


def _valance(B, gf, hw, w, z0, z1, n, mats=("CT_PlasterRed", "CT_TileWhite")):
    """Striped valance strips along u at the front plane w."""
    for i in range(n):
        u0 = -hw + 2 * hw * i / n
        u1 = u0 + 2 * hw / n
        lbox(B.detail, mats[i % len(mats)], gf, u0, u1, w, w + 0.035, z0, z1)


def _roof(B, gf, hw, w0, w1, z, mat="CT_ConcreteWhite"):
    lbox(B.detail, mat, gf, -hw, hw, w0, w1, z, z + 0.07)


def _parasol(B, gf, u, w, zt, r=1.15, zp=2.95, n=8, mats=("CT_PlasterRed", "CT_TileWhite")):
    """A striped parasol: a pole and n triangular gores."""
    _bar(B.detail, "CT_Wood", gf, (u, w, 0.0), (u, w, zp + 0.1), 0.05)
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        pa = gf.p(u + r * math.cos(a0), w + r * math.sin(a0), zt)
        pb = gf.p(u + r * math.cos(a1), w + r * math.sin(a1), zt)
        top = gf.p(u, w, zp)
        mat = mats[i % 2]
        # upper face is seen from the side: both sides drawn
        _face_dir(B.detail, mat, [pa, pb, top], (0.0, 0.0, 1.0)) if False else None
        B.detail.face(mat, [pa, pb, top])
        B.detail.face(mat, [pb, pa, top])


def _ring(d, mat, gf, u, w, z, r=0.085, segs=6, wd=0.016):
    pts = [gf.p(u + r * math.cos(2 * math.pi * k / segs), w + r * math.sin(2 * math.pi * k / segs), z) for k in range(segs)]
    for a, b in zip(pts, pts[1:] + pts[:1]):
        fu.rod(d, mat, a, b, wd, caps=False)


def _stand(B, gf, sid, u, w, face_back=True, kind="stand"):
    x, y, _ = gf.p(u, w)
    B.spot(sid, kind, (x, y, 0.0), gf.yaw + (180 if face_back else 0))


def bunting_line(B, s, t0=0.7, t1=-0.7, sag=0.8, n=7, seed=0):
    x0, y0 = surf(s, t0)
    x1, y1 = surf(s, t1)
    za = z_top(s, t0) - thick(s, t0) - 0.14
    zb = z_top(s, t1) - thick(s, t1) - 0.14
    segs = 6
    pts = []
    for i in range(segs + 1):
        f = i / segs
        tt = t0 + (t1 - t0) * f
        px, py = surf(s, tt)
        pz = za + (zb - za) * f - sag * 4 * f * (1 - f)
        pts.append((px, py, pz))
    for a, b in zip(pts, pts[1:]):
        fu.rod(B.detail, "CT_Steel", a, b, 0.01, caps=False)
    for (px, py, pz), zz in ((pts[0], za + 0.14), (pts[-1], zb + 0.14)):
        fu.rod(B.detail, "CT_Steel", (px, py, pz), (px, py, zz), 0.012, caps=False)
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    for j in range(n):
        f = (j + 0.6) / n
        i = min(int(f * segs), segs - 1)
        g = f * segs - i
        px = pts[i][0] + (pts[i + 1][0] - pts[i][0]) * g
        py = pts[i][1] + (pts[i + 1][1] - pts[i][1]) * g
        pz = pts[i][2] + (pts[i + 1][2] - pts[i][2]) * g - 0.02
        hw_ = 0.1
        a = (px - ux * hw_, py - uy * hw_, pz)
        b = (px + ux * hw_, py + uy * hw_, pz)
        c = (px, py, pz - 0.28)
        _tri(B.detail, FLAGS[(j + seed) % len(FLAGS)], a, b, c)


def strongman_tower(B, s=26.0, t=-0.4, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_Wood", gf, -0.8, 0.8, -0.8, 0.8, 0.0, 0.2)                       # plinth (solid)
    lbox(B.m, "CT_PlasterRed", gf, -0.34, 0.34, -0.30, -0.20, 0.2, 3.5)              # backboard (solid)
    for sg in (-1, 1):
        lbox(B.m, "CT_Wood", gf, sg * 0.34 - 0.04 + 0.04 * sg, sg * 0.34 + 0.04 + 0.04 * sg, -0.30, 0.0, 0.2, 3.5)
    lbox(d, "CT_PlasterYellow", gf, -0.5, 0.5, -0.4, 0.2, 3.5, 3.62)                  # cap
    lbox(d, "CT_PlasterRed", gf, -0.42, 0.42, -0.34, 0.14, 3.62, 3.7)
    _bar(d, "CT_Steel", gf, (0, -0.19, 0.3), (0, -0.19, 3.45), 0.014)                 # the puck's wire
    lbox(d, "CT_SteelRed", gf, -0.07, 0.07, -0.24, -0.12, 0.26, 0.38)                 # the puck, at rest
    for k in range(13):                                                              # scale
        z = 0.6 + 0.2 * k
        lbox(d, "CT_TileWhite", gf, 0.12 if k % 2 == 0 else 0.17, 0.3, -0.2, -0.192, z, z + 0.025)
    lbox(d, "CT_Brass", gf, -0.3, -0.12, -0.2, -0.185, 2.62, 2.7)                     # the record clip
    blob(d, "CT_Brass", *gf.p(0, -0.1, 3.33)[:2], gf.p(0, 0, 3.33)[2], 0.13, 6)       # the bell
    _bar(d, "CT_Steel", gf, (0, -0.1, 3.5), (0, -0.1, 3.42), 0.02)
    _bar(d, "CT_Steel", gf, (0, -0.1, 3.7), (0, -0.1, 4.15), 0.025)                   # flag pole
    _tri(d, "CT_PlasterRed", gf.p(0, -0.1, 4.15), gf.p(0, -0.1, 3.9), gf.p(0.5, -0.1, 4.03))
    for sg in (-1, 1):
        for k in range(8):
            x, y, z = gf.p(sg * 0.4, 0.04, 0.5 + 0.38 * k)
            blob(d, "CT_Opal", x, y, z, 0.05, 4)
    lbox(d, "CT_Leather", gf, -0.38, 0.38, 0.0, 0.55, 0.2, 0.3)                        # the pad
    lbox(d, "CT_Steel", gf, -0.04, 0.04, -0.15, 0.55, 0.3, 0.33)
    # the mallet, lying on the plinth
    _bar(d, "CT_Wood", gf, (0.56, 0.7, 0.24), (0.56, 0.06, 0.24), 0.045)
    lbox(d, "CT_Wood", gf, 0.42, 0.7, -0.1, 0.06, 0.2, 0.38)
    lbox(d, "CT_Steel", gf, 0.42, 0.7, -0.105, -0.095, 0.2, 0.38)
    lbox(d, "CT_Steel", gf, 0.42, 0.7, 0.055, 0.065, 0.2, 0.38)
    lbox(d, "CT_Brass", gf, -0.34, 0.34, 0.8, 0.82, 0.05, 0.16)                       # the record plaque
    B.lamp(gf.p(0, 0.5, 3.9), lumens=200, colour="#FFD2A0", kind="lobby", radius=6.0, fixture=False)
    B.thing("strongman_tower", "sport", gf.p(0, 0.2, 1.2), yaw, what="strongman")
    B.thing("strongman_record", "board", gf.p(0, 0.85, 0.55), yaw, topic="logbook")
    _stand(B, gf, "strongman_stand", 0.0, 1.5)
    _stand(B, gf, "strongman_stand_2", 1.3, 1.3)
    return gf


def shooting_booth(B, s=73.0, t=0.0, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterRed", gf, -1.6, 1.6, 0.62, 0.95, 0.0, 0.95)                  # counter (solid)
    lbox(d, "CT_Wood", gf, -1.68, 1.68, 0.55, 1.05, 0.95, 1.0)
    for sg in (-1, 1):                                                               # side walls (solid)
        lbox(B.m, "CT_PlasterBlue", gf, sg * 1.62 - 0.05, sg * 1.62 + 0.05, -0.85, 0.7, 0.0, 2.6)
    lbox(B.m, "CT_PlasterYellow", gf, -1.67, 1.67, -0.9, -0.8, 0.0, 2.6)              # back wall (solid)
    lbox(d, "CT_PlasterBlue", gf, -1.5, 1.5, -0.8, -0.78, 0.2, 2.45)                       # painted sky behind the targets
    for k in range(4):                                                                # clouds on it
        lbox(d, "CT_TileWhite", gf, -1.2 + 0.7 * k, -0.8 + 0.7 * k, -0.782, -0.775, 2.1 + 0.08 * (k % 2), 2.2 + 0.08 * (k % 2))
    _roof(B, gf, 1.9, -1.0, 1.2, 2.6)
    _valance(B, gf, 1.9, 1.17, 2.2, 2.6, 10)
    kit.sign_plate(B, *gf.p(0, 1.22)[:2], 0.0, yaw, name="sign_toys", w=1.7, zc=2.85)
    for sg in (-1, 1):
        lbox(d, "CT_Wood", gf, sg * 1.7 - 0.04, sg * 1.7 + 0.04, 1.0, 1.08, 0.0, 2.6)   # front posts
    # the chain of targets: a rail high at the back, ducks hanging from it
    _bar(d, "CT_Steel", gf, (-1.5, -0.55, 2.0), (1.5, -0.55, 2.0), 0.03)
    for k in range(7):
        u = -1.35 + 0.45 * k
        _bar(d, "CT_Steel", gf, (u, -0.55, 2.0), (u, -0.55, 1.78), 0.012)
        mat = ("CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterBlue")[k % 3]
        lbox(d, mat, gf, u - 0.12, u + 0.12, -0.57, -0.53, 1.58, 1.78)
        blob(d, mat, *gf.p(u + 0.09, -0.55, 1.88)[:2], gf.p(0, 0, 1.88)[2], 0.06, 5)
    # a second row: bullseyes on a lower chain
    for k in range(5):
        u = -1.1 + 0.55 * k
        lbox(d, "CT_SteelRed", gf, u - 0.15, u + 0.15, -0.69, -0.66, 0.95, 1.25)
        lbox(d, "CT_TileWhite", gf, u - 0.1, u + 0.1, -0.695, -0.665, 1.0, 1.2)
        lbox(d, "CT_SteelRed", gf, u - 0.045, u + 0.045, -0.7, -0.67, 1.075, 1.125)
    # a little windmill target in the corner and the bell to ring
    _bar(d, "CT_Steel", gf, (1.25, -0.5, 0.95), (1.25, -0.5, 1.5), 0.03)
    for a in (0, 90):
        ca, sa = math.cos(math.radians(a + 25)), math.sin(math.radians(a + 25))
        lbox(d, "CT_PlasterRed", gf, 1.25 - 0.22 * ca - 0.03, 1.25 + 0.22 * ca + 0.03, -0.54, -0.5, 1.5 - 0.22 * sa - 0.03, 1.5 + 0.22 * sa + 0.03) if a == 0 else \
            lbox(d, "CT_PlasterYellow", gf, 1.25 - 0.03, 1.25 + 0.03, -0.54, -0.5, 1.28, 1.72)
    blob(d, "CT_Brass", *gf.p(-1.35, -0.5)[:2], gf.p(0, 0, 1.1)[2], 0.08, 6)
    # two rifles chained to the counter
    for u in (-0.55, 0.55):
        _bar(d, "CT_Steel", gf, (u, 0.55, 1.06), (u, -0.1, 1.12), 0.025)                # barrel
        lbox(d, "CT_Wood", gf, u - 0.04, u + 0.04, 0.5, 0.95, 1.0, 1.1)               # stock
        _bar(d, "CT_Steel", gf, (u, 0.85, 1.0), (u + 0.18, 0.88, 0.88), 0.008, caps=False)
        _bar(d, "CT_Steel", gf, (u + 0.18, 0.88, 0.88), (u + 0.32, 0.95, 1.18), 0.008, caps=False)
        lbox(d, "CT_Steel", gf, u + 0.3, u + 0.34, 0.93, 0.97, 1.0, 1.22)
    lbox(d, "CT_PlasterRed", gf, -0.15, 0.15, 0.7, 0.9, 1.0, 1.04)                     # pellets tin
    trade_shelf(B, *gf.p(-1.57, 0.0)[:2], yaw + 90, w=1.1, h=1.9, trade="toys", seed=3, z0=0.0)
    B.lamp(gf.p(0, 0.2, 2.45), lumens=320, colour="#FFD2A0", kind="public", radius=6.0)
    B.thing("shooting_booth", "play", gf.p(0, 0.8, 1.15), yaw, what="shooting_gallery")
    _stand(B, gf, "gallery_stand", -0.5, 1.5)
    _stand(B, gf, "gallery_stand_2", 0.7, 1.6)
    return gf


def ring_toss_table(B, s=34.0, t=0.0, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterBlue", gf, -1.15, 1.15, 0.0, 0.85, 0.0, 0.66)
    lbox(d, "CT_Wood", gf, -1.22, 1.22, -0.05, 0.92, 0.66, 0.7)
    lbox(d, "CT_TileWhite", gf, -1.3, 1.3, 1.3, 1.36, 0.0, 0.015)                       # the throwing line
    # a field of little prize stands, some with a ring on
    k = 0
    for row, w in enumerate((0.15, 0.5)):
        for col in range(6):
            u = -0.95 + 0.38 * col + (0.19 if row else 0.0)
            if u > 1.0:
                continue
            x, y, z = gf.p(u, w, 0.7)
            mat = FLAGS[(col + row) % 5]
            cyl(d, mat, x, y, 0.7, 0.88, 0.07, 6)
            if (col + row) % 3 == 0:
                _ring(d, "CT_SteelRed" if row else "CT_Brass", gf, u, w, 0.885, r=0.09)
            k += 1
    # the rings: a peg with a stack of rings
    x, y, _ = gf.p(1.0, 0.7)
    _bar(d, "CT_Wood", gf, (1.0, 0.7, 0.7), (1.0, 0.7, 0.95), 0.03)
    for j in range(3):
        _ring(d, ("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen")[j], gf, 1.0, 0.7, 0.72 + 0.04 * j, r=0.1)
    # the prize shelf behind
    trade_shelf(B, *gf.p(0, -0.95)[:2], yaw, w=2.4, h=2.2, trade="toys", seed=7, z0=0.0)
    for sg in (-1, 1):                                                               # posts and roof
        lbox(d, "CT_Wood", gf, sg * 1.3 - 0.04, sg * 1.3 + 0.04, 0.95, 1.03, 0.0, 2.55)
    _roof(B, gf, 1.5, -1.1, 1.1, 2.55)
    _valance(B, gf, 1.5, 1.07, 2.2, 2.55, 8, mats=("CT_PlasterBlue", "CT_TileWhite"))
    lbox(d, "CT_PlasterYellow", gf, -0.6, 0.6, 1.07, 1.11, 2.62, 2.9)                   # a name board over the roof
    kit.poster(B, *gf.p(0, 1.11)[:2], 0.0, yaw, name="kids_2", w=0.5, h=0.24, zc=2.76)
    kit.crate(B, *gf.p(-1.25, 0.4)[:2], 0.7, yaw + 90, fill="CT_PlasterYellow", seed=3)
    B.thing("ring_table", "play", gf.p(0, 0.3, 0.95), yaw, what="ring_toss")
    B.thing("prize_shelf", "shelf", gf.p(0, -0.6, 1.4), yaw, topic="kids")
    _stand(B, gf, "ring_stand", 0.0, 1.75)
    _stand(B, gf, "ring_stand_2", 1.1, 1.8)
    return gf


def lottery_drum(B, s=40.0, t=0.25, face=-1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterRed", gf, -0.55, 0.55, -0.4, 0.4, 0.0, 0.78)
    lbox(d, "CT_TileWhite", gf, -0.6, 0.6, -0.45, 0.45, 0.78, 0.82)
    for sg in (-1, 1):                                                               # drum cradle
        _bar(d, "CT_Wood", gf, (sg * 0.45, -0.3, 0.82), (sg * 0.45, 0.0, 1.12), 0.05)
        _bar(d, "CT_Wood", gf, (sg * 0.45, 0.3, 0.82), (sg * 0.45, 0.0, 1.12), 0.05)
    cz = 1.17
    r, n = 0.34, 8
    ring0 = [gf.p(-0.4, r * math.cos(2 * math.pi * k / n), cz + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    ring1 = [gf.p(0.4, r * math.cos(2 * math.pi * k / n), cz + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        fu.rod(d, "CT_PlasterRed", ring0[k], ring1[k], 0.03, caps=False)
        for ring in (ring0, ring1):
            fu.rod(d, "CT_Brass", ring[k], ring[(k + 1) % n], 0.035, caps=False)
    _bar(d, "CT_Steel", gf, (-0.55, 0.0, cz), (0.62, 0.0, cz), 0.03)                    # axle
    _bar(d, "CT_Steel", gf, (0.62, 0.0, cz), (0.62, 0.24, cz + 0.12), 0.02)             # crank
    blob(d, "CT_PlasterRed", *gf.p(0.62, 0.26)[:2], cz + 0.12, 0.04, 5)
    for k in range(8):                                                               # tickets inside
        a = k * 2.399
        lbox(d, FLAGS[k % 6] if k % 3 else "CT_Paper", gf, -0.25 + 0.5 * ((k * 5) % 7) / 7, -0.25 + 0.5 * ((k * 5) % 7) / 7 + 0.06,
             0.1 * math.cos(a) - 0.04, 0.1 * math.cos(a) + 0.04, cz - 0.2 + 0.05 * (k % 4), cz - 0.17 + 0.05 * (k % 4))
    lbox(d, "CT_PlasterYellow", gf, -0.25, 0.1, 0.1, 0.34, 0.82, 0.88)                  # a tray with the won tickets
    for j in range(5):
        lbox(d, "CT_Paper", gf, -0.22 + 0.06 * j, -0.17 + 0.06 * j, 0.14, 0.3, 0.88, 0.9)
    _parasol(B, gf, 0.0, -0.55, 2.35, r=1.1, zp=2.85)
    kit.bottles(B, *gf.p(-0.45, 0.2)[:2], 0.82, n=2, seed=4)
    B.lamp(gf.p(0, 0.2, 2.4), lumens=200, colour="#FFC890", kind="public", radius=5.0, fixture=False)
    B.thing("lottery_drum", "play", gf.p(0, 0.4, 1.17), yaw, what="lottery")
    _stand(B, gf, "lottery_stand", 0.0, 1.5)
    return gf


def fortune_automaton(B, s=109.0, t=-0.4, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterBlue", gf, -0.55, 0.55, -0.4, 0.4, 0.0, 0.9)
    lbox(d, "CT_Brass", gf, -0.58, 0.58, -0.43, 0.43, 0.9, 0.93)
    lbox(d, "CT_Brass", gf, -0.58, 0.58, 0.4, 0.43, 0.0, 0.05)
    for u in (-0.3, 0.3):
        lbox(d, "CT_Brass", gf, u - 0.012, u + 0.012, 0.402, 0.41, 0.2, 0.84)             # gilt trim on the front
    lbox(d, "CT_SteelRed", gf, -0.08, 0.08, 0.402, 0.42, 0.62, 0.68)                    # the coin slot
    lbox(d, "CT_Steel", gf, -0.2, 0.2, 0.3, 0.44, 0.33, 0.37)                          # the card tray
    lbox(d, "CT_Paper", gf, -0.06, 0.1, 0.34, 0.42, 0.37, 0.375)
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):                                # glass case posts
        lbox(d, "CT_Wood", gf, sx * 0.5 - 0.03, sx * 0.5 + 0.03, sy * 0.36 - 0.03, sy * 0.36 + 0.03, 0.93, 2.25)
    lbox(d, "CT_PlasterBlue", gf, -0.6, 0.6, -0.46, 0.46, 2.25, 2.35)
    lbox(d, "CT_PlasterYellow", gf, -0.5, 0.5, -0.38, 0.38, 2.35, 2.45)
    blob(d, "CT_Brass", *gf.p(0, 0)[:2], 2.55, 0.1, 6)
    lbox(d, "CT_PlasterBlue", gf, -0.48, 0.48, -0.37, -0.33, 0.93, 2.25)                # back panel, a starry sky
    for k in range(9):
        blob(d, "CT_Brass", *gf.p(-0.4 + 0.1 * ((k * 5) % 9), -0.32)[:2], 1.5 + 0.09 * ((k * 4) % 8), 0.018, 4)
    lbox(d, "CT_PlasterRed", gf, -0.3, 0.3, -0.3, 0.12, 0.93, 1.1)                      # the fortune-teller's table
    # no figure of a person: a wooden owl nods on its perch, a brass pointer arm sweeps the cards, a wheel of fortune
    # turns on the back panel
    _bar(d, "CT_Wood", gf, (-0.32, -0.08, 1.1), (-0.32, -0.08, 1.2), 0.03)                # the owl's perch
    lbox(d, "CT_Wood", gf, -0.45, -0.19, -0.14, -0.02, 1.2, 1.23)
    blob(d, "CT_Wood", *gf.p(-0.32, -0.08)[:2], gf.p(0, 0, 1.32)[2], 0.1, 6)              # body
    blob(d, "CT_WoodLight", *gf.p(-0.32, 0.0)[:2], gf.p(0, 0, 1.28)[2], 0.06, 5)          # breast
    blob(d, "CT_Wood", *gf.p(-0.32, 0.0)[:2], gf.p(0, 0, 1.46)[2], 0.075, 6)              # head
    for sg in (-1, 1):
        blob(d, "CT_Opal", *gf.p(-0.32 + sg * 0.035, 0.06)[:2], gf.p(0, 0, 1.47)[2], 0.022, 4)     # eyes
        _tri(d, "CT_Wood", gf.p(-0.32 + sg * 0.05, 0.0, 1.5), gf.p(-0.32 + sg * 0.085, 0.0, 1.58), gf.p(-0.32 + sg * 0.01, 0.0, 1.52))
    _tri(d, "CT_Brass", gf.p(-0.34, 0.07, 1.45), gf.p(-0.3, 0.07, 1.45), gf.p(-0.32, 0.11, 1.42))
    blob(d, "CT_Brass", *gf.p(0.3, -0.12)[:2], gf.p(0, 0, 1.16)[2], 0.035, 5)             # the pointer's pivot
    _bar(d, "CT_Brass", gf, (0.3, -0.12, 1.16), (0.12, 0.22, 1.13), 0.022)
    _tri(d, "CT_Brass", gf.p(0.12, 0.22, 1.13), gf.p(0.08, 0.2, 1.125), gf.p(0.1, 0.3, 1.12))
    wz = 1.78                                                                          # the wheel of fortune
    for kk in range(8):
        a0, a1 = 2 * math.pi * kk / 8, 2 * math.pi * (kk + 1) / 8
        _tri(d, ("CT_PlasterRed", "CT_PlasterYellow")[kk % 2], gf.p(0, -0.31, wz), gf.p(0.27 * math.cos(a0), -0.31, wz + 0.27 * math.sin(a0)),
             gf.p(0.27 * math.cos(a1), -0.31, wz + 0.27 * math.sin(a1)))
        fu.rod(d, "CT_Brass", gf.p(0.27 * math.cos(a0), -0.305, wz + 0.27 * math.sin(a0)), gf.p(0.27 * math.cos(a1), -0.305, wz + 0.27 * math.sin(a1)), 0.016, caps=False)
    blob(d, "CT_Brass", *gf.p(0, -0.3)[:2], gf.p(0, 0, wz)[2], 0.04, 5)
    _tri(d, "CT_SteelRed", gf.p(-0.03, -0.29, wz + 0.31), gf.p(0.03, -0.29, wz + 0.31), gf.p(0, -0.29, wz + 0.22))
    for sg in (-1, 1):                                                                 # velvet curtains at the case's sides
        lbox(d, "CT_PlasterRed", gf, sg * 0.49 - 0.025, sg * 0.49 + 0.025, -0.35, 0.33, 1.05, 2.25)
    lbox(d, "CT_Brass", gf, 0.28, 0.4, 0.32, 0.4, 0.93, 0.96)                          # the coin dish
    blob(d, "CT_Opal", *gf.p(0, 0.2)[:2], 1.27, 0.09, 6)                                  # the crystal ball
    lbox(d, "CT_Brass", gf, -0.06, 0.06, 0.14, 0.26, 1.1, 1.17)
    for j in range(5):                                                               # a fan of cards on the table
        lbox(d, "CT_Paper", gf, -0.2 + 0.1 * j, -0.14 + 0.1 * j, 0.24, 0.3, 1.1, 1.103)
    B.lamp(gf.p(0, 0.15, 2.1), lumens=240, colour="#FFB090", kind="public", radius=5.0, fixture=False)
    B.thing("fortune_automaton", "machine", gf.p(0, 0.45, 1.3), yaw, what="fortune")
    _stand(B, gf, "fortune_stand", 0.0, 1.3)
    return gf


def weighing_chair(B, s=102.0, t=-0.4, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(B.m, "CT_PlasterGreen", gf, -0.5, 0.5, -0.55, 0.55, 0.0, 0.14)                   # platform
    lbox(d, "CT_Steel", gf, -0.52, 0.52, 0.55, 0.58, 0.0, 0.14)
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):                                # chair
        lbox(d, "CT_Wood", gf, sx * 0.22 - 0.02, sx * 0.22 + 0.02, sy * 0.18 - 0.02, sy * 0.18 + 0.02, 0.14, 0.58)
    lbox(d, "CT_Leather", gf, -0.27, 0.27, -0.23, 0.23, 0.58, 0.63)
    lbox(d, "CT_Leather", gf, -0.25, 0.25, -0.26, -0.2, 0.63, 1.2)
    for sg in (-1, 1):
        lbox(d, "CT_Wood", gf, sg * 0.28 - 0.025, sg * 0.28 + 0.025, -0.2, 0.2, 0.84, 0.88)
        lbox(d, "CT_Wood", gf, sg * 0.28 - 0.02, sg * 0.28 + 0.02, -0.2, -0.16, 0.63, 0.88)
    lbox(d, "CT_Wood", gf, -0.25, 0.25, 0.3, 0.42, 0.14, 0.2)                           # footrest
    lbox(d, "CT_SteelRed", gf, -0.09, 0.09, -0.52, -0.36, 0.14, 1.95)                   # the scale's post
    lbox(d, "CT_Steel", gf, -0.62, 0.62, -0.48, -0.4, 1.9, 1.96)                        # the beam
    lbox(d, "CT_Brass", gf, 0.25, 0.37, -0.5, -0.38, 1.84, 2.02)                        # the sliding weight
    lbox(d, "CT_Steel", gf, 0.5, 0.58, -0.48, -0.4, 1.7, 1.9)
    lbox(d, "CT_Paper", gf, -0.25, 0.25, -0.358, -0.35, 1.36, 1.84)
    kit.poster(B, *gf.p(0, -0.35)[:2], 0.0, yaw, name="clock_2", w=0.4, h=0.4, zc=1.62)
    lbox(d, "CT_Steel", gf, -0.01, 0.01, -0.34, -0.3, 1.58, 1.72)                       # the pointer
    _parasol(B, gf, 0.0, -0.8, 2.35, r=1.0, zp=2.8, mats=("CT_PlasterGreen", "CT_TileWhite"))
    B.thing("weighing_chair", "machine", gf.p(0, -0.15, 1.62), yaw, what="scales")
    B.thing("weighing_seat", "seat", gf.p(0, 0.0, 0.59), yaw, height=0.45)
    _stand(B, gf, "weighing_stand", 0.0, 1.5)
    # the Lost and Found basket beside the platform: an umbrella, a mitten
    kit.crate(B, *gf.p(1.0, 0.1)[:2], 0.0, yaw + 90, w=0.5, d=0.38, h=0.34, fill="CT_PlasterPink", seed=21)
    _bar(d, "CT_Wood", gf, (0.95, 0.05, 0.3), (0.8, 0.0, 0.62), 0.02)
    blob(d, "CT_PlasterBlue", *gf.p(1.1, 0.12)[:2], 0.38, 0.06, 5)
    B.thing("fair_lost_basket", "board", gf.p(1.0, 0.1, 0.5), yaw, topic="lost")
    return gf


def photographer(B, s=87.0, t=-0.2, face=1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    kit.photo_backdrop(B, *gf.p(0, -1.5)[:2], 0.0, yaw, w=2.2, h=2.3, mat="CT_PlasterBlue")
    lbox(d, "CT_PlasterRed", gf, -0.22, 0.22, -0.8, -0.38, 0.4, 0.45)                    # the posing chair
    lbox(d, "CT_PlasterRed", gf, -0.22, 0.22, -0.82, -0.78, 0.45, 0.95)
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        lbox(d, "CT_Wood", gf, sx * 0.19 - 0.02, sx * 0.19 + 0.02, -0.59 + sy * 0.19 - 0.02, -0.59 + sy * 0.19 + 0.02, 0.0, 0.4)
    kit.camera_tripod(B, *gf.p(0, 1.9)[:2], 0.0, yaw + 180)
    kit.cardboard_box(B, *gf.p(0.9, 1.8)[:2], 0.0, yaw + 20, w=0.6, d=0.4, h=0.3, seed=11)
    kit.sandwich_board(B, *gf.p(-1.4, 0.5)[:2], 0.0, yaw, name="price_1", w=0.55)
    # a flash lamp on a pole
    _bar(d, "CT_Steel", gf, (-0.9, 1.5, 0.0), (-0.9, 1.5, 1.75), 0.03)
    lbox(d, "CT_Aluminium", gf, -1.05, -0.75, 1.42, 1.5, 1.65, 1.9)
    blob(d, "CT_Opal", *gf.p(-0.9, 1.4)[:2], 1.78, 0.07, 5)
    B.thing("photo_camera", "machine", gf.p(0, 1.9, 1.35), yaw, what="photographer")
    B.thing("photo_chair", "seat", gf.p(0, -0.6, 0.45), yaw + 180, height=0.45)
    _stand(B, gf, "photo_stand", 1.2, 1.0)
    return gf


def chalk_board(B, s=78.0, t=0.4, face=-1):
    gf, yaw = _gframe(s, t, face)
    d = B.detail
    lbox(d, "CT_Wood", gf, -0.68, 0.68, -0.09, -0.04, 0.6, 1.8)
    for sg in (-1, 1):
        _bar(d, "CT_Wood", gf, (sg * 0.58, -0.07, 1.75), (sg * 0.64, 0.42, 0.0), 0.05)
        _bar(d, "CT_Wood", gf, (sg * 0.58, -0.07, 1.75), (sg * 0.64, -0.52, 0.0), 0.04)
    lbox(d, "CT_ConcreteDark", gf, -0.62, 0.62, -0.04, 0.0, 0.7, 1.7)                 # the slate
    for k, ln in enumerate((0.9, 0.7, 1.0, 0.6, 0.8, 0.5)):                          # chalk lines: prizes and scores
        z = 1.55 - 0.15 * k
        lbox(d, "CT_TileWhite", gf, -0.52, -0.52 + ln * 0.9, 0.0, 0.004, z, z + 0.02)
        lbox(d, "CT_TileWhite", gf, 0.4, 0.5, 0.0, 0.004, z - 0.005, z + 0.03)
    lbox(d, "CT_Wood", gf, -0.66, 0.66, 0.0, 0.1, 0.64, 0.7)                          # the chalk tray
    for k in range(3):
        lbox(d, "CT_TileWhite", gf, -0.3 + 0.12 * k, -0.24 + 0.12 * k, 0.03, 0.05, 0.7, 0.72)
    lbox(d, "CT_PlasterRed", gf, -0.7, 0.7, -0.12, -0.03, 1.8, 1.86)
    B.thing("prize_board", "board", gf.p(0, 0.05, 1.2), yaw, topic="fair")
    _stand(B, gf, "chalk_stand", 0.0, 1.1)
    return gf


def lost_sandal(B, s=12.0, t=-0.35):
    """A child's sandal under the bench, a thing to find."""
    cx, cy = surf(s, t)
    x, y, tx, ty = at(s)
    fr = Fr(cx, cy, tx, ty)
    px, py = fr.p(0.5, 0.0)
    lbox(B.detail, "CT_PlasterRed", MFr(px, py, 20.0), -0.07, 0.07, -0.05, 0.1, 0.0, 0.04)
    lbox(B.detail, "CT_PlasterRed", MFr(px, py, 20.0), -0.07, 0.07, -0.05, -0.02, 0.04, 0.09)
    B.thing("fair_sandal", "board", (px, py, 0.1), 20.0, topic="lost")


# ======================================================================================================== second pass
def _post(d, gf, u, w, z1, mat="CT_Wood", r=0.03, segs=6):
    x, y, _ = gf.p(u, w)
    cyl(d, mat, x, y, 0.0, z1, r, segs, top=True)


def _rope(d, gf, a, b, z, sag=0.07, mat="CT_PlasterRed", wd=0.018):
    """A sagging rope between two floor points (u, w) at height z."""
    ax, ay = a
    bx, by = b
    mid = ((ax + bx) / 2, (ay + by) / 2)
    pts = [gf.p(ax, ay, z), gf.p(mid[0], mid[1], z - sag), gf.p(bx, by, z)]
    for p0, p1 in zip(pts, pts[1:]):
        fu.rod(d, mat, p0, p1, wd, caps=False)


def _toy_duck(d, gf, u, w, z):
    blob(d, "CT_PlasterYellow", *gf.p(u, w)[:2], z + 0.045, 0.05, 6)
    blob(d, "CT_PlasterYellow", *gf.p(u + 0.04, w)[:2], z + 0.1, 0.03, 5)
    lbox(d, "CT_PlasterRed", gf, u + 0.065, u + 0.1, w - 0.012, w + 0.012, z + 0.095, z + 0.105)
    _tri(d, "CT_PlasterYellow", gf.p(u - 0.045, w, z + 0.06), gf.p(u - 0.085, w, z + 0.09), gf.p(u - 0.04, w, z + 0.09))
    lbox(d, "CT_Steel", gf, u - 0.01, u + 0.01, w - 0.03, w + 0.03, z, z + 0.012)             # the key's base plate


def _toy_lighthouse(d, gf, u, w, z):
    for j, mat in enumerate(("CT_PlasterRed", "CT_TileWhite", "CT_PlasterRed")):
        x, y, _ = gf.p(u, w)
        cyl(d, mat, x, y, z + 0.07 * j, z + 0.07 * (j + 1), 0.04 - 0.004 * j, 6, top=(j == 2))
    lbox(d, "CT_Steel", gf, u - 0.03, u + 0.03, w - 0.03, w + 0.03, z + 0.21, z + 0.235)
    blob(d, "CT_Opal", *gf.p(u, w)[:2], z + 0.255, 0.022, 4)


def _toy_hedgehog(d, gf, u, w, z):
    blob(d, "CT_ConcreteDark", *gf.p(u, w)[:2], z + 0.045, 0.06, 6)
    blob(d, "CT_PlasterPink", *gf.p(u + 0.06, w)[:2], z + 0.035, 0.03, 4)
    for j in range(6):
        fu.rod(d, "CT_Wood", gf.p(u - 0.02 - 0.012 * j, w + (j - 2.5) * 0.015, z + 0.06), gf.p(u - 0.06 - 0.01 * j, w + (j - 2.5) * 0.022, z + 0.11), 0.008, caps=False)


def _toy_harmonica(d, gf, u, w, z):
    lbox(d, "CT_Steel", gf, u - 0.06, u + 0.06, w - 0.015, w + 0.015, z, z + 0.028)
    lbox(d, "CT_Brass", gf, u - 0.062, u - 0.05, w - 0.017, w + 0.017, z - 0.002, z + 0.03)
    lbox(d, "CT_Brass", gf, u + 0.05, u + 0.062, w - 0.017, w + 0.017, z - 0.002, z + 0.03)


def _toy_gull(d, gf, u, w, z):
    blob(d, "CT_TileWhite", *gf.p(u, w)[:2], z + 0.04, 0.045, 5)
    blob(d, "CT_TileWhite", *gf.p(u + 0.04, w)[:2], z + 0.085, 0.025, 4)
    _tri(d, "CT_PlasterRed", gf.p(u + 0.06, w, z + 0.085), gf.p(u + 0.1, w, z + 0.08), gf.p(u + 0.06, w, z + 0.07))
    for sg in (-1, 1):
        _tri(d, "CT_Steel", gf.p(u - 0.01, w + sg * 0.03, z + 0.05), gf.p(u + 0.03, w + sg * 0.03, z + 0.06), gf.p(u - 0.02, w + sg * 0.12, z + 0.09))
    lbox(d, "CT_Steel", gf, u - 0.012, u + 0.012, w - 0.012, w + 0.012, z, z + 0.01)


def strongman_extra(B, gf, yaw):
    d = B.detail
    # a queue: three posts and two ropes along the left of the stand, a ledger slate, a bucket and a ladle, the weights
    for j, w in enumerate((0.95, 1.75, 2.55)):
        _post(d, gf, -1.0, w, 0.9, "CT_SteelRed", 0.03)
        x, y, _ = gf.p(-1.0, w)
        blob(d, "CT_Brass", x, y, 0.94, 0.05, 5)
        cyl(d, "CT_SteelRed", x, y, 0.0, 0.02, 0.12, 8)
    for a, b in (((-1.0, 0.95), (-1.0, 1.75)), ((-1.0, 1.75), (-1.0, 2.55))):
        _rope(d, gf, a, b, 0.82)
    su, sw = -1.95, 0.45
    for sg in (-1, 1):
        _bar(d, "CT_Wood", gf, (su + sg * 0.3, sw - 0.12, 0.0), (su + sg * 0.3, sw, 0.8), 0.045)
        _bar(d, "CT_Wood", gf, (su + sg * 0.3, sw + 0.12, 0.0), (su + sg * 0.3, sw, 0.8), 0.045)
    lbox(d, "CT_Wood", gf, su - 0.38, su + 0.38, sw - 0.03, sw + 0.03, 0.78, 1.46)
    lbox(d, "CT_ConcreteDark", gf, su - 0.33, su + 0.33, sw + 0.03, sw + 0.036, 0.84, 1.4)
    for k, ln in enumerate((0.5, 0.35, 0.58, 0.28, 0.45)):
        z = 1.3 - 0.095 * k
        lbox(d, "CT_TileWhite", gf, su - 0.28, su - 0.28 + ln, sw + 0.036, sw + 0.039, z, z + 0.018)
        lbox(d, "CT_TileWhite", gf, su + 0.2, su + 0.28, sw + 0.036, sw + 0.039, z, z + 0.018)
    lbox(d, "CT_Wood", gf, su - 0.36, su + 0.36, sw + 0.03, sw + 0.1, 0.8, 0.84)
    B.thing("strongman_ledger", "board", gf.p(su, sw + 0.1, 1.12), yaw, topic="logbook")
    bx, by, _ = gf.p(-0.6, 0.6)
    cyl(d, "CT_SteelRed", bx, by, 0.2, 0.46, 0.12, 8, top=False)
    cyl(d, "CT_PlasterBlue", bx, by, 0.44, 0.45, 0.108, 8)
    _bar(d, "CT_Wood", gf, (-0.6 + 0.05, 0.6, 0.45), (-0.3, 0.55, 0.3), 0.014)                 # the ladle
    blob(d, "CT_Steel", *gf.p(-0.28, 0.55)[:2], 0.3, 0.035, 5)
    lbox(d, "CT_Wood", gf, 1.2, 2.3, -0.1, 0.32, 0.0, 0.3)                                    # the weights' bench
    lbox(d, "CT_Steel", gf, 1.22, 2.28, -0.08, 0.3, 0.3, 0.32)
    for u, r in ((1.45, 0.09), (1.75, 0.115), (2.07, 0.145)):
        x, y, _ = gf.p(u, 0.11)
        blob(d, "CT_Steel", x, y, 0.32 + r, r, 6)
        for sg in (-1, 1):
            _bar(d, "CT_Steel", gf, (u + sg * 0.05, 0.11, 0.32 + 1.7 * r), (u + sg * 0.05, 0.11, 0.32 + 2.3 * r), 0.016, caps=False)
        lbox(d, "CT_Steel", gf, u - 0.055, u + 0.055, 0.1, 0.12, 0.32 + 2.28 * r, 0.32 + 2.34 * r)
    lbox(d, "CT_TileWhite", gf, 1.3, 2.2, 0.31, 0.318, 0.06, 0.24)                             # a numbers plate: 16, 24, 32
    B.thing("strongman_weights", "sport", gf.p(1.75, 0.11, 0.55), yaw, what="barbell")
    B.thing("strongman_keeper", "counter", gf.p(-1.35, 0.35, 1.2), yaw, role="vendor", service="games", trade="strongman")
    _stand(B, gf, "strongman_work", -1.35, 0.0, face_back=False, kind="stall")


def shooting_extra(B, gf, yaw):
    d = B.detail
    kit.stool(B, *gf.p(-0.95, 0.05)[:2], 0.0, yaw, mat="CT_PlasterBlue", h=0.55)
    kit.radio(B, *gf.p(-1.15, 0.78)[:2], 1.0, yaw + 12)
    B.thing("gallery_radio", "machine", gf.p(-1.15, 0.78, 1.15), yaw, what="radio")
    x, y, _ = gf.p(1.4, 0.76)
    cyl(d, "CT_PlasterGreen", x, y, 1.0, 1.24, 0.045, 8, top=False)
    cyl(d, "CT_Aluminium", x, y, 1.24, 1.27, 0.05, 8)
    kit.cup(B, *gf.p(1.22, 0.85)[:2], 1.0, yaw=yaw)
    kit.sandwich_board(B, *gf.p(2.25, 1.5)[:2], 0.0, yaw + 10, name="price_0", w=0.55)
    B.thing("gallery_prices", "board", gf.p(2.25, 1.5, 0.55), yaw, topic="prices")
    for k, (w0, z0) in enumerate(((-0.3, 1.5), (0.15, 1.25), (0.55, 1.55))):                  # target cards pinned on the right wall
        lbox(d, "CT_TileWhite", gf, 1.54, 1.558, w0, w0 + 0.22, z0, z0 + 0.3)
        for r_, mat in ((0.1, "CT_SteelRed"), (0.06, "CT_TileWhite"), (0.025, "CT_SteelRed")):
            lbox(d, mat, gf, 1.5535 - 0.001 * k, 1.5545 + 0.0, w0 + 0.11 - r_ * 0.8, w0 + 0.11 + r_ * 0.8, z0 + 0.15 - r_ * 0.8, z0 + 0.15 + r_ * 0.8)
    for j in range(5):                                                                        # the day's tally: pellets in a row of cups
        x, y, _ = gf.p(0.95 + 0.07 * j, 0.62)
        cyl(d, "CT_Steel", x, y, 1.0, 1.04, 0.025, 5)
    B.thing("gallery_keeper", "counter", gf.p(1.0, 0.85, 1.1), yaw, role="vendor", service="games", trade="gallery")
    _stand(B, gf, "gallery_work", 0.15, 0.0, face_back=False, kind="stall")


def ring_extra(B, gf, yaw):
    d = B.detail
    # a three-tier prize stand left of the table with the toys that the chalk board names
    su, sw = -2.05, 0.35
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        _bar(d, "CT_Wood", gf, (su + sx * 0.25, sw + sy * 0.17, 0.0), (su + sx * 0.25, sw + sy * 0.17, 1.28), 0.03, caps=True)
    for zb, dw in ((0.62, 0.2), (0.95, 0.19), (1.26, 0.17)):
        lbox(d, "CT_WoodLight", gf, su - 0.3, su + 0.3, sw - dw, sw + dw, zb - 0.025, zb)
    _toy_duck(d, gf, su - 0.12, sw, 0.62)
    _toy_hedgehog(d, gf, su + 0.14, sw, 0.62)
    _toy_lighthouse(d, gf, su - 0.15, sw, 0.95)
    _toy_harmonica(d, gf, su + 0.12, sw + 0.02, 0.95)
    _toy_gull(d, gf, su, sw, 1.26)
    B.thing("prize_stand", "board", gf.p(su, sw + 0.2, 1.1), yaw, topic="kids")
    kit.sandwich_board(B, *gf.p(1.95, 1.25)[:2], 0.0, yaw - 8, name="price_1", w=0.55)
    x, y, _ = gf.p(1.75, 0.35)
    cyl(d, "CT_PlasterBlue", x, y, 0.0, 0.3, 0.14, 8, top=False)                                # the pail of spare rings
    for j in range(5):
        _ring(d, ("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen", "CT_PlasterBlue", "CT_Brass")[j], gf, 1.75, 0.35, 0.31 + 0.025 * j, r=0.1)
    B.thing("ring_keeper", "counter", gf.p(-0.95, 0.95, 0.85), yaw, role="vendor", service="games", trade="rings")
    _stand(B, gf, "ring_work", 0.0, -0.4, face_back=False, kind="stall")


def lottery_extra(B, gf, yaw):
    d = B.detail
    # the numbers board on two posts, a clothesline of postcards, the keeper's spot behind the parasol pole
    for u in (1.1, 2.0):
        _bar(d, "CT_Wood", gf, (u, 0.0, 0.0), (u, 0.0, 1.95), 0.05)
    lbox(d, "CT_PlasterBlue", gf, 1.05, 2.05, -0.03, 0.03, 1.0, 1.85)
    lbox(d, "CT_Wood", gf, 1.02, 2.08, -0.05, 0.05, 1.85, 1.91)
    for r_ in range(3):
        for c in range(4):
            u0 = 1.12 + 0.22 * c
            z0 = 1.08 + 0.24 * r_
            lbox(d, FLAGS[(r_ * 4 + c) % 6], gf, u0, u0 + 0.17, 0.03, 0.04, z0, z0 + 0.17)
            lbox(d, "CT_TileWhite", gf, u0 + 0.05, u0 + 0.12, 0.04, 0.045, z0 + 0.05, z0 + 0.12)
    B.thing("lottery_numbers", "board", gf.p(1.55, 0.1, 1.4), yaw, topic="prices")
    for u in (-1.1, -2.1):
        _bar(d, "CT_Wood", gf, (u, 0.6, 0.0), (u, 0.6, 1.8), 0.045)
    fu.rod(d, "CT_Steel", gf.p(-1.1, 0.6, 1.72), gf.p(-2.1, 0.6, 1.72), 0.008, caps=False)
    for j in range(5):
        u0 = -1.2 - 0.18 * j
        lbox(d, "CT_Paper", gf, u0 - 0.07, u0 + 0.07, 0.585, 0.615, 1.5, 1.72)
        lbox(d, FLAGS[j % 5], gf, u0 - 0.055, u0 + 0.055, 0.615, 0.62, 1.55, 1.66)
    B.thing("lottery_keeper", "counter", gf.p(-0.95, 0.2, 0.95), yaw, role="vendor", service="games", trade="lottery")
    _stand(B, gf, "lottery_work", 0.0, -1.0, face_back=False, kind="stall")


def weigh_extra(B, gf, yaw):
    d = B.detail
    su, sw = -0.95, 0.5
    lbox(d, "CT_Wood", gf, su - 0.04, su + 0.04, sw - 0.025, sw + 0.025, 0.0, 2.05)
    for k in range(15):
        z = 0.5 + 0.1 * k
        lbox(d, "CT_TileWhite", gf, su - (0.07 if k % 5 == 0 else 0.045), su, sw + 0.025, sw + 0.03, z, z + 0.012)
    lbox(d, "CT_Brass", gf, su - 0.1, su + 0.1, sw + 0.03, sw + 0.06, 1.42, 1.46)               # the sliding bar
    lbox(d, "CT_SteelRed", gf, su - 0.11, su - 0.09, sw + 0.03, sw + 0.065, 1.38, 1.5)
    lbox(d, "CT_Wood", gf, su - 0.14, su + 0.14, sw - 0.06, sw + 0.1, 0.0, 0.03)
    B.thing("fair_height_post", "machine", gf.p(su, sw + 0.1, 1.45), yaw, what="scales")


def photo_extra(B, gf, yaw):
    d = B.detail
    tb = kit.table(B, *gf.p(1.95, 1.65)[:2], 0.0, yaw, w=0.8, d=0.5, h=0.8)
    top = tb["top"]
    lbox(d, "CT_Leather", gf, 1.7, 2.0, 1.55, 1.78, top, top + 0.035)                         # the album, open
    lbox(d, "CT_Paper", gf, 1.72, 1.98, 1.57, 1.76, top + 0.035, top + 0.045)
    for j in range(4):
        lbox(d, "CT_Paper", gf, 2.05 + 0.02 * j, 2.2 + 0.02 * j, 1.55 + 0.01 * j, 1.7 + 0.01 * j, top + 0.0, top + 0.004 + 0.002 * j)
    B.thing("photo_album", "board", gf.p(1.85, 1.66, top + 0.08), yaw, topic="exhibit")
    for w in (0.2, 1.9):                                                                       # a line of prints drying
        _bar(d, "CT_Wood", gf, (-2.35, w, 0.0), (-2.35, w, 1.85), 0.045)
    fu.rod(d, "CT_Steel", gf.p(-2.35, 0.2, 1.78), gf.p(-2.35, 1.9, 1.78), 0.008, caps=False)
    for j in range(6):
        w = 0.33 + 0.27 * j
        lbox(d, "CT_Paper", gf, -2.38, -2.32, w - 0.08, w + 0.08, 1.46, 1.76)
        lbox(d, ("CT_PlasterBlue", "CT_ConcreteDark", "CT_PlasterGreen")[j % 3], gf, -2.381, -2.319, w - 0.065, w + 0.065, 1.51, 1.7)
    lbox(d, "CT_TileWhite", gf, 1.0, 1.7, -0.72, -0.69, 0.95, 1.75)                            # a reflector on a tripod
    for sg in (-1, 0, 1):
        _bar(d, "CT_Steel", gf, (1.35, -0.7, 0.95), (1.35 + sg * 0.3, -0.7 + (0.0 if sg else -0.2), 0.0), 0.015, caps=False)


def carousel(B, s=118.0, t=0.1):
    """A small children's roundabout in the east clearing: a platform, a striped drum and roof, six horses on brass poles,
    the operator's control box with a bell (spot staff_carousel_operator). Pure animals and machinery, nobody on it."""
    cx, cy = surf(s, t)
    x_, y_, tx, ty = at(s)
    d = B.detail
    R = 1.5
    cyl(d, "CT_Wood", cx, cy, 0.0, 0.18, R, 16)
    cyl(d, "CT_PlasterRed", cx, cy, 0.18, 0.21, R - 0.02, 16)
    cyl(d, "CT_PlasterYellow", cx, cy, 0.21, 2.3, 0.3, 12, top=False)
    for z0_, z1_ in ((0.5, 0.85), (1.35, 1.7)):
        cyl(d, "CT_PlasterRed", cx, cy, z0_, z1_, 0.315, 12, top=False)
    gf = MFr(cx, cy, 0.0)
    _parasol(B, gf, 0.0, 0.0, 2.35, r=R + 0.05, zp=3.0, n=12, mats=("CT_PlasterRed", "CT_TileWhite"))
    for j in range(12):                                                                       # bulbs round the roof's rim
        a = 2 * math.pi * (j + 0.5) / 12
        blob(d, "CT_Opal", cx + (R + 0.04) * math.cos(a), cy + (R + 0.04) * math.sin(a), 2.31, 0.04, 4)
    for k in range(6):
        a = 2 * math.pi * k / 6
        px, py = cx + 1.0 * math.cos(a), cy + 1.0 * math.sin(a)
        heading = (-math.sin(a), math.cos(a))
        yawh = math.degrees(math.atan2(heading[0], -heading[1]))
        F = MFr(px, py, yawh)
        up = 0.14 if k % 2 else 0.0
        _bar(d, "CT_Brass", F, (0, 0, 0.21), (0, 0, 2.35), 0.03)
        mat = ("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow")[k % 3]
        lbox(d, mat, F, -0.07, 0.07, -0.27, 0.27, 0.86 + up, 1.14 + up)                       # body
        lbox(d, "CT_Leather", F, -0.075, 0.075, -0.1, 0.12, 1.14 + up, 1.17 + up)             # saddle
        _bar(d, mat, F, (0, 0.2, 1.1 + up), (0, 0.33, 1.4 + up), 0.1)                         # neck
        lbox(d, mat, F, -0.06, 0.06, 0.28, 0.47, 1.28 + up, 1.46 + up)                         # head
        lbox(d, "CT_Wood", F, -0.012, 0.012, 0.18, 0.33, 1.4 + up, 1.5 + up)                    # mane
        _bar(d, "CT_TileWhite", F, (0, -0.27, 1.1 + up), (0, -0.4, 0.85 + up), 0.035)          # tail
        for fx_, fy_, fz_ in ((0.14, 0.2, 0.12), (-0.14, 0.2, 0.12)):
            for gx in (-0.04, 0.04):
                _bar(d, "CT_Wood", F, (gx, 0.17, 0.9 + up), (gx, 0.26, 0.66 + up), 0.03, caps=False)
                _bar(d, "CT_Wood", F, (gx, -0.17, 0.9 + up), (gx, -0.24, 0.66 + up), 0.03, caps=False)
            break
    # control box and bell on the walk side (west of the roundabout along the walk)
    bx, by = surf(s - 2.7, t)
    face = math.degrees(math.atan2(tx, -ty))                                                   # toward the carousel = +tangent
    F = MFr(bx, by, face)
    lbox(d, "CT_PlasterBlue", F, -0.28, 0.28, -0.2, 0.2, 0.0, 1.0)
    lbox(d, "CT_Wood", F, -0.32, 0.32, -0.24, 0.24, 1.0, 1.04)
    _bar(d, "CT_Steel", F, (-0.1, 0.0, 1.04), (-0.1, 0.07, 1.3), 0.02)
    blob(d, "CT_SteelRed", *F.p(-0.1, 0.07)[:2], 1.32, 0.04, 5)
    blob(d, "CT_Brass", *F.p(0.15, 0.0)[:2], 1.1, 0.075, 6)                                   # the bell
    lbox(d, "CT_Steel", F, 0.1, 0.2, -0.04, 0.04, 1.04, 1.06)
    lbox(d, "CT_TileWhite", F, -0.2, 0.2, -0.2, -0.198, 0.3, 0.9)
    lbox(d, "CT_PlasterRed", F, -0.2, 0.2, -0.201, -0.2, 0.7, 0.85)
    B.thing("fair_carousel", "machine", (cx, cy, 1.1), face, what="carousel")
    B.thing("carousel_keeper", "counter", F.p(0.0, 0.35, 1.05), face + 180, role="carousel_operator", service="rides", trade="carousel")
    B.spot("staff_carousel_operator", "work", (*F.p(0.0, -0.55)[:2], 0.0), face)
    B.spot("carousel_view", "stand", (*F.p(0.0, 1.0)[:2], 0.0), face)


def pingpong(B, s=54.75, t=0.05):
    gf, yaw = _gframe(s, t, 1)
    cx, cy = gf.p(0, 0)[:2]
    kit.pingpong_table(B, cx, cy, 0.0, yaw)
    B.thing("fair_pingpong", "play", (cx, cy, 0.85), yaw, what="table_tennis")
    d = B.detail
    lbox(d, "CT_Wood", gf, 1.55, 1.85, 1.1, 1.3, 0.0, 0.5)                                    # a box of bats and balls
    lbox(d, "CT_PlasterRed", gf, 1.58, 1.82, 1.12, 1.28, 0.5, 0.52)
    lbox(d, "CT_Wood", gf, 1.65, 1.77, 1.19, 1.21, 0.52, 0.64)
    blob(d, "CT_TileWhite", *gf.p(1.72, 1.25)[:2], 0.545, 0.02, 4)
    B.spot("pingpong_end_1", "stand", (*gf.p(-1.95, 0.0)[:2], 0.0), yaw + 90)
    B.spot("pingpong_end_2", "stand", (*gf.p(1.95, 0.0)[:2], 0.0), yaw - 90)


def dominoes_table(B, s=4.5, t=0.0):
    gf, yaw = _gframe(s, t, 1)
    cx, cy = gf.p(0, 0)[:2]
    tb = kit.table(B, cx, cy, 0.0, yaw, w=0.8, d=0.8, h=0.75)
    top = tb["top"]
    d = B.detail
    for k in range(7):                                                                       # a row of tiles, some face-up
        u = -0.3 + 0.085 * k
        lbox(d, "CT_TileWhite", gf, u, u + 0.04, 0.02, 0.1, top, top + 0.012)
        lbox(d, "CT_ConcreteDark", gf, u + 0.0, u + 0.04, 0.059, 0.061, top + 0.0, top + 0.0125)
    for k in range(5):
        lbox(d, "CT_Steel", gf, -0.25 + 0.07 * k, -0.21 + 0.07 * k, -0.3, -0.22, top, top + 0.012)
    for sg, nm in ((-1, "dom_seat_1"), (1, "dom_seat_2")):
        px, py = gf.p(0, sg * 0.66)[:2]
        face = yaw + (0 if sg < 0 else 180)                                                   # looking over the table
        kit.stool(B, px, py, 0.0, face, mat="CT_PlasterGreen", h=0.45)
        B.thing(nm, "seat", (px, py, 0.45), face, height=0.45)
        B.spot(nm, "seat", (px, py, 0.45), face)
    for sg in (-1, 1):
        px, py = gf.p(sg * 0.66, 0.0)[:2]
        kit.stool(B, px, py, 0.0, yaw + (90 if sg < 0 else -90), mat="CT_PlasterRed", h=0.45)
        B.spot("dom_seat_%d" % (3 if sg < 0 else 4), "seat", (px, py, 0.45), yaw + (90 if sg < 0 else -90))
    B.thing("fair_dominoes", "play", (cx, cy, top + 0.05), yaw, what="dominoes")


def shell_table(B, s=96.0, t=0.3):
    gf, yaw = _gframe(s, t, -1)
    d = B.detail
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        _bar(d, "CT_Wood", gf, (sx * 0.3, sy * 0.2, 0.0), (sx * 0.34, sy * 0.22, 0.78), 0.035, caps=True)
    lbox(B.m, "CT_PlasterGreen", gf, -0.36, 0.36, -0.26, 0.26, 0.76, 0.8)
    lbox(d, "CT_Paper", gf, -0.3, 0.3, -0.2, 0.2, 0.8, 0.805)
    for k, u in enumerate((-0.2, 0.0, 0.2)):                                                  # three cups, a pea under one
        x, y, _ = gf.p(u, 0.0)
        cyl(d, "CT_Brass", x, y, 0.805, 0.88, 0.055, 8, top=True)
        blob(d, "CT_Brass", x, y, 0.89, 0.025, 4)
    blob(d, "CT_PlasterRed", *gf.p(0.33, 0.1)[:2], 0.83, 0.022, 4)
    kit.stool(B, *gf.p(0.0, -0.75)[:2], 0.0, yaw, mat="CT_PlasterRed", h=0.5)
    B.thing("fair_shells", "play", gf.p(0.0, 0.0, 0.9), yaw, what="shells")
    _stand(B, gf, "shell_stand", 0.0, 0.95)


def clock_post(B, s=63.0, t=-0.1):
    """A street clock on a post in the middle of the walk: two dials back to back."""
    cx, cy = surf(s, t)
    x, y, tx, ty = at(s)
    fr = Fr(cx, cy, tx, ty)
    face = fr.yaw_dir(0, 1)
    d = B.detail
    F = MFr(cx, cy, face)
    lbox(d, "CT_SteelRed", F, -0.05, 0.05, -0.05, 0.05, 0.0, 2.9)
    lbox(d, "CT_Steel", F, -0.12, 0.12, -0.12, 0.12, 0.0, 0.12)
    lbox(d, "CT_SteelRed", F, -0.2, 0.2, -0.09, 0.09, 3.28, 3.34)
    for sg in (-1, 1):
        px, py, _ = F.p(0, 0.06 * sg)
        kit.wall_clock(B, px, py, 0.0, face + (0 if sg > 0 else 180), zc=3.0)
    blob(d, "CT_Brass", cx, cy, 3.4, 0.05, 5)
    B.thing("fair_clock", "board", F.p(0.0, 0.2, 3.0), face, topic="clock")


def festoon_switch(B, s, t, tid):
    cx, cy = surf(s, t)
    x, y, tx, ty = at(s)
    fr = Fr(cx, cy, tx, ty)
    face = fr.yaw_dir(0, 1 if t < 0 else -1)
    F = MFr(cx, cy, face)
    d = B.detail
    lbox(d, "CT_Wood", F, -0.06, 0.06, -0.06, 0.06, 0.0, 2.25)
    lbox(d, "CT_Steel", F, -0.13, 0.13, 0.06, 0.12, 1.65, 2.0)
    lbox(d, "CT_TileWhite", F, -0.08, 0.08, 0.12, 0.125, 1.8, 1.95)
    _bar(d, "CT_SteelRed", F, (0.0, 0.13, 1.74), (0.0, 0.2, 1.84), 0.02)
    blob(d, "CT_SteelRed", *F.p(0.0, 0.2)[:2], 1.86, 0.03, 4)
    B.thing(tid, "machine", F.p(0.0, 0.15, 1.85), face, what="switch", switch=30)


def planters(B):
    for s, t, tag in ((1.6, 0.5, 0), (1.6, -0.5, 1), (121.0, 0.4, 2), (121.0, -0.4, 3)):
        cx, cy = surf(s, t)
        x, y, tx, ty = at(s)
        kit.planter(B, cx, cy, 0.0, math.degrees(math.atan2(tx, -ty)) + 90, w=1.4, d=0.45, seed=tag)


def second_pass(B, G):
    strongman_extra(B, G["strong"], G["strong"].yaw)
    shooting_extra(B, G["gallery"], G["gallery"].yaw)
    ring_extra(B, G["ring"], G["ring"].yaw)
    lottery_extra(B, G["lottery"], G["lottery"].yaw)
    weigh_extra(B, G["weigh"], G["weigh"].yaw)
    photo_extra(B, G["photo"], G["photo"].yaw)
    carousel(B)
    pingpong(B)
    dominoes_table(B)
    shell_table(B)
    clock_post(B)
    festoon_switch(B, 2.6, 0.3, "festoon_switch_w")
    festoon_switch(B, 119.5, -0.35, "festoon_switch_e")
    planters(B)


def fair_games(B):
    G = {}
    G["strong"] = strongman_tower(B)
    G["ring"] = ring_toss_table(B)
    G["lottery"] = lottery_drum(B)
    G["gallery"] = shooting_booth(B)
    chalk_board(B)
    G["photo"] = photographer(B)
    G["weigh"] = weighing_chair(B)
    fortune_automaton(B)
    lost_sandal(B)
    for i, s in enumerate((9, 21, 31, 43, 57, 69, 83, 95, 111)):
        bunting_line(B, float(s), seed=i)
    # people watch from the far side of the walk
    B.spot("fair_view_1", "view", (*surf(60.0, 0.1), 0.0), 0.0)
    B.spot("fair_view_2", "view", (*surf(30.0, 0.0), 0.0), 0.0)
    second_pass(B, G)


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
    fair_games(B)
    fair_board(B, 52.0, -0.5, "fair_board", "fair")
    fair_board(B, 100.0, 0.5, "fair_programme", "cinema", posters=True)
    # benches
    k = 0
    for ss, t in [(12, -0.35), (30, 0.4), (38, -0.4), (56, 0.35), (60, -0.35), (79, -0.4), (84, 0.4), (103.6, 0.35), (116, -0.4), (8, 0.45)]:
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
