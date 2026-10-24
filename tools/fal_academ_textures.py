#!/usr/bin/env python3
"""Tileable surface textures for the Academgorodok of Laboratory No. 2 (Moscow 1945-48): ochre lime plaster,
white trim plaster, dark rusticated stone, vermiculated quoins, red brick, standing-seam steel roof, stained
timber, asphalt, concrete, flowerbed soil and cast stone, plus a world-aligned grime/streak mask.

Two steps:

1. Fetch the raw photos through fal.ai (Nano Banana 2.1, 2K) into textures/academ/raw/ (not committed).
   Plain Python, standard library only. The key is read from FAL_API_KEY at runtime, never stored:
     FAL_API_KEY=... python3 tools/fal_academ_textures.py [raw ...]
2. Turn them into the textures the build uses (Blender, for its numpy):
     blender -b --factory-startup --python tools/fal_academ_textures.py -- post [texture ...]
   For each texture: even out the photo's light, make it seamless (cross-fade with its half-shifted self),
   or lay out its joints in code (brick, rustic blocks, roof seams) with the photo as the surface of each
   unit; match the mean colour to the target in blender/academgorodok/common.py; derive
     <name>_D.jpg  base colour (sRGB)
     <name>_N.jpg  tangent-space normal, DirectX green (Unreal); Blender flips G
     <name>_R.jpg  linear mask: R roughness (mean = target), G cavity AO, B height
   and academ_grime_mask.jpg (greyscale, white = dirt, 1 tile = 4 m), all into textures/academ/, plus
   textures/academ/academ_textures.json (tile size in metres, size, mean colour, roughness of each).

JPEG like textures/ground: these are big photo-like maps stored in Git LFS, and JPEG at q92 is ~4x smaller than
PNG with no visible loss at this noise level (props/ keeps PNG because those are small and few).
"""
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ENDPOINT = "google/nano-banana-2.1"
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "textures" / "academ"
RAW = OUT / "raw"
STYLE = ("Seamless tileable texture, photographed straight on, orthographic, perfectly flat even overcast light, "
         "no cast shadows, no perspective, no vignette, no text, no watermark, no objects, detail spread evenly "
         "with no single feature in the middle, fills the whole square edge to edge, photorealistic. ")

# raw photo name -> prompt (the material surface; joints/seams of brick, rustic and roof are laid out in code)
PROMPTS = {
    "plaster_ochre": "A 2 x 2 metre patch of an old facade wall of 1940s Moscow neoclassical housing: ochre / pale "
                     "yellow lime plaster painted with lime wash, matte, subtle trowel and float marks, faint grey "
                     "dirt and weathering, a few small patches where the paint has peeled showing paler grey "
                     "plaster underneath, a few hairline cracks, ",
    "plaster_white": "A 1 x 1 metre patch of smooth off-white painted lime plaster of a cornice or window surround, "
                     "matte chalky paint, very faint brush strokes, slight grey weathering, a few tiny chips, ",
    "rustic_rock": "Rough rock-faced (quarry-pitched) dark grey-black stone surface, coarse chisel-split bumps and "
                   "hollows, granular texture, slightly dusty, one continuous surface with no joints or edges, ",
    "quoin_vermiculated": "A 1 x 1 metre patch of the face of a vermiculated rusticated stone quoin: grey-beige "
                          "cast stone carved with dense irregular worm-tracked grooves (vermiculation) all over, "
                          "the raised flat islands between the winding channels, old paint, light dust in the "
                          "grooves, one continuous surface with no block edges, ",
    "brick_face": "The flat face of old fired red clay bricks seen very close, filling the frame with one continuous "
                  "surface: sandy pitted orange-red clay, small dark burnt specks, tiny lime spots, slight soot, "
                  "no mortar joints, no brick edges, ",
    "steel_paint": "A flat sheet of old painted roofing steel: light grey oil paint, slightly chalky and faded, "
                   "faint brush strokes, a few tiny rust specks and small dirt streaks, no seams, no rivets, ",
    "timber_stained": "Old planed pine boards of a roof eave stained dark brown, straight grain running "
                      "horizontally, a few small knots, worn stain, slightly weathered matte surface, no gaps, ",
    "asphalt": "A 2 x 2 metre patch of old grey Soviet asphalt of a courtyard drive, fine gravel aggregate, "
               "slightly worn and faded, a few fine cracks and small dark patches, ",
    "concrete": "A 2 x 2 metre patch of old cast concrete slab of a step or pavement, grey, fine pores, small "
                "aggregate showing, faint stains, ",
    "soil_bed": "A 1 x 1 metre patch of dark brown loose garden soil of a city flowerbed, small clods, a few tiny "
                "pebbles and bits of dry leaves, ",
    "stone_cast": "A 1 x 1 metre patch of white cast stone of a fountain bowl: off-white, fine sandy grain, faint "
                  "grey weathering, a few tiny pores, ",
}

