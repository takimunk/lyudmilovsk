"""The game's whole outdoor map, at real-world scale: one place for the shape of the land and where
things stand, read by the Unreal build (unreal/scripts/import_world.py) and the map render
(tools/render_world_map.py). Pure Python, no bpy or unreal.

Frame: metres, origin on the club's portal face at floor level (the spur), X to the left of
someone walking out of the club (west, the sea), Y straight out of the door (north), Z up.
Unreal: (X * 100, portal_face_y + Y * 100, Z * 100) cm; Blender: (X, face_y - Y, Z).

The place is the Sambian coast of the Baltic: a high upland under an old pine and spruce forest
ends at the sea in a steep clay and sand cliff over a narrow beach. The club is dug into the
upland's north slope; its door opens on a terrace at the club's floor level (the spur), where a
short path with an alley leads to the upper cable-car station on the spur's brow. Below lies the
valley with the town, 37 m lower and open to the sea; wooded hills close it in the north and inland.

The macro shape lives here (height, keep_flat); tools/terrain.py (the terrain thread) adds the
natural detail on top of it.
"""
import functools
import math
import random

SEA_Z = -43.0                 # sea level below the club floor (Unreal -4300 cm)
BEACH_W = 25.0                # sand from the water line to the foot of the cliff
DOOR_X = -1.93                # the blast door's centre (layout exit_door.center)

# Map extents (the sky dome is 2800 m around the portal).
X_MIN, X_MAX = -1300.0, 1300.0
Y_MIN, Y_MAX = -900.0, 2200.0

# Water line X along the coast, (Y, X) control points: the shore runs north past the club and bends
# out to sea in the north, so the walk out of the door sees the coast run out to a cape ahead on the
# left; the beach right below the club shows from the brow, a few steps left of the path.
SHORE = [(-1400, 460), (-900, 330), (-400, 190), (0, 135), (400, 140), (1000, 175), (1600, 260),
         (2200, 480), (2800, 780)]


def _catmull(pts, y):
    for i in range(len(pts) - 1):
        if pts[i][0] <= y <= pts[i + 1][0] or i == len(pts) - 2:
            break
    p0 = pts[max(i - 1, 0)][1]
    p1, p2 = pts[i][1], pts[i + 1][1]
    p3 = pts[min(i + 2, len(pts) - 1)][1]
    t = (y - pts[i][0]) / (pts[i + 1][0] - pts[i][0])
    return 0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3)


def shore_x(y):
    """Water line X at Y."""
    return _catmull(SHORE, y)


# The coast is a Sambian one: a forested upland ~60 m over the sea ends at the sea in a steep clay
# and sand cliff over a narrow beach. The club is dug into the upland's north slope; its door opens
# on a terrace (the spur) at the club's floor level, from which the cable car goes down into the
# valley with the town. Hills close the valley in the north. The club's corridor plays no part in
# any of it: it runs under the upland, which is high over it everywhere.

TOP_Z = 18.0                      # the upland, over the club's floor (61 m over the sea)
LOW_Z = SEA_Z + 6.0               # the valley floor with the town
NORTH_Z = 12.0                    # the hills beyond the town
CORRIDOR_END_Y = -470.0           # the club's corridor runs this far back under the upland (repeated chunks)
SPUR_C = (-25.0, 75.0)            # the terrace in front of the door: centre, half widths east and north
SPUR_RX, SPUR_RY = 75.0, 105.0
SPUR_FALL = 45.0                  # metres its east and north sides take down to the valley


def _smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def escarpment_y(x):
    """Y of the middle of the upland's north slope: just behind the door, swinging north inland in
    broad lobes."""
    lobes = 35.0 * math.sin((x + 40.0) / 170.0) * min(1.0, max(-x - 80.0, 0.0) / 200.0)
    return -30.0 + 0.00045 * max(-x - 40.0, 0.0) ** 2 + lobes


def valley(x, y):
    """The valley floor: a few metres over the sea, rising a little inland and into a ridge on the map's
    inland edge."""
    h = LOW_Z + 0.006 * max(-x - 100.0, 0.0)
    if x < -800.0:
        h += ((-x - 800.0) / 500.0) ** 2 * 30.0
    return h


