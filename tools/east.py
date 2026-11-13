"""The East: the land between the town and Topi (docs/east.md), south of the North (Y < 1450).

Behind the town's upper terrace the ground used to be a dead level plain under square forest tiles, ending in a
straight ramp at the map's edge. Here it is an old dune country instead: a wooded scarp behind the town, then
fossil dunes under pines (long ridges lying north to south, heath in the slacks between them), lower and barer
toward the east until the living sand of Topi (tools/topi.py, docs/topi.md) comes over the last of them. A pass
through the ridges on the town's main axis (Y ~905) carries the track to Topi.

world_map uses three things from here:
  lift(x, y, low)   the valley's height with the dune country on it (world_map.valley)
  claims(x, y)      is the forest's cover here this module's? (world_map.forest_cover)
  wood(x, y)        0..1: the share of a forest tile's trees that stand here

  python3 tools/east.py            -> Saved/East/east.png (relief, woods, the track's pass), numbers
"""
import math

X_EDGE = -1300.0          # the map's east edge (world_map.X_MIN)
Y_NORTH = 1450.0          # north of it the land is the North's (tools/north.py)
Y_FADE = 80.0             # the dune country lets go of the ground over this much before Y_NORTH
PASS_Y = 905.0            # the town's main axis: the pass and the track to Topi
PLATEAU_Z = -7.0          # the old dunes stand on this (the town's upper terrace is -8)
SAND_FROM, SAND_TO = -1100.0, -1200.0     # the trees thin out from here to nothing: Topi's sand begins


def _smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def _hash(i, j, seed):
    h = (i * 73856093 ^ j * 19349663 ^ seed * 83492791) & 0xffffffff
    h = ((h ^ (h >> 13)) * 0x5bd1e995) & 0xffffffff
    h ^= h >> 15
    return (h & 0xffffff) / float(0xffffff)


def noise(x, y, seed):
    """Smooth value noise 0..1."""
    fx, fy = math.floor(x), math.floor(y)
    i, j = int(fx), int(fy)
    tx, ty = x - fx, y - fy
    tx = tx * tx * (3 - 2 * tx)
    ty = ty * ty * (3 - 2 * ty)
    a, b = _hash(i, j, seed), _hash(i + 1, j, seed)
    c, d = _hash(i, j + 1, seed), _hash(i + 1, j + 1, seed)
    return (a + (b - a) * tx) * (1 - ty) + (c + (d - c) * tx) * ty


_BACK = {}
_SOFT = {}


def _town_back(y):
    """X of the back of the town's upper wall (W5) at Y; south and north of the town, as at its ends."""
    k = int(round(min(1450.0, max(380.0, y)) / 10.0))
    if k not in _BACK:
        import city_plan as C
        import world_map as W
        yy = k * 10.0
        _BACK[k] = W.shore_x(yy) - C.wall_s(len(C.WALLS) - 1, yy) - C.WALL_DEPTH
    return _BACK[k]


def _soft_back(i):
    """The wall's line at Y = 10 i, smoothed along Y (it jumps ~180 m at the Cascade's bend)."""
    if i not in _SOFT:
        w = [math.exp(-(n / 9.0) ** 2) for n in range(-18, 19)]
        _SOFT[i] = sum(wn * min(_town_back((i + n) * 10.0), -700.0) for n, wn in zip(range(-18, 19), w)) / sum(w)
    return _SOFT[i]


def scarp_x(y):
    """X of the foot of the scarp behind the town: a wavy line that keeps clear of the upper terrace's plots
    (they reach ~130 m behind the wall)."""
    t = y / 10.0
    i = math.floor(t)
    back = _soft_back(i) + (_soft_back(i + 1) - _soft_back(i)) * (t - i)
    return back - 195.0 + 38.0 * math.sin(y / 210.0 + 0.4) + 14.0 * math.sin(y / 83.0 + 1.9)


def pass_y(x):
    """Y of the pass's middle line at X: it winds a little on its way east."""
    return PASS_Y + 26.0 * math.sin((x + 700.0) / 95.0) * _smooth((-x - 760.0) / 120.0) * (1.0 - _smooth((-x - 1120.0) / 60.0))


def dunes(x, y):
    """The fossil dunes over their plateau, m (0 in the slacks .. ~13 on the highest crest), and 0..1 how much of
    a ridge this is (for the woods)."""
    # the wind that laid them blew from the sea: ridges run north to south, bent into bows
    wx = x + 150.0 * (noise(x / 420.0, y / 300.0, 5) - 0.5) + 40.0 * (noise(x / 130.0, y / 110.0, 6) - 0.5)
    big = 0.5 + 0.5 * math.sin(wx / 27.0 + 3.0 * (noise(y / 380.0, 0.0, 7) - 0.5))
    big = big ** 1.6                                             # broad slacks, narrower crests
    swell = 0.45 + 0.55 * noise(x / 260.0, y / 330.0, 8)        # every ridge swells and dies along its length
    low = noise(x / 85.0, y / 110.0, 9)
    ridge = big * swell
    return 10.5 * ridge + 2.6 * low * (0.4 + 0.6 * ridge), ridge


def _pass(x, y):
    """0..1: in the pass (the dunes stand back from the track)."""
    d = (y - pass_y(x)) / 42.0
    return math.exp(-d * d)


