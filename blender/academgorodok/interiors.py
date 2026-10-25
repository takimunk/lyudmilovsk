"""Walkable insides of the three houses (pure Python): floors and ceilings, the inner faces of the
outer walls, a dog-leg stair behind every entrance, and the flats off its landings with their
rooms, corridors and doorways. furniture.py furnishes the rooms; build_academgorodok.py calls
build(house) after plan.py has made the facades.

House frame as in plan.py: x along the house, y across. The street side and the yard side differ
per house (Novikova 1: square at -y, yard at +y; side houses: street at +y, courtyard at -y).

Layout (an inferred plan: no flat plan of these houses is published, docs/academgorodok/
interior_notes.md): every stair cell runs through the house, 2.8 m wide; on the ground floor it is
a lobby from the street door to the yard door, above that a landing hall. A corridor band 1.4 m
wide crosses the house at the stair's landing; each flat lies left or right of a stair, its rooms
in a street row and a yard row on either side of its corridor, partitions between the window
axes, a doorway from the corridor into every room. Doorways are at least 1.1 x 2.2 m (the game's
character standard: 1.08 x 2.10 m, tools/character_standard.py).
"""
import math

import geom
from geom import Mesh, box
import plan
from plan import FLOOR_H, WALL_T, floor_z

INTERIOR_MATERIALS = {
    "AC_IWall":     ("#E9E4D6", 0.9, "lime-white plaster of the inner walls and partitions"),
    "AC_ICeil":     ("#F2EFE6", 0.95, "ceilings"),
    "AC_ISlab":     ("#8E8A82", 0.95, "bare concrete under the finishes (rarely seen)"),
    "AC_IDado":     ("#6F7F6A", 0.6, "green-grey oil-paint dado of the stair halls"),
    "AC_ITile":     ("#B98E6A", 0.7, "metlakh tiles of the landings and lobbies"),
    "AC_IStep":     ("#B7B2A6", 0.6, "terrazzo steps: risers"),
    "AC_ITread":    ("#C9C3B5", 0.45, "terrazzo treads with their nosing"),
    "AC_IHandrail": ("#5A3A22", 0.4, "varnished wooden handrail"),
    "AC_IDoorCase": ("#EDE9DF", 0.5, "painted door cases (architraves)"),
    "AC_ILeaf":     ("#D9D2C0", 0.5, "painted panelled door leaves, standing open"),
    "AC_IOpal":     ("#FFF4DD", 0.3, "opal glass of the stair lamps (emissive)"),
}

CEIL = 3.45            # clear height of a storey (the slab and floor make up the rest of FLOOR_H)
PART_T = 0.12          # partitions
CELL_T = 0.2           # stair cell walls
CELL_HALF = 1.4        # stair cell: centre lines of its walls at sx +- CELL_HALF
BAND = 0.7             # half width of the corridor band
DOOR_W, DOOR_H = 1.1, 2.2
FLAT_DOOR_W = 1.2
RISERS = 11            # per flight; two flights a storey
TREAD = 0.28
FLIGHT_GAP = 0.08      # half the gap between the two flights
VESTIBULE_BACK = -1.5  # Novikova 1: the Gastronom vestibule's back wall (gastronom.py)


class Spec:
    """What interiors need to know about a house beyond its facades."""

    def __init__(self, house):
        self.h = house
        if house.name == "Novikova1":
            L, D = plan.N1_L, plan.N1_D
            self.F = plan.N1_FLOORS
            self.xin = (-L / 2 + WALL_T, L / 2 - WALL_T)
            self.street_in = -D / 2 + WALL_T
            self.yard_sign = 1
            self.yc = 0.0
            self.gastronom = (-2.45, 18.0, -D / 2 + WALL_T, 1.5)
            # ground floor: the shop and its back rooms fill the middle section
            self.closed_ground = (-10.4, 19.45)
        else:
            L, D = plan.S_L, plan.S_D
            self.F = plan.S_FLOORS
            self.xin = (-L / 2 + WALL_T, L / 2 - WALL_T)
            self.street_in = D / 2 - WALL_T
            self.yard_sign = -1
            self.yc = 2.15
            self.gastronom = None
            self.closed_ground = None
        self.stairs = sorted(house.stairs)

    def yard_in(self, x):
        """Inner face of the yard wall at x."""
        if self.h.name == "Novikova1":
            return plan.N1_D / 2 - WALL_T
        D, r, rh = plan.S_D, plan.S_RECESS, plan.S_RECESS_HALF
        return -D / 2 + r + WALL_T if abs(x) < rh + WALL_T else -D / 2 + WALL_T

    def lo_hi(self, x):
        """(low y, high y) of the inside at x."""
        a, b = self.street_in, self.yard_in(x)
        return (min(a, b), max(a, b))


