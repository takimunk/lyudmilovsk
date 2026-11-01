"""The Europa hatch's textures (docs/europa.md): procedural, no outside source, the same seed gives the same pictures.

    python3 tools/europa/europa_textures.py            -> textures/europa/*.png

  T_EU_<name>_C.png   tiling surfaces of the airlock kit (blender/europa/europa_kit.py's tx_* slots, metre UVs):
                      RGB colour, A roughness. paint (the Soviet teal-green of the room and the hatches, chipped,
                      rust-streaked), steel (worn dark steel of the ladders, rims, grating), concrete (the room's floor
                      and the collar), algae (the sea collar's crust)
  T_EU_ice_C.png      Europa's ice, tiling 16 m: bluish white with fine cracks and frost; T_EU_ice_N.png its normal
  T_EU_planet_C.png   the planet over Europa: ocean all round and one small island, Lyudmilovsk (equirectangular,
                      lon 0 at the centre); T_EU_clouds.png its clouds (grey = cover), with a clear patch over the island

Noise: tiling surfaces from periodic FFT noise; the planet's from 3D value noise on the sphere (tools/sky/europa_bake.py's,
so the poles and the seam are whole). Pure numpy + PIL.
"""
import math
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "textures", "europa")
sys.path.insert(0, os.path.join(REPO, "tools", "sky"))
import europa_bake as EB  # noqa: E402  (vnoise / fbm on 3D points)

SEED = 4417
# the island on the planet (degrees): where the town really is (ALyudSky's place), so the planet seen from Europa turns
# it into view only while Europa stands over the town's horizon (ALyudEuropaSite::SkyAt)
ISLAND_LON, ISLAND_LAT = 20.40, 54.96
ISLAND_SIZE = 5.0          # degrees across, roughly


def periodic_noise(n, beta, seed):
    """Tiling noise n x n with a 1/f^beta spectrum, normalised to 0..1."""
    rng = np.random.default_rng(seed)
    f = np.fft.fftfreq(n)
    fx, fy = np.meshgrid(f, f)
    r = np.sqrt(fx * fx + fy * fy)
    r[0, 0] = 1.0
    spec = (rng.normal(size=(n, n)) + 1j * rng.normal(size=(n, n))) / r ** beta
    spec[0, 0] = 0
    a = np.real(np.fft.ifft2(spec))
    a -= a.min()
    return a / a.max()


def band(n, lo, hi, seed):
    """Tiling noise with only the frequencies lo..hi (cycles per tile): blotches of one size."""
    rng = np.random.default_rng(seed)
    f = np.fft.fftfreq(n) * n
    fx, fy = np.meshgrid(f, f)
    r = np.sqrt(fx * fx + fy * fy)
    keep = (r >= lo) & (r <= hi)
    spec = (rng.normal(size=(n, n)) + 1j * rng.normal(size=(n, n))) * keep
    a = np.real(np.fft.ifft2(spec))
    a -= a.min()
    return a / max(a.max(), 1e-9)


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def srgb(c):
    return np.array(c, dtype=np.float32)


def save_rgba(name, rgb, rough):
    img = np.concatenate([np.clip(rgb, 0, 1), np.clip(rough, 0, 1)[..., None]], axis=-1)
    Image.fromarray((img * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT, name))
    print("wrote", name)


def save_rgb(name, rgb):
    Image.fromarray((np.clip(rgb, 0, 1) * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT, name))
    print("wrote", name)


def streaks(n, seed, count, length):
    """Tiling vertical runs (rust weeping down from rivets and seams): 0..1."""
    rng = np.random.default_rng(seed)
    a = np.zeros((n, n), np.float32)
    for _ in range(count):
        x = rng.integers(0, n)
        y0 = rng.integers(0, n)
        ln = int(length * (0.4 + rng.random()))
        w = rng.uniform(1.5, 5.0)
        ys = (y0 + np.arange(ln)) % n
        fade = np.linspace(1.0, 0.0, ln) ** 1.5 * rng.uniform(0.4, 1.0)
        xs = np.arange(-8, 9)
        prof = np.exp(-(xs / w) ** 2)
        for k, dx in enumerate(xs):
            a[ys, (x + dx) % n] = np.maximum(a[ys, (x + dx) % n], fade * prof[k])
    return a


