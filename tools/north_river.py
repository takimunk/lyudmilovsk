"""The river of the North (река): a braided gravel river like Kamchatka's (the Avacha above its mouth), in a wide open
valley between the town's woods and volcano country. It is born as seeps and springs at the foot of the Topi's dune on
the map's east edge, gathers into channels that part and join round gravel bars and alder islands, and reaches the sea
through a gap in the cliffs. Pure Python; tools/north.py hooks it into the land (apply), the masks (gravel), the woods
(island) and north.json (water, threads, bridges). docs/north.md.

Frame: map metres (X west, Y north). The river runs from east to west: s = metres along its middle line from the
east edge, v = metres off it (positive on the north side).
"""
import bisect
import functools
import math

import north_land as L

SEA_Z = -43.0
# the valley's middle line, east to west (X must grow all the way)
COURSE = [(-1330.0, 2050.0), (-1185.0, 2040.0), (-1010.0, 2085.0), (-800.0, 2030.0), (-590.0, 1985.0), (-360.0, 2040.0),
          (-110.0, 2030.0), (120.0, 1975.0), (300.0, 1945.0), (400.0, 1935.0), (520.0, 1930.0)]
SOURCE_X = -1185.0              # the seeps under the dune: no open water east of this
MOUTH_X = 330.0                 # about where the water line is
FLOOR = (-15.0, -22.5, -42.6)   # the valley floor at the east edge, at the source, at the mouth
HALF = [(0.0, 26.0), (0.18, 62.0), (0.45, 100.0), (0.75, 128.0), (1.0, 185.0)]   # the gravel plain's half width down the river
WATER = -0.3                    # the water sheet under the floor's datum: channels dip below it, bars stand over it
BANK = (8.0, 2.2)               # the cut bank at the plain's edge: metres wide, metres high
TERRACE = (30.0, 60.0)          # the low terrace behind it (old flood plain): its width wanders between these
BLUFF = (0.4, 0.52)             # the south side's slope (the river undercuts it: 22..27 degrees)
SIDE = (0.2, 0.3)               # the north side's slope wanders between these (11..17 degrees)
# the channels: (how far across the plain it swings 0..1, long wave m, short wave m, phase, half width m, depth m)
THREADS = [
    (0.50, 330.0, 97.0, 0.3, 7.5, 1.35),       # the main stream
    (0.85, 250.0, 71.0, 2.1, 4.5, 0.95),       # the second one
]
BRAID = (190.0, 34.0, 0.085, 0.7)               # the braids: a bar's length and width m, the channels' share, depth m


def _smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


@functools.lru_cache(maxsize=1)
def line():
    """The middle line sampled every ~8 m: lists xs, ys, ss (metres along)."""
    pts = COURSE
    xs, ys = [], []
    for i in range(len(pts) - 1):
        p0, p1, p2, p3 = pts[max(i - 1, 0)], pts[i], pts[i + 1], pts[min(i + 2, len(pts) - 1)]
        n = max(2, int(math.hypot(p2[0] - p1[0], p2[1] - p1[1]) / 8.0))
        for k in range(n):
            t = k / n
            f = lambda a, b, c, d: 0.5 * (2 * b + (c - a) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (-a + 3 * b - 3 * c + d) * t ** 3)
            xs.append(f(p0[0], p1[0], p2[0], p3[0]))
            ys.append(f(p0[1], p1[1], p2[1], p3[1]))
    xs.append(pts[-1][0])
    ys.append(pts[-1][1])
    ss = [0.0]
    for k in range(1, len(xs)):
        ss.append(ss[-1] + math.hypot(xs[k] - xs[k - 1], ys[k] - ys[k - 1]))
    assert all(b > a for a, b in zip(xs, xs[1:])), "the river's course must run west all the way"
    return xs, ys, ss


@functools.lru_cache(maxsize=1)
def _cos():
    """Per point of the middle line: the cosine of its angle to the X axis (what turns metres along Y into metres
    across the valley)."""
    xs, ys, _ = line()
    out = []
    for i in range(len(xs)):
        a, b = max(0, i - 1), min(len(xs) - 1, i + 1)
        out.append((xs[b] - xs[a]) / math.hypot(xs[b] - xs[a], ys[b] - ys[a]))
    return out


