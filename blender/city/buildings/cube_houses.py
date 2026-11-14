"""Cube houses: a row of tilted cube houses standing on hexagonal concrete pillar cores, with a covered pedestrian
street underneath and a bridge-like deck between the pillars. An original design in the spirit of the 1980s Dutch
tilted cube houses (Rotterdam / Helmond) - ours is different: every cube is a square prism tipped 45 deg on one
EDGE (its section is a diamond, the long axis runs into the plot), the row bends in a gentle arc, the cubes
alternate in colour and sit at slightly different heights, undersides in timber.

Parameters (module level, set before build()): COUNT cubes (default 7; 5 fits a 40 m plot, 7 a 60 m plot),
PITCH between pillars, ARC the sag of the row ends towards the hill.
Per cube (edge 5.4 m, section diagonal 7.64 m, 5.4 m long): one flat on 3 levels inside (not modelled), one door at
the foot of its hexagonal pillar (a deep niche, entrance "c<n>", kind flat_door). Faces: the street-facing diamond has
a square window and three triangular windows (top and the two side corners), the back diamond a square one, the two
upper slopes a square roof window each. Underneath: a concrete deck at z 3.0 (rise-free, 6.6 m wide) between the
pillars, railings, and a stair at each end going down to the street (rise 0.158, going 0.30). Headroom under a cube
edge >= 2.4 m. Max height ~13.6 m.
Enterable: nothing inside; the deck is a public walk. Spots: benches on the deck.
Ideas for later: lit lanterns under the cubes, kiosks in the pillars' niches, planters on the deck.
Material requests: none.
"""
import math
import random

from citykit import box, prism, offset_polyline, extrude_profile, Frame, Mesh, DOOR_W, DOOR_H
import geom
import furn_int3 as fu
import interior_kit as kit
from _surrounds import add_surrounds

INFO = {
    "name_ru": "Кубические дома", "name_en": "Cube Houses",
    "style": "experimental / postmodern", "use": "residential",
    "inspired_by": "Tilted cube houses of the Netherlands (1980s): cubes on hexagonal pillar cores over a pedestrian "
                   "street; ours are edge-down prisms in an arc with colour and height variation",
}

COUNT = 7
VARIANTS = {"P-T1H1": {"COUNT": 5, "WHO": "a"},       # the plan's two plots: 40 m and 60 m fronts; one cube (c2) is open
            "P-T2H1": {"COUNT": 7, "WHO": "b"}}       # in each: "a" two flatmates, "b" the Dubovik family
WHO = "a"
HOUSE = 1                    # index of the open cube (entrance c2, flat 0-02)
PITCH = 8.0
ARC = 1.6
E = 5.4                      # cube edge
H = E / math.sqrt(2)         # half diagonal
VC = 9.0                     # distance of the row axis from the street front line
DECK_Z = 3.0
DECK_HALF = 3.3              # half width of the deck
END_EXT = 3.95               # deck beyond the first / last pillar
COLOURS = ["CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterRed", "CT_PlasterPink", "CT_Plaster", "CT_PlasterBlue",
           "CT_PlasterYellow", "CT_PlasterRed"]
S2 = math.sqrt(0.5)


# ------------------------------------------------------------------------------------------------ helpers
def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _add(a, b, k=1.0):
    return (a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k)


def face_to(m, mat, pts, n=None, toward=None):
    """Add a face whose normal points along n (or towards the point `toward`)."""
    nn = _cross(_sub(pts[1], pts[0]), _sub(pts[2], pts[0]))
    if n is not None:
        flip = _dot(nn, n) < 0
    else:
        c = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
        flip = _dot(nn, _sub(toward, c)) < 0
    m.face(mat, pts[::-1] if flip else pts)


def plane_wall(m, mat, O, e1, e2, n, w, h, holes, depth, reveal_mat=None):
    """A wall in the plane O + a*e1 + b*e2 (e1 x e2 = n outward), a 0..w, b 0..h, rectangular holes
    [(a0, a1, b0, b1)] lined by reveals `depth` deep (into -n)."""
    P = lambda a, b: (O[0] + e1[0] * a + e2[0] * b, O[1] + e1[1] * a + e2[1] * b, O[2] + e1[2] * a + e2[2] * b)
    us = sorted(set([0.0, w] + [x for hh in holes for x in hh[:2]]))
    zs = sorted(set([0.0, h] + [x for hh in holes for x in hh[2:]]))
    for i in range(len(us) - 1):
        for j in range(len(zs) - 1):
            cu, cz = (us[i] + us[i + 1]) / 2, (zs[j] + zs[j + 1]) / 2
            if any(hh[0] < cu < hh[1] and hh[2] < cz < hh[3] for hh in holes):
                continue
            face_to(m, mat, [P(us[i], zs[j]), P(us[i + 1], zs[j]), P(us[i + 1], zs[j + 1]), P(us[i], zs[j + 1])], n=n)
    for (a0, a1, b0, b1) in holes:
        ctr = _add(P((a0 + a1) / 2, (b0 + b1) / 2), n, -depth / 2)
        rm = reveal_mat or mat
        for (pa, pb) in (((a0, b0), (a1, b0)), ((a1, b0), (a1, b1)), ((a1, b1), (a0, b1)), ((a0, b1), (a0, b0))):
            A, Bp = P(*pa), P(*pb)
            face_to(m, rm, [A, Bp, _add(Bp, n, -depth), _add(A, n, -depth)], toward=ctr)


def poly_reveal(m, mat, pts, n, depth):
    """Reveal faces round a polygon (pts on the wall plane) into -n, facing the hole."""
    c = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
    ctr = _add(c, n, -depth / 2)
    for k in range(len(pts)):
        A, Bp = pts[k], pts[(k + 1) % len(pts)]
        face_to(m, mat, [A, Bp, _add(Bp, n, -depth), _add(A, n, -depth)], toward=ctr)


def shrink(pts, f):
    c = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
    return [tuple(c[i] + (p[i] - c[i]) * f for i in range(3)) for p in pts]


# ------------------------------------------------------------------------------------------------ one cube
def _cube(B, fr, idx, zb, colour, fid):
    """The edge-down cube at the local frame fr (origin at the pillar axis, u along the row, v inland)."""
    lm = Mesh()
    v0, v1 = -E / 2, E / 2
    zc = zb + H
    Bt, Rt, Tt, Lt = (0.0, zb), (H, zb + H), (0.0, zb + 2 * H), (-H, zb + H)

    def side(base, e2u, e2z, nu, nz, mat, holes=(), cards=False):
        e2 = (e2u, 0.0, e2z)
        n = (nu, 0.0, nz)
        e1 = _cross(e2, n)
        vv = v0 if e1[1] > 0 else v1
        O = (base[0], vv, base[1])
        plane_wall(lm, mat, O, e1, e2, n, E, E, list(holes), 0.32, "CT_Concrete")
        return O, e1, e2, n

    side(Bt, S2, S2, S2, -S2, "CT_Wood")                    # lower right
    side(Bt, -S2, S2, -S2, -S2, "CT_Wood")                  # lower left
    win_holes = [(1.7, 3.7, 1.7, 3.7)]
    slopes = []
    slopes.append(side(Rt, -S2, S2, S2, S2, "CT_ConcreteWhite", win_holes))      # upper right
    slopes.append(side(Lt, S2, S2, -S2, S2, "CT_ConcreteWhite", win_holes))      # upper left

    # the two diamond ends: a square opening (front: also three triangular ones)
    s = 1.2
    h = H
    tris = {
        "R": [(s, -s), (h, 0), (s, s)], "TR": [(s, s), (h, 0), (0, h)], "T": [(s, s), (0, h), (-s, s)],
        "TL": [(-s, s), (0, h), (-h, 0)], "L": [(-s, s), (-h, 0), (-s, -s)], "BL": [(-s, -s), (-h, 0), (0, -h)],
        "B": [(-s, -s), (0, -h), (s, -s)], "BR": [(s, -s), (0, -h), (h, 0)],
    }
    cards = []

    def Pend(vv, uz):
        return (uz[0], vv, zc + uz[1])

    for vv, n, front in ((v0, (0.0, -1.0, 0.0), True), (v1, (0.0, 1.0, 0.0), False)):
        window_tris = ("T", "L", "R") if front else ()
        for name, tri in tris.items():
            pts = [Pend(vv, p) for p in tri]
            if name in window_tris:
                inner = shrink(pts, 0.6)
                for k in range(3):
                    face_to(lm, colour, [pts[k], pts[(k + 1) % 3], inner[(k + 1) % 3], inner[k]], n=n)
                poly_reveal(lm, "CT_Concrete", inner, n, 0.32)
                cards.append(("tri", inner, n))
            else:
                face_to(lm, colour, pts, n=n)
        sq = [Pend(vv, p) for p in ((-s, -s), (s, -s), (s, s), (-s, s))]
        poly_reveal(lm, "CT_Concrete", sq, n, 0.32)
        cards.append(("sq", sq, n))
    B.m.extend(fr.apply(lm))

    # window cards (world coordinates) behind the openings
    inset = 0.2
    for kind, pts, n in cards:
        w = [fr(p) for p in pts]
        nw = _cross(_sub(w[1], w[0]), _sub(w[2], w[0]))
        # outward direction of the opening in world space (rotation only)
        o = fr((0.0, 0.0, 0.0))
        out = _sub(fr(n), o)
        if _dot(nw, out) < 0:
            w = w[::-1]
        if kind == "sq":
            # bottom-left, bottom-right, top-right, top-left seen from outside
            order = _square_order(w, out)
            B.window(order, flat=fid, inset=inset, cols=2, rows=1, frame=0.05)
        else:
            order = _tri_order(w, out)
            B.window(order, flat=fid, inset=inset, frame=0)
    # upper roof windows on the slopes
    for (O, e1, e2, n) in slopes:
        a0, a1, b0, b1 = win_holes[0]
        P = lambda a, b: (O[0] + e1[0] * a + e2[0] * b, O[1] + e1[1] * a + e2[1] * b, O[2] + e1[2] * a + e2[2] * b)
        pts = [fr(P(a0, b0)), fr(P(a1, b0)), fr(P(a1, b1)), fr(P(a0, b1))]
        B.window(pts, flat=fid, inset=0.2, cols=2, rows=2, frame=0.05)


