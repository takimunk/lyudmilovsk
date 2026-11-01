"""Nestling kit for Lyudmilovsk: static chick meshes for every species in blender/birds/nests/nest_table.json.

    Blender -b --factory-startup --python blender/birds/chicks/build_chicks.py -- [--only <Id>] [--previews] [--no-lint]

Per species 6 meshes SM_Chick_<Id>_<Hatch|Mid|Late>_<Rest|Beg>: Rest = huddled low with the head tucked, Beg = neck up,
bill gaping (pigeons: head pushed forward, bill a little open; gulls/swan/raptors: bill open, calling). Static, no rig.
Blender coordinates: +X forward (the bill), Z up, origin on the nest floor under the belly. One material M_ChickKit and
one atlas (T_ChickKit_C/N/R.png) painted here with numpy. Output in export/: chick_kit.glb, chick_kit.json, the atlas
PNGs, lint_report.json, previews/.
"""
import json
import math
import os
import struct
import subprocess
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
BIRDS = os.path.dirname(HERE)
sys.path.insert(0, BIRDS)
import bpy  # noqa: E402
import mathutils  # noqa: E402
import numpy as np  # noqa: E402

import bird_common as C  # noqa: E402
import lint_bird  # noqa: E402

ROOT = os.path.abspath(os.path.join(BIRDS, "..", ".."))
OUT = os.path.join(HERE, "export")
PREV = os.path.join(OUT, "previews")
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
PREV_ONLY = "--previews-only" in argv      # worker mode: build the given species and render their previews only
JOBS = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 6
PREVIEWS = "--previews" in argv
DO_LINT = "--no-lint" not in argv

# =============================================================================================================
# per-species art table (what each nestling looks like)
# =============================================================================================================
# (inlined) Per-species art table for the chick kit: what each nestling looks like (field-guide knowledge, written down).
#
# Colours are sRGB hex. Fields:
#   typ    body plan / behaviour class (song, crow, pigeon, gull, swan, heron, cormorant, raptor, swift)
#   L      adult length, cm (fallback for species missing in species.json)
#   skin   naked skin at hatching (or, for downy types, the colour of the first down coat)
#   down   down tufts / down coat colour (None: practically naked)
#   blot   (downy types) colour of the dark blotches in the down
#   pin    colour of the pin-feather sheaths (Mid)
#   juv    (base, accent, pattern) juvenile plumage at Late; pattern: plain streak speckle mottle scale pied
#   gape   mouth lining colour;  flange  the swollen rim at the mouth corners
#   bill   bill base colour, tip  the bill tip colour, cere (raptors) the yellow cere
#   iris   open eye colour;  leg  leg colour
#   bl     adult bill length as a fraction of adult length (overrides the type default)
#   gp     gape width factor (1 = type default)
KIT = {}


def art(id, typ, L, skin, down, pin, juv, gape, flange, bill, tip, iris, leg, **kw):
    KIT[id] = dict(id=id, typ=typ, L=L, skin=skin, down=down, pin=pin, juv=juv, gape=gape, flange=flange, bill=bill,
                   tip=tip, iris=iris, leg=leg, **dict(dict(blot=None, cere=None, bl=None, gp=1.0, crest=False), **kw))


# ---- crows: pink-grey skin, sparse dark down, bright pink-red gape with pale flanges, grey-blue pins --------------
art("Rook", "crow", 45, "#c99f95", "#5d5955", "#6c7684", ("#2b2931", "#4d4756", "plain"), "#e0455f", "#f2c9c4",
    "#cfb9a8", "#3a3434", "#4a3f3a", "#c9a79c")
art("HoodedCrow", "crow", 48, "#c79b92", "#6a6663", "#6f7886", ("#55504e", "#8e8d90", "mottle"), "#e04a5e", "#f0c6c0",
    "#c9b4a6", "#3a3434", "#7c8a98", "#c9a79c")
art("CarrionCrow", "crow", 47, "#c79b92", "#55514e", "#6c7684", ("#2e2c30", "#514a52", "plain"), "#e04a5e", "#f0c6c0",
    "#c9b4a6", "#3a3434", "#7c8a98", "#c9a79c")
art("Jackdaw", "crow", 34, "#cba096", "#5b5753", "#6e7886", ("#2d2c2f", "#5a5a5f", "plain"), "#e24b66", "#f3cdc8",
    "#cdb8a8", "#3a3636", "#9db1c0", "#c9a79c")
art("Magpie", "crow", 45, "#c99c92", "#4e4b4a", "#6a7482", ("#201f25", "#e9e7e0", "pied"), "#e03f5a", "#f2c4be",
    "#c9b6a8", "#403a3a", "#5a4c40", "#c9a79c")
art("Raven", "crow", 64, "#b99d99", "#4a4644", "#666e7a", ("#201c1f", "#403840", "plain"), "#e0536a", "#efc8c4",
    "#c4aea2", "#2e2a2a", "#4a3d36", "#b99d99")
art("Jay", "crow", 34, "#d1a592", "#7a746d", "#7b8492", ("#a78f74", "#4f3b2c", "streak"), "#e8774c", "#f4cfbc",
    "#cdbca2", "#5a5046", "#4e86a0", "#c9a28c", bl=0.095)
art("Nutcracker", "crow", 32, "#cfa391", "#6b645c", "#7b8492", ("#5b4432", "#e8dfcf", "speckle"), "#e6606a", "#f2cdc4",
    "#c8b6a2", "#3c3430", "#3c2e24", "#c9a28c", bl=0.15)
# ---- pigeons: grey skin, yellow hairy down, soft pale bill with a swollen cere, narrow gape ----------------------
art("FeralPigeon", "pigeon", 32, "#bfa49a", "#e4cc6a", "#8a8f9e", ("#7c808d", "#aeb2bc", "scale"), "#d98c88", "#ecccc4",
    "#ead2c4", "#b49c92", "#4a3a30", "#d9a8a0")
art("WoodPigeon", "pigeon", 41, "#c0a99c", "#eed793", "#8c919f", ("#74726f", "#9e9c99", "scale"), "#d98c88", "#ecccc4",
    "#e6c8b6", "#b49c8c", "#4a3a30", "#d9a8a0")
# ---- gulls: precocial, grey-buff down with dark blotches --------------------------------------------------------
art("HerringGull", "gull", 60, "#b7a88f", "#b7a88f", "#6c5c4a", ("#6f5d4b", "#a89279", "mottle"), "#d96a66", "#eab4a2",
    "#cdb5a5", "#3c302c", "#2d2218", "#dcaa9e", blot="#4b3b2d")
art("BlackHeadedGull", "gull", 38, "#c4ae88", "#c4ae88", "#6c5a46", ("#9e7c58", "#5d4833", "mottle"), "#d9645e", "#eab8a4",
    "#cdab9b", "#473830", "#2d2218", "#d9a07c", blot="#3f3025")
art("CommonGull", "gull", 42, "#b3ac99", "#b3ac99", "#6a5e50", ("#7b6f61", "#b1a791", "mottle"), "#d96a66", "#eab4a2",
    "#c9b8a8", "#3c332e", "#2d2218", "#aab59c", blot="#544a3d")
# ---- songbirds: pink skin, grey down on head and back, yellow/orange/red gapes with pale flanges ---------------
art("GreatTit", "song", 14, "#e8937c", "#8d8c8a", "#78869b", ("#9b9662", "#d8d18e", "plain"), "#f2b53a", "#f6edac",
    "#cbbb8c", "#5b5548", "#2e2418", "#d6a48c")
art("BlueTit", "song", 12, "#e8977f", "#9a9a98", "#8796a8", ("#a9b48a", "#e8e3a2", "plain"), "#f2b53a", "#f6edac",
    "#cbbb8e", "#5b5548", "#2e2418", "#d6a48c")
art("Goldfinch", "song", 12.5, "#e4917f", "#8f8d8a", "#7b8397", ("#9d8b71", "#5b4a3b", "streak"), "#e4506a", "#f5d4ce",
    "#dac4b2", "#7a6a5c", "#2e2418", "#d6a48c")
art("Chaffinch", "song", 15, "#e4917f", "#8c8a88", "#7a8295", ("#9a836c", "#5a4636", "streak"), "#e4546a", "#f5d2cc",
    "#d6bfae", "#6a5c50", "#2e2418", "#d6a48c")
art("Bullfinch", "song", 16, "#df8c80", "#8a8886", "#79809a", ("#9c8670", "#705b47", "plain"), "#b02a6e", "#eab9cc",
    "#c8b8a8", "#4d443e", "#2e2418", "#d6a48c")
art("HouseSparrow", "song", 15, "#e69a8b", "#908e8a", "#7a8398", ("#9b8971", "#6e5a46", "streak"), "#f3c04a", "#f6eab0",
    "#d4c29a", "#6a5e50", "#2e2418", "#d6a48c")
art("TreeSparrow", "song", 13, "#e69a8a", "#8e8c88", "#7a8398", ("#a08569", "#5d4630", "streak"), "#f3c04a", "#f6eab0",
    "#d4c29a", "#6a5e50", "#2e2418", "#d6a48c")
art("Starling", "song", 21, "#d89e90", "#8b8782", "#747f90", ("#8d7c6b", "#a99887", "plain"), "#f0b23c", "#f1e6a0",
    "#bfae8c", "#4d4640", "#2e2418", "#d29c8c", bl=0.11)
art("SongThrush", "song", 22, "#e29a82", "#8e8a82", "#7b8696", ("#8f6f4b", "#e8cd9d", "speckle"), "#f0a43a", "#f4e3a0",
    "#cdb88e", "#5e5246", "#2e2418", "#d6a48c", bl=0.085)
art("Fieldfare", "song", 26, "#e29a84", "#8e8a84", "#7b8696", ("#8b7b63", "#dac9a9", "speckle"), "#f2a83c", "#f4e3a2",
    "#cdb88e", "#5e5246", "#2e2418", "#d6a48c", bl=0.085)
art("Blackbird", "song", 25, "#e2988a", "#8a8782", "#757f8e", ("#4f3b2d", "#b57d4c", "speckle"), "#f0a03a", "#f4e0a0",
    "#bfae8e", "#4a4038", "#2e2418", "#c9988a", bl=0.085)
art("Robin", "song", 14, "#e2988a", "#8d8a84", "#7a8496", ("#7f6043", "#d9b37c", "speckle"), "#f2a83a", "#f4e5a0",
    "#c9b69a", "#4f4238", "#2e2418", "#d6a48c")
art("WhiteWagtail", "song", 18, "#e0988a", "#8d8b86", "#79838f", ("#8f8d85", "#d0cbc0", "plain"), "#f2c24a", "#f5ecb0",
    "#a89d8c", "#4a423c", "#2e2418", "#a89a90", bl=0.07)
# ---- aerial insectivores --------------------------------------------------------------------------------------
art("CommonSwift", "swift", 17, "#a89b97", None, "#6c6a70", ("#3f3a38", "#cdc5bd", "scale"), "#dcd3b8", "#ece6d2",
    "#6e655f", "#4a423e", "#2e2520", "#c6aca4", bl=0.03, gp=1.3)