def spur(x, y):
    """0..1: the terrace in front of the door (open to the sea in the west; its east and north
    sides fall to the valley)."""
    dx = max(SPUR_C[0] - x, 0.0) / SPUR_RX           # only the east side is bounded
    dy = (y - SPUR_C[1]) / SPUR_RY if y > SPUR_C[1] else 0.0
    d = math.hypot(dx, dy)
    return 1.0 - _smooth((d - 1.0) * SPUR_RY / SPUR_FALL)


def north_hills(x, y):
    """0..1: the hills beyond the town, rising from Y ~1450."""
    return _smooth((y - 1450.0) / 260.0)


def upland(x, y):
    """0..1: on the upland south of its north slope (80 m wide, steepest in the middle)."""
    return 1.0 - _smooth((y - escarpment_y(x) + 40.0) / 80.0)


# The entrance: a forecourt cut into the hillside in front of the portal, held by two concrete wing
# walls that flare out from the portal's ends (world_mesh.portal draws them).
FORECOURT_LEN = 6.5               # metres out from the portal face
WING_SPLAY = math.tan(math.radians(29.0))


def forecourt_half(d):
    """Half width of the forecourt d metres out from the portal (inside of the wing walls); 0 past it."""
    if d < 0.0 or d > FORECOURT_LEN:
        return 0.0
    return 6.2 + d * WING_SPLAY


def raised(x, y):
    """Metres the land stands over the valley floor here (0 in the valley)."""
    return max(0.0, plateau(x, y) - valley(x, y))


def mound(x, d):
    """The hillside right round the portal (ported from the scene build): 6 m of earth over the door,
    flush with the portal top, falling away on both sides, cut by the forecourt."""
    if 0.0 < d <= FORECOURT_LEN and abs(x) <= forecourt_half(d) + 0.5:
        return 0.0                                   # the drop hides behind the wall (0.45 m thick)
    if d < -60.0 or abs(x) > 37.0:
        return 0.0
    m = 6.0 + min(max(-d, 0.0), 30.0) * 0.08
    if abs(x) <= 6.2:
        return m if d <= 0.0 else 0.0
    front = min((abs(x) - 6.2) * 1.1, 4.0)
    k = min(1.0, max(0.0, (abs(x) - 12.0) / 25.0))
    m *= 1 - k * k * (3 - 2 * k)
    t = min(1.0, max(0.0, (d - front) / 7.0))
    return m * (1 - t * t * (3 - 2 * t))


def plateau(x, y):
    """The land before the sea cuts it: the valley, raised to the spur, the north hills and the upland."""
    low = valley(x, y)
    z = max(low, low + (0.0 - low) * spur(x, y), low + (NORTH_Z - low) * north_hills(x, y))
    z = z + (TOP_Z - z) * upland(x, y) if TOP_Z > z else z
    if abs(x) <= 37.0 and -60.0 <= y <= 12.0:        # the hillside right round the portal
        z = max(z, mound(x, y))
    return z


def inland(x, y):
    """Metres inland from the water line (negative = under the sea)."""
    return shore_x(y) - x


BEACH_TOP = SEA_Z + 1.5


def cliff_width(top):
    """Horizontal width of the fall from the land's edge to the beach: a steep face (~53 degrees on
    average, under 60 at its steepest) where the land is high, a low dune bank where it is not."""
    return max(25.0, (top - BEACH_TOP) * 0.75)


def _edge_top(y):
    """The land's height just inside the coast at Y (sets how high and wide the cliff is)."""
    x = shore_x(y) - BEACH_W - 30.0
    return plateau(x, y)


def height(x, y):
    """Ground height at (X, Y): smooth (the natural detail is tools/terrain.py's); level under the
    Academgorodok."""
    z = _height(x, y)
    k = academ_flat(x, y)
    return z + (academ_z() - z) * k if k > 0.0 else z


def _height(x, y):
    s = inland(x, y)
    if s <= 0.0:                                     # sea bed: shallow, like the Baltic
        return SEA_Z - 0.3 - min(-s * 0.06, 9.0)
    if s <= BEACH_W:
        return SEA_Z - 0.3 + (s / BEACH_W) * 1.8
    top = plateau(x, y)
    w = cliff_width(_edge_top(y))
    if s >= BEACH_W + w:
        return top
    t = (s - BEACH_W) / w
    f = 0.5 * t + 0.5 * t * t * (3 - 2 * t)          # talus at the foot, a rounded brow
    return BEACH_TOP + (top - BEACH_TOP) * f


