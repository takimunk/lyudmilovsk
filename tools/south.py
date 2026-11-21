"""The Aton coast: the land between the club (the Aton station on its headland) and the town's south edge, and the
ways down from one to the other. docs/south.md is the description; everything here is a proposal, not story canon.

What this module gives the map (tools/world_map.py calls it, the way it calls tools/north.py and tools/east.py):

- the **headland** the club's door opens on (`spur`): the old terrace with a natural outline, not a rounded box;
- the **shoulder** (отрог): a grassy ridge that leaves the headland's north-east brow and comes down to the valley in
  an S, at a grade one can walk; the trail to the town runs on its crest (`shoulder`);
- the **brook** (ручей): born in a wooded ravine of the upland's slope, it crosses the valley under the town's south
  edge, feeds the starlings' reedbed and breaks through the beach where a tide channel takes it on (`brook`);
- level ground for the places (`PADS`), the corridor of the stair down the sea cliff (`STAIR`);
- the trails as fixed tracks (`fixed_tracks`), the places (`places`), the loose things (`things`), the plants.

  python3 tools/south.py            writes Content/South/south.json and Content/Dev/teleports/south.json
  python3 tools/south.py --check    the land's own rules (grades, clearances), no files written

Frame: map metres (docs/world_map.md): X west toward the sea, Y north, Z up from the club's floor.
"""
import functools
import json
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
CONTENT = os.path.join(HERE, "..", "unreal", "Lyudmilovsk", "Content")
OUT = os.path.join(CONTENT, "South", "south.json")
OUT_TELEPORTS = os.path.join(CONTENT, "Dev", "teleports", "south.json")

# the module's own ground: nothing here changes the land outside this box
BOX = (-720.0, 190.0, -110.0, 372.0)        # x0, x1, y0, y1


def _smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def _ramp(d, r, soft):
    """1 within r, falling to 0 over soft metres."""
    return min(1.0, max(0.0, (r + soft - d) / soft))


def _smax(a, b, k):
    """A soft maximum: the larger of the two, the crease where they meet filled in and rounded (k: how far apart in
    height the two still feel each other)."""
    h = max(k - abs(a - b), 0.0) / k
    return max(a, b) + h * h * k * 0.25


def _smin(a, b, k):
    return -_smax(-a, -b, k)


def _seg(x, y, pts):
    """Nearest point of a polyline: (distance, distance along the line, segment index, t on it)."""
    best = (1e18, 0.0, 0, 0.0)
    s = 0.0
    for i, ((x0, y0), (x1, y1)) in enumerate(zip(pts, pts[1:])):
        dx, dy = x1 - x0, y1 - y0
        ln = math.hypot(dx, dy) or 1e-9
        t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / (ln * ln)))
        d = math.hypot(x - x0 - dx * t, y - y0 - dy * t)
        if d < best[0]:
            best = (d, s + t * ln, i, t)
        s += ln
    return best


def _spline(pts, step):
    """Catmull-Rom through pts, resampled every ~step metres: [(x, y)]."""
    out = []
    n = len(pts)
    for i in range(n - 1):
        p0, p1, p2, p3 = pts[max(i - 1, 0)], pts[i], pts[i + 1], pts[min(i + 2, n - 1)]
        k = max(2, int(math.hypot(p2[0] - p1[0], p2[1] - p1[1]) / step))
        for j in range(k):
            t = j / k
            out.append(tuple(0.5 * (2 * p1[a] + (p2[a] - p0[a]) * t + (2 * p0[a] - 5 * p1[a] + 4 * p2[a] - p3[a]) * t * t
                                    + (-p0[a] + 3 * p1[a] - 3 * p2[a] + p3[a]) * t ** 3) for a in (0, 1)))
    out.append(tuple(pts[-1][:2]))
    return out


def _lengths(pts):
    s = [0.0]
    for p, q in zip(pts, pts[1:]):
        s.append(s[-1] + math.hypot(q[0] - p[0], q[1] - p[1]))
    return s


# --- the headland ---------------------------------------------------------------------------------------------------
# The terrace in front of the door (world_map SPUR_C / SPUR_RX / SPUR_RY) keeps its size and its level top (the
# forecourt, the alley, the Aton station and the sea line's corner pylons stand on it), but its brow wanders and its
# flanks are not one slope any more: steep and short over the reedbed (the starlings' side), longer and gentler where
# the shoulder leaves it and along the east.

def _wander(x, y):
    """Metres the brow stands out (+) or in (-) of the old outline."""
    return 7.0 * math.sin(x / 31.0 + 0.7) + 5.5 * math.sin(y / 27.0 + 1.9) + 3.5 * math.sin((x + y) / 17.0 + 0.4)


def _fall(x, y):
    """Metres the flank takes down to the valley here."""
    east = _smooth((-40.0 - x) / 70.0)                    # 0 over the reedbed, 1 from x -110 east
    return 46.0 + east * (20.0 + 8.0 * math.sin(y / 41.0 + 0.5) + 6.0 * math.sin(x / 37.0))


def spur(x, y):
    """0..1: the headland's level top (1) and its flanks down to the valley (0); open to the sea in the west."""
    cx, cy = W.SPUR_C
    dx = max(cx - x, 0.0) / W.SPUR_RX                       # only the east side is bounded
    dy = (y - cy) / W.SPUR_RY if y > cy else 0.0
    d = math.hypot(dx, dy)
    if d < 0.7:
        return 1.0
    out = (d - 1.0) * W.SPUR_RY - _wander(x, y)
    return 1.0 - _smooth(out / _fall(x, y))


# --- the shoulder ---------------------------------------------------------------------------------------------------
# Crest points (x, y) from the brow down to the valley floor. The trail to the town walks on it.
SHOULDER = [(-70.0, 152.0), (-88.0, 172.0), (-108.0, 200.0), (-139.0, 222.0), (-151.0, 255.0), (-176.0, 278.0),
            (-187.0, 306.0)]
SHOULDER_TOP = 1                 # the crest starts to fall at this point (the brow)
SHOULDER_SIDE = math.tan(math.radians(25.0))      # its flanks: grass holds, the bluff's gullies do not start
SHOULDER_CROWN = 5.0             # half width of the rounded top, m


@functools.lru_cache(maxsize=1)
def _crest():
    """The crest as a dense line: ([(x, y)], [distance along], [height])."""
    pts = _spline(SHOULDER, 1.5)
    s = _lengths(pts)
    top = _seg(SHOULDER[SHOULDER_TOP][0], SHOULDER[SHOULDER_TOP][1], pts)[1]
    foot = W.valley(*SHOULDER[-1]) + 0.3
    z = []
    for v in s:
        t = (v - top) / (s[-1] - top)
        # an even grade with a soft start on the brow and a soft run-out at the foot
        z.append(0.0 if t <= 0.0 else foot * (0.82 * t + 0.18 * _smooth(t)))
    return pts, s, z


def _crest_box():
    xs = [p[0] for p in SHOULDER]
    ys = [p[1] for p in SHOULDER]
    return min(xs) - 85.0, max(xs) + 85.0, min(ys) - 10.0, max(ys) + 60.0


