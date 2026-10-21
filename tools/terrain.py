"""The ground of the outdoor map: the macro shape from tools/world_map.py (the club's hill, the town's
lowland, the shore, the beach) made natural. Pure Python, no bpy, unreal or numpy.

The heavy part (noise, hydraulic erosion, hummocks, ruts, surface layers, the grass mask) runs in
C++ (ULyudTerrainLibrary, Source/Lyudmilovsk/Private/Terrain) on the inputs this file writes:

  write_inputs(dir)  ->  terrain_in.json + terrain_in.bin   (macro height, protect, kind, tracks, forest)
  C++ BuildTerrain   ->  terrain_out.bin                    (final height, read back by ground_z)

Everyone who places things on the ground uses ground_z(x, y) (or a line trace in Unreal).

Grid: one vertex per metre in map metres, from (GRID_X0, GRID_Y0), GRID_NX x GRID_NY vertices: that
is the Unreal Landscape's grid (126-quad components, 21 x 25 of them).
"""
import heapq
import json
import math
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402

COMPONENT_QUADS = 126                 # 2 x 2 sections of 63 quads
COMPONENTS = (21, 25)
GRID_X0, GRID_Y0 = int(W.X_MIN), int(W.Y_MIN)
GRID_NX = COMPONENTS[0] * COMPONENT_QUADS + 1
GRID_NY = COMPONENTS[1] * COMPONENT_QUADS + 1
SEED = 1987

# kind of ground (macro), per vertex
LAND, BEACH, SEABED = 0, 1, 2

# The club's portal: the mound stands 6 m over the door and its face is a vertical drop at Y 0 that
# the concrete portal hides; in front of it a forecourt is cut into the mound between two wing walls
# (W.forecourt_half), with near-vertical steps behind them. A heightfield cannot do vertical, so the
# landscape has a hole here (quads whose min corner is in HOLE_X x HOLE_Y, metres) and a fine mesh
# patch (portal_patch) fills it, sharing the hole's border edges so there is no seam.
HOLE_X = (-12, 12)
HOLE_Y = (-2, 7)


def _club_footprint(x, y):
    """1 over the club's hall, corridor and the mound/rampart above them: the ground there must stay
    exactly the macro height (any dip would open the ceiling)."""
    if y > 8.0 or y < W.CORRIDOR_END_Y - 60.0:
        return 0.0
    return 1.0 - min(1.0, max(0.0, (abs(x) - 24.0) / 16.0))


def _path_dist(x, y, pts):
    best = 1e9
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        dx, dy = x1 - x0, y1 - y0
        l2 = dx * dx + dy * dy or 1.0
        t = min(1.0, max(0.0, ((x - x0) * dx + (y - y0) * dy) / l2))
        best = min(best, math.hypot(x - x0 - dx * t, y - y0 - dy * t))
    return best


def _fallback_keep_flat(x, y, path):
    """Until world_map has keep_flat: the slab path and the apron in front of the door."""
    if y < -20.0 or y > W.PATH_LEN + 30.0 or abs(x) > 60.0:
        return 0.0
    d = min(_path_dist(x, y, path), math.hypot(x - W.DOOR_X, y - 2.0) - 4.0)
    return 1.0 - min(1.0, max(0.0, (d - 2.5) / 4.0))


def protect(x, y, path=None):
    """0..1: how much of the natural detail is held back here (1 = the macro height exactly)."""
    kf = W.keep_flat(x, y) if hasattr(W, "keep_flat") else _fallback_keep_flat(x, y, path or W.path_points())
    return max(kf, _club_footprint(x, y))


# --- off-road tracks: dirt roads worn by cars and feet, following the land ----------------------

def _anchors():
    """Places the dirt tracks join: the end of the alley, the town, the cable car's top, the woods."""
    pts = []
    end = W.path_points()[-1]
    pts.append(("alley_end", end))
    town = getattr(W, "TOWN_CENTER", None)
    if town:
        pts.append(("town", town))
    if hasattr(W, "cable_car"):
        cc = W.cable_car()
        if "upper" in cc:
            pts.append(("cable_top", cc["upper"]))
    pts += [("woods_inland", (-1000.0, 150.0)), ("woods_south", (-350.0, -700.0)),
            ("woods_north", (-500.0, 1950.0)), ("club_back", (-40.0, -120.0))]
    return dict(pts)