def paint(n=1024):
    """The teal-green paint over steel, one tile = 2 m: chips down to the primer and the metal, rust weeping from them."""
    base = srgb((0.20, 0.36, 0.33))
    tone = periodic_noise(n, 1.4, SEED + 1)
    chip_field = band(n, 6, 40, SEED + 2) * 0.7 + periodic_noise(n, 0.9, SEED + 3) * 0.3
    chips = smooth(0.80, 0.83, chip_field)
    primer = smooth(0.77, 0.79, chip_field) - chips
    rust = streaks(n, SEED + 4, 40, n * 0.35)
    rust = np.maximum(rust * 0.55, chips * 0.5)
    col = base[None, None, :] * (0.82 + 0.3 * tone[..., None])
    col = col * (1 - primer[..., None]) + srgb((0.36, 0.24, 0.19))[None, None, :] * primer[..., None]
    col = col * (1 - chips[..., None]) + srgb((0.30, 0.28, 0.26))[None, None, :] * chips[..., None]
    col = col * (1 - 0.65 * rust[..., None]) + srgb((0.38, 0.17, 0.07))[None, None, :] * (0.65 * rust[..., None])
    rough = 0.55 + 0.25 * tone + 0.2 * rust - 0.15 * chips
    save_rgba("T_EU_paint_C.png", col, rough)


def steel(n=1024):
    """Worn dark steel, one tile = 1 m: brushed along U, darker grime in the low places, bright wear."""
    rng = np.random.default_rng(SEED + 10)
    tone = periodic_noise(n, 1.6, SEED + 11)
    brush = np.real(np.fft.ifft(np.fft.fft(rng.normal(size=(n, n)), axis=1) * (np.abs(np.fft.fftfreq(n)) < 0.02)[None, :], axis=1))
    brush = (brush - brush.min()) / (brush.max() - brush.min())
    grime = smooth(0.55, 0.85, periodic_noise(n, 1.2, SEED + 12))
    v = 0.30 + 0.10 * tone + 0.06 * brush - 0.12 * grime
    col = np.stack([v, v * 1.01, v * 1.03], -1)
    col = col * (1 - 0.3 * grime[..., None]) + srgb((0.25, 0.14, 0.08))[None, None, :] * (0.3 * grime[..., None])
    rough = 0.38 + 0.2 * grime + 0.1 * tone
    save_rgba("T_EU_steel_C.png", col, rough)


def concrete(n=1024):
    """Grey concrete, one tile = 2 m: mottled, small pores, a damp darker lower band is the material's (not here)."""
    rng = np.random.default_rng(SEED + 20)
    tone = periodic_noise(n, 1.3, SEED + 21)
    fine = periodic_noise(n, 0.4, SEED + 22)
    pores = (rng.random((n, n)) > 0.9985).astype(np.float32)
    pores = np.maximum(pores, np.roll(pores, 1, 0) * 0.7)
    v = 0.46 + 0.12 * tone + 0.06 * (fine - 0.5) - 0.25 * pores
    col = np.stack([v * 1.02, v, v * 0.96], -1)
    save_rgba("T_EU_concrete_C.png", col, 0.85 + 0.1 * fine)


