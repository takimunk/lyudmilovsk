"""Capsule tower: two concrete service cores with prefabricated living capsules plugged onto them,
each capsule one flat with one big round window. An original design in the spirit of 1970s Japanese
Metabolism (prefab capsules on cores), not a copy of any building."""
import math
import random

from citykit import box, prism, ring, DOOR_W, DOOR_H

INFO = {
    "name_ru": "Капсульная башня", "name_en": "Capsule Tower",
    "style": "metabolism", "use": "residential",
    "inspired_by": "Japanese Metabolism of the early 1970s: prefabricated steel capsules bolted to concrete cores",
}

CAP_W, CAP_D, CAP_H = 2.5, 4.0, 2.5         # one capsule (a studio flat)
STOREY = 2.8
STOREYS = 12
CORE = 6.0                                    # each core is CORE x CORE
GAP = 4.0                                     # the bridge between the two cores
WIN_R = 0.62


def _round_window(B, cx, cz, face_y, out, flat, axis="y", fx=None):
    """A round window (a 16-gon card) on a capsule's face; out = +1/-1 the outward sign along the axis."""
    segs = 16
    pts = []
    for k in range(segs):
        a = 2 * math.pi * k / segs
        u, v = WIN_R * math.cos(a), WIN_R * math.sin(a)
        if axis == "y":
            pts.append((cx - u * out, face_y, cz + v))
        else:
            pts.append((fx, cx + u * out, cz + v))
    # the card: approximate the circle by its bounding quad (the window shader masks a circle by UV)
    if axis == "y":
        q = [(cx - WIN_R * out, face_y, cz - WIN_R), (cx + WIN_R * out, face_y, cz - WIN_R),
             (cx + WIN_R * out, face_y, cz + WIN_R), (cx - WIN_R * out, face_y, cz + WIN_R)]
    else:
        q = [(fx, cx + WIN_R * out, cz - WIN_R), (fx, cx - WIN_R * out, cz - WIN_R),
             (fx, cx - WIN_R * out, cz + WIN_R), (fx, cx + WIN_R * out, cz + WIN_R)]
    # bezel ring (detail) standing 6 cm off the face
    for k in range(segs):
        a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
        r0, r1 = WIN_R, WIN_R + 0.12
        def P(r, a, o):
            u, v = r * math.cos(a), r * math.sin(a)
            if axis == "y":
                return (cx - u * out, face_y + out * o, cz + v)
            return (fx + out * o, cx + u * out, cz + v)
        B.detail.face("CT_SteelWhite", [P(r0, a0, 0.06), P(r0, a1, 0.06), P(r1, a1, 0.06), P(r1, a0, 0.06)][::-1])
        B.detail.face("CT_SteelWhite", [P(r0, a0, 0.0), P(r0, a1, 0.0), P(r0, a1, 0.06), P(r0, a0, 0.06)])
    return B.window(q, flat=flat, kind="flat", inset=0.05, frame=0, shape="round")


