"""Capsule library: a public library and reading room. A tall octagonal concrete core (stair, lantern room on top)
stands in a glazed ground-floor hall; reading pods (small enterable capsules, each with one big round or rounded-square
window, a desk, a chair and a lamp) are cantilevered from its faces on three levels, tilted in plan in 45 degree
steps. An open concrete stair climbs along the core to the first pod level; the core has its own stair too.
Original design in the spirit of Japanese Metabolism (Kurokawa, Kikutake: capsules plugged onto a service core);
not a copy of any building.

Plot about 30 x 30, height_max 20. Footprint x -13..13, y 3..27 (hall 26 x 24), core centre (0, 15), apothem 4.0.
Levels (floor tops): hall 0 (ceiling 3.6, terrace roof 3.9), pods L1 4.5, L2 8.5, L3 12.5, core roof 16.5, mast to 19.
Hall (ENTERABLE): 9 bays of glass cards on the street front (double door "main" in the middle bay), bays on the sides
and back, a service door "staff" in the east wall; lending counter (librarian), card catalogue, two double-sided book
stacks and book walls round the core, reading tables, window tables. The core door and stairs lead up to every pod.
Windows are all kind "public" (open hours: INFO hours 9..21; the importer's HOURS table decides) or "stair".
Spots: counter / work / stand / seat / table (yaw = the way the person faces; 0 = -y toward the street).
Collision: floors, walls, stairs, counter and table tops are in the shell; chairs, books and lamps are detail.
"""
import math
import random

import furn_int3 as fu
from citykit import box, prism, DOOR_W, DOOR_H
from _surrounds import add_surrounds
from capsule_cluster import _clip, _sat, _rect, _quad_out, _tri_faces, _poly, _edges

INFO = {
    "name_ru": "Библиотека-капсулы", "name_en": "Capsule Library",
    "style": "metabolism", "use": "public", "kind": "library", "hours": [9, 21],
    "inspired_by": "Japanese Metabolism of the 1960s-70s (Kurokawa, Kikutake): prefab capsules cantilevered from a "
                   "concrete service core; here as reading pods round a tall core over a glass-walled library hall",
}

SEED = 1972
VARIANTS = {}
HX0, HX1, HY0, HY1 = -13.0, 13.0, 3.0, 27.0
HALL_H = 3.6
CORE = dict(id="L", kind="oct", c=(0.0, 15.0), a=4.0)
CX, CY = CORE["c"]
LEVELS = [0.0, 4.5, 8.5, 12.5]
ROOF = 16.5
WALL = 0.3
POD_W, POD_D, POD_IN = 3.1, 4.2, 2.7
BOOKS = ["CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen", "CT_SteelWhite", "CT_Door",
         "CT_WoodLight", "CT_PlasterPink", "CT_Plaster", "CT_Frame"]
STAIR_X0, STAIR_X1 = -5.8, -4.2            # the open stair beside the core (west)
GOING = 0.28


def _slab(m, poly, z0, z1, top, side="CT_Concrete", bottom="CT_ConcreteDark"):
    prism(m, side, poly, z0, z1, top=False, bottom=False)
    m.face(top, [(x, y, z1) for x, y in poly])
    m.face(bottom, [(x, y, z0) for x, y in reversed(poly)])


def _bays(L, n, doors=None):
    """Hole list (u0, u1) of n bays along a wall of length L; doors {bay index: (width)} are returned apart."""
    pitch = L / n
    out = []
    for i in range(n):
        u0, u1 = i * pitch + 0.22, (i + 1) * pitch - 0.22
        out.append((u0, u1, i))
    return out


