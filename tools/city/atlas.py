#!/usr/bin/env python3
"""The town interiors' picture atlas (CT_Atlas): one 4096 x 4096 sRGB texture of everything flat and printed inside
the buildings of Lyudmilovsk, drawn here with Pillow (no paid generation, no network, all original):

    python3 tools/city/atlas.py            -> textures/city/atlas_D.jpg, blender/city/atlas_cells.json
    python3 tools/city/atlas.py --sheet    -> also renders/city/interior/atlas_sheet.jpg (cells with their names)

Deterministic: every cell has its own seed (no time, no global random), the packing is a fixed shelf packer.

Cell groups (name prefixes; blender/city/interior_kit.py picks cells by name or by group):
    spine_00..39      book-spine strips (a run of 14-30 books across, full height): bookshelves map blocks of books here
    painting_*        framed pictures: seascapes, dunes, pines, lighthouse, still life, a portrait (oil-dab look)
    poster_*          constructivist and sun-motif posters, birds of the coast, health / safety posters
    cinema_*          film posters of the town's cinema
    travel_*          travel posters for the Baltic seaside town of Lyudmilovsk
    kids_*            children's drawings (crayon on paper)
    map_*             the town map, the coast chart, an imagined world map (also the globe)
    chart_*           timetables, eye chart, anatomy, periodic table, the birds' chart, a calendar
    board_*           blackboards with chalk writing
    menu_* price_*    canteen menus, shop price boards
    notice_*          notice-board collages (cork, pinned sheets)
    rug_*             10 rugs and carpets (rug_round_* are round, drawn in a square)
    tile_*            10 tile / mosaic panels (each cell is a whole panel of tiles, not a repeat)
    clock_*           clock faces
    screen_*          TV pictures, arcade screens, film stills, the cinema's test frame
    sleeve_*          record sleeves;  mag_*: magazine covers;  news_*: newspapers;  paper_*: sheets, letters
    label_00..59      small goods labels (tins, boxes, jars, packets)
    sign_*            shop and door signs (wide)
    misc: keys_piano (a keyboard, 52 white keys), chess_board, dartboard, aquarium_back, dial_phone, dial_radio,
          record_label, exit_sign, typewriter_keys, cash_display, fabric_*, spines_sides
The json: {"size": 4096, "cells": {name: {"px": [x, y, w, h], "uv": [u0, v0, u1, v1]}}}. px: the drawn cell, top-left
origin. uv: the same rect in Blender UV convention (v up from the image's BOTTOM, i.e. v = 1 - y / size), shrunk by
1.5 px so mip-mapping does not bleed (every cell is also padded with its own edge pixels).

Orientation (Blender preview vs Unreal): Blender samples images with v up from the bottom row; the glTF exporter
writes v' = 1 - v and Unreal samples with v down from the top row, so a cell drawn upright here shows upright in
both, with no flip anywhere. A quad whose corners (bottom-left, bottom-right, top-right, top-left seen from its front)
get (u0, v0), (u1, v0), (u1, v1), (u0, v1) reads correctly (interior_kit.atlas_quad does exactly that).
"""
import json
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
FONTS = os.path.join(ROOT, "fonts")
OUT_TEX = os.path.join(ROOT, "textures", "city", "atlas_D.jpg")
OUT_JSON = os.path.join(ROOT, "blender", "city", "atlas_cells.json")
SIZE = 4096
PAD = 6          # bleed around every cell (its own edge pixels)
SS = 2           # cells are drawn at SS x and scaled down (smooth edges)

FONT_FILES = {"dela": "DelaGothicOne-Regular.ttf", "mp": "MPLUS1p-Regular.ttf", "mpb": "MPLUS1p-Bold.ttf",
              "cav": "Caveat-Regular.ttf", "cavb": "Caveat-Bold.ttf"}
_FC = {}


def _sorted_box(fn):
    """Pillow refuses boxes with x1 < x0: drawing code here gives corners in any order."""
    def g(self, xy, *a, **k):
        if len(xy) == 4 and not isinstance(xy[0], (tuple, list)):
            x0, y0, x1, y1 = xy
            xy = (min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1))
        return fn(self, xy, *a, **k)
    return g


for _m in ("ellipse", "rectangle", "pieslice", "arc", "rounded_rectangle", "chord"):
    setattr(ImageDraw.ImageDraw, _m, _sorted_box(getattr(ImageDraw.ImageDraw, _m)))


def font(name, size):
    size = max(6, int(size))
    k = (name, size)
    if k not in _FC:
        _FC[k] = ImageFont.truetype(os.path.join(FONTS, FONT_FILES[name]), size)
    return _FC[k]


# ------------------------------------------------------------------------------------------------ palettes
def hx(s):
    s = s.lstrip("#")
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))


CREAM, PAPER, INK = hx("EFE6D2"), hx("F3EEE2"), hx("1E1B18")
RED, NAVY, SEA, TEAL = hx("C8452D"), hx("1F3A5F"), hx("3E7CA6"), hx("5FA8A0")
SUN, OCHRE, SAND, PINE = hx("E8833A"), hx("D9A441"), hx("E2C48A"), hx("2F5D46")
GREY, SKY, ROSE, PLUM = hx("8A8478"), hx("A9CBDD"), hx("D98C7A"), hx("5B3A5C")
PALETTES = [
    [CREAM, RED, INK, OCHRE],
    [CREAM, NAVY, SUN, SEA],
    [hx("E9E2CF"), hx("2E4A3F"), hx("D6A13A"), hx("B4472F")],
    [hx("F1E4C8"), hx("23395B"), hx("E55B3C"), hx("8FB7C4")],
    [hx("EDE3D0"), hx("6A2E35"), hx("D9B26F"), hx("3B5B7A")],
    [hx("F4EBDD"), hx("1F5F5B"), hx("F2A03D"), hx("C2402F")],
]


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def jit(c, rnd, a):
    return tuple(max(0, min(255, int(v + rnd.uniform(-a, a)))) for v in c)


# ------------------------------------------------------------------------------------------------ helpers
def fit(d, text, fname, bw, bh, start):
    size = int(start)
    while size > 7:
        f = font(fname, size)
        b = d.textbbox((0, 0), text, font=f)
        if b[2] - b[0] <= bw and b[3] - b[1] <= bh:
            return f
        size = int(size * 0.92) if size > 30 else size - 1
    return font(fname, size)


def text_c(d, text, fname, cx, cy, bw, bh, fill, start=None, anchor="c"):
    """Text fitted into bw x bh, centred on (cx, cy) (anchor l: left-aligned at cx)."""
    f = fit(d, text, fname, bw, bh, start or bh)
    b = d.textbbox((0, 0), text, font=f)
    w, h = b[2] - b[0], b[3] - b[1]
    x = cx - w / 2 - b[0] if anchor == "c" else cx - b[0]
    d.text((x, cy - h / 2 - b[1]), text, font=f, fill=fill)
    return f


def lines_c(d, lines, fname, cx, y0, bw, lh, fill, anchor="c", spacing=1.25):
    f = min((fit(d, s, fname, bw, lh, lh) for s in lines if s), key=lambda f: f.size)
    y = y0
    for s in lines:
        if s:
            b = d.textbbox((0, 0), s, font=f)
            x = cx - (b[2] - b[0]) / 2 - b[0] if anchor == "c" else cx - b[0]
            d.text((x, y - b[1]), s, font=f, fill=fill)
        y += lh * spacing
    return y


def grain(img, amount, seed):
    """Print grain: a little luminance noise (numpy, seeded) and a faint paper fibre blur."""
    a = np.asarray(img).astype(np.float32)
    rng = np.random.default_rng(seed)
    n = rng.normal(0, amount, a.shape[:2]).astype(np.float32)
    a += n[..., None]
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def vignette(img, strength=0.25, colour=(50, 40, 30)):
    w, h = img.size
    m = Image.new("L", (w, h), 255)
    d = ImageDraw.Draw(m)
    d.rectangle((w * 0.05, h * 0.05, w * 0.95, h * 0.95), fill=0)
    m = m.filter(ImageFilter.GaussianBlur(min(w, h) * 0.08)).point(lambda v: int(v * strength))
    return Image.composite(Image.new("RGB", img.size, colour), img, m)


def wave_poly(w, y, amp, length, phase, bottom):
    pts = [(x, y + amp * math.sin(x / length * 2 * math.pi + phase)) for x in np.linspace(0, w, 60)]
    return pts + [(w, bottom), (0, bottom)]


def gull(d, cx, cy, s, fill, flap=0.0):
    """A flying gull: two curved wings (an M), s = span."""
    pts = []
    for k in range(13):
        t = k / 12.0
        x = cx - s / 2 + s * t
        u = abs(t - 0.5) * 2
        y = cy - s * (0.22 + flap * 0.1) * math.sin(u * math.pi) * (1 - u * 0.35) + s * 0.03 * u
        pts.append((x, y))
    back = []
    for k in range(13):
        t = 1 - k / 12.0
        x = cx - s / 2 + s * t
        u = abs(t - 0.5) * 2
        y = cy - s * (0.22 + flap * 0.1) * math.sin(u * math.pi) * (1 - u * 0.35) + s * 0.03 * u + s * 0.05 * (1 - u)
        back.append((x, y))
    d.polygon(pts + back, fill=fill)


def heron(d, x, y, s, fill):
    """A standing heron (y = feet), s = height."""
    d.line([(x - s * 0.02, y), (x, y - s * 0.38)], fill=fill, width=max(1, int(s * 0.015)))
    d.line([(x + s * 0.05, y), (x + s * 0.02, y - s * 0.38)], fill=fill, width=max(1, int(s * 0.015)))
    d.polygon([(x - s * 0.16, y - s * 0.40), (x + s * 0.14, y - s * 0.62), (x + s * 0.18, y - s * 0.52),
               (x + s * 0.05, y - s * 0.36), (x - s * 0.22, y - s * 0.33)], fill=fill)
    d.line([(x + s * 0.12, y - s * 0.58), (x + s * 0.04, y - s * 0.78), (x + s * 0.12, y - s * 0.92)], fill=fill,
           width=max(2, int(s * 0.035)))
    d.ellipse((x + s * 0.08, y - s * 0.97, x + s * 0.18, y - s * 0.89), fill=fill)
    d.polygon([(x + s * 0.17, y - s * 0.95), (x + s * 0.34, y - s * 0.91), (x + s * 0.17, y - s * 0.91)], fill=fill)


def swallow(d, cx, cy, s, fill, ang=0.0):
    pts = [(0, 0), (-0.5, -0.35), (-0.2, -0.05), (-0.62, 0.05), (-0.15, 0.08), (-0.05, 0.12), (0.05, 0.12),
           (0.15, 0.08), (0.62, 0.05), (0.2, -0.05), (0.5, -0.35)]
    pts = [(0, -0.12)] + pts[1:]
    tail = [(-0.05, 0.12), (-0.18, 0.42), (0.0, 0.2), (0.18, 0.42), (0.05, 0.12)]
    c, sn = math.cos(ang), math.sin(ang)
    T = lambda p: (cx + (p[0] * c - p[1] * sn) * s, cy + (p[0] * sn + p[1] * c) * s)
    d.polygon([T(p) for p in pts], fill=fill)
    d.polygon([T(p) for p in tail], fill=fill)


def sun(d, cx, cy, r, fill, rays=0, ray_fill=None, ray_len=0.6):
    if rays:
        for k in range(rays):
            a = 2 * math.pi * k / rays
            a0, a1 = a - math.pi / rays * 0.45, a + math.pi / rays * 0.45
            d.polygon([(cx + r * 1.12 * math.cos(a0), cy + r * 1.12 * math.sin(a0)),
                       (cx + r * (1.15 + ray_len) * math.cos(a), cy + r * (1.15 + ray_len) * math.sin(a)),
                       (cx + r * 1.12 * math.cos(a1), cy + r * 1.12 * math.sin(a1))], fill=ray_fill or fill)
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=fill)


def pine(d, x, y, h, fill, trunk=None):
    if trunk:
        d.rectangle((x - h * 0.03, y - h * 0.25, x + h * 0.03, y), fill=trunk)
    for k in range(4):
        t = k / 4
        w = h * (0.32 - t * 0.06)
        yb = y - h * 0.15 - t * h * 0.22
        d.polygon([(x - w, yb), (x + w, yb), (x, yb - h * 0.36)], fill=fill)


def boat(d, x, y, s, hull, sail, sail2=None):
    d.polygon([(x - s * 0.5, y), (x + s * 0.5, y), (x + s * 0.38, y + s * 0.14), (x - s * 0.4, y + s * 0.14)], fill=hull)
    d.line([(x, y), (x, y - s * 0.95)], fill=hull, width=max(1, int(s * 0.03)))
    d.polygon([(x + s * 0.03, y - s * 0.92), (x + s * 0.03, y - s * 0.05), (x + s * 0.45, y - s * 0.05)], fill=sail)
    d.polygon([(x - s * 0.03, y - s * 0.75), (x - s * 0.03, y - s * 0.05), (x - s * 0.32, y - s * 0.05)],
              fill=sail2 or sail)


def lighthouse(d, x, y, h, body, stripe, lamp):
    w0, w1 = h * 0.12, h * 0.07
    d.polygon([(x - w0, y), (x + w0, y), (x + w1, y - h * 0.8), (x - w1, y - h * 0.8)], fill=body)
    for k in range(3):
        z0 = y - h * (0.12 + k * 0.24)
        z1 = z0 - h * 0.1
        t0, t1 = (y - z0) / (h * 0.8), (y - z1) / (h * 0.8)
        a0, a1 = w0 + (w1 - w0) * t0, w0 + (w1 - w0) * t1
        d.polygon([(x - a0, z0), (x + a0, z0), (x + a1, z1), (x - a1, z1)], fill=stripe)
    d.rectangle((x - w1 * 0.8, y - h * 0.93, x + w1 * 0.8, y - h * 0.8), fill=lamp)
    d.polygon([(x - w1 * 1.2, y - h * 0.93), (x + w1 * 1.2, y - h * 0.93), (x, y - h)], fill=stripe)


def figure(d, x, y, h, fill):
    """A simple standing person silhouette (cinema posters), y = feet."""
    d.ellipse((x - h * 0.07, y - h, x + h * 0.07, y - h * 0.86), fill=fill)
    d.polygon([(x - h * 0.12, y - h * 0.84), (x + h * 0.12, y - h * 0.84), (x + h * 0.16, y - h * 0.42),
               (x - h * 0.16, y - h * 0.42)], fill=fill)
    d.polygon([(x - h * 0.11, y - h * 0.44), (x - h * 0.01, y - h * 0.44), (x - h * 0.03, y), (x - h * 0.1, y)], fill=fill)
    d.polygon([(x + h * 0.01, y - h * 0.44), (x + h * 0.11, y - h * 0.44), (x + h * 0.1, y), (x + h * 0.03, y)], fill=fill)


def paint_dabs(img, seed, size=0.012, n=9000):
    """Oil-painting look: the flat scene re-painted with small oriented dabs sampled from itself."""
    rnd = random.Random(seed)
    w, h = img.size
    src = img.filter(ImageFilter.GaussianBlur(max(1, w * 0.004)))
    px = src.load()
    out = img.copy()
    d = ImageDraw.Draw(out)
    r = max(2, w * size)
    for _ in range(n):
        x, y = rnd.uniform(0, w - 1), rnd.uniform(0, h - 1)
        c = jit(px[int(x), int(y)], rnd, 9)
        a = rnd.uniform(-0.5, 0.5)
        L, Wd = r * rnd.uniform(1.2, 2.2), r * rnd.uniform(0.45, 0.7)
        ca, sa = math.cos(a), math.sin(a)
        pts = [(x + ca * L - sa * Wd * s1, y + sa * L + ca * Wd * s1) for s1 in (-1, 1)]
        pts += [(x - ca * L + sa * Wd * s1, y - sa * L - ca * Wd * s1) for s1 in (-1, 1)]
        d.polygon([pts[0], pts[1], pts[3], pts[2]], fill=c)
    return Image.blend(out, img, 0.25)


# ------------------------------------------------------------------------------------------------ registry
CELLS = []          # (name, w, h, fn(img, draw, rnd, W, H))


