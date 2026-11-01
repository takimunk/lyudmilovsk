"""Europa's ground round the far hatch (Dima, 2026-11-01: a far more interesting landscape: cliffs, scarps, complex
relief, beautiful from eye level). Procedural, baked once, the same every time: every number is in moon_terrain.json.

    python3 tools/europa/moon_terrain.py [--preview DIR] [--only-preview]

  -> design/europa/game/moon_terrain.glb   (not committed: rebuilt from the json) the ground's tiles SM_EU_T_<level>_<i>_<j>,
                                           the far ring SM_EU_Horizon, boulders SM_EU_Boulders, the leads' water SM_EU_Water_<k>
  -> unreal/Lyudmilovsk/Content/Europa/moon.json   what the game reads: the tiles, the water levels and outlines, the site on
                                           Europa, the hatch's ground, a checksum of the heights (the bake pinned)

What it is made of (Europa as Galileo saw it, at a walker's scale):
- the ridged plains: a domain-warped ridged multifractal (sharp low crests and hummocks) from ~600 m swells down to a few
  dm, faded per mesh level below twice its spacing (no aliasing in the far tiles);
- lineae: dozens of double ridges (two crests, a trough between, steep inner faces) crossing each other, younger ones
  standing on the older (they flatten what was under them), a few cycloid chains (arcs meeting in cusps) and one big
  double ridge 70 m high across the side of the sky the planet hangs in;
- fault scarps: the ground stepped down in terraces with steep risers (cliffs) and tilted treads;
- chaos: the plains broken into rafts (Voronoi cells) that keep their old surface, lifted, tilted and drifted, with cliff
  sides, in a lower hummocky matrix; a giant chaos massif beyond the walk makes the skyline;
- leads: open cracks to the ocean, ice cliffs down to black water (the game puts the sea's water material there);
- talus: thermal erosion on a 4 m grid, kept mostly as fill (scree aprons under cliffs), little as cut, so cliffs stay;
- Europa's curve (1561 km): the horizon falls away as it should.
Vertex colours: R the brown salts (lineae troughs, margins, chaos matrix), G fresh exposed ice (risers, walls, raft
sides), B bright frost (crests and flats), A cavity (troughs and bases). The material (import_moon.py M_EU_Terrain) is
triplanar, so there are no UV seams or stretching on cliffs.
"""
import argparse
import hashlib
import json
import math
import os
import struct
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
PARAMS = os.path.join(HERE, "moon_terrain.json")
OUT_GLB = os.path.join(REPO, "design", "europa", "game", "moon_terrain.glb")
OUT_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Europa", "moon.json")

F32 = np.float32


# -- noise ---------------------------------------------------------------------------------------------------------

def _hash2(ix, iy, seed):
    with np.errstate(over="ignore"):
        return _hash2_(ix, iy, seed)


def _hash2_(ix, iy, seed):
    h = (ix.astype(np.uint32) * np.uint32(0x8DA6B343)) ^ (iy.astype(np.uint32) * np.uint32(0xD8163841)) ^ np.uint32((seed * 0x165667B1) & 0xFFFFFFFF)
    h = (h ^ (h >> np.uint32(15))) * np.uint32(0x2C1B3C6D)
    h = (h ^ (h >> np.uint32(12))) * np.uint32(0x297A2D39)
    h ^= h >> np.uint32(15)
    return h


def _u01(h):
    return (h & np.uint32(0xFFFFFF)).astype(F32) / F32(16777215.0)


def perlin(x, y, seed):
    """Gradient noise, about -1..1."""
    xi = np.floor(x)
    yi = np.floor(y)
    xf = (x - xi).astype(F32)
    yf = (y - yi).astype(F32)
    xi = xi.astype(np.int64)
    yi = yi.astype(np.int64)
    u = xf * xf * xf * (xf * (xf * 6 - 15) + 10)
    v = yf * yf * yf * (yf * (yf * 6 - 15) + 10)

    def g(dx, dy):
        a = _u01(_hash2(xi + dx, yi + dy, seed)) * F32(2 * math.pi)
        return np.cos(a) * (xf - dx) + np.sin(a) * (yf - dy)
    n00, n10, n01, n11 = g(0, 0), g(1, 0), g(0, 1), g(1, 1)
    return (n00 + u * (n10 - n00) + v * (n01 - n00) + u * v * (n00 - n10 - n01 + n11)) * F32(1.414)


def fade_for(wavelength, res):
    """How much of an octave of this wavelength a mesh of spacing res can carry (1 above 4 cells, 0 under 2)."""
    return min(1.0, max(0.0, (wavelength / max(res, 1e-6) - 2.0) / 2.0))


def fbm(x, y, wavelength, octaves, seed, res=0.0, gain=0.5, lac=2.0):
    out = np.zeros_like(x, dtype=F32)
    a, wl = 1.0, wavelength
    for o in range(octaves):
        k = fade_for(wl, res)
        if k <= 0:
            break
        out += F32(a * k) * perlin(x / wl, y / wl, seed + o * 101)
        a *= gain
        wl /= lac
    return out


def ridged(x, y, wavelength, octaves, seed, res=0.0, offset=1.0, gain=2.0, H=0.9, lac=2.03):
    """Musgrave's ridged multifractal, about 0..1: sharp crests where the noise crosses zero, the detail of each octave
    weighted by the last (rough on the crests, smooth in the hollows)."""
    out = np.zeros_like(x, dtype=F32)
    w = np.ones_like(x, dtype=F32)
    wl, norm = wavelength, 0.0
    for o in range(octaves):
        k = fade_for(wl, res)
        if k <= 0:
            break
        s = F32(offset) - np.abs(perlin(x / wl, y / wl, seed + o * 37))
        s = s * s * w
        amp = (wavelength / wl) ** (-H)
        out += s * F32(amp * k)
        norm += amp
        w = np.clip(s * F32(gain), 0, 1)
        wl /= lac
    return out / F32(max(norm, 1e-6))


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def bearing_dir(deg):
    b = math.radians(deg)
    return np.array([-math.sin(b), math.cos(b)])      # Unreal: east is -x, north +y


# -- the ground ----------------------------------------------------------------------------------------------------

