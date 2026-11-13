"""The North's land, taken from a real place: the cinder cones of the Northern Breakthrough of the Great Tolbachik
Fissure Eruption (Kamchatka, 1975) and the lava plain west of them, at half size (2 real metres per map metre, heights
halved too, so every slope is the real one). tools/north_dem/make.py fetched and prepared the heights
(tools/north_dem/dem.i16 + dem.json); this module reads them with the standard library only and derives what the rest
needs: the height anywhere, how far a spot stands over its surroundings (the cones), where summer snow would lie
(shaded hollows), where the lava field lies (rough ground). docs/north.md.
"""
import array
import functools
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEM = os.path.join(HERE, "north_dem")


class Grid:
    """A rectangle of values over the map: rows from y0 north, each from x0 toward +X."""

    def __init__(self, x0, y0, step, nx, ny, v):
        self.x0, self.y0, self.step, self.nx, self.ny, self.v = x0, y0, step, nx, ny, v

    def like(self, v):
        return Grid(self.x0, self.y0, self.step, self.nx, self.ny, v)

    def cell(self, x, y):
        i = min(self.nx - 1, max(0, int(round((x - self.x0) / self.step))))
        j = min(self.ny - 1, max(0, int(round((y - self.y0) / self.step))))
        return i, j

    def xy(self, i, j):
        return self.x0 + i * self.step, self.y0 + j * self.step

    def at(self, x, y):
        """Bilinear."""
        fx = min(self.nx - 1.001, max(0.0, (x - self.x0) / self.step))
        fy = min(self.ny - 1.001, max(0.0, (y - self.y0) / self.step))
        i, j = int(fx), int(fy)
        tx, ty = fx - i, fy - j
        v, k = self.v, j * self.nx + i
        return (v[k] * (1 - tx) + v[k + 1] * tx) * (1 - ty) + (v[k + self.nx] * (1 - tx) + v[k + self.nx + 1] * tx) * ty

    def smooth(self, x, y):
        """Bicubic (Catmull-Rom): no creases between cells."""
        fx = min(self.nx - 2.001, max(1.0, (x - self.x0) / self.step))
        fy = min(self.ny - 2.001, max(1.0, (y - self.y0) / self.step))
        i, j = int(fx), int(fy)
        tx, ty = fx - i, fy - j
        wx = (((-tx + 2) * tx - 1) * tx / 2, ((3 * tx - 5) * tx * tx + 2) / 2, ((-3 * tx + 4) * tx + 1) * tx / 2,
              (tx - 1) * tx * tx / 2)
        wy = (((-ty + 2) * ty - 1) * ty / 2, ((3 * ty - 5) * ty * ty + 2) / 2, ((-3 * ty + 4) * ty + 1) * ty / 2,
              (ty - 1) * ty * ty / 2)
        v, out = self.v, 0.0
        for b in range(4):
            k = (j + b - 1) * self.nx + i - 1
            out += wy[b] * (wx[0] * v[k] + wx[1] * v[k + 1] + wx[2] * v[k + 2] + wx[3] * v[k + 3])
        return out

    def blurred(self, radius):
        """A box blur run three times (close to a Gaussian), radius in map metres."""
        r = max(1, int(round(radius / self.step)))
        v, nx, ny = list(self.v), self.nx, self.ny
        for _ in range(3):
            out = [0.0] * len(v)
            for j in range(ny):                       # along the rows
                row = v[j * nx:(j + 1) * nx]
                acc = [0.0]
                for q in row:
                    acc.append(acc[-1] + q)
                for i in range(nx):
                    a, b = max(0, i - r), min(nx, i + r + 1)
                    out[j * nx + i] = (acc[b] - acc[a]) / (b - a)
            v = out
            out = [0.0] * len(v)
            for i in range(nx):                       # along the columns
                acc = [0.0]
                for j in range(ny):
                    acc.append(acc[-1] + v[j * nx + i])
                for j in range(ny):
                    a, b = max(0, j - r), min(ny, j + r + 1)
                    out[j * nx + i] = (acc[b] - acc[a]) / (b - a)
            v = out
        return self.like(v)


@functools.lru_cache(maxsize=1)
def heights():
    """The prepared heights (metres over the map's zero)."""
    with open(os.path.join(DEM, "dem.json")) as f:
        m = json.load(f)
    a = array.array("h")
    with open(os.path.join(DEM, "dem.i16"), "rb") as f:
        a.frombytes(f.read())
    if sys.byteorder != "little":
        a.byteswap()
    assert len(a) == m["nx"] * m["ny"], "tools/north_dem/dem.i16 does not match dem.json"
    return Grid(m["x0"], m["y0"], m["step"], m["nx"], m["ny"], [q * m["unit"] for q in a])


def z(x, y):
    return heights().smooth(x, y)


@functools.lru_cache(maxsize=1)
def relief():
    """Metres a spot stands over (or under) the land 300 m around: the cones are what stands high."""
    h = heights()
    b = h.blurred(300.0)
    return h.like([p - q for p, q in zip(h.v, b.v)])


def slope(x, y, d=10.0):
    """(dz/dx, dz/dy) of the real land."""
    return (z(x + d, y) - z(x - d, y)) / (2 * d), (z(x, y + d) - z(x, y - d)) / (2 * d)


@functools.lru_cache(maxsize=None)
def snow(min_z, amount):
    """0..1 where a summer snowfield lies: lee hollows and shaded north-facing slopes above min_z, the way they do on
    Kamchatka's cones in July. amount = the share of the land above min_z that keeps snow."""
    h = heights()
    soft = h.blurred(14.0)
    wide = h.blurred(60.0)
    nx, ny, st = h.nx, h.ny, h.step
    score = [-9.0] * len(h.v)
    for j in range(2, ny - 2):
        for i in range(2, nx - 2):
            k = j * nx + i
            if h.v[k] < min_z:
                continue
            gx = (soft.v[k + 1] - soft.v[k - 1]) / (2 * st)
            gy = (soft.v[k + nx] - soft.v[k - nx]) / (2 * st)
            g = math.hypot(gx, gy)
            north = -gy / (g + 0.05)                      # 1 where the slope faces north (falls toward +Y)
            lee = gx / (g + 0.05)                         # the sea wind blows from +X: snow drops behind what faces -X
            steep = min(1.0, g / 0.25) * (1.0 if g < 0.62 else max(0.0, 1.0 - (g - 0.62) / 0.15))   # none on the steepest
            hollow = min(1.5, max(-1.5, (wide.v[k] - soft.v[k]) / 1.4))
            score[k] = hollow + steep * (0.6 * north + 0.25 * lee) + 0.004 * (h.v[k] - min_z)
    live = sorted(s for s in score if s > -9.0)
    if not live:
        return h.like([0.0] * len(h.v))
    cut = live[int(len(live) * (1.0 - amount))]
    out = h.like([min(1.0, max(0.0, (s - cut) / 0.35)) for s in score])
    return out.blurred(6.0)


@functools.lru_cache(maxsize=1)
def rough():
    """Metres of small relief (what the land does within 40 m, averaged over 25 m): the lava field's levees, channels
    and collapsed crusts are rough, the ash plain and the meadows smooth."""
    h = heights()
    b = h.blurred(40.0)
    return h.like([abs(p - q) for p, q in zip(h.v, b.v)]).blurred(25.0)
