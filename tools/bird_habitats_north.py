"""The North's bird places (everything north of Y 1450, docs/north.md) for tools/bird_habitats.py: where the wild birds
of the bird life (docs/bird_life.md) can be on the new land, taken from the land's own functions (tools/north.py,
tools/north_river.py, tools/world_map.py), never from hand-drawn rectangles. Read-only use of those modules: nothing
here changes them.

  python3 tools/bird_habitats_north.py        a summary of the North's sites by place and kind (no files written)

What goes where (the habitat kinds are LyudBirds::EHabitat's, BirdSim.h; a species likes a kind or not, BirdEcology.cpp):

  woods          the cover mask (world_map.forest_cover) thins the forest tiles' tree guesses: old pine and spruce on
                 the ridge (Conifer, some Broadleaf, ForestFloor under it), the willow and alder galleries and the bars'
                 willow thickets, the stone birch groves (Broadleaf); in the game the real crowns replace the guesses
  river          the dry gravel bars of the braided plain (Beach: walk and peck at the water's edge), denser toward the
                 mouth; the mouth's bars and the bay's water (Beach, Sea)
  coast          the generic sea, beach and cliff rows of bird_habitats.py, thinned north of the town (a long empty
                 coast), the bay at the mouth kept full; no beach on the lava cape's clinker
  sea stacks     off the points of the capes: the stacks' tops and ledges (Cliff) when north.things() lists the
                 `sea_stack`s, always a raft of water round them (Sea)
  meadows        a coarse grid over the open land (Meadow), the tall-herb patches (meadowsweet, cow parsnip), the
                 camp's and the hut's trampled ground
  perches        the footbridges' handrails, the trail's marker posts, the gauge post, the camp's food pole and
                 clothes line (Wire); the driftwood on the bars, the big boulders (Cliff); the hut's roof ridge (Wire: a row
                 of birds along it, without the town's roof birds)
  nothing        on hot ground (the Fumarole Valley, round every vent and the spring), few on bare cinder, lava and
                 snow
"""
import functools
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402
import terrain as T  # noqa: E402
import north as N  # noqa: E402
import north_river as R  # noqa: E402

Y0 = N.HILLS_Y                      # the North starts here
BAY_Y = (1780.0, 2000.0)            # the river mouth's bay (no stacks; the beach and the sea kept full there)

# how much of the generic coast rows bird_habitats.py lays north of the town it keeps (the town's coast keeps them all)
SEA_KEEP_OFFSETS = (35.0, 190.0)    # of (35, 100, 190, 320) m off the water line
SEA_ROW_EVERY = 2                   # every second 70 m row north of the town (the mouth's bay keeps all of its own)
BEACH_ROW_EVERY = 3                 # every third 30 m beach site outside the bay
CLIFF_ROW_EVERY = 3                 # every third 25 m piece of the coast's cliff face
MEADOW_LATTICE = 7                  # one 45 m meadow cell in MEADOW_LATTICE x MEADOW_LATTICE (a 315 m grid)
MEADOW_SITE = (35.0, 60)            # its radius m and capacity: few big meadows, not a carpet of small ones (every
                                    # site is a candidate home: many would pull the town's meadow birds out here)

# the river's bars: a site every BAR_STEP metres down the plain and across it, kept with a share growing toward the
# mouth (the bars are widest and barest there; the upper river is narrow and shaded)
BAR_STEP = (42.0, 34.0)
BAR_KEEP = (0.12, 0.35)             # share kept at the source .. at the mouth
CANOPY_NORTH = {"gallery": 12.0, "riverbar": 2.5}   # crown heights of the river's woods (chosenia 11 m, bar willow 2.4 m)


def _smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def in_north(y):
    return y > Y0


@functools.lru_cache(maxsize=1)
def _vents():
    return [(f["x"], f["y"], 4.0 + 3.5 * f["size"] + 8.0) for f in N.fumaroles()]


