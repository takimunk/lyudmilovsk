"""Motor ship «Полдень» (project 543 «Алупка» type adapted, 62.0 x 10.0 m): the cruise ship as a town building.

The ship stands still at the "cruise stage" far out at sea (plot P-SHIP); the sea is made to move past it. It is a
building of the town-interiors pipeline, so seats, beds, shops, games and staff work as anywhere in town.

Frame and numbers: _shipframe.py (ship frame: x from the stern 0 to the bow 62, y port negative, z from the
waterline; S.p / S.yaw give the building frame). R1's plan is kept; every deck is lifted so that no walkable floor
is under the waterline (the sea is not masked inside a hull): D 0.30, C 2.80, B 5.40, A 8.00, bridge 10.60.

This module: the shell. Hull (lofted: raked stem, cruiser stern, flare forward), decks, sides, bulkheads with their
door holes, windows and portholes, all stairs (real steps), rails (in the collision shell: nobody walks overboard),
bulwarks. Exterior fittings: _shipkit.py. The rooms are furnished by _ship_inside_cd.py (furnish_cd: decks D, C) and
_ship_inside_ba.py (furnish_ba: decks B, A, bridge), called at the end if they are there.

Open passages are plain holes (>= 1.10 x 2.10 m). A CLOSED door is a solid wall with a door leaf drawn on it
(leaf(): the furnishers put their `door` thing there). Lists: OPEN_DOORS, CLOSED_DOORS (ship frame), filled while
building and written to the notes.
Livery: deep navy hull and bulwarks, red boot-topping and bottom, a white sheer stripe, white superstructure, cream
funnel, masts and ventilators, the funnel with a red band and a brass sun disc and a black top; planked wood decks
(the timber texture's boards run along U = fore and aft), green deck paint on the steel foredeck and mooring deck.
Requests: none.
"""
import math
import os

import citykit
import geom
from geom import Mesh, Facade
import _shipframe as S

INFO = {
    "name_ru": "Теплоход «Полдень»", "name_en": "Motor ship Polden",
    "style": "1960s coastal passenger motor ship", "use": "public",
    "inspired_by": "Soviet project 543 local-line passenger ships (the «Алупка» type, 1958-66)",
}

WHITE = "CT_SteelWhite"
HULL = "CT_Steel"
RED = "CT_SteelRed"
WOOD = "CT_Wood"
NAVY = "CT_ShipNavy"
CREAM = "CT_ShipCream"
DECKPAINT = "CT_Baize"          # green deck paint on the steel working decks
INT = "CT_Interior"
FLOOR = "CT_Floor"

OPEN_DOORS = []        # (deck, name, x, y, axis, width)
CLOSED_DOORS = []


# ------------------------------------------------------------------------------------------------ small helpers
def qface(m, mat, pts, nrm, uvs=None):
    """A face given in SHIP coordinates, wound so that it looks along nrm (a ship-frame direction)."""
    q = [S.p(*p) for p in pts]
    n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[2], q[0]))
    nb = (nrm[0], -nrm[1], nrm[2])
    if n[0] * nb[0] + n[1] * nb[1] + n[2] * nb[2] < 0:
        q = q[::-1]
        if uvs:
            uvs = uvs[::-1]
    m.face(mat, q, uvs)


def swall(B, a, b, z0, z1, mat, out, holes=(), t=S.WALL_T, centre=False, in_mat=None, m=None, caps=""):
    """A straight wall from a to b (ship 2D), z0..z1 (ship z), t thick. `out` is a ship-frame 2D direction: the face
    on the line a-b looks that way (material mat), the other face (in_mat) is t behind it. centre: the line a-b is the
    wall's middle. holes [(s0, s1, h0, h1)]: s metres from a along a->b, h ship z. caps: "a", "b": close that end.
    Returns (Facade, U) with U(s) -> the facade's u."""
    pa, pb = S.p2(*a), S.p2(*b)
    ob = (out[0], -out[1])
    d = (pb[0] - pa[0], pb[1] - pa[1])
    n = (d[1], -d[0])
    swap = n[0] * ob[0] + n[1] * ob[1] < 0
    p0, p1 = (pb, pa) if swap else (pa, pb)
    F = Facade(p0, p1)
    if centre:
        p0 = (p0[0] + F.n[0] * t / 2, p0[1] + F.n[1] * t / 2)
        p1 = (p1[0] + F.n[0] * t / 2, p1[1] + F.n[1] * t / 2)
        F = Facade(p0, p1)
    Lw = F.length
    U = (lambda s: Lw - s) if swap else (lambda s: s)
    Z0 = S.Z0
    hs = []
    for (s0, s1, h0, h1) in holes:
        u0, u1 = sorted((U(s0), U(s1)))
        hs.append((max(0.0, u0), min(Lw, u1), max(h0, z0) + Z0, min(h1, z1) + Z0))
    mm = Mesh()
    geom.wall_with_holes(mm, mat, Lw, z0 + Z0, z1 + Z0, hs)
    im = in_mat or mat
    inner = Mesh()
    geom.wall_with_holes(inner, im, Lw, z0 + Z0, z1 + Z0, hs)
    inner.verts = [(u, -t, z) for (u, _, z) in inner.verts]
    inner.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in inner.faces]
    mm.extend(inner)
    D = -t
    rv = Mesh()
    for (u0, u1, h0, h1) in hs:
        if h0 > z0 + Z0 + 1e-6:
            rv.face(im, [(u0, 0, h0), (u1, 0, h0), (u1, D, h0), (u0, D, h0)])
        if h1 < z1 + Z0 - 1e-6:
            rv.face(im, [(u1, 0, h1), (u0, 0, h1), (u0, D, h1), (u1, D, h1)])
        rv.face(im, [(u0, 0, h1), (u0, 0, h0), (u0, D, h0), (u0, D, h1)])
        rv.face(im, [(u1, 0, h0), (u1, 0, h1), (u1, D, h1), (u1, D, h0)])
    rv.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in rv.faces]
    mm.extend(rv)
    za, zb = z0 + Z0, z1 + Z0
    for c in caps:
        end_a = (c == "a") != swap           # which facade end: u = 0 (True) or u = Lw
        if end_a:
            mm.face(im, [(0, 0, zb), (0, 0, za), (0, D, za), (0, D, zb)][::-1])
        else:
            mm.face(im, [(Lw, 0, za), (Lw, 0, zb), (Lw, D, zb), (Lw, D, za)][::-1])
    (m or B.m).extend(F.apply(mm))
    return F, U


def win(B, FU, s0, s1, h0, h1, kind="public", cols=1, rows=1, shape="rect", inset=0.10):
    """A window card in the hole (s0, s1, h0, h1) of a wall made by swall (FU = its result)."""
    F, U = FU
    u0, u1 = sorted((U(s0), U(s1)))
    pts = [F.to_building((u, 0.0, z + S.Z0)) for u, z in ((u0, h0), (u1, h0), (u1, h1), (u0, h1))]
    return B.window(pts, kind=kind, cols=cols, rows=rows, inset=inset, shape=shape)


def wall_x(B, deck, y, x0, x1, doors=(), side=1, mat=INT, in_mat=None, t=S.WALL_T, z1=None, z0=None, caps="", closed=()):
    """A bulkhead along the ship (constant y), centred on y. doors: [(xa, xb[, name])] open holes 2.10 high.
    closed: [(xa, xb, name, side)] a door leaf drawn on the +y (1) or -y (-1) face."""
    zf = S.Z[deck] if z0 is None else z0
    zc = S.ceil(deck) if z1 is None else z1
    hs = [(d[0] - x0, d[1] - x0, zf, zf + S.DOOR_H) for d in doors]
    r = swall(B, (x0, y), (x1, y), zf, zc, mat, (0, side), hs, t=t, centre=True, in_mat=in_mat, caps=caps)
    for d in doors:
        OPEN_DOORS.append((deck, d[2] if len(d) > 2 else "", round((d[0] + d[1]) / 2, 2), y, "x", round(d[1] - d[0], 2)))
    for (xa, xb, name, sd) in closed:
        leaf(B, deck, "x", y, xa, xb, sd, name, t=t)
    return r


def wall_y(B, deck, x, y0, y1, doors=(), side=1, mat=INT, in_mat=None, t=S.WALL_T, z1=None, z0=None, caps="", closed=()):
    """A bulkhead across the ship (constant x), centred on x. doors: [(ya, yb[, name])]. closed leaves: side +1 = the
    face looking toward the bow."""
    zf = S.Z[deck] if z0 is None else z0
    zc = S.ceil(deck) if z1 is None else z1
    hs = [(d[0] - y0, d[1] - y0, zf, zf + S.DOOR_H) for d in doors]
    r = swall(B, (x, y0), (x, y1), zf, zc, mat, (side, 0), hs, t=t, centre=True, in_mat=in_mat, caps=caps)
    for d in doors:
        OPEN_DOORS.append((deck, d[2] if len(d) > 2 else "", x, round((d[0] + d[1]) / 2, 2), "y", round(d[1] - d[0], 2)))
    for (ya, yb, name, sd) in closed:
        leaf(B, deck, "y", x, ya, yb, sd, name, t=t)
    return r