art("BarnSwallow", "song", 19, "#df9a8c", "#9a9a98", "#7c8498", ("#585862", "#c9a77e", "plain"), "#f2c24a", "#f5ecb2",
    "#bfae8c", "#4f4640", "#2e2418", "#d6a8a0", bl=0.035, gp=1.25)
art("SandMartin", "song", 12, "#e09c8e", "#9a9892", "#7c8696", ("#8e8378", "#e6e1d8", "plain"), "#f2c24a", "#f5ecb2",
    "#bfae96", "#4f4640", "#2e2418", "#d6a8a0", bl=0.035, gp=1.25)
# ---- waterbirds -----------------------------------------------------------------------------------------------
art("Cormorant", "cormorant", 85, "#2d2625", "#3a312b", "#4a423e", ("#4c3c30", "#cdc0af", "mottle"), "#e8bd72", "#f0e0b0",
    "#9c9086", "#4a4038", "#3a3028", "#3a3030", blot="#241d1a")
art("MuteSwan", "swan", 150, "#aaa59d", "#aaa59d", "#7e766c", ("#8c8074", "#b3a99a", "mottle"), "#d98a88", "#ecc4bc",
    "#8c7c76", "#3a3736", "#2b2118", "#3d3a3a", blot="#8d8880")
art("GreyHeron", "heron", 94, "#bab7af", "#bab7af", "#7d7a74", ("#7f8289", "#c6c2ba", "streak"), "#e8b070", "#f2d8a8",
    "#dac272", "#7a6a4a", "#d6c25e", "#a9ad86", blot="#8e8b85", crest=True)
# ---- raptors: white fluffy down, dark eyes, hooked dark bill, yellow cere ---------------------------------------
art("Sparrowhawk", "raptor", 33, "#d9c5b8", "#f0ede6", "#7a6652", ("#7b6049", "#e0d4c0", "streak"), "#e49a8a", "#ecd0b8",
    "#2f2d2f", "#1c1a1c", "#3a2a1a", "#d8c860", cere="#d8c86c", blot="#dcd6ca")
art("Peregrine", "raptor", 42, "#d8c8bc", "#f3f0e9", "#78624e", ("#6a5645", "#e0cfae", "streak"), "#e49a8a", "#ecd0b8",
    "#3a393c", "#1c1a1c", "#3a2a1a", "#d2d09a", cere="#d4d2a8", blot="#e0dacd")
art("WhiteTailedEagle", "raptor", 86, "#d6c6ba", "#ede8df", "#78644f", ("#6d5b4a", "#a89279", "mottle"), "#e49a8a", "#ecd0b8",
    "#6f6358", "#2a2828", "#3a2a1a", "#d6c26a", cere="#dcc66c", blot="#d6cec0")
art("GoldenEagle", "raptor", 85, "#d6c6ba", "#f3efe9", "#6e5846", ("#4f3c2d", "#e8e0d0", "mottle"), "#e49a8a", "#ecd0b8",
    "#52504e", "#222022", "#3a2a1a", "#d6c26a", cere="#d6c068", blot="#dbd4c6")
art("CommonBuzzard", "raptor", 53, "#d8c8b8", "#efe7d0", "#78634f", ("#6f5946", "#dac9a9", "streak"), "#e49a8a", "#ecd0b8",
    "#4a4846", "#222022", "#3a2a1a", "#d8c870", cere="#d4c470", blot="#d6ccb4")


AGES = ("Hatch", "Mid", "Late")
POSES = ("Rest", "Beg")
SIZE = 4096
CELL = 256
PAD = 2.0


# =============================================================================================================
# per body-plan tables. Lengths are fractions of u (= chick length = k * adult length); head etc. per age.
# =============================================================================================================
TYPES = {
    #            k (hatch, mid, late)        rh per age              fat   neck len (u)  neck dx   beg dir  gape deg  bill/L  bw(rh)  hu   hl   billg
    "song":      dict(k=(.25, .60, .87), rh=(.200, .168, .140), fat=0.94, nlen=.30, ndx=.04, adir=64, gape=64, bl=.080, bwr=.62, hu=.40, hl=.30, wing=(.10, .27, .42), tail=.17),
    "crow":      dict(k=(.23, .58, .87), rh=(.190, .160, .132), fat=0.96, nlen=.30, ndx=.04, adir=60, gape=56, bl=.150, bwr=.58, hu=.40, hl=.30, wing=(.10, .27, .42), tail=.17),
    "swift":     dict(k=(.24, .55, .85), rh=(.190, .165, .140), fat=0.96, nlen=.26, ndx=.03, adir=60, gape=50, bl=.030, bwr=.70, hu=.40, hl=.30, wing=(.10, .30, .50), tail=.12),
    "pigeon":    dict(k=(.22, .58, .82), rh=(.145, .122, .105), fat=1.05, nlen=.20, ndx=.20, adir=32, gape=14, bl=.080, bwr=.40, hu=.38, hl=.30, wing=(.10, .27, .40), tail=.14),
    "gull":      dict(k=(.20, .50, .88), rh=(.150, .115, .095), fat=.90, nlen=.26, ndx=.07, adir=54, gape=34, bl=.085, bwr=.40, hu=.34, hl=.26, wing=(.08, .22, .42), tail=.12),
    "swan":      dict(k=(.15, .38, .60), rh=(.140, .110, .090), fat=.95, nlen=.26, ndx=.06, adir=52, gape=26, bl=.085, bwr=.46, hu=.36, hl=.26, wing=(.07, .20, .38), tail=.10),
    "heron":     dict(k=(.20, .55, .90), rh=(.110, .085, .068), fat=.85, nlen=.40, ndx=.05, adir=52, gape=30, bl=.200, bwr=.28, hu=.26, hl=.20, wing=(.08, .24, .42), tail=.10),
    "cormorant": dict(k=(.18, .50, .85), rh=(.135, .104, .085), fat=.95, nlen=.30, ndx=.05, adir=54, gape=34, bl=.110, bwr=.34, hu=.30, hl=.22, wing=(.08, .22, .40), tail=.12),
    "raptor":    dict(k=(.20, .55, .90), rh=(.150, .115, .095), fat=1.00, nlen=.24, ndx=.05, adir=48, gape=50, bl=.045, bwr=.50, hu=.55, hl=.34, wing=(.08, .22, .42), tail=.12),
}
# down tufts / quills per age (hatch, mid, late); kind: cone wisps or octahedron puffs
DOWN = {
    "song": ("cone", (14, 10, 7), (0, 24, 0)), "crow": ("cone", (12, 9, 6), (0, 24, 0)), "swift": ("cone", (4, 3, 0), (0, 18, 0)),
    "pigeon": ("cone", (66, 44, 16), (0, 12, 0)), "gull": ("puff", (56, 46, 12), (0, 14, 0)), "swan": ("puff", (54, 40, 10), (0, 12, 0)),
    "heron": ("cone", (40, 36, 12), (0, 12, 0)), "cormorant": ("puff", (22, 46, 12), (0, 12, 0)), "raptor": ("puff", (84, 72, 24), (0, 10, 0)),
}
STAND = {"gull": 0.16, "swan": 0.12}          # Beg: standing on the feet (body lifted, legs visible), fraction of u
HOOK = {"raptor": 0.30, "cormorant": 0.20}    # upper bill hook: tip drop in bill lengths * 0.3
KEEP_SITTING_FEET = True


def rgb(h):
    h = h.lstrip("#")
    return np.array([int(h[2 * i:2 * i + 2], 16) / 255.0 for i in range(3)])


def seed_of(*a):
    return zlib.crc32("_".join(str(x) for x in a).encode())


# =============================================================================================================
# atlas: 4096^2, per species 5 tiles of 256^2 (skin, down, pin, juv, misc = four 128^2: gape bill eye leg)
# =============================================================================================================
class Atlas:
    size = SIZE

    def __init__(self, ids):
        self.cells = {}
        self.order = list(ids)
        for si, sid in enumerate(self.order):
            row, c0 = si // 3, (si % 3) * 5
            x0, y0 = c0 * CELL, row * CELL
            for k, name in enumerate(("skin", "down", "pin", "juv")):
                self.cells[f"{sid}:{name}"] = (x0 + k * CELL, y0, CELL, CELL)
            mx = x0 + 4 * CELL
            self.cells[f"{sid}:gape"] = (mx, y0, 128, 128)
            self.cells[f"{sid}:bill"] = (mx + 128, y0, 128, 128)
            self.cells[f"{sid}:eye"] = (mx, y0 + 128, 128, 128)
            self.cells[f"{sid}:leg"] = (mx + 128, y0 + 128, 128, 128)

    def uv(self, key, u, v):
        x0, y0, w, h = self.cells[key]
        return ((x0 + PAD + min(max(u, 0.0), 1.0) * (w - 2 * PAD)) / SIZE, (y0 + PAD + min(max(v, 0.0), 1.0) * (h - 2 * PAD)) / SIZE)


class _Param:
    def __contains__(self, key):
        return key.rsplit(":", 1)[-1] in ("gape", "bill", "eye", "leg")


lint_bird.PARAMETRIC = _Param()


def nz(seed, scale, n=CELL, aniso=(1.0, 1.0)):
    return C.tile_noise(n, seed, scale, aniso)


def smooth(a, lo, hi):
    t = np.clip((a - lo) / (hi - lo), 0, 1)
    return t * t * (3 - 2 * t)


def mixc(a, b, t):
    return a * (1 - t[..., None]) + b * t[..., None]


