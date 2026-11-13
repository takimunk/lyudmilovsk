"""The North (Север): volcano country beyond the north hills, a long walk from the town. Pure Python, no bpy, unreal or
numpy; tools/world_map.py hooks it in (plateau, keep_flat, forests, grass) the way it does tools/city_plan.py, and
tools/terrain.py reads its masks (snow, scoria, lava, sulfur) for TerrainGen. docs/north.md.

The land (map metres, the docs/world_map.md frame: X west toward the sea, Y north, Z over the club's floor):

  Y 2200..2550  the tree line: the old forest of the north hills thins into stone birch park woodland and shrubs;
                the ranger's hut (сторожка) stands by the track at the reserve's gate
  Y 2550..3400  the meadows (луга): a broad rolling plateau of grass 35..45 m up, ending at the sea in a lava cliff;
                the campsite with its tents in a sheltered hollow of it
  Y ~3520       the Ridge (Гряда): a low east-west rise whose north slope keeps snowfields (снежники) all summer
  the gap       the Fumarole Valley cuts the Ridge: bare coloured clay, steam vents, mud pots, a hot spring
  Y ~3840       the Cone (Сопка): a young cinder cone 60 m high with a crater breached to the west, and the lava flow
                that left it for the sea
  Y 3950..      the foot of the big volcanoes: the land climbs out of the map; the volcanoes themselves are far
                backdrop meshes (far_volcanoes)

Everything a build needs comes out of build(): Content/North/north.json (places, fumaroles, tents, the hut, signs,
snowfields, far volcanoes) and Content/Dev/teleports/north.json (the dev panel's teleport points).
"""
import functools
import json
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402  (world_map imports this module lazily)

HERE = os.path.dirname(os.path.abspath(__file__))
CONTENT = os.path.join(HERE, "..", "unreal", "Lyudmilovsk", "Content")
OUT = os.path.join(CONTENT, "North", "north.json")
OUT_TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "north.json")

Y0 = 2200.0                     # the old map's north edge: nothing south of it changes
BLEND = (2180.0, 2560.0)        # the north hills' own height gives way to the North's over this band
FOREST_END = 2330.0             # the old forest's tiles stop here; park woodland and shrubs beyond
MAX_Z = 106.0                   # the landscape holds +-128 m: the macro shape stays under this (detail adds a few m)

# --- places (x, y) ---------------------------------------------------------------------------------------------
GATE = (-205.0, 2395.0)         # the barrier across the track, with the reserve's board
HUT = (-178.0, 2440.0)          # the ranger's hut (сторожка), its door facing the track (west)
HUT_YAW = 0.0                   # Unreal yaw of the hut's front (+X = west)
CAMP = (140.0, 3160.0)          # the campsite's fire ring
CAMP_R = 24.0
FUMAROLES_C = (-430.0, 3490.0)  # the Fumarole Valley's middle
FUMAROLES_R = (70.0, 135.0)     # its half sizes east-west, north-south
SPRING = (-372.0, 3398.0)       # the hot spring's pool at the valley's mouth
CONE = (-640.0, 3840.0)         # the cinder cone's centre
CONE_RIM_R = 52.0
CONE_BASE_R = 205.0
CONE_H = 58.0                   # rim over the land around
CRATER_DEPTH = 27.0
BREACH_YAW = math.radians(8.0)  # the rim is broken toward map +X (west), a little north of it
BREACH_HALF = math.radians(34.0)
RIDGE_X = (-960.0, 260.0)       # the Ridge runs between these, fading over 160 m at each end
RIDGE_H = 18.0
RIDGE_HALF = 62.0
# the lava flow: from the breach down the trough between the Ridge and the northern foot, west to the cliff's brow
LAVA = [(-592.0, 3846.0), (-520.0, 3832.0), (-430.0, 3800.0), (-300.0, 3772.0), (-150.0, 3786.0), (0.0, 3790.0),
        (130.0, 3762.0), (250.0, 3712.0), (330.0, 3668.0)]
