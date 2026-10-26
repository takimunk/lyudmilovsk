"""The feral pigeon's colour morphs, made from the one base plumage (the generated blue-bar body texture and the
scanned feathers) by rules, not by more generations:

    Blender -b --python blender/birds/hero_pigeon/make_morphs.py

morphs.json lists them with how common each is in a city flock (the same table as UBirdSpeciesLibrary::PigeonMorphs).
Every pixel is first sorted by what it is on the base bird: wing-bar black, neck sheen, eye / bill / cere (kept), or the
blue-grey ground, and where it is on the bird (texel -> body position by rasterising the hero mesh's UVs), so the
checker of the coverts and the white patches of a pied bird lie on the body, not on the texture's islands.
Output: blender/birds/export/FeralPigeon/T_FeralPigeon_Hero_Albedo_<Morph>.png and
T_FeralPigeon_HeroFeathers_Albedo_<Morph>.png, previews/morphs.png (all of them on the body texture's thumbnails).
"""
import json
import math
import os
import zlib

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "export", "FeralPigeon")
MORPHS = json.load(open(os.path.join(HERE, "morphs.json")))["morphs"]
RES = 2048      # masks and positions are worked out at this size, colours at the texture's own


def load(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    return px.reshape(h, w, 4)


def save(arr, path):
    h, w = arr.shape[:2]
    im = bpy.data.images.new(os.path.basename(path), w, h, alpha=True)
    im.pixels.foreach_set(np.clip(arr, 0, 1).astype(np.float32).ravel())
    im.filepath_raw = path
    im.file_format = "PNG"
    im.save()
    bpy.data.images.remove(im)


def upsample(a, n):
    """Nearest-neighbour resize of an (h, w, ...) array to (n, n, ...)."""
    yi = (np.arange(n) * a.shape[0] // n)
    xi = (np.arange(n) * a.shape[1] // n)
    return a[yi][:, xi]


# ====================================================================== texel -> body position
def position_map(slots=(0, 1)):
    """(RES, RES, 3) body position of every texel of the body texture (NaN where no triangle), from the hero glb."""
    bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, "FeralPigeon_Hero.glb"))
    ob = next(o for o in bpy.data.objects if o.type == "MESH" and o.name.startswith("SK_"))   # (the rig's bone shape is a mesh too)
    me = ob.data
    me.calc_loop_triangles()
    co = np.array([ob.matrix_world @ v.co for v in me.vertices])
    uvl = me.uv_layers.active.data
    pos = np.full((RES, RES, 3), np.nan, np.float32)
    for t in me.loop_triangles:
        if t.material_index not in slots:
            continue
        uv = np.array([uvl[li].uv for li in t.loops]) * RES
        p = co[list(t.vertices)]
        x0, y0 = np.floor(uv.min(0)).astype(int)
        x1, y1 = np.ceil(uv.max(0)).astype(int)
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, RES - 1), min(y1, RES - 1)
        if x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        a, b, c = uv
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-12:
            continue
        l0 = ((b[1] - c[1]) * (gx - c[0]) + (c[0] - b[0]) * (gy - c[1])) / d
        l1 = ((c[1] - a[1]) * (gx - c[0]) + (a[0] - c[0]) * (gy - c[1])) / d
        l2 = 1 - l0 - l1
        inside = (l0 >= -0.02) & (l1 >= -0.02) & (l2 >= -0.02)
        if not inside.any():
            continue
        P = l0[..., None] * p[0] + l1[..., None] * p[1] + l2[..., None] * p[2]
        sub = pos[y0:y1 + 1, x0:x1 + 1]
        sub[inside] = P[inside]
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    # bleed into the gutters so mip levels and bilinear lookups stay on the body
    ok = ~np.isnan(pos[..., 0])
    for _ in range(8):
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.roll(np.roll(pos, dy, 0), dx, 1)
            shok = np.roll(np.roll(ok, dy, 0), dx, 1)
            take = ~ok & shok
            pos[take] = sh[take]
            ok |= take
    return pos


# ====================================================================== what each pixel is
def rgb2hsv(c):
    mx, mn = c.max(-1), c.min(-1)
    v = mx
    s = np.where(mx > 1e-5, (mx - mn) / np.maximum(mx, 1e-5), 0)
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    d = np.maximum(mx - mn, 1e-5)
    h = np.where(mx == r, (g - b) / d % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) / 6
    return h, s, v


def classes(c):
    """bar (wing-bar / tail-band black), sheen (neck iridescence), keep (eye, bill, cere, feet), ground (the rest)."""
    h, s, v = rgb2hsv(c)
    lum = c @ np.array([0.2126, 0.7152, 0.0722])
    sheen = (s > 0.22) & (((h > 0.25) & (h < 0.55)) | ((h > 0.70) & (h < 0.95))) & (v > 0.12)
    keep = (s > 0.30) & ((h < 0.12) | (h > 0.95)) & ~sheen        # orange eye, pink skin
    bar = np.clip((0.16 - lum) / 0.08, 0, 1) * ~sheen * ~keep
    return bar, sheen.astype(np.float32), keep.astype(np.float32), lum