def _square_order(w, out):
    """Order 4 coplanar points of a square as bottom-left, bottom-right, top-right, top-left seen from outside
    (right = out x up for an observer looking against `out`)."""
    up = (0.0, 0.0, 1.0)
    right = _cross(up, out)                  # up x out: for out = -y (street) gives +x? checked below
    # for a viewer looking against `out`, their right is  up x out ... verify sign with a known case:
    # out = (0,-1,0): viewer looks +y, right is +x;  up x out = (0,0,1) x (0,-1,0) = (1,0,0) -> +x. ok
    cx = [sum(p[i] for p in w) / 4 for i in range(3)]
    def key(p):
        d = _sub(p, tuple(cx))
        return (_dot(d, up) > 0, _dot(d, right) > 0)
    bl = next(p for p in w if key(p) == (False, False))
    br = next(p for p in w if key(p) == (False, True))
    tr = next(p for p in w if key(p) == (True, True))
    tl = next(p for p in w if key(p) == (True, False))
    return [bl, br, tr, tl]


def _tri_order(w, out):
    """A triangle as a degenerate quad [A, B, C, C] wound counter-clockwise seen from outside."""
    nw = _cross(_sub(w[1], w[0]), _sub(w[2], w[0]))
    if _dot(nw, out) < 0:
        w = [w[0], w[2], w[1]]
    return [w[0], w[1], w[2], w[2]]


# ------------------------------------------------------------------------------------------------ pillar
def _pillar(B, fr, ztop, eid, lobby=False):
    """Hexagonal concrete core with a deep door niche on the street-facing face."""
    lm = Mesh()
    hx = [(-1.3, -1.5), (1.3, -1.5), (2.0, 0.0), (1.3, 1.5), (-1.3, 1.5), (-2.0, 0.0)]
    # front face (edge 0): wall with the door niche
    O = (-1.3, -1.5, 0.0)
    e1, e2, n = (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, -1.0, 0.0)
    door = (0.75, 0.75 + DOOR_W, 0.0, DOOR_H)
    plane_wall(lm, "CT_ConcreteDark", O, e1, e2, n, 2.6, ztop, [door], 1.4, "CT_ConcreteDark")
    # back panel of the niche (the lobby wall; open in the cube with rooms: the doorway leads into the street room)
    if not lobby:
        face_to(lm, "CT_Interior", [(-1.3 + door[0], -1.5 + 1.4, 0.0), (-1.3 + door[1], -1.5 + 1.4, 0.0),
                                    (-1.3 + door[1], -1.5 + 1.4, door[3]), (-1.3 + door[0], -1.5 + 1.4, door[3])], n=n)
    face_to(lm, "CT_Floor", [(-1.3 + door[0], -1.5, 0.01), (-1.3 + door[1], -1.5, 0.01),
                             (-1.3 + door[1], -1.5 + 1.4, 0.01), (-1.3 + door[0], -1.5 + 1.4, 0.01)], n=(0, 0, 1))
    for k in range(1, 6):
        a, b = hx[k], hx[(k + 1) % 6]
        face_to(lm, "CT_ConcreteDark", [(a[0], a[1], 0.0), (b[0], b[1], 0.0), (b[0], b[1], ztop), (a[0], a[1], ztop)],
                n=(0.0, 0.0, 0.0) if False else (((a[0] + b[0]) / 2) * 1.0, ((a[1] + b[1]) / 2) * 1.0, 0.0))
    B.m.extend(fr.apply(lm))
    dm = Mesh()
    box(dm, "CT_Neon", -0.15 + 0.0 + (door[0] + door[1]) / 2 - 1.3 + 0.0, 0.15 + (door[0] + door[1]) / 2 - 1.3, -1.58, -1.5, 2.45, 2.6)
    B.detail.extend(fr.apply(dm))
    pos = fr((0.0, -1.5, 0.0))
    o = fr((0.0, 0.0, 0.0))
    out = _sub(fr((0.0, -1.0, 0.0)), o)
    B.entrance(eid, pos, math.degrees(math.atan2(out[0], -out[1])), DOOR_W, DOOR_H, "flat_door")


