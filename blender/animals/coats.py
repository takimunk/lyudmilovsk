"""Coats of the town's cats and dogs, painted in 3-d (pure numpy): every texel of the body texture knows where on the
animal it is (its bind-pose point, how far along the body, round the body, down the leg, along the tail, what part)
and the coat is a function of that. Colours are linear RGB.

Cats (Cat): 0 brown mackerel tabby, 1 ginger tabby, 2 black, 3 black and white (tuxedo), 4 blue-grey,
5 tortoiseshell (females only), 6 white with tabby patches.
Laika: 0 grey-sable (wolf-grey agouti, pale mask and underside), 1 black with a white chest, feet and tail tip.
Shepherd: 0 black and tan (black saddle), 1 grey sable.
Mutt: 0 ginger with white, 1 white with brown patches.
Maru: 0 cream (white with a pale apricot wash over the back, the head and the tail; the generated texture when built --gen).
Eye colours per coat in EYES (iris inner, outer).
"""
import numpy as np

import sdf


def srgb(h):
    h = h.lstrip("#")
    c = np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


COATS = {
    "Cat": ["tabby", "ginger", "black", "tuxedo", "blue", "tortie", "white_tabby"],
    "Laika": ["wolf", "black_white"],
    "Shepherd": ["black_tan", "grey_sable"],
    "Mutt": ["ginger_white", "white_brown"],
    "Maru": ["cream"],
}
EYES = {
    "Cat": [("#c9b03a", "#7f8f2e"), ("#d89a2a", "#b0701c"), ("#d9a826", "#b98a1e"), ("#8fb04a", "#5f8a30"),
            ("#d8a43a", "#c08328"), ("#cf8f22", "#a86a18"), ("#7fa9c0", "#5a87a6")],
    "Laika": [("#7a4a1e", "#4a2a10"), ("#5a3416", "#3a200c")],
    "Shepherd": [("#6a3e18", "#3e220c"), ("#7a4a1e", "#4a2a10")],
    "Mutt": [("#6e4018", "#42240e"), ("#6a3c18", "#3c200c")],
    "Maru": [("#7e5c36", "#4a3220")],      # hazel-brown (Dima's photos of her eyes)
}


def noise(P, freq, seed=0):
    return sdf.vnoise(P[:, 0], P[:, 1], P[:, 2], freq, seed)


def fbm(P, freq, seed=0, oct=3):
    v, a, f = 0.0, 0.5, freq
    for i in range(oct):
        v = v + a * noise(P, f, seed + i * 17)
        a *= 0.5
        f *= 2.03
    return v


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    t = np.asarray(t)[:, None] if np.ndim(t) == 1 else t
    return a * (1 - t) + b * t


class Attr:
    """Per texel (or vertex): P point (N x 3), N normal, s along the body (0 tail base .. 1 nose), theta round the body
    (0 the spine .. pi the belly), and part weights: head, ear, muzzle, leg, paw, tail, body (each N), legt (0 top .. 1
    toes), tailt (0 base .. 1 tip), front (1 front legs, 0 hind)."""

    def __init__(self, d):
        self.__dict__.update(d)


def ticking(A, base, tip, freq, amt=0.5, seed=3):
    """Agouti: banded hairs, a fine grain of darker and lighter."""
    n = noise(A.P, freq, seed) * 0.5 + noise(A.P, freq * 2.3, seed + 5) * 0.5
    return mix(base, tip, np.clip(0.5 + n * amt, 0, 1))


def tabby(A, ground, stripe, scale):
    """Mackerel tabby: a dark line down the spine, narrow stripes down the flanks, bars on the legs, rings on the tail,
    the M on the forehead and the lines from the eyes; agouti ground between."""
    P = A.P
    warp = fbm(P, 18.0 / scale, 11) * 0.9
    # flank stripes: narrow bands round the body running down from the spine, wavy and broken
    flank = np.sin((A.s * 40.0 + warp * 3.0 + np.sin(A.theta * 1.6) * 1.1) * np.pi)
    broken = smooth(-0.35, 0.1, fbm(P, 40.0 / scale, 13))
    flank_m = smooth(0.45, 0.85, flank) * smooth(2.2, 1.4, A.theta) * A.body * (0.45 + 0.55 * broken)
    spine = smooth(0.42, 0.18, A.theta) * A.body * smooth(0.05, 0.12, A.s) * smooth(0.95, 0.85, A.s)
    legs = smooth(0.45, 0.85, np.sin((A.legt * 11.0 + warp) * np.pi)) * A.leg * smooth(1.0, 0.85, A.legt) * smooth(0.2, 0.5, A.legt)
    rings = smooth(0.35, 0.8, np.sin((A.tailt * 11.0 + warp * 0.4) * np.pi)) * A.tail
    tip = smooth(0.82, 0.92, A.tailt) * A.tail
    head = A.head * smooth(0.3, 0.75, np.sin((P[:, 1] * 120.0 / scale + warp * 0.6) * np.pi)) * smooth(0.6, 1.4, A.theta * 0 + A.headup)
    m = np.clip(np.maximum.reduce([flank_m * 0.95, spine, legs * 0.9, rings, tip, head * 0.85]), 0, 1)
    g = ticking(A, ground, ground * 1.35, 160.0 / scale, 0.7)
    return mix(g, stripe, m)


