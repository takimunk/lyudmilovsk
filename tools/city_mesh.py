"""The town's concrete: the terrace walls with their flats' doors and windows, the shop units in W1, the stairs
(along the walls, grand ones on the axes, steps from the promenade to the sand), parapets, the seawall and the
pier's deck on piles (docs/city_plan.md). Pure Python on tools/world_mesh.py's Prim/Mesh; world_mesh.build() adds
these meshes to the world's .glb.

Everything is drawn from tools/city_plan.py's bays, on tools/terrain.py's ground (ground_z) where it matters
(the walls' feet and tops are the terraces' own exact heights: the town keeps the macro shape, keep_flat).

  python3 tools/city_mesh.py out.glb        # just the town, for a look in Blender
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import city_plan as C  # noqa: E402
import world_map as W  # noqa: E402

CONC = "City_Concrete"
CONC_DARK = "City_ConcreteDark"
GLASS = "City_Glass"
DOOR = "City_Door"
SHOPGLASS = "City_ShopGlass"
TIMBER = "City_Timber"
ASPHALT = "City_Asphalt"
DOORS = ["City_Door_Blue", "City_Door_Red", "City_Door_Green", "City_Door_Ochre", "City_Door"]   # painted steel doors

RISE, TREAD = 0.175, 0.30
FOOT = 0.8          # how far walls and stair masses go below the ground
PARAPET_H, PARAPET_T = 1.05, 0.25
REVEAL = 0.28       # depth of the window and door reveals


def _frame(b):
    """Bay frame: origin at the bay's middle on the face; a = along (to p1), n = seaward normal."""
    ax, ay = b["p1"][0] - b["p0"][0], b["p1"][1] - b["p0"][1]
    l = math.hypot(ax, ay) or 1.0
    ax, ay = ax / l, ay / l
    return (b["mid"][0], b["mid"][1]), (ax, ay), (ay, -ax), l


def _P(o, a, n, u, s, z):
    """A point u along, s seaward (negative = into the wall), at z."""
    return (o[0] + a[0] * u + n[0] * s, o[1] + a[1] * u + n[1] * s, z)


def _quad(prim, o, a, n, pts, normal, tile=2.0):
    # world-anchored UVs: a face cut into cells round its windows keeps one continuous concrete texture
    prim.quad([_P(o, a, n, *p) for p in pts], normal, tile, anchor=True)


def _box(prim, o, a, n, u0, u1, s0, s1, z0, z1, top=True, tile=2.0):
    """Axis box in the bay frame: u0..u1 along, s0..s1 seaward, z0..z1."""
    if u1 - u0 < 1e-3 or s1 - s0 < 1e-3 or z1 - z0 < 1e-3:
        return
    N = (n[0], n[1], 0.0)
    S = (-n[0], -n[1], 0.0)
    A = (a[0], a[1], 0.0)
    B = (-a[0], -a[1], 0.0)
    _quad(prim, o, a, n, [(u0, s1, z0), (u1, s1, z0), (u1, s1, z1), (u0, s1, z1)], N, tile)   # seaward face
    _quad(prim, o, a, n, [(u1, s0, z0), (u0, s0, z0), (u0, s0, z1), (u1, s0, z1)], S, tile)
    _quad(prim, o, a, n, [(u1, s1, z0), (u1, s0, z0), (u1, s0, z1), (u1, s1, z1)], A, tile)
    _quad(prim, o, a, n, [(u0, s0, z0), (u0, s1, z0), (u0, s1, z1), (u0, s0, z1)], B, tile)
    if top:
        _quad(prim, o, a, n, [(u0, s0, z1), (u1, s0, z1), (u1, s1, z1), (u0, s1, z1)], (0, 0, 1), tile)


def _levels(b, depth):
    """(lower, upper) ground z at the bay: from the macro shape (the town keeps it exactly), so the fades at the
    town's ends taper the wall."""
    o, a, n, _ = _frame(b)
    lo = W.height(*_P(o, a, n, 0.0, 2.5, 0.0)[:2])
    up = W.height(*_P(o, a, n, 0.0, -(depth + 1.0), 0.0)[:2])
    return lo, up


