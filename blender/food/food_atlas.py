"""The street food kit's atlas (docs/food.md): one 2048 texture set T_FoodKit_{C,N,R}.png, 4 x 4 cells of 512, painted with numpy
from seeded noise (no generated pictures, no photos). Each cell is a material; build_food.py / build_stalls.py map faces into
cells (CELLS: name -> (column, row), row 0 at the bottom, v up). Plain Python 3 with numpy + PIL (not Blender's Python):

    python3 blender/food/food_atlas.py          writes blender/food/export/T_FoodKit_{C,N,R}.png

Height maps give the normal map (OpenGL green up, as Blender bakes; the importer flips green for Unreal).
"""
import math
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "export")
FONT = os.path.join(HERE, "..", "..", "fonts", "Caveat-Bold.ttf")
SIZE = 2048
CELL = 512
# name -> (column, row); row 0 is the atlas's bottom row (v 0..0.25)
CELLS = {
    "kernel": (0, 0), "cob": (1, 0), "napkin": (2, 0), "rind": (3, 0),
    "curd": (0, 1), "smoked": (1, 1), "fibre": (2, 1), "canvas": (3, 1),
    "wood": (0, 2), "alu": (1, 2), "oilcloth": (2, 2), "sign_cheese": (3, 2),
    "umbrella": (0, 3), "wicker": (1, 3), "card": (2, 3), "steel": (3, 3),   # card: the corn stall's cardboard sign
}


def uv_rect(name, inset=6):
    """(u0, v0, u1, v1) of a cell, inset by a few texels against bleeding."""
    c, r = CELLS[name]
    e = inset / SIZE
    return (c * 0.25 + e, r * 0.25 + e, (c + 1) * 0.25 - e, (r + 1) * 0.25 - e)


def _noise(seed, n=CELL, octaves=5, base=4):
    """Tileable value noise 0..1 (sum of octaves of bilinear-upsampled random grids)."""
    rng = np.random.default_rng(seed)
    out = np.zeros((n, n))
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        g = base * 2 ** o
        grid = rng.random((g, g))
        y = np.linspace(0, g, n, endpoint=False)
        i0 = np.floor(y).astype(int)
        t = y - i0
        t = t * t * (3 - 2 * t)
        i1 = (i0 + 1) % g
        a = grid[i0][:, i0] * (1 - t)[None, :] + grid[i0][:, i1] * t[None, :]
        b = grid[i1][:, i0] * (1 - t)[None, :] + grid[i1][:, i1] * t[None, :]
        out += amp * (a * (1 - t)[:, None] + b * t[:, None])
        tot += amp
        amp *= 0.5
    return out / tot


def _grid(n=CELL):
    y, x = np.mgrid[0:n, 0:n] / n
    return x, y


def _mix(c0, c1, t):
    t = np.clip(t, 0, 1)[..., None]
    return np.array(c0)[None, None, :] * (1 - t) + np.array(c1)[None, None, :] * t


def kernel():
    x, y = _grid()
    n = _noise(11)
    col = _mix((0.93, 0.70, 0.16), (1.0, 0.83, 0.34), n)
    # salt crystals: a few bright specks
    rng = np.random.default_rng(3)
    s = rng.random((CELL, CELL)) > 0.9985
    col[s] = (0.97, 0.96, 0.92)
    h = n * 0.3 + s * 0.6
    rough = 0.32 + 0.15 * n
    return col, h, rough


def cob():
    x, y = _grid()
    # sockets in rows (the kernels' pits) on pale pithy flesh
    cells = (np.sin(x * math.pi * 28) * np.sin(y * math.pi * 22))
    n = _noise(12)
    col = _mix((0.86, 0.80, 0.58), (0.95, 0.90, 0.70), n * 0.6 + 0.4 * (cells > 0))
    h = 0.5 + 0.5 * cells * 0.6 + n * 0.2
    return col, h, 0.75 + 0.1 * n


def napkin():
    x, y = _grid()
    n = _noise(13, octaves=6, base=3)
    creases = np.abs(np.sin((x * 3 + n * 2.5) * math.pi * 3)) ** 8
    col = _mix((0.93, 0.93, 0.90), (0.99, 0.99, 0.97), n)
    col -= creases[..., None] * 0.08
    # a grease spot or two from the hot cob
    d = np.hypot(x - 0.35, y - 0.6)
    col[d < 0.12] *= 0.9
    return col, n * 0.3 + creases * 0.5, 0.85 + 0.05 * n