def cell(name, w, h):
    def deco(fn):
        CELLS.append((name, w, h, fn))
        return fn
    return deco


def many(prefix, n, w, h, fn):
    for i in range(n):
        CELLS.append(("%s%02d" % (prefix, i) if prefix.endswith("_") else "%s_%d" % (prefix, i), w, h,
                      (lambda i: lambda img, d, rnd, W, H: fn(img, d, rnd, W, H, i))(i)))


# ------------------------------------------------------------------------------------------------ book spines
SPINE_WORDS = ["ПТИЦЫ", "МОРЕ", "СТИХИ", "ДЮНЫ", "ЛОЦИЯ", "ФИЗИКА", "АТЛАС", "СКАЗКИ", "РОМАН", "ВОЛНЫ", "ЗВЁЗДЫ",
               "ГРИБЫ", "ЯНТАРЬ", "МАЯК", "ПОВЕСТИ", "ХИМИЯ", "ЛЕС", "ПИСЬМА", "ДНЕВНИК", "МЕХАНИКА", "ОСТРОВ",
               "ВЕТЕР", "ЧАЙКИ", "РЫБЫ", "ГЕОГРАФИЯ", "АЛГЕБРА", "МУЗЫКА", "ТОМ", "ЛЕТО", "ОБЛАКА"]
SPINE_SETS = [
    [hx("7A2E2A"), hx("2F4A3A"), hx("1F3354"), hx("6B4A2B"), hx("3A3530")],                # library cloth
    [hx("C8452D"), hx("E8B23A"), hx("3E7CA6"), hx("4E9A62"), hx("E07A9A"), hx("F2EBDD")],  # children's
    [hx("E9E1CF"), hx("D9CDB0"), hx("C7D3D6"), hx("E3C9A8"), hx("BFC9B2")],                # paperbacks
    [hx("5A2620"), hx("40281C"), hx("2B2A28"), hx("6A4A2A")],                              # old leather
    [hx("234B6A")],                                                                         # an encyclopaedia set
    [hx("8C2B22")],                                                                         # a red collected works
    [hx("2F5D46"), hx("D9A441"), hx("1E1B18"), hx("C8452D"), hx("EFE6D2")],                # Soviet modern
]


def draw_spines(img, d, rnd, W, H, i):
    pal = SPINE_SETS[i % len(SPINE_SETS)]
    uniform = len(pal) == 1
    d.rectangle((0, 0, W, H), fill=hx("1A1715"))
    x = 0.0
    vol = rnd.randint(1, 5)
    gold = hx("D8B76A")
    while x < W:
        bw = (W * rnd.uniform(0.045, 0.075)) if uniform else W * rnd.uniform(0.028, 0.085)
        bw = min(bw, W - x)
        if bw < W * 0.012:
            break
        c = jit(pal[0] if uniform else rnd.choice(pal), rnd, 10)
        top = H * (0.0 if uniform else rnd.uniform(0.0, 0.10))
        d.rectangle((x + 1, top, x + bw - 1, H), fill=c)
        dark = mix(c, (0, 0, 0), 0.35)
        d.line([(x + bw - 2, top), (x + bw - 2, H)], fill=dark, width=max(1, int(bw * 0.08)))
        d.line([(x + 2, top), (x + 2, H)], fill=mix(c, (255, 255, 255), 0.18), width=max(1, int(bw * 0.06)))
        lum = sum(c) / 3
        tc = gold if lum < 110 else (hx("2A2622") if lum > 170 else hx("F3EEE2"))
        if i % len(SPINE_SETS) in (0, 3, 4, 5) or rnd.random() < 0.5:
            for zz in (0.12, 0.86):
                y = top + (H - top) * zz
                d.rectangle((x + bw * 0.12, y, x + bw * 0.88, y + H * 0.018), fill=tc)
        # title: vertical text reading bottom to top (rotated)
        word = ("ТОМ %d" % vol) if uniform else rnd.choice(SPINE_WORDS)
        vol += 1
        th = int(bw * 0.62)
        if th >= 7:
            L = int((H - top) * 0.62)
            t = Image.new("RGBA", (L, th), (0, 0, 0, 0))
            td = ImageDraw.Draw(t)
            f = fit(td, word, "mpb", L * 0.95, th * 0.95, th)
            b = td.textbbox((0, 0), word, font=f)
            td.text(((L - (b[2] - b[0])) / 2 - b[0], (th - (b[3] - b[1])) / 2 - b[1]), word, font=f, fill=tc + (255,))
            t = t.rotate(90, expand=True)
            img.paste(t, (int(x + (bw - th) / 2), int(top + (H - top) * 0.19)), t)
        if rnd.random() < 0.3 and bw > W * 0.04:
            d.rectangle((x + bw * 0.2, H * 0.9, x + bw * 0.8, H * 0.95), fill=mix(c, (255, 255, 255), 0.5))
        x += bw
    return grain(img, 5, 1000 + i)


many("spine_", 40, 384, 128, draw_spines)


# ------------------------------------------------------------------------------------------------ paintings
def _sky(d, W, y1, top, bot, bands=10):
    for k in range(bands):
        t = k / (bands - 1)
        d.rectangle((0, y1 * k / bands, W, y1 * (k + 1) / bands + 1), fill=mix(top, bot, t))


@cell("painting_sea_calm", 400, 288)
def p_sea1(img, d, rnd, W, H):
    _sky(d, W, H * 0.55, hx("9DB8CF"), hx("F0D9B5"), 14)
    sun(d, W * 0.7, H * 0.42, H * 0.09, hx("F6E3B4"))
    for k in range(8):
        t = k / 7
        d.rectangle((0, H * (0.55 + 0.45 * k / 8), W, H), fill=mix(hx("5F86A0"), hx("2C4A63"), t))
    for k in range(40):
        y = rnd.uniform(H * 0.56, H * 0.98)
        x = rnd.uniform(0, W)
        d.line([(x, y), (x + W * rnd.uniform(0.03, 0.1), y)], fill=hx("D8E2E6"), width=2)
    boat(d, W * 0.28, H * 0.6, H * 0.22, hx("3A2E2A"), hx("EFE6D2"), hx("C8452D"))
    for k in range(3):
        gull(d, W * (0.45 + k * 0.08), H * (0.2 + 0.05 * k), W * 0.05, hx("3A3A3A"), k * 0.3)
    return grain(paint_dabs(img, 11), 4, 11)


@cell("painting_sea_storm", 400, 288)
def p_sea2(img, d, rnd, W, H):
    _sky(d, W, H * 0.5, hx("4A5560"), hx("8C9399"), 12)
    for k in range(5):
        d.ellipse((W * rnd.uniform(-0.1, 0.8), H * rnd.uniform(0.0, 0.25), W * rnd.uniform(0.3, 1.1),
                   H * rnd.uniform(0.25, 0.45)), fill=jit(hx("5A6168"), rnd, 12))
    d.rectangle((0, H * 0.5, W, H), fill=hx("2F4652"))
    for k in range(9):
        y = H * (0.5 + 0.06 * k)
        d.polygon(wave_poly(W, y, H * 0.025, W * rnd.uniform(0.1, 0.2), rnd.uniform(0, 6), H),
                  fill=mix(hx("2F4652"), hx("1C2B34"), k / 9))
        for _ in range(3):
            x = rnd.uniform(0, W)
            d.polygon([(x, y), (x + W * rnd.uniform(0.04, 0.08), y - H * 0.006), (x + W * 0.1, y + H * 0.004)],
                      fill=hx("C9D3D6"))
    lighthouse(d, W * 0.84, H * 0.56, H * 0.42, hx("E8E2D6"), hx("A33A2A"), hx("F7D77A"))
    return grain(paint_dabs(img, 12), 4, 12)


@cell("painting_sea_night", 400, 288)
def p_sea3(img, d, rnd, W, H):
    _sky(d, W, H * 0.6, hx("101B33"), hx("2F3F66"), 12)
    for _ in range(60):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H * 0.5)
        d.ellipse((x, y, x + 2.5, y + 2.5), fill=hx("F3EEDA"))
    sun(d, W * 0.3, H * 0.28, H * 0.08, hx("F2E6C0"))
    d.rectangle((0, H * 0.6, W, H), fill=hx("15233A"))
    for k in range(18):
        y = H * (0.62 + 0.02 * k)
        w = W * 0.04 * (1 + k * 0.15)
        d.rectangle((W * 0.3 - w / 2 + rnd.uniform(-5, 5), y, W * 0.3 + w / 2, y + 2), fill=hx("C9C3A4"))
    d.polygon([(0, H * 0.75), (W * 0.2, H * 0.68), (W * 0.45, H * 0.8), (W * 0.45, H), (0, H)], fill=hx("0B1220"))
    return grain(paint_dabs(img, 13), 4, 13)


@cell("painting_dunes", 400, 288)
def p_dunes(img, d, rnd, W, H):
    _sky(d, W, H * 0.5, hx("B5CCDA"), hx("EDE2C9"), 12)
    cols = [hx("E3C892"), hx("D7B57A"), hx("C9A166"), hx("B88B54")]
    for k, c in enumerate(cols):
        y = H * (0.45 + 0.13 * k)
        d.polygon(wave_poly(W, y, H * 0.05, W * (0.7 - k * 0.1), k * 1.7, H), fill=c)
    for _ in range(90):
        x = rnd.uniform(0, W)
        y = rnd.uniform(H * 0.55, H)
        d.line([(x, y), (x + rnd.uniform(-6, 6), y - rnd.uniform(8, 22))], fill=hx("7D8A4E"), width=2)
    return grain(paint_dabs(img, 14), 4, 14)


@cell("painting_pines", 288, 400)
def p_pines(img, d, rnd, W, H):
    _sky(d, W, H * 0.7, hx("C6D9E0"), hx("F1E3C4"), 12)
    d.rectangle((0, H * 0.7, W, H), fill=hx("D9C08A"))
    for k in range(7):
        x = W * rnd.uniform(0.05, 0.95)
        h = H * rnd.uniform(0.45, 0.8)
        d.line([(x, H * 0.85), (x + rnd.uniform(-8, 8), H * 0.85 - h)], fill=hx("9A5A3A"), width=int(W * 0.025))
        cx, cy = x, H * 0.85 - h
        for j in range(4):
            d.ellipse((cx - W * 0.12 + rnd.uniform(-8, 8), cy - H * 0.04 + j * H * 0.03, cx + W * 0.12,
                       cy + H * 0.04 + j * H * 0.03), fill=jit(hx("2F5D46"), rnd, 14))
    return grain(paint_dabs(img, 15), 4, 15)


@cell("painting_lighthouse", 288, 400)
def p_lh(img, d, rnd, W, H):
    _sky(d, W, H * 0.62, hx("E8B48A"), hx("F5E6C8"), 14)
    sun(d, W * 0.25, H * 0.5, W * 0.11, hx("F07E4A"))
    d.rectangle((0, H * 0.62, W, H), fill=hx("4F7690"))
    d.polygon([(W * 0.4, H * 0.66), (W * 1.0, H * 0.6), (W, H * 0.75), (W * 0.5, H * 0.75)], fill=hx("6C5E4E"))
    lighthouse(d, W * 0.72, H * 0.63, H * 0.5, hx("EFE6D2"), hx("C8452D"), hx("F7D77A"))
    for k in range(4):
        gull(d, W * (0.15 + k * 0.12), H * (0.18 + 0.04 * k), W * 0.09, hx("3E3A36"), k * 0.25)
    return grain(paint_dabs(img, 16), 4, 16)


@cell("painting_still_life", 320, 384)
def p_still(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("5C4A3E"))
    d.rectangle((0, H * 0.62, W, H), fill=hx("C9B18A"))
    d.polygon([(W * 0.0, H * 0.62), (W * 0.55, H * 0.6), (W * 0.8, H * 1.0), (0, H)], fill=hx("EDE6D8"))
    d.polygon([(W * 0.38, H * 0.62), (W * 0.62, H * 0.62), (W * 0.58, H * 0.3), (W * 0.42, H * 0.3)], fill=hx("3C6E8E"))
    d.ellipse((W * 0.36, H * 0.4, W * 0.64, H * 0.66), fill=hx("3C6E8E"))
    for k in range(9):
        a = -math.pi / 2 + (k - 4) * 0.22
        x, y = W * 0.5 + math.cos(a) * W * 0.25, H * 0.3 + math.sin(a) * H * 0.2
        d.line([(W * 0.5, H * 0.32), (x, y)], fill=hx("4E7A3E"), width=3)
        d.ellipse((x - W * 0.05, y - W * 0.05, x + W * 0.05, y + W * 0.05),
                  fill=rnd.choice([hx("E8D9A0"), hx("F2F0E6"), hx("D98C7A"), hx("E7B23A")]))
    for k, c in enumerate([hx("C8452D"), hx("E3A43A"), hx("9BB04A")]):
        x = W * (0.15 + 0.2 * k)
        d.ellipse((x, H * 0.72, x + W * 0.15, H * 0.72 + W * 0.15), fill=c)
    return grain(paint_dabs(img, 17), 4, 17)


@cell("painting_portrait", 320, 400)
def p_portrait(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("7F9AA0"))
    d.ellipse((W * -0.2, H * 0.62, W * 1.2, H * 1.5), fill=hx("2F4A5F"))
    d.rectangle((W * 0.43, H * 0.48, W * 0.57, H * 0.66), fill=hx("D7A98A"))
    d.ellipse((W * 0.3, H * 0.18, W * 0.7, H * 0.56), fill=hx("D9AC8E"))
    d.pieslice((W * 0.26, H * 0.12, W * 0.74, H * 0.5), 180, 360, fill=hx("4A3022"))
    d.polygon([(W * 0.26, H * 0.31), (W * 0.32, H * 0.31), (W * 0.3, H * 0.62), (W * 0.22, H * 0.6)], fill=hx("4A3022"))
    d.polygon([(W * 0.74, H * 0.31), (W * 0.68, H * 0.31), (W * 0.7, H * 0.62), (W * 0.78, H * 0.6)], fill=hx("4A3022"))
    d.ellipse((W * 0.3, H * 0.62, W * 0.7, H * 0.7), fill=hx("EFE6D2"))
    return grain(paint_dabs(img, 18), 4, 18)


@cell("painting_abstract", 384, 384)
def p_abs(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("EDE3D0"))
    d.rectangle((W * 0.08, H * 0.08, W * 0.55, H * 0.6), fill=hx("C8452D"))
    d.ellipse((W * 0.4, H * 0.3, W * 0.92, H * 0.82), fill=hx("23395B"))
    d.rectangle((W * 0.15, H * 0.68, W * 0.85, H * 0.74), fill=hx("1E1B18"))
    d.polygon([(W * 0.6, H * 0.08), (W * 0.92, H * 0.08), (W * 0.92, H * 0.25)], fill=hx("E8B23A"))
    return grain(paint_dabs(img, 19, n=5000), 4, 19)


# ------------------------------------------------------------------------------------------------ posters
def poster_bg(img, d, W, H, c):
    d.rectangle((0, 0, W, H), fill=c)


def finish_print(img, seed, g=7):
    return vignette(grain(img, g, seed), 0.18)


BIRDS = [("Чайка серебристая", "gull"), ("Серая цапля", "heron"), ("Ласточка", "swallow")]


def draw_bird_poster(img, d, rnd, W, H, i):
    pal = PALETTES[[1, 3, 5][i]]
    poster_bg(img, d, W, H, pal[0])
    name, kind = BIRDS[i]
    d.rectangle((0, H * 0.62, W, H), fill=pal[3])
    sun(d, W * 0.72, H * 0.3, W * 0.17, pal[2])
    if kind == "gull":
        for k in range(4):
            gull(d, W * (0.3 + 0.13 * k), H * (0.25 + 0.08 * (k % 2)), W * (0.35 - 0.06 * k), pal[1], 0.2 * k)
    elif kind == "heron":
        heron(d, W * 0.42, H * 0.78, H * 0.55, pal[1])
        for k in range(5):
            d.line([(W * (0.1 + 0.18 * k), H * 0.78), (W * (0.1 + 0.18 * k) + 4, H * 0.62)], fill=pal[1], width=3)
    else:
        for k in range(5):
            swallow(d, W * (0.25 + 0.14 * k), H * (0.22 + 0.07 * ((k * 3) % 4)), W * 0.2, pal[1], -0.3 + 0.15 * k)
    text_c(d, "ПТИЦЫ", "dela", W / 2, H * 0.73, W * 0.84, H * 0.1, pal[0])
    text_c(d, "ЛЮДМИЛОВСКА", "dela", W / 2, H * 0.82, W * 0.84, H * 0.06, pal[0])
    text_c(d, name, "mpb", W / 2, H * 0.92, W * 0.8, H * 0.045, pal[1])
    return finish_print(img, 200 + i)