def algae(n=1024):
    """The sea collar's crust, one tile = 1 m: olive-brown film with darker tufts and pale barnacle dots."""
    rng = np.random.default_rng(SEED + 30)
    tone = periodic_noise(n, 1.5, SEED + 31)
    tufts = smooth(0.55, 0.8, band(n, 20, 70, SEED + 32))
    dots = (rng.random((n, n)) > 0.9993).astype(np.float32)
    for _ in range(3):
        dots = np.maximum(dots, np.maximum(np.roll(dots, 1, 0), np.roll(dots, 1, 1)))
    col = srgb((0.18, 0.21, 0.12))[None, None, :] * (0.7 + 0.5 * tone[..., None])
    col = col * (1 - 0.5 * tufts[..., None]) + srgb((0.08, 0.11, 0.05))[None, None, :] * (0.5 * tufts[..., None])
    col = col * (1 - dots[..., None]) + srgb((0.62, 0.60, 0.52))[None, None, :] * dots[..., None]
    save_rgba("T_EU_algae_C.png", col, 0.7 + 0.2 * tone)


def ice(n=2048):
    """Europa's surface ice, one tile = 16 m: bluish white, frost, fine red-brown cracks; with its normal map."""
    rng = np.random.default_rng(SEED + 40)
    tone = periodic_noise(n, 1.5, SEED + 41)
    fine = periodic_noise(n, 0.6, SEED + 42)
    # cracks: long gently wandering lines in a few directions, as the lineae run (tile-safe: integer directions
    # over the tile, a tiling warp); each family shows only in places
    u, v = np.meshgrid(np.arange(n) / n, np.arange(n) / n)
    warp = periodic_noise(n, 2.4, SEED + 43) - 0.5
    crack = np.zeros((n, n), np.float32)
    for k, (a, b) in enumerate(((3, 1), (1, -4), (-2, 5), (5, 2))):
        f = (a * u + b * v) + warp * (0.6 + 0.2 * k)
        line = np.abs(np.sin(f * math.pi * (2 + k)))
        width = 0.012 + 0.01 * band(n, 2, 5, SEED + 46 + k)
        c = 1.0 - smooth(0.0, 1.0, line / width)
        crack = np.maximum(crack, c * smooth(0.45, 0.65, band(n, 1, 4, SEED + 50 + k)))
    frost = smooth(0.6, 0.9, fine) * 0.6
    col = srgb((0.80, 0.85, 0.90))[None, None, :] * (0.86 + 0.18 * tone[..., None])
    col = col * (1 - 0.55 * crack[..., None]) + srgb((0.55, 0.36, 0.25))[None, None, :] * (0.55 * crack[..., None])
    col = col * (1 - frost[..., None]) + srgb((0.95, 0.96, 0.98))[None, None, :] * frost[..., None]
    rough = 0.35 + 0.35 * frost + 0.2 * crack
    save_rgba("T_EU_ice_C.png", col, rough)
    # normal from a height: cracks are grooves, frost a little raised, the rest gently lumpy
    h = 0.6 * tone + 0.25 * fine - 0.8 * crack + 0.2 * frost
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * n / 16.0 * 0.04
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * n / 16.0 * 0.04
    nrm = np.stack([-dx, dy, np.ones_like(dx)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    save_rgb("T_EU_ice_N.png", nrm * 0.5 + 0.5)
    _ = rng


def sphere_points(w, h):
    lon = (np.arange(w) + 0.5) / w * 2 * math.pi - math.pi          # -180..180, 0 at the centre
    lat = math.pi / 2 - (np.arange(h) + 0.5) / h * math.pi
    LON, LAT = np.meshgrid(lon, lat)
    P = np.stack([np.cos(LAT) * np.cos(LON), np.cos(LAT) * np.sin(LON), np.sin(LAT)], -1)
    return P, LON, LAT


def planet(w=2048, h=1024):
    """The planet: open ocean everywhere, one small island; clouds."""
    P, LON, LAT = sphere_points(w, h)
    ilon, ilat = math.radians(ISLAND_LON), math.radians(ISLAND_LAT)
    I = np.array([math.cos(ilat) * math.cos(ilon), math.cos(ilat) * math.sin(ilon), math.sin(ilat)])
    ang = np.degrees(np.arccos(np.clip((P * I).sum(-1), -1, 1)))          # degrees from the island
    # the ocean: deep blue, a little greener and lighter in a broad shallow round the island and at low latitudes
    deep = EB.fbm(P, 2.0, SEED + 50, octaves=5)
    shelf = np.exp(-(ang / (ISLAND_SIZE * 2.2)) ** 2)
    col = np.zeros(P.shape, np.float32)
    ocean = srgb((0.035, 0.11, 0.24))
    shallow = srgb((0.09, 0.30, 0.38))
    col[:] = ocean * (0.85 + 0.3 * deep[..., None])
    col = col * (1 - 0.6 * shelf[..., None]) + shallow * (0.6 * shelf[..., None])
    # the island: a blob with a ragged coast, sand round it, green inland, a darker forested upland on its south half
    coast = EB.fbm(P, 60.0, SEED + 51, octaves=4)
    r = ang / (ISLAND_SIZE * 0.5) + (coast - 0.5) * 0.9
    land = smooth(1.02, 0.92, r)
    sand = land * (1 - smooth(0.92, 0.75, r))
    green = srgb((0.20, 0.33, 0.12))
    forest = srgb((0.10, 0.20, 0.08))
    south = smooth(0.0, -0.6, (np.degrees(LAT) - ISLAND_LAT) / ISLAND_SIZE)
    landcol = green * (1 - south[..., None]) + forest * south[..., None]
    landcol = landcol * (1 - sand[..., None]) + srgb((0.72, 0.64, 0.46)) * sand[..., None]
    col = col * (1 - land[..., None]) + landcol * land[..., None]
    save_rgb("T_EU_planet_C.png", col)
    # clouds: swirls by latitude (a calm band at the equator, storm tracks at mid latitudes), kept off the island
    warp = np.stack([EB.fbm(P, 3.0, SEED + 60 + k, octaves=3) - 0.5 for k in range(3)], -1)
    Q = P + warp * 0.35
    c = EB.fbm(Q * np.array([1.0, 1.0, 2.2]), 4.0, SEED + 63, octaves=6)
    latd = np.degrees(LAT)
    belt = 0.45 + 0.25 * np.exp(-((np.abs(latd) - 45) / 18) ** 2) + 0.15 * np.exp(-(latd / 8) ** 2)
    cover = smooth(0.62 - 0.25 * belt, 0.85 - 0.2 * belt, c)
    cover *= smooth(ISLAND_SIZE * 0.9, ISLAND_SIZE * 3.0, ang)
    # G: the town's lights on the island's night side (a few warm clusters along its coast and one bright centre)
    lights = np.zeros_like(cover)
    rng = np.random.default_rng(SEED + 70)
    for k in range(9):
        a = rng.uniform(0, 2 * math.pi)
        rr = ISLAND_SIZE * (0.05 if k == 0 else rng.uniform(0.15, 0.4))
        clat = ISLAND_LAT + rr * math.sin(a)
        clon = ISLAND_LON + rr * math.cos(a) / math.cos(math.radians(ISLAND_LAT))
        C = np.array([math.cos(math.radians(clat)) * math.cos(math.radians(clon)),
                      math.cos(math.radians(clat)) * math.sin(math.radians(clon)), math.sin(math.radians(clat))])
        d = np.degrees(np.arccos(np.clip((P * C).sum(-1), -1, 1)))
        lights = np.maximum(lights, (1.0 if k == 0 else rng.uniform(0.35, 0.7)) * np.exp(-(d / (ISLAND_SIZE * (0.22 if k == 0 else 0.12))) ** 2))
    save_rgb("T_EU_clouds.png", np.stack([cover, np.clip(lights, 0, 1), cover], -1))


def main():
    os.makedirs(OUT, exist_ok=True)
    only = sys.argv[1:]
    for f in (paint, steel, concrete, algae, ice, planet):
        if not only or f.__name__ in only:
            f()


if __name__ == "__main__":
    main()