def paint_species(S, atl, Cc, Nn, Rr):
    """Fill the five tiles of one species into the atlas arrays (uint8). Row 0 = bottom (uv v = 0)."""
    sid = S["id"]
    sd = seed_of("tex", sid)
    typ = S["typ"]
    dense = typ in ("gull", "swan", "heron", "raptor", "pigeon")
    skin, down, pin = rgb(S["skin"]), (rgb(S["down"]) if S["down"] else None), rgb(S["pin"])
    base, acc, pat = rgb(S["juv"][0]), rgb(S["juv"][1]), S["juv"][2]
    n = CELL
    yy, xx = np.mgrid[0:n, 0:n]
    fine = nz(sd + 1, 1.2)
    mid = nz(sd + 2, 5.0)
    big = nz(sd + 3, 22.0)

    def put(key, col, height, rough, strength=1.0):
        x0, y0, w, h = atl.cells[key]
        Cc[y0:y0 + h, x0:x0 + w] = (np.clip(col, 0, 1) * 255 + 0.5).astype(np.uint8)
        nm = C.normal_from_height(height, strength)
        Nn[y0:y0 + h, x0:x0 + w] = (nm * 255 + 0.5).astype(np.uint8)
        Rr[y0:y0 + h, x0:x0 + w] = (np.clip(rough, 0, 1) * 255 + 0.5).astype(np.uint8)

    def fit(a, w):   # crop a CELL-sized array to a sub-cell (128)
        return a[:w, :w] if a.shape[0] != w else a

    # ---- down coat colour field (tufts + the body of downy types)
    if down is not None:
        dcol = down[None, None, :] * (1 + 0.07 * fine[..., None] + 0.05 * mid[..., None])
        if S["blot"] is not None:
            m = smooth(big * 0.8 + mid * 0.35, 0.55, 0.95)
            dcol = mixc(dcol, rgb(S["blot"])[None, None, :] * (1 + 0.06 * fine[..., None]), m)
        # fluffy tufts: gradient dark at the root, light and fine at the tip (v up the tuft)
        grad = (yy / (n - 1.0))[..., None]
        dtuft = dcol * (0.85 + 0.3 * grad) + 0.06 * grad
        streak = nz(sd + 4, 1.0, aniso=(1.0, 0.12))
        dtuft = dtuft * (1 + 0.08 * streak[..., None])
        put(f"{sid}:down", dtuft, 0.9 * nz(sd + 5, 0.9, aniso=(1.0, 0.2)), 0.92 + 0.0 * fine, 0.8)
    else:
        put(f"{sid}:down", skin[None, None, :] * (0.9 + 0.05 * fine[..., None]), 0.3 * fine, 0.6 + 0 * fine, 0.4)

    # ---- skin tile (Hatch): naked pink skin with veins, or the dense first down coat
    veins = np.abs(nz(sd + 6, 3.0)) < 0.18
    if dense and down is not None:
        col = dcol * (1 + 0.04 * fine[..., None])
        # a baby coat: skin shows through only a little at the roots
        col = mixc(col, skin[None, None, :] * 0.95, 0.12 * smooth(mid, 0.8, 1.8))
        put(f"{sid}:skin", col, 1.2 * nz(sd + 7, 1.0), 0.9 + 0 * fine, 0.9)
    else:
        col = skin[None, None, :] * (1 + 0.05 * fine[..., None] + 0.04 * mid[..., None])
        col = mixc(col, skin[None, None, :] * np.array([0.82, 0.62, 0.66])[None, None, :], 0.22 * veins * smooth(mid, -0.5, 0.8))
        if typ == "cormorant":
            col = col * 0.55 + 0.03
        # sparse short fuzz dots where the down will grow
        if down is not None:
            fz = smooth(nz(sd + 8, 1.6), 1.3, 1.9)
            col = mixc(col, down[None, None, :] * 1.05, 0.30 * fz)
        put(f"{sid}:skin", col, 0.7 * mid + 0.4 * fine, 0.58 + 0.06 * fine + 0.0 * mid, 0.5)

    # ---- pin feathers (Mid): body with blue-grey / brown quills in tracts, between them skin (or down)
    sheath = smooth(nz(sd + 9, 2.2, aniso=(1.0, 0.3)), 0.9, 1.5)
    sheath2 = smooth(nz(sd + 10, 1.4, aniso=(1.0, 0.25)), 1.1, 1.7)
    if dense and down is not None:
        under = dcol * (1 + 0.04 * fine[..., None])
        qcol = pin[None, None, :] * (0.85 + 0.25 * (yy / (n - 1.0))[..., None])
        col = mixc(under, qcol, 0.8 * np.maximum(sheath, sheath2))
        h = 2.0 * np.maximum(sheath, sheath2) + 0.6 * fine
        put(f"{sid}:pin", col, h, 0.85 - 0.25 * sheath, 1.2)
    else:
        under = skin[None, None, :] * (0.97 + 0.04 * fine[..., None])
        if typ == "cormorant":
            under = under * 0.5
        qcol = pin[None, None, :] * (0.8 + 0.4 * (yy / (n - 1.0))[..., None])   # darker root, lighter tip
        m = np.maximum(sheath, sheath2)
        col = mixc(under, qcol, 0.75 * m)
        if down is not None:
            col = mixc(col, down[None, None, :], 0.45 * smooth(nz(sd + 11, 1.5), 1.0, 1.7) * (1 - m))
        put(f"{sid}:pin", col, 2.4 * m + 0.4 * fine, 0.72 - 0.22 * m, 1.3)

    # ---- juvenile plumage (Late)
    barb = nz(sd + 12, 1.0, aniso=(1.0, 0.12))   # fine barbs along v
    jc = base[None, None, :] * (1 + 0.07 * barb[..., None] + 0.05 * mid[..., None])
    if pat == "streak":
        m = smooth(nz(sd + 13, 2.4, aniso=(1.0, 0.18)), 0.2, 0.9)
        jc = mixc(jc, acc[None, None, :], 0.7 * m)
    elif pat == "speckle":
        m = smooth(nz(sd + 13, 2.0, aniso=(0.7, 0.7)), 1.05, 1.5)
        jc = mixc(jc, acc[None, None, :], 0.95 * m)
        jc = jc * (1 - 0.15 * smooth(nz(sd + 14, 3.0), 0.6, 1.4))[..., None]
    elif pat == "mottle":
        m = smooth(nz(sd + 13, 6.0) * 0.8 + nz(sd + 14, 2.0) * 0.4, 0.3, 1.1)
        jc = mixc(jc, acc[None, None, :], 0.65 * m)
    elif pat == "scale":
        cs = 28.0
        row = np.floor(yy / cs)
        xo = (xx + (row % 2) * cs / 2) % cs
        yo = yy % cs
        d = np.sqrt((xo - cs / 2) ** 2 + (yo - cs * 0.15) ** 2) / (cs * 0.62)
        m = smooth(d, 0.82, 1.0)
        jc = mixc(jc, acc[None, None, :], 0.75 * m)
    elif pat == "pied":
        m = smooth(nz(sd + 13, 16.0) + 0.3 * nz(sd + 14, 4.0), 0.25, 0.55)
        jc = mixc(jc, acc[None, None, :], 0.9 * m)
    else:   # plain: faint lighter feather edges
        m = smooth(nz(sd + 13, 3.0, aniso=(1.0, 0.4)), 0.4, 1.4)
        jc = mixc(jc, acc[None, None, :], 0.28 * m)
    put(f"{sid}:juv", jc, 1.0 * barb + 0.7 * mid, 0.80 + 0.05 * barb, 0.9)

    # ---- misc tile: gape (lining | flange), bill, eye (closed | slit | open), leg
    g = rgb(S["gape"])
    fl = rgb(S["flange"])
    w = 128
    y2, x2 = np.mgrid[0:w, 0:w]
    t = np.clip(x2 / 61.0, 0, 1)
    vein = nz(sd + 20, 1.5, w)
    lining = mixc(np.broadcast_to(g * 0.32, (w, w, 3)), np.broadcast_to(g, (w, w, 3)), smooth(t, 0.0, 0.85))
    lining = lining * (1 + 0.08 * vein[..., None])
    hl = np.sin(np.pi * y2 / (w - 1.0))
    flange = fl[None, None, :] * (0.82 + 0.22 * hl[..., None]) * (1 + 0.04 * vein[..., None])
    gp = np.where((x2 < 62)[..., None], lining, np.where((x2 < 66)[..., None], g[None, None, :] * 0.9, flange))
    put(f"{sid}:gape", gp, 0.5 * vein * (x2 > 64), 0.22 + 0 * vein, 0.4)

    # bill: u = base -> tip, v: lower half 0..0.5 / upper half 0.5..1
    b, tp = rgb(S["bill"]), rgb(S["tip"])
    ut = x2 / (w - 1.0)
    bc = mixc(np.broadcast_to(b, (w, w, 3)), np.broadcast_to(tp, (w, w, 3)), smooth(ut, 0.55, 1.0))
    bc = bc * (1 + 0.05 * nz(sd + 21, 2.0, w)[..., None])
    up = (y2 >= 64)
    if S["cere"] is not None:
        cm = (ut < 0.30) & up
        bc = np.where(cm[..., None], rgb(S["cere"])[None, None, :] * (1 + 0.05 * vein[..., None]), bc)
    if typ == "gull":   # dark tip, pale base, red spot is not there yet
        bc = mixc(bc, tp[None, None, :] * 1.0, smooth(ut, 0.7, 0.9) * 0.9)
    bc = np.where(((ut < 0.06) & ~up)[..., None], bc * 0.95, bc)
    if typ in ("pigeon",):   # soft, swollen cere-like base
        bc = np.where((ut < 0.35)[..., None] & up[..., None], (b * 1.05 + 0.04)[None, None, :], bc)
    put(f"{sid}:bill", bc, 0.4 * nz(sd + 22, 1.5, w), 0.42 + 0 * vein, 0.4)

    # eye: three vertical strips; each strip is a sphere's lon (u) x lat (v) with lon 0 (looking out) in the middle
    ec = np.zeros((w, w, 3))
    eh = np.zeros((w, w))
    er = np.full((w, w), 0.55)
    iris = rgb(S["iris"])
    lid = np.array([0.30, 0.19, 0.23]) if typ != "cormorant" else skin * 1.2
    if S["down"] is not None and typ in ("raptor", "gull", "swan", "heron"):
        lid = np.array([0.30, 0.26, 0.25])
    sw = w / 3.0
    for s_i in range(3):
        xs = (x2 - s_i * sw) / sw          # 0..1 across the strip: lon -pi..pi, centre = looking out
        ok = (xs >= 0) & (xs < 1)
        lon = (xs - 0.5) * 2 * np.pi
        lat = (y2 / (w - 1.0) - 0.5) * np.pi
        # position on the sphere seen from outside: (dx, dy) in eye radii
        dx = np.sin(lon) * np.cos(lat)
        dy = np.sin(lat)
        r = np.sqrt(dx * dx + dy * dy)
        front = np.cos(lon) * np.cos(lat) > 0
        col = np.broadcast_to(lid[None, None, :], (w, w, 3)).copy() * (1 + 0.05 * vein[..., None])
        if s_i == 0:   # closed: bulge of dark skin with a horizontal crease
            crease = np.exp(-(dy / 0.07) ** 2) * front
            col = col * (1 - 0.45 * crease[..., None])
            rough = 0.5
        elif s_i == 1:   # slit opening
            slit = (np.abs(dy) < 0.09 * (1 - (dx / 0.62) ** 2).clip(0, 1)) & (np.abs(dx) < 0.62) & front
            col = np.where(slit[..., None], np.array([0.05, 0.03, 0.03])[None, None, :], col)
            rough = 0.35
        else:          # open: lid ring, iris, pupil
            ring = r < 0.82
            col = np.where((ring & front)[..., None], iris[None, None, :] * (1 + 0.08 * vein[..., None]), col)
            pup = r < 0.36
            col = np.where((pup & front)[..., None], np.array([0.02, 0.02, 0.02])[None, None, :], col)
            edge = (r > 0.7) & (r < 0.86) & front
            col = np.where(edge[..., None], col * 0.6, col)
            rough = 0.12
        ec = np.where(ok[..., None], col, ec)
        er = np.where(ok, rough, er)
    ec = np.where((ec.sum(-1) == 0)[..., None], lid[None, None, :], ec)
    put(f"{sid}:eye", ec, eh, er, 0.3)

    lg = rgb(S["leg"])
    lc = lg[None, None, :] * (1 + 0.06 * nz(sd + 23, 2.0, w)[..., None] + 0.0 * vein[..., None])
    scales = smooth(np.abs(np.sin(y2 * 0.6)) + 0.0 * x2, 0.8, 1.0)
    lc = lc * (1 - 0.12 * scales[..., None])
    put(f"{sid}:leg", lc, 0.8 * scales, 0.55 + 0 * vein, 0.6)


def write_png(path, arr):
    a = np.ascontiguousarray(arr[::-1])
    h, w = a.shape[:2]
    ct = 0 if a.ndim == 2 else 2
    raw = b"".join(b"\x00" + a[y].tobytes() for y in range(h))

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, ct, 0, 0, 0)) \
        + chunk(b"IDAT", zlib.compress(raw, 4)) + chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(png)