# --- small geometry ------------------------------------------------------------------------------

def hplane(m, mat, x0, x1, y0, y1, z, holes=(), up=True):
    """Horizontal rectangle with rectangular holes [(x0, x1, y0, y1)], facing up or down."""
    xs = sorted(set([x0, x1] + [c for h in holes for c in (h[0], h[1]) if x0 < c < x1]))
    ys = sorted(set([y0, y1] + [c for h in holes for c in (h[2], h[3]) if y0 < c < y1]))
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            cx, cy = (xs[i] + xs[i + 1]) / 2, (ys[j] + ys[j + 1]) / 2
            if any(h[0] < cx < h[1] and h[2] < cy < h[3] for h in holes):
                continue
            a, b, c, d = xs[i], xs[i + 1], ys[j], ys[j + 1]
            pts = [(a, c, z), (b, c, z), (b, d, z), (a, d, z)]
            m.face(mat, pts if up else pts[::-1])


def _frame(p0, p1):
    """Right-handed map of a wall's local (u along p0->p1, v to its left, z) into the house frame."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    L = math.hypot(dx, dy)
    d = (dx / L, dy / L)
    n = (-d[1], d[0])
    return L, (lambda p: (p0[0] + d[0] * p[0] + n[0] * p[1], p0[1] + d[1] * p[0] + n[1] * p[1], p[2]))


def wall(m, mat, p0, p1, t, z0, z1, holes=(), mat_b=None, ends=True):
    """A straight wall of thickness t centred on p0->p1 with door holes [(u centre, w, h over z0)];
    both faces, the liners of the holes, the top and the ends."""
    L, f = _frame(p0, p1)
    loc = Mesh()
    rects = [(c - w / 2, c + w / 2, z0, min(z0 + hh, z1)) for c, w, hh in holes]
    side = Mesh()
    geom.wall_with_holes(side, mat, L, z0, z1, rects)            # faces +y (the wall's left)
    loc.extend(side.transformed(lambda p: (p[0], p[1] + t / 2, p[2])))
    other = Mesh()
    geom.wall_with_holes(other, mat_b or mat, L, z0, z1, [(L - b, L - a, c, d) for a, b, c, d in rects])
    other = other.transformed(lambda p: (L - p[0], -p[1] - t / 2, p[2]))   # turned round: faces -y
    loc.extend(other)
    for u0, u1, a, b in rects:
        r = Mesh()
        geom.reveal(r, "AC_IDoorCase", u0, u1, a, b, t)
        # reveal() lines a hole from y=0 into -y: shift it to span the wall
        loc.extend(r.transformed(lambda p: (p[0], p[1] + t / 2, p[2])))
        # door case: a 7 cm architrave on both faces
        for s in (1, -1):
            yy = s * (t / 2 + 0.015)
            box(loc, "AC_IDoorCase", u0 - 0.08, u0, min(yy, yy - s * 0.015), max(yy, yy - s * 0.015), a, b + 0.08)
            box(loc, "AC_IDoorCase", u1, u1 + 0.08, min(yy, yy - s * 0.015), max(yy, yy - s * 0.015), a, b + 0.08)
            box(loc, "AC_IDoorCase", u0 - 0.08, u1 + 0.08, min(yy, yy - s * 0.015), max(yy, yy - s * 0.015), b, b + 0.08)
    if ends:
        box(loc, mat, 0.0, L, -t / 2, t / 2, z1 - 0.001, z1, faces="Z")
        box(loc, mat, 0.0, L, -t / 2, t / 2, z0, z1, faces="xX")
    m.extend(loc.transformed(f))


def steps_flight(m, x0, x1, y_start, sgn, z_start, n=RISERS, rise=None, tread=TREAD):
    """A straight flight climbing along sgn*y from y_start at z_start: n risers, n-1 treads, the last
    riser up onto whatever is at y_start + sgn*(n-1)*tread."""
    rise = rise or (FLOOR_H / 2) / n
    for i in range(1, n):
        ya, yb = y_start + sgn * (i - 1) * tread, y_start + sgn * i * tread
        top = z_start + i * rise
        # riser block, and the terrazzo tread on it with a rounded-off nosing over the riser below
        box(m, "AC_IStep", x0, x1, min(ya, yb), max(ya, yb), top - 0.32, top - 0.04)
        yn = ya - sgn * 0.035
        box(m, "AC_ITread", x0, x1, min(yn, yb), max(yn, yb), top - 0.04, top - 0.008)
        y0n, y1n = (yn + sgn * 0.008, yb) if sgn > 0 else (yb, yn + sgn * 0.008)
        box(m, "AC_ITread", x0, x1, min(y0n, y1n), max(y0n, y1n), top - 0.008, top, faces="Z")
        # the nose's rounded edge: a bevel strip between the tread's top and its front
        fa, fb = (yn, yn + sgn * 0.008)
        q = [(x0, fa, top - 0.008), (x1, fa, top - 0.008), (x1, fb, top), (x0, fb, top)]
        m.face("AC_ITread", q if sgn > 0 else q[::-1])
    # soffit under the flight: a sloped plate
    ya, yb = y_start, y_start + sgn * (n - 1) * tread
    za, zb = z_start - 0.3, z_start + (n - 1) * rise - 0.3
    pts = [(x0, ya, za), (x1, ya, za), (x1, yb, zb), (x0, yb, zb)]
    m.face("AC_ICeil", pts if sgn < 0 else pts[::-1])


def railing(m, p0, p1, z0, z1, h=0.95):
    """Iron balusters every 12 cm with a wooden handrail, from (p0, z0) to (p1, z1) (a slope)."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    L = math.hypot(dx, dy)
    n = max(1, int(L / 0.12))
    for i in range(n + 1):
        t = i / n
        x, y = p0[0] + dx * t, p0[1] + dy * t
        z = z0 + (z1 - z0) * t
        box(m, "AC_Iron", x - 0.008, x + 0.008, y - 0.008, y + 0.008, z, z + h)
    # handrail: a 6 x 5 cm bar along the slope
    ux, uy = (dx / L, dy / L) if L else (1, 0)
    nx, ny = -uy * 0.03, ux * 0.03
    a, b = (p0[0], p0[1], z0 + h), (p1[0], p1[1], z1 + h)
    q = [(a[0] - nx, a[1] - ny, a[2]), (b[0] - nx, b[1] - ny, b[2]), (b[0] + nx, b[1] + ny, b[2]), (a[0] + nx, a[1] + ny, a[2])]
    m.face("AC_IHandrail", [(p[0], p[1], p[2] + 0.05) for p in q])
    m.face("AC_IHandrail", [(p[0], p[1], p[2]) for p in q][::-1])
    for (pa, pb) in ((q[0], q[1]), (q[2], q[3])):
        m.face("AC_IHandrail", [pa, pb, (pb[0], pb[1], pb[2] + 0.05), (pa[0], pa[1], pa[2] + 0.05)])
        m.face("AC_IHandrail", [pb, pa, (pa[0], pa[1], pa[2] + 0.05), (pb[0], pb[1], pb[2] + 0.05)])