def _seat(x):
    xs, _, _ = line()
    k = min(len(xs) - 2, max(0, bisect.bisect_right(xs, x) - 1))
    return k, min(1.0, max(0.0, (x - xs[k]) / (xs[k + 1] - xs[k])))


def locate(x, y):
    """(s, v) of a map point: metres along the middle line and off it (north positive). The line runs west all the
    way, so a point belongs to the line's point of the same X (never a jump, however far off the point lies)."""
    xs, ys, ss = line()
    cs = _cos()
    k, t = _seat(x)
    return ss[k] + (ss[k + 1] - ss[k]) * t, (y - ys[k] - (ys[k + 1] - ys[k]) * t) * (cs[k] + (cs[k + 1] - cs[k]) * t)


def at(s):
    """(x, y, nx, ny) on the middle line at s: the point, and the step on the map that is a metre of v (north)."""
    xs, ys, ss = line()
    cs = _cos()
    i = min(len(ss) - 2, max(0, bisect.bisect_right(ss, s) - 1))
    t = (s - ss[i]) / (ss[i + 1] - ss[i])
    return xs[i] + (xs[i + 1] - xs[i]) * t, ys[i] + (ys[i + 1] - ys[i]) * t, 0.0, 1.0 / (cs[i] + (cs[i + 1] - cs[i]) * t)


@functools.lru_cache(maxsize=1)
def _marks():
    xs, ys, ss = line()
    s_src = ss[bisect.bisect_left(xs, SOURCE_X)]
    s_mouth = ss[bisect.bisect_left(xs, MOUTH_X)]
    return s_src, s_mouth, ss[-1]


def reach(s):
    """0 at the source .. 1 at the mouth."""
    s_src, s_mouth, _ = _marks()
    return (s - s_src) / (s_mouth - s_src)


def floor(s):
    """The gravel plain's datum height at s."""
    s_src, s_mouth, _ = _marks()
    if s <= s_src:
        return FLOOR[0] + (FLOOR[1] - FLOOR[0]) * _smooth(s / s_src)
    t = min(1.0, (s - s_src) / (s_mouth - s_src))
    return FLOOR[1] + (FLOOR[2] - FLOOR[1]) * t


def half(s):
    """Half the gravel plain's width at s."""
    t = reach(s)
    if t <= 0.0:
        return 26.0 - 10.0 * min(1.0, -t * 8.0)
    t = min(t, 1.0)
    for (t0, h0), (t1, h1) in zip(HALF, HALF[1:]):
        if t <= t1:
            h = h0 + (h1 - h0) * _smooth((t - t0) / (t1 - t0))
            break
    return h * (1.0 + 0.1 * math.sin(s / 140.0 + 1.0) + 0.05 * math.sin(s / 53.0))


def thread_v(k, s):
    """Where channel k runs at s (metres off the middle line)."""
    swing, l1, l2, ph, _, _ = THREADS[k]
    w = max(0.0, half(s) - THREADS[k][4] - 6.0)
    return w * swing * (0.68 * math.sin(s / l1 + ph) + 0.32 * math.sin(s / l2 + 2.3 * ph))