def build_atlas(ids):
    atl = Atlas(ids)
    Cc = np.full((SIZE, SIZE, 3), 128, np.uint8)
    Nn = np.zeros((SIZE, SIZE, 3), np.uint8)
    Nn[..., 0] = 128
    Nn[..., 1] = 128
    Nn[..., 2] = 255
    Rr = np.full((SIZE, SIZE), 200, np.uint8)
    for sid in ids:
        paint_species(KIT[sid], atl, Cc, Nn, Rr)
    os.makedirs(OUT, exist_ok=True)
    write_png(os.path.join(OUT, "T_ChickKit_C.png"), Cc)
    write_png(os.path.join(OUT, "T_ChickKit_N.png"), Nn)
    write_png(os.path.join(OUT, "T_ChickKit_R.png"), Rr)
    return atl


# =============================================================================================================
# geometry kit
# =============================================================================================================
def unit(v):
    v = np.asarray(v, float)
    return v / (np.linalg.norm(v) + 1e-20)


class MB:
    """Triangle soup builder: shared vertices, per-face-corner uvs, a lint part and an atlas cell per face."""

    def __init__(self, sid, atl):
        self.sid, self.atl = sid, atl
        self.V, self.F, self.UV, self.PART, self.TILE, self.pn = [], [], [], [], [], []

    def part(self, name):
        self.pn.append(name)
        return len(self.pn) - 1

    def verts(self, pts):
        b = len(self.V)
        self.V.extend(tuple(map(float, p)) for p in pts)
        return b

    def face(self, a, b, c, part, tile, uv=None):
        self.F.append((a, b, c))
        self.UV.append(uv)
        self.PART.append(part)
        self.TILE.append(tile)

    def flip_if(self, f0, test):
        """for faces f0..: test(centroid, normal) -> True when the face points the wrong way: swap b,c (and their uvs)."""
        V = np.array(self.V)
        for i in range(f0, len(self.F)):
            a, b, c = self.F[i]
            n = np.cross(V[b] - V[a], V[c] - V[a])
            if test(V[[a, b, c]].mean(0), n):
                self.F[i] = (a, c, b)
                if self.UV[i] is not None:
                    u = self.UV[i]
                    self.UV[i] = (u[0], u[2], u[1])

    def tris(self):
        return len(self.F)


def spline(P, vals, per):
    X = np.hstack([np.asarray(P, float), np.asarray(vals, float)])
    m = len(X)
    pad = np.vstack([2 * X[0] - X[1], X, 2 * X[-1] - X[-2]])
    out = []
    for tt in np.linspace(0, m - 1, (m - 1) * per + 1):
        i = min(int(tt), m - 2)
        f = tt - i
        p0, p1, p2, p3 = pad[i], pad[i + 1], pad[i + 2], pad[i + 3]
        out.append(0.5 * ((2 * p1) + (-p0 + p2) * f + (2 * p0 - 5 * p1 + 4 * p2 - p3) * f * f + (-p0 + 3 * p1 - 3 * p2 + p3) * f ** 3))
    out = np.array(out)
    return out[:, :3], out[:, 3:]


def loft(mb, part, tile, Cc, W, H, ref, seg=10, cap0=True, cap1=True, capk0=0.55, capk1=0.55, uvr=None):
    """Elliptic tube along the polyline Cc with half-widths W (along the transported side axis) and H. uvr=None: the faces
    are box-projected into `tile` later; uvr=(u0,u1,v0,v1): parametric uv (round x along) into that part of the cell."""
    Cc, W, H = np.asarray(Cc, float), np.asarray(W, float), np.asarray(H, float)
    n = len(Cc)
    T = np.gradient(Cc, axis=0)
    T /= np.linalg.norm(T, axis=1, keepdims=True) + 1e-20
    S = np.zeros((n, 3))
    N = np.zeros((n, 3))
    s = np.asarray(ref, float)
    for i in range(n):
        s = s - T[i] * np.dot(s, T[i])
        if np.linalg.norm(s) < 1e-6:
            s = np.array([0.0, 1.0, 0.0]) - T[i] * T[i][1]
        s = unit(s)
        S[i] = s
        N[i] = np.cross(T[i], s)
    th = np.linspace(0, 2 * np.pi, seg, endpoint=False)
    ring = Cc[:, None, :] + W[:, None, None] * np.cos(th)[None, :, None] * S[:, None, :] + H[:, None, None] * np.sin(th)[None, :, None] * N[:, None, :]
    b = mb.verts(ring.reshape(-1, 3))

    def vid(i, j):
        return b + i * seg + (j % seg)

    def puv(i, j):
        u0, u1, v0, v1 = uvr
        return (u0 + (u1 - u0) * j / seg, v0 + (v1 - v0) * i / max(n - 1, 1))
    for i in range(n - 1):
        for j in range(seg):
            a, bb, c, d = vid(i, j), vid(i, j + 1), vid(i + 1, j + 1), vid(i + 1, j)
            if uvr is None:
                mb.face(a, bb, c, part, tile)
                mb.face(a, c, d, part, tile)
            else:
                pa, pb, pc, pd = puv(i, j), puv(i, j + 1), puv(i + 1, j + 1), puv(i + 1, j)
                pa, pb, pc, pd = [mb.atl.uv(tile, *p) for p in (pa, pb, pc, pd)]
                mb.face(a, bb, c, part, tile, (pa, pb, pc))
                mb.face(a, c, d, part, tile, (pa, pc, pd))
    for k, (end, cap, ck) in enumerate(((0, cap0, capk0), (n - 1, cap1, capk1))):
        if not cap:
            continue
        sgn = -1.0 if end == 0 else 1.0
        pole = mb.verts([Cc[end] + sgn * T[end] * ck * min(W[end], H[end])])
        for j in range(seg):
            if end == 0:
                tri = (pole, vid(0, j + 1), vid(0, j))
            else:
                tri = (pole, vid(n - 1, j), vid(n - 1, j + 1))
            if uvr is None:
                mb.face(*tri, part, tile)
            else:
                u0, u1, v0, v1 = uvr
                vv = v0 if end == 0 else v1
                jj = (j + 0.5) / seg
                pu = [(u0 + (u1 - u0) * (j + (1 if (end == 0) else 0)) / seg, vv), (u0 + (u1 - u0) * jj, vv + (v1 - v0) * (-0.05 if end == 0 else 0.05)),
                      (u0 + (u1 - u0) * (j + (0 if end == 0 else 1)) / seg, vv)]
                # order of tri: (pole, p1, p2)
                if end == 0:
                    uvs = [(u0 + (u1 - u0) * jj, vv - 0.05 * (v1 - v0)), (u0 + (u1 - u0) * (j + 1) / seg, vv), (u0 + (u1 - u0) * j / seg, vv)]
                else:
                    uvs = [(u0 + (u1 - u0) * jj, vv + 0.05 * (v1 - v0)), (u0 + (u1 - u0) * j / seg, vv), (u0 + (u1 - u0) * (j + 1) / seg, vv)]
                mb.face(*tri, part, tile, tuple(mb.atl.uv(tile, *p) for p in uvs))
    return dict(C=Cc, T=T, S=S, N=N, W=W, H=H, n=n)


def surf(info, fi, th, out=0.0):
    """point and outward normal on a loft at fractional ring index fi, angle th."""
    i = int(min(max(math.floor(fi), 0), info["n"] - 2))
    f = fi - i
    g = lambda k: info[k][i] * (1 - f) + info[k][i + 1] * f
    c, s, nn, w, h = g("C"), unit(g("S")), unit(g("N")), float(g("W")), float(g("H"))
    p = c + w * math.cos(th) * s + h * math.sin(th) * nn
    nrm = unit(math.cos(th) / max(w, 1e-9) * s + math.sin(th) / max(h, 1e-9) * nn)
    return p + nrm * out, nrm, unit(g("T")), s, nn


def ellipsoid(mb, part, tile, c, axes, radii, nlon=10, nlat=6, uvfun=None):
    """Closed ellipsoid; axes = 3 orthonormal vectors (a: pole axis last). Faces point out. uvfun(lon, lat)->uv or None (projected)."""
    a, b, p = [np.asarray(x, float) for x in axes]
    ra, rb, rp = radii
    rows = []
    lats = np.linspace(-np.pi / 2, np.pi / 2, nlat + 1)
    vid = {}
    f0 = len(mb.F)

    def pt(lon, lat):
        return c + ra * math.cos(lat) * math.cos(lon) * a + rb * math.cos(lat) * math.sin(lon) * b + rp * math.sin(lat) * p
    S0 = mb.verts([pt(0, -np.pi / 2)])
    S1 = mb.verts([pt(0, np.pi / 2)])
    ring_ids = []
    for k in range(1, nlat):
        ids = mb.verts([pt(2 * np.pi * j / nlon - np.pi, lats[k]) for j in range(nlon)])
        ring_ids.append(ids)
    lons = [2 * np.pi * j / nlon - np.pi for j in range(nlon + 1)]

    def U(lo, la):
        return mb.atl.uv(tile, *uvfun(lo, la)) if uvfun else None

    def mk(i0, i1, i2, p0, p1, p2):
        mb.face(i0, i1, i2, part, tile, (U(*p0), U(*p1), U(*p2)) if uvfun else None)
    for j in range(nlon):
        # south cap
        mk(S0, ring_ids[0] + (j + 1) % nlon, ring_ids[0] + j, ((lons[j] + lons[j + 1]) / 2, lats[0]), (lons[j + 1], lats[1]), (lons[j], lats[1]))
        # north cap
        mk(S1, ring_ids[-1] + j, ring_ids[-1] + (j + 1) % nlon, ((lons[j] + lons[j + 1]) / 2, lats[-1]), (lons[j], lats[-2]), (lons[j + 1], lats[-2]))
    for k in range(len(ring_ids) - 1):
        for j in range(nlon):
            a0, a1 = ring_ids[k] + j, ring_ids[k] + (j + 1) % nlon
            b0, b1 = ring_ids[k + 1] + j, ring_ids[k + 1] + (j + 1) % nlon
            la0, la1 = lats[k + 1], lats[k + 2]
            mk(a0, a1, b1, (lons[j], la0), (lons[j + 1], la0), (lons[j + 1], la1))
            mk(a0, b1, b0, (lons[j], la0), (lons[j + 1], la1), (lons[j], la1))
    mb.flip_if(f0, lambda cen, n: np.dot(n, cen - c) < 0)


def cone(mb, part, tile, base, d, ln, r, rect, sides=3, curl=None, sink=None):
    """Open cone (tuft / quill / down wisp), base buried `sink` deep. rect = (u0, v0, du, dv) in the tile (0..1)."""
    d = unit(d)
    a = unit(np.cross(d, [0.3, 0.9, 0.2]))
    b = np.cross(d, a)
    sink = r * 1.6 if sink is None else sink
    bc = np.asarray(base, float) - d * sink
    ring = [bc + r * (math.cos(2 * math.pi * i / sides) * a + math.sin(2 * math.pi * i / sides) * b) for i in range(sides)]
    tip = np.asarray(base, float) + d * ln + (np.asarray(curl, float) if curl is not None else 0)
    i0 = mb.verts(ring + [tip])
    u0, v0, du, dv = rect
    f0 = len(mb.F)
    for i in range(sides):
        uv = (mb.atl.uv(tile, u0 + du * i / sides, v0), mb.atl.uv(tile, u0 + du * (i + 1) / sides, v0), mb.atl.uv(tile, u0 + du * (i + 0.5) / sides, v0 + dv))
        mb.face(i0 + i, i0 + (i + 1) % sides, i0 + sides, part, tile, uv)
    mb.flip_if(f0, lambda cen, n: np.dot(n, cen - (bc + d * np.dot(cen - bc, d))) < 0)
    return tip


