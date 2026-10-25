"""The ensemble's plan and facades (pure Python): three houses, two gates, the square.

Sources: OpenStreetMap footprints (ways 27830839 Novikova 1, 27830823 Novikova 3, 27830844
Maksimova 8) and the photos in docs/academgorodok/refs (facade_notes.md has the reasoning behind
every number here; `(?)` there means a guess).

Ensemble frame (metres): x = east, y = north, z = up, origin at the centre of Novikova 1's
footprint at ground level. Each house has its own frame (u along its length, v across, z up,
Frame in geom.py). Everything a house is made of comes out as
  mesh:    geom.Mesh with walls, reveals, roofs, rooms behind the windows, slabs, steps
  details: [(name, kwargs, (x, y, z), yaw)] detail meshes from details.py placed in the house frame
"""
import math
import random

from common import FLOOR_H, PLINTH_H, SILL_H, WIN_H, WIN_W
import geom
from geom import Facade, Frame, Mesh, box

WALL_T = 0.5            # reveal depth (the real brick walls are ~0.64 m)
ROOM_D = 1.6            # depth of the lit room box behind each window
GROUND_SINK = 1.0       # walls go this far under the ground so slopes never show a gap

# --- frames from OSM (OSM local metres around 55.8005 N 37.4697 E, x east, y north) -------------
N1_OSM = (2.05, 5.08)
FRAMES = {
    # name: (centre x, centre y, yaw deg of the u axis, mirror u)
    "Novikova1": (0.0, 0.0, 93.0, False),
    "Novikova3": (-12.83 - N1_OSM[0], 76.14 - N1_OSM[1], -34.6, False),
    "Maksimova8": (-5.86 - N1_OSM[0], -68.74 - N1_OSM[1], -138.2, True),
}

# Novikova 1: 60.7 x 14.5 m, 4 floors; u = north, v+ = west (the yard), v- = east (the square)
N1_L, N1_D, N1_FLOORS = 60.7, 14.5, 4
# Side houses (mirror twins): 63.3 x 19.8 m, 3 floors; u+ = the gate end, v+ = the street,
# v- = the courtyard where the middle 35.5 m is set back 4.3 m between two end wings
S_L, S_D, S_FLOORS = 63.3, 19.8, 3
S_RECESS, S_RECESS_HALF = 4.3, 17.75

N1_PITCH = math.radians(22.0)
S_PITCH = math.radians(27.0)


def floor_z(k):
    """Floor level of storey k (0 = ground floor) over the ground."""
    return PLINTH_H + k * FLOOR_H


def eaves_z(floors):
    return floor_z(floors) + 0.35


class House:
    def __init__(self, name, frame):
        self.name = name
        self.frame = frame
        self.mesh = Mesh()
        self.inside = Mesh()         # what is only seen from close by or from within (a separate, near-only mesh)
        self.details = []
        self.rnd = random.Random(name)
        self.facades = []            # (Facade, [Opening], wall top) for interiors.py
        self.interiors = True        # walkable inside: interiors.py builds the stairs and flats
        self.stairs = []             # stair axes (house x)

    # facade-local helpers -----------------------------------------------------------------
    def detail(self, fac, name, u, y, z, at=None, **kw):
        if fac is None:
            self.details.append((name, kw, at, 0.0))
            return
        x, yy, zz = fac.to_building((u, y, z))
        self.details.append((name, kw, (x, yy, zz), fac.detail_yaw()))

    def add(self, fac, m):
        self.mesh.extend(fac.apply(m))


class Opening:
    def __init__(self, uc, z0, w, h, kind="window", unit="double", arch=False, surround="band",
                 room=True, frame_y=-0.12, fill=True):
        self.uc, self.z0, self.w, self.h = uc, z0, w, h
        self.fill = fill            # False: an open doorway (no leaves)
        self.kind, self.unit, self.arch, self.surround, self.room = kind, unit, arch, surround, room
        self.frame_y = frame_y

    @property
    def rect(self):
        return (self.uc - self.w / 2, self.uc + self.w / 2, self.z0, self.z0 + self.h)


def build_facade(house, fac, z_top, openings, extra=None, wall_mat="AC_Wall", plinth=True):
    """Wall with holes, reveals, the rooms behind, window/door units and their surrounds."""
    m = Mesh()
    holes = [o.rect for o in openings]
    geom.wall_with_holes(m, wall_mat, fac.length, -GROUND_SINK, z_top, holes)
    for o in openings:
        u0, u1, z0, z1 = o.rect
        geom.reveal(m, "AC_Wall", u0, u1, z0, z1, WALL_T, arch=o.arch)
        if o.arch:
            pass  # the hole is square; the unit and its surround fill the spandrels (see below)
        if o.room and o.kind == "window" and not house.interiors:
            _room(m, house.rnd, u0, u1, z0, z1, fac.length)
        if o.kind == "door":
            # a real door (ALyudDoor) instead of modelled leaves: shut, swinging out of the house
            import interiors
            x_, y_, _ = fac.to_building((o.uc, -WALL_T / 2, 0.0))
            door_h = o.h
            if o.arch:
                # ALyudDoor is rectangular: it fills the hole up to the springing, a fixed glazed
                # fanlight fills the half round above it
                r = o.w / 2
                door_h = o.h - r
                zs = z0 + door_h
                fan = [(o.uc + r * math.cos(math.pi * k / 12), zs + r * math.sin(math.pi * k / 12)) for k in range(13)]
                geom.extrude_profile(m, "AC_Glass", "AC_Frame", fan, 0.24, 0.25, lambda u, v, t: (u, -t, v))
                box(m, "AC_Frame", u0, u1, -0.28, -0.2, zs - 0.03, zs + 0.03)
                for a in (math.pi / 3, 2 * math.pi / 3):
                    for k in range(6):
                        rr = r * (k + 0.5) / 6
                        cx, cz = o.uc + rr * math.cos(a), zs + rr * math.sin(a)
                        box(m, "AC_Frame", cx - 0.02, cx + 0.02, -0.27, -0.21, cz - 0.04, cz + 0.04)
            locked = house.name == "Novikova1" and -10.4 < x_ < -2.5 and z0 < 1.0      # the shut-off rooms by the library
            interiors.add_door(house, (x_, y_, max(z0, 0.0)), fac.n, o.w, door_h, WALL_T, 2 if o.w >= 1.3 else 1,
                               not o.fill and not locked, locked)
        elif o.fill:
            house.detail(fac, o.kind, o.uc, 0.0, z0, w=o.w, h=o.h, unit=o.unit)
        if o.surround:
            house.detail(fac, "surround", o.uc, 0.0, z0, w=o.w, h=o.h, style=o.surround)
        if o.arch:
            geom.arch_spandrels(m, wall_mat, u0, u1, z1)
            geom.reveal(m, "AC_Wall", u0, u1, z0, z1, WALL_T, arch=True)
    if plinth:
        # plinth band: projects 4 cm, from below ground to the ground-floor level
        segs = _split_by_doors(fac.length, [o for o in openings if o.z0 < PLINTH_H + 0.01])
        for a, b in segs:
            box(m, "AC_Plinth", a, b, 0.0, 0.04, -GROUND_SINK, PLINTH_H - 0.05)
    if extra:
        extra(m)
    house.add(fac, m)
    house.facades.append((fac, openings, z_top))


