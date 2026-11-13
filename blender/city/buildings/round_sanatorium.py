"""Round sanatorium («Санаторий «Корона»»): Soviet modernist resort ring, an original design.

In the spirit of the round sanatorium buildings of the 1980s Crimean / Black Sea coast: a ring (44 m outer
diameter at the balcony tips, 10 m deep) of 5 storeys of guest rooms, floating over a lower podium (R 18 m) on
cantilevers and slanting concrete struts. The ring is cut into 24 room bays; each bay's outer wall is a long
angled window face turned 40 degrees from the radial, so the outline is a sawtooth "crown"; the triangular notch in
front of every window face is that room's balcony (slab with railing), the short return wall of the neighbouring
bay closes it and is painted blue / yellow / pink in turn. Light concrete + white + colour.

Size 44 x 44 m (plot 60 x 60), height ~22 m. 5 storeys x 24 bays = 120 flats ("<storey>-<nn>", 1-2 beds), each with
a living-room window and a balcony-door window (240 flat windows) + corridor windows towards the courtyard.
Podium: solid blue-tile wall, except the ground hall in the 90 degree sector facing the street: ENTERABLE lobby
with the main entrance (under the first balcony tips), reception counter, benches, a dining area; floor
CT_Floor, walls CT_Interior, ceiling; big windows to the sea side and to the courtyard.
Courtyard (R 12 m): lawn, fountain basin, trees (not reachable tonight).
Spots: counter (reception), bench x2, table x3 (dining). Entrance "main" (2.4 m, double leaf).
Ideas for later: garden door from the hall, roof solarium, a sea-water pool in the courtyard.
Material requests: none.
"""
import math
import random

from citykit import box, prism, ring, Facade, Mesh, wall_with_holes, DOOR_W
import geom
from furn_int2 import Furn
import interior_kit

INFO = {
    "name_ru": "Санаторий «Корона»", "name_en": "Crown Sanatorium",
    "style": "soviet modernism", "use": "residential",
    "inspired_by": "1980s round resort buildings of the Crimean / Black Sea coast (ring sanatoria on stilts), "
                   "Soviet late-modernist concrete with painted accents; the sawtooth crown of angled balcony "
                   "bays and all proportions are our own",
}

CX, CY = 0.0, 26.0
R_IN, R_P, R_BODY, R_TIP = 12.0, 18.0, 18.6, 22.0
NB = 24
HB = 7.5                    # half bay angle, degrees
DELTA = 0.6                 # the tip leads the bay start by this angle
STH = 3.1                   # storey height
Z0 = 4.5                    # underside of the ring body (top of the podium)
NS = 5
ZT = Z0 + NS * STH          # roof level = 20.0


def pol(R, deg, cx=CX, cy=CY):
    a = math.radians(deg)
    return (cx + R * math.cos(a), cy + R * math.sin(a))


def _orient(pts, want):
    n = geom.cross(geom.sub(pts[1], pts[0]), geom.sub(pts[2], pts[0]))
    if n[0] * want[0] + n[1] * want[1] + n[2] * want[2] < 0:
        return pts[::-1]
    return pts


def hexa(m, mat, v):
    c = tuple(sum(p[i] for p in v) / 8 for i in range(3))
    for q in [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]:
        pts = [v[i] for i in q]
        fc = tuple(sum(p[i] for p in pts) / 4 for i in range(3))
        m.face(mat, _orient(pts, tuple(fc[i] - c[i] for i in range(3))))


def beam(m, mat, a, b, w, zlo, zhi):
    L = math.hypot(b[0] - a[0], b[1] - a[1])
    hx, hy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
    sx, sy = -hy, hx
    ringc = [(-w / 2, zlo), (w / 2, zlo), (w / 2, zhi), (-w / 2, zhi)]
    s0 = [(a[0] + sx * l, a[1] + sy * l, a[2] + vz) for l, vz in ringc]
    s1 = [(b[0] + sx * l, b[1] + sy * l, b[2] + vz) for l, vz in ringc]
    hexa(m, mat, [s0[0], s0[1], s1[1], s1[0], s0[3], s0[2], s1[2], s1[3]])


def inset_poly(poly, d):
    """CCW polygon moved inward by d (mitred)."""
    n = len(poly)
    lines = []
    for i in range(n):
        p, q = poly[i], poly[(i + 1) % n]
        L = math.hypot(q[0] - p[0], q[1] - p[1])
        ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L
        nx, ny = -uy, ux                       # left = inward for CCW
        lines.append(((p[0] + nx * d, p[1] + ny * d), (ux, uy)))
    out = []
    for i in range(n):
        (p1, d1), (p2, d2) = lines[i - 1], lines[i]
        den = d1[0] * d2[1] - d1[1] * d2[0]
        if abs(den) < 1e-9:
            out.append(p2)
            continue
        t = ((p2[0] - p1[0]) * d2[1] - (p2[1] - p1[1]) * d2[0]) / den
        out.append((p1[0] + d1[0] * t, p1[1] + d1[1] * t))
    return out


def tri_orient_up(pts2, z):
    area = sum(pts2[i][0] * pts2[(i + 1) % len(pts2)][1] - pts2[(i + 1) % len(pts2)][0] * pts2[i][1] for i in range(len(pts2)))
    p = pts2 if area > 0 else pts2[::-1]
    return [(x, y, z) for x, y in p]


CEIL = Z0 - 0.01                       # underside of the hall ceiling
PIERS = [-135.0 + 90.0 * i / 7 for i in range(8)]      # angles of the panel joints (solid piers between the windows)
R_LO, R_HI = 12.3, 17.7                # radial span of a partition (its ends hide inside the walls)


def _yout(th):
    """Kit yaw of a piece at polar angle th that faces the street wall (away from the courtyard centre)."""
    return (th + 90.0) % 360.0


def _yin(th):
    return (th - 90.0) % 360.0


def _ytan(th):
    """Facing along the arc toward growing angle (east)."""
    return (th + 180.0) % 360.0


def _tan(th):
    a = math.radians(th)
    return (-math.sin(a), math.cos(a))


def _at(R, th, s=0.0):
    x, y = pol(R, th)
    tx, ty = _tan(th)
    return (x + s * tx, y + s * ty)


def _dth(s, R):
    """Angle in degrees that spans s metres along the arc of radius R."""
    return math.degrees(s / R)


def _fy(dx, dy):
    return math.degrees(math.atan2(dx, -dy)) % 360.0