# ---------------------------------------------------------------- core
def _core(B, pods_by_level, reserved):
    core = CORE
    poly = _poly(core)
    edges = _edges(poly)
    core["poly"], core["edges"] = poly, edges

    def edge_idx(yaw):
        for i, e in enumerate(edges):
            if abs(((e["yaw"] - yaw + 180) % 360) - 180) < 1.0:
                return i

    holes = {i: [] for i in range(len(edges))}
    wins = {i: [] for i in range(len(edges))}
    s_door = edge_idx(0.0)                                   # ground door, south face, onto the hall
    e = edges[s_door]
    holes[s_door].append((e["L"] / 2 - 0.85, e["L"] / 2 + 0.85, 0.0, 2.4))
    n_door = edge_idx(180.0)                                 # a second ground door, north face
    e = edges[n_door]
    holes[n_door].append((e["L"] / 2 - 0.85, e["L"] / 2 + 0.85, 0.0, 2.4))
    w_door = edge_idx(270.0)                                 # the open stair arrives here at level 1
    e = edges[w_door]
    holes[w_door].append((e["L"] / 2 - 0.75, e["L"] / 2 + 0.75, LEVELS[1], LEVELS[1] + 2.35))
    for lv, plist in pods_by_level.items():
        for ei in plist:
            e = edges[ei]
            holes[ei].append((e["L"] / 2 - 0.8, e["L"] / 2 + 0.8, LEVELS[lv], LEVELS[lv] + 2.4))
    for ei, e in enumerate(edges):
        for lv in (1, 2):
            if ei in pods_by_level.get(lv, []) or (ei == w_door and lv == 1):
                continue
            z = LEVELS[lv]
            wins[ei].append((e["L"] / 2 - 0.35, e["L"] / 2 + 0.35, z + 0.9, z + 2.6))
        if ei not in pods_by_level.get(3, []):                                 # lantern windows of the top room
            wins[ei].append((e["L"] / 2 - 0.9, e["L"] / 2 + 0.9, LEVELS[3] + 0.9, LEVELS[3] + 3.2))
    top = ROOF
    for i, e in enumerate(edges):
        F = B.facade(e["p0"], e["p1"], 0.0, top - WALL, "CT_Concrete", holes=holes[i] + wins[i], depth=WALL,
                     reveal_mat="CT_ConcreteDark")
        for (u0, u1, h0, h1) in wins[i]:
            kind = "public" if h0 > LEVELS[3] else "stair"
            B.facade_window(F, u0, u1, h0, h1, kind=kind, inset=0.18)
    k = (core["a"] - WALL) / core["a"]
    inner = [(CX + (x - CX) * k, CY + (y - CY) * k) for x, y in poly]
    prism(B.m, "CT_Floor", inner, -0.3, 0.0, top=True, bottom=False)
    for lv in (1, 2, 3):
        zf = LEVELS[lv]
        for (x0, x1, y0, y1) in ((CX + 0.2, CX + 9, CY - 9, CY + 9), (CX - 9, CX - 2.5, CY - 9, CY + 9),
                                 (CX - 2.5, CX + 0.2, CY - 9, CY - 2.1), (CX - 2.5, CX + 0.2, CY + 2.6, CY + 9)):
            piece = _clip(inner, x0, x1, y0, y1)
            if piece:
                prism(B.m, "CT_Concrete", piece, zf - WALL, zf, top=True, bottom=True)
        B.railing([(CX + 0.25, CY - 2.05), (CX + 0.25, CY + 2.55)], zf, h=1.0)
    _slab(B.m, poly, top - WALL, top, "CT_Roof")
    ro = [(CX + (x - CX) * 0.93, CY + (y - CY) * 0.93) for x, y in poly]
    ri = [(CX + (x - CX) * 0.88, CY + (y - CY) * 0.88) for x, y in poly]
    for i in range(len(ro)):
        a0, a1, b0, b1 = ro[i], ro[(i + 1) % 8], ri[i], ri[(i + 1) % 8]
        B.detail.face("CT_ConcreteDark", [(a0[0], a0[1], top + 0.05), (a1[0], a1[1], top + 0.05),
                                           (b1[0], b1[1], top + 0.05), (b0[0], b0[1], top + 0.05)][::-1])
    box(B.m, "CT_ConcreteDark", CX - 1.2, CX + 1.2, CY + 0.4, CY + 2.0, top, top + 1.0)                    # roof hatch
    fu.rod(B.detail, "CT_Steel", (CX + 1.4, CY - 1.4, top), (CX + 1.4, CY - 1.4, 19.0), 0.07)             # mast
    for h in (17.2, 18.0):
        fu.rod(B.detail, "CT_Steel", (CX + 1.2, CY - 1.4, h), (CX + 1.6, CY - 1.4, h), 0.04)
    # stairs
    for s in range(3):
        z = LEVELS[s]
        S = LEVELS[s + 1] - LEVELS[s]
        nf = int(math.ceil(S / 2 / 0.165))
        r = S / (2 * nf)
        y0 = CY - 2.1
        for i in range(nf):
            ya = y0 + GOING * i
            box(B.m, "CT_Concrete", CX - 2.5, CX - 1.3, ya, ya + GOING, z + r * (i + 1) - 0.3, z + r * (i + 1))
        ye = y0 + GOING * nf
        box(B.m, "CT_Concrete", CX - 2.5, CX + 0.1, ye, CY + 2.6, z + nf * r - 0.3, z + nf * r)
        for j in range(nf):
            yb = ye - GOING * j
            box(B.m, "CT_Concrete", CX - 1.1, CX + 0.1, yb - GOING, yb, z + (nf + j + 1) * r - 0.3, z + (nf + j + 1) * r)
    # lamps in the core
    for z in (3.9, 7.9, 11.9, 14.6):
        B.lamp((CX - 0.8, CY - 2.35, z), lumens=420, colour="#FFE4B8", kind="stair", radius=5.0)
    return edges, s_door, w_door


