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
import interior_kit as kit
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
DIM_LIT = {(3, 2), (3, 6)}                  # level-3 sea rooms: a low reading lamp only
EXTRA_LIT = {(1, 2), (1, 4), (1, 6), (2, 2), (2, 4), (2, 6), (3, 2), (3, 6)}   # pods that would otherwise be pitch dark at night (QA, pass 2: the carrels too)
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
    for z, lm, rad in ((3.9, 420, 5.0), (7.9, 420, 5.0), (13.2, 520, 6.0)):      # the two upper flights share one lamp
        B.lamp((CX - 0.8, CY - 2.35, z), lumens=lm, colour="#FFE4B8", kind="stair", radius=rad)
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
    # furniture and things (the mood of the level, the subject of the pod)
    P_ = _PodRoom(B, fr, e["yaw"], zf, lv, k)
    seat = P_.furnish()
    B.spot("pod_%d_%02d" % (lv, k), "seat", (seat[0], seat[1], zf), seat[3])
    B.thing("pod_%d_%02d" % (lv, k), "seat", (seat[0], seat[1], seat[2]), seat[3], height=round(seat[2] - zf, 2))
    if lamp_on:
        B.lamp(fr.p(0.0, D / 2 + 0.2)[:2] + (zf + POD_IN - 0.25,), lumens=(260 if (lv, k) in DIM_LIT else 300), colour="#FFE2B0", kind="public", radius=4.0)
    return P


# ---------------------------------------------------------------- pod furniture: one mood per level
# level 1: warm hobby nooks (rugs, armchairs, red and yellow, wood); level 2: quiet study carrels (green lamps, dark wood,
# shelves to the ceiling); level 3: airy sea-light rooms (low seats, white and blue, plants, the sea on the walls).
POD_TOPIC = {(1, 1): "birds", (1, 2): "music", (1, 3): "garden", (1, 4): "cooking", (1, 5): "chess", (1, 6): "engineering",
             (2, 1): "medicine", (2, 2): "sun", (3, 1): "local", (3, 2): "travel"}
LEVEL_TOPIC = {1: "library", 2: "poetry", 3: "sea"}
WARM = ["CT_PlasterRed", "CT_PlasterYellow", "CT_Leather"]
QUIET = ["CT_PlasterGreen", "CT_PlasterBlue", "CT_Leather"]
AIRY = ["CT_PlasterBlue", "CT_SteelWhite", "CT_PlasterGreen"]
SEA_PICS = ["painting_sea_calm", "painting_sea_storm", "painting_sea_night", "painting_dunes", "painting_lighthouse"]