# --- the house -------------------------------------------------------------------------------------

def _windows(house, k):
    """Openings at storey k as (side 'lo'|'hi'|'x0'|'x1', x or y along that wall, width, sill over the
    floor, height, opening, facade) in the house frame."""
    out = []
    for fac, ops, _ in house.facades:
        for o in ops:
            if not (floor_z(k) - 0.3 <= o.z0 < floor_z(k) + FLOOR_H - 0.3):
                continue
            x, y, _ = fac.to_building((o.uc, 0.0, 0.0))
            out.append({"x": x, "y": y, "n": fac.n, "w": o.w, "sill": o.z0 - floor_z(k), "h": o.h, "o": o, "fac": fac})
    return out


def _boundary(spec, wins_all, a, b):
    """A partition line between a and b that cuts no window on the long walls (any storey)."""
    best, best_cost = (a + b) / 2, 1e9
    x = a + 1.0
    while x < b - 1.0:
        cost = abs(x - (a + b) / 2)
        for w in wins_all:
            if abs(w["n"][1]) > 0.5 and abs(w["x"] - x) < w["w"] / 2 + 0.25:
                cost += 1000
        if cost < best_cost:
            best, best_cost = x, cost
        x += 0.05
    return best


def build(house):
    """Mesh of the house's insides (house frame) and the rooms for furniture.py."""
    spec = Spec(house)
    m = Mesh()
    rooms = []
    closed = []                       # window openings that look into a space nobody can enter
    F = spec.F
    x_lo, x_hi = spec.xin
    wins_all = [w for k in range(F) for w in _windows(house, k)]
    st = spec.stairs
    # flats' x ranges: between the stair cells and the section boundaries
    bounds = [x_lo] + [_boundary(spec, wins_all, a, b) for a, b in zip(st, st[1:])] + [x_hi]
    s = spec.yard_sign
    band = (spec.yc - BAND, spec.yc + BAND)
    yb = spec.yc + s * BAND                     # where the flights start: the band's yard edge
    flight_len = (RISERS - 1) * TREAD

    # stair voids (floor holes over the flights) and the cells' y ranges
    voids = []
    for sx in st:
        lo, hi = spec.lo_hi(sx)
        ye = spec.yard_in(sx)
        # the flights and the half landing: open up to the next storey (headroom over the landing)
        voids.append((sx - CELL_HALF + CELL_T / 2, sx + CELL_HALF - CELL_T / 2, min(yb, ye), max(yb, ye)))

    # --- floors and ceilings: the footprint's inner rectangles -------------------------------------
    for k in range(F):
        fz = floor_z(k)
        cz = fz + CEIL
        for (rx0, rx1, ry0, ry1) in _inner_rects(spec):
            holes_f = [v for v in voids] if k > 0 else []
            holes_c = [v for v in voids] if k < F - 1 else []
            if k == 0 and spec.gastronom:
                # the Gastronom's own floor and ceiling (gastronom.py): its vestibule and its hall
                gx0, gx1, gy0, gy1 = spec.gastronom
                g = [(gx0, -gx0, gy0, VESTIBULE_BACK), (-gx0, gx1, gy0, gy1)]
                holes_f = holes_f + g
                holes_c = holes_c + g
            hplane(m, "AC_ITile", rx0, rx1, ry0, ry1, fz - 0.01, _clip(holes_f, rx0, rx1, ry0, ry1), up=True)
            hplane(m, "AC_ICeil", rx0, rx1, ry0, ry1, cz, _clip(holes_c, rx0, rx1, ry0, ry1), up=False)
            # slab underside/top between storeys are hidden in the slab: nothing more

    # --- inner faces of the outer walls --------------------------------------------------------------
    for fac, ops, z_top in house.facades:
        _inner_face(m, house, fac, ops, floor_z(0) - 0.02, floor_z(F - 1) + CEIL + 0.02)

    # --- flats ----------------------------------------------------------------------------------------
    doors_at = {sx: set() for sx in st}
    for k in range(F):
        wins = _windows(house, k)
        for i, sx in enumerate(st):
            for side in (-1, 1):
                if side < 0:
                    fx0, fx1 = bounds[i] + (PART_T / 2 if i > 0 else 0.0), sx - CELL_HALF - CELL_T / 2
                else:
                    fx0, fx1 = sx + CELL_HALF + CELL_T / 2, bounds[i + 1] - (PART_T / 2 if i + 1 < len(st) else 0.0)
                if fx1 - fx0 < 2.5:
                    continue
                if k == 0 and spec.closed_ground and spec.closed_ground[0] - 0.1 < (fx0 + fx1) / 2 < spec.closed_ground[1] + 0.1:
                    closed += [w for w in wins if fx0 < w["x"] < fx1]
                    continue
                doors_at[sx].add((k, side))
                _flat(m, spec, k, fx0, fx1, side, sx, band, wins, rooms, closed)
        # section boundary partitions (between neighbouring flats)
        for bx in bounds[1:-1]:
            lo, hi = spec.lo_hi(bx)
            wall(m, "AC_IWall", (bx, lo), (bx, hi), PART_T, floor_z(k) - 0.01, floor_z(k) + CEIL)

    # --- stair cells ------------------------------------------------------------------------------
    for sx in st:
        _stair(m, spec, sx, band, yb, flight_len, rooms, doors_at[sx])
    house.walks = _walks(spec, rooms, band, yb, flight_len)
    # closed spaces: a lit room box behind each window so it never reads as a hole
    for w in closed:
        o, fac = w["o"], w["fac"]
        if o.kind != "window":
            continue
        u0, u1, z0, z1 = o.rect
        r = Mesh()
        plan._room(r, house.rnd, u0, u1, z0, z1, fac.length)
        m.extend(fac.apply(r))
    return m, rooms