class Ground:
    def __init__(self, P):
        self.P = P
        self.seed = P["seed"]
        rng = np.random.default_rng(self.seed)
        self.R = P["europa_radius_m"]
        # the planet's bearing from the site (tidally locked: fixed)
        lat, lon = math.radians(P["site"]["lat_deg"]), math.radians(P["site"]["lon_deg"])
        e, n, u = -math.sin(lon), -math.sin(lat) * math.cos(lon), math.cos(lat) * math.cos(lon)
        self.planet_bearing = math.degrees(math.atan2(e, n))
        self.planet_alt = math.degrees(math.asin(u))
        # lineae: random lines over the area, oldest first
        L = P["lineae"]
        self.lineae = []
        for i in range(L["count"]):
            c = rng.uniform(-L["extent"], L["extent"], 2)
            ang = rng.uniform(0, math.pi)
            # tidal stress turns with time: a slow drift of the strike from the oldest to the youngest
            ang = (ang * 0.35 + i / L["count"] * math.pi * 1.7) % math.pi
            h = rng.uniform(*L["height"]) * (0.5 + 0.5 * i / L["count"])
            self.lineae.append(dict(c=c, ang=ang, h=h, a=h * L["spacing_k"] + rng.uniform(1, 4),
                                    length=rng.uniform(*L["length"]), seed=int(rng.integers(1, 1 << 20)),
                                    wander=L["wander"], cyc=0.0, arc=0.0))
        for i in range(L["cycloids"]):
            c = rng.uniform(-L["extent"] * 0.6, L["extent"] * 0.6, 2)
            arc = rng.uniform(*L["cycloid_arc"])
            h = rng.uniform(3.0, 9.0)
            self.lineae.append(dict(c=c, ang=rng.uniform(0, math.pi), h=h, a=h * L["spacing_k"],
                                    length=arc * rng.integers(5, 12), seed=int(rng.integers(1, 1 << 20)),
                                    wander=0.01, cyc=arc * rng.uniform(0.18, 0.3), arc=arc))
        # keep the lineae's crests off the hatch's apron (move any that pass within 25 m)
        for li in self.lineae:
            d = np.array([-math.sin(li["ang"]), math.cos(li["ang"])])
            off = float(np.dot(-li["c"], d))
            if abs(off) < li["a"] + 25 and abs(float(np.dot(-li["c"], [math.cos(li["ang"]), math.sin(li["ang"])]))) < li["length"] / 2:
                li["c"] = li["c"] - d * (li["a"] + 30 - abs(off)) * (1 if off < 0 else -1)
        self.rift_levels = None

    # pieces ------------------------------------------------------------------------------------------------------
    def plains(self, x, y, res):
        p = self.P["plains"]
        s = self.seed
        wx = x + p["warp"] * fbm(x, y, p["warp_wavelength"], 4, s + 1, res)
        wy = y + p["warp"] * fbm(x + 517.0, y - 233.0, p["warp_wavelength"], 4, s + 2, res)
        r = ridged(wx, wy, p["wavelength"], p["octaves"], s + 3, res, p["ridged_offset"], p["ridged_gain"])
        h = F32(p["amp"]) * (r - F32(0.45))
        # broad swells under it, and the fine grain (hummocks, frost) the walker sees
        h += F32(p["amp"] * 0.8) * fbm(x, y, 2600.0, 3, s + 4, res)
        # hummocks: a second, finer ridged field (the plains' knobbly grain at a walker's scale)
        h += F32(p["hummock_amp"]) * (ridged(wx * 1.7 + 311.0, wy * 1.7, p["hummock_wavelength"], 5, s + 6, res) - F32(0.45))
        h += F32(p["fine_amp"]) * fbm(wx, wy, p["fine_wavelength"], p["fine_octaves"], s + 5, res)
        return h, r

    def ridge_profile(self, d, half, h, crest_w, trough):
        """A double ridge across signed distance d: two crests half apart, a trough between, steep inner faces and long
        outer flanks."""
        ad = np.abs(d)
        inner = ad < half
        t_in = np.where(inner, (half - ad) / (crest_w * 0.55), 0)
        t_out = np.where(inner, 0, (ad - half) / (crest_w * 1.6))
        crest = np.where(inner, np.exp(-t_in * t_in * 2.2), np.exp(-t_out * t_out))
        tr = -trough * np.exp(-(ad / (half * 0.42)) ** 2)
        return F32(h) * (crest + tr)

    def lineae_h(self, x, y, h, res, masks):
        for li in self.lineae:
            ca, sa = math.cos(li["ang"]), math.sin(li["ang"])
            dx, dy = x - li["c"][0], y - li["c"][1]
            along = dx * ca + dy * sa
            reach = li["length"] / 2 + li["a"] * 4
            sel = np.abs(along) < reach
            if not sel.any():
                continue
            across = -dx * sa + dy * ca
            sel &= np.abs(across) < li["a"] * 5 + 50
            if not sel.any():
                continue
            al, ac = along[sel], across[sel]
            ac = ac + F32(li["length"] * li["wander"]) * fbm(al, F32(0.0) * al + li["seed"] % 977, li["length"] * 0.35, 3, li["seed"])
            if li["cyc"] > 0:
                # a cycloid chain: arcs bowing out between cusps
                ac = ac - F32(li["cyc"]) * np.abs(np.sin(F32(math.pi) * al / F32(li["arc"])))
            ends = smooth(li["length"] / 2, li["length"] / 2 - li["a"] * 3 - 60, np.abs(al))
            hh = li["h"] * (0.75 + 0.5 * fbm(al, ac * 0 + 3.1, 420.0, 2, li["seed"] + 5) )
            ridge = self.ridge_profile(ac, li["a"], 1.0, max(li["a"] * 0.7, 3.0), 0.35) * hh * ends
            foot = np.exp(-(np.maximum(np.abs(ac) - li["a"], 0) / (li["a"] * 1.5 + 6)) ** 2) * ends
            # the younger ridge stands on what was there, flattening it
            sub = h[sel]
            h[sel] = sub * (1 - 0.55 * foot) + ridge
            masks["brown"][sel] = np.maximum(masks["brown"][sel], foot * np.exp(-(ac / (li["a"] * 0.6)) ** 2) * 0.9)
            masks["frost"][sel] += np.clip(ridge / max(li["h"], 1), 0, 1) * 0.35
        return h

    def big_ridge(self, x, y, h, masks):
        b = self.P["big_ridge"]
        d = bearing_dir(self.planet_bearing)
        n = np.array([-d[1], d[0]])
        c = d * b["dist"]
        across = (x - c[0]) * d[0] + (y - c[1]) * d[1]
        along = (x - c[0]) * n[0] + (y - c[1]) * n[1]
        across = across + F32(28.0) * fbm(along, across * 0, 1300.0, 3, self.seed + 40)
        ends = smooth(b["length"] / 2, b["length"] / 2 - 900, np.abs(along))
        hh = F32(b["height"]) * (0.8 + 0.4 * fbm(along, across * 0 + 7, 1900.0, 2, self.seed + 41))
        r = self.ridge_profile(across, b["half_sep"], 1.0, b["crest_width"], b["trough"]) * hh * ends
        # mass wasting: the flanks stepped into terraces (each a little riser and a tilted tread)
        k = b["terraces"]
        step = F32(b["height"]) / k
        q = r / step
        fl = np.floor(q)
        fr = q - fl
        terr = (fl + smooth(0.62, 0.92, fr) + 0.25 * fr) * step / 1.25
        flank = smooth(0.15, 0.4, r / F32(b["height"])) * (1 - smooth(0.85, 0.98, r / F32(b["height"])))
        r = r * (1 - 0.7 * flank) + terr * 0.7 * flank
        h += r
        masks["fresh"] += flank * smooth(0.65, 0.8, fr) * 0.8
        masks["brown"] = np.maximum(masks["brown"], np.exp(-(across / (b["half_sep"] * 0.5)) ** 2) * ends * 0.8)
        masks["frost"] += smooth(0.75, 0.95, r / F32(b["height"])) * 0.5
        return h

    def scarps(self, x, y, h, masks):
        for i, s in enumerate(self.P["scarps"]):
            c = bearing_dir(s["at_bearing_deg"]) * s["dist"]
            st = bearing_dir(s["strike_deg"])
            nrm = np.array([-st[1], st[0]])
            along = (x - c[0]) * st[0] + (y - c[1]) * st[1]
            across = (x - c[0]) * nrm[0] + (y - c[1]) * nrm[1]
            across = across * s["up_side"] + F32(s["riser"] * 2.5) * fbm(along, across * 0 + i, 600.0, 4, self.seed + 60 + i)
            ends = smooth(s["length"] / 2, s["length"] / 2 - 500, np.abs(along))
            n = s["steps"]
            out = np.zeros_like(x, dtype=F32)
            fresh = np.zeros_like(x, dtype=F32)
            for k in range(n):
                # each step's riser some way back from the last, a tread between (tilted back a little)
                at = k * s["riser"] * 4.5 + s["riser"] * 2.0 * (perlin(along / 300.0, F32(k), self.seed + 70 + i) )
                t = (across - at) / F32(s["riser"])
                out += smooth(-0.5, 0.5, t) * F32(s["height"] / n)
                fresh += np.exp(-t * t * 3.0)
            back = smooth(0, s["riser"] * 4.5 * n, across) * F32(s["height"] * 0.08)
            h += (out - F32(s["height"]) * 0.5 - back) * ends
            masks["fresh"] += np.clip(fresh, 0, 1) * ends
            masks["cavity"] += np.exp(-((across + s["riser"]) / (s["riser"] * 1.2)) ** 2) * ends * 0.5
        return h

    def chaos(self, x, y, h, res, masks, plains_fn):
        for i, c in enumerate(self.P["chaos"]):
            ctr = bearing_dir(c["at_bearing_deg"]) * c["dist"]
            dx, dy = x - ctr[0], y - ctr[1]
            r = np.sqrt(dx * dx + dy * dy)
            sel = r < c["radius"] * 1.35
            if not sel.any():
                continue
            xs, ys, rs = x[sel], y[sel], r[sel]
            edge = rs / F32(c["radius"]) + F32(0.25) * fbm(xs, ys, c["radius"] * 0.5, 3, self.seed + 80 + i)
            inside = smooth(1.0, 0.82, edge)
            # the matrix: lower, hummocky, brown
            matrix = -F32(c["drop"]) + F32(c["drop"] * 0.18) * fbm(xs, ys, c["cell"] * 0.35, 4, self.seed + 90 + i, res)
            raft = np.full_like(xs, -1e9, dtype=F32)
            side = np.zeros_like(xs, dtype=F32)
            for scale, keep, s_off in ((c["cell"], 0.62, 0), (c["small_cell"], 0.45, 7)):
                f1, f2, cid, pt = self._voronoi(xs, ys, scale, self.seed + 100 + i * 13 + s_off)
                e = (f2 - f1) * 0.5                                 # distance to the cell's edge
                u = _u01(_hash2(cid[0], cid[1], self.seed + 120 + s_off))
                v = _u01(_hash2(cid[1], cid[0], self.seed + 121 + s_off))
                w = _u01(_hash2(cid[0] + 7, cid[1] - 3, self.seed + 122 + s_off))
                is_raft = u < keep
                top = F32(c["raft_height"][0]) + (F32(c["raft_height"][1] - c["raft_height"][0]) * v * v) * (1.0 if s_off == 0 else 0.4)
                # tilted: the raft's plane leans by up to tilt_deg one way
                ta = w * F32(2 * math.pi)
                tilt = np.tan(np.radians(F32(c["tilt_deg"]) * u))
                lean = ((xs - pt[0]) * np.cos(ta) + (ys - pt[1]) * np.sin(ta)) * tilt
                cliff = F32(c["cliff"] if s_off == 0 else c["cliff"] * 0.6)
                inset = F32(scale * 0.06)
                prof = smooth(inset, inset + cliff, e)
                hr = np.where(is_raft, matrix + (top + lean) * prof - F32(c["drop"] * 0.0), -1e9)
                side = np.maximum(side, np.where(is_raft, np.exp(-((e - inset - cliff * 0.5) / cliff) ** 2), 0))
                raft = np.maximum(raft, hr)
            # the rafts keep the old plains' surface on top (displaced a little: they drifted)
            old, _ = plains_fn(xs + 37.0, ys - 19.0, res)
            hc = np.maximum(matrix, raft + (old - h[sel] * 0) * 0.35)
            on_raft = raft > matrix + 1.0
            h[sel] = h[sel] * (1 - inside) + (h[sel] * 0.25 + hc) * inside
            masks["brown"][sel] = np.maximum(masks["brown"][sel], inside * np.where(on_raft, 0.15, 0.85))
            masks["fresh"][sel] += inside * side * 0.9
            masks["cavity"][sel] += inside * np.where(on_raft, 0.0, 0.35)
        return h

    def _voronoi(self, x, y, cell, seed):
        gx = np.floor(x / cell).astype(np.int64)
        gy = np.floor(y / cell).astype(np.int64)
        f1 = np.full_like(x, 1e9, dtype=F32)
        f2 = np.full_like(x, 1e9, dtype=F32)
        id0 = np.zeros_like(gx)
        id1 = np.zeros_like(gy)
        nx = np.zeros_like(x, dtype=F32)
        ny = np.zeros_like(y, dtype=F32)
        for ox in (-1, 0, 1):
            for oy in (-1, 0, 1):
                cx, cy = gx + ox, gy + oy
                px = (cx + 0.1 + 0.8 * _u01(_hash2(cx, cy, seed))) * cell
                py = (cy + 0.1 + 0.8 * _u01(_hash2(cy, cx, seed + 1))) * cell
                d = np.sqrt((x - px) ** 2 + (y - py) ** 2).astype(F32)
                closer = d < f1
                f2 = np.where(closer, f1, np.minimum(f2, d))
                id0 = np.where(closer, cx, id0)
                id1 = np.where(closer, cy, id1)
                nx = np.where(closer, px, nx)
                ny = np.where(closer, py, ny)
                f1 = np.where(closer, d, f1)
        return f1, f2, (id0, id1), (nx, ny)

    def rift_frames(self):
        out = []
        for i, r in enumerate(self.P["rifts"]):
            c = bearing_dir(r["at_bearing_deg"]) * r["dist"]
            st = bearing_dir(r["strike_deg"])
            out.append((i, r, c, st, np.array([-st[1], st[0]])))
        return out

    def rift_width(self, i, r, along):
        """Half width along the lead: ragged, closing at both ends."""
        w = F32(r["half_width"]) * (1 + 0.35 * fbm(along, along * 0 + 11 * i, 140.0, 4, self.seed + 140 + i))
        return w * np.sqrt(np.clip(1 - (2 * along / F32(r["length"])) ** 2, 0, 1))

    def rifts(self, x, y, h, masks, water):
        for i, r, c, st, nrm in self.rift_frames():
            along = (x - c[0]) * st[0] + (y - c[1]) * st[1]
            across = (x - c[0]) * nrm[0] + (y - c[1]) * nrm[1]
            sel = (np.abs(along) < r["length"] / 2 + 40) & (np.abs(across) < r["half_width"] * 3 + 40)
            if not sel.any():
                continue
            al, ac = along[sel], across[sel]
            ac = ac + F32(r["half_width"] * 0.35) * fbm(al, ac * 0 + i, 260.0, 3, self.seed + 150 + i)
            W = self.rift_width(i, r, al)
            zw = self.rift_levels[i]
            bed = F32(zw - r["bed"])
            d = np.abs(ac)
            # the walls: near-vertical ice cliffs, a slump bench part way down, a raised lip on the rim
            t = (W - d) / F32(r["wall"])
            wall = smooth(0.0, 1.0, t)
            bench = smooth(1.2, 1.6, t) * F32(r["drop"] * 0.25) * (0.5 + 0.5 * perlin(al / 70.0, F32(i), self.seed + 160))
            sub = h[sel]
            lip = F32(1.4) * np.exp(-((d - W - 4) / 5) ** 2) * (W > 0.5)
            inner = bed + (sub - bed) * (1 - wall) + bench * wall * (1 - smooth(1.6, 3.0, t))
            h[sel] = np.where(t > 0, np.minimum(sub, inner), sub + lip)
            masks["fresh"][sel] += np.exp(-((d - W) / (r["wall"] * 1.4)) ** 2) * (W > 0.5) * 0.9
            masks["brown"][sel] = np.maximum(masks["brown"][sel], np.exp(-((d - W) / 22.0) ** 2) * (W > 0.5) * 0.75)
            if water is not None:
                water[sel] = np.maximum(water[sel], (t > 0.5).astype(F32))
        return h

    def far(self, x, y, res):
        f = self.P["far"]
        r = np.sqrt(x * x + y * y)
        k = smooth(f["start"], f["start"] + 2500, r)
        if not np.any(k > 0):
            return 0
        rr = ridged(x + 900.0, y - 400.0, f["wavelength"], f["octaves"], self.seed + 200, res, 1.0, 2.2)
        return F32(f["amp"]) * k * (rr - F32(0.35))

    # all of it ------------------------------------------------------------------------------------------------------
    def height(self, x, y, res, with_masks=True, erosion=None, raw=False):
        x = x.astype(F32)
        y = y.astype(F32)
        masks = {k: np.zeros_like(x, dtype=F32) for k in ("brown", "fresh", "frost", "cavity")}
        water = np.zeros_like(x, dtype=F32)
        h, rg = self.plains(x, y, res)
        masks["frost"] += smooth(0.55, 0.8, rg) * 0.5
        masks["brown"] += smooth(0.3, 0.1, rg) * 0.25
        h = self.lineae_h(x, y, h, res, masks)
        h = self.big_ridge(x, y, h, masks)
        h = self.scarps(x, y, h, masks)
        h = self.chaos(x, y, h, res, masks, self.plains)
        h = h + self.far(x, y, res)
        if erosion is not None:
            h = h + erosion(x, y)
        if self.rift_levels is not None:
            h = self.rifts(x, y, h, masks, water)
        if raw:
            return h
        # the hatch's apron: flat round the collar
        r = np.sqrt(x * x + y * y)
        a0, a1 = self.P["apron"]
        k = smooth(a0, a1, r)
        h0 = self.h_site
        h = F32(self.P["site_z"]) + (h - h0) * k
        # Europa's curve
        h = h - r * r / F32(2 * self.R)
        if not with_masks:
            return h
        for m in masks.values():
            np.clip(m, 0, 1, out=m)
        return h, masks, water

    def prepare(self):
        """The site's own height (the apron is levelled to it), the leads' water levels, and the talus field."""
        self.h_site = F32(self.height(np.zeros(1), np.zeros(1), 0.5, with_masks=False, raw=True)[0])
        # each lead's water (in the raw heights' frame): its drop below the lowest ground its water strip covers
        levels = []
        for i, r, c, st, nrm in self.rift_frames():
            t = np.linspace(-r["length"] / 2, r["length"] / 2, 240)
            u = np.linspace(-(r["half_width"] * 1.6 + 6), r["half_width"] * 1.6 + 6, 25)
            T, U = np.meshgrid(t, u)
            px, py = c[0] + st[0] * T + nrm[0] * U, c[1] + st[1] * T + nrm[1] * U
            g = self.height(px.ravel(), py.ravel(), 2.0, with_masks=False, raw=True)
            levels.append(float(np.min(g) - r["drop"]))
        self.rift_levels = levels
        self._erosion()

    def _erosion(self):
        E = self.P["erosion"]
        cell = E["cell"]
        half = self.P["mesh"]["levels"][-1][1] + 64
        n = int(2 * half / cell) + 1
        xs = np.linspace(-half, half, n, dtype=F32)
        X, Y = np.meshgrid(xs, xs)
        t0 = time.time()
        rl = self.rift_levels
        self.rift_levels = None
        H = self.height(X, Y, cell, with_masks=False, raw=True) + 0
        self.rift_levels = rl
        H0 = H.copy()
        talus = math.tan(math.radians(E["talus_deg"])) * cell
        for it in range(E["iterations"]):
            moved = np.zeros_like(H)
            for dx, dy, f in ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.41421), (-1, -1, 1.41421), (1, -1, 1.41421), (-1, 1, 1.41421)):
                nb = np.roll(np.roll(H, dy, 0), dx, 1)
                d = H - nb - talus * f
                m = np.where(d > 0, d * E["rate"] / 8.0, 0)
                moved -= m
                moved += np.roll(np.roll(m, -dy, 0), -dx, 1)
            H += moved
        D = H - H0
        D = np.where(D > 0, D * E["keep_fill"], D * E["keep_cut"]).astype(F32)
        D[:2, :] = D[-2:, :] = 0
        D[:, :2] = D[:, -2:] = 0
        print("MOONT erosion: %d x %d, %.1f s, fill max %.1f m, cut max %.1f m" % (n, n, time.time() - t0, D.max(), -D.min()))

        def sample(x, y):
            fx = (x + half) / cell
            fy = (y + half) / cell
            ix = np.clip(np.floor(fx).astype(np.int64), 0, n - 2)
            iy = np.clip(np.floor(fy).astype(np.int64), 0, n - 2)
            tx = np.clip(fx - ix, 0, 1).astype(F32)
            ty = np.clip(fy - iy, 0, 1).astype(F32)
            v = (D[iy, ix] * (1 - tx) * (1 - ty) + D[iy, ix + 1] * tx * (1 - ty) + D[iy + 1, ix] * (1 - tx) * ty + D[iy + 1, ix + 1] * tx * ty)
            out_of = (np.abs(x) > half - cell) | (np.abs(y) > half - cell)
            return np.where(out_of, 0, v).astype(F32)
        self.erode = sample