def _partition(B, th, gaps=(), t=0.14, lo="CT_TileWhite", hi="CT_Interior", low=1.5, lintel=2.2):
    """A radial partition wall at polar angle th across the hall (R_LO..R_HI), solid (B.m), with doorways
    gaps = [(R0, R1, head_z)]: lower band tile, upper plaster, a lintel over every gap, wooden door casing."""
    p0, p1 = pol(R_LO, th), pol(R_HI, th)
    L = math.dist(p0, p1)
    dx, dy = (p1[0] - p0[0]) / L, (p1[1] - p0[1]) / L
    nx, ny = -dy * t / 2, dx * t / 2

    def rect(a, b, e=0.0):
        ax, ay = p0[0] + dx * a, p0[1] + dy * a
        bx, by = p0[0] + dx * b, p0[1] + dy * b
        k = nx * (1 + e / (t / 2)) if e else nx
        kk = ny * (1 + e / (t / 2)) if e else ny
        return [(ax - k, ay - kk), (bx - k, by - kk), (bx + k, by + kk), (ax + k, ay + kk)]

    cuts = sorted((g[0] - R_LO, g[1] - R_LO, g[2]) for g in gaps)
    pos = 0.0
    segs = []
    for a, b, hz in cuts:
        segs.append((pos, a))
        pos = b
    segs.append((pos, L))
    for a, b in segs:
        if b - a < 0.02:
            continue
        poly = rect(a, b)
        prism(B.m, lo, poly, 0.0, low, top=False, bottom=False)
        prism(B.m, hi, poly, low, CEIL, top=False, bottom=False)
    for a, b, hz in cuts:
        poly = rect(a, b)
        prism(B.m, hi, poly, hz, CEIL, top=False, bottom=False)
        # soffit and casing
        B.m.face("CT_Frame", [(poly[0][0], poly[0][1], hz), (poly[1][0], poly[1][1], hz), (poly[2][0], poly[2][1], hz),
                              (poly[3][0], poly[3][1], hz)][::-1])
        for (u0, u1) in ((a - 0.05, a), (b, b + 0.05)):
            prism(B.detail, "CT_Frame", rect(u0, u1, 0.025), 0.0, hz + 0.05, top=True, bottom=False)
        prism(B.detail, "CT_Frame", rect(a - 0.05, b + 0.05, 0.025), hz, hz + 0.07, top=True, bottom=True)


def _floor_sector(B, mat, th0, th1, r0, r1, z=0.008, n=6):
    """A thin floor covering over an annular sector (detail, no collision)."""
    a = [(pol(r1, th0 + (th1 - th0) * i / n)) for i in range(n + 1)]
    b = [(pol(r0, th0 + (th1 - th0) * i / n)) for i in range(n + 1)][::-1]
    poly = a + b
    B.detail.face(mat, [(x, y, z) for x, y in poly])


def _edge_pt(q, e, u):
    """Point u metres along the e-th wall of the inset hall outline q; returns ((x, y), yaw of a piece facing the room)."""
    n = len(q)
    a, b = q[e], q[(e + 1) % n]
    L = math.dist(a, b)
    dx, dy = (b[0] - a[0]) / L, (b[1] - a[1]) / L
    return (a[0] + dx * u, a[1] + dy * u), _fy(-dy, dx), L


def _thing(B, tid, kind, xy, z, yaw, **props):
    B.thing(tid, kind, (xy[0], xy[1], z), yaw, **props)