def _walks(spec, rooms, band, yb, flight_len):
    """Routes for the walking test (unreal/scripts/test_academ_walk.py), house frame, feet on the
    floor: up every stair from the lobby to the top landing, and from each landing through every
    flat's door into each of its rooms and back."""
    s = spec.yard_sign
    out = []
    for sx in spec.stairs:
        lo, hi = spec.lo_hi(sx)
        y0 = (spec.street_in + spec.yc) / 2
        if spec.gastronom and spec.gastronom[0] < sx < spec.gastronom[1]:
            y0 = (VESTIBULE_BACK + spec.yc) / 2                    # behind the Gastronom's vestibule
        pts = [(sx, y0, floor_z(0)), (sx, spec.yc, floor_z(0))]
        xa = sx - CELL_HALF / 2
        xb = sx + CELL_HALF / 2
        y_mid = yb + s * (flight_len + 0.6)
        for k in range(spec.F - 1):
            fz = floor_z(k)
            pts += [(xa, spec.yc, fz), (xa, yb + s * 0.2, fz + 0.1), (xa, y_mid, fz + FLOOR_H / 2),
                    (xb, y_mid, fz + FLOOR_H / 2), (xb, yb, fz + FLOOR_H), (sx, spec.yc, fz + FLOOR_H)]
        out.append(("stair %.1f" % sx, pts))
    for r in rooms:
        if r["kind"] != "corridor":
            continue
        z = r["z"]
        d0 = r["doors"][0]
        x_in = r["x1"] - 0.6 if d0["side"] == "x1" else r["x0"] + 0.6
        x_cell = r["x1"] + 1.0 if d0["side"] == "x1" else r["x0"] - 1.0
        yc = (r["y0"] + r["y1"]) / 2
        pts = [(x_cell, yc, z), (x_in, yc, z)]
        for d in r["doors"][1:]:
            room = next((q for q in rooms if q is not r and q.get("z") == z and q["kind"] != "stair" and
                         q["x0"] - 0.01 <= d["c"] <= q["x1"] + 0.01 and
                         (abs(q["y1"] - r["y0"]) < 0.3 if d["side"] == "y0" else abs(q["y0"] - r["y1"]) < 0.3)), None)
            if room is None:
                continue
            inside_y = room["y1"] - 1.0 if d["side"] == "y0" else room["y0"] + 1.0
            # through the doorway into the clear zone furniture.py keeps in front of it, and back
            pts += [(d["c"], yc, z), (d["c"], inside_y, z), (d["c"], yc, z)]
        pts.append((x_in, yc, z))
        out.append(("flat %.1f %.1f" % (r["x0"], z), pts))
    return out


