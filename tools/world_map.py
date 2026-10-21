"""The game's whole outdoor map, at real-world scale: one place for the shape of the land and where
things stand, read by the Unreal build (unreal/scripts/import_world.py) and the map render
(tools/render_world_map.py). Pure Python, no bpy or unreal.

Frame: metres, origin on the club's portal face at floor level (the hilltop), X to the left of
someone walking out of the club (west, the sea), Y straight out of the door (north), Z up.
Unreal: (X * 100, portal_face_y + Y * 100, Z * 100) cm; Blender: (X, face_y - Y, Z).

The place is a Baltic coast town on a high shore (think Svetlogorsk): the club is dug into a
hill whose west flank runs down to a sand beach; north along the shore the hill becomes a
steep wooded bluff with a small town on top and a cable car down to the beach promenade.
"""
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


def mound(x, d):
    """The club's hilltop near the portal (ported from the scene build): 6 m of earth over the
    club, flush with the portal top, and a grassy crown in front of it."""
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
    if y >= 0.0 or y < CORRIDOR_END_Y - 40.0:
        return 0.0
    k = min(1.0, max(0.0, (abs(x) - 8.0) / 12.0))
    e = min(1.0, max(0.0, (y - CORRIDOR_END_Y + 40.0) / 40.0))
    return 7.5 * (1 - k * k * (3 - 2 * k)) * e * e * (3 - 2 * e)


def plateau(x, y):
    """The land on top, before the shore cuts it: nearly flat around the club and the town, rising
    into wooded ridges at the map's inland and far ends."""
    h = -0.004 * max(y - 30.0, 0.0) * (1 - min(max((y - 1000.0) / 600.0, 0.0), 1.0) * 0.5)
    if x < -800.0:
        h += ((-x - 800.0) / 500.0) ** 2 * 30.0
    if y > 1850.0:
        h += ((y - 1850.0) / 350.0) ** 2 * 25.0
    if y < -500.0:
        h += ((-y - 500.0) / 400.0) ** 2 * 20.0
    n = math.sin(x * 0.011 + y * 0.007) * 1.2 + math.sin(x * 0.023 - y * 0.017) * 0.6
    fade = min(1.0, max(0.0, (math.hypot(x - DOOR_X, y) - 15.0) / 60.0))
    near = math.sin(x * 0.21 + y * 0.13) * 0.25 + math.sin(x * 0.07 - y * 0.05) * 0.6
    return h + n * fade + near * min(1.0, max(0.0, (y - 3.0) / 10.0)) * 0.5


def inland(x, y):
    """Metres inland from the water line (negative = under the sea)."""
    return shore_x(y) - x


def height(x, y):
    """Ground height at (X, Y)."""
    top = plateau(x, y) + max(mound(x, y), berm(x, y))
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
    right (inland, toward the town)."""
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


# --- the cable car from the town down to the beach --------------------------------------------

CABLE_Y = 860.0


def cable_car():
    """Upper station on the bluff top, lower station on the beach, one tower at the bluff edge."""
    y = CABLE_Y
    up = (edge_x(y) - 30.0, y)
    low = (shore_x(y) - 14.0, y)
    tower = (edge_x(y) + 6.0, y)
    return {"upper": up, "lower": low, "tower": tower}


# --- the town on the bluff --------------------------------------------------------------------

TOWN_CENTER = (-330.0, 1180.0)    # the square
TOWN_ROT = math.radians(8.0)      # street grid turned a little off north


def town():
    """Placeholder town: houses (x, y, yaw, w, d, floors, kind) on a street grid, and its trees.
    kind: 'house' (1-2 storeys, steep tiled roof), 'block' (five-storey flat-roofed flats),
    'kirche' (the old church tower on the square)."""
    rnd = random.Random(1945)
    houses, trees = [], []
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
                    if math.hypot(x - cx, y - cy) > 620 + rnd.uniform(-80, 60):
                        continue
                    if rnd.random() < 0.12:
                        continue                     # an empty lot, a garden
                    central = math.hypot(x - cx, y - cy) < 260
                    if central and rnd.random() < 0.18 and lu % 2 == 0:
                        houses.append((x, y, TOWN_ROT, 36.0, 12.0, 5, "block"))
                    else:
                        houses.append((x, y, TOWN_ROT + (math.pi / 2 if rnd.random() < 0.3 else 0.0),
                                       rnd.uniform(8, 12), rnd.uniform(9, 13), rnd.choice((1, 1, 2, 2, 2)), "house"))
                    for _ in range(rnd.choice((0, 1, 1, 2))):
                        tx, ty = world(u + rnd.uniform(-8, 8), v + rnd.choice((-1, 1)) * rnd.uniform(9, 14))
                        trees.append((tx, ty, rnd.uniform(2.5, 4.5), rnd.uniform(7, 13)))
    houses.append((cx + 20, cy + 10, TOWN_ROT, 9.0, 9.0, 12, "kirche"))
    # avenue trees along the main street to the cable car and around the square
    for i in range(-30, 31):
        u = i * 12.0
        for side in (-1, 1):
            x, y = world(u, side * 7.0 - 37.5 + 37.5)
            if x < edge_x(y) - 8 and math.hypot(x - cx, y - cy) < 640:
                trees.append((x, y, 3.0, 11.0))
    return houses, [t for t in trees if t[0] < edge_x(t[1]) - 5]


def woods():
    """Placeholder woods on the hill behind the club and the ridges: (x, y, crown radius, height)."""
    rnd = random.Random(55)
    out = []
    for _ in range(2600):
        x, y = rnd.uniform(X_MIN + 30, 300.0), rnd.uniform(Y_MIN + 30, Y_MAX - 30)
        if x > edge_x(y) - 4:
            continue
        ridge = x < -750 or y > 1800 or y < -380
        near_club = math.hypot(x - DOOR_X, y) < 140
        in_town = math.hypot(x - TOWN_CENTER[0], y - TOWN_CENTER[1]) < 700
        if near_club or in_town:
            continue
        if not ridge and rnd.random() < 0.75:
            continue                                  # meadows between the copses
        out.append((x, y, rnd.uniform(2.5, 4.0), rnd.uniform(12, 22)))
    return out


# --- Baltic plants and grass near the club (the plants thread's AProceduralPlant / AProceduralGrass) ---

def coast_plants():
    """(x, y, species, seed, age) of the real trees and shrubs around the club: Scots pines on the
    rampart behind the portal, dune pines and juniper down the west flank, sea buckthorn at the foot
    of the slope over the beach, a rowan and an aspen where the alley ends. Few on purpose: every
    procedural tree is up to ~1.5M triangles up close; the woods further off are placeholders."""
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
    add(end[0] - 6, end[1] + 6, "Rowan", 18)
    add(end[0] + 7, end[1] + 2, "Aspen", 25)
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


def starlings():
    """Where the murmuration roams: over the sea off the club, to the left of the door."""
    y = 250.0
    return (shore_x(y) + 300.0, y)


def to_unreal(x, y, z, portal_face_y):
    """Map metres -> Unreal centimetres."""
    return (x * 100.0, portal_face_y + y * 100.0, z * 100.0)