def shoulder(x, y):
    """The ridge's own height at (x, y), or None away from it: the upper envelope of a rounded slope falling away
    from every point of the crest (so it has no seam where two bends of the crest are equally near)."""
    x0, x1, y0, y1 = _CREST_BOX
    if not (x0 < x < x1 and y0 < y < y1):
        return None
    pts, s, z = _crest()
    best = -1e9
    for (px, py), e in zip(pts, z):
        d = math.hypot(x - px, y - py)
        if d < SHOULDER_CROWN:
            drop = d * d / (2.0 * SHOULDER_CROWN) * SHOULDER_SIDE
        else:
            drop = (d - SHOULDER_CROWN * 0.5) * SHOULDER_SIDE
        if e - drop > best:
            best = e - drop
    return best


_CREST_BOX = _crest_box()


# --- the brook ------------------------------------------------------------------------------------------------------
# Course control points downstream: (x, y, kind). "ravine": the cut in the upland's slope (the bed's height is drawn
# between the head and the valley); "valley": the bed lies BROOK_DEPTH under the valley floor; "reed": the same, wider
# and shallower, through the reedbed; "bank": the cut through the dune bank and the beach to the sea.
BROOK = [(-650.0, 36.0, "ravine"), (-618.0, 70.0, "ravine"), (-584.0, 104.0, "ravine"), (-548.0, 140.0, "valley"),
         (-474.0, 186.0, "valley"), (-402.0, 212.0, "valley"), (-334.0, 258.0, "valley"), (-266.0, 290.0, "valley"),
         (-203.0, 324.0, "valley"), (-144.0, 303.0, "valley"), (-84.0, 311.0, "valley"), (-32.0, 296.0, "reed"),
         (18.0, 283.0, "reed"), (62.0, 264.0, "reed"), (98.0, 256.0, "bank"), (140.0, 253.0, "bank")]
RAVINE_DEPTH = 8.0               # the ravine's bed under the slope it is cut into, at its deepest
BROOK_DEPTH = 0.9                # the bed under the valley floor
BROOK_HALF = 1.1                 # half width of the bed, m
BROOK_WATER = 0.46               # water over the bed in the valley
RAVINE_SIDE = math.tan(math.radians(31.0))
BANK_SIDE = 0.62                 # the valley banks' slope (rise over run)
BROOK_REACH = 46.0               # nothing further than this from the course is touched
_CELL = 16.0


@functools.lru_cache(maxsize=1)
def _course():
    """The brook as a dense line: {pts, s, bed, half, side, kind} and a grid of the points near each 16 m cell."""
    ctrl = [(p[0], p[1]) for p in BROOK]
    kinds = [p[2] for p in BROOK]
    pts = _spline(ctrl, 2.0)
    s = _lengths(pts)
    # which control span a sample belongs to: by the nearest control point's distance along
    cs = [_seg(c[0], c[1], pts)[1] for c in ctrl]
    # small bends of its own in the valley (none in the ravine, the cut through the bank, or by the footbridge)
    out = []
    for k, (p, v) in enumerate(zip(pts, s)):
        q, r = pts[min(k + 1, len(pts) - 1)], pts[max(k - 1, 0)]
        tx, ty = q[0] - r[0], q[1] - r[1]
        tl = math.hypot(tx, ty) or 1.0
        env = _smooth((v - cs[3]) / 40.0) * _smooth((cs[-2] - v) / 30.0)
        env *= 1.0 - 0.85 * _ramp(abs(v - cs[8]), 6.0, 22.0)
        off = env * (4.5 * math.sin(v / 23.0 + 1.0) + 2.2 * math.sin(v / 8.7 + 2.0))
        out.append((p[0] - ty / tl * off, p[1] + tx / tl * off))
    pts = out
    s = _lengths(pts)
    kind, bed, half, side = [], [], [], []
    ravine_end = cs[3]
    for p, v in zip(pts, s):
        i = max(j for j in range(len(cs)) if cs[j] <= v + 1e-6)
        kd = kinds[i]
        kind.append(kd)
        floor = W.valley(p[0], p[1])
        if kd == "ravine":
            t = v / ravine_end
            # it follows the slope it is cut into, RAVINE_DEPTH under it in the middle, easing onto the valley floor
            raw = floor + (W.TOP_Z - floor) * W.upland(p[0], p[1])
            bed.append(raw - BROOK_DEPTH - (RAVINE_DEPTH - BROOK_DEPTH) * math.sin(math.pi * min(1.0, t * 1.08)) ** 0.7
                       - 2.0 * (1.0 - t))
            half.append(0.7)
            side.append(RAVINE_SIDE)
        elif kd == "bank":
            t = (v - cs[-2]) / (s[-1] - cs[-2])
            # under the dune bank's own fall and the beach's sand all the way (their profile: world_map._height0)
            si = W.inland(p[0], p[1])
            if si <= W.BEACH_W:
                nat = W.SEA_Z - 0.3 + max(si, 0.0) / W.BEACH_W * 1.8
            else:
                u = min(1.0, (si - W.BEACH_W) / W.cliff_width(floor))
                nat = W.BEACH_TOP + (floor - W.BEACH_TOP) * (0.5 * u + 0.5 * u * u * (3 - 2 * u))
            bed.append(min(floor - 1.1 + (W.SEA_Z - 0.35 - floor + 1.1) * _smooth(t * 1.05), nat - 0.45))
            half.append(1.3 + 1.8 * t)
            side.append(BANK_SIDE * (1.0 - 0.45 * t))
        else:
            reed = kd == "reed"
            bed.append(floor - (BROOK_DEPTH + 0.2 if reed else BROOK_DEPTH))
            half.append(BROOK_HALF * (1.5 if reed else 1.0) * (1.0 + 0.35 * math.sin(v / 13.0)))
            side.append(BANK_SIDE * (0.7 if reed else 1.0))
    for k, p in enumerate(pts):                           # near the coast: under the bank's own fall too
        si = W.inland(p[0], p[1])
        if kind[k] != "ravine" and W.BEACH_W < si < 90.0:
            floor = W.valley(p[0], p[1])
            u = min(1.0, (si - W.BEACH_W) / W.cliff_width(floor))
            bed[k] = min(bed[k], W.BEACH_TOP + (floor - W.BEACH_TOP) * (0.5 * u + 0.5 * u * u * (3 - 2 * u)) - 0.6)
    for k in range(1, len(bed)):                          # water never runs uphill
        bed[k] = min(bed[k], bed[k - 1] - 0.001 * (s[k] - s[k - 1]))
    grid = {}
    n = int(BROOK_REACH // _CELL) + 1
    for k, p in enumerate(pts):
        ci, cj = int(math.floor(p[0] / _CELL)), int(math.floor(p[1] / _CELL))
        for i in range(ci - n, ci + n + 1):
            for j in range(cj - n, cj + n + 1):
                grid.setdefault((i, j), []).append(k)
    return {"pts": pts, "s": s, "bed": bed, "half": half, "side": side, "kind": kind, "grid": grid, "ctrl_s": cs}


def brook_at(x, y):
    """The brook's nearest point: (distance from the middle line, index, t to the next sample), or None when it is
    further than BROOK_REACH."""
    c = _course()
    ks = c["grid"].get((int(math.floor(x / _CELL)), int(math.floor(y / _CELL))))
    if not ks:
        return None
    pts = c["pts"]
    best = None
    lo, hi = ks[0], ks[-1]
    for k in range(max(lo - 1, 0), min(hi + 1, len(pts) - 1)):
        (x0, y0), (x1, y1) = pts[k], pts[k + 1]
        dx, dy = x1 - x0, y1 - y0
        l2 = dx * dx + dy * dy or 1e-9
        t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / l2))
        d = math.hypot(x - x0 - dx * t, y - y0 - dy * t)
        if best is None or d < best[0]:
            best = (d, k, t)
    return best if best and best[0] < BROOK_REACH else None