def _split_by_doors(length, doors):
    cuts = sorted((o.rect[0], o.rect[1]) for o in doors)
    segs, a = [], 0.0
    for c0, c1 in cuts:
        if c0 > a:
            segs.append((a, c0))
        a = max(a, c1)
    if a < length:
        segs.append((a, length))
    return segs


def _room(m, rnd, u0, u1, z0, z1, length):
    """A shallow lit room behind a window so it never reads as a black hole: side, top and bottom
    reveals continue into a box with a back wall; some windows get a curtain."""
    y0, y1 = WALL_T, WALL_T + ROOM_D
    pad = 0.6
    a, b = min(u0 - 0.02, max(u0 - pad, 0.6)), max(u1 + 0.02, min(u1 + pad, length - 0.6))
    c, d = z0 - SILL_H + 0.05, z1 + 0.4
    # (local frame: +y is outward; the room lies at -y)
    box(m, "AC_Room", a, b, -y1, -y0, c, d, faces="xXzZY")
    m.face("AC_Room", [(a, -y0, c), (b, -y0, c), (b, -y0, z0), (a, -y0, z0)])       # inner wall below sill
    if rnd.random() < 0.55:
        side = rnd.choice((-1, 1))
        cw = (u1 - u0) * rnd.uniform(0.25, 0.45)
        cu0, cu1 = (u0 - 0.1, u0 + cw) if side < 0 else (u1 - cw, u1 + 0.1)
        m.face("AC_Curtain", [(cu1, -y0 - 0.08, z0 - 0.3), (cu0, -y0 - 0.08, z0 - 0.3),
                              (cu0, -y0 - 0.08, z1 + 0.15), (cu1, -y0 - 0.08, z1 + 0.15)])
    if rnd.random() < 0.3:
        tu = rnd.uniform(u0, u1)          # a lamp shade or a cupboard silhouette deep in the room
        box(m, "AC_Furniture", tu - 0.4, tu + 0.4, -y1 + 0.02, -y1 + 0.45, c, c + rnd.uniform(1.2, 1.9))


def axes(start, end, n):
    """n evenly spaced window axes between start and end (centres)."""
    if n <= 0:
        return []
    s = (end - start) / n
    return [start + s * (i + 0.5) for i in range(n)]


def windows_on_axes(us, floors, unit="double", surrounds=None, skip=()):
    out = []
    for k in range(floors):
        for u in us:
            if (k, round(u, 2)) in skip:
                continue
            sur = (surrounds or {}).get(k, "band")
            out.append(Opening(u, floor_z(k) + SILL_H, WIN_W, WIN_H, unit=unit, surround=sur))
    return out


# ============================================================================================
# Novikova 1
# ============================================================================================

# Square facade, measured on commons_Marshal_Novikov_str._1_1_.jpg (the panorama) in units of the
# window step and scaled to the 60.7 m footprint: 4 windows between the bays, 2 beyond the side bays.
N1_BAYS = (-20.85, 0.0, 20.85)     # loggia bays on the square (east) facade, u of the bay axis
N1_BAY_W = 5.4
N1_WIN_STEP = 4.05
N1_BAY_GAP = 1.65                  # bay edge to the next window's axis
GASTRONOM_X = (-2.5, 18.2)         # the shop's ground floor (house x), gastronom.py
STAIR_WIN_Z = FLOOR_H / 2 + 0.8     # stair window sill over its storey's floor: 0.8 m over the half landing
SIDE_STAIRS = (-20.7, 0.0, 20.7)    # side houses: stair axes behind the street portals