# -- meshes --------------------------------------------------------------------------------------------------------

class Mesh:
    def __init__(self, name):
        self.name = name
        self.P, self.N, self.C, self.UV, self.I = [], [], [], [], []
        self.nv = 0

    def add(self, P, N, C, UV, I):
        self.P.append(P.astype(F32))
        self.N.append(N.astype(F32))
        self.C.append(C.astype(F32))
        self.UV.append(UV.astype(F32))
        self.I.append((I + self.nv).astype(np.uint32))
        self.nv += len(P)

    def arrays(self):
        return (np.concatenate(self.P), np.concatenate(self.N), np.concatenate(self.C), np.concatenate(self.UV),
                np.concatenate(self.I))


def grid_tile(G, x0, y0, size, sp, snap_edges, disc):
    """A tile's heights on its grid (one extra sample round it for the normals), the snapping of edges that meet a
    coarser level, and its triangles (outside the disc dropped)."""
    n = int(round(size / sp)) + 1
    xs = x0 + np.arange(-1, n + 1) * sp
    ys = y0 + np.arange(-1, n + 1) * sp
    X, Y = np.meshgrid(xs, ys)
    h, m, w = G.height(X.ravel(), Y.ravel(), sp, erosion=G.erode)
    H = h.reshape(X.shape)
    # normals from the grid
    gx = (H[1:-1, 2:] - H[1:-1, :-2]) / (2 * sp)
    gy = (H[2:, 1:-1] - H[:-2, 1:-1]) / (2 * sp)
    Hc = H[1:-1, 1:-1].copy()
    Xc, Yc = X[1:-1, 1:-1], Y[1:-1, 1:-1]
    # an edge against a coarser level: the in-between samples on the coarse samples' straight line
    for edge, ratio in snap_edges:
        if edge == "S":
            line, lx, ly = Hc[0, :], Xc[0, :], Yc[0, :]
        elif edge == "N":
            line, lx, ly = Hc[-1, :], Xc[-1, :], Yc[-1, :]
        elif edge == "W":
            line, lx, ly = Hc[:, 0], Xc[:, 0], Yc[:, 0]
        else:
            line, lx, ly = Hc[:, -1], Xc[:, -1], Yc[:, -1]
        k = np.arange(len(line))
        base = (k // ratio) * ratio
        nxt = np.minimum(base + ratio, len(line) - 1)
        t = (k - base) / ratio
        # the coarse neighbour's own samples (its octaves fade at its spacing)
        coarse = G.height(lx[::ratio], ly[::ratio], sp * ratio, with_masks=False, erosion=G.erode)
        cfull = np.zeros(len(line), F32)
        cfull[::ratio] = coarse
        line[:] = cfull[base] * (1 - t) + cfull[nxt] * t
    nrm = np.stack([-gx, -gy, np.ones_like(gx)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    mk = {k: v.reshape(X.shape)[1:-1, 1:-1] for k, v in m.items()}
    col = np.stack([mk["brown"], mk["fresh"], np.clip(mk["frost"], 0, 1), mk["cavity"]], -1)
    P = np.stack([Xc, Yc, Hc], -1).reshape(-1, 3)
    N = nrm.reshape(-1, 3)
    C = col.reshape(-1, 4)
    UV = (P[:, :2] / 16.0)
    # triangles: (i, j), (i, j+1), (i+1, j+1) is front-facing up once in glTF's frame (see to_gltf)
    jj, ii = np.meshgrid(np.arange(n - 1), np.arange(n - 1))
    a = (jj * n + ii).ravel()
    b = a + 1
    c = a + n
    d = a + n + 1
    I = np.stack([a, c, d, a, d, b], -1).reshape(-1, 3)
    if disc:
        r = np.sqrt((P[:, 0] ** 2 + P[:, 1] ** 2))
        keep = (r[I] < disc).any(1)
        I = I[keep]
    I = fix_winding(P, N, I)
    surface = len(I)
    # a skirt down round the tile: hides sub-pixel cracks against its neighbours
    ring = np.concatenate([np.arange(n), (n - 1) + np.arange(1, n) * n, (n * n - 1) - np.arange(1, n), (n - 1) * n - np.arange(1, n - 1) * n])
    skirt = G.P["mesh"]["skirt"] * max(1.0, sp)
    SP = P[ring] - np.array([0, 0, skirt], F32)
    base_i = len(P)
    sk = []
    m_ = len(ring)
    for q in range(m_):
        r0, r1 = ring[q], ring[(q + 1) % m_]
        s0, s1 = base_i + q, base_i + (q + 1) % m_
        sk += [[r0, s1, s0], [r0, r1, s1], [r0, s0, s1], [r0, s1, r1]]
    sk = np.array(sk)
    if disc:
        rr = np.sqrt(P[ring, 0] ** 2 + P[ring, 1] ** 2)
        sk = sk[np.repeat(rr < disc, 4)]
    P = np.concatenate([P, SP])
    N = np.concatenate([N, N[ring]])
    C = np.concatenate([C, C[ring]])
    UV = np.concatenate([UV, UV[ring]])
    I = np.concatenate([I, sk]) if len(sk) else I
    w2 = w.reshape(X.shape)[1:-1, 1:-1]
    return P, N, C, UV, I, H[1:-1, 1:-1], w2, surface


def ring_mesh(G, r0, r1, seg):
    radii = [r0]
    r = r0
    while r < r1:
        r += max(30.0, r * 0.012)
        radii.append(min(r, r1))
    a = np.linspace(0, 2 * math.pi, seg, endpoint=False)
    R, A = np.meshgrid(np.array(radii), a, indexing="ij")
    X, Y = R * np.cos(A), R * np.sin(A)
    sp = np.maximum(R * 2 * math.pi / seg, 30.0)
    h, m, _ = G.height(X.ravel(), Y.ravel(), 30.0, erosion=G.erode)
    H = h.reshape(X.shape) - 1.0                     # a metre under the disc where they overlap
    # beyond 12 km the far edge sinks away under the horizon (no visible end of the world)
    H -= np.clip((R - 12000.0) / 4000.0, 0, 1) ** 2 * 400.0
    P = np.stack([X, Y, H], -1).reshape(-1, 3)
    nr, na = X.shape
    # normals from neighbours (radial and round)
    Hr = np.gradient(H, np.array(radii), axis=0)
    Ha = np.gradient(H, axis=1) / (R * (2 * math.pi / seg))
    er = np.stack([np.cos(A), np.sin(A)], -1)
    ea = np.stack([-np.sin(A), np.cos(A)], -1)
    g = er * Hr[..., None] + ea * Ha[..., None]
    N = np.concatenate([-g, np.ones(X.shape + (1,))], -1)
    N /= np.linalg.norm(N, axis=-1, keepdims=True)
    mk = {k: v.reshape(X.shape) for k, v in m.items()}
    C = np.stack([mk["brown"], mk["fresh"], np.clip(mk["frost"], 0, 1), mk["cavity"]], -1).reshape(-1, 4)
    UV = P[:, :2] / 16.0
    I = []
    for i in range(nr - 1):
        for j in range(na):
            j1 = (j + 1) % na
            p00, p01, p10, p11 = i * na + j, i * na + j1, (i + 1) * na + j, (i + 1) * na + j1
            I += [[p00, p10, p11], [p00, p11, p01]]
    I = np.array(I)
    # wind so that the faces point up (checked against the normals below)
    _ = sp
    return P, N.reshape(-1, 3), C, UV, I


def boulders(G):
    B = G.P["boulders"]
    rng = np.random.default_rng(G.seed + B["seed_offset"])
    mesh = Mesh("SM_EU_Boulders")
    # an icosphere (80 faces) to deform per boulder
    t = (1 + 5 ** 0.5) / 2
    V = np.array([[-1, t, 0], [1, t, 0], [-1, -t, 0], [1, -t, 0], [0, -1, t], [0, 1, t], [0, -1, -t], [0, 1, -t],
                  [t, 0, -1], [t, 0, 1], [-t, 0, -1], [-t, 0, 1]], float)
    F = [[0, 11, 5], [0, 5, 1], [0, 1, 7], [0, 7, 10], [0, 10, 11], [1, 5, 9], [5, 11, 4], [11, 10, 2], [10, 7, 6], [7, 1, 8],
         [3, 9, 4], [3, 4, 2], [3, 2, 6], [3, 6, 8], [3, 8, 9], [4, 9, 5], [2, 4, 11], [6, 2, 10], [8, 6, 7], [9, 8, 1]]
    V /= np.linalg.norm(V, axis=1, keepdims=True)
    mid = {}
    V = list(V)
    F2 = []
    for a, b, c in F:
        def m(i, j):
            k = (min(i, j), max(i, j))
            if k not in mid:
                p = (V[i] + V[j]) / 2
                V.append(p / np.linalg.norm(p))
                mid[k] = len(V) - 1
            return mid[k]
        ab, bc, ca = m(a, b), m(b, c), m(c, a)
        F2 += [[a, ab, ca], [b, bc, ab], [c, ca, bc], [ab, bc, ca]]
    V = np.array(V)
    F2 = np.array(F2)
    # where they lie: more under cliffs and in the chaos, few on the plains, none on the apron or in the leads
    want = B["count"]
    xs = rng.uniform(-B["radius"], B["radius"], want * 6)
    ys = rng.uniform(-B["radius"], B["radius"], want * 6)
    h, m, w = G.height(xs, ys, 1.0, erosion=G.erode)
    r = np.sqrt(xs * xs + ys * ys)
    p = 0.08 + 1.4 * m["fresh"] + 0.6 * m["cavity"] + 0.5 * m["brown"]
    p *= (r > 14) * (w < 0.5) * (r < B["radius"])
    p *= np.exp(-r / 900.0) + 0.25
    pick = rng.random(len(xs)) < p / p.max()
    idx = np.nonzero(pick)[0][:want]
    lo, hi = B["size"]
    for k in idx:
        s = lo * (hi / lo) ** (rng.random() ** 2.2)
        sx, sy, sz = s * rng.uniform(0.7, 1.3), s * rng.uniform(0.7, 1.3), s * rng.uniform(0.35, 0.75)
        jit = 1 + rng.uniform(-0.2, 0.2, len(V))
        Q = V * jit[:, None] * np.array([sx, sy, sz])
        ang = rng.uniform(0, 2 * math.pi)
        c, sn = math.cos(ang), math.sin(ang)
        Q = Q @ np.array([[c, -sn, 0], [sn, c, 0], [0, 0, 1]]).T
        Q += np.array([xs[k], ys[k], h[k] - sz * 0.35])
        # faceted normals look like broken ice: flat-shade by splitting the faces
        P = Q[F2].reshape(-1, 3)
        e1 = P[1::3] - P[0::3]
        e2 = P[2::3] - P[0::3]
        n = np.cross(e1, e2)
        n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9
        N = np.repeat(n, 3, 0)
        C = np.tile(np.array([0.15, 0.7, 0.4, 0.0]), (len(P), 1))
        UV = P[:, :2] / 4.0
        I = np.arange(len(P)).reshape(-1, 3)
        mesh.add(P, N, C, UV, I)
    print("MOONT boulders: %d" % len(idx))
    return mesh


def water_meshes(G):
    out = []
    for i, r, c, st, nrm in G.rift_frames():
        mesh = Mesh("SM_EU_Water_%d" % i)
        n = int(r["length"] / 6) + 1
        t = np.linspace(-r["length"] / 2, r["length"] / 2, n)
        W = r["half_width"] * 1.6 + 6
        cols = 9
        u = np.linspace(-W, W, cols)
        T, U = np.meshgrid(t, u, indexing="ij")
        X = c[0] + st[0] * T + nrm[0] * U
        Y = c[1] + st[1] * T + nrm[1] * U
        Rr = np.sqrt(X * X + Y * Y)
        # the levels are in the raw heights' frame: the apron's levelling shifts the whole ground by h_site, the curve
        Z = G.rift_levels[i] - float(G.h_site) + G.P["site_z"] - Rr * Rr / (2 * G.R)
        P = np.stack([X, Y, Z], -1).reshape(-1, 3)
        N = np.tile([0, 0, 1.0], (len(P), 1))
        C = np.ones((len(P), 4))
        UV = P[:, :2] / 10.0
        I = []
        for a in range(n - 1):
            for b in range(cols - 1):
                p00, p01, p10, p11 = a * cols + b, a * cols + b + 1, (a + 1) * cols + b, (a + 1) * cols + b + 1
                I += [[p00, p10, p11], [p00, p11, p01]]
        mesh.add(P, N, C, UV, np.array(I))
        out.append((mesh, i, r, float(Z.mean()), c, st, nrm))
    return out


def fix_winding(P, N, I):
    """Each triangle's winding so its face normal agrees with the vertex normals (front faces up in glTF)."""
    a, b, c = P[I[:, 0]], P[I[:, 1]], P[I[:, 2]]
    fn = np.cross(b - a, c - a)
    # in the Unreal-axes frame (x, y, z up) a front face is clockwise seen from above once mirrored into glTF
    ok = (fn * (N[I[:, 0]] + N[I[:, 1]] + N[I[:, 2]])).sum(1) < 0
    I = I.copy()
    I[~ok] = I[~ok][:, [0, 2, 1]]
    return I


# -- glTF ------------------------------------------------------------------------------------------------------------

def to_gltf(meshes, path):
    """A minimal glTF 2.0 binary: one mesh per node, POSITION, NORMAL, COLOR_0, TEXCOORD_0, uint32 indices. Unreal
    (Interchange) reads glTF (x, y, z) as (x, -z, y)... in metres: we write Unreal (X, Y, Z) cm as glTF (X, Z, Y) m
    (a mirror: the winding is set by fix_winding)."""
    bin_ = bytearray()
    views, accs, gmeshes, nodes = [], [], [], []
    mats = [{"name": "EU_terrain", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.82, 0.85, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.6}},
            {"name": "EU_water", "pbrMetallicRoughness": {"baseColorFactor": [0.02, 0.06, 0.08, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.1}},
            {"name": "EU_boulder", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.82, 0.85, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.6}}]

    def view(data, target=None):
        while len(bin_) % 4:
            bin_.append(0)
        off = len(bin_)
        bin_.extend(data)
        v = {"buffer": 0, "byteOffset": off, "byteLength": len(data)}
        if target:
            v["target"] = target
        views.append(v)
        return len(views) - 1

    def acc(arr, typ, comp, target, minmax=False):
        a = {"bufferView": view(arr.tobytes(), target), "componentType": comp, "count": len(arr), "type": typ}
        if minmax:
            a["min"] = arr.min(0).tolist()
            a["max"] = arr.max(0).tolist()
        accs.append(a)
        return len(accs) - 1

    for name, (P, N, C, UV, I), mat in meshes:
        G = np.stack([P[:, 0], P[:, 2], P[:, 1]], -1).astype(F32)
        GN = np.stack([N[:, 0], N[:, 2], N[:, 1]], -1).astype(F32)
        pa = acc(G, "VEC3", 5126, 34962, True)
        na = acc(GN, "VEC3", 5126, 34962)
        ca = acc(np.clip(C, 0, 1).astype(F32), "VEC4", 5126, 34962)
        ua = acc(UV.astype(F32), "VEC2", 5126, 34962)
        ia = acc(I.astype(np.uint32).ravel(), "SCALAR", 5125, 34963)
        gmeshes.append({"name": name, "primitives": [{"attributes": {"POSITION": pa, "NORMAL": na, "COLOR_0": ca, "TEXCOORD_0": ua},
                                                      "indices": ia, "material": mat}]})
        nodes.append({"name": name, "mesh": len(gmeshes) - 1})
    gl = {"asset": {"version": "2.0", "generator": "tools/europa/moon_terrain.py"}, "scene": 0,
          "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes, "meshes": gmeshes, "materials": mats,
          "accessors": accs, "bufferViews": views, "buffers": [{"byteLength": len(bin_)}]}
    js = json.dumps(gl, separators=(",", ":")).encode()
    while len(js) % 4:
        js += b" "
    while len(bin_) % 4:
        bin_.append(0)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(bin_)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A))
        f.write(js)
        f.write(struct.pack("<II", len(bin_), 0x004E4942))
        f.write(bin_)