def _clip(holes, x0, x1, y0, y1):
    out = []
    for h in holes:
        a, b, c, d = max(h[0], x0), min(h[1], x1), max(h[2], y0), min(h[3], y1)
        if a < b and c < d:
            out.append((a, b, c, d))
    return out


def _inner_rects(spec):
    x0, x1 = spec.xin
    if spec.h.name == "Novikova1":
        return [(x0, x1, spec.street_in, plan.N1_D / 2 - WALL_T)]
    rh = plan.S_RECESS_HALF + WALL_T
    yr = spec.yard_in(0.0)
    yw = spec.yard_in(x1)
    return [(x0, x1, yr, spec.street_in), (x0, -rh, yw, yr), (rh, x1, yw, yr)]


def _inner_face(m, house, fac, ops, z0, z1):
    """The outer wall's inside face (WALL_T in from the facade), with the facade's holes, facing in.
    Convex corners are shortened, the recess's reflex corners lengthened, by the wall thickness."""
    a, b = _corner_trim(house, fac)
    holes = [(o.rect[0] - a, o.rect[1] - a, o.rect[2], o.rect[3]) for o in ops]
    loc = Mesh()
    geom.wall_with_holes(loc, "AC_IWall", b - a, z0, z1, holes)
    # facade-local +y is outward; the inside face sits at -WALL_T facing -y (inward)
    loc = loc.transformed(lambda p: (p[0] + a, -WALL_T - p[1], p[2]))
    loc.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in loc.faces]
    m.extend(fac.apply(loc))