def rind():
    """Adyghe cheese's side: the basket's weave pressed into fresh white curd."""
    x, y = _grid()
    # finer round the rim (u) than across it (v): the rim's uvs run 0..1 round 35 cm and across ~6 cm
    u, v = x * 36, y * 7
    cu, cv = np.floor(u), np.floor(v)
    fu, fv = u - cu, v - cv
    over = ((cu + cv) % 2 == 0)
    strand = np.where(over, np.sin(fv * math.pi), np.sin(fu * math.pi)) ** 0.6
    n = _noise(14)
    h = 0.3 + 0.55 * strand + 0.15 * n
    col = _mix((0.86, 0.84, 0.76), (0.97, 0.96, 0.91), h * 0.8 + 0.2 * n)
    return col, h, 0.55 + 0.15 * (1 - strand)


def curd():
    x, y = _grid()
    n = _noise(15, octaves=6, base=6)
    rng = np.random.default_rng(5)
    holes = np.zeros((CELL, CELL))
    for _ in range(90):
        cx, cy, r = rng.random(), rng.random(), rng.uniform(0.004, 0.015)
        holes = np.maximum(holes, np.clip(1 - np.hypot(x - cx, y - cy) / r, 0, 1))
    col = _mix((0.93, 0.92, 0.86), (0.99, 0.98, 0.94), n)
    col -= holes[..., None] * 0.12
    return col, 0.5 + 0.3 * n - holes * 0.6, 0.5 + 0.2 * n


def smoked():
    """The smoked braid's skin: amber to brown, darker in the twists, glossy."""
    x, y = _grid()
    n = _noise(16)
    streak = _noise(17, octaves=3, base=2)
    t = 0.35 * n + 0.65 * (0.5 + 0.5 * np.sin((x * 6 + streak * 2) * math.pi * 2))
    col = _mix((0.55, 0.28, 0.08), (0.86, 0.55, 0.20), t)
    return col, t * 0.4, 0.35 + 0.2 * n


def fibre():
    """Inside the braid: ivory strings along the strand."""
    x, y = _grid()
    n = _noise(18)
    strings = np.abs(np.sin((x + n * 0.05) * math.pi * 60))
    col = _mix((0.90, 0.86, 0.72), (0.98, 0.95, 0.85), strings * 0.6 + n * 0.4)
    return col, strings * 0.5, 0.6


def canvas():
    """The tent: wide red and white stripes along v, a coarse weave, dust low down."""
    x, y = _grid()
    n = _noise(19)
    red = (np.floor(x * 8) % 2 == 0)
    col = np.where(red[..., None], np.array((0.62, 0.10, 0.08)), np.array((0.92, 0.90, 0.84)))
    weave = 0.5 + 0.5 * np.sin(x * math.pi * 512) * np.sin(y * math.pi * 512)
    col = col * (0.92 + 0.08 * weave[..., None]) * (0.9 + 0.1 * n[..., None])
    return col, weave * 0.2, 0.85


def wood():
    x, y = _grid()
    n = _noise(20, octaves=4, base=3)
    rings = 0.5 + 0.5 * np.sin((y * 40 + n * 6) * math.pi)
    plank = (np.abs(((x * 4) % 1) - 0.5) > 0.485)
    col = _mix((0.42, 0.28, 0.16), (0.62, 0.45, 0.28), rings * 0.6 + n * 0.4)
    col[plank] *= 0.45
    return col, rings * 0.2 - plank * 0.5, 0.7


def alu():
    x, y = _grid()
    n = _noise(21, octaves=6, base=8)
    brushed = _noise(22, octaves=3, base=2)
    col = _mix((0.62, 0.63, 0.64), (0.78, 0.79, 0.80), n * 0.5 + 0.5 * np.sin(y * math.pi * 300) ** 2 * 0.3 + brushed * 0.2)
    return col, n * 0.1, 0.3 + 0.2 * n


def oilcloth():
    """A Soviet oilcloth: little blue flowers on white, a check."""
    x, y = _grid()
    k = 10
    fu, fv = (x * k) % 1, (y * k) % 1
    check = ((np.floor(x * k) + np.floor(y * k)) % 2 == 0)
    col = np.where(check[..., None], np.array((0.92, 0.93, 0.95)), np.array((0.82, 0.88, 0.95)))
    d = np.hypot(fu - 0.5, fv - 0.5)
    petals = (d < 0.18) & (np.cos(np.arctan2(fv - 0.5, fu - 0.5) * 5) * 0.05 + 0.13 > d)
    col[petals] = (0.2, 0.35, 0.7)
    col[d < 0.04] = (0.95, 0.8, 0.2)
    return col, petals * 0.1, 0.25