def cliff(x, y):
    """0..1: 1 on the sea cliff's face, from the top edge to the foot, where the land is high."""
    s = inland(x, y)
    top = _edge_top(y)
    w = cliff_width(top)
    on = 1.0 - _smooth(abs(s - BEACH_W - w / 2) - w / 2 + 2.0) if BEACH_W - 2 < s < BEACH_W + w + 2 else 0.0
    return on * _smooth((top - BEACH_TOP - 12.0) / 15.0)


def cliff_top(y):
    """X of the sea cliff's top edge at Y."""
    return shore_x(y) - BEACH_W - cliff_width(_edge_top(y))


def is_beach(x, y):
    s = inland(x, y)
    return s <= BEACH_W + 2.0


def normal(x, y, e=0.5):
    hx = (height(x + e, y) - height(x - e, y)) / (2 * e)
    hy = (height(x, y + e) - height(x, y - e)) / (2 * e)
    n = math.sqrt(hx * hx + hy * hy + 1)
    return (-hx / n, -hy / n, 1 / n)


def edge_x(y):
    """X where the land ends and starts falling to the beach (the cliff top)."""
    return cliff_top(y)


# --- the short path out of the club, lined with poplars and maples ---------------------------

PATH_LEN = 120.0


def path_points(step=3.0):
    """Centre line from the apron in front of the door: straight for 20 m, then bending gently
    right, to the upper cable-car station on the brow."""
    pts = []
    y = 3.0
    while y <= PATH_LEN + 0.01:
        b = max(y - 20.0, 0.0)
        pts.append((DOOR_X - 0.0012 * b * b, y))
        y += step
    return pts


def alley_trees():
    """(x, y, species, seed, age) along both sides of the path: poplars and maples taking turns,
    one pair every 13 m from 18 m out to 83 m, young (10..14 years) to keep the leaf count down."""
    out = []
    rnd = random.Random(7)
    pts = path_points(1.0)
    for i, y in enumerate(range(18, 84, 13)):
        cx, cy = min(pts, key=lambda p: abs(p[1] - y))
        for side in (-1, 1):
            sp = "PyramidalPoplar" if (i + (side > 0)) % 2 == 0 else "Maple"
            out.append((cx + side * (4.2 + rnd.uniform(-0.3, 0.3)), cy + rnd.uniform(-0.8, 0.8), sp,
                        100 + len(out), round(rnd.uniform(10.0, 14.0), 1)))
    return out


# --- the cable car from the spur down into the town ---------------------------------------------

STATION_UP = (-15.0, 133.0)       # on the brow, where the path ends
STATION_DOWN = (-255.0, 520.0)    # on the town's south edge
ROPE_Z = 6.0                      # rope height over a station floor
TOWER_H = 14.0
CLEARANCE = 6.0                   # least height of the rope over the ground between supports


@functools.lru_cache(maxsize=1)
def cable_car():
    """Stations and towers: {'upper': (x, y), 'lower': (x, y), 'towers': [(x, y), ...],
    'supports': [(x, y, rope_z), ...] from the top down}. Towers go where a straight rope would
    come closer than CLEARANCE to the ground (the spur's brow first)."""
    (x0, y0), (x1, y1) = STATION_UP, STATION_DOWN
    sup = [(0.0, height(x0, y0) + ROPE_Z), (1.0, height(x1, y1) + ROPE_Z)]
    for _ in range(6):
        worst = None
        for (ta, za), (tb, zb) in zip(sup, sup[1:]):
            for k in range(1, 40):
                t = ta + (tb - ta) * k / 40
                rz = za + (zb - za) * k / 40
                gz = height(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t)
                if rz - gz < CLEARANCE and (worst is None or rz - gz < worst[2]):
                    worst = (t, gz, rz - gz)
        if worst is None:
            break
        sup.append((worst[0], worst[1] + TOWER_H))
        sup.sort()
    pts = [(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z) for t, z in sup]
    return {"upper": STATION_UP, "lower": STATION_DOWN, "towers": [(p[0], p[1]) for p in pts[1:-1]],
            "supports": pts}