def _corner_trim(house, fac):
    """(u start, u end) of a wall's inner face: -WALL_T at a reflex corner, +WALL_T at a convex one."""
    ends = []
    for p in (fac.p0, fac.p1):
        # the neighbouring facade sharing this corner
        other = next((f for f, _, _ in house.facades if f is not fac and (f.p1 == p or f.p0 == p)), None)
        if other is None:
            ends.append(WALL_T)
            continue
        if other.p1 == p:      # other comes in, we go out: turn = cross(other.d, fac.d)
            cr = other.d[0] * fac.d[1] - other.d[1] * fac.d[0]
        else:                  # we come in, other goes out
            cr = fac.d[0] * other.d[1] - fac.d[1] * other.d[0]
        # counter-clockwise footprint walked with the outside on the right: a right turn (cr < 0) is
        # convex, a left turn (cr > 0) is the recess's reflex corner
        ends.append(WALL_T if cr < 0 else -WALL_T)
    return ends[0], fac.length - ends[1]


def _stair(m, spec, sx, band, yb, flight_len, rooms, doors_at):
    """doors_at: {(storey, -1|1)} where a flat opens off this stair's landing."""
    F = spec.F
    s = spec.yard_sign
    lo, hi = spec.lo_hi(sx)
    ye = spec.yard_in(sx)
    xa0, xa1 = sx - CELL_HALF + CELL_T / 2, sx - FLIGHT_GAP          # flight A (up from a floor)
    xb0, xb1 = sx + FLIGHT_GAP, sx + CELL_HALF - CELL_T / 2           # flight B (up to the next floor)
    rise = (FLOOR_H / 2) / RISERS
    y_mid0 = yb + s * flight_len                                      # the half landing's edge
    lights = getattr(spec.h, "lights", None)
    if lights is None:
        spec.h.lights = lights = []
    for k in range(F):
        fz = floor_z(k)
        # a landing lamp: an opal globe on a short rod over the corridor band
        lz = fz + CEIL
        box(m, "AC_Iron", sx - 0.01, sx + 0.01, spec.yc - 0.01, spec.yc + 0.01, lz - 0.35, lz)
        box(m, "AC_IOpal", sx - 0.13, sx + 0.13, spec.yc - 0.13, spec.yc + 0.13, lz - 0.62, lz - 0.35)
        lights.append((sx, spec.yc, lz - 0.7, "#FFE2B8", 350, "stair"))
        # side walls (two storeys of wall per storey: full height) with the flat doors at the band
        glo = lo
        if k == 0 and spec.gastronom and spec.gastronom[0] < sx < spec.gastronom[1]:
            glo = VESTIBULE_BACK                 # the Gastronom's vestibule is in front of this lobby
        for sd, xw in ((-1, sx - CELL_HALF), (1, sx + CELL_HALF)):
            holes = [(spec.yc - glo, FLAT_DOOR_W, 2.3)] if (k, sd) in doors_at else []
            wall(m, "AC_IWall", (xw, glo), (xw, hi), CELL_T, fz - 0.01, fz + CEIL, holes, ends=False)
            _dado(m, xw - sd * (CELL_T / 2 + 0.003), -sd, glo, hi, fz, holes)
        if k < F - 1:
            # flights and the half landing
            steps_flight(m, xa0, xa1, yb, s, fz, rise=rise)
            zl = fz + FLOOR_H / 2
            y0l, y1l = sorted((y_mid0, ye))
            box(m, "AC_ITile", xa0, xb1, y0l, y1l, zl - 0.25, zl, faces="Zz" + ("y" if s > 0 else "Y"))
            steps_flight(m, xb0, xb1, y_mid0, -s, zl, rise=rise)
            # railings along the gap, both flights
            railing(m, (sx - FLIGHT_GAP + 0.03, yb), (sx - FLIGHT_GAP + 0.03, y_mid0), fz, zl)
            railing(m, (sx + FLIGHT_GAP - 0.03, y_mid0), (sx + FLIGHT_GAP - 0.03, yb), zl, fz + FLOOR_H)
        else:
            # top floor: guard the void over the last flights
            railing(m, (xa0, yb), (sx, yb), fz, fz)
            railing(m, (sx, yb), (sx, y_mid0), fz, fz)
    rooms.append({"kind": "stair", "x0": sx - CELL_HALF + CELL_T / 2, "x1": sx + CELL_HALF - CELL_T / 2,
                  "y0": lo, "y1": hi, "z": floor_z(0), "h": CEIL})