def _lerp(a, k, t):
    return a[k] + (a[k + 1] - a[k]) * t


def brook_cut(x, y):
    """The height the brook's cut allows at (x, y) (the land is lowered to it), or None away from the brook."""
    b = brook_at(x, y)
    if b is None:
        return None
    d, k, t = b
    c = _course()
    bed, half, side = _lerp(c["bed"], k, t), _lerp(c["half"], k, t), _lerp(c["side"], k, t)
    if d <= half:
        return bed - 0.12 * (1.0 - (d / half) ** 2)       # a dished bed
    rise = (d - half) * side
    if c["kind"][k] == "ravine":
        # the ravine's sides: a V with a wavy rim, side gullies biting into it
        rise *= 1.0 + 0.22 * math.sin(c["s"][k] / 11.0) + 0.12 * math.sin(c["s"][k] / 4.7 + d / 9.0)
    return bed + rise


def brook_water():
    """The water for the game: [[x, y, surface z, half width]] downstream, every ~6 m, from just under the ravine's
    head to the beach (the sea takes it from there)."""
    c = _course()
    out = []
    last = -1e9
    for k, (p, v) in enumerate(zip(c["pts"], c["s"])):
        if v < 14.0 or v - last < 6.0:
            continue
        if c["kind"][k] == "bank" and c["bed"][k] + 0.1 < W.SEA_Z - 0.2:
            break
        depth = 0.26 if c["kind"][k] == "ravine" else 0.24 if c["kind"][k] == "bank" else BROOK_WATER
        half = c["half"][k] * (0.75 if c["kind"][k] == "ravine" else 1.08) + depth / max(c["side"][k], 0.2)
        # by the coast the bank's own fall (world_map._height0) lowers the cut further: the water lies on the real bed
        # (and everywhere the water is measured from the land as it is, not from the designed bed: the cut's blend
        # leaves the real bed a hand higher, and a sheet at the bed's own level does not show)
        bed = W.height(p[0], p[1])
        z = bed + depth if not out else min(bed + depth, out[-1][2])       # never uphill; a level pool may stand
        out.append([round(p[0], 2), round(p[1], 2), round(z, 2), round(half, 2)])
        last = v
    return out


def wet(x, y):
    """0..1 for the ground's `gravel` mask (TerrainGen): 1 on the brook's bed (silt and sand under the water), ~0.5 on
    the strip of bare wet ground beside it."""
    b = brook_at(x, y)
    if b is None:
        return 0.0
    d, k, t = b
    half = _lerp(_course()["half"], k, t)
    if d <= half + 0.3:
        return 1.0
    return 0.55 * _ramp(d - half, 0.3, 1.6)


# --- level ground for the places, the stair's corridor --------------------------------------------------------------
BELVEDERE = (27.0, 190.0)        # on the very brow: from further back the brow itself hides the reedbed below
STAIR_Y = 38.0                   # the timber stair down the sea cliff: straight down the face at this Y
SPRING = (-541.0, 133.0)         # the spring's trough where the ravine opens on the valley
ORCHARD = (-372.0, 196.0)        # the old orchard's middle, between the brook and the upland's foot
ORCHARD_R = (95.0, 48.0)
STELE = (-207.0, 356.0)          # the town's name by the track, where the valley ends and the first terrace begins
PICNIC = (-168.0, 262.0)         # the bench half way down the shoulder (its last bend)

# the second round's landmarks (docs/south.md "Landmarks"); the cliff's ones stand back from the edge
LIGHT = (112.0, -432.0)       # the lighthouse south of the Mayak station
BUNKER = (69.0, -128.0)       # the old pillbox on the cliff walk, its slit to the sea
WRECK = (121.0, 132.0)           # the boat's ribs in the sand between the brook's mouth and the stair
STONE = (-304.0, 254.0)                            # the erratic boulder on the meadow by the spring trail
SPLIT_STONE = (90.0, -306.0)  # the split one on the cliff walk
HALT = (120.5, STAIR_Y - 13.0)     # the shelter on the beach's top by the stair's foot

PADS = [(BELVEDERE, 7.0, 6.0), (LIGHT, 7.0, 5.0), (BUNKER, 4.5, 4.0)]   # (centre, radius, fade): the macro height is kept exactly (no hummocks, no gullies)


def stair_line():
    """The stair's top and foot: ((x, y) on the brow, (x, y) on the beach's top)."""
    top = W.cliff_top(STAIR_Y)
    return (top - 2.0, STAIR_Y), (W.shore_x(STAIR_Y) - W.BEACH_W + 1.0, STAIR_Y)


SWELL = 1.5                                   # metres: the valley meadow's low swells (it was one tilted plane)


def swell(x, y, z):
    """Metres the valley's meadow rises here: long low swells (90 and 37 m) on what was a single tilted plane, so the
    contours wander and the spring trail goes over and round something. None by the brook (its banks keep their
    height), on the reedbed's wet flat, under the town's first terrace and at the box's edges."""
    if z > -30.0 or y > 345.0 or x > -20.0:
        return 0.0
    x0, x1, y0, y1 = BOX
    k = _smooth((-30.0 - z) / 3.0) * _smooth((345.0 - y) / 30.0) * _smooth((-20.0 - x) / 50.0)
    k *= _smooth((x - x0) / 50.0) * _smooth((y - y0) / 50.0)
    b = brook_at(x, y)
    if b is not None:
        k *= _smooth((b[0] - 5.0) / 38.0)
    if k <= 0.0:
        return 0.0
    n = 0.68 * _noise(x, y, 90.0, 11) + 0.32 * _noise(x, y, 37.0, 12)
    return SWELL * k * _smooth((n - 0.25) / 0.6)


def shape(x, y, z):
    """world_map.plateau_south's hook: the land with the shoulder on it and the brook cut into it."""
    x0, x1, y0, y1 = BOX
    if not (x0 < x < x1 and y0 < y < y1):
        return z
    z += swell(x, y, z)
    h = shoulder(x, y)
    if h is not None:
        z = _smax(z, h, 1.6)
    c = brook_cut(x, y)
    if c is not None and c < z + 1.5:
        z = _smin(z, c, 0.5)
    return z


def outlet(x, y, z):
    """world_map._height's hook: the brook's cut through the dune bank and the beach."""
    if not (40.0 < x < 190.0 and 225.0 < y < 285.0):
        return z
    c = brook_cut(x, y)
    return z if c is None or c > z + 1.0 else _smin(z, c, 0.35)