# ---------------------------------------------------------------- pods
def _pod(B, rnd, e, lv, k, skin, accent, shape, lamp_on):
    zf = LEVELS[lv]
    fr = fu.Fr(e["mid"][0], e["mid"][1], e["yaw"], zf)
    W, D = POD_W, POD_D
    P = [fr.p(-W / 2, 0.0)[:2], fr.p(-W / 2, D)[:2], fr.p(W / 2, D)[:2], fr.p(W / 2, 0.0)[:2]]      # CCW in plan
    z0, z1 = zf - WALL, zf + POD_IN + WALL
    # walls (the back is open to the core)
    B.facade(P[0], P[1], z0, z1, skin, depth=0.2)
    B.facade(P[2], P[3], z0, z1, skin, depth=0.2)
    zs0, zs1 = zf + 0.85, zf + 2.25
    F = B.facade(P[1], P[2], z0, z1, accent, holes=[(W / 2 - 0.7, W / 2 + 0.7, zs0, zs1)], depth=0.2)
    pts = [F.to_building((u, 0.0, z)) for u, z in ((W / 2 - 0.7, zs0), (W / 2 + 0.7, zs0), (W / 2 + 0.7, zs1), (W / 2 - 0.7, zs1))]
    if shape == "round":
        B.window(pts, kind="public", inset=0.12, frame=0, shape="round")
        zc = (zs0 + zs1) / 2
        r = 0.7
        out = (F.n[0], F.n[1], 0.0)
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            arc = []
            for q in range(5):
                a = math.pi / 2 * q / 4
                du = su * r * math.cos(a)
                dv = sv * r * math.sin(a)
                arc.append(F.to_building((W / 2 + du, 0.04, zc + dv)))
            corner = F.to_building((W / 2 + su * r, 0.04, zc + sv * r))
            for q in range(4):
                tri = [corner, arc[q], arc[q + 1]]
                B.detail.face("CT_Frame", tri if _tri_faces(tri, out) else tri[::-1])
    else:
        B.window(pts, kind="public", inset=0.12, frame=0.07, cols=1, rows=1)
        out = (F.n[0], F.n[1], 0.0)
        zc = (zs0 + zs1) / 2
        r, c = 0.7, 0.24
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            tri = [F.to_building((W / 2 + su * r, 0.04, zc + sv * (r - c))), F.to_building((W / 2 + su * (r - c), 0.04, zc + sv * r)),
                   F.to_building((W / 2 + su * r, 0.04, zc + sv * r))]
            B.detail.face("CT_Frame", tri if _tri_faces(tri, out) else tri[::-1])
    # a bezel ring round the window on the outside (a frame of four strips, 5 cm proud)
    # floor, ceiling and roof slabs (inset 2 cm so that they hide behind the walls)
    ins = [fr.p(-W / 2 + 0.02, -0.05)[:2], fr.p(-W / 2 + 0.02, D - 0.02)[:2], fr.p(W / 2 - 0.02, D - 0.02)[:2],
           fr.p(W / 2 - 0.02, -0.05)[:2]]
    _slab(B.m, ins, zf - WALL, zf, "CT_Floor", side="CT_ConcreteDark", bottom="CT_ConcreteDark")
    _slab(B.m, ins, zf + POD_IN, zf + POD_IN + WALL, "CT_Roof", side="CT_Concrete", bottom="CT_Interior")
    # furniture: a desk under the window, a chair, a lamp, a shelf of books on the left wall
    dw = D - 0.2 - 0.38
    fu.lbox(B.detail, "CT_WoodLight", fr, -0.75, 0.75, dw - 0.34, dw + 0.34, 0.72, 0.76)
    for su in (-0.7, 0.7):
        fu.lbox(B.detail, "CT_Steel", fr, su - 0.025, su + 0.025, dw - 0.3, dw + 0.3, 0.0, 0.72, faces="xXyY")
    cp = fr.p(0.0, dw - 0.78)
    fu.chair(B, cp[0], cp[1], e["yaw"], seat_mat=rnd.choice(("CT_PlasterBlue", "CT_SteelRed", "CT_PlasterGreen", "CT_PlasterYellow")),
             z=zf)
    lp = fr.p(0.55, dw + 0.05)
    fu.cyl(B.detail, "CT_Steel", lp[0], lp[1], zf + 0.76, zf + 0.79, 0.09, 8)
    lq = fr.p(0.55, dw + 0.05)
    fu.rod(B.detail, "CT_Steel", (lq[0], lq[1], zf + 0.79), (lq[0] - fr.s * 0.0, lq[1] + fr.c * 0.0, zf + 1.1), 0.025)
    sh = fr.p(0.55, dw - 0.05)
    fu.cyl(B.detail, "CT_Opal", sh[0], sh[1], zf + 1.0, zf + 1.18, 0.13, 8)
    # book wall on the left
    sfr = fu.Fr(*fr.p(-W / 2 + 0.2, D - 0.55)[:2], e["yaw"] + 90, zf)
    _stack(B, sfr, 0.0, 1.8, rnd, depth=0.26, tiers=3, zmax=1.9, z0=0.2, solid=False)
    # a cushion bench along the right wall
    fu.lbox(B.detail, rnd.choice(("CT_PlasterRed", "CT_PlasterBlue", "CT_Aluminium")), fr, W / 2 - 0.75, W / 2 - 0.22, 1.1, 2.4, 0.0, 0.42)
    # the reader
    B.spot("pod_%d_%02d" % (lv, k), "seat", (cp[0], cp[1], zf), e["yaw"])
    if lamp_on:
        B.lamp(fr.p(0.0, D / 2 + 0.2)[:2] + (zf + POD_IN - 0.25,), lumens=300, colour="#FFE2B0", kind="public", radius=4.0)
    return P


