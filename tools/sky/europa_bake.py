"""Europa's surface for the moon in the sky, made procedurally and baked once (Dima, 2026-10-29: our own Europa, its
relief real 3D, then fixed): a height field and an albedo on the sphere, written as two equirectangular PNGs that
unreal/scripts/import_sky.py imports and tools/sky/sky_hlsl.py MOON_HLSL renders (relief normal mapping, the relief's
own shadows near the terminator).

    python3 tools/sky/europa_bake.py [--preview out.png]

Reads tools/sky/europa_bake.json (the seed and every parameter: the same file gives the same maps), writes
textures/sky/europa_albedo.png (RGB, sRGB) and textures/sky/europa_relief.png (RGBA, linear: R, G = the relief normal's
east and north components * 0.5 + 0.5, B = height * HEIGHT_GAIN + 0.5, A = 1). East longitude 0..360 left to right,
north up, as the shader samples it.

What is on it, oldest first (younger features cut and overprint older ones):
- ridged plains: cream ice, darker brownish mottling, patches of blue-white young ice, a fine grain of tiny ridges;
- lineae: long arcs of great and small circles, a little wavy; most are double ridges (two crests, a trough between),
  some triple bands (a bright central ridge in diffuse red-brown margins), some smooth grey bands with striations;
  their margins red-brown to ochre;
- chaos terrain: patches where the crust broke into tilted ice blocks (keeping the old plains and lineae on them)
  in a lowered hummocky red-brown matrix;
- a few craters (bowl, rim, central peak in the larger ones, bright ejecta), one with long bright rays.
"""
import argparse
import json
import math
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
PARAMS = os.path.join(HERE, "europa_bake.json")
OUT_ALBEDO = os.path.join(REPO, "textures", "sky", "europa_albedo.png")
OUT_RELIEF = os.path.join(REPO, "textures", "sky", "europa_relief.png")
HEIGHT_GAIN = 0.4      # B = height * HEIGHT_GAIN + 0.5 (heights are in lineae-crest units, ~-1..1)


# -- noise on the sphere ---------------------------------------------------------------------------------------

def _hash(ix, iy, iz, seed):
    h = (ix.astype(np.uint64) * np.uint64(0x8DA6B343) ^ iy.astype(np.uint64) * np.uint64(0xD8163841)
         ^ iz.astype(np.uint64) * np.uint64(0xCB1AB31F) ^ np.uint64(seed * 0x165667B1 & 0xFFFFFFFF))
    h = (h ^ (h >> np.uint64(15))) * np.uint64(0x2C1B3C6D) & np.uint64(0xFFFFFFFF)
    h = (h ^ (h >> np.uint64(12))) * np.uint64(0x297A2D39) & np.uint64(0xFFFFFFFF)
    h ^= h >> np.uint64(15)
    return (h & np.uint64(0xFFFFFF)).astype(np.float32) / np.float32(16777215.0)


def vnoise(p, freq, seed):
    """Value noise 0..1 at points p (..., 3), quintic between lattice cells of size 1/freq."""
    x = p * freq + 1000.0
    i = np.floor(x)
    f = (x - i).astype(np.float32)
    u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0)
    i = i.astype(np.int64)
    out = 0.0
    for dz in (0, 1):
        for dy in (0, 1):
            for dx in (0, 1):
                w = (u[..., 0] if dx else 1.0 - u[..., 0]) * (u[..., 1] if dy else 1.0 - u[..., 1]) * \
                    (u[..., 2] if dz else 1.0 - u[..., 2])
                out = out + w * _hash(i[..., 0] + dx, i[..., 1] + dy, i[..., 2] + dz, seed)
    return out


def fbm(p, freq, seed, octaves=5, gain=0.5):
    n, a, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        n = n + a * vnoise(p, freq * 2.03 ** o, seed + 101 * o)
        tot += a
        a *= gain
    return n / tot


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def gauss(x, s):
    return np.exp(-(x / s) ** 2)