def _furnish_hall(B, hall, t):
    """The ground hall (annular sector R 12..18, angles -135..-45, ceiling 4.5), seven bays between the piers:
    bay 0 winter garden, bays 1-2 dining hall, bays 3-4 lobby (entrance in bay 3, reception in bay 4), bay 5 the
    mineral-water pump room, bay 6 the treatment room. Radial partitions stand on the piers."""
    q = inset_poly(hall, t)
    d = B.detail
    kit = interior_kit
    P = PIERS
    th_g0, th_g1 = P[0], P[1]
    th_d0, th_d1 = P[1], P[3]
    th_l0, th_l1 = P[3], P[5]
    th_p0, th_p1 = P[5], P[6]
    th_t0, th_t1 = P[6], P[7]

    # ------------------------------------------------------------------------------------------ partitions
    _partition(B, P[1], gaps=[(14.0, 16.2, 2.55)], lo="CT_Interior")                      # garden | dining: wide archway
    _partition(B, P[5], gaps=[(14.45, 15.55, 2.2)])                                       # lobby | pump room: a door
    _partition(B, P[6], gaps=[(13.0, 14.0, 2.2)], lo="CT_Tile")                           # pump room | treatment room

    # ------------------------------------------------------------------------------------------ floors
    _floor_sector(B, "CT_Stone", th_g0 + 0.5, th_g1 - 0.2, 12.45, 17.45)                  # garden paving
    _floor_sector(B, "CT_TileWhite", th_p0 + 0.7, th_p1 - 0.6, 12.45, 17.45)               # pump room tiles
    _floor_sector(B, "CT_TileWhite", th_t0 + 0.6, th_t1 - 0.9, 12.45, 17.45)               # treatment room tiles
    _floor_sector(B, "CT_Tile", th_p0 + 0.7, th_p1 - 0.6, 14.75, 15.25, z=0.012)           # blue runner strip

    # ============================================================================================ LOBBY
    thc = (th_l0 + th_l1) / 2
    # reception desk on the courtyard side of bay 4; the clerk stands behind it facing the street wall
    th_r = -77.0
    x, y = pol(14.0, th_r)
    r = desk = kit.reception_desk(B, x, y, yaw=_yout(th_r), w=2.4, front="CT_PlasterBlue")
    B.spot("reception", "counter", r["staff"][:2] + (0.0,), _yout(th_r))
    B.thing("reception", "counter", (x, y, 1.1), _yout(th_r), role="receptionist", service="reception",
            shop="sanatorium_kiosk")
    ax, ay = r["on"][0], r["on"][1]
    kit.telephone(B, ax + 0.0, ay + 0.05, r["top"], yaw=_yout(th_r) + 30)
    kit.cup(B, ax + 1.0 * math.cos(math.radians(th_r + 90)) * 0.0 + 0.25, ay + 0.15, r["top"], yaw=40)
    # a free newspaper and a postcard rack on the desk
    nx_, ny_ = _at(14.0, th_r, 0.55)
    kit.newspaper(B, nx_, ny_ - 0.0, 1.1, yaw=_yout(th_r) + 15, cellname=None, seed=3)
    B.thing("lobby_paper", "take", (nx_, ny_, 1.12), _yout(th_r), item="daily_paper")
    kit.table_lamp(B, *_at(14.0, th_r, 0.0), z=1.1, yaw=_yout(th_r))
    kit.chair(B, *_at(13.1, th_r, 0.0), yaw=_yout(th_r), kind="tube", mat="CT_PlasterRed")
    pt, yw, L = _edge_pt(q, 10, L_of(q, 10) / 2)
    kit.picture(B, pt[0], pt[1], yaw=yw, name="painting_sea_night", w=0.9, zc=4.12)
    # lounge corner on the courtyard side of bay 3: a sofa against the windows, a low table, two armchairs, a rug
    thl = -91.5
    kit.rug(B, *pol(13.7, thl), w=2.7, round=True, seed=2)
    s3 = kit.sofa(B, *pol(12.95, thl), yaw=_yout(thl), seats=2, mat="CT_PlasterBlue")
    B.spot("lounge_3", "seat", s3["seats"][0][:3], s3["seats"][0][3])
    B.thing("lobby_sofa", "seat", s3["seats"][0][:3], s3["seats"][0][3], height=0.45)
    lx, ly = pol(14.0, thl)
    ct = kit.coffee_table(B, lx, ly, yaw=_yout(thl) + 90, w=0.9, d=0.5)
    kit.newspaper(B, lx - 0.1, ly, ct["top"], yaw=20, seed=5)
    kit.vase(B, lx + 0.15, ly + 0.05, ct["top"], seed=2)
    ths = (thl - _dth(1.2, 14.0), thl + _dth(1.2, 14.0))
    a1 = kit.armchair(B, *pol(14.0, ths[0]), yaw=_ytan(ths[0]), mat="CT_PlasterRed")
    a2 = kit.armchair(B, *pol(14.0, ths[1]), yaw=ths[1], mat="CT_PlasterYellow")
    B.spot("lounge_1", "seat", a1["seats"][0][:3], a1["seats"][0][3])
    B.spot("lounge_2", "seat", a2["seats"][0][:3], a2["seats"][0][3])
    B.thing("lobby_lounge_a", "seat", a1["seats"][0][:3], a1["seats"][0][3], height=0.44)
    B.thing("lobby_lounge_b", "seat", a2["seats"][0][:3], a2["seats"][0][3], height=0.44)
    kit.floor_lamp(B, *pol(12.9, thl - _dth(1.75, 12.9)), yaw=0)
    # benches either side of the entrance
    for sgn, sid, tid in ((-1, "bench_-4", "lobby_bench_w"), (1, "bench_4", "lobby_bench_e")):
        thb = -90.0 + sgn * _dth(2.9, 17.0)
        bx, by = pol(16.95, thb)
        bn = kit.bench(B, bx, by, yaw=_yin(thb), w=1.4, kind="park")
        B.spot(sid, "bench", bn["seats"][0][:3], _yin(thb))
        B.thing(tid, "seat", bn["seats"][0][:3], _yin(thb), height=0.45)
    # east partition (lobby face, normal facing -theta): coat rack, a notice board and a health poster
    for (rr, cls) in ((16.5, "coat"),):
        cx_, cy_ = pol(rr, th_l1)
        tx, ty = _tan(th_l1)
        cr = kit.coat_rack(B, cx_ - 0.07 * tx - 0.32 * tx, cy_ - 0.07 * ty - 0.32 * ty, yaw=th_l1, w=1.7, coats=0.6)
        B.thing("cloakroom", "counter", (cr["pos"][0], cr["pos"][1], 1.2), th_l1, role="receptionist",
                service="cloakroom")
    nbx, nby = _at(13.2, th_l1, -0.075)
    kit.notice_board(B, nbx, nby, yaw=th_l1, w=1.0, zc=1.65, seed=4)
    B.thing("lobby_board", "board", (nbx, nby, 1.65), th_l1, topic="rules")
    px_, py_ = _at(14.0, th_l1, -0.075)
    kit.poster(B, px_ + 0 * 1, py_, yaw=th_l1, name="poster_health", w=0.5, zc=2.2)
    # sign above the entrance and pictures above the lobby windows
    pt, yw, L = _edge_pt(q, 3, 2.05)
    kit.sign_plate(B, pt[0], pt[1], yaw=yw, name="sign_quiet", w=1.1, zc=3.0)
    for e, nm in ((4, "painting_dunes"), (2, "painting_sea_calm")):
        pt, yw, L = _edge_pt(q, e, L_of(q, e) / 2)
        kit.picture(B, pt[0], pt[1], yaw=yw, name=nm, w=0.9, zc=4.12)
    B.thing("lobby_painting", "board", (pt[0], pt[1], 4.1), yw, topic="painting")
    for e in (2, 4):
        pt, yw, L = _edge_pt(q, e, L_of(q, e) / 2)
        kit.radiator(B, pt[0], pt[1], yaw=yw, w=1.3, h=0.42)
    # plants either side of the door
    kit.plant(B, *pol(16.8, -90.0 - _dth(4.35, 16.8)), kind="ficus", pot="CT_PlasterRed", h=1.7, seed=1)

    # ============================================================================================ DINING HALL
    centres = (-117.0, -108.7, -100.2)
    paints = ("CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterPink")
    for i, thd in enumerate(centres):
        x, y = pol(14.9, thd)
        tb = kit.restaurant_table(B, x, y, yaw=_yout(thd), w=0.95, d=0.95, places=4, seed=i)
        ch = kit.around_table(B, x, y, 0.95, 0.95, yaw=_yout(thd), per_side=(1, 1, 1, 1), kind="wood",
                              mat=paints[i], chair_fn=None)
        B.spot("dining_%d" % i, "table", ch[0]["seats"][0][:3], ch[0]["seats"][0][3])
        B.thing("dining_table_%d" % i, "seat", ch[0]["seats"][0][:3], ch[0]["seats"][0][3],
                height=0.45)
        kit.pendant(B, x, y, CEIL, drop=1.15, kind="cone", light=(i == 1), lumens=900, radius=7.5)
    # serving pass-through: a buffet on the street wall next to the garden archway, menu board and cloth
    thb = -118.8
    bx, by = pol(17.0, thb)
    bf = kit.shop_counter(B, bx, by, yaw=_yin(thb), w=1.8, d=0.55, front="CT_PlasterYellow")
    kit.samovar(B, bx - 0.4 * math.cos(math.radians(thb + 90)), by, bf["top"], yaw=_yin(thb))
    kit.plates(B, *_at(17.0, thb, 0.35), z=bf["top"], yaw=thb, n=4)
    B.thing("dining_buffet", "counter", (bx, by, 1.05), _yin(thb), role="receptionist", service="bar")
    # partition between garden and dining (dining face, faces +theta): the menu and a still life
    wx, wy = _at(16.95, th_d0, 0.075)
    kit.menu_board(B, wx, wy, yaw=_ytan(th_d0), name="menu_0", w=0.62, zc=1.7)
    B.thing("dining_menu", "board", (wx, wy, 1.7), _ytan(th_d0), topic="prices")
    wx, wy = _at(13.1, th_d0, 0.075)
    kit.picture(B, wx, wy, yaw=_ytan(th_d0), name="painting_still_life", w=0.7, zc=1.75)
    for e, nm in ((1, "painting_dunes"), (2, "map_coast")):
        pt, yw, L = _edge_pt(q, e, L_of(q, e) / 2)
        (kit.picture if e == 1 else kit.poster)(B, pt[0], pt[1], yaw=yw, name=nm, w=0.9, zc=4.12)
    # curtains on the courtyard windows and radiators below
    for j, pn in enumerate((5, 4)):
        e = 8 + (6 - pn)
        pt, yw, L = _edge_pt(q, e, L_of(q, e) / 2)
        kit.curtains(B, pt[0], pt[1], yaw=yw, w=1.6, h=3.0, mat=("CT_PlasterRed", "CT_PlasterBlue")[j])
    for pn in (1, 2):
        pt, yw, L = _edge_pt(q, 8 + (6 - pn), L_of(q, 8 + (6 - pn)) / 2)
        kit.radiator(B, pt[0], pt[1], yaw=yw, w=1.3, h=0.42)
    # planters along the courtyard windows
    for thp in (-120.2, -112.8, -98.0):     # -104.6: the library trolley now stands there
        kit.plant(B, *pol(12.85, thp), kind=("fern", "ficus")[int(thp) % 2], pot="CT_TileWhite", h=1.1, seed=int(thp))
    # a dado of floor border
    _floor_sector(B, "CT_ConcreteDark", th_d0 + 0.4, th_d1 - 0.2, 12.45, 12.7, z=0.01)

    # ============================================================================================ WINTER GARDEN
    g = (th_g0 + th_g1) / 2
    # piano against the solid west end wall (radial wall, face 0.4 m off the radial line)
    t0 = _tan(th_g0)
    pxy = (pol(15.0, th_g0)[0] + t0[0] * 0.75, pol(15.0, th_g0)[1] + t0[1] * 0.75)
    pn = kit.piano(B, pxy[0], pxy[1], yaw=_ytan(th_g0))
    B.thing("piano", "play", (pxy[0], pxy[1], 1.0), _ytan(th_g0), what="piano")
    stx, sty = pxy[0] + t0[0] * 0.8, pxy[1] + t0[1] * 0.8
    kit.stool(B, stx, sty, yaw=th_g0, mat="CT_PlasterRed", h=0.5)
    B.spot("piano", "seat", (stx, sty, 0.0), th_g0)
    kit.rug(B, *_at(15.3, g, 0.2), w=2.0, d=2.6, yaw=_yout(g), seed=3)
    gb = kit.table_round(B, *_at(15.3, g, 0.2), r=0.35, h=0.5)
    kit.teapot(B, *_at(15.3, g, 0.2), z=gb["top"], yaw=10)
    kit.cup(B, *_at(15.3, g, 0.45), z=gb["top"])
    kit.cup(B, *_at(15.3, g, -0.05), z=gb["top"], yaw=90)
    c1 = kit.armchair(B, *_at(14.35, g, 0.25), yaw=_yout(g), mat="CT_PlasterGreen")      # faces the street wall
    c2 = kit.armchair(B, *_at(16.3, g, 0.15), yaw=_yin(g), mat="CT_PlasterGreen")       # faces the courtyard
    B.thing("garden_chair_a", "seat", c1["seats"][0][:3], c1["seats"][0][3], height=0.44)
    B.thing("garden_chair_b", "seat", c2["seats"][0][:3], c2["seats"][0][3], height=0.44)
    gbn = kit.bench(B, *_at(17.0, g, 0.6), yaw=_yin(g), w=1.5, kind="park")
    B.thing("garden_bench", "seat", gbn["seats"][0][:3], _yin(g), height=0.45)
    # the plants: palms in the corners, ficus and monstera, ferns on the sill, cacti by the arch
    plan = [("palm", 12.9, th_g0 + 4.0, 2.5, "CT_PlasterRed"), ("palm", 17.0, th_g0 + 3.4, 2.6, "CT_PlasterGreen"),
            ("palm", 17.1, th_g1 - 3.8, 2.4, "CT_PlasterYellow"), ("palm", 12.95, th_g1 - 4.8, 2.3, "CT_PlasterBlue"),   # QA: leaves went through the walls
            ("ficus", 13.4, th_g0 + 0.9, 2.0, "CT_TileWhite"), ("ficus", 16.4, th_g0 + 2.5, 1.9, "CT_PlasterRed"),
            ("monstera", 12.9, g + 0.2, 1.4, "CT_PlasterGreen"), ("monstera", 17.1, g - 0.4, 1.5, "CT_TileWhite"),
            ("fern", 13.0, g + 2.6, 0.9, "CT_PlasterYellow"), ("fern", 13.0, g - 2.6, 0.9, "CT_PlasterBlue"),
            ("fern", 17.2, g + 2.2, 0.9, "CT_PlasterRed"), ("cactus", 13.8, th_g1 - 0.5, 0.7, "CT_PlasterPink"),
            ("cactus", 16.3, th_g1 - 0.45, 0.7, "CT_PlasterYellow"), ("ficus", 12.9, g - 1.2, 1.7, "CT_PlasterPink"),
            ("fern", 15.2, g + 2.9, 0.9, "CT_TileWhite")]
    for k, (kind, R, th_, h, pot) in enumerate(plan):
        kit.plant(B, *pol(R, th_), kind=kind, pot=pot, h=h, seed=k)
    B.thing("garden_plants", "board", (*pol(15.0, g + 1.0), 1.4), _yout(g), topic="garden")
    for R, nm, w_, zc_ in ((15.0, "painting_pines", 0.8, 2.3), (13.0, "poster_sun_1", 0.55, 1.7)):
        ex, ey = pol(R, th_g0)
        fn = kit.picture if nm.startswith("painting") else kit.poster
        fn(B, ex + t0[0] * 0.4, ey + t0[1] * 0.4, yaw=_ytan(th_g0), name=nm, w=w_, zc=zc_)

    # ============================================================================================ PUMP ROOM
    pc = (th_p0 + th_p1) / 2 + 0.3
    px_, py_ = pol(15.45, pc)
    bc_ = kit.bar_counter(B, px_, py_, yaw=_yin(pc), w=2.3, d=0.6, taps=3, front="CT_PlasterBlue", top="CT_Steel")
    for k in range(5):
        kit.glass(B, *_at(15.45, pc, -0.9 + 0.45 * k), z=bc_["top"], yaw=0)
    kit.bottles(B, *_at(15.5, pc, 0.75), z=bc_["top"], n=3, seed=7)
    B.thing("pump_room", "machine", (px_, py_, 1.1), _yin(pc), what="pump_room")
    B.spot("pump_attendant", "work", (*_at(16.55, pc, 0.0), 0.0), _yin(pc))
    # drinkers' benches along the courtyard windows, a round table and the wall gear
    pb = kit.bench(B, *_at(12.95, pc, -0.2), yaw=_yout(pc), w=1.6, kind="park")
    B.thing("pump_bench", "seat", pb["seats"][0][:3], _yout(pc), height=0.45)
    tr = kit.table_round(B, *_at(14.2, pc, 0.75), r=0.4, h=0.78)
    kit.cup(B, *_at(14.2, pc, 0.75), z=tr["top"])
    kit.teapot(B, *_at(14.25, pc, 0.62), z=tr["top"], yaw=20)
    for sg in (-1, 1):
        kit.stool(B, *_at(14.2, pc, 0.75 + sg * 0.62), mat="CT_PlasterBlue", h=0.45)
    kit.rug(B, *_at(14.7, pc, -0.3), w=1.0, d=2.8, yaw=_yin(pc) + 90, seed=6)
    # west partition (pump room face, faces +theta): the price list and schedule
    wx, wy = _at(13.85, th_p0, 0.075)
    kit.menu_board(B, wx, wy, yaw=_ytan(th_p0), name="price_0", w=0.5, zc=1.6)
    B.thing("pump_prices", "board", (wx, wy, 1.6), _ytan(th_p0), topic="prices")
    wx, wy = _at(16.4, th_p0, 0.075)
    kit.poster(B, wx, wy, yaw=_ytan(th_p0), name="chart_calendar", w=0.6, zc=1.7)
    B.thing("pump_schedule", "board", (wx, wy, 1.7), _ytan(th_p0), topic="timetable")
    # east partition (pump face, faces -theta): a tile mosaic panel, a clock
    wx, wy = _at(16.4, th_p1, -0.075)
    kit.mosaic_panel(B, wx, wy, yaw=th_p1, name="tile", w=1.1, h=1.1, zc=1.6, seed=3)
    wx, wy = _at(12.95, th_p1, -0.075)
    kit.poster(B, wx, wy, yaw=th_p1, name="poster_health", w=0.5, zc=1.8)
    wx, wy = _at(14.7, th_p1, -0.075)
    kit.wall_clock(B, wx, wy, yaw=th_p1, zc=2.5)
    for e, nm in ((5, "painting_sea_storm"), (6, "map_town")):
        pt, yw, L = _edge_pt(q, e, L_of(q, e) / 2)
        (kit.picture if e == 5 else kit.poster)(B, pt[0], pt[1], yaw=yw, name=nm, w=0.9, zc=4.12)
    kit.plant(B, *pol(17.15, pc + 0.4), kind="cactus", pot="CT_TileWhite", h=1.0, seed=11)      # QA: the palm leaves cut the back bar
    kit.plant(B, *pol(12.9, pc + 3.3), kind="fern", pot="CT_PlasterGreen", h=0.9, seed=12)
    kit.pendant(B, *pol(14.8, pc - 1.5), CEIL, drop=1.1, kind="opal", light=True, lumens=900, radius=7)

    # ============================================================================================ TREATMENT ROOM
    tc = (th_t0 + th_t1) / 2 - 0.2
    # couches lie along the radial direction, heads to the street wall, behind a folding screen
    for k, (s, who) in enumerate(((0.85, "a"), (-0.85, "b"))):
        cx_, cy_ = _at(15.2, tc, s)
        kit.medical_couch(B, cx_, cy_, yaw=tc)
        B.thing("couch_" + who, "bed", (cx_, cy_, 0.7), tc, height=0.7, rest="nap")
    kit.screen_divider(B, *_at(15.6, tc, 0.0), yaw=tc, mat="fabric_2")
    # the instrument table, scales, glass cabinet and a sink along the courtyard wall
    tb = kit.lab_bench(B, *_at(13.1, tc, 1.0), yaw=_yout(tc), w=1.4, d=0.6, seed=2)
    sc = kit.scales(B, *_at(13.1, tc, -0.7), yaw=_yout(tc))
    B.thing("treat_scales", "machine", (sc["pos"][0], sc["pos"][1], 1.1), _yout(tc), what="scales")
    dc = kit.display_case(B, *_at(16.9, tc, -1.15), yaw=_yin(tc), w=0.9, d=0.45, goods="pharmacy", seed=3)
    B.thing("treat_cabinet", "shelf", (dc["pos"][0], dc["pos"][1], 1.0), _yin(tc), topic="medicine")
    # east end wall (solid, face 0.4 m off the radial line, room on the -theta side)
    t1 = _tan(th_t1)
    for R, nm, w_ in ((14.2, "chart_anatomy", 0.8), (16.1, "poster_health", 0.55)):
        ex, ey = pol(R, th_t1)
        kit.poster(B, ex - t1[0] * 0.4, ey - t1[1] * 0.4, yaw=th_t1, name=nm, w=w_, zc=1.7)
    B.thing("treat_chart", "board", (*[v - 0.4 * tt for v, tt in zip(pol(14.2, th_t1), t1)], 1.7), th_t1, topic="health")
    wb = kit.wall_bars(B, *[v - 0.4 * tt for v, tt in zip(pol(15.5, th_t1), t1)], yaw=th_t1, w=0.9, h=2.4)
    B.thing("treat_wall_bars", "sport", (*[v - 0.4 * tt for v, tt in zip(pol(15.5, th_t1), t1)], 1.2), th_t1,
            what="wall_bars")
    # west face of the P6 partition (treatment side, faces +theta): eye chart and a mirror
    wx, wy = _at(15.8, th_t0, 0.075)
    kit.eye_chart(B, wx, wy, yaw=_ytan(th_t0), zc=1.6)
    wx, wy = _at(17.0, th_t0, 0.075)
    kit.mirror(B, wx, wy, yaw=_ytan(th_t0), zc=1.5, w=0.5, h=0.9)
    kit.pendant(B, *pol(15.2, tc), CEIL, drop=1.2, kind="cone", light=True, lumens=900, radius=7)
    kit.plant(B, *pol(16.95, tc + 4.2), kind="ficus", pot="CT_TileWhite", h=1.4, seed=21)     # was on the lab bench

    # ------------------------------------------------------------------------------------------ lobby/dining light
    kit.pendant(B, *pol(15.0, -92.0), CEIL, drop=1.2, kind="opal", light=True, lumens=900, radius=8)
    kit.pendant(B, *pol(15.0, -80.0), CEIL, drop=1.2, kind="opal", light=True, lumens=900, radius=8)
    kit.pendant(B, *pol(15.0, -110.0), CEIL, drop=1.2, kind="opal", light=True, lumens=900, radius=8)
    kit.pendant(B, *pol(15.0, -128.0), CEIL, drop=1.4, kind="opal", light=True, lumens=900, radius=8)
    for tl in (-100.2, -117.0):
        pass

    # ------------------------------------------------------------------------------------------ skirting, doormat
    f = Furn(B)
    nh = len(q)
    for i in range(nh):
        a, b = q[i], q[(i + 1) % nh]
        if i == 3:
            mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
            ux, uy = (b[0] - a[0]), (b[1] - a[1])
            L = math.hypot(ux, uy)
            ux, uy = ux / L, uy / L
            f.skirting([a, (mx - ux * 1.3, my - uy * 1.3)], h=0.13)
            f.skirting([(mx + ux * 1.3, my + uy * 1.3), b], h=0.13)
        else:
            f.skirting([a, b], h=0.13)
    box(d, "CT_PlasterGrey", -1.0, 1.0, 8.5, 9.5, 0.0, 0.014, faces="Z")
    _depth(B, q, desk, th_r, dc)
    msg = kit.check(B)
    for m_ in msg:
        print("KIT:", m_)