TRACKS = [("alley_end", "town"), ("alley_end", "woods_inland"), ("club_back", "woods_south"),
          ("town", "woods_north"), ("alley_end", "cable_top")]


def _route(a, b, step=8.0):
    """Cheapest walk from a to b on an 8 m grid, steep ground costing much more (tracks wind round
    hills like real ones do); then smoothed."""
    nx = int((W.X_MAX - W.X_MIN) / step) + 1
    ny = int((W.Y_MAX - W.Y_MIN) / step) + 1

    def cell(p):
        return (int(round((p[0] - W.X_MIN) / step)), int(round((p[1] - W.Y_MIN) / step)))

    hcache = {}

    def h(c):
        if c not in hcache:
            x, y = W.X_MIN + c[0] * step, W.Y_MIN + c[1] * step
            hcache[c] = W.height(x, y) if W.inland(x, y) > W.BEACH_W + 4 else None
        return hcache[c]

    start, goal = cell(a), cell(b)
    dist = {start: 0.0}
    prev = {}
    pq = [(0.0, start)]
    rnd = random.Random(hash((start, goal)) & 0xffff)
    jitter = {}
    while pq:
        d, c = heapq.heappop(pq)
        if c == goal:
            break
        if d > dist.get(c, 1e18):
            continue
        hc = h(c)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if not dx and not dy:
                    continue
                n = (c[0] + dx, c[1] + dy)
                if not (0 <= n[0] < nx and 0 <= n[1] < ny):
                    continue
                hn = h(n)
                if hn is None or hc is None:
                    continue
                run = step * math.hypot(dx, dy)
                g = abs(hn - hc) / run
                if n not in jitter:
                    jitter[n] = rnd.uniform(0.0, 0.35)
                x, y = W.X_MIN + n[0] * step, W.Y_MIN + n[1] * step
                avoid = 6.0 * _club_footprint(x, y)
                cost = run * (1.0 + 40.0 * g * g + jitter[n] + avoid)
                nd = d + cost
                if nd < dist.get(n, 1e18):
                    dist[n] = nd
                    prev[n] = c
                    heapq.heappush(pq, (nd, n))
    if goal not in prev and goal != start:
        return []
    path = [goal]
    while path[-1] != start:
        path.append(prev[path[-1]])
    pts = [(W.X_MIN + c[0] * step, W.Y_MIN + c[1] * step) for c in reversed(path)]
    for _ in range(4):                                # Chaikin: soft bends
        out = [pts[0]]
        for p, q in zip(pts, pts[1:]):
            out.append((0.75 * p[0] + 0.25 * q[0], 0.75 * p[1] + 0.25 * q[1]))
            out.append((0.25 * p[0] + 0.75 * q[0], 0.25 * p[1] + 0.75 * q[1]))
        out.append(pts[-1])
        pts = out
    # meander: push the line sideways by a slow wave, zero at both ends
    total = sum(math.hypot(q[0] - p[0], q[1] - p[1]) for p, q in zip(pts, pts[1:])) or 1.0
    ph = rnd.uniform(0, 6.28)
    out, s = [], 0.0
    for k, p in enumerate(pts):
        if k:
            s += math.hypot(p[0] - pts[k - 1][0], p[1] - pts[k - 1][1])
        q = pts[min(k + 1, len(pts) - 1)]
        r = pts[max(k - 1, 0)]
        tx, ty = q[0] - r[0], q[1] - r[1]
        tl = math.hypot(tx, ty) or 1.0
        env = math.sin(math.pi * s / total)
        off = env * (5.0 * math.sin(s / 23.0 + ph) + 2.5 * math.sin(s / 9.0 + 2 * ph))
        out.append((p[0] - ty / tl * off, p[1] + tx / tl * off))
    return out


def tracks():
    """Dirt tracks as polylines of (x, y) map metres."""
    an = _anchors()
    out = []
    for a, b in TRACKS:
        if a in an and b in an:
            r = _route(an[a], an[b])
            if r:
                out.append(r)
    return out


# --- inputs for the C++ build ----------------------------------------------------------------