def white_mask(A, chest=1.0, paws=1.0, muzzle=1.0, belly=1.0, tail_tip=0.0, extra=0.0, seed=7):
    """Where white spotting goes (from the belly up, the chest, the feet, the muzzle), with a ragged edge."""
    e = fbm(A.P, 22.0, seed) * 0.35
    m = smooth(1.85, 2.15, A.theta + e) * A.body * belly
    m = np.maximum(m, smooth(1.35, 1.8, A.theta + e) * A.body * smooth(0.7, 0.8, A.s) * chest)
    m = np.maximum(m, smooth(0.55, 0.75, A.legt + e * 0.5) * A.leg * paws)
    m = np.maximum(m, A.paw * paws)
    m = np.maximum(m, A.muzzle * smooth(0.1, 0.5, -A.headup + 0.6 + e) * muzzle)
    m = np.maximum(m, smooth(0.8, 0.9, A.tailt + e * 0.3) * A.tail * tail_tip)
    if extra:
        m = np.maximum(m, smooth(0.2, 0.0, fbm(A.P, 6.0, seed + 3) + extra - 0.5) * (A.body + A.leg))
    return np.clip(m, 0, 1)


def paint(breed, coat, A, scale):
    """RGB (N x 3) of the coat at the points of A; scale = the animal's size (withers height / 0.25 m)."""
    name = COATS[breed][coat]
    n = len(A.P)
    W = np.ones((n, 1))
    belly = smooth(1.5, 2.4, A.theta) * A.body
    if breed == "Cat":
        if name in ("tabby", "white_tabby"):
            c = tabby(A, srgb("#7a6c58"), srgb("#231d17"), scale)
            c = mix(c, srgb("#c9b899"), belly * 0.6)
            if name == "white_tabby":
                c = mix(c, srgb("#eeeae2"), white_mask(A, extra=0.15, seed=5))
        elif name == "ginger":
            c = tabby(A, srgb("#d2853d"), srgb("#9a4c1c"), scale)
            c = mix(c, srgb("#e9c493"), belly * 0.7)
        elif name == "black":
            c = ticking(A, srgb("#151210"), srgb("#2a211b"), 140.0 / scale, 0.5)
        elif name == "tuxedo":
            c = ticking(A, srgb("#121010"), srgb("#1f1a17"), 140.0 / scale, 0.5)
            c = mix(c, srgb("#efebe4"), white_mask(A, chest=1.0, paws=1.0, muzzle=1.0))
        elif name == "blue":
            c = ticking(A, srgb("#6c6f74"), srgb("#8a8d92"), 140.0 / scale, 0.6)
            c = mix(c, tabby(A, srgb("#6c6f74"), srgb("#5a5d62"), scale), 0.35)
        elif name == "tortie":
            blot = fbm(A.P, 9.0 / scale, 21, 4)
            red = ticking(A, srgb("#c97a38"), srgb("#e0a060"), 140.0 / scale, 0.6)
            blk = ticking(A, srgb("#151110"), srgb("#2b1f18"), 140.0 / scale, 0.5)
            c = mix(blk, red, smooth(0.02, 0.12, blot))
            c = mix(c, srgb("#d9b37e"), smooth(0.25, 0.35, fbm(A.P, 14.0 / scale, 31)) * 0.6)
        else:
            c = np.tile(srgb("#808080"), (n, 1))
        # the cat's pink nose leather and paw pads, dark lips and eye rims
        nose = A.nose
        c = mix(c, srgb("#c98a86") if name not in ("black", "tortie") else srgb("#2a1c1c"), nose)
        c = mix(c, srgb("#3a2826") if name not in ("tuxedo", "white_tabby") else srgb("#c98a86"), A.pad)
        c = mix(c, srgb("#d49a95"), A.inner_ear * 0.75)
        c = mix(c, srgb("#2a1f1c"), A.lips)
        return c
    # dogs
    if breed == "Laika":
        if name == "wolf":
            saddle = smooth(1.2, 0.5, A.theta + fbm(A.P, 5.0, 3) * 0.3) * A.body
            base = ticking(A, srgb("#7d7466"), srgb("#4a443c"), 320.0, 0.7)
            base = mix(base, srgb("#5a534a"), smooth(-0.2, 0.4, fbm(A.P, 4.0, 8)) * 0.4)
            c = mix(base, srgb("#2f2b27"), saddle * 0.55)
            pale = np.maximum(belly, smooth(0.5, 0.8, A.legt) * A.leg)
            pale = np.maximum(pale, A.muzzle * smooth(0.4, -0.2, A.headup))
            c = mix(c, srgb("#d6cbb6"), np.clip(pale, 0, 1) * 0.85)
            c = mix(c, srgb("#2b2622"), smooth(0.75, 0.95, A.tailt) * A.tail * 0.7)
        else:
            c = ticking(A, srgb("#111010"), srgb("#1f1b19"), 300.0, 0.4)
            c = mix(c, srgb("#ece7de"), white_mask(A, chest=1.0, paws=1.0, muzzle=0.6, tail_tip=1.0, belly=0.6))
    elif breed == "Maru":
        c = ticking(A, srgb("#f1e9dc"), srgb("#fbf6ee"), 300.0, 0.4)
        wash = smooth(1.5, 0.7, A.theta + fbm(A.P, 3.0, 4) * 0.3) * A.body
        wash = np.maximum(wash, A.ear * 0.9 + A.head * smooth(0.2, 0.7, A.headup) * 0.6)
        wash = np.maximum(wash, A.tail * 0.7)
        c = mix(c, srgb("#e2bf8c"), np.clip(wash, 0, 1) * 0.75)
    elif breed == "Shepherd":
        if name == "black_tan":
            tan = ticking(A, srgb("#a76a32"), srgb("#c99254"), 300.0, 0.5)
            saddle = smooth(1.45, 0.95, A.theta + fbm(A.P, 4.0, 5) * 0.25) * A.body * smooth(0.08, 0.18, A.s) * smooth(0.74, 0.6, A.s)
            mask = A.muzzle * 0.9 + A.head * smooth(0.0, 0.6, A.headup) * 0.4
            c = mix(tan, srgb("#141210"), np.clip(np.maximum(saddle, mask) + A.tail * smooth(0.0, 0.4, A.tailt) * 0.8, 0, 1))
        else:
            c = ticking(A, srgb("#746c60"), srgb("#3a352f"), 300.0, 0.75)
            c = mix(c, srgb("#2c2824"), smooth(1.3, 0.6, A.theta) * A.body * 0.6)
            c = mix(c, srgb("#c9bfae"), np.clip(belly + smooth(0.6, 0.9, A.legt) * A.leg, 0, 1) * 0.7)
    else:   # Mutt
        if name == "ginger_white":
            c = ticking(A, srgb("#b9773c"), srgb("#d39a5c"), 320.0, 0.5)
            c = mix(c, srgb("#efe8dc"), white_mask(A, chest=1.0, paws=1.0, muzzle=1.0, belly=1.0, seed=9))
        else:
            c = np.tile(srgb("#ebe5da"), (n, 1))
            patch = smooth(0.05, 0.15, fbm(A.P, 5.0, 41, 4)) * (A.body + A.head + A.ear)
            c = mix(c, ticking(A, srgb("#6a4022"), srgb("#875533"), 320.0, 0.5), np.clip(patch, 0, 1))
    c = mix(c, srgb("#151212"), A.nose)
    c = mix(c, srgb("#1c1715"), A.pad)
    c = mix(c, srgb("#d9cbbd"), A.inner_ear * 0.45)
    c = mix(c, srgb("#1c1715"), A.lips)
    return c