class _PodRoom:
    """Furnishes one pod in its own frame: u across (+u = left of the person looking out of the window), v from the
    core door (0) to the window wall (4.0); inner width +-1.35; the way in is a 1.3 m lane |u| < 0.65 from v = 0."""

    def __init__(self, B, fr, Y, zf, lv, k):
        self.B, self.fr, self.Y, self.zf, self.lv, self.k = B, fr, Y, zf, lv, k
        self.topic = POD_TOPIC.get((lv, k), LEVEL_TOPIC[lv])
        self.rnd = random.Random("pod%d%d" % (lv, k))

    def put(self, fn, u, v, dyaw=0.0, top=None, **kw):
        p = self.fr.p(u, v)
        return fn(self.B, p[0], p[1], self.zf if top is None else top, self.Y + dyaw, **kw)

    def lw(self, fn, v, **kw):                      # on the left wall (+u), facing into the pod
        return self.put(fn, 1.35, v, 270.0, **kw)

    def rw(self, fn, v, **kw):                      # on the right wall (-u)
        return self.put(fn, -1.35, v, 90.0, **kw)

    def bw(self, fn, u, **kw):                      # on the window wall, facing the core
        return self.put(fn, u, 4.0, 180.0, **kw)

    def seat_of(self, r, i=0):
        s = r["seats"][i]
        return (s[0], s[1], s[2], s[3])

    def desk(self, typewriter=True, mat="CT_PlasterGreen"):
        r = self.put(kit.office_desk, 0.0, 3.62, 0.0, w=1.4, d=0.7, typewriter_on=typewriter, papers=True, phone=False)
        self.put(kit.lamp_green, 0.5, 3.7, 180.0, top=self.zf + 0.75)
        c = self.put(kit.chair, 0.0, 2.85, 0.0, kind="wood", mat=mat)
        return self.seat_of(c)

    def furnish(self):
        s = {1: self.l1, 2: self.l2, 3: self.l3}[self.lv]()
        if self.k == 1:                                  # a clock per level, on the window wall beside the round window
            self.bw(kit.wall_clock, -1.02, zc=2.05)
            p = self.fr.p(-1.35, 0.6)                    # and a light switch inside the right wall by the door
            _switch(self.B, p[0], p[1], self.Y + 90.0, self.zf + 1.2, "light_switch_pod_%d" % self.lv, "room")
        return s

    # ------------------------------------------------------------ level 1
    def l1(self):
        t, rnd = self.topic, self.rnd
        self.put(kit.rug, 0.0, 2.2, 0.0, w=1.9, d=2.8, seed=self.k)
        self.put(kit.plant, 1.05, 0.7, 0.0, kind="fern", pot="CT_PlasterYellow", h=0.8, seed=self.k)
        if t == "birds":
            s = self.seat_of(self.put(kit.armchair, 0.0, 3.1, 0.0, solid=False, mat="CT_PlasterRed"))
            self.put(kit.floor_lamp, -0.95, 3.5, 0.0)
            self.rw(kit.poster, 2.4, name="poster_birds_0", w=0.7, zc=1.5)
            self.rw(kit.poster, 1.4, name="poster_birds_1", w=0.7, zc=1.5)
            self.lw(kit.poster, 2.2, name="chart_birds", w=0.8, zc=1.5)
            self.lw(kit.bookshelf, 3.0, w=0.9, h=1.2, levels=3, topic="library", seed=1)
            self.put(kit.table, 0.0, 1.5, 0.0, w=0.7, d=0.5, h=0.5)
            self.put(kit.books_stack, 0.0, 1.5, 0.0, top=self.zf + 0.5, n=3)
        elif t == "music":
            s = self.seat_of(self.put(kit.armchair, -0.3, 3.2, 0.0, solid=False, mat="CT_PlasterYellow"))
            pn = self.lw(kit.piano, 2.4)
            self.B.thing("pod_1_02_piano", "play", pn["pos"], pn["yaw"], what="piano")
            self.put(kit.stool, 0.35, 2.4, 90.0)
            self.rw(kit.poster, 2.0, name="sleeve_1", w=0.5, zc=1.5)
            self.rw(kit.poster, 2.7, name="sleeve_4", w=0.5, zc=1.5)
            self.rw(kit.poster, 3.4, name="sleeve_6", w=0.5, zc=1.5)
            r = self.put(kit.coffee_table, -0.9, 1.3, 90.0, w=0.8, d=0.5)
            rp = self.put(kit.record_player, -0.9, 1.3, 90.0, top=r["top"])
            self.B.thing("pod_1_02_player", "machine", (rp["pos"][0], rp["pos"][1], r["top"] + 0.1), 90.0, what="record_player")
        elif t == "garden":
            s = self.seat_of(self.put(kit.armchair, 0.0, 3.1, 0.0, solid=False, mat="CT_PlasterGreen"))
            for (u, v, kd, hh) in ((1.0, 3.62, "monstera", 0.7), (-1.0, 3.6, "palm", 0.9), (-1.05, 0.7, "ficus", 1.0), (1.05, 2.3, "ficus", 0.8)):
                self.put(kit.plant, u, v, 0.0, kind=kd, h=hh, pot=rnd.choice(("CT_PlasterRed", "CT_PlasterYellow")), seed=u > 0)
            self.rw(kit.bookshelf, 2.3, w=1.0, h=1.4, levels=4, topic="old", seed=2)
            self.rw(kit.poster, 3.4, name="poster_cons_2", w=0.6, zc=1.5)
        elif t == "cooking":
            s = self.seat_of(self.put(kit.chair, -0.3, 2.8, 0.0, kind="wood", mat="CT_PlasterRed"))
            r = self.put(kit.table, -0.3, 3.5, 0.0, w=1.0, d=0.7, h=0.75)
            self.put(kit.plates, -0.5, 3.5, 0.0, top=self.zf + 0.75, n=2)
            self.put(kit.teapot, 0.0, 3.55, 0.0, top=self.zf + 0.75)
            k_ = self.lw(kit.kitchen_unit, 2.2, modules="cupboard,stove,cupboard")
            self.rw(kit.menu_board, 2.4, name="menu_0", w=0.6, zc=1.6)
            self.rw(kit.bookshelf, 1.5, w=0.9, h=1.2, levels=3, topic="collected", seed=3)
        elif t == "chess":
            self.put(kit.chess_table, 0.0, 3.2, 0.0)
            q = self.fr.p(0.0, 3.2)
            self.B.thing("pod_1_05_chess", "play", (q[0], q[1], self.zf + 0.74), self.Y, what="chess")
            self.put(kit.chair, -0.7, 3.2, 90.0, kind="wood", mat="CT_PlasterYellow")
            c = self.put(kit.chair, 0.7, 3.2, 270.0, kind="wood", mat="CT_PlasterRed")
            s = self.seat_of(c)
            self.rw(kit.poster, 2.2, name="chess_board", w=0.7, zc=1.5)
            self.lw(kit.bookshelf, 2.0, w=1.0, h=1.4, levels=4, topic="paperback", seed=4)
        else:                       # engineering
            self.put(kit.drafting_table, 0.0, 3.4, 0.0)
            c = self.put(kit.stool, 0.0, 2.65, 0.0)
            s = self.seat_of(c)
            self.rw(kit.poster, 2.0, name="chart_periodic", w=0.55, zc=1.5)
            self.rw(kit.poster, 3.0, name="paper_drawing", w=0.55, zc=1.5)
            self.lw(kit.lockers, 2.2, n=2, mat="CT_PlasterBlue")
        return s

    # ------------------------------------------------------------ level 2
    def l2(self):
        t = self.topic
        self.put(kit.rug, 0.0, 2.1, 0.0, w=1.6, d=2.4, name="rug_%d" % (2 + self.k % 3), seed=self.k)
        s = self.desk(typewriter=t in ("poetry", "medicine"))
        self.lw(kit.bookshelf, 1.6, w=1.0, h=2.3, levels=5, topic="old", seed=self.k)
        self.lw(kit.bookshelf, 2.65, w=1.0, h=2.3, levels=5, topic="collected", seed=self.k + 9)
        if t == "poetry":
            self.rw(kit.picture, 1.6, name="painting_portrait", w=0.5, zc=1.7)
            self.rw(kit.poster, 2.6, name="notice_%d" % (self.k % 3), w=0.5, zc=1.6)
            self.put(kit.plant, -1.0, 2.3, 0.0, kind="fern", pot="CT_PlasterGreen", h=0.8)
        elif t == "medicine":
            self.rw(kit.poster, 1.9, name="chart_anatomy", w=0.9, zc=1.6)
            self.put(kit.skeleton, -1.0, 2.9, 90.0)
        else:                       # sun
            self.rw(kit.poster, 2.9, name="poster_sun_1", w=0.6, zc=1.6)
            ts = self.put(kit.telescope, -0.85, 1.7, 0.0)
            self.B.thing("pod_2_02_telescope", "machine", ts["pos"], ts["yaw"], what="telescope")
        return s

    # ------------------------------------------------------------ level 3
    def l3(self):
        t, k = self.topic, self.k
        self.put(kit.rug, 0.0, 2.3, 0.0, w=2.0, d=2.0, round=True, name="rug_round_%d" % (k % 2), seed=k)
        pic = SEA_PICS[k % len(SEA_PICS)]
        pic2 = SEA_PICS[(k + 2) % len(SEA_PICS)]
        if t == "local":
            self.rw(kit.wall_map, 2.4, w=1.3, h=0.9, zc=1.6, name="map_town")
        elif t == "travel":
            self.rw(kit.poster, 1.6, name="travel_0", w=0.55, zc=1.6)
            self.rw(kit.poster, 2.4, name="travel_2", w=0.55, zc=1.6)
            self.rw(kit.poster, 3.2, name="travel_4", w=0.55, zc=1.6)
            self.put(kit.globe, -0.95, 1.1, 0.0, floor=True)
        else:
            self.rw(kit.picture, 2.4, name=pic, w=0.9, zc=1.6)
        self.lw(kit.picture, 2.4, name=pic2, w=0.9, zc=1.6)
        if k % 3 == 0:
            s = self.seat_of(self.put(kit.bean_bag, 0.0, 3.1, 0.0, mat="CT_PlasterBlue"))
            self.put(kit.plant, 1.05, 3.6, 0.0, kind="palm", pot="CT_SteelWhite", h=0.9)
        elif k % 3 == 1:
            s = self.seat_of(self.put(kit.zabuton, 0.0, 2.6, 0.0, mat="CT_PlasterGreen"))
            self.put(kit.chabudai, 0.0, 3.4, 0.0)
            self.put(kit.table_lamp, 0.0, 3.4, 0.0, top=self.zf + 0.33, shade="CT_SteelWhite")
            self.put(kit.plant, -1.0, 3.6, 0.0, kind="monstera", pot="CT_SteelWhite", h=0.7)
        else:
            s = self.seat_of(self.put(kit.armchair, 0.0, 3.1, 0.0, solid=False, mat="CT_SteelWhite"))
            ts = self.put(kit.telescope, -0.95, 3.5, 0.0)
            self.B.thing("pod_3_%02d_telescope" % k, "machine", ts["pos"], ts["yaw"], what="telescope")
            self.put(kit.plant, 1.05, 3.6, 0.0, kind="ficus", pot="CT_PlasterBlue", h=1.0)
        self.lw(kit.bookshelf, 1.0, w=0.9, h=1.2, levels=3, topic="modern", seed=k)
        return s



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
    for (lx, ly, lm, rad) in ((-5.0, 7.5, 1250, 10.0), (5.0, 7.5, 1250, 10.0), (-9.0, 22.0, 720, 8.5), (9.0, 22.0, 720, 8.5),
                              (-9.0, 15.0, 720, 8.5), (9.0, 15.0, 720, 8.5)):      # the front hall's three lamps merged into two
        B.lamp((lx, ly, HALL_H - 0.3), lumens=lm, colour="#FFD8A0", kind="public", radius=rad)
    return y_bot