def puff(mb, part, tile, c, d, ln, r, rect):
    """Octahedron blob of down: long axis d (half-length ln), radius r, centre c."""
    d = unit(d)
    a = unit(np.cross(d, [0.3, 0.9, 0.2]))
    b = np.cross(d, a)
    pts = [c + d * ln, c - d * ln * 0.55, c + a * r, c - a * r, c + b * r, c - b * r]
    i0 = mb.verts(pts)
    u0, v0, du, dv = rect
    f0 = len(mb.F)
    ring = [2, 4, 3, 5]
    for k in range(4):
        for pole in (0, 1):
            uv = (mb.atl.uv(tile, u0, v0), mb.atl.uv(tile, u0 + du, v0), mb.atl.uv(tile, u0 + du * 0.5, v0 + dv))
            mb.face(i0 + pole, i0 + ring[k], i0 + ring[(k + 1) % 4], part, tile, uv)
    mb.flip_if(f0, lambda cen, n: np.dot(n, cen - c) < 0)


def card(mb, part, tile, base, d, ln, w0, w1, up, rect, ridge=0.05):
    """A flat feather card (folded a hair along its axis): 4 triangles. d = direction, up = the face normal hint."""
    d = unit(d)
    sd = unit(np.cross(d, up))
    nn = np.cross(sd, d)
    p = lambda f, o, h=0.0: np.asarray(base, float) + d * ln * f + sd * o + nn * h
    w_mid = (w0 + w1) * 0.5
    pts = [p(0, -w0), p(0, w0), p(1, w1), p(1, -w1), p(0, 0, ridge * w0), p(1, 0, ridge * w1)]
    i0 = mb.verts(pts)
    u0, v0, du, dv = rect
    r = min(w1 / max(w0, 1e-9), 1.0)
    uvs = {0: (0, 0), 1: (1, 0), 2: (0.5 + 0.5 * r, 1), 3: (0.5 - 0.5 * r, 1), 4: (0.5, 0), 5: (0.5, 1)}
    uv = lambda k: mb.atl.uv(tile, u0 + du * uvs[k][0], v0 + dv * uvs[k][1])
    f0 = len(mb.F)
    for tri in ((0, 4, 5), (0, 5, 3), (4, 1, 2), (4, 2, 5)):
        mb.face(*(i0 + k for k in tri), part, tile, tuple(uv(k) for k in tri))
    mb.flip_if(f0, lambda cen, n: np.dot(n, nn) < 0)


# =============================================================================================================
# bill (two shells, hinge at the mouth corners), flange rolls, throat
# =============================================================================================================
def build_bill(mb, S, O, f, s, u, Lu, Ll, wb, hu, hl, theta, hook, open_, fl_r):
    """Bill with origin O (centre of the base ring, mouth plane = f/s plane), forward f, side s, up u. Returns tips."""
    sid = S["id"]
    nt, nphi = 6, 8
    Rm = np.array([f, s, u]).T          # local -> world
    to_w = lambda p: O + Rm @ np.asarray(p, float)
    cth, sth = math.cos(theta), math.sin(theta)

    def rot(p):                          # pitch the lower half down by theta about the hinge (local y axis through the origin)
        x, y, z = p
        return np.array([x * cth + z * sth, y, -x * sth + z * cth])

    def half(sign, L, h0, hk, name):
        pid = mb.part(name)
        ts = np.arange(nt + 1) / nt * 0.92
        w = wb * (1 - ts) ** 0.7
        h = h0 * (1 - ts ** 1.5) ** 0.9
        hz = -hk * L * ts ** 3
        phis = np.linspace(0, np.pi, nphi + 1)
        kap = 0.86
        tip = np.array([L, 0.0, hz[-1] * 1.0 + (-hk * L * (1 - 0.92 ** 3) if hk else 0.0)])
        tip[2] = -hk * L if hk else 0.0
        out = np.zeros((nt + 1, nphi + 1, 3))
        inn = np.zeros((nt + 1, nphi + 1, 3))
        for k in range(nt + 1):
            out[k, :, 0] = ts[k] * L
            out[k, :, 1] = w[k] * np.cos(phis)
            out[k, :, 2] = sign * h[k] * np.sin(phis) + hz[k]
            inn[k, :, 0] = ts[k] * L
            inn[k, :, 1] = kap * w[k] * np.cos(phis)
            inn[k, :, 2] = sign * kap * h[k] * np.sin(phis) + hz[k]
        tipi = tip + np.array([-0.07 * L, 0.0, 0.0])
        tile_b, tile_g = f"{sid}:bill", f"{sid}:gape"
        v_lo = 0.5 if sign > 0 else 0.0
        T = (lambda p: to_w(rot(p) if sign < 0 and open_ else p))
        ob = mb.verts([T(p) for p in out.reshape(-1, 3)])
        tp = mb.verts([T(tip)])
        f0 = len(mb.F)
        uvo = lambda k, p: mb.atl.uv(tile_b, ts[k] / 0.92 * 0.97, v_lo + 0.5 * p / nphi)
        for k in range(nt):
            for p in range(nphi):
                a0, a1, b0, b1 = ob + k * (nphi + 1) + p, ob + k * (nphi + 1) + p + 1, ob + (k + 1) * (nphi + 1) + p, ob + (k + 1) * (nphi + 1) + p + 1
                mb.face(a0, a1, b1, pid, tile_b, (uvo(k, p), uvo(k, p + 1), uvo(k + 1, p + 1)))
                mb.face(a0, b1, b0, pid, tile_b, (uvo(k, p), uvo(k + 1, p + 1), uvo(k + 1, p)))
        for p in range(nphi):
            mb.face(ob + nt * (nphi + 1) + p, ob + nt * (nphi + 1) + p + 1, tp, pid, tile_b,
                    (uvo(nt, p), uvo(nt, p + 1), mb.atl.uv(tile_b, 1.0, v_lo + 0.25)))
        # faces must point away from the half's own axis (hinge -> tip)
        def wrong(cen, n):
            # nearest point of the (straight) half axis from the hinge towards the tip
            tipw = to_w(rot(tip) if (sign < 0 and open_) else tip)
            ax = unit(tipw - O)
            q = O + ax * np.dot(cen - O, ax)
            return np.dot(n, cen - q) < 0
        mb.flip_if(f0, wrong)
        if not open_:
            return pid, ob, None, tip
        ib = mb.verts([T(p) for p in inn.reshape(-1, 3)])
        itp = mb.verts([T(tipi)])
        f1 = len(mb.F)
        uvi = lambda k, p: mb.atl.uv(tile_g, 0.03 + 0.43 * ts[k] / 0.92, 0.08 + 0.84 * p / nphi)
        for k in range(nt):
            for p in range(nphi):
                a0, a1, b0, b1 = ib + k * (nphi + 1) + p, ib + k * (nphi + 1) + p + 1, ib + (k + 1) * (nphi + 1) + p, ib + (k + 1) * (nphi + 1) + p + 1
                mb.face(a0, a1, b1, pid, tile_g, (uvi(k, p), uvi(k, p + 1), uvi(k + 1, p + 1)))
                mb.face(a0, b1, b0, pid, tile_g, (uvi(k, p), uvi(k + 1, p + 1), uvi(k + 1, p)))
        for p in range(nphi):
            mb.face(ib + nt * (nphi + 1) + p, ib + nt * (nphi + 1) + p + 1, itp, pid, tile_g,
                    (uvi(nt, p), uvi(nt, p + 1), mb.atl.uv(tile_g, 0.47, 0.08 + 0.84 * (p + 0.5) / nphi)))
        # inner faces point into the cavity (toward the half's axis)
        mb.flip_if(f1, lambda cen, n: not wrong(cen, n))
        # lip strip between outer and inner rim (flange coloured), faces toward the other half
        f2 = len(mb.F)
        for side_p in (0, nphi):
            for k in range(nt):
                o0, o1 = ob + k * (nphi + 1) + side_p, ob + (k + 1) * (nphi + 1) + side_p
                i0, i1 = ib + k * (nphi + 1) + side_p, ib + (k + 1) * (nphi + 1) + side_p
                u_a, u_b = 0.58 + 0.38 * k / nt, 0.58 + 0.38 * (k + 1) / nt
                mb.face(o0, o1, i1, pid, tile_g, (mb.atl.uv(tile_g, u_a, 0.62), mb.atl.uv(tile_g, u_b, 0.62), mb.atl.uv(tile_g, u_b, 0.38)))
                mb.face(o0, i1, i0, pid, tile_g, (mb.atl.uv(tile_g, u_a, 0.62), mb.atl.uv(tile_g, u_b, 0.38), mb.atl.uv(tile_g, u_a, 0.38)))
        zl = Rm @ (rot(np.array([0, 0, 1.0])) if (sign < 0 and open_) else np.array([0, 0, 1.0]))
        mb.flip_if(f2, lambda cen, n: np.dot(n, zl * (-sign)) < 0)
        return pid, ob, ib, tip

    pu, obu, ibu, tipu = half(+1, Lu, hu, hook, "bill_upper")
    pl, obl, ibl, tipl = half(-1, Ll, hl, 0.0, "bill_lower")
    tip_w = to_w(tipu)
    info = dict(tipUpper=tip_w, tipLower=to_w(rot(tipl) if open_ else tipl), base=O, dir=f, up=u)
    # flange rolls: swollen rim at the mouth corners, one per side, wrapping the corner over both halves
    pf = {}
    for sg, nm in ((1, "L"), (-1, "R")):
        pid = mb.part("flange_" + nm)
        wf = lambda t: wb * (1 - t) ** 0.7
        up_pt = lambda t: np.array([t * Lu, sg * wf(t), -hook * Lu * t ** 3])
        lo_pt = lambda t: rot(np.array([t * Ll, sg * wf(t), 0.0])) if open_ else np.array([t * Ll, sg * wf(t), 0.0])
        if open_:
            path = [lo_pt(0.42), lo_pt(0.2), np.array([0.0, sg * wb, 0.0]), up_pt(0.2), up_pt(0.48)]
            rr = [0.45, 0.9, 1.0, 0.9, 0.5]
        else:
            path = [np.array([0.0, sg * wb * 1.02, 0.0]), up_pt(0.18), up_pt(0.42)]
            rr = [1.0, 0.8, 0.35]
        # the roll sits a little outside the rim so it never pokes into the cavity
        path = [p + np.array([0, sg * fl_r * 0.15, 0]) for p in path]
        P3, V = spline([to_w(p) for p in path], np.array(rr)[:, None] * fl_r, 3)
        Wr = np.maximum(V[:, 0], fl_r * 0.12)
        loft(mb, pid, f"{sid}:gape", P3, Wr, Wr * 0.9, u, seg=7, uvr=(0.56, 0.96, 0.15, 0.85), capk0=0.5, capk1=0.5)
    # throat: the opening between the halves closed by a dark bulge facing the viewer
    if open_:
        pid = mb.part("throat")
        vv = np.array(mb.V)
        loop_ids = [ibu + p for p in range(nphi + 1)] + [ibl + p for p in range(nphi - 1, 0, -1)]
        loop_pts = vv[loop_ids]
        cen = loop_pts.mean(0) + f * (0.30 * wb)
        ci = mb.verts([cen])
        f3 = len(mb.F)
        tile_g = f"{sid}:gape"
        m = len(loop_ids)
        for i in range(m):
            a, b = loop_ids[i], loop_ids[(i + 1) % m]
            uvs = (mb.atl.uv(tile_g, 0.02, 0.5), mb.atl.uv(tile_g, 0.07, 0.15 + 0.7 * i / m), mb.atl.uv(tile_g, 0.07, 0.15 + 0.7 * (i + 1) / m))
            mb.face(ci, a, b, pid, tile_g, uvs)
        mb.flip_if(f3, lambda cn, n: np.dot(n, f) < 0)
    return info