def novikova1():
    h = House("Novikova1", Frame(*FRAMES["Novikova1"][:2], math.radians(FRAMES["Novikova1"][2])))
    L, D, F = N1_L, N1_D, N1_FLOORS
    x0, x1, y0, y1 = -L / 2, L / 2, -D / 2, D / 2
    ez = eaves_z(F)

    # --- east facade (the square): walked from south to north at v = -D/2 -------------------
    fac = Facade((x0, y0), (x1, y0))
    ops = []
    cu = lambda u: u - x0           # building u -> facade u
    for b in N1_BAYS:
        half = N1_BAY_W / 2
        # windows: 3 between the bays, 2 beyond the side bays (facade_notes.md: 3 (?4))
        ops += _n1_bay_openings(cu(b))
    win_us = []
    for b0, b1 in zip(N1_BAYS, N1_BAYS[1:]):
        s = b0 + N1_BAY_W / 2 + N1_BAY_GAP
        win_us += [s + i * N1_WIN_STEP for i in range(4)]
    for side in (-1, 1):
        s = side * (N1_BAYS[2] + N1_BAY_W / 2 + N1_BAY_GAP)
        win_us += [s + side * i * N1_WIN_STEP for i in range(2)]
    win_us = sorted(cu(u) for u in win_us)
    for k in range(1, F):
        for u in win_us:
            ops.append(Opening(u, floor_z(k) + SILL_H, WIN_W, WIN_H))
    # ground floor: a double door next to each bay towards the middle, shop windows elsewhere
    door_us = {cu(N1_BAYS[1] - N1_BAY_W / 2 - N1_BAY_GAP), cu(N1_BAYS[1] + N1_BAY_W / 2 + N1_BAY_GAP)}
    for u in win_us:
        if any(abs(u - d) < 0.1 for d in door_us):
            ops.append(Opening(u, floor_z(0) - 0.15, 1.5, 2.65, kind="door", unit="double", room=False, surround=None))
        else:
            ops.append(Opening(u, floor_z(0) + 0.55, 2.0, 2.25, unit="shop", surround=None))
    # the Gastronom (gastronom.py) fills the ground floor from the central entrance to the north bay:
    # no room boxes behind its windows, the central entrance stands open
    for o in ops:
        bx = o.uc + x0
        if o.z0 < floor_z(1) and GASTRONOM_X[0] < bx < GASTRONOM_X[1]:
            o.room = False
            if o.kind == "door" and abs(bx) < 0.1:
                o.fill = False
    h.stairs = list(N1_BAYS)
    ground = [o for o in ops if o.z0 < floor_z(1) and (o.unit == "shop" or (o.kind == "door" and o.surround is None
                                                                            and o.w < 1.55))]
    lib_door = min((o.uc for o in ground if o.kind == "door"), key=lambda u: abs(u + x0 - 4.35))
    build_facade(h, fac, ez, ops, extra=lambda m: _n1_ground_details(m, ground, lib_door))
    for b in N1_BAYS:
        _n1_bay_details(h, fac, cu(b), ez)
    # the shop of old is a library now (Dima, 2026-10-25): library.py, the Gastronom as a fallback
    try:
        import library as shop
    except ImportError:
        import gastronom as shop
    h.inside.extend(shop.build())
    h.lights = list(shop.LIGHTS)
    # its boards: a picture per language on a quad over each label (AcademToggle.cpp, make_academ_signs.py)
    for (key, lx, ly, lz, lw, lh), nrm in zip(getattr(shop, "LABELS", []), getattr(shop, "LABEL_NORMALS", [])):
        if not key.startswith("sign.academ.library."):
            continue
        nx, ny = nrm[0], nrm[1]
        tx, ty = -ny, nx                        # along the board, to the viewer's right when facing it
        c = (lx + nx * 0.006, ly + ny * 0.006)
        pts = [(c[0] - tx * lw / 2, c[1] - ty * lw / 2, lz - lh / 2), (c[0] + tx * lw / 2, c[1] + ty * lw / 2, lz - lh / 2),
               (c[0] + tx * lw / 2, c[1] + ty * lw / 2, lz + lh / 2), (c[0] - tx * lw / 2, c[1] - ty * lw / 2, lz + lh / 2)]
        h.inside.face("AC_Sign_" + key.split(".")[-1], pts, [(0, 0), (1, 0), (1, 1), (0, 1)])
    for u in win_us:
        if not any(abs(u - d) < 0.1 for d in door_us):
            continue
        h.detail(fac, "steps", u, 0.0, 0.0, w=2.2, n=2, rise=(floor_z(0) - 0.15) / 2, tread=0.35)

    # --- north end (u = +L/2): walked from east to west ----------------------------------------
    _n1_end(h, Facade((x1, y0), (x1, y1)), ez)
    # --- west facade (yard): walked from north to south --------------------------------------
    fac = Facade((x1, y1), (x0, y1))
    ops = []
    entr = [cu(-u) for u in N1_BAYS]               # the stairs sit behind the bays (?)
    for e in entr:
        ops.append(Opening(e, floor_z(0) - 0.15, 1.4, 2.6, kind="door", unit="double", room=False, fill=False))
        for k in range(F - 1):                     # stair windows over the half landings
            ops.append(Opening(e, floor_z(k) + STAIR_WIN_Z, 1.9, 1.7, unit="triple"))
    wus = []
    for e0, e1 in zip([0.0] + entr, entr + [L]):
        a, b = (e0 + 2.0 if e0 else 1.4), (e1 - 2.0 if e1 < L else L - 1.4)
        n = max(1, round((b - a) / N1_WIN_STEP))
        wus += axes(a, b, n)
    for k in range(F):
        for u in wus:
            ops.append(Opening(u, floor_z(k) + SILL_H, WIN_W, WIN_H))
    build_facade(h, fac, ez, ops)
    for e in entr:
        h.detail(fac, "oculus", e, 0.0, ez - 1.25, d=0.75)
        h.detail(fac, "steps", e, 0.0, 0.0, w=2.0, n=2, rise=(floor_z(0) - 0.15) / 2, tread=0.35)
    for k in (1, 2, 3):                             # balconies on the yard side (?)
        for u in wus[1::3]:
            h.detail(fac, "balcony", u, 0.0, floor_z(k), length=2.6, depth=1.0, style="bars")
    # --- south end: walked from west to east ----------------------------------------------------
    _n1_end(h, Facade((x0, y1), (x0, y0)), ez)

    # --- roof with three gables over the square side -----------------------------------------------
    over = 0.95
    roof = Mesh()
    zr = geom.hip_roof(roof, "AC_Roof", "AC_Trim", "AC_Trim", x0, x1, y0, y1, ez, N1_PITCH, over)
    h.mesh.extend(roof)
    h.notches = [(b - N1_BAY_W / 2, b + N1_BAY_W / 2, y0 - over - 0.1, y0 + 0.02) for b in N1_BAYS]
    for b in N1_BAYS:
        _gable(h, b, y0, ez, N1_BAY_W, 3.0, back=D / 2)
    # modillions under the eaves, broken at the bays
    for side, (pa, pb) in enumerate((((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0)))):
        fac = Facade(pa, pb)
        for u in _rows(0.4, fac.length - 0.4, 0.62):
            bu = fac.to_building((u, 0, 0))
            if side == 0 and any(abs(bu[0] - b) < N1_BAY_W / 2 + 0.1 for b in N1_BAYS):
                continue
            h.detail(fac, "modillion", u, 0.0, ez, depth=over - 0.05)
    # chimneys: pairs either side of each gable and over the yard side
    for cx in (-24.0, -9.6, 9.6, 24.0):
        for cy in (-1.5, 2.5):
            z = geom.roof_z(cx, cy, x0, x1, y0, y1, ez, N1_PITCH, over)
            h.detail(None, "chimney", 0, 0, 0, at=(cx, cy, z - 0.3), h=2.2)
    # downpipes at the bay edges and the corners
    for b in N1_BAYS:
        for s in (-1, 1):
            h.detail(Facade((x0, y0), (x1, y0)), "downpipe", cu(b + s * (N1_BAY_W / 2 + 0.15)), 0.12, 0.0, h=ez)
    h.roof_top = zr
    return h


def _n1_bay_openings(bu):
    """Openings of one loggia bay (facade u of its axis): the portal door (open: the stair hall or
    the Gastronom behind it), two loggia doors, the Palladian window on the top floor."""
    ops = [Opening(bu, floor_z(0) - 0.15, 1.6, 2.6, kind="door", unit="double", room=False, surround=None, fill=False)]
    for k in (1, 2):
        ops.append(Opening(bu, floor_z(k) + 0.05, 2.4, 2.75, unit="french_door", surround=None))
    ops.append(Opening(bu, floor_z(3) + 0.05, 2.6, 2.95, unit="palladian", surround=None))
    return ops


def _n1_ground_details(m, ground, sign_door_u):
    """The square facade's ground floor (Dima's photo docs/academgorodok/refs/dima/shop.jpg): wide white
    architraves round the doors and the big windows, a light well with a grate in front of each window,
    a globe lamp over each door, and the library's enamel sign beside its door (facade-local frame)."""
    for o in ground:
        u0, u1, z0, z1 = o.rect
        bw = 0.3
        box(m, "AC_Trim", u0 - bw, u0, 0.0, 0.05, max(z0 - (0.12 if o.kind != "door" else 0.0), PLINTH_H - 0.3), z1 + bw)
        box(m, "AC_Trim", u1, u1 + bw, 0.0, 0.05, max(z0 - (0.12 if o.kind != "door" else 0.0), PLINTH_H - 0.3), z1 + bw)
        box(m, "AC_Trim", u0 - bw, u1 + bw, 0.0, 0.05, z1, z1 + bw)
        if o.kind != "door":
            box(m, "AC_Trim", u0 - bw, u1 + bw, 0.0, 0.05, z0 - 0.12 - bw * 0.4, z0 - 0.12)
            box(m, "AC_FrameGreen", u0, u1, 0.0, 0.12, z0 - 0.04, z0)                  # green-painted sill
            # light well: a low concrete box with a grate, out from the wall
            box(m, "AC_Concrete", u0 - 0.25, u1 + 0.25, 0.0, 0.9, -0.2, 0.3, faces="xXyYZ")
            for i in range(int((u1 - u0 + 0.3) / 0.06)):
                gx = u0 - 0.15 + i * 0.06
                box(m, "AC_Iron", gx, gx + 0.012, 0.06, 0.84, 0.3, 0.32)
        else:
            # a globe lamp on a bracket over the door
            cu_ = u0 - 0.55
            box(m, "AC_Iron", cu_ - 0.02, cu_ + 0.02, 0.0, 0.22, z1 + 0.5, z1 + 0.54)
            box(m, "AC_LampGlobe", cu_ - 0.12, cu_ + 0.12, 0.1, 0.34, z1 + 0.3, z1 + 0.54)
    for o in ground:
        if o.kind == "door" and abs(o.uc - sign_door_u) < 0.01:
            u1 = o.rect[1] + 0.3 + 0.35
            box(m, "AC_Trim", u1, u1 + 0.82, 0.0, 0.015, PLINTH_H + 1.55, PLINTH_H + 2.12)
            # the enamel board: its picture comes per language (sign.academ.library, AcademToggle.cpp)
            m.face("AC_Sign_library", [(u1 + 0.81, 0.02, PLINTH_H + 1.56), (u1 + 0.01, 0.02, PLINTH_H + 1.56),
                                      (u1 + 0.01, 0.02, PLINTH_H + 2.11), (u1 + 0.81, 0.02, PLINTH_H + 2.11)],
                   [(1, 0), (0, 0), (0, 1), (1, 1)])