LAVA_HALF = (26.0, 48.0)        # half width at its head and at its toe
LAVA_THICK = 4.5
# snowfields: (x, y, half along, half across, turn of the long axis from map X in degrees)
SNOWFIELDS = [
    (-150.0, 3596.0, 95.0, 30.0, 6.0),      # the Ridge's north slope, the big one over the camp's trail
    (60.0, 3588.0, 60.0, 24.0, -8.0),
    (-760.0, 3602.0, 70.0, 26.0, 4.0),
    (-640.0, 4006.0, 85.0, 34.0, 0.0),      # in the hollow between the Cone and the northern foot
    (-60.0, 4030.0, 120.0, 30.0, 5.0),      # gullies of the northern foot
    (420.0, 4010.0, 100.0, 28.0, -6.0),
    (-1010.0, 3920.0, 90.0, 36.0, 20.0),
    (36.0, 3345.0, 40.0, 16.0, 30.0),       # a small one by the trail north of the camp
]
SNOW_HOLLOW = 1.6               # metres each snowfield's hollow is dug into the macro shape (the snow fills it)


def _smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def _ramp(d, r, soft):
    return min(1.0, max(0.0, (r + soft - d) / soft))


def _seg_dist(x, y, pts):
    """(distance to the polyline, 0..1 along it)."""
    best, at, run, total = 1e9, 0.0, 0.0, 0.0
    lens = [math.hypot(q[0] - p[0], q[1] - p[1]) for p, q in zip(pts, pts[1:])]
    total = sum(lens) or 1.0
    for (p, q), ln in zip(zip(pts, pts[1:]), lens):
        dx, dy = q[0] - p[0], q[1] - p[1]
        t = min(1.0, max(0.0, ((x - p[0]) * dx + (y - p[1]) * dy) / (ln * ln or 1.0)))
        d = math.hypot(x - p[0] - dx * t, y - p[1] - dy * t)
        if d < best:
            best, at = d, (run + ln * t) / total
        run += ln
    return best, at


def ridge_y(x):
    return 3520.0 + 38.0 * math.sin(x / 300.0)


def _ridge(x, y):
    """The Ridge's height here (0 away from it): a gap where the Fumarole Valley cuts it."""
    if not (RIDGE_X[0] - 170.0 < x < RIDGE_X[1] + 170.0) or abs(y - ridge_y(x)) > 3.2 * RIDGE_HALF:
        return 0.0
    ends = _smooth((x - RIDGE_X[0] + 160.0) / 160.0) * _smooth((RIDGE_X[1] + 160.0 - x) / 160.0)
    gap = 1.0 - math.exp(-((x - FUMAROLES_C[0]) / 105.0) ** 2)
    d = (y - ridge_y(x)) / RIDGE_HALF
    # the south side long and easy, the north side shorter: snow lies on it
    d = d * (1.25 if d > 0 else 0.85)
    return RIDGE_H * math.exp(-d * d) * ends * gap * (0.8 + 0.2 * math.sin(x / 83.0 + 0.4))


def _land(x, y):
    """The North's plateau without its built-up features: the rise from the hills, broad swells, the Ridge, the
    eastern and northern feet of the mountains."""
    z = 12.0 + 22.0 * _smooth((y - 2250.0) / 450.0) + 11.0 * _smooth((y - 2900.0) / 650.0)
    z += 3.6 * math.sin(x / 210.0 + 0.7) * math.sin(y / 260.0 + 1.1) + 2.2 * math.sin((x + 0.6 * y) / 140.0)
    z += _ridge(x, y)
    # the Fumarole Valley: a trough through the Ridge, opening south onto the meadows
    fx, fy = (x - FUMAROLES_C[0]) / (FUMAROLES_R[0] * 1.5), (y - FUMAROLES_C[1]) / (FUMAROLES_R[1] * 1.35)
    q = fx * fx + fy * fy
    if q < 4.0:
        z -= 7.0 * math.exp(-q * 1.2)
    # the mountains' feet: inland (east) and north the land climbs out of the map
    east = 52.0 * _smooth((-x - 880.0) / 430.0) ** 1.5
    wav = 28.0 * math.sin(x / 260.0 + 1.9) + 14.0 * math.sin(x / 97.0)
    north = 60.0 * _smooth((y - 3950.0 - wav) / 300.0) ** 1.3
    z += max(east, north) + 0.35 * min(east, north)
    return z