def _stack(B, fr, u0, u1, rnd, depth=0.4, tiers=4, zmax=2.1, z0=0.1, solid=True):
    """Book shelving along u0..u1 of frame fr (w out into the room): carcass in the shell, book runs in detail."""
    m = B.m if solid else B.detail
    d = B.detail
    fu.lbox(m, "CT_WoodLight", fr, u0, u1, 0.0, 0.025, z0, zmax)
    n_up = max(2, int((u1 - u0) / 1.6) + 1)
    for i in range(n_up):
        u = u0 + (u1 - u0 - 0.04) * i / (n_up - 1)
        fu.lbox(m, "CT_WoodLight", fr, u, u + 0.04, 0.0, depth, z0, zmax)
    fu.lbox(m, "CT_WoodLight", fr, u0, u1, 0.0, depth, z0, z0 + 0.05)
    fu.lbox(m, "CT_WoodLight", fr, u0, u1, 0.0, depth, zmax - 0.03, zmax)
    pitch = (zmax - z0 - 0.05) / tiers
    for t in range(tiers):
        zb = z0 + 0.05 + pitch * t
        if t:
            fu.lbox(m, "CT_WoodLight", fr, u0, u1, 0.0, depth, zb - 0.02, zb + 0.015)
        zt = zb + (0.015 if t else 0.0)
        room = pitch - 0.07
        u = u0 + 0.06
        while u < u1 - 0.15:
            if rnd.random() < 0.12:
                u += rnd.uniform(0.15, 0.3)
                continue
            w = rnd.uniform(0.22, 0.5)
            w = min(w, u1 - 0.06 - u)
            h = min(room - 0.02, rnd.uniform(0.2, 0.32))
            fu.lbox(d, rnd.choice(BOOKS), fr, u, u + w, 0.04, depth - 0.05, zt, zt + h, faces="xXyYZ")
            u += w