def _n1_bay_details(h, fac, bu, ez):
    """The loggia stack in front of a bay: dark portal, four consoles, two loggias on columns with
    a solid parapet below and bars above, the top balcony; facade_notes.md section 2."""
    w = N1_BAY_W
    h.detail(fac, "portal_dark", bu, 0.0, floor_z(0) - 0.15, w=1.6, h=2.6)
    h.detail(fac, "steps", bu, 0.0, 0.0, w=2.6, n=2, rise=(floor_z(0) - 0.15) / 2, tread=0.35)
    for i in range(4):
        h.detail(fac, "console_big", bu - 1.95 + i * 1.3, 0.0, floor_z(1) - 0.25, h=1.2)
    depth = 1.0
    for k in (1, 2, 3):                                   # slabs at the loggia floors
        h.detail(fac, "slab", bu, 0.0, floor_z(k) - 0.25, w=w - 0.2, depth=depth + 0.1, t=0.25)
    for k in (1, 2):
        for s in (-1, 1):
            h.detail(fac, "pilaster", bu + s * (w / 2 - 0.35), depth - 0.25, floor_z(k), h=FLOOR_H - 0.25, w=0.5)
            h.detail(fac, "column", bu + s * 0.9, depth - 0.2, floor_z(k), h=FLOOR_H - 0.25, d=0.26)
    h.detail(fac, "parapet", bu, depth - 0.12, floor_z(1), length=w - 0.6, h=1.0)
    h.detail(fac, "railing", bu, depth - 0.12, floor_z(2), length=w - 0.6, h=1.0, style="bars")
    h.detail(fac, "railing", bu, depth - 0.12, floor_z(3), length=w - 0.6, h=1.0, style="bars")
    for s in (-1, 1):                                     # side returns of the loggias
        for k in (1, 2):
            h.detail(fac, "railing_side", bu + s * (w / 2 - 0.15), 0.0, floor_z(k), length=depth, h=1.0)
    # over the top floor's window a lunette under a white archivolt, and piers at the top balcony's
    # corners (docs/academgorodok/refs/dima/bay.jpg)
    lm = Mesh()
    r0, r1 = 0.95, 1.13
    zc = floor_z(3) + 0.05 + 2.95 - 0.25
    pts = []
    for k in range(13):
        a = math.pi * k / 12
        pts.append((bu + r1 * math.cos(a), zc + r1 * math.sin(a)))
    for k in range(12, -1, -1):
        a = math.pi * k / 12
        pts.append((bu + r0 * math.cos(a), zc + r0 * math.sin(a)))
    geom.extrude_profile(lm, "AC_Trim", "AC_Trim", pts, -0.06, 0.0, lambda u, v, t: (u, -t, v))
    for sgn in (-1, 1):
        box(lm, "AC_Trim", bu + sgn * (w / 2 - 0.3) - 0.14, bu + sgn * (w / 2 - 0.3) + 0.14, depth - 0.26, depth,
            floor_z(3), floor_z(3) + 1.12)
    h.add(fac, lm)
    h.detail(fac, "oculus", bu, 0.0, ez + 0.9, d=0.95)
    for s in (-1, 1):
        h.detail(fac, "swag", bu + s * 1.55, 0.0, ez + 0.75, length=1.3)


def _n1_end(h, fac, ez):
    """Short end (14.5 m): an arched side door at the square corner, a two-storey polygonal bay
    window at the yard corner with a balcony on top, 2-3 windows a floor and small vents."""
    L = fac.length
    ops = []
    # walked east -> west on the north end, west -> east on the south end: 'square' end differs
    square_first = fac.p0[1] < fac.p1[1]
    def at(t):            # t measured from the square corner
        return t if square_first else L - t
    ops.append(Opening(at(1.9), floor_z(0) - 0.15, 1.2, 2.7, kind="door", unit="arched", arch=True,
                       room=False, surround="arch"))
    for k in range(4):
        ops.append(Opening(at(5.2), floor_z(k) + SILL_H, 1.3, WIN_H))
    for k in (2, 3):
        ops.append(Opening(at(9.4), floor_z(k) + SILL_H, 1.3, WIN_H))
        ops.append(Opening(at(12.3), floor_z(k) + SILL_H, 1.3, WIN_H))
    for k in (0, 1):     # the bay window: wide glazed openings behind the polygonal bay
        ops.append(Opening(at(10.9), floor_z(k) + 0.8, 3.6, 2.1, unit="triple", surround=None))
    build_facade(h, fac, ez, ops)
    h.detail(fac, "bay_window", at(10.9), 0.0, PLINTH_H - 0.1, w=4.2, depth=1.1, h=2 * FLOOR_H + 0.4)
    h.detail(fac, "railing", at(10.9), 1.1, floor_z(2), length=4.0, h=1.0, style="bars")
    for k in range(1, 4):
        h.detail(fac, "vent", at(7.2), 0.0, floor_z(k) + 1.6, s=0.3)


def _gable(h, u, wall_v, ez, w, rise, back):
    """A gable rising from the wall plane over a bay (house frame: u along x, wall at v=wall_v,
    outside at -v): the triangle wall, a white raking cornice and its little roof running back
    into the main roof."""
    m = Mesh()
    a, b = u - w / 2, u + w / 2
    top = ez + rise
    # wall triangle (and the strip of wall up to the eaves line that the notch exposes)
    m.face("AC_Wall", [(b, wall_v, ez - 0.4), (a, wall_v, ez - 0.4), (a, wall_v, ez + 0.25), (u, wall_v, top), (b, wall_v, ez + 0.25)])
    over, t = 0.35, 0.22
    for s in (-1, 1):                      # raking cornice + roof slopes
        e = (u + s * (w / 2 + over), ez + 0.25 - over * rise / (w / 2))
        r = (u, top)
        y_front, y_back = wall_v - over, wall_v + back
        m.face("AC_Roof", [(e[0], y_front, e[1] + t), (e[0], y_back, e[1] + t), (r[0], y_back, r[1] + t), (r[0], y_front, r[1] + t)][::(1 if s < 0 else -1)])
        m.face("AC_Trim", [(e[0], y_front, e[1]), (r[0], y_front, r[1]), (r[0], y_front, r[1] + t), (e[0], y_front, e[1] + t)][::(-1 if s < 0 else 1)])
        m.face("AC_Trim", [(e[0], y_front, e[1]), (r[0], y_front, r[1]), (r[0], y_back, r[1]), (e[0], y_back, e[1])][::(1 if s < 0 else -1)])
    # (the faces above are listed clockwise: flip them to face out)
    m.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in m.faces]
    h.mesh.extend(m)


def _rows(a, b, step):
    n = max(1, int((b - a) / step))
    s = (b - a) / n
    return [a + s * (i + 0.5) for i in range(n)]