def _cone(x, y):
    """The cinder cone's height over the land (0 away from it)."""
    dx, dy = x - CONE[0], y - CONE[1]
    r = math.hypot(dx, dy)
    if r > CONE_BASE_R * 1.25:
        return 0.0
    a = math.atan2(dy, dx)
    floor = CONE_H - CRATER_DEPTH                     # the crater's floor: level, the same from every side
    # the breach: toward the west the rim is gone down to the floor, where the lava broke out (the way in)
    da = abs((a - BREACH_YAW + math.pi) % (2 * math.pi) - math.pi)
    br = 1.0 - _smooth((da - BREACH_HALF * 0.3) / (BREACH_HALF * 0.9))
    rim_h = CONE_H * (1.0 + 0.045 * math.sin(3.0 * a + 0.8) + 0.02 * math.sin(7.0 * a))
    rim_h += (floor + 1.0 - rim_h) * br
    if r >= CONE_RIM_R:
        t = (r - CONE_RIM_R) / (CONE_BASE_R - CONE_RIM_R)
        if t >= 1.25:
            return 0.0
        # ~32 degrees under the rim, easing out into the ash apron
        h = rim_h * max(0.0, 1.0 - t) ** 1.45
        return h + 1.2 * _smooth((1.25 - t) / 0.4) * (1.0 - _smooth((1.0 - t) / 0.3))
    # the crater: a flat floor 40 m across, walls steepening up to a rounded rim
    u = r / CONE_RIM_R
    wall = _smooth((u - 0.38) / 0.62)
    return floor + (rim_h - floor) * (0.35 * wall + 0.65 * wall * wall)


def lava(x, y):
    """0..1 on the lava flow (a rough blocky tongue from the breach to the cliff's brow)."""
    if not (LAVA[0][0] - 80.0 < x < LAVA[-1][0] + 80.0 and 3600.0 < y < 3920.0):
        return 0.0
    d, t = _seg_dist(x, y, LAVA)
    half = LAVA_HALF[0] + (LAVA_HALF[1] - LAVA_HALF[0]) * t
    half *= 1.0 + 0.22 * math.sin(t * 23.0) + 0.12 * math.sin(t * 61.0 + 1.0)
    # none inside the crater and its breach: the way in stays a cinder floor
    return _ramp(d, half - 7.0, 7.0) * (1.0 - _ramp(math.hypot(x - CONE[0], y - CONE[1]), CONE_RIM_R + 12.0, 45.0))


def _snow_local(x, y, f):
    cx, cy, ha, hb, turn = f
    c, s = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    u, v = ((x - cx) * c + (y - cy) * s) / ha, (-(x - cx) * s + (y - cy) * c) / hb
    return u * u + v * v


def snow(x, y):
    """0..1 on a snowfield (TerrainGen frays the edge and lays the snow)."""
    if y < 3300.0:
        return 0.0
    best = 0.0
    for f in SNOWFIELDS:
        if abs(x - f[0]) > f[2] + 10.0 or abs(y - f[1]) > f[2] + 10.0:
            continue
        q = _snow_local(x, y, f)
        if q < 1.0:
            best = max(best, 1.0 - q)                 # a plain dome: TerrainGen's noise cuts the outline
    return best


@functools.lru_cache(maxsize=None)
def _pad_z(px, py):
    return _raw(px, py)


def _raw(x, y):
    z = _land(x, y)
    z += _cone(x, y)
    lv = lava(x, y)
    if lv > 0.0:
        z += LAVA_THICK * lv
    for f in SNOWFIELDS:
        if abs(x - f[0]) < f[2] and abs(y - f[1]) < f[2]:
            q = _snow_local(x, y, f)
            if q < 1.0:
                z -= SNOW_HOLLOW * (1.0 - q) ** 2
    return min(z, MAX_Z)


PADS = [(HUT, 10.0, 6.0), (CAMP, CAMP_R, 12.0), (SPRING, 5.0, 5.0)]     # level ground: centre, radius, fade