# ---------------------------------------------------------------- hall furniture
def _facing(yaw):
    a = math.radians(yaw)
    return math.sin(a), -math.cos(a)


def _board(B, x, y, yaw, w, zc, cells, h=1.75):
    """A free-standing double-sided board on two feet: cells[0] on the front (faces yaw), cells[1] on the back."""
    fx, fy = _facing(yaw)
    ux, uy = -fy, fx
    box_ = lambda hw, hd, z0, z1, mat, m=B.detail: fu.lbox(m, mat, fu.Fr(x, y, yaw), -hw, hw, -hd, hd, z0, z1)
    fu.lbox(B.detail, "CT_WoodLight", fu.Fr(x, y, yaw), -w / 2, w / 2, -0.025, 0.025, 0.25, 0.25 + h)
    for s_ in (-1, 1):
        fu.lbox(B.detail, "CT_Steel", fu.Fr(x, y, yaw), s_ * (w / 2 - 0.12) - 0.03, s_ * (w / 2 - 0.12) + 0.03, -0.2, 0.2, 0.0, 0.05)
        fu.lbox(B.detail, "CT_Steel", fu.Fr(x, y, yaw), s_ * (w / 2 - 0.12) - 0.02, s_ * (w / 2 - 0.12) + 0.02, -0.02, 0.02, 0.05, 0.3)
    fu.lbox(B.m, "CT_WoodLight", fu.Fr(x, y, yaw), -w / 2 + 0.05, w / 2 - 0.05, -0.02, 0.02, 0.4, 1.9)
    for i, nm in enumerate(cells):
        sgn = 1 if i == 0 else -1
        kit.poster(B, x + fx * 0.027 * sgn, y + fy * 0.027 * sgn, 0.0, (yaw + (0 if i == 0 else 180)) % 360, name=nm,
                   w=w * 0.8, zc=zc)


def _hall_partition(B, x0, x1, y0, y1, z1, gap=None, mat="CT_Plaster"):
    """A 12 cm interior wall box; gap = (u0, u1) opening along its long side, with a lintel over it."""
    if gap is None:
        box(B.m, mat, x0, x1, y0, y1, 0.0, z1)
        return
    if x1 - x0 > y1 - y0:                               # runs along x
        box(B.m, mat, x0, gap[0], y0, y1, 0.0, z1)
        box(B.m, mat, gap[1], x1, y0, y1, 0.0, z1)
        box(B.m, mat, gap[0], gap[1], y0, y1, 2.2, z1)
    else:
        box(B.m, mat, x0, x1, y0, gap[0], 0.0, z1)
        box(B.m, mat, x0, x1, gap[1], y1, 0.0, z1)
        box(B.m, mat, x0, x1, gap[0], gap[1], 2.2, z1)