def leaf(B, deck, axis, c, s0, s1, side, name="", t=S.WALL_T, zf=None):
    """A closed door drawn on a solid wall: a timber leaf 3 cm proud with a frame and a brass handle (B.detail)."""
    zf = S.Z[deck] if zf is None else zf
    d = B.detail
    f0, f1 = c + side * t / 2, c + side * (t / 2 + 0.03)
    k0, k1 = c + side * t / 2, c + side * (t / 2 + 0.045)
    if axis == "x":
        S.box(d, "CT_Door", s0 + 0.05, s1 - 0.05, f0, f1, zf + 0.02, zf + 2.05)
        S.box(d, WHITE, s0 - 0.02, s0 + 0.05, k0, k1, zf, zf + 2.12)
        S.box(d, WHITE, s1 - 0.05, s1 + 0.02, k0, k1, zf, zf + 2.12)
        S.box(d, WHITE, s0 - 0.02, s1 + 0.02, k0, k1, zf + 2.05, zf + 2.12)
        S.box(d, "CT_Brass", s1 - 0.20, s1 - 0.14, f1, f1 + side * 0.05, zf + 1.0, zf + 1.06)
        CLOSED_DOORS.append((deck, name, round((s0 + s1) / 2, 2), round(c + side * (t / 2 + 0.03), 3), "x", side))
    else:
        S.box(d, "CT_Door", f0, f1, s0 + 0.05, s1 - 0.05, zf + 0.02, zf + 2.05)
        S.box(d, WHITE, k0, k1, s0 - 0.02, s0 + 0.05, zf, zf + 2.12)
        S.box(d, WHITE, k0, k1, s1 - 0.05, s1 + 0.02, zf, zf + 2.12)
        S.box(d, WHITE, k0, k1, s0 - 0.02, s1 + 0.02, zf + 2.05, zf + 2.12)
        S.box(d, "CT_Brass", f1, f1 + side * 0.05, s1 - 0.20, s1 - 0.14, zf + 1.0, zf + 1.06)
        CLOSED_DOORS.append((deck, name, round(c + side * (t / 2 + 0.03), 3), round((s0 + s1) / 2, 2), "y", side))


def stations(x0, x1, extra=()):
    xs = {x0, x1}
    xs.update(x for x in S.STATIONS if x0 < x < x1)
    xs.update(x for x in extra if x0 < x < x1)
    return sorted(xs)


def deck_slab(B, x0, x1, hfun, z1, t=S.DECK_T, voids=(), top=WOOD, bot=WHITE, edge=WHITE, zfun=None, extra=(),
              sides=True, m=None, top_only=False):
    """A deck plate between x0 and x1, symmetric, half-breadth hfun(x), top at z1 (or zfun(x)), with rectangular
    voids [(xa, xb, ya, yb)] (ship frame)."""
    m = m or B.m
    ex = list(extra)
    for v in voids:
        ex += [v[0], v[1]]
    xs = stations(x0, x1, ex)
    zf = zfun or (lambda x: z1)
    for xa, xb in zip(xs, xs[1:]):
        ha, hb = hfun(xa), hfun(xb)
        if ha <= 1e-6 and hb <= 1e-6:
            continue
        za, zb = zf(xa), zf(xb)
        vs = sorted([v for v in voids if v[0] <= xa + 1e-9 and v[1] >= xb - 1e-9], key=lambda v: v[2])
        cuts = [None] + [c for v in vs for c in (v[2], v[3])] + [None]
        for k in range(0, len(cuts), 2):
            ya, yb = cuts[k], cuts[k + 1]
            la = (-ha if ya is None else ya, -hb if ya is None else ya)
            lb = (ha if yb is None else yb, hb if yb is None else yb)
            pts = [(xa, la[0]), (xb, la[1]), (xb, lb[1]), (xa, lb[0])]
            if abs(pts[0][1] - pts[3][1]) < 1e-6:
                pts = pts[:3] if abs(pts[1][1] - pts[2][1]) > 1e-6 else None
            elif abs(pts[1][1] - pts[2][1]) < 1e-6:
                pts = [pts[0], pts[1], pts[3]]
            if not pts:
                continue
            zz = lambda q: za if q[0] == xa else zb
            qface(m, top, [(q[0], q[1], zz(q)) for q in pts], (0, 0, 1))
            if not top_only:
                qface(m, bot, [(q[0], q[1], zz(q) - t) for q in pts], (0, 0, -1))
        if top_only:
            continue
        if sides:
            for sg in (-1, 1):
                if abs(ha - hb) < 1e-9 and ha < 1e-6:
                    continue
                qface(m, edge, [(xa, sg * ha, za - t), (xb, sg * hb, zb - t), (xb, sg * hb, zb), (xa, sg * ha, za)], (0, sg, 0))
        for v in vs:
            if len(v) > 4 and not v[4]:
                continue
            for yy, sg in ((v[2], 1), (v[3], -1)):
                qface(m, edge, [(xa, yy, za - t), (xb, yy, zb - t), (xb, yy, zb), (xa, yy, za)], (0, sg, 0))
    if top_only:
        return
    if sides:
        for xe, sg in ((x0, -1), (x1, 1)):
            h = hfun(xe)
            if h > 1e-3:
                z = zf(xe)
                qface(m, edge, [(xe, -h, z - t), (xe, h, z - t), (xe, h, z), (xe, -h, z)], (sg, 0, 0))
    for v in voids:
        if len(v) > 4 and not v[4]:
            continue
        for xe, sg in ((v[0], 1), (v[1], -1)):
            z = zf(xe)
            qface(m, edge, [(xe, v[2], z - t), (xe, v[3], z - t), (xe, v[3], z), (xe, v[2], z)], (sg, 0, 0))


def floor_patch(B, x0, x1, y0, y1, z, mat=FLOOR, hfun=None, voids=()):
    """A floor finish 5 mm over the deck (linoleum in the rooms; the wood shows on the open decks). With hfun: the
    whole breadth -hfun(x)..hfun(x), minus the voids (stair wells)."""
    if hfun is None:
        qface(B.m, mat, [(x0, y0, z + 0.005), (x1, y0, z + 0.005), (x1, y1, z + 0.005), (x0, y1, z + 0.005)], (0, 0, 1))
        return
    deck_slab(B, x0, x1, hfun, z + 0.005, voids=voids, top=mat, top_only=True)


# ------------------------------------------------------------------------------------------------ hull
_sm = S._sm
stem_x, stern_x, hull_point = S.stem_x, S.stern_x, S.hull_point


HULL_U = sorted(set(S.STATIONS + [14.0 + i * 2.0 for i in range(14)] + [9.0, 12.4, 0.3, 0.75, 1.25, 2.5, 3.5]))
HULL_ROWS_LOW = [S.Z_KEEL, -2.6, -2.2, -1.6, -1.0, -0.5, -0.15, 0.25]
HULL_T = [0.2, 0.42, 0.64, 0.84, 0.93, 1.0]        # between z 0.25 and the sheer