for _i in range(3):
    CELLS.append(("poster_birds_%d" % _i, 288, 400, (lambda i: lambda img, d, rnd, W, H: draw_bird_poster(img, d, rnd, W, H, i))(_i)))


@cell("poster_sun_1", 288, 400)
def poster_sun1(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("F1E4C8"))
    sun(d, W * 0.5, H * 0.4, W * 0.24, hx("E55B3C"), rays=16, ray_fill=hx("F2A03D"), ray_len=0.55)
    d.rectangle((0, H * 0.66, W, H), fill=hx("23395B"))
    for k in range(4):
        d.polygon(wave_poly(W, H * (0.68 + 0.07 * k), H * 0.012, W * 0.18, k, H * (0.71 + 0.07 * k)), fill=hx("8FB7C4"))
    text_c(d, "СОЛНЦЕ", "dela", W / 2, H * 0.1, W * 0.86, H * 0.1, hx("23395B"))
    text_c(d, "ВСЕМ!", "dela", W / 2, H * 0.92, W * 0.6, H * 0.08, hx("F1E4C8"))
    return finish_print(img, 210)


@cell("poster_sun_2", 400, 288)
def poster_sun2(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("23395B"))
    for k in range(6):
        r = H * (0.9 - k * 0.14)
        d.pieslice((W * 0.5 - r, H * 0.95 - r, W * 0.5 + r, H * 0.95 + r), 180, 360,
                   fill=[hx("E55B3C"), hx("F2A03D"), hx("F1E4C8")][k % 3])
    text_c(d, "ЛЕТО В ЛЮДМИЛОВСКЕ", "dela", W / 2, H * 0.12, W * 0.9, H * 0.11, hx("F1E4C8"))
    return finish_print(img, 211)


CONS = [("ЧИТАЙ", "КНИГИ!", 0), ("ВСЕ", "НА ПЛЯЖ!", 1), ("БЕРЕГИ", "ДЮНЫ", 2), ("УЧИСЬ", "ПЛАВАТЬ", 3),
        ("СТРОИМ", "ГОРОД У МОРЯ", 4)]


def draw_cons(img, d, rnd, W, H, i):
    a, b, kind = CONS[i]
    pal = PALETTES[i % 3 if i < 3 else i % 6]
    bg, c1, c2, c3 = pal[0], pal[1], pal[2], pal[3]
    poster_bg(img, d, W, H, bg)
    if kind == 0:
        d.polygon([(0, H * 0.55), (W, H * 0.25), (W, H * 0.42), (0, H * 0.72)], fill=c1)
        d.ellipse((W * 0.5, H * 0.04, W * 0.95, H * 0.34), fill=c3)
        d.rectangle((W * 0.1, H * 0.12, W * 0.42, H * 0.46), fill=c2)
        d.rectangle((W * 0.14, H * 0.15, W * 0.38, H * 0.43), fill=bg)
        for k in range(6):
            d.line([(W * 0.17, H * (0.19 + k * 0.04)), (W * 0.35, H * (0.19 + k * 0.04))], fill=c2, width=3)
    elif kind == 1:
        sun(d, W * 0.72, H * 0.26, W * 0.16, c1, rays=12, ray_fill=c3, ray_len=0.4)
        for k in range(5):
            d.polygon(wave_poly(W, H * (0.42 + 0.06 * k), H * 0.02, W * 0.25, k * 0.8, H * (0.47 + 0.06 * k)), fill=c3)
        for k in range(3):
            figure(d, W * (0.2 + 0.12 * k), H * 0.62, H * 0.2, c2)
    elif kind == 2:
        for k in range(5):
            d.polygon(wave_poly(W, H * (0.3 + 0.1 * k), H * 0.05, W * 0.6, k * 1.3, H), fill=mix(c3, c1, k / 5))
        d.rectangle((0, H * 0.7, W, H), fill=bg)
        for k in range(3):
            pine(d, W * (0.25 + 0.25 * k), H * (0.3 - 0.03 * (k % 2)), H * 0.2, c1)
    elif kind == 3:
        d.rectangle((0, H * 0.4, W, H * 0.7), fill=c3)
        for k in range(3):
            d.arc((W * (0.15 + 0.22 * k), H * 0.42, W * (0.35 + 0.22 * k), H * 0.62), 180, 360, fill=c1, width=6)
        d.ellipse((W * 0.62, H * 0.2, W * 0.78, H * 0.36), fill=c2)
        d.polygon([(W * 0.2, H * 0.36), (W * 0.62, H * 0.28), (W * 0.62, H * 0.33), (W * 0.2, H * 0.4)], fill=c2)
    else:
        for k in range(4):
            x = W * (0.1 + 0.2 * k)
            hh = H * (0.25 + 0.08 * ((k * 5) % 3))
            d.rectangle((x, H * 0.66 - hh, x + W * 0.15, H * 0.66), fill=[c1, c2, c3, INK][k])
            for j in range(int(hh / (H * 0.05))):
                d.rectangle((x + W * 0.03, H * 0.66 - hh + j * H * 0.05 + H * 0.015, x + W * 0.12,
                             H * 0.66 - hh + j * H * 0.05 + H * 0.03), fill=bg)
        d.polygon([(0, H * 0.1), (W, H * 0.02), (W, H * 0.08), (0, H * 0.16)], fill=c1)
    tc = c1 if kind != 0 else INK
    text_c(d, a, "dela", W / 2, H * 0.79, W * 0.86, H * 0.1, tc)
    text_c(d, b, "dela", W / 2, H * 0.9, W * 0.86, H * 0.075, c2 if kind else c1)
    return finish_print(img, 220 + i)


for _i in range(len(CONS)):
    CELLS.append(("poster_cons_%d" % _i, 288, 400, (lambda i: lambda img, d, rnd, W, H: draw_cons(img, d, rnd, W, H, i))(_i)))


@cell("poster_health", 288, 400)
def poster_health(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("EAF1EE"))
    d.rectangle((0, 0, W, H * 0.16), fill=hx("2F7A6E"))
    text_c(d, "БУДЬ ЗДОРОВ!", "dela", W / 2, H * 0.08, W * 0.86, H * 0.08, hx("EAF1EE"))
    d.rectangle((W * 0.42, H * 0.26, W * 0.58, H * 0.56), fill=hx("C8452D"))
    d.rectangle((W * 0.27, H * 0.33, W * 0.73, H * 0.49), fill=hx("C8452D"))
    for k, s in enumerate(["Мой руки перед едой", "Гуляй у моря", "Спи восемь часов", "Пей воду"]):
        d.ellipse((W * 0.1, H * (0.64 + k * 0.08), W * 0.15, H * (0.64 + k * 0.08) + W * 0.05), fill=hx("2F7A6E"))
        text_c(d, s, "mp", W * 0.2, H * (0.655 + k * 0.08), W * 0.72, H * 0.045, INK, anchor="l")
    return finish_print(img, 230)


@cell("poster_safety", 288, 400)
def poster_safety(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("F2E9D0"))
    d.polygon([(W * 0.5, H * 0.08), (W * 0.9, H * 0.55), (W * 0.1, H * 0.55)], fill=hx("E8B23A"))
    d.polygon([(W * 0.5, H * 0.16), (W * 0.8, H * 0.5), (W * 0.2, H * 0.5)], fill=hx("F2E9D0"))
    text_c(d, "!", "dela", W * 0.5, H * 0.38, W * 0.2, H * 0.2, INK)
    text_c(d, "НЕ ЗАПЛЫВАЙ", "dela", W / 2, H * 0.67, W * 0.86, H * 0.08, hx("C8452D"))
    text_c(d, "ЗА БУЙКИ", "dela", W / 2, H * 0.77, W * 0.86, H * 0.08, hx("C8452D"))
    text_c(d, "Спасательная станция Людмиловска", "mp", W / 2, H * 0.9, W * 0.86, H * 0.04, INK)
    return finish_print(img, 231)


CINEMA = [("ТЁПЛОЕ МОРЕ", "кинокомедия", 3), ("ДЕВОЧКА И ЧАЙКА", "фильм для детей", 1),
          ("ПОЛЁТ НАД ДЮНАМИ", "приключения", 5), ("ПОСЛЕДНИЙ ТРАМВАЙ", "мелодрама", 4),
          ("ТАЙНА МАЯКА", "детектив", 0), ("ЛЕТО НАВСЕГДА", "музыкальный фильм", 2)]


def draw_cinema(img, d, rnd, W, H, i):
    title, genre, p = CINEMA[i]
    pal = PALETTES[p]
    bg, c1, c2, c3 = pal
    poster_bg(img, d, W, H, c1)
    if i % 3 == 0:
        d.ellipse((W * 0.1, H * 0.12, W * 0.9, H * 0.68), fill=c2)
        figure(d, W * 0.42, H * 0.66, H * 0.42, c1)
        figure(d, W * 0.6, H * 0.66, H * 0.36, INK)
    elif i % 3 == 1:
        d.rectangle((0, H * 0.45, W, H * 0.68), fill=c3)
        gull(d, W * 0.6, H * 0.22, W * 0.5, bg, 0.3)
        figure(d, W * 0.3, H * 0.66, H * 0.3, c2)
    else:
        for k in range(4):
            d.polygon(wave_poly(W, H * (0.36 + 0.08 * k), H * 0.04, W * 0.5, k, H * 0.68), fill=mix(c2, c3, k / 4))
        d.polygon([(W * 0.2, H * 0.2), (W * 0.8, H * 0.15), (W * 0.55, H * 0.22)], fill=bg)
    d.rectangle((0, H * 0.68, W, H), fill=INK)
    text_c(d, title, "dela", W / 2, H * 0.76, W * 0.88, H * 0.08, bg)
    text_c(d, genre, "mpb", W / 2, H * 0.845, W * 0.7, H * 0.04, c2)
    text_c(d, "КИНОТЕАТР «МАЯК»", "mp", W / 2, H * 0.92, W * 0.7, H * 0.035, c3)
    return finish_print(img, 240 + i)


for _i in range(len(CINEMA)):
    CELLS.append(("cinema_%d" % _i, 288, 400, (lambda i: lambda img, d, rnd, W, H: draw_cinema(img, d, rnd, W, H, i))(_i)))


TRAVEL = [("ЛЮДМИЛОВСК", "жемчужина Балтики"), ("ОТДЫХАЙТЕ", "на балтийском берегу"),
          ("КАНАТНАЯ ДОРОГА", "весь город за 20 минут"), ("ДЮНЫ И СОСНЫ", "пешие прогулки летом"),
          ("ЯНТАРНЫЙ БЕРЕГ", "Людмиловск ждёт вас")]


def draw_travel(img, d, rnd, W, H, i):
    t1, t2 = TRAVEL[i]
    _sky(d, W, H * 0.6, [hx("8FC0D8"), hx("F2C08A"), hx("A9CBDD"), hx("CFE2E8"), hx("F5D6A6")][i],
         hx("F6EBD5"), 10)
    if i in (0, 4):
        sun(d, W * 0.7, H * 0.32, W * 0.12, hx("F07E4A"))
        d.rectangle((0, H * 0.5, W, H * 0.62), fill=hx("3E7CA6"))
        d.polygon(wave_poly(W, H * 0.6, H * 0.02, W * 0.5, 0, H), fill=hx("E2C48A"))
        lighthouse(d, W * 0.22, H * 0.6, H * 0.3, hx("F6EBD5"), hx("C8452D"), hx("F7D77A"))
        for k in range(3):
            gull(d, W * (0.4 + k * 0.1), H * (0.15 + k * 0.04), W * 0.08, INK)
    elif i == 1:
        d.rectangle((0, H * 0.48, W, H), fill=hx("2E6E8E"))
        d.polygon(wave_poly(W, H * 0.66, H * 0.03, W * 0.6, 1, H), fill=hx("EBD39A"))
        for k in range(4):
            x = W * (0.15 + k * 0.2)
            d.polygon([(x, H * 0.82), (x + W * 0.06, H * 0.7), (x + W * 0.12, H * 0.82)],
                      fill=[hx("C8452D"), hx("F6EBD5"), hx("E8B23A"), hx("3E7CA6")][k])
            d.line([(x + W * 0.06, H * 0.7), (x + W * 0.06, H * 0.86)], fill=INK, width=3)
        boat(d, W * 0.7, H * 0.5, H * 0.15, INK, hx("F6EBD5"))
    elif i == 2:
        d.polygon([(0, H * 0.7), (W * 0.3, H * 0.4), (W * 0.6, H * 0.6), (W, H * 0.35), (W, H), (0, H)], fill=hx("4E7A55"))
        d.line([(0, H * 0.25), (W, H * 0.45)], fill=INK, width=3)
        for k, x in enumerate((0.3, 0.7)):
            y = H * (0.25 + 0.2 * x) + H * 0.02
            d.line([(W * x, y - H * 0.02), (W * x, y + H * 0.04)], fill=INK, width=3)
            d.rounded_rectangle((W * x - W * 0.07, y + H * 0.04, W * x + W * 0.07, y + H * 0.13), radius=8,
                                fill=[hx("C8452D"), hx("E8B23A")][k])
            d.rectangle((W * x - W * 0.05, y + H * 0.055, W * x + W * 0.05, y + H * 0.085), fill=hx("CFE2E8"))
    else:
        d.polygon(wave_poly(W, H * 0.55, H * 0.04, W * 0.8, 0.5, H), fill=hx("E3C892"))
        for k in range(5):
            pine(d, W * (0.1 + 0.2 * k), H * (0.62 + 0.03 * (k % 2)), H * 0.35, hx("2F5D46"), hx("8A5A3A"))
    d.rectangle((0, H * 0.84, W, H), fill=hx("1F3A5F"))
    text_c(d, t1, "dela", W / 2, H * 0.89, W * 0.88, H * 0.06, hx("F6EBD5"))
    text_c(d, t2, "mpb", W / 2, H * 0.955, W * 0.8, H * 0.032, hx("E8B23A"))
    return finish_print(img, 250 + i)


for _i in range(len(TRAVEL)):
    CELLS.append(("travel_%d" % _i, 288, 400, (lambda i: lambda img, d, rnd, W, H: draw_travel(img, d, rnd, W, H, i))(_i)))


# ------------------------------------------------------------------------------------------------ kids' drawings
def crayon(d, pts, fill, rnd, width, closed=False, passes=3):
    for _ in range(passes):
        q = [(x + rnd.uniform(-width * 0.4, width * 0.4), y + rnd.uniform(-width * 0.4, width * 0.4)) for x, y in pts]
        if closed:
            q.append(q[0])
        d.line(q, fill=fill, width=width, joint="curve")


def scribble_fill(d, box, fill, rnd, width):
    x0, y0, x1, y1 = box
    y = y0
    while y < y1:
        d.line([(x0 + rnd.uniform(0, 3), y), (x1 - rnd.uniform(0, 3), y + rnd.uniform(-2, 2))], fill=fill, width=width)
        y += width * 0.9


