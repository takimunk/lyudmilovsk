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
import north_land as L  # noqa: E402
import north_river as R  # noqa: E402
import world_map as W  # noqa: E402  (world_map imports this module lazily)

HERE = os.path.dirname(os.path.abspath(__file__))
CONTENT = os.path.join(HERE, "..", "unreal", "Lyudmilovsk", "Content")
OUT = os.path.join(CONTENT, "North", "north.json")
OUT_TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "north.json")

Y0 = 2200.0                     # where volcano country's own ground (snow, cinder, lava) may begin
HILLS_Y = 1450.0                # the town's north edge: from here north the land is this module's
WOODS_Y = (1450.0, 2700.0)      # the woods between the town and the meadows are drawn by wood() (a cover mask)
BLEND = (2180.0, 2520.0)        # the north hills' own height gives way to the North's over this band
FOREST_END = 2330.0             # the old forest's tiles stop here; park woodland and shrubs beyond
MAX_Z = 215.0                   # the landscape holds +-256 m (LyudTerrain.cpp LandscapeZScale 100)

# --- places (map metres). The land is real (tools/north_land.py: Tolbachik's Northern Breakthrough cones at half
# size); these are spots chosen on it -----------------------------------------------------------------------------
GATE = (100.0, 2445.0)          # the barrier across the track, with the reserve's board: the brow over the river
HUT = (130.0, 2500.0)           # the ranger's hut (сторожка) on a level shelf of the brow, its door facing the track
CROSSING_X = -150.0             # where the trail crosses the river's plain (north_river.crossing)
POLAR = (-360.0, 1660.0)        # the cable car's Polar station on the ridge over the river: the trail starts there
HUT_YAW = 180.0                 # Unreal yaw of the hut's front (map -X, east: toward the track)
CAMP = (140.0, 3100.0)          # the campsite's fire ring: a level meadow under a low terrace, the cones behind it
CAMP_R = 24.0
OLD_CONE = (-385.0, 2720.0)     # the Old Cone (Старая сопка): the southernmost cone, grassed over, over the hut
OLD_CONE_TOP = (-370.0, 2780.0)
HIGH_CONE = (-490.0, 3125.0)    # the High Cone (Высокая сопка): the tallest, a closed crater 150 m across, snow in it
HIGH_RIM = (-455.0, 3212.0)     # where the path gains its rim
CONE = (-560.0, 3535.0)         # the Cone (Сопка): the live one. Its crater is a horseshoe open to the south-west;
CONE_RIM_R = 95.0               # steam comes out of its floor and all down the breach
CONE_BASE_R = 330.0
FUMAROLES_C = (-440.0, 3503.0)  # the Fumarole Valley: the trough that runs out of the Cone's breach to the meadow
FUMAROLES_R = (125.0, 34.0)     # its half sizes east-west, north-south
SPRING = (-296.0, 3502.0)       # the hot spring's pool at the valley's mouth
SPRING_R = 2.6                  # the pool's radius
SPRING_DEPTH = 1.05             # its bed under the ground round it (the water stands 0.15 m under the rim)
SMALL_CONE = (-650.0, 3900.0)   # the Small Cone (Малая сопка), the northernmost
LAVA_VENT = (-405.0, 3600.0)    # where the lava left the Cone's flank
LAVA_HEAD_X = -400.0            # the lava field: from here west to the sea,
LAVA_Y = (3440.0, 3830.0)       # between these
SNOW_MIN_Z = 62.0               # no summer snow below this
SNOW_SHARE = 0.11               # the share of the land above it that keeps snow
SNOW_HOLLOW = 1.2               # metres a snowfield's bed is dug in (the snow fills it)


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


def _near(x, y, c, r, soft):
    return _ramp(math.hypot(x - c[0], y - c[1]), r, soft)


def lava(x, y):
    """0..1 on the lava field: blocky clinker. It is where the real one is: the levees and channels the flows of the
    Cone left on their way west to the sea are rough ground in the heights (north_land.rough), the ash plain smooth."""
    if not (LAVA_Y[0] - 60.0 < y < LAVA_Y[1] + 60.0 and x > LAVA_HEAD_X - 80.0):
        return 0.0
    k = _smooth((L.rough().at(x, y) - 1.8) / 1.2)
    if k <= 0.0:
        return 0.0
    k *= _smooth((y - LAVA_Y[0] + 60.0) / 90.0) * _smooth((LAVA_Y[1] + 60.0 - y) / 90.0) * _smooth((x - LAVA_HEAD_X + 80.0) / 120.0)
    k *= 1.0 - _smooth((L.relief().at(x, y) - 7.0) / 20.0)             # the cones are cinder, not lava
    return k * (1.0 - _near(x, y, SPRING, 45.0, 50.0)) * (1.0 - _near(x, y, FUMAROLES_C, 60.0, 80.0))


def snow(x, y):
    """0..1 on a summer snowfield (TerrainGen frays the edge and lays the firn): shaded hollows and north slopes of
    the cones, found on the real land (north_land.snow), never on hot ground."""
    if y < 2900.0:
        return 0.0
    k = L.snow(SNOW_MIN_Z, SNOW_SHARE).at(x, y)
    if k <= 0.0:
        return 0.0
    hot = max(_near(x, y, FUMAROLES_C, 150.0, 60.0), _near(x, y, CONE, 70.0, 40.0), lava(x, y))
    return _smooth(k * 1.6 - 0.25) * (1.0 - hot)


