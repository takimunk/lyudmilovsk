"""The game's whole outdoor map, at real-world scale: one place for the shape of the land and where
things stand, read by the Unreal build (unreal/scripts/import_world.py) and the map render
(tools/render_world_map.py). Pure Python, no bpy or unreal.

Frame: metres, origin on the club's portal face at floor level (the hilltop), X to the left of
someone walking out of the club (west, the sea), Y straight out of the door (north), Z up.
Unreal: (X * 100, portal_face_y + Y * 100, Z * 100) cm; Blender: (X, face_y - Y, Z).

The place is a Baltic coast: the club is dug into the top of a hill whose west flank runs down
to a sand beach. A short path from the door crosses the hilltop to the upper cable-car station
on the brow, where the town shows below: it lies in the coastal lowland to the north, 37 m
lower, and the cable car runs down into it. Ridges close the map inland and at both ends.

The macro shape lives here (height, keep_flat); tools/terrain.py (the terrain thread) adds the
natural detail on top of it.
"""
import functools
import math
import random

SEA_Z = -43.0                 # sea level below the club floor (Unreal -4300 cm)
BEACH_W = 35.0                # sand from the water line to the foot of the slope
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


def _club(y):
    """1 at the club, fading to 0 a few hundred metres along the shore."""
    return math.exp(-(y / 220.0) ** 2)


def slope_width(y):
    """Horizontal width of the land falling from the top to the beach: a long grassy flank at the
    club, a steep bluff along the town."""
    return 55.0 + 37.0 * _club(y)


# The entrance: a forecourt cut into the mound in front of the portal, held by two concrete wing
# walls that flare out from the portal's ends (world_mesh.portal draws them).
FORECOURT_LEN = 6.5               # metres out from the portal face
WING_SPLAY = math.tan(math.radians(29.0))


def forecourt_half(d):
    """Half width of the forecourt d metres out from the portal (inside of the wing walls); 0 past it."""
    if d < 0.0 or d > FORECOURT_LEN:
        return 0.0
    return 6.2 + d * WING_SPLAY


def mound(x, d):
    """The club's hilltop near the portal (ported from the scene build): 6 m of earth over the
    club, flush with the portal top, and a grassy crown in front of it, cut by the forecourt."""
    if 0.0 < d <= FORECOURT_LEN and abs(x) <= forecourt_half(d) + 0.5:
        return 0.0                                   # the drop hides behind the wall (0.45 m thick)
    m = 6.0 + min(max(-d, 0.0), 30.0) * 0.08
    if d < -60.0:                                    # the hill over the far corridor settles into the land
        k = min(1.0, (-60.0 - d) / 60.0)
        m *= 1 - k * k * (3 - 2 * k)
    if abs(x) <= 6.2:
        return m if d <= 0.0 else 0.0
    front = min((abs(x) - 6.2) * 1.1, 4.0)
    k = min(1.0, max(0.0, (abs(x) - 12.0) / 25.0))
    m *= 1 - k * k * (3 - 2 * k)
    t = min(1.0, max(0.0, (d - front) / 7.0))
    return m * (1 - t * t * (3 - 2 * t))


CORRIDOR_END_Y = -470.0            # the club's corridor runs this far back under the hill (repeated chunks)


def berm(x, y):
    """A long grassy rampart over the buried corridor (above the land around), so the land never cuts into it."""
    if y >= -40.0 or y < CORRIDOR_END_Y - 40.0:
        return 0.0                                   # by the portal the mound covers the hall and the corridor
    k = min(1.0, max(0.0, (abs(x) - 8.0) / 12.0))
    e = min(1.0, max(0.0, (y - CORRIDOR_END_Y + 40.0) / 40.0))
    e *= min(1.0, (-40.0 - y) / 20.0)               # rises out of the mound 40..60 m behind the portal
    return 7.5 * (1 - k * k * (3 - 2 * k)) * e * e * (3 - 2 * e)


LOW_Z = SEA_Z + 6.0               # the coastal lowland with the town
HILL_C = (-20.0, -80.0)           # the club's hill: centre, half widths across and to the north / south
HILL_RX, HILL_RY_N, HILL_RY_S = 260.0, 380.0, 800.0


def hill(x, y):
    """1 on the hilltop, 0 in the lowland: flat top out to 55 % of the radii (the brow is ~130 m
    north of the door), then down to the lowland by 105 %."""
    ry = HILL_RY_N if y > HILL_C[1] else HILL_RY_S
    d = math.hypot((x - HILL_C[0]) / HILL_RX, (y - HILL_C[1]) / ry)
    t = min(1.0, max(0.0, (1.05 - d) / 0.5))
    return t * t * (3 - 2 * t)