def near_cable(x, y, r):
    """Within r metres of the cable car's line."""
    (x0, y0), (x1, y1) = STATION_UP, STATION_DOWN
    dx, dy = x1 - x0, y1 - y0
    t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy)))
    return math.hypot(x - x0 - dx * t, y - y0 - dy * t) < r


# --- the Academgorodok before the town (blender/academgorodok, docs/academgorodok) ----------------
# The ensemble of Laboratory No. 2 (Moscow, 1945-48) stands where the cable car brings you down: its
# square opens towards the lower station, its two gates lead on into the town. Its own frame (the
# Blender build's): x east, y north, origin at the middle house's centre; here its "east" (the
# square) faces the station.

ACADEM_SQUARE_END = 80.0          # the ensemble's x where its square meets the street
ACADEM_GAP = 26.0                 # from the street in front of the square to the lower station
ACADEM_YAW = math.radians(-8.0)   # turn of the ensemble's x axis from map -Y (south), with the town grid
ACADEM_RECT = (-60.0, 110.0, -115.0, 115.0)     # ensemble x0, x1, y0, y1 kept free of houses and level


def academ():
    """(X, Y, yaw) of the ensemble's origin; yaw turns its x axis (east) onto the map: x -> map
    (sin yaw, -cos yaw), its y axis (north) -> (-cos yaw, -sin yaw), both in map X (west), Y (north)."""
    sx, sy = STATION_DOWN
    ex, ey = math.sin(ACADEM_YAW), -math.cos(ACADEM_YAW)
    d = ACADEM_SQUARE_END + ACADEM_GAP
    return (sx - ex * d, sy - ey * d, ACADEM_YAW)


def academ_local(x, y):
    """Map (X, Y) -> the ensemble's (x east, y north)."""
    ox, oy, yaw = academ()
    dx, dy = x - ox, y - oy
    ex, ey = math.sin(yaw), -math.cos(yaw)
    nx, ny = -math.cos(yaw), -math.sin(yaw)
    return (dx * ex + dy * ey, dx * nx + dy * ny)


def academ_to_map(lx, ly):
    ox, oy, yaw = academ()
    return (ox + lx * math.sin(yaw) - ly * math.cos(yaw), oy - lx * math.cos(yaw) - ly * math.sin(yaw))


def academ_flat(x, y):
    """1 on the ensemble's ground (its rectangle), falling to 0 over 25 m."""
    lx, ly = academ_local(x, y)
    x0, x1, y0, y1 = ACADEM_RECT
    d = max(x0 - lx, lx - x1, y0 - ly, ly - y1)
    return _ramp(d, 0.0, 25.0)


def academ_tree_free(x, y):
    """True where forest trees must not stand: the ensemble's ground (its own garden is planted by
    import_academgorodok.py) and a 45 m wide sight line from the cable car's descent over the lower
    station to the middle house's front, so the ensemble shows as you arrive."""
    if academ_flat(x, y) > 0.0:
        return True
    (ax, ay), (sx, sy) = ACADEM_VIEW_FROM, STATION_DOWN
    nx, ny = academ_to_map(7.25, 1.5)
    for (px, py), (qx, qy) in (((ax, ay), (sx, sy)), ((sx, sy), (nx, ny))):
        dx, dy = qx - px, qy - py
        t = max(0.0, min(1.0, ((x - px) * dx + (y - py) * dy) / (dx * dx + dy * dy)))
        if math.hypot(x - px - dx * t, y - py - dy * t) < ACADEM_VIEW_HALF:
            return True
    return False


ACADEM_VIEW_FROM = (-147.0, 346.0)    # half way down the rope
ACADEM_VIEW_HALF = 45.0


@functools.lru_cache(maxsize=1)
def academ_z():
    ox, oy, _ = academ()
    return round(_height(ox, oy), 2)


# --- the town in the lowland ------------------------------------------------------------------

TOWN_CENTER = (-330.0, 900.0)     # the square
TOWN_R = 560.0
TOWN_ROT = math.radians(8.0)      # street grid turned a little off north