def _face(prim_c, prims, b, o, a, n, half, z0, z1, openings):
    """The wall's front face at s = 0 from z0 to z1 over u -half..half, with openings [(u0, u1, zb, zt, material)]
    recessed by REVEAL (material None: an open doorway, no panel: a promenade unit's room is behind it)."""
    us = sorted(set([-half, half] + [v for op in openings for v in (op[0], op[1])]))
    zs = sorted(set([z0, z1] + [v for op in openings for v in (op[2], op[3])]))
    N = (n[0], n[1], 0.0)
    for i in range(len(us) - 1):
        for j in range(len(zs) - 1):
            uc, zc = (us[i] + us[i + 1]) / 2, (zs[j] + zs[j + 1]) / 2
            if any(op[0] < uc < op[1] and op[2] < zc < op[3] for op in openings):
                continue
            _quad(prim_c, o, a, n, [(us[i], 0, zs[j]), (us[i + 1], 0, zs[j]), (us[i + 1], 0, zs[j + 1]), (us[i], 0, zs[j + 1])], N)
    for u0, u1, zb, zt, mat in openings:
        r = -REVEAL
        A, B = (a[0], a[1], 0.0), (-a[0], -a[1], 0.0)
        _quad(prim_c, o, a, n, [(u0, 0, zb), (u0, r, zb), (u0, r, zt), (u0, 0, zt)], A)        # left reveal faces +a
        _quad(prim_c, o, a, n, [(u1, r, zb), (u1, 0, zb), (u1, 0, zt), (u1, r, zt)], B)
        _quad(prim_c, o, a, n, [(u0, r, zt), (u1, r, zt), (u1, 0, zt), (u0, 0, zt)], (0, 0, -1))  # head
        if mat is None:
            # an open doorway: a concrete threshold 2 cm over the ground (the landscape is level with zb there),
            # flush with the room's floor behind it
            _box(prim_c, o, a, n, u0, u1, r, 0.07, zb - 0.06, zb + 0.02)
        elif zb > z0 + 0.01:
            _quad(prim_c, o, a, n, [(u0, 0, zb), (u1, 0, zb), (u1, r, zb), (u0, r, zb)], (0, 0, 1))  # sill
            # a precast sill sticking out a little
            _box(prim_c, o, a, n, u0 - 0.08, u1 + 0.08, 0.0, 0.07, zb - 0.06, zb)
        if mat is None:
            continue
        _quad(prims[mat], o, a, n, [(u0, r, zb), (u1, r, zb), (u1, r, zt), (u0, r, zt)], N, tile=max(u1 - u0, 0.5))
        if mat == GLASS:                          # a mullion and a transom: windows read as windows
            uc = (u0 + u1) / 2
            _box(prims[DOOR], o, a, n, uc - 0.03, uc + 0.03, r, r + 0.05, zb, zt)
            zt3 = zt - (zt - zb) * 0.3
            _box(prims[DOOR], o, a, n, u0, u1, r, r + 0.05, zt3 - 0.03, zt3 + 0.03)


def _gaps(u0, u1, holes):
    """The pieces of u0..u1 left between the holes [(a, b)]."""
    out, u = [], u0
    for a, b in sorted(holes):
        if a > u:
            out.append((u, a))
        u = max(u, b)
    if u < u1:
        out.append((u, u1))
    return out


def _parapet(prim, o, a, n, u0, u1, up):
    # 2 cm proud of the wall's face: a coping line, not a 5 cm strip coplanar with the face below (z-fighting)
    _box(prim, o, a, n, u0, u1, -PARAPET_T, 0.02, up - 0.05, up + PARAPET_H)