def plateau(x, y, natural):
    """world_map.plateau's hook: the land before the sea cuts it. natural = the map's own height there (the north
    hills); the North takes over across BLEND."""
    if y <= BLEND[0]:
        return natural
    z = _raw(x, y)
    for (px, py), r, soft in PADS:
        d = math.hypot(x - px, y - py)
        if d < r + soft:
            z += (_pad_z(px, py) - z) * _ramp(d, r, soft)
    k = _smooth((y - BLEND[0]) / (BLEND[1] - BLEND[0]))
    return natural + (z - natural) * k


def keep_flat(x, y):
    """0..1: where the exact macro height must stay (the hut's and the camp's ground, the spring's pool)."""
    if y < Y0:
        return 0.0
    k = 0.0
    for (px, py), r, soft in PADS:
        k = max(k, _ramp(math.hypot(x - px, y - py), r * 0.7, soft))
    return k


def scoria(x, y):
    """0..1: loose volcanic cinder: the Cone and its ash apron."""
    r = math.hypot(x - CONE[0], y - CONE[1])
    return _ramp(r, CONE_BASE_R * 0.92, 75.0)


@functools.lru_cache(maxsize=1)
def fumaroles():
    """The vents, mud pots and springs: dicts {id, kind, x, y, size}. kind: 'vent' (a hissing steam jet from a crusted
    hole), 'steam' (soft steaming ground), 'mudpot' (a pool of plopping grey mud), 'spring' (a hot pool).
    Deterministic; ids are stable (quests can name them)."""
    rnd = random.Random(1736)
    out = []

    def add(kind, x, y, size):
        out.append({"id": "F%02d" % (len(out) + 1), "kind": kind, "x": round(x, 1), "y": round(y, 1), "size": round(size, 2)})

    cx, cy = FUMAROLES_C
    rx, ry = FUMAROLES_R
    tries = 0
    kinds = ["vent"] * 9 + ["steam"] * 12 + ["mudpot"] * 5
    while kinds and tries < 4000:
        tries += 1
        u, v = rnd.uniform(-1, 1), rnd.uniform(-1, 1)
        if u * u + v * v > 1.0:
            continue
        x, y = cx + u * rx, cy + v * ry
        if any(math.hypot(x - f["x"], y - f["y"]) < 13.0 for f in out):
            continue
        kind = kinds.pop(0)
        add(kind, x, y, {"vent": rnd.uniform(0.7, 1.6), "steam": rnd.uniform(0.8, 1.5), "mudpot": rnd.uniform(1.2, 3.2)}[kind])
    add("spring", SPRING[0], SPRING[1], 3.2)
    # the crater: two vents on its floor and steaming ground under the rim
    add("vent", CONE[0] - 9.0, CONE[1] + 6.0, 1.8)
    add("vent", CONE[0] + 14.0, CONE[1] - 11.0, 1.2)
    add("steam", CONE[0] - 24.0, CONE[1] - 26.0, 1.4)
    # and one far out on the lava flow's edge
    add("steam", -120.0, 3746.0, 1.0)
    return out


def sulfur(x, y):
    """0..1: ground altered by the steam (pale clay, sulphur crusts): the Fumarole Valley's floor and a halo round
    every vent."""
    if not (3300.0 < y < 3950.0 and -760.0 < x < -40.0):
        return 0.0
    fx, fy = (x - FUMAROLES_C[0]) / (FUMAROLES_R[0] * 1.25), (y - FUMAROLES_C[1]) / (FUMAROLES_R[1] * 1.15)
    k = 0.42 * _smooth(1.35 - (fx * fx + fy * fy))   # patchy on the valley floor (TerrainGen's noise), solid at the vents
    for f in fumaroles():
        d = math.hypot(x - f["x"], y - f["y"])
        r = 4.0 + 3.5 * f["size"]
        if d < r + 9.0:
            k = max(k, _ramp(d, r, 9.0))
    return k


def tree_free(x, y):
    """True where nothing woody grows: the Cone, the lava, the steaming ground, snow, the camp's and the hut's pads."""
    if y < Y0:
        return False
    if scoria(x, y) > 0.3 or lava(x, y) > 0.0 or sulfur(x, y) > 0.2 or snow(x, y) > 0.0:
        return True
    return any(math.hypot(x - px, y - py) < r + 4.0 for (px, py), r, _ in PADS)


# --- vegetation ---------------------------------------------------------------------------------------------------