# ====================================================================== 3D patterns
def hash3(i):
    """Stable pseudo-random 0..1 per integer cell (n, 3) -> (n,)."""
    i = i.astype(np.int64)
    x = (i[..., 0] * 73856093) ^ (i[..., 1] * 19349663) ^ (i[..., 2] * 83492791)
    x = (x ^ (x >> 13)) * 1274126177
    return ((x ^ (x >> 16)) & 0xFFFFFF) / float(0xFFFFFF)


def value_noise(p, scale, seed):
    q = p / scale + seed * 17.13
    i = np.floor(q)
    f = q - i
    f = f * f * (3 - 2 * f)
    out = 0
    for dz in (0, 1):
        for dy in (0, 1):
            for dx in (0, 1):
                w = (f[..., 0] if dx else 1 - f[..., 0]) * (f[..., 1] if dy else 1 - f[..., 1]) * (f[..., 2] if dz else 1 - f[..., 2])
                out = out + w * hash3(i + np.array([dx, dy, dz]))
    return out


def checker(p, cell, dark_frac):
    """A pigeon's checker: each covert dark in the middle with a pale fringe (cells of a jittered grid on the body)."""
    q = p / cell
    i = np.floor(q)
    best = np.full(p.shape[:-1], 9.0)
    for dz in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                j = i + np.array([dx, dy, dz])
                ctr = j + 0.5 + 0.6 * (np.stack([hash3(j), hash3(j + 7), hash3(j + 13)], -1) - 0.5)
                best = np.minimum(best, np.linalg.norm(q - ctr, axis=-1))
    return np.clip((dark_frac - best) / 0.12, 0, 1)


# ====================================================================== the morphs
def lerp(a, b, t):
    return a + (b - a) * t[..., None]


def recolour(c, pos, m, part):
    """c: (h, w, 3) base colours; pos: body positions (NaN for none); m: the morph; part: body | primary | secondary
    | tail | covert. Returns new colours."""
    bar, sheen, keep, lum = classes(c)
    ground = (1 - bar) * (1 - sheen) * (1 - keep)
    # how light the pixel is against the base bird's blue-grey ground (keeps shading and feather texture)
    shade = lum / 0.42
    out = c.copy()
    has = ~np.isnan(pos[..., 0]) if pos is not None else np.zeros(c.shape[:2], bool)
    P = np.where(has[..., None], pos, 0.0) if pos is not None else None
    ay = np.abs(P[..., 1]) if P is not None else None
    if P is not None:
        # the upper wing coverts and the mantle (where checkers show), the head (keeps its slate), the flight feathers' cut ends
        upper = has & (P[..., 2] > 0.078) & (P[..., 0] < 0.075) & (P[..., 0] > -0.09)
        wing = upper & (ay > 0.03)
    t = m["kind"]
    if t == "checker" and P is not None:
        k = checker(P, m.get("cell", 0.0075), m.get("dark", 0.42)) * upper * ground
        dark = np.array(m.get("dark_rgb", [0.16, 0.17, 0.20]))
        out = lerp(out, dark * shade[..., None] / 0.45, k * 0.9)
        if m.get("body_dim"):
            out = out * (1 - m["body_dim"] * ground)[..., None]
    if t == "checker" and part in ("secondary", "covert"):
        # the coverts and secondaries: bar widened into a dark spot (a checker covert is mostly dark)
        out = lerp(out, np.array(m.get("dark_rgb", [0.16, 0.17, 0.20])) * np.maximum(shade, 0.6)[..., None] * 0.5,
                   np.clip(bar * 2 + ground * m.get("dark", 0.42) * 0.8, 0, 1))
    if t == "spread":
        black = np.array([0.075, 0.078, 0.088])
        out = lerp(out, black * (0.6 + 0.8 * shade[..., None]), (ground + bar) * 0.92)
        out = lerp(out, out * np.array([0.8, 1.25, 1.1]), sheen)            # a stronger green sheen on black
    if t == "barless":
        out = lerp(out, np.array([0.40, 0.43, 0.48]) * np.maximum(shade, 0.5)[..., None] * 1.05, bar * (part != "tail" and part != "primary"))
    if t in ("ashred", "brown", "white"):
        g, b2, sh = np.array(m["ground_rgb"]), np.array(m["bar_rgb"]), np.array(m["sheen_rgb"])
        out = lerp(out, g * shade[..., None], ground)
        out = lerp(out, b2 * (0.6 + 0.6 * shade[..., None]), bar)
        out = lerp(out, sh * (0.5 + 0.7 * shade[..., None]), sheen)
    if t == "pied":
        if P is not None:
            n = value_noise(P, 0.028, 3) * 0.75 + value_noise(P, 0.011, 5) * 0.25
            white = np.clip((n - 0.56) / 0.04, 0, 1) * has * (1 - keep)
            # the head and the flight feathers are more often white in pied birds than the breast
            out = lerp(out, np.array([0.93, 0.92, 0.90]) * np.clip(0.75 + 0.35 * shade, 0.8, 1.05)[..., None], white)
        elif part in ("primary", "secondary", "tail"):
            pass   # per feather: decided in morph_feathers
    return out