def keep_flat(x, y):
    """0..1 for world_map.keep_flat: the places' level ground, the brook's bed and banks (the erosion must not fill
    them), the stair's corridor on the cliff's face (no gullies under the stair)."""
    x0, x1, y0, y1 = BOX
    if not (x0 < x < x1 and y0 < y < y1):
        return 0.0
    k = 0.0
    for (px, py), r, soft in PADS:
        k = max(k, _ramp(math.hypot(x - px, y - py), r, soft))
    if abs(y - STAIR_Y) < 16.0:                                # (the ramp is done by 14 m)
        (tx, _), (fx, _) = stair_line()
        if tx - 8.0 < x < fx + 4.0:
            k = max(k, _ramp(abs(y - STAIR_Y), 5.0, 9.0))          # a long fade: no straight-edged chute in the cliff
    if -52.0 < x < 92.0 and 232.0 < y < 284.0:                 # the boardwalk's line and the hide: the reedbed's
        k = max(k, _ramp(_seg(x, y, BOARDWALK)[0], 2.2, 3.0))  # hummocks must not put its pieces at different heights
        k = max(k, _ramp(math.hypot(x - HIDE_AT[0], y - HIDE_AT[1]), 5.0, 4.0))
    b = brook_at(x, y)
    if b is not None:
        d, i, t = b
        c = _course()
        if c["kind"][i] != "ravine":                       # the ravine keeps its natural detail, but for the bed
            k = max(k, _ramp(d, _lerp(c["half"], i, t) + 1.6, 2.0))
        else:
            k = max(k, _ramp(d, 1.2, 1.5))
    return k


def gravel(x, y):
    """terrain.write_inputs' hook: the brook's share of the `gravel` mask (0..1)."""
    x0, x1, y0, y1 = BOX
    if not (x0 < x < x1 and y0 < y < y1):
        return 0.0
    return wet(x, y)


# --- the trails -----------------------------------------------------------------------------------------------------
ALLEY_END = (-19.2, 123.0)       # where the slab path ends by the Aton station (world_map.path_points()[-1])
FOOT = SHOULDER[-1]              # the shoulder's foot: the junction by the brook
BRIDGE_AT = 8                    # the BROOK control point the town trail crosses at
GATE = (-214.0, 386.0)           # where the trail meets the town's first terrace (the routed track goes on from here)
BOARDWALK = [(-40.0, 271.0), (-10.0, 262.0), (22.0, 259.0), (52.0, 247.0), (80.0, 242.0)]   # through the reeds to the bank
MAYAK = (88.0, -384.0)           # by the Mayak station's foot (network.json "mayak" at 95, -400)
CLIFF_LOOKOUT_Y = -236.0

TRAILS = {
    # the way to the town: round the station, down the shoulder's crest, over the brook, past the stele
    "town": [ALLEY_END, (-40.0, 127.0), (-56.0, 139.0)] + SHOULDER + [None, (-207.0, 342.0), (-211.0, 364.0), GATE],
    "belvedere": [ALLEY_END, (-1.0, 127.0), (13.0, 150.0), (BELVEDERE[0] - 1.0, BELVEDERE[1] - 6.0)],
    "sea": [ALLEY_END, (6.0, 108.0), (36.0, 72.0), (60.0, 46.0), None],
}


def bridge():
    """The footbridge on the town trail: (x, y, yaw of the deck's axis as the trail runs, bank height)."""
    c = _course()
    k = min(range(len(c["pts"])), key=lambda i: abs(c["s"][i] - c["ctrl_s"][BRIDGE_AT]))
    x, y = c["pts"][k]
    return x, y, k


HIDE_AT = (-47.0, 273.0)         # the bird hide: on the reedbed's inland edge, south of the brook, looking west


def _trails():
    b = bridge()
    (tx, ty), _ = stair_line()
    t = dict(TRAILS)
    t["town"] = [p if p is not None else (b[0], b[1]) for p in TRAILS["town"]]
    t["sea"] = [p if p is not None else (tx - 1.0, ty) for p in TRAILS["sea"]]
    t["hide"] = [FOOT, (-150.0, 291.0), (-100.0, 289.0), (-66.0, 278.0), (HIDE_AT[0] - 5.0, HIDE_AT[1] + 4.0)]
    t["spring"] = [FOOT, (-226.0, 288.0), (-256.0, 275.0), (-284.0, 247.0), (-326.0, 236.0), (ORCHARD[0], ORCHARD[1] + 4.0),
                   (-412.0, 191.0), (-444.0, 165.0), (-482.0, 158.0), (-512.0, 142.0), (SPRING[0] + 5.0, SPRING[1] - 3.0)]
    return t


@functools.lru_cache(maxsize=1)
def trails():
    """{name: [(x, y)] every ~3 m}: the designed trails (terrain.tracks() takes them as fixed tracks)."""
    return {k: _spline(v, 3.0) for k, v in _trails().items()}


def fixed_tracks():
    return [list(t) for t in trails().values()]


# anchors and routed tracks for tools/terrain.py: the town trail's end to the town; the spring to the inland woods; the
# cliff-top walk from the club's forecourt south to the Mayak station
ANCHORS = {"south_gate": GATE, "south_spring": (SPRING[0] + 5.0, SPRING[1] - 3.0), "south_forecourt": (17.0, 12.0),
           "south_cliff": (62.0, -150.0), "south_mayak": MAYAK, "south_light": (LIGHT[0] - 7.0, LIGHT[1] + 6.0)}
TRACKS = [("south_gate", "town"), ("south_spring", "woods_inland"), ("south_forecourt", "south_cliff"),
          ("south_cliff", "south_mayak"), ("south_mayak", "south_light")]


def cliff_lookout():
    return (W.cliff_top(CLIFF_LOOKOUT_Y) - 5.0, CLIFF_LOOKOUT_Y)


# --- plants ---------------------------------------------------------------------------------------------------------
GALLERY = (-556.0, -60.0, 120.0, 336.0)       # x0, x1, y0, y1: the box the brook's alders are sown in
COPSE = (-176.0, 96.0, 44.0, 40.0)            # centre and half sizes: the wood at the headland's east foot


def _hash(i, j, k=0):
    return W._hash01(i, j, k)


def _noise(x, y, cell, k=0):
    fx, fy = x / cell, y / cell
    ix, iy = math.floor(fx), math.floor(fy)
    tx, ty = _smooth(fx - ix), _smooth(fy - iy)
    a = _hash(ix, iy, k) + (_hash(ix + 1, iy, k) - _hash(ix, iy, k)) * tx
    b = _hash(ix, iy + 1, k) + (_hash(ix + 1, iy + 1, k) - _hash(ix, iy + 1, k)) * tx
    return a + (b - a) * ty


def _near_trail(x, y, r):
    for pts in trails().values():
        for px, py in pts[::2]:
            if abs(px - x) < r and abs(py - y) < r and math.hypot(px - x, py - y) < r:
                return True
    return False