@functools.lru_cache(maxsize=1)
def snowfields():
    """The snowfields as patches for the game and the docs: {id, x, y, r (the radius of a circle of its area), area}.
    Largest first; ids are stable while the land and SNOW_* stay."""
    g = L.snow(SNOW_MIN_Z, SNOW_SHARE)
    nx, ny, st = g.nx, g.ny, g.step
    live = [False] * len(g.v)
    for j in range(ny):
        for i in range(nx):
            x, y = g.xy(i, j)
            live[j * nx + i] = g.v[j * nx + i] > 0.3 and snow(x, y) > 0.35
    out = []
    for k0 in range(len(live)):
        if not live[k0]:
            continue
        todo, cells = [k0], []
        live[k0] = False
        while todo:
            k = todo.pop()
            cells.append(k)
            for q in (k - 1, k + 1, k - nx, k + nx):
                if 0 <= q < len(live) and live[q]:
                    live[q] = False
                    todo.append(q)
        if len(cells) * st * st < 300.0:
            continue
        cx = sum(g.xy(k % nx, k // nx)[0] for k in cells) / len(cells)
        cy = sum(g.xy(k % nx, k // nx)[1] for k in cells) / len(cells)
        area = len(cells) * st * st
        out.append({"x": round(cx, 1), "y": round(cy, 1), "r": round(math.sqrt(area / math.pi), 1), "area": round(area)})
    out.sort(key=lambda f: -f["area"])
    for n, f in enumerate(out):
        f["id"] = "S%02d" % (n + 1)
    return out


def _raw(x, y):
    z = L.z(x, y)
    sn = L.snow(SNOW_MIN_Z, SNOW_SHARE).at(x, y) if y > 2900.0 else 0.0
    if sn > 0.0:
        z -= SNOW_HOLLOW * _smooth(sn * 1.6 - 0.25)
    return min(z, MAX_Z)


PADS = [(HUT, 10.0, 8.0), (CAMP, CAMP_R, 14.0), (SPRING, 5.0, 6.0)]     # level ground: centre, radius, fade


def hills(x, y):
    """The hills between the town and the river (world_map.plateau raises the valley to this from HILLS_Y): a broad
    rolling ridge, higher inland, a lower headland toward the sea."""
    z = 14.0 + 9.0 * L.fbm(x, y, 420.0, 3, 61) + 3.5 * L.fbm(x, y, 140.0, 2, 62)
    return z + 8.0 * _smooth((-x - 200.0) / 700.0) - 7.0 * _smooth((x - 50.0) / 250.0)


def _shape(x, y, natural):
    """The land without the pads: the hills, giving way to the real volcano land across BLEND, the river's valley cut
    into both."""
    z = natural
    if y > BLEND[0]:
        z = natural + (_raw(x, y) - natural) * _smooth((y - BLEND[0]) / (BLEND[1] - BLEND[0]))
    return R.apply(x, y, z)


@functools.lru_cache(maxsize=None)
def _pad_z(px, py):
    return _shape(px, py, W.plateau_south(px, py))


def plateau(x, y, natural):
    """world_map.plateau's hook: the land before the sea cuts it. natural = the map's own height there (the hills,
    hills()); the river cuts its valley, the North takes over across BLEND."""
    z = _shape(x, y, natural)
    for (px, py), r, soft in PADS:
        d = math.hypot(x - px, y - py)
        if d < r + soft:
            z += (_pad_z(px, py) - z) * _ramp(d, r, soft)
    d = math.hypot(x - SPRING[0], y - SPRING[1])
    if d < SPRING_R + 0.5:                               # the hot spring's basin: a ledge to sit on, deeper in the middle
        z -= SPRING_DEPTH * (0.62 * _ramp(d, SPRING_R - 0.9, 0.7) + 0.38 * _ramp(d, 0.5, 0.9))
    return z


def keep_flat(x, y):
    """0..1: where the exact macro height must stay (the hut's and the camp's ground, the spring's pool, the river's
    gravel plain with its channels and bars)."""
    k = R.protect(x, y)
    for (px, py), r, soft in PADS:
        k = max(k, _ramp(math.hypot(x - px, y - py), r * 0.7, soft))
    return k


def shore_shift(y):
    """Metres the water line stands west of world_map.SHORE's smooth curve (world_map.shore_x adds it north of the
    town): the coast wanders in coves and points, gives way in a bay at the river's mouth between two headlands, and
    the Cone's lava stands out into the sea as a cape."""
    f = _smooth((y - 1500.0) / 150.0)
    if f <= 0.0:
        return 0.0
    d = 42.0 * L.fbm(y, 0.0, 330.0, 2, 81) + 15.0 * L.fbm(y, 7.0, 110.0, 2, 82)
    d += -45.0 * math.exp(-((y - 1885.0) / 130.0) ** 2)
    d += 26.0 * math.exp(-((y - 1700.0) / 90.0) ** 2) + 32.0 * math.exp(-((y - 2240.0) / 110.0) ** 2)
    d += 60.0 * _smooth((y - LAVA_Y[0]) / 90.0) * _smooth((LAVA_Y[1] - y) / 90.0) * (0.75 + 0.25 * math.sin(y / 37.0))
    return f * d


def scoria(x, y):
    """0..1: loose volcanic cinder. Whatever stands high over the land around is a cone (north_land.relief); the young
    ones are bare, the Old Cone and the small eastern ones have grassed over, grass climbs the feet of the rest."""
    if y < 2700.0:
        return 0.0
    k = _smooth((L.relief().at(x, y) - 3.0) / 42.0)
    if k <= 0.0:
        return 0.0
    k *= 1.0 - _near(x, y, OLD_CONE, 330.0, 170.0)
    if x < -820.0:
        k *= 1.0 - 0.75 * _smooth((-820.0 - x) / 120.0)
    return k


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
    kinds = ["vent"] * 8 + ["steam"] * 11 + ["mudpot"] * 5
    while kinds and tries < 6000:
        tries += 1
        u, v = rnd.uniform(-1, 1), rnd.uniform(-1, 1)
        if u * u + v * v > 1.0:
            continue
        x = cx + u * rx
        y = cy + v * ry + 14.0 * math.sin((x - cx) / 60.0)          # the trough winds a little
        if any(math.hypot(x - f["x"], y - f["y"]) < 12.0 for f in out):
            continue
        gx, gy = L.slope(x, y)
        kind = kinds[0]
        if kind == "mudpot" and math.hypot(gx, gy) > 0.16:           # mud pools only where the floor is nearly level
            continue
        kinds.pop(0)
        add(kind, x, y, {"vent": rnd.uniform(0.7, 1.6), "steam": rnd.uniform(0.8, 1.5), "mudpot": rnd.uniform(1.2, 3.2)}[kind])
    add("spring", SPRING[0], SPRING[1], 3.2)
    # the Cone's crater: vents on its floor and steaming ground under the back wall
    add("vent", CONE[0] + 8.0, CONE[1] - 4.0, 1.8)
    add("vent", CONE[0] - 14.0, CONE[1] + 12.0, 1.3)
    add("steam", CONE[0] - 30.0, CONE[1] - 12.0, 1.4)
    add("steam", CONE[0] + 2.0, CONE[1] + 30.0, 1.2)
    # one in the High Cone's crater, and one where the lava left the flank
    add("steam", HIGH_CONE[0] + 12.0, HIGH_CONE[1] + 8.0, 1.1)
    add("steam", LAVA_VENT[0] + 26.0, LAVA_VENT[1] - 6.0, 1.0)
    return out


def sulfur(x, y):
    """0..1: ground altered by the steam (pale clay, sulphur crusts): the Fumarole Valley's floor and a halo round
    every vent."""
    if not (3050.0 < y < 3750.0 and -700.0 < x < -220.0):
        return 0.0
    fx = (x - FUMAROLES_C[0]) / (FUMAROLES_R[0] * 1.2)
    fy = (y - FUMAROLES_C[1] - 14.0 * math.sin((x - FUMAROLES_C[0]) / 60.0)) / (FUMAROLES_R[1] * 1.3)
    k = 0.42 * _smooth(1.35 - (fx * fx + fy * fy))   # patchy on the valley floor (TerrainGen's noise), solid at the vents
    k = max(k, 0.42 * _near(x, y, CONE, 34.0, 26.0))  # and on the crater's floor
    for f in fumaroles():
        d = math.hypot(x - f["x"], y - f["y"])
        r = 4.0 + 3.5 * f["size"]
        if d < r + 9.0:
            k = max(k, _ramp(d, r, 9.0))
    return k


def tree_free(x, y):
    """True where nothing woody grows: the Cone, the lava, the steaming ground, snow, the camp's and the hut's pads."""
    if y < Y0:
        return any(math.hypot(x - px, y - py) < r + 4.0 for (px, py), r, _ in PADS)
    if scoria(x, y) > 0.3 or lava(x, y) > 0.8 or sulfur(x, y) > 0.2 or snow(x, y) > 0.0:
        return True
    return any(math.hypot(x - px, y - py) < r + 4.0 for (px, py), r, _ in PADS)


# --- vegetation ---------------------------------------------------------------------------------------------------

FOREST_MIX = {
    # Kamchatka's park woodland: stone birch with rowan, wide apart over tall grass
    "stonebirch": [("StoneBirch", 0.7, 15, 70), ("Rowan", 0.18, 8, 25), ("DwarfBirch", 0.12, 6, 18)],
    # the belt above it on the drier rises: Siberian dwarf pine (кедровый стланик) with dwarf birch and juniper
    "dwarfpine": [("SiberianDwarfPine", 0.6, 15, 60), ("DwarfBirch", 0.2, 6, 20), ("Juniper", 0.12, 10, 30),
                  ("Rowan", 0.08, 6, 14)],
    # gullies, hollows and the Ridge's wet north slope: dwarf alder thickets (ольховник)
    "alder": [("DwarfAlder", 0.65, 8, 30), ("DwarfBirch", 0.2, 6, 20), ("Rowan", 0.15, 6, 14)],
    # the thin scatter over the upper meadows: a dwarf pine or birch here and there, a juniper
    "shrub": [("DwarfBirch", 0.4, 6, 20), ("SiberianDwarfPine", 0.3, 12, 45), ("Juniper", 0.2, 10, 30),
              ("DwarfAlder", 0.1, 8, 25)],
    # golden rhododendron carpets round the snowfields (knee-high, evergreen, pale yellow in June)
    "rhodo": [("GoldenRhododendron", 0.85, 8, 30), ("SiberianDwarfPine", 0.15, 10, 30)],
}
# the forests' draw distance by mix, cm (AProceduralForest.CullDistance; 500 m when not named): knee-high shrubs go
# sooner than trees (import_world sets it)
FOREST_DETAIL = {"dwarfpine": {"cull_distance": 35000.0}, "alder": {"cull_distance": 35000.0},
                 "shrub": {"cull_distance": 30000.0}, "rhodo": {"cull_distance": 15000.0}}

BELT = (2700.0, 4190.0)         # the shrub belt: dense at its foot by the birches, thinning north over the lava plain
BELT_DENSITY = (0.17, 0.05)
BELT_CELL = (130.0, 120.0)      # cells it is sorted into (alder or dwarf pine), merged into runs along x


def _hollow(x, y, r=45.0):
    """The ground here over the ring r metres round it (negative in a hollow or a gully, positive on a rise)."""
    ring = sum(_raw(x + r * math.cos(k * math.pi / 4), y + r * math.sin(k * math.pi / 4)) for k in range(8)) / 8.0
    return _raw(x, y) - ring


def north_slope(x, y):
    """True on a slope that faces north and is not flat (snow lies late there and its meltwater keeps the ground wet)."""
    gx, gy = L.slope(x, y, 20.0)
    return gy < -0.07 and abs(gx) < 2.0 * -gy


def _wet_site(x, y):
    """Where alder grows rather than pine: the real land's gullies and hollows (the old lava channels, the troughs
    between the cones' feet) and the north-facing slopes."""
    return _hollow(x, y) < -0.8 or (north_slope(x, y) and _hollow(x, y) < 0.4)


def _runs(x0, x1, y0, y1, cell, kind):
    """Rectangles (x, y, hx, hy, kind) over x0..x1 x y0..y1: cells of about cell metres, each given kind(x, y, w, h)
    (None: left out), merged into runs of one kind along x."""
    nx, ny = max(1, round((x1 - x0) / cell[0])), max(1, round((y1 - y0) / cell[1]))
    cw, ch = (x1 - x0) / nx, (y1 - y0) / ny
    out = []
    for j in range(ny):
        cy = y0 + (j + 0.5) * ch
        row = [kind(x0 + (i + 0.5) * cw, cy, cw, ch) for i in range(nx)]
        i = 0
        while i < nx:
            k = i
            while k < nx and row[k] == row[i]:
                k += 1
            if row[i] is not None:
                out.append((x0 + (i + k) * cw / 2, cy, (k - i) * cw / 2, ch / 2, row[i]))
            i = k
    return out


def _majority(x, y, w, h, test):
    pts = [(x, y), (x - w / 3, y - h / 3), (x + w / 3, y - h / 3), (x - w / 3, y + h / 3), (x + w / 3, y + h / 3)]
    return sum(1 for px, py in pts if test(px, py)) >= 3


GLADES = [(POLAR, 34.0, 40.0), ((POLAR[0] - 10.0, POLAR[1] + 95.0), 45.0, 50.0)]   # open ground: centre, radius, fade


@functools.lru_cache(maxsize=1)
def _crossing():
    return R.crossing(CROSSING_X)


@functools.lru_cache(maxsize=1)
def _glades():
    return GLADES + [(p, 14.0, 22.0) for p in _crossing()[:2]] + [((HUT[0], HUT[1] + 6.0), 14.0, 16.0), (GATE, 8.0, 14.0)]


@functools.lru_cache(maxsize=65536)
def wood(x, y):
    """(mix, cover 0..1) of the woods between the town and the meadows (WOODS_Y): how much of a forest tile's trees
    stand here. No squares: old pine and spruce forest on the ridge between the town and the river, in broad masses
    with glades, frayed at its edges; alder, aspen and birch galleries on the river's terraces and islands; stone birch
    groves over grass on the sunny north side, thinning into the meadows. The game reads it as a mask
    (tools/forest_cover.py -> Content/World/forest_cover.fcv), world_map lays the tiles the mask thins."""
    if not (WOODS_Y[0] < y < WOODS_Y[1]):
        return ("", 1.0)
    i = R.info(x, y)
    north = y > 2040.0 if i is None else i["v"] > 0.0
    wave = L.fbm(x, y, 300.0, 3, 72)
    if north:
        mix = "stonebirch"
        c = _smooth((L.fbm(x, y, 210.0, 3, 73) + 0.04) / 0.26)
        c *= 1.0 - 0.75 * _smooth((y - 2520.0) / 180.0)                  # groves get rarer toward the meadows
    else:
        mix = "old"
        edge = 1505.0 + 45.0 * L.fbm(x, 3.0, 260.0, 2, 74)               # the wood's wavering foot over the town
        c = _smooth((wave + 0.2) / 0.26) * _smooth((y - edge) / 60.0)
    if i is not None:
        e = i["e"]
        if e <= 0.0:
            mix, c = "wet", 0.9 * _smooth((i["island"] - 0.3) / 0.4)
        elif e < 100.0:
            g = _smooth(e / 9.0) * _smooth((L.fbm(x, y, 110.0, 2, 71) + 0.22) / 0.3) * (1.0 - _smooth((e - 55.0) / 45.0))
            if g > c:
                mix, c = "wet", g
            else:
                c *= _smooth(e / 20.0)
    c *= 1.0 - _smooth((-1100.0 - x) / 100.0)                            # the Topi's dune: no trees by X -1200
    c *= _smooth((W.shore_x(y) - 70.0 - x) / 150.0)                      # open grass on the headlands over the sea
    for (gx, gy), r, soft in _glades():
        d = math.hypot(x - gx, y - gy)
        if d < r + soft:
            c *= 1.0 - _ramp(d, r, soft)
    return (mix, c)


def cover(x, y):
    """world_map.forest_cover's hook: 0..1, the share of a forest tile's trees that stand at (x, y)."""
    return wood(x, y)[1]


WOOD_DENSITY = {"old": 1.0, "wet": 0.5, "stonebirch": 0.2}


def forests(tiles):
    """Forest tiles of the North: (x, y, half x, half y, mix, density, min z, max z). tiles = world_map._tiles.
    From the town to the meadows the tiles only say which wood grows where (wood()): the cover mask shapes it. Above:
    the shrub belt: dwarf alder in the hollows and gullies, dwarf pine on the rises, thinning over the meadows; alder
    thickets on the Ridge's north slope; rhododendron round the snowfields. world_map carves out the snow, cinder,
    lava and steaming ground (tree_free)."""
    out = []
    x1 = W.cliff_top(2500.0) - 10.0

    def woods(x, y, w, h):
        votes = {}
        for u in (-0.4, 0.0, 0.4):
            for v in (-0.4, 0.0, 0.4):
                mix, c = wood(x + u * w, y + v * h)
                if c > 0.08:
                    votes[mix] = votes.get(mix, 0.0) + c
        return max(votes, key=votes.get) if votes else None

    for x, y, hx, hy, mix in _runs(W.X_MIN + 60.0, x1, WOODS_Y[0] + 20.0, WOODS_Y[1], (130.0, 125.0), woods):
        out.append((x, y, hx, hy, mix, WOOD_DENSITY[mix], -42.0, 200.0))

    def belt(x, y, w, h):
        if scoria(x, y) > 0.6 and scoria(x - w / 3, y) > 0.6 and scoria(x + w / 3, y) > 0.6:
            return None                                  # bare cinder: nothing to carve out of
        if _majority(x, y, w, h, _wet_site):
            return "alder"
        return "dwarfpine" if y < 3150.0 or scoria(x, y) > 0.05 else "shrub"

    for x, y, hx, hy, mix in _runs(W.X_MIN, x1, BELT[0], BELT[1], BELT_CELL, belt):
        t = _smooth((y - BELT[0] - BELT_CELL[1] / 2) / (1100.0 - BELT_CELL[1]))
        d = BELT_DENSITY[0] + (BELT_DENSITY[1] - BELT_DENSITY[0]) * t
        out.append((x, y, hx, hy, mix, d * (1.3 if mix == "alder" else 1.0), -30.0, 200.0))
    # stone birches climb the Old Cone: it has been quiet long enough
    for x, y, hx, hy in tiles(OLD_CONE[0] - 300.0, OLD_CONE[0] + 300.0, BELT[0], OLD_CONE[1] + 240.0):
        out.append((x, y, hx, hy, "stonebirch", 0.05, 20.0, 200.0))
    # rhododendron round each snowfield: a frame round its outline (the snow itself is carved out)
    for f in snowfields()[:14]:
        out.append((f["x"], f["y"], f["r"] + 30.0, f["r"] + 30.0, "rhodo", 0.4, 15.0, 215.0))
    return out


def _grass_site(x, y):
    """Ground where meadow plants may stand (not snow, cinder, lava, steam or a pad)."""
    return not tree_free(x, y) and scoria(x, y) < 0.15


def _apron_site(x, y):
    """The Cone's ash apron where the cinder thins out: pioneers take hold there."""
    return lava(x, y) == 0.0 and snow(x, y) == 0.0 and sulfur(x, y) < 0.2 and scoria(x, y) < 0.45


HERB_PATCH_SETTINGS = {
    # AProceduralGrass herb patches (import_world sets these on the patch by its species): plants per m2 (times the
    # row's density), the terrain mask they follow, age in years
    "Fireweed": {"herb_density": 0.7, "herb_habitat": "Meadow", "herb_age": 4.0},
    "CottonGrass": {"herb_density": 3.0, "herb_habitat": "Wet", "herb_age": 4.0},
    "MountainAvens": {"herb_density": 2.0, "herb_habitat": "Meadow", "herb_age": 8.0},
    "Lingonberry": {"herb_density": 1.5, "herb_habitat": "Meadow", "herb_age": 6.0},
}


def herb_patches(seed0=1400):
    """The meadow flowers as world_map.grass_patches rows (AProceduralGrass herb patches, drawn within 25..60 m of the
    camera): fireweed along the lava's edges, on the Cone's ash apron and round the camp; cotton grass in the wet
    hollows; mountain avens and lingonberry on the cinder's edge; reindeer lichen (a grass) near the Cone."""
    out = []
    seed = [seed0]

    def add(x, y, h, sp, density=1.0, z0=10.0, z1=110.0, need=_grass_site):
        if not need(x, y):
            return
        if any(abs(x - o[0]) < h and abs(y - o[1]) < h and o[4] == sp for o in out):
            return
        out.append((round(x, 1), round(y, 1), h, h, sp, seed[0], density, z0, z1))
        seed[0] += 1

    # fireweed: the lava flow's edges (it is the first to take burnt ground)
    n = 0
    for gy in range(3380, 3960, 45):
        for gx in range(int(LAVA_VENT[0]) + 40, int(W.cliff_top(gy)) - 60, -45):
            if n < 26 and lava(gx, gy) == 0.0 and max(lava(gx + 26, gy), lava(gx - 26, gy), lava(gx, gy + 26), lava(gx, gy - 26)) > 0.5:
                before = len(out)
                add(float(gx), float(gy), 20.0, "Fireweed")
                n += len(out) - before
    # the young cones' ash aprons: fireweed where the cinder gives out, mountain avens, lingonberry and lichen outside
    for c, count in ((CONE, 12), (HIGH_CONE, 12), (SMALL_CONE, 6)):
        for k in range(count):
            a = 2 * math.pi * k / count + 0.2
            r = 60.0
            while r < 520.0 and scoria(c[0] + r * math.cos(a), c[1] + r * math.sin(a)) > 0.3:
                r += 15.0
            if r >= 520.0:
                continue                                  # the next cone begins before this one's cinder ends
            add(c[0] + (r + 10.0) * math.cos(a), c[1] + (r + 10.0) * math.sin(a), 20.0, "Fireweed", z1=215.0, need=_apron_site)
            b = a + math.pi / count
            add(c[0] + (r + 4.0) * math.cos(b), c[1] + (r + 4.0) * math.sin(b), 15.0,
                "MountainAvens" if k % 2 else "Lingonberry", z1=215.0, need=_apron_site)
            if k % 3 == 0:
                add(c[0] + (r + 60.0) * math.cos(b), c[1] + (r + 60.0) * math.sin(b), 25.0, "ReindeerLichen", z1=215.0)
    # mountain avens on the gravel by the snowfields' lower edges
    for f in snowfields()[:10]:
        gx, gy = L.slope(f["x"], f["y"], 20.0)
        g = math.hypot(gx, gy) or 1.0
        add(f["x"] - gx / g * (f["r"] + 18.0), f["y"] - gy / g * (f["r"] + 18.0), 15.0, "MountainAvens", z1=215.0,
            need=lambda x, y: snow(x, y) == 0.0 and lava(x, y) == 0.0)
    # round the camp: fireweed on the trodden edge
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        add(CAMP[0] + 52.0 * math.cos(a), CAMP[1] + 52.0 * math.sin(a), 20.0, "Fireweed")
    # cotton grass: the wettest hollows of the meadows (the terrain's wetness mask decides inside the patch)
    sites = []
    for gy in range(2640, 4140, 60):
        for gx in range(int(W.X_MIN) + 180, int(W.cliff_top(gy) - 40.0), 60):
            if _grass_site(gx, gy) and sulfur(gx, gy) == 0.0:
                h = _hollow(gx, gy, 35.0)
                if h < -1.2 and math.hypot(*L.slope(gx, gy, 20.0)) < 0.06:
                    sites.append((h, gx, gy))
    for h, gx, gy in sorted(sites)[:16]:
        add(float(gx), float(gy), 25.0, "CottonGrass")
    return out


def grass_patches(seed0=1200):
    """Meadow grass where the hero walks and stops: round the hut, the camp, the spring and along the trail between
    them, as world_map.grass_patches rows; then the herb patches (herb_patches)."""
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
    return out + herb_patches()


# --- the trail ----------------------------------------------------------------------------------------------------

# the dirt track's anchors (tools/terrain.py routes between them over the land)
ANCHORS = {
    "polar": (POLAR[0] + 14.0, POLAR[1] + 22.0),
    "river_south": None,            # the crossing's ends (filled in below)
    "river_north": None,
    "north_gate": GATE,
    "north_meadow": (40.0, 2760.0),
    "north_camp": (CAMP[0] - 30.0, CAMP[1] - 8.0),
    "north_spring": (SPRING[0] + 14.0, SPRING[1] - 12.0),
    "north_fumaroles": (FUMAROLES_C[0], FUMAROLES_C[1] + 6.0),
    "north_crater": (CONE[0] + 16.0, CONE[1] - 6.0),
    "north_saddle": (-455.0, 3345.0),
    "north_high_rim": HIGH_RIM,
    "north_lava": (-190.0, 3610.0),
}
TRACKS = [("town", "polar"), ("polar", "river_south"), ("river_north", "north_gate"), ("north_gate", "north_meadow"), ("north_meadow", "north_camp"),
          ("north_camp", "north_spring"), ("north_spring", "north_fumaroles"), ("north_fumaroles", "north_crater"),
          ("north_spring", "north_saddle"), ("north_saddle", "north_high_rim"), ("north_spring", "north_lava")]


ANCHORS["river_south"], ANCHORS["river_north"] = _crossing()[0], _crossing()[1]


def fixed_tracks():
    """Tracks that are not routed over the land: the straight way across the river's plain, bridge to bridge."""
    a, b, _ = _crossing()
    n = int(math.hypot(b[0] - a[0], b[1] - a[1]) / 6.0)
    return [[(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n) for k in range(n + 1)]]


BRIDGES = [(11.0, 0), (16.0, 1), (26.0, 2)]     # log footbridges: the longest water each variant spans, m


def things():
    """Loose things the importer stands on the map (north.json things): {id, kind, x, y, yaw, variant, z (the mesh
    origin's map height, when it must not follow the ground)}. The log footbridges over the river's channels: the
    origin is the deck's middle at walking height."""
    out = []
    for n, (x, y, yaw, span, z) in enumerate(b for b in _crossing()[2] if b[3] >= 4.0):    # narrower: a stride
        variant = next((v for reach, v in BRIDGES if span <= reach), BRIDGES[-1][1])
        out.append({"id": "river_bridge_%d" % (n + 1), "kind": "log_bridge", "x": round(x, 2), "y": round(y, 2),
                    "yaw": round(yaw, 1), "variant": variant, "z": round(z, 2), "span": round(span, 1)})
    return out


def river():
    """The river for the game (north.json river): the water sheet's ribbon [x, y, z, half width], the channels' middle
    lines, the mouth."""
    m = R.mouth()
    return {"water": R.water(), "threads": R.threads(), "mouth": [round(m[0], 1), round(m[1], 1)]}


def spring_seats():
    """Where one sits in the hot spring: on the pool's underwater ledge, back to the rim, facing the middle."""
    out = []
    for a in (200.0, 290.0, 20.0, 110.0):
        r = math.radians(a)
        out.append({"x": round(SPRING[0] + 1.75 * math.cos(r), 2), "y": round(SPRING[1] + 1.75 * math.sin(r), 2),
                    "yaw": round((a + 180.0) % 360.0, 1)})
    return out


def trail_points(step):
    """Points every step metres along the straight lines between the trail's anchors (a rough line for planting;
    the real track is terrain.tracks())."""
    names = ["north_gate", "north_meadow", "north_camp", "north_spring", "north_fumaroles", "north_crater"]
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
        dx, dy = dx * 0.66, dy * 0.66                                        # close round the fire, not strewn about
        out.append({"id": "camp_tent_%d" % (i + 1), "kind": "tent_" + kind, "x": CAMP[0] + dx, "y": CAMP[1] + dy,
                    "yaw": round(yaw, 1), "variant": variant})
    for i, a in enumerate((35.0, 150.0, 265.0)):
        out.append({"id": "camp_log_%d" % (i + 1), "kind": "log_bench", "x": CAMP[0] + 2.6 * math.cos(math.radians(a)),
                    "y": CAMP[1] + 2.6 * math.sin(math.radians(a)), "yaw": a + 180.0, "variant": i % 2})
    out += [
        {"id": "camp_table", "kind": "camp_table", "x": CAMP[0] + 6.5, "y": CAMP[1] + 4.5, "yaw": 30.0, "variant": 0},
        {"id": "camp_tarp", "kind": "tarp_shelter", "x": CAMP[0] + 6.5, "y": CAMP[1] + 4.5, "yaw": 30.0, "variant": 0},
        {"id": "camp_woodpile", "kind": "woodpile", "x": CAMP[0] - 4.5, "y": CAMP[1] - 4.0, "yaw": 70.0, "variant": 0},
        {"id": "camp_foodpole", "kind": "food_pole", "x": CAMP[0] - 14.0, "y": CAMP[1] - 8.0, "yaw": 0.0, "variant": 0},
        {"id": "camp_line", "kind": "clothes_line", "x": CAMP[0] + 13.5, "y": CAMP[1] + 2.0, "yaw": 100.0, "variant": 0},
        {"id": "camp_board", "kind": "board_camp", "x": CAMP[0] - 19.0, "y": CAMP[1] - 6.0, "yaw": 190.0, "variant": 0},
        {"id": "camp_toilet", "kind": "outhouse", "x": CAMP[0] + 21.0, "y": CAMP[1] + 17.0, "yaw": 215.0, "variant": 0},
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
        {"id": "sign_hot_s", "kind": "sign_hot_ground", "x": SPRING[0] - 26.0, "y": SPRING[1] + 9.0, "yaw": 0.0, "text": "north.sign.hot_ground"},
        {"id": "sign_hot_n", "kind": "sign_hot_ground", "x": CONE[0] + 62.0, "y": CONE[1] - 30.0, "yaw": 0.0, "text": "north.sign.hot_ground"},
        {"id": "sign_spring", "kind": "sign_post", "x": SPRING[0] + 6.5, "y": SPRING[1] - 5.0, "yaw": 200.0, "text": "north.sign.spring"},
        {"id": "sign_cone", "kind": "sign_post", "x": ANCHORS["north_saddle"][0] + 4.0, "y": ANCHORS["north_saddle"][1] + 3.0, "yaw": 300.0, "text": "north.sign.cone"},
        {"id": "sign_camp", "kind": "sign_post", "x": CAMP[0] - 24.0, "y": CAMP[1] - 10.0, "yaw": 180.0, "text": "north.sign.camp"},
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
    sf = snowfields()
    snow_at = (sf[0]["x"], sf[0]["y"] - sf[0]["r"] - 6.0) if sf else HIGH_CONE
    return [
        {"id": "north_river", "name": "place.north.river", "x": round(_crossing()[0][0], 1), "y": round(_crossing()[0][1] - 4.0, 1), "yaw": 90.0},
        {"id": "north_mouth", "name": "place.north.mouth", "x": round(R.mouth()[0] - 70.0, 1), "y": round(R.mouth()[1] + 40.0, 1), "yaw": 180.0},
        {"id": "north_gate", "name": "place.north.gate", "x": GATE[0] + 3.0, "y": GATE[1] - 8.0, "yaw": 90.0},
        {"id": "north_hut", "name": "place.north.hut", "x": HUT[0] - 9.0, "y": HUT[1], "yaw": 0.0},
        {"id": "north_meadows", "name": "place.north.meadows", "x": 40.0, "y": 2760.0, "yaw": 100.0},
        {"id": "north_old_cone", "name": "place.north.old_cone", "x": OLD_CONE_TOP[0], "y": OLD_CONE_TOP[1], "yaw": 90.0},
        {"id": "north_camp", "name": "place.north.camp", "x": CAMP[0] - 6.0, "y": CAMP[1] - 5.0, "yaw": 45.0},
        {"id": "north_spring", "name": "place.north.spring", "x": SPRING[0] + 6.0, "y": SPRING[1] - 6.0, "yaw": 180.0},
        {"id": "north_fumaroles", "name": "place.north.fumaroles", "x": FUMAROLES_C[0] + 70.0, "y": FUMAROLES_C[1] + 4.0, "yaw": 180.0},
        {"id": "north_crater", "name": "place.north.crater", "x": CONE[0] + 22.0, "y": CONE[1] - 8.0, "yaw": 160.0},
        {"id": "north_lava", "name": "place.north.lava", "x": -190.0, "y": 3606.0, "yaw": 20.0},
        {"id": "north_snowfield", "name": "place.north.snowfield", "x": round(snow_at[0], 1), "y": round(snow_at[1], 1), "yaw": 90.0},
        {"id": "north_saddle", "name": "place.north.saddle", "x": -455.0, "y": 3345.0, "yaw": 0.0},
        {"id": "north_rim", "name": "place.north.rim", "x": HIGH_RIM[0], "y": HIGH_RIM[1], "yaw": 250.0},
    ]


def build():
    return {
        "frame": "map metres (docs/world_map.md): x west, y north; heights from the built ground (terrain.ground_z)",
        "places": places(),
        "hut": {"x": HUT[0], "y": HUT[1], "yaw": HUT_YAW},
        "camp": camp(),
        "signs": signs(),
        "fumaroles": fumaroles(),
        "spring": {"x": SPRING[0], "y": SPRING[1], "r": SPRING_R, "seats": spring_seats(),
                   "surface_z": round(_pad_z(*SPRING) - 0.15, 2)},
        "river": river(),
        "things": things(),
        "cones": [{"id": "cone", "x": CONE[0], "y": CONE[1]}, {"id": "high", "x": HIGH_CONE[0], "y": HIGH_CONE[1]},
                  {"id": "old", "x": OLD_CONE[0], "y": OLD_CONE[1]}, {"id": "small", "x": SMALL_CONE[0], "y": SMALL_CONE[1]}],
        "snowfields": snowfields(),
        "lava_vent": list(LAVA_VENT),
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


# --- people (the ranger and the campers: tools/people_map.py build_north -> people_places.json, docs/north.md "People")

HUT_FLOOR = 0.35                # the hut's floor over the ground at its pad (a log hut on a sill, a step up)


def people_spots():
    """Named spots for the North's people, map metres (x, y) with an Unreal yaw (the way a seated person faces, a
    standing one looks; a bed: head to foot). A proposal of where things stand until blender/export/north/manifest.json
    has the hut's furniture and the benches' seats (people_map.py prefers those when it finds them). Pure data.

    hut       door (the porch point outside it), inside (the way in), floor (m over the ground), slots: the bunk, the
              chair at the table (the radio), the stool by the stove, the stove spot
    porch     the bench against the hut's front wall (two seats)
    gate      four standing spots at the barrier, looking down the track and into the reserve
    weather   the weather post (a screen and a rain gauge) south of the hut
    fire      the log benches round the fire ring: two seats a log, facing the fire
    table     the camp table's benches: two seats a side, facing the table
    spring    the bench on the hot spring's deck, facing the pool
    patrol    the ranger's round in order: gate -> camp -> spring -> camp -> gate, every ~90 m along the trail's anchors
    walk      a walk about the meadows (the camp's hollow, the meadow's brow, the spring, the valley's mouth, the snow)
    field     where the geology students work: the fresh lava, the snowfields' feet, the valley's mouth
    All of it is offsets from the named places (HUT, GATE, CAMP, SPRING, FUMAROLES_C, CONE, ANCHORS), so it follows
    the land when they move.
    view      where the photographer waits for the light on the Cone
    tents     camp tent id -> {door (outside it), bed (inside), mat (a seat inside), yaw, kind}
    """
    def turn(c, yaw, u, v):
        a = math.radians(yaw)
        return (c[0] + u * math.cos(a) - v * math.sin(a), c[1] + u * math.sin(a) + v * math.cos(a))

    def hut(u, v):
        return turn(HUT, HUT_YAW, u, v)

    def r(p):
        return [round(p[0], 2), round(p[1], 2)]

    def look_at(p, q):
        return round(math.degrees(math.atan2(q[1] - p[1], q[0] - p[0])), 1)

    # the hut, local u along its front (west), v to its left: the door in the front wall, the bunk along the back-left
    # wall, the table under the left window with the radio on it, the stove in the back-right corner
    hy = HUT_YAW
    hut_slots = [
        ("bed", "bunk", hut(-1.75, 0.65), hy - 90.0, 0.45, hut(-0.95, 0.65)),
        ("kitchen_seat", "table", hut(0.25, 0.75), hy + 90.0, 0.45, hut(0.25, 0.3)),
        ("armchair", "stool", hut(-0.55, -0.85), hy + 200.0, 0.42, hut(-0.1, -0.6)),
        ("stove", "stove", hut(-1.0, -0.55), hy - 150.0, 0.0, hut(-1.0, -0.55)),
    ]
    out = {
        "hut": {"door": r(hut(3.3, -0.6)), "inside": [r(hut(2.3, -0.6)), r(hut(1.4, -0.6)), r(hut(0.5, -0.2))],
                "floor": HUT_FLOOR,
                "slots": [{"kind": k, "tag": t, "x": r(p)[0], "y": r(p)[1], "yaw": round(y, 1), "height": h, "approach": r(a)}
                          for k, t, p, y, h, a in hut_slots]},
        "porch": [{"x": r(hut(2.95, v))[0], "y": r(hut(2.95, v))[1], "yaw": hy} for v in (0.55, 1.15)],
        "gate": [
            {"x": GATE[0] + 2.5, "y": GATE[1] - 2.0, "yaw": -90.0},      # on the track, looking down it to the town
            {"x": GATE[0] - 3.0, "y": GATE[1] - 1.0, "yaw": -60.0},
            {"x": GATE[0] + 5.5, "y": GATE[1] + 0.5, "yaw": -120.0},     # by the reserve's board
            {"x": GATE[0] + 1.0, "y": GATE[1] + 6.0, "yaw": 90.0},       # inside, looking up the track into the reserve
        ],
        "weather": [{"x": r(hut(-4.0, -10.0))[0], "y": r(hut(-4.0, -10.0))[1], "yaw": hy - 90.0}],
    }
    things = camp()
    fire = []
    for o in things:
        if o["kind"] == "log_bench":
            a = math.radians(o["yaw"] - 180.0)          # from the fire out to the log
            for s in (-0.45, 0.45):
                fire.append({"x": round(o["x"] - s * math.sin(a), 2), "y": round(o["y"] + s * math.cos(a), 2),
                             "yaw": round(o["yaw"], 1), "log": o["id"]})
    out["fire"] = fire
    t = next(o for o in things if o["kind"] == "camp_table")
    table = []
    for side in (-1.0, 1.0):
        for along in (-0.5, 0.5):
            p = turn((t["x"], t["y"]), t["yaw"], side * 0.75, along)
            table.append({"x": round(p[0], 2), "y": round(p[1], 2), "yaw": round(t["yaw"] + (180.0 if side > 0 else 0.0), 1)})
    out["table"] = table
    out["spring"] = [{"x": round(SPRING[0] + k, 2), "y": round(SPRING[1] - 4.3, 2), "yaw": 90.0} for k in (-0.55, 0.0, 0.55)]
    names = [n for n in ("north_gate", "north_meadow", "north_camp", "north_spring") if n in ANCHORS]
    pts = []
    for p, q in zip([ANCHORS[n] for n in names], [ANCHORS[n] for n in names[1:]]):
        n = max(1, int(round(math.hypot(q[0] - p[0], q[1] - p[1]) / 90.0)))
        pts += [(p[0] + (q[0] - p[0]) * k / n, p[1] + (q[1] - p[1]) * k / n) for k in range(n)]
    pts.append(ANCHORS["north_spring"])
    loop = pts + pts[-2::-1]
    out["patrol"] = [{"x": round(p[0], 2), "y": round(p[1], 2),
                      "yaw": look_at(p, loop[i + 1] if i + 1 < len(loop) else loop[i - 1])} for i, p in enumerate(loop)]
    def lerp(p, q, t):
        return (p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t)

    def spot(p, yaw=None, at=None):
        return {"x": round(p[0], 2), "y": round(p[1], 2), "yaw": look_at(p, at) if at else round(yaw, 1)}

    # the snowfields' south feet, largest first: the DEM land's snowfields() or the older hand-placed SNOWFIELDS
    feet = []
    try:
        if callable(globals().get("snowfields")):
            feet = [(f["x"], f["y"] - f["r"] - 8.0) for f in snowfields()[:3]]
        else:
            feet = [(f[0], f[1] - f[3] - 10.0) for f in sorted(globals().get("SNOWFIELDS", []), key=lambda f: -f[2] * f[3])[:3]]
    except Exception:       # the land's data (LFS) is missing: the students keep to the lava and the valley
        feet = []
    feet = [f for f in feet if math.hypot(f[0] - CAMP[0], f[1] - CAMP[1]) < 900.0]
    mouth = (FUMAROLES_C[0] + FUMAROLES_R[0] + 25.0, FUMAROLES_C[1] - FUMAROLES_R[1] - 20.0)    # beside the hot ground
    camp_a, spring_a = ANCHORS["north_camp"], ANCHORS["north_spring"]
    meadow = ANCHORS.get("north_meadow", lerp(GATE, CAMP, 0.5))
    out["walk"] = [
        spot((CAMP[0] - 40.0, CAMP[1] + 35.0), 120.0),
        spot((CAMP[0] + 55.0, CAMP[1] - 30.0), 0.0),                   # the meadow's brow over the sea
        spot((meadow[0] - 15.0, meadow[1]), 90.0),
        spot(lerp(camp_a, spring_a, 0.5), at=CONE),
        spot((SPRING[0] + 30.0, SPRING[1] - 25.0), 135.0),
        spot(mouth, at=FUMAROLES_C),
    ] + [spot(f, 90.0) for f in feet[:2]] + [spot(ANCHORS[n], at=CONE) for n in ("north_saddle",) if n in ANCHORS]
    # the geology students: the fresh lava (its anchor on the DEM land, else the hand-drawn flow's south edge), the
    # snowfields' feet, the valley's mouth
    if "north_lava" in ANCHORS:
        c = ANCHORS["north_lava"]
        lava_spots = [(c[0], c[1] - 12.0), (c[0] + 45.0, c[1] - 20.0), (c[0] - 40.0, c[1] - 8.0)]
    else:
        lava_spots = [(q[0], q[1] - 55.0) for q in globals().get("LAVA", [])[3:8:2]]
    out["field"] = ([spot(q, 90.0) for q in lava_spots] + [spot(f, 90.0) for f in feet]
                    + [spot((mouth[0] + 20.0, mouth[1] - 6.0), at=FUMAROLES_C)])
    views = [(CAMP[0] - 15.0, CAMP[1] + 30.0), (SPRING[0] + 25.0, SPRING[1] + 35.0), lerp(camp_a, spring_a, 0.3)]
    out["view"] = [spot(q, at=CONE) for q in views]
    tents = {}
    for o in things:
        if o["kind"].startswith("tent_"):
            c = (o["x"], o["y"])
            tents[o["id"]] = {"door": r(turn(c, o["yaw"], 1.6, 0.0)), "bed": r(turn(c, o["yaw"], -0.3, 0.0)),
                              "mat": r(turn(c, o["yaw"], 0.5, 0.0)), "yaw": o["yaw"], "kind": o["kind"]}
    out["tents"] = tents
    return out