def _furnish(B, rnd):
    R = random.Random(SEED + 11)
    K = kit
    thing = B.thing

    # ---- entrance: mat, plants, coats
    fu.doormat(B, fu.Fr(0.0, HY0, 0.0), 0.0, -0.9, size=(1.8, 0.8))
    for (px, py, kd, pot) in ((-2.0, 4.0, "ficus", "CT_PlasterBlue"), (2.0, 4.0, "ficus", "CT_PlasterRed"),
                              (11.6, 3.7, "ficus", "CT_Concrete"), (-12.2, 3.9, "monstera", "CT_PlasterBlue")):
        K.plant(B, px, py, 0.0, 0.0, kind=kd, pot=pot, h=1.2, seed=int(px))
    K.coat_rack(B, 4.6, HY0 + 0.38, 0.0, 180.0, w=2.4, seed=3)

    # ---- lending desk (the librarian behind it), returns and the children's shelves back to back
    d = K.lending_desk(B, -9.5, 8.4, 0.0, 0.0, w=5.4, front="CT_PlasterGreen", seed=2)
    K.books_stack(B, -11.5, 8.4, 1.0, 20.0, n=5, seed=1)
    K.books_stack(B, -10.2, 8.5, 1.0, -10.0, n=3, seed=2)
    K.telephone(B, -7.7, 8.5, 1.0, 190.0)
    K.lamp_green(B, -11.9, 8.6, 1.0, 180.0)
    K.cup(B, -8.3, 8.5, 1.0, 0.0)
    K.sign_plate(B, -9.5, 8.4, 0.0, 0.0, name="sign_library", w=1.4, zc=2.9, hang=True)
    for i in range(4):
        K.bookshelf(B, -11.7 + i * 1.0, 11.45, 0.0, 0.0, w=1.0, h=2.0, levels=4, topic="library", seed=20 + i)
    for i in range(3):
        K.bookshelf(B, -11.7 + i * 1.0, 11.47, 0.0, 180.0, w=1.0, h=1.2, levels=3, topic="kids", seed=30 + i)
    chair = K.chair(B, -8.0, 10.0, 0.0, 190.0, kind="tube", mat="CT_SteelRed")
    B.spot("lending_counter", "counter", (-9.5, 7.9, 0.0), 0.0 + 180.0)
    B.spot("lending_counter_2", "counter", (-7.5, 7.9, 0.0), 180.0)
    B.spot("librarian", "work", (-9.5, 9.3, 0.0), 0.0)
    B.spot("librarian_2", "work", (-7.4, 9.6, 0.0), 0.0)
    thing("lending_counter", "counter", (-9.5, 8.4, 1.0), 0.0, role="librarian", service="lending", shop="library_desk")
    thing("lending_counter_2", "counter", (-7.5, 8.4, 1.0), 0.0, role="librarian", service="lending", shop="library_desk")

    # ---- card catalogue and the house rules board
    for i in range(4):
        K.card_catalogue(B, 2.6 + 0.9 * i, 6.5, 0.0, 0.0, cols=4)
    K.books_stack(B, 2.6, 6.5, 1.27, 0.0, n=3)
    K.books_stack(B, 4.4, 6.5, 1.27, 30.0, n=2)
    K.lamp_green(B, 3.5, 6.5, 1.27, 0.0)
    B.spot("catalogue", "stand", (4.4, 5.85, 0.0), 180.0)
    B.spot("catalogue_2", "stand", (3.0, 5.85, 0.0), 180.0)
    thing("catalogue", "board", (4.4, 6.5, 1.2), 0.0, topic="catalogue")
    _board(B, 6.7, 5.4, 0.0, 1.2, 1.5, ("notice_0", "notice_1"))
    thing("catalogue_2", "board", (6.7, 5.4, 1.5), 0.0, topic="rules")

    # ---- window tables on the street front (west), the stair foot stays clear
    K.table_round(B, -8.6, 4.4, 0.0, 0.0, r=0.45, h=0.74)
    K.cup(B, -8.7, 4.4, 0.74, 0.0)
    K.newspaper(B, -8.5, 4.35, 0.74, 20.0)
    for s_, (sx, yaw) in enumerate(((-0.78, 90.0), (0.78, 270.0))):
        c = K.chair(B, -8.6 + sx, 4.4, 0.0, yaw, kind="wood", mat="CT_PlasterYellow")
        B.spot("window_seat_0_%d" % s_, "seat", (-8.6 + sx, 4.4, 0.0), yaw)
    thing("window_seat_0", "seat", (-9.38, 4.4, 0.45), 90.0, height=0.45)

    # ---- periodicals: the long table with papers, magazine racks on the east glass
    pt = K.reading_table(B, 9.6, 5.0, 0.0, 0.0, w=2.4, d=1.0, lamps=1)
    chs = K.around_table(B, 9.6, 5.0, 2.4, 1.0, 0.0, per_side=(2, 2, 0, 0), kind="wood", mat="CT_PlasterRed")
    for j, nx in enumerate((8.8, 9.6, 10.4)):
        K.newspaper(B, nx, 5.0 + (0.15 if j % 2 else -0.15), 0.75, 15.0 * j - 10, seed=j)
    K.cup(B, 10.7, 4.8, 0.75, 0.0)
    for j, c in enumerate(chs):
        s_ = c["seats"][0]
        B.spot("window_seat_1_%d" % j if j < 2 else "period_seat_%d" % j, "seat", (s_[0], s_[1], 0.0), s_[3])
    for j, ry in enumerate((4.4, 5.5, 6.6)):
        K.magazine_rack(B, 12.66, ry, 0.0, 270.0, w=0.9, rows=4, seed=j)
    thing("window_seat_1", "seat", (chs[0]["seats"][0][0], chs[0]["seats"][0][1], 0.45), 180.0, height=0.45)
    thing("periodicals_rack_a", "shelf", (12.45, 5.0, 1.0), 270.0, topic="local")
    thing("periodicals_rack_b", "shelf", (12.45, 6.1, 1.0), 270.0, topic="sun")
    thing("periodicals_paper", "take", (9.6, 5.0, 0.76), 0.0, item="daily_paper")

    # ---- lounge south-east of the core: sofa, armchairs, coffee table, new arrivals
    K.rug(B, 4.0, 9.2, 0.0, 90.0, w=2.6, d=3.6, name="rug_1")
    sf = K.sofa(B, 4.0, 10.25, 0.0, 0.0, seats=3, mat="CT_PlasterBlue", solid=False)
    a1 = K.armchair(B, 2.2, 9.2, 0.0, 90.0, mat="CT_PlasterRed", solid=False)
    a2 = K.armchair(B, 5.8, 9.2, 0.0, 270.0, mat="CT_PlasterYellow", solid=False)
    ct = K.coffee_table(B, 4.0, 9.1, 0.0, 0.0)
    K.books_stack(B, 3.7, 9.1, ct["top"], 10.0, n=4, seed=5)
    K.vase(B, 4.4, 9.1, ct["top"], 0.0)
    K.floor_lamp(B, 1.95, 10.15, 0.0, 0.0)
    _dominoes(B, 3.85, 8.95, ct["top"], 0.0)
    thing("lounge_dominoes", "play", (4.0, 9.1, ct["top"] + 0.02), 0.0, what="dominoes")
    thing("lounge_sofa", "seat", (4.0, 10.25, 0.45), 0.0, height=0.45)
    thing("lounge_armchair_1", "seat", (a1["seats"][0][0], a1["seats"][0][1], a1["seats"][0][2]), 90.0, height=0.45)
    thing("lounge_armchair_2", "seat", (a2["seats"][0][0], a2["seats"][0][1], a2["seats"][0][2]), 270.0, height=0.45)
    B.spot("lounge_sofa", "seat", (4.0, 10.25, 0.0), 0.0)
    B.spot("lounge_armchair_1", "seat", (2.2, 9.2, 0.0), 90.0)

    # ---- the double-sided stacks east of the core (topics by thing id: see tools/city/content/plots.json)
    runs = (("7", 7.2, (("a", 270.0, 7.0, 11.0, "birds", "library"), ("c", 90.0, 7.0, 11.0, "cooking", "collected"),
                         ("d", 270.0, 14.0, 17.0, "medicine", "encyclopaedia"), ("b", 90.0, 14.0, 17.0, "science", "encyclopaedia"))),
            ("10", 10.4, (("a", 270.0, 7.0, 11.0, "sea", "old"), ("c", 90.0, 7.0, 11.0, "music", "modern"),
                          ("d", 270.0, 14.0, 17.0, "engineering", "encyclopaedia"), ("b", 90.0, 14.0, 17.0, "travel", "paperback"))))
    for tag, sx, faces in runs:
        for (sid, yaw, y0, y1, topic, spine) in faces:
            n = int(round(y1 - y0))
            for i in range(n):
                K.bookshelf(B, sx + (0.012 if yaw == 90.0 else -0.012), y0 + 0.5 + i, 0.0, yaw, w=1.0, h=2.0, levels=4,
                            topic=spine, seed=int(sx * 10) + i + (7 if yaw == 90.0 else 0) + y0)
            fxp = sx + (0.17 if yaw == 90.0 else -0.17)
            thing("stack_%s_%s" % (tag, sid), "shelf", (fxp, (y0 + y1) / 2, 1.3), yaw, topic=topic)
            if sid in ("a", "b", "c", "d"):
                px = sx + (0.9 if yaw == 90.0 else -0.9)
                B.spot("stack_%s_%s" % (tag, sid), "stand", (px, (y0 + y1) / 2, 0.0), (yaw + 180.0) % 360.0)
    K.sign_plate(B, 8.8, 13.5, 0.0, 0.0, name="sign_books", w=1.2, zc=2.85, hang=True)

    # ---- book walls on the core faces, with a picture over each
    over = {45: ("map", "map_town"), 90: ("poster", "poster_sun_1"), 135: ("picture", "painting_portrait"),
            225: ("poster", "kids_1"), 315: ("picture", "painting_still_life")}
    topics = {45: ("local", "old"), 90: ("sun", "encyclopaedia"), 135: ("poetry", "collected"), 225: ("kids", "kids"),
              315: ("art", "modern")}
    for e in CORE["edges"]:
        yaw = int(round(e["yaw"]))
        if yaw not in over:
            continue
        fr = fu.Fr(e["mid"][0], e["mid"][1], e["yaw"], 0.0)
        for j, u in enumerate((-0.62, 0.62)):
            p = fr.p(u, 0.0)
            K.bookshelf(B, p[0], p[1], 0.0, e["yaw"], w=1.2, h=2.3, levels=5, topic=topics[yaw][1], seed=yaw + j)
        p = fr.p(0.0, 0.0)
        kind, nm = over[yaw]
        if kind == "map":
            K.wall_map(B, p[0], p[1], 0.0, e["yaw"], w=1.0, h=0.75, zc=3.05, name=nm)
        elif kind == "poster":
            K.poster(B, p[0], p[1], 0.0, e["yaw"], name=nm, w=0.62, zc=3.05)
        else:
            K.picture(B, p[0], p[1], 0.0, e["yaw"], name=nm, w=0.7, zc=3.05)
        q = fr.p(0.0, 0.9)
        B.spot("core_wall_%d" % yaw, "stand", (q[0], q[1], 0.0), (e["yaw"] + 180) % 360)
        m = fr.p(0.0, 0.16)
        thing("core_wall_%d" % yaw, "shelf", (m[0], m[1], 1.3), e["yaw"], topic=topics[yaw][0])

    # ---- reading tables in the north (ids and places as before)
    for (tx, ty) in ((-9.5, 20.5), (-9.5, 24.0), (-2.4, 23.5), (3.2, 23.5), (7.4, 23.0)):
        tid = (int(tx), int(ty)) if (tx, ty) != (7.4, 23.0) else (9, 24)
        K.reading_table(B, tx, ty, 0.0, 0.0, w=2.4, d=1.0, lamps=1)
        chs = K.around_table(B, tx, ty, 2.4, 1.0, 0.0, per_side=(2, 2, 0, 0), kind="wood",
                             mat=R.choice(("CT_PlasterBlue", "CT_PlasterGreen", "CT_PlasterYellow", "CT_SteelRed")))
        K.books_stack(B, tx - 0.7, ty, 0.75, R.uniform(-30, 30), n=R.randint(2, 4), seed=tid[0])
        _table_clutter(B, [(-9.5, 20.5), (-9.5, 24.0), (-2.4, 23.5), (3.2, 23.5), (7.4, 23.0)].index((tx, ty)), tx, ty)
        for j, c in enumerate(chs):
            s_ = c["seats"][0]
            B.spot("hall_seat_%d_%d_%d" % (tid[0], tid[1], j), "seat", (s_[0], s_[1], 0.0), s_[3])
        B.spot("hall_table_%d_%d" % tid, "table", (tx, ty - 0.95, 0.0), 0.0)
        K.pendant(B, tx - 0.6, ty, HALL_H, drop=0.9, kind="green")
        K.pendant(B, tx + 0.6, ty, HALL_H, drop=0.9, kind="green")
        for j in (0, 3):
            s_ = chs[j]["seats"][0]
            thing("hall_seat_%d_%d_%d" % (tid[0], tid[1], j), "seat", (s_[0], s_[1], s_[2]), s_[3], height=0.45)
    K.sign_plate(B, -6.0, 22.3, 0.0, 0.0, name="sign_quiet", w=1.0, zc=2.95, hang=True)

    # ---- children's corner (west, behind the stair side): low shelves, rug, bean bags, a board of drawings
    for i in range(2):
        K.bookshelf(B, -12.62, 13.2 + 1.0 * i, 0.0, 90.0, w=1.0, h=1.2, levels=3, topic="kids", seed=40 + i)
    K.rug(B, -9.4, 15.8, 0.0, 0.0, w=3.2, d=3.2, round=True, name="rug_round_1")
    b1 = K.bean_bag(B, -10.6, 16.3, 0.0, 300.0, mat="CT_PlasterRed")
    b2 = K.bean_bag(B, -8.4, 14.9, 0.0, 130.0, mat="CT_PlasterYellow")
    b3 = K.bean_bag(B, -8.3, 17.0, 0.0, 200.0, mat="CT_PlasterGreen")
    kt = K.coffee_table(B, -9.5, 15.7, 0.0, 0.0, w=1.0, d=0.55)
    K.books_stack(B, -9.7, 15.7, kt["top"], 0.0, n=3, seed=8)
    K.globe(B, -9.2, 15.7, kt["top"], 0.0)
    st1 = K.stool(B, -9.3, 16.6, 0.0, 200.0, mat="CT_PlasterBlue")
    st2 = K.stool(B, -10.1, 15.0, 0.0, 20.0, mat="CT_PlasterRed")
    K.mats(B, -11.4, 12.9, 0.0, 0.0, w=1.4, d=0.9, n=2, mat="CT_PlasterBlue")
    K.plant(B, -12.1, 18.6, 0.0, 0.0, kind="palm", pot="CT_PlasterYellow", h=1.2)
    _board(B, -9.8, 18.7, 0.0, 1.0, 1.1, ("kids_0", "kids_3"), h=1.3)
    thing("kids_shelf_1", "shelf", (-12.45, 13.7, 0.7), 90.0, topic="kids")
    thing("kids_shelf_2", "shelf", (-12.45, 15.7, 0.7), 90.0, topic="kids")
    thing("kids_beanbag_1", "seat", (b1["seats"][0][0], b1["seats"][0][1], b1["seats"][0][2]), 64.0, height=0.3)
    thing("kids_beanbag_2", "seat", (b2["seats"][0][0], b2["seats"][0][1], b2["seats"][0][2]), 237.0, height=0.3)
    thing("kids_stool", "seat", (st1["seats"][0][0], st1["seats"][0][1], st1["seats"][0][2]), 348.0, height=0.45)
    B.spot("kids_corner", "stand", (-9.5, 14.2, 0.0), 180.0)

    # ---- listening corner (north-east of the core)
    K.rug(B, 6.0, 20.9, 0.0, 90.0, w=1.8, d=2.6, name="rug_4")
    lt = K.table_round(B, 6.0, 20.9, 0.0, 0.0, r=0.38, h=0.55)
    rp = K.record_player(B, 6.0, 20.9, 0.55, 0.0)
    l1 = K.armchair(B, 5.0, 20.9, 0.0, 90.0, mat="CT_PlasterBlue", solid=False)
    l2 = K.armchair(B, 7.0, 20.9, 0.0, 270.0, mat="CT_PlasterGreen", solid=False)
    K.floor_lamp(B, 5.0, 21.9, 0.0, 0.0)
    thing("listening_player", "machine", (6.0, 20.9, 0.7), 0.0, what="record_player")
    thing("listening_chair_1", "seat", (l1["seats"][0][0], l1["seats"][0][1], l1["seats"][0][2]), 90.0, height=0.45)
    thing("listening_chair_2", "seat", (l2["seats"][0][0], l2["seats"][0][1], l2["seats"][0][2]), 270.0, height=0.45)

    # ---- the librarian's office: a room in the north-east corner behind the service door
    _office(B, R)

    # ---- pass 2: the returns box, the shelf of returned books, notices, the story-hour chair, the librarian's tea
    _extras(B, R)

    # ---- plants on the north side
    for (px, py, kd, pot) in ((-12.2, 26.0, "ficus", "CT_PlasterRed"), (-5.6, 25.8, "palm", "CT_PlasterBlue"),
                              (0.0, 25.9, "monstera", "CT_Concrete"), (5.4, 25.9, "ficus", "CT_PlasterYellow")):
        K.plant(B, px, py, 0.0, 0.0, kind=kd, pot=pot, h=1.2, seed=int(py))
    K.globe(B, 0.3, 21.4, 0.0, 0.0, floor=True)