def eye_texture(breed, coat, size=256, slit=None):
    """An eye ball texture for a UV sphere whose pole (v = 1) looks forward: white round the back, the iris, the pupil
    (a cat's vertical slit, a dog's round pupil), a dark limbal ring."""
    inner, outer = EYES[breed][coat % len(EYES[breed])]
    cat = breed == "Cat"
    v, u = np.mgrid[0:size, 0:size] / (size - 1.0)
    # angle from the front pole: v = 1 at the pole (Blender's uv sphere: v runs from the bottom pole to the top)
    a = (1.0 - v) * np.pi              # 0 at the front pole
    lon = u * 2 * np.pi
    x = np.sin(a) * np.cos(lon)
    y = np.sin(a) * np.sin(lon)
    iris_r = 0.82 if cat else 0.62   # in sin(angle): cats' irises fill the eye
    r = np.sin(np.minimum(a, np.pi / 2))
    front = a < np.pi / 2
    col = np.zeros((size, size, 3))
    sclera = srgb("#d8d0c2") * 0.6 if cat else srgb("#3a2618")
    col[:] = sclera
    t = np.clip(r / iris_r, 0, 1)
    rad = 0.5 + 0.5 * np.sin(lon * 23.0) * 0.5 + 0.25 * np.sin(lon * 57.0 + 1.3)
    iris = srgb(inner)[None, None, :] * (1 - t[..., None]) + srgb(outer)[None, None, :] * t[..., None]
    iris = iris * (0.85 + 0.3 * rad[..., None] * t[..., None])
    on = front & (r < iris_r)
    col[on] = iris[on]
    ring = front & (r > iris_r * 0.92) & (r < iris_r * 1.02)
    col[ring] = col[ring] * 0.35
    if cat:
        pupil = front & ((np.abs(x) / 0.1) ** 2 + (y / (iris_r * 0.92)) ** 2 < 1.0)    # a vertical slit (dusk: half open)
    else:
        pupil = front & (r < 0.24)
    col[pupil] = srgb("#050505")
    return col