FOREST_MIX = {
    # Kamchatka's park woodland: stone birch with rowan, wide apart over tall grass
    "stonebirch": [("StoneBirch", 0.7, 15, 70), ("Rowan", 0.18, 8, 25), ("DwarfBirch", 0.12, 6, 18)],
    # the shrub belt above it: dwarf birch and juniper in clumps
    "shrub": [("DwarfBirch", 0.55, 6, 20), ("Juniper", 0.3, 10, 30), ("Rowan", 0.15, 6, 14)],
}


def forests(tiles):
    """Forest tiles of the North: (x, y, half x, half y, mix, density, min z, max z). tiles = world_map._tiles."""
    out = []
    x1 = W.cliff_top(2500.0) - 10.0
    for x, y, hx, hy in tiles(W.X_MIN, x1, FOREST_END, 2700.0):
        out.append((x, y, hx, hy, "stonebirch", 0.085, 5.0, 60.0))
    for x, y, hx, hy in tiles(W.X_MIN, x1, 2700.0, 3420.0):
        out.append((x, y, hx, hy, "shrub", 0.08, 20.0, 70.0))
    return out


def grass_patches(seed0=1200):
    """Meadow grass where the hero walks and stops: round the hut, the camp, the spring and along the trail between
    them, as world_map.grass_patches rows."""
    out = []
    meadow = ("MeadowGrass", "RedFescue", "WavyHairGrass")
    spots = [HUT, CAMP, (CAMP[0] + 60.0, CAMP[1]), (CAMP[0], CAMP[1] + 60.0), (CAMP[0] - 60.0, CAMP[1]),
             (CAMP[0], CAMP[1] - 60.0), (SPRING[0] + 40.0, SPRING[1] - 50.0)]
    spots += trail_points(120.0)
    seen = []
    for i, (x, y) in enumerate(spots):
        cx, cy = round(x / 60.0) * 60.0, round(y / 60.0) * 60.0
        if (cx, cy) in seen or tree_free(cx, cy) and sulfur(cx, cy) > 0.5:
            continue
        seen.append((cx, cy))
        out.append((cx, cy, 30.0, 30.0, meadow[len(seen) % 3], seed0 + len(seen), 1.0, 0.0, 80.0))
    return out


# --- the trail ----------------------------------------------------------------------------------------------------

# the dirt track's anchors (tools/terrain.py routes between them over the land)
ANCHORS = {
    "north_gate": GATE,
    "north_camp": (CAMP[0] - 30.0, CAMP[1] - 8.0),
    "north_spring": (SPRING[0] + 14.0, SPRING[1] - 16.0),
    "north_fumaroles": (FUMAROLES_C[0] + 20.0, FUMAROLES_C[1] + 150.0),
    "north_breach": (CONE[0] + 150.0, CONE[1] + 44.0),
    "north_crater": (CONE[0] + 20.0, CONE[1] + 4.0),
}
TRACKS = [("woods_north", "north_gate"), ("north_gate", "north_camp"), ("north_camp", "north_spring"),
          ("north_spring", "north_fumaroles"), ("north_fumaroles", "north_breach"), ("north_breach", "north_crater")]


def trail_points(step):
    """Points every step metres along the straight lines between the trail's anchors (a rough line for planting;
    the real track is terrain.tracks())."""
    names = ["north_gate", "north_camp", "north_spring", "north_fumaroles", "north_breach", "north_crater"]
    pts = [ANCHORS[n] for n in names]
    out = []
    for p, q in zip(pts, pts[1:]):
        n = max(1, int(math.hypot(q[0] - p[0], q[1] - p[1]) / step))
        out += [(p[0] + (q[0] - p[0]) * k / n, p[1] + (q[1] - p[1]) * k / n) for k in range(n)]
    return out + [pts[-1]]


# --- things standing there --------------------------------------------------------------------------------------