def town():
    """Placeholder town: houses (x, y, yaw, w, d, floors, kind) on a street grid (its trees are
    the "town" forests).
    kind: 'house' (1-2 storeys, steep tiled roof), 'block' (five-storey flat-roofed flats),
    'kirche' (the old church tower on the square)."""
    rnd = random.Random(1945)
    houses = []
    cx, cy = TOWN_CENTER
    ca, sa = math.cos(TOWN_ROT), math.sin(TOWN_ROT)

    def world(u, v):
        return cx + u * ca - v * sa, cy + u * sa + v * ca

    def free(x, y, r):
        return all(math.hypot(x - h[0], y - h[1]) > r + max(h[3], h[4]) * 0.6 for h in houses)

    blocks_u = range(-6, 6)
    blocks_v = range(-5, 6)
    for bu in blocks_u:
        for bv in blocks_v:
            u0, v0 = bu * 90.0, bv * 75.0           # 90 x 75 m blocks with 12 m streets
            if bu in (-1, 0) and bv in (-1, 0):
                continue                             # the square
            for lu in range(4):
                for lv in range(2):
                    u = u0 + 12 + lu * 19.5 + rnd.uniform(-1.5, 1.5)
                    v = v0 + 12 + 8 + lv * 31.0 + rnd.uniform(-1.0, 1.0)
                    x, y = world(u, v)
                    if not (Y_MIN + 50 < y < 1820 and x < edge_x(y) - 25.0 and x > -900):
                        continue
                    if math.hypot(x - cx, y - cy) > TOWN_R + rnd.uniform(-80, 60):
                        continue
                    if raised(x, y) > 1.0 or near_cable(x, y, 14.0) or math.hypot(x - STATION_DOWN[0], y - STATION_DOWN[1]) < 25:
                        continue                     # not up the slopes, not under the rope
                    if academ_flat(x, y) > 0.0:
                        continue                     # the Academgorodok's ground
                    if rnd.random() < 0.12:
                        continue                     # an empty lot, a garden
                    central = math.hypot(x - cx, y - cy) < 260
                    if central and rnd.random() < 0.18 and lu % 2 == 0:
                        houses.append((x, y, TOWN_ROT, 36.0, 12.0, 5, "block"))
                    else:
                        houses.append((x, y, TOWN_ROT + (math.pi / 2 if rnd.random() < 0.3 else 0.0),
                                       rnd.uniform(8, 12), rnd.uniform(9, 13), rnd.choice((1, 1, 2, 2, 2)), "house"))
    houses.append((cx + 20, cy + 10, TOWN_ROT, 9.0, 9.0, 12, "kirche"))
    return houses


# Forests: AProceduralForest actors (the plants thread). Each actor bakes its own trees, so the map
# has a dozen big ones rather than many small. mix -> [(species, weight, young age, mature age)].
FOREST_MIX = {
    # an old, wild Baltic forest: tall pines and spruce over birch and rowan
    "old": [("ScotsPine", 0.45, 20, 80), ("NorwaySpruce", 0.3, 15, 70), ("SilverBirch", 0.15, 12, 45),
            ("Rowan", 0.1, 8, 25)],
    "dune": [("DunePine", 0.7, 10, 35), ("Juniper", 0.2, 10, 30), ("SeaBuckthorn", 0.1, 6, 12)],
    "wet": [("BlackAlder", 0.5, 10, 35), ("Aspen", 0.25, 10, 30), ("DownyBirch", 0.25, 10, 30)],
    "town": [("Maple", 0.25, 12, 40), ("Apple", 0.2, 10, 30), ("SilverBirch", 0.2, 10, 35),
             ("PyramidalPoplar", 0.15, 12, 35), ("Rowan", 0.2, 8, 25)],
}
FOREST_TILE = (260.0, 240.0)      # one AProceduralForest per tile (each actor bakes its own trees)
FOREST_SPACING = 5.0              # an old forest: a tree every ~5 m