def cover(x, y):
    """world_map.forest_cover's hook: 0..1 where this module draws the wood (the alders along the brook, the old
    orchard, the copse under the headland), None elsewhere."""
    gx0, gx1, gy0, gy1 = GALLERY
    ox, oy = ORCHARD
    orx, ory = ORCHARD_R
    cx, cy, chx, chy = COPSE
    in_gallery = gx0 <= x <= gx1 and gy0 <= y <= gy1
    in_copse = abs(x - cx) <= chx and abs(y - cy) <= chy
    if not (in_gallery or in_copse):
        return None
    if _near_trail(x, y, 5.0):
        return 0.0
    c = 0.0
    if in_gallery:
        q = math.hypot((x - ox) / orx, (y - oy) / ory)
        if q < 1.15:
            # the orchard: old trees in a loose order, gaps where some died
            c = max(c, (1.0 - _smooth((q - 0.8) / 0.35)) * (0.35 + 0.65 * _smooth((_noise(x, y, 26.0, 3) - 0.3) / 0.2)))
        b = brook_at(x, y)
        if b is not None:
            d, k, t = b
            if _course()["kind"][k] in ("valley", "reed"):
                # the gallery: alders on the banks in broken runs, thicker at the bends, none in the bed
                run = _smooth((_noise(x, y, 44.0, 5) - 0.36) / 0.16)
                c = max(c, run * _ramp(d, 9.0, 8.0) * _smooth((d - 1.6) / 1.5))
    if in_copse:
        q = math.hypot((x - cx) / chx, (y - cy) / chy) + 0.35 * (_noise(x, y, 30.0, 7) - 0.5)
        c = max(c, 1.0 - _smooth((q - 0.7) / 0.25))
    return c


GALLERY_CELL = 48.0


def forests(tiles):
    """The module's forest tiles for world_map.forests(): (x, y, hx, hy, mix, density, z0, z1). cover() thins them.
    The alders' tiles are small squares strung along the brook (none over the orchard, whose own tile takes the banks
    there), so a tile sows few trees that the cover then drops."""
    out = []
    lo, hi = W.LOW_Z - 3.0, W.LOW_Z + 9.0
    ox, oy = ORCHARD
    orx, ory = ORCHARD_R[0] * 1.15, ORCHARD_R[1] * 1.15
    c = _course()
    seen = set()
    for k, (x, y) in enumerate(c["pts"]):
        if c["kind"][k] not in ("valley", "reed"):
            continue
        cell = (math.floor(x / GALLERY_CELL), math.floor(y / GALLERY_CELL))
        cx, cy = (cell[0] + 0.5) * GALLERY_CELL, (cell[1] + 0.5) * GALLERY_CELL
        if cell in seen or (abs(cx - ox) < orx + GALLERY_CELL / 2 and abs(cy - oy) < ory + GALLERY_CELL / 2):
            continue
        seen.add(cell)
        out.append((cx, cy, GALLERY_CELL / 2, GALLERY_CELL / 2, "wet", 1.0, lo, hi))
    out.append((ox, oy, orx, ory, "town", 0.45, lo, hi))
    cx, cy, chx, chy = COPSE
    out.append((cx, cy, chx, chy, "town", 0.7, lo, W.LOW_Z + 30.0))
    return out


def tree_free(x, y):
    """Forest tiles of the others keep off the places and the trails here."""
    x0, x1, y0, y1 = BOX
    if not (x0 < x < x1 and y0 - 400.0 < y < y1 + 20.0):
        return False
    for (px, py), r in ((BELVEDERE, 12.0), (HIDE_AT, 7.0), (SPRING, 6.0), (STELE, 9.0), (LIGHT, 14.0), (BUNKER, 8.0),
                        (STONE, 6.0), (SPLIT_STONE, 6.0)):
        if math.hypot(x - px, y - py) < r:
            return True
    return False


def plants():
    """(x, y, species, seed, age) of single trees (AProceduralPlant), for world_map.coast_plants(): few, where one
    stands and looks at them."""
    out = []

    def add(x, y, sp, age):
        out.append((round(x, 1), round(y, 1), sp, 700 + len(out), age))

    add(BELVEDERE[0] - 11.0, BELVEDERE[1] - 6.0, "SwampWhiteOak", 90.0)       # the old oak by the lookout
    add(BELVEDERE[0] + 12.0, BELVEDERE[1] - 9.0, "Rowan", 22.0)
    add(PICNIC[0] - 7.0, PICNIC[1] + 5.0, "WeepingBirch", 45.0)                # the birch on the shoulder's last bend
    add(STELE[0] - 9.0, STELE[1] + 3.0, "PyramidalPoplar", 30.0)               # poplars by the stele
    add(STELE[0] - 13.0, STELE[1] - 2.0, "PyramidalPoplar", 26.0)
    add(SPRING[0] - 6.0, SPRING[1] - 7.0, "SwampWhiteOak", 120.0)              # the oak over the spring
    bx, by, _ = bridge()
    add(bx + 9.0, by - 4.0, "BlackPoplar", 60.0)                               # the black poplar by the footbridge
    lx, ly = cliff_lookout()
    add(lx - 6.0, ly + 5.0, "ScotsPine", 110.0)                                # the lone pine on the cliff
    # the old orchard's apple trees in loose rows (the tiles' town mix alone reads as a birch wood)
    rnd = random.Random(431)
    ox, oy = ORCHARD
    for i in range(-3, 4):
        for j in (-1, 0, 1):
            if (i, j) in ((0, 0), (1, 0), (-2, 1)) or rnd.random() < 0.2:      # the table's clearing, gaps where one died
                continue
            x, y = ox + i * 13.0 + rnd.uniform(-2.5, 2.5), oy + j * 12.0 + rnd.uniform(-2.5, 2.5) - 6.0
            if not _near_trail(x, y, 3.5):
                add(x, y, "Apple", rnd.uniform(30.0, 60.0))
    return out


def grass_patches(seed0=1700):
    """(x, y, half_x, half_y, species, seed, density, min_z, max_z): meadow along the trails (each patch is drawn only
    near the camera), reed and sedge along the brook's lower reach."""
    out = []
    seen = set()
    meadow = ("RedFescue", "MeadowGrass", "WavyHairGrass")
    for name in ("town", "belvedere", "sea", "hide", "spring"):
        for x, y in trails()[name][::6]:
            c = (round(x / 60.0) * 60.0, round(y / 60.0) * 60.0)
            if c in seen or (-140.0 < c[0] < 100.0 and -60.0 < c[1] < 180.0):     # the headland has its patches already
                continue
            seen.add(c)
            out.append((c[0], c[1], 30.0, 30.0, meadow[len(seen) % 3], seed0 + len(seen), 1.0, W.LOW_Z - 2.0, 50.0))
    n = len(seen)
    for x, y, sp in ((-112.0, 306.0, "CommonReed"), (-170.0, 318.0, "SandSedge"), (-250.0, 296.0, "SandSedge"),
                     (-352.0, 250.0, "CommonReed")):
        n += 1
        out.append((x, y, 30.0, 30.0, sp, seed0 + n, 1.0, -100.0, W.LOW_Z + 1.0))
    return out


# --- the places and the things ----------------------------------------------------------------------------------------