# ====================================================================================================== second pass
def _guest_book(B, x, y, z, yaw):
    """[prop] An open visitors' book with a pen, lying on the desk."""
    p = interior_kit._P(B, x, y, z, yaw, "guest_book")
    p.kind = "prop"
    p.box("CT_PlasterRed", -0.19, 0.19, -0.14, 0.14, 0.0, 0.022)
    p.box("CT_Paper", -0.175, -0.004, -0.125, 0.125, 0.022, 0.036)
    p.box("CT_Paper", 0.004, 0.175, -0.125, 0.125, 0.022, 0.034)
    p.rod("CT_Steel", (0.24, -0.06, 0.012), (0.3, 0.1, 0.012), 0.012, 6)
    return p.done(top=0.036)


def _trolley(B, x, y, yaw):
    """A three-shelf library trolley on four small wheels with a push handle; books on two shelves."""
    p = interior_kit._P(B, x, y, 0.0, yaw, "book_trolley")
    W, D = 0.8, 0.4
    for sx in (-W / 2 + 0.03, W / 2 - 0.03):
        for sy in (-D / 2 + 0.03, D / 2 - 0.03):
            p.rod("CT_Wood", (sx, sy, 0.07), (sx, sy, 0.86), 0.035, 4)
            p.cyl("CT_Rubber", sx, sy, 0.0, 0.07, 0.04, 6)
    for zz in (0.16, 0.48, 0.80):
        p.box("CT_WoodLight", -W / 2, W / 2, -D / 2, D / 2, zz, zz + 0.025)
    for zz in (0.185, 0.505):                           # low rails so the books do not slide off
        p.box("CT_Wood", -W / 2, W / 2, -D / 2, -D / 2 + 0.015, zz, zz + 0.07)
    p.rod("CT_Steel", (-W / 2 + 0.03, D / 2 - 0.03, 0.86), (-W / 2 + 0.03, D / 2 + 0.12, 0.95), 0.025, 6)
    p.rod("CT_Steel", (W / 2 - 0.03, D / 2 - 0.03, 0.86), (W / 2 - 0.03, D / 2 + 0.12, 0.95), 0.025, 6)
    p.rod("CT_Steel", (-W / 2 + 0.03, D / 2 + 0.12, 0.95), (W / 2 - 0.03, D / 2 + 0.12, 0.95), 0.03, 6)
    p.core(-W / 2 + 0.03, W / 2 - 0.03, -D / 2 + 0.03, D / 2 - 0.03, 0.482, 0.495)
    r = p.done(solid=True, top=0.825)
    for k, (zz, u) in enumerate(((0.185, -0.22), (0.185, 0.14), (0.505, -0.12), (0.505, 0.24))):
        wx, wy, _ = p.w(u, 0.0, 0.0)
        interior_kit.books_stack(B, wx, wy, zz + 0.0, yaw=yaw + 90 * (k % 2), n=3 + k % 2, seed=40 + k)
    return r