def _woods():
    """The pine forests' rectangles (W.forests(): x, y, half x, half y, ...), grown by a soft edge."""
    if not hasattr(W, "forests"):
        return [(-1050.0, 600.0, 250.0, 900.0), (-450.0, 2000.0, 600.0, 200.0), (-450.0, -720.0, 600.0, 160.0)]
    return [(f[0], f[1], f[2], f[3]) for f in W.forests() if f[4] == "pine"]


EDGE = 30.0                           # metres over which the forest floor gives way to the meadow


def _forest(x, y, woods):
    """0..1 forest floor (needles, cones, moss) under the pine woods, with a wavy edge."""
    best = 0.0
    for cx, cy, hx, hy in woods:
        dx, dy = abs(x - cx) - hx, abs(y - cy) - hy
        if dx > EDGE or dy > EDGE:
            continue
        wave = 14.0 * math.sin(x * 0.021 + y * 0.017) + 9.0 * math.sin(x * 0.067 - y * 0.051) + 4.0 * math.sin(x * 0.19 + y * 0.13)
        d = max(dx, dy) + wave
        best = max(best, min(1.0, max(0.0, (EDGE * 0.5 - d) / EDGE)))
    return best


CLUB_ZONE = (-70.0, 70.0, W.CORRIDOR_END_Y - 80.0, 40.0)   # x0, x1, y0, y1: macro sampled every metre here


def _cached(fn):
    """fn memoized (the map's path and cable car are rebuilt on every keep_flat call otherwise)."""
    import functools
    return functools.lru_cache(maxsize=None)(fn)


def write_inputs(out_dir, log=print):
    """Samples the macro shape on the landscape grid and writes terrain_in.json/.bin for the C++
    build. About a minute of pure Python."""
    saved = {n: getattr(W, n) for n in ("path_points", "cable_car") if hasattr(W, n)}
    for n, fn in saved.items():
        setattr(W, n, _cached(fn))
    try:
        return _write_inputs(out_dir, log)
    finally:
        for n, fn in saved.items():
            setattr(W, n, fn)


def _write_inputs(out_dir, log):
    os.makedirs(out_dir, exist_ok=True)
    path = W.path_points()
    tr = tracks()
    log("terrain: %d tracks, %.1f km" % (len(tr), sum(math.hypot(q[0] - p[0], q[1] - p[1])
                                                      for t in tr for p, q in zip(t, t[1:])) / 1000.0))
    height = bytearray()
    prot = bytearray()
    kind = bytearray()
    forest = bytearray()
    hpack = struct.Struct("<f")
    woods = _woods()
    # protect() on a 4 m lattice first: exact per metre only in cells where it changes
    C = 4
    cnx, cny = (GRID_NX - 1) // C + 2, (GRID_NY - 1) // C + 2
    coarse = [[protect(GRID_X0 + ci * C, GRID_Y0 + cj * C, path) for ci in range(cnx)] for cj in range(cny)]
    # the macro shape is smooth away from the club: every 2 m, bilinear in between
    M = 2
    mnx, mny = (GRID_NX - 1) // M + 2, (GRID_NY - 1) // M + 2
    macro = [[W.height(GRID_X0 + mi * M, GRID_Y0 + mj * M) for mi in range(mnx)] for mj in range(mny)]
    zx0, zx1, zy0, zy1 = CLUB_ZONE
    for j in range(GRID_NY):
        y = GRID_Y0 + j
        sx = W.shore_x(y)
        row_woods = [w for w in woods if abs(y - w[1]) < w[3] + EDGE + 12]
        near_woods = bool(row_woods)
        for i in range(GRID_NX):
            x = GRID_X0 + i
            if zx0 <= x <= zx1 and zy0 <= y <= zy1:
                z = W.height(x, y)
            else:
                mi, mj = i // M, j // M
                fu, fv = (i - mi * M) / M, (j - mj * M) / M
                r0, r1 = macro[mj], macro[mj + 1]
                z = (r0[mi] * (1 - fu) + r0[mi + 1] * fu) * (1 - fv) + (r1[mi] * (1 - fu) + r1[mi + 1] * fu) * fv
            height += hpack.pack(z)
            s = sx - x
            kind.append(SEABED if s <= 0 else BEACH if s <= W.BEACH_W + 2.0 else LAND)
            ci, cj = i // C, j // C
            c = (coarse[cj][ci], coarse[cj][ci + 1], coarse[cj + 1][ci], coarse[cj + 1][ci + 1])
            if max(c) <= 0.0:
                prot.append(0)
            elif min(c) >= 1.0:
                prot.append(255)
            else:
                prot.append(int(round(255 * protect(x, y, path))))
            forest.append(int(255 * _forest(x, y, row_woods)) if near_woods else 0)
    hole = bytearray(GRID_NX * GRID_NY)
    for j in range(HOLE_Y[0], HOLE_Y[1]):
        for i in range(HOLE_X[0], HOLE_X[1]):
            hole[(j - GRID_Y0) * GRID_NX + (i - GRID_X0)] = 255
    with open(os.path.join(out_dir, "terrain_in.bin"), "wb") as f:
        # header: magic, nx, ny, x0, y0, seed, sea_z, track count; each track: point count, x y pairs
        f.write(struct.pack("<4siiiiIfi", b"LYTR", GRID_NX, GRID_NY, GRID_X0, GRID_Y0, SEED, W.SEA_Z, len(tr)))
        for t in tr:
            f.write(struct.pack("<i", len(t)))
            f.write(struct.pack("<%df" % (2 * len(t)), *[c for p in t for c in p]))
        for blob in (height, prot, kind, forest, hole):
            f.write(blob)
    meta = {"nx": GRID_NX, "ny": GRID_NY, "x0": GRID_X0, "y0": GRID_Y0, "cell": 1.0, "seed": SEED,
            "sea_z": W.SEA_Z, "beach_w": W.BEACH_W, "tracks": [[list(p) for p in t] for t in tr],
            "components": list(COMPONENTS), "component_quads": COMPONENT_QUADS,
            "layout": ["height:f32", "protect:u8", "kind:u8", "forest:u8", "hole:u8"]}
    with open(os.path.join(out_dir, "terrain_in.json"), "w") as f:
        json.dump(meta, f)
    return meta