def draw_kids(img, d, rnd, W, H, i):
    poster_bg(img, d, W, H, hx("F5F1E6"))
    lw = max(3, int(W * 0.012))
    if i == 0:      # house, sun, family
        crayon(d, [(W * 0.75, H * 0.15 + W * 0.1 * math.sin(a)) if False else
                   (W * 0.78 + W * 0.1 * math.cos(a), H * 0.17 + W * 0.1 * math.sin(a)) for a in np.linspace(0, 6.3, 20)],
               hx("F2B300"), rnd, lw)
        for k in range(10):
            a = k / 10 * 6.28
            crayon(d, [(W * 0.78 + W * 0.13 * math.cos(a), H * 0.17 + W * 0.13 * math.sin(a)),
                       (W * 0.78 + W * 0.2 * math.cos(a), H * 0.17 + W * 0.2 * math.sin(a))], hx("F2B300"), rnd, lw, passes=1)
        scribble_fill(d, (W * 0.1, H * 0.45, W * 0.5, H * 0.75), hx("E07A5F"), rnd, lw)
        crayon(d, [(W * 0.06, H * 0.47), (W * 0.3, H * 0.28), (W * 0.54, H * 0.47)], hx("8C2B22"), rnd, lw * 2)
        scribble_fill(d, (W * 0.2, H * 0.52, W * 0.3, H * 0.62), hx("6FA8DC"), rnd, lw)
        for k, c in enumerate([hx("3E7CA6"), hx("C8452D"), hx("4E9A62")]):
            x = W * (0.62 + 0.1 * k)
            s = H * (0.18 - 0.03 * k)
            crayon(d, [(x + s * 0.12 * math.cos(a), H * 0.75 - s + s * 0.12 * math.sin(a)) for a in np.linspace(0, 6.3, 10)], c, rnd, lw, passes=2)
            crayon(d, [(x, H * 0.75 - s * 0.88), (x, H * 0.75 - s * 0.35)], c, rnd, lw, passes=2)
            crayon(d, [(x - s * 0.15, H * 0.75), (x, H * 0.75 - s * 0.35), (x + s * 0.15, H * 0.75)], c, rnd, lw, passes=2)
            crayon(d, [(x - s * 0.2, H * 0.75 - s * 0.7), (x + s * 0.2, H * 0.75 - s * 0.7)], c, rnd, lw, passes=2)
        scribble_fill(d, (0, H * 0.78, W, H * 0.86), hx("6DBE45"), rnd, lw)
        text_c(d, "Моя семья", "cavb", W / 2, H * 0.93, W * 0.7, H * 0.08, hx("3E5BA6"))
    elif i == 1:    # sea with a boat and fish
        scribble_fill(d, (0, H * 0.5, W, H * 0.9), hx("4F8FD8"), rnd, lw)
        crayon(d, [(W * 0.3, H * 0.48), (W * 0.7, H * 0.48), (W * 0.62, H * 0.56), (W * 0.36, H * 0.56)], hx("8C5A2B"), rnd, lw * 2, True)
        crayon(d, [(W * 0.5, H * 0.48), (W * 0.5, H * 0.2), (W * 0.68, H * 0.44), (W * 0.5, H * 0.44)], hx("E04A3A"), rnd, lw)
        for k in range(4):
            x, y = W * rnd.uniform(0.15, 0.8), H * rnd.uniform(0.62, 0.84)
            crayon(d, [(x, y), (x + W * 0.06, y - H * 0.02), (x + W * 0.1, y), (x + W * 0.06, y + H * 0.02), (x, y),
                       (x - W * 0.03, y - H * 0.02), (x - W * 0.03, y + H * 0.02), (x, y)], hx("F29E1F"), rnd, lw, passes=2)
        for k in range(3):
            crayon(d, [(W * (0.15 + 0.2 * k), H * 0.2), (W * (0.19 + 0.2 * k), H * 0.17), (W * (0.23 + 0.2 * k), H * 0.2)],
                   hx("333333"), rnd, lw, passes=1)
        text_c(d, "Море", "cavb", W / 2, H * 0.08, W * 0.5, H * 0.09, hx("C8452D"))
    elif i == 2:    # cable car over hills
        scribble_fill(d, (0, H * 0.62, W, H * 0.9), hx("6DBE45"), rnd, lw)
        crayon(d, [(0, H * 0.25), (W, H * 0.45)], hx("333333"), rnd, lw, passes=1)
        for x in (0.3, 0.7):
            y = H * (0.25 + 0.2 * x)
            crayon(d, [(W * x, y), (W * x, y + H * 0.06)], hx("333333"), rnd, lw, passes=1)
            scribble_fill(d, (W * x - W * 0.08, y + H * 0.06, W * x + W * 0.08, y + H * 0.15), hx("E04A3A"), rnd, lw)
        sun(d, W * 0.12, H * 0.1, W * 0.07, hx("F2B300"))
        text_c(d, "Канатка", "cavb", W * 0.6, H * 0.08, W * 0.6, H * 0.08, hx("3E5BA6"))
    else:           # a bird and a tree
        crayon(d, [(W * 0.3, H * 0.9), (W * 0.32, H * 0.45)], hx("8C5A2B"), rnd, lw * 3)
        for k in range(12):
            x, y = W * 0.31 + rnd.uniform(-0.18, 0.18) * W, H * 0.38 + rnd.uniform(-0.12, 0.1) * H
            crayon(d, [(x + W * 0.06 * math.cos(a), y + W * 0.06 * math.sin(a)) for a in np.linspace(0, 6.3, 8)],
                   hx("4E9A62"), rnd, lw, passes=1)
        crayon(d, [(W * 0.62, H * 0.5), (W * 0.82, H * 0.45), (W * 0.9, H * 0.52), (W * 0.8, H * 0.6), (W * 0.62, H * 0.5)],
               hx("3E7CA6"), rnd, lw * 2)
        crayon(d, [(W * 0.9, H * 0.52), (W * 0.96, H * 0.53)], hx("F29E1F"), rnd, lw)
        crayon(d, [(W * 0.72, H * 0.5), (W * 0.78, H * 0.38), (W * 0.84, H * 0.5)], hx("3E7CA6"), rnd, lw)
        text_c(d, "птичка", "cavb", W * 0.7, H * 0.86, W * 0.5, H * 0.08, hx("E04A3A"))
    return grain(img, 6, 260 + i)


for _i in range(4):
    CELLS.append(("kids_%d" % _i, 320, 384 if _i % 2 else 320,
                  (lambda i: lambda img, d, rnd, W, H: draw_kids(img, d, rnd, W, H, i))(_i)))


# ------------------------------------------------------------------------------------------------ maps, charts
@cell("map_town", 512, 384)
def map_town(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("EFE8D6"))
    d.polygon(wave_poly(W, H * 0.78, H * 0.03, W * 0.6, 0.3, H), fill=hx("9CC3D5"))
    d.polygon([(0, 0), (W * 0.18, 0), (W * 0.22, H * 0.3), (W * 0.12, H * 0.6), (0, H * 0.7)], fill=hx("D8C48E"))
    for k in range(9):
        d.line([(W * (0.25 + k * 0.08), H * 0.05), (W * (0.22 + k * 0.08), H * 0.75)], fill=hx("FFFFFF"), width=6)
    for k in range(6):
        d.line([(W * 0.2, H * (0.1 + k * 0.11)), (W * 0.98, H * (0.08 + k * 0.11))], fill=hx("FFFFFF"), width=6)
    for k in range(18):
        x, y = W * rnd.uniform(0.25, 0.95), H * rnd.uniform(0.08, 0.7)
        d.rectangle((x, y, x + W * 0.04, y + H * 0.05), fill=rnd.choice([hx("D9A79A"), hx("C9C0B0"), hx("E3C374")]))
    d.ellipse((W * 0.55, H * 0.35, W * 0.7, H * 0.5), fill=hx("8FB27A"))
    d.line([(W * 0.05, H * 0.2), (W * 0.5, H * 0.3), (W * 0.95, H * 0.15)], fill=hx("C8452D"), width=4)
    d.rectangle((0, 0, W, H * 0.08), fill=hx("1F3A5F"))
    text_c(d, "ПЛАН ГОРОДА ЛЮДМИЛОВСКА", "mpb", W / 2, H * 0.04, W * 0.9, H * 0.05, hx("EFE8D6"))
    text_c(d, "Балтийское море", "cav", W * 0.5, H * 0.9, W * 0.4, H * 0.06, hx("1F3A5F"))
    text_c(d, "Топи", "cav", W * 0.08, H * 0.4, W * 0.14, H * 0.06, hx("6A5A3A"))
    d.ellipse((W * 0.6, H * 0.55, W * 0.63, H * 0.59), fill=hx("C8452D"))
    text_c(d, "вы здесь", "mpb", W * 0.66, H * 0.57, W * 0.15, H * 0.035, hx("C8452D"), anchor="l")
    return grain(img, 4, 300)


@cell("map_coast", 512, 384)
def map_coast(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("D7E6EA"))
    land = [(0, 0), (W, 0), (W, H * 0.25)]
    for k in range(20):
        t = k / 19
        land.append((W * (1 - t), H * (0.3 + 0.15 * math.sin(t * 7) + 0.1 * t)))
    land.append((0, H * 0.6))
    d.polygon(land, fill=hx("E8DDB8"))
    for k in range(7):
        d.line([(0, H * (0.65 + k * 0.05)), (W, H * (0.35 + k * 0.08))], fill=hx("B9D0D8"), width=2)
    for k in range(25):
        x, y = rnd.uniform(0, W), rnd.uniform(H * 0.6, H)
        text_c(d, str(rnd.randint(4, 40)), "mp", x, y, W * 0.05, H * 0.025, hx("5A7A8A"))
    sun(d, W * 0.85, H * 0.82, W * 0.06, hx("F6EBD5"), rays=8, ray_fill=hx("1F3A5F"), ray_len=0.8)
    text_c(d, "ЛОЦИЯ БЕРЕГА", "mpb", W * 0.3, H * 0.1, W * 0.5, H * 0.06, hx("1F3A5F"))
    return grain(img, 4, 301)


@cell("map_world", 512, 256)
def map_world(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("7FA9C0"))
    for k in range(1, 12):
        d.line([(W * k / 12, 0), (W * k / 12, H)], fill=hx("9CBFD1"), width=1)
    for k in range(1, 6):
        d.line([(0, H * k / 6), (W, H * k / 6)], fill=hx("9CBFD1"), width=1)
    r2 = random.Random(302)
    for k in range(7):
        cx, cy, R = W * r2.uniform(0.08, 0.92), H * r2.uniform(0.2, 0.8), W * r2.uniform(0.05, 0.13)
        pts = []
        for j in range(16):
            a = 2 * math.pi * j / 16
            rr = R * r2.uniform(0.6, 1.3)
            pts.append((cx + rr * math.cos(a) * 1.3, cy + rr * math.sin(a) * 0.9))
        d.polygon(pts, fill=r2.choice([hx("E3C892"), hx("A9C27E"), hx("D9A79A"), hx("C9D49A")]))
    return grain(img, 3, 302)


def table_cell_text(d, x, y, w, h, s, fname="mp", fill=INK, anchor="l"):
    text_c(d, s, fname, x + (w / 2 if anchor == "c" else w * 0.06), y + h / 2, w * 0.9, h * 0.7, fill, anchor=anchor)


@cell("chart_timetable_bus", 384, 512)
def tt_bus(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("F3EEE2"))
    d.rectangle((0, 0, W, H * 0.14), fill=hx("2E6E8E"))
    text_c(d, "АВТОБУС  7", "dela", W / 2, H * 0.07, W * 0.8, H * 0.08, hx("F3EEE2"))
    rows = 14
    for r in range(rows):
        y = H * (0.18 + r * 0.056)
        if r % 2:
            d.rectangle((W * 0.04, y, W * 0.96, y + H * 0.056), fill=hx("E2DBC9"))
        table_cell_text(d, W * 0.04, y, W * 0.18, H * 0.056, "%02d" % (6 + r), "mpb")
        mins = sorted(rnd.sample(range(0, 60, 5), 4))
        table_cell_text(d, W * 0.22, y, W * 0.74, H * 0.056, "   ".join("%02d" % m for m in mins))
    text_c(d, "Вокзал — Набережная — Канатная", "mp", W / 2, H * 0.97, W * 0.9, H * 0.03, INK)
    return grain(img, 3, 310)


@cell("chart_timetable_cable", 384, 512)
def tt_cable(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("23395B"))
    text_c(d, "КАНАТНАЯ ДОРОГА", "dela", W / 2, H * 0.07, W * 0.88, H * 0.06, hx("F2A03D"))
    stations = ["Набережная", "Рынок", "Площадь", "Библиотека", "Школа", "Санаторий", "Северная"]
    d.line([(W * 0.15, H * 0.17), (W * 0.15, H * 0.83)], fill=hx("F2A03D"), width=6)
    for k, s in enumerate(stations):
        y = H * (0.17 + 0.11 * k)
        d.ellipse((W * 0.15 - 12, y - 12, W * 0.15 + 12, y + 12), fill=hx("F1E4C8"), outline=hx("F2A03D"), width=5)
        text_c(d, s, "mpb", W * 0.24, y, W * 0.5, H * 0.035, hx("F1E4C8"), anchor="l")
        text_c(d, "%d мин" % (k * 3), "mp", W * 0.82, y, W * 0.2, H * 0.03, hx("8FB7C4"))
    text_c(d, "с 6:30 до 23:00, каждые 4 минуты", "mp", W / 2, H * 0.93, W * 0.88, H * 0.03, hx("F1E4C8"))
    return grain(img, 3, 311)


@cell("chart_eye", 288, 400)
def chart_eye(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("FAFAF5"))
    rows = ["Ш Б", "М Н К", "Ы М Б Ш", "Б Ы Н К М", "И Н Ш М К", "Н К И Б М Ш", "Ш И Н Б К Ы", "К Н Ш М Ы Б И",
            "Б К Ш М И Ы Н", "Н Ш Ы И К Б М Ш"]
    y = H * 0.04
    for k, s in enumerate(rows):
        h = H * (0.13 * 0.78 ** k)
        text_c(d, s, "mpb", W / 2, y + h / 2, W * 0.8, h, INK)
        text_c(d, "%.1f" % (0.1 * (k + 1)), "mp", W * 0.93, y + h / 2, W * 0.08, min(h, H * 0.02), hx("888888"))
        y += h * 1.25
    return grain(img, 2, 320)


@cell("chart_anatomy", 288, 400)
def chart_anatomy(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("F1ECE0"))
    text_c(d, "ЧЕЛОВЕК", "dela", W / 2, H * 0.06, W * 0.7, H * 0.06, hx("8C2B22"))
    cx = W * 0.45
    d.ellipse((cx - W * 0.08, H * 0.12, cx + W * 0.08, H * 0.24), fill=hx("E5B9A0"))
    d.polygon([(cx - W * 0.16, H * 0.26), (cx + W * 0.16, H * 0.26), (cx + W * 0.13, H * 0.58), (cx - W * 0.13, H * 0.58)], fill=hx("E5B9A0"))
    for s in (-1, 1):
        d.polygon([(cx + s * W * 0.16, H * 0.27), (cx + s * W * 0.22, H * 0.28), (cx + s * W * 0.25, H * 0.56), (cx + s * W * 0.2, H * 0.56)], fill=hx("E5B9A0"))
        d.polygon([(cx + s * W * 0.02, H * 0.58), (cx + s * W * 0.12, H * 0.58), (cx + s * W * 0.1, H * 0.92), (cx + s * W * 0.04, H * 0.92)], fill=hx("E5B9A0"))
    d.ellipse((cx - W * 0.1, H * 0.3, cx - W * 0.01, H * 0.42), fill=hx("D88A8A"))
    d.ellipse((cx + W * 0.01, H * 0.3, cx + W * 0.1, H * 0.42), fill=hx("D88A8A"))
    d.ellipse((cx - W * 0.03, H * 0.34, cx + W * 0.04, H * 0.4), fill=hx("B3372C"))
    d.ellipse((cx - W * 0.09, H * 0.43, cx + W * 0.05, H * 0.48), fill=hx("8A4A2C"))
    for k in range(5):
        d.arc((cx - W * 0.08, H * (0.48 + k * 0.018), cx + W * 0.08, H * (0.5 + k * 0.018)), 0, 180, fill=hx("D49A7A"), width=4)
    for k, (s, y) in enumerate([("лёгкие", 0.33), ("сердце", 0.38), ("печень", 0.45), ("кишечник", 0.52)]):
        d.line([(cx + W * 0.05, H * y), (W * 0.7, H * y)], fill=INK, width=2)
        text_c(d, s, "mp", W * 0.72, H * y, W * 0.26, H * 0.03, INK, anchor="l")
    return grain(img, 3, 321)