def old_ridge(x, low):
    """The straight ramp the map's edge used to have (still there north of Y_NORTH until the North reshapes it)."""
    return low + ((-x - 800.0) / 500.0) ** 2 * 30.0 if x < -800.0 else low


def lift(x, y, low):
    """The valley's height with the dune country: low is the valley floor's own height (world_map.valley)."""
    if x > -700.0:
        return low
    old = old_ridge(x, low)
    ky = _smooth((Y_NORTH - y) / Y_FADE)
    kx = _smooth((scarp_x(y) - x) / 110.0)
    if ky <= 0.0:
        return old
    d, _ = dunes(x, y)
    east = _smooth((-x - 900.0) / 380.0)
    plateau = PLATEAU_Z + 5.0 * east
    d *= (0.8 + 0.2 * east) * (1.0 - 0.8 * _pass(x, y))
    new = low + (plateau + d - low) * kx
    return old + (new - old) * ky


def claims(x, y):
    return y <= Y_NORTH


def _glades(x, y):
    """0..1: 0 in a glade. Irregular openings with soft, lobed edges: a few big, more small."""
    wx = x + 60.0 * (noise(x / 90.0, y / 90.0, 31) - 0.5)
    wy = y + 60.0 * (noise(x / 90.0 + 5.0, y / 90.0, 32) - 0.5)
    g = 0.65 * noise(wx / 170.0 + 11.0, wy / 150.0, 21) + 0.35 * noise(wx / 48.0, wy / 52.0 + 7.0, 22)
    return _smooth((g - 0.31) / 0.1)


def wood(x, y):
    """0..1: the share of the forest's trees that stand here."""
    if y > Y_NORTH:
        return 1.0
    cover = _glades(x, y)
    k = _smooth((y - 400.0) / 120.0) if x <= -640.0 else 0.0
    if k > 0.0:
        # the dune country: pines on the ridges, heath in the slacks, the pass kept open
        _, ridge = dunes(x, y)
        country = (0.22 + 0.78 * _smooth((ridge - 0.04) / 0.2)) * (1.0 - 0.9 * _pass(x, y))
        # the forest's front toward the town: a lobed edge with lone trees ahead of it
        front = scarp_x(y) + 45.0 + 90.0 * (noise(y / 75.0, x / 260.0, 23) - 0.5) + 40.0 * (noise(y / 28.0, x / 90.0, 25) - 0.5)
        country *= 0.06 * _smooth((front + 90.0 - x) / 40.0) + 0.94 * _smooth((front - x) / 50.0)
        cover *= 1.0 + (country - 1.0) * k
    # toward Topi the trees thin out and stop: the sand has them
    line = 50.0 * (noise(y / 90.0, 3.0, 24) - 0.5)
    cover *= 1.0 - _smooth((SAND_FROM + line - x) / (SAND_FROM - SAND_TO))
    return max(0.0, min(1.0, cover))


if __name__ == "__main__":
    import os
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import world_map as W
    from PIL import Image

    step = 2.0
    x_w, x_e, y_s, y_n = -560.0, X_EDGE - 60.0, 250.0, 1550.0
    nx, ny = int((x_w - x_e) / step), int((y_n - y_s) / step)
    hs = [[W.height(x_w - i * step, y_n - j * step) for i in range(nx + 1)] for j in range(ny + 1)]
    tiles = [t for t in W.forests() if t[0] - t[2] < x_w and t[1] + t[3] > y_s and t[1] - t[3] < y_n]
    im = Image.new("RGB", (nx, ny))
    px = im.load()
    zs = []
    for j in range(ny):
        for i in range(nx):
            x, y = x_w - i * step, y_n - j * step
            z = hs[j][i]
            hx = (hs[j][i + 1] - z) / -step
            hy = (hs[j + 1][i] - z) / -step
            lit = min(1.35, max(0.2, (-hx * 1.6 + hy * 1.2 + 0.8) / math.sqrt(hx * hx + hy * hy + 1.0) / 0.8))
            # trees: the tiles' own density (trees per 25 m2) times the cover, drawn as dots
            dens = sum(t[5] / (4.0 * t[2] * t[3]) * 25.0 for t in tiles
                       if abs(x - t[0]) <= t[2] and abs(y - t[1]) <= t[3] and t[7] <= z <= t[8])
            c = min(1.0, dens) * (wood(x, y) if claims(x, y) else 1.0)
            tree = _hash(i, j, 77) < c * 0.75
            ground = (150, 160, 104) if z > -40.0 else (120, 150, 170)
            if x < SAND_TO - 40.0:
                ground = (222, 204, 160)                 # Topi's sand comes over the ground here (tools/topi.py)
            if abs(y - pass_y(x)) < 2.0 and -1180.0 < x < -700.0:
                ground = (200, 120, 70)                  # the pass's middle line
            r, g, b = (44, 74, 44) if tree else ground
            px[i, j] = (int(min(255, r * lit)), int(min(255, g * lit)), int(min(255, b * lit)))
            if x < -900.0 and 300.0 < y < 1400.0:
                zs.append(z)
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Saved", "East")
    os.makedirs(out, exist_ok=True)
    im = im.resize((nx * 2, ny * 2), Image.LANCZOS)
    im.save(os.path.join(out, "east.png"))
    print("wrote Saved/East/east.png %d x %d; dune country z %.1f..%.1f, mean %.1f" % (nx, ny, min(zs), max(zs), sum(zs) / len(zs)))