def _dominoes(B, x, y, z, yaw):
    """A short snake of dominoes laid out on a low table (white tiles with a dark centre line, 0.04 x 0.08)."""
    fr = fu.Fr(x, y, yaw)
    spots = [(-0.16, 0.0, 0), (-0.08, 0.0, 0), (0.0, 0.0, 0), (0.06, 0.05, 90), (0.1, 0.12, 0), (0.1, 0.2, 0), (0.05, 0.27, 90)]
    for u, w, rot in spots:
        f = fu.Fr(*fr.p(u, w)[:2], yaw + rot)
        fu.lbox(B.detail, "CT_SteelWhite", f, -0.02, 0.02, -0.04, 0.04, z, z + 0.01)
        fu.lbox(B.detail, "CT_Steel", f, -0.019, 0.019, -0.002, 0.002, z + 0.0095, z + 0.0105)


def _switch(B, x, y, yaw, z, tid, scope="room"):
    """A wall light switch: a small white plate with a rocker at height z on the wall at (x, y), facing yaw."""
    fr = fu.Fr(x, y, yaw)
    fu.lbox(B.detail, "CT_SteelWhite", fr, -0.04, 0.04, 0.0, 0.012, z - 0.06, z + 0.06)
    fu.lbox(B.detail, "CT_Steel", fr, -0.012, 0.012, 0.012, 0.02, z - 0.025, z + 0.03)
    B.thing(tid, "machine", fr.p(0.0, 0.03, z), yaw, what="switch", switch=scope)