def hot(x, y):
    """True on hot ground: the Fumarole Valley's altered floor, round every vent, mud pot and the hot spring. Nothing
    sits or feeds there."""
    if not (3000.0 < y < 3800.0 and -760.0 < x < -200.0):
        return False
    if N.sulfur(x, y) > 0.05:
        return True
    return any(math.hypot(x - vx, y - vy) < r for vx, vy, r in _vents())


def bare(x, y):
    """0..1: bare volcanic ground (cinder, the lava's clinker, firn): next to nothing for a bird there."""
    if y < 2700.0:
        return 0.0
    return max(N.scoria(x, y), N.lava(x, y), N.snow(x, y))


def open_ground_ok(x, y, rnd):
    """Whether an open-ground (meadow, beach) site may stand at (x, y) in the North: never on hot ground, one in six
    on bare cinder, lava or snow, none on the river's gravel plain (that is the bars' own, below)."""
    if hot(x, y):
        return False
    if R.near(x, y):
        i = R.info(x, y)
        if i is not None and i["e"] <= 2.0:
            return False
    if bare(x, y) > 0.5:
        return rnd.random() < 1.0 / 6.0
    return True


def woods_open(x, y):
    """True where the woods leave the ground open enough for meadow birds: no dense old forest or gallery."""
    if not (N.WOODS_Y[0] < y < N.WOODS_Y[1]):
        return True
    mix, c = N.wood(x, y)
    return not (mix in ("old", "gallery") and c > 0.5)


def tree_kind(mix, rnd):
    return {"old": "Conifer" if rnd.random() < 0.75 else "Broadleaf", "gallery": "Broadleaf", "riverbar": "Broadleaf",
            "stonebirch": "Broadleaf", "dwarfpine": "Conifer", "alder": "Broadleaf", "shrub": "Broadleaf",
            "rhodo": "Broadleaf"}[mix]


# share of a North mix's tree guesses left out after the cover mask: stone birch is a park, its crowns wide apart over the
# tall herbs; most of the belt's dwarf pine and shrubs are too low to sit in (the game only takes crowns of 2 m and more).
# Beyond the woods (the shrub belt, the Old Cone's birches, Y 2700..4190) far fewer: that land is wide and far from
# everything, and the bird life gives a remote site a home whenever one of a group's ten draws hits it (BirdSim.cpp
# Populate: the farthest of ten), so a dense belt would take a third of the map's tits, thrushes and starlings
NORTH_SPARSE = {"stonebirch": 0.5, "dwarfpine": 0.85, "alder": 0.6, "shrub": 0.95, "rhodo": 1.0}
BELT_SPARSE = {"stonebirch": 0.85, "dwarfpine": 0.95, "alder": 0.88, "shrub": 0.98, "rhodo": 1.0}


def tree_keep(x, y, mix, rnd):
    """Whether a tree guess at (x, y) of a North forest tile stands: the cover mask (world_map.forest_cover: the game's
    AProceduralForest drops the trees it leaves out) and NORTH_SPARSE between the town and the meadows, BELT_SPARSE
    beyond."""
    if hot(x, y):
        return False
    if N.WOODS_Y[0] < y < N.WOODS_Y[1]:
        if rnd.random() >= min(1.0, 1.3 * W.forest_cover(x, y)):
            return False
        return rnd.random() >= NORTH_SPARSE.get(mix, 0.0)
    return rnd.random() >= BELT_SPARSE.get(mix, 0.0)


def canopy_cover(x, y):
    """0..1 for the obstacle grid: how much of a North tile's canopy stands over (x, y)."""
    if N.WOODS_Y[0] < y < N.WOODS_Y[1]:
        return W.forest_cover(x, y)
    return 1.0