# =============================================================================================================
# one chick
# =============================================================================================================
def build_chick(S, age, pose, atl, nscale=1.0):
    sid, typ = S["id"], S["typ"]
    ai = AGES.index(age)
    T = TYPES[typ]
    rng = np.random.default_rng(seed_of(sid, age, pose))
    L = S["L"] / 100.0
    u_ = L * T["k"][ai]
    rh = u_ * T["rh"][ai]
    bw = u_ * 0.205 * T["fat"] * (1.0, 0.99, 0.93)[ai]
    bh = bw * 0.86
    beg = pose == "Beg"
    stand = STAND.get(typ, 0.0) * u_ if (beg and ai >= 0) else 0.0
    lift = stand
    mb = MB(sid, atl)
    tile_body = f"{sid}:" + ("skin", "pin", "juv")[ai]
    P_body = mb.part("body")

    # ---------------- bill size
    bl_ad = S["bl"] or T["bl"]
    g_age = (0.36, 0.70, 0.95)[ai]
    Lu = L * bl_ad * g_age
    if typ in ("heron", "cormorant", "crow", "swan"):
        Lu = max(Lu, rh * 0.9)
    gp = S["gp"] * (1.35 if ai == 0 and typ in ("song", "swift", "crow") else 1.15 if ai == 1 and typ in ("song", "swift", "crow") else 1.0)
    wb = rh * T["bwr"] * gp
    wb = min(wb, rh * 0.80)
    if not beg:
        wb *= 0.62
    hu, hl = rh * T["hu"], rh * T["hl"]
    Ll = Lu * 0.88
    hook = HOOK.get(typ, 0.0) if True else 0.0
    gape_deg = T["gape"] if beg else 0.0
    theta = math.radians(gape_deg)
    open_ = beg and gape_deg > 0.5
    fl_r = max(hu * 0.28, 0.0005)
    if typ in ("raptor", "heron", "gull", "swan", "cormorant"):
        fl_r *= 0.55

    # ---------------- spine control rows (x, z, w, h)
    rows = []
    b0 = [(-0.34, 0.26, 0.22), (-0.28, 0.62, 0.62), (-0.15, 0.93, 0.93), (0.0, 1.0, 1.0), (0.12, 0.88, 0.86)]
    for x, hh, ww in b0:
        h_ = bh * hh
        rows.append((x * u_, h_ + lift, ww * bw, h_))
    xc = 0.13 * u_
    if not beg:
        xh = 0.21 * u_ + 0.1 * rh
        zh = max(0.95 * rh, 1.30 * bh + 0.12 * rh) + lift
        alpha = math.radians(-6)
        nb = ((xc + xh) / 2, (rows[-1][1] + zh) / 2, 0.88 * rh, 0.88 * rh)
        mid_rows = [nb]
    else:
        alpha = math.radians(T["adir"])
        nlen = T["nlen"] * u_
        ndx = T["ndx"] * u_
        zs = rows[-1][1] + rows[-1][3] * 0.55
        nr = max(rh * 0.5, bw * 0.3)
        sh = (xc + 0.02 * u_, zs, 0.62 * bw, 0.75 * bh)
        xh = xc + 0.02 * u_ + ndx
        zh = zs + nlen + rh * 0.2
        mid_rows = [sh, (xh - ndx * 0.4, zs + nlen * 0.5, nr, nr), (xh - ndx * 0.1, zs + nlen * 0.85, nr * 1.05, nr * 1.05)]
    d_head = np.array([math.cos(alpha), 0.0, math.sin(alpha)])
    hc_row = (xh, zh, rh, rh * 0.96)
    wf = max(0.80 * rh, 1.2 * wb)
    hf = max(0.76 * rh, 1.2 * max(hu, hl))
    fpos = np.array([xh, 0, zh]) + d_head * rh * 0.92
    face_row = (fpos[0], fpos[2], wf, hf)
    allrows = rows + mid_rows + [hc_row, face_row]
    Pc = np.array([[r[0], 0.0, r[1]] for r in allrows])
    Vc = np.array([[r[2], r[3]] for r in allrows])
    per = 2
    Cs, Vs = spline(Pc, Vc, per)
    Ws, Hs = np.maximum(Vs[:, 0], 1e-4), np.maximum(Vs[:, 1], 1e-4)
    info = loft(mb, P_body, tile_body, Cs, Ws, Hs, [0, 1, 0], seg=12, cap0=True, cap1=True, capk0=0.7, capk1=0.0)
    ih = (len(allrows) - 2) * per          # ring index of the head centre
    nB = len(rows) - 1                     # last body control row index
    # ---------------- bill frame at the face disc
    f = info["T"][-1]
    s_ax = info["S"][-1]
    u_ax = np.cross(f, s_ax)
    O = info["C"][-1]
    binfo = build_bill(mb, S, O, f, s_ax, u_ax, Lu, Ll, wb, hu, hl, theta, hook, open_, fl_r)

    # ---------------- eyes
    re = rh * (0.19 if ai == 0 else 0.18 if ai == 1 else 0.17)
    if typ in ("raptor",):
        re *= 1.0
    strip = ai                   # 0 closed, 1 slit, 2 open
    tile_eye = f"{sid}:eye"
    sw = 1.0 / 3.0
    for sg, nm in ((1, "L"), (-1, "R")):
        pid = mb.part("eye_" + nm)
        fi = ih + 0.30 * rh / max(np.linalg.norm(info["C"][ih + 1] - info["C"][ih]), 1e-9)
        th = 0.30 if sg > 0 else math.pi - 0.30
        p, nrm, tg, ss, nn_ = surf(info, fi, th, out=-0.42 * re)
        e = nrm
        pole = unit(nn_ - e * np.dot(nn_, e))
        e2 = np.cross(pole, e)
        uvf = lambda lon, lat, st=strip: ((st + 0.06 + 0.88 * (0.5 + lon / (2 * np.pi))) * sw, 0.04 + 0.92 * (lat / np.pi + 0.5))
        ellipsoid(mb, pid, tile_eye, p, (e, e2 * sg, pole), (re, re, re), nlon=10, nlat=6, uvfun=uvf)

    # ---------------- legs
    leg_t = f"{sid}:leg"
    lr = max(u_ * 0.028, 0.0008)
    if typ in ("heron", "raptor", "cormorant"):
        lr *= 1.1
    for sg, nm in ((1, "L"), (-1, "R")):
        pid = mb.part("leg_" + nm)
        hip = np.array([-0.01 * u_, sg * bw * 0.62, bh * 0.55 + lift])
        if stand > 0:
            ank = np.array([0.02 * u_, sg * bw * 0.62, lr * 1.2])
        else:
            ank = np.array([0.03 * u_, sg * bw * 0.66, lr * 1.2])
        P3, V = spline([hip, (hip + ank) / 2 + np.array([0, 0, lr * 0.5]), ank], np.ones((3, 1)), 3)
        wid = np.linspace(lr * 1.15, lr * 0.85, len(P3))
        loft(mb, pid, leg_t, P3, wid, wid, [1, 0, 0], seg=6, uvr=(0.1, 0.9, 0.1, 0.9), capk0=0.3, capk1=0.4)
        # foot: flat pad with three toe ridges is overkill at this scale: a flat wedge
        pid = mb.part("foot_" + nm)
        fx = ank[0] + (0.035 * u_ if typ in ("gull", "swan", "cormorant") else 0.02 * u_)
        ellipsoid(mb, pid, leg_t, np.array([fx, ank[1], lr * 0.55]), (np.array([1.0, 0, 0]), np.array([0, 1.0, 0]), np.array([0, 0, 1.0])),
                  (0.04 * u_ + lr * 0.5, 0.026 * u_ + lr * 0.5, lr * 0.62), nlon=8, nlat=4,
                  uvfun=lambda lon, lat: (0.1 + 0.8 * (0.5 + lon / (2 * np.pi)), 0.1 + 0.8 * (lat / np.pi + 0.5)))

    # ---------------- wings
    ws = T["wing"][ai] * u_
    i_sh = nB * per - 0.4
    for sg, nm in ((1, "L"), (-1, "R")):
        pid = mb.part("wing_" + nm)
        th = (0.62 if sg > 0 else math.pi - 0.62)
        p0, nrm, tg, ss, nn_ = surf(info, i_sh, th, out=0.0)
        if ai == 0:
            Wd0, Hd0, Wd1, Hd1 = ws * 0.34, ws * 0.30, ws * 0.18, ws * 0.17
        else:
            Wd0, Hd0, Wd1, Hd1 = ws * 0.15, ws * 0.06, ws * 0.07, ws * 0.03
        if not beg:
            dirv = unit([-1.0, sg * 0.10, -0.12])
            ref = [0, 0, 1.0]
        else:
            dirv = unit([-0.85, sg * 0.42, 0.14])
            ref = [1.0, 0, 0]
        start = p0 + nrm * Hd0 * 0.9
        pts = [start, start + dirv * ws * 0.5, start + dirv * ws]
        P3, V = spline(pts, np.array([[Wd0, Hd0], [(Wd0 + Wd1) / 2, (Hd0 + Hd1) / 2], [Wd1, Hd1]]), 3)
        loft(mb, pid, tile_body, P3, V[:, 0], V[:, 1], ref, seg=8, capk0=0.5, capk1=0.7)
        # pin quills (Mid) or feather cards (Late) along the wing
        if ai == 1:
            nq = int(8 * nscale)
            for q in range(nq):
                fr = 0.25 + 0.7 * q / max(nq - 1, 1)
                pp = start + dirv * ws * fr
                dq = unit(dirv * 0.7 + np.array([-0.5, 0, 0]) + rng.normal(0, 0.08, 3))
                pid_q = mb.part(f"quill_w{nm}{q}")
                cone(mb, pid_q, f"{sid}:pin", pp + unit(np.cross(dirv, ref)) * Wd0 * 0.4 * (1 if q % 2 else -1), dq, ws * 0.28, ws * 0.025, (rng.uniform(0.05, 0.6), rng.uniform(0.05, 0.5), 0.2, 0.4), sides=3)
        if ai == 2:
            nf = 5
            for q in range(nf):
                fr = 0.30 + 0.62 * q / (nf - 1)
                pp = start + dirv * ws * fr + nn_ * 0.0006 * q
                ang = (q - (nf - 1) / 2) * 0.10
                dq = unit(dirv * 0.55 + np.array([-0.95, 0, -0.10]) + np.array([0, sg * 0.15 * (q - 2), 0]))
                pid_q = mb.part(f"wcard_{nm}{q}")
                # card normal hint: the wing's flat face normal (up for a folded wing is lateral; use the wing's own n)
                hint = nn_ if not beg else np.array([0, 0, 1.0])
                card(mb, pid_q, f"{sid}:juv", pp + np.array([0, 0, 0.0004 * q]), dq, ws * 0.50, ws * 0.06, ws * 0.018, hint, (rng.uniform(0.0, 0.55), rng.uniform(0.0, 0.5), 0.4, 0.45))

    # ---------------- tail
    tail_len = T["tail"] * u_
    p_r = info["C"][0] + np.array([0.0, 0.0, 0.0])
    if ai == 2:
        nt_ = 5
        for q in range(nt_):
            ang = (q - (nt_ - 1) / 2) * 0.17
            dq = unit([-math.cos(ang), math.sin(ang) * 0.9, -0.05 - 0.0 * q])
            pid_q = mb.part(f"tail_{q}")
            card(mb, pid_q, f"{sid}:juv", p_r + np.array([-0.002, 0, bh * 0.38 + 0.00045 * (q if q < 3 else 5 - q) + 0.0006]), dq, tail_len * (1.0 - 0.08 * abs(q - 2)),
                 tail_len * 0.20, tail_len * 0.10, [0, 0, 1.0], (rng.uniform(0.0, 0.55), rng.uniform(0.0, 0.5), 0.4, 0.45))
    elif ai == 1:
        for q in range(3):
            pid_q = mb.part(f"tailquill_{q}")
            dq = unit([-1.0, (q - 1) * 0.35, 0.1])
            cone(mb, pid_q, f"{sid}:pin", p_r + np.array([0.0, (q - 1) * 0.004, bh * 0.38]), dq, u_ * 0.06, u_ * 0.012, (rng.uniform(0.05, 0.6), rng.uniform(0.05, 0.5), 0.2, 0.4), sides=3)

    # ---------------- down tufts, quills, crest
    kind, dn, qn = DOWN[typ]
    ntuft = int(dn[ai] * nscale)
    nq = int(qn[ai] * nscale)
    has_down = S["down"] is not None
    tile_d = f"{sid}:down"
    tile_pf = f"{sid}:skin" if typ in ("gull", "swan", "heron", "raptor", "pigeon") else tile_d
    zones = [(0.5, max(nB * per - 1.0, 1.2), (0.35, math.pi - 0.35), 0.82), (ih - 1.2, ih + 0.8, (0.5, math.pi - 0.5), 0.18)]
    if ntuft and has_down:
        for q in range(ntuft):
            z = zones[0] if rng.random() < zones_prob(typ, ai) else zones[1]
            fi = rng.uniform(z[0], z[1])
            th = rng.uniform(*z[2]) if rng.random() < 0.9 else rng.uniform(-0.2, 0.2)
            p, nrm, tg, ss, nn_ = surf(info, fi, th)
            dv = unit(nrm * 0.45 - tg * 0.85 + rng.normal(0, 0.15, 3))
            pid_t = mb.part(f"down_{q}")
            ln = u_ * (0.045 if kind == "cone" else 0.04) * rng.uniform(0.7, 1.3)
            if kind == "cone":
                cone(mb, pid_t, tile_d, p, dv, ln, ln * 0.19, (rng.uniform(0.0, 0.7), rng.uniform(0.0, 0.5), 0.22, 0.45), sides=3,
                     curl=unit(rng.normal(0, 1, 3)) * ln * 0.25)
            else:
                puff(mb, pid_t, tile_pf, p, unit(dv - nrm * 0.3), ln * 0.55, ln * 0.34, (rng.uniform(0.0, 0.7), rng.uniform(0.0, 0.5), 0.25, 0.45))
    if S["crest"]:   # the heron's spiky "punk" crest of long head down
        for q in range(int(12 * nscale)):
            fi = ih - 0.8 + rng.uniform(0, 1.6)
            th = math.pi / 2 + rng.uniform(-0.55, 0.55)
            p, nrm, tg, ss, nn_ = surf(info, fi, th)
            dv = unit(nrm * 0.8 - tg * 0.5 + rng.normal(0, 0.2, 3))
            pid_t = mb.part(f"crest_{q}")
            ln = u_ * 0.16 * rng.uniform(0.7, 1.2) * (1.0, 0.9, 0.7)[ai]
            cone(mb, pid_t, tile_d, p, dv, ln, ln * 0.035, (rng.uniform(0.0, 0.7), rng.uniform(0.0, 0.4), 0.18, 0.55), sides=3, curl=unit(rng.normal(0, 1, 3)) * ln * 0.2)
    if nq:
        for q in range(nq):
            fi = rng.uniform(0.8, max(nB * per - 0.6, 1.5))
            th = rng.uniform(0.35, math.pi - 0.35)
            p, nrm, tg, ss, nn_ = surf(info, fi, th)
            dv = unit(nrm * 0.45 - tg * 0.85 + rng.normal(0, 0.1, 3))
            pid_t = mb.part(f"quill_{q}")
            ln = u_ * 0.05 * rng.uniform(0.7, 1.2)
            cone(mb, pid_t, f"{sid}:pin", p, dv, ln, ln * 0.10, (rng.uniform(0.05, 0.6), rng.uniform(0.05, 0.5), 0.2, 0.4), sides=3)
    return mb, dict(billTip=binfo["tipUpper"], tipLower=binfo["tipLower"], billBase=binfo["base"], billDir=binfo["dir"], billUp=binfo["up"], gapeDeg=gape_deg,
                    head=np.array([xh, 0, zh]), u=u_, R=0.0)