# ------------------------------------------------------------------------------------------------ interiors
def _bike(B, x, y, yaw=0.0, mat="CT_SteelRed", z=0.0):
    """A city bicycle leaning on a railing (local: length along y, front wheel toward -y)."""
    p = kit._P(B, x, y, z, yaw, "bicycle")
    p.kind = "prop"
    R = 0.33
    for yc in (-0.55, 0.55):
        ring_pts = [(0.0, yc + R * math.cos(2 * math.pi * k / 14), R + R * math.sin(2 * math.pi * k / 14)) for k in range(15)]
        p.tube("CT_Rubber", ring_pts, 0.04, 4)
    p.rod(mat, (0.0, 0.55, R), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod(mat, (0.0, 0.05, 0.62), (0.0, -0.4, 0.66), 0.03, 4)
    p.rod(mat, (0.0, 0.55, R), (0.0, 0.0, 0.3), 0.03, 4)
    p.rod(mat, (0.0, 0.0, 0.3), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod(mat, (0.0, -0.4, 0.66), (0.0, -0.55, R), 0.03, 4)
    p.rod(kit.STEEL, (0.0, -0.42, 0.62), (0.0, -0.42, 0.98), 0.025, 4)
    p.rod(kit.STEEL, (-0.28, -0.42, 0.99), (0.28, -0.42, 0.99), 0.025, 4)
    p.rod(kit.LAQ, (0.0, 0.1, 0.62), (0.0, 0.1, 0.92), 0.025, 4)
    p.box(kit.LAQ, -0.06, 0.06, 0.02, 0.24, 0.92, 0.96)
    return p.done()


def _niche(B, fr, yaw_deg, i, open_back=False):
    """The door niche of a pillar (1.1 m wide, 1.4 m deep, v -1.5 .. -0.1) as a house entrance: a doormat, letter boxes on
    the west side wall, a notice board on the back wall."""
    x, y, _ = fr((-0.55, -0.85, 0.0))
    fu.mailboxes(B, fu.Fr(x, y, 90.0 + yaw_deg, 0.0), 0.0, 0.95, cols=2, rows=3)
    if open_back:                           # the back wall is a doorway: the board moves to the east side wall
        x, y, _ = fr((0.55, -0.62, 0.0))
        kit.notice_board(B, x, y, 0.0, yaw_deg + 270.0, w=0.5, zc=1.5, seed=20 + i)
    else:
        x, y, _ = fr((0.0, -0.1, 0.0))
        kit.notice_board(B, x, y, 0.0, yaw_deg, w=0.5, zc=1.5, seed=20 + i)
    B.kit_items.pop()                       # a board on the dead-end wall of the niche does not block the door
    x, y, _ = fr((0.0, -0.85, 0.0))
    fu.doormat(B, fu.Fr(x, y, yaw_deg, 0.0), 0.0, 0.0, size=(0.8, 0.5))
    if open_back:
        B.thing("niche_board_c%d" % (i + 1), "board", fr((0.52, -0.62, 1.5)), yaw_deg + 270.0, topic="lost")
    else:
        B.thing("niche_board_c%d" % (i + 1), "board", fr((0.0, -0.13, 1.5)), yaw_deg, topic="lost")


def _deck(B, P, th, Lp, Rp, Q, t1):
    """Street furniture of the deck (z 3.0): park benches on the inland side, a chess table, boards on the pillars' back
    faces, plants, a bicycle on the railing, a coin telescope at the east end."""
    n = len(P)
    Z = DECK_Z
    for i in range(n - 1):
        mx, my = (P[i][0] + P[i + 1][0]) / 2, (P[i][1] + P[i + 1][1]) / 2
        a = (th[i] + th[i + 1]) / 2
        fr = Frame(mx, my, a)
        yw = math.degrees(a)
        W = lambda u, v, z=0.0: fr((u, v, z))
        if i % 2 == 1:                                           # a bench against the inland railing, facing the street
            x, y, _ = W(0.0, DECK_HALF - 0.45, Z)
            bn = kit.bench(B, x, y, Z, yw, w=1.8, kind="park")
            sx, sy, sz, syaw = bn["seats"][0]
            B.spot("deck_bench_%d" % i, "bench", (sx, sy, Z), syaw)
            B.thing("deck_bench_%d_seat" % i, "seat", (sx, sy, sz), syaw, height=round(sz - Z, 2))
            x, y, _ = W(2.0, DECK_HALF - 0.4, Z)
            kit.plant(B, x, y, Z, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.8, seed=i)
        elif i == n // 2 - (1 if (n // 2) % 2 else 0):         # a chess corner on the inland side
            x, y, _ = W(0.0, 2.35, Z)
            kit.chess_table(B, x, y, Z, yw, seed=i, solid=False)
            for sgn in (-1, 1):
                x, y, _ = W(0.0, 2.35 + sgn * 0.62, Z)
                kit.stool(B, x, y, Z, yw + (0 if sgn < 0 else 180.0))
            B.thing("deck_chess", "play", W(0.0, 2.35, Z + 0.75), yw, what="chess")
            B.spot("deck_chess_seat", "seat", W(0.0, 2.35 - 0.62, Z), yw)
        else:                                                    # a planter pair and a bike
            x, y, _ = W(-1.6, DECK_HALF - 0.45, Z)
            kit.plant(B, x, y, Z, 0.0, kind=("ficus", "monstera", "palm")[i % 3], pot="CT_PlasterRed", h=1.2, seed=i)
            if i == 0:
                x, y, _ = W(0.8, DECK_HALF - 0.38, Z)
                _bike(B, x, y, yw + 90.0, mat="CT_PlasterBlue", z=Z)
        x, y, _ = W(2.4, -DECK_HALF + 0.4, Z)                    # a plant at the street rail
        kit.plant(B, x, y, Z, 0.0, kind=("fern", "ficus")[i % 2], pot="CT_PlasterBlue", h=0.9, seed=i + 7)
    # boards on the pillars' back faces (inland side), facing +v
    for i in range(n):
        fr = Frame(P[i][0], P[i][1], th[i])
        yw = math.degrees(th[i]) + 180.0
        x, y, _ = fr((0.0, 1.5, 0.0))
        topic = "club" if i % 2 == 0 else "timetable"
        if topic == "club":
            kit.notice_board(B, x, y, Z, yw, w=0.9, zc=1.5, seed=30 + i)
        else:
            kit.poster(B, x, y, Z, yw, name="chart_timetable_bus", w=0.8, zc=1.5)
        B.thing("pillar_board_%d" % (i + 1), "board", fr((0.0, 1.47, Z + 1.5)), yw, topic=topic)
    _deck_extras(B, P, th)
    # a coin telescope at the east end, street side, looking over the sea
    fr = Frame(P[-1][0], P[-1][1], th[-1])
    x, y, _ = fr((-2.4, -DECK_HALF + 0.6, Z))
    kit.telescope(B, x, y, Z, math.degrees(th[-1]))
    B.thing("deck_telescope", "machine", fr((-2.4, -DECK_HALF + 0.6, Z + 1.2)), math.degrees(th[-1]), what="telescope")


def _postbox(B, x, y, z, yaw):
    """A blue steel post box on a short post: a slit, a brass plate with the collection times, a little roof."""
    p = kit._P(B, x, y, z, yaw, "postbox")
    p.box("CT_Steel", -0.05, 0.05, -0.05, 0.05, 0.0, 0.5, f="xXyYZ")
    p.box("CT_PlasterBlue", -0.24, 0.24, -0.17, 0.17, 0.5, 1.2, f="xXyYz")
    p.loft("CT_PlasterBlue", [[(-0.24, -0.17, 1.2), (0.24, -0.17, 1.2), (0.24, 0.17, 1.2), (-0.24, 0.17, 1.2)],
                              [(-0.2, -0.13, 1.28), (0.2, -0.13, 1.28), (0.2, 0.13, 1.28), (-0.2, 0.13, 1.28)]], top=True)
    p.box("CT_Concrete", -0.14, 0.14, -0.178, -0.17, 0.98, 1.02, f="y")
    p.box("CT_Brass", -0.1, 0.1, -0.178, -0.17, 0.72, 0.9, f="y")
    p.core(-0.24, 0.24, -0.17, 0.17, 0.5, 1.2)
    return p.done(solid=True, top=1.28)


def _deck_extras(B, P, th):
    """Round 2 of the deck: a post box and a notice stand (the day's notices), a bench with a view and planters on the street
    side. New ids: deck_postbox, deck_board, deck_view, deck_bench_s*, deck_planter."""
    n = len(P)
    Z = DECK_Z

    def seg(i):
        fr = Frame((P[i][0] + P[i + 1][0]) / 2, (P[i][1] + P[i + 1][1]) / 2, (th[i] + th[i + 1]) / 2)
        return fr, math.degrees((th[i] + th[i + 1]) / 2)
    fr, yw = seg(0)                                           # the post box near the west stair, street side
    x, y, _ = fr((-2.2, -DECK_HALF + 0.55, Z))
    _postbox(B, x, y, Z, yw)
    B.thing("deck_postbox", "board", fr((-2.2, -DECK_HALF + 0.55, Z + 1.0)), yw + 180.0, topic="post")
    fr, yw = seg(1)                                           # the notice stand across the deck's street edge
    x, y, _ = fr((-1.3, -DECK_HALF + 0.9, Z))
    kit.notice_stand(B, x, y, Z, yw + 90.0, w=1.1, h=0.8, front="notice", back="notice", seed=71)
    B.thing("deck_board", "board", fr((-1.3 + 0.1, -DECK_HALF + 0.9, Z + 1.45)), yw + 90.0, topic="deck_board")
    fr, yw = seg(2)                                           # a bench at the street rail, looking out over it
    x, y, _ = fr((1.2, -DECK_HALF + 0.45, Z))
    bn = kit.bench(B, x, y, Z, yw + 180.0, w=1.8, kind="park")
    sx, sy, sz, syaw = bn["seats"][0]
    B.spot("deck_bench_s2", "bench", (sx, sy, Z), syaw)
    B.thing("deck_bench_s2_seat", "seat", (sx, sy, sz), syaw, height=round(sz - Z, 2))
    B.thing("deck_view", "board", fr((1.2, -DECK_HALF + 0.12, Z + 1.3)), yw + 180.0, topic="view")
    fr, yw = seg(3)                                           # a planter trough on the street side
    x, y, _ = fr((-1.2, -DECK_HALF + 0.4, Z))
    kit.planter(B, x, y, Z, yw, w=1.6, seed=9)
    B.thing("deck_planter", "board", fr((-1.2, -DECK_HALF + 0.4, Z + 0.7)), yw + 180.0, topic="deck_planter")


# ------------------------------------------------------------------------------------------------ build
def build(B):
    rnd = random.Random(1983)
    m = B.m
    n = COUNT
    xs = [(i - (n - 1) / 2) * PITCH for i in range(n)]
    half = max(abs(xs[0]), 1.0) + END_EXT
    P = [(x, VC - ARC + ARC * (x / half) ** 2) for x in xs]            # pillar axis points
    # heading at every pillar (central differences)
    th = []
    for i in range(n):
        a, b = P[max(i - 1, 0)], P[min(i + 1, n - 1)]
        th.append(math.atan2(b[1] - a[1], b[0] - a[0]))
    top = 0.0
    for i in range(n):
        zb = 5.4 + 0.25 * ((i * 3) % 4)             # cube bottom edge heights vary a little
        fr = Frame(P[i][0], P[i][1], th[i])
        fid = "0-%02d" % (i + 1)
        B.flat(fid, storey=0, beds=2 + (i % 2), door=fr((0.0, -1.5 + 1.3, 0.0)), entrance="c%d" % (i + 1))
        _pillar(B, fr, zb + 1.9, "c%d" % (i + 1), lobby=(i == HOUSE))
        _niche(B, fr, math.degrees(th[i]), i, open_back=(i == HOUSE))
        _cube(B, fr, i, zb, COLOURS[i % len(COLOURS)], fid)
        if i == HOUSE:
            _open_house(B, fr, th[i], zb, WHO)
        top = max(top, zb + 2 * H)

    # ---------------------------------------------------------------- the deck (bridge) along the axis
    t0 = (math.cos(th[0]), math.sin(th[0]))
    t1 = (math.cos(th[-1]), math.sin(th[-1]))
    Q = [(P[0][0] - t0[0] * END_EXT, P[0][1] - t0[1] * END_EXT)] + P + [(P[-1][0] + t1[0] * END_EXT, P[-1][1] + t1[1] * END_EXT)]
    Lp = offset_polyline(Q, DECK_HALF)           # inland side
    Rp = offset_polyline(Q, -DECK_HALF)          # street side
    for i in range(len(Q) - 1):
        poly = [Rp[i], Rp[i + 1], Lp[i + 1], Lp[i]]
        prism(m, "CT_ConcreteDark", poly, DECK_Z - 0.45, DECK_Z, top=False, bottom=True)
        m.face("CT_Paving", [(x, y, DECK_Z) for x, y in poly])
    # railings: inland side all along; street side except the two stair openings; ends closed
    B.railing(Lp, DECK_Z, h=1.05)
    k_open = 3.4                                   # the stair opening runs this far from each end
    def cut(pts, d0, d1):
        """Sub-polyline of pts between arc distances d0 and d1."""
        out = []
        acc = 0.0
        for (a, b) in zip(pts, pts[1:]):
            L = math.dist(a, b)
            lo, hi = max(d0 - acc, 0.0), min(d1 - acc, L)
            if hi > lo:
                f = lambda t: (a[0] + (b[0] - a[0]) * t / L, a[1] + (b[1] - a[1]) * t / L)
                if not out:
                    out.append(f(lo))
                out.append(f(hi))
            acc += L
        return out
    total = sum(math.dist(a, b) for a, b in zip(Rp, Rp[1:]))
    seg = cut(Rp, k_open, total - k_open)
    if len(seg) >= 2:
        B.railing(seg, DECK_Z, h=1.05)
    B.railing([Rp[0], Lp[0]], DECK_Z, h=1.05)
    B.railing([Lp[-1], Rp[-1]], DECK_Z, h=1.05)

    # ---------------------------------------------------------------- stairs at both ends (down to the street)
    stair_pts = []
    for end, (qa, qb, tt) in enumerate(((Q[0], Q[1], t0), (Q[-1], Q[-2], t1))):
        sgn = 1 if end == 0 else -1
        # the flight starts on the street-side deck edge, 1.7 m from the deck end
        d = sgn * 1.7
        cx, cy = Q[0 if end == 0 else -1][0] + tt[0] * d, Q[0 if end == 0 else -1][1] + tt[1] * d
        th_e = math.atan2(tt[1], tt[0])
        fr = Frame(cx - (-math.sin(th_e)) * (-DECK_HALF) * -1 * 0.0 + math.sin(th_e) * DECK_HALF,
                   cy - math.cos(th_e) * DECK_HALF, th_e)
        _flight(B, fr)
        pos = fr((0.0, -5.4, 0.0))
        stair_pts.append(pos)
        o = fr((0.0, 0.0, 0.0))
        out = _sub(fr((0.0, -1.0, 0.0)), o)
        B.entrance("deck_stair_%s" % ("w" if end == 0 else "e"), pos, math.degrees(math.atan2(out[0], -out[1])), 2.6, 2.2, "street")

    # ---------------------------------------------------------------- the deck as a street: benches, chess, boards, plants
    _deck(B, P, th, Lp, Rp, Q, t1)

    # ---------------------------------------------------------------- footprint, height
    xs_all = [v[0] for v in m.verts] + [v[0] for v in B.detail.verts]
    ys_all = [v[1] for v in m.verts] + [v[1] for v in B.detail.verts]
    x0, x1, y0, y1 = min(xs_all), max(xs_all), min(ys_all), max(ys_all)
    B.footprint = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    B.height = top
    B.note("%d cube houses on hexagonal pillars, deck at z %.1f, max height %.1f m" % (n, DECK_Z, top))
    add_surrounds(B, kinds=("street", "flat_door"), seed=5)


def _flight(B, fr):
    """A stair of 19 risers going down from the deck edge (local v = 0, z = 3.0) towards -v, 2.6 m wide."""
    nst = 19
    r = DECK_Z / nst
    g = 0.30
    pts = []      # (0, DECK_Z) would lie on the back edge's line (the deck's own edge is the top riser): a collinear point, a degenerate face
    for k in range(1, nst):
        pts.append((-g * (k - 1), DECK_Z - k * r))
        pts.append((-g * k, DECK_Z - k * r))
    pts.append((-g * (nst - 1), 0.0))
    pts.append((-g * (nst - 1), -0.2))
    pts.append((0.0, -0.2))
    clean = []
    for p in pts:
        if not clean or math.hypot(p[0] - clean[-1][0], p[1] - clean[-1][1]) > 1e-6:
            clean.append(p)
    lm = Mesh()
    # profile (a = v, b = z) extruded along u (v x z = u, right-handed)
    extrude_profile(lm, "CT_Concrete", "CT_Concrete", clean, -1.3, 1.3, lambda a, b, t: (t, a, b))
    B.m.extend(fr.apply(lm))
    # rails along both sides (posts + sloped rail), detail
    dm = Mesh()
    for sx in (-1.22, 1.22):
        for k in range(1, nst, 4):
            v = -g * (k - 1)
            z = DECK_Z - k * r
            box(dm, "CT_Steel", sx - 0.02, sx + 0.02, v - 0.02 - g, v + 0.02 - g, z, z + 1.05)
        za, zb = DECK_Z + 1.05 - r, 1.05
        va, vb = 0.0, -g * (nst - 1)
        for dx in (0.0,):
            dm.face("CT_Steel", [(sx - 0.02, va, za), (sx - 0.02, vb, zb), (sx + 0.02, vb, zb), (sx + 0.02, va, za)])
            dm.face("CT_Steel", [(sx + 0.02, va, za), (sx + 0.02, vb, zb), (sx - 0.02, vb, zb), (sx - 0.02, va, za)])
    B.detail.extend(fr.apply(dm))


# ================================================================================================ the open cube (c2)
# One cube is a home you can walk into: the street room in the pillar's foot (the niche's back wall is a doorway), a lift
# in its back wall, and over the pillar the cube's own two levels - the living level (floor 1.95 over the cube's lowest
# edge: a V-shaped room, 3.0 m of floor widening to 6.5 m at the ceiling, its walls leaning OUT) and the tip (floor 4.25:
# 5.9 m of floor under a tent roof whose walls lean IN, a skylight over the bed). The levels are joined by a lift whose
# doors sit on a wooden service core in the back of the cube (ids lift_g, lift_living, lift_tip).
# Local frame of the cube: u along the row, v inland (the cube's long axis), z over the cube's lowest edge (zb).
WALL = 0.32                          # the reveals' depth: the lining stands this far behind the outer skin
K = WALL * math.sqrt(2)              # the lining's diamond is smaller by this much (in z at the corners)
HI = H - K                           # half diagonal of the inner diamond
VE = E / 2 - WALL                    # the lining of the two ends stands at v = -VE and +VE
F1, F2, SLAB = 1.95, 4.25, 0.15      # floors of the living level and the tip, thickness of the slab between
CORE_U, CORE_V = 0.7, 1.5            # the service core: |u| < CORE_U, v > CORE_V (to the back wall)
LOBBY = (-1.15, 1.15, -0.1, 1.3, 2.6)    # the street room in the pillar: u0, u1, v0, v1, ceiling


def _half(zr):
    """Half width of the room at height zr over the cube's lowest edge (inside the lining)."""
    return min(zr - K, 2 * H - K - zr)


class _House:
    """Placing helpers in the cube's local frame (u, v, floor height F over the cube's lowest edge)."""

    def __init__(self, B, fr, th, zb):
        self.B, self.fr, self.zb = B, fr, zb
        self.a = math.degrees(th)
        self.ox, self.oy, _ = fr((0.0, 0.0, 0.0))

    def piece(self, F, name, kind="prop"):
        """A kit piece builder whose local frame IS the cube's: draw in (u, v, z over F)."""
        p = kit._P(self.B, self.ox, self.oy, self.zb + F, self.a, name)
        p.kind = kind
        return p

    def xy(self, u, v):
        x, y, _ = self.fr((u, v, 0.0))
        return x, y

    def w(self, u, v, z):
        x, y = self.xy(u, v)
        return (x, y, self.zb + z)

    def kp(self, fn, u, v, F, yaw=0.0, **kw):
        x, y = self.xy(u, v)
        return fn(self.B, x, y, self.zb + F, self.a + yaw, **kw)

    def thing(self, tid, kind, u, v, z, yaw=0.0, **props):
        self.B.thing(tid, kind, self.w(u, v, z), self.a + yaw, **props)

    def spot(self, sid, kind, u, v, F, yaw=0.0):
        self.B.spot(sid, kind, self.w(u, v, F), self.a + yaw)

    def lamp(self, u, v, z, lumens=420, radius=6.0, colour="#FFD9A8"):
        self.B.lamp(self.w(u, v, z), lumens=lumens, colour=colour, kind="public", radius=radius)


def _inner_shell(B, fr, zb):
    """The lining of the cube: four sloping walls (holes for the two skylights), the lining of both ends (holes for the
    windows), the two floors and the service core. Everything single-sided, facing in; the slabs' edges hide inside the skin."""
    lm = Mesh()
    zc = zb + H
    L = HI * math.sqrt(2)
    s_f1 = (F1 - K) / S2
    for sgn in (-1, 1):
        # lower wall (timber) from the living floor to the widest corner
        P = lambda s, v, sg=sgn: (sg * s * S2, v, zb + K + s * S2)
        for (sa, sb) in ((s_f1, L),):
            face_to(lm, "CT_WoodLight", [P(sa, -VE), P(sb, -VE), P(sb, VE), P(sa, VE)], n=(-sgn * S2, 0.0, S2))
        # upper wall (plaster) with the skylight hole s 1.38..3.38, v -1..1
        Q = lambda s, v, sg=sgn: (sg * (HI - s * S2), v, zc + s * S2)
        ss, vs = [0.0, 1.38, 3.38, L], [-VE, -1.0, 1.0, VE]
        for i in range(3):
            for j in range(3):
                if i == 1 and j == 1:
                    continue
                face_to(lm, "CT_Interior", [Q(ss[i], vs[j]), Q(ss[i + 1], vs[j]), Q(ss[i + 1], vs[j + 1]), Q(ss[i], vs[j + 1])],
                        n=(-sgn * S2, 0.0, -S2))
    # the two ends
    s = 1.2

    def tris(h):
        return {"R": [(s, -s), (h, 0), (s, s)], "TR": [(s, s), (h, 0), (0, h)], "T": [(s, s), (0, h), (-s, s)],
                "TL": [(-s, s), (0, h), (-h, 0)], "L": [(-s, s), (-h, 0), (-s, -s)], "BL": [(-s, -s), (-h, 0), (0, -h)],
                "B": [(-s, -s), (0, -h), (s, -s)], "BR": [(s, -s), (0, -h), (h, 0)]}
    tin, tout = tris(HI), tris(H)
    for front in (True, False):
        vv = -VE if front else VE
        n = (0.0, 1.0, 0.0) if front else (0.0, -1.0, 0.0)
        for name, tri in tin.items():
            pts = [(p[0], vv, zc + p[1]) for p in tri]
            if front and name in ("T", "L", "R"):
                hole = shrink([(p[0], vv, zc + p[1]) for p in tout[name]], 0.6)
                for k in range(3):
                    face_to(lm, "CT_Interior", [pts[k], pts[(k + 1) % 3], hole[(k + 1) % 3], hole[k]], n=n)
            else:
                face_to(lm, "CT_Interior", pts, n=n)
    # the service core: a wooden pentagon-section column in the back, from the living floor to the roof
    cu, vc = CORE_U, CORE_V
    zr = zb + 2 * H - K
    face_to(lm, "CT_WoodLight", [(-cu, vc, zb + F1), (cu, vc, zb + F1), (cu, vc, zr - cu), (0.0, vc, zr), (-cu, vc, zr - cu)],
            n=(0.0, -1.0, 0.0))
    for sg in (-1, 1):
        face_to(lm, "CT_WoodLight", [(sg * cu, vc, zb + F1), (sg * cu, VE, zb + F1), (sg * cu, VE, zr - cu), (sg * cu, vc, zr - cu)],
                n=(sg, 0.0, 0.0))
    B.m.extend(fr.apply(lm))
    # floors: the living floor (parquet) and the slab between (timber on both faces)
    h1 = _half(F1)
    poly = [fr((x, y, 0.0))[:2] for x, y in ((-h1, -VE), (h1, -VE), (h1, VE), (-h1, VE))]
    prism(B.m, "CT_Wood", poly, zb + F1 - 0.4, zb + F1, top=True, bottom=False, side_mat="CT_Concrete")
    h2 = _half(F2 - SLAB)
    poly = [fr((x, y, 0.0))[:2] for x, y in ((-h2, -VE), (h2, -VE), (h2, VE), (-h2, VE))]
    prism(B.m, "CT_WoodLight", poly, zb + F2 - SLAB, zb + F2, top=True, bottom=True, side_mat="CT_Concrete")


def _lobby_shell(B, fr):
    """The street room inside the pillar's foot: walls, ceiling, floor, the doorway in the niche's back wall."""
    u0, u1, v0, v1, zc = LOBBY
    lm = Mesh()
    mat = "CT_Interior"
    face_to(lm, mat, [(u0, v0, 0.0), (u0, v1, 0.0), (u0, v1, zc), (u0, v0, zc)], n=(1.0, 0.0, 0.0))
    face_to(lm, mat, [(u1, v0, 0.0), (u1, v1, 0.0), (u1, v1, zc), (u1, v0, zc)], n=(-1.0, 0.0, 0.0))
    face_to(lm, mat, [(u0, v1, 0.0), (u1, v1, 0.0), (u1, v1, zc), (u0, v1, zc)], n=(0.0, -1.0, 0.0))
    dw = 0.55
    face_to(lm, mat, [(u0, v0, 0.0), (-dw, v0, 0.0), (-dw, v0, zc), (u0, v0, zc)], n=(0.0, 1.0, 0.0))
    face_to(lm, mat, [(dw, v0, 0.0), (u1, v0, 0.0), (u1, v0, zc), (dw, v0, zc)], n=(0.0, 1.0, 0.0))
    face_to(lm, mat, [(-dw, v0, DOOR_H), (dw, v0, DOOR_H), (dw, v0, zc), (-dw, v0, zc)], n=(0.0, 1.0, 0.0))
    face_to(lm, "CT_WoodLight", [(u0, v0, zc), (u1, v0, zc), (u1, v1, zc), (u0, v1, zc)], n=(0.0, 0.0, -1.0))
    face_to(lm, "CT_Floor", [(u0, v0, 0.01), (u1, v0, 0.01), (u1, v1, 0.01), (u0, v1, 0.01)], n=(0.0, 0.0, 1.0))
    B.m.extend(fr.apply(lm))


def _lift_door(B, fr, u, v, z):
    """A small lift door set in the wall plane v (facing -v): frame, two steel leaves, a call button and a lamp over it."""
    dm = Mesh()
    hw = 0.45
    box(dm, "CT_Steel", u - hw - 0.06, u + hw + 0.06, v - 0.05, v, z, z + 2.12)
    box(dm, "CT_SteelWhite", u - hw, u - 0.004, v - 0.075, v - 0.05, z, z + 2.05)
    box(dm, "CT_SteelWhite", u + 0.004, u + hw, v - 0.075, v - 0.05, z, z + 2.05)
    box(dm, "CT_Steel", u - 0.03, u + 0.03, v - 0.09, v - 0.075, z, z + 2.05)
    box(dm, "CT_ConcreteWhite", u + hw + 0.09, u + hw + 0.17, v - 0.03, v, z + 1.0, z + 1.2)
    box(dm, "CT_SteelRed", u + hw + 0.115, u + hw + 0.145, v - 0.045, v - 0.03, z + 1.07, z + 1.13)
    box(dm, "CT_Opal", u - 0.1, u + 0.1, v - 0.035, v, z + 2.2, z + 2.3)
    B.detail.extend(fr.apply(dm))


def _wedge(p, side, v0, v1, wl, uf, h, mat_front, mat_top, upper=False, core=True):
    """A built-in block against a sloping wall: its front face vertical at |u| = uf, its top at height h meeting the wall
    (a lower wall leans out: the wall is at wl + z, an upper one leans in: at wl - z). side -1 left, +1 right."""
    sg = side
    wall_top = wl - h if upper else wl + h
    X = lambda a, v, z: (sg * a, v, z)
    A0, B0, C0, D0 = (uf, 0.0), (wl, 0.0), (wall_top, h), (uf, h)
    face_to(p.m, mat_front, [X(uf, v0, 0.0), X(uf, v1, 0.0), X(uf, v1, h), X(uf, v0, h)], n=(-sg, 0.0, 0.0))
    face_to(p.m, mat_top, [X(uf, v0, h), X(uf, v1, h), X(wall_top, v1, h), X(wall_top, v0, h)], n=(0.0, 0.0, 1.0))
    for vv, nn in ((v0, (0.0, -1.0, 0.0)), (v1, (0.0, 1.0, 0.0))):
        face_to(p.m, mat_front, [X(a, vv, z) for (a, z) in (A0, B0, C0, D0)], n=nn)
    if core:
        hi = min(wl, wall_top) - 0.04
        p.core(min(sg * uf, sg * hi), max(sg * uf, sg * hi), v0 + 0.02, v1 - 0.02, 0.0, h - 0.01)
    return p


def _dominoes(p, u, v, z, yaw_u=0.0):
    """A game of dominoes in progress on a table top: a snake of tiles and a hand (cube-local frame of p)."""
    for k in range(7):
        x0 = u - 0.22 + k * 0.075
        p.box("CT_TileWhite", x0, x0 + 0.06, v - 0.03, v + 0.03, z, z + 0.012)
        p.box("CT_ConcreteDark", x0 + 0.028, x0 + 0.032, v - 0.03, v + 0.03, z + 0.012, z + 0.0125, f="Z")
    for k in range(3):
        p.box("CT_TileWhite", u - 0.12 + k * 0.07, u - 0.06 + k * 0.07, v + 0.17, v + 0.23, z, z + 0.012)


def _tray(p, u, v, z, w=0.36, d=0.24, sprouts=5, seed=0):
    """A seed tray: a box of soil with little green sprouts."""
    p.box("CT_WoodLight", u - w / 2, u + w / 2, v - d / 2, v + d / 2, z, z + 0.05, f="xXyYz")
    p.box("CT_ConcreteDark", u - w / 2 + 0.015, u + w / 2 - 0.015, v - d / 2 + 0.015, v + d / 2 - 0.015, z + 0.04, z + 0.052, f="Z")
    rnd = random.Random(seed)
    for k in range(sprouts):
        a = u - w / 2 + 0.05 + (w - 0.1) * (k + 0.5) / sprouts
        b = v + rnd.uniform(-d / 4, d / 4)
        hh = rnd.uniform(0.06, 0.13)
        p.rod("CT_Leaf", (a, b, z + 0.05), (a + rnd.uniform(-0.02, 0.02), b, z + 0.05 + hh), 0.012, 3)
        p.box("CT_Leaf", a - 0.03, a + 0.03, b - 0.012, b + 0.012, z + 0.05 + hh * 0.8, z + 0.05 + hh * 0.8 + 0.008, f="xXyYZ")


def _can(p, u, v, z):
    """A watering can: a tin body, a long spout, a handle."""
    p.cyl("CT_SteelWhite", u, v, z, z + 0.2, 0.09, 8, r1=0.08, top=False)
    p.rod("CT_SteelWhite", (u + 0.07, v, z + 0.06), (u + 0.26, v, z + 0.24), 0.025, 4)
    p.rod("CT_Steel", (u - 0.06, v, z + 0.2), (u - 0.12, v, z + 0.1), 0.012, 3)


def _tin(p, u, v, z, mat="CT_SteelRed"):
    p.cyl(mat, u, v, z, z + 0.09, 0.065, 8, top=True)
    p.cyl("CT_Brass", u, v, z + 0.09, z + 0.1, 0.067, 8, top=True)


def _frame(p, u, v, z, w=0.14, h=0.18, turn=0.0):
    """A small photo in a wooden frame standing on a shelf (faces -v; turn in degrees about z)."""
    c, s = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    for (a, b, d) in ((-w / 2, w / 2, 0.012),):
        pts = [(u + (x * c - y * s), v + (x * s + y * c)) for x, y in ((a, 0.0), (b, 0.0), (b, d), (a, d))]
        p.face("CT_Wood", [(pts[0][0], pts[0][1], z), (pts[1][0], pts[1][1], z), (pts[1][0], pts[1][1], z + h), (pts[0][0], pts[0][1], z + h)][::-1])
    p.box("CT_Wood", u - w / 2, u + w / 2, v, v + 0.012, z, z + h, f="xXyYZ")
    p.box("CT_TileWhite", u - w / 2 + 0.012, u + w / 2 - 0.012, v - 0.002, v, z + 0.012, z + h - 0.012, f="y")


def _open_house(B, fr, th, zb, who):
    """The open cube: street room, living level, tip (see the section comment above)."""
    _inner_shell(B, fr, zb)
    _lobby_shell(B, fr)
    _street_room(B, fr, th, who)
    _living(B, fr, th, zb, who)
    _tip(B, fr, th, zb, who)


def _street_room(B, fr, th, who):
    A = who == "a"
    h = _House(B, fr, th, 0.0)
    u0, u1, v0, v1, zc = LOBBY
    _lift_door(B, fr, 0.0, v1, 0.0)
    h.thing("lift_g", "door", 0.0, v1 - 0.06, 1.1, 0.0, to="lift")
    # the light switch on the doorway wall, a slate and a clock on the back wall
    p = h.piece(0.0, "hall_switch")
    p.box("CT_ConcreteWhite", -0.89, -0.81, v0, v0 + 0.025, 1.2, 1.4)
    p.box("CT_Steel", -0.86, -0.84, v0 + 0.025, v0 + 0.035, 1.27, 1.33)
    p.done()
    h.thing("cu_hall_switch", "machine", -0.85, v0 + 0.03, 1.3, 180.0, what="switch", switch="room")
    h.kp(kit.wall_clock, -0.85, v1, 0.0, 0.0, zc=2.15, r=0.15, face="clock_1")
    h.thing("cu_hall_clock", "board", -0.85, v1 - 0.04, 2.15, 0.0, topic="clock")
    h.kp(kit.notice_board, 0.9, v1, 0.0, 0.0, w=0.42, zc=1.5, seed=61 if A else 62)
    h.thing("cu_hall_note", "board", 0.9, v1 - 0.04, 1.5, 0.0, topic="note")
    # the left wall: a shoe rack; the corner: a hat stand with the coats
    h.kp(kit.shoe_rack, u0, 0.35, 0.0, 90.0, w=0.8, rows=3, seed=5 if A else 6)
    h.thing("cu_hall_shoes", "board", u0 + 0.2, 0.35, 0.6, 90.0, topic="shoes")
    h.kp(kit.hat_stand, -0.82, 1.05, 0.0, 0.0)
    h.thing("cu_hall_hooks", "board", -0.82, 1.05, 1.3, 0.0, topic="hooks")
    # the right wall: a bench with the boots under it, a mirror over it, a fern in the corner
    h.kp(kit.bench, u1 - 0.22, 0.35, 0.0, 270.0, w=0.8, kind="park")
    h.thing("cu_hall_bench", "seat", u1 - 0.22, 0.35, 0.45, 270.0, height=0.45)
    h.kp(kit.mirror, u1, 0.35, 0.0, 270.0, zc=1.5, w=0.5, h=0.9)
    h.thing("cu_hall_mirror", "board", u1 - 0.03, 0.35, 1.5, 270.0, topic="mirror")
    h.kp(kit.plant, 0.88, 1.12, 0.0, 0.0, kind="fern", pot="CT_PlasterBlue" if A else "CT_PlasterYellow", h=0.8, seed=3)
    h.thing("cu_hall_plant", "board", 0.88, 1.12, 0.6, 0.0, topic="hall_plant")
    B.lamp(h.w(0.0, 0.45, 2.35), lumens=240, colour="#FFD9A8", kind="public", radius=3.6)
    h.spot("cu_hall", "stand", 0.0, 0.5, 0.0, 180.0)


def _living(B, fr, th, zb, who):
    A = who == "a"
    h = _House(B, fr, th, zb)
    F = F1
    wl = _half(F)
    fab = ("CT_PlasterBlue", "CT_PlasterRed", "CT_PlasterGreen") if A else ("CT_PlasterYellow", "CT_PlasterPink", "CT_PlasterGreen")
    _lift_door(B, fr, 0.0, CORE_V, zb + F)
    h.thing("lift_living", "door", 0.0, CORE_V - 0.06, F + 1.1, 0.0, to="lift")
    # --- the window bench across the front end: the long window just over it looks down at the deck
    p = h.piece(F, "window_bench", "floor")
    p.box("CT_Wood", -1.45, 1.45, -VE, -1.93, 0.0, 0.42)
    p.cushion(fab[0], -1.42, -0.04, -VE + 0.03, -1.96, 0.42, 0.52)
    p.cushion(fab[1], 0.04, 1.42, -VE + 0.03, -1.96, 0.42, 0.52)
    p.core(-1.4, 1.4, -VE + 0.02, -1.95, 0.0, 0.41)
    p.done(solid=True, top=0.52)
    # reach audit: the seat thing sits on the right cushion, clear of the low dominoes table in front of the bench middle
    h.thing("cu_liv_window_seat", "seat", 0.45, -2.15, F + 0.52, 0.0, height=0.52)
    h.thing("cu_liv_view_front", "board", 0.0, -VE + 0.03, F + 1.5, 180.0, topic="view")
    h.thing("cu_liv_view_back", "board", 0.75, VE - 0.03, F + 1.5, 0.0, topic="view")      # clear of the kitchen unit's end
    # --- the left ledge: a sofa built into the leaning wall, bolsters against the slope
    p = h.piece(F, "sofa_wedge", "floor")
    _wedge(p, -1, -1.8, -0.2, wl, 1.0, 0.42, "CT_Wood", "CT_Wood")
    for k, (va, vb) in enumerate(((-1.78, -1.0), (-0.98, -0.2))):
        p.cushion(fab[0], -1.85, -1.03, va, vb, 0.42, 0.54)
        bot = [(-1.85, va, 0.54), (-1.62, va, 0.54), (-1.62, vb, 0.54), (-1.85, vb, 0.54)]
        top = [(-2.27, va, 0.96), (-2.04, va, 0.96), (-2.04, vb, 0.96), (-2.27, vb, 0.96)]
        p.hexa(fab[2] if k == 0 else fab[1], bot, top)
    p.done(solid=True, top=0.54)
    h.thing("cu_liv_sofa_a", "seat", -1.4, -1.4, F + 0.54, 90.0, height=0.54)
    h.thing("cu_liv_sofa_b", "seat", -1.4, -0.6, F + 0.54, 90.0, height=0.54)
    h.spot("cu_sofa", "seat", -0.9, -1.0, F, 90.0)
    # --- the right ledge: a low bookshelf with a deep top under the slope
    h.kp(kit.bookshelf, 1.45, -1.1, F, 270.0, w=1.3, h=0.85, levels=3, topic="local" if A else "kids", seed=44 if A else 45)
    p = h.piece(F, "ledge_wedge", "floor")
    _wedge(p, 1, -1.75, -0.45, wl, 1.45, 0.85, "CT_Wood", "CT_WoodLight", core=False)
    p.done()
    h.thing("cu_liv_shelf", "shelf", 1.3, -1.1, F + 0.55, 270.0, topic="local" if A else "kids")
    h.kp(kit.plant, 1.8, -0.7, F + 0.85, 0.0, kind="fern", pot="CT_PlasterYellow" if A else "CT_PlasterRed", h=0.4, seed=11)
    h.kp(kit.radio, 1.85, -1.4, F + 0.85, 270.0)
    h.thing("cu_liv_radio", "machine", 1.85, -1.4, F + 1.1, 270.0, what="radio")
    p = h.piece(F, "ledge_photos")
    for k, (uu, vv, tn) in enumerate(((1.62, -0.85, 6.0), (1.7, -1.7, -8.0), (2.0, -1.25, 3.0))):
        _frame(p, uu, vv, 0.85, w=0.14, h=0.19, turn=90 + tn)
    p.done()
    h.thing("cu_liv_photos", "board", 1.7, -0.95, F + 1.05, 270.0, topic="photos")
    # --- the kitchen along the right wall, its worktop reaching the slope
    ku = h.kp(kit.kitchen_unit, 1.48, 0.6, F, 270.0, modules="cupboard,sink,stove", wall_cupboards=False)
    p = h.piece(F, "worktop_wedge", "floor")
    _wedge(p, 1, -0.3, 1.5, wl, 1.48, 0.896, "CT_PlasterGrey", "CT_PlasterGrey", core=False)
    p.done()
    h.kp(kit.teapot, 1.9, 0.2, F + 0.9, 90.0)
    h.kp(kit.cup, 1.7, 0.45, F + 0.9, 0.0)
    h.kp(kit.plant, 2.0, 0.8, F + 0.9, 0.0, kind="cactus", pot="CT_PlasterRed", h=0.3, seed=21)
    h.kp(kit.plant, 1.9, 1.38, F + 0.9, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.3, seed=22)
    h.thing("cu_liv_stove", "machine", 1.2, 0.0, F + 0.95, 270.0, what="stove")
    h.thing("cu_liv_sink", "machine", 1.2, 0.6, F + 0.95, 270.0, what="sink")
    # --- the left nook: the fridge with the household's notes; the middle of the left wall belongs to the household
    h.kp(kit.kitchen_unit, -1.48, 2.0, F, 90.0, modules="fridge")
    h.kp(kit.poster, -0.86, 1.88, F, 90.0, name="notice", w=0.14, zc=1.42, seed=1)
    h.kp(kit.poster, -0.86, 2.12, F, 90.0, name="notice", w=0.17, zc=1.25, seed=2)
    h.thing("cu_liv_fridge", "board", -0.86, 2.0, F + 1.3, 90.0, topic="fridge")
    if A:                                   # Gennady's arcade cabinet in the corner of the wall
        h.kp(kit.arcade, -1.05, 0.45, F, 90.0, mat="CT_PlasterBlue", seed=3)
        h.thing("cu_liv_play", "machine", -1.05, 0.45, F + 1.1, 90.0, what="arcade")
        h.kp(kit.bookshelf, -1.45, 1.15, F, 90.0, w=0.7, h=0.95, levels=3, topic="sea", seed=46)
        h.thing("cu_liv_games", "shelf", -1.3, 1.15, F + 0.6, 90.0, topic="games")
    else:                                   # Mette's corner: a toy bin and an easel
        h.kp(kit.toy_bin, -1.12, 0.2, F, 90.0, mat="CT_PlasterRed", seed=4)
        h.thing("cu_liv_play", "board", -1.12, 0.2, F + 0.4, 90.0, topic="toys")
        h.kp(kit.easel, -1.1, 0.95, F, 90.0, canvas="painting_sea_calm", seed=5)
        h.thing("cu_liv_easel", "craft", -1.1, 0.95, F + 1.0, 90.0, what="easel")
    # --- the middle: a rug and the low table with a game of dominoes
    h.kp(kit.rug, 0.0, -1.05, F, 0.0, w=1.7, d=1.8, name="rug", seed=7 if A else 8)
    h.kp(kit.coffee_table, 0.0, -1.05, F, 90.0, w=1.0, d=0.55)
    p = h.piece(F, "dominoes")
    _dominoes(p, 0.0, -1.05, 0.42)
    p.done()
    h.thing("cu_liv_table", "play", 0.0, -1.05, F + 0.47, 90.0, what="dominoes")
    # --- the right nook: a palm, the light switch on the core's side
    h.kp(kit.plant, 1.05, 2.1, F, 0.0, kind="monstera", pot="CT_PlasterBlue" if A else "CT_PlasterGreen", h=1.2, seed=13)
    p = h.piece(F, "living_switch")
    p.box("CT_ConcreteWhite", CORE_U, CORE_U + 0.025, 1.78, 1.86, 1.2, 1.4)
    p.box("CT_Steel", CORE_U + 0.025, CORE_U + 0.035, 1.8, 1.84, 1.27, 1.33)
    p.done()
    h.thing("cu_liv_switch", "machine", CORE_U + 0.03, 1.82, F + 1.3, 90.0, what="switch", switch="room")
    h.lamp(0.0, -1.0, F + 1.95, 420, 6.5)
    h.lamp(0.2, 0.9, F + 1.95, 360, 5.5, "#FFE0B0")
    h.spot("cu_window", "view", 0.0, -1.8, F, 0.0)
    h.spot("cu_kitchen", "work", 0.55, 0.3, F, 90.0)
    h.spot("cu_table", "seat", 0.0, -0.45, F, 180.0)


def _tip(B, fr, th, zb, who):
    A = who == "a"
    h = _House(B, fr, th, zb)
    F = F2
    wl = _half(F)
    _lift_door(B, fr, 0.0, CORE_V, zb + F)
    h.thing("lift_tip", "door", 0.0, CORE_V - 0.06, F + 1.1, 0.0, to="lift")
    h.kp(kit.wall_clock, 0.0, CORE_V, F, 0.0, zc=2.52, r=0.14, face="clock_2")
    h.thing("cu_tip_clock", "board", 0.0, CORE_V - 0.04, F + 2.52, 0.0, topic="clock")
    p = h.piece(F, "tip_switch")
    p.box("CT_ConcreteWhite", -CORE_U - 0.025, -CORE_U, 1.78, 1.86, 1.2, 1.4)
    p.box("CT_Steel", -CORE_U - 0.035, -CORE_U - 0.025, 1.8, 1.84, 1.27, 1.33)
    p.done()
    h.thing("cu_tip_switch", "machine", -CORE_U - 0.03, 1.82, F + 1.3, 270.0, what="switch", switch="room")
    # --- the bed under the right skylight, its head at the back wall
    h.kp(kit.bed, 1.4, 1.3, F, 0.0, size="single", blanket="CT_PlasterYellow" if A else "CT_PlasterPink")
    h.thing("cu_tip_bed", "bed", 1.4, 1.3, F + 0.5, 0.0, rest="private", height=0.5)
    h.thing("cu_tip_sky", "board", 1.68, 0.0, 5.5, 270.0, topic="view")
    h.thing("cu_tip_sky_l", "board", -1.68, 0.0, 5.5, 90.0, topic="view")
    h.thing("cu_tip_view_floor", "board", 0.0, -VE + 0.03, F + 0.4, 180.0, topic="view")
    # --- the greenhouse bench along the left wall, under the left skylight
    p = h.piece(F, "garden_wedge", "floor")
    _wedge(p, -1, -1.7, 1.9, wl, 1.25, 0.5, "CT_Wood", "CT_WoodLight", upper=True)
    p.done(solid=True, top=0.5)
    p = h.piece(F, "garden_things")
    for k, vv in enumerate((-1.5, -0.95, -0.4)):
        _tray(p, -1.6, vv, 0.5, w=0.3, d=0.4, sprouts=5, seed=k + (0 if A else 5))
    _can(p, -1.5, 0.05, 0.5)
    _tin(p, -1.4, 0.5, 0.5, "CT_SteelRed" if A else "CT_Brass")
    _tin(p, -1.72, 0.52, 0.5, "CT_Steel")
    p.done()
    h.kp(kit.books_stack, -1.55, 0.9, F + 0.5, 90.0, n=2, seed=9)
    kinds = (("ficus", "CT_PlasterRed", 0.55), ("monstera", "CT_PlasterBlue", 0.5), ("fern", "CT_PlasterYellow", 0.4),
             ("cactus", "CT_PlasterRed", 0.3), ("palm", "CT_PlasterGreen", 0.6))
    for k, vv in enumerate((1.3, 1.78)):
        kind, pot, hh = kinds[(k + (0 if A else 2)) % len(kinds)]
        h.kp(kit.plant, -1.5, vv, F + 0.5, 0.0, kind=kind, pot=pot, h=hh, seed=30 + k)
    h.thing("cu_tip_trays", "board", -1.6, -0.95, F + 0.65, 90.0, topic="seedlings")
    h.thing("cu_tip_garden", "shelf", -1.55, 0.9, F + 0.65, 90.0, topic="garden")
    h.thing("cu_tip_can", "machine", -1.5, 0.05, F + 0.6, 90.0, what="watering")
    h.thing("cu_tip_seeds", "take", -1.4, 0.5, F + 0.6, 90.0, item="seeds")
    # --- the front: cushions on the floor under the low window, a low shelf on the right
    h.kp(kit.bean_bag, 0.0, -1.7, F, 0.0, mat="CT_PlasterYellow" if A else "CT_PlasterGreen")
    h.thing("cu_tip_beanbag", "seat", 0.0, -1.7, F + 0.4, 0.0, height=0.4)
    for sg in (-1, 1):
        h.kp(kit.zabuton, sg * 0.85, -2.0, F, 0.0, mat="CT_PlasterRed" if A else "CT_PlasterBlue")
    h.thing("cu_tip_cushion_l", "seat", -0.85, -2.0, F + 0.07, 0.0, height=0.07)
    h.thing("cu_tip_cushion_r", "seat", 0.85, -2.0, F + 0.07, 0.0, height=0.07)
    p = h.piece(F, "tip_shelf", "floor")
    _wedge(p, 1, -2.2, -0.55, wl, 1.3, 0.55, "CT_Wood", "CT_WoodLight", upper=True)
    p.done(solid=True, top=0.55)
    h.kp(kit.books_stack, 1.55, -1.7, F + 0.55, 0.0, n=4, seed=15)
    h.kp(kit.books_stack, 1.7, -1.1, F + 0.55, 90.0, n=3, seed=16)
    if A:
        h.kp(kit.globe, 1.6, -0.8, F + 0.55, 0.0)
    else:
        h.kp(kit.vase, 1.6, -0.8, F + 0.55, 0.0, seed=2)
    h.thing("cu_tip_shelf", "shelf", 1.6, -1.4, F + 0.7, 270.0, topic="sun" if A else "kids")
    h.lamp(0.0, 0.0, F + 2.3, 380, 6.0, "#FFE4B8")
    h.spot("cu_tip_beanbag_spot", "seat", 0.0, -1.7, F, 0.0)
    h.spot("cu_tip_plants", "work", -0.85, 0.3, F, 270.0)
