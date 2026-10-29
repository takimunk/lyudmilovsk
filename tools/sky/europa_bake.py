"""Europa's surface for the moon in the sky, made procedurally and baked once (Dima, 2026-10-29: our own Europa, its
relief real 3D, then fixed): a height field and an albedo on the sphere, written as two equirectangular PNGs that
unreal/scripts/import_sky.py imports and tools/sky/sky_hlsl.py MOON_HLSL renders (relief normal mapping, the relief's
own shadows near the terminator).

    python3 tools/sky/europa_bake.py [--preview out.png] [--detail out.png]

Reads tools/sky/europa_bake.json (the seed and every parameter: the same file gives the same maps), writes
textures/sky/europa_albedo.png (RGB, sRGB) and textures/sky/europa_relief.png (RGBA, linear: R, G = the relief normal's
east and north components * 0.5 + 0.5, B = height * HEIGHT_GAIN + 0.5, A = 1). East longitude 0..360 left to right,
north up, as the shader samples it.

How it is made (v4, after Galileo's close-ups of the ridged plains; no craters, no stamped plates):
- the plains: ridged multifractal noise from continent-sized swells down to a pixel (hummocky, ridged), with broad
  low regions;
- a fracture network grown in a tidal stress field: each epoch the field is turned a little in longitude (the shell
  drifting over the tides, non-synchronous rotation), so each generation of cracks runs another way and crosscuts the
  older ones. Thousands of cracks at four scales (global lineae, cycloidal chains of arcs with cusps, regional
  ridges, and a dense fine net that makes the plains' grain), each traced step by step along the field with its own
  wander. Oldest first: a younger ridge flattens what was under it and stands on top;
- every crack is a double ridge (two crests, a central trough), softer and lower the older it is; some global ones
  are triple bands (wide dark red-brown margins), a few smooth grey striated bands;
- the colour from height and age: troughs and low ground red-brown, ridge crests bright white, old terrain mottled
  brown, young ice bluish white, the margins and bands painted by their cracks.
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

def _blur(x, sigma):
    """Gaussian blur of a lat/lon field: wrapping in longitude, reflecting at the poles."""
    from scipy.ndimage import gaussian_filter
    return gaussian_filter(x, sigma, mode=("reflect", "wrap"))


def ridged(P, freq, seed, octaves, lacunarity=2.07, gain=0.55):
    """Ridged multifractal (Musgrave): sharp crests, each octave weighted by the one above it."""
    out, w, a, tot = 0.0, 1.0, 1.0, 0.0
    for o in range(octaves):
        r = 1.0 - np.abs(vnoise(P, freq * lacunarity ** o, seed + 71 * o) * 2.0 - 1.0)
        r = r * r * w
        out = out + r * a
        tot += a
        w = np.clip(r * 2.0, 0.0, 1.0)
        a *= gain
    return out / tot


class Surface:
    def __init__(self, cfg):
        self.cfg = cfg
        self.rng = np.random.default_rng(cfg["seed"])
        self.seed = cfg["seed"]
        W, H = cfg["width"], cfg["height"]
        self.W, self.H = W, H
        self.lon1 = (np.arange(W) + 0.5) / W * 2.0 * math.pi
        self.lat1 = (0.5 - (np.arange(H) + 0.5) / H) * math.pi
        lat = np.repeat(self.lat1[:, None], W, 1).astype(np.float32)
        lo = np.repeat(self.lon1[None, :], H, 0)
        cl = np.cos(lat)
        self.p = np.stack([cl * np.cos(lo), cl * np.sin(lo), np.sin(lat)], -1).astype(np.float32)
        self.h = np.zeros((H, W), np.float32)
        self.age = np.ones((H, W), np.float32)          # 1 = the oldest plains, 0 = the youngest ice
        self.margin = np.zeros((H, W), np.float32)      # red-brown stain along lineae
        self.band = np.zeros((H, W), np.float32)        # smooth grey bands
        self.C = {k: np.array(v, np.float32) for k, v in cfg["colours"].items()}
        self.fields = []

    def rand_dir(self):
        return unit(self.rng.normal(size=3))

    # -- the plains ---------------------------------------------------------------------------------------------
    def plains(self):
        c, P = self.cfg["plains"], self.p
        hum = ridged(P, c["ridged_freq"], self.seed + 1, c["ridged_octaves"])
        swell = fbm(P, c["swell_freq"], self.seed + 2, 4)
        self.low = smooth(0.5, 0.25, swell)                       # broad low regions
        self.h += c["ridged"] * (hum - 0.35) + c["swell"] * (swell - 0.5)
        self.h += c["hummock"] * (fbm(P, c["hummock_freq"], self.seed + 3, 3) - 0.5)
        # younger and older provinces of the plains
        self.age[:] = np.clip(0.75 + 0.5 * (fbm(P, c["age_freq"], self.seed + 4, 4) - 0.5) * 2.0, 0.35, 1.0)

    # -- the stress field ---------------------------------------------------------------------------------------
    def make_fields(self, epochs):
        """One orientation field per epoch on a coarse grid: the angle of the cracks from local east. A tidal
        pattern (diagonal in mid latitudes, swinging with longitude) turned by the epoch's drift, bent by noise."""
        f = self.cfg["fractures"]
        gw, gh = 256, 128
        lon = (np.arange(gw) + 0.5) / gw * 2.0 * math.pi
        lat = (0.5 - (np.arange(gh) + 0.5) / gh) * math.pi
        LA, LO = np.meshgrid(lat, lon, indexing="ij")
        cl = np.cos(LA)
        Pg = np.stack([cl * np.cos(LO), cl * np.sin(LO), np.sin(LA)], -1).astype(np.float32)
        for k in range(epochs):
            drift = math.radians(f["drift_deg"]) * k + self.rng.uniform(-0.2, 0.2)
            th = (f["tidal"] * np.sin(2.0 * (LO - drift)) * np.cos(LA) + 0.9 * np.sin(LA) * np.cos(LO - drift)
                  + f["bend"] * (fbm(Pg, 1.6, self.seed + 500 + k, 4) - 0.5) * 2.0 * math.pi)
            self.fields.append(th.astype(np.float32))

    def theta(self, k, q):
        th = self.fields[k]
        gh, gw = th.shape
        lat = math.asin(max(-1.0, min(1.0, q[2])))
        lon = math.atan2(q[1], q[0]) % (2.0 * math.pi)
        return float(th[min(int((0.5 - lat / math.pi) * gh), gh - 1), int(lon / (2.0 * math.pi) * gw) % gw])

    # -- tracing a crack ----------------------------------------------------------------------------------------
    def trace(self, k, p, length, step, follow, wander, cycloid=None):
        """A path on the sphere along epoch k's field: points (n, 3). cycloid = (arc length, turn per arc): the path
        curves one way along each arc and snaps back at a cusp."""
        t = None
        pts = [p]
        s, arc_s, turn_sign = 0.0, 0.0, (1.0 if self.rng.random() < 0.5 else -1.0)
        drift = 0.0
        while s < length:
            e = np.cross([0.0, 0.0, 1.0], p)
            ne = np.linalg.norm(e)
            e = e / ne if ne > 1e-6 else np.array([1.0, 0.0, 0.0])
            n = np.cross(p, e)
            th = self.theta(k, p)
            d = math.cos(th) * e + math.sin(th) * n
            if t is None:
                t = d
            if np.dot(d, t) < 0.0:
                d = -d
            t = unit(t * (1.0 - follow) + d * follow)
            drift += self.rng.normal() * wander
            drift *= 0.85
            ang = drift * step
            if cycloid:
                arc_len, arc_turn = cycloid
                ang += turn_sign * arc_turn / arc_len * step
                arc_s += step
                if arc_s >= arc_len:          # the cusp
                    arc_s = 0.0
                    ang -= turn_sign * arc_turn * self.rng.uniform(0.9, 1.15)
            b = np.cross(p, t)
            t = t * math.cos(ang) + b * math.sin(ang)
            p = unit(p + t * step)
            t = unit(t - p * np.dot(t, p))
            pts.append(p)
            s += step
        return np.array(pts, np.float32)

    def candidates(self, pts, reach):
        """Flat indices of the map's pixels within reach of the path (by lat/lon windows over chunks of it)."""
        H, W = self.H, self.W
        lat = np.arcsin(np.clip(pts[:, 2], -1.0, 1.0))
        lon = np.mod(np.arctan2(pts[:, 1], pts[:, 0]), 2.0 * math.pi)
        out = []
        n = len(pts)
        chunk = max(int(0.12 / max(np.linalg.norm(pts[1] - pts[0]), 1e-6)), 2) if n > 1 else 1
        for i in range(0, n, chunk):
            la, lo = lat[i:i + chunk + 1], lon[i:i + chunk + 1]
            r0 = max(int((0.5 - (la.max() + reach) / math.pi) * H) - 1, 0)
            r1 = min(int((0.5 - (la.min() - reach) / math.pi) * H) + 2, H)
            cmin = max(math.cos(min(abs(la).max() + reach, 1.55)), 0.0)
            if cmin < 0.03:
                cols = np.arange(W)
            else:
                lo0 = lo[0]
                rel = np.mod(lo - lo0 + math.pi, 2.0 * math.pi) - math.pi
                rw = reach / cmin
                c0 = int(math.floor((lo0 + rel.min() - rw) / (2.0 * math.pi) * W)) - 1
                c1 = int(math.ceil((lo0 + rel.max() + rw) / (2.0 * math.pi) * W)) + 2
                cols = np.arange(c0, c1) % W if c1 - c0 < W else np.arange(W)
            rows = np.arange(r0, r1)
            out.append((rows[:, None] * W + cols[None, :]).ravel())
        return np.unique(np.concatenate(out))

    def crack(self, pts, w, amp, age, kind, margin):
        """Lay one crack along the path: the double ridge's relief over what was there, its age, its stains."""
        from scipy.spatial import cKDTree
        if len(pts) < 3:
            return
        reach = w * (7.0 if kind == "triple" else 4.0 if kind == "band" else 3.2)
        idx = self.candidates(pts, reach)
        P = self.p.reshape(-1, 3)
        q = P[idx]
        tan = np.gradient(pts, axis=0)
        tan /= np.linalg.norm(tan, axis=1, keepdims=True)
        seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
        sal = np.concatenate([[0.0], np.cumsum(seg)])
        L = sal[-1]
        dist, j = cKDTree(pts).query(q, distance_upper_bound=reach + seg.max())
        ok = np.isfinite(dist)
        if not ok.any():
            return
        idx, q, j = idx[ok], q[ok], j[ok]
        dq = q - pts[j]
        nrm = np.cross(pts[j], tan[j])
        x = np.abs(np.sum(dq * nrm, 1))
        along = sal[j] + np.sum(dq * tan[j], 1)
        keep = x < reach
        idx, x, along = idx[keep], x[keep], along[keep]
        end = min(0.15 * L, 6.0 * w)
        taper = smooth(-0.5 * w, end, along) * smooth(-0.5 * w, end, L - along)
        hf, af, mf, bf = (a.reshape(-1) for a in (self.h, self.age, self.margin, self.band))
        h = hf[idx]
        soft = 1.0 + 0.9 * age                                   # old ridges are worn
        a_eff = amp * (1.0 - 0.55 * age)
        if kind == "band":
            bw = 3.2 * w
            inside = (1.0 - smooth(bw * 0.75, bw * 1.15, x)) * taper
            prof = a_eff * (-0.18 + 0.07 * np.sin(x / bw * math.pi * 7.0))
            hf[idx] = h * (1.0 - inside) + (h * 0.1 + prof) * inside
            bf[idx] = np.maximum(bf[idx] * (1.0 - inside), inside * (1.0 - 0.5 * age))
            af[idx] = af[idx] * (1.0 - inside) + age * inside
            mf[idx] = np.maximum(mf[idx] * (1.0 - inside), gauss(x - bw, 0.5 * bw) * taper * margin * 0.6)
            return
        s = 0.5 * w * soft
        prof = a_eff * (gauss(x - w, s) + gauss(x + w, s) - 0.55 * gauss(x, 0.55 * w * soft))
        foot = gauss(x, 2.0 * w * soft) * taper
        hf[idx] = h * (1.0 - 0.9 * foot) + prof * taper
        af[idx] = af[idx] * (1.0 - foot) + age * foot
        mw = (4.5 if kind == "triple" else 1.8) * w
        mf[idx] = np.maximum(mf[idx] * (1.0 - 0.6 * foot), gauss(x, mw) * taper * margin)
        bf[idx] = bf[idx] * (1.0 - foot)

    # -- the network ------------------------------------------------------------------------------------------
    def fractures(self):
        f = self.cfg["fractures"]
        E = f["epochs"]
        self.make_fields(E)
        jobs = []
        for name, sc in f["scales"].items():
            for _ in range(sc["count"]):
                k = int(self.rng.integers(sc["epochs"][0], sc["epochs"][1] + 1))
                # age: the epoch (oldest first) with a little spread inside it
                age = float(np.clip(1.0 - (k + self.rng.random()) / E, 0.0, 1.0))
                jobs.append((age, name, k))
        # older first: the young cut the old
        jobs.sort(key=lambda j: -j[0])
        # fine cracks swarm: start points weighted by a regional density
        P = self.p
        dens = fbm(P[::8, ::8], 3.0, self.seed + 600, 3)
        for n, (age, name, k) in enumerate(jobs):
            sc = f["scales"][name]
            while True:
                p0 = self.rand_dir()
                la = math.asin(p0[2])
                lo = math.atan2(p0[1], p0[0]) % (2 * math.pi)
                dv = dens[min(int((0.5 - la / math.pi) * dens.shape[0]), dens.shape[0] - 1),
                          int(lo / (2 * math.pi) * dens.shape[1]) % dens.shape[1]]
                if self.rng.random() < 0.25 + 1.2 * max(dv - 0.35, 0.0) or name in ("global", "cycloid"):
                    break
            w = self.rng.uniform(*sc["half_width_rad"])
            L = self.rng.uniform(*sc["length_rad"])
            amp = sc["height"] * self.rng.uniform(0.7, 1.15)
            step = min(0.6 * w, 0.004)
            cyc = None
            if name == "cycloid":
                cyc = (self.rng.uniform(*f["cycloid_arc_rad"]), self.rng.uniform(*f["cycloid_turn"]))
            pts = self.trace(k, p0.astype(np.float64), L, step, sc["follow"], sc["wander"], cyc)
            r = self.rng.random()
            kind = "double"
            if name == "global" and r < f["band_share"]:
                kind = "band"
            elif name in ("global", "cycloid") and r < f["band_share"] + f["triple_share"]:
                kind = "triple"
            margin = sc["margin"] * self.rng.uniform(0.6, 1.2) * (1.0 - 0.6 * age)
            self.crack(pts, w, amp, age, kind, margin)
            if n % 2000 == 0:
                print("  cracks %d / %d" % (n, len(jobs)))

    # -- colour -----------------------------------------------------------------------------------------------
    def colour(self):
        c, C, P = self.cfg["colour_rules"], self.C, self.p
        A = smooth(0.15, 0.95, self.age)[..., None]
        col = C["young_ice"] * (1.0 - A) + C["plains"] * A
        mott = smooth(0.36, 0.68, fbm(P, c["mottle_freq"], self.seed + 700, 5)) * (0.55 + 0.9 * fbm(P, c["mottle_freq"] * 7.0, self.seed + 701, 3))
        mott = np.clip(mott * c["mottle"] * smooth(0.3, 1.0, self.age), 0.0, 1.0)[..., None]
        col = col * (1.0 - mott) + C["mottle"] * mott
        # height: local troughs red-brown, crests bright; broad low ground red-brown too
        hp = self.h - _blur(self.h, c["relief_blur_px"])
        trough = smooth(0.0, -c["trough_h"], hp)[..., None]
        crest = smooth(0.0, c["crest_h"], hp)[..., None]
        regional = self.h - _blur(self.h, c["regional_blur_px"])
        low = np.clip(smooth(0.0, -c["low_h"], _blur(self.h, 4.0) - _blur(self.h, c["regional_blur_px"])) * 0.6
                      + self.low * c["low_regions"], 0.0, 1.0)[..., None]
        col = col * (1.0 - c["low"] * low) + C["lowland"] * c["low"] * low
        col = col * (1.0 - c["trough"] * trough) + C["trough"] * c["trough"] * trough
        col = col * (1.0 - c["crest"] * crest) + C["crest"] * c["crest"] * crest
        m = np.clip(self.margin, 0.0, 1.0)[..., None]
        tint = C["lineae"] * (1.0 - fbm(P, 40.0, self.seed + 702, 3)[..., None]) + C["lineae_ochre"] * fbm(P, 40.0, self.seed + 702, 3)[..., None]
        col = col * (1.0 - m) + tint * m
        b = np.clip(self.band, 0.0, 1.0)[..., None] * 0.8
        col = col * (1.0 - b) + C["band"] * b
        # a faint fine grain of brightness, so the albedo isn't flat between ridges
        col *= (0.94 + 0.12 * vnoise(P, 900.0, self.seed + 703))[..., None]
        del regional
        return np.clip(col, 0.0, 1.0)

    def bake(self):
        print("  plains")
        self.plains()
        print("  fractures")
        self.fractures()
        print("  colour")
        return self.h, self.colour()


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
    ap.add_argument("--detail", help="also write a lit 1:1 crop of the map here (low sun from the west)")
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
    if args.detail:
        H, W = h.shape
        y0, x0 = H // 2 - 400, int(285.0 / 360.0 * W) - 600
        cn, cc = n[y0:y0 + 800, x0:x0 + 1200], col[y0:y0 + 800, x0:x0 + 1200]
        s = np.array([-0.8, 0.0, 0.6])
        lit = np.clip(cn[..., 0] * s[0] + cn[..., 2] * s[2], 0.0, None) / s[2]
        Image.fromarray((srgb(np.clip(cc * lit[..., None] * 0.8, 0, 1)) * 255).astype(np.uint8)).save(args.detail)
        Image.fromarray((srgb(cc) * 255).astype(np.uint8)).save(args.detail.replace(".png", "_albedo.png"))


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