def zones_prob(typ, ai):
    return 0.78


# =============================================================================================================
# mesh -> Blender object
# =============================================================================================================
def box_uvs(mb, V, ctr, R):
    """fill the missing face uvs: box projection by dominant normal axis into the cell, extent R about ctr."""
    for i, (uv, (a, b, c)) in enumerate(zip(mb.UV, mb.F)):
        if uv is not None:
            continue
        P = V[[a, b, c]]
        n = np.cross(P[1] - P[0], P[2] - P[0])
        ax = int(np.argmax(np.abs(n)))
        k = [1, 2] if ax == 0 else [0, 2] if ax == 1 else [0, 1]
        q = (P[:, k] - ctr[k]) / (2 * R) + 0.5
        # mirror one axis on the -side so a symmetric chick keeps its texture orientation
        mb.UV[i] = tuple(mb.atl.uv(mb.TILE[i], *p) for p in q)


def make_object(mb, name, mat, info):
    V = np.array(mb.V)
    ctr = (V.min(0) + V.max(0)) / 2
    R = np.max(np.linalg.norm(V - ctr, axis=1)) * 1.02
    box_uvs(mb, V, ctr, R)
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in V], [], [list(f) for f in mb.F])
    uvl = me.uv_layers.new(name="UVMap")
    flat = np.array([uv for f in mb.UV for uv in f], np.float32)
    uvl.data.foreach_set("uv", flat.ravel())
    pa = me.attributes.new("lint_part", "INT", "FACE")
    pa.data.foreach_set("value", np.array(mb.PART, np.int32))
    for p in me.polygons:
        p.use_smooth = True
    me.update()
    me.validate(clean_customdata=False)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(mat)
    return ob


def make_material(atl):
    m = bpy.data.materials.new("M_ChickKit")
    m.use_nodes = True
    nt = m.node_tree
    p = C.principled(m)
    p.inputs["Base Color"].default_value = (1, 1, 1, 1)

    def img(fn, cs):
        im = bpy.data.images.load(os.path.join(OUT, fn))
        im.colorspace_settings.name = cs
        im.pack()
        return im
    def tex(im, loc):
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = im
        t.location = loc
        return t
    tc = tex(img("T_ChickKit_C.png", "sRGB"), (-700, 300))
    tn = tex(img("T_ChickKit_N.png", "Non-Color"), (-700, 0))
    tr = tex(img("T_ChickKit_R.png", "Non-Color"), (-700, -300))
    nt.links.new(tc.outputs["Color"], p.inputs["Base Color"])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nm.inputs["Strength"].default_value = 0.8
    nt.links.new(tn.outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], p.inputs["Normal"])
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(tr.outputs["Color"], sep.inputs["Color"])
    nt.links.new(sep.outputs[0], p.inputs["Roughness"])
    if "Specular IOR Level" in p.inputs:
        p.inputs["Specular IOR Level"].default_value = 0.4
    m.use_backface_culling = False
    return m


# =============================================================================================================
# previews
# =============================================================================================================
def setup_render():
    sc = bpy.context.scene
    C.set_engine(sc)
    sc.render.resolution_x, sc.render.resolution_y = 480, 400
    sc.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("W")
    sc.world = w
    w.use_nodes = True
    b = next(n for n in w.node_tree.nodes if n.type == "BACKGROUND")
    b.inputs["Color"].default_value = (0.62, 0.66, 0.72, 1)
    b.inputs["Strength"].default_value = 0.8
    sd = bpy.data.lights.new("Sun", "SUN")
    sd.energy = 3.0
    sd.color = (1.0, 0.93, 0.85)
    sun = bpy.data.objects.new("Sun", sd)
    sc.collection.objects.link(sun)
    sun.rotation_euler = mathutils.Vector((-0.45, -0.5, -0.75)).to_track_quat("-Z", "Y").to_euler()
    cd = bpy.data.cameras.new("Cam")
    cd.clip_start = 0.001
    cd.clip_end = 10
    co = bpy.data.objects.new("Cam", cd)
    sc.collection.objects.link(co)
    sc.camera = co
    # floor disc so the chick sits on something (a nest-brown plate)
    fm = bpy.data.materials.new("Floor")
    fm.use_nodes = True
    C.principled(fm).inputs["Base Color"].default_value = (0.20, 0.14, 0.09, 1)
    C.principled(fm).inputs["Roughness"].default_value = 0.95
    bpy.ops.mesh.primitive_circle_add(vertices=48, radius=1.0, fill_type="NGON", location=(0, 0, -0.0004))
    fl = bpy.context.active_object
    fl.name = "PreviewFloor"
    fl.data.materials.append(fm)
    return co, fl


def shoot(co, ob, path, target, radius, az=30, el=18, ortho=True, dist_k=1.0, scale=None):
    sc = bpy.context.scene
    cd = co.data
    cd.type = "ORTHO"
    cd.ortho_scale = radius * 2.15 if scale is None else scale
    a, e = math.radians(az), math.radians(el)
    d = mathutils.Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e)))
    co.location = mathutils.Vector(target) + d * 2.0
    co.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