def lowland(x, y):
    """The land below the hill: a few metres over the sea, rising gently inland and into wooded
    ridges at the map's inland and far ends."""
    h = LOW_Z + 0.006 * max(-x - 100.0, 0.0)
    if x < -800.0:
        h += ((-x - 800.0) / 500.0) ** 2 * 30.0
    if y > 1850.0:
        h += ((y - 1850.0) / 350.0) ** 2 * 25.0
    if y < -500.0:
        h += ((-y - 500.0) / 400.0) ** 2 * 20.0
    return h


def hilltop(x, y):
    """The top of the club's hill, at the club's floor level (the bumps are tools/terrain.py's)."""
    return 0.0


def plateau(x, y):
    """The land before the shore cuts it: the hill (with the mound and the rampart over the club)
    blended into the lowland."""
    low = lowland(x, y)
    top = hilltop(x, y) + max(mound(x, y), berm(x, y))
    return low + (top - low) * hill(x, y)


def inland(x, y):
    """Metres inland from the water line (negative = under the sea)."""
    return shore_x(y) - x


def height(x, y):
    """Ground height at (X, Y)."""
    top = plateau(x, y)
    s = inland(x, y)
    beach_top = SEA_Z + 1.5
    if s <= 0.0:                                     # sea bed: shallow, like the Baltic
        return SEA_Z - 0.3 - min(-s * 0.06, 9.0)
    if s <= BEACH_W:
        return SEA_Z - 0.3 + (s / BEACH_W) * 1.8
    w = slope_width(y)
    if s >= BEACH_W + w:
        return top
    t = (s - BEACH_W) / w
    g = _club(y)
    f = g * (0.6 * t + 0.4 * t * t) + (1 - g) * t * t   # a walkable flank at the club, a cliff further on
    return beach_top + (top - beach_top) * f


def is_beach(x, y):
    s = inland(x, y)
    return s <= BEACH_W + 2.0


def normal(x, y, e=0.5):
    hx = (height(x + e, y) - height(x - e, y)) / (2 * e)
    hy = (height(x, y + e) - height(x, y - e)) / (2 * e)
    n = math.sqrt(hx * hx + hy * hy + 1)
    return (-hx / n, -hy / n, 1 / n)


def edge_x(y):
    """X where the top ends and the land starts falling to the beach."""
    return shore_x(y) - BEACH_W - slope_width(y)


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


# --- the cable car from the hill down into the town ---------------------------------------------

STATION_UP = (-15.0, 133.0)       # on the brow, where the path ends
STATION_DOWN = (-255.0, 520.0)    # on the town's south edge
ROPE_Z = 6.0                      # rope height over a station floor
TOWER_H = 14.0
CLEARANCE = 6.0                   # least height of the rope over the ground between supports


@functools.lru_cache(maxsize=1)
def cable_car():
    """Stations and towers: {'upper': (x, y), 'lower': (x, y), 'towers': [(x, y), ...],
    'supports': [(x, y, rope_z), ...] from the top down}. Towers go where a straight rope would
    come closer than CLEARANCE to the ground (the hill's brow first)."""
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
                    if hill(x, y) > 0.02 or near_cable(x, y, 14.0) or math.hypot(x - STATION_DOWN[0], y - STATION_DOWN[1]) < 25:
                        continue                     # not up the hill, not under the rope
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
    "pine": [("ScotsPine", 0.6, 15, 60), ("NorwaySpruce", 0.15, 12, 50), ("SilverBirch", 0.15, 10, 35),
             ("Rowan", 0.1, 8, 20)],
    "dune": [("DunePine", 0.7, 10, 35), ("Juniper", 0.2, 10, 30), ("SeaBuckthorn", 0.1, 6, 12)],
    "wet": [("BlackAlder", 0.5, 10, 35), ("Aspen", 0.25, 10, 30), ("DownyBirch", 0.25, 10, 30)],
    "town": [("Maple", 0.25, 12, 40), ("Apple", 0.2, 10, 30), ("SilverBirch", 0.2, 10, 35),
             ("PyramidalPoplar", 0.15, 12, 35), ("Rowan", 0.2, 8, 25)],
}