def dot(p, a):
    """p (..., 3) . a (3,) (np.matmul on float32 trips spurious FP warnings on macOS' Accelerate)."""
    a = np.asarray(a, np.float32)
    return p[..., 0] * a[0] + p[..., 1] * a[1] + p[..., 2] * a[2]


def unit(v):
    return v / np.linalg.norm(v)


def basis(a):
    """Two unit vectors perpendicular to a and to each other."""
    t = np.array([0.0, 0.0, 1.0]) if abs(a[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    e1 = unit(np.cross(a, t))
    return e1, np.cross(a, e1)


# -- the surface -----------------------------------------------------------------------------------------------

class Surface:
    def __init__(self, cfg):
        self.cfg = cfg
        self.rng = np.random.default_rng(cfg["seed"])
        self.seed = cfg["seed"]
        W, H = cfg["width"], cfg["height"]
        lon = (np.arange(W) + 0.5) / W * 2.0 * math.pi
        lat = (0.5 - (np.arange(H) + 0.5) / H) * math.pi
        self.lat = np.repeat(lat[:, None], W, 1).astype(np.float32)
        lo = np.repeat(lon[None, :], H, 0)
        cl = np.cos(self.lat)
        self.p = np.stack([cl * np.cos(lo), cl * np.sin(lo), np.sin(self.lat)], -1).astype(np.float32)
        self.h = np.zeros((H, W), np.float32)
        self.col = np.zeros((H, W, 3), np.float32)
        self.C = {k: np.array(v, np.float32) for k, v in cfg["colours"].items()}

    def rand_dir(self):
        v = self.rng.normal(size=3)
        return unit(v)

    def paint(self, sel, colour, k):
        """Blend colour into the albedo at index sel by weight k (0..1)."""
        k = k[..., None]
        self.col[sel] = self.col[sel] * (1.0 - k) + colour * k

    # ridged plains
    def plains(self):
        c, P = self.cfg["plains"], self.p
        self.col[:] = self.C["plains"]
        mott = smooth(0.42, 0.75, fbm(P, c["mottle_freq"], self.seed + 1)) * c["mottle"]
        mott = mott * (0.6 + 0.8 * fbm(P, c["mottle_freq"] * 6.0, self.seed + 2, 3))
        self.col = self.col * (1.0 - mott[..., None]) + self.C["mottle"] * mott[..., None]
        young = smooth(1.0 - c["young_share"], 1.0 - c["young_share"] + 0.12, fbm(P, c["young_freq"], self.seed + 3, 4))
        self.col = self.col * (1.0 - 0.8 * young[..., None]) + self.C["young_ice"] * 0.8 * young[..., None]
        # the grain: countless tiny ridges (ridged noise), a little relief everywhere
        r1 = 1.0 - np.abs(vnoise(P, c["grain_freq"], self.seed + 4) * 2.0 - 1.0)
        r2 = 1.0 - np.abs(vnoise(P, c["grain_freq"] * 2.3, self.seed + 5) * 2.0 - 1.0)
        self.h += c["grain"] * (r1 ** 6 + 0.6 * r2 ** 6) + 0.12 * (fbm(P, 9.0, self.seed + 6, 4) - 0.5)

    # one lineament along an arc of a small circle about axis a
    def lineament(self, kind, w, amp, length, tint):
        P = self.p
        a = self.rand_dir()
        e1, e2 = basis(a)
        c0 = self.rng.uniform(-0.35, 0.35)           # small-circle offset: curved arcs
        d = np.arcsin(np.clip(dot(P, a), -1.0, 1.0)) - c0
        reach = (6.0 if kind == "triple" else 4.0) * (w if kind != "band" else w * 3.0)
        sel = np.nonzero(np.abs(d) < reach + 0.02)
        if sel[0].size == 0:
            return
        q = P[sel]
        th = np.arctan2(dot(q, e2), dot(q, e1))
        t0 = self.rng.uniform(0.0, 2.0 * math.pi)
        rel = np.mod(th - t0, 2.0 * math.pi)
        along = smooth(0.0, 0.12 * length, rel) * smooth(0.0, 0.12 * length, length - rel) * (rel < length)
        wig = (vnoise(q, 7.0, self.seed + 31 + int(t0 * 100)) - 0.5) * 0.02 + (vnoise(q, 30.0, self.seed + 37) - 0.5) * 0.004
        x = d[sel] + wig
        h = self.h[sel]
        if kind == "band":
            bw = w * 3.0
            inside = 1.0 - smooth(bw * 0.8, bw * 1.1, np.abs(x))
            stri = 0.08 * np.sin(x / bw * math.pi * 6.0)
            prof = amp * (-0.25 + stri)
            m = inside * along
            self.h[sel] = h * (1.0 - m) + (h * 0.15 + prof) * m
            self.paint(sel, self.C["band"], m * 0.75)
            self.paint(sel, tint, (1.0 - smooth(0.0, bw * 0.5, np.abs(np.abs(x) - bw))) * along * 0.35)
            return
        s = 0.55 * w
        prof = amp * (gauss(x - w, s) + gauss(x + w, s) - 0.45 * gauss(x, 0.6 * s))
        foot = gauss(x, 2.2 * w) * along
        self.h[sel] = h * (1.0 - 0.85 * foot) + prof * along
        margin = gauss(x, (5.0 if kind == "triple" else 2.2) * w) * along
        self.paint(sel, tint, np.minimum(margin * (0.85 if kind == "triple" else 0.6) * self.cfg["lineae"]["margin_gain"], 1.0))
        if kind == "triple":
            self.paint(sel, self.C["plains"] * 1.03, gauss(x, 0.7 * w) * along * 0.7)

    def lineae(self, n, major):
        c = self.cfg["lineae"]
        lo, hi = c["half_width_rad"]
        for _ in range(n):
            r = self.rng.random()
            kind = "band" if r < c["band_share"] else ("triple" if r < c["band_share"] + c["triple_share"] and major else "double")
            w = self.rng.uniform(lo, hi) * (1.0 if major else 0.55)
            amp = c["height"] * (w / hi) ** 0.5 * (1.0 if major else 0.6)
            length = self.rng.uniform(0.6, 2.6) if major else self.rng.uniform(0.08, 0.7)
            tint = self.C["lineae"] if self.rng.random() < 0.6 else self.C["lineae_ochre"]
            tint = tint * self.rng.uniform(0.92, 1.06)
            if not major:
                tint = tint * 0.75 + self.C["plains"] * 0.25
            self.lineament(kind, w, amp, length, tint)

    def chaos(self):
        from scipy.spatial import cKDTree
        c, P = self.cfg["chaos"], self.p
        for k in range(c["regions"]):
            ctr = self.rand_dir().astype(np.float32)
            R = self.rng.uniform(*c["radius_rad"])
            ang = np.arccos(np.clip(dot(P, ctr), -1.0, 1.0))
            sel = np.nonzero(ang < R * 1.5)
            q = P[sel]
            edge = ang[sel] / R + 0.8 * (fbm(q, 4.0 / R * 0.1, self.seed + 200 + k, 4) - 0.5)
            inside = 1.0 - smooth(0.85, 1.0, edge)
            # blocks: a Voronoi of seeds scattered over the patch; some cells stay up as tilted plates
            nseed = int((R / c["block_size_rad"]) ** 2 * 2.2) + 8
            e1, e2 = basis(ctr)
            rr = R * 1.3 * np.sqrt(self.rng.random(nseed))
            tt = self.rng.random(nseed) * 2.0 * math.pi
            seeds = np.array([unit(ctr + e1 * r * math.cos(t) + e2 * r * math.sin(t)) for r, t in zip(rr, tt)], np.float32)
            dist, idx = cKDTree(seeds).query(q, k=2)
            f1, f2 = dist[:, 0], dist[:, 1]
            near = idx[:, 0]
            up = self.rng.random(nseed) < c["block_share"]
            tilt = self.rng.normal(size=(nseed, 3)).astype(np.float32) * 6.0
            lift = self.rng.uniform(0.15, 0.45, nseed).astype(np.float32)
            block = up[near] * smooth(0.0015, 0.004, f2 - f1)
            hb = lift[near] + np.sum((q - seeds[near]) * tilt[near], -1)
            matrix_h = -0.3 + 0.22 * fbm(q, 140.0, self.seed + 230 + k, 3) + 0.1 * (fbm(q, 40.0, self.seed + 240, 3) - 0.5)
            h = self.h[sel]
            new = block * (h * 0.6 + hb) + (1.0 - block) * matrix_h
            self.h[sel] = h * (1.0 - inside) + new * inside
            mtint = self.C["chaos"] * (0.85 + 0.3 * fbm(q, 60.0, self.seed + 250 + k, 3))[:, None]
            self.paint(sel, mtint, inside * (1.0 - block) * 0.95)
            # the broken plates' rims and the patch's margin stained red-brown too
            self.paint(sel, self.C["lineae"], (1.0 - smooth(0.6, 1.3, edge)) * smooth(0.6, 1.0, edge) * 0.5)

    def craters(self):
        c, P = self.cfg["craters"], self.p
        for k in range(c["count"]):
            ctr = self.rand_dir().astype(np.float32)
            rayed = k < c["rayed"]
            R = self.rng.uniform(*c["radius_rad"]) if not rayed else c["radius_rad"][1] * 0.8
            ang = np.arccos(np.clip(dot(P, ctr), -1.0, 1.0))
            reach = R * (14.0 if rayed else 4.0)
            sel = np.nonzero(ang < reach)
            q = P[sel]
            x = ang[sel] / R
            x = x * (1.0 + 0.08 * (vnoise(q, 0.6 / R, self.seed + 300 + k) - 0.5))
            D = 0.9 * min(R / 0.02, 1.6)
            bowl = np.where(x < 1.0, -D * (1.0 - x * x), 0.0)
            rim = 0.3 * D * gauss(x - 1.0, 0.18)
            peak = (0.4 * D * gauss(x, 0.16)) if R > 0.015 else 0.0
            blanket = 0.12 * D * np.exp(-(x - 1.0) * 1.5) * (x > 1.0)
            h = self.h[sel]
            fresh = 1.0 - smooth(1.0, 1.4, x)
            self.h[sel] = h * (1.0 - fresh) + (bowl + rim + peak) + blanket + h * fresh * 0.1
            self.paint(sel, self.C["ejecta"], (1.0 - smooth(1.0, 3.2, x)) * 0.75)
            self.paint(sel, self.C["mottle"] * 0.9, (1.0 - smooth(0.5, 0.95, x)) * 0.35)
            if rayed:
                e1, e2 = basis(ctr)
                az = np.arctan2(dot(q, e2), dot(q, e1))
                ring = np.stack([np.cos(az), np.sin(az), np.zeros_like(az)], -1) * 3.0
                ray = smooth(0.62, 0.8, vnoise(ring, 1.0, self.seed + 333) * 0.6 + vnoise(ring * 3.0, 1.0, self.seed + 334) * 0.4)
                self.paint(sel, self.C["ejecta"], ray * np.exp(-(x - 1.0) / 4.0) * (x > 1.0) * 0.8)

    def bake(self):
        cfg = self.cfg
        self.plains()
        self.lineae(cfg["lineae"]["minor"], False)
        self.lineae(cfg["lineae"]["major"] // 2, True)
        self.chaos()
        self.lineae(cfg["lineae"]["major"] - cfg["lineae"]["major"] // 2, True)   # younger ridges cross the chaos
        self.craters()
        return self.h, np.clip(self.col, 0.0, 1.0)


def normals(h, cfg):
    """The relief normal (east, north, up) from the height field: height unit = relief_scale of Europa's radius."""
    H, W = h.shape
    k = cfg["relief_scale"] * cfg["normal_strength"]
    lat = (0.5 - (np.arange(H) + 0.5) / H) * math.pi
    dlon, dlat = 2.0 * math.pi / W, math.pi / H
    he = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) / (2.0 * dlon * np.maximum(np.cos(lat), 0.05)[:, None])
    hn = np.zeros_like(h)
    hn[1:-1] = (h[:-2] - h[2:]) / (2.0 * dlat)                    # row 0 is north
    n = np.stack([-he * k, -hn * k, np.ones_like(h)], -1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def srgb(c):
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1.0 / 2.4) - 0.055)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--preview", help="also write a lit preview sheet (full, half, crescent) here")
    args = ap.parse_args()
    cfg = json.load(open(PARAMS))
    h, col = Surface(cfg).bake()
    n = normals(h, cfg)
    Image.fromarray((srgb(col) * 255.0 + 0.5).astype(np.uint8)).save(OUT_ALBEDO, optimize=True)
    rel = np.concatenate([n[..., :2] * 0.5 + 0.5, np.clip(h * HEIGHT_GAIN + 0.5, 0.0, 1.0)[..., None],
                          np.ones_like(h)[..., None]], -1)
    Image.fromarray((rel * 255.0 + 0.5).astype(np.uint8)).save(OUT_RELIEF, optimize=True)
    print("europa_bake: seed %d, %d x %d, height %.2f..%.2f -> %s, %s" % (
        cfg["seed"], cfg["width"], cfg["height"], h.min(), h.max(), OUT_ALBEDO, OUT_RELIEF))
    if args.preview:
        preview(col, n, args.preview)


def preview(col, n, path, size=420, centre_lon=285.0):
    """Europa as seen from the coast at three phases (Lambert + relief normals), without the shader's shadows."""
    H, W, _ = col.shape
    ys, xs = np.mgrid[0:size, 0:size]
    u, v = (xs + 0.5) / size * 2.0 - 1.0, 1.0 - (ys + 0.5) / size * 2.0
    r2 = u * u + v * v
    on = r2 < 1.0
    z = np.sqrt(np.clip(1.0 - r2, 0.0, 1.0))
    lat = np.arcsin(np.clip(v, -1, 1))
    lon = np.radians(centre_lon) + np.arctan2(u, z)
    px = (np.mod(lon / (2 * math.pi), 1.0) * W).astype(int) % W
    py = np.clip(((0.5 - lat / math.pi) * H).astype(int), 0, H - 1)
    alb, nn = col[py, px], n[py, px]
    # the tangent frame on the view sphere: east, north, out
    N = np.stack([u, v, z], -1)
    E = np.stack([z, np.zeros_like(z), -u], -1)
    E = E / np.maximum(np.linalg.norm(E, axis=-1, keepdims=True), 1e-6)
    No = np.cross(N, E)
    sheet = []
    for phase in (0.0, 70.0, 110.0):
        s = np.array([math.sin(math.radians(phase)), 0.0, math.cos(math.radians(phase))])
        geo = dot(N, s)
        rn = E * nn[..., :1] + No * nn[..., 1:2] + N * nn[..., 2:3]
        lit = np.clip(dot(rn, s), 0.0, None) * smooth(-0.02, 0.05, geo)
        img = srgb(np.clip(alb * lit[..., None] * 1.1, 0, 1)) * on[..., None]
        sheet.append(img)
    Image.fromarray((np.concatenate(sheet, 1) * 255).astype(np.uint8)).save(path)
    print("preview ->", path)


if __name__ == "__main__":
    main()