def _dado(m, x, facing, lo, hi, fz, holes):
    """Oil-paint dado 1.5 m high on a cell wall face at x facing +x (facing=1) or -x, broken at the door."""
    segs = [(lo, hi)]
    for c, w, hh in holes:
        yc = lo + c
        segs = [(lo, yc - w / 2 - 0.08), (yc + w / 2 + 0.08, hi)]
    for a, b in segs:
        if b - a < 0.05:
            continue
        pts = [(x, b, fz), (x, a, fz), (x, a, fz + 1.5), (x, b, fz + 1.5)]      # faces -x as listed
        m.face("AC_IDado", pts if facing < 0 else pts[::-1])
        # a painted line (dado rail) on top of it
        q = [(x, b, fz + 1.5), (x, a, fz + 1.5), (x, a, fz + 1.53), (x, b, fz + 1.53)]
        m.face("AC_IHandrail", q if facing < 0 else q[::-1])


def _flat(m, spec, k, fx0, fx1, side, sx, band, wins, rooms, closed):
    """One flat between fx0 and fx1 at storey k: corridor band from the stair's door, a street row and
    a yard row of rooms."""
    fz, cz = floor_z(k), floor_z(k) + CEIL
    s = spec.yard_sign
    lo_st, hi_st = (spec.street_in, band[0]) if s > 0 else (band[1], spec.street_in)
    seed = int(abs(fx0) * 100) + k * 7919 + (1 if side > 0 else 0)
    # corridor
    rooms.append({"kind": "corridor", "x0": fx0, "x1": fx1, "y0": band[0] + PART_T / 2, "y1": band[1] - PART_T / 2,
                  "z": fz, "h": CEIL, "seed": seed,
                  "doors": [{"side": "x1" if side < 0 else "x0", "c": spec.yc, "w": FLAT_DOOR_W, "h": 2.3}],
                  "windows": []})
    corr = rooms[-1]
    for row in ("street", "yard"):
        # this row's rectangle(s); the yard row of a side house is deeper in the wings
        cuts = [fx0, fx1]
        if spec.h.name != "Novikova1":
            rh = plan.S_RECESS_HALF + WALL_T
            for c in (-rh, rh):
                if fx0 + 0.3 < c < fx1 - 0.3 and row == "yard":
                    cuts.append(c)
        cuts.sort()
        for zx0, zx1 in zip(cuts, cuts[1:]):
            if row == "street":
                ylo, yhi = (spec.street_in, band[0] - PART_T / 2) if s > 0 else (band[1] + PART_T / 2, spec.street_in)
                wall_y = spec.street_in
            else:
                yi = spec.yard_in((zx0 + zx1) / 2)
                ylo, yhi = (band[1] + PART_T / 2, yi) if s > 0 else (yi, band[0] - PART_T / 2)
                wall_y = yi
            if zx1 - zx0 < 1.6:
                # a strip too narrow to walk (where a wing meets the recess): walled off
                closed += [w for w in wins if zx0 - 0.6 < w["x"] < zx1 + 0.6 and not _on_wall(w, spec.street_in)
                           and (abs(w["n"][0]) > 0.5 or zx0 < w["x"] < zx1)]
                xw = zx1 if zx0 == cuts[0] else zx0
                wall(m, "AC_IWall", (xw, ylo), (xw, yhi), PART_T, fz - 0.01, cz)
                yb_ = yhi if s < 0 else ylo
                wall(m, "AC_IWall", (zx0, yb_), (zx1, yb_), PART_T, fz - 0.01, cz)
                continue
            _row_rooms(m, spec, k, zx0, zx1, ylo, yhi, row, wall_y, side, wins, rooms, corr, seed, closed)
    # corridor walls along the band (with the room doorways cut into them by _row_rooms)