def _tiles(x0, x1, y0, y1):
    nx = max(1, round((x1 - x0) / FOREST_TILE[0]))
    ny = max(1, round((y1 - y0) / FOREST_TILE[1]))
    hx, hy = (x1 - x0) / nx / 2, (y1 - y0) / ny / 2
    for i in range(nx):
        for j in range(ny):
            yield x0 + hx * (2 * i + 1), y0 + hy * (2 * j + 1), hx, hy


def forests():
    """(x, y, half_x, half_y, mix, count, spacing, min_z, max_z, seed) in metres. The trees are traced
    onto the ground, skip steep ground, anything outside min_z..max_z and actors tagged NoTree, so the
    tiles can overlap the open ground around them."""
    out = []

    def add(x, y, hx, hy, mix, z0, z1, density=1.0):
        # never in the sea or on the beach: the tile stops short of the beach on its seaward side, and
        # trees only stand above the beach's top
        land = min(shore_x(y - hy + k * hy / 4.0) for k in range(9)) - BEACH_W - 3.0
        if x + hx > land:
            x1 = land
            x0 = x - hx
            if x1 - x0 < 20.0:
                return
            x, hx = (x0 + x1) / 2, (x1 - x0) / 2
        z0 = max(z0, BEACH_TOP + 1.0)
        count = int(4 * hx * hy / FOREST_SPACING ** 2 * 0.8 * density)
        out.append((x, y, hx, hy, mix, count, FOREST_SPACING, z0, z1, 1 + len(out)))

    # the upland: thick old forest from the cliff top inland, over the club and up its north slope
    for x, y, hx, hy in _tiles(X_MIN, cliff_top(-300) - 5, Y_MIN, -40.0):
        add(x, y, hx, hy, "old", TOP_Z - 14.0, 100.0)
    for x, y, hx, hy in _tiles(X_MIN, -560.0, -40.0, 400.0):     # its north slope swings north inland
        add(x, y, hx, hy, "old", LOW_Z + 8.0, 100.0)
    # the hills beyond the town
    for x, y, hx, hy in _tiles(X_MIN, cliff_top(1800) - 5, 1480.0, Y_MAX):
        add(x, y, hx, hy, "old", LOW_Z + 6.0, 100.0)
    # the inland ridge closing the valley
    for x, y, hx, hy in _tiles(X_MIN, -880.0, 400.0, 1480.0):
        add(x, y, hx, hy, "old", LOW_Z + 4.0, 100.0)
    # the dune bank along the valley's beach
    for yc in range(400, 1401, 200):
        add(shore_x(yc) - BEACH_W - 25, yc, 25, 100, "dune", SEA_Z + 2.0, LOW_Z + 3.0, 0.5)
    # wet copses on the valley floor, and trees between the houses (the houses are NoTree)
    add(-760, 900, 110, 330, "wet", -100.0, LOW_Z + 3.0, 0.5)
    add(-120, 1380, 260, 80, "wet", -100.0, LOW_Z + 3.0, 0.5)
    add(TOWN_CENTER[0], STATION_DOWN[1] + 270, TOWN_R, 260, "town", -100.0, LOW_Z + 4.0, 0.12)
    add(TOWN_CENTER[0], STATION_DOWN[1] + 790, TOWN_R, 260, "town", -100.0, LOW_Z + 4.0, 0.12)
    return out


# --- Baltic plants and grass near the club (the plants thread's AProceduralPlant / AProceduralGrass) ---

def coast_plants():
    """(x, y, species, seed, age) of the real trees and shrubs around the club: Scots pines on the
    hillside over the portal, dune pines and juniper on the spur toward the cliff, sea buckthorn on
    the beach at the cliff's foot, a rowan and an aspen past the alley, before the station. Few on
    purpose: every procedural tree is up to ~1.5M triangles up close; further off the forests take over."""
    rnd = random.Random(21)
    out = []

    def add(x, y, sp, age):
        out.append((x, y, sp, 300 + len(out), round(age * rnd.uniform(0.85, 1.15), 1)))

    for x, y in ((-12, -18), (14, -26), (-16, -48), (10, -60)):
        add(x + rnd.uniform(-2, 2), y + rnd.uniform(-3, 3), "ScotsPine", 45)
    for x, y in ((34, -30), (48, 8), (62, 40), (40, 70), (70, -5), (55, 110)):
        add(x, y, "DunePine", 30)
    for x, y in ((26, 20), (30, 55), (44, -12), (52, 85)):
        add(x, y, "Juniper", 25)
    for y in (-30, 40, 110):
        add(shore_x(y) - BEACH_W + 3 + rnd.uniform(-1, 1), y + rnd.uniform(-4, 4), "SeaBuckthorn", 12)
    end = path_points()[-1]
    add(end[0] - 7, end[1] - 14, "Rowan", 18)
    add(end[0] + 7, end[1] - 20, "Aspen", 25)
    return out