def _oxygen_unit(B, x, y, z, yaw):
    """[prop] A white oxygen cylinder with a brass valve, a rubber hose and a glass whisk jar, for the cocktail bar."""
    p = interior_kit._P(B, x, y, z, yaw, "oxygen_unit")
    p.kind = "prop"
    p.cyl("CT_SteelWhite", -0.25, 0.0, 0.0, 0.5, 0.075, 8, top=False)
    p.lathe("CT_SteelWhite", -0.25, 0.0, [(0.075, 0.5), (0.055, 0.56), (0.02, 0.6)], 8)
    p.cyl("CT_Brass", -0.25, 0.0, 0.6, 0.64, 0.025, 6)
    p.rod("CT_Rubber", (-0.25, 0.0, 0.62), (-0.1, -0.12, 0.42), 0.018, 5)
    p.rod("CT_Rubber", (-0.1, -0.12, 0.42), (0.1, -0.1, 0.2), 0.018, 5)
    p.cyl("CT_Glass", 0.1, -0.1, 0.0, 0.2, 0.065, 8, r1=0.075)
    p.cyl("CT_PlasterPink", 0.1, -0.1, 0.0, 0.1, 0.06, 8)
    p.cyl("CT_TileWhite", 0.1, -0.1, 0.1, 0.19, 0.063, 8)
    return p.done(top=0.64)