# ============================================================================================
# Side houses (Novikova 3; Maksimova 8 is its mirror twin)
# ============================================================================================

def side_house(name):
    cx, cy, yaw, mirror = FRAMES[name]
    h = House(name, Frame(cx, cy, math.radians(yaw), mirror=mirror))
    L, D, F = S_L, S_D, S_FLOORS
    x0, x1, y0, y1 = -L / 2, L / 2, -D / 2, D / 2
    r, rh = S_RECESS, S_RECESS_HALF
    ez = eaves_z(F)
    # footprint, counter-clockwise: courtyard side (y0) with the recess, gate end (x1), street (y1)
    poly = [(x0, y0), (-rh, y0), (-rh, y0 + r), (rh, y0 + r), (rh, y0), (x1, y0), (x1, y1), (x0, y1)]

    # --- street facade (y1), walked from the gate end (x1) to the far end (x0) -------------------
    fac = Facade((x1, y1), (x0, y1))
    Lf = fac.length
    ops, plan = [], []
    # From the gate corner: pier, 2 windows, portal, 5, portal (middle), 5, portal, 2, pier. The
    # 1946 street elevation (refs/interior/m24_2015_houses_06.jpg) and the 2025 photo show the
    # portals: an arched door in rustication, a balcony on consoles above it, a pedimented window
    # over that and a small gable on the roof; ground-floor windows with hoods on brackets.
    pier = 2.3
    seq = ["w", "w", "P", "w", "w", "w", "w", "w", "P", "w", "w", "w", "w", "w", "P", "w", "w"]
    widths = {"w": 3.35, "P": 4.2}
    total = sum(widths[s] for s in seq)
    scale = (Lf - 2 * pier) / total
    u = pier
    for s in seq:
        wdt = widths[s] * scale
        plan.append((s, u + wdt / 2))
        u += wdt
    hood = {0: "hood_ears", 1: "hood", 2: "band"}
    portal_us = [uc for s, uc in plan if s == "P"]
    for s, uc in plan:
        if s == "w":
            near = any(abs(uc - p) < 4.0 for p in portal_us)
            for k in range(F):
                sur = hood[k] if (k == 0 or near) else "band"
                ops.append(Opening(uc, floor_z(k) + SILL_H, 1.35, 1.85, surround=sur))
        else:
            ops.append(Opening(uc, floor_z(0) - 0.15, 1.3, 2.8, kind="door", unit="arched", arch=True,
                               room=False, surround=None, fill=False))
            ops.append(Opening(uc, floor_z(1) + 0.05, 1.3, 2.6, unit="french_door", surround="pediment"))
            ops.append(Opening(uc, floor_z(2) + SILL_H, 1.35, 1.85, surround="ears"))
    build_facade(h, fac, ez, ops)
    for s, uc in plan:
        if s == "P":
            h.detail(fac, "portal_side", uc, 0.0, floor_z(0) - 0.15, w=1.3, h=2.6)
            h.detail(fac, "steps", uc, 0.0, 0.0, w=2.2, n=2, rise=(floor_z(0) - 0.15) / 2, tread=0.35)
            for i in range(2):
                h.detail(fac, "console_big", uc - 1.15 + i * 2.3, 0.0, floor_z(1) - 0.25, h=1.2)
            h.detail(fac, "slab", uc, 0.0, floor_z(1) - 0.25, w=3.0, depth=0.9, t=0.25)
            h.detail(fac, "railing", uc, 0.85, floor_z(1), length=2.9, h=1.0, style="ornamental")
    # quoins and the 1947 cartouche at the gate-end corner of the street facade (u = 0 here)
    h.detail(fac, "quoin", 0.0, 0.0, PLINTH_H - 0.1, courses=11, course_h=0.5, corner=(x1, y1))
    h.detail(fac, "cartouche", 0.0, 0.0, PLINTH_H - 0.1 + 11 * 0.5, corner=(x1, y1))

    # --- gate end (x1), walked from the courtyard (y0) to the street (y1) --------------------------
    fac = Facade((x1, y0), (x1, y1))
    ops = []
    ends = [3.0, 7.6, 12.2, 16.6]
    for k in range(F):
        for i, u in enumerate(ends):
            ops.append(Opening(u, floor_z(k) + SILL_H, 1.35, 1.85, surround=hood[k] if k < 2 else "band"))
    build_facade(h, fac, ez, ops)
    h.gate_corner = fac.to_building((0.0, 0.0, 0.0))     # where the wicket block attaches

    # --- far end (x0), walked from the street (y1) to the courtyard (y0) ---------------------------
    fac = Facade((x0, y1), (x0, y0))
    ops = [Opening(3.0, floor_z(0) + 0.5, 1.8, 2.6, unit="arched", arch=True, surround="arch")]
    for k in (1, 2):
        ops.append(Opening(3.0, floor_z(k) + SILL_H, 1.35, 1.85))
    for u in (7.7, 12.4, 16.8):
        for k in range(F):
            ops.append(Opening(u, floor_z(k) + SILL_H, 1.35, 1.85))
    build_facade(h, fac, ez, ops)
    h.detail(fac, "bay_window", 7.7, 0.0, floor_z(1) - 0.3, w=3.4, depth=1.0, h=FLOOR_H + 0.3)
    h.detail(fac, "railing", 7.7, 1.0, floor_z(2), length=3.2, h=1.0, style="bars")

    # --- courtyard side (y0 with the recess), walked from the far end to the gate end ---------------
    seg = [((x0, y0), (-rh, y0)), ((-rh, y0), (-rh, y0 + r)), ((-rh, y0 + r), (rh, y0 + r)),
           ((rh, y0 + r), (rh, y0)), ((rh, y0), (x1, y0))]
    for i, (pa, pb) in enumerate(seg):
        fac = Facade(pa, pb)
        Lf = fac.length
        ops = []
        balc = []
        if i in (0, 4):                    # wing ends facing the courtyard; the axis next to the stair is its entrance
            us = axes(1.0, Lf - 1.0, 3)
            su = min(us, key=lambda u: min(abs(fac.to_building((u, 0, 0))[0] - sx) for sx in SIDE_STAIRS))
            ops += windows_on_axes([u for u in us if u != su], F)
            # the wing's stair stops 1.6 m past its flights (interiors.py): its yard door, and ordinary
            # windows above it (a store room behind them)
            ops += _stair_entrance(su, 1)
            ops += [o for o in windows_on_axes([su], F) if o.z0 > floor_z(1) - 0.5]
        elif i in (1, 3):                  # wing side faces: paired windows with a colonnette
            for k in range(F):
                ops.append(Opening(Lf / 2, floor_z(k) + SILL_H, 2.3, 1.85, unit="paired"))
        else:                              # the recess: the middle entrance, windows, two balconies
            ops += _stair_entrance(Lf / 2, F)
            for a0, b0 in ((1.2, Lf / 2 - 1.8), (Lf / 2 + 1.8, Lf - 1.2)):
                us = axes(a0, b0, max(1, round((b0 - a0) / 3.6)))
                mid = us[len(us) // 2]
                balc.append(mid)
                ops += windows_on_axes([u for u in us if u != mid], F)
                ops.append(Opening(mid, floor_z(0) + SILL_H, WIN_W, WIN_H))
                for k in (1, 2):
                    ops.append(Opening(mid, floor_z(k) + 0.05, 1.3, 2.6, unit="french_door"))
        build_facade(h, fac, ez, ops)
        for o in ops:
            if o.kind == "door":
                h.detail(fac, "steps", o.uc, 0.0, 0.0, w=2.0, n=2, rise=(floor_z(0) - 0.15) / 2, tread=0.35)
        for mid in balc:
            for k in (1, 2):
                h.detail(fac, "balcony", mid, 0.0, floor_z(k), length=3.2, depth=1.2, style="ornamental", consoles=True)
    h.stairs = list(SIDE_STAIRS)

    # --- roof: the main hip over the full-depth part, the wings' own hips into it -------------------
    over = 1.1
    roof = Mesh()
    zr = geom.hip_roof(roof, "AC_Roof", "AC_Timber", "AC_Timber", x0, x1, y0 + r, y1, ez, S_PITCH, over)
    for wx0, wx1 in ((x0, -rh), (rh, x1)):
        geom.hip_roof(roof, "AC_Roof", "AC_Timber", "AC_Timber", wx0, wx1, y0, y0 + r + 4.0, ez, S_PITCH, over)
    h.mesh.extend(roof)
    h.notches = []
    # the gabled bay on the street side
    for s, uc in plan:
        if s == "P":
            bx = Facade((x1, y1), (x0, y1)).to_building((uc, 0, 0))[0]
            _gable_side(h, bx, y1, ez, 3.6, 1.7, 3.5)
            h.notches.append((bx - 1.8, bx + 1.8, y1 - 0.02, y1 + over + 0.1))
    # rafter tails all round
    for pa, pb in (((x0, y1 - 0.0), (x0, y0)),):
        pass
    outline = [(x0, y0), (-rh, y0), (-rh, y0 + r), (rh, y0 + r), (rh, y0), (x1, y0), (x1, y1), (x0, y1)]
    for pa, pb in zip(outline, outline[1:] + outline[:1]):
        fac = Facade(pa, pb)
        if fac.length < 5.0:
            continue                      # the short recess returns are under the wings' eaves
        for u in _rows(0.6, fac.length - 0.6, 1.1):
            bp = fac.to_building((u, 0, 0))
            if any(n[0] - 0.1 < bp[0] < n[1] + 0.1 and abs(bp[1] - y1) < 0.1 for n in h.notches):
                continue
            h.detail(fac, "rafter", u, 0.0, ez, length=over)
    for cx in (-22.0, -8.0, 8.0, 22.0):
        z = geom.roof_z(cx, 3.0, x0, x1, y0 + r, y1, ez, S_PITCH, over)
        h.detail(None, "chimney", 0, 0, 0, at=(cx, 3.0, z - 0.3), h=2.0)
    for u in (1.1, S_L - 0.6):          # beside the quoin (commons_Maksimova_str._8_1_.jpg)
        h.detail(Facade((x1, y1), (x0, y1)), "downpipe", u, 0.12, 0.0, h=ez)
    h.roof_top = zr
    return h


def _stair_entrance(u, F):
    """A yard entrance (open) with the stair's windows over the half landings above it."""
    ops = [Opening(u, floor_z(0) - 0.15, 1.4, 2.5, kind="door", unit="double", room=False, fill=False)]
    for k in range(F - 1):
        ops.append(Opening(u, floor_z(k) + STAIR_WIN_Z, 1.7, 1.6, unit="triple"))
    return ops


def _gable_side(h, u, wall_v, ez, w, rise, back):
    """Gable over the street-side loggia bay of a side house (outside at +v here)."""
    m = Mesh()
    _gable(_Proxy(m), -u, -wall_v, ez, w, rise, back)
    # mirror v back: (x, y) -> (-x, -y) is a rotation, winding stays
    h.mesh.extend(m.transformed(lambda p: (-p[0], -p[1], p[2])))
    fac = Facade((u + w, wall_v), (u - w, wall_v))
    for sgn in (-1, 1):
        h.detail(fac, "swag", w + sgn * 0.75, 0.0, ez + 0.55, length=0.9)


class _Proxy:
    def __init__(self, m):
        self.mesh = m


# ============================================================================================
# Gates and the square
# ============================================================================================

# A gate (Dima's photos docs/academgorodok/refs/dima/arch1.jpg, arch2.jpg): on each side of the drive an
# ochre block against the house, with an arched passage for people under a white archivolt, a white band
# and cornice and a low hipped cap; from each block a scrolled wing (ochre, its S-curved edge white)
# steps down towards the drive; a concrete plinth under both.
GATE_BLOCK_W, GATE_BLOCK_D, GATE_BLOCK_H = 3.0, 1.1, 5.0
GATE_ARCH_W, GATE_ARCH_H = 1.5, 3.1
GATE_WING = [(0.0, 3.45), (0.25, 3.52), (0.5, 3.44), (0.62, 3.22), (0.58, 2.97), (0.42, 2.82), (0.34, 2.56),
             (0.42, 2.27), (0.66, 2.02), (0.95, 1.77), (1.07, 1.63), (1.22, 1.56), (1.45, 1.32), (1.56, 1.02),
             (1.52, 0.72), (1.37, 0.52), (1.22, 0.46), (1.22, 0.35), (0.0, 0.35)]
GATE_WING_T = 0.5


def gates(houses):
    """The two gates. Returns ([], lines, mesh): the gates' geometry in the ensemble frame (no detail
    pieces any more) and each gate's line from the side house's corner to Novikova 1's."""
    n1 = houses["Novikova1"]
    m = Mesh()
    lines = []
    for name, n1_corner in (("Novikova3", (N1_L / 2, -N1_D / 2)), ("Maksimova8", (-N1_L / 2, -N1_D / 2))):
        s = houses[name]
        a = s.frame(s.gate_corner)
        b = n1.frame((n1_corner[0], n1_corner[1], 0.0))
        dx, dy = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dy)
        d = (dx / L, dy / L)
        nl = (-d[1], d[0])                      # left of the gate line: (u along it, y, z) is right-handed
        loc = Mesh()
        W = GATE_BLOCK_W
        _gate_block(loc, 0.0, W, +1)            # against the side house, its wing towards the drive
        _gate_block(loc, L - W, L, -1)          # against Novikova 1
        m.extend(loc.transformed(lambda p, a=a, d=d, nl=nl: (a[0] + d[0] * p[0] + nl[0] * p[1],
                                                             a[1] + d[1] * p[0] + nl[1] * p[1], p[2])))
        lines.append((a, b))
    return [], lines, m