def hull(B):
    m = B.m
    rows = []                                    # each row: [(point, ...)] over HULL_U

    def row_fixed(z):
        return [hull_point(u, z) for u in HULL_U]
    for z in HULL_ROWS_LOW:
        rows.append((row_fixed(z), None))
    for t in HULL_T:
        rows.append(([hull_point(u, 0.25 + t * (S.foredeck_z(u) - 0.25)) for u in HULL_U], t))
    mats = []
    for j in range(len(rows) - 1):
        zmid = (rows[j][0][len(HULL_U) // 2][2] + rows[j + 1][0][len(HULL_U) // 2][2]) / 2
        t1 = rows[j + 1][1]
        mats.append(RED if zmid < 0.25 else (WHITE if (t1 is not None and t1 >= 0.93) else NAVY))
    for j in range(len(rows) - 1):
        r0, r1 = rows[j][0], rows[j + 1][0]
        for i in range(len(HULL_U) - 1):
            a, b, c, d = r0[i], r0[i + 1], r1[i + 1], r1[i]
            for sg in (1, -1):
                q = [(p[0], sg * p[1], p[2]) for p in (a, b, c, d)]
                cx = sum(p[0] for p in q) / 4
                cz = sum(p[2] for p in q) / 4
                cy = sum(p[1] for p in q) / 4
                nrm = (0.0, cy if abs(cy) > 1e-6 else sg, min(0.0, cz) * 2.0 - 0.02)
                for tri in ((q[0], q[1], q[2]), (q[0], q[2], q[3])):
                    n = geom.cross(geom.sub(tri[1], tri[0]), geom.sub(tri[2], tri[0]))
                    if n[0] ** 2 + n[1] ** 2 + n[2] ** 2 < 1e-10:
                        continue
                    qface(m, mats[j], list(tri), nrm)
    # the rubbing strake at the sheer and the bulwarks fore and aft
    top = rows[-1][0]
    strake = [u for u in HULL_U]
    for i in range(len(strake) - 1):
        a, b = top[i], top[i + 1]
        for sg in (1, -1):
            pa, pb = (a[0], sg * a[1], a[2]), (b[0], sg * b[1], b[2])
            na = (0, sg, 0)
            o = 0.07
            qa = [(pa[0], pa[1] + sg * o, pa[2] - 0.20), (pb[0], pb[1] + sg * o, pb[2] - 0.20),
                  (pb[0], pb[1] + sg * o, pb[2] - 0.06), (pa[0], pa[1] + sg * o, pa[2] - 0.06)]
            qface(B.detail, WHITE, qa, na)
            qface(B.detail, WHITE, [qa[3], qa[2], (pb[0], pb[1], pb[2] - 0.03), (pa[0], pa[1], pa[2] - 0.03)], (0, sg, 1))
            qface(B.detail, WHITE, [qa[0], qa[1], (pb[0], pb[1], pb[2] - 0.23), (pa[0], pa[1], pa[2] - 0.23)], (0, sg, -1))


def bulwark(B, x0, x1, h=S.BULWARK_H, t=0.10, gaps=()):
    """The solid bulwark on top of the side between x0 and x1 (outer face flush with the hull, inner face t inboard,
    a wooden cap rail). gaps: [(xa, xb)] left open (fairleads are drawn, not cut)."""
    m = B.m
    xs = stations(x0, x1, [g for gp in gaps for g in gp])
    for xa, xb in zip(xs, xs[1:]):
        if any(g[0] <= xa and xb <= g[1] for g in gaps):
            continue
        ha, hb = S.half(xa), S.half(xb)
        za, zb = S.foredeck_z(xa), S.foredeck_z(xb)
        for sg in (1, -1):
            oa, ob = (xa, sg * ha), (xb, sg * hb)
            ia, ib = (xa, sg * max(0.0, ha - t)), (xb, sg * max(0.0, hb - t))
            if ha < 1e-6 and hb < 1e-6:
                continue
            qface(m, NAVY, [(oa[0], oa[1], za - 0.02), (ob[0], ob[1], zb - 0.02), (ob[0], ob[1], zb + h), (oa[0], oa[1], za + h)], (0, sg, 0))
            if max(ha, hb) > t + 0.02:
                qface(m, WHITE, [(ia[0], ia[1], za), (ib[0], ib[1], zb), (ib[0], ib[1], zb + h), (ia[0], ia[1], za + h)], (0, -sg, 0))
            # cap rail
            ca, cb = (xa, sg * (ha + 0.03)), (xb, sg * (hb + 0.03))
            da, db = (xa, sg * max(0.0, ha - t - 0.03)), (xb, sg * max(0.0, hb - t - 0.03))
            if abs(ca[1] - da[1]) > 1e-6 or abs(cb[1] - db[1]) > 1e-6:
                pts = [(ca[0], ca[1], za + h + 0.05), (cb[0], cb[1], zb + h + 0.05), (db[0], db[1], zb + h + 0.05), (da[0], da[1], za + h + 0.05)]
                if abs(ca[1] - da[1]) < 1e-6:
                    pts = pts[:3]
                elif abs(cb[1] - db[1]) < 1e-6:
                    pts = [pts[0], pts[1], pts[3]]
                qface(m, WOOD, pts, (0, 0, 1))
            qface(m, WOOD, [(ca[0], ca[1], za + h), (cb[0], cb[1], zb + h), (cb[0], cb[1], zb + h + 0.05), (ca[0], ca[1], za + h + 0.05)], (0, sg, 0))
            if max(ha, hb) > t + 0.05:
                qface(m, WOOD, [(da[0], da[1], za + h), (db[0], db[1], zb + h), (db[0], db[1], zb + h + 0.05), (da[0], da[1], za + h + 0.05)], (0, -sg, 0))
    # close the ends of the bulwark
    for xe, sgx in ((x0, -1), (x1, 1)):
        he = S.half(xe)
        if he > t + 0.02:
            ze = S.foredeck_z(xe)
            for sg in (1, -1):
                qface(m, WHITE, [(xe, sg * he, ze), (xe, sg * (he - t), ze), (xe, sg * (he - t), ze + h), (xe, sg * he, ze + h)], (sgx, 0, 0))


# ------------------------------------------------------------------------------------------------ rails and stairs
def rail(B, pts, z, h=S.RAIL_H, post=1.5, wood=True, m=None, z_end=None, canvas=False):
    """An open three-bar rail along the polyline pts (ship 2D) standing on z (z_end: the far end's z, a sloping
    rail). It goes into the SHELL (collision): nobody walks through it. canvas: a white dodger fills it."""
    m = m or B.m
    total = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:])) or 1.0
    run = 0.0
    zz = lambda r: z if z_end is None else z + (z_end - z) * r / total
    for a, b in zip(pts, pts[1:]):
        Ls = math.hypot(b[0] - a[0], b[1] - a[1])
        if Ls < 1e-6:
            continue
        ux, uy = (b[0] - a[0]) / Ls, (b[1] - a[1]) / Ls
        nx, ny = -uy, ux
        z0, z1 = zz(run), zz(run + Ls)
        bars = [(h - 0.06, h, 0.035, WOOD if wood else WHITE), (h * 0.66 - 0.015, h * 0.66 + 0.015, 0.012, WHITE),
                (h * 0.33 - 0.015, h * 0.33 + 0.015, 0.012, WHITE)]
        if canvas:
            bars = [bars[0], (0.08, h - 0.06, 0.008, WHITE)]
        for (b0, b1, w, mat) in bars:
            c = [(a[0] + nx * w, a[1] + ny * w), (b[0] + nx * w, b[1] + ny * w), (b[0] - nx * w, b[1] - ny * w), (a[0] - nx * w, a[1] - ny * w)]
            qface(m, mat, [(c[0][0], c[0][1], z0 + b0), (c[1][0], c[1][1], z1 + b0), (c[1][0], c[1][1], z1 + b1), (c[0][0], c[0][1], z0 + b1)], (nx, ny, 0))
            qface(m, mat, [(c[3][0], c[3][1], z0 + b0), (c[2][0], c[2][1], z1 + b0), (c[2][0], c[2][1], z1 + b1), (c[3][0], c[3][1], z0 + b1)], (-nx, -ny, 0))
            qface(m, mat, [(c[0][0], c[0][1], z0 + b1), (c[1][0], c[1][1], z1 + b1), (c[2][0], c[2][1], z1 + b1), (c[3][0], c[3][1], z0 + b1)], (0, 0, 1))
            if not canvas:
                qface(m, mat, [(c[0][0], c[0][1], z0 + b0), (c[1][0], c[1][1], z1 + b0), (c[2][0], c[2][1], z1 + b0), (c[3][0], c[3][1], z0 + b0)], (0, 0, -1))
        k = max(1, int(round(Ls / post)))
        for i in range(k + 1):
            if i == 0 and run > 0:
                continue
            px, py = a[0] + ux * Ls * i / k, a[1] + uy * Ls * i / k
            pz = z0 + (z1 - z0) * i / k
            S.box(m, WHITE, px - 0.022, px + 0.022, py - 0.022, py + 0.022, pz, pz + h - 0.06, faces="xXyY")
        run += Ls


def edge_pts(deck, x0, x1, sg, inset=0.06, extra=()):
    """The deck edge of a deck between x0 and x1 on side sg (+1 starboard), inset from the edge: a polyline."""
    xs = stations(x0, x1, extra)
    return [(x, sg * (S.deck_half(deck, x) - inset)) for x in xs]


def flight(B, xs, xe, y0, y1, z0, z1, n, mat=WOOD, depth=0.36, m=None, last=False):
    """A straight flight of real steps from ship x = xs (foot, at z0) toward xe (head, z1): n risers, n - 1 treads
    (the n-th riser lands on the deck or landing at xe). Each step is a box `depth` deep (an open soffit)."""
    m = m or B.m
    r = (z1 - z0) / n
    g = (xe - xs) / (n - 1) if not last else (xe - xs) / n
    for k in range(1, n if not last else n + 1):
        xa, xb = xs + g * (k - 1), xs + g * k
        zt = z0 + r * k
        S.box(m, mat, xa, xb, y0, y1, max(z0, zt - depth), zt)
        S.box(B.detail, "CT_Brass", (xa if g > 0 else xa - 0.03), (xa + 0.03 if g > 0 else xa), y0 + 0.02, y1 - 0.02, zt - 0.02, zt + 0.004)
    return r, g


def stringer(B, xs, xe, y, z0, z1, h=1.0, t=0.04, drop=0.30, mat=WHITE):
    """A solid side panel along a flight (balustrade + stringer in one): a sloped slab at y, from `drop` under the
    pitch line to h above it."""
    poly = [(xs, z0 - drop), (xe, z1 - drop), (xe, z1 + h), (xs, z0 + h)]
    if xe < xs:
        poly = [(xe, z1 - drop), (xs, z0 - drop), (xs, z0 + h), (xe, z1 + h)]
    y0, y1 = y - t / 2, y + t / 2
    m = B.m
    for yy, sg in ((y0, -1), (y1, 1)):
        qface(m, mat, [(px, yy, pz) for px, pz in poly], (0, sg, 0))
    for i in range(4):
        (ax, az), (bx, bz) = poly[i], poly[(i + 1) % 4]
        nx, nz = (bz - az), -(bx - ax)
        qface(m, mat, [(ax, y0, az), (bx, y0, bz), (bx, y1, bz), (ax, y1, az)], (nx, 0, nz))