def _quartz_lamp(B, x, y, z, yaw):
    """[prop] A tabletop quartz lamp (a blue-white tube in a round reflector on a stand) and a pair of goggles."""
    p = interior_kit._P(B, x, y, z, yaw, "quartz_lamp")
    p.kind = "prop"
    p.cyl("CT_Steel", 0.0, 0.0, 0.0, 0.03, 0.12, 8)
    p.rod("CT_Steel", (0.0, 0.0, 0.03), (0.0, 0.0, 0.34), 0.03, 6)
    p.cyl("CT_SteelWhite", 0.0, -0.1, 0.30, 0.42, 0.2, 10, r1=0.13, top=False)
    p.cyl("CT_Glass", 0.0, -0.1, 0.32, 0.33, 0.12, 10)
    p.rod("CT_Neon", (-0.1, -0.2, 0.36), (0.1, -0.2, 0.36), 0.03, 6)
    p.box("CT_Rubber", 0.2, 0.3, -0.05, 0.02, 0.0, 0.012)
    p.cyl("CT_Glass", 0.225, -0.015, 0.012, 0.02, 0.025, 6)
    p.cyl("CT_Glass", 0.275, -0.015, 0.012, 0.02, 0.025, 6)
    return p.done(top=0.0)


def _depth(B, q, desk, th_r, dc):
    """The second, deeper pass: regime board, guest book, weekly menu, oxygen bar, chess corner, library trolley,
    plant labels, quartz lamp and check-up, the night radio of the paid couch, courtyard window, pump-room clock,
    the lobby's evening floor."""
    kit = interior_kit
    P = PIERS
    th_g1, th_p0, th_p1 = P[1], P[5], P[6]
    th_t0, th_t1 = P[6], P[7]
    tc = (th_t0 + th_t1) / 2 - 0.2
    pc = (th_p0 + th_p1) / 2 + 0.3

    # ---------------------------------------------------------------- lobby: the regime board by the door, the book
    pt, yw, L = _edge_pt(q, 3, L_of(q, 3) - 0.42)
    kit.notice_board(B, pt[0], pt[1], yaw=yw, w=0.55, zc=1.55, seed=8)
    B.thing("regime_board", "board", (pt[0], pt[1], 1.55), yw, topic="regime")
    pt, yw, L = _edge_pt(q, 3, 0.5)
    kit.wall_clock(B, pt[0], pt[1], yaw=yw, zc=2.55)
    gx, gy = _at(14.0, th_r, -0.78)
    _guest_book(B, gx, gy, desk["top"], yaw=_yout(th_r) - 12)
    B.thing("guest_book", "board", (gx, gy, desk["top"] + 0.1), _yout(th_r), topic="guestbook")
    # evening floor: a patch of parquet by the door, the gramophone's corner
    thd = -84.2
    kit.parquet(B, *pol(15.6, thd), yaw=_yout(thd), w=2.3, d=1.5)
    B.thing("lobby_dance", "machine", (*pol(15.6, thd), 0.6), _yout(thd), what="dance")
    B.spot("dance_floor", "stand", (*pol(15.6, thd), 0.0), _yout(thd))

    # ---------------------------------------------------------------- dining hall
    thm = -114.4
    mx, my = pol(16.85, thm)
    kit.sandwich_board(B, mx, my, yaw=_yin(thm), w=0.55, name="menu")
    B.thing("menu_week", "board", (mx, my, 0.75), _yin(thm), topic="menu")
    # the oxygen cocktail bar on the courtyard windows
    tho = -108.6
    ox, oy = pol(12.95, tho)
    ob = kit.bar_counter(B, ox, oy, yaw=_yout(tho), w=1.0, d=0.5, taps=0, front="CT_PlasterPink", top="CT_Steel")
    _oxygen_unit(B, *_at(12.95, tho, -0.3), z=ob["top"], yaw=_yout(tho))
    for k in range(3):
        kit.glass(B, *_at(13.1, tho, 0.15 + 0.17 * k), z=ob["top"], yaw=0)
    B.thing("oxygen_bar", "machine", (ox, oy, ob["top"] + 0.15), _yout(tho), what="oxygen_bar")
    B.spot("oxygen_bar", "stand", (*pol(13.75, tho), 0.0), _yin(tho))
    kit.pendant(B, *pol(13.1, tho), CEIL, drop=1.5, kind="cone", light=True, lumens=700, radius=5)
    # the holidaymakers' chess corner on the street wall
    thc = -110.2
    cx_, cy_ = pol(16.75, thc)
    kit.chess_table(B, cx_, cy_, yaw=_ytan(thc), seed=5)
    B.thing("chess_corner", "play", (cx_, cy_, 0.8), _ytan(thc), what="chess")
    for sg, nm, yy in ((-1, "chess_1", _ytan(thc)), (1, "chess_2", thc)):
        sx_, sy_ = _at(16.75, thc, sg * 0.66)
        kit.chair(B, sx_, sy_, yaw=yy, kind="wood", mat="CT_PlasterRed" if sg < 0 else "CT_PlasterBlue")
        B.spot(nm, "seat", (sx_, sy_, 0.45), yy)
    # the library trolley where the fourth dining plant used to stand (courtyard window)
    thr = -103.6
    tx_, ty_ = pol(12.9, thr)
    _trolley(B, tx_, ty_, _yout(thr))
    B.thing("book_trolley", "shelf", (tx_, ty_, 0.85), _yout(thr), topic="trolley")
    B.spot("trolley", "stand", (*pol(13.7, thr), 0.0), _yin(thr))

    # ---------------------------------------------------------------- winter garden: plant labels, a reading chair
    g = (PIERS[0] + PIERS[1]) / 2
    lx, ly = pol(13.55, g + 0.7)
    p = kit._P(B, lx, ly, 0.0, _yout(g), "plant_tag")
    p.kind = "prop"
    p.rod("CT_Wood", (0.0, 0.0, 0.0), (0.0, 0.0, 0.42), 0.02, 4)
    p.box("CT_Paper", -0.09, 0.09, -0.03, -0.012, 0.3, 0.42)
    p.done(top=0.42)
    B.thing("garden_labels", "board", (lx, ly, 0.5), _yout(g), topic="labels")

    # ---------------------------------------------------------------- pump room: the clock that stopped
    wx, wy = _at(14.7, th_p1, -0.075)
    B.thing("pump_clock", "board", (wx, wy, 2.5), th_p1, topic="clock")

    # ---------------------------------------------------------------- treatment room: check-up, quartz, the night corner
    bx_, by_ = _at(13.1, tc, 0.4)
    B.thing("treat_checkup", "machine", (bx_, by_, 1.0), _yout(tc), what="checkup")
    B.spot("doctor", "work", (*_at(14.1, tc, 0.9), 0.0), _yout(tc))
    qx, qy = _at(13.1, tc, 0.85)
    _quartz_lamp(B, qx, qy, 0.9, yaw=_yout(tc))
    B.thing("treat_quartz", "machine", (qx, qy, 1.15), _yout(tc), what="quartz")
    vx, vy = _at(12.55, tc, -0.05)
    B.thing("treat_window", "board", (vx, vy, 1.5), _yout(tc), topic="view")
    # the paid couch is a guest bed: blanket, pillow, the bedside radio and a night lamp on the cabinet
    cbx, cby = _at(15.2, tc, -0.85)
    p = kit._P(B, cbx, cby, 0.7, tc, "couch_bedding")
    p.kind = "prop"
    p.cushion("CT_PlasterBlue", -0.86, -0.04, -0.28, 0.28, 0.0, 0.1, 0.02)
    p.cushion("CT_TileWhite", 0.26, 0.56, -0.2, 0.2, 0.0, 0.1, 0.02)
    p.box("CT_PlasterYellow", -0.12, -0.05, -0.285, 0.285, 0.0, 0.103, f="xXyYZ")
    p.done(top=0.8)
    rx, ry = _at(16.9, tc, -1.53)
    kit.radio(B, rx, ry, dc["top"], yaw=_yin(tc))
    B.thing("night_radio", "machine", (rx, ry, dc["top"] + 0.15), _yin(tc), what="night_radio")
    kit.table_lamp(B, *_at(16.9, tc, -0.9), z=dc["top"], yaw=0, light=True)