@cell("chart_periodic", 512, 320)
def chart_periodic(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("F3EEE2"))
    els = ("H He Li Be B C N O F Ne Na Mg Al Si P S Cl Ar K Ca Sc Ti V Cr Mn Fe Co Ni Cu Zn Ga Ge As Se Br Kr Rb Sr Y Zr "
           "Nb Mo Tc Ru Rh Pd Ag Cd In Sn Sb Te I Xe Cs Ba La Hf Ta W Re Os Ir Pt Au Hg Tl Pb Bi Po At Rn").split()
    pos = []
    for p in range(6):
        cols = [0, 17] if p == 0 else ([0, 1] + list(range(12, 18)) if p < 3 else list(range(18)))
        pos += [(p, c) for c in cols]
    cw, ch = W * 0.9 / 18, H * 0.8 / 7
    groups = [hx("E8B9A0"), hx("F2D59A"), hx("BFD8B8"), hx("B8CFE0"), hx("D9C2E0")]
    text_c(d, "ПЕРИОДИЧЕСКАЯ СИСТЕМА ЭЛЕМЕНТОВ", "mpb", W / 2, H * 0.06, W * 0.9, H * 0.06, INK)
    for (p, c), s in zip(pos, els):
        x, y = W * 0.05 + c * cw, H * 0.14 + p * ch
        g = 0 if c < 2 else (1 if c < 12 else (2 if c < 16 else (3 if c < 17 else 4)))
        d.rectangle((x + 1, y + 1, x + cw - 1, y + ch - 1), fill=groups[g], outline=hx("8A8478"))
        text_c(d, s, "mpb", x + cw / 2, y + ch * 0.55, cw * 0.8, ch * 0.5, INK)
    return grain(img, 3, 322)


@cell("chart_birds", 400, 288)
def chart_birds(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("EDE6D2"))
    text_c(d, "ПТИЦЫ НАШЕГО БЕРЕГА", "dela", W / 2, H * 0.08, W * 0.9, H * 0.08, hx("2F5D46"))
    names = ["чайка", "цапля", "ласточка", "кулик", "баклан", "крачка"]
    for k in range(6):
        cx, cy = W * (0.18 + 0.32 * (k % 3)), H * (0.38 + 0.38 * (k // 3))
        d.ellipse((cx - W * 0.12, cy - H * 0.17, cx + W * 0.12, cy + H * 0.13), fill=hx("F7F2E6"), outline=hx("C9BFA6"), width=3)
        if k == 1:
            heron(d, cx - W * 0.03, cy + H * 0.1, H * 0.25, hx("4A5560"))
        elif k == 2:
            swallow(d, cx, cy - H * 0.03, W * 0.15, hx("23395B"), 0.2)
        else:
            gull(d, cx, cy, W * 0.16, [hx("6A6A6A"), hx("23395B"), hx("8A5A3A"), hx("1E1B18"), hx("4A5560"), hx("6A6A6A")][k], 0.2)
        text_c(d, names[k], "mpb", cx, cy + H * 0.16, W * 0.22, H * 0.05, INK)
    return grain(img, 3, 323)


@cell("chart_calendar", 288, 400)
def chart_cal(img, d, rnd, W, H):
    poster_bg(img, d, W, H, hx("F7F3EA"))
    d.rectangle((0, 0, W, H * 0.45), fill=hx("3E7CA6"))
    sun(d, W * 0.7, H * 0.18, W * 0.1, hx("F2A03D"))
    d.polygon(wave_poly(W, H * 0.33, H * 0.02, W * 0.4, 0, H * 0.45), fill=hx("23395B"))
    text_c(d, "ИЮЛЬ", "dela", W / 2, H * 0.51, W * 0.5, H * 0.06, hx("C8452D"))
    days = "ПН ВТ СР ЧТ ПТ СБ ВС".split()
    cw = W * 0.9 / 7
    for c, s in enumerate(days):
        text_c(d, s, "mpb", W * 0.05 + cw * (c + 0.5), H * 0.59, cw * 0.8, H * 0.03, hx("C8452D") if c > 4 else INK)
    n = 1
    for r in range(5):
        for c in range(7):
            if r == 0 and c < 2 or n > 31:
                continue
            text_c(d, str(n), "mp", W * 0.05 + cw * (c + 0.5), H * (0.65 + r * 0.065), cw * 0.8, H * 0.035,
                   hx("C8452D") if c > 4 else INK)
            n += 1
    return grain(img, 3, 324)


# ------------------------------------------------------------------------------------------------ boards
BOARDS = [["Тема: Птицы Балтики", "1. Чайка серебристая", "2. Серая цапля", "3. Кулик-сорока", "Д/з: стр. 42, упр. 3"],
          ["Двенадцатое сентября", "Классная работа", "2 + 3 = 5      7 − 4 = 3", "12 : 4 = 3    5 × 6 = 30"],
          ["F = m · a", "v = s / t", "E = m · c²", "Лабораторная работа № 4"],
          ["Сегодня в клубе:", "18:00  шахматный турнир", "19:30  танцы", "Добро пожаловать!"]]


def draw_board(img, d, rnd, W, H, i):
    d.rectangle((0, 0, W, H), fill=hx("2E4A3F") if i != 3 else hx("2A2E2C"))
    for _ in range(30):         # wiped chalk clouds
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        d.ellipse((x, y, x + W * rnd.uniform(0.05, 0.25), y + H * rnd.uniform(0.03, 0.1)), fill=hx("3A564B") if i != 3 else hx("373B39"))
    img2 = img.filter(ImageFilter.GaussianBlur(W * 0.01))
    img.paste(img2)
    d = ImageDraw.Draw(img)
    lines = BOARDS[i]
    y = H * 0.1
    for k, s in enumerate(lines):
        h = H * (0.17 if k == 0 else 0.14)
        f = fit(d, s, "cavb" if k == 0 else "cav", W * 0.86, h, h)
        x = W * (0.5 - 0 if k == 0 else 0.07)
        b = d.textbbox((0, 0), s, font=f)
        tx = W / 2 - (b[2] - b[0]) / 2 - b[0] if k == 0 else W * 0.07 - b[0]
        d.text((tx, y - b[1]), s, font=f, fill=hx("E9EDE6"))
        y += h * 1.25
        _ = x
    if i == 0:
        swallow(d, W * 0.82, H * 0.6, W * 0.12, hx("E9EDE6"), 0.3)
    return grain(img, 4, 400 + i)


for _i in range(4):
    CELLS.append(("board_%d" % _i, 512, 256, (lambda i: lambda img, d, rnd, W, H: draw_board(img, d, rnd, W, H, i))(_i)))


MENUS = [("СТОЛОВАЯ", [("Борщ", "45"), ("Уха балтийская", "60"), ("Котлета с пюре", "80"), ("Салат «Весна»", "30"),
                      ("Компот", "10"), ("Чай", "5"), ("Пирожок с капустой", "12")]),
         ("КАФЕ «ЧАЙКА»", [("Мороженое", "25"), ("Кофе", "20"), ("Какао", "18"), ("Пирожное", "22"),
                          ("Лимонад", "15"), ("Блины", "35")])]


def draw_menu(img, d, rnd, W, H, i):
    title, items = MENUS[i]
    poster_bg(img, d, W, H, hx("F3EEE2") if i == 0 else hx("23395B"))
    fg = INK if i == 0 else hx("F3EEE2")
    acc = hx("C8452D") if i == 0 else hx("F2A03D")
    text_c(d, title, "dela", W / 2, H * 0.08, W * 0.86, H * 0.07, acc)
    text_c(d, "МЕНЮ", "mpb", W / 2, H * 0.15, W * 0.4, H * 0.04, fg)
    for k, (a, b) in enumerate(items):
        y = H * (0.24 + k * 0.1)
        text_c(d, a, "mp", W * 0.08, y, W * 0.62, H * 0.045, fg, anchor="l")
        text_c(d, b + " р.", "mpb", W * 0.82, y, W * 0.2, H * 0.045, acc)
        for x in np.arange(W * 0.08, W * 0.92, W * 0.025):
            d.point((x, y + H * 0.03), fill=mix(fg, hx("888888"), 0.5))
    return grain(img, 3, 410 + i)


for _i in range(2):
    CELLS.append(("menu_%d" % _i, 288, 400, (lambda i: lambda img, d, rnd, W, H: draw_menu(img, d, rnd, W, H, i))(_i)))

PRICES = [("ОВОЩИ — ФРУКТЫ", [("Картофель", "0-12"), ("Морковь", "0-18"), ("Яблоки", "0-90"), ("Капуста", "0-15"),
                              ("Лук", "0-20"), ("Огурцы", "1-10")]),
          ("ХЛЕБ", [("Батон", "0-22"), ("Чёрный", "0-16"), ("Бублик", "0-05"), ("Булочка", "0-07"), ("Рогалик", "0-09")])]


def draw_price(img, d, rnd, W, H, i):
    title, items = PRICES[i]
    poster_bg(img, d, W, H, hx("1F2421"))
    d.rectangle((W * 0.03, H * 0.03, W * 0.97, H * 0.97), outline=hx("C9A86A"), width=4)
    text_c(d, title, "mpb", W / 2, H * 0.1, W * 0.86, H * 0.07, hx("F2D59A"))
    for k, (a, b) in enumerate(items):
        y = H * (0.24 + k * 0.12)
        text_c(d, a, "cav", W * 0.08, y, W * 0.6, H * 0.08, hx("F3EEE2"), anchor="l")
        text_c(d, b, "cavb", W * 0.82, y, W * 0.25, H * 0.08, hx("F2D59A"))
    return grain(img, 3, 420 + i)


for _i in range(2):
    CELLS.append(("price_%d" % _i, 320, 384, (lambda i: lambda img, d, rnd, W, H: draw_price(img, d, rnd, W, H, i))(_i)))

NOTES = ["Продаю велосипед", "Пропал кот, рыжий", "Кружок рисования", "Хор ветеранов", "Шахматы по средам",
         "Сдаю комнату", "Нашли ключи", "Лекция о птицах", "Субботник", "Танцы в клубе", "Ищу попутчика",
         "Отдам котят", "Уроки музыки"]


def draw_notice(img, d, rnd, W, H, i):
    poster_bg(img, d, W, H, hx("B98C5A"))
    for _ in range(1500):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        d.point((x, y), fill=jit(hx("A07448"), rnd, 20))
    for k in range(9):
        w, h = W * rnd.uniform(0.18, 0.3), H * rnd.uniform(0.22, 0.38)
        x, y = rnd.uniform(0, W - w), rnd.uniform(0, H - h)
        c = rnd.choice([hx("F7F3EA"), hx("F7F3EA"), hx("F2E58F"), hx("CFE2E8"), hx("F2C6C0")])
        sheet = Image.new("RGBA", (int(w), int(h)), c + (255,))
        sd = ImageDraw.Draw(sheet)
        s = rnd.choice(NOTES)
        text_c(sd, s, "mpb" if rnd.random() < 0.5 else "cavb", w / 2, h * 0.2, w * 0.86, h * 0.14, INK)
        for j in range(5):
            yy = h * (0.38 + j * 0.1)
            sd.line([(w * 0.1, yy), (w * rnd.uniform(0.5, 0.9), yy)], fill=hx("777777"), width=2)
        if rnd.random() < 0.5:
            for j in range(6):
                xx = w * (0.08 + j * 0.15)
                sd.line([(xx, h * 0.88), (xx, h)], fill=hx("777777"), width=1)
        sheet = sheet.rotate(rnd.uniform(-6, 6), expand=True, resample=Image.BICUBIC)
        img.paste(sheet, (int(x), int(y)), sheet)
        d = ImageDraw.Draw(img)
        d.ellipse((x + w / 2 - 5, y + 4, x + w / 2 + 5, y + 14), fill=rnd.choice([hx("C8452D"), hx("3E7CA6"), hx("E8B23A")]))
    return grain(img, 4, 430 + i)


for _i in range(3):
    CELLS.append(("notice_%d" % _i, 400, 288, (lambda i: lambda img, d, rnd, W, H: draw_notice(img, d, rnd, W, H, i))(_i)))


# ------------------------------------------------------------------------------------------------ rugs
def rug_border(d, W, H, cols, bw):
    for k, c in enumerate(cols):
        m = bw * k
        d.rectangle((m, m, W - m, H - m), fill=c)


def draw_rug(img, d, rnd, W, H, i):
    pals = [[hx("7A1F1F"), hx("D9B26F"), hx("1F3354"), hx("EFE3C8")], [hx("5A1F2A"), hx("C9A86A"), hx("2E4A3F"), hx("E8D6B0")],
            [hx("23395B"), hx("E8B23A"), hx("C8452D"), hx("EFE6D2")], [hx("2F5D46"), hx("D9A441"), hx("8C2B22"), hx("F1E4C8")],
            [hx("8C2B22"), hx("1E1B18"), hx("D9B26F"), hx("EFE6D2")]]
    p = pals[i % len(pals)]
    if i in (0, 1, 4):          # medallion carpets
        bw = W * 0.035
        rug_border(d, W, H, [p[1], p[0], p[2], p[0]], bw)
        f0 = bw * 4
        d.rectangle((f0, f0, W - f0, H - f0), fill=p[0])
        for k in range(int((W - 2 * f0) / (W * 0.08))):
            x = f0 + (k + 0.5) * W * 0.08
            for y in (bw * 1.5, H - bw * 1.5):
                d.polygon([(x, y - bw * 0.4), (x + bw * 0.6, y), (x, y + bw * 0.4), (x - bw * 0.6, y)], fill=p[3])
        cx, cy = W / 2, H / 2
        for k, c in enumerate([p[2], p[1], p[3], p[0], p[2]]):
            s = 1 - k * 0.18
            d.polygon([(cx, cy - H * 0.3 * s), (cx + W * 0.3 * s, cy), (cx, cy + H * 0.3 * s), (cx - W * 0.3 * s, cy)], fill=c)
        for sx in (-1, 1):
            for sy in (-1, 1):
                d.pieslice((cx + sx * (W / 2 - f0) - W * 0.18, cy + sy * (H / 2 - f0) - W * 0.18,
                            cx + sx * (W / 2 - f0) + W * 0.18, cy + sy * (H / 2 - f0) + W * 0.18), 0, 360, fill=p[2])
                d.pieslice((cx + sx * (W / 2 - f0) - W * 0.11, cy + sy * (H / 2 - f0) - W * 0.11,
                            cx + sx * (W / 2 - f0) + W * 0.11, cy + sy * (H / 2 - f0) + W * 0.11), 0, 360, fill=p[1])
        d.rectangle((0, 0, f0 - bw, H), fill=None)
        d.rectangle((0, 0, W, f0), outline=None)
        for k in range(8):
            yy = H * (0.2 + k * 0.08)
            d.ellipse((W * 0.17 - 4, yy - 4, W * 0.17 + 4, yy + 4), fill=p[3])
            d.ellipse((W * 0.83 - 4, yy - 4, W * 0.83 + 4, yy + 4), fill=p[3])
    elif i == 2:                # kilim zigzags
        band = H / 12
        for k in range(12):
            c = p[k % 4]
            d.rectangle((0, k * band, W, (k + 1) * band), fill=c)
            n = 8
            for j in range(n):
                x = W * j / n
                d.polygon([(x, (k + 1) * band), (x + W / n / 2, k * band + band * 0.3), (x + W / n, (k + 1) * band)],
                          fill=p[(k + 2) % 4])
    elif i == 3:                # striped runner
        rug_border(d, W, H, [p[0]], 0)
        for k in range(16):
            d.rectangle((0, H * k / 16, W, H * (k + 0.5) / 16), fill=p[k % 3 + 1] if k % 2 else p[0])
        d.rectangle((W * 0.1, 0, W * 0.9, H), outline=p[3], width=6)
    elif i == 5:                # checker rag rug
        n = 8
        for a in range(n):
            for b in range(int(n * H / W) + 1):
                d.rectangle((W * a / n, W * b / n, W * (a + 1) / n, W * (b + 1) / n),
                            fill=[hx("C8452D"), hx("EFE6D2"), hx("3E7CA6"), hx("E8B23A")][(a + b * 3) % 4])
        for _ in range(3000):
            x, y = rnd.uniform(0, W), rnd.uniform(0, H)
            d.line([(x, y), (x + 4, y)], fill=jit(hx("7A6A5A"), rnd, 30), width=1)
    elif i == 6:                # mid-century shapes
        d.rectangle((0, 0, W, H), fill=hx("E8DCC2"))
        for _ in range(12):
            c = rnd.choice([hx("C8452D"), hx("2F5D46"), hx("E8B23A"), hx("23395B")])
            x, y, r = rnd.uniform(0, W), rnd.uniform(0, H), W * rnd.uniform(0.05, 0.15)
            if rnd.random() < 0.5:
                d.ellipse((x - r, y - r, x + r, y + r), fill=c)
            else:
                d.polygon([(x, y - r), (x + r, y + r), (x - r, y + r)], fill=c)
        d.rectangle((0, 0, W, H), outline=hx("1E1B18"), width=8)
    elif i == 7:                # Baltic waves
        d.rectangle((0, 0, W, H), fill=hx("D7E6EA"))
        for k in range(14):
            d.polygon(wave_poly(W, H * (0.04 + k * 0.07), H * 0.015, W * 0.25, k * 0.9, H * (0.08 + k * 0.07)),
                      fill=[hx("23395B"), hx("3E7CA6"), hx("8FB7C4")][k % 3])
    elif i == 8:                # round, concentric
        d.rectangle((0, 0, W, H), fill=hx("7A1F1F"))
        for k in range(7):
            r = W / 2 * (1 - k * 0.13)
            d.ellipse((W / 2 - r, H / 2 - r, W / 2 + r, H / 2 + r), fill=[hx("7A1F1F"), hx("D9B26F"), hx("2E4A3F"), hx("EFE3C8")][k % 4])
        for k in range(16):
            a = 2 * math.pi * k / 16
            d.ellipse((W / 2 + W * 0.42 * math.cos(a) - 6, H / 2 + H * 0.42 * math.sin(a) - 6,
                       W / 2 + W * 0.42 * math.cos(a) + 6, H / 2 + H * 0.42 * math.sin(a) + 6), fill=hx("EFE3C8"))
    else:                       # round braided
        d.rectangle((0, 0, W, H), fill=hx("C8452D"))
        for k in range(14):
            r = W / 2 * (1 - k * 0.07)
            d.ellipse((W / 2 - r, H / 2 - r, W / 2 + r, H / 2 + r),
                      fill=[hx("C8452D"), hx("E8B23A"), hx("3E7CA6"), hx("EFE6D2"), hx("4E9A62")][k % 5])
    # woven texture
    a = np.asarray(img).astype(np.float32)
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    weave = (np.sin(xx * 1.4) * np.sin(yy * 1.4) * 6).astype(np.float32)
    a += weave[..., None]
    return grain(Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)), 6, 500 + i)