def main_stair(B):
    """The U-stair in the core x 27..31, y +-1.5, decks D..A: starboard flight up (rising aft) to the landing
    x 27.05..29.04, port flight from the landing up to the next deck (rising forward). 16 risers a deck."""
    X0, X1, Y0, Y1 = S.STAIRWELL
    xl = 29.04
    decks = ["D", "C", "B", "A"]
    for i in range(3):
        za, zb = S.Z[decks[i]], S.Z[decks[i + 1]]
        zl = (za + zb) / 2
        flight(B, X1, xl, 0.10, Y1 - 0.05, za, zl, 8)                     # starboard, going aft
        S.box(B.m, WOOD, X0 + 0.05, xl, Y0 + 0.05, Y1 - 0.05, zl - 0.18, zl)     # landing
        flight(B, xl, X1, Y0 + 0.05, -0.10, zl, zb, 8)                    # port, going forward
        # the spine between the flights: a solid balustrade panel following both flights
        S.box(B.m, WHITE, xl, X1, -0.10, 0.10, za if i == 0 else za - 0.9, zb + 1.0 if i < 2 else zb + 1.0, faces="xXyYZ")
        S.box(B.detail, WOOD, xl - 0.02, X1 + 0.02, -0.12, 0.12, zb + 1.0, zb + 1.05)
        # handrails on the core walls
        for (ya, sg) in ((Y1 - 0.09, 1), (Y0 + 0.09, -1)):
            pass
    # at the top deck: close the starboard half (a hole over the flight from below) with a balustrade
    zt = S.Z_A
    S.box(B.m, WHITE, X1 - 0.04, X1 + 0.04, 0.10, Y1 - 0.05, zt, zt + 1.05)
    S.box(B.detail, WOOD, X1 - 0.06, X1 + 0.06, 0.08, Y1 - 0.03, zt + 1.05, zt + 1.10)
    # at the bottom deck the port half is a dead niche under the flight: a locker front closes it
    S.box(B.m, WHITE, X1 - 0.06, X1, Y0 + 0.05, -0.10, S.Z_D, S.Z_D + 2.2)


# ------------------------------------------------------------------------------------------------ decks and rooms
def lining_d(B):
    """Deck D: the inner lining of the hull (the rooms' outer walls), with the portholes."""
    zf, zc = S.Z_D, S.ceil("D")
    xs = stations(8.0, 50.0, [8.0 + 0.5 * k for k in range(84)])
    for sg in (1, -1):
        for xa, xb in zip(xs, xs[1:]):
            ha, hb = S.deck_half("D", xa), S.deck_half("D", xb)
            qface(B.m, INT, [(xa, sg * ha, zf), (xb, sg * hb, zf), (xb, sg * hb, zc), (xa, sg * ha, zc)], (0, -sg, 0))
    # portholes: 2 a cabin (hold cabins 15-20), 2 in the lobby rooms a side; the card is proud of the hull outside,
    # a second card faces the room on the lining, in a brass ring
    ports = []
    for (xa, xb) in ((33.0, 36.5), (36.5, 40.0), (40.0, 43.5)):
        ports += [xa + 1.0, xb - 1.0]
    ports += [28.5, 30.5, 10.0, 12.5, 15.0, 20.5, 24.5]
    for x in ports:
        for sg in (1, -1):
            porthole(B, x, sg, zf + 1.60, 0.36, inner_y=S.deck_half("D", x))


def porthole(B, x, sg, z, dia, inner_y=None, outer_y=None, ring=True):
    """A round porthole at ship x on side sg, centre height z: an outside card 2 cm proud of the side (the shader cuts
    the circle), a brass ring, and the room-side card on the inner wall at |y| = inner_y."""
    r = dia / 2
    yo = (outer_y if outer_y is not None else hull_y(x, z)) + 0.02
    a, b = (x - r, x + r) if sg > 0 else (x + r, x - r)
    # corners CCW seen from outside
    c = [S.p(a, sg * yo, z - r), S.p(b, sg * yo, z - r), S.p(b, sg * yo, z + r), S.p(a, sg * yo, z + r)]
    n = geom.cross(geom.sub(c[1], c[0]), geom.sub(c[3], c[0]))
    if n[1] * (-sg) < 0:
        c = [c[1], c[0], c[3], c[2]]
    B.window(c, kind="public", inset=0.0, frame=0, shape="round", inside=False)
    slot = B.windows[-1]["slot"]
    if ring:
        _ring(B.detail, "CT_Brass", x, sg * (yo + 0.004), z, r + 0.05, r - 0.01, sg, 0.03)
    if inner_y is not None:
        yi = inner_y - 0.012
        q = [S.p(a, sg * yi, z - r), S.p(b, sg * yi, z - r), S.p(b, sg * yi, z + r), S.p(a, sg * yi, z + r)]
        n = geom.cross(geom.sub(q[1], q[0]), geom.sub(q[3], q[0]))
        if n[1] * sg < 0:                       # must look inboard
            q = [q[1], q[0], q[3], q[2]]
        B.win.face("CT_WindowIn", q, [(2, 0), (3, 0), (3, 1), (2, 1)])
        B.win_slot.append(slot)
        _ring(B.detail, "CT_Brass", x, sg * (yi - 0.004), z, r + 0.06, r - 0.01, -sg, 0.03)


def _ring(m, mat, x, y, z, ro, ri, sg, depth, segs=16):
    """A flat ring in the x-z plane at ship y, looking along sg * y."""
    for k in range(segs):
        a0, a1 = 2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs
        pts = [(x + ri * math.cos(a0), y, z + ri * math.sin(a0)), (x + ro * math.cos(a0), y, z + ro * math.sin(a0)),
               (x + ro * math.cos(a1), y, z + ro * math.sin(a1)), (x + ri * math.cos(a1), y, z + ri * math.sin(a1))]
        qface(m, mat, pts, (0, sg, 0))


def hull_y(x, z):
    """Half-breadth of the hull's outer surface at ship (x, z)."""
    return S.hull_half(x, z)


def side_c(B):
    """Deck C: the ship's sides between x 8 and 46 (they carry the promenade deck), with the saloon and cabin windows,
    the galley portholes and the two gangway ports."""
    zf, zc = S.Z_C, S.Z_B - S.DECK_T
    xs = stations(8.0, 46.0)
    sal = [9.2 + 1.6 * k for k in range(6)]
    cab = []
    for xa in (32.0, 34.8, 37.6, 40.4, 43.2):
        cab += [xa + 0.7, xa + 2.1]
    for sg in (1, -1):
        for xa, xb in zip(xs, xs[1:]):
            a, b = (xa, sg * S.half(xa)), (xb, sg * S.half(xb))
            Ls = math.hypot(b[0] - a[0], b[1] - a[1])
            f = Ls / (xb - xa)
            holes, wins = [], []
            for c in sal:
                if xa < c < xb:
                    holes.append(((c - 0.55 - xa) * f, (c + 0.55 - xa) * f, zf + 0.85, zf + 1.85))
                    wins.append((holes[-1], "rect", 2))
            for c in cab:
                if xa < c < xb:
                    holes.append(((c - 0.30 - xa) * f, (c + 0.30 - xa) * f, zf + 1.0, zf + 1.75))
                    wins.append((holes[-1], "rect", 1))
            ports = []
            if sg < 0:
                ports += [21.0, 22.5, 24.0, 25.5, 19.0]        # galley and servery
            else:
                ports += [19.5, 22.5, 25.5]                    # WCs and linen
            ports += [28.0, 29.3] if sg > 0 else [28.0, 29.3]
            gang = xa <= 30.4 and xb >= 31.8
            if gang:
                holes.append(((30.4 - xa) * f, (31.8 - xa) * f, zf, zf + 2.15))
            FU = swall(B, a, b, zf - 0.0, zc, WHITE, (0, sg), holes, t=S.SIDE_T, in_mat=INT)
            for h, shp, cols in wins:
                win(B, FU, h[0], h[1], h[2], h[3], cols=cols)
            for px in ports:
                if xa < px < xb:
                    porthole(B, px, sg, zf + 1.55, 0.40, inner_y=S.half(px) - S.SIDE_T, outer_y=S.half(px))
            if gang:
                # the gangway port: a solid steel gate 1.1 m high in the shell door, a chain rail above it
                y = S.half(30.4)
                S.box(B.m, WHITE, 30.4, 31.8, sg * (y - 0.09), sg * (y - 0.03), zf, zf + 1.10)
                S.box(B.m, WOOD, 30.4, 31.8, sg * (y - 0.12), sg * y, zf + 1.10, zf + 1.15)
                OPEN_DOORS.append(("C", "gangway port %s (gate 1.1 m, not passable)" % ("starboard" if sg > 0 else "port"), 31.1, sg * y, "x", 1.4))