def build(B):
    rnd = random.Random(1972)
    W = CORE * 2 + GAP
    x0 = -W / 2
    cores = [(x0, x0 + CORE), (x0 + CORE + GAP, x0 + W)]
    y0, y1 = 6.0, 6.0 + CORE                      # cores stand 6 m back from the street line
    top = STOREYS * STOREY + 2.0
    for cx0, cx1 in cores:
        prism(B.m, "CT_Concrete", [(cx0, y0), (cx1, y0), (cx1, y1), (cx0, y1)], 0, top)
        # stair windows: a slit up each core's street face
        for s in range(1, STOREYS):
            z = s * STOREY + 0.8
            u = (cx0 + cx1) / 2
            B.window([(u - 0.3, y0, z), (u + 0.3, y0, z), (u + 0.3, y0, z + 1.4), (u - 0.3, y0, z + 1.4)],
                     kind="stair", inset=0.02, frame=0.03)
    # bridges between the cores every storey (walkways to the capsules' doors)
    for s in range(1, STOREYS + 1):
        z = s * STOREY
        box(B.m, "CT_ConcreteDark", cores[0][1], cores[1][0], y0 + 1.5, y1 - 1.5, z - 0.25, z)
        B.railing([(cores[0][1], y0 + 1.5), (cores[1][0], y0 + 1.5)], z, h=1.1)
        B.railing([(cores[1][0], y1 - 1.5), (cores[0][1], y1 - 1.5)], z, h=1.1)
    # roof: the lift machine rooms
    box(B.m, "CT_ConcreteDark", cores[0][0] + 1, cores[0][1] - 1, y0 + 1, y1 - 1, top, top + 2.4)
    box(B.m, "CT_ConcreteDark", cores[1][0] + 1, cores[1][1] - 1, y0 + 1, y1 - 1, top, top + 2.4)
    # ground floor: a glazed lobby between and under the cores, a canopy over the door
    lob = (x0 - 1.0, x0 + W + 1.0, 2.0, y1 + 1.0)
    box(B.m, "CT_ConcreteDark", lob[0], lob[1], lob[2], lob[3], 3.6, 3.9)               # lobby roof slab
    for u in (lob[0] + 0.15, lob[1] - 0.15):
        box(B.m, "CT_Concrete", u - 0.15, u + 0.15, lob[2], lob[2] + 0.3, 0, 3.6)
    # lobby glazing: big shop cards along the street front with the door gap in the middle
    gx = [lob[0] + 0.3, -DOOR_W / 2 - 0.2, DOOR_W / 2 + 0.2, lob[1] - 0.3]
    for a, b in ((gx[0], gx[1]), (gx[2], gx[3])):
        n = max(1, int((b - a) / 1.8))
        for i in range(n):
            u0 = a + (b - a) * i / n
            u1 = a + (b - a) * (i + 1) / n
            B.window([(u0, lob[2], 0.1), (u1, lob[2], 0.1), (u1, lob[2], 3.5), (u0, lob[2], 3.5)],
                     kind="public", inset=0.0, frame=0.06)
    box(B.m, "CT_Steel", -2.0, 2.0, -0.6, lob[2], 3.3, 3.45)                                # canopy
    B.entrance("main", (0.0, lob[2], 0.0), 0.0, width=DOOR_W * 2, leaves=2, kind="street")
    # capsules: on the outer faces of the cores, staggered, each a flat
    n_flat = 0
    sides = []
    for ci, (cx0, cx1) in enumerate(cores):
        sides.append(("front", ci, cx0, cx1))
        sides.append(("back", ci, cx0, cx1))
        sides.append(("outer", ci, cx0, cx1))
    for s in range(1, STOREYS + 1):
        zf = s * STOREY - 0.3
        k = 0
        for side, ci, cx0, cx1 in sides:
            if side in ("front", "back"):
                count = 2
                for i in range(count):
                    if rnd.random() < 0.12:
                        continue
                    jitter = rnd.uniform(-0.15, 0.15)
                    u = cx0 + 0.25 + i * (CAP_W + 0.5) + jitter
                    tilt = rnd.choice((0, 0, 0.35))
                    if side == "front":
                        ya, yb = y0 - CAP_D, y0
                        face, out = ya, -1
                    else:
                        ya, yb = y1, y1 + CAP_D
                        face, out = yb, 1
                    za = zf + tilt
                    box(B.m, "CT_Aluminium", u, u + CAP_W, ya, yb, za, za + CAP_H)
                    k += 1
                    fid = "%d-%02d" % (s, k)
                    B.flat(fid, storey=s, beds=1, door=((u + CAP_W / 2), y0 if side == "front" else y1, zf + 0.3),
                           entrance="main")
                    cz = za + CAP_H / 2 + 0.05
                    _round_window(B, u + CAP_W / 2, cz, face, out, fid, axis="y")
            else:
                # the outer side of each core: two capsules side by side along y, facing out along x
                xs = (cx0 - CAP_D, cx0) if ci == 0 else (cx1, cx1 + CAP_D)
                out = -1 if ci == 0 else 1
                for i in range(2):
                    if rnd.random() < 0.12:
                        continue
                    v = y0 + 0.25 + i * (CAP_W + 0.5)
                    za = zf + rnd.choice((0, 0, 0.35))
                    box(B.m, "CT_Aluminium", xs[0], xs[1], v, v + CAP_W, za, za + CAP_H)
                    k += 1
                    fid = "%d-%02d" % (s, k)
                    fx = xs[0] if ci == 0 else xs[1]
                    B.flat(fid, storey=s, beds=1, door=(cx0 if ci == 0 else cx1, v + CAP_W / 2, zf + 0.3), entrance="main")
                    _round_window(B, v + CAP_W / 2, za + CAP_H / 2 + 0.05, None, out, fid, axis="x", fx=fx)
        n_flat += k
    B.footprint = [(x0 - CAP_D, y0 - CAP_D), (x0 + W + CAP_D, y0 - CAP_D), (x0 + W + CAP_D, y1 + CAP_D), (x0 - CAP_D, y1 + CAP_D)]
    B.footprint = [(min(p[0], lob[0]), min(p[1], lob[2])) if False else p for p in B.footprint]
    B.height = top + 2.4
    B.spot("lobby_bench", "bench", (-3.0, lob[2] + 1.5, 0.0), 0.0)
    B.note("%d capsule flats on %d storeys; capsules jut %.1f m over the street line at the front" % (n_flat, STOREYS, CAP_D - y0 if CAP_D > y0 else 0))