# --- the final ground, read back -------------------------------------------------------------

_OUT = None


def load(out_dir):
    """Reads terrain_out.bin (final heights, float32, same grid) written by the C++ build."""
    global _OUT, _PATCH
    import array
    a = array.array("f")
    with open(os.path.join(out_dir, "terrain_out.bin"), "rb") as f:
        a.frombytes(f.read(GRID_NX * GRID_NY * 4))
    _OUT = a
    _PATCH = None                                     # its border follows the heights just read
    return a


def default_dir():
    """Saved/World of the Unreal project in this checkout."""
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.normpath(os.path.join(here, "..", "unreal", "Lyudmilovsk", "Saved", "World"))


def _h(i, j):
    i = min(max(i, 0), GRID_NX - 1)
    j = min(max(j, 0), GRID_NY - 1)
    return _OUT[j * GRID_NX + i]


def ground_z(x, y):
    """Ground height at map (X, Y), metres: the built landscape's surface, exactly (same two
    triangles per quad as Unreal: split from (0,0) to (1,1)). Falls back to the macro shape when the
    terrain has not been built in this checkout."""
    if _OUT is None:
        try:
            load(default_dir())
        except OSError:
            return W.height(x, y)
    if HOLE_X[0] <= x < HOLE_X[1] and HOLE_Y[0] <= y < HOLE_Y[1]:
        return _patch_z(x, y)
    u, v = x - GRID_X0, y - GRID_Y0
    i, j = int(math.floor(u)), int(math.floor(v))
    fu, fv = u - i, v - j
    h00, h11 = _h(i, j), _h(i + 1, j + 1)
    if fu >= fv:
        h10 = _h(i + 1, j)
        return h00 + (h10 - h00) * fu + (h11 - h10) * fv
    h01 = _h(i, j + 1)
    return h00 + (h11 - h01) * fu + (h01 - h00) * fv


_PATCH = None


def _patch_z(x, y):
    """Height of the portal patch's own triangles at (x, y)."""
    global _PATCH
    if _PATCH is None:
        _PATCH = portal_patch()
    verts, tris = _PATCH
    best = None
    for a, b, c in tris:
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = verts[a], verts[b], verts[c]
        if min(ay, by, cy) - 1e-6 > y or max(ay, by, cy) + 1e-6 < y or min(ax, bx, cx) - 1e-6 > x or max(ax, bx, cx) + 1e-6 < x:
            continue
        d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-12:
            continue
        u = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / d
        v = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / d
        if u >= -1e-6 and v >= -1e-6 and u + v <= 1 + 1e-6:
            z = u * az + v * bz + (1 - u - v) * cz
            best = z if best is None else max(best, z)      # on a vertical step: its top
    return W.height(x, y) if best is None else best