def grass_patches():
    """(x, y, half_x, half_y, species, seed, density, min_z, max_z) in metres, one habitat per patch
    (the terrain's masks decide where each species actually grows): meadow on the spur and the
    hillside over the door, dune grass on the beach under the cliff, reed and sedge on the wet valley
    floor by the cable car's lower station. 60 x 60 m patches (~20 000 tufts each), drawn only
    within 90 m of the camera."""
    out = []
    seed = 500
    meadow = ("RedFescue", "MeadowGrass", "WavyHairGrass")
    for gx in range(-2, 2):                            # the spur and the hillside, -140..100 m across
        for gy in range(-1, 3):                        # -60..180 m along
            x, y = -50 + gx * 60.0, 30 + gy * 60.0
            out.append((x, y, 30.0, 30.0, meadow[(gx + gy) % 3], seed, 1.0, LOW_Z - 1.0, 50.0))
            seed += 1
    for y in range(-60, 181, 60):                      # the beach under the cliff
        x = shore_x(y) - BEACH_W * 0.5
        out.append((x, y, BEACH_W * 0.5, 30.0, "MarramGrass" if y % 120 else "SeaLymeGrass", seed, 0.8,
                    SEA_Z + 0.4, SEA_Z + 8.0))
        seed += 1
    for dx, dy in ((-60, 0), (0, 60), (60, -30)):     # the wet valley floor by the lower station
        out.append((STATION_DOWN[0] + dx, STATION_DOWN[1] + dy, 30.0, 30.0,
                    "CommonReed" if dx < 0 else "SandSedge", seed, 1.0, -100.0, LOW_Z + 2.0))
        seed += 1
    return out


def _ramp(d, r, soft):
    """1 within r, falling to 0 over soft metres."""
    return min(1.0, max(0.0, (r + soft - d) / soft))


PROMENADE_Y = (450.0, 1250.0)


def keep_flat(x, y):
    """0..1: how much of the exact macro height must stay (1 = no natural detail on top), for the
    things built on the ground: the apron and the slab path, the cable-car stations and towers, the
    promenade, the town's lots and streets."""
    k = 0.0
    if -2.0 < y < 6.0:
        k = max(k, _ramp(abs(x - DOOR_X), 5.0, 3.0))
    if 0.0 < y < PATH_LEN + 6:
        px = min(path_points(1.0), key=lambda p: abs(p[1] - y))[0]
        k = max(k, _ramp(abs(x - px), 1.8, 2.5))
    for sx, sy in (STATION_UP, STATION_DOWN):
        k = max(k, _ramp(math.hypot(x - sx, y - sy), 12.0, 6.0))
    for tx, ty in cable_car()["towers"] if abs(x) < 400 and -50 < y < 600 else ():
        k = max(k, _ramp(math.hypot(x - tx, y - ty), 3.0, 3.0))
    if PROMENADE_Y[0] - 10 < y < PROMENADE_Y[1] + 10:
        k = max(k, _ramp(abs(inland(x, y) - (BEACH_W - 5.0)), 5.0, 3.0))
    k = max(k, _ramp(math.hypot(x - TOWN_CENTER[0], y - TOWN_CENTER[1]), TOWN_R, 60.0) * (1 - _smooth(raised(x, y) / 3.0)))
    k = max(k, academ_flat(x, y))
    return k


def starlings():
    """Where the murmuration roams: over the sea off the club, to the left of the door."""
    y = 250.0
    return (shore_x(y) + 300.0, y)


def to_unreal(x, y, z, portal_face_y):
    """Map metres -> Unreal centimetres."""
    return (x * 100.0, portal_face_y + y * 100.0, z * 100.0)
