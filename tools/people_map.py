"""Where the town's people can live, sit, sleep, eat, read and go, for the people life simulation
(Private/People/PeopleSim.h). Pure Python (no bpy, no unreal): it rebuilds the Academgorodok's insides
(blender/academgorodok plan.py, interiors.py, furniture.py, library.py: deterministic, the same code the
Blender build runs), reads the club's layout (blender/export/scene01_layout.json), the outdoor map
(tools/world_map.py) and the animals' walk grid (Content/AnimalsData/animal_world.bin), and writes

  unreal/Lyudmilovsk/Content/PeopleData/people_places.json

    stairs   every stair of the three houses: the point outside its street door (on the pavement) and its
             yard door, and per storey the walk from the street door through the lobby up the flights to
             that storey's landing (the layout.json "walks" stair routes)
    flats    every flat (numbered like real flats: by stair, then storey, two a landing): the point on the
             landing in front of its door, its rooms (centre, the walk from the door) and its slots (beds,
             sofas, armchairs, kitchen, dining and desk chairs) with the walk from the door to each
    places   the library (reading chairs, the librarian's chair), the Academgorodok's square and benches,
             the cable car's stations, the club's door, the spur's lookout and alley benches, the beach and
             the promenade, the town square
    club     inside the club: the way in from the portal to the walkway, a seat per computer, the vending
             machines, the owner's booth

  python3 tools/people_map.py [--face-y CM] [--plot] [--cache FILE]

Frame: Unreal world metres (UE cm / 100), z absolute. Academgorodok points go house frame -> ensemble
(plan.FRAMES, geom.Frame, Maksimova 8 mirrored) -> map (world_map.academ_to_map, academ_z) -> Unreal
(world_map.to_unreal), as unreal/scripts/test_academ_walk.py does.

Yaw: Unreal degrees, 0 = +X, 90 = +Y (top view: X right, Y down). For a seat it is the way the seated
person faces; for a bed the way from its head to its foot (the way a person lying in it looks along); for
a standing spot the way the person looks.

Slot pos: the floor point under the seat's middle (a sofa: under its middle, "spots" under each sitter; a
bed: under the middle of the mattress, "spots" under each sleeper's half). height: seat / mattress top
over that floor. approach: where to stand before sitting or lying down (free of furniture by the
walker's radius and reachable from the door).

--plot writes Saved/PeopleMap/*.png (PIL): a plan per house storey, the library, the club, the world.
"""
import argparse
import heapq
import json
import math
import os
import pickle
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
ACADEM = os.path.join(ROOT, "blender", "academgorodok")
sys.path.insert(0, HERE)
sys.path.insert(0, ACADEM)
import world_map as W  # noqa: E402

OUT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "PeopleData", "people_places.json")
GRID = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "AnimalsData", "animal_world.bin")
PLOTS = os.path.join(ROOT, "Saved", "PeopleMap")
MAIN_SCENE = "/Users/diipushk/projects/lyudmilovsk/blender/export/scene01_layout.json"
FACE_Y = 790.0

R_WALL = 0.28          # walker radius against walls (m)
R_FURN = 0.25          # ... against furniture
CELL = 0.05            # indoor planning grid
DOOR_SIDE = 0.25       # a doorway's walkable middle: its width less this on each side
MIN_CLEAR_H = 1.95 + 0.15   # tools/character_standard.py MinDoorClearHeight (m)


def r2(v):
    return round(v + 0.0, 2)


def rp(p):
    return [r2(p[0]), r2(p[1]), r2(p[2])]


def ang(a):
    """Degrees in (-180, 180]."""
    a = math.fmod(a, 360.0)
    if a <= -180.0:
        a += 360.0
    elif a > 180.0:
        a -= 360.0
    return round(a, 1)


# --- frames -------------------------------------------------------------------------------------------------

def ens_to_ue(p):
    """Ensemble metres (x east, y north, z over its ground) -> Unreal metres."""
    mx, my = W.academ_to_map(p[0], p[1])
    x, y, z = W.to_unreal(mx, my, W.academ_z() + p[2], FACE_Y)
    return (x / 100.0, y / 100.0, z / 100.0)


def map_to_ue(x, y, z):
    return (x, FACE_Y / 100.0 + y, z)


class HouseXf:
    """House frame (x along the house, y across, z over the ground) -> Unreal metres."""

    def __init__(self, frame):
        self.f = frame

    def pt(self, p):
        return ens_to_ue(self.f(p))

    def yaw(self, p, a):
        """Unreal yaw of the house-frame direction at angle a (radians, from house +x towards +y) at p."""
        q = (p[0] + 0.5 * math.cos(a), p[1] + 0.5 * math.sin(a), p[2])
        u, v = self.pt(p), self.pt(q)
        return ang(math.degrees(math.atan2(v[1] - u[1], v[0] - u[0])))

    def path(self, pts):
        return [rp(self.pt(p)) for p in pts]


# --- the walk grid (tools/animal_map.py) -----------------------------------------------------------------

class WalkGrid:
    def __init__(self, path):
        with open(path, "rb") as f:
            data = f.read()
        magic, self.nx, self.ny = struct.unpack_from("<4sii", data, 0)
        assert magic == b"AWG1"
        self.x0, self.y0, self.cell = struct.unpack_from("<fff", data, 12)
        n = self.nx * self.ny
        self.z = struct.unpack_from("<%dh" % n, data, 24)
        self.kind = data[24 + 2 * n: 24 + 3 * n]
        self.comp = None

    def ij(self, x, y):
        return int(round((x - self.x0) / self.cell)), int(round((y - self.y0) / self.cell))

    def walkable(self, x, y):
        i, j = self.ij(x, y)
        return 0 <= i < self.nx and 0 <= j < self.ny and (self.kind[j * self.nx + i] & 7) != 0

    def ground(self, x, y):
        """Bilinear ground height (m) at Unreal metres x, y."""
        fx, fy = (x - self.x0) / self.cell, (y - self.y0) / self.cell
        i, j = max(0, min(self.nx - 2, int(math.floor(fx)))), max(0, min(self.ny - 2, int(math.floor(fy))))
        tx, ty = min(1.0, max(0.0, fx - i)), min(1.0, max(0.0, fy - j))
        z = self.z
        a = z[j * self.nx + i] * (1 - tx) + z[j * self.nx + i + 1] * tx
        b = z[(j + 1) * self.nx + i] * (1 - tx) + z[(j + 1) * self.nx + i + 1] * tx
        return (a * (1 - ty) + b * ty) / 100.0

    def components(self):
        """8-connected components of the walkable cells (label per cell, 0 = blocked)."""
        if self.comp is not None:
            return self.comp
        nx, ny = self.nx, self.ny
        lab = [0] * (nx * ny)
        cur = 0
        for s in range(nx * ny):
            if lab[s] or (self.kind[s] & 7) == 0:
                continue
            cur += 1
            lab[s] = cur
            st = [s]
            while st:
                c = st.pop()
                i, j = c % nx, c // nx
                for di in (-1, 0, 1):
                    for dj in (-1, 0, 1):
                        a, b = i + di, j + dj
                        if 0 <= a < nx and 0 <= b < ny:
                            k = b * nx + a
                            if not lab[k] and (self.kind[k] & 7) != 0:
                                lab[k] = cur
                                st.append(k)
        self.comp = lab
        return lab

    def component(self, x, y):
        i, j = self.ij(x, y)
        if not (0 <= i < self.nx and 0 <= j < self.ny):
            return 0
        return self.components()[j * self.nx + i]


# --- indoor planning grid ------------------------------------------------------------------------------------