for _i in range(10):
    nm = "rug_round_%d" % (_i - 8) if _i >= 8 else "rug_%d" % _i
    CELLS.append((nm, 256 if _i >= 8 else 256, 256 if _i >= 8 else 384,
                  (lambda i: lambda img, d, rnd, W, H: draw_rug(img, d, rnd, W, H, i))(_i)))


# ------------------------------------------------------------------------------------------------ tiles, mosaics
def draw_tile(img, d, rnd, W, H, i):
    n = 4 if i < 6 else 8
    s = W / n
    grout = hx("D8D4CA")
    d.rectangle((0, 0, W, H), fill=grout)
    for a in range(n):
        for b in range(n):
            x0, y0 = a * s + 1.5, b * s + 1.5
            x1, y1 = x0 + s - 3, y0 + s - 3
            cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
            if i == 0:          # azulejo blue/white
                d.rectangle((x0, y0, x1, y1), fill=hx("F1F0EA"))
                for k in range(4):
                    ang = k * math.pi / 2
                    px, py = cx + math.cos(ang) * s * 0.28, cy + math.sin(ang) * s * 0.28
                    d.ellipse((px - s * 0.12, py - s * 0.12, px + s * 0.12, py + s * 0.12), fill=hx("2E5B9A"))
                d.ellipse((cx - s * 0.1, cy - s * 0.1, cx + s * 0.1, cy + s * 0.1), fill=hx("E8B23A"))
                for (qx, qy) in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
                    d.pieslice((qx - s * 0.2, qy - s * 0.2, qx + s * 0.2, qy + s * 0.2), 0, 360, fill=hx("2E5B9A"))
            elif i == 1:        # wave azulejo (Burle Marx-ish)
                d.rectangle((x0, y0, x1, y1), fill=hx("F1F0EA"))
                rot = (a + b) % 2
                if rot:
                    d.pieslice((x0 - s * 0.5, y0 - s * 0.5, x0 + s * 0.5, y0 + s * 0.5), 0, 90, fill=hx("23395B"))
                    d.pieslice((x1 - s * 0.5, y1 - s * 0.5, x1 + s * 0.5, y1 + s * 0.5), 180, 270, fill=hx("23395B"))
                else:
                    d.pieslice((x1 - s * 0.5, y0 - s * 0.5, x1 + s * 0.5, y0 + s * 0.5), 90, 180, fill=hx("23395B"))
                    d.pieslice((x0 - s * 0.5, y1 - s * 0.5, x0 + s * 0.5, y1 + s * 0.5), 270, 360, fill=hx("23395B"))
            elif i == 2:        # checker
                d.rectangle((x0, y0, x1, y1), fill=hx("1E1B18") if (a + b) % 2 else hx("EFEAE0"))
            elif i == 3:        # half-diagonal triangles
                c1, c2 = hx("C8452D"), hx("EFE6D2")
                d.rectangle((x0, y0, x1, y1), fill=c2)
                d.polygon([(x0, y0), (x1, y0), (x0, y1)] if (a * 3 + b) % 2 else [(x1, y0), (x1, y1), (x0, y1)], fill=c1)
            elif i == 4:        # green glazed, uneven
                d.rectangle((x0, y0, x1, y1), fill=jit(hx("5E8C7A"), rnd, 18))
                d.rectangle((x0 + 3, y0 + 3, x1 - 3, y0 + 8), fill=jit(hx("8FB7A6"), rnd, 10))
            elif i == 5:        # sun tile
                d.rectangle((x0, y0, x1, y1), fill=hx("F2E7CF"))
                sun(d, cx, cy, s * 0.17, hx("E8833A"), rays=8, ray_fill=hx("C8452D"), ray_len=0.9)
            else:
                pass
    if i == 6:                  # mosaic: seascape of small tesserae
        m = 32
        t = W / m
        for a in range(m):
            for b in range(m):
                y = b / m
                if y < 0.45:
                    c = mix(hx("8FC0D8"), hx("F2D59A"), y / 0.45)
                    if math.hypot(a / m - 0.68, y - 0.28) < 0.1:
                        c = hx("F07E4A")
                elif y < 0.8 + 0.04 * math.sin(a / m * 9):
                    c = mix(hx("3E7CA6"), hx("1F3A5F"), (y - 0.45) / 0.35)
                else:
                    c = hx("E2C48A")
                d.rectangle((a * t + 1, b * t + 1, (a + 1) * t - 1, (b + 1) * t - 1), fill=jit(c, rnd, 14))
    elif i == 7:                # terrazzo
        d.rectangle((0, 0, W, H), fill=hx("E3DED3"))
        for _ in range(900):
            x, y, r = rnd.uniform(0, W), rnd.uniform(0, H), rnd.uniform(1.5, 7)
            d.ellipse((x - r, y - r * 0.7, x + r, y + r * 0.7), fill=rnd.choice([hx("8A8478"), hx("C8452D"), hx("3A3530"), hx("D9B26F"), hx("FFFFFF")]))
    elif i == 8:                # hexagons
        d.rectangle((0, 0, W, H), fill=grout)
        r = W / 10
        for row in range(14):
            for col in range(8):
                cx = col * r * 1.75 + (r * 0.87 if row % 2 else 0)
                cy = row * r * 0.76 * 1.0
                pts = [(cx + r * 0.95 * math.cos(math.pi / 6 + k * math.pi / 3) * 0.95,
                        cy + r * 0.95 * math.sin(math.pi / 6 + k * math.pi / 3) * 0.95) for k in range(6)]
                d.polygon(pts, fill=rnd.choice([hx("EFEAE0"), hx("EFEAE0"), hx("8FB7C4"), hx("23395B")]))
    elif i == 9:                # mosaic: a gull and sun, constructivist
        m = 32
        t = W / m
        for a in range(m):
            for b in range(m):
                x, y = a / m, b / m
                c = hx("E9DFC8")
                if math.hypot(x - 0.5, y - 0.45) < 0.32:
                    c = hx("E8833A")
                if y > 0.72:
                    c = hx("23395B") if (a + b) % 3 else hx("3E7CA6")
                if abs(y - (0.42 - 0.35 * abs(x - 0.5) + 0.25 * (x - 0.5) ** 2 * 4)) < 0.04 and 0.15 < x < 0.85:
                    c = hx("1E1B18")
                d.rectangle((a * t + 1, b * t + 1, (a + 1) * t - 1, (b + 1) * t - 1), fill=jit(c, rnd, 12))
    return grain(img, 3, 600 + i)


for _i in range(10):
    CELLS.append(("tile_%d" % _i, 256, 256, (lambda i: lambda img, d, rnd, W, H: draw_tile(img, d, rnd, W, H, i))(_i)))


# ------------------------------------------------------------------------------------------------ clocks, screens
def draw_clock(img, d, rnd, W, H, i):
    face = [hx("F4F0E6"), hx("1E1B18"), hx("F2E7CF"), hx("23395B")][i]
    ink = [INK, hx("F4F0E6"), hx("8C2B22"), hx("F2D59A")][i]
    d.rectangle((0, 0, W, H), fill=face)
    cx, cy, r = W / 2, H / 2, W * 0.46
    for k in range(60):
        a = 2 * math.pi * k / 60
        r0 = r * (0.86 if k % 5 == 0 else 0.92)
        d.line([(cx + r0 * math.sin(a), cy - r0 * math.cos(a)), (cx + r * 0.97 * math.sin(a), cy - r * 0.97 * math.cos(a))],
               fill=ink, width=max(2, int(W * (0.02 if k % 5 == 0 else 0.008))))
    if i in (0, 2):
        for k in range(1, 13):
            a = 2 * math.pi * k / 12
            text_c(d, str(k), "mpb", cx + r * 0.7 * math.sin(a), cy - r * 0.7 * math.cos(a), W * 0.12, W * 0.1, ink)
    hrs, mins = [(10, 10), (3, 40), (7, 25), (12, 0)][i]
    for ang, L, wd in ((2 * math.pi * (hrs % 12 + mins / 60) / 12, 0.5, 0.045), (2 * math.pi * mins / 60, 0.78, 0.028)):
        d.line([(cx, cy), (cx + r * L * math.sin(ang), cy - r * L * math.cos(ang))], fill=ink, width=int(W * wd))
    d.ellipse((cx - W * 0.03, cy - W * 0.03, cx + W * 0.03, cy + W * 0.03), fill=hx("C8452D"))
    return grain(img, 2, 700 + i)


for _i in range(4):
    CELLS.append(("clock_%d" % _i, 160, 160, (lambda i: lambda img, d, rnd, W, H: draw_clock(img, d, rnd, W, H, i))(_i)))


def scanlines(img, k=0.15):
    a = np.asarray(img).astype(np.float32)
    a[::3] *= (1 - k)
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    cx, cy = a.shape[1] / 2, a.shape[0] / 2
    r = ((xx - cx) / cx) ** 2 + ((yy - cy) / cy) ** 2
    a *= (1 - 0.35 * np.clip(r - 0.4, 0, 1))[..., None]
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


@cell("screen_tv_test", 256, 192)
def scr_test(img, d, rnd, W, H):
    cols = [hx("C0C0C0"), hx("C0C000"), hx("00C0C0"), hx("00C000"), hx("C000C0"), hx("C00000"), hx("0000C0")]
    for k, c in enumerate(cols):
        d.rectangle((W * k / 7, 0, W * (k + 1) / 7, H), fill=c)
    d.ellipse((W * 0.25, H * 0.12, W * 0.75, H * 0.88), outline=hx("FFFFFF"), width=4)
    d.rectangle((W * 0.3, H * 0.42, W * 0.7, H * 0.58), fill=hx("111111"))
    text_c(d, "ЛЮДМИЛОВСК ТВ", "mpb", W / 2, H * 0.5, W * 0.36, H * 0.1, hx("FFFFFF"))
    return scanlines(img)


@cell("screen_tv_news", 256, 192)
def scr_news(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("2A4A6A"))
    d.rectangle((W * 0.55, H * 0.15, W * 0.92, H * 0.6), fill=hx("8FB7C4"))
    gull(d, W * 0.73, H * 0.35, W * 0.2, hx("1E1B18"))
    d.ellipse((W * 0.15, H * 0.2, W * 0.35, H * 0.5), fill=hx("D9AC8E"))
    d.pieslice((W * 0.13, H * 0.16, W * 0.37, H * 0.4), 180, 360, fill=hx("3A2A20"))
    d.rectangle((W * 0.08, H * 0.5, W * 0.42, H * 0.8), fill=hx("23395B"))
    d.rectangle((0, H * 0.8, W, H), fill=hx("C8452D"))
    text_c(d, "НОВОСТИ ГОРОДА", "mpb", W / 2, H * 0.9, W * 0.8, H * 0.12, hx("FFFFFF"))
    return scanlines(img)


@cell("screen_tv_sea", 256, 192)
def scr_sea(img, d, rnd, W, H):
    _sky(d, W, H * 0.55, hx("6A9AC0"), hx("DCE8EE"))
    d.rectangle((0, H * 0.55, W, H), fill=hx("2E5E80"))
    boat(d, W * 0.4, H * 0.58, H * 0.3, hx("1E1B18"), hx("F3EEE2"))
    return scanlines(img.convert("L").convert("RGB") if False else img)


GAMES = [("ЧАЙКИ", hx("0B0F2A")), ("ПОДЛОДКА", hx("06283A")), ("КОСМОС-2", hx("000000"))]