def deck_d(B):
    zf = S.Z_D
    hf = lambda x: S.deck_half("D", x) + 0.17
    deck_slab(B, 8.0, 50.0, hf, zf, top=FLOOR, bot=HULL, sides=False, extra=[8.0 + 0.5 * k for k in range(84)])
    lining_d(B)
    hd = lambda x: S.deck_half("D", x)
    # watertight bulkheads
    wall_y(B, "D", 8.0, -hd(8), hd(8), mat=WHITE, t=0.12)
    wall_y(B, "D", 18.0, -hd(18), hd(18), mat=WHITE, t=0.12)
    wall_y(B, "D", 27.0, -hd(27), hd(27), mat=WHITE, in_mat=INT, t=0.12)
    wall_y(B, "D", 33.0, -hd(33), hd(33), doors=[(-0.55, 0.55, "lobby D - cabin corridor")], t=0.12)
    wall_y(B, "D", 43.5, -hd(43.5), hd(43.5), t=0.12, closed=[(-0.55, 0.55, "hold_door", -1)])
    wall_y(B, "D", 50.0, -hd(50), hd(50), mat=WHITE, t=0.12)
    # the stair core's walls are the walls of the baggage room (port) and the showers (starboard)
    wall_x(B, "D", -1.5, 27.0, 33.0, side=1, closed=[(31.5, 32.6, "baggage_room", 1)])
    wall_x(B, "D", 1.5, 27.0, 33.0, side=-1, doors=[(31.5, 32.6, "showers")])
    # hold cabins 15-20: corridor walls with the doors at the aft end of each cabin
    for sg, nums in ((1, (15, 17, 19)), (-1, (16, 18, 20))):
        doors, closed = [], []
        for k, xa in enumerate((33.0, 36.5, 40.0)):
            if nums[k] == 15:
                doors.append((xa + 0.25, xa + 1.35, "cabin 15"))
            else:
                closed.append((xa + 0.25, xa + 1.35, "cabin%d_door" % nums[k], -sg))
        wall_x(B, "D", sg * 0.75, 33.0, 43.5, doors=doors, closed=closed, side=-sg)
        for xp in (36.5, 40.0):
            y0, y1 = sorted((sg * 0.80, sg * hd(xp)))
            wall_y(B, "D", xp, y0, y1)
    # the engine room: steel plates (the diesel, generators and switchboard are W2's: _ship_inside_cd.py)
    floor_patch(B, 18.06, 26.94, -4.3, 4.3, zf, mat="CT_Steel")


def deck_c(B):
    zf = S.Z_C
    X0, X1, Y0, Y1 = S.STAIRWELL
    voids = [(X0 + 0.06, X1, Y0 + 0.05, Y1 - 0.05),
             (20.06, 22.0, -1.44, 1.44, False), (22.0, 25.5, -1.44, 0.30, False), (25.5, 26.94, -1.44, 1.44, False)]
    zl0, zl1 = zf - S.DECK_T, zf
    for (a, b, nrm) in (((20.06, -1.44), (26.94, -1.44), (0, 1, 0)), ((20.06, -1.44), (20.06, 1.44), (1, 0, 0)),
                        ((26.94, -1.44), (26.94, 1.44), (-1, 0, 0)), ((20.06, 1.44), (22.0, 1.44), (0, -1, 0)),
                        ((25.5, 1.44), (26.94, 1.44), (0, -1, 0)), ((22.0, 0.30), (25.5, 0.30), (0, -1, 0)),
                        ((22.0, 0.30), (22.0, 1.44), (-1, 0, 0)), ((25.5, 0.30), (25.5, 1.44), (1, 0, 0))):
        qface(B.m, WHITE, [(a[0], a[1], zl0), (b[0], b[1], zl0), (b[0], b[1], zl1), (a[0], a[1], zl1)], nrm)
    hf = lambda x: S.half(x) - 0.03
    deck_slab(B, 0.0, 46.0, hf, zf, voids=voids, top=WOOD, bot=WHITE, sides=False)
    deck_slab(B, 46.0, S.L, hf, zf, top=DECKPAINT, bot=HULL, zfun=S.foredeck_z, sides=False)
    floor_patch(B, 8.08, 45.92, 0, 0, zf, hfun=lambda x: S.half(x) - S.SIDE_T - 0.01, voids=[(27.0, 31.06, -1.51, 1.51), (19.95, 27.0, -1.55, 1.55)])
    floor_patch(B, 22.0, 25.5, 0.30, 1.44, zf, mat="CT_Steel")
    floor_patch(B, 0.0, 7.95, 0, 0, zf, mat=DECKPAINT, hfun=lambda x: max(0.0, S.half(x) - 0.11))
    side_c(B)
    hi = lambda x: S.half(x) - S.SIDE_T
    # ---- house ends
    FU = swall(B, (8.0, -S.half(8)), (8.0, S.half(8)), zf, S.ceil("C"), WHITE, (-1, 0),
               [(S.half(8) - 0.8, S.half(8) + 0.8, zf, zf + S.DOOR_H)] +
               [(S.half(8) + c - 0.5, S.half(8) + c + 0.5, zf + 0.85, zf + 1.85) for c in (-3.3, -1.9, 1.9, 3.3)],
               t=S.SIDE_T, in_mat=INT)
    for c in (-3.3, -1.9, 1.9, 3.3):
        win(B, FU, S.half(8) + c - 0.5, S.half(8) + c + 0.5, zf + 0.85, zf + 1.85, cols=2)
    OPEN_DOORS.append(("C", "dining saloon - aft deck (double)", 8.0, 0.0, "y", 1.6))
    h46 = S.half(46)
    FU = swall(B, (46.0, -h46), (46.0, h46), zf, S.ceil("C"), WHITE, (1, 0),
               [(h46 - 0.6, h46 + 0.6, zf, zf + S.DOOR_H)] +
               [(h46 + c - 0.2, h46 + c + 0.2, zf + 1.3, zf + 1.7) for c in (-2.6, 2.6)], t=S.SIDE_T, in_mat=INT)
    OPEN_DOORS.append(("C", "cabin corridor - foredeck (weather door)", 46.0, 0.0, "y", 1.2))
    for c in (-2.6, 2.6):
        win(B, FU, h46 + c - 0.2, h46 + c + 0.2, zf + 1.3, zf + 1.7, shape="round")
    # ---- dining saloon / servery / galley / casing / corridor / WCs
    swall(B, (18.0, -hi(18)), (18.0, hi(18)), zf, S.ceil("C"), "CT_WoodLight", (-1, 0),
          [(hi(18) + 1.6, hi(18) + 2.9, zf, zf + S.DOOR_H), (hi(18) - 1.4, hi(18) + 1.4, zf + 0.95, zf + 2.0)],
          t=S.WALL_T, centre=True, in_mat=INT)
    OPEN_DOORS.append(("C", "dining saloon - starboard corridor", 18.0, 2.25, "y", 1.3))
    OPEN_DOORS.append(("C", "serving hatch (hole 0.95..2.0 over the floor, not passable)", 18.0, 0.0, "y", 2.8))
    leaf(B, "C", "y", 18.0, -3.6, -2.5, -1, "servery service door")
    S.box(B.detail, "CT_WoodLight", 17.80, 18.20, -1.42, 1.42, zf + 0.92, zf + 0.96)        # the hatch's shelf
    # (the serving hatch: a counter-height opening y -1.4..+1.4 is cut below)
    wall_y(B, "C", 20.0, -hi(20), 1.5, side=-1, closed=[(-3.4, -2.3, "galley door", -1)])
    wall_x(B, "C", -1.5, 20.0, 27.0, side=-1, mat=WHITE)                       # casing, port face
    wall_x(B, "C", 1.5, 18.0, 27.0, side=1, doors=[(23.0, 24.1, "engine_door: corridor - engine gallery")], in_mat=WHITE)
    wall_x(B, "C", 3.0, 18.0, 27.0, side=-1, closed=[(19.0, 20.1, "WC men", -1), (22.0, 23.1, "WC women", -1), (25.0, 26.1, "linen", -1)])
    wall_y(B, "C", 21.0, 3.05, hi(21))
    wall_y(B, "C", 24.0, 3.05, hi(24))
    # bulkhead 27: galley half-door (a hole above 1.0 m), casing, corridor door
    zc = S.ceil("C")
    swall(B, (27.0, -hi(27)), (27.0, hi(27)), zf, zc, INT, (1, 0),
          [(hi(27) - 3.6, hi(27) - 2.5, zf + 1.0, zf + 2.1), (hi(27) + 1.6, hi(27) + 2.9, zf, zf + S.DOOR_H)], t=S.WALL_T, centre=True)
    OPEN_DOORS.append(("C", "starboard corridor - vestibule", 27.0, 2.25, "y", 1.3))
    OPEN_DOORS.append(("C", "galley half-door (hole above 1.0 m, not passable)", 27.0, -3.05, "y", 1.1))
    S.box(B.detail, "CT_WoodLight", 26.90, 27.10, -3.62, -2.48, zf + 0.98, zf + 1.02)      # the half-door's shelf
    # the engine gallery: gratings x 22..25.5, y 0.3..1.44 with a rail; the well is open down to the plates
    rail(B, [(22.0, 1.40), (22.0, 0.34), (25.5, 0.34), (25.5, 1.40)], zf, post=1.2, wood=False)
    # the inner faces of the casing well under deck C are the engine room's
    # ---- vestibule: the stair core walls, bulkhead 32
    wall_x(B, "C", -1.5, 27.05, 31.0, side=-1, mat="CT_WoodLight", in_mat=INT, caps="b")
    wall_x(B, "C", 1.5, 27.05, 31.0, side=1, mat="CT_WoodLight", in_mat=INT, caps="b")
    wall_y(B, "C", 32.0, -hi(32), hi(32), doors=[(-0.8, 0.8, "vestibule - cabin corridor (double)")], side=-1)
    # ---- cabins 1-10
    for sg, nums in ((1, (9, 7, 5, 3, 1)), (-1, (10, 8, 6, 4, 2))):
        doors, closed = [], []
        for k, xa in enumerate((32.0, 34.8, 37.6, 40.4, 43.2)):
            d = (xa + 0.25, xa + 1.35)
            if nums[k] in (1, 2, 7):
                doors.append(d + ("cabin %d" % nums[k],))
            else:
                closed.append(d + ("cabin%d_door" % nums[k], -sg))
        wall_x(B, "C", sg * 0.80, 32.05, 45.95, doors=doors, closed=closed, side=-sg)
        for xp in (34.8, 37.6, 40.4, 43.2):
            y0, y1 = sorted((sg * 0.85, sg * hi(xp)))
            wall_y(B, "C", xp, y0, y1)