def _stair_flight(prim, frames, a_start, lower, upper, width, s0=0.0, balustrade=True):
    """A straight flight along the face of one or more bays, starting at arc a_start (metres from the first bay's
    start) and rising toward p1. frames: [(o, a, n, length)] of consecutive bays."""
    n_steps = max(1, int(round((upper - lower) / RISE)))
    rise = (upper - lower) / n_steps
    starts = []
    acc = 0.0
    for f in frames:
        starts.append(acc)
        acc += f[3]

    def put(arc0, arc1, z_top, s_lo, s_hi):
        for (o, a, n, l), st in zip(frames, starts):
            lo, hi = max(arc0, st), min(arc1, st + l)
            if hi - lo > 1e-3:
                _box(prim, o, a, n, lo - st - l / 2, hi - st - l / 2, s_lo, s_hi, lower - FOOT, z_top)

    put(a_start - 1.0, a_start, lower + 0.02, s0, s0 + width)                 # bottom landing
    for k in range(n_steps):
        arc0 = a_start + k * TREAD
        put(arc0, arc0 + TREAD, lower + (k + 1) * rise, s0, s0 + width)
        if balustrade:
            put(arc0, arc0 + TREAD, lower + (k + 1) * rise + 0.95, s0 + width, s0 + width + 0.15)
    top0 = a_start + n_steps * TREAD
    put(top0, top0 + 1.4, upper, s0, s0 + width)                             # top landing, against the face
    if balustrade:
        put(top0, top0 + 1.4, upper + 0.95, s0 + width, s0 + width + 0.15)
    return top0 + 1.4


def _grand(prim, o, a, n, width, lower, upper, tread=TREAD, rise_t=RISE, cheeks=True):
    n_steps = max(1, int(round((upper - lower) / rise_t)))
    rise = (upper - lower) / n_steps
    for k in range(n_steps):
        s_hi = (n_steps - k) * tread
        _box(prim, o, a, n, -width / 2, width / 2, 0.0, s_hi, lower - FOOT if k == 0 else lower + k * rise - 0.4,
             lower + (k + 1) * rise)
    if cheeks:
        run = n_steps * tread
        for side in (-1, 1):
            u0 = side * width / 2
            u1 = u0 + side * 0.4
            for k in range(n_steps):
                s_hi = (n_steps - k) * tread
                _box(prim, o, a, n, min(u0, u1), max(u0, u1), s_hi - tread, s_hi, lower - FOOT, lower + (k + 1) * rise + 0.6)
    return n_steps * tread


def _channel(prim, o, a, n, u0, width, lower, upper, tread=TREAD, rise_t=RISE):
    """A stepped concrete channel from u0 to u0 + width (along), the grand stair's pitch, its bed CASCADE_BED under the
    stair's steps, and an outer cheek 0.4 m thick standing 0.6 m over the steps."""
    n_steps = max(1, int(round((upper - lower) / rise_t)))
    rise = (upper - lower) / n_steps
    bed = C.CASCADE_BED
    for k in range(n_steps):
        s_hi = (n_steps - k) * tread
        _box(prim, o, a, n, u0, u0 + width, 0.0, s_hi, lower - FOOT if k == 0 else lower + k * rise - 0.4 - bed,
             lower + (k + 1) * rise - bed)
        _box(prim, o, a, n, u0 + width, u0 + width + 0.4, s_hi - tread, s_hi, lower - FOOT, lower + (k + 1) * rise + 0.6)