def draw_arcade(img, d, rnd, W, H, i):
    title, bg = GAMES[i]
    d.rectangle((0, 0, W, H), fill=bg)
    if i == 0:
        for k in range(14):
            gull(d, W * (0.1 + 0.13 * (k % 7)), H * (0.25 + 0.15 * (k // 7)), W * 0.08, hx("F3EEE2"))
        d.polygon([(W * 0.45, H * 0.85), (W * 0.55, H * 0.85), (W * 0.5, H * 0.75)], fill=hx("F2A03D"))
    elif i == 1:
        d.rectangle((0, H * 0.3, W, H), fill=hx("0B3E5A"))
        for k in range(3):
            d.ellipse((W * (0.1 + k * 0.3), H * (0.5 + 0.1 * k), W * (0.28 + k * 0.3), H * (0.58 + 0.1 * k)), fill=hx("9AD06A"))
        d.rectangle((W * 0.4, H * 0.2, W * 0.6, H * 0.3), fill=hx("E8B23A"))
    else:
        for _ in range(60):
            x, y = rnd.uniform(0, W), rnd.uniform(0, H)
            d.point((x, y), fill=hx("FFFFFF"))
        for k in range(5):
            x = W * (0.15 + k * 0.17)
            d.rectangle((x, H * 0.3, x + W * 0.08, H * 0.36), fill=hx("E04A3A"))
        d.polygon([(W * 0.45, H * 0.88), (W * 0.55, H * 0.88), (W * 0.5, H * 0.76)], fill=hx("6AD0E0"))
    text_c(d, title, "dela", W / 2, H * 0.1, W * 0.7, H * 0.12, hx("F2D59A"))
    text_c(d, "ОЧКИ 001250", "mpb", W * 0.8, H * 0.95, W * 0.35, H * 0.06, hx("FFFFFF"))
    return scanlines(img, 0.25)


for _i in range(3):
    CELLS.append(("screen_arcade_%d" % _i, 192, 256, (lambda i: lambda img, d, rnd, W, H: draw_arcade(img, d, rnd, W, H, i))(_i)))


def draw_film(img, d, rnd, W, H, i):
    if i == 0:
        _sky(d, W, H * 0.6, hx("E8B48A"), hx("F7E3C2"), 10)
        d.rectangle((0, H * 0.6, W, H), fill=hx("3E6A86"))
        figure(d, W * 0.42, H * 0.75, H * 0.45, hx("1E1B18"))
        figure(d, W * 0.52, H * 0.75, H * 0.4, hx("1E1B18"))
    else:
        d.rectangle((0, 0, W, H), fill=hx("C9B79A"))
        for k in range(6):
            pine(d, W * (0.08 + 0.17 * k), H * 0.9, H * 0.7, hx("2F3F35"), hx("4A3A2A"))
        gull(d, W * 0.5, H * 0.2, W * 0.15, hx("1E1B18"))
    img = img.convert("L").convert("RGB") if i == 1 else img
    d = ImageDraw.Draw(img)
    d.rectangle((0, H * 0.84, W, H), fill=hx("000000") if i == 1 else hx("101010"))
    text_c(d, ["Я вернусь к морю.", "Слышишь? Это чайки."][i], "mp", W / 2, H * 0.92, W * 0.9, H * 0.08, hx("F3EEE2"))
    return grain(img, 6, 720 + i)


for _i in range(2):
    CELLS.append(("screen_film_%d" % _i, 384, 192, (lambda i: lambda img, d, rnd, W, H: draw_film(img, d, rnd, W, H, i))(_i)))


# ------------------------------------------------------------------------------------------------ sleeves, magazines, papers
SLEEVES = ["ВОЛНЫ", "ДЖАЗ У МОРЯ", "ВАЛЬСЫ", "ХОР ГОРОДА", "ПЕСНИ ДЮН", "СИМФОНИЯ №2", "ТАНЦЫ", "КОЛЫБЕЛЬНЫЕ"]


def draw_sleeve(img, d, rnd, W, H, i):
    pal = PALETTES[i % 6]
    d.rectangle((0, 0, W, H), fill=pal[1])
    kind = i % 4
    if kind == 0:
        for k in range(8):
            d.ellipse((W * 0.5 - W * 0.05 * k, H * 0.45 - W * 0.05 * k, W * 0.5 + W * 0.05 * k, H * 0.45 + W * 0.05 * k),
                      outline=pal[2], width=3)
    elif kind == 1:
        for k in range(5):
            d.rectangle((W * (0.1 + 0.16 * k), H * 0.15, W * (0.18 + 0.16 * k), H * (0.4 + 0.08 * k)), fill=pal[(k % 3) + 1 if k % 3 else 2])
    elif kind == 2:
        d.polygon([(0, H), (W * 0.6, H * 0.1), (W, H * 0.7)], fill=pal[3])
        sun(d, W * 0.3, H * 0.3, W * 0.12, pal[2])
    else:
        for k in range(6):
            d.polygon(wave_poly(W, H * (0.2 + 0.1 * k), H * 0.03, W * 0.4, k, H * (0.25 + 0.1 * k)), fill=pal[2 + k % 2])
    d.rectangle((0, H * 0.74, W, H), fill=pal[0])
    text_c(d, SLEEVES[i], "dela", W / 2, H * 0.84, W * 0.88, H * 0.12, pal[1])
    return grain(img, 5, 800 + i)


for _i in range(8):
    CELLS.append(("sleeve_%d" % _i, 160, 160, (lambda i: lambda img, d, rnd, W, H: draw_sleeve(img, d, rnd, W, H, i))(_i)))

MAGS = ["ЧАЙКА", "НАУКА И МЫ", "КИНОНЕДЕЛЯ", "ХОЗЯЙКА", "ЮНЫЙ ПТИЦЕЛОВ", "МОРЕ", "ЗДОРОВЬЕ", "ВЕСЁЛЫЕ КАРТИНКИ",
        "ТЕХНИКА", "СПОРТ"]


def draw_mag(img, d, rnd, W, H, i):
    pal = PALETTES[(i * 5) % 6]
    d.rectangle((0, 0, W, H), fill=pal[0])
    d.rectangle((0, 0, W, H * 0.2), fill=pal[1])
    text_c(d, MAGS[i], "dela", W / 2, H * 0.1, W * 0.9, H * 0.13, pal[0])
    k = i % 4
    if k == 0:
        gull(d, W * 0.5, H * 0.5, W * 0.6, pal[1])
    elif k == 1:
        sun(d, W * 0.5, H * 0.55, W * 0.2, pal[2], rays=10, ray_fill=pal[3])
    elif k == 2:
        figure(d, W * 0.5, H * 0.88, H * 0.6, pal[1])
    else:
        for j in range(3):
            d.rectangle((W * (0.15 + j * 0.25), H * (0.35 + j * 0.1), W * (0.35 + j * 0.25), H * 0.85), fill=pal[1 + j])
    text_c(d, "№ %d" % (i + 3), "mpb", W * 0.85, H * 0.95, W * 0.25, H * 0.05, pal[1])
    return grain(img, 5, 820 + i)


for _i in range(10):
    CELLS.append(("mag_%d" % _i, 128, 176, (lambda i: lambda img, d, rnd, W, H: draw_mag(img, d, rnd, W, H, i))(_i)))


def draw_news(img, d, rnd, W, H, i):
    d.rectangle((0, 0, W, H), fill=hx("E8E3D6"))
    text_c(d, ["ВЕСТНИК ЛЮДМИЛОВСКА", "ГОРОДСКАЯ НЕДЕЛЯ"][i], "dela", W / 2, H * 0.07, W * 0.92, H * 0.07, INK)
    d.line([(W * 0.04, H * 0.13), (W * 0.96, H * 0.13)], fill=INK, width=3)
    text_c(d, ["Чайки вернулись на набережную", "Открыт новый маршрут канатной дороги"][i], "mpb", W / 2, H * 0.19,
           W * 0.92, H * 0.045, INK)
    d.rectangle((W * 0.04, H * 0.25, W * 0.5, H * 0.55), fill=hx("9A968C"))
    gull(d, W * 0.27, H * 0.4, W * 0.25, hx("3A3A3A"))
    for col in range(3):
        x0 = W * (0.04 + col * 0.31) if col else W * 0.54
        x1 = x0 + W * (0.27 if col else 0.42)
        y0 = H * 0.27 if col == 0 else H * 0.58
        y = y0 if col == 0 else H * 0.58
        if col:
            x0, x1 = W * (0.04 + (col - 1) * 0.47), W * (0.04 + (col - 1) * 0.47 + 0.44)
        while y < H * 0.95:
            d.line([(x0, y), (x0 + (x1 - x0) * rnd.uniform(0.7, 1.0), y)], fill=hx("6A665E"), width=2)
            y += H * 0.018
    return grain(img, 4, 840 + i)


for _i in range(2):
    CELLS.append(("news_%d" % _i, 256, 352, (lambda i: lambda img, d, rnd, W, H: draw_news(img, d, rnd, W, H, i))(_i)))


@cell("paper_letter", 160, 224)
def paper_letter(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("F6F2E8"))
    for k in range(14):
        y = H * (0.1 + k * 0.06)
        d.line([(W * 0.08, y), (W * rnd.uniform(0.6, 0.92), y + rnd.uniform(-1, 1))], fill=hx("4A5A8A"), width=2)
    return grain(img, 3, 850)


@cell("paper_typed", 160, 224)
def paper_typed(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("F7F5EF"))
    text_c(d, "СПРАВКА", "mpb", W / 2, H * 0.08, W * 0.5, H * 0.05, INK)
    for k in range(18):
        y = H * (0.16 + k * 0.042)
        x = W * 0.1
        while x < W * rnd.uniform(0.7, 0.9):
            w = W * rnd.uniform(0.04, 0.14)
            d.rectangle((x, y, x + w, y + H * 0.012), fill=hx("3A3A3A"))
            x += w + W * 0.025
    return grain(img, 3, 851)


@cell("paper_drawing", 224, 160)
def paper_drawing(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("F2F4F6"))
    for k in range(0, W, 12):
        d.line([(k, 0), (k, H)], fill=hx("D6E0E8"))
    for k in range(0, H, 12):
        d.line([(0, k), (W, k)], fill=hx("D6E0E8"))
    d.rectangle((W * 0.2, H * 0.3, W * 0.7, H * 0.75), outline=hx("2E5B9A"), width=3)
    d.line([(W * 0.2, H * 0.3), (W * 0.45, H * 0.12), (W * 0.7, H * 0.3)], fill=hx("2E5B9A"), width=3)
    d.ellipse((W * 0.75, H * 0.45, W * 0.92, H * 0.75), outline=hx("2E5B9A"), width=3)
    return img


# ------------------------------------------------------------------------------------------------ labels, signs
GOODS = [("ШПРОТЫ", "fish"), ("КИЛЬКА", "fish"), ("ГОРОШЕК", "pea"), ("ТОМАТЫ", "dot"), ("СГУЩЁНКА", "drop"),
         ("ЧАЙ", "cup"), ("КОФЕ", "cup"), ("МУКА", "wheat"), ("СОЛЬ", "dot"), ("САХАР", "cube"), ("ВАРЕНЬЕ", "berry"),
         ("МЁД", "drop"), ("КРУПА", "wheat"), ("ПЕЧЕНЬЕ", "dot"), ("КОНФЕТЫ", "star"), ("СПИЧКИ", "flame"),
         ("МЫЛО", "bubble"), ("ПОРОШОК", "bubble"), ("КРАСКА", "drop"), ("ГВОЗДИ", "nail"), ("КЛЕЙ", "drop"),
         ("ЛИМОНАД", "bubble"), ("КВАС", "wheat"), ("МОЛОКО", "drop"), ("КЕФИР", "drop"), ("ОГУРЦЫ", "pea"),
         ("ГРИБЫ", "dot"), ("ПЕРЕЦ", "dot"), ("ИКРА", "dot"), ("КАКАО", "cup")]
LABEL_COLS = [(hx("C8452D"), hx("F3EEE2")), (hx("23395B"), hx("F2D59A")), (hx("2F5D46"), hx("F3EEE2")),
              (hx("E8B23A"), hx("1E1B18")), (hx("3E7CA6"), hx("FFFFFF")), (hx("F3EEE2"), hx("8C2B22")),
              (hx("5B3A5C"), hx("F2D59A")), (hx("D98C7A"), hx("23395B"))]


def icon(d, kind, cx, cy, s, c):
    if kind == "fish":
        d.ellipse((cx - s, cy - s * 0.45, cx + s * 0.6, cy + s * 0.45), fill=c)
        d.polygon([(cx + s * 0.5, cy), (cx + s, cy - s * 0.45), (cx + s, cy + s * 0.45)], fill=c)
    elif kind == "pea":
        for k in range(3):
            d.ellipse((cx - s + k * s * 0.7, cy - s * 0.3, cx - s * 0.4 + k * s * 0.7, cy + s * 0.3), fill=c)
    elif kind == "cup":
        d.rectangle((cx - s * 0.6, cy - s * 0.4, cx + s * 0.4, cy + s * 0.5), fill=c)
        d.arc((cx + s * 0.2, cy - s * 0.25, cx + s * 0.8, cy + s * 0.3), 270, 90, fill=c, width=max(2, int(s * 0.2)))
    elif kind == "wheat":
        d.line([(cx, cy + s), (cx, cy - s)], fill=c, width=max(2, int(s * 0.12)))
        for k in range(4):
            y = cy - s * 0.8 + k * s * 0.4
            d.ellipse((cx - s * 0.45, y - s * 0.15, cx - s * 0.05, y + s * 0.15), fill=c)
            d.ellipse((cx + s * 0.05, y - s * 0.15, cx + s * 0.45, y + s * 0.15), fill=c)
    elif kind == "drop":
        d.ellipse((cx - s * 0.5, cy - s * 0.1, cx + s * 0.5, cy + s * 0.8), fill=c)
        d.polygon([(cx - s * 0.45, cy + s * 0.2), (cx, cy - s * 0.8), (cx + s * 0.45, cy + s * 0.2)], fill=c)
    elif kind == "cube":
        d.rectangle((cx - s * 0.5, cy - s * 0.5, cx + s * 0.5, cy + s * 0.5), fill=c)
    elif kind == "berry":
        for dx, dy in ((-0.3, 0.1), (0.3, 0.1), (0, -0.3)):
            d.ellipse((cx + dx * s - s * 0.35, cy + dy * s - s * 0.35, cx + dx * s + s * 0.35, cy + dy * s + s * 0.35), fill=c)
    elif kind == "star":
        pts = [(cx + s * (1 if k % 2 == 0 else 0.45) * math.sin(k * math.pi / 5), cy - s * (1 if k % 2 == 0 else 0.45) * math.cos(k * math.pi / 5)) for k in range(10)]
        d.polygon(pts, fill=c)
    elif kind == "flame":
        d.polygon([(cx, cy - s), (cx + s * 0.45, cy + s * 0.2), (cx, cy + s * 0.7), (cx - s * 0.45, cy + s * 0.2)], fill=c)
    elif kind == "bubble":
        for dx, dy, r in ((-0.3, 0.2, 0.4), (0.35, -0.2, 0.3), (0.1, 0.55, 0.2)):
            d.ellipse((cx + dx * s - r * s, cy + dy * s - r * s, cx + dx * s + r * s, cy + dy * s + r * s), outline=c, width=max(2, int(s * 0.12)))
    elif kind == "nail":
        d.rectangle((cx - s * 0.08, cy - s * 0.6, cx + s * 0.08, cy + s * 0.9), fill=c)
        d.rectangle((cx - s * 0.4, cy - s * 0.8, cx + s * 0.4, cy - s * 0.6), fill=c)
    else:
        d.ellipse((cx - s * 0.5, cy - s * 0.5, cx + s * 0.5, cy + s * 0.5), fill=c)


def draw_label(img, d, rnd, W, H, i):
    name, kind = GOODS[i % len(GOODS)]
    bg, fg = LABEL_COLS[(i * 3 + i // len(GOODS)) % len(LABEL_COLS)]
    d.rectangle((0, 0, W, H), fill=bg)
    style = (i // len(GOODS)) % 2
    if style == 0:
        d.rectangle((0, H * 0.62, W, H * 0.92), fill=fg)
        icon(d, kind, W / 2, H * 0.32, H * 0.2, fg)
        text_c(d, name, "mpb", W / 2, H * 0.77, W * 0.88, H * 0.2, bg)
    else:
        d.ellipse((W / 2 - H * 0.3, H * 0.1, W / 2 + H * 0.3, H * 0.7), fill=fg)
        icon(d, kind, W / 2, H * 0.4, H * 0.18, bg)
        text_c(d, name, "dela", W / 2, H * 0.84, W * 0.9, H * 0.18, fg)
    d.rectangle((0, 0, W, H * 0.05), fill=mix(bg, (0, 0, 0), 0.3))
    return grain(img, 4, 900 + i)


many("label_", 60, 112, 84, draw_label)

SIGNS = [("ХЛЕБ", hx("8C2B22"), hx("F2D59A")), ("МОЛОКО", hx("3E7CA6"), hx("FFFFFF")), ("КНИГИ", hx("2F5D46"), hx("F3EEE2")),
         ("АПТЕКА", hx("F3EEE2"), hx("2F7A6E")), ("ОВОЩИ — ФРУКТЫ", hx("4E9A62"), hx("F3EEE2")),
         ("КИНО", hx("1E1B18"), hx("F2A03D")), ("СТОЛОВАЯ", hx("C8452D"), hx("F3EEE2")),
         ("ПАРИКМАХЕРСКАЯ", hx("23395B"), hx("F2D59A")), ("ТКАНИ", hx("5B3A5C"), hx("F3EEE2")),
         ("ИГРУШКИ", hx("E8B23A"), hx("23395B")), ("БИБЛИОТЕКА", hx("23395B"), hx("F3EEE2")),
         ("КАССА", hx("F3EEE2"), hx("1E1B18")), ("ГАРДЕРОБ", hx("F3EEE2"), hx("1E1B18")), ("БУФЕТ", hx("8C2B22"), hx("F3EEE2")),
         ("ТИШИНА!", hx("F3EEE2"), hx("C8452D")), ("СПОРТЗАЛ", hx("2E6E8E"), hx("F3EEE2")),
         ("ФОТО", hx("E8B23A"), hx("1E1B18"))]


def draw_sign(img, d, rnd, W, H, i):
    t, bg, fg = SIGNS[i]
    d.rectangle((0, 0, W, H), fill=bg)
    d.rectangle((W * 0.015, H * 0.08, W * 0.985, H * 0.92), outline=fg, width=max(2, int(H * 0.04)))
    text_c(d, t, "dela" if i % 3 != 2 else "mpb", W / 2, H * 0.5, W * 0.88, H * 0.6, fg)
    return grain(img, 3, 950 + i)


for _i in range(len(SIGNS)):
    nm = "sign_" + ["bread", "milk", "books", "pharmacy", "greens", "cinema", "canteen", "barber", "fabric", "toys",
                    "library", "till", "cloakroom", "buffet", "quiet", "gym", "photo"][_i]
    CELLS.append((nm, 384, 96, (lambda i: lambda img, d, rnd, W, H: draw_sign(img, d, rnd, W, H, i))(_i)))


@cell("exit_sign", 192, 96)
def exit_sign(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("2E8B57"))
    figure(d, W * 0.2, H * 0.85, H * 0.7, hx("FFFFFF"))
    text_c(d, "ВЫХОД", "mpb", W * 0.62, H * 0.5, W * 0.6, H * 0.5, hx("FFFFFF"))
    return img


# ------------------------------------------------------------------------------------------------ misc surfaces
@cell("keys_piano", 1024, 64)
def keys_piano(img, d, rnd, W, H):
    """52 white keys left to right; black keys on the top 62 % (the part away from the player). v up = away."""
    d.rectangle((0, 0, W, H), fill=hx("1A1A1A"))
    n = 52
    kw = W / n
    for k in range(n):
        d.rectangle((k * kw + 1, 0, (k + 1) * kw - 1, H), fill=hx("F2EEE4"))
    for k in range(n - 1):
        if (k + 5) % 7 in (2, 6):       # no black key between E-F and B-C (keyboard starts at A0)
            continue
        x = (k + 1) * kw
        d.rectangle((x - kw * 0.3, 0, x + kw * 0.3, H * 0.62), fill=hx("151515"))
    return img


@cell("chess_board", 256, 256)
def chess_board(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("4A2E1E"))
    m = W * 0.06
    s = (W - 2 * m) / 8
    for a in range(8):
        for b in range(8):
            d.rectangle((m + a * s, m + b * s, m + (a + 1) * s, m + (b + 1) * s), fill=hx("E8D2A6") if (a + b) % 2 == 0 else hx("7A4A2A"))
    return grain(img, 4, 990)


@cell("dartboard", 192, 192)
def dartboard(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("1E1B18"))
    cx, cy = W / 2, H / 2
    for ring_, (r0, colours) in enumerate(((0.47, (hx("C8452D"), hx("2F7A4E"))), (0.44, (hx("1E1B18"), hx("EFE3C8"))),
                                           (0.29, (hx("C8452D"), hx("2F7A4E"))), (0.26, (hx("1E1B18"), hx("EFE3C8"))))):
        for k in range(20):
            a0, a1 = -99 + k * 18, -99 + (k + 1) * 18
            d.pieslice((cx - W * r0, cy - W * r0, cx + W * r0, cy + W * r0), a0, a1, fill=colours[k % 2])
    d.ellipse((cx - W * 0.06, cy - W * 0.06, cx + W * 0.06, cy + W * 0.06), fill=hx("2F7A4E"))
    d.ellipse((cx - W * 0.025, cy - W * 0.025, cx + W * 0.025, cy + W * 0.025), fill=hx("C8452D"))
    return img


@cell("aquarium_back", 384, 192)
def aquarium(img, d, rnd, W, H):
    for k in range(12):
        d.rectangle((0, H * k / 12, W, H * (k + 1) / 12 + 1), fill=mix(hx("5FB0B8"), hx("1E5A6A"), k / 11))
    d.polygon(wave_poly(W, H * 0.85, H * 0.03, W * 0.5, 0, H), fill=hx("D8C08A"))
    for k in range(9):
        x = W * rnd.uniform(0.05, 0.95)
        h = H * rnd.uniform(0.3, 0.7)
        pts = [(x + math.sin(t * 4) * W * 0.02, H * 0.9 - h * t) for t in np.linspace(0, 1, 10)]
        d.line(pts, fill=jit(hx("3E8A4E"), rnd, 20), width=int(W * 0.012))
    for k in range(6):
        x, y = W * rnd.uniform(0.1, 0.9), H * rnd.uniform(0.2, 0.7)
        icon(d, "fish", x, y, W * 0.03, rnd.choice([hx("F29E1F"), hx("E04A3A"), hx("F2D59A")]))
    return grain(img, 3, 991)


@cell("dial_phone", 128, 128)
def dial_phone(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("1E1B18"))
    d.ellipse((W * 0.05, H * 0.05, W * 0.95, H * 0.95), fill=hx("E8E2D6"))
    for k in range(10):
        a = math.radians(60 + k * 27)
        x, y = W / 2 + W * 0.33 * math.cos(a), H / 2 + H * 0.33 * math.sin(a)
        d.ellipse((x - W * 0.08, y - W * 0.08, x + W * 0.08, y + W * 0.08), fill=hx("1E1B18"))
        text_c(d, str((k + 1) % 10), "mpb", x, y, W * 0.1, W * 0.1, hx("E8E2D6"))
    d.ellipse((W * 0.35, H * 0.35, W * 0.65, H * 0.65), fill=hx("C8452D"))
    return img


@cell("dial_radio", 384, 96)
def dial_radio(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("E9DFC0"))
    for k in range(40):
        x = W * (0.05 + 0.9 * k / 39)
        d.line([(x, H * 0.55), (x, H * (0.75 if k % 5 else 0.85))], fill=INK, width=2)
    for k, s in enumerate(["Москва", "Рига", "Таллин", "Людмиловск", "Хельсинки"]):
        text_c(d, s, "mp", W * (0.12 + 0.19 * k), H * 0.3, W * 0.17, H * 0.22, INK)
    d.line([(W * 0.68, H * 0.1), (W * 0.68, H * 0.95)], fill=hx("C8452D"), width=4)
    return grain(img, 3, 992)


@cell("record_label", 96, 96)
def record_label(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("151515"))
    d.ellipse((W * 0.3, H * 0.3, W * 0.7, H * 0.7), fill=hx("C8452D"))
    d.ellipse((W * 0.48, H * 0.48, W * 0.52, H * 0.52), fill=hx("151515"))
    for k in range(5):
        r = W * (0.36 + k * 0.025)
        d.ellipse((W / 2 - r, H / 2 - r, W / 2 + r, H / 2 + r), outline=hx("2A2A2A"), width=1)
    return img


@cell("typewriter_keys", 192, 96)
def tw_keys(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("2A2E2C"))
    rows = ["ЙЦУКЕНГШЩЗХ", "ФЫВАПРОЛДЖЭ", "ЯЧСМИТЬБЮ"]
    for r, s in enumerate(rows):
        for k, ch in enumerate(s):
            x = W * (0.06 + r * 0.03 + k * 0.08)
            y = H * (0.2 + r * 0.28)
            d.ellipse((x, y, x + W * 0.065, y + W * 0.065), fill=hx("E8E2D6"))
            text_c(d, ch, "mpb", x + W * 0.0325, y + W * 0.0325, W * 0.05, W * 0.05, INK)
    return img


@cell("cash_display", 128, 64)
def cash_display(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("1E1B18"))
    text_c(d, "1-25", "mpb", W / 2, H / 2, W * 0.8, H * 0.6, hx("7FE07F"))
    return img


@cell("panel_control", 192, 128)
def panel_control(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("2A2E2C"))
    for k in range(3):
        d.ellipse((W * (0.1 + k * 0.15), H * 0.55, W * (0.2 + k * 0.15), H * 0.55 + W * 0.1), fill=[hx("C8452D"), hx("E8B23A"), hx("3E7CA6")][k])
    d.ellipse((W * 0.65, H * 0.25, W * 0.85, H * 0.25 + W * 0.2), fill=hx("151515"))
    d.rectangle((W * 0.1, H * 0.15, W * 0.55, H * 0.4), fill=hx("4A7A5A"))
    return img


FABRICS = [(hx("C8452D"), hx("F3EEE2"), "dots"), (hx("23395B"), hx("F2D59A"), "stripes"), (hx("2F5D46"), hx("E8B23A"), "check"),
           (hx("E8DCC2"), hx("8C2B22"), "flowers"), (hx("D98C7A"), hx("F3EEE2"), "check"), (hx("3E7CA6"), hx("F3EEE2"), "waves")]


def draw_fabric(img, d, rnd, W, H, i):
    a, b, kind = FABRICS[i]
    d.rectangle((0, 0, W, H), fill=a)
    if kind == "dots":
        for y in range(0, H, 16):
            for x in range(8 if (y // 16) % 2 else 0, W, 16):
                d.ellipse((x - 3, y - 3, x + 3, y + 3), fill=b)
    elif kind == "stripes":
        for x in range(0, W, 24):
            d.rectangle((x, 0, x + 8, H), fill=b)
    elif kind == "check":
        for x in range(0, W, 32):
            d.rectangle((x, 0, x + 12, H), fill=mix(a, b, 0.5))
        for y in range(0, H, 32):
            d.rectangle((0, y, W, y + 12), fill=mix(a, b, 0.5))
    elif kind == "flowers":
        for _ in range(40):
            x, y = rnd.uniform(0, W), rnd.uniform(0, H)
            for k in range(5):
                ang = k * 2 * math.pi / 5
                d.ellipse((x + 6 * math.cos(ang) - 4, y + 6 * math.sin(ang) - 4, x + 6 * math.cos(ang) + 4, y + 6 * math.sin(ang) + 4), fill=b)
    else:
        for k in range(10):
            d.polygon(wave_poly(W, H * k / 10, 4, W / 4, k, H * k / 10 + 6), fill=b)
    return grain(img, 5, 970 + i)


for _i in range(len(FABRICS)):
    CELLS.append(("fabric_%d" % _i, 128, 128, (lambda i: lambda img, d, rnd, W, H: draw_fabric(img, d, rnd, W, H, i))(_i)))


@cell("canvas_blank", 128, 160)
def canvas_blank(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("EDE7DA"))
    a = np.asarray(img).astype(np.float32)
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    a += (np.sin(xx * 2.0) * np.sin(yy * 2.0) * 5)[..., None]
    img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    d.line([(W * 0.2, H * 0.6), (W * 0.5, H * 0.35), (W * 0.8, H * 0.55)], fill=hx("8A8478"), width=2)
    d.ellipse((W * 0.55, H * 0.15, W * 0.75, H * 0.3), outline=hx("8A8478"), width=2)
    return img


@cell("film_reel_side", 128, 128)
def film_reel(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("2A2A2A"))
    d.ellipse((W * 0.03, H * 0.03, W * 0.97, H * 0.97), fill=hx("8A8C8E"))
    for k in range(5):
        a = k * 2 * math.pi / 5
        x, y = W / 2 + W * 0.28 * math.cos(a), H / 2 + H * 0.28 * math.sin(a)
        d.ellipse((x - W * 0.12, y - W * 0.12, x + W * 0.12, y + W * 0.12), fill=hx("2A2A2A"))
    d.ellipse((W * 0.44, H * 0.44, W * 0.56, H * 0.56), fill=hx("2A2A2A"))
    return img


@cell("grille", 128, 128)
def grille(img, d, rnd, W, H):
    d.rectangle((0, 0, W, H), fill=hx("C9B79A"))
    for k in range(0, H, 6):
        d.line([(0, k), (W, k)], fill=hx("8A7A5A"), width=2)
    return img


# ------------------------------------------------------------------------------------------------ pack and write
def pack(cells):
    """Shelf packing, tallest first (stable by name); returns {name: (x, y, w, h)} of the drawn rects."""
    order = sorted(cells, key=lambda c: (-(c[2] + 2 * PAD), -(c[1] + 2 * PAD), c[0]))
    out = {}
    x = y = 0
    shelf_h = 0
    for name, w, h, _ in order:
        W, H = w + 2 * PAD, h + 2 * PAD
        if x + W > SIZE:
            x, y = 0, y + shelf_h
            shelf_h = 0
        if y + H > SIZE:
            raise RuntimeError("atlas full at %s (%d cells placed)" % (name, len(out)))
        out[name] = (x + PAD, y + PAD, w, h)
        x += W
        shelf_h = max(shelf_h, H)
    return out, y + shelf_h


def paste_padded(atlas, img, x, y):
    w, h = img.size
    atlas.paste(img, (x, y))
    for k in range(1, PAD + 1):
        atlas.paste(img.crop((0, 0, w, 1)), (x, y - k))
        atlas.paste(img.crop((0, h - 1, w, h)), (x, y + h - 1 + k))
    strip_l = atlas.crop((x, y - PAD, x + 1, y + h + PAD))
    strip_r = atlas.crop((x + w - 1, y - PAD, x + w, y + h + PAD))
    for k in range(1, PAD + 1):
        atlas.paste(strip_l, (x - k, y - PAD))
        atlas.paste(strip_r, (x + w - 1 + k, y - PAD))


def build(sheet=False):
    names = [c[0] for c in CELLS]
    dup = {n for n in names if names.count(n) > 1}
    if dup:
        raise RuntimeError("duplicate cells: %s" % sorted(dup))
    rects, used_h = pack(CELLS)
    atlas = Image.new("RGB", (SIZE, SIZE), (128, 124, 118))
    for i, (name, w, h, fn) in enumerate(sorted(CELLS, key=lambda c: c[0])):
        rnd = random.Random(sum(ord(ch) * (k + 1) for k, ch in enumerate(name)))
        img = Image.new("RGB", (w * SS, h * SS), (255, 255, 255))
        d = ImageDraw.Draw(img)
        res = fn(img, d, rnd, w * SS, h * SS)
        img = (res or img).resize((w, h), Image.LANCZOS)
        x, y, _, _ = rects[name]
        paste_padded(atlas, img, x, y)
    os.makedirs(os.path.dirname(OUT_TEX), exist_ok=True)
    atlas.save(OUT_TEX, quality=92, subsampling=0, optimize=True)
    inset = 1.5
    cells = {}
    for name in sorted(rects):
        x, y, w, h = rects[name]
        cells[name] = {"px": [x, y, w, h],
                       "uv": [round((x + inset) / SIZE, 6), round(1 - (y + h - inset) / SIZE, 6),
                              round((x + w - inset) / SIZE, 6), round(1 - (y + inset) / SIZE, 6)]}
    with open(OUT_JSON, "w") as f:
        json.dump({"size": SIZE, "texture": "textures/city/atlas_D.jpg", "note": "uv = [u0, v0, u1, v1], Blender "
                   "convention (v up from the bottom); px = [x, y, w, h] from the top-left; tools/city/atlas.py",
                   "cells": cells}, f, indent=0, ensure_ascii=False)
    print("ATLAS %d cells, %d px of %d used in height -> %s, %s" % (len(cells), used_h, SIZE,
                                                                      os.path.relpath(OUT_TEX, ROOT), os.path.relpath(OUT_JSON, ROOT)))
    if sheet:
        sh = atlas.copy()
        d = ImageDraw.Draw(sh)
        f = font("mpb", 18)
        for name, (x, y, w, h) in rects.items():
            d.rectangle((x, y, x + w, y + h), outline=(255, 0, 255), width=2)
            d.text((x + 4, y + 4), name, font=f, fill=(255, 0, 255), stroke_width=2, stroke_fill=(0, 0, 0))
        p = os.path.join(ROOT, "renders", "city", "interior", "atlas_sheet.jpg")
        os.makedirs(os.path.dirname(p), exist_ok=True)
        sh.resize((2048, 2048), Image.LANCZOS).save(p, quality=85)
        print("ATLAS sheet", os.path.relpath(p, ROOT))


if __name__ == "__main__":
    build(sheet="--sheet" in sys.argv)