def camp():
    """The campsite: tents round the fire ring, a table under a tarp, a log bench, the food pole, the board.
    Each {id, kind, x, y, yaw (Unreal degrees, the front), variant}."""
    rnd = random.Random(52)
    out = [{"id": "camp_fire", "kind": "fire_ring", "x": CAMP[0], "y": CAMP[1], "yaw": 0.0, "variant": 0}]
    tents = [(-13.0, 6.5, "dome", 0), (-8.0, -12.0, "dome", 1), (7.5, -14.5, "ridge", 0), (15.5, -3.0, "dome", 2),
             (12.0, 11.5, "tunnel", 0), (-2.0, 16.5, "ridge", 1)]
    for i, (dx, dy, kind, variant) in enumerate(tents):
        yaw = math.degrees(math.atan2(-dy, -dx)) + rnd.uniform(-18, 18)      # doors toward the fire
        out.append({"id": "camp_tent_%d" % (i + 1), "kind": "tent_" + kind, "x": CAMP[0] + dx, "y": CAMP[1] + dy,
                    "yaw": round(yaw, 1), "variant": variant})
    for i, a in enumerate((35.0, 150.0, 265.0)):
        out.append({"id": "camp_log_%d" % (i + 1), "kind": "log_bench", "x": CAMP[0] + 2.6 * math.cos(math.radians(a)),
                    "y": CAMP[1] + 2.6 * math.sin(math.radians(a)), "yaw": a + 180.0, "variant": i % 2})
    out += [
        {"id": "camp_table", "kind": "camp_table", "x": CAMP[0] + 6.5, "y": CAMP[1] + 4.5, "yaw": 30.0, "variant": 0},
        {"id": "camp_tarp", "kind": "tarp_shelter", "x": CAMP[0] + 6.5, "y": CAMP[1] + 4.5, "yaw": 30.0, "variant": 0},
        {"id": "camp_woodpile", "kind": "woodpile", "x": CAMP[0] - 4.5, "y": CAMP[1] - 4.0, "yaw": 70.0, "variant": 0},
        {"id": "camp_foodpole", "kind": "food_pole", "x": CAMP[0] - 19.0, "y": CAMP[1] - 9.0, "yaw": 0.0, "variant": 0},
        {"id": "camp_line", "kind": "clothes_line", "x": CAMP[0] + 17.0, "y": CAMP[1] + 4.0, "yaw": 100.0, "variant": 0},
        {"id": "camp_board", "kind": "board_camp", "x": CAMP[0] - 27.0, "y": CAMP[1] - 6.0, "yaw": 190.0, "variant": 0},
        {"id": "camp_toilet", "kind": "outhouse", "x": CAMP[0] + 31.0, "y": CAMP[1] + 22.0, "yaw": 215.0, "variant": 0},
    ]
    for o in out:
        o["x"], o["y"] = round(o["x"], 2), round(o["y"], 2)
    return out


def signs():
    """Boards and posts: {id, kind, x, y, yaw, text (a strings key)}."""
    fx, fy = ANCHORS["north_fumaroles"]
    return [
        {"id": "sign_gate", "kind": "board_reserve", "x": GATE[0] + 4.0, "y": GATE[1] + 1.5, "yaw": 250.0, "text": "north.sign.reserve"},
        {"id": "gate_barrier", "kind": "barrier", "x": GATE[0], "y": GATE[1], "yaw": 160.0, "text": ""},
        {"id": "sign_hot_s", "kind": "sign_hot_ground", "x": SPRING[0] - 22.0, "y": SPRING[1] + 20.0, "yaw": 250.0, "text": "north.sign.hot_ground"},
        {"id": "sign_hot_n", "kind": "sign_hot_ground", "x": fx + 6.0, "y": fy - 6.0, "yaw": 90.0, "text": "north.sign.hot_ground"},
        {"id": "sign_spring", "kind": "sign_post", "x": SPRING[0] + 6.5, "y": SPRING[1] - 5.0, "yaw": 200.0, "text": "north.sign.spring"},
        {"id": "sign_cone", "kind": "sign_post", "x": ANCHORS["north_breach"][0] + 5.0, "y": ANCHORS["north_breach"][1] - 3.0, "yaw": 300.0, "text": "north.sign.cone"},
        {"id": "sign_camp", "kind": "sign_post", "x": CAMP[0] - 34.0, "y": CAMP[1] - 12.0, "yaw": 180.0, "text": "north.sign.camp"},
    ]