def _gate_block(m, u0, u1, wing_dir):
    """One block of a gate in the gate's local frame (u along the line, y across it, z up), its wing
    on the side wing_dir (+1: towards +u)."""
    D, H = GATE_BLOCK_D, GATE_BLOCK_H
    y0, y1 = -D / 2, D / 2
    uc = (u0 + u1) / 2
    aw, ah = GATE_ARCH_W, GATE_ARCH_H
    r = aw / 2
    ua0, ua1, zs = uc - r, uc + r, ah - r
    pl = 0.35
    # plinth (split at the passage) and the piers, the wall over the arch, the spandrels and the soffit
    box(m, "AC_Concrete", u0 - 0.03, ua0, y0 - 0.03, y1 + 0.03, -0.4, pl)
    box(m, "AC_Concrete", ua1, u1 + 0.03, y0 - 0.03, y1 + 0.03, -0.4, pl)
    box(m, "AC_Wall", u0, ua0, y0, y1, pl, ah, faces="xXyY")
    box(m, "AC_Wall", ua1, u1, y0, y1, pl, ah, faces="xXyY")
    box(m, "AC_Wall", u0, u1, y0, y1, ah, H - 0.2, faces="xXyY")
    seg = 14
    arc = [(uc + r * math.cos(math.pi * k / seg), zs + r * math.sin(math.pi * k / seg)) for k in range(seg + 1)]
    half = seg // 2
    right = [(ua1, zs), (ua1, ah)] + [(uc, ah)] + arc[half - 1:0:-1]
    left = [(uc, ah), (ua0, ah), (ua0, zs)] + arc[seg - 1:half:-1]
    to3d = lambda u, v, t: (u, -t, v)          # profile (u, z) solid across y
    for poly in (right, left):
        geom.extrude_profile(m, "AC_Wall", "AC_Wall", poly, -y1, -y0, to3d)
    # the archivolt (a white U round the passage) on both faces
    band = 0.18
    outer = [(uc + (r + band) * math.cos(math.pi * k / seg), zs + (r + band) * math.sin(math.pi * k / seg)) for k in range(seg + 1)]
    inner = [(uc + r * math.cos(math.pi * k / seg), zs + r * math.sin(math.pi * k / seg)) for k in range(seg + 1)]
    u_ring = [(ua1 + band, pl), (ua1 + band, zs)] + outer[1:-1] + [(ua0 - band, zs), (ua0 - band, pl), (ua0, pl), (ua0, zs)] + \
        inner[-2:0:-1] + [(ua1, zs), (ua1, pl)]
    for t0, t1 in ((-y0, -y0 + 0.035), (-y1 - 0.035, -y1)):
        geom.extrude_profile(m, "AC_Trim", "AC_Trim", u_ring, t0, t1, to3d)
    # a white band at 4 m, the cornice and the low hipped cap
    box(m, "AC_Trim", u0 - 0.06, u1 + 0.06, y0 - 0.06, y1 + 0.06, 3.95, 4.12)
    box(m, "AC_Trim", u0 - 0.03, u1 + 0.03, y0 - 0.03, y1 + 0.03, 4.12, 4.2)
    box(m, "AC_Trim", u0 - 0.2, u1 + 0.2, y0 - 0.2, y1 + 0.2, H - 0.2, H)
    geom.hip_roof(m, "AC_Roof", "AC_Trim", "AC_Trim", u0, u1, y0, y1, H, math.radians(14), 0.25, fascia_h=0.06)
    # the scrolled wing, its S-curved edge white, and a white band along the curve on both faces
    uf = u1 if wing_dir > 0 else u0
    prof = [(uf + wing_dir * p[0], p[1]) for p in GATE_WING]
    t = GATE_WING_T / 2
    geom.extrude_profile(m, "AC_Wall", "AC_Trim", prof, -t, t, to3d)
    curve = GATE_WING[:-2]
    for i in range(len(curve) - 1):
        (s0, z0), (s1, z1) = curve[i], curve[i + 1]
        ln = math.hypot(s1 - s0, z1 - z0) or 1.0
        nx, nz = (z1 - z0) / ln, -(s1 - s0) / ln          # into the wing
        q = [(s0, z0), (s1, z1), (s1 + nx * 0.2, z1 + nz * 0.2), (s0 + nx * 0.2, z0 + nz * 0.2)]
        q = [(uf + wing_dir * qq[0], qq[1]) for qq in q]
        for yy, flip in ((-t - 0.008, False), (t + 0.008, True)):
            pts = [(p[0], yy, p[1]) for p in q]
            if (wing_dir > 0) != flip:
                pts = pts[::-1]
            m.face("AC_Trim", pts)
    box(m, "AC_Concrete", min(uf, uf + wing_dir * 1.3), max(uf, uf + wing_dir * 1.3), -t - 0.05, t + 0.05, -0.4, 0.35)


