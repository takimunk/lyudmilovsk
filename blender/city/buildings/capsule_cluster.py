"""Capsule cluster: three short hollow concrete service cores (two square, one octagonal, 4-5 storeys) joined by open
steel bridges, with prefab living capsules plugged onto the cores' faces, tilted in plan in 45 degree steps. Every
capsule (or pair of capsules = one flat) is one flat with one big round or rounded-square window. An original low-rise
design in the spirit of Japanese Metabolism (Kurokawa, Kikutake); not a copy of any building. Unlike capsule_tower
(two cores, 13 storeys, one capsule type) this is a horizontal cluster, with enterable stair cores.

Plot 40 x 30 m, height_max 18 (roofs 13.2 / 16.5, hatches to 17.4).
Layout (x along the street, y inland): cores A (square, 5 storeys) x -11.8, B (octagon, 4 storeys) x 0,
C (square, 5 storeys) x +11.8, all at y = 14. Bridges A-B and B-C at levels 1 and 3.
Each core is a hollow shell (facade walls with reveals, solid slabs, a U-shaped concrete stair with 0.16 risers)
with a ground door on the street face (entrances A, B, C), stair windows on the faces that carry no capsule, and
bridge doors. Capsule doors are drawn on the cores' stair landings only as flat records (the capsules are solid pods).
"""
import math
import random

import furn_int3 as fu
from citykit import box, prism, DOOR_W, DOOR_H
from _surrounds import add_surrounds

INFO = {
    "name_ru": "Капсульный кластер", "name_en": "Capsule Cluster",
    "style": "metabolism", "use": "residential",
    "inspired_by": "Japanese Metabolism of the 1960s-70s (Kurokawa, Kikutake): prefab capsules plugged onto concrete "
                   "service cores linked by bridges, here as a low-rise horizontal cluster",
}

SEED = 1970
STOREY = 3.3
WALL = 0.3
CAP_W, CAP_D, CAP_H = 2.3, 3.3, 2.45
WIN_R = 0.55
SKINS = ("CT_Aluminium", "CT_Aluminium", "CT_Aluminium", "CT_SteelWhite", "CT_SteelWhite", "CT_SteelWhite",
         "CT_PlasterYellow", "CT_PlasterYellow", "CT_SteelRed", "CT_SteelRed")
VARIANTS = {}

CY = 14.0
CORES = [
    dict(id="A", kind="sq", c=(-11.8, CY), a=2.7, n=5),
    dict(id="B", kind="oct", c=(0.0, CY), a=3.3, n=4),
    dict(id="C", kind="sq", c=(11.8, CY), a=2.8, n=5),
]
BRIDGE_LEVELS = (1, 3)


# ------------------------------------------------------------------ geometry helpers
def _poly(core):
    cx, cy = core["c"]
    a = core["a"]
    if core["kind"] == "sq":
        return [(cx - a, cy - a), (cx + a, cy - a), (cx + a, cy + a), (cx - a, cy + a)]
    R = a / math.cos(math.radians(22.5))
    return [(cx + R * math.cos(math.radians(-22.5 + 45 * k)), cy + R * math.sin(math.radians(-22.5 + 45 * k)))
            for k in range(8)]