def serving_hatch(B):
    pass


def deck_b(B):
    zf = S.Z_B
    X0, X1, Y0, Y1 = S.STAIRWELL
    voids = [(X0 + 0.06, X1, Y0 + 0.05, Y1 - 0.05), (3.3, 7.5, 1.5, 2.8)]
    hf = lambda x: S.deck_half("B", x)
    deck_slab(B, 2.0, 48.0, hf, zf, voids=voids, extra=[45.0 + 0.25 * k for k in range(1, 12)])
    zc = S.ceil("B")
    Hh = 3.5
    # ---- deckhouse sides x 12..42.5
    for sg in (1, -1):
        holes, wins, doors = [], [], []
        for c in (13.0, 14.4, 16.2, 17.6):
            wins.append((c - 0.45, c + 0.45, zf + 0.9, zf + 1.8, 2))
        doors.append((18.7, 19.8, "cross passage - promenade"))
        if sg > 0:
            for c in (21.5, 23.5, 25.5):
                wins.append((c - 0.5, c + 0.5, zf + 0.9, zf + 1.85, 2))
        else:
            for c in (22.0, 25.0):
                wins.append((c - 0.3, c + 0.3, zf + 1.2, zf + 1.8, 1))
        for c in (28.3, 29.9):
            wins.append((c - 0.5, c + 0.5, zf + 0.9, zf + 1.85, 2))
        doors.append((31.6, 32.8, "lobby B - promenade"))
        for c in (34.0, 35.5, 39.2, 40.6, 41.9):
            if sg < 0 and c < 36.5:
                continue                      # the bar's mirror back-bar stands against the port wall there
            wins.append((c - 0.55, c + 0.55, zf + 0.85, zf + 1.85, 2))
        doors.append((37.0, 38.2, "music saloon - promenade"))
        holes = [(w[0] - 12.0, w[1] - 12.0, w[2], w[3]) for w in wins] + [(d[0] - 12.0, d[1] - 12.0, zf, zf + S.DOOR_H) for d in doors]
        FU = swall(B, (12.0, sg * Hh), (42.5, sg * Hh), zf, zc, WHITE, (0, sg), holes, t=S.SIDE_T, in_mat="CT_WoodLight")
        for w in wins:
            win(B, FU, w[0] - 12.0, w[1] - 12.0, w[2], w[3], cols=w[4])
        for d in doors:
            OPEN_DOORS.append(("B", d[2] + (" (starboard)" if sg > 0 else " (port)"), round((d[0] + d[1]) / 2, 2), sg * Hh, "x", round(d[1] - d[0], 2)))
    # ---- the saloon's semi-elliptic front x 42.5..45 with its band of windows
    N = 10
    pts = [(42.5 + 2.5 * math.sin(math.pi * k / N), -Hh * math.cos(math.pi * k / N)) for k in range(N + 1)]
    for a, b in zip(pts, pts[1:]):
        Ls = math.hypot(b[0] - a[0], b[1] - a[1])
        mx, my = (a[0] + b[0]) / 2 - 42.5, (a[1] + b[1]) / 2
        h = (Ls / 2 - 0.40, Ls / 2 + 0.40, zf + 0.85, zf + 1.85)
        FU = swall(B, a, b, zf, zc, WHITE, (mx + 0.01, my), [h], t=S.SIDE_T, in_mat="CT_WoodLight")
        win(B, FU, *h)
    # ---- aft wall x 12 with the door to the smoking deck
    FU = swall(B, (12.0, -Hh), (12.0, Hh), zf, zc, WHITE, (-1, 0),
               [(Hh - 0.6, Hh + 0.6, zf, zf + S.DOOR_H)] + [(Hh + c - 0.45, Hh + c + 0.45, zf + 0.9, zf + 1.8) for c in (-2.1, 2.1)],
               t=S.SIDE_T, in_mat=INT)
    for c in (-2.1, 2.1):
        win(B, FU, Hh + c - 0.45, Hh + c + 0.45, zf + 0.9, zf + 1.8, cols=2)
    OPEN_DOORS.append(("B", "aft corridor - smoking deck", 12.0, 0.0, "y", 1.2))
    floor_patch(B, 12.16, 42.5, 0, 0, zf, mat=FLOOR, hfun=lambda x: Hh - 0.155, voids=[(X0, X1 + 0.06, Y0 - 0.06, Y1 + 0.06)])
    hi = Hh - S.SIDE_T
    # ---- lux cabins 11-14 and their corridor
    for sg, nums in ((1, (11, 13)), (-1, (12, 14))):
        doors, closed = [], []
        for k, xa in enumerate((12.15, 15.25)):
            d = (xa + 0.2, xa + 1.3)
            if nums[k] == 11:
                doors.append(d + ("lux cabin 11",))
            else:
                closed.append(d + ("cabin%d_door" % nums[k], -sg))
        wall_x(B, "B", sg * 0.75, 12.15, 18.5, doors=doors, closed=closed, side=-sg)
        y0, y1 = sorted((sg * 0.80, sg * hi))
        wall_y(B, "B", 15.25, y0, y1)
        wall_y(B, "B", 18.5, y0, y1, side=1)
    # ---- casing, pantry, bar store
    wall_y(B, "B", 20.0, -hi, 1.5, side=-1, closed=[(-3.0, -1.9, "pantry", -1)], caps="b")
    wall_x(B, "B", 1.5, 20.0, 27.0, side=1, mat="CT_WoodLight", in_mat=WHITE)
    wall_y(B, "B", 27.0, -hi, 1.5, side=1, closed=[(-3.0, -1.9, "bar store", 1)], caps="b")
    # ---- lobby: stair core walls; bulkhead 33 with the saloon's double door
    wall_x(B, "B", -1.5, 27.05, 31.0, side=-1, mat="CT_WoodLight", in_mat=INT, caps="b")
    wall_x(B, "B", 1.5, 27.05, 31.0, side=1, mat="CT_WoodLight", in_mat=INT, caps="b")
    wall_y(B, "B", 33.0, -hi, hi, doors=[(-0.8, 0.8, "lobby B - music saloon (double)")], side=1, mat="CT_WoodLight")


def house_a_plan():
    """The officers' house / wheelhouse plan (ship 2D), from the aft port corner clockwise seen in the ship frame:
    aft wall x 34, sides y +-3.0 to x 41.4, a curved front to the apex x 43."""
    N = 8
    front = [(41.4 + 1.6 * math.sin(math.pi * k / N), -3.0 * math.cos(math.pi * k / N)) for k in range(N + 1)]
    return front