def _trolley(B, x, y, yaw, seed=0):
    """A book trolley: two shelves of books on four casters and a push bar (0.8 x 0.4 x 0.95), detail only plus a core."""
    fr = fu.Fr(x, y, yaw)
    for z in (0.18, 0.58):
        fu.lbox(B.detail, "CT_WoodLight", fr, -0.4, 0.4, -0.2, 0.2, z, z + 0.04)
    for sx in (-0.38, 0.38):
        for sy in (-0.18, 0.18):
            fu.lbox(B.detail, "CT_Steel", fr, sx - 0.015, sx + 0.015, sy - 0.015, sy + 0.015, 0.0, 0.18)
            fu.lbox(B.detail, "CT_Steel", fr, sx - 0.012, sx + 0.012, sy - 0.012, sy + 0.012, 0.22, 0.6)
    fu.lbox(B.detail, "CT_Steel", fr, 0.36, 0.4, -0.2, 0.2, 0.9, 0.94)
    for sy in (-0.18, 0.18):                              # the push bar's two uprights (QA r8: the bar floated)
        fu.lbox(B.detail, "CT_Steel", fr, 0.365, 0.395, sy - 0.012, sy + 0.012, 0.6, 0.91)
    fu.lbox(B.m, "CT_WoodLight", fr, -0.38, 0.38, -0.18, 0.18, 0.3, 0.6)
    p = fr.p(-0.1, 0.0)
    kit.books_stack(B, p[0], p[1], 0.22, yaw + 100, n=4, seed=seed)
    p = fr.p(0.15, 0.0)
    kit.books_stack(B, p[0], p[1], 0.62, yaw + 80, n=3, seed=seed + 1)