def _edges(poly):
    out = []
    n = len(poly)
    for i in range(n):
        p0, p1 = poly[i], poly[(i + 1) % n]
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        L = math.hypot(dx, dy)
        yaw = math.degrees(math.atan2(dy, dx)) % 360.0
        out.append(dict(p0=p0, p1=p1, L=L, yaw=round(yaw, 3) % 360.0, mid=((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2)))
    return out


def _clip(poly, x0, x1, y0, y1):
    """Sutherland-Hodgman clip of a convex polygon to a rectangle."""
    def clip(pts, inside, inter):
        out = []
        for i in range(len(pts)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            ia, ib = inside(a), inside(b)
            if ia:
                out.append(a)
            if ia != ib:
                out.append(inter(a, b))
        return out
    pts = list(poly)
    for ins, inter in (
        (lambda p: p[0] >= x0, lambda a, b: (x0, a[1] + (b[1] - a[1]) * (x0 - a[0]) / (b[0] - a[0]))),
        (lambda p: p[0] <= x1, lambda a, b: (x1, a[1] + (b[1] - a[1]) * (x1 - a[0]) / (b[0] - a[0]))),
        (lambda p: p[1] >= y0, lambda a, b: (a[0] + (b[0] - a[0]) * (y0 - a[1]) / (b[1] - a[1]), y0)),
        (lambda p: p[1] <= y1, lambda a, b: (a[0] + (b[0] - a[0]) * (y1 - a[1]) / (b[1] - a[1]), y1)),
    ):
        if not pts:
            break
        pts = clip(pts, ins, inter)
    if len(pts) < 3:
        return []
    area = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
    return pts if abs(area) > 0.02 else []


def _sat(p, q, margin=0.0):
    """Do the convex polygons p and q overlap (closer than margin)?"""
    for poly in (p, q):
        n = len(poly)
        for i in range(n):
            ex, ey = poly[(i + 1) % n][0] - poly[i][0], poly[(i + 1) % n][1] - poly[i][1]
            L = math.hypot(ex, ey) or 1.0
            nx, ny = -ey / L, ex / L
            a = [x * nx + y * ny for x, y in p]
            b = [x * nx + y * ny for x, y in q]
            if max(a) + margin <= min(b) or max(b) + margin <= min(a):
                return False
    return True


def _rect(x0, x1, y0, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def _quad_out(m, mat, pts, out):
    """A quad that faces `out` whatever the order of pts."""
    p0, p1, p2 = pts[0], pts[1], pts[2]
    u = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
    v = (p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2])
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    if n[0] * out[0] + n[1] * out[1] + n[2] * out[2] < 0:
        pts = pts[::-1]
    m.face(mat, list(pts))


# ------------------------------------------------------------------ cores
def _core_shell(B, core, holes_by_edge, wins_by_edge):
    poly = _poly(core)
    edges = core["edges"]
    top = core["n"] * STOREY
    for i, e in enumerate(edges):
        F = B.facade(e["p0"], e["p1"], 0.0, top - WALL, "CT_Concrete", holes=holes_by_edge.get(i, []), depth=WALL,
                     reveal_mat="CT_ConcreteDark")
        for (u0, u1, h0, h1) in wins_by_edge.get(i, []):
            B.facade_window(F, u0, u1, h0, h1, kind="stair", inset=0.18)
    cx, cy = core["c"]
    k = (core["a"] - WALL) / core["a"]
    inner = [(cx + (x - cx) * k, cy + (y - cy) * k) for x, y in poly]
    core["inner"] = inner
    # ground floor
    prism(B.m, "CT_Floor", inner, -0.3, 0.0, top=True, bottom=False)
    # upper floors with the stair well cut out
    for lv in range(1, core["n"]):
        zf = lv * STOREY
        for (x0, x1, y0, y1) in ((cx + 0.1, cx + 9, cy - 9, cy + 9), (cx - 9, cx - 2.1, cy - 9, cy + 9),
                                 (cx - 2.1, cx + 0.1, cy - 9, cy - 1.5), (cx - 2.1, cx + 0.1, cy + 2.1, cy + 9)):
            piece = _clip(inner, x0, x1, y0, y1)
            if piece:
                prism(B.m, "CT_Concrete", piece, zf - WALL, zf, top=True, bottom=True)
    # roof: concrete slab, membrane on top, a hatch and a vent
    prism(B.m, "CT_Roof", poly, top - WALL, top, top=True, bottom=False, side_mat="CT_Concrete")
    B.m.face("CT_ConcreteDark", [(x, y, top - WALL) for x, y in reversed(poly)])
    # parapet ring (a low concrete kerb in the detail mesh)
    ro = [(cx + (x - cx) * 0.93, cy + (y - cy) * 0.93) for x, y in poly]
    ri = [(cx + (x - cx) * 0.88, cy + (y - cy) * 0.88) for x, y in poly]
    for i in range(len(ro)):
        a0, a1 = ro[i], ro[(i + 1) % len(ro)]
        b0, b1 = ri[i], ri[(i + 1) % len(ri)]
        B.detail.face("CT_ConcreteDark", [(a0[0], a0[1], top + 0.05), (a1[0], a1[1], top + 0.05), (b1[0], b1[1], top + 0.05),
                                           (b0[0], b0[1], top + 0.05)][::-1])
    box(B.m, "CT_ConcreteDark", cx - 1.3, cx + 1.3, cy + 0.2, cy + 2.0, top, top + 0.9)           # stair head house
    box(B.detail, "CT_Steel", cx + 1.4, cx + 1.8, cy - 1.8, cy - 1.4, top, top + 0.7)
    fu.cyl(B.detail, "CT_Steel", cx + 1.6, cy - 1.6, top + 0.7, top + 1.0, 0.2, 8)
    return inner


def _core_stairs(B, core):
    cx, cy = core["c"]
    for s in range(0, core["n"] - 1):
        z = s * STOREY
        r = STOREY / 20.0
        for i in range(10):                                      # flight 1: x -2.1..-1.1 climbing +y
            ya = cy - 1.5 + 0.3 * i
            box(B.m, "CT_Concrete", cx - 2.1, cx - 1.1, ya, ya + 0.3, z + r * (i + 1) - 0.3, z + r * (i + 1))
        box(B.m, "CT_Concrete", cx - 2.1, cx + 0.1, cy + 1.5, cy + 2.1, z + 10 * r - 0.3, z + 10 * r)   # mid landing
        for j in range(10):                                      # flight 2: x -0.9..0.1 climbing -y
            yb = cy + 1.5 - 0.3 * j
            box(B.m, "CT_Concrete", cx - 0.9, cx + 0.1, yb - 0.3, yb, z + (10 + j + 1) * r - 0.3, z + (10 + j + 1) * r)
        # rail along the well
        B.railing([(cx - 1.05, cy + 1.45), (cx - 1.05, cy - 1.45)], z + 1.6, h=0.95, post=1.5)


# ------------------------------------------------------------------ capsules
def _capsule(B, rnd, fr, tilt, mat, zf, off, fid, shape, wmat):
    """One capsule on face frame fr (origin on the wall plane, w out): tilt degrees in plan; returns its 2D polygon."""
    fr_t = fu.Fr(fr.x, fr.y, fr.yaw + tilt, 0.0)
    # push the capsule out until its back corners clear the wall
    out = (fr.s, -fr.c)
    w0 = 0.12
    for _ in range(40):
        f2 = fr.sub(0, w0, tilt)
        m = min((f2.p(u, 0.0)[0] - fr.x) * out[0] + (f2.p(u, 0.0)[1] - fr.y) * out[1] for u in (-CAP_W / 2, CAP_W / 2))
        if m >= 0.1:
            break
        w0 += 0.1
    f2 = fr.sub(0, w0, tilt)
    z0 = zf + off - 0.1
    z1 = z0 + CAP_H
    fu.lbox(B.m, "CT_ConcreteDark", fr, -0.5, 0.5, -0.02, w0 + 0.55, zf, min(zf + 2.1, z1 - 0.1))      # neck
    fu.lbox(B.m, mat, f2, -CAP_W / 2, CAP_W / 2, 0.0, CAP_D, z0, z1)
    # skirt, roof vent
    fu.lbox(B.detail, "CT_ConcreteDark", f2, -CAP_W / 2 - 0.03, CAP_W / 2 + 0.03, 0.0, CAP_D + 0.03, z0 - 0.0, z0 + 0.14,
            faces="xXyYz")
    fu.lbox(B.detail, "CT_Steel", f2, -0.3, 0.3, 0.7, 1.2, z1, z1 + 0.22, faces="xXyYZ")
    # window on the end face
    zc = (z0 + z1) / 2 + 0.05
    D = CAP_D
    onx = (f2.s, -f2.c, 0.0)
    r = WIN_R
    if shape == "round":
        corners = [f2.p(-r, D, zc - r), f2.p(r, D, zc - r), f2.p(r, D, zc + r), f2.p(-r, D, zc + r)]
        B.window(corners, flat=fid, kind="flat", inset=-0.03, frame=0, shape="round")
        segs = 16
        for k in range(segs):
            a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
            P = lambda rr, a: f2.p(rr * math.cos(a), D + 0.05, zc + rr * math.sin(a))
            _quad_out(B.detail, wmat, [P(r - 0.02, a0), P(r - 0.02, a1), P(r + 0.13, a1), P(r + 0.13, a0)], onx)
    else:
        r = WIN_R * 0.95
        corners = [f2.p(-r, D, zc - r), f2.p(r, D, zc - r), f2.p(r, D, zc + r), f2.p(-r, D, zc + r)]
        B.window(corners, flat=fid, kind="flat", inset=-0.03, frame=0.08, shape="rect")
        c = 0.26                                             # chamfered corners: dark triangles over the card
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            tri = [f2.p(su * r, D + 0.05, zc + sv * (r - c)), f2.p(su * (r - c), D + 0.05, zc + sv * r),
                   f2.p(su * r, D + 0.05, zc + sv * r)]
            B.detail.face(wmat, tri if _tri_faces(tri, onx) else tri[::-1])
        # outer rounded-square bezel: a frame of four strips
        for (a0, a1, b0, b1) in ((-r - 0.1, r + 0.1, r, r + 0.1), (-r - 0.1, r + 0.1, -r - 0.1, -r),
                                 (-r - 0.1, -r, -r, r), (r, r + 0.1, -r, r)):
            _quad_out(B.detail, wmat, [f2.p(a0, D + 0.05, zc + b0), f2.p(a1, D + 0.05, zc + b0),
                                       f2.p(a1, D + 0.05, zc + b1), f2.p(a0, D + 0.05, zc + b1)], onx)
    poly = [f2.p(-CAP_W / 2, 0.0)[:2], f2.p(CAP_W / 2, 0.0)[:2], f2.p(CAP_W / 2, CAP_D)[:2], f2.p(-CAP_W / 2, CAP_D)[:2]]
    return poly, f2


def _tri_faces(tri, out):
    u = (tri[1][0] - tri[0][0], tri[1][1] - tri[0][1], tri[1][2] - tri[0][2])
    v = (tri[2][0] - tri[0][0], tri[2][1] - tri[0][1], tri[2][2] - tri[0][2])
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    return n[0] * out[0] + n[1] * out[1] + n[2] * out[2] > 0


def _bridge(B, xa, xb, yc, zf):
    box(B.m, "CT_Steel", xa - 0.05, xb + 0.05, yc - 0.9, yc + 0.9, zf - 0.28, zf)
    for y in (yc - 0.8, yc + 0.8):
        B.railing([(xa, y), (xb, y)], zf, h=1.1, post=1.0)
        n = max(2, int((xb - xa) / 1.1))
        for i in range(n):
            xs, xe = xa + (xb - xa) * i / n, xa + (xb - xa) * (i + 1) / n
            a, b = ((xs, zf), (xe, zf + 1.05)) if i % 2 == 0 else ((xs, zf + 1.05), (xe, zf))
            fu.rod(B.detail, "CT_Steel", (a[0], y, a[1]), (b[0], y, b[1]), 0.05)
    for x in (xa + 0.4, (xa + xb) / 2, xb - 0.4):                  # cross beams under the deck
        box(B.detail, "CT_Steel", x - 0.05, x + 0.05, yc - 1.0, yc + 1.0, zf - 0.42, zf - 0.28)


# ------------------------------------------------------------------ build
def build(B):
    rnd = random.Random(SEED)
    for core in CORES:
        core["poly"] = _poly(core)
        core["edges"] = _edges(core["poly"])

    def edge_idx(core, yaw):
        for i, e in enumerate(core["edges"]):
            if abs(((e["yaw"] - yaw + 180) % 360) - 180) < 1.0:
                return i
        return None

    holes = {c["id"]: {} for c in CORES}
    wins = {c["id"]: {} for c in CORES}
    reserved = {c["id"]: set() for c in CORES}          # edges taken by a door or a bridge
    bridges = []                                        # (xa, xb, yc)
    for ca, cb in zip(CORES, CORES[1:]):
        ea, eb = edge_idx(ca, 90.0), edge_idx(cb, 270.0)
        xa = ca["edges"][ea]["mid"][0]
        xb = cb["edges"][eb]["mid"][0]
        bridges.append((xa, xb, ca["c"][1], ca["id"], ea, cb["id"], eb))
        reserved[ca["id"]].add(ea)
        reserved[cb["id"]].add(eb)
        for lv in BRIDGE_LEVELS:
            for cid, ei, cc in ((ca["id"], ea, ca), (cb["id"], eb, cb)):
                if lv < cc["n"]:
                    L = cc["edges"][ei]["L"]
                    holes[cid].setdefault(ei, []).append((L / 2 - 0.75, L / 2 + 0.75, lv * STOREY, lv * STOREY + 2.35))
    bdirs = {c["id"]: [] for c in CORES}
    for b in bridges:
        bdirs[b[3]].append(90.0)
        bdirs[b[5]].append(270.0)
    # street doors (south faces)
    for core in CORES:
        ei = edge_idx(core, 0.0)
        e = core["edges"][ei]
        holes[core["id"]].setdefault(ei, []).append((e["L"] / 2 - 0.7, e["L"] / 2 + 0.7, 0.0, 2.4))
        reserved[core["id"]].add(ei)
        core["door_edge"] = ei

    # ---- capsules (decide first so that stair windows go to the free faces)
    cap_polys = {}                                      # level -> polygons
    nflat = {}
    used = {c["id"]: {} for c in CORES}                 # level -> set of edges with a capsule
    plan = []                                           # deferred capsule builds
    bound = (-19.6, 19.6, 0.8, 29.4)
    for core in CORES:
        for lv in range(1, core["n"]):
            order = list(range(len(core["edges"])))
            rnd.shuffle(order)
            for ei in order:
                e = core["edges"][ei]
                if ei in reserved[core["id"]] and ei != core["door_edge"]:
                    continue
                bridging = lv in BRIDGE_LEVELS
                if bridging and any(abs(((e["yaw"] - yw + 180) % 360) - 180) <= 46 for yw in bdirs[core["id"]]):
                    continue
                if rnd.random() < (0.22 if core["kind"] == "sq" else 0.32):
                    continue
                pair = core["kind"] == "sq" and rnd.random() < 0.22
                tilts = [0, 0, 45, -45] if core["kind"] == "sq" else [0, 0, 0, 45, -45]
                if pair:
                    tilts = [0]
                rnd.shuffle(tilts)
                fr = fu.Fr(e["mid"][0], e["mid"][1], e["yaw"], 0.0)
                placed = None
                for tilt in tilts:
                    offs = [-1.2, 1.2] if pair else [0.0]
                    fr_list = [fr.sub(u, 0.0) for u in offs]
                    res = []
                    ok = True
                    for fj in fr_list:
                        w0 = 0.12
                        for _ in range(40):
                            f2 = fj.sub(0, w0, tilt)
                            out = (fj.s, -fj.c)
                            mn = min((f2.p(u, 0.0)[0] - fj.x) * out[0] + (f2.p(u, 0.0)[1] - fj.y) * out[1]
                                     for u in (-CAP_W / 2, CAP_W / 2))
                            if mn >= 0.1:
                                break
                            w0 += 0.1
                        f2 = fj.sub(0, w0, tilt)
                        poly = [f2.p(-CAP_W / 2, 0.0)[:2], f2.p(CAP_W / 2, 0.0)[:2],
                                f2.p(CAP_W / 2, CAP_D + 0.15)[:2], f2.p(-CAP_W / 2, CAP_D + 0.15)[:2]]
                        neck = [fj.p(-0.5, 0.0)[:2], fj.p(0.5, 0.0)[:2], fj.p(0.5, w0 + 0.55)[:2], fj.p(-0.5, w0 + 0.55)[:2]]
                        if not (bound[0] < min(p[0] for p in poly) and max(p[0] for p in poly) < bound[1] and
                                bound[2] < min(p[1] for p in poly) and max(p[1] for p in poly) < bound[3]):
                            ok = False
                            break
                        for q in cap_polys.get(lv, []):
                            if _sat(poly, q, 0.12):
                                ok = False
                                break
                        if not ok:
                            break
                        for c2 in CORES:
                            if c2 is core:
                                if _sat(poly, c2["poly"], 0.05):
                                    ok = False
                            elif _sat(poly, c2["poly"], 0.6):
                                ok = False
                        for b in bridges:
                            if lv in BRIDGE_LEVELS and _sat(poly, _rect(b[0], b[1], b[2] - 1.5, b[2] + 1.5), 0.0):
                                ok = False
                        if not ok:
                            break
                        res.append(poly)
                    if ok:
                        placed = (tilt, res, offs)
                        break
                if not placed:
                    continue
                tilt, polys, offs = placed
                cap_polys.setdefault(lv, []).extend(polys)
                used[core["id"]].setdefault(lv, set()).add(ei)
                plan.append((core, lv, ei, tilt, offs, rnd.choice(SKINS), rnd.choice((0.0, 0.0, 0.3)),
                             rnd.choice(("round", "round", "rect"))))
    # stair windows on the free faces
    for core in CORES:
        for ei, e in enumerate(core["edges"]):
            if e["L"] < 1.8:
                continue
            for lv in range(0, core["n"]):
                if lv >= 1 and ei in used[core["id"]].get(lv, set()):
                    continue
                if ei in reserved[core["id"]] and (lv == 0 or lv in BRIDGE_LEVELS) :
                    continue
                if ei in reserved[core["id"]] and ei == core["door_edge"]:
                    if lv == 0:
                        continue
                if lv == 0 and ei == core["door_edge"]:
                    continue
                zf = lv * STOREY
                wins[core["id"]].setdefault(ei, []).append((e["L"] / 2 - 0.3, e["L"] / 2 + 0.3, zf + 1.0, zf + 2.4))
    # the shells
    for core in CORES:
        hs, ws = holes[core["id"]], wins[core["id"]]
        # windows are holes too
        for ei, lst in ws.items():
            hs.setdefault(ei, []).extend(lst)
        _core_shell(B, core, hs, ws)
        _core_stairs(B, core)

    # bridges
    for (xa, xb, yc, ida, ea, idb, eb) in bridges:
        for lv in BRIDGE_LEVELS:
            if lv < min(c["n"] for c in CORES if c["id"] in (ida, idb)):
                _bridge(B, xa, xb, yc, lv * STOREY)

    # capsules (now built)
    n_flat = 0
    lvl_count = {}
    n_win = 0
    for (core, lv, ei, tilt, offs, mat, off, shape) in plan:
        e = core["edges"][ei]
        zf = lv * STOREY
        lvl_count[lv] = lvl_count.get(lv, 0) + 1
        fid = "%d-%02d" % (lv, lvl_count[lv])
        B.flat(fid, storey=lv, beds=2 if len(offs) == 2 else 1,
               door=(e["mid"][0], e["mid"][1], zf), entrance=core["id"])
        wmat = "CT_Frame" if mat in ("CT_Aluminium", "CT_SteelWhite") else "CT_SteelWhite"
        fr = fu.Fr(e["mid"][0], e["mid"][1], e["yaw"], 0.0)
        for u in offs:
            _capsule(B, rnd, fr.sub(u, 0.0), tilt, mat, zf, off, fid, shape, wmat)
        if len(offs) == 2:                                                     # collar joining a pair
            fu.lbox(B.m, "CT_ConcreteDark", fr, -1.3, 1.3, 0.0, 0.5, zf, zf + 2.0)
        n_flat += 1

    # ---- ground floor: entrances, lobby details, lamps
    z_top = max(c["n"] for c in CORES) * STOREY
    for core in CORES:
        e = core["edges"][core["door_edge"]]
        x, y = e["mid"]
        B.entrance(core["id"], (x, y, 0.0), 0.0, width=1.4, height=2.4, kind="street", leaves=1)
        box(B.m, "CT_Steel", x - 1.3, x + 1.3, y - 1.3, y, 2.55, 2.7)               # canopy
        for sx in (-1.25, 1.25):
            box(B.detail, "CT_Steel", x + sx - 0.04, x + sx + 0.04, y - 1.25, y - 1.17, 0.0, 2.55)
        cx, cy = core["c"]
        # lobby: doormat, mailboxes on the east wall, lamps
        fu.doormat(B, fu.Fr(x, y, 0.0), 0.0, -0.7, size=(1.0, 0.6))
        ee = core["edges"][edge_idx(core, 90.0)]
        wall_x = ee["p0"][0] - WALL
        fu.mailboxes(B, fu.Fr(wall_x, cy - 0.3, 270.0, 0.0), 0.0, 0.9, cols=4, rows=3) if core["kind"] == "sq" else None
        B.lamp((cx - 0.5, cy - 1.95, 2.7), lumens=420, colour="#FFE0B0", kind="lobby", radius=5.5)
        for lv in range(1, core["n"]):
            B.lamp((cx - 0.5, cy - 1.95, lv * STOREY + 2.7), lumens=380, colour="#FFE8C0", kind="stair", radius=5.0)
        fu.plant(B, cx + 1.6, cy - 1.7, 0.0, 0.9, "CT_PlasterRed")
    # a paving path from the street to the middle door
    box(B.m, "CT_Paving", -1.0, 1.0, 0.3, CORES[1]["edges"][CORES[1]["door_edge"]]["mid"][1] - 2.6, 0.0, 0.02, faces="Z")

    ext = [CORES[0]["c"][0] - 7.0, CORES[-1]["c"][0] + 7.0]
    xs, ys = [], []
    for polys in cap_polys.values():
        for p in polys:
            xs += [q[0] for q in p]
            ys += [q[1] for q in p]
    for c in CORES:
        xs += [q[0] for q in c["poly"]]
        ys += [q[1] for q in c["poly"]]
    x0, x1 = max(-20.0, min(xs) - 0.1), min(20.0, max(xs) + 0.1)
    y0, y1 = 0.0, min(30.0, max(ys) + 0.1)
    B.footprint = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    B.height = z_top + 0.9
    B.note("%d flats (%d paired); cores A/C 5 storeys, B 4; bridges at levels 1 and 3" % (
        n_flat, sum(1 for p in plan if len(p[4]) == 2)))
    add_surrounds(B, seed=11, bounds=(x0, x1, 0.3, 12.0))