LANDSCAPE_Z_STEP = 0.5 / 128.0       # metres per landscape height unit (Z scale 50 cm, LyudTerrain.cpp)


def _landscape_h(i, j):
    """The landscape's own height at grid vertex (i, j) map metres, quantized as Unreal stores it."""
    if _OUT is None:
        try:
            load(default_dir())
        except OSError:
            return round(W.height(i, j) / LANDSCAPE_Z_STEP) * LANDSCAPE_Z_STEP
    return round(_h(i - GRID_X0, j - GRID_Y0) / LANDSCAPE_Z_STEP) * LANDSCAPE_Z_STEP


def _strip(a, b, n):
    return [a + (b - a) * k / n for k in range(n)]


def portal_patch():
    """The ground in the landscape's hole at the portal: (vertices, triangles) in map metres. Rows run
    across the forecourt; every row has the same columns, two of them on either side of the cut's
    step (W.forecourt_half + 0.5), so the steps stay sharp. Border vertices sit exactly on the
    landscape's border edges, so the two surfaces meet without a gap."""
    x0, x1 = HOLE_X
    y0, y1 = HOLE_Y
    flen = getattr(W, "FORECOURT_LEN", 0.0)

    def step(y):
        half = W.forecourt_half(min(max(y, 1e-4), flen)) if flen else 6.2
        return half + 0.5

    def columns(y):
        e = step(y)
        right = _strip(6.2, e, 6) + [e, e + 0.02] + _strip(e + 0.02, float(x1), 8)[1:] + [float(x1)]
        mid = _strip(-6.2, 6.2, 50)
        return [-x for x in reversed(right)] + mid + right

    ys = _strip(float(y0), 0.0, 4) + [0.0, 0.01]
    ys += [y for y in _strip(0.25, flen, int(round((flen - 0.25) / 0.25))) if y > 0.01] + [flen, flen + 0.02] if flen else []
    y = flen + 0.25
    while y < y1 - 0.1:
        ys.append(y)
        y += 0.25
    ys.append(float(y1))
    ys = sorted(set(round(y, 4) for y in ys))

    def border(x, y):
        """On the hole's edge: linear between the two landscape vertices of that edge."""
        if y in (y0, y1):
            a = math.floor(x)
            t = x - a
            return _landscape_h(a, int(y)) * (1 - t) + _landscape_h(min(a + 1, x1), int(y)) * t
        b = math.floor(y)
        t = y - b
        return _landscape_h(int(x), b) * (1 - t) + _landscape_h(int(x), min(b + 1, y1)) * t

    verts, rows = [], []
    for y in ys:
        row = []
        for x in columns(y):
            if x in (x0, x1) or y in (y0, y1):
                z = border(x, y)
            elif y == 0.0:
                z = W.height(x, -0.001)              # top of the drop behind the portal
            elif y == 0.01:
                z = W.height(x, 0.02)                # its foot
            else:
                z = W.height(x, y)
            row.append(len(verts))
            verts.append((x, y, z))
        rows.append(row)
    tris = []
    for j in range(len(ys) - 1):
        r0, r1 = rows[j], rows[j + 1]
        for i in range(len(r0) - 1):
            if ys[j] == 0.0 and abs(verts[r0[i]][0]) <= 6.2 + 1e-6 and abs(verts[r0[i + 1]][0]) <= 6.2 + 1e-6:
                continue                              # the portal stands in the drop
            a, b, c, d = r0[i], r0[i + 1], r1[i + 1], r1[i]
            tris += [(a, b, c), (a, c, d)]
    return verts, tris


def to_unreal(x, y, z, portal_face_y):
    return W.to_unreal(x, y, z, portal_face_y)


if __name__ == "__main__":
    import time
    t = time.time()
    m = write_inputs(sys.argv[1] if len(sys.argv) > 1 else default_dir())
    print("wrote %d x %d in %.1f s" % (m["nx"], m["ny"], time.time() - t))