def capes():
    """The points of the coast north of Y 1500 outside the mouth's bay: [(y, shore x, prominence m)], most prominent
    first (local maxima of world_map.shore_x within +-60 m, standing 20 m or more out over the coast +-120 m round;
    the rule the sea stacks are placed by)."""
    ys = [1500.0 + 2.0 * i for i in range(int((W.Y_MAX - 1500.0) / 2.0))]
    xs = [W.shore_x(y) for y in ys]
    out = []
    for i in range(30, len(ys) - 30):
        if xs[i] < max(xs[i - 30:i + 31]) or BAY_Y[0] <= ys[i] <= BAY_Y[1]:
            continue
        prom = xs[i] - min(xs[max(0, i - 60):i + 61])
        if prom >= 20.0:
            out.append((ys[i], xs[i], prom))
    return sorted(out, key=lambda c: -c[2])


# the sea stacks (blender/north/props_sea.py): where birds stand on each variant, mesh space (x along the stack's
# local X, z over the sea at mid tide), and how many
STACK_PERCHES = {
    0: [(0.0, 13.6, 6), (1.8, 2.5, 6)],                     # the pillar's ragged top, its foot's ledges
    1: [(-2.4, 8.6, 4), (2.7, 4.7, 3), (0.2, 2.5, 4)],       # the two teeth, the shared ledge
    2: [(-2.5, 5.8, 5), (2.5, 5.8, 5)],                      # the arch's top (cormorants dry their wings here)
    3: [(-4.2, 2.8, 3), (0.0, 1.8, 2), (2.0, 2.4, 3), (5.0, 1.4, 2)],   # the skerries
}


def sea_stacks():
    """The sea stacks north.things() stands on the map, [(x, y, yaw degrees, variant)]; empty while the land has
    none (then the capes' rafts stand in for them)."""
    return [(t["x"], t["y"], t.get("yaw", 0.0), t.get("variant", 0)) for t in N.things() if t["kind"] == "sea_stack"]


def add_coast_extras(add):
    """Stacks and capes, the river mouth's bay."""
    stacks = sea_stacks()
    for x, y, yaw, v in stacks:
        a = math.radians(yaw)
        ux, uy = -math.sin(a), math.cos(a)          # the mesh's local X on the map (front -Y heads at yaw)
        for lx, z, cap in STACK_PERCHES.get(v, STACK_PERCHES[0]):
            add("Cliff", (x + lx * ux, y + lx * uy, W.SEA_Z + z), radius=0.0, cap=cap)
    # off every point a raft on the water where the stacks stand (gulls and cormorants rest and fish there); while
    # the land has no stacks yet, the raft alone, where the stacks' rule puts the tall ones (40..65 m out)
    anchors = [(y, x) for x, y, _, _ in stacks] or [(y, sx + 50.0) for y, sx, _ in capes()]
    for y, x in anchors:
        add("Sea", (x + 18.0, y, W.SEA_Z), radius=16.0, cap=24)
    # the mouth: where the river's water fans out into the bay the gulls gather; the spit's bars
    mx, my = R.mouth()
    for dx, dy, r, cap in ((45.0, 0.0, 18.0, 30), (95.0, -25.0, 22.0, 30), (90.0, 30.0, 22.0, 24)):
        add("Sea", (mx + dx, my + dy, W.SEA_Z), radius=r, cap=cap)
    for dy in (-34.0, -14.0, 14.0, 34.0):
        x = W.shore_x(my + dy) - 4.0
        add("Beach", (x, my + dy, T.ground_z(x, my + dy) + 0.02), radius=7.0, cap=20)
    return len(anchors)


def add_river_bars(add, rnd):
    """The braided plain's dry gravel bars (Beach: gulls, crows, ravens, wagtails, herons walk the water's edge):
    across the plain every BAR_STEP, kept more toward the mouth; not on the islands (willow) or in the channels."""
    s_src, s_mouth, _ = R._marks()
    n = 0
    s = s_src + 30.0
    while s < s_mouth - 10.0:
        x0, y0, nx, ny = R.at(s)
        h = R.half(s)
        keep = BAR_KEEP[0] + (BAR_KEEP[1] - BAR_KEEP[0]) * R.reach(s)
        v = -h + BAR_STEP[1] * 0.5
        while v < h:
            jx, jy = rnd.uniform(-8.0, 8.0), rnd.uniform(-6.0, 6.0)
            x, y = x0 + nx * v + jx, y0 + ny * v + jy
            v += BAR_STEP[1]
            if rnd.random() > keep:
                continue
            i = R.info(round(x, 1), round(y, 1))
            if i is None or i["e"] > -4.0 or i["wet"] > 0.3 or i["island"] > 0.4:
                continue
            if any(abs(x - b["x"]) < 6.0 and abs(y - b["y"]) < 18.0 for b in bridges()):
                continue                                   # the way across stays the people's
            add("Beach", (x, y, T.ground_z(x, y) + 0.02), radius=8.0, cap=14)
            n += 1
        s += BAR_STEP[0]
    return n