# ---------------------------------------------------------------- hall
def _hall(B, rnd):
    L_w, L_d = HX1 - HX0, HY1 - HY0
    hall = [(HX0, HY0), (HX1, HY0), (HX1, HY1), (HX0, HY1)]
    prism(B.m, "CT_Floor", hall, -0.3, 0.0, top=True, bottom=False)
    walls = [((HX0, HY0), (HX1, HY0), 9), ((HX1, HY0), (HX1, HY1), 8), ((HX1, HY1), (HX0, HY1), 9), ((HX0, HY1), (HX0, HY0), 8)]
    for wi, (p0, p1, n) in enumerate(walls):
        L = math.dist(p0, p1)
        holes, cards = [], []
        door = None
        for (u0, u1, i) in _bays(L, n):
            if wi == 0 and i == n // 2:
                holes.append((u0, u1, 0.0, 2.55))
                cards.append((u0, u1, 2.65, HALL_H - 0.3))
                door = (L / 2)
                holes.append((u0, u1, 2.65, HALL_H - 0.3))
            elif wi == 1 and i == 6:
                uc = (i + 0.5) * L / n
                holes.append((uc - 0.7, uc + 0.7, 0.0, 2.3))
                B.entrance("staff", (p0[0] + (p1[0] - p0[0]) * uc / L, p0[1] + (p1[1] - p0[1]) * uc / L, 0.0), 90.0,
                           width=1.4, height=2.3, kind="service", leaves=1)
                cards.append((uc - 0.7, uc + 0.7, 2.4, HALL_H - 0.3))
                holes.append((uc - 0.7, uc + 0.7, 2.4, HALL_H - 0.3))
            else:
                holes.append((u0, u1, 0.25, HALL_H - 0.3))
                cards.append((u0, u1, 0.25, HALL_H - 0.3))
        F = B.facade(p0, p1, 0.0, HALL_H, "CT_Concrete", holes=holes, depth=0.3, reveal_mat="CT_ConcreteDark")
        for (u0, u1, h0, h1) in cards:
            cols = max(1, int((u1 - u0) / 1.2))
            rows = 3 if h1 - h0 > 2.5 else 1
            B.facade_window(F, u0, u1, h0, h1, kind="public", cols=cols, rows=rows, inset=0.15)
    B.entrance("main", (0.0, HY0, 0.0), 0.0, width=2.4, height=2.55, kind="street", leaves=2)
    # canopy over the street door
    box(B.m, "CT_Steel", -1.9, 1.9, HY0 - 1.5, HY0, 2.85, 3.0)
    for sx in (-1.8, 1.8):
        box(B.detail, "CT_Steel", sx - 0.05, sx + 0.05, HY0 - 1.4, HY0 - 1.3, 0.0, 2.85)
    # terrace roof with the voids for the core and the open stair
    xs = [HX0, STAIR_X0 - 0.2, CX - 4.0, CX + 4.0, HX1]
    ys = [HY0, 5.4, CY - 4.0, 16.6, CY + 4.0, HY1]
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            cell = (xs[i], xs[i + 1], ys[j], ys[j + 1])
            if i == 2 and ys[j] >= CY - 4.0 - 1e-6 and ys[j + 1] <= CY + 4.0 + 1e-6:
                continue                                            # inside the core square
            if i == 1 and ys[j] >= 5.4 - 1e-6 and ys[j + 1] <= 16.6 + 1e-6:
                continue                                            # the stair void (x -6..-4)
            poly = [(cell[0], cell[2]), (cell[1], cell[2]), (cell[1], cell[3]), (cell[0], cell[3])]
            _slab(B.m, poly, HALL_H, HALL_H + 0.3, "CT_Roof", bottom="CT_Interior")
    a = 4.0 * math.tan(math.radians(22.5))
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        tri = [(CX + sx * 4.0, CY + sy * 4.0), (CX + sx * 4.0, CY + sy * a), (CX + sx * a, CY + sy * 4.0)]
        if sx * sy < 0:
            tri = tri[::-1]
        B.m.face("CT_Roof", [(x, y, HALL_H + 0.3) for x, y in tri])
        B.m.face("CT_Interior", [(x, y, HALL_H) for x, y in tri[::-1]])
        prism(B.m, "CT_Concrete", tri, HALL_H, HALL_H + 0.3, top=False, bottom=False)
    # low parapet round the terrace
    t0 = HALL_H + 0.3
    for (p0, p1) in (((HX0, HY0), (HX1, HY0)), ((HX1, HY0), (HX1, HY1)), ((HX1, HY1), (HX0, HY1)), ((HX0, HY1), (HX0, HY0))):
        B.railing([p0, p1], t0, h=0.5, post=2.0, mat="CT_Steel")
    # roof garden beds on the terrace
    for (bx0, bx1, by0, by1) in ((-11.8, -7.0, 4.0, 6.0), (6.0, 11.8, 4.0, 6.0), (7.0, 11.8, 24.0, 26.0), (-11.8, -7.0, 24.0, 26.0)):
        box(B.m, "CT_Concrete", bx0, bx1, by0, by1, t0, t0 + 0.4)
        box(B.m, "CT_Grass", bx0 + 0.08, bx1 - 0.08, by0 + 0.08, by1 - 0.08, t0 + 0.4, t0 + 0.44, faces="Z")
        for k in range(3):
            fu.blob(B.detail, "CT_Grass", bx0 + (bx1 - bx0) * (k + 0.5) / 3, (by0 + by1) / 2, t0 + 0.9, 0.45, 7)
    # ---- the open stair to level 1 (west side of the core)
    S = LEVELS[1]
    n = int(math.ceil(S / 0.165))
    r = S / n
    ly0, ly1 = CY - 0.8, CY + 0.8                      # landing y
    y_top = ly0
    for i in range(n):
        ya = y_top - GOING * (n - i)
        box(B.m, "CT_Concrete", STAIR_X0, STAIR_X1, ya, ya + GOING, r * (i + 1) - 0.3, r * (i + 1))
    box(B.m, "CT_Concrete", STAIR_X0, CX - 4.0, ly0, ly1, S - 0.3, S)                  # landing
    for yy in (ly0 + 0.2, ly1 - 0.2):
        B.column_round(STAIR_X0 + 0.15, yy, 0.0, S - 0.3, 0.12, mat="CT_Steel", segs=8)
    y_bot = y_top - GOING * n
    # rails: posts, hand rail on the outer side (x = STAIR_X0 + 0.05)
    xr = STAIR_X0 + 0.05
    for i in range(0, n, 3):
        ya = y_top - GOING * (n - i) + GOING / 2
        fu.rod(B.detail, "CT_Steel", (xr, ya, r * (i + 1)), (xr, ya, r * (i + 1) + 1.0), 0.05)
    fu.rod(B.detail, "CT_Steel", (xr, y_bot + GOING / 2, r + 1.0), (xr, y_top - GOING / 2, S + 1.0), 0.05)
    fu.rod(B.detail, "CT_Steel", (xr, y_bot + GOING / 2, r + 0.5), (xr, y_top - GOING / 2, S + 0.5), 0.04)
    B.railing([(xr, ly0), (xr, ly1), (CX - 4.0, ly1)], S, h=1.0, post=1.0)
    B.spot("stair_foot", "stand", (STAIR_X0 + 0.8, y_bot - 0.6, 0.0), 180.0)
    # ---- furniture
    _furnish(B, rnd)
    # lamps in the hall
    for (lx, ly) in ((-9.0, 8.0), (9.0, 8.0), (-9.0, 22.0), (9.0, 22.0), (0.0, 6.0), (-9.0, 15.0), (9.0, 15.0)):
        B.lamp((lx, ly, HALL_H - 0.3), lumens=720, colour="#FFD8A0", kind="public", radius=8.5)
    return y_bot