# texture -> (raw photo, metres per tile, output size px, target hex, target roughness, bump strength, kind)
# target hex / roughness come from common.MATERIALS of the slot the texture is made for.
TEXTURES = {
    "plaster_ochre":      ("plaster_ochre", 2.0, 2048, "#E2D3A0", 0.90, 1.6, "photo"),
    "plaster_white":      ("plaster_white", 1.0, 2048, "#F1EEE4", 0.80, 1.0, "photo"),
    "rustic_dark":        ("rustic_rock", 1.0, 2048, "#3A3532", 0.85, 7.0, "rustic"),
    "quoin_vermiculated": ("quoin_vermiculated", 1.0, 2048, "#A39C8E", 0.85, 6.0, "photo"),
    "brick_red":          ("brick_face", 1.0, 2048, "#9C5040", 0.85, 3.0, "brick"),
    "roof_steel":         ("steel_paint", 1.2, 2048, "#8E959A", 0.45, 1.0, "roof"),
    "timber_stained":     ("timber_stained", 1.0, 2048, "#6A4A2E", 0.70, 2.0, "photo"),
    "asphalt":            ("asphalt", 2.0, 1024, "#4A4A48", 0.95, 3.0, "photo"),
    "concrete":           ("concrete", 2.0, 1024, "#9A9690", 0.90, 2.5, "photo"),
    "soil_bed":           ("soil_bed", 1.0, 1024, "#4B3B2C", 1.00, 4.0, "photo"),
    "stone_cast":         ("stone_cast", 1.0, 2048, "#D9D6CE", 0.80, 2.0, "photo"),
}
GRIME = ("academ_grime_mask", 4.0, 1024)


# ----------------------------------------------------------------------------------------------- fetch

def key():
    k = os.environ.get("FAL_API_KEY") or os.environ.get("FAL_KEY")
    if not k:
        sys.exit("set FAL_API_KEY in the environment")
    return k.strip()