SQUARE = {
    "fountain": (22.9 - N1_OSM[0], 6.6 - N1_OSM[1]),
    "bed_r": 5.7,
    "x_end": 80.0,          # the street
}


def square_mesh():
    """Paths and lawns of the square in front of Novikova 1 (ensemble frame)."""
    m = Mesh()
    fx, fy = SQUARE["fountain"]
    # asphalt apron along the front and a ring round the bed, the axis path to the street
    _flat(m, "AC_Asphalt", [(8.0, -40.0), (16.0, -40.0), (16.0, 40.0), (8.0, 40.0)], 0.02)
    _flat(m, "AC_Asphalt", [(fx, fy - 1.6), (SQUARE["x_end"] + 26.0, fy - 1.6), (SQUARE["x_end"] + 26.0, fy + 1.6), (fx, fy + 1.6)], 0.02)
    # the street along the square (to the lower station and on into the town)
    _flat(m, "AC_Asphalt", [(SQUARE["x_end"], -115.0), (SQUARE["x_end"] + 9.0, -115.0), (SQUARE["x_end"] + 9.0, 115.0), (SQUARE["x_end"], 115.0)], 0.03)
    # the garden: clipped hedges along the paths (gaps where paths cross), flowers round the fountain
    # and along the facade (commons_Marshal_Novikov_str._1_1_.jpg, _2_.jpg)
    xe = SQUARE["x_end"]
    for side in (-1, 1):
        y_in = fy + side * 2.4
        for a, b in ((fx + SQUARE["bed_r"] + 2.6, xe - 1.2),):
            _hedge(m, a, b, y_in - 0.35, y_in + 0.35)
        for a, b in ((fy + side * 9.0, fy + side * 34.0),):
            y0, y1 = sorted((a, b))
            _hedge(m, 17.0, 17.7, y0, y1)               # behind the apron
            _hedge(m, xe - 1.0, xe - 0.3, y0, y1)       # along the street
    rnd = random.Random(7)
    for i in range(70):                                 # day lilies and marigolds inside the fountain's ring
        t = 2 * math.pi * i / 70
        r = SQUARE["bed_r"] - rnd.uniform(0.6, 2.2)
        _flower_clump(m, fx + r * math.cos(t), fy + r * math.sin(t), rnd, h=rnd.uniform(0.45, 0.75))
    # under the ground-floor windows, kept 1.7 m clear of every door (Dima: the beds blocked the
    # stair entrances behind the bays)
    f1 = Frame(*FRAMES["Novikova1"][:2], math.radians(FRAMES["Novikova1"][2]))
    doors_y = [f1((bx, 0.0, 0.0))[1] for bx in (-N1_BAYS[2], -4.35, 0.0, 4.35, N1_BAYS[2])]
    for y0, y1 in ((-29.5, -5.5), (5.5, 29.5)):
        n = int((y1 - y0) / 0.45)
        for k in range(n):
            yy = y0 + (k + 0.5) * (y1 - y0) / n
            if any(abs(yy - dy) < 1.7 for dy in doors_y):
                continue
            _flower_clump(m, 7.75 + rnd.uniform(-0.15, 0.15), yy, rnd, w=0.55, h=rnd.uniform(0.4, 0.65))
    # drives through the gates into the yard
    for gy in (38.4, -38.9):
        _flat(m, "AC_Asphalt", [(-35.0, gy - 2.2), (SQUARE["x_end"], gy - 2.2), (SQUARE["x_end"], gy + 2.2), (-35.0, gy + 2.2)], 0.025)
    ring = [(fx + (SQUARE["bed_r"] + 2.2) * math.cos(t), fy + (SQUARE["bed_r"] + 2.2) * math.sin(t))
            for t in [2 * math.pi * i / 48 for i in range(48)]]
    _flat(m, "AC_Asphalt", ring, 0.021)
    bed = [(fx + SQUARE["bed_r"] * math.cos(t), fy + SQUARE["bed_r"] * math.sin(t)) for t in [2 * math.pi * i / 48 for i in range(48)]]
    _flat(m, "AC_Soil", bed, 0.12)
    return m