def _plain(s, v):
    """(relief over the datum, island 0..1, wet 0..1, metres the bed lies under the datum) on the gravel plain."""
    gather = _smooth((reach(s) + 0.02) / 0.1)            # at the source the channels are only starting
    h = half(s)
    cut, dmin = 0.0, 1e9
    for k, (_, _, _, _, w, depth) in enumerate(THREADS):
        d = abs(v - thread_v(k, s))
        dmin = min(dmin, d - w)
        cut = max(cut, depth * (0.35 + 0.65 * gather) * (1.0 - _smooth((d - 0.5 * w) / (1.1 * w))))
    # the braids: the lesser channels part and join round lens-shaped bars (the zero lines of a noise drawn out along
    # the flow), each a few metres wide and knee-deep
    n = L.vnoise(s / BRAID[0], v / BRAID[1], 44) + 0.45 * L.vnoise(s / (0.45 * BRAID[0]) + 9.0, v / (0.45 * BRAID[1]), 45)
    inside = _smooth((h - abs(v) - 4.0) / 14.0) * gather
    braid = (1.0 - _smooth(abs(n) / BRAID[2])) * inside
    cut = max(cut, BRAID[3] * braid)
    x, y, nx, ny = at(s)
    px, py = x + nx * v, y + ny * v
    lump = L.fbm(px, py, 34.0, 2, 41)
    high = _smooth((abs(n) - 0.22) / 0.3)                 # the bars' backs, away from any channel
    bar = 0.12 + 0.45 * high + 0.14 * lump
    # the islands: the broadest bars have stood dry for years: a metre higher, grass and alder on them
    isle = _smooth((dmin - 10.0) / 10.0) * _smooth((abs(n) - 0.42) / 0.2) * _smooth((L.fbm(px, py, 150.0, 2, 43) + 0.3) / 0.3) * inside
    bar += 0.95 * isle
    return bar - cut, isle, _smooth((cut - 0.25) / 0.2), cut


@functools.lru_cache(maxsize=1)
def _band():
    """Per 16 m of X: (lowest, highest Y) a point may have and still be within ~80 m of the gravel plain."""
    xs, ys, ss = line()
    out = {}
    for x, y, s in zip(xs, ys, ss):
        k = int(math.floor(x / 16.0))
        r = half(s) * 1.3 + 90.0
        for q in (k - 1, k, k + 1):
            a, b = out.get(q, (1e9, -1e9))
            out[q] = (min(a, y - r), max(b, y + r))
    return out


def near(x, y):
    """A cheap test: False when (x, y) is surely more than ~60 m outside the gravel plain."""
    b = _band().get(int(math.floor(x / 16.0)))
    return b is not None and b[0] < y < b[1]


@functools.lru_cache(maxsize=8192)
def info(x, y):
    """What the river makes of a map point, or None far from it: dict s, v, e (metres outside the gravel plain, <= 0
    on it), floor, plain relief, island, wet."""
    if not (1600.0 < y < 2760.0):
        return None
    s, v = locate(x, y)
    h = half(s)
    e = abs(v) - h
    if e > 600.0:
        return None
    out = {"s": s, "v": v, "e": e, "half": h, "floor": floor(s), "relief": 0.0, "island": 0.0, "wet": 0.0}
    if e < 4.0:
        r, isle, wet, _ = _plain(s, v)
        edge = _smooth((-e - 0.0) / 10.0)                 # the plain's rim: a strip of dry gravel under the bank
        out["relief"] = r * edge + 0.5 * (1.0 - edge)
        out["island"] = isle * edge
        out["wet"] = wet * edge
    return out


def apply(x, y, land):
    """The land's height with the valley cut into it: the gravel plain with its channels and bars, the cut bank, the
    low terrace, the valley sides rising to the land as it was."""
    i = info(x, y)
    if i is None:
        return land
    z0 = i["floor"]
    e = i["e"]
    if e <= 0.0:
        cut = z0 + i["relief"]
    else:
        px, py = x, y
        t_w = TERRACE[0] + (TERRACE[1] - TERRACE[0]) * (0.5 + 0.5 * L.fbm(px, py, 260.0, 2, 51))
        k = SIDE[0] + (SIDE[1] - SIDE[0]) * (0.5 + 0.5 * L.fbm(px, py, 330.0, 2, 52))
        if i["v"] < 0.0:                                  # the south side is the undercut one: a steep bluff, the
            t_w *= 0.35                                   # terrace under it only a strip
            k = BLUFF[0] + (BLUFF[1] - BLUFF[0]) * (0.5 + 0.5 * L.fbm(px, py, 330.0, 2, 52))
        bank = BANK[1] * _smooth(e / BANK[0])
        up = max(0.0, e - BANK[0] - t_w)
        # the side eases off the terrace and steepens: a foot slope, then the hill
        cut = z0 + 0.5 + bank + 0.025 * min(e, BANK[0] + t_w) + k * up * _smooth(up / 40.0 + 0.35) + 0.0008 * up * up + 1.2 * L.fbm(px, py, 60.0, 2, 53) * _smooth(up / 30.0)
    if cut >= land + 12.0:
        return land
    # a smooth minimum: a rounded brow where the valley side meets the land
    r = 5.0
    return -r * math.log(math.exp(-(cut - land) / r) + 1.0) + land if cut - land > -60.0 else cut