def deck_a(B):
    zf = S.Z_A
    X0, X1, Y0, Y1 = S.STAIRWELL
    voids = [(X0 + 0.06, X1, Y0 + 0.05, Y1 - 0.05), (6.5, 10.7, -4.3, -3.05)]
    hf = lambda x: S.deck_half("A", x)
    deck_slab(B, 6.0, 46.0, hf, zf, voids=voids, extra=[6.0 + 0.2 * k for k in range(1, 6)] + [43.0 + 0.25 * k for k in range(1, 12)])
    zc = S.ceil("A")
    # ---- the stair hood x 27..32.3, y +-1.7
    for sg in (1, -1):
        FU = swall(B, (27.0, sg * 1.7), (32.3, sg * 1.7), zf, zc, WHITE, (0, sg),
                   [(4.1, 5.2, zf, zf + S.DOOR_H), (1.2, 2.6, zf + 1.0, zf + 1.8)], t=0.12, in_mat=INT)
        win(B, FU, 1.2, 2.6, zf + 1.0, zf + 1.8, cols=2)
        OPEN_DOORS.append(("A", "stair hood - boat deck (%s)" % ("starboard" if sg > 0 else "port"), 31.65, sg * 1.7, "x", 1.1))
    FU = swall(B, (27.0, -1.7), (27.0, 1.7), zf, zc, WHITE, (-1, 0), [(0.7, 2.7, zf + 1.0, zf + 1.9)], t=0.12, in_mat=INT)
    win(B, FU, 0.7, 2.7, zf + 1.0, zf + 1.9, cols=3)
    swall(B, (32.3, -1.7), (32.3, 1.7), zf, zc, WHITE, (1, 0), t=0.12, in_mat=INT)
    S.box(B.m, WHITE, 26.9, 32.4, -1.8, 1.8, zc, zc + 0.12)                 # the hood's roof
    floor_patch(B, 31.0, 32.18, -1.58, 1.58, zf)
    # ---- the officers' house x 34..43 (its roof is the bridge deck)
    front = house_a_plan()
    for sg in (1, -1):
        wins = [(c - 0.35, c + 0.35, zf + 1.0, zf + 1.8) for c in (35.2, 36.4, 38.4, 39.8, 40.9)]
        FU = swall(B, (34.0, sg * 3.0), (41.4, sg * 3.0), zf, zc, WHITE, (0, sg),
                   [(w[0] - 34.0, w[1] - 34.0, w[2], w[3]) for w in wins], t=S.SIDE_T, in_mat="CT_WoodLight")
        for w in wins:
            win(B, FU, w[0] - 34.0, w[1] - 34.0, w[2], w[3])
    for a, b in zip(front, front[1:]):
        Ls = math.hypot(b[0] - a[0], b[1] - a[1])
        h = (Ls / 2 - 0.35, Ls / 2 + 0.35, zf + 1.0, zf + 1.8)
        FU = swall(B, a, b, zf, zc, WHITE, ((a[0] + b[0]) / 2 - 41.39, (a[1] + b[1]) / 2), [h], t=S.SIDE_T, in_mat="CT_WoodLight")
        win(B, FU, *h)
    swall(B, (34.0, -3.0), (34.0, 3.0), zf, zc, WHITE, (-1, 0), [(3.0 - 0.6, 3.0 + 0.6, zf, zf + S.DOOR_H)], t=S.SIDE_T, in_mat=INT)
    OPEN_DOORS.append(("A", "officers' corridor - boat deck (aft door)", 34.0, 0.0, "y", 1.2))
    hi = 3.0 - S.SIDE_T
    wall_x(B, "A", -0.75, 34.15, 39.0, side=1, doors=[(34.5, 35.6, "radio room")], closed=[(37.4, 38.5, "chief mate", 1)])
    wall_x(B, "A", 0.75, 34.15, 39.0, side=-1, closed=[(35.0, 36.1, "captain's bedroom", -1)])
    wall_y(B, "A", 39.0, -0.8, 0.8, side=-1, closed=[(-0.55, 0.55, "captain_door", -1)])
    wall_y(B, "A", 37.0, -hi, -0.8)
    wall_y(B, "A", 38.0, 0.8, hi)
    wall_y(B, "A", 39.0, -hi, -0.8)
    wall_y(B, "A", 39.0, 0.8, hi)
    floor_patch(B, 34.16, 41.4, -hi, hi, zf)
    # ---- engine-room skylight
    S.box(B.m, WHITE, 18.5, 20.0, -1.2, 1.2, zf, zf + 0.45)
    for sg in (1, -1):
        poly = [(18.5, sg * 1.2, zf + 0.45), (20.0, sg * 1.2, zf + 0.45), (20.0, 0, zf + 0.75), (18.5, 0, zf + 0.75)]
        qface(B.m, "CT_Glass", poly, (0, sg, 1))
        for k in range(4):
            xx = 18.5 + 0.5 * k
            qface(B.detail, WHITE, [(xx - 0.03, sg * 1.2, zf + 0.455), (xx + 0.03, sg * 1.2, zf + 0.455), (xx + 0.03, 0, zf + 0.755), (xx - 0.03, 0, zf + 0.755)], (0, sg, 1))
    for xx in (18.5, 20.0):
        qface(B.m, WHITE, [(xx, -1.2, zf + 0.45), (xx, 1.2, zf + 0.45), (xx, 0, zf + 0.75)], (-1 if xx < 19 else 1, 0, 0))


def deck_bridge(B):
    zf = S.Z_BRIDGE
    front = house_a_plan()
    # the bridge deck plate: the house's plan + the wings
    poly = [(34.0, -3.0)] + front + [(34.0, 3.0)]
    bp = S.poly(poly)
    geom.prism(B.m, WHITE, bp, zf - S.DECK_T + S.Z0, zf + S.Z0, top=False, bottom=True)
    B.m.face(WOOD, [(x, y, zf + S.Z0) for x, y in bp])
    for sg in (1, -1):
        y0, y1 = sorted((sg * 3.0, sg * 5.0))
        S.box(B.m, WOOD, 39.0, 41.5, y0, y1, zf - S.DECK_T, zf, faces="Z")
        S.box(B.m, WHITE, 39.0, 41.5, y0, y1, zf - S.DECK_T, zf, faces="xXz" + ("Y" if sg > 0 else "y"))
        # a bracket under the wing
        S.box(B.detail, WHITE, 40.2, 40.3, y0, y1, zf - 0.6, zf - S.DECK_T)
    zc = S.Z_WHEELHOUSE_TOP - 0.10
    # ---- wheelhouse x 38.5..43: sides with the sliding doors to the wings, the curved front with 8 windows
    for sg in (1, -1):
        FU = swall(B, (38.5, sg * 3.0), (41.4, sg * 3.0), zf, zc, WHITE, (0, sg),
                   [(1.0, 2.2, zf, zf + S.DOOR_H), (0.15, 0.85, zf + 1.0, zf + 1.9), (2.35, 2.85, zf + 1.0, zf + 1.9)], t=0.12, in_mat="CT_WoodLight")
        win(B, FU, 0.15, 0.85, zf + 1.0, zf + 1.9)
        win(B, FU, 2.35, 2.85, zf + 1.0, zf + 1.9)
        OPEN_DOORS.append(("BR", "wheelhouse - bridge wing (%s)" % ("starboard" if sg > 0 else "port"), 40.1, sg * 3.0, "x", 1.2))
    for a, b in zip(front, front[1:]):
        Ls = math.hypot(b[0] - a[0], b[1] - a[1])
        h = (0.10, Ls - 0.10, zf + 1.0, zf + 1.95)
        FU = swall(B, a, b, zf, zc, WHITE, ((a[0] + b[0]) / 2 - 41.39, (a[1] + b[1]) / 2), [h], t=0.12, in_mat="CT_WoodLight")
        win(B, FU, *h)
    # aft bulkhead of the wheelhouse: the arch to the chart room (port) and a door to the open deck aft (starboard)
    swall(B, (38.5, -3.0), (38.5, 3.0), zf, zc, WHITE, (-1, 0),
          [(0.5, 2.5, zf, zf + S.DOOR_H), (3.9, 5.0, zf, zf + S.DOOR_H)], t=0.12, in_mat="CT_WoodLight")
    OPEN_DOORS.append(("BR", "wheelhouse - chart room (arch 2.0)", 38.5, -1.5, "y", 2.0))
    OPEN_DOORS.append(("BR", "wheelhouse - open deck aft", 38.5, 1.45, "y", 1.1))
    # chart room x 36..38.5, y -3..0
    FU = swall(B, (36.0, -3.0), (38.5, -3.0), zf, zc, WHITE, (0, -1), [(0.7, 1.8, zf + 1.0, zf + 1.9)], t=0.12, in_mat="CT_WoodLight")
    win(B, FU, 0.7, 1.8, zf + 1.0, zf + 1.9, cols=2)
    swall(B, (36.0, -3.0), (36.0, 0.0), zf, zc, WHITE, (-1, 0), t=0.12, in_mat="CT_WoodLight")
    FU = swall(B, (36.0, 0.0), (38.5, 0.0), zf, zc, WHITE, (0, 1), [(0.7, 1.8, zf + 1.0, zf + 1.9)], t=0.12, in_mat="CT_WoodLight")
    win(B, FU, 0.7, 1.8, zf + 1.0, zf + 1.9, cols=2)
    floor_patch(B, 36.07, 38.44, -2.87, -0.13, zf)
    floor_patch(B, 38.57, 41.4, -2.87, 2.87, zf)
    # roof: over the wheelhouse and the chart room, with a small overhang
    rp = S.poly([(38.3, -3.15)] + [(p[0] + 0.15 * (1 if p[0] > 41.4 else 0), p[1] * 1.05) for p in front] + [(38.3, 3.15)])
    geom.prism(B.m, WHITE, rp, zc + S.Z0, S.Z_WHEELHOUSE_TOP + S.Z0, top=True, bottom=True)
    S.box(B.m, WHITE, 35.85, 38.3, -3.15, 0.15, zc, S.Z_WHEELHOUSE_TOP)