def _table(B, x, y, seats, rnd):
    fu.lbox(B.m, "CT_WoodLight", fu.Fr(x, y, 0.0), -1.2, 1.2, -0.5, 0.5, 0.70, 0.75)
    for sx in (-1.1, 1.1):
        for sy in (-0.4, 0.4):
            fu.lbox(B.detail, "CT_Steel", fu.Fr(x + sx, y + sy, 0.0), -0.03, 0.03, -0.03, 0.03, 0.0, 0.70, faces="xXyY")
    # a table lamp and papers
    fu.cyl(B.detail, "CT_Steel", x, y, 0.75, 0.78, 0.1, 8)
    fu.cyl(B.detail, "CT_Opal", x, y, 0.95, 1.15, 0.13, 8)
    fu.rod(B.detail, "CT_Steel", (x, y, 0.78), (x, y, 0.95), 0.025)
    for k, (sx, sy, yaw) in enumerate(((-0.6, -0.82, 180.0), (0.6, -0.82, 180.0), (-0.6, 0.82, 0.0), (0.6, 0.82, 0.0))[:seats]):
        fu.chair(B, x + sx, y + sy, yaw, seat_mat=rnd.choice(("CT_PlasterBlue", "CT_SteelRed", "CT_PlasterGreen", "CT_PlasterYellow")))
        B.spot("hall_seat_%d_%d_%d" % (int(x), int(y), k), "seat", (x + sx, y + sy, 0.0), yaw)
    B.spot("hall_table_%d_%d" % (int(x), int(y)), "table", (x, y, 0.0), 0.0)
    for sx in (-0.5, 0.5):
        fu.lbox(B.detail, rnd.choice(BOOKS), fu.Fr(x + sx, y, rnd.choice((0.0, 20.0, -15.0))), -0.15, 0.15, -0.12, 0.12, 0.75, 0.79)