def _dir(pts, k):
    a, b = pts[max(k - 1, 0)], pts[min(k + 1, len(pts) - 1)]
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def places():
    """Named places: {id, name (a strings key), x, y, r (found within), kind, yaw (where an arriving hero looks)}."""
    bx, by, _ = bridge()
    (tx, ty), (fx, fy) = stair_line()
    lx, ly = cliff_lookout()
    c = _course()
    rav = c["pts"][len([v for v in c["s"] if v < c["ctrl_s"][2]])]
    mouth = (W.shore_x(253.0) - W.BEACH_W * 0.6, 247.0)
    P = [
        ("aton_belvedere", BELVEDERE[0], BELVEDERE[1] - 3.0, 9.0, "building", 90.0),
        ("aton_descent", SHOULDER[3][0], SHOULDER[3][1], 16.0, "nature", 120.0),
        ("brook_bridge", bx + 1.5, by - 7.0, 9.0, "nature", 110.0),
        ("south_stele", STELE[0] - 4.0, STELE[1] - 8.0, 12.0, "building", 80.0),
        ("bird_hide", HIDE_AT[0] - 5.0, HIDE_AT[1] + 4.0, 9.0, "building", 0.0),
        ("brook_mouth", mouth[0], mouth[1], 16.0, "nature", 60.0),
        ("old_orchard", ORCHARD[0], ORCHARD[1], 38.0, "nature", 180.0),
        ("south_spring", SPRING[0] + 5.0, SPRING[1] - 3.0, 9.0, "nature", 200.0),
        ("spring_ravine", rav[0], rav[1], 18.0, "nature", 225.0),
        ("cliff_stair", tx - 3.0, ty, 7.0, "building", 0.0),
        ("cliff_lookout", lx, ly, 9.0, "nature", 0.0),
        ("mayak_lighthouse", LIGHT[0] - 7.0, LIGHT[1] + 6.0, 14.0, "building", 300.0),
        ("coast_bunker", BUNKER[0] - 5.0, BUNKER[1], 9.0, "building", 0.0),
        ("boat_wreck", WRECK[0] - 5.0, WRECK[1], 10.0, "nature", 0.0),
        ("devils_stone", STONE[0] + 5.0, STONE[1] + 2.0, 9.0, "nature", 200.0),
        ("mayak_station", MAYAK[0], MAYAK[1], 18.0, "transport", 270.0),
    ]
    return [{"id": i, "name": "place.%s.name" % i, "x": round(x, 1), "y": round(y, 1), "r": r, "kind": k, "yaw": yaw}
            for i, x, y, r, k, yaw in P]


def _boardwalk():
    """The boardwalk's pieces: 6 m chords along BOARDWALK, an end ramp at each end: [(x, y, deck axis heading, variant)]."""
    pts = _spline(BOARDWALK, 0.25)
    out = []
    k = 0
    pattern = (0, 1, 0, 2, 0, 1, 0, 0, 2, 0, 1, 0, 0, 0, 0, 0)
    while True:
        a = pts[k]
        j = next((i for i in range(k + 1, len(pts)) if math.hypot(pts[i][0] - a[0], pts[i][1] - a[1]) >= 6.0), None)
        if j is None:
            break
        b = pts[j]
        out.append(((a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0, math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])),
                    pattern[len(out) % len(pattern)]))
        k = j
    end = pts[k]
    first = out[0]
    h0 = math.radians(first[2])
    ramps = [(first[0] - 4.5 * math.cos(h0), first[1] - 4.5 * math.sin(h0), first[2] + 180.0, 3)]
    h1 = math.radians(out[-1][2])
    ramps.append((end[0] + 1.5 * math.cos(h1), end[1] + 1.5 * math.sin(h1), out[-1][2], 3))
    return out, ramps


def _axis_yaw(heading):
    """The yaw of a thing whose long axis is its mesh's local X (a bridge, a boardwalk piece), for that axis to run
    point along `heading` (degrees from map +X toward +Y, which is Unreal's yaw). A thing's yaw is where its front
    (Blender -Y) looks and its local +X is a quarter turn before that, so the front is on the heading's +90 side
    (a boardwalk bay, a rail on the other side; an end ramp falls toward the heading)."""
    return (heading + 90.0) % 360.0


# the finger-posts: variant -> ((x, y), [(place word, distance word, (x, y) its plank points at)]); the words are
# tools/south_signs.py's keys south.sign.trail.<word> and south.sign.trail.dist.<word>. Every post stands with its front
# at SIGN_YAW; sign_planks() gives each plank's turn on the post for blender/south/props_land.py.
SIGN_YAW = 90.0
SIGNS = {
    0: ((ALLEY_END[0] - 3.2, ALLEY_END[1] - 1.6), [("town", "0_5km", (-56.0, 139.0)), ("lookout", "60m", BELVEDERE),
                                                   ("sea", "110m", (74.0, STAIR_Y))]),
    1: ((FOOT[0] + 3.0, FOOT[1] - 1.0), [("aton", "0_3km", (-151.0, 255.0)), ("town", "0_2km", (-207.0, 342.0)),
                                         ("spring", "0_4km", (-256.0, 275.0))]),
    2: ((107.0, 244.0), [("promenade", "0_2km", (112.0, 440.0)), ("hide", "0_2km", (52.0, 247.0)),
                         ("station_beach", "0_3km", (150.0, -60.0))]),
    3: ((14.5, 13.5), [("cliff", "0_25km", (62.0, -150.0)), ("aton", "110m", (-14.0, 119.0))]),
}


def sign_planks():
    """{variant: [(place word, distance word, yaw_deg)]}: yaw_deg is the plank's turn on its post in the mesh's frame
    (counter-clockwise seen from above, 0 = the post's front): the front's yaw minus the bearing to the place."""
    out = {}
    for v, ((x, y), rows) in SIGNS.items():
        px, py = _beside(x, y, BESIDE["trail_sign"])
        out[v] = [(name, dist, round((SIGN_YAW - math.degrees(math.atan2(ty - py, tx - px)) + 180.0) % 360.0 - 180.0, 0))
                  for name, dist, (tx, ty) in rows]
    return out


BESIDE = {"trail_sign": 1.9, "info_board": 2.3, "picnic_table": 2.6, "town_stele": 5.2, "erratic": 4.6, "park_lamp": 1.8,
          "park_bench": 1.9, "bike_rack": 2.4, "drinking_fountain": 1.9, "flower_bed": 2.2, "litter_urn": 1.7}     # kind: metres from a trail's middle


def _beside(x, y, r):
    """(x, y) pushed straight away from the nearest trail point until it is r from every trail's middle."""
    pts = [p for t in trails().values() for p in t]
    for _ in range(6):
        px, py = min(pts, key=lambda p: (p[0] - x) ** 2 + (p[1] - y) ** 2)
        d = math.hypot(x - px, y - py)
        if d >= r:
            break
        ux, uy = ((x - px) / d, (y - py) / d) if d > 0.05 else (1.0, 0.0)
        x, y = px + ux * (r + 0.05), py + uy * (r + 0.05)
    return x, y