def stairs_out(B):
    """The outside stairs: aft deck C -> B, smoking deck B -> A, boat deck -> both bridge wings."""
    for name, (da, db, xs, xe, y0, y1) in S.STAIRS.items():
        za, zb = S.Z[da], S.Z[db]
        flight(B, xs, xe, y0, y1, za, zb, 16)
        for y in (y0 - 0.02, y1 + 0.02):
            stringer(B, xs, xe, y, za, zb)
            sgn = 1 if xe > xs else -1
            S.box(B.detail, WOOD, min(xs, xe), max(xs, xe), y - 0.035, y + 0.035, 0, 0.001) if False else None
        # wooden caps on the stringers
        for y in (y0 - 0.02, y1 + 0.02):
            qface(B.detail, WOOD, [(xs, y - 0.035, za + 1.003), (xe, y - 0.035, zb + 1.003), (xe, y + 0.035, zb + 1.003), (xs, y + 0.035, za + 1.003)], (0, 0, 1))


def rails(B):
    # ---- deck B: the whole edge (x 2..48), both sides, and round the stern end
    for sg in (1, -1):
        rail(B, edge_pts("B", 2.0, 45.0, sg), S.Z_B)
        rail(B, edge_pts("B", 45.0, 47.9, sg, extra=[45.0 + 0.5 * k for k in range(1, 6)]), S.Z_B, canvas=True, post=0.8)
    rail(B, [(47.9, -S.deck_half("B", 47.9) + 0.06), (47.94, 0.0), (47.9, S.deck_half("B", 47.9) - 0.06)], S.Z_B, canvas=True, post=0.8)
    hb = S.deck_half("B", 2.0) - 0.06
    rail(B, [(2.06, -hb), (2.06, hb)], S.Z_B)
    # the stair opening down to deck C (x 3.3..7.5, y 1.5..2.8): arrival at the aft end x 3.3
    rail(B, [(3.3, 2.86), (7.56, 2.86), (7.56, 1.44), (3.3, 1.44)], S.Z_B, post=1.4)
    # ---- deck A: the edge x 6..46 (boats hang outboard of it), the fore end, the aft end
    for sg in (1, -1):
        rail(B, edge_pts("A", 6.0, 45.9, sg, extra=[6.0 + 0.2 * k for k in range(1, 6)] + [43.0 + 0.5 * k for k in range(1, 6)]), S.Z_A)
    ha = S.deck_half("A", 6.0) - 0.06
    rail(B, [(6.06, -ha), (6.06, ha)], S.Z_A)
    hx = S.deck_half("A", 45.9) - 0.06
    rail(B, [(45.9, -hx), (45.94, 0.0), (45.9, hx)], S.Z_A, post=0.8)
    # the stair opening from deck B (x 6.5..10.7, y -4.3..-3.05): arrival at the fwd end x 10.7
    rail(B, [(10.7, -2.99), (6.44, -2.99), (6.44, -4.36)], S.Z_A, post=1.4)
    # the bridge ladders fill the side decks: the strip left outboard of them (0.5 m) leads nowhere a passenger
    # should go (the forward end of the boat deck is the crew's: ventilators, the stowed companion ladder, the
    # lifejacket chest), so a short rail closes it at the ladder's foot on both sides
    for sg in (1, -1):
        rail(B, [(35.3, sg * 4.45), (35.3, sg * (S.deck_half("A", 35.3) - 0.06))], S.Z_A, post=0.5)
    # ---- bridge deck: wings and the open deck aft
    zb = S.Z_BRIDGE
    for sg in (1, -1):
        # wing: aft edge has the stair arrival y 3.1..4.4
        rail(B, [(39.0, sg * 4.44), (39.0, sg * 4.94), (41.44, sg * 4.94), (41.44, sg * 3.06)], zb, canvas=True, post=0.9)
        # the house roof edge aft of the wing
        rail(B, [(39.0, sg * 3.06), (39.0, sg * 2.94), (34.06, sg * 2.94), (34.06, 0.0)], zb, post=1.3)
        # the stairs' opening: a rail along its inboard side is the house wall below; outboard the stringer


def lamps(B):
    """24 real lights, spread over the public rooms and decks (the pool shows 24 at once)."""
    def L(x, y, deck, lum=900, r=7.0, kind="public", dz=0.25, colour="#FFD9A8"):
        S.lamp(B, (x, y, S.ceil(deck) - dz), lumens=lum, radius=r, kind=kind, colour=colour)
    L(30.0, 3.2, "D", 700, 6.0)             # lobby D / showers side
    L(38.0, 0.0, "D", 600, 7.0)             # hold-cabin corridor
    L(34.8, 2.6, "D", 600, 4.5)             # cabin 15
    L(23.5, 3.0, "D", 900, 8.0, colour="#FFE9C8")     # engine room
    L(11.0, -2.2, "C", 1000, 7.5)           # dining saloon
    L(15.0, 2.2, "C", 1000, 7.5)
    L(22.5, 2.25, "C", 600, 6.0)            # starboard corridor
    L(29.5, 3.2, "C", 900, 7.0)             # vestibule
    L(29.5, -3.2, "C", 900, 7.0)
    L(39.0, 0.0, "C", 700, 8.5)             # cabin corridor
    L(23.6, -3.2, "C", 700, 6.0)            # galley (seen through the half-door)
    L(36.2, 2.8, "C", 600, 4.5)             # cabin 7
    L(44.6, 2.4, "C", 500, 4.5)             # cabin 1
    L(44.6, -2.4, "C", 500, 4.5)            # cabin 2
    L(13.6, 2.0, "B", 600, 4.5)             # lux 11
    L(19.2, 0.0, "B", 600, 6.5)             # cross passage / aft corridor
    L(29.5, 2.6, "B", 800, 6.5)             # lobby B
    L(29.5, -2.6, "B", 800, 6.5)            # library corner
    L(35.5, -1.6, "B", 1000, 7.0)           # bar
    L(35.0, 2.2, "B", 800, 6.0)             # card table
    L(41.3, 0.0, "B", 1300, 8.5)            # dance floor / bandstand
    L(7.0, 0.0, "B", 700, 8.0, kind="lobby")            # smoking deck (under the boat deck)
    L(36.0, -1.9, "A", 600, 5.0)            # radio room
    L(40.6, 0.0, "BR", 500, 6.0, dz=0.35)   # wheelhouse


def build(B):
    del OPEN_DOORS[:]
    del CLOSED_DOORS[:]
    hull(B)
    bulwark(B, 46.0, S.L)
    bulwark(B, 0.0, 8.0)
    deck_d(B)
    deck_c(B)
    deck_b(B)
    deck_a(B)
    deck_bridge(B)
    main_stair(B)
    stairs_out(B)
    rails(B)
    try:
        import _shipkit
        import importlib
        importlib.reload(_shipkit)
        _shipkit.fittings(B, S, __import__(__name__))
        S.FITTINGS = dict(_shipkit.FITTINGS)          # ship-frame points of the deck fittings, for the furnishers
    except ImportError as e:
        B.note("_shipkit missing: %s" % e)
    lamps(B)
    try:
        ft = _shipkit.FITTINGS.get("funnel_top")
        if ft:
            S.spot(B, "funnel_top", "mark", ft, 0.0)
    except NameError:
        pass
    for mod, fn in (("_ship_inside_cd", "furnish_cd"), ("_ship_inside_ba", "furnish_ba")):
        try:
            mm = __import__(mod)
        except ImportError as e:
            B.note("%s not there yet (%s)" % (mod, e))
            continue
        f = getattr(mm, fn, None)
        if f is None:
            B.note("%s.%s not there yet" % (mod, fn))
            continue
        if os.environ.get("SHIP_SHELL_ONLY"):
            continue
        try:
            f(B, S)
        except Exception as e:  # noqa: BLE001  - a furnisher in progress must not stop the shell build
            import traceback
            traceback.print_exc()
            print("SHIP: %s.%s FAILED: %r" % (mod, fn, e))
            B.note("%s.%s failed: %r" % (mod, fn, e))
    B.footprint = S.poly([(0, -5), (62, -5), (62, 5), (0, 5)])
    B.height = 21.0
    B.note("frame: blender/city/buildings/_shipframe.py; ship (x, y, z) -> building (x - 31, 5 - y, z - 1.5); waterline at building z -1.5")
    B.note("open doors: " + "; ".join("%s %s @(%.2f, %.2f) w%.2f" % (d[0], d[1], d[2], d[3], d[5]) for d in OPEN_DOORS))
    B.note("closed doors (leaf on a solid wall): " + "; ".join("%s %s @(%.2f, %.2f)" % (d[0], d[1], d[2], d[3]) for d in CLOSED_DOORS))
    if os.environ.get("KIT_CHECK"):
        import interior_kit as kit
        print(kit.check(B))