def walls(mesh):
    """The terrace walls W1..W5: bays with flats (W2..W5) or shop units (W1), wall stairs, grand stairs."""
    prim_c = mesh.p(CONC)
    prims = {GLASS: mesh.p(GLASS), DOOR: mesh.p(DOOR), SHOPGLASS: mesh.p(SHOPGLASS), CONC: prim_c}
    for i in range(len(C.WALLS)):
        depth = C.wall_depth(i)
        bays = C.wall_bays(i)
        for k, b in enumerate(bays):
            if hasattr(mesh, "at"):
                mesh.at(b["mid"][1], b["wall"])
            prim_c = mesh.p(CONC)
            prims = {GLASS: mesh.p(GLASS), DOOR: mesh.p(DOOR), SHOPGLASS: mesh.p(SHOPGLASS), CONC: prim_c}
            hsh = sum(ord(c) * (j + 7) for j, c in enumerate(b["id"]))
            door_mat = DOORS[hsh % len(DOORS)]
            prims[door_mat] = mesh.p(door_mat)
            o, a, n, l = _frame(b)
            half = l / 2 + 0.02
            lower, upper = _levels(b, depth)
            if upper - lower < 0.25:
                continue
            openings = []
            full = b["mask"] >= 0.999
            if full and b["kind"] == "dwelling":
                for kind, u, sill, ww, wh in C.DWELLING_OPENINGS:
                    openings.append((u - ww / 2, u + ww / 2, lower + sill, lower + sill + wh, door_mat if kind == "door" else GLASS))
                    if kind == "door":                # a little concrete canopy over the door
                        _box(prim_c, o, a, n, u - ww / 2 - 0.35, u + ww / 2 + 0.35, 0.0, 0.85, lower + sill + wh + 0.15,
                             lower + sill + wh + 0.27)
                if hsh % 3 == 0:                      # a balcony along the upper storey (danchi style), solid balustrade
                    zb = lower + 4.25
                    _box(prim_c, o, a, n, -l / 2 + 0.4, l / 2 - 0.4, 0.0, 1.15, zb - 0.16, zb)
                    _box(prim_c, o, a, n, -l / 2 + 0.4, l / 2 - 0.4, 1.0, 1.15, zb, zb + 1.0)
                    for su in (-l / 2 + 0.4, l / 2 - 0.55):
                        _box(prim_c, o, a, n, su, su + 0.15, 0.0, 1.0, zb, zb + 1.0)
            elif full and b["kind"] == "unit":
                # the doorway is open: the unit's room (blender/city/buildings/promenade_unit.py, placed by
                # import_city.py at the unit's "front") stands in the hollow block behind the face; the shop window
                # keeps its glass (ALyudCityWindows lights the pane W1-Uxx-w1 in front of it)
                for kind, u, sill, ww, wh in C.UNIT_OPENINGS:
                    openings.append((u - ww / 2, u + ww / 2, lower + sill, lower + sill + wh, None if kind == "door" else SHOPGLASS))
            _face(prim_c, prims, b, o, a, n, half, lower - FOOT, upper + 0.02, openings)
            # the block's top (with the upper terrace) and its ends (seen where the fade steps it down);
            # 2 cm over the terrace: the landscape's ramp behind the face (city_plan.WALL_RAMP) is level with the upper
            # terrace for the block's last 1.6 m, and a top at exactly that height would fight the ground there
            top = upper + 0.02
            _quad(prim_c, o, a, n, [(-half, 0, top), (half, 0, top), (half, -depth, top), (-half, -depth, top)], (0, 0, 1))
            _quad(prim_c, o, a, n, [(-half, -depth, lower - FOOT), (-half, 0, lower - FOOT), (-half, 0, top), (-half, -depth, top)],
                  (-a[0], -a[1], 0.0))
            _quad(prim_c, o, a, n, [(half, 0, lower - FOOT), (half, -depth, lower - FOOT), (half, -depth, top), (half, 0, top)],
                  (a[0], a[1], 0.0))
            # a plinth line and a drip course: the face is not a flat sheet
            # (open at a unit's doorway: nothing to trip over on the way in)
            for p0, p1 in _gaps(-half, half, [(u - ww / 2, u + ww / 2) for kind, u, _s, ww, _h in C.UNIT_OPENINGS
                                              if kind == "door"] if full and b["kind"] == "unit" else []):
                _box(prim_c, o, a, n, p0, p1, 0.0, 0.06, lower - 0.1, lower + 0.35)
            if upper - lower > 4.0:
                zc = lower + 3.7
                _box(prim_c, o, a, n, -half, half, 0.0, 0.12, zc - 0.12, zc)
            # parapets on the upper edge (open where stairs arrive and over the grand stairs)
            if b["kind"] == "stair":
                if i == 0:
                    _parapet(prim_c, o, a, n, -half, half - 3.0, upper)
                elif b.get("stair_part") == 1:
                    _parapet(prim_c, o, a, n, -half, half - 2.8, upper)
                else:
                    _parapet(prim_c, o, a, n, -half, half, upper)
            elif b["kind"] != "grand":
                _parapet(prim_c, o, a, n, -half, half, upper)
        # wall stairs along the face
        for k, b in enumerate(bays):
            if b["kind"] != "stair" or b["mask"] < 0.999:
                continue
            if hasattr(mesh, "at"):
                mesh.at(b["mid"][1], b["wall"])
            prim_c = mesh.p(CONC)
            lower, upper = b["lower"], b["upper"]
            if i == 0:
                o, a, n, l = _frame(b)
                _stair_flight(prim_c, [(o, a, n, l)], 1.6, lower, upper, 2.2)
            elif b.get("stair_part") == 0 and k + 1 < len(bays):
                fr = [_frame(b), _frame(bays[k + 1])]
                _stair_flight(prim_c, fr, 1.3, lower, upper, 2.2)
        # grand stairs: one per group of grand bays, centred on the group, perpendicular to the face
        groups = {}
        for b in bays:
            if b["kind"] == "grand" and b["mask"] >= 0.999:
                groups.setdefault(b.get("grand_y", 905.0), []).append(b)
        for gy, grp in groups.items():
            c = grp[len(grp) // 2]
            if hasattr(mesh, "at"):
                mesh.at(c["mid"][1], c["wall"])
            prim_c = mesh.p(CONC)
            o, a, n, l = _frame(c)
            if len(grp) % 2 == 0:                 # centre between the two middle bays
                c0 = grp[len(grp) // 2 - 1]
                o = ((c0["mid"][0] + c["mid"][0]) / 2, (c0["mid"][1] + c["mid"][1]) / 2)
            if i == 0:
                _grand(prim_c, o, a, n, 20.0, c["lower"], c["upper"], tread=0.36, rise_t=0.15)
            else:
                width = C.GRAND_W.get(gy, 8.0)
                _grand(prim_c, o, a, n, width, c["lower"], c["upper"])
                extra = 0.0
                if gy == C.CASCADE_Y:                 # the water channel beside the stair's north cheek
                    extra = C.CASCADE_W + 0.4
                    _channel(prim_c, o, a, n, width / 2 + 0.4, C.CASCADE_W, c["lower"], c["upper"])
                # parapets on the grand bays beside the stair
                arc_c = sum(g["arc"] for g in grp[len(grp) // 2 - (1 - len(grp) % 2): len(grp) // 2 + 1]) / (2 - len(grp) % 2)
                for g in grp:
                    go, ga, gn, gl = _frame(g)
                    u0, u1 = -gl / 2, gl / 2
                    lo_cut = arc_c - width / 2 - 0.4 - g["arc"]
                    hi_cut = arc_c + width / 2 + 0.4 + extra - g["arc"]
                    if lo_cut > u0:
                        _parapet(prim_c, go, ga, gn, u0, min(u1, lo_cut), g["upper"])
                    if hi_cut < u1:
                        _parapet(prim_c, go, ga, gn, max(u0, hi_cut), u1, g["upper"])


def lanes(mesh):
    """The lanes along the foot of W2..W5: asphalt from the face out to LANE_W with a granite-grey kerb on the outer edge,
    and the street lamps (concrete poles with an arm; the heads glow at night: ALyudCityWindows reads city_plan's lamps)."""
    for i in range(1, len(C.WALLS)):
        for b in C.wall_bays(i):
            if b["mask"] < 0.999 or b["kind"] == "plot":
                continue
            if hasattr(mesh, "at"):
                mesh.at(b["mid"][1], b["wall"] + "L")
            o, a, n, l = _frame(b)
            half = l / 2 + 0.03
            z = b["lower"]
            _box(mesh.p(ASPHALT), o, a, n, -half, half, 0.0, C.LANE_W, z - 0.3, z + 0.05, tile=4.0)
            _box(mesh.p(CONC_DARK), o, a, n, -half, half, C.LANE_W, C.LANE_W + 0.3, z - 0.3, z + 0.15)
    for lp in C.lamps():
        b = next(x for x in C.wall_bays(int(lp["wall"][1]) - 1) if x["id"] == lp["bay"])
        if hasattr(mesh, "at"):
            mesh.at(b["mid"][1], b["wall"] + "L")
        o, a, n, l = _frame(b)
        z = b["lower"]
        pc = mesh.p(CONC)
        s0 = C.LANE_W + 0.5
        _box(pc, o, a, n, -0.11, 0.11, s0 - 0.11, s0 + 0.11, z, z + C.LAMP_H + 0.15)          # the pole
        _box(pc, o, a, n, -0.05, 0.05, C.LANE_W - 0.9, s0, z + C.LAMP_H + 0.05, z + C.LAMP_H + 0.15)   # the arm
        _box(mesh.p(DOOR), o, a, n, -0.22, 0.22, C.LANE_W - 1.0, C.LANE_W - 0.4, z + C.LAMP_H - 0.08, z + C.LAMP_H + 0.07)  # the head


def seawall(mesh):
    """The promenade's seawall over the beach: a sloped-foot concrete face, a kerb to sit on, steps to the sand."""
    for b in C.wall_bays(-1):
        if hasattr(mesh, "at"):
            mesh.at(b["mid"][1])
        prim = mesh.p(CONC_DARK)
        kerb = mesh.p(CONC)
        o, a, n, l = _frame(b)
        half = l / 2 + 0.02
        lower = W.height(*_P(o, a, n, 0.0, 3.0, 0.0)[:2])
        upper = W.height(*_P(o, a, n, 0.0, -3.0, 0.0)[:2])
        if upper - lower < 0.25:
            continue
        # the face with a curved-back toe (a recurve wave wall, stepped): three setbacks
        steps = b["kind"] == "sea_steps" and b["mask"] >= 0.999
        # where the steps come down the toe and the setback stay off them (their tops lay 1 cm under the steps' treads)
        for u0, u1 in ((-half, -1.9), (1.9, half)) if steps else ((-half, half),):
            _box(prim, o, a, n, u0, u1, -1.5, 0.9, lower - 1.2, lower + 0.35)
            _box(prim, o, a, n, u0, u1, -1.5, 0.45, lower + 0.35, lower + 0.8)
        if steps:
            _box(prim, o, a, n, -1.9, 1.9, -1.5, 0.0, lower - 1.2, lower + 0.8, top=False)
        _box(prim, o, a, n, -half, half, -1.5, 0.0, lower + 0.8, upper)
        # the sitting kerb, open where the steps come up and where the pier leaves the promenade
        gaps = []
        if steps:
            _grand(kerb, o, a, n, 3.0, lower, upper, tread=0.32, rise_t=0.17, cheeks=True)
            gaps.append((-1.9, 1.9))
        px, py = C.to_map(C.SEAWALL_S, PIER_Y)
        up = (px - o[0]) * a[0] + (py - o[1]) * a[1]
        if abs(up) < half + PIER_W / 2 + 0.5:
            gaps.append((up - PIER_W / 2 - 0.5, up + PIER_W / 2 + 0.5))
        u = -half
        for g0, g1 in sorted(gaps) + [(half, half)]:
            if g0 > u:
                _box(kerb, o, a, n, u, min(g0, half), -0.45, 0.02, upper - 0.05, upper + 0.45)
            u = max(u, g1)


PIER_Y = 905.0
PIER_W = 12.0
PIER_OUT = -140.0      # s of the pier's head
PIER_DECK = -39.5      # the promenade's level
PIER_HEAD = 26.0       # the square platform at the end for the lighthouse


def pier(mesh):
    """The pier on the town's axis: a concrete deck on pairs of piles every 7 m, a square head for the lighthouse."""
    conc = mesh.p(CONC)
    dark = mesh.p(CONC_DARK)
    x0, y0 = C.to_map(C.SEAWALL_S, PIER_Y)
    x1, y1 = C.to_map(PIER_OUT, PIER_Y)
    ax, ay = (x1 - x0), (y1 - y0)
    L = math.hypot(ax, ay)
    ax, ay = ax / L, ay / L                        # out to sea
    # frame: "along" = across the pier (north), "n" = out to sea; a box spans u across, s out
    o = (x0, y0)
    nrm = (ax, ay)
    al = (-ay, ax)
    # the deck runs 0.5 m onto the seawall's top: 1.5 cm proud of it, or the dark and the light concrete fight there
    _box(conc, o, al, nrm, -PIER_W / 2, PIER_W / 2, -0.5, L, PIER_DECK - 0.55, PIER_DECK + 0.015)
    hc = L + PIER_HEAD / 2 - 2.0
    _box(conc, o, al, nrm, -PIER_HEAD / 2, PIER_HEAD / 2, hc - PIER_HEAD / 2, hc + PIER_HEAD / 2, PIER_DECK - 0.55, PIER_DECK)
    # a low kerb along both edges (the public spaces put railings on it)
    for side in (-1, 1):
        u = side * (PIER_W / 2 - 0.15)
        _box(conc, o, al, nrm, u - 0.15, u + 0.15, 0.0, hc - PIER_HEAD / 2, PIER_DECK, PIER_DECK + 0.2)
    # piles
    s = 8.0
    while s < L:
        for side in (-1, 1):
            u = side * (PIER_W / 2 - 1.2)
            px, py = o[0] + al[0] * u + nrm[0] * s, o[1] + al[1] * u + nrm[1] * s
            bed = W.height(px, py)
            _box(dark, o, al, nrm, u - 0.35, u + 0.35, s - 0.35, s + 0.35, bed - 2.0, PIER_DECK - 0.55, top=False)
        s += 7.0
    for du in (-1, 1):
        for ds in (-1, 1):
            u, s = du * (PIER_HEAD / 2 - 1.5), hc + ds * (PIER_HEAD / 2 - 1.5)
            px, py = o[0] + al[0] * u + nrm[0] * s, o[1] + al[1] * u + nrm[1] * s
            _box(dark, o, al, nrm, u - 0.5, u + 0.5, s - 0.5, s + 0.5, W.height(px, py) - 2.0, PIER_DECK - 0.55, top=False)


CHUNK = 60.0    # metres of coast per mesh: culling, distance fields and streaming want pieces, not a 1 km mesh


class _Chunker:
    """Looks like a world_mesh.Mesh to the builders above, but sends every primitive to the mesh of the chunk
    (by map Y of the bay being drawn) it belongs to."""

    def __init__(self, name, mesh_cls):
        self.name, self.mesh_cls, self.meshes, self.cur = name, mesh_cls, {}, None

    def at(self, y, tag=""):
        k = (tag, int(math.floor((y - C.CITY_Y0 + C.CITY_FADE) / CHUNK)))
        if k not in self.meshes:
            self.meshes[k] = self.mesh_cls("%s%s_%02d" % (self.name, "_" + tag if tag else "", k[1]))
        self.cur = self.meshes[k]

    def p(self, mat):
        return self.cur.p(mat)


def build_meshes():
    """The town's meshes for world_mesh.build(): (name, fill function)."""
    return [("City_Walls", walls), ("City_Lanes", lanes), ("City_Seawall", seawall), ("City_Pier", pier)]


def build_chunked(mesh_cls):
    """[Mesh] of the town, chunked along the coast."""
    out = []
    for name, fn in build_meshes():
        ch = _Chunker(name, mesh_cls)
        ch.at(PIER_Y)
        fn(ch)
        # a mesh with no triangles (the chunker's starting chunk when nothing lands there) is a glTF mesh without
        # primitives, which Interchange refuses ("Bad MeshDescription")
        out.extend(m for _, m in sorted(ch.meshes.items()) if any(p.idx for p in m.prims.values()))
    return out


MATERIALS = {
    # name -> (rgb, roughness, texture (repo-relative or a file in world_mesh's tex_dir) or None, uv metres per tile)
    CONC: ((0.62, 0.6, 0.56), 0.92, "textures/academ/concrete_D.jpg", 2.0),
    CONC_DARK: ((0.42, 0.41, 0.39), 0.95, "textures/academ/concrete_D.jpg", 2.0),
    GLASS: ((0.05, 0.07, 0.09), 0.08, None, 1.0),
    SHOPGLASS: ((0.09, 0.12, 0.13), 0.06, None, 1.0),
    DOOR: ((0.18, 0.2, 0.22), 0.55, None, 1.0),
    "City_Door_Blue": ((0.12, 0.22, 0.38), 0.5, None, 1.0),
    "City_Door_Red": ((0.45, 0.12, 0.08), 0.5, None, 1.0),
    "City_Door_Green": ((0.14, 0.3, 0.2), 0.5, None, 1.0),
    "City_Door_Ochre": ((0.6, 0.42, 0.14), 0.5, None, 1.0),
    ASPHALT: ((0.3, 0.3, 0.3), 0.9, "textures/academ/asphalt_D.jpg", 4.0),
    TIMBER: ((0.36, 0.26, 0.17), 0.8, None, 1.0),
}


if __name__ == "__main__":
    import world_mesh
    ms = build_chunked(world_mesh.Mesh)
    world_mesh.MATERIALS.update(MATERIALS)
    print("%d triangles" % world_mesh.write_glb(sys.argv[1], ms, None))