def far_volcanoes():
    """The big volcanoes beyond the map (backdrop meshes, never walked on): {id, x, y, base z, height, radius (all
    metres), crater radius, snow line z, plume 0..1}."""
    return [
        {"id": "big", "x": -1500.0, "y": 9600.0, "base": 60.0, "height": 2850.0, "radius": 5200.0, "crater": 230.0,
         "snow": 1250.0, "plume": 0.0},
        {"id": "smoker", "x": -5200.0, "y": 7600.0, "base": 80.0, "height": 1950.0, "radius": 3600.0, "crater": 260.0,
         "snow": 1350.0, "plume": 1.0},
        {"id": "sea", "x": 3900.0, "y": 13500.0, "base": -43.0, "height": 1500.0, "radius": 3300.0, "crater": 140.0,
         "snow": 1150.0, "plume": 0.0},
    ]


def places():
    """Named places: {id, name (a strings key), x, y, yaw (where an arriving hero looks, Unreal degrees)}."""
    cb = ANCHORS["north_breach"]
    return [
        {"id": "north_gate", "name": "place.north.gate", "x": GATE[0] + 3.0, "y": GATE[1] - 8.0, "yaw": 90.0},
        {"id": "north_hut", "name": "place.north.hut", "x": HUT[0] + 9.0, "y": HUT[1], "yaw": 180.0},
        {"id": "north_meadows", "name": "place.north.meadows", "x": -30.0, "y": 2800.0, "yaw": 90.0},
        {"id": "north_camp", "name": "place.north.camp", "x": CAMP[0] - 6.0, "y": CAMP[1] - 5.0, "yaw": 45.0},
        {"id": "north_spring", "name": "place.north.spring", "x": SPRING[0] + 6.0, "y": SPRING[1] - 6.0, "yaw": 135.0},
        {"id": "north_fumaroles", "name": "place.north.fumaroles", "x": FUMAROLES_C[0] + 30.0, "y": FUMAROLES_C[1] - 100.0, "yaw": 100.0},
        {"id": "north_snowfield", "name": "place.north.snowfield", "x": -150.0, "y": 3622.0, "yaw": 270.0},
        {"id": "north_lava", "name": "place.north.lava", "x": -300.0, "y": 3740.0, "yaw": 110.0},
        {"id": "north_breach", "name": "place.north.breach", "x": cb[0], "y": cb[1], "yaw": 190.0},
        {"id": "north_crater", "name": "place.north.crater", "x": CONE[0] + 22.0, "y": CONE[1] + 4.0, "yaw": 180.0},
        {"id": "north_rim", "name": "place.north.rim", "x": CONE[0] - 2.0, "y": CONE[1] - CONE_RIM_R + 1.0, "yaw": 270.0},
    ]


def build():
    return {
        "frame": "map metres (docs/world_map.md): x west, y north; heights from the built ground (terrain.ground_z)",
        "places": places(),
        "hut": {"x": HUT[0], "y": HUT[1], "yaw": HUT_YAW},
        "camp": camp(),
        "signs": signs(),
        "fumaroles": fumaroles(),
        "spring": {"x": SPRING[0], "y": SPRING[1], "r": 2.6},
        "cone": {"x": CONE[0], "y": CONE[1], "rim_r": CONE_RIM_R, "base_r": CONE_BASE_R},
        "snowfields": [{"x": f[0], "y": f[1], "half_along": f[2], "half_across": f[3], "turn": f[4]} for f in SNOWFIELDS],
        "lava": [list(p) for p in LAVA],
        "far_volcanoes": far_volcanoes(),
    }


def write():
    data = build()
    for path, obj in ((OUT, data),
                      (OUT_TELEPORTS, {"teleports": [{"id": p["id"], "group": "north", "name": "dev.tp." + p["id"],
                                                      "x": p["x"], "y": p["y"], "yaw": p["yaw"]} for p in data["places"]]})):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(obj, f, indent=1, ensure_ascii=False)
            f.write("\n")
    return data


if __name__ == "__main__":
    d = write()
    print("north: %d places, %d fumaroles, %d camp things, %d signs -> %s" %
          (len(d["places"]), len(d["fumaroles"]), len(d["camp"]), len(d["signs"]), os.path.normpath(OUT)))