@functools.lru_cache(maxsize=1)
def _things():
    return N.things()


def bridges():
    return [t for t in _things() if t["kind"] == "log_bridge"]


BRIDGE_LEN = {0: 16.0, 1: 21.0, 2: 31.0}      # the deck's length by variant; the last 2.5 m each end are ramps


def add_perches(add):
    """Perches on the things standing in the North: footbridge handrails, driftwood, boulders, posts; the hut and the
    camp. Heights from the meshes (blender/north/*.py, manifests)."""
    count = {}

    def put(kind, p, q=None, radius=0.0, cap=4, what=""):
        add(kind, p, q, radius, cap)
        count[what] = count.get(what, 0) + 1

    for t in _things():
        k, x, y, yaw, v = t["kind"], t["x"], t["y"], t.get("yaw", 0.0), t.get("variant", 0)
        if k == "log_bridge":
            # the pole handrail on the mesh's +Y side, 0.9 m over the deck, over the flat part; yaw 0: the deck along
            # map Y, the rail on its east (-X) side
            a = BRIDGE_LEN.get(v, 16.0) / 2.0 - 2.5
            z = t["z"] + 0.9
            n = max(1, int(math.ceil(2 * a / 4.0)))
            for j in range(n):
                y0, y1 = y - a + 2 * a * j / n, y - a + 2 * a * (j + 1) / n
                put("Wire", (x - 0.62, y0, z), (x - 0.62, y1, z), 0.0, max(2, int((y1 - y0) / 0.4)), "footbridge rails")
            continue
        g = None
        if k in ("driftwood", "river_boulder", "trail_marker", "gauge_post"):
            g = T.ground_z(x, y)
        a = math.radians(yaw)
        ux, uy = -math.sin(a), math.cos(a)           # the mesh's local X on the map (front -Y heads at yaw)
        if k == "driftwood":
            if v == 0:          # a trunk 6.2 m along local X (its root plate at -X): along its top
                put("Cliff", (x - 2.4 * ux, y - 2.4 * uy, g + 0.45), (x + 2.0 * ux, y + 2.0 * uy, g + 0.3), 0.0, 6, "driftwood")
            elif v == 1:        # a tangle of three logs
                put("Cliff", (x - 1.5 * ux, y - 1.5 * uy, g + 0.8), (x + 1.5 * ux, y + 1.5 * uy, g + 0.8), 0.0, 4, "driftwood")
            elif int(t["id"].rsplit("_", 1)[1]) % 2:   # every second 1 m stump with its roots
                put("Cliff", (x, y, g + 1.0), radius=0.0, cap=1, what="driftwood")
        elif k == "river_boulder" and v == 0:
            put("Cliff", (x, y, g + 1.0), radius=0.0, cap=2, what="boulders")
        elif k == "trail_marker" and int(t["id"].rsplit("_", 1)[1]) % 2:    # every second post: a wagtail's, a buzzard's lookout
            put("Wire", (x, y, g + 1.22), radius=0.0, cap=1, what="trail posts")
        elif k == "gauge_post":
            put("Wire", (x, y, g + 2.2), radius=0.0, cap=1, what="gauge post")

    # the ranger's hut: the roof's ridge (hut-local Y, -3.8..3.8, 4.45 m over the ground; the hut's front faces map
    # -X, so the ridge runs along map X)
    hx, hy = N.HUT
    gz = T.ground_z(hx, hy)
    # a Wire, not a Roof: birds sit along it in a row the same, but a Roof would bring the town's pigeons out here and
    # turn the trees round the hut into garden trees in the game (BirdLife.cpp AddRealTrees)
    put("Wire", (hx - 3.8, hy, gz + 4.45), (hx + 3.8, hy, gz + 4.45), 0.0, 12, "hut roof ridge")
    # the camp: the food pole's crossbar (4.78 m), the clothes line (1.9 m)
    for c in N.camp():
        x, y = c["x"], c["y"]
        g = T.ground_z(x, y)
        a = math.radians(c["yaw"] - 90.0)
        if c["kind"] == "food_pole":
            put("Wire", (x - 0.9 * math.cos(a), y - 0.9 * math.sin(a), g + 4.78), (x + 0.9 * math.cos(a), y + 0.9 * math.sin(a), g + 4.78), 0.0, 4, "food pole")
        elif c["kind"] == "clothes_line":
            put("Wire", (x - 2.5 * math.cos(a), y - 2.5 * math.sin(a), g + 1.9), (x + 2.5 * math.cos(a), y + 2.5 * math.sin(a), g + 1.9), 0.0, 8, "clothes line")
    return count