def forests():
    """(x, y, half_x, half_y, mix, count, spacing, min_z, max_z, seed) in metres. The trees are traced
    onto the ground, skip steep ground, anything outside min_z..max_z and actors tagged NoTree."""
    hz = HILL_C
    return [
        # the club hill's slopes (between the lowland and the open hilltop), west, south, east
        (hz[0] + 60, hz[1] - 330, 220, 260, "pine", 2200, 7.0, LOW_Z + 1.5, -2.5, 1),
        (hz[0] - 230, hz[1] + 60, 110, 300, "pine", 1600, 7.0, LOW_Z + 1.5, -2.5, 2),
        (112, hz[1] + 330, 120, 90, "pine", 700, 7.0, LOW_Z + 1.5, -2.5, 3),   # east of the cable car
        # the dune bank north along the shore, above the beach
        (shore_x(900) - BEACH_W - 30, 900, 45, 650, "dune", 1200, 8.0, SEA_Z + 2.5, LOW_Z + 4.0, 4),
        # the inland ridge and the two ends of the map
        (-1050, 600, 220, 900, "pine", 3000, 8.0, -100.0, 100.0, 5),
        (-450, 2000, 600, 180, "pine", 2600, 8.0, -100.0, 100.0, 6),
        (-450, -720, 600, 160, "pine", 2400, 8.0, -100.0, 100.0, 7),
        # wet copses in the lowland around the town
        (-760, 700, 120, 300, "wet", 900, 7.0, -100.0, LOW_Z + 5.0, 8),
        (-330, 1560, 350, 110, "wet", 900, 7.0, -100.0, LOW_Z + 5.0, 9),
        # trees between the houses (the houses are NoTree), north of the cable car's low end
        (TOWN_CENTER[0], STATION_DOWN[1] + 270, TOWN_R, 260, "town", 1400, 14.0, -100.0, LOW_Z + 6.0, 10),
        (TOWN_CENTER[0], STATION_DOWN[1] + 790, TOWN_R, 260, "town", 1400, 14.0, -100.0, LOW_Z + 6.0, 11),
    ]


# --- Baltic plants and grass near the club (the plants thread's AProceduralPlant / AProceduralGrass) ---

def coast_plants():
    """(x, y, species, seed, age) of the real trees and shrubs around the club: Scots pines on the
    rampart behind the portal, dune pines and juniper down the west flank, sea buckthorn at the foot
    of the slope over the beach, a rowan and an aspen past the alley, before the station. Few on purpose: every
    procedural tree is up to ~1.5M triangles up close; further off the forests take over."""
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
        add(shore_x(y) - BEACH_W - 4 + rnd.uniform(-2, 2), y + rnd.uniform(-4, 4), "SeaBuckthorn", 12)
    end = path_points()[-1]
    add(end[0] - 7, end[1] - 14, "Rowan", 18)
    add(end[0] + 7, end[1] - 20, "Aspen", 25)
    return out


def grass_patches():
    """(x, y, half_x, half_y, species, seed, density, min_z, max_z) in metres: a meadow around the
    club and down its west flank, dune grasses on the beach below. 60 x 60 m patches (~20 000 tufts
    each); the tufts are drawn only within 90 m of the camera."""
    out = []
    seed = 500
    for gx in range(-2, 3):                            # the hilltop and the flank, -110..190 m across
        for gy in range(-1, 3):                        # -60..180 m along
            x, y = -50 + gx * 60.0, 30 + gy * 60.0
            if x > shore_x(y) - BEACH_W:
                continue
            on_flank = x > edge_x(y) + 10
            sp = ("WavyHairGrass" if (gx + gy) % 2 else "RedFescue") if on_flank else \
                ("MeadowGrass" if (gx + gy) % 2 else "RedFescue")
            out.append((x, y, 30.0, 30.0, sp, seed, 1.0, SEA_Z + 3.0, 50.0))
            seed += 1
    for y in range(-60, 181, 60):                      # the beach: marram and lyme grass up the dune foot
        x = shore_x(y) - BEACH_W * 0.45
        out.append((x, y, BEACH_W * 0.55, 30.0, "MarramGrass" if y % 120 else "SeaLymeGrass", seed, 0.8,
                    SEA_Z + 0.4, SEA_Z + 6.0))
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
    k = max(k, _ramp(math.hypot(x - TOWN_CENTER[0], y - TOWN_CENTER[1]), TOWN_R, 60.0) * (1 - hill(x, y)))
    return k


def starlings():
    """Where the murmuration roams: over the sea off the club, to the left of the door."""
    y = 250.0
    return (shore_x(y) + 300.0, y)


def to_unreal(x, y, z, portal_face_y):
    """Map metres -> Unreal centimetres."""
    return (x * 100.0, portal_face_y + y * 100.0, z * 100.0)