def L_of(q, e):
    return math.dist(q[e], q[(e + 1) % len(q)])


def build(B):
    rnd = random.Random(1984)
    m = B.m
    cols = ["CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterRed", "CT_PlasterPink"]
    # ------------------------------------------------------------------ the ring body: 24 bays x 5 storeys
    bays = []
    for k in range(NB):
        th = -90.0 + 15.0 * k
        a = pol(R_BODY, th - HB)
        b = pol(R_TIP, th - HB + DELTA)
        c = pol(R_BODY, th + HB)
        ic, im, ia = pol(R_IN, th + HB), pol(R_IN, th), pol(R_IN, th - HB)
        bays.append(dict(th=th, a=a, b=b, c=c, ic=ic, im=im, ia=ia))
    for k, bay in enumerate(bays):
        a, b, c, ic, im, ia = (bay[x] for x in ("a", "b", "c", "ic", "im", "ia"))
        nxt = bays[(k + 1) % NB]
        poly = [a, b, c, ic, im, ia]
        m.face("CT_Roof", tri_orient_up(poly, ZT))
        m.face("CT_ConcreteDark", [(x, y, Z0) for x, y in tri_orient_up(poly, 0)[::-1]][::1] if False else
               [(x, y, Z0) for x, y, _ in tri_orient_up(poly, 0)][::-1])
        colr = cols[k % 4]
        wl = math.dist(bay["b"], bay["c"])
        for s in range(NS):
            z0, z1 = Z0 + s * STH, Z0 + (s + 1) * STH
            # the return wall a->b (painted) : closes the previous bay's balcony
            m.face(colr, [(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1)])
            # the window face b->c
            hl = [(0.10 * wl, 0.60 * wl, z0 + 0.95, z0 + 2.45), (0.68 * wl, 0.86 * wl, z0 + 0.15, z0 + 2.5)]
            F = B.facade(b, c, z0, z1, "CT_Concrete", holes=hl, depth=0.3, back=False)
            fid = "%d-%02d" % (s + 1, k + 1)
            B.flat(fid, storey=s + 1, beds=rnd.choice((1, 2, 2)), door=(*pol(R_IN + 1.6, bay["th"]), z0 + 0.1), entrance="main")
            B.facade_window(F, *hl[0], flat=fid, cols=2, rows=1, inset=0.2)
            B.facade_window(F, *hl[1], flat=fid, cols=1, rows=1, inset=0.2)
            # inner (courtyard) arc: two chords with a corridor window
            for (p0, p1) in ((ic, im), (im, ia)):
                Lh = math.dist(p0, p1)
                F2 = B.facade(p0, p1, z0, z1, "CT_Concrete", holes=[(Lh / 2 - 0.4, Lh / 2 + 0.4, z0 + 1.0, z0 + 2.1)], depth=0.3, back=False)
                B.facade_window(F2, Lh / 2 - 0.4, Lh / 2 + 0.4, z0 + 1.0, z0 + 2.1, kind="stair", inset=0.15)
            # balcony: the triangle b, c, b' with its slab and railing
            tb = nxt["b"]
            zs = z0
            tri = [b, c, tb]
            area = sum(tri[i][0] * tri[(i + 1) % 3][1] - tri[(i + 1) % 3][0] * tri[i][1] for i in range(3))
            T = tri if area > 0 else tri[::-1]
            m.face("CT_Concrete", [(x, y, zs) for x, y in T])
            m.face("CT_ConcreteDark", [(x, y, zs - 0.25) for x, y in T[::-1]])
            # the outer edge b -> b' (facing out): CCW order of the triangle gives the outward direction
            for i in range(3):
                p, q = T[i], T[(i + 1) % 3]
                if (p, q) in ((b, tb), (tb, b)):
                    m.face("CT_Concrete", [(p[0], p[1], zs - 0.25), (q[0], q[1], zs - 0.25), (q[0], q[1], zs), (p[0], p[1], zs)])
            B.railing([b, tb], zs, h=1.05, mat="CT_SteelWhite", post=1.4)
        # roof crown: a low coloured parapet on the window face and the tip
        for (p, q, mat) in ((a, b, colr), (b, c, "CT_ConcreteWhite")):
            L = math.dist(p, q)
            ux, uy = (q[0] - p[0]) / L, (q[1] - p[1]) / L
            nx, ny = uy, -ux                       # outward
            d = 0.25
            quad = [(p[0], p[1]), (q[0], q[1]), (q[0] - nx * d, q[1] - ny * d), (p[0] - nx * d, p[1] - ny * d)]
            m.face(mat, [(p[0], p[1], ZT), (q[0], q[1], ZT), (q[0], q[1], ZT + 0.7), (p[0], p[1], ZT + 0.7)])
            m.face(mat, [(q[0] - nx * d, q[1] - ny * d, ZT), (p[0] - nx * d, p[1] - ny * d, ZT),
                         (p[0] - nx * d, p[1] - ny * d, ZT + 0.7), (q[0] - nx * d, q[1] - ny * d, ZT + 0.7)])
            m.face(mat, [(x, y, ZT + 0.7) for x, y in (quad[0], quad[1], quad[2], quad[3])][::-1] if False else
                   [(quad[0][0], quad[0][1], ZT + 0.7), (quad[1][0], quad[1][1], ZT + 0.7), (quad[2][0], quad[2][1], ZT + 0.7), (quad[3][0], quad[3][1], ZT + 0.7)])
    # ------------------------------------------------------------------ podium
    # solid sectors outside the hall (hall = angles -135 .. -45)
    def sector(t0, t1, nseg):
        angs = [t0 + (t1 - t0) * i / nseg for i in range(nseg + 1)]
        outer = [pol(R_P, t) for t in angs]
        inner = [pol(R_IN, t) for t in angs]
        for i in range(nseg):
            (ax, ay), (bx, by) = outer[i], outer[i + 1]
            m.face("CT_Tile", [(ax, ay, 0), (bx, by, 0), (bx, by, Z0), (ax, ay, Z0)])
            (ax, ay), (bx, by) = inner[i + 1], inner[i]
            m.face("CT_Concrete", [(ax, ay, 0), (bx, by, 0), (bx, by, Z0), (ax, ay, Z0)])
    sector(-45.0, 225.0, 36)
    # ground hall, sector -135..-45
    ang = [-135.0 + 90.0 * i / 7 for i in range(8)]
    outer = [pol(R_P, t) for t in ang]
    inner = [pol(R_IN, t) for t in ang][::-1]
    hall = outer + inner                       # CCW
    nh = len(hall)
    t = 0.4
    holes = {}
    for i in range(7):
        L = math.dist(outer[i], outer[i + 1])
        holes[i] = [(1.1, 3.1, 0.0, 2.6)] if i == 3 else [(0.45, L - 0.45, 0.5, 3.7)]
    for j in range(7):
        i = 8 + j
        L = math.dist(hall[i], hall[(i + 1) % nh])
        holes[i] = [(0.4, L - 0.4, 0.3, 3.8)]
    q = inset_poly(hall, t)
    for i in range(nh):
        p0, p1 = hall[i], hall[(i + 1) % nh]
        hl = holes.get(i, [])
        F = B.facade(p0, p1, 0.0, Z0, "CT_ConcreteWhite", holes=hl, depth=t, back=False)
        L = F.length
        for hi, h in enumerate(hl):
            if i == 3:
                continue
            B.facade_window(F, h[0], h[1], h[2], h[3], kind="public", cols=2 if i < 8 else 1, rows=1, inset=0.15)
        # inner skin
        Fi = Facade(q[(i + 1) % nh], q[i])
        Li = Fi.length
        ih = [(max(0.0, Li - h[1] * Li / L), min(Li, Li - h[0] * Li / L), h[2], h[3]) for h in hl]
        mi = Mesh()
        wall_with_holes(mi, "CT_Interior", Li, 0.0, Z0, ih)
        m.extend(Fi.apply(mi))
    prism(m, "CT_Floor", hall, -0.3, 0.0, top=True, bottom=True, side_mat="CT_ConcreteDark")
    m.face("CT_Interior", [(x, y, Z0 - 0.01) for x, y in q][::-1])
    ex, ey = pol(R_P, -90.0)
    B.entrance("main", (ex, ey, 0.0), 0.0, width=2.4, height=2.6, leaves=2, kind="street")
    _furnish_hall(B, hall, t)
    # ------------------------------------------------------------------ struts under some balcony tips
    for k in range(0, NB, 3):
        th = bays[k]["th"] - HB + DELTA
        top = pol(R_TIP - 0.5, th)
        bot = pol(R_P + 0.7, th)
        beam(m, "CT_Concrete", (bot[0], bot[1], 0.0), (top[0], top[1], Z0 - 0.7), 0.9, -0.2, 0.2)
    # ------------------------------------------------------------------ courtyard: lawn, fountain, trees
    prism(m, "CT_Grass", ring(CX, CY, R_IN - 0.05, 40), 0.0, 0.08, top=True, bottom=False)
    prism(m, "CT_Stone", ring(CX, CY, 3.2, 24), 0.08, 0.55, top=True)
    prism(m, "CT_TileWhite", ring(CX, CY, 2.7, 24), 0.55, 0.58, top=True)
    prism(m, "CT_Stone", ring(CX, CY, 0.45, 12), 0.58, 1.9, top=True)
    prism(m, "CT_Stone", ring(CX, CY, 1.1, 16), 1.9, 2.1, top=True)
    for i in range(9):
        th = 360.0 * i / 9 + 10
        x, y = pol(8.0, th)
        box(m, "CT_Wood", x - 0.15, x + 0.15, y - 0.15, y + 0.15, 0.08, 2.6)
        prism(m, "CT_Grass", ring(x, y, 1.7, 8), 2.4, 4.6, top=True, bottom=True)
    # ------------------------------------------------------------------ roof extras: stair cap, courtyard-side rail
    for k in range(0, NB, 4):
        x, y = pol(15.5, bays[k]["th"])
        box(m, "CT_ConcreteWhite", x - 1.2, x + 1.2, y - 1.2, y + 1.2, ZT, ZT + 2.2)
    B.footprint = ring(CX, CY, R_TIP, 48)
    B.height = ZT + 2.2
    B.note("120 flats (5 storeys x 24 bays); hall sector -135..-45 deg enterable; entrance at (0, 8)")