def _on_wall(w, y):
    return abs(w["y"] - y) < WALL_T + 0.1 and abs(w["n"][1]) > 0.5


def _row_rooms(m, spec, k, x0, x1, ylo, yhi, row, wall_y, side, wins, rooms, corr, seed, closed):
    """Splits a row between window axes and builds its partitions and corridor-side wall with a
    doorway into every room."""
    fz, cz = floor_z(k), floor_z(k) + CEIL
    s = spec.yard_sign
    rw = sorted([w for w in wins if x0 + 0.2 < w["x"] < x1 - 0.2 and _on_wall(w, wall_y)], key=lambda w: w["x"])
    # side walls of the flat (the end walls of the house) can carry windows too
    side_wins = [w for w in wins if abs(w["n"][0]) > 0.5 and ylo - 0.1 < w["y"] < yhi + 0.1 and
                 (abs(w["x"] - (x0 - WALL_T)) < 0.15 or abs(w["x"] - (x1 + WALL_T)) < 0.15)]
    cuts = [x0]
    acc = x0
    for i, w in enumerate(rw[:-1]):
        nxt = rw[i + 1]
        mid = (w["x"] + w["w"] / 2 + nxt["x"] - nxt["w"] / 2) / 2
        if mid - acc >= 3.2 and x1 - mid >= 2.4:
            cuts.append(mid)
            acc = mid
    cuts.append(x1)
    n = len(cuts) - 1
    # kinds: street: living (the widest), study, bedrooms; yard: kitchen next to the stair, bath, bedrooms
    widths = [cuts[i + 1] - cuts[i] for i in range(n)]
    order = list(range(n)) if side > 0 else list(reversed(range(n)))      # from the stair outwards
    kinds = ["bedroom"] * n
    if row == "street":
        big = max(range(n), key=lambda i: widths[i])
        kinds[big] = "living"
        rest = [i for i in order if i != big]
        if rest:
            kinds[rest[0]] = "study"
    else:
        kinds[order[0]] = "kitchen"
        if n > 2:
            kinds[order[1]] = "bath"
    band_wall_y = yhi if (row == "street") == (s > 0) else ylo        # the side facing the corridor
    doors = []
    for i in range(n):
        a, b = cuts[i], cuts[i + 1]
        dw = 1.4 if kinds[i] == "living" and b - a > 3.0 else DOOR_W
        c = min(max((a + b) / 2, a + dw / 2 + 0.25), b - dw / 2 - 0.25)
        doors.append((c, dw))
        room_wins = [{"side": "y0" if wall_y < band_wall_y else "y1", "c": w["x"], "w": w["w"],
                      "sill": w["sill"], "h": w["h"]} for w in rw if a < w["x"] < b]
        for w in side_wins:
            if (abs(w["x"] - (x0 - WALL_T)) < 0.15 and i == 0) or (abs(w["x"] - (x1 + WALL_T)) < 0.15 and i == n - 1):
                room_wins.append({"side": "x0" if w["x"] < a + 0.5 else "x1", "c": w["y"], "w": w["w"],
                                  "sill": w["sill"], "h": w["h"]})
        rooms.append({"kind": kinds[i], "x0": a + (PART_T / 2 if i > 0 else 0), "x1": b - (PART_T / 2 if i < n - 1 else 0),
                      "y0": ylo, "y1": yhi, "z": fz, "h": CEIL, "seed": seed + 31 * i + (0 if row == "street" else 977),
                      "doors": [{"side": "y1" if band_wall_y == yhi else "y0", "c": c, "w": dw, "h": DOOR_H}],
                      "windows": room_wins})
        if i > 0:
            wall(m, "AC_IWall", (a, ylo), (a, yhi), PART_T, fz - 0.01, cz)
    # the corridor-side wall of this row, with the doorways
    yw = band_wall_y + (PART_T / 2 if band_wall_y == yhi else -PART_T / 2)
    holes = [(c - x0, dw, DOOR_H) for c, dw in doors]
    wall(m, "AC_IWall", (x0, yw), (x1, yw), PART_T, fz - 0.01, cz, holes)
    corr["doors"] += [{"side": "y0" if yw < (corr["y0"] + corr["y1"]) / 2 else "y1", "c": c, "w": dw, "h": DOOR_H}
                      for c, dw in doors]
    # the flat's far end in this row: close the band too when the flat stops short of the house's end