def sign(text_lines, seed, bg=(236, 230, 214)):
    """Hand-lettered card: brown marker on white card, its lines centred."""
    from PIL import Image, ImageDraw, ImageFont
    img = Image.new("RGB", (CELL, CELL), bg)
    d = ImageDraw.Draw(img)
    sizes = [118, 92, 120]
    yy = 30
    for i, line in enumerate(text_lines):
        f = ImageFont.truetype(FONT, sizes[i % len(sizes)])
        w = d.textlength(line, font=f)
        while w > CELL - 30:
            f = ImageFont.truetype(FONT, int(f.size * 0.92))
            w = d.textlength(line, font=f)
        d.text(((CELL - w) / 2, yy), line, fill=(120, 30, 20) if i != 1 else (40, 60, 120), font=f)
        yy += f.size + 25
    col = np.asarray(img).astype(float) / 255.0
    col = col[::-1]                                  # v up: row 0 of the cell is its bottom
    n = _noise(seed)
    col *= (0.95 + 0.05 * n)[..., None]
    return col, n * 0.1, 0.8


def umbrella():
    """The corn stall's beach umbrella: yellow and white gores (u around), a faded edge."""
    x, y = _grid()
    gore = (np.floor(x * 8) % 2 == 0)
    col = np.where(gore[..., None], np.array((0.93, 0.74, 0.15)), np.array((0.94, 0.92, 0.85)))
    col *= (0.9 + 0.1 * _noise(23))[..., None]
    return col, np.zeros_like(x), 0.8


def wicker():
    x, y = _grid()
    k = 24
    u, v = x * k, y * k
    over = ((np.floor(u) + np.floor(v * 0.5)) % 2 == 0)
    strand = np.where(over, np.sin((u % 1) * math.pi), np.sin(((v * 0.5) % 1) * math.pi)) ** 0.7
    col = _mix((0.45, 0.32, 0.16), (0.78, 0.62, 0.38), strand * 0.8 + 0.2 * _noise(24))
    return col, strand, 0.75


def card():
    x, y = _grid()
    n = _noise(25)
    col = _mix((0.62, 0.48, 0.30), (0.74, 0.60, 0.40), n)
    corr = 0.5 + 0.5 * np.sin(x * math.pi * 120)
    return col, corr * 0.2, 0.9


def steel():
    x, y = _grid()
    n = _noise(26, octaves=6, base=6)
    col = _mix((0.16, 0.30, 0.22), (0.22, 0.38, 0.28), n)      # green enamel, the Soviet park kind
    chips = n > 0.82
    col[chips] = (0.45, 0.42, 0.38)
    return col, n * 0.1, 0.45 + 0.3 * chips


PAINT = {
    "kernel": kernel, "cob": cob, "napkin": napkin, "rind": rind, "curd": curd, "smoked": smoked, "fibre": fibre,
    "canvas": canvas, "wood": wood, "alu": alu, "oilcloth": oilcloth,
    "sign_cheese": lambda: sign(["Адыгейский", "сыр", "косичка · головки"], 27),
    "umbrella": umbrella, "wicker": wicker, "steel": steel,
    "card": lambda: sign(["Варёная", "кукуруза", "горячая · 15 р."], 28, bg=(176, 140, 96)),
}


def normal_from_height(h, strength=4.0):
    gy, gx = np.gradient(h)
    n = np.stack([-gx * strength, -gy * strength, np.ones_like(h)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def main():
    from PIL import Image
    os.makedirs(OUT, exist_ok=True)
    C = np.zeros((SIZE, SIZE, 3))
    N = np.zeros((SIZE, SIZE, 3))
    R = np.zeros((SIZE, SIZE))
    for name, (c, r) in CELLS.items():
        col, h, rough = PAINT[name]()
        ys, xs = slice(r * CELL, (r + 1) * CELL), slice(c * CELL, (c + 1) * CELL)
        C[ys, xs] = np.clip(col, 0, 1)
        N[ys, xs] = normal_from_height(np.asarray(h, dtype=float) * np.ones((CELL, CELL)))
        R[ys, xs] = np.clip(rough * np.ones((CELL, CELL)), 0.05, 1)
    flip = lambda a: a[::-1]                           # numpy rows go down, the atlas's v goes up
    Image.fromarray((flip(C) * 255).astype(np.uint8)).save(os.path.join(OUT, "T_FoodKit_C.png"))
    Image.fromarray((flip(N) * 255).astype(np.uint8)).save(os.path.join(OUT, "T_FoodKit_N.png"))
    Image.fromarray((flip(R) * 255).astype(np.uint8)).save(os.path.join(OUT, "T_FoodKit_R.png"))
    print("atlas ->", OUT)


if __name__ == "__main__":
    main()