def _table_clutter(B, i, tx, ty):
    """One thing of its own on each long reading table (the shared books/newspaper stay)."""
    K = kit
    z = 0.75
    if i == 0:                                           # an atlas lies open
        K.newspaper(B, tx + 0.45, ty + 0.12, z, 12.0, cellname="map_world")
    elif i == 1:                                         # a magnifier on a page
        K.newspaper(B, tx + 0.4, ty - 0.1, z, -15.0, cellname="news_0")
        fr = fu.Fr(tx + 0.4, ty - 0.1, -15.0)
        fu.cyl(B.detail, "CT_Steel", tx + 0.4, ty - 0.1, z + 0.004, z + 0.012, 0.06, segs=14, top=True)
        fu.cyl(B.detail, "CT_SteelWhite", tx + 0.4, ty - 0.1, z + 0.012, z + 0.016, 0.048, segs=14, top=True)
        fu.lbox(B.detail, "CT_WoodLight", fr, 0.05, 0.19, -0.012, 0.012, z + 0.004, z + 0.02)
    elif i == 2:                                         # a stack with a ribbon bookmark hanging out
        K.books_stack(B, tx + 0.5, ty - 0.05, z, 25.0, n=4, seed=77)
        fu.lbox(B.detail, "CT_PlasterRed", fu.Fr(tx + 0.5, ty - 0.05, 25.0), 0.12, 0.15, -0.14, -0.05, z + 0.06, z + 0.064)
    elif i == 3:                                         # a forgotten pair of glasses
        fr = fu.Fr(tx + 0.55, ty + 0.15, 20.0)
        for sx in (-0.045, 0.045):
            fu.cyl(B.detail, "CT_Steel", fr.p(sx, 0)[0], fr.p(sx, 0)[1], z, z + 0.006, 0.03, segs=10, top=True)
        fu.lbox(B.detail, "CT_Steel", fr, -0.015, 0.015, -0.004, 0.004, z, z + 0.006)
        for sx in (-0.07, 0.07):
            fu.lbox(B.detail, "CT_Steel", fr, sx - 0.003, sx + 0.003, 0.0, 0.1, z, z + 0.006)
    else:                                                # a teapot and a cup between the books
        K.teapot(B, tx + 0.45, ty + 0.12, z, 0.0)
        K.cup(B, tx + 0.7, ty - 0.1, z, 0.0)


def _extras(B, R):
    K = kit
    th = B.thing
    # third layer: topic pictures on the stack ends, trolleys, an open catalogue drawer, a step stool, a globe, a clock
    for (sx, ey, yw, nm) in ((7.2, 7.0, 0.0, "poster_birds_2"), (7.2, 11.0, 180.0, "poster_health"), (7.2, 14.0, 0.0, "poster_safety"),
                             (7.2, 17.0, 180.0, "chart_periodic"), (10.4, 7.0, 0.0, "painting_sea_calm"), (10.4, 11.0, 180.0, "sleeve_2"),
                             (10.4, 14.0, 0.0, "travel_1"), (10.4, 17.0, 180.0, "painting_abstract")):
        K.poster(B, sx, ey, 0.0, yw, name=nm, w=0.42, zc=1.7)
    _trolley(B, 8.8, 12.5, 0.0, seed=61)
    _trolley(B, -11.6, 9.7, 0.0, seed=63)
    fr = fu.Fr(5.3, 6.275, 0.0)                       # the fourth catalogue's second drawer pulled out
    fu.lbox(B.detail, "CT_WoodLight", fr, -0.17, 0.17, -0.3, 0.0, 0.88, 0.98)
    fu.lbox(B.detail, "CT_Paper", fr, -0.15, 0.15, -0.28, -0.02, 0.98, 1.0)
    fu.lbox(B.detail, "CT_Steel", fr, -0.04, 0.04, -0.31, -0.3, 0.92, 0.94)
    fr = fu.Fr(6.55, 13.0, 0.0)                       # a two-step stool at the end of a stack
    fu.lbox(B.detail, "CT_WoodLight", fr, -0.2, 0.2, -0.15, 0.15, 0.0, 0.2)
    fu.lbox(B.detail, "CT_WoodLight", fr, -0.2, 0.2, 0.0, 0.15, 0.2, 0.4)
    fu.lbox(B.m, "CT_WoodLight", fr, -0.18, 0.18, -0.13, 0.13, 0.05, 0.19)
    K.globe(B, 10.95, 5.3, 0.75, 0.0)
    e0 = [e for e in CORE["edges"] if int(round(e["yaw"])) == 0][0]
    cf = fu.Fr(e0["mid"][0], e0["mid"][1], e0["yaw"], 0.0).p(1.3, 0.0)
    K.wall_clock(B, cf[0], cf[1], 0.0, e0["yaw"], zc=2.7)
    # the door leaf of the librarian's office, open against the jamb
    fu.lbox(B.detail, "CT_Door", fu.Fr(10.12, 21.5, 0.0), -0.02, 0.02, -0.5, 0.5, 0.0, 2.1)
    fu.lbox(B.detail, "CT_Steel", fu.Fr(10.12, 21.5, 0.0), 0.02, 0.05, 0.38, 0.42, 0.95, 1.0)
    # ---- the "returned" shelf under the east glass: two low shelves, a reading lamp, an armchair, a typed card on the glass
    for yy in (10.65, 11.55):
        K.bookshelf(B, 12.62, yy, 0.0, 270.0, w=0.9, h=1.2, levels=3, topic="old", seed=int(yy * 10))
    K.books_stack(B, 12.5, 10.65, 1.2, 80.0, n=3, seed=41)
    K.table_lamp(B, 12.5, 11.55, 1.2, 270.0, shade="CT_PlasterGreen")
    K.poster(B, 12.69, 11.1, 0.0, 270.0, name="paper_typed", w=0.34, zc=1.75)
    ra = K.armchair(B, 11.75, 11.1, 0.0, 90.0, mat="CT_PlasterGreen", solid=False)
    th("returned_shelf", "shelf", (12.45, 11.1, 0.8), 270.0, topic="returned")
    th("returned_card", "board", (12.5, 12.35, 0.9), 270.0, topic="returned")
    K.stool(B, 12.2, 12.35, 0.0, 270.0, mat="CT_PlasterYellow")
    B.spot("returned_shelf", "stand", (11.7, 10.4, 0.0), 90.0)
    B.spot("returned_armchair", "seat", (ra["seats"][0][0], ra["seats"][0][1], 0.0), 90.0)
    # ---- the returns box by the street wall (steel, green, a slot and a typed card)
    fr = fu.Fr(-3.2, 3.72, 180.0)
    fu.lbox(B.detail, "CT_PlasterGreen", fr, -0.26, 0.26, -0.2, 0.2, 0.0, 1.05)
    fu.lbox(B.detail, "CT_Steel", fr, -0.3, 0.3, -0.24, 0.24, 1.05, 1.1)
    fu.lbox(B.detail, "CT_ConcreteDark", fr, -0.18, 0.18, 0.2, 0.215, 0.78, 0.84)
    fu.lbox(B.m, "CT_PlasterGreen", fr, -0.24, 0.24, -0.18, 0.18, 0.1, 1.0)
    K.poster(B, -3.2, 3.72 + 0.215, 0.0, 180.0, name="paper_letter", w=0.3, zc=0.93)
    th("returns_box", "board", (-3.2, 3.95, 0.9), 180.0, topic="returns")
    B.spot("returns_box", "stand", (-3.2, 4.6, 0.0), 0.0)
    # ---- by the entrance: the day's notices and the reading-diary stand (double-sided boards, kept apart)
    _board(B, -1.9, 6.4, 0.0, 1.2, 1.5, ("notice_2", "map_coast"))
    th("notice_board_day", "board", (-1.9, 6.4, 1.5), 0.0, topic="notices")
    B.spot("notice_board_day", "stand", (-1.9, 5.75, 0.0), 180.0)
    _board(B, -3.9, 6.4, 0.0, 0.95, 1.3, ("chart_calendar", "poster_cons_3"))
    th("diary_board", "board", (-3.9, 6.4, 1.5), 0.0, topic="diary")
    B.spot("diary_board", "stand", (-3.9, 5.75, 0.0), 180.0)
    # ---- a stamp and a lamp at the left end of the lending desk
    K.cup(B, -11.2, 8.45, 1.0, 0.0)
    th("library_stamp", "counter", (-11.5, 8.4, 1.0), 0.0, role="librarian", service="stamp")
    B.spot("lending_counter_3", "counter", (-11.5, 7.9, 0.0), 180.0)
    # ---- the children's story hour: the reading armchair at the edge of the round rug, a screen-less "slides" hour
    sa = K.armchair(B, -7.5, 15.9, 0.0, 270.0, mat="CT_PlasterRed", solid=False)
    K.floor_lamp(B, -7.0, 17.2, 0.0, 0.0)
    th("kids_story_chair", "seat", (sa["seats"][0][0], sa["seats"][0][1], sa["seats"][0][2]), 270.0, height=0.45)
    th("kids_story_hour", "screen", (-9.4, 15.9, 0.5), 90.0, what="story_hour")
    B.spot("story_armchair", "seat", (sa["seats"][0][0], sa["seats"][0][1], 0.0), 270.0)