def things():
    """Everything loose: {id, kind, x, y, yaw (Unreal degrees of the front), variant, set ("south": blender/south's
    meshes, "north": the North's kit), z (only where the thing must not follow the ground)}."""
    rnd = random.Random(2011)
    out = []

    def put(kind, x, y, yaw=0.0, variant=0, kit="south", z=None, name=None):
        n = sum(1 for o in out if o["kind"] == kind) + 1
        if kind in BESIDE:                                 # posts and tables stand beside a trail, never on it
            x, y = _beside(x, y, BESIDE[kind])
        o = {"id": name or "%s_%d" % (kind, n), "kind": kind, "x": round(x, 2), "y": round(y, 2),
             "yaw": round(yaw % 360.0, 1), "variant": variant, "set": kit}
        if z is not None:
            o["z"] = round(z, 2)
        out.append(o)

    T = trails()
    # the lookout: the pavilion's front to the north (the town), the viewer at its rail, the board by the path
    put("belvedere", BELVEDERE[0], BELVEDERE[1], 90.0)
    put("viewer", BELVEDERE[0] + 4.6, BELVEDERE[1] + 1.2, 80.0)
    put("brow_bench", BELVEDERE[0] - 8.5, BELVEDERE[1] - 1.0, 100.0, kit="north")
    # the junction at the alley's end
    put("trail_sign", *SIGNS[0][0], SIGN_YAW, 0)
    put("info_board", ALLEY_END[0] + 4.0, ALLEY_END[1] - 3.0, 250.0, 0)
    put("trail_sign", *SIGNS[3][0], SIGN_YAW, 3)
    # the stair down the cliff: its front (the way down) to the sea
    (tx, ty), (fx, fy) = stair_line()
    put("cliff_stair", tx, ty, 0.0, z=W.height(tx, ty))
    put("brow_bench", tx - 4.0, ty + 6.0, 0.0, kit="north")
    # down the shoulder: a bench on the last bend, marks on the way
    put("brow_bench", PICNIC[0] + 3.0, PICNIC[1] + 3.0, 60.0, kit="north")
    for k in range(18, len(T["town"]) - 6, 26):
        x, y = T["town"][k]
        h = math.radians(_dir(T["town"], k) + 90.0)
        put("trail_marker", x + 1.7 * math.cos(h), y + 1.7 * math.sin(h), rnd.uniform(0.0, 360.0), kit="north")
    # the foot: the junction's post, the footbridge, the stele
    put("trail_sign", *SIGNS[1][0], SIGN_YAW, 1)
    bx, by, bk = bridge()
    town = T["town"]
    kb = min(range(len(town)), key=lambda i: math.hypot(town[i][0] - bx, town[i][1] - by))
    # the 9 m one: the cut is 6 m from bank to bank; its deck a hand over the meadow, the ramps' toes in the turf
    put("plank_bridge", bx, by, _axis_yaw(_dir(town, kb)), 1, z=W.valley(bx, by) + 0.12)
    put("town_stele", STELE[0], STELE[1], 250.0)
    # the hide and the boardwalk through the reeds to the beach
    put("bird_hide", HIDE_AT[0], HIDE_AT[1], 0.0)
    put("info_board", HIDE_AT[0] - 6.5, HIDE_AT[1] + 7.0, 200.0, 1)
    pieces, ramps = _boardwalk()
    for x, y, h, v in pieces:
        put("boardwalk", x, y, _axis_yaw(h), v)
    for x, y, h, v in ramps:
        put("boardwalk", x, y, _axis_yaw(h), v)
    # the brook's mouth: the fishermen's corner on the bank's top, stones to step over the water on the beach
    mx = W.shore_x(250.0)
    put("fisher_shed", mx - W.BEACH_W - 24.0, 226.0, 20.0)
    put("net_rack", mx - W.BEACH_W - 15.0, 233.0, 100.0, 0)
    put("net_rack", mx - W.BEACH_W - 13.0, 219.0, 80.0, 1)
    put("rowboat", mx - W.BEACH_W - 4.0, 236.0, 70.0, kit="north")
    put("rowboat", mx - W.BEACH_W - 2.0, 229.0, 95.0, kit="north")
    put("trail_sign", *SIGNS[2][0], SIGN_YAW, 2)
    c = _course()
    for k in range(len(c["pts"])):
        if c["kind"][k] == "bank" and W.inland(*c["pts"][k]) < W.BEACH_W * 0.8 and k % 2 == 0:
            x, y = c["pts"][k]
            if W.inland(x, y) > 6.0:
                put("river_boulder", x + rnd.uniform(-0.3, 0.3), y + rnd.uniform(-0.4, 0.4), rnd.uniform(0.0, 360.0), 2, kit="north")
    for y, v in ((205.0, 0), (292.0, 1), (330.0, 2)):
        put("driftwood", W.shore_x(y) - rnd.uniform(6.0, 14.0), y, rnd.uniform(60.0, 120.0), v, kit="north")
    # stones in the brook, more in the ravine
    for k in range(6, len(c["pts"]) - 30, 7):
        x, y = c["pts"][k]
        rav = c["kind"][k] == "ravine"
        if rnd.random() < (0.8 if rav else 0.3):
            off = rnd.uniform(-1.0, 1.0) * (c["half"][k] + 0.5)
            h = math.radians(_dir(c["pts"], k) + 90.0)
            put("river_boulder", x + off * math.cos(h), y + off * math.sin(h), rnd.uniform(0.0, 360.0),
                rnd.choice((0, 1, 2) if rav else (1, 2, 2)), kit="north")
    # the spring: the trough under the ravine's mouth facing the path, a bench
    put("spring_trough", SPRING[0], SPRING[1], 300.0)
    put("brow_bench", SPRING[0] + 6.5, SPRING[1] - 6.0, 120.0, kit="north")
    # the old orchard: a table, the hives in a row, a stretch of fence along the path, a ladder left in a tree
    ox, oy = ORCHARD
    put("picnic_table", ox + 6.0, oy + 10.0, 200.0)
    for i in range(6):
        put("beehive", ox - 30.0 + i * 2.3, oy - 22.0 + 0.3 * i + rnd.uniform(-0.2, 0.2), 265.0 + rnd.uniform(-6.0, 6.0), i % 2)
    sp = T["spring"]
    ks = [k for k in range(len(sp)) if abs(sp[k][0] - ox) < 60.0]
    for n, k in enumerate(ks[::2][:14]):
        x, y = sp[k]
        h = _dir(sp, k)
        a = math.radians(h - 90.0)
        if n % 5 != 3:                                         # a gate's width left open
            put("wattle_fence", x + 3.4 * math.cos(a), y + 3.4 * math.sin(a), _axis_yaw(h), 1 if n % 4 == 2 else 0)
    put("orchard_ladder", ox - 14.0, oy + 2.0, 40.0)
    put("scarecrow", ox + 30.0, oy - 12.0, 300.0)
    # haystacks on the meadow between the shoulder and the orchard
    for x, y, v in ((-236.0, 238.0, 0), (-262.0, 214.0, 0), (-228.0, 196.0, 1), (-286.0, 176.0, 0), (-204.0, 226.0, 1)):
        put("hay_stack", x, y, rnd.uniform(0.0, 360.0), v)
    # the cliff walk: the bench on the edge
    lx, ly = cliff_lookout()
    put("brow_bench", lx, ly, 0.0, kit="north")
    # landmarks: the lighthouse past the Mayak station (its door to the track), the pillbox and the split stone on the
    # cliff walk, the wreck on the beach (its keel along the shore), the Devil's stone on the meadow
    put("lighthouse", LIGHT[0], LIGHT[1], 140.0)
    put("park_bench", LIGHT[0] + 7.0, LIGHT[1] + 9.0, 0.0, 1)
    put("bunker", BUNKER[0], BUNKER[1], 0.0)
    put("erratic", SPLIT_STONE[0], SPLIT_STONE[1], 35.0, 1)
    put("shipwreck", WRECK[0], WRECK[1], 12.0)
    put("driftwood", WRECK[0] + 5.0, WRECK[1] - 7.0, 80.0, 1, kit="north")
    put("erratic", STONE[0], STONE[1], 250.0, 0)
    # the halt's shelter on the bank by the stair's foot, open to the sea
    put("halt_shelter", HALT[0], HALT[1], 0.0)
    put("park_lamp", HALT[0] - 1.0, HALT[1] + 5.0, 0.0, 0)
    put("litter_urn", HALT[0] + 0.5, HALT[1] - 3.2, 0.0)
    # the alley from the club's door to the station (world_map.alley_trees: pairs every 13 m, 4.2 m off the middle):
    # lamps in the trees' line between the pairs, taking sides in turn; benches facing the path, an urn by each
    alley = W.path_points(0.5)

    def at(y, off):
        cx, cy = min(alley, key=lambda q: abs(q[1] - y))
        return cx + off, cy

    for i, y in enumerate((24.5, 37.5, 50.5, 63.5, 76.5, 89.5, 102.5)):
        put("park_lamp", *at(y, 4.3 if i % 2 == 0 else -4.3), 180.0 if i % 2 == 0 else 0.0, 0)
    for i, y in enumerate((24.5, 37.5, 76.5, 89.5)):          # across the path from that spot's lamp, between the trees
        side = -1.0 if i % 2 == 0 else 1.0
        bx_, by_ = at(y, side * 3.5)
        put("park_bench", bx_, by_, 0.0 if side < 0 else 180.0, i % 2)
        put("litter_urn", bx_, by_ + 1.7, 0.0)
    for side in (-1.0, 1.0):
        put("flower_bed", *at(96.0, side * 3.6), 0.0)
    # the junction at the alley's end: a two-globe lamp, water, the bicycles
    put("park_lamp", ALLEY_END[0] + 9.2, ALLEY_END[1] - 1.2, 200.0, 1)
    put("drinking_fountain", ALLEY_END[0] + 11.4, ALLEY_END[1] - 4.2, 200.0)
    put("bike_rack", ALLEY_END[0] - 8.5, ALLEY_END[1] - 4.5, 80.0)
    # the lookout and the stair's top: a lamp and a bench each
    put("park_lamp", BELVEDERE[0] - 5.5, BELVEDERE[1] - 6.0, 60.0, 1)
    put("park_bench", BELVEDERE[0] + 8.0, BELVEDERE[1] - 2.5, 90.0, 0)
    put("park_lamp", tx - 6.0, ty - 5.0, 0.0, 0)
    return out