# -- checks and previews -------------------------------------------------------------------------------------------

def lint(name, P, N, I):
    """Glitch checks (the project's 3D rule): degenerate or flipped triangles, NaNs, normals against the faces."""
    bad = []
    if not np.isfinite(P).all() or not np.isfinite(N).all():
        bad.append("NaN")
    a, b, c = P[I[:, 0]], P[I[:, 1]], P[I[:, 2]]
    fn = np.cross(b - a, c - a)
    area = np.linalg.norm(fn, axis=1) * 0.5
    deg = int((area < 1e-6).sum())
    if deg:
        bad.append("%d degenerate" % deg)
    vn = N[I[:, 0]] + N[I[:, 1]] + N[I[:, 2]]
    flip = int(((fn * vn).sum(1) > 0).sum())       # after fix_winding every face is "clockwise" against its normal
    if flip:
        bad.append("%d flipped" % flip)
    print("MOONT lint %s: %d tris%s" % (name, len(I), (": " + ", ".join(bad)) if bad else ", clean"))
    return bad


def preview(G, out_dir, tiles):
    from PIL import Image
    os.makedirs(out_dir, exist_ok=True)
    # a hillshade of the walkable disc and of the near 1 km
    for half, px, name in ((4200.0, 1024, "moon_map.png"), (600.0, 1024, "moon_near.png")):
        xs = np.linspace(-half, half, px, dtype=F32)
        X, Y = np.meshgrid(xs, -xs)
        h, m, w = G.height(X.ravel(), Y.ravel(), 2 * half / px, erosion=G.erode)
        H = h.reshape(X.shape)
        sp = 2 * half / px
        gy, gx = np.gradient(H, sp)
        L = np.array([-0.5, 0.6, 0.62])
        L /= np.linalg.norm(L)
        nrm = np.stack([-gx, gy, np.ones_like(gx)], -1)
        nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
        shade = np.clip((nrm * L).sum(-1), 0, 1)
        M = {k: v.reshape(X.shape) for k, v in m.items()}
        base = np.stack([0.86 - 0.35 * M["brown"], 0.87 - 0.5 * M["brown"], 0.9 - 0.6 * M["brown"]], -1)
        base = base * (1 - 0.3 * M["fresh"][..., None]) + np.array([0.6, 0.8, 0.95]) * 0.3 * M["fresh"][..., None]
        img = base * (0.25 + 0.75 * shade[..., None])
        wat = w.reshape(X.shape) > 0.5
        img[wat] = [0.02, 0.06, 0.09]
        Image.fromarray((np.clip(img, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)).save(os.path.join(out_dir, name))
        print("MOONT preview %s: heights %.0f .. %.0f m" % (name, H.min(), H.max()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    ap.add_argument("--only-preview", action="store_true")
    args = ap.parse_args()
    with open(PARAMS) as f:
        P = json.load(f)
    t0 = time.time()
    G = Ground(P)
    G.prepare()
    print("MOONT planet at bearing %.1f, altitude %.1f deg; site h %.2f; leads %s" % (G.planet_bearing, G.planet_alt, float(G.h_site), ["%.1f" % z for z in G.rift_levels]))
    if args.preview:
        preview(G, args.preview, None)
        if args.only_preview:
            return
    meshes = []
    tiles = []
    digest = hashlib.sha1()
    levels = P["mesh"]["levels"]
    disc = P["mesh"]["disc_radius"]
    for li, (sp, half, tile) in enumerate(levels):
        inner = levels[li - 1][1] if li > 0 else 0.0
        nt = int(round(2 * half / tile))
        for i in range(nt):
            for j in range(nt):
                x0, y0 = -half + i * tile, -half + j * tile
                cx, cy = x0 + tile / 2, y0 + tile / 2
                if max(abs(cx), abs(cy)) < inner:
                    continue
                if li == len(levels) - 1 and math.hypot(max(abs(cx) - tile / 2, 0), max(abs(cy) - tile / 2, 0)) > disc:
                    continue
                # edges on this level's outer boundary meet the next, coarser level
                snap = []
                if li < len(levels) - 1:
                    ratio = int(round(levels[li + 1][0] / sp))
                    if abs(y0 + half) < 1e-3:
                        snap.append(("S", ratio))
                    if abs(y0 + tile - half) < 1e-3:
                        snap.append(("N", ratio))
                    if abs(x0 + half) < 1e-3:
                        snap.append(("W", ratio))
                    if abs(x0 + tile - half) < 1e-3:
                        snap.append(("E", ratio))
                Pp, N, C, UV, I, H, W, surf = grid_tile(G, x0, y0, tile, sp, snap, disc if li == len(levels) - 1 else 0)
                name = "SM_EU_T_%d_%d_%d" % (li, i, j)
                lint(name, Pp, N, I[:surf])
                digest.update(np.round(H, 2).astype(F32).tobytes())
                meshes.append((name, (Pp, N, C, UV, I), 0))
                tiles.append({"name": name, "level": li, "x0": x0, "y0": y0, "size": tile, "spacing": sp,
                              "zmin": float(H.min()), "zmax": float(H.max())})
        print("MOONT level %d (%.1f m): %d tiles so far, %.0f s" % (li, sp, len(tiles), time.time() - t0))
    r0, r1, seg = P["mesh"]["ring"]
    Pp, N, C, UV, I = ring_mesh(G, r0, r1, seg)
    I = fix_winding(Pp, N, I)
    lint("SM_EU_Horizon", Pp, N, I)
    meshes.append(("SM_EU_Horizon", (Pp, N, C, UV, I), 0))
    b = boulders(G)
    Pp, N, C, UV, I = b.arrays()
    I = fix_winding(Pp, N, I)
    lint("SM_EU_Boulders", Pp, N, I)
    meshes.append(("SM_EU_Boulders", (Pp, N, C, UV, I), 2))
    waters = []
    for wm, i, r, z, c, st, nrm in water_meshes(G):
        Pp, N, C, UV, I = wm.arrays()
        I = fix_winding(Pp, N, I)
        lint(wm.name, Pp, N, I)
        meshes.append((wm.name, (Pp, N, C, UV, I), 1))
        waters.append({"name": wm.name, "z_cm": round(z * 100, 1), "centre_cm": [round(c[0] * 100, 1), round(c[1] * 100, 1)],
                       "strike": [round(st[0], 5), round(st[1], 5)], "length_cm": r["length"] * 100,
                       "half_width_cm": r["half_width"] * 160 + 600})
    total = sum(len(m[1][4]) for m in meshes)
    to_gltf(meshes, OUT_GLB)
    data = {
        "_doc": "Written by tools/europa/moon_terrain.py from tools/europa/moon_terrain.json: do not edit by hand. "
                "UE cm in ALyudEuropaSite's frame (its origin = the far hatch's lid seat).",
        "seed": P["seed"],
        "site": P["site"],
        "planet_bearing_deg": round(G.planet_bearing, 2), "planet_altitude_deg": round(G.planet_alt, 2),
        "heights_sha1": digest.hexdigest(),
        "triangles": int(total),
        "tiles": tiles,
        "horizon": "SM_EU_Horizon", "boulders": "SM_EU_Boulders",
        "water": waters,
        "walk_radius_cm": 350000.0,
    }
    with open(OUT_JSON, "w") as f:
        json.dump(data, f, indent=1)
        f.write("\n")
    print("MOONT wrote %s (%.0f MB, %d meshes, %.1f M triangles) and %s in %.0f s; heights sha1 %s"
          % (OUT_GLB, os.path.getsize(OUT_GLB) / 1e6, len(meshes), total / 1e6, OUT_JSON, time.time() - t0, digest.hexdigest()))


if __name__ == "__main__":
    main()