def gravel(x, y):
    """0..1: bare river gravel (bars, channel beds, the strip under the bank); less on the islands."""
    if not near(x, y):
        return 0.0
    i = info(x, y)
    if i is None or i["e"] > 3.0:
        return 0.0
    return _smooth((3.0 - i["e"]) / 4.0) * (1.0 - 0.8 * i["island"])


def protect(x, y):
    """0..1: where the built ground must keep this module's exact shape (the plain and its bank)."""
    if not near(x, y):
        return 0.0
    i = info(x, y)
    if i is None:
        return 0.0
    return _smooth((BANK[0] + 6.0 - i["e"]) / 6.0)


def wet(x, y):
    """0..1: under the river's water (the channels' beds)."""
    if not near(x, y):
        return 0.0
    i = info(x, y)
    return i["wet"] if i is not None else 0.0


def outlet(x, y):
    """The main stream's bed where it breaks through the beach's berm to the sea (world_map._height takes the lower
    of this and the beach): a height, 1e9 away from the stream."""
    if x < MOUTH_X - 90.0 or not (1700.0 < y < 2150.0):
        return 1e9
    s, v = locate(x, y)
    d = abs(v - thread_v(0, s))
    w = THREADS[0][4] + 2.0
    if d > 3.0 * w:
        return 1e9
    bed = floor(s) - THREADS[0][5] * 0.8
    return bed + 2.2 * _smooth((d - 0.6 * w) / (1.6 * w)) + (1e9 if d > 2.2 * w else 0.0)


def mouth():
    """(x, y) where the main stream meets the water line."""
    _, s_mouth, _ = _marks()
    x, y, nx, ny = at(s_mouth)
    v = thread_v(0, s_mouth)
    return x + nx * v, y + ny * v


def water():
    """The water sheet for the game: [x, y, z, half width] every ~15 m from the source to the mouth (one ribbon mesh,
    flat across; the ground's bars rise through it)."""
    s_src, s_mouth, s_end = _marks()
    out, s = [], s_src + 6.0
    while s < s_mouth + 45.0:
        x, y, _, _ = at(s)
        out.append([round(x, 1), round(y, 1), round(max(floor(s) + WATER, SEA_Z + 0.25), 2), round(half(s) + 1.0, 1)])
        s += 15.0
    return out


def threads():
    """The channels' middle lines as polylines of [x, y] (for sound, foam, bridges)."""
    s_src, s_mouth, _ = _marks()
    out = []
    for k in range(len(THREADS)):
        pts, s = [], s_src + 30.0
        while s < s_mouth + 20.0:
            x, y, nx, ny = at(s)
            v = thread_v(k, s)
            pts.append([round(x + nx * v, 1), round(y + ny * v, 1)])
            s += 12.0
        out.append(pts)
    return out


def crossing(x_at):
    """A straight way across the plain where the middle line passes X = x_at: (south end, north end, bridges), the
    ends on the terraces, bridges = [(x, y, yaw degrees of the way across, span metres, deck z)] one per channel."""
    xs, ys, ss = line()
    s = ss[bisect.bisect_left(xs, x_at)]
    x, y, nx, ny = at(s)
    h = half(s)
    ends = ((x, y - ny * (h + 14.0)), (x, y + ny * (h + 14.0)))
    out, v, start = [], -h, None
    while v <= h:
        wet = _plain(s, v)[3] > 0.6                       # the knee-deep braids are waded
        if wet and start is None:
            start = v
        if (not wet or v + 0.5 > h) and start is not None:
            mid, span = (start + v) / 2, v - start
            out.append((x, y + ny * mid, 90.0, span * ny, floor(s) + 0.95))
            start = None
        v += 0.5
    return ends[0], ends[1], out