class Plan:
    """A walkability grid over a few rectangles (house frame or any 2D frame): rooms inset by the
    walker's radius, doorways joining them, furniture inflated; shortest ways and their smoothing."""

    def __init__(self, x0, y0, x1, y1, cell=CELL):
        self.cell = cell
        self.x0, self.y0 = x0, y0
        self.nx = int(math.ceil((x1 - x0) / cell)) + 1
        self.ny = int(math.ceil((y1 - y0) / cell)) + 1
        self.ok = bytearray(self.nx * self.ny)
        self.dist = None

    def _span(self, a0, a1, o, n):
        i0 = max(0, int(math.ceil((a0 - o) / self.cell - 0.5)))
        i1 = min(n - 1, int(math.floor((a1 - o) / self.cell - 0.5)))
        return i0, i1

    def rect(self, x0, x1, y0, y1, val):
        if x1 <= x0 or y1 <= y0:
            return
        i0, i1 = self._span(x0, x1, self.x0, self.nx)
        j0, j1 = self._span(y0, y1, self.y0, self.ny)
        for j in range(j0, j1 + 1):
            row = j * self.nx
            for i in range(i0, i1 + 1):
                self.ok[row + i] = val

    def room(self, x0, x1, y0, y1, r=R_WALL):
        self.rect(x0 + r, x1 - r, y0 + r, y1 - r, 1)

    def block(self, x0, x1, y0, y1, r=R_FURN):
        self.rect(x0 - r, x1 + r, y0 - r, y1 + r, 0)

    def cell_of(self, p):
        return int(math.floor((p[0] - self.x0) / self.cell)), int(math.floor((p[1] - self.y0) / self.cell))

    def centre(self, i, j):
        return (self.x0 + (i + 0.5) * self.cell, self.y0 + (j + 0.5) * self.cell)

    def free(self, p):
        i, j = self.cell_of(p)
        return 0 <= i < self.nx and 0 <= j < self.ny and self.ok[j * self.nx + i] == 1

    def reached(self, p):
        i, j = self.cell_of(p)
        return 0 <= i < self.nx and 0 <= j < self.ny and self.dist is not None and self.dist[j * self.nx + i] < 1e17

    def flood(self, start):
        """Dijkstra from start over 8-connected free cells; start snaps to the nearest free cell."""
        s = self.nearest(start, lambda k: self.ok[k] == 1)
        nx = self.nx
        dist = [1e18] * (nx * self.ny)
        par = [-1] * (nx * self.ny)
        if s is None:
            self.dist, self.par = dist, par
            return False
        k0 = s[1] * nx + s[0]
        dist[k0] = 0.0
        h = [(0.0, k0)]
        steps = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.4142), (1, -1, 1.4142), (-1, 1, 1.4142), (-1, -1, 1.4142)]
        ok = self.ok
        while h:
            d, k = heapq.heappop(h)
            if d > dist[k]:
                continue
            i, j = k % nx, k // nx
            for di, dj, c in steps:
                a, b = i + di, j + dj
                if 0 <= a < nx and 0 <= b < self.ny:
                    q = b * nx + a
                    if ok[q] != 1:
                        continue
                    if di and dj and (ok[j * nx + a] != 1 or ok[b * nx + i] != 1):
                        continue
                    nd = d + c
                    if nd < dist[q]:
                        dist[q] = nd
                        par[q] = k
                        heapq.heappush(h, (nd, q))
        self.dist, self.par = dist, par
        self.start = start
        return True

    def nearest(self, p, test, rmax=60):
        i0, j0 = self.cell_of(p)
        best = None
        for r in range(rmax):
            for j in range(j0 - r, j0 + r + 1):
                for i in range(i0 - r, i0 + r + 1):
                    if max(abs(i - i0), abs(j - j0)) != r:
                        continue
                    if 0 <= i < self.nx and 0 <= j < self.ny and test(j * self.nx + i):
                        c = self.centre(i, j)
                        d = math.hypot(c[0] - p[0], c[1] - p[1])
                        if best is None or d < best[0]:
                            best = (d, i, j)
            if best is not None and best[0] < (r - 1) * self.cell:
                break
        return None if best is None else (best[1], best[2])

    def nearest_reached(self, p, rmax=60):
        c = self.nearest(p, lambda k: self.dist[k] < 1e17, rmax)
        return None if c is None else self.centre(*c)

    def los(self, a, b):
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(L / (self.cell * 0.5)))
        for k in range(n + 1):
            t = k / n
            if not self.free((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)):
                return False
        return True

    def way(self, goal):
        """Smoothed polyline from the flood's start to goal (2D points), or None."""
        i, j = self.cell_of(goal)
        if not (0 <= i < self.nx and 0 <= j < self.ny) or self.dist[j * self.nx + i] >= 1e17:
            return None
        cells = []
        k = j * self.nx + i
        while k != -1:
            cells.append(self.centre(k % self.nx, k // self.nx))
            k = self.par[k]
        cells.reverse()
        pts = [tuple(self.start[:2])] + cells[1:-1] + [tuple(goal[:2])]
        if not self.free(pts[0]):
            pts[0] = cells[0]
            pts.insert(0, tuple(self.start[:2]))
        out = [pts[0]]
        a = 0
        while a < len(pts) - 1:
            b = len(pts) - 1
            while b > a + 1 and not self.los(pts[a], pts[b]):
                b -= 1
            out.append(pts[b])
            a = b
        return out


# --- instrumenting the furniture code (records what it places and where the seats are) -----------------------

SEAT_PARTS = {
    # part name: (seat centre in the part's frame, seat / mattress top height); faces local -y
    "dining_chair": ((0.0, -0.02), 0.47),
    "bentwood_chair": ((0.0, -0.02), 0.465),
    "stool": ((0.0, 0.0), 0.45),
    "leather_armchair": ((0.0, -0.08), 0.5),
    "armchair": ((0.0, -0.08), 0.49),
    "sofa": ((0.0, -0.09), 0.48),
    "bed": ((0.0, 0.0), 0.58),
}
LIB_PLACED = []


def instrument():
    import furniture as FU
    import library as LIB

    def tagger(fn, kind):
        def wrapped(*a, **k):
            m = fn(*a, **k)
            m._kind = kind
            m._self_bb = FU.bbox(m)
            return m
        return wrapped

    for name in SEAT_PARTS:
        setattr(FU, name, tagger(getattr(FU, name), name))

    def add(m, part, dx=0.0, dy=0.0, dz=0.0, yaw=0.0):
        if not hasattr(m, "_parts"):
            m._base = FU.bbox(m) if m.verts else None
            m._parts = []
            m._seats = list(getattr(m, "_seats", []))
        mv = FU.moved(part, dx, dy, dz, yaw)
        m.extend(mv)
        m._parts.append(FU.bbox(mv))
        fr = __import__("geom").Frame(dx, dy, yaw, dz)
        if getattr(part, "_kind", None):
            m._seats.append((part._kind, (dx, dy, dz), yaw, getattr(part, "_base", None) or part._self_bb))
        for k, p, a, bb in getattr(part, "_seats", []):
            m._seats.append((k, fr(p), yaw + a, bb))

    FU.add = add
    orig_commit = FU.Layout.commit

    def commit(self, name, part, frame_args, aabb, target=None):
        orig_commit(self, name, part, frame_args, aabb, target)
        if not hasattr(self, "_placed"):
            self._placed = {}
        self._placed[id(aabb)] = (name, part, frame_args)

    FU.Layout.commit = commit
    orig_put = LIB.put

    def put(m, part, x, y, z=LIB.Z0, yaw=0.0, name=None):
        p = orig_put(m, part, x, y, z, yaw, name)
        LIB_PLACED.append((name, part, (x, y, yaw, z)))
        return p

    LIB.put = put
    return FU, LIB


def part_seats(part, frame_args):
    """[(part kind, house xy of the seat centre, facing angle rad, height, local bbox, part yaw)] of a placed part."""
    import geom
    tx, ty, yaw, z = frame_args
    fr = geom.Frame(tx, ty, yaw, z)
    out = []
    seats = []
    if getattr(part, "_kind", None):
        seats.append((part._kind, (0.0, 0.0, 0.0), 0.0, part._base if getattr(part, "_base", None) else part._self_bb))
    seats += getattr(part, "_seats", [])
    for kind, p, a, bb in seats:
        (cx, cy), h = SEAT_PARTS[kind]
        c, s = math.cos(a), math.sin(a)
        lp = (p[0] + cx * c - cy * s, p[1] + cx * s + cy * c, p[2])
        out.append({"part": kind, "xy": fr(lp)[:2], "face": yaw + a - math.pi / 2, "height": h, "bb": bb,
                    "rot": yaw + a, "origin": fr(p)[:2]})
    return out


def part_boxes(part, frame_args, aabb):
    """Floor footprints (house frame) of a placed part: its own pieces when it is a set (a table and its
    chairs), else its box. Only what stands in a walker's way (0.05 .. 1.9 m over the floor)."""
    import geom
    tx, ty, yaw, z = frame_args
    fr = geom.Frame(tx, ty, yaw, 0.0)
    if not getattr(part, "_parts", None):
        return [aabb]
    out = []
    for bb in ([part._base] if part._base else []) + part._parts:
        if bb[4] > 1.9 or bb[5] < 0.05:
            continue
        pts = [fr((x, y, 0.0)) for x in (bb[0], bb[1]) for y in (bb[2], bb[3])]
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        out.append((min(xs), max(xs), min(ys), max(ys), z + bb[4], z + bb[5]))
    return out


# --- the Academgorodok ---------------------------------------------------------------------------------------

def extract_houses():
    """Everything people_map needs from the Academgorodok's build, as plain data (cacheable)."""
    FU, LIB = instrument()
    import interiors
    import plan
    houses = plan.ensemble()
    out = {"houses": {}, "library": None}
    for name, h in houses.items():
        spec = interiors.Spec(h)
        _, rooms = interiors.build(h)
        rr = []
        for r in rooms:
            q = {k: r[k] for k in ("kind", "x0", "x1", "y0", "y1", "z") if k in r}
            q["doors"] = r.get("doors", [])
            q["windows"] = r.get("windows", [])
            if r["kind"] != "stair":
                P = FU.furnish_parts(r)
                L = P["layout"]
                placed = getattr(L, "_placed", {})
                boxes, seats, items = [], [], []
                for iname, aabb in L.items:
                    pl = placed.get(id(aabb))
                    if pl:
                        _, part, fa = pl
                        bxs = part_boxes(part, fa, aabb)
                        for s in part_seats(part, fa):
                            s["item"] = iname
                            seats.append(s)
                    else:
                        bxs = [aabb]
                    items.append((iname, aabb))
                    for b in bxs:
                        if b[4] < r["z"] + 1.9 and b[5] > r["z"] + 0.05 and not (
                                b[4] > r["z"] + 1.0 and min(b[1] - b[0], b[3] - b[2]) < 0.06):
                            boxes.append((iname,) + tuple(b))
                q["boxes"], q["seats"], q["items"] = boxes, seats, items
            rr.append(q)
        out["houses"][name] = {
            "frame": (h.frame.ox, h.frame.oy, h.frame.yaw, h.frame.oz, h.frame.mirror),
            "stairs": list(spec.stairs), "F": spec.F, "street_in": spec.street_in, "yard_sign": spec.yard_sign,
            "yc": spec.yc, "yard_in": {sx: spec.yard_in(sx) for sx in spec.stairs},
            "gastronom": spec.gastronom, "rooms": rr, "walks": h.walks}
    # the library (Novikova 1's ground floor, house frame)
    lib = {"footprints": list(LIB.FOOTPRINTS), "placed": []}
    for name, part, fa in LIB_PLACED:
        if not name:
            continue
        e = {"name": name, "seats": part_seats(part, fa) if (getattr(part, "_kind", None) or getattr(part, "_seats", None)) else [],
             "boxes": part_boxes(part, fa, None) if getattr(part, "_parts", None) else None}
        lib["placed"].append(e)
    out["library"] = lib
    return out


def frame_of(fr):
    import geom
    ox, oy, yaw, oz, mirror = fr
    return geom.Frame(ox, oy, yaw, oz, mirror)


class Approacher:
    """Picks the stand-before-sitting point of a seat: in front, beside, behind or at the foot, the
    first that is free and reachable on the plan; else the nearest reachable cell."""

    def __init__(self, plan):
        self.plan = plan

    def pick(self, xy, face, cands, room=None):
        P = self.plan
        inside = (lambda q: True) if room is None else \
            (lambda q: room[0] + 0.2 < q[0] < room[1] - 0.2 and room[2] + 0.2 < q[1] < room[3] - 0.2)
        cf, sf = math.cos(face), math.sin(face)
        for fwd, side in cands:
            p = (xy[0] + cf * fwd - sf * side, xy[1] + sf * fwd + cf * side)
            if P.free(p) and P.reached(p) and inside(p):
                return p
        c = P.nearest(xy, lambda k: P.dist[k] < 1e17 and inside(P.centre(k % P.nx, k // P.nx)), 40)
        return None if c is None else P.centre(*c)


CANDS = {
    "bed": None,                                                       # filled per bed (its width)
    "sofa": [(0.6, 0.0), (0.75, 0.0), (0.6, 0.4), (0.6, -0.4), (0.9, 0.0)],
    "armchair": [(0.6, 0.0), (0.75, 0.0), (0.55, 0.35), (0.55, -0.35), (0.9, 0.0), (0.0, 0.65), (0.0, -0.65)],
    "chair_at_table": [(-0.15, 0.55), (-0.15, -0.55), (-0.6, 0.0), (-0.55, 0.4), (-0.55, -0.4), (-0.75, 0.0), (0.0, 0.7), (0.0, -0.7)],
    "wall_stool": [(0.55, 0.0), (0.7, 0.0), (0.45, 0.4), (0.45, -0.4)],
}


def flat_slot_kind(item, part, room_kind):
    if part == "bed":
        return "bed"
    if item == "desk" and part in ("leather_armchair", "bentwood_chair"):
        return "desk_seat"
    if item in ("dining_set", "round_table") and part == "dining_chair":
        return "dining_seat"
    if item == "table" and part == "stool":
        return "kitchen_seat"
    if item == "stool" and part == "stool" and room_kind == "kitchen":
        return "kitchen_seat"
    if item == "sofa" and part == "sofa":
        return "sofa"
    if item == "armchair" and part in ("armchair", "leather_armchair"):
        return "armchair"
    return None


def build_flats(data):
    """stairs, flats, plus plot material per house storey."""
    import interiors
    from plan import floor_z, FLOOR_H, WALL_T
    stairs, flats, plot = [], [], []
    for hname in ("Novikova1", "Novikova3", "Maksimova8"):
        H = data["houses"][hname]
        xf = HouseXf(frame_of(H["frame"]))
        s = H["yard_sign"]
        yc = H["yc"]
        F = H["F"]
        rooms = H["rooms"]
        walks = {lab: pts for lab, pts in H["walks"]}
        # ---- stairs
        st_ids = {}
        for si, sx in enumerate(H["stairs"]):
            sid = "%s.s%d" % (hname, si)
            st_ids[sx] = (sid, si)
            pts = walks["stair %.1f" % sx]
            out_street = -s                                          # street side is -y for Novikova 1 (s = +1)
            face_street = H["street_in"] + out_street * WALL_T
            ye = H["yard_in"][sx]
            face_yard = ye + s * WALL_T
            gz = floor_z(0)
            behind_vest = H["gastronom"] and H["gastronom"][0] < sx < H["gastronom"][1]
            street = None if behind_vest else (sx, face_street + out_street * 2.0, 0.0)
            street_in = [] if street is None else [street, (sx, face_street + out_street * 0.15, gz - 0.15),
                                                   (sx, H["street_in"] - out_street * 0.3, gz)]
            yard = (sx, face_yard + s * 2.0, 0.0)
            yard_in = [yard, (sx, face_yard + s * 0.15, gz - 0.15), (sx, ye - s * 0.3, gz), (sx, yc, gz)]
            # under the half landing over the yard door: its slab's underside over the lobby floor
            yard_clear = round((FLOOR_H / 2 - 0.25), 2)
            floors = []
            for k in range(F):
                route = pts[:2 + 6 * k]
                if street is None:
                    route = yard_in + route[2:] if k else yard_in
                else:
                    route = street_in + route
                floors.append({"floor": k, "path": xf.path(route)})
            stairs.append({"id": sid, "house": hname, "entrance": si + 1, "street": rp(xf.pt(street)) if street else None,
                           "yard": rp(xf.pt(yard)), "yard_path": xf.path(yard_in), "yard_clear_m": yard_clear,
                           "yard_passable": yard_clear >= MIN_CLEAR_H, "floors": floors,
                           "_house_xy": {"street": street, "yard": yard}})
        # ---- flats: a corridor starts each flat, its rooms follow it
        groups = []
        for r in rooms:
            if r["kind"] == "stair":
                continue
            if r["kind"] == "corridor":
                groups.append([r])
            else:
                groups[-1].append(r)
        recs = []
        for g in groups:
            corr = g[0]
            d0 = corr["doors"][0]
            side = -1 if d0["side"] == "x1" else 1
            edge = corr["x1"] if side < 0 else corr["x0"]
            sx = min(H["stairs"], key=lambda v: abs(v - (edge - side * (interiors.CELL_HALF + interiors.CELL_T / 2))))
            k = int(round((corr["z"] - floor_z(0)) / FLOOR_H))
            recs.append((H["stairs"].index(sx), k, side, sx, g))
        recs.sort(key=lambda t: (t[0], t[1], t[2]))
        for n, (si, k, side, sx, g) in enumerate(recs, 1):
            fl = flat_entry(hname, xf, H, si, k, side, sx, g, n, st_ids)
            flats.append(fl)
            plot.append((hname, k, fl))
    return stairs, flats, plot


def flat_entry(hname, xf, H, si, k, side, sx, g, number, st_ids):
    import interiors
    corr = g[0]
    z = corr["z"]
    yc = H["yc"]
    s = H["yard_sign"]
    ch, ct = interiors.CELL_HALF, interiors.CELL_T
    cx0, cx1 = sx - ch + ct / 2, sx + ch - ct / 2                    # the stair cell's inside
    yb = yc + s * interiors.BAND
    ly0, ly1 = sorted((H["street_in"], yb))
    # the plan: the landing, the corridor, the rooms, the doorways; furniture blocks
    xs = [cx0, cx1] + [r["x0"] for r in g] + [r["x1"] for r in g]
    ys = [ly0, ly1] + [r["y0"] for r in g] + [r["y1"] for r in g]
    P = Plan(min(xs) - 0.5, min(ys) - 0.5, max(xs) + 0.5, max(ys) + 0.5)
    P.room(cx0, cx1, ly0, ly1)
    for r in g:
        P.room(r["x0"], r["x1"], r["y0"], r["y1"])
    # doorways (after the rooms, before the furniture)
    fw = interiors.FLAT_DOOR_W
    edge = corr["x1"] if side < 0 else corr["x0"]
    wall_x = (edge, cx0) if side < 0 else (cx1, edge)
    hw = fw / 2 - DOOR_SIDE
    P.rect(min(wall_x) - 0.45, max(wall_x) + 0.45, yc - hw, yc + hw, 1)
    for r in g:
        for d in r["doors"]:
            if r is corr and d is corr["doors"][0]:
                continue
            hw = d["w"] / 2 - DOOR_SIDE
            if d["side"] in ("y0", "y1"):
                wc = r[d["side"]]
                P.rect(d["c"] - hw, d["c"] + hw, wc - 0.5, wc + 0.5, 1)
            else:
                wc = r[d["side"]]
                P.rect(wc - 0.5, wc + 0.5, d["c"] - hw, d["c"] + hw, 1)
    for r in g:
        for b in r["boxes"]:
            P.block(b[1], b[2], b[3], b[4])
    door = (edge - side * (ct + 0.45), yc)                           # on the landing in front of the flat's door
    P.flood(door)
    A = Approacher(P)

    def way(goal):
        w = P.way(goal)
        if w is None:
            return None
        return xf.path([(p[0], p[1], z) for p in w])

    room_out, slot_out, misses = [], [], []
    for ri, r in enumerate(g):
        c = ((r["x0"] + r["x1"]) / 2, (r["y0"] + r["y1"]) / 2)
        cc = c if (P.free(c) and P.reached(c)) else P.nearest_reached(c, 80)
        room_out.append({"kind": r["kind"], "center": rp(xf.pt((cc[0], cc[1], z))) if cc else None,
                         "path": way(cc) if cc else None, "_c": cc})
        if cc is None:
            misses.append("room %s centre unreachable" % r["kind"])
        for seat in r.get("seats", []):
            kind = flat_slot_kind(seat["item"], seat["part"], r["kind"])
            if kind is None:
                continue
            xy, face = seat["xy"], seat["face"]
            bb = seat["bb"]
            spots, cap = None, 1
            if kind == "bed":
                w = bb[1] - bb[0]
                L = bb[3] - bb[2]
                cands = [(0.0, w / 2 + 0.42), (0.0, -w / 2 - 0.42), (0.25, w / 2 + 0.42), (0.25, -w / 2 - 0.42),
                         (L / 2 + 0.42, 0.0), (-0.3, w / 2 + 0.42), (-0.3, -w / 2 - 0.42)]
                if w > 1.25:
                    cap = 2
                    rot = seat["rot"]
                    spots = [(xy[0] + math.cos(rot) * d, xy[1] + math.sin(rot) * d) for d in (-w / 4, w / 4)]
            elif kind == "sofa":
                w = bb[1] - bb[0]
                cap = 3 if w >= 1.75 else 2
                rot = seat["rot"]
                offs = [-0.6, 0.0, 0.6] if cap == 3 else [-0.35, 0.35]
                spots = [(xy[0] + math.cos(rot) * d, xy[1] + math.sin(rot) * d) for d in offs]
                cands = CANDS["sofa"]
            elif kind == "armchair":
                cands = CANDS["armchair"]
            elif seat["item"] == "stool":
                cands = CANDS["wall_stool"]
            else:
                cands = CANDS["chair_at_table"]
            ap = A.pick(xy, face, cands, (r["x0"], r["x1"], r["y0"], r["y1"]))
            if ap is None:
                misses.append("%s in %s: no approach" % (kind, r["kind"]))
                continue
            e = {"kind": kind, "room": ri, "pos": rp(xf.pt((xy[0], xy[1], z))), "yaw": xf.yaw((xy[0], xy[1], z), face),
                 "capacity": cap, "height": r2(seat["height"]), "approach": rp(xf.pt((ap[0], ap[1], z))),
                 "path": way(ap)}
            if spots:
                e["spots"] = [rp(xf.pt((p[0], p[1], z))) for p in spots]
            if e["path"] is None:
                misses.append("%s in %s: approach unreachable" % (kind, r["kind"]))
            e["_xy"], e["_ap"], e["_face"] = xy, ap, face
            slot_out.append(e)
    sid, _ = st_ids[sx]
    return {"id": "%s-%d-%d" % (hname, si + 1, number), "house": hname, "stair": sid, "floor": k, "number": number,
            "door": rp(xf.pt((door[0], door[1], z))), "rooms": room_out, "slots": slot_out,
            "_plan": P, "_g": g, "_door": door, "_misses": misses, "_xf": xf, "_z": z}


# --- the library -----------------------------------------------------------------------------------------------

def build_library(data, h_frame):
    import library as LIB
    xf = HouseXf(frame_of(h_frame))
    Z0 = LIB.Z0
    hx0, hx1, hy0, hy1 = LIB.HALL
    vx0, vx1, vy0, vy1 = LIB.VEST
    P = Plan(vx0 - 1.0, LIB.YO - 3.0, hx1 + 0.5, hy1 + 0.5)
    P.room(vx0, vx1, vy0, vy1)
    P.room(hx0, hx1, hy0, hy1)
    P.rect(vx1 - 0.5, hx0 + 0.5, LIB.ARCH_Y[0] + DOOR_SIDE, LIB.ARCH_Y[1] - DOOR_SIDE, 1)    # the arch
    ex, _, ew, _ = LIB.ENTRANCE
    P.rect(ex - ew / 2 + DOOR_SIDE, ex + ew / 2 - DOOR_SIDE, LIB.YO - 0.6, LIB.YI + 0.5, 1)   # the entrance
    P.rect(ex - 1.3, ex + 1.3, LIB.YO - 2.6, LIB.YO - 0.3, 1)                                 # the steps outside
    lib = data["library"]
    fine = {}
    for e in lib["placed"]:
        if e["boxes"]:
            fine.setdefault(e["name"], []).extend(e["boxes"])
    for name, x0, x1, y0, y1 in lib["footprints"]:
        if name in fine:
            continue
        P.block(x0, x1, y0, y1)
    for name, bxs in fine.items():
        for b in bxs:
            P.block(b[0], b[1], b[2], b[3])
    door = (ex, LIB.YO - 2.0)
    P.flood(door)
    A = Approacher(P)

    def zp(p, z=None):
        if z is None:
            z = 0.0 if p[1] < LIB.YO - 0.75 else (Z0 - 0.15 if p[1] < LIB.YO + 0.1 else Z0)
        return (p[0], p[1], z)

    def way(goal):
        w = P.way(goal)
        if w is None:
            return None
        return xf.path([zp(p) for p in w])

    slots, staff, misses = [], None, []
    for e in lib["placed"]:
        for st in e["seats"]:
            xy, face = st["xy"], st["face"]
            if e["name"] == "issue_desk":
                tag, cands = "staff", CANDS["chair_at_table"]
            elif e["name"] == "reading_table":
                tag, cands = "table", CANDS["chair_at_table"]
            elif e["name"] == "newspaper_table":
                tag, cands = "newspaper", CANDS["chair_at_table"]
            elif e["name"] == "armchair":
                tag, cands = "nook", CANDS["armchair"]
            else:
                continue
            ap = A.pick(xy, face, cands)
            if ap is None:
                misses.append("library %s: no approach" % tag)
                continue
            s = {"kind": "staff_seat" if tag == "staff" else "read_seat", "tag": tag,
                 "pos": rp(xf.pt((xy[0], xy[1], Z0))), "yaw": xf.yaw((xy[0], xy[1], Z0), face),
                 "approach": rp(xf.pt((ap[0], ap[1], Z0))), "height": r2(st["height"]), "capacity": 1, "path": way(ap),
                 "_xy": xy, "_ap": ap, "_face": face}
            if s["path"] is None:
                misses.append("library %s: approach unreachable" % tag)
            if tag == "staff":
                staff = s
            else:
                slots.append(s)
    hall_c = P.nearest_reached((10.6, -2.45), 40)
    place = {"id": "library", "kind": "library", "door": rp(xf.pt(zp(door))),
             "yaw": xf.yaw(zp(door), math.pi / 2), "path": way(hall_c), "slots": slots, "staff": staff,
             "_plan": P, "_xf": xf, "_misses": misses, "_door": door}
    return place


# --- the club (scene01_layout.json, Unreal cm) -----------------------------------------------------------------

def build_club(scene):
    cm = lambda p: [r2(p[0] / 100.0), r2(p[1] / 100.0), r2(p[2] / 100.0)]
    ex = scene["exit_door"]["center"]
    face = scene.get("outside", {}).get("portal_face_y", FACE_Y)
    pd = scene["door"]["center"]
    wk = scene["walkway"]
    wx = (wk["x"][0] + wk["x"][1]) / 2
    wy0 = wk["y0"]
    belt_z = 14.0
    entry = [(ex[0], face + 550.0, 0.0), (ex[0], face, 0.0), (ex[0], ex[1], 0.0), (ex[0], ex[1] - 110.0, 0.0),
             (pd[0], 130.0, 0.0), (pd[0], pd[1], 0.0), (pd[0], wy0 - 10.0, 0.0), (wx, wy0 - 20.0, belt_z)]
    aisle_x = (wk["x"][1] + 20.0) / 2 - 10.0          # the narrow aisle between the walkway and the chairs' backs
    seats = []
    for st in scene["stations"]:
        cx, cy, _ = st["chair"]
        yaw = float(st.get("sitter_yaw", 0.0))
        seats.append({"index": st["index"], "kind": "club_seat", "station": st["kind"], "chair": st["chair_kind"],
                      "reachable": bool(st.get("reachable", True)), "pos": cm((cx, cy, 0.0)), "yaw": ang(yaw),
                      "height": 0.47, "approach": cm((aisle_x, cy, 0.0)),
                      "path": [cm((wx, wy0 - 20.0, belt_z)), cm((wx, cy, belt_z)), cm((aisle_x, cy, 0.0))]})
    clerk = scene.get("clerk", {})
    for s in seats:
        if clerk and abs(s["pos"][1] * 100 - clerk["chair"][1]) < 1.0:
            s["staff"] = "clerk"
    hero = scene.get("hero")
    v = scene["vending"]
    vend = [{"kind": "vending", "tag": "drinks", "pos": cm((v[0] + 75.0, v[1], 0.0)), "yaw": 180.0, "machine": cm(v)}]
    for hl in scene.get("hall_lights", []):
        p = hl["pos"]
        if abs(p[0] - (v[0] + 80.0)) < 10 and abs(p[1] - v[1] - 155.0) < 10:     # the snack machine's light
            vend.append({"kind": "vending", "tag": "snacks", "pos": cm((v[0] + 78.0, p[1], 0.0)), "yaw": 180.0,
                         "machine": cm((v[0] + 2.0, p[1], 100.0))})
    ow = scene["owner"]
    wn = ow["window"]
    wyaw = math.radians(ow.get("window_yaw", 180.0))
    booth = {"owner": {"kind": "booth", "pos": cm(ow["chair"]), "yaw": ang(ow.get("sitter_yaw", 0.0)), "height": 0.47},
             "window": cm((wn[0], wn[1], 0.0)),
             "customer": {"kind": "stand", "pos": cm((wn[0] + 60.0 * math.cos(wyaw), wn[1] + 60.0 * math.sin(wyaw), 0.0)),
                          "yaw": ang(ow.get("window_yaw", 180.0) + 180.0)}}
    hall_chair = {"kind": "chair", "tag": "hall_newspaper", "pos": [-2.55, 5.2, 0.0], "yaw": 0.0, "height": 0.465,
                  "approach": [-2.0, 5.2, 0.0]}
    return {"portal": cm((ex[0], face + 550.0, 0.0)), "entry_path": [cm(p) for p in entry],
            "walkway": {"x": [r2(wk["x"][0] / 100), r2(wk["x"][1] / 100)], "y0": r2(wy0 / 100), "z": r2(belt_z / 100)},
            "seats": seats, "hero_seat": ({"pos": cm(hero["chair"]), "yaw": ang(hero.get("chair_yaw", 0.0)),
                                           "height": r2(hero.get("seat_height", 47.0) / 100)} if hero else None),
            "vending": vend, "booth": booth, "hall_chair": hall_chair}


# --- outdoors ----------------------------------------------------------------------------------------------------

def bench(pid, kind, centre, face, grid, n=3, length=1.8, tag="", island_of=None):
    """A bench: centre (Unreal m x, y), the sitters' facing (deg); n sitters along it."""
    a = math.radians(face)
    fx, fy = math.cos(a), math.sin(a)
    tx, ty = -fy, fx
    slots = []
    for k in range(n):
        d = (k - (n - 1) / 2) * (length / n)
        x, y = centre[0] + tx * d, centre[1] + ty * d
        ax, ay = x + fx * 0.55, y + fy * 0.55
        slots.append({"kind": "bench", "pos": [r2(x), r2(y), r2(grid.ground(x, y))], "yaw": ang(face), "height": 0.45,
                      "capacity": 1, "approach": [r2(ax), r2(ay), r2(grid.ground(ax, ay))]})
    p = {"id": pid, "kind": kind, "pos": [r2(centre[0]), r2(centre[1]), r2(grid.ground(*centre))], "yaw": ang(face),
         "slots": slots, "tag": tag}
    return p


def stand(pid, kind, xy, face, grid, n=1, spread=1.2, tag=""):
    a = math.radians(face)
    tx, ty = -math.sin(a), math.cos(a)
    slots = []
    for k in range(n):
        d = (k - (n - 1) / 2) * spread
        x, y = xy[0] + tx * d, xy[1] + ty * d
        slots.append({"kind": "stand", "pos": [r2(x), r2(y), r2(grid.ground(x, y))], "yaw": ang(face), "capacity": 1,
                      "height": 0.0, "approach": [r2(x), r2(y), r2(grid.ground(x, y))]})
    return {"id": pid, "kind": kind, "pos": [r2(xy[0]), r2(xy[1]), r2(grid.ground(*xy))], "yaw": ang(face),
            "slots": slots, "tag": tag}


def face_to(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def build_outdoor(grid):
    import plan
    places = []
    fy = FACE_Y / 100.0
    E = lambda x, y: ens_to_ue((x, y, 0.0))[:2]
    # the Academgorodok's square: benches round the fountain's ring facing it, standing spots on the ring
    fxe, fye = plan.SQUARE["fountain"]
    fc = E(fxe, fye)
    ring = plan.SQUARE["bed_r"] + 2.2
    for k, deg in enumerate((45, 135, 225, 315)):
        t = math.radians(deg)
        c = E(fxe + (ring + 0.9) * math.cos(t), fye + (ring + 0.9) * math.sin(t))
        places.append(bench("academ_bench_%d" % k, "bench", c, face_to(c, fc), grid, tag="academ_square"))
    for k, deg in enumerate((90, 270)):
        t = math.radians(deg)
        c = E(fxe + (ring + 0.9) * math.cos(t), fye + (ring + 0.9) * math.sin(t))
        places.append(bench("academ_bench_%d" % (k + 4), "bench", c, face_to(c, fc), grid, tag="academ_square"))
    sq = stand("academ_square", "square", E(fxe + ring - 0.9, fye), 0.0, grid, n=0, tag="academ")
    for k in range(8):
        t = 2 * math.pi * k / 8 + math.pi / 8
        x, y = E(fxe + (ring - 0.9) * math.cos(t), fye + (ring - 0.9) * math.sin(t))
        yaw = face_to((x, y), fc)
        sq["slots"].append({"kind": "stand", "pos": [r2(x), r2(y), r2(grid.ground(x, y))], "yaw": ang(yaw),
                            "capacity": 1, "height": 0.0, "approach": [r2(x), r2(y), r2(grid.ground(x, y))]})
    sq["pos"] = [r2(fc[0]), r2(fc[1]), r2(grid.ground(*fc))]
    places.append(sq)
    # the cable car's stations: the entrance spot (the stations themselves are blocked on the walk grid)
    up, dn = W.STATION_UP, W.STATION_DOWN
    p = map_to_ue(up[0], up[1] - 9.0, 0.0)
    places.append(stand("station_upper", "station_upper", p[:2], face_to(p, map_to_ue(*up, 0.0)), grid, n=3, tag="spur"))
    places[-1]["station"] = [r2(map_to_ue(*up, 0.0)[0]), r2(map_to_ue(*up, 0.0)[1]), r2(W.height(*up))]
    p = map_to_ue(dn[0] + 9.0, dn[1], 0.0)
    places.append(stand("station_lower", "station_lower", p[:2], face_to(p, map_to_ue(*dn, 0.0)), grid, n=3, tag="valley"))
    places[-1]["station"] = [r2(map_to_ue(*dn, 0.0)[0]), r2(map_to_ue(*dn, 0.0)[1]), r2(W.height(*dn))]
    # the club's door: outside the portal on the forecourt, facing the door (-Y)
    p = map_to_ue(W.DOOR_X, 5.5, 0.0)
    cd = stand("club_door", "club_door", p[:2], -90.0, grid, n=1, tag="spur")
    cd["portal"] = [r2(W.DOOR_X), r2(fy), 0.0]
    places.append(cd)
    # the spur's lookout over the sea (west = +X), and benches along the alley between its tree pairs
    p = map_to_ue(W.DOOR_X - 8.0, 60.0, 0.0)
    places.append(stand("spur_lookout", "lookout", p[:2], 0.0, grid, n=3, tag="spur"))
    pts = W.path_points(1.0)
    for k, y in enumerate((24.5, 50.5, 76.5)):
        cx = min(pts, key=lambda q: abs(q[1] - y))[0]
        side = 1 if k % 2 == 0 else -1                    # +1: the sea side (west, +X)
        c = map_to_ue(cx + side * 2.9, y, 0.0)
        places.append(bench("spur_bench_%d" % k, "bench", c[:2], 180.0 if side > 0 else 0.0, grid, tag="spur_alley"))
    # the beach and the promenade (every 60 m, facing the sea, +X)
    for k, y in enumerate(range(int(W.PROMENADE_Y[0]) + 30, int(W.PROMENADE_Y[1]), 60)):
        x = W.shore_x(y) - 10.0
        places.append(stand("beach_%d" % k, "beach", map_to_ue(x, y, 0.0)[:2], 0.0, grid, n=4, spread=1.5, tag="beach"))
        xb = W.shore_x(y) - (W.BEACH_W - 5.0) - 1.6
        places.append(bench("promenade_bench_%d" % k, "promenade_bench", map_to_ue(xb, y, 0.0)[:2], 0.0, grid,
                            tag="promenade"))
    # the town square: benches round its two halves, facing their middles
    cx, cy = W.TOWN_CENTER
    for k, (ox, oy) in enumerate(((-30.0, -25.0), (35.0, 30.0))):
        c0 = map_to_ue(cx + ox, cy + oy, 0.0)
        places.append(stand("town_square_%d" % k, "town_square", c0[:2], 0.0, grid, n=0, tag="town"))
        for j in range(4):
            t = W.TOWN_ROT + math.pi / 4 + j * math.pi / 2
            c = map_to_ue(cx + ox + 16.0 * math.cos(t), cy + oy + 16.0 * math.sin(t), 0.0)
            places.append(bench("town_bench_%d_%d" % (k, j), "town_bench", c[:2], face_to(c, c0), grid, tag="town_square"))
        sq = places[-5]
        for j in range(6):
            t = 2 * math.pi * j / 6
            x, y = c0[0] + 6.0 * math.cos(t), c0[1] + 6.0 * math.sin(t)
            sq["slots"].append({"kind": "stand", "pos": [r2(x), r2(y), r2(grid.ground(x, y))],
                                "yaw": ang(face_to((x, y), c0)), "capacity": 1, "height": 0.0,
                                "approach": [r2(x), r2(y), r2(grid.ground(x, y))]})
    return places


# --- checks --------------------------------------------------------------------------------------------------------

def check_outdoor(grid, places, stairs, club, library):
    bad = []
    valley = grid.component(*places_by(places, "station_lower")["slots"][0]["pos"][:2])
    spur = grid.component(*places_by(places, "club_door")["slots"][0]["pos"][:2])

    def isl(x, y):
        c = grid.component(x, y)
        return 0 if c == valley else (1 if c == spur else (-1 if c == 0 else 2 + c))

    for p in places:
        pts = [("pos", p["pos"])] + [("slot%d" % i, s["pos"]) for i, s in enumerate(p["slots"])] + \
              [("approach%d" % i, s["approach"]) for i, s in enumerate(p["slots"])]
        for lab, q in pts:
            if not grid.walkable(q[0], q[1]):
                bad.append("%s %s (%.1f, %.1f)" % (p["id"], lab, q[0], q[1]))
        anchor = p["slots"][0]["approach"] if p["slots"] else p["pos"]
        p["island"] = isl(anchor[0], anchor[1])
    for s in stairs:
        for lab in ("street", "yard"):
            q = s[lab]
            if q is None:
                continue
            if not grid.walkable(q[0], q[1]):
                bad.append("%s %s (%.1f, %.1f)" % (s["id"], lab, q[0], q[1]))
            s[lab + "_island"] = isl(q[0], q[1])
    q = library["door"]
    if not grid.walkable(q[0], q[1]):
        bad.append("library door (%.1f, %.1f)" % (q[0], q[1]))
    library["island"] = isl(q[0], q[1])
    q = club["portal"]
    if not grid.walkable(q[0], q[1]):
        bad.append("club portal (%.1f, %.1f)" % (q[0], q[1]))
    club["island"] = isl(q[0], q[1])
    return bad


def places_by(places, pid):
    return next(p for p in places if p["id"] == pid)


def strip(o):
    if isinstance(o, dict):
        return {k: strip(v) for k, v in o.items() if not k.startswith("_")}
    if isinstance(o, list):
        return [strip(v) for v in o]
    return o


# --- plots ----------------------------------------------------------------------------------------------------------

def plots(stairs, flats, plot_rows, library, club, places, grid, data):
    from PIL import Image, ImageDraw
    os.makedirs(PLOTS, exist_ok=True)
    COL = {"bed": (40, 90, 200), "sofa": (200, 60, 160), "armchair": (220, 120, 0), "kitchen_seat": (0, 150, 60),
           "dining_seat": (170, 30, 30), "desk_seat": (110, 60, 20), "read_seat": (170, 30, 30), "staff_seat": (0, 120, 160),
           "bench": (170, 30, 30), "stand": (40, 90, 200), "club_seat": (170, 30, 30), "promenade_bench": (170, 30, 30),
           "town_bench": (170, 30, 30)}
    ROOMC = {"living": (250, 240, 215), "study": (230, 240, 250), "bedroom": (240, 230, 245), "kitchen": (230, 250, 230),
             "corridor": (238, 238, 238), "bath": (225, 245, 250), "stair": (215, 215, 215)}

    class View:
        def __init__(self, pts, s=40, pad=1.5):
            xs, ys = [p[0] for p in pts], [p[1] for p in pts]
            self.x0, self.y0, self.s = min(xs) - pad, min(ys) - pad, s
            self.w = int((max(xs) + pad - self.x0) * s)
            self.h = int((max(ys) + pad - self.y0) * s)

        def __call__(self, p):
            return ((p[0] - self.x0) * self.s, (p[1] - self.y0) * self.s)

    def arrow(d, v, p, yaw, col, L=0.45, w=2):
        a = math.radians(yaw)
        q = (p[0] + L * math.cos(a), p[1] + L * math.sin(a))
        P, Q = v(p), v(q)
        d.line([P, Q], fill=col, width=w)
        for s in (-1, 1):
            b = a + math.pi + s * 0.5
            h = (q[0] + 0.15 * math.cos(b), q[1] + 0.15 * math.sin(b))
            d.line([Q, v(h)], fill=col, width=w)
        r = 4
        d.ellipse([P[0] - r, P[1] - r, P[0] + r, P[1] + r], outline=col, width=2)

    def poly_h(d, v, xf, x0, x1, y0, y1, z, **kw):
        d.polygon([v(xf.pt((x, y, z))) for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))], **kw)

    def slot_draw(d, v, s):
        col = COL.get(s["kind"], (0, 0, 0))
        if s.get("path"):
            d.line([v(p) for p in s["path"]], fill=(150, 150, 255), width=1)
        ap = s["approach"]
        A = v(ap)
        d.rectangle([A[0] - 3, A[1] - 3, A[0] + 3, A[1] + 3], fill=(0, 170, 0))
        d.line([A, v(s["pos"])], fill=(0, 170, 0), width=1)
        arrow(d, v, s["pos"], s["yaw"], col)
        for sp in s.get("spots", []):
            Q = v(sp)
            d.ellipse([Q[0] - 2, Q[1] - 2, Q[0] + 2, Q[1] + 2], fill=col)

    # ---- a plan per house storey
    by = {}
    for hname, k, fl in plot_rows:
        by.setdefault((hname, k), []).append(fl)
    for (hname, k), fls in sorted(by.items()):
        H = data["houses"][hname]
        xf = fls[0]["_xf"]
        z = fls[0]["_z"]
        allp = []
        for r in H["rooms"]:
            if r["kind"] == "stair" or abs(r["z"] - z) < 0.01:
                for x in (r["x0"], r["x1"]):
                    for y in (r["y0"], r["y1"]):
                        allp.append(xf.pt((x, y, z)))
        v = View(allp, 32)
        im = Image.new("RGB", (v.w, v.h), (255, 255, 255))
        d = ImageDraw.Draw(im)
        for r in H["rooms"]:
            if r["kind"] == "stair":
                poly_h(d, v, xf, r["x0"], r["x1"], r["y0"], r["y1"], z, fill=ROOMC["stair"], outline=(90, 90, 90))
        for fl in fls:
            for r in fl["_g"]:
                poly_h(d, v, xf, r["x0"], r["x1"], r["y0"], r["y1"], z, fill=ROOMC.get(r["kind"], (240, 240, 240)),
                       outline=(60, 60, 60))
                for dd in r["doors"]:
                    hw = dd["w"] / 2
                    if dd["side"] in ("y0", "y1"):
                        a, b = xf.pt((dd["c"] - hw, r[dd["side"]], z)), xf.pt((dd["c"] + hw, r[dd["side"]], z))
                    else:
                        a, b = xf.pt((r[dd["side"]], dd["c"] - hw, z)), xf.pt((r[dd["side"]], dd["c"] + hw, z))
                    d.line([v(a), v(b)], fill=(255, 255, 255), width=4)
            for r in fl["_g"]:
                for b in r.get("boxes", []):
                    poly_h(d, v, xf, b[1], b[2], b[3], b[4], z, outline=(150, 110, 70))
        for fl in fls:
            for rm in fl["rooms"]:
                if rm["path"]:
                    d.line([v(p) for p in rm["path"]], fill=(255, 150, 150), width=1)
            for s in fl["slots"]:
                slot_draw(d, v, s)
            D = v(fl["door"])
            d.ellipse([D[0] - 5, D[1] - 5, D[0] + 5, D[1] + 5], fill=(255, 0, 0))
            d.text((D[0] + 6, D[1] - 6), "%d" % fl["number"], fill=(200, 0, 0))
        for s in stairs:
            if s["house"] != hname:
                continue
            fp = s["floors"][k]["path"]
            d.line([v(p) for p in fp], fill=(255, 120, 0), width=2)
            for lab in ("street", "yard"):
                if s[lab]:
                    Q = v(s[lab])
                    d.rectangle([Q[0] - 5, Q[1] - 5, Q[0] + 5, Q[1] + 5], fill=(255, 120, 0) if lab == "street" else (120, 120, 120))
        d.text((10, 10), "%s storey %d  (UE top view: X right, Y down; arrows = yaw; green = approach)" % (hname, k), fill=(0, 0, 0))
        im.save(os.path.join(PLOTS, "%s_%d.png" % (hname, k)))
    # ---- the library
    L = library
    xf = L["_xf"]
    P = L["_plan"]
    import library as LIB
    pts = [xf.pt((x, y, LIB.Z0)) for x in (LIB.VEST[0] - 1, LIB.HALL[1]) for y in (LIB.YO - 3, LIB.HALL[3])]
    v = View(pts, 50)
    im = Image.new("RGB", (v.w, v.h), (255, 255, 255))
    d = ImageDraw.Draw(im)
    for j in range(P.ny):
        for i in range(P.nx):
            if P.ok[j * P.nx + i] == 1:
                c = P.centre(i, j)
                Q = v(xf.pt((c[0], c[1], LIB.Z0)))
                d.point(Q, fill=(200, 230, 200) if P.dist[j * P.nx + i] < 1e17 else (240, 200, 200))
    for name, x0, x1, y0, y1 in data["library"]["footprints"]:
        poly_h(d, v, xf, x0, x1, y0, y1, LIB.Z0, outline=(150, 110, 70))
    for e in data["library"]["placed"]:
        for b in e["boxes"] or []:
            poly_h(d, v, xf, b[0], b[1], b[2], b[3], LIB.Z0, outline=(200, 80, 80))
    for rect in (LIB.VEST, LIB.HALL):
        poly_h(d, v, xf, rect[0], rect[1], rect[2], rect[3], LIB.Z0, outline=(0, 0, 0))
    if L["path"]:
        d.line([v(p) for p in L["path"]], fill=(255, 120, 0), width=3)
    for s in L["slots"] + [L["staff"]]:
        slot_draw(d, v, s)
    D = v(L["door"])
    d.ellipse([D[0] - 6, D[1] - 6, D[0] + 6, D[1] + 6], fill=(255, 0, 0))
    d.text((10, 10), "library (UE top view)", fill=(0, 0, 0))
    im.save(os.path.join(PLOTS, "library.png"))
    # ---- the club
    C = club
    pts = [(-3.2, 9.5), (3.2, -9.0)]
    v = View(pts, 40, 0.5)
    im = Image.new("RGB", (v.w, v.h), (255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([v((-3.0, 0.0)), v((3.0, 7.0))], outline=(0, 0, 0))
    d.rectangle([v((-1.7, -9.0)), v((1.7, 0.0))], outline=(0, 0, 0))
    d.rectangle([v((C["walkway"]["x"][0], -9.0)), v((C["walkway"]["x"][1], C["walkway"]["y0"]))], fill=(220, 220, 220))
    d.line([v(p) for p in C["entry_path"]], fill=(255, 120, 0), width=3)
    for s in C["seats"]:
        if s["pos"][1] > -9.0:
            d.rectangle([v((1.295 - 0.4, s["pos"][1] - 0.5)), v((1.695, s["pos"][1] + 0.5))], outline=(150, 110, 70))
            slot_draw(d, v, s)
    for s in C["vending"]:
        arrow(d, v, s["pos"], s["yaw"], (0, 120, 160))
    b = C["booth"]
    arrow(d, v, b["owner"]["pos"], b["owner"]["yaw"], (120, 0, 120))
    arrow(d, v, b["customer"]["pos"], b["customer"]["yaw"], (0, 120, 160))
    d.text((10, 10), "club (UE top view; first 9 m of the corridor)", fill=(0, 0, 0))
    im.save(os.path.join(PLOTS, "club.png"))
    # ---- the world
    sc = 1
    im = Image.new("RGB", (grid.nx * sc, grid.ny * sc), (255, 255, 255))
    pal = {0: (90, 90, 90), 1: (200, 225, 170), 2: (215, 215, 215), 3: (235, 225, 200), 4: (245, 235, 180), 5: (150, 190, 140)}
    px = im.load()
    for j in range(grid.ny):
        for i in range(grid.nx):
            px[i, j] = pal.get(grid.kind[j * grid.nx + i] & 7, (0, 0, 0))
    big = im.resize((grid.nx * 2, grid.ny * 2), Image.NEAREST)
    d = ImageDraw.Draw(big)
    g = lambda p: ((p[0] - grid.x0) / grid.cell * 2, (p[1] - grid.y0) / grid.cell * 2)
    for s in stairs:
        for lab in ("street", "yard"):
            if s[lab]:
                Q = g(s[lab])
                d.rectangle([Q[0] - 2, Q[1] - 2, Q[0] + 2, Q[1] + 2], fill=(255, 120, 0) if lab == "street" else (0, 0, 255))
    for p in places + [{"pos": library["door"], "slots": []}, {"pos": club["portal"], "slots": []}]:
        Q = g(p["pos"])
        d.ellipse([Q[0] - 4, Q[1] - 4, Q[0] + 4, Q[1] + 4], outline=(200, 0, 0), width=2)
        for s in p["slots"]:
            Q = g(s["pos"])
            d.point(Q, fill=(200, 0, 0))
    big.save(os.path.join(PLOTS, "world.png"))
    # zoom on the Academgorodok and the spur
    for name, (cx, cy, half) in {"world_academ": (ens_to_ue((30.0, 0.0, 0))[0], ens_to_ue((30.0, 0.0, 0))[1], 120.0),
                                  "world_spur": (-10.0, FACE_Y / 100 + 70.0, 80.0)}.items():
        S = 6
        v = View([(cx - half, cy - half), (cx + half, cy + half)], S, 0)
        im = Image.new("RGB", (v.w, v.h), (255, 255, 255))
        d = ImageDraw.Draw(im)
        for j in range(grid.ny):
            y = grid.y0 + j * grid.cell
            if abs(y - cy) > half + 2:
                continue
            for i in range(grid.nx):
                x = grid.x0 + i * grid.cell
                if abs(x - cx) > half + 2:
                    continue
                a, b = v((x - grid.cell / 2, y - grid.cell / 2)), v((x + grid.cell / 2, y + grid.cell / 2))
                d.rectangle([a, b], fill=pal.get(grid.kind[j * grid.nx + i] & 7))
        for s in stairs:
            for lab in ("street", "yard"):
                if s[lab]:
                    Q = v(s[lab])
                    d.rectangle([Q[0] - 3, Q[1] - 3, Q[0] + 3, Q[1] + 3], fill=(255, 120, 0) if lab == "street" else (0, 0, 255))
        for fl in flats:
            Q = v(fl["door"])
            d.point(Q, fill=(255, 0, 0))
        for p in places:
            for s in p["slots"]:
                if s["kind"] != "stand":
                    slot_draw(d, v, s)
                else:
                    arrow(d, v, s["pos"], s["yaw"], (40, 90, 200), 1.0)
        for q in (library["door"], club["portal"]):
            Q = v(q)
            d.ellipse([Q[0] - 5, Q[1] - 5, Q[0] + 5, Q[1] + 5], outline=(255, 0, 0), width=2)
        if name == "world_academ" and library["path"]:
            d.line([v(p) for p in library["path"]], fill=(255, 120, 0), width=2)
        im.save(os.path.join(PLOTS, name + ".png"))


# --- main -------------------------------------------------------------------------------------------------------------

def main():
    global FACE_Y
    ap = argparse.ArgumentParser()
    ap.add_argument("--face-y", type=float, default=790.0, help="portal face, Unreal Y cm (main's outside.portal_face_y)")
    ap.add_argument("--plot", action="store_true")
    ap.add_argument("--cache", default="", help="pickle of the Academgorodok's extraction (rebuilt when missing)")
    ap.add_argument("--scene", default=MAIN_SCENE, help="scene01_layout.json with 'outside' (the main checkout's)")
    a = ap.parse_args()
    FACE_Y = a.face_y
    if a.cache and os.path.exists(a.cache):
        with open(a.cache, "rb") as f:
            data = pickle.load(f)
        import library  # noqa: F401
    else:
        data = extract_houses()
        if a.cache:
            with open(a.cache, "wb") as f:
                pickle.dump(data, f)
    grid = WalkGrid(GRID)
    stairs, flats, plot_rows = build_flats(data)
    library = build_library(data, data["houses"]["Novikova1"]["frame"])
    with open(a.scene) as f:
        scene = json.load(f)
    club = build_club(scene)
    places = build_outdoor(grid)
    bad = check_outdoor(grid, places, stairs, club, library)
    misses = [m for fl in flats for m in fl["_misses"]] + library["_misses"]
    doc = {"version": 1, "frame": "unreal_metres", "face_y_cm": FACE_Y,
           "yaw": "Unreal degrees, 0 = +X, 90 = +Y; seats: the way the seated person faces; beds: head to foot; stand: the way the person looks",
           "islands": "0 the valley (town, Academgorodok, beach), 1 the club's spur, -1 not on the walk grid",
           "stairs": strip(stairs), "flats": strip(flats),
           "places": strip([dict(library, kind="library", id="library")] + places), "club": strip(club)}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(doc, f, separators=(",", ":"))
    counts = {}
    for fl in flats:
        for s in fl["slots"]:
            counts[s["kind"]] = counts.get(s["kind"], 0) + 1
    kinds = {}
    for p in places:
        kinds[p["kind"]] = kinds.get(p["kind"], 0) + 1
    print("people_map: %d stairs, %d flats, flat slots %s, library %d read seats + staff, %d places %s, club %d seats -> %s"
          % (len(stairs), len(flats), counts, len(library["slots"]), len(places), kinds, len(club["seats"]), OUT))
    print("not walkable on the walk grid:", bad or "none")
    print("indoor misses:", misses or "none")
    if a.plot:
        plots(stairs, flats, plot_rows, library, club, places, grid, data)
        print("plots ->", PLOTS)


if __name__ == "__main__":
    main()