def _office(B, R):
    K = kit
    x0, x1, y0, y1 = 9.6, 12.7, 21.0, 26.7
    _hall_partition(B, x0 - 0.12, x0, y0, y1, HALL_H)                                # west wall
    _hall_partition(B, x0, x1, y0 - 0.12, y0, HALL_H, gap=(10.1, 11.1))             # south wall with the door
    B.spot("office_desk", "work", (11.0, 25.1, 0.0), 0.0)   # the librarian sits at the far wall and looks into the room
    # door caption
    B.thing("office_door", "door", (10.6, y0, 1.2), 180.0, to="staff")
    K.rug(B, 11.2, 23.4, 0.0, 0.0, w=1.7, d=2.2, name="rug_5")
    d = K.office_desk(B, 11.0, 24.35, 0.0, 0.0, w=1.4, d=0.7, typewriter_on=True, papers=True, phone=True)
    K.chair(B, 11.0, 25.1, 0.0, 0.0, kind="tube", mat="CT_PlasterGreen")
    B.thing("office_chair", "seat", (11.0, 25.1, 0.45), 0.0, height=0.45)
    K.lamp_green(B, 10.55, 24.45, 0.75, 0.0)
    for i in range(2):
        K.bookshelf(B, x0 + 0.02, 22.0 + i * 1.0, 0.0, 90.0, w=1.0, h=2.2, levels=4, topic="old", seed=60 + i)
    B.thing("office_shelf", "shelf", (9.8, 22.5, 1.3), 90.0, topic="local")
    K.filing_cabinet(B, x0 + 0.02, 25.2, 0.0, 90.0, drawers=4, mat="CT_PlasterGreen")
    K.filing_cabinet(B, x0 + 0.02, 25.8, 0.0, 90.0, drawers=4, mat="CT_PlasterGreen")
    K.table(B, 12.3, 24.4, 0.0, 0.0, w=0.6, d=0.5, h=0.75)
    K.samovar(B, 12.3, 24.4, 0.75, 0.0)
    K.cup(B, 12.15, 24.55, 0.75, 0.0)
    K.plant(B, 12.35, 26.3, 0.0, 0.0, kind="ficus", pot="CT_PlasterRed", h=1.4)
    # pass 2: a clock over the door, a picture and a card board on the wall, a book trolley, a radio on the cabinets
    K.wall_clock(B, 10.6, y0, 0.0, 180.0, zc=2.75)
    K.picture(B, 12.1, y0, 0.0, 180.0, name="painting_pines", w=0.6, zc=1.6)
    K.poster(B, 11.55, y0, 0.0, 180.0, name="paper_drawing", w=0.34, zc=1.5)
    _trolley(B, 10.05, 24.15, 90.0, seed=51)
    _switch(B, 11.25, y0 + 0.0, 180.0, 1.2, "light_switch_office")
    K.radio(B, 9.85, 25.5, 1.32, 90.0)
    B.thing("office_radio", "machine", (9.9, 25.5, 1.45), 90.0, what="radio")
    B.thing("office_ledger", "board", (11.0, 24.35, 0.82), 0.0, topic="ledger")
    B.thing("office_kettle", "take", (12.3, 24.4, 0.9), 270.0, item="tea")
    B.spot("office_tea", "stand", (11.7, 24.4, 0.0), 90.0)
    K.pendant(B, 11.2, 23.5, HALL_H, drop=0.7, kind="cone")
    B.lamp((11.2, 23.5, HALL_H - 0.9), lumens=520, colour="#FFE2B0", kind="public", radius=5.0, fixture=False)



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
            lamp_on = lamp_on or (lv, k + 1) in EXTRA_LIT
            _pod(B, rnd, edges[ei], lv, k + 1, rnd.choice(skins), rnd.choice(accents), "round" if (k + lv) % 2 == 0 else "rect",
                 lamp_on)
            n_pods += 1
    _hall(B, rnd)
    B.footprint = [(HX0, HY0), (HX1, HY0), (HX1, HY1), (HX0, HY1)]
    B.height = 19.0
    B.note("%d reading pods on 3 levels round an octagonal core; glazed hall; open stair to level 1" % n_pods)
    print("KITCHECK", kit.check(B))
    add_surrounds(B, seed=5, bounds=(HX0, HX1, 0.3, 5.5), max_lamps=2)