def square_trees():
    """(x, y, species, seed, age) of the square's trees (ensemble frame). The physicists planted
    limes, spruces and birches; Norway maple stands in for the lime (no lime in the plant library)."""
    fx, fy = SQUARE["fountain"]
    out = []
    seed = 47
    for dx, dy, sp, age in ((-6.5, 11.5, "Maple", 55), (-6.5, -11.5, "Maple", 55),
                            (14.0, 21.0, "SilverBirch", 45), (14.0, -21.0, "SilverBirch", 45),
                            (34.0, 24.0, "NorwaySpruce", 60), (34.0, -24.0, "NorwaySpruce", 60),
                            (46.0, 14.0, "Maple", 50), (46.0, -14.0, "Maple", 50)):
        out.append((fx + dx, fy + dy, sp, seed, age))
        seed += 1
    return out


def _hedge(m, x0, x1, y0, y1, h=0.95, seed=0):
    """A clipped hedge: a dark core and alpha leaf cards over its faces and through it
    (textures/academ/foliage_hedge.png, tools/make_academ_foliage.py)."""
    rnd = random.Random(int(abs(x0 * 7 + y0 * 13)) + seed)
    i = 0.14
    box(m, "AC_Hedge", x0 + i, x1 - i, y0 + i, y1 - i, 0.0, h - i, faces="xXyYZ")
    def card(c, u, v, w, hh, uv0):
        """A card centred at c spanning w along unit vector u and hh along v."""
        p = [(c[0] - u[0] * w / 2 - v[0] * hh / 2, c[1] - u[1] * w / 2 - v[1] * hh / 2, c[2] - u[2] * w / 2 - v[2] * hh / 2)]
        p.append((p[0][0] + u[0] * w, p[0][1] + u[1] * w, p[0][2] + u[2] * w))
        p.append((p[1][0] + v[0] * hh, p[1][1] + v[1] * hh, p[1][2] + v[2] * hh))
        p.append((p[0][0] + v[0] * hh, p[0][1] + v[1] * hh, p[0][2] + v[2] * hh))
        a, b = uv0
        m.face("AC_Foliage", p, [(a, b), (a + 0.5, b), (a + 0.5, b + 0.5), (a, b + 0.5)])
    uv = lambda: (rnd.choice((0.0, 0.5)), rnd.choice((0.0, 0.5)))
    L = (x1 - x0, y1 - y0)
    along_x = L[0] >= L[1]
    length = max(L)
    n = max(2, int(length / 0.35))
    for k in range(n):
        t = (k + rnd.uniform(0.2, 0.8)) / n
        if along_x:
            px = x0 + (x1 - x0) * t
            for side, yy in ((-1, y0 + rnd.uniform(-0.03, 0.05)), (1, y1 - rnd.uniform(-0.03, 0.05))):
                for zc in (h * 0.3, h * 0.72):
                    card((px, yy, zc + rnd.uniform(-0.05, 0.05)), (1, 0, 0), (0, 0, 1), 0.55, 0.55, uv())
            card((px, (y0 + y1) / 2, h - rnd.uniform(0.0, 0.05)), (1, 0, 0), (0, 1, 0), 0.5, (y1 - y0) + 0.1, uv())
        else:
            py = y0 + (y1 - y0) * t
            for side, xx in ((-1, x0 + rnd.uniform(-0.03, 0.05)), (1, x1 - rnd.uniform(-0.03, 0.05))):
                for zc in (h * 0.3, h * 0.72):
                    card((xx, py, zc + rnd.uniform(-0.05, 0.05)), (0, 1, 0), (0, 0, 1), 0.55, 0.55, uv())
            card(((x0 + x1) / 2, py, h - rnd.uniform(0.0, 0.05)), (0, 1, 0), (1, 0, 0), 0.5, (x1 - x0) + 0.1, uv())
        # a few tilted cards poking out of the top so the silhouette is not ruler-straight
        if rnd.random() < 0.5:
            a_ = rnd.uniform(0, math.pi)
            card((x0 + (x1 - x0) * (t if along_x else 0.5), y0 + (y1 - y0) * (0.5 if along_x else t), h + 0.02),
                 (math.cos(a_), math.sin(a_), 0), (0, 0, 1), 0.45, 0.3, uv())
    for xe in ((x0, x1) if along_x else ()):
        card((xe, (y0 + y1) / 2, h / 2), (0, 1, 0), (0, 0, 1), (y1 - y0) + 0.08, h, uv())
    for ye in ((y0, y1) if not along_x else ()):
        card(((x0 + x1) / 2, ye, h / 2), (1, 0, 0), (0, 0, 1), (x1 - x0) + 0.08, h, uv())


def _flower_clump(m, cx, cy, rnd, w=0.6, h=0.6):
    """Day lilies and marigolds: three crossed cards (textures/academ/foliage_flowers.png)."""
    for k in range(3):
        a = math.pi * k / 3 + rnd.uniform(-0.2, 0.2)
        dx, dy = math.cos(a) * w / 2, math.sin(a) * w / 2
        p = [(cx - dx, cy - dy, 0.0), (cx + dx, cy + dy, 0.0), (cx + dx, cy + dy, h), (cx - dx, cy - dy, h)]
        m.face("AC_FlowerCards", p, [(0, 0), (1, 0), (1, 1), (0, 1)])


# Two plantings either side of the square's axis: the view from the lower station along the axis to the
# middle house and its fountain stays open (QA: arriving by cable car you must see the ensemble).
SQUARE_GARDEN = [
    {"centre": (48.0, 25.5), "half": (30.0, 13.0), "count": 16,
     "mix": [("Maple", 0.5, 30, 70), ("SilverBirch", 0.25, 25, 55), ("NorwaySpruce", 0.25, 30, 70)]},
    {"centre": (48.0, -22.5), "half": (30.0, 13.0), "count": 16,
     "mix": [("Maple", 0.5, 30, 70), ("SilverBirch", 0.25, 25, 55), ("NorwaySpruce", 0.25, 30, 70)]},
]


def _flat(m, mat, poly, z):
    m.face(mat, [(x, y, z) for x, y in poly])


def ensemble():
    houses = {"Novikova1": novikova1(), "Novikova3": side_house("Novikova3"), "Maksimova8": side_house("Maksimova8")}
    return houses


if __name__ == "__main__":
    hs = ensemble()
    for k, h in hs.items():
        print(k, "tris", h.mesh.tri_count(), "details", len(h.details))
    g, lines, gm = gates(hs)
    for a, b in lines:
        print("gate", [round(v, 1) for v in a], [round(v, 1) for v in b], round(math.dist(a[:2], b[:2]), 1))