def morph_feathers(atlas, rects, m, seed):
    out = atlas.copy()
    H, W = atlas.shape[:2]
    rng = np.random.default_rng(seed)
    for name, r in rects.items():
        u0, v0, u1, v1 = r["rect"]
        x0, y0, x1, y1 = int(u0 * W), int(v0 * H), int(u1 * W), int(v1 * H)
        tile = atlas[y0:y1, x0:x1, :3]
        new = recolour(tile, None, m, r["kind"])
        if m["kind"] == "pied" and rng.random() < 0.35:
            new = np.array([0.93, 0.92, 0.90]) * np.clip(0.75 + 0.35 * classes(tile)[3] / 0.42, 0.8, 1.05)[..., None]
        out[y0:y1, x0:x1, :3] = new
    return out


EYE = (0.1036, 0.0945)    # the eyes' middle, x and z (fit.py puts the generated eyes there)


def paint_bill(body, pos):
    """A pigeon's bill: dark horn grey, with the white, powdery cere over its base (the generated one is pale all over)."""
    small = upsample(body, RES)[..., :3]
    has = ~np.isnan(pos[..., 0])
    P = np.where(has[..., None], pos, 0.0)
    ex, ez = EYE
    s = P[..., 0] - (ex + 0.009)
    band = has & (np.abs(P[..., 1]) < 0.012) & (P[..., 2] > ez - 0.04) & (P[..., 2] < ez + 0.01)
    # the bill's base height (where it grows from the face), then the bill and its cere relative to it: the forehead
    # above the base is feathers and stays as it is
    near = band & (s > 0.005) & (s < 0.009) & (np.abs(P[..., 1]) < 0.004)   # surely bill: ahead of the face
    zb = float(np.median(P[near][:, 2])) if near.any() else ez - 0.012
    z = P[..., 2] - zb
    bill = band & (s > -0.001) & (z < 0.0045) & (z > -0.008) & (np.abs(P[..., 1]) < 0.0055 + 0.004 * np.clip(s / 0.01, 0, 1))
    cere = bill & (s < 0.0045) & (z > 0.0012) & (np.abs(P[..., 1]) < 0.0045)
    lum = small @ np.array([0.2126, 0.7152, 0.0722])
    out = small.copy()
    k = np.clip((s + 0.001) / 0.002, 0, 1) * bill
    horn = np.array([0.105, 0.10, 0.11])[None, None, :] * np.clip(0.8 + 0.4 * (lum - 0.5), 0.6, 1.2)[..., None]
    out = out * (1 - k[..., None]) + horn * k[..., None]
    w = cere.astype(np.float32)
    out = out * (1 - w[..., None]) + np.array([0.86, 0.85, 0.82]) * w[..., None]
    factor = np.where(small > 1e-3, out / np.maximum(small, 1e-3), 1.0)
    full = body.copy()
    full[..., :3] = np.clip(body[..., :3] * upsample(factor, body.shape[0]), 0, 1)
    changed = upsample((k > 0) | cere, body.shape[0])
    full[changed, :3] = upsample(out, body.shape[0])[changed]
    print("BILL", int(bill.sum()), "texels, cere", int(cere.sum()), "base z", round(zb, 4))
    return full


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    raw = load(os.path.join(HERE, "gen", "hero_albedo_raw.png"))
    feathers = load(os.path.join(OUT, "T_FeralPigeon_HeroFeathers_Albedo.png"))
    rects = json.load(open(os.path.join(HERE, "gen", "feather_rects.json")))
    pos = position_map()
    body = paint_bill(raw, pos)
    save(body, os.path.join(OUT, "T_FeralPigeon_Hero_Albedo.png"))
    # colours at RES (the masks' size), then back up to the texture's size: the recolour is a per-pixel factor
    small = upsample(body, RES)[..., :3]
    thumbs = []
    for m in MORPHS:
        if not m["name"]:
            continue
        new = recolour(small, pos, m, "body")
        factor = np.where(small > 1e-3, new / np.maximum(small, 1e-3), 1.0)
        full = body.copy()
        full[..., :3] = np.clip(body[..., :3] * upsample(factor, body.shape[0]), 0, 1)
        # where the base was near black the factor is unstable: take the new colour directly
        dark = (upsample(small.max(-1), body.shape[0]) < 0.03)
        full[dark, :3] = upsample(new, body.shape[0])[dark]
        save(full, os.path.join(OUT, f"T_FeralPigeon_Hero_Albedo_{m['name']}.png"))
        f = morph_feathers(feathers, rects, m, zlib.crc32(m["name"].encode()))
        save(f, os.path.join(OUT, f"T_FeralPigeon_HeroFeathers_Albedo_{m['name']}.png"))
        thumbs.append(upsample(full, 512))
        print("MORPH", m["name"])
    sheet = np.concatenate([upsample(body, 512)] + thumbs, axis=1)
    save(sheet, os.path.join(OUT, "previews", "morphs.png"))


main()