def _furnish(B, rnd):
    det = B.detail
    # lending counter (front left), librarian behind it, patrons in front
    cf = fu.Fr(-9.5, 8.0, 0.0)
    fu.lbox(B.m, "CT_WoodLight", cf, -2.8, 2.8, 0.0, 0.8, 0.0, 1.05)
    fu.lbox(det, "CT_Plaster", cf, -2.85, 2.85, -0.05, 0.85, 1.05, 1.09)
    fu.lbox(B.m, "CT_WoodLight", fu.Fr(-12.0, 8.0, 0.0), -0.4, 0.4, 0.8, 3.0, 0.0, 1.05)             # return of the counter
    fu.lbox(det, "CT_Plaster", fu.Fr(-12.0, 8.0, 0.0), -0.45, 0.45, 0.8, 3.05, 1.05, 1.09)
    for k, sx in enumerate((-1.8, 1.2)):
        fu.lbox(det, "CT_Frame", cf, sx, sx + 0.5, 0.3, 0.6, 1.09, 1.12)                           # till / scanner
        fu.lbox(det, "CT_Frame", cf, sx + 0.1, sx + 0.4, 0.55, 0.58, 1.12, 1.4)
    for k in range(5):
        fu.lbox(det, rnd.choice(BOOKS), cf, -0.8 + 0.3 * k, -0.6 + 0.3 * k, 0.15, 0.45, 1.09, 1.09 + rnd.uniform(0.12, 0.26))
    # shelf behind the counter: returns
    _stack(B, fu.Fr(-12.6, 11.4, 90.0), 0.0, 4.0, rnd, depth=0.4, tiers=4, zmax=2.0)
    B.spot("lending_counter", "counter", (-9.5, 6.9, 0.0), 180.0)
    B.spot("lending_counter_2", "counter", (-7.5, 6.9, 0.0), 180.0)
    B.spot("librarian", "work", (-9.5, 9.5, 0.0), 0.0)
    fu.chair(B, -9.5, 9.5, 0.0, seat_mat="CT_SteelRed", back=True)
    B.spot("librarian_2", "work", (-7.4, 9.6, 0.0), 0.0)
    # card catalogue: three cabinets
    for k in range(3):
        fx = fu.Fr(3.2 + 1.35 * k, 6.3, 0.0)
        fu.lbox(B.m, "CT_Wood", fx, -0.6, 0.6, 0.0, 0.5, 0.0, 1.2)
        for rr in range(4):
            for cc in range(3):
                fu.lbox(det, "CT_WoodLight", fx, -0.55 + cc * 0.37, -0.22 + cc * 0.37, -0.02, 0.0, 0.1 + rr * 0.28, 0.34 + rr * 0.28)
                fu.lbox(det, "CT_Steel", fx, -0.41 + cc * 0.37, -0.33 + cc * 0.37, -0.04, -0.02, 0.2 + rr * 0.28, 0.24 + rr * 0.28)
    B.spot("catalogue", "stand", (5.2, 5.2, 0.0), 0.0)
    B.spot("catalogue_2", "stand", (4.0, 5.2, 0.0), 0.0)
    # two double-sided stacks east of the core
    for sx in (7.2, 10.4):
        for (u0, u1) in ((7.0, 13.0), (14.0, 19.0)):
            _stack(B, fu.Fr(sx, u0, 90.0), 0.0, u1 - u0, rnd, depth=0.4, tiers=4, zmax=2.1)
            _stack(B, fu.Fr(sx, u1, 270.0), 0.0, u1 - u0, rnd, depth=0.4, tiers=4, zmax=2.1)
        B.spot("stack_%d_a" % int(sx), "stand", (sx - 0.9, 10.0, 0.0), 90.0)
        B.spot("stack_%d_b" % int(sx), "stand", (sx + 0.9, 16.5, 0.0), 270.0)
    # book walls on the core's hall faces (north, north-east, north-west, south-east, south-west)
    for e in CORE["edges"]:
        if e["yaw"] in (0.0, 270.0, 180.0):
            continue
        fr = fu.Fr(e["mid"][0], e["mid"][1], e["yaw"], 0.0)
        _stack(B, fr, -1.35, 1.35, rnd, depth=0.4, tiers=4, zmax=2.1)
        p = fr.p(0.0, 0.9)
        B.spot("core_wall_%d" % int(e["yaw"]), "stand", (p[0], p[1], 0.0), (e["yaw"] + 180) % 360)
    # reading tables
    for k, (tx, ty) in enumerate(((-9.5, 20.5), (-9.5, 24.0), (-2.4, 23.5), (3.2, 23.5), (9.6, 24.0))):
        _table(B, tx, ty, 4, rnd)
    # window tables on the street front
    for k, tx in enumerate((-4.2, 7.8)):
        ty = 4.4
        fu.table_round(B, tx, ty, r=0.45, h=0.74)
        for sx, yaw in ((-0.75, 90.0), (0.75, 270.0)):
            fu.chair(B, tx + sx, ty, yaw, seat_mat="CT_PlasterYellow")
            B.spot("window_seat_%d_%d" % (k, int(sx > 0)), "seat", (tx + sx, ty, 0.0), yaw)
    # plants
    for (px, py) in ((-2.2, 4.2), (2.2, 4.2), (12.0, 4.4), (-12.2, 25.8), (12.2, 25.8), (-6.4, 4.2)):
        fu.plant(B, px, py, 0.0, 1.0, rnd.choice(("CT_PlasterRed", "CT_PlasterBlue", "CT_Concrete")))
    fu.doormat(B, fu.Fr(0.0, HY0, 0.0), 0.0, -0.9, size=(1.8, 0.8))