def post(url, k, body=None):
    req = urllib.request.Request(url, json.dumps(body).encode() if body is not None else None,
                                 {"Authorization": "Key " + k, "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=180) as f:
            return json.load(f)
    except urllib.error.HTTPError as e:
        raise RuntimeError("%s: %s" % (e.code, e.read()[:400]))


def generate(name, k):
    dest = RAW / (name + ".png")
    if dest.exists():
        return
    sub = post("https://queue.fal.run/" + ENDPOINT, k, {
        "prompt": STYLE + PROMPTS[name], "aspect_ratio": "1:1", "resolution": "2K",
        "num_images": 1, "output_format": "png"})
    while True:
        st = post(sub["status_url"], k)
        if st.get("status") == "COMPLETED":
            break
        if st.get("status") in ("FAILED", "ERROR"):
            raise RuntimeError(str(st)[:300])
        time.sleep(2)
    res = post(sub["response_url"], k)
    RAW.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(res["images"][0]["url"], timeout=180) as f:
        dest.write_bytes(f.read())
    print("saved", dest)


def fetch(names):
    k = key()
    for n in names or PROMPTS:
        for attempt in range(4):      # the content checker is flaky on harmless texture prompts
            try:
                generate(n, k)
                break
            except (RuntimeError, OSError) as e:
                print("retry", n, str(e)[:80])
                time.sleep(3)


# ----------------------------------------------------------------------------------------------- post (Blender)

def post_process(names):
    import bpy
    import numpy as np

    scene = bpy.context.scene
    scene.view_settings.view_transform = "Standard"   # no filmic tone curve on the pixels
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0

    def load(path, size):
        im = bpy.data.images.load(str(path))
        im.colorspace_settings.name = "Non-Color"         # raw stored values, no conversion
        if tuple(im.size) != (size, size):
            im.scale(size, size)
        a = np.array(im.pixels[:], dtype=np.float32).reshape(size, size, 4)[:, :, :3]
        bpy.data.images.remove(im)
        return a

    def save(a, path, quality=92):
        s = a.shape[0]
        if a.ndim == 2:
            a = np.repeat(a[:, :, None], 3, axis=2)
        im = bpy.data.images.new("out", s, s, alpha=False)
        im.colorspace_settings.name = "Non-Color"
        rgba = np.concatenate([np.clip(a, 0, 1), np.ones((s, s, 1), np.float32)], axis=2)
        im.pixels[:] = rgba.ravel()
        st = scene.render.image_settings
        st.file_format = "JPEG"
        st.color_mode = "RGB"
        st.quality = quality
        im.save_render(str(path), scene=scene)
        bpy.data.images.remove(im)

    def blur(a, sigma):
        """Gaussian blur with wrap-around (FFT)."""
        s = a.shape[0]
        f = np.fft.fftfreq(s)
        g = np.exp(-2 * (np.pi * sigma) ** 2 * (f[:, None] ** 2 + f[None, :] ** 2)).astype(np.float32)
        if a.ndim == 2:
            return np.real(np.fft.ifft2(np.fft.fft2(a) * g)).astype(np.float32)
        return np.stack([blur(a[:, :, c], sigma) for c in range(a.shape[2])], axis=2)

    def blur_xy(a, sx, sy):
        s = a.shape[0]
        f = np.fft.fftfreq(s)
        g = np.exp(-2 * np.pi ** 2 * ((sy * f[:, None]) ** 2 + (sx * f[None, :]) ** 2)).astype(np.float32)
        return np.real(np.fft.ifft2(np.fft.fft2(a) * g)).astype(np.float32)

    def seamless(a, seed=3):
        """Cross-fades the photo with itself shifted by half, so the shifted copy covers the borders."""
        s = a.shape[0]
        rolled = np.roll(np.roll(a, s // 2, 0), s // 2, 1)
        t = np.minimum(np.arange(s), s - 1 - np.arange(s)) / float(s)
        m1 = np.clip((t - 0.06) / 0.22, 0, 1)
        m1 = m1 * m1 * (3 - 2 * m1)
        m = m1[:, None] * m1[None, :]
        rng = np.random.default_rng(seed)
        n = blur(rng.standard_normal((s, s)).astype(np.float32), s / 42.0)
        n /= np.abs(n).max() + 1e-6
        m = np.clip(m + 0.35 * n * m * (1 - m) * 4, 0, 1)
        if a.ndim == 3:
            m = m[:, :, None]
        return a * m + rolled * (1 - m)

    def even_light(a, keep=0.25):
        """Takes out large-scale light and colour drift (it repeats visibly when tiled)."""
        low = blur(a, a.shape[0] / 10.0)
        mean = a.reshape(-1, 3).mean(0)
        return a * (keep + (1 - keep) * mean / np.maximum(low, 1e-3))

    def to_lin(c):
        c = np.clip(c, 0, 1)
        return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4).astype(np.float32)

    def to_srgb(c):
        c = np.clip(c, 0, 1)
        return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055).astype(np.float32)

    def hex_lin(h):
        h = h.lstrip("#")
        return to_lin(np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)], np.float32))

    def recolour(a, target_hex, sat=1.0):
        """Scales the linear colour so the mean is the target; sat < 1 pulls the variation toward grey first."""
        lin = to_lin(a)
        if sat != 1.0:
            lum = lin @ np.array([0.2126, 0.7152, 0.0722], np.float32)
            lin = lum[:, :, None] + (lin - lum[:, :, None]) * sat
        mean = lin.reshape(-1, 3).mean(0)
        lin = lin * (hex_lin(target_hex) / np.maximum(mean, 1e-4))
        return to_srgb(lin)

    def lum(a):
        return a @ np.array([0.3, 0.59, 0.11], np.float32)

    def normal_map(h, strength):
        s = h.shape[0]
        k = strength * 8 * s / 1024.0
        gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * k
        gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * k
        n = np.stack([-gx, gy, np.ones_like(h)], axis=2)  # rows run bottom-up, so +gy is DirectX green (Unreal's)
        n /= np.linalg.norm(n, axis=2, keepdims=True)
        return n * 0.5 + 0.5

    def photo_height(a, s):
        l = lum(a)
        h = l - blur(l, s / 100.0)
        return blur(h, 0.8)

    def norm01(h):
        lo, hi = np.percentile(h, 1), np.percentile(h, 99)
        return np.clip((h - lo) / max(hi - lo, 1e-6), 0, 1)

    def mask_map(h01, rough, s, extra_rough=None):
        """R roughness (mean = target, cavities rougher), G cavity AO, B height."""
        cav = np.clip(blur(h01, s / 256.0) - h01, 0, 1)
        cav = cav / max(np.percentile(cav, 99.5), 1e-4)
        ao = np.clip(1.0 - 0.6 * cav - 0.25 * (1 - h01) ** 2, 0, 1)
        r = 1.0 + 0.25 * (0.5 - h01) + 0.15 * cav
        if extra_rough is not None:
            r = r + extra_rough
        r = r / r.mean() * rough
        return np.stack([np.clip(r, 0.02, 1), ao, h01], axis=2)

    def cells(s, rows, cols, stagger, seed):
        """Running-bond layout over an s x s tile: per pixel the unit's (row, col) id, the in-unit distance to the
        nearest joint centre line (px) and a per-unit random array generator."""
        y = np.arange(s, dtype=np.float32)[:, None] * np.ones((1, s), np.float32)
        x = np.ones((s, 1), np.float32) * np.arange(s, dtype=np.float32)[None, :]
        ch, cw = s / rows, s / cols
        r = np.floor(y / ch).astype(np.int32) % rows
        xs = (x + (r % 2) * stagger * cw) % s
        c = np.floor(xs / cw).astype(np.int32) % cols
        fy = y - r * ch
        fx = xs - c * cw
        d = np.minimum(np.minimum(fy, ch - fy), np.minimum(fx, cw - fx))
        rng = np.random.default_rng(seed)
        return r, c, d, rng

    def scatter(surface, r, c, rng, rows, cols, s):
        """Each unit shows the (seamless) surface photo at its own random offset."""
        oy = rng.integers(0, s, (rows, cols))
        ox = rng.integers(0, s, (rows, cols))
        yy = (np.arange(s)[:, None] + oy[r, c]) % s
        xx = (np.arange(s)[None, :] + ox[r, c]) % s
        return surface[yy, xx]

    def build_brick(src, s, spec):
        # Soviet brick 250 x 65 mm, 10 mm joints: 4 stretchers and 13 courses per metre (76.9 mm course)
        rows, cols = 13, 4
        joint = s * 0.010 / 2.0
        surface = seamless(even_light(src))
        r, c, d, rng = cells(s, rows, cols, 0.5, 11)
        face = scatter(surface, r, c, rng, rows, cols, s)
        tone = rng.normal(1.0, 0.07, (rows, cols)).clip(0.75, 1.2)
        burnt = rng.random((rows, cols)) < 0.08                # a few dark overburnt bricks
        tone = np.where(burnt, tone * 0.62, tone)
        face = face * tone[r, c][:, :, None]
        noise = blur(np.random.default_rng(12).standard_normal((s, s)).astype(np.float32), 1.5)
        mortar = np.stack([0.62, 0.60, 0.56], 0)[None, None, :] * (1 + 0.15 * noise[:, :, None])
        edge = joint + 1.5 + 2.5 * blur(np.random.default_rng(13).standard_normal((s, s)).astype(np.float32), 3.0)
        m = np.clip((d - edge) / 3.0, 0, 1)                    # 0 in the joint, 1 on the brick face
        a = face * m[:, :, None] + mortar * (1 - m[:, :, None])
        a_col = recolour(a, spec[3])
        # mortar colour should not be pulled red by the brick target: blend it back to grey
        mort_lin = to_lin(mortar) * (hex_lin("#9A958C") / to_lin(mortar).reshape(-1, 3).mean(0))
        a_col = a_col * m[:, :, None] + to_srgb(mort_lin) * (1 - m[:, :, None])
        bevel = np.clip((d - joint) / (s * 0.006), 0, 1)
        bevel = np.sqrt(bevel)
        h = 0.25 + 0.65 * bevel + 0.15 * norm01(photo_height(face, s)) * m
        return a_col, h, None

    def build_rustic(src, s, spec):
        # rough-faced blocks 1.0 x 0.5 m, staggered by half, 20 mm sunk joints, chamfer ~40 mm
        rows, cols = 2, 1
        joint = s * 0.020 / 2.0
        surface = seamless(even_light(src))
        r, c, d, rng = cells(s, rows, cols, 0.5, 21)
        face = scatter(surface, r, c, rng, rows, cols, s)
        tone = rng.normal(1.0, 0.08, (rows, cols)).clip(0.8, 1.2)
        face = face * tone[r, c][:, :, None]
        rough_h = norm01(blur(lum(face), 2.0) - blur(lum(face), s / 30.0))
        # the pitched face bulges: highest in the middle of each block, drops into a chamfer at the edges
        dome = np.clip((d - joint) / (s * 0.06), 0, 1) ** 0.6
        m = np.clip((d - joint - 2) / 4.0, 0, 1)
        joint_col = np.full((s, s, 3), 0.18, np.float32)
        a = face * m[:, :, None] + joint_col * (1 - m[:, :, None])
        a_col = recolour(a, spec[3], sat=0.6)
        h = 0.05 + 0.55 * dome + 0.4 * rough_h * dome
        return a_col, h, None

    def build_roof(src, s, spec):
        # tile 1.2 m: two 0.6 m panels running along V (down the slope); standing seams ~25 mm wide at the
        # panel edges, a flat transverse lock seam every 1.42 m sheet (here one per tile, staggered per panel)
        surface = seamless(even_light(src, keep=0.4))
        x = np.arange(s, dtype=np.float32)
        y = np.arange(s, dtype=np.float32)
        panel = s / 2.0
        fx = x % panel
        dx = np.minimum(fx, panel - fx)                         # px to the nearest standing seam
        seam_w = s * 0.025 / 1.2 / 2.0
        rib = np.clip(1.0 - dx / seam_w, 0, 1) ** 0.5           # rounded rib
        flank = np.exp(-(dx / (s * 0.012)) ** 2) * 0.15         # sheet curls up into the seam
        # oil-canning: slow waves across each panel
        wave = 0.04 * np.sin(np.pi * fx / panel) ** 2
        h = np.zeros((s, s), np.float32) + (rib + flank + wave)[None, :]
        pid = (x // panel).astype(np.int32)
        cross_y = np.where(pid == 0, 0.30 * s, 0.78 * s)
        dy = np.abs(((y[:, None] - cross_y[None, :]) + s / 2) % s - s / 2)
        lock = np.clip(1.0 - dy / (s * 0.006), 0, 1) * 0.12
        step = np.clip((((y[:, None] - cross_y[None, :]) % s) < s * 0.01), 0, 1) * 0.02
        h = h + lock + step
        lin_rib = (rib > 0.05).astype(np.float32)
        a = surface * (1 - 0.12 * lock[:, :, None] / 0.12) * (1 + 0.06 * lin_rib)[None, :, None]
        a_col = recolour(a, spec[3], sat=0.5)
        # paint wears on the seam tops: a little smoother there
        extra = -0.25 * np.broadcast_to(lin_rib[None, :], (s, s))
        h = h + 0.05 * norm01(photo_height(surface, s))
        return a_col, h / h.max(), extra

    def build_photo(src, s, spec):
        a = seamless(even_light(src))
        sat = 0.8 if spec[3] in ("#F1EEE4", "#D9D6CE") else 1.0
        a_col = recolour(a, spec[3], sat=sat)
        return a_col, norm01(photo_height(a_col, s)), None

    def build_grime(s, seed=7):
        """White = dirt. Vertical streaks that start sharp and fade downward (sills, cornices, joints) and
        soft blotches near the bottom of the tile's repeating 4 m band; wraps in both directions."""
        rng = np.random.default_rng(seed)
        y = np.arange(s, dtype=np.float32)
        x = np.arange(s, dtype=np.float32)
        acc = np.zeros((s, s), np.float32)
        for _ in range(170):
            x0 = rng.uniform(0, s)
            w = rng.uniform(1.5, 14.0) * s / 1024.0
            y0 = rng.uniform(0, s)
            L = rng.uniform(0.04, 0.45) * s
            k = rng.uniform(0.25, 1.0)
            dxw = np.abs(((x - x0) + s / 2) % s - s / 2)
            prof_x = np.exp(-(dxw / w) ** 2)
            dyw = (y - y0) % s                                   # image rows run bottom-up in Blender: streaks run down
            dyw = (s - dyw) % s
            prof_y = np.where(dyw < L, np.clip(1 - dyw / L, 0, 1) ** 1.6, 0.0) * np.clip(dyw / (0.01 * s), 0, 1) ** 0.3
            acc += k * prof_y[:, None].astype(np.float32) * prof_x[None, :].astype(np.float32)
        streak_noise = blur_xy(rng.standard_normal((s, s)).astype(np.float32), 2.0, 40.0)
        streak_noise = (streak_noise - streak_noise.min()) / (np.ptp(streak_noise) + 1e-6)
        acc = acc * (0.6 + 0.8 * streak_noise)
        blot = blur(rng.standard_normal((s, s)).astype(np.float32), s / 16.0)
        blot = np.clip((blot - blot.mean()) / (blot.std() + 1e-6) * 0.5, 0, 1)
        fine = blur(rng.standard_normal((s, s)).astype(np.float32), 1.0)
        fine = (fine - fine.min()) / (np.ptp(fine) + 1e-6)
        g = 1 - np.exp(-1.4 * acc) + 0.35 * blot
        g = np.clip(g * (0.85 + 0.3 * fine), 0, 1)
        return g

    builders = {"photo": build_photo, "brick": build_brick, "rustic": build_rustic, "roof": build_roof}
    OUT.mkdir(parents=True, exist_ok=True)
    meta_path = OUT / "academ_textures.json"
    meta = json.loads(meta_path.read_text()) if meta_path.exists() else {}
    for name in names or list(TEXTURES) + [GRIME[0]]:
        if name == GRIME[0]:
            g = build_grime(GRIME[2])
            save(g, OUT / (name + ".jpg"))
            meta[name] = {"tile_m": GRIME[1], "size": GRIME[2], "kind": "grime"}
            print("academ texture", name, "mean %.3f" % g.mean())
            continue
        spec = TEXTURES[name]
        raw, tile, s, target, rough, bump, kind = spec
        src_path = RAW / (raw + ".png")
        if not src_path.exists():
            print("missing", src_path)
            continue
        src = load(src_path, s)
        a_col, h, extra = builders[kind](src, s, spec)
        h01 = norm01(h)
        save(a_col, OUT / (name + "_D.jpg"))
        save(normal_map(blur(h01, 0.7), bump), OUT / (name + "_N.jpg"))
        save(mask_map(h01, rough, s, extra), OUT / (name + "_R.jpg"))
        mean_lin = to_lin(a_col).reshape(-1, 3).mean(0)
        mean_hex = "#%02X%02X%02X" % tuple(int(round(v * 255)) for v in to_srgb(mean_lin))
        meta[name] = {"tile_m": tile, "size": s, "kind": kind, "target_hex": target, "mean_hex": mean_hex,
                      "roughness": rough, "raw": raw}
        print("academ texture", name, mean_hex, "target", target)
    meta_path.write_text(json.dumps(meta, indent=1, sort_keys=True) + "\n")


if __name__ == "__main__":
    try:
        import bpy  # noqa: F401
        in_blender = True
    except ImportError:
        in_blender = False
    if in_blender:
        argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
        if argv and argv[0] == "post":
            argv = argv[1:]
        post_process(argv)
    else:
        fetch(sys.argv[1:])