def run_previews(objs, meta, ids, atl):
    os.makedirs(PREV, exist_ok=True)
    co, floor = setup_render()
    for o in objs.values():
        o.hide_render = True
    made = {}
    for sid in ids:
        files = []
        for age in AGES:
            for pose in POSES:
                nm = f"SM_Chick_{sid}_{age}_{pose}"
                o = objs[nm]
                o.hide_render = False
                m = meta[nm]
                b = np.array(m["bounds"])
                ctr = (np.array(b[0]) + np.array(b[1])) / 2
                rad = float(np.linalg.norm(np.array(b[1]) - np.array(b[0]))) / 2
                floor.scale = (rad * 1.5, rad * 1.5, 1)
                path = os.path.join(PREV, f"{sid}_{age}_{pose}.png")
                shoot(co, o, path, ctr, rad)
                o.hide_render = True
                files.append(path)
        made[sid] = files
        # gape close-up: Mid Beg, camera 35 deg off the bill axis, from above/front, ortho on the bill
        nm = f"SM_Chick_{sid}_Mid_Beg"
        o = objs[nm]
        o.hide_render = False
        m = meta[nm]
        base = np.array(m["billBase"])
        bd = np.array(m["billDir"])
        tip = np.array(m["billTip"])
        bl = float(np.linalg.norm(tip - base))
        mouth = base + bd * bl * 0.45
        bu = np.array(m["billUp"])
        hg = math.radians(m["gapeDeg"]) / 2
        lat = np.cross(bu, bd)
        cam_dir = mathutils.Vector(unit(bd * math.cos(hg) - bu * math.sin(hg) + 0.42 * lat + 0.15 * bu))
        cam_dir.normalize()
        co.data.type = "ORTHO"
        co.data.ortho_scale = max(bl * 3.4, float(np.linalg.norm(m["size"])) * 0.30, 0.01)
        co.location = mathutils.Vector(mouth) + cam_dir * 1.0
        co.rotation_euler = (-cam_dir).to_track_quat("-Z", "Y").to_euler()
        bpy.context.scene.render.filepath = os.path.join(PREV, f"{sid}_gape.png")
        bpy.ops.render.render(write_still=True)
        o.hide_render = True
    floor.hide_render = True
    return made


def sheets(ids):
    bl = bpy.app.binary_path
    cs = os.path.join(BIRDS, "contact_sheet.py")
    outs = []
    for sid in ids:
        files = [os.path.join(PREV, f"{sid}_{a}_{p}.png") for p in ("Rest", "Beg") for a in AGES] + [os.path.join(PREV, f"{sid}_gape.png")]
        out = os.path.join(PREV, f"sheet_{sid}.png")
        subprocess.run([bl, "-b", "--factory-startup", "--python", cs, "--", out, "3", "480"] + files, check=True, stdout=subprocess.DEVNULL)
        outs.append(out)
    return outs


def mega(outs, name, cols=3, cw=700):
    bl = bpy.app.binary_path
    cs = os.path.join(BIRDS, "contact_sheet.py")
    subprocess.run([bl, "-b", "--factory-startup", "--python", cs, "--", os.path.join(PREV, name), str(cols), str(cw)] + outs, check=True, stdout=subprocess.DEVNULL)


MOVABLE_PREFIX = ("down_", "quill_", "crest_", "wcard_", "tail_", "tailquill_", "wing_", "flange_", "eye_", "foot_", "leg_")


def separate(mb, pushes):
    """Lift apart what the lint calls z-fighting: the movable part of each near-coplanar pair (tufts, quills, cards, wings, flange
    rolls, eyes, feet) moves 3 x ZF away from the other face along its normal. Returns True when something moved."""
    pv = {}
    for f, p in zip(mb.F, mb.PART):
        pv.setdefault(p, set()).update(f)
    disp = {}
    for pi, pj, nvec, d in pushes:
        mi = mb.pn[pj].startswith(MOVABLE_PREFIX)
        mj = mb.pn[pi].startswith(MOVABLE_PREFIX)
        need = lint_bird.ZF * 3.0 - d + 0.00005
        if mi:
            tgt, vec = pj, np.asarray(nvec) * need
        elif mj:
            tgt, vec = pi, -np.asarray(nvec) * need
        else:
            continue
        cur = disp.get(tgt)
        if cur is None or np.linalg.norm(vec) > np.linalg.norm(cur):
            disp[tgt] = vec
    if not disp:
        return False
    V = np.array(mb.V)
    for p, vec in disp.items():
        idx = np.array(sorted(pv[p]))
        V[idx] += vec
    mb.V = [tuple(v) for v in V]
    return True


# =============================================================================================================
def main():
    t0 = time.time()
    table = json.load(open(os.path.join(BIRDS, "nests", "nest_table.json")))["species"]
    all_ids = [s["id"] for s in table]
    ids = ONLY if ONLY else all_ids
    spj = {s["id"]: s for s in json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "species.json")))["species"]}
    for sid in all_ids:
        if sid in spj:
            KIT[sid]["L"] = float(spj[sid]["lengthCm"])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    atl = Atlas(all_ids) if PREV_ONLY else build_atlas(all_ids)
    print(f"ATLAS painted {time.time() - t0:.1f}s")
    mat = make_material(atl)
    objs, meta, lintres = {}, {}, {}
    for sid in ids:
        S = KIT[sid]
        cap = 5000 if S["L"] >= 40 else 2500
        for age in AGES:
            for pose in POSES:
                name = f"SM_Chick_{sid}_{age}_{pose}"
                ns = 1.0
                for attempt in range(6):
                    mb, inf = build_chick(S, age, pose, atl, ns)
                    if mb.tris() <= cap:
                        break
                    ns *= 0.75
                ob = make_object(mb, name, mat, inf)
                objs[name] = ob
                r = None
                if DO_LINT:
                    for it in range(6):
                        r = lint_bird.lint(ob, atl, name, {i: n for i, n in enumerate(mb.pn)}, verbose=False, collect=True)
                        if not r["issues"] or not r.get("_push"):
                            break
                        if not separate(mb, r["_push"]):
                            break
                        bpy.data.objects.remove(ob)
                        bpy.data.meshes.remove(bpy.data.meshes[name]) if name in bpy.data.meshes else None
                        ob = make_object(mb, name, mat, inf)
                        objs[name] = ob
                V = np.array(mb.V)
                mn, mx = V.min(0), V.max(0)
                meta[name] = dict(tris=mb.tris(), bounds=[[round(float(x), 5) for x in mn], [round(float(x), 5) for x in mx]],
                                  size=[round(float(x), 5) for x in (mx - mn)],
                                  billTip=[round(float(x), 5) for x in inf["billTip"]], billBase=[round(float(x), 5) for x in inf["billBase"]],
                                  billDir=[round(float(x), 5) for x in inf["billDir"]], billUp=[round(float(x), 5) for x in inf["billUp"]], gapeDeg=inf["gapeDeg"],
                                  head=[round(float(x), 5) for x in inf["head"]], lengthM=round(float(inf["u"]), 5))
                if DO_LINT:
                    lintres[name] = {k: r[k] for k in ("tris", "degenerate", "uv_zero", "uv_stretch", "uv_outside", "zfight", "inside", "near_layers", "crossing", "issues")}
                    lintres[name]["pairs"] = r["zfight_pairs"]
                    lintres[name]["examples"] = r["examples"]
                    lintres[name]["separate_passes"] = it
                    print(f"LINT {name}: tris {r['tris']} deg={r['degenerate']} uv0={r['uv_zero']} str={r['uv_stretch']} out={r['uv_outside']} zf={r['zfight']} in={r['inside']} passes={it}" + (f" {r['zfight_pairs'][:3]} {r['examples']}" if r['issues'] else ""), flush=True)
        print(f"BUILT {sid} {time.time() - t0:.0f}s", flush=True)
    if PREV_ONLY:
        run_previews(objs, meta, ids, atl)
        sheets(ids)
        return
    suffix = "_" + "_".join(ONLY) if ONLY else ""
    # strip lint attribute, export
    for o in objs.values():
        if "lint_part" in o.data.attributes:
            o.data.attributes.remove(o.data.attributes["lint_part"])
    json.dump(dict(species=ids, meshes=meta, atlas=dict(size=SIZE, cell=CELL, files=["T_ChickKit_C.png", "T_ChickKit_N.png", "T_ChickKit_R.png"], material="M_ChickKit"),
                   note="coordinates: Blender (X forward, Z up), metres, origin on the nest floor under the belly; billTip = tip of the upper bill (Beg: the gaping bill, "
                        "billBase/billDir/billUp = mouth hinge, bill axis and up; head = head centre)"),
              open(os.path.join(OUT, f"chick_kit{suffix}.json"), "w"), indent=1)
    if DO_LINT:
        json.dump(lintres, open(os.path.join(OUT, f"lint_report{suffix}.json"), "w"), indent=1, default=str)
        keys = ("tris", "degenerate", "uv_zero", "uv_stretch", "uv_outside", "zfight", "inside", "issues")
        lines = ["LINT SUMMARY (per species, 6 meshes each)", f"{'species':18s} " + " ".join(f"{k:>10s}" for k in keys) + "   max_tris"]
        for sid in ids:
            rs = [v for k, v in lintres.items() if k.startswith(f"SM_Chick_{sid}_")]
            lines.append(f"{sid:18s} " + " ".join(f"{sum(v[k] for v in rs):10d}" for k in keys) + f"   {max(v['tris'] for v in rs):7d}")
        tot = {k: sum(v[k] for v in lintres.values()) for k in keys}
        lines.append(f"LINT TOTAL {tot} meshes {len(lintres)}")
        print("\n".join(lines))
        open(os.path.join(OUT, f"lint_summary{suffix}.txt"), "w").write("\n".join(lines) + "\n")
    if PREVIEWS:
        os.makedirs(PREV, exist_ok=True)
        me = os.path.abspath(__file__)
        chunks = [ids[i::JOBS] for i in range(min(JOBS, len(ids)))]
        procs = [subprocess.Popen([bpy.app.binary_path, "-b", "--factory-startup", "--python", me, "--", "--previews-only", "--only", ",".join(c)],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) for c in chunks]
        for pr in procs:
            pr.wait()
        if not ONLY:
            mega([os.path.join(PREV, f"sheet_{sid}.png") for sid in ids], "ALL_species_sheets.png", cols=4, cw=480)
            mega([os.path.join(PREV, f"{sid}_gape.png") for sid in ids], "ALL_gapes.png", cols=7, cw=280)
            mega([os.path.join(PREV, f"{sid}_Late_Beg.png") for sid in ids], "ALL_Late_Beg.png", cols=7, cw=280)
            mega([os.path.join(PREV, f"{sid}_Hatch_Rest.png") for sid in ids], "ALL_Hatch_Rest.png", cols=7, cw=280)
            mega([os.path.join(PREV, f"{sid}_Mid_Beg.png") for sid in ids], "ALL_Mid_Beg.png", cols=7, cw=280)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs.values():
        o.select_set(True)
    bpy.context.view_layer.objects.active = next(iter(objs.values()))
    path = os.path.join(OUT, f"chick_kit{suffix}.glb")
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=False, export_normals=True,
              export_texcoords=True, export_materials="EXPORT", export_skins=False, export_animations=False, export_vertex_color="NONE",
              export_image_format="NONE")
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})
    print("EXPORTED", path, os.path.getsize(path), "bytes;", len(objs), "meshes;", f"{time.time() - t0:.0f}s")


main()