# ---------------------------------------------------------------- build
def build(B):
    rnd = random.Random(SEED)
    CORE["poly"] = _poly(CORE)
    CORE["edges"] = _edges(CORE["poly"])
    edges = CORE["edges"]
    # decide the pods
    cap = {1: [], 2: [], 3: []}
    polys = {1: [], 2: [], 3: []}
    stair_rect = _rect(STAIR_X0 - 0.4, STAIR_X1 + 0.3, 5.0, CY + 1.3)
    order = list(range(8))
    w_door = [i for i, e in enumerate(edges) if abs(((e["yaw"] - 270 + 180) % 360) - 180) < 1.0][0]
    for lv in (1, 2, 3):
        rnd.shuffle(order)
        for ei in order:
            if len(cap[lv]) >= 6:
                break
            if lv == 1 and ei == w_door:
                continue
            e = edges[ei]
            fr = fu.Fr(e["mid"][0], e["mid"][1], e["yaw"], 0.0)
            poly = [fr.p(-POD_W / 2, 0.0)[:2], fr.p(-POD_W / 2, POD_D)[:2], fr.p(POD_W / 2, POD_D)[:2], fr.p(POD_W / 2, 0.0)[:2]]
            ok = all(HX0 + 0.8 < p[0] < HX1 - 0.8 and 1.0 < p[1] < 29.0 for p in poly)
            if ok and any(_sat(poly, q, 0.04) for q in polys[lv]):
                ok = False
            if ok and lv == 1 and _sat(poly, stair_rect, 0.1):
                ok = False
            if ok:
                cap[lv].append(ei)
                polys[lv].append(poly)
    _core(B, cap, None)
    n_pods = 0
    lamp_budget = 9
    skins = ["CT_SteelWhite", "CT_Aluminium", "CT_SteelWhite", "CT_Plaster"]
    accents = ["CT_PlasterRed", "CT_PlasterYellow", "CT_SteelRed", "CT_Tile", "CT_PlasterGreen", "CT_Tile"]
    for lv in (1, 2, 3):
        for k, ei in enumerate(cap[lv]):
            lamp_on = lamp_budget > 0 and k % 2 == 0
            if lamp_on:
                lamp_budget -= 1
            _pod(B, rnd, edges[ei], lv, k + 1, rnd.choice(skins), rnd.choice(accents), "round" if (k + lv) % 2 == 0 else "rect",
                 lamp_on)
            n_pods += 1
    _hall(B, rnd)
    B.footprint = [(HX0, HY0), (HX1, HY0), (HX1, HY1), (HX0, HY1)]
    B.height = 19.0
    B.note("%d reading pods on 3 levels round an octagonal core; glazed hall; open stair to level 1" % n_pods)
    add_surrounds(B, seed=5, bounds=(HX0, HX1, 0.3, 5.5), max_lamps=2)