def add_people_ground(add):
    """The trampled ground round the camp and the hut, where crows, ravens and wagtails pick up what people drop
    (Meadow patches: not Street, which would bring the town's pigeons and sparrows out here)."""
    n = 0
    cx, cy = N.CAMP
    for dx, dy in ((-10.0, 9.0), (9.0, -11.0), (16.0, 8.0), (-18.0, -12.0)):
        x, y = cx + dx, cy + dy
        add("Meadow", (x, y, T.ground_z(x, y) + 0.02), radius=6.0, cap=10)
        n += 1
    hx, hy = N.HUT
    for dx, dy in ((-9.0, 4.0), (-7.0, -9.0)):
        x, y = hx + dx, hy + dy
        add("Meadow", (x, y, T.ground_z(x, y) + 0.02), radius=5.0, cap=8)
        n += 1
    return n


def add_tall_herbs(add, rnd):
    """The tall-herb meadows (meadowsweet, cow parsnip) on the river's terraces and in the birch groves: a patch site
    over each herb patch."""
    n = 0
    for p in W.grass_patches():
        if p[1] > Y0 and p[4] in ("KamchatkaMeadowsweet", "CowParsnip"):
            x, y = p[0] + rnd.uniform(-4.0, 4.0), p[1] + rnd.uniform(-4.0, 4.0)
            if hot(x, y):
                continue
            add("Meadow", (x, y, T.ground_z(x, y) + 0.02), radius=min(p[2], p[3]) * 0.6, cap=16)
            n += 1
    return n


def hut_box():
    """The hut for the obstacle grid: (x, y, yaw, half w, half d, top z), map metres."""
    hx, hy = N.HUT
    return (hx, hy, 0.0, 4.4, 3.7, T.ground_z(hx, hy) + 4.6)


def add_extras(add, rnd):
    """Everything the North adds on top of bird_habitats.py's generic rows; returns a summary {what: sites}."""
    out = {}
    out["river bars"] = add_river_bars(add, rnd)
    out["capes / stacks"] = add_coast_extras(add)
    out["tall herbs"] = add_tall_herbs(add, rnd)
    out["camp and hut ground"] = add_people_ground(add)
    out.update(add_perches(add))
    return out


if __name__ == "__main__":
    sites = []

    def _add(kind, p, q=None, radius=0.0, cap=8, flags=0):
        sites.append((kind, p, q or p, radius, cap))

    print("capes:", [(round(y), round(x), round(p)) for y, x, p in capes()])
    print("sea stacks on the land:", len(sea_stacks()))
    print(add_extras(_add, random.Random(1450)))
    kinds = {}
    for s in sites:
        kinds[s[0]] = kinds.get(s[0], 0) + 1
    print(kinds)