def stair_profile():
    """The macro ground under the cliff stair for blender/south (stair_profile.json)."""
    (tx, ty), (fx, fy) = stair_line()
    z0 = W.height(tx, ty)
    n = int((fx - tx) / 0.5) + 8
    return {"comment": "Written by tools/south.py: the macro ground under the cliff stair, [metres out from the top "
                       "(toward the sea), ground height relative to the top]. The game's ground is kept to this exactly "
                       "in a corridor 10 m wide (south.keep_flat).",
            "top": [round(tx, 2), ty], "foot": [round(fx, 2), fy], "drop": round(z0 - W.height(fx, fy), 3),
            "profile": [[round(i * 0.5, 2), round(W.height(tx + i * 0.5, ty) - z0, 3)] for i in range(n)]}


def build():
    bx, by, _ = bridge()
    return {
        "frame": "map metres (docs/world_map.md): x west, y north; heights from the built ground (terrain.ground_z) "
                 "unless a thing has its own z; yaw: Unreal degrees of a thing's front (map +X is 0, +Y is 90)",
        "places": places(),
        "things": things(),
        "brook": {"water": brook_water(), "mouth": [round(v, 1) for v in _course()["pts"][-1]]},
        "trails": {k: [[round(x, 1), round(y, 1)] for x, y in v[::2]] for k, v in trails().items()},
        "bridge": {"x": round(bx, 2), "y": round(by, 2)},
        "stair": stair_profile(),
    }


def write():
    data = build()
    tps = [{"id": p["id"], "group": "south", "name": "dev.tp." + p["id"], "x": p["x"], "y": p["y"], "yaw": p["yaw"]}
           for p in data["places"]]
    for path, obj in ((OUT, data), (OUT_TELEPORTS, {"teleports": tps})):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(obj, f, indent=1, ensure_ascii=False)
            f.write("\n")
    return data


def check():
    """The land's own rules. Returns the list of problems (empty: fine)."""
    bad = []
    for name, pts in trails().items():
        worst = 0.0
        for p, q in zip(pts, pts[1:]):
            run = math.hypot(q[0] - p[0], q[1] - p[1])
            if run > 0.5:
                worst = max(worst, abs(W.height(*q) - W.height(*p)) / run)
        deg = math.degrees(math.atan(worst))
        print("trail %-10s %4.0f m, steepest %4.1f deg" % (name, _lengths(pts)[-1], deg))
        # the sea trail ends on the brow; the town trail's one step is the brook's bank under the footbridge
        if deg > (40.0 if name == "town" else 17.0):
            bad.append("trail %s is %.1f degrees at its steepest" % (name, deg))
    c = _course()
    for k in range(1, len(c["bed"])):
        if c["bed"][k] > c["bed"][k - 1]:
            bad.append("the brook runs uphill at %s" % (c["pts"][k],))
            break
    for k, p in enumerate(c["pts"]):
        if abs(W.height(*p) - c["bed"][k]) > 0.6 and W.inland(*p) > 60.0:
            bad.append("the brook's bed is not cut at %s: ground %.2f, bed %.2f" % (p, W.height(*p), c["bed"][k]))
            break
    for (x, y) in W.cable_car()["towers"]:
        b = brook_at(x, y)
        if b is not None and b[0] < 7.0:
            bad.append("a cable car pylon stands %.1f m from the brook at (%.0f, %.0f)" % (b[0], x, y))
        if _near_trail(x, y, 4.0):
            bad.append("a cable car pylon stands on a trail at (%.0f, %.0f)" % (x, y))
    for sx, sy in W.cable_car()["stations"]:
        for t in things():
            if abs(t["x"] - sx) < 9.5 and abs(t["y"] - sy) < 9.5:
                bad.append("%s stands in a cable car station's footprint at (%.1f, %.1f)" % (t["id"], t["x"], t["y"]))
    for sx, sy in (W.STATION_UP,):
        if abs(W.height(sx, sy)) > 0.01 or abs(W.height(sx + 20.0, sy + 8.0)) > 0.01:
            bad.append("the Aton station's ground is not level")
    if abs(W.height(W.DOOR_X, 3.0)) > 0.01:
        bad.append("the forecourt moved")
    return bad


if __name__ == "__main__":
    if "--check" in sys.argv:
        problems = check()
        for p in problems:
            print("PROBLEM", p)
        sys.exit(1 if problems else 0)
    d = write()
    print("south: %d places, %d things, %d water samples" % (len(d["places"]), len(d["things"]), len(d["brook"]["water"])))
