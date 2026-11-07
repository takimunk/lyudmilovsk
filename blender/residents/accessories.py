"""Procedural accessories (hats, bags, scarf, aprons, glasses, cane, facial hair) for Lyudmilovsk residents.

Every accessory is FITTED to the evaluated MPFB body (rest pose, shape keys applied) and returned as one
separate mesh object, rigidly skinned to a bone of the shared UE skeleton:

    import accessories as A
    obj = A.add("flat_cap", ctx)            # ctx = {basemesh, armature, hair_volume, sex, age}
    A.make_tiles("out/dir")                 # 512x512 tile textures (greyscale + tintable, or real colour)

Each object carries custom props: tile, tint_slot, tintable, kind, tint_rgb (preview default tint;
the game shader colours tintable tiles as  colour = tint * texel * 2 ).
Material: one slot named "Cloth" (opaque) or "Hair" (alpha-tested strands).  Blender material names are
unique per file, so when several accessories share a scene the 2nd+ get suffixes ("Cloth.001"): strip the
suffix on export (obj["material_name"] holds the intended name).
Runs under Blender (bpy 4.2+ / 5.x), uses numpy + PIL only.
"""
import math
import os
import tempfile

import numpy as np

KINDS = ["flat_cap", "knit_beanie", "baseball_cap", "sun_hat", "headscarf", "chef_toque", "backpack",
         "shoulder_bag", "string_bag", "shopping_bag", "scarf", "waist_apron", "bib_apron", "glasses",
         "cane", "mustache", "short_beard", "full_beard"]

PI = math.pi
N = 512


# ----------------------------------------------------------------------------------------------
# noise / texture helpers
# ----------------------------------------------------------------------------------------------
def _fnoise(beta=2.0, seed=0, n=N):
    """Tileable noise with a 1/f^beta spectrum, zero mean unit std."""
    r = np.random.default_rng(seed)
    F = np.fft.fft2(r.standard_normal((n, n)))
    fx = np.fft.fftfreq(n)[:, None]
    fy = np.fft.fftfreq(n)[None, :]
    f = np.sqrt(fx ** 2 + fy ** 2)
    f[0, 0] = 1.0
    F = F / f ** (beta / 2.0)
    F[0, 0] = 0
    a = np.fft.ifft2(F).real
    return (a - a.mean()) / (a.std() + 1e-9)


def _blur(a, sx, sy):
    """Tileable anisotropic gaussian blur (sigma in pixels along x (cols), y (rows))."""
    n = a.shape[0]
    fx = np.fft.fftfreq(n)[None, :]
    fy = np.fft.fftfreq(n)[:, None]
    g = np.exp(-2 * PI ** 2 * ((sx * fx) ** 2 + (sy * fy) ** 2))
    return np.fft.ifft2(np.fft.fft2(a) * g).real


def _streak(seed, sx, sy):
    r = np.random.default_rng(seed)
    a = _blur(r.standard_normal((N, N)), sx, sy)
    return (a - a.mean()) / (a.std() + 1e-9)


def _grid():
    yy, xx = np.mgrid[0:N, 0:N] / float(N)
    return xx, yy


def _fin(a, mask=None, lo=0.03, hi=0.97):
    """Normalise a greyscale tile to a mean of 0.5 (inside mask) and clip."""
    a = np.asarray(a, dtype=np.float64)
    for _ in range(4):
        m = a[mask].mean() if mask is not None else a.mean()
        a = a * (0.5 / max(m, 1e-4))
        a = np.clip(a, lo, hi)
    return a


def _smooth01(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def _t_knit():
    xx, yy = _grid()
    cols, rows = 72, 90
    cu, cv = xx * cols, yy * rows
    fu = cu % 1.0
    knit_col = (np.floor(cu) % 2 == 0)
    d = np.abs(fu - 0.5) * 2
    ch = (cv + 0.62 * d) % 1.0
    knit = 0.30 + 0.70 * np.sin(PI * ch) ** 1.3
    knit *= 0.62 + 0.38 * np.sin(PI * fu)
    purl = 0.26 + 0.34 * np.sin(2 * PI * cv) ** 2
    purl *= 0.7 + 0.3 * np.sin(PI * fu)
    a = np.where(knit_col, knit, purl)
    a = a * (0.88 + 0.12 * _fnoise(1.0, 1)) + 0.05 * _fnoise(0.3, 2) + 0.03 * _streak(3, 0.8, 0.8)
    return _fin(a)


def _t_felt():
    xx, yy = _grid()
    sx = np.floor(xx * 16) % 2
    tw = np.where(sx == 0, (xx * 128 + yy * 128) % 1.0, (xx * 128 - yy * 128) % 1.0)
    a = 0.5 + 0.17 * np.sin(2 * PI * tw)
    a += 0.12 * _fnoise(0.4, 4) + 0.10 * _streak(5, 0.7, 0.7) + 0.05 * _fnoise(2.0, 6)
    spk = (np.random.default_rng(7).random((N, N)) > 0.985) * 0.18
    return _fin(a + spk)


def _t_straw():
    xx, yy = _grid()
    k = 40
    sx, sy = xx * k, yy * k
    ix, iy = np.floor(sx), np.floor(sy)
    fx, fy = sx - ix, sy - iy
    over = ((ix + iy) % 2 == 0)
    hor = 0.50 + 0.38 * np.sin(PI * fy) ** 0.8
    ver = 0.50 + 0.38 * np.sin(PI * fx) ** 0.8
    a = np.where(over, hor, ver)
    fibre = np.where(over, _streak(8, 6, 0.6), _streak(9, 0.6, 6))
    a = a * (0.85 + 0.10 * fibre) + 0.04 * _fnoise(1.0, 10)
    gap = np.minimum(np.minimum(fx, 1 - fx), np.minimum(fy, 1 - fy))
    a *= 0.75 + 0.25 * _smooth01(gap / 0.12)
    return _fin(a)


def _t_canvas():
    xx, yy = _grid()
    wx = 0.5 + 0.5 * np.cos(2 * PI * xx * 150)
    wy = 0.5 + 0.5 * np.cos(2 * PI * yy * 150)
    chk = ((np.floor(xx * 150) + np.floor(yy * 150)) % 2)
    a = 0.30 + 0.34 * np.where(chk == 0, wx, wy) + 0.18 * (wx + wy) / 2
    a = a * (0.9 + 0.08 * _streak(11, 0.6, 4)) * (0.92 + 0.06 * _streak(12, 4, 0.6)) + 0.04 * _fnoise(1.2, 13)
    return _fin(a)


def _t_leather():
    r = 1 - np.abs(_fnoise(1.8, 14))
    g = np.clip(r, 0, 1) ** 4
    a = 0.42 + 0.35 * g + 0.10 * _fnoise(0.8, 15) + 0.05 * _fnoise(2.5, 16)
    return _fin(a)


def _t_wood():
    xx, yy = _grid()
    w = _fnoise(2.0, 17)
    rings = np.sin(2 * PI * (xx * 9 + 0.09 * w + 0.04 * _streak(18, 2, 30)))
    a = 0.5 + 0.14 * rings + 0.14 * _streak(19, 0.9, 40) + 0.05 * _fnoise(1.0, 20)
    return _fin(a)


def _t_plastic():
    r = 1 - np.abs(_fnoise(2.3, 21))
    a = 0.46 + 0.22 * np.clip(r, 0, 1) ** 3 + 0.05 * _fnoise(1.5, 22) + 0.02 * _fnoise(0.2, 23)
    # a faint printed pattern of rings (supermarket bag), still greyscale
    xx, yy = _grid()
    dd = np.sqrt(((xx * 4) % 1 - 0.5) ** 2 + ((yy * 4) % 1 - 0.5) ** 2)
    a *= 1.0 - 0.18 * (np.abs(dd - 0.28) < 0.035)
    return _fin(a)


def _t_cotton():
    """Babushka kerchief: ditsy flowers + leaves + dots on a woven ground (greyscale, tintable)."""
    from PIL import Image, ImageDraw
    S = N * 2
    im = Image.new("L", (S, S), 150)
    d = ImageDraw.Draw(im)
    r = np.random.default_rng(31)
    for _ in range(110):
        cx, cy = r.integers(0, S, 2)
        rad = int(r.integers(16, 28))
        for ox in (-S, 0, S):
            for oy in (-S, 0, S):
                x, y = cx + ox, cy + oy
                if x < -80 or y < -80 or x > S + 80 or y > S + 80:
                    continue
                for k in range(5):
                    a = 2 * PI * k / 5 + 0.4
                    px, py = x + math.cos(a) * rad * 0.62, y + math.sin(a) * rad * 0.62
                    d.ellipse([px - rad * .46, py - rad * .46, px + rad * .46, py + rad * .46], fill=70)
                d.ellipse([x - rad * .28, y - rad * .28, x + rad * .28, y + rad * .28], fill=235)
    for _ in range(260):
        cx, cy = r.integers(0, S, 2)
        for ox in (-S, 0, S):
            for oy in (-S, 0, S):
                x, y = cx + ox, cy + oy
                d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=100 if r.random() < .5 else 205)
    for _ in range(200):
        cx, cy = r.integers(0, S, 2)
        a = r.random() * 2 * PI
        for ox in (-S, 0, S):
            for oy in (-S, 0, S):
                x, y = cx + ox, cy + oy
                d.ellipse([x + math.cos(a) * 17 - 7, y + math.sin(a) * 17 - 4, x + math.cos(a) * 17 + 7,
                           y + math.sin(a) * 17 + 4], fill=110)
    im = im.resize((N, N), Image.LANCZOS)
    a = np.asarray(im, dtype=np.float64) / 255.0
    xx, yy = _grid()
    weave = 0.5 + 0.5 * np.cos(2 * PI * xx * 200) * np.cos(2 * PI * yy * 200)
    a = a * (0.9 + 0.1 * weave) + 0.03 * _fnoise(1.0, 32)
    return _fin(a)


def _t_metal():
    """Glasses: top part (v < .62 from top) brushed frame metal/plastic, bottom band dark lens."""
    xx, yy = _grid()
    a = 0.5 + 0.10 * _streak(41, 18, 0.8) + 0.04 * _fnoise(1.0, 42)
    frame = _fin(a, lo=0.25, hi=0.75)
    lens = 0.07 + 0.05 * (1 - np.abs(yy - 0.85) * 4) + 0.02 * _fnoise(1.0, 43)
    out = np.where(yy < 0.70, frame, lens)
    return out


CELLS = {  # uv-space cells (u0, v0, u1, v1) of the avoska atlas, v up
    "cord": (0.02, 0.52, 0.32, 0.98), "bread": (0.35, 0.52, 0.65, 0.98), "bottle": (0.68, 0.52, 0.98, 0.98),
    "cabbage": (0.02, 0.02, 0.32, 0.48), "apple": (0.35, 0.02, 0.65, 0.48), "foil": (0.68, 0.02, 0.98, 0.48)}


def _t_avoska():
    rgb = np.zeros((N, N, 3))
    rgb[:] = (0.5, 0.5, 0.5)
    xx, yy = _grid()
    yv = 1 - yy  # uv v

    def cell(name):
        u0, v0, u1, v1 = CELLS[name]
        m = (xx >= u0) & (xx < u1) & (yv >= v0) & (yv < v1)
        lu = np.clip((xx - u0) / (u1 - u0), 0, 1)
        lv = np.clip((yv - v0) / (v1 - v0), 0, 1)
        return m, lu, lv

    nz = _fnoise(1.0, 51)
    nz2 = _fnoise(2.2, 52)
    m, lu, lv = cell("cord")
    tw = 0.5 + 0.5 * np.sin(2 * PI * (lu * 10 + lv * 26))
    c = np.stack([0.80 + 0.12 * tw, 0.72 + 0.12 * tw, 0.52 + 0.10 * tw], -1) * (0.93 + 0.04 * nz[..., None])
    rgb[m] = c[m]
    m, lu, lv = cell("bread")
    cuts = np.clip(1 - np.abs(((lu * 4.2 + lv * 0.9) % 1) - 0.5) * 7, 0, 1) * (lv > 0.2) * (lv < 0.8)
    base = np.stack([0.58 + 0.12 * lv, 0.33 + 0.08 * lv, 0.13 + 0.04 * lv], -1)
    c = base * (0.88 + 0.10 * nz2[..., None]) + cuts[..., None] * np.array([0.25, 0.20, 0.10])
    rgb[m] = c[m]
    m, lu, lv = cell("bottle")
    glass = np.stack([0.10 + 0.05 * lv, 0.35 + 0.15 * np.sin(PI * lu) ** 2, 0.18 + 0.05 * lv], -1)
    label = ((lv > 0.28) & (lv < 0.62))[..., None]
    c = np.where(label, np.array([0.90, 0.92, 0.95]) * (0.95 + 0.04 * nz[..., None]), glass)
    c = np.where(((lv > 0.40) & (lv < 0.5))[..., None] & label, np.array([0.15, 0.30, 0.65]), c)
    c = np.where((lv > 0.92)[..., None], np.array([0.75, 0.76, 0.80]), c)
    rgb[m] = c[m]
    m, lu, lv = cell("cabbage")
    vein = np.abs(np.sin(2 * PI * (lu * 7 + 2 * np.sin(PI * lv * 3))))
    c = np.stack([0.38 + 0.25 * vein, 0.58 + 0.22 * vein, 0.22 + 0.15 * vein], -1) * (0.88 + 0.12 * nz[..., None])
    rgb[m] = c[m]
    m, lu, lv = cell("apple")
    c = np.stack([0.72 - 0.20 * lv + 0.1 * np.sin(PI * lu), 0.10 + 0.20 * (1 - lv) * np.sin(PI * lu), 0.07 + 0 * lu], -1)
    c = c * (0.90 + 0.10 * nz2[..., None])
    rgb[m] = c[m]
    m, lu, lv = cell("foil")
    c = np.stack([0.72 + 0.08 * nz2, 0.73 + 0.08 * nz2, 0.76 + 0.08 * nz2], -1)
    rgb[m] = c[m]
    return np.clip(rgb, 0, 1)


def _t_beard():
    """RGBA: vertical hair strands (greyscale detail, mean ~0.5 over opaque texels) with alpha."""
    from PIL import Image, ImageDraw
    S = N * 2
    col = Image.new("L", (S, S), 0)
    alp = Image.new("L", (S, S), 0)
    dc, da = ImageDraw.Draw(col), ImageDraw.Draw(alp)
    r = np.random.default_rng(61)
    # dense root layer (skin shadow / stubble) over the top third
    da.rectangle([0, 0, S, int(S * 0.34)], fill=255)
    dc.rectangle([0, 0, S, int(S * 0.34)], fill=96)
    for _ in range(1900):
        x0 = r.random() * S
        y0 = r.random() * S * 0.45
        ln = S * (0.30 + 0.62 * r.random() ** 0.8)
        y1 = min(S - 2, y0 + ln)
        wav = (r.random() - 0.5) * 38
        ph = r.random() * 6
        wd = int(r.integers(3, 7))
        g = int(np.clip(r.normal(128, 30), 40, 230))
        pts = []
        for k in range(14):
            t = k / 13.0
            pts.append((x0 + wav * math.sin(ph + t * 3.2) * t, y0 + (y1 - y0) * t))
        for ox in (-S, 0, S):
            pp = [(px + ox, py) for px, py in pts]
            dc.line(pp, fill=g, width=wd)
            da.line(pp, fill=255, width=wd)
    col = np.asarray(col.resize((N, N), Image.LANCZOS), dtype=np.float64) / 255.0
    alp = np.asarray(alp.resize((N, N), Image.LANCZOS), dtype=np.float64) / 255.0
    col = col * (0.9 + 0.1 * _fnoise(0.5, 62))
    # alpha: soften the cut-off so tips become ragged but keep strands crisp
    m = alp > 0.5
    col = _fin(col, mask=m, lo=0.05, hi=0.95)
    rgba = np.stack([col, col, col, alp], -1)
    return rgba


_TILE_FUNCS = {"knit": (_t_knit, True), "felt": (_t_felt, True), "straw": (_t_straw, True),
               "canvas": (_t_canvas, True), "leather": (_t_leather, True), "wood": (_t_wood, True),
               "plastic": (_t_plastic, True), "cotton": (_t_cotton, True), "metal": (_t_metal, True),
               "avoska": (_t_avoska, False), "beard": (_t_beard, True)}
TILE_TINTABLE = {k: v[1] for k, v in _TILE_FUNCS.items()}
_tile_cache = {}


def _tile_array(name):
    if name not in _tile_cache:
        _tile_cache[name] = _TILE_FUNCS[name][0]()
    return _tile_cache[name]


def make_tiles(out_dir):
    """Writes every tile as a 512x512 PNG into out_dir and returns {tile_name: path}."""
    from PIL import Image
    os.makedirs(out_dir, exist_ok=True)
    paths = {}
    for name in _TILE_FUNCS:
        a = _tile_array(name)
        if a.ndim == 2:
            img = Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), "L").convert("RGB")
        elif a.shape[2] == 4:
            img = Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")
        else:
            img = Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")
        p = os.path.join(out_dir, "acc_%s.png" % name)
        img.save(p)
        paths[name] = p
    return paths


# ----------------------------------------------------------------------------------------------
# body fitting
# ----------------------------------------------------------------------------------------------
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402


def _sm(a, b, x):
    t = np.clip((np.asarray(x, dtype=np.float64) - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def _unit(v):
    v = np.asarray(v, dtype=np.float64)
    n = np.linalg.norm(v, axis=-1, keepdims=True)
    return v / np.maximum(n, 1e-12)


def _dirv(az, el):
    """az 0 = front (-Y), + towards +X (character's left); el above horizon."""
    az, el = np.asarray(az, dtype=np.float64), np.asarray(el, dtype=np.float64)
    return np.stack([np.sin(az) * np.cos(el), -np.cos(az) * np.cos(el), np.sin(el) + 0 * az], -1)


_NOT_CLOTHES = ["high-poly", "eyebrow", "eyelash", "teeth", "tongue", "eyes", "short0", "long0", "bob0", "ponytail",
                "afro", "braid", "hair"]


class Fit:
    """Measures the evaluated (rest pose, shape keys on, masks off) body of one resident."""

    AZ_N, EL0, EL_N = 72, -60.0, 31   # head field: 5 deg bins, el -60..90

    def __init__(self, ctx):
        self.ctx = ctx
        bm, arm = ctx["basemesh"], ctx["armature"]
        self.bm, self.arm = bm, arm
        self.hair_volume = float(ctx.get("hair_volume", 0.0) or 0.0)
        self.sex = ctx.get("sex", "m")
        self.age = int(ctx.get("age", 30))
        masks = [(m, m.show_viewport, m.show_render) for m in bm.modifiers if m.type == "MASK"]
        pp = arm.data.pose_position
        for m, _, _ in masks:
            m.show_viewport = False
            m.show_render = False
        arm.data.pose_position = "REST"
        try:
            bpy.context.view_layer.update()
            dg = bpy.context.evaluated_depsgraph_get()
            ev = bm.evaluated_get(dg)
            me = ev.to_mesh()
            n = len(me.vertices)
            co = np.empty(n * 3)
            me.vertices.foreach_get("co", co)
            co = co.reshape(n, 3)
            mw = np.array(bm.matrix_world)
            co = co @ mw[:3, :3].T + mw[:3, 3]
            polys = [tuple(p.vertices) for p in me.polygons]
            ev.to_mesh_clear()
            cloth = []
            for o in arm.children_recursive:
                if o.type != "MESH" or o is bm or "kind" in o.keys():
                    continue
                if any(k in o.name.lower() for k in _NOT_CLOTHES):
                    continue
                e2 = o.evaluated_get(dg)
                m2 = e2.to_mesh()
                n2 = len(m2.vertices)
                c2 = np.empty(n2 * 3)
                m2.vertices.foreach_get("co", c2)
                c2 = c2.reshape(n2, 3)
                mw2 = np.array(o.matrix_world)
                c2 = c2 @ mw2[:3, :3].T + mw2[:3, 3]
                cloth.append((c2, [tuple(p.vertices) for p in m2.polygons]))
                e2.to_mesh_clear()
        finally:
            arm.data.pose_position = pp
            for m, a, b in masks:
                m.show_viewport, m.show_render = a, b
            bpy.context.view_layer.update()
        self.co = co

        def grp(name):
            g = bm.vertex_groups.get(name)
            ok = np.zeros(n, bool)
            if g is None:
                return ok
            for v in bm.data.vertices:
                if v.index < n:
                    for ge in v.groups:
                        if ge.group == g.index and ge.weight > 0:
                            ok[v.index] = True
                            break
            return ok
        body = grp("body")
        if body.sum() < 100:
            body[:] = True
        self.body = body
        polys = [p for p in polys if all(body[i] for i in p)]
        self.bvh = BVHTree.FromPolygons([tuple(c) for c in co.tolist()], polys)
        allv, allp, off = [co], list(polys), n
        for c2, p2 in cloth:
            allv.append(c2)
            allp.extend([tuple(i + off for i in pp) for pp in p2])
            off += len(c2)
        self.obvh = BVHTree.FromPolygons([tuple(c) for c in np.vstack(allv).tolist()], allp)   # body + clothes
        self.n_cloth = len(cloth)
        self.ears = co[grp("ears")]
        self.lips = co[grp("lips")]
        self.height = float(co[body][:, 2].max())
        self.bs = self.height / 1.75           # body scale
        # --- skeleton points
        self.bones = {}
        for b in arm.data.bones:
            self.bones[b.name] = (np.array(arm.matrix_world @ b.head_local), np.array(arm.matrix_world @ b.tail_local))
        # --- head centre (ear axis)
        ec = self.ears.mean(0) if len(self.ears) else np.array([0, -0.05, self.bones["head"][0][2] + 0.02])
        self.c = np.array([0.0, ec[1], ec[2]])
        self._head_field()
        self.hs = float(np.clip(self.R_side / 0.085, 0.55, 1.35))
        d = self.ears - self.c
        r = np.linalg.norm(d, axis=1)
        el = np.arcsin(np.clip(d[:, 2] / np.maximum(r, 1e-9), -1, 1))
        self.ear_el = float(el.max()) if len(el) else 0.3
        self.ear_el_min = float(el.min()) if len(el) else -0.3
        self.ear_az = float(np.abs(np.arctan2(d[:, 0], -d[:, 1])).mean()) if len(el) else 1.5
        self.chin = self.bones["jaw"][1].copy()
        if len(self.lips):
            self.lip_top, self.lip_bot = float(self.lips[:, 2].max()), float(self.lips[:, 2].min())
            self.lip_hw = float(np.abs(self.lips[:, 0]).max())
            self.lip_y = float(self.lips[:, 1].min())
        else:
            self.lip_top, self.lip_bot, self.lip_hw, self.lip_y = self.chin[2] + 0.05, self.chin[2] + 0.03, 0.028, -0.13
        self.top_z = float(self.c[2] + self.field_R[:, -1].mean())

    # ---- rays -------------------------------------------------------------------------------
    def ray(self, o, d, maxd=2.0, outer=False):
        loc, nrm, idx, dist = (self.obvh if outer else self.bvh).ray_cast(Vector(o), Vector(d), maxd)
        if loc is None:
            return None
        return np.array(loc), np.array(nrm), dist

    def nearest(self, p, maxd=0.5, outer=False):
        loc, nrm, idx, dist = (self.obvh if outer else self.bvh).find_nearest(Vector(p), maxd)
        if loc is None:
            return None
        return np.array(loc), np.array(nrm), dist

    def bone(self, name, tail=False):
        return self.bones[name][1 if tail else 0]

    # ---- head field -------------------------------------------------------------------------
    def _head_field(self):
        n_az, n_el = self.AZ_N, self.EL_N
        raw = np.full((n_az, n_el), np.nan)
        for i in range(n_az):
            az = -PI + 2 * PI * i / n_az
            for j in range(n_el):
                el = math.radians(self.EL0 + 5.0 * j)
                h = self.ray(self.c, _dirv(az, el), 0.6)
                if h is not None:
                    raw[i, j] = h[2]
        fill = np.nanmean(raw)
        raw = np.where(np.isnan(raw), fill, raw)
        # ears hang outside the ray hits (rays leave through the skull): splat their vertices
        d = self.ears - self.c
        if len(d):
            r = np.linalg.norm(d, axis=1)
            az = np.arctan2(d[:, 0], -d[:, 1])
            el = np.arcsin(np.clip(d[:, 2] / np.maximum(r, 1e-9), -1, 1))
            for a, e, rr in zip(az, el, r):
                i = int(round((a + PI) / (2 * PI) * n_az)) % n_az
                j = int(round((math.degrees(e) - self.EL0) / 5.0))
                if 0 <= j < n_el:
                    raw[i, j] = max(raw[i, j], rr)
        self.field_raw = raw
        pad = np.concatenate([raw[:, :1], raw, raw[:, -1:]], 1)
        mx = raw.copy()
        for di in (-1, 0, 1):
            for dj in (0, 1, 2):
                mx = np.maximum(mx, np.roll(pad, di, axis=0)[:, dj:dj + n_el])
        sm = mx
        for _ in range(2):
            left = np.concatenate([sm[:, :1], sm[:, :-1]], 1)
            right = np.concatenate([sm[:, 1:], sm[:, -1:]], 1)
            sm = 0.5 * sm + 0.25 * left + 0.25 * right
            sm = 0.5 * sm + 0.25 * np.roll(sm, 1, 0) + 0.25 * np.roll(sm, -1, 0)
        self.field_R = np.maximum(sm, raw + 0.0015)
        # top row must be one value
        self.field_R[:, -1] = self.field_R[:, -1].mean()
        jm = int(round((0 - self.EL0) / 5.0))
        self.R_side = float(0.5 * (self.field_R[int(n_az * 0.75), jm] + self.field_R[int(n_az * 0.25), jm]))

    def R(self, az, el):
        """Smoothed head radius from the centre for az/el arrays (radians)."""
        az = np.asarray(az, dtype=np.float64)
        el = np.asarray(el, dtype=np.float64)
        fa = ((az + PI) / (2 * PI) * self.AZ_N) % self.AZ_N
        fe = np.clip((np.degrees(el) - self.EL0) / 5.0, 0, self.EL_N - 1.0001)
        i0 = np.floor(fa).astype(int)
        ta = fa - i0
        i1 = (i0 + 1) % self.AZ_N
        j0 = np.floor(fe).astype(int)
        te = fe - j0
        F = self.field_R
        return ((F[i0, j0] * (1 - ta) + F[i1, j0] * ta) * (1 - te) + (F[i0, j0 + 1] * (1 - ta) + F[i1, j0 + 1] * ta) * te)

    def hv(self, el):
        """How far hair stands off the scalp at this elevation."""
        return self.hair_volume * _sm(math.radians(-2), math.radians(38), el)

    def HP(self, az, el, off=0.0, hair=True):
        az, el = np.asarray(az, dtype=np.float64), np.asarray(el, dtype=np.float64)
        r = self.R(az, el) + off + (self.hv(el) if hair else 0.0)
        return self.c + _dirv(az, el) * r[..., None]

    # ---- torso queries ----------------------------------------------------------------------
    def front(self, x, z, off=0.0):
        h = self.ray((x, -1.2, z), (0, 1, 0), outer=True)
        if h is None:
            return None
        p, n, _ = h
        if n[1] > 0:
            n = -n
        return p + n * off, n

    def back(self, x, z, off=0.0):
        h = self.ray((x, 1.2, z), (0, -1, 0), outer=True)
        if h is None:
            return None
        p, n, _ = h
        if n[1] < 0:
            n = -n
        return p + n * off, n

    def top(self, x, y, off=0.0):
        h = self.ray((x, y, 2.4), (0, 0, -1), outer=True)
        if h is None:
            return None
        p, n, _ = h
        if n[2] < 0:
            n = -n
        return p + n * off, n

    def side(self, y, z, sgn, off=0.0):
        h = self.ray((sgn * 1.2, y, z), (-sgn, 0, 0), outer=True)
        if h is None:
            return None
        p, n, _ = h
        if n[0] * sgn < 0:
            n = -n
        return p + n * off, n

    def outline(self, z, n=40, y0=0.0, maxr=0.6):
        """Radial distances of the torso/neck section at height z (inside-out rays from the axis)."""
        cy = y0
        for _ in range(2):
            pts = []
            for k in range(8):
                a = 2 * PI * k / 8
                h = self.ray((0, cy, z), (math.sin(a), -math.cos(a), 0), maxr, outer=True)
                if h is not None:
                    pts.append(h[0])
            if pts:
                pts = np.array(pts)
                cy = 0.5 * (pts[:, 1].min() + pts[:, 1].max())
        out = np.zeros(n + 1)
        for k in range(n + 1):
            a = -PI + 2 * PI * k / n
            h = self.ray((0, cy, z), (math.sin(a), -math.cos(a), 0), maxr, outer=True)
            out[k] = h[2] if h is not None else (out[k - 1] if k else 0.1)
        return cy, out

    def push_out(self, P, min_d=0.003, iters=2, outer=False):
        """Moves points that are inside / closer than min_d to the body out along the surface normal."""
        P = np.array(P, dtype=np.float64)
        for _ in range(iters):
            for i in range(len(P)):
                r = self.nearest(P[i], 0.08, outer)
                if r is None:
                    continue
                loc, nrm, dist = r
                inside = np.dot(P[i] - loc, nrm) < 0
                if inside or dist < min_d:
                    P[i] = loc + nrm * min_d
        return P

    def min_clearance(self, P, step=1):  # body only
        m = 9.0
        for p in np.asarray(P)[::step]:
            r = self.nearest(p, 0.3)
            if r is None:
                continue
            loc, nrm, dist = r
            d = dist if np.dot(p - loc, nrm) >= 0 else -dist
            m = min(m, d)
        return m


# ----------------------------------------------------------------------------------------------
# mesh builder
# ----------------------------------------------------------------------------------------------
TILE_M = {"knit": 0.64, "felt": 0.40, "straw": 0.50, "canvas": 0.45, "leather": 0.30, "wood": 0.35,
          "plastic": 0.50, "cotton": 0.50, "metal": 0.10, "avoska": 0.2, "beard": 0.30}


class MB:
    """Accumulates verts / faces / per-corner UVs / per-vertex bone weights."""

    def __init__(self):
        self.P, self.W, self.F, self.UV = [], [], [], []

    @staticmethod
    def _area_normal(pts):
        pts = np.asarray(pts)
        n = np.zeros(3)
        for i in range(1, len(pts) - 1):
            n += np.cross(pts[i] - pts[0], pts[i + 1] - pts[0])
        return n

    def grid(self, P, uv=None, w=None, out="ring", tri=None, skip=None, flip=False):
        """Quad grid P (R,C,3) (rows x cols). uv (R,C,2) or tri=(centre, tile_m) for per-face triplanar UVs.
        out: 'ring' (normals away from each ring's centre), a vector, or callable(centroid)->vector."""
        P = np.asarray(P, dtype=np.float64)
        R, C = P.shape[:2]
        base = len(self.P)
        flat = P.reshape(-1, 3)
        self.P.extend(flat.tolist())
        self.W.extend(w(flat) if callable(w) else [w] * len(flat))
        quads = []
        for i in range(R - 1):
            for j in range(C - 1):
                if skip is not None and skip(i, j):
                    continue
                idx = [base + i * C + j, base + i * C + j + 1, base + (i + 1) * C + j + 1, base + (i + 1) * C + j]
                pts = [P[i, j], P[i, j + 1], P[i + 1, j + 1], P[i + 1, j]]
                uvs = None if uv is None else [uv[i, j], uv[i, j + 1], uv[i + 1, j + 1], uv[i + 1, j]]
                keep = []
                for k in range(4):
                    if np.linalg.norm(pts[k] - pts[(k + 1) % 4]) > 1e-7:
                        keep.append(k)
                # drop corners equal to the previous corner (poles)
                if len(keep) < 3:
                    continue
                kk = [k for k in range(4) if np.linalg.norm(pts[k] - pts[(k - 1) % 4]) > 1e-7]
                if len(kk) < 3:
                    continue
                cen = np.mean([pts[k] for k in kk], 0)
                nrm = self._area_normal([pts[k] for k in kk])
                if out == "ring":
                    o = cen - 0.5 * (P[i].mean(0) + P[i + 1].mean(0))
                elif callable(out):
                    o = np.asarray(out(cen))
                else:
                    o = np.asarray(out)
                quads.append((kk, idx, uvs, pts, cen, nrm, o))
        sgn = sum(float(np.dot(q[5], q[6])) for q in quads)
        rev = (sgn < 0) != bool(flip)
        for kk, idx, uvs, pts, cen, nrm, o in quads:
            order = list(kk)[::-1] if rev else list(kk)
            self.F.append([idx[k] for k in order])
            if tri is not None:
                n = -nrm if rev else nrm
                self.UV.append(self._tri_uv([pts[k] for k in order], n, tri))
            elif uvs is not None:
                self.UV.append([tuple(uvs[k]) for k in order])
            else:
                self.UV.append([(0.5, 0.5)] * len(order))

    @staticmethod
    def _tri_uv(pts, n, tri):
        c, tm = tri
        a = int(np.argmax(np.abs(n)))
        ax = [i for i in range(3) if i != a]
        s = -1.0 if n[a] < 0 else 1.0
        return [(0.5 + s * (p[ax[0]] - c[ax[0]]) / tm * (1 if a != 1 else 1), 0.5 + (p[ax[1]] - c[ax[1]]) / tm) for p in pts]

    def mesh_arrays(self):
        return np.array(self.P)


def polar_uv(P, tile_m, v_sign=1.0):
    """UV for a ring-grid P (R,C,3): u around, v = arc length down the meridian, in tile units."""
    R, C = P.shape[:2]
    seg = np.linalg.norm(P[1:] - P[:-1], axis=2).mean(1)
    v = np.concatenate([[0], np.cumsum(seg)]) / tile_m
    circ = max(np.linalg.norm(P[i, 1:] - P[i, :-1], axis=1).sum() for i in range(R))
    u = np.linspace(0, 1, C) * circ / tile_m
    uv = np.zeros((R, C, 2))
    uv[..., 0] = u[None, :]
    uv[..., 1] = v[:, None]
    return uv


def sweep_grid(path, nrm, w, h, nsides=4, closed=False):
    """Section frames along a path: returns P (R, nsides+1, 3) and ring arc lengths."""
    path = np.asarray(path, dtype=np.float64)
    nrm = _unit(np.asarray(nrm, dtype=np.float64))
    n = len(path)
    if closed:
        t = np.roll(path, -1, 0) - np.roll(path, 1, 0)
    else:
        t = np.gradient(path, axis=0)
    t = _unit(t)
    side = _unit(np.cross(t, nrm))
    nn = _unit(np.cross(side, t))
    if np.isscalar(w):
        w = np.full(n, float(w))
    if np.isscalar(h):
        h = np.full(n, float(h))
    if nsides == 4:
        sec = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    else:
        sec = [(math.cos(2 * PI * k / nsides), math.sin(2 * PI * k / nsides)) for k in range(nsides)]
    sec = sec + [sec[0]]
    P = np.zeros((n, len(sec), 3))
    for k, (a, b) in enumerate(sec):
        P[:, k] = path + side * (a * w[:, None] / 2) + nn * (b * h[:, None] / 2)
    return P


def sweep_uv(P, tile_m, region=None):
    R, C = P.shape[:2]
    seg = np.linalg.norm(P[1:] - P[:-1], axis=2).mean(1)
    v = np.concatenate([[0], np.cumsum(seg)]) / tile_m
    per = np.linalg.norm(P[0, 1:] - P[0, :-1], axis=1).sum() / tile_m
    uv = np.zeros((R, C, 2))
    uv[..., 0] = (np.linspace(0, 1, C) * per)[None, :]
    uv[..., 1] = v[:, None]
    return uv


def catmull(pts, n_per=4, closed=False):
    pts = np.asarray(pts, dtype=np.float64)
    m = len(pts)
    out = []
    rng = range(m) if closed else range(m - 1)
    for i in rng:
        p0 = pts[(i - 1) % m] if (closed or i > 0) else pts[i]
        p1 = pts[i]
        p2 = pts[(i + 1) % m]
        p3 = pts[(i + 2) % m] if (closed or i + 2 < m) else pts[(i + 1) % m]
        for k in range(n_per):
            t = k / n_per
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    if not closed:
        out.append(pts[-1])
    return np.array(out)


def superquad(c, ax, ay, az, e1, e2, nphi, nth, rot=None):
    phi = np.linspace(-PI / 2, PI / 2, nphi + 1)
    th = np.linspace(-PI, PI, nth + 1)

    def sp(x, e):
        return np.sign(x) * np.abs(x) ** e
    cp, spp = sp(np.cos(phi), e1)[:, None], sp(np.sin(phi), e1)[:, None]
    ct, st = sp(np.cos(th), e2)[None], sp(np.sin(th), e2)[None]
    X = ax * cp * ct
    Y = ay * cp * st
    Z = az * spp * np.ones_like(ct)
    P = np.stack([X, Y, Z], -1)
    P[0] = P[0].mean(0)
    P[-1] = P[-1].mean(0)
    if rot is not None:
        P = P @ np.asarray(rot).T
    return P + np.asarray(c)


def _rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])


# ---- weights -----------------------------------------------------------------------------------
def W1(bone):
    return {bone: 1.0}


def _wblend(a, b, t):
    t = float(np.clip(t, 0, 1))
    d = {}
    if 1 - t > 1e-4:
        d[a] = 1 - t
    if t > 1e-4:
        d[b] = t
    return d


# ----------------------------------------------------------------------------------------------
# object + material creation
# ----------------------------------------------------------------------------------------------
DEFAULT_TINTS = {
    "flat_cap": [(0.30, 0.28, 0.25), (0.20, 0.22, 0.26), (0.42, 0.36, 0.28), (0.15, 0.15, 0.16)],
    "knit_beanie": [(0.55, 0.12, 0.10), (0.15, 0.25, 0.45), (0.20, 0.20, 0.22), (0.85, 0.78, 0.60)],
    "baseball_cap": [(0.10, 0.25, 0.55), (0.65, 0.10, 0.10), (0.12, 0.12, 0.12), (0.90, 0.90, 0.88)],
    "sun_hat": [(0.80, 0.65, 0.36), (0.88, 0.80, 0.58), (0.92, 0.88, 0.76)],
    "headscarf": [(0.62, 0.14, 0.14), (0.20, 0.30, 0.60), (0.80, 0.70, 0.30), (0.25, 0.45, 0.30), (0.85, 0.85, 0.85)],
    "chef_toque": [(0.96, 0.96, 0.94)],
    "backpack": [(0.12, 0.25, 0.60), (0.65, 0.12, 0.15), (0.12, 0.45, 0.30), (0.15, 0.15, 0.17), (0.80, 0.45, 0.10)],
    "shoulder_bag": [(0.33, 0.22, 0.14), (0.15, 0.15, 0.15), (0.35, 0.40, 0.25)],
    "string_bag": [(1.0, 1.0, 1.0)],
    "shopping_bag": [(0.92, 0.92, 0.92), (0.95, 0.80, 0.15), (0.20, 0.45, 0.85)],
    "scarf": [(0.60, 0.12, 0.12), (0.15, 0.30, 0.50), (0.85, 0.85, 0.80), (0.25, 0.25, 0.28)],
    "waist_apron": [(0.10, 0.10, 0.11), (0.14, 0.14, 0.2)],
    "bib_apron": [(0.95, 0.95, 0.93), (0.20, 0.30, 0.45), (0.45, 0.20, 0.15)],
    "glasses": [(0.10, 0.09, 0.08), (0.45, 0.30, 0.15), (0.75, 0.60, 0.25), (0.55, 0.57, 0.60)],
    "cane": [(0.42, 0.26, 0.12), (0.22, 0.15, 0.10)],
    "mustache": [(0.14, 0.10, 0.07), (0.35, 0.22, 0.12), (0.55, 0.55, 0.55), (0.75, 0.70, 0.60)],
    "short_beard": [(0.14, 0.10, 0.07), (0.35, 0.22, 0.12), (0.55, 0.55, 0.55)],
    "full_beard": [(0.14, 0.10, 0.07), (0.35, 0.22, 0.12), (0.62, 0.62, 0.62), (0.78, 0.74, 0.68)],
}
TINT_SLOT = {"flat_cap": "hat", "knit_beanie": "hat", "baseball_cap": "hat", "sun_hat": "hat", "headscarf": "hat",
             "chef_toque": "hat", "backpack": "bag", "shoulder_bag": "bag", "string_bag": "bag",
             "shopping_bag": "bag", "scarf": "neck", "waist_apron": "apron", "bib_apron": "apron",
             "glasses": "glasses", "cane": "cane", "mustache": "facial_hair", "short_beard": "facial_hair",
             "full_beard": "facial_hair"}

_HEAD_KINDS = {"flat_cap", "knit_beanie", "baseball_cap", "sun_hat", "headscarf", "chef_toque", "glasses", "mustache",
               "short_beard", "full_beard"}
_img_cache = {}
_tile_dir = None
_tiles_written = []


def _tile_image(tile):
    global _tile_dir
    im = _img_cache.get(tile)
    if im is not None and im.name in bpy.data.images:
        return im
    if _tile_dir is None:
        _tile_dir = os.path.join(tempfile.gettempdir(), "lyud_acc_tiles")
    path = os.path.join(_tile_dir, "acc_%s.png" % tile)
    if not _tiles_written:
        make_tiles(_tile_dir)
        _tiles_written.append(1)
    im = bpy.data.images.load(path, check_existing=True)
    im.colorspace_settings.name = "sRGB"
    im.alpha_mode = "CHANNEL_PACKED"
    _img_cache[tile] = im
    return im


def _material(matname, tile, tintable, tint, rough):
    mat = bpy.data.materials.new(matname)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = _tile_image(tile)
    tex.interpolation = "Linear"
    tex.extension = "EXTEND"
    bsdf.inputs["Roughness"].default_value = rough
    if tintable:
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs[0].default_value = 1.0
        mix.inputs[7].default_value = (tint[0] * 2, tint[1] * 2, tint[2] * 2, 1.0)
        nt.links.new(tex.outputs["Color"], mix.inputs[6])
        nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    else:
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    if matname.startswith("Hair"):
        nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
        try:
            mat.surface_render_method = "DITHERED"
        except Exception:
            pass
        try:
            mat.blend_method = "CLIP"
            mat.alpha_threshold = 0.5
        except Exception:
            pass
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    mat.use_backface_culling = False
    mat["tile"] = tile
    return mat


def _welded_normals(P, F):
    P = np.asarray(P)
    key = np.round(P / 1e-5).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    acc = np.zeros((inv.max() + 1, 3))
    for f in F:
        pts = P[f]
        n = np.zeros(3)
        for i in range(1, len(f) - 1):
            n += np.cross(pts[i] - pts[0], pts[i + 1] - pts[0])
        for v in f:
            acc[inv[v]] += n
    nn = _unit(acc)
    return nn[inv]


def _finish(mb, ctx_fit, kind, name, tile, tintable, matname, params, rough=0.8, uv_fit=True, push=None):
    P = np.array(mb.P)
    if push:
        P = ctx_fit.push_out(P, push, 2, outer=kind not in _HEAD_KINDS)
        if kind not in _HEAD_KINDS:
            P = ctx_fit.push_out(P, min(push, 0.0015), 1, outer=False)
    me = bpy.data.meshes.new(name)
    me.from_pydata(P.tolist(), [], mb.F)
    me.update()
    # UVs
    uvl = me.uv_layers.new(name="UVMap")
    flat = []
    for poly, uvs in zip(me.polygons, mb.UV):
        flat.extend([c for uv in uvs for c in uv])
    uv = np.array(flat).reshape(-1, 2)
    if uv_fit and len(uv):
        lo, hi = uv.min(0), uv.max(0)
        span = hi - lo
        s = min(1.0, 0.97 / max(span.max(), 1e-9))
        uv = (uv - lo) * s + 0.015
        if span.max() * s < 0.97:
            pass
    uv = np.clip(uv, 0.0, 1.0)
    uvl.data.foreach_set("uv", uv.reshape(-1))
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    try:
        me.normals_split_custom_set_from_vertices(_welded_normals(P, mb.F).tolist())
    except Exception:
        pass
    obj = bpy.data.objects.new(name, me)
    fit = ctx_fit
    coll = fit.bm.users_collection[0] if fit.bm.users_collection else bpy.context.scene.collection
    coll.objects.link(obj)
    # weights
    names = {}
    for i, wd in enumerate(mb.W):
        for b, wv in wd.items():
            names.setdefault(b, []).append((i, wv))
    for b, lst in names.items():
        vg = obj.vertex_groups.new(name=b)
        for i, wv in lst:
            vg.add([i], wv, "REPLACE")
    arm = fit.arm
    obj.parent = arm
    obj.matrix_parent_inverse = arm.matrix_world.inverted()
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    # material
    tint = params.get("tint") or _pick_tint(kind, params)
    mat = _material(matname, tile, tintable, tint, rough)
    obj.data.materials.append(mat)
    obj["tile"] = tile
    obj["tint_slot"] = TINT_SLOT[kind]
    obj["tintable"] = bool(tintable)
    obj["kind"] = kind
    obj["material_name"] = matname
    obj["tint_rgb"] = [float(c) for c in tint]
    obj["tris"] = int(sum(len(f) - 2 for f in mb.F))
    return obj


def _pick_tint(kind, params):
    pal = DEFAULT_TINTS[kind]
    seed = params.get("seed")
    if seed is None:
        return pal[0]
    return pal[int(seed) % len(pal)]


# ----------------------------------------------------------------------------------------------
# head wear
# ----------------------------------------------------------------------------------------------
D = math.radians
ONE = 1.0


def dome_rows(F, rim_fn, off_fn, n_az=28, n_el=8, lip=0.003, top_el=PI / 2, ease=1.0):
    """Rows of a dome around the head centre from top_el down to rim_fn(az), then an inward lip."""
    az = -PI + 2 * PI * np.arange(n_az + 1) / n_az
    rim = rim_fn(az)
    rows = []
    for k in range(n_el + 1):
        t = k / n_el
        el = top_el + (rim - top_el) * t ** ease
        rows.append(F.HP(az, el, off_fn(az, el, t)))
    if top_el >= PI / 2 - 1e-6:
        rows[0][:] = rows[0].mean(0)
    if lip:
        o = off_fn(az, rim, 1.0)
        rows.append(F.HP(az, rim, o - lip))
        rows.append(F.HP(az, rim + 0.14, o - lip))
    return np.array(rows), az


def _ear_aware(F, r, a, margin=0.07):
    """Keep a rim above the ears (sides) when r (radians) would cut them."""
    ear = F.ear_el + margin
    wt = _sm(D(40), D(65), a) * (1 - _sm(D(125), D(150), a))
    return r * (1 - wt) + np.maximum(r, ear) * wt


def _brim(mb, F, rim_ring, az, L_fn, droop, thick=0.0035, az_lim=None, rows=4, side_droop=0.0, up=0.0):
    """Slab brim hanging off a rim ring (n+1,3): top sheet, bottom sheet, edge."""
    cc = rim_ring[:-1].mean(0)
    hdir = rim_ring - cc
    hdir[:, 2] = 0
    hdir = _unit(hdir)
    L = L_fn(az)
    sel = np.arange(len(az))
    if az_lim is not None:
        sel = np.where(np.abs(az) <= az_lim)[0]
    s = np.linspace(0, 1, rows + 1)[:, None]
    base = rim_ring[sel][None]
    d = hdir[sel][None]
    La = L[sel][None]
    drop = droop * s ** 2 + side_droop * (np.abs(az[sel])[None] / (az_lim or PI)) ** 2 * s
    top = base + d * s[..., None] * La[..., None] + np.array([0, 0, 1.0]) * (up * s - drop)[..., None] + np.array([0, 0, thick / 2])
    bot = top - np.array([0, 0, thick])
    w = W1("head")
    mb.grid(top, None, w, out=(0, 0, 1), tri=(F.c, TILE_M[mb.tile]))
    mb.grid(bot, None, w, out=(0, 0, -1), tri=(F.c, TILE_M[mb.tile]))
    edge = np.stack([top[-1], bot[-1]], 0)
    mb.grid(edge, None, w, out=lambda cen: np.array([cen[0] - cc[0], cen[1] - cc[1], 0]), tri=(F.c, TILE_M[mb.tile]))
    if az_lim is not None:   # close the two short ends
        for e in (0, -1):
            cap = np.stack([top[:, e], bot[:, e]], 1)
            mb.grid(cap, None, w, out=lambda cen, e=e: np.array([cen[0] - cc[0], cen[1] - cc[1], 0]), tri=(F.c, TILE_M[mb.tile]))


def _hat_obj(mb, F, kind, p, tile, name=None, rough=0.9, tint=None, uv_fit=True):
    return _finish(mb, F, kind, name or kind, tile, TILE_TINTABLE[tile], "Cloth", p, rough, uv_fit, push=0.0015)


def _dome_mesh(mb, F, rows, tile, w=None):
    mb.grid(rows, polar_uv(rows, TILE_M[tile]), w or W1("head"), out="ring")


def _knob(mb, F, centre, radii, tile, e=1.0, n=(7, 12)):
    P = superquad(centre, radii[0], radii[1], radii[2], e, e, n[0], n[1])
    mb.grid(P, None, W1("head"), out="ring", tri=(centre, TILE_M[tile] * 0.6))


def b_flat_cap(F, p):
    hs = F.hs
    base = 0.0065 + 0.003 * hs
    mb = MB()
    mb.tile = "felt"

    def rim(az):
        a = np.abs(az)
        r = D(9) + D(17) * _sm(D(140), D(30), a)
        return _ear_aware(F, r, a)

    def off(az, el, t):
        front = 0.5 + 0.5 * np.cos(az)
        bulge = 0.022 * hs * np.sin(PI * np.clip(t, 0, 1) ** 0.75) ** 0.9 * (0.35 + 0.65 * front)
        return base + bulge
    rows, az = dome_rows(F, rim, off, 28, 8)
    _dome_mesh(mb, F, rows, "felt")
    ring = rows[-3]
    _brim(mb, F, ring, az, lambda a: 0.052 * hs * np.cos(np.clip(a / D(58), -1, 1) * PI / 2) ** 0.7, 0.010 * hs, 0.0035,
          az_lim=D(58), rows=3, side_droop=0.006)
    top = rows[0][0]
    _knob(mb, F, top + np.array([0, 0, 0.002]), (0.007, 0.007, 0.0045), "felt", n=(5, 8))
    return _hat_obj(mb, F, "flat_cap", p, "felt")


def b_baseball_cap(F, p):
    hs = F.hs
    base = 0.0075 + 0.003 * hs
    mb = MB()
    mb.tile = "canvas"

    def rim(az):
        a = np.abs(az)
        r = D(24) - D(16) * _sm(D(30), D(150), a)
        return _ear_aware(F, r, a, 0.06)

    def off(az, el, t):
        front = 0.5 + 0.5 * np.cos(az)
        return base + 0.013 * hs * np.sin(PI * np.clip(t, 0, 1) ** 0.9) * (0.6 + 0.4 * front)
    rows, az = dome_rows(F, rim, off, 28, 8)
    _dome_mesh(mb, F, rows, "canvas")
    ring = rows[-3]
    _brim(mb, F, ring, az, lambda a: 0.078 * hs * np.cos(np.clip(a / D(52), -1, 1) * PI / 2) ** 0.55, 0.012 * hs, 0.004,
          az_lim=D(52), rows=4, side_droop=0.026 * hs)
    _knob(mb, F, rows[0][0] + np.array([0, 0, 0.003]), (0.009, 0.009, 0.006), "canvas", n=(5, 8))
    # back adjuster strap
    ia = len(az) // 2
    return _hat_obj(mb, F, "baseball_cap", p, "canvas")


def b_knit_beanie(F, p):
    hs = F.hs
    base = 0.010 + 0.004 * hs
    mb = MB()
    mb.tile = "knit"
    side_rim = F.ear_el_min - 0.04

    def rim(az):
        a = np.abs(az)
        return np.interp(a, [0, D(35), D(80), D(115), D(160), PI], [D(27), D(27), side_rim, side_rim + D(8), D(-3), D(-4)])
    cuff = p.get("cuff", 0.013)

    def off(az, el, t):
        r = rim(az)
        return base + cuff * (1 - _sm(r + D(13), r + D(16), el))
    rows, az = dome_rows(F, rim, off, 28, 9, lip=0.0045)
    _dome_mesh(mb, F, rows, "knit")
    if p.get("pompom", False):
        top = rows[0][0]
        _knob(mb, F, top + np.array([0, 0, 0.018 * hs]), (0.03 * hs,) * 3, "knit", n=(7, 12))
    return _hat_obj(mb, F, "knit_beanie", p, "knit", rough=0.95)


def b_sun_hat(F, p):
    hs = F.hs
    base = 0.0085 + 0.003 * hs
    mb = MB()
    mb.tile = "straw"
    rim_el = D(33)

    def rim(az):
        return np.full_like(az, rim_el)

    def off(az, el, t):
        lift = 0.030 * hs * _sm(D(40), D(88), el)
        band = 0.0045 * np.exp(-((el - rim_el - D(9)) / D(5)) ** 2)
        return base + lift + band
    rows, az = dome_rows(F, rim, off, 28, 8, lip=0.0035)
    _dome_mesh(mb, F, rows, "straw")
    ring = rows[-3]
    brim = p.get("brim", 0.085) * (0.8 + 0.2 * hs)
    _brim(mb, F, ring, az, lambda a: brim + 0.004 * np.sin(3 * a + 0.7), 0.020 * hs, 0.004, az_lim=None, rows=4, up=0.006)
    return _hat_obj(mb, F, "sun_hat", p, "straw")


def b_chef_toque(F, p):
    hs = F.hs
    mb = MB()
    mb.tile = "canvas"
    base = 0.010 + 0.003 * hs
    n_az = 32
    az = -PI + 2 * PI * np.arange(n_az + 1) / n_az
    a_ = np.abs(az)
    rim = _ear_aware(F, D(21) - D(10) * _sm(D(50), D(150), a_), a_, 0.06)
    B = F.HP(az, rim, base)
    ax = np.array([F.c[0], F.c[1]])
    k = 0.8 + 0.2 * hs
    prof = [(0.0, 1.0), (0.018, 1.02), (0.040, 1.04), (0.065, 1.16), (0.095, 1.30), (0.125, 1.38), (0.158, 1.41),
            (0.174, 1.37), (0.187, 1.27), (0.196, 1.09), (0.201, 0.80)]
    rows = []
    for h, sc in prof:
        s = sc * (1 + 0.04 * min(1, h / 0.12) * np.cos(18 * az))
        P = B.copy()
        P[:, :2] = ax + (B[:, :2] - ax) * s[:, None]
        P[:, 2] = B[:, 2] + h * k
        rows.append(P)
    rows = rows[::-1]
    top = rows[0].mean(0)
    top[2] += 0.004
    rows = [np.repeat(top[None], len(az), 0)] + rows
    lip1 = B.copy()
    lip1[:, :2] = ax + (B[:, :2] - ax) * 0.985
    lip2 = lip1.copy()
    lip2[:, 2] += 0.03
    rows = np.array(rows + [lip1, lip2])
    mb.grid(rows, polar_uv(rows, TILE_M["canvas"] * 0.9), W1("head"), out="ring")
    return _hat_obj(mb, F, "chef_toque", p, "canvas", rough=0.9)


def b_headscarf(F, p):
    hs = F.hs
    base = 0.0075 + 0.003 * hs
    mb = MB()
    mb.tile = "cotton"

    def rim(az):
        a = np.abs(az)
        front, cheek = D(36), -D(34)
        r = front + (cheek - front) * _sm(D(30), D(72), a)
        back = cheek + (-D(30) - cheek) * _sm(D(72), D(180), a)
        return np.where(a >= D(72), back, r)

    def off(az, el, t):
        return base + 0.005 * hs * np.sin(PI * np.clip(t, 0, 1))
    rows, az = dome_rows(F, rim, off, 36, 10, lip=0.0025)
    tile_m = TILE_M["cotton"]
    P = rows
    # hybrid UVs (planar on top, cylindrical on the sides) so the print is not pinched at the crown
    R, C = P.shape[:2]
    uv = np.zeros((R, C, 2))
    Rref = float(F.R_side + base)
    for i in range(R):
        for j in range(C):
            d = P[i, j] - F.c
            el = math.asin(np.clip(d[2] / max(np.linalg.norm(d), 1e-9), -1, 1))
            a = az[j]
            ub = 0.5 + a * Rref / tile_m * 0.62
            vb = 0.5 + d[2] / tile_m
            ut = 0.5 + d[0] / tile_m
            vt = 0.5 + d[1] / tile_m
            k = float(_sm(D(48), D(72), el))
            uv[i, j] = (ub * (1 - k) + ut * k, vb * (1 - k) + vt * k)
    mb.grid(P, uv, W1("head"), out="ring")
    # chin strap + knot + tails
    iL = int(round((D(72) + PI) / (2 * PI) * 36))
    az_l = D(72)
    Lp = F.HP(np.array(az_l), np.array(-D(34)), base)
    Rp = F.HP(np.array(-az_l), np.array(-D(34)), base)
    ch = F.chin
    zc = ch[2] - 0.010
    way = [Lp, np.array([0.55 * Lp[0], 0.5 * (Lp[1] + ch[1]), min(Lp[2], zc) ]),
           np.array([0.18 * Lp[0], ch[1] + 0.022, zc]),
           np.array([0.0, ch[1] + 0.022, zc]),
           np.array([-0.18 * Lp[0], ch[1] + 0.022, zc]),
           np.array([-0.55 * Lp[0], 0.5 * (Lp[1] + ch[1]), min(Lp[2], zc)]), Rp]
    path = catmull(way, 3)
    nrm = _unit(path - np.array([F.c[0], F.c[1], F.c[2] - 0.02]))
    path = F.push_out(path, 0.012)
    Ps = sweep_grid(path, nrm, 0.030 * hs, 0.008, 4)
    mb.grid(Ps, sweep_uv(Ps, tile_m), W1("head"), out="ring")
    K = np.array([0.0, ch[1] + 0.012, zc - 0.012])
    Pk = superquad(K, 0.020 * hs, 0.015 * hs, 0.014 * hs, 1.0, 1.0, 6, 10)
    mb.grid(Pk, None, W1("head"), out="ring", tri=(K, tile_m))
    for sgn in (-1, 1):
        tp = np.array([K + np.array([sgn * 0.004, 0.0, -0.004]), K + np.array([sgn * 0.016, 0.004, -0.045 * hs]),
                       K + np.array([sgn * 0.022, 0.008, -0.085 * hs])])
        tp = F.push_out(catmull(tp, 3), 0.012)
        tn = np.tile(np.array([0, -1.0, 0]), (len(tp), 1))
        Pt = sweep_grid(tp, tn, np.linspace(0.016, 0.026, len(tp)) * hs, 0.006, 4)
        mb.grid(Pt, sweep_uv(Pt, tile_m), W1("head"), out="ring")
    return _finish(mb, F, "headscarf", "headscarf", "cotton", True, "Cloth", p, 0.9, push=0.002)


# ----------------------------------------------------------------------------------------------
# glasses
# ----------------------------------------------------------------------------------------------
def _lens_outline(style, n=22):
    t = np.linspace(0, 2 * PI, n, endpoint=False)
    if style == "round":
        return np.stack([np.cos(t), np.sin(t)], 1)
    e = 3.2 if style == "rect" else 2.6
    c, s = np.cos(t), np.sin(t)
    x = np.sign(c) * np.abs(c) ** (2 / e)
    z = np.sign(s) * np.abs(s) ** (2 / e)
    if style == "sun":
        x = x * (1 + 0.10 * z)
        z = z * 1.0
    return np.stack([x, z], 1)


def b_glasses(F, p):
    style = p.get("style", "round")
    eL, eR = F.bone("eye_l"), F.bone("eye_r")
    if eL[0] < eR[0]:
        eL, eR = eR, eL
    ipd = abs(eL[0] - eR[0])
    gs = ipd / 0.060
    zc = 0.5 * (eL[2] + eR[2]) + 0.001
    if style == "round":
        a, b = 0.0235 * gs, 0.0235 * gs
    elif style == "sun":
        a, b = 0.0285 * gs, 0.0225 * gs
    else:
        a, b = 0.0265 * gs, 0.0188 * gs
    fw, fh = (0.0050, 0.0060) if style == "sun" else (0.0028, 0.0038)
    tw = 0.0075 if style == "sun" else 0.0035
    outline = _lens_outline(style)
    y_l = eL[1] - 0.026 * gs
    mb = None
    for _ in range(14):
        mb = MB()
        mb.tile = "metal"
        parts = []
        lens_pts = {}
        for sgn in (1, -1):
            cx = sgn * ipd / 2
            pts = np.array([[cx + o[0] * a, y_l, zc + o[1] * b] for o in outline])
            lens_pts[sgn] = pts
        allp = np.vstack(list(lens_pts.values()))
        # bridge arc between the lenses
        xin = ipd / 2 - a * 1.0
        br = np.array([[xin, y_l, zc + 0.35 * b], [xin * 0.5, y_l + 0.004 * gs, zc + 0.55 * b], [0, y_l + 0.007 * gs, zc + 0.5 * b],
                       [-xin * 0.5, y_l + 0.004 * gs, zc + 0.55 * b], [-xin, y_l, zc + 0.35 * b]])
        brp = catmull(br, 3)
        if F.min_clearance(np.vstack([allp, brp])) >= 0.0025:
            break
        y_l -= 0.002
    for sgn in (1, -1):
        pts = lens_pts[sgn]
        nrm = np.tile(np.array([0, -1.0, 0]), (len(pts), 1))
        Pf = sweep_grid(pts, nrm, fw, fh, 4, closed=True)
        Pf = np.concatenate([Pf[:], Pf[:1]], 0)     # close the loop
        uv = np.zeros(Pf.shape[:2] + (2,))
        uv[..., 0] = np.linspace(0.05, 0.95, Pf.shape[0])[:, None]
        uv[..., 1] = np.linspace(0.40, 0.60, Pf.shape[1])[None, :]
        mb.grid(Pf, uv, W1("head"), out="ring")
        if style == "sun":
            cen = pts.mean(0)
            ring = np.stack([np.repeat(cen[None], len(pts) + 1, 0), np.vstack([pts, pts[:1]])], 0)
            ring[1] = ring[1] + np.array([0, 0.0008, 0])
            uvl = np.zeros(ring.shape[:2] + (2,)) + np.array([0.5, 0.12])
            mb.grid(ring, uvl, W1("head"), out=(0, -1, 0))
    nrm = np.tile(np.array([0, -1.0, 0]), (len(brp), 1))
    Pb = sweep_grid(brp, nrm, fw * 1.1, fh, 4)
    uvb = np.zeros(Pb.shape[:2] + (2,))
    uvb[..., 0] = np.linspace(0.05, 0.95, Pb.shape[0])[:, None]
    uvb[..., 1] = np.linspace(0.40, 0.60, Pb.shape[1])[None, :]
    mb.grid(Pb, uvb, W1("head"), out="ring")
    # temples
    ear_top = float(F.ears[:, 2].max()) if len(F.ears) else zc
    ear_y0 = float(F.ears[:, 1].min()) if len(F.ears) else -0.07
    ear_y1 = float(F.ears[:, 1].max()) if len(F.ears) else -0.03
    for sgn in (1, -1):
        hx = sgn * (ipd / 2 + a)
        zt = zc + 0.55 * b
        ys = np.linspace(y_l + 0.002, ear_y0 + 0.006, 6)
        way = []
        for k, y in enumerate(ys):
            s = k / (len(ys) - 1)
            z = zt + (ear_top + 0.005 - zt) * _sm(0, 1, s)
            h = F.side(y, z, sgn)
            x = abs(h[0][0]) + 0.0045 if h is not None else abs(hx)
            x = max(abs(hx), x * _sm(0.0, 0.6, s) + abs(hx) * (1 - _sm(0.0, 0.6, s)))
            way.append([sgn * x, y, z])
        for dy, dz in ((0.010, -0.004), (0.020, -0.016), (0.026, -0.030)):
            y = ear_y1 + dy - 0.012
            z = ear_top + 0.005 + dz
            h = F.side(y, z, sgn)
            x = (abs(h[0][0]) + 0.004) if h is not None else abs(way[-1][0])
            way.append([sgn * x, y, z])
        path = catmull(np.array(way), 2)
        path = F.push_out(path, 0.003)
        nrm = np.tile(np.array([sgn, 0, 0.0]), (len(path), 1))
        Pt = sweep_grid(path, nrm, tw, 0.0028, 4)
        uvt = np.zeros(Pt.shape[:2] + (2,))
        uvt[..., 0] = np.linspace(0.05, 0.95, Pt.shape[0])[:, None]
        uvt[..., 1] = np.linspace(0.40, 0.60, Pt.shape[1])[None, :]
        mb.grid(Pt, uvt, W1("head"), out="ring")
    return _finish(mb, F, "glasses", "glasses_" + style, "metal", True, "Cloth", p, 0.35, uv_fit=False)


# ----------------------------------------------------------------------------------------------
# facial hair (alpha strand shells)
# ----------------------------------------------------------------------------------------------
def _beard_weights(F):
    lb = F.lip_bot

    def w(P):
        out = []
        for q in np.asarray(P):
            a = abs(math.atan2(q[0] - F.c[0], -(q[1] - F.c[1])))
            t = float(_sm(lb + 0.004, lb - 0.034, q[2])) * (1 - 0.6 * float(_sm(D(55), D(100), a)))
            out.append(_wblend("head", "jaw", t))
        return out
    return w


def _face_shell(F, mb, top_fn, bot_fn, az_max, na, ns, thick, z_hi_ref, z_lo_ref, hang=None, uv_off=(0, 0),
                uv_scale=(1, 1), weights=None):
    """Strand shell between bottom_fn(|az|) and top_fn(|az|) following the face (outside-in rays from the head axis)."""
    azs = -az_max + 2 * az_max * np.arange(na + 1) / na
    Z = np.zeros((ns + 1, na + 1))
    Rr = np.zeros((ns + 1, na + 1))
    for j, a in enumerate(azs):
        zt, zb = top_fn(abs(a)), bot_fn(abs(a))
        Z[:, j] = zt + (zb - zt) * np.arange(ns + 1) / ns
        dv = np.array([math.sin(a), -math.cos(a), 0.0])
        for i in range(ns + 1):
            h = F.ray(np.array([F.c[0], F.c[1], Z[i, j]]) + dv * 0.32, -dv, 0.5)   # outside-in: skips the mouth interior
            Rr[i, j] = (0.32 - h[2]) if h is not None else (Rr[i - 1, j] if i else 0.1)
        if hang is not None:
            for i in range(1, ns + 1):
                Rr[i, j] = max(Rr[i, j], Rr[i - 1, j] - hang * abs(Z[i, j] - Z[i - 1, j]))
    Rs = Rr.copy()
    Rs[:, 1:-1] = 0.25 * Rr[:, :-2] + 0.5 * Rr[:, 1:-1] + 0.25 * Rr[:, 2:]
    Rs = np.maximum(Rs, Rr - 0.0004)
    P = np.zeros((ns + 1, na + 1, 3))
    P[..., 0] = F.c[0] + np.sin(azs)[None, :] * (Rs + thick)
    P[..., 1] = F.c[1] - np.cos(azs)[None, :] * (Rs + thick)
    P[..., 2] = Z
    uv = np.zeros((ns + 1, na + 1, 2))
    uv[..., 0] = np.clip(uv_off[0] + (np.arange(na + 1) / na)[None, :] * uv_scale[0], 0.01, 0.99)
    vv = np.clip((z_hi_ref - Z) / (z_hi_ref - z_lo_ref), 0, 1)
    uv[..., 1] = np.clip(uv_off[1] + (1 - vv) * uv_scale[1], 0.01, 0.99)
    mb.grid(P, uv, weights or _beard_weights(F), out=lambda cen: np.array([cen[0] - F.c[0], cen[1] - F.c[1], 0]))


def _beard_regions(F, full):
    hs = F.hs
    lt, lb, cz = F.lip_top, F.lip_bot, F.c[2]
    chin_z = F.chin[2]
    gon = chin_z + 0.30 * (cz - chin_z)
    mouth_az = D(20)

    def top(a):
        t1 = _sm(D(18), D(30), a)
        z1 = (lb + 0.002) + ((lt + 0.012) - (lb + 0.002)) * t1
        if full:
            zs = cz - 0.012
            z1 = z1 + (zs - z1) * _sm(D(45), D(98), a)
        return z1

    def bottom(a, hang):
        return chin_z + 0.004 + (gon - chin_z - 0.004) * _sm(D(10), D(100), a) - hang * (1 - _sm(D(40), D(104), a))
    return top, bottom


def _beard(F, p, kind, full):
    hs = F.hs
    mb = MB()
    mb.tile = "beard"
    top, bottom = _beard_regions(F, full)
    hang = (0.050 if full else 0.0) * hs
    az_max = D(100 if full else 90)
    for li, th in enumerate([0.0035, 0.0105] if full else [0.0030]):
        hg = hang * (1.0 if li == 0 else 1.5)
        z_ref_hi = (F.c[2] - 0.010) if full else (F.lip_top + 0.010)
        z_ref_lo = F.chin[2] - hg
        _face_shell(F, mb, top, lambda a, hg=hg: bottom(a, hg), az_max, 28 if full else 24, 8 if full else 6, th,
                    z_ref_hi, z_ref_lo, hang=(0.55 if full else None), uv_off=(0.07 * li + 0.12, 0.02), uv_scale=(0.8, 0.95))
    if full:
        _mustache_into(F, mb, p, layers=2)
    return _finish(mb, F, kind, kind, "beard", True, "Hair", p, 0.55, uv_fit=False)


def _mustache_into(F, mb, p, layers=2):
    hs = F.hs
    lt = F.lip_top
    amax = D(27)
    z_hi = lt + 0.0095 * hs
    z_lo = lt - 0.005
    for li in range(layers):
        th = [0.0035, 0.0085][li]
        _face_shell(F, mb, lambda a: z_hi - 0.011 * (a / amax) ** 1.5, lambda a: z_lo - 0.011 * (a / amax) ** 2, amax, 12, 3, th,
                    z_hi, z_lo - 0.004, hang=None, uv_off=(0.1 + 0.4 * li, 0.36), uv_scale=(0.4, 0.5),
                    weights=lambda P: [W1("head")] * len(P))


def b_mustache(F, p):
    mb = MB()
    mb.tile = "beard"
    _mustache_into(F, mb, p, 2)
    return _finish(mb, F, "mustache", "mustache", "beard", True, "Hair", p, 0.55, uv_fit=False)


def b_short_beard(F, p):
    return _beard(F, p, "short_beard", False)


def b_full_beard(F, p):
    return _beard(F, p, "full_beard", True)


# ----------------------------------------------------------------------------------------------
# torso items: backpack, shoulder bag, scarf, aprons
# ----------------------------------------------------------------------------------------------
def _surf_path(F, specs, off=0.008):
    """specs: list of ('front',x,z) ('back',x,z) ('top',x,y) ('side',y,z,sgn); returns pts, normals."""
    pts, nrm = [], []
    for s in specs:
        k = s[0]
        if k == "front":
            r = F.front(s[1], s[2], off)
        elif k == "back":
            r = F.back(s[1], s[2], off)
        elif k == "tside":
            cy2, o2 = F.outline(s[1], 40)
            x = s[2] * o2[30 if s[2] > 0 else 10]
            r = (np.array([x, cy2, s[1]]) + np.array([s[2], 0, 0.0]) * off, np.array([s[2], 0, 0.0]))
        elif k == "top":
            h = F.ray((s[1], s[2], 1.53), (0, 0, -1), 2.0, True)
            r = None if h is None else (h[0] + (h[1] if h[1][2] > 0 else -h[1]) * off, h[1] if h[1][2] > 0 else -h[1])
        else:
            r = F.side(s[1], s[2], s[3], off)
        if r is None:
            continue
        pts.append(r[0])
        nrm.append(r[1])
    return np.array(pts), np.array(nrm)


def _ribbon(mb, path, nrm, w, h, tile, weights, tile_m=None, nsides=4, closed=False):
    P = sweep_grid(path, nrm, w, h, nsides, closed=closed)
    if closed:
        P = np.concatenate([P, P[:1]], 0)
    mb.grid(P, sweep_uv(P, tile_m or TILE_M[tile]), weights, out="ring")


def _wspine(z):
    t = float(_sm(1.09, 1.30, z))
    return _wblend("spine_02", "spine_03", t)


def b_backpack(F, p):
    bs = F.bs
    mb = MB()
    mb.tile = "canvas"
    z_cl = F.bone("clavicle_r")[2]
    z_top = z_cl - 0.025 * bs
    z_bot = F.bone("spine_01")[2] + 0.005
    H = min(z_top - z_bot, 0.44 * bs)
    z_bot = z_top - H
    z_mid = 0.5 * (z_top + z_bot)
    cy, out = F.outline(z_mid, 40)
    hw = 0.5 * (out[30] + out[10])
    W = min(2 * hw * 0.92, 0.34 * bs)
    yb = -9
    for z in np.linspace(z_bot, z_top, 7):
        for x in (-0.08 * bs, 0.0, 0.08 * bs):
            r = F.back(x, z)
            if r is not None:
                yb = max(yb, r[0][1])
    y_face = yb + 0.008
    Dp = 0.15 * bs
    cyp = y_face + Dp / 2
    P = superquad((0, cyp, z_mid), W / 2, Dp / 2, H / 2, 0.30, 0.34, 12, 28)
    mb.grid(P, None, _wspine_fn, out="ring", tri=((0, cyp, z_mid), TILE_M["canvas"]))
    # front pocket
    pw, ph, pd = W * 0.74, H * 0.46, 0.06 * bs
    pc = (0, y_face + Dp + pd * 0.30, z_bot + H * 0.34)
    P = superquad(pc, pw / 2, pd / 2, ph / 2, 0.30, 0.34, 8, 20)
    mb.grid(P, None, _wspine_fn, out="ring", tri=(pc, TILE_M["canvas"]))
    # shoulder straps
    z_sh = z_cl
    for sgn in (-1, 1):
        xs = sgn * 0.085
        y_s = None
        fr = F.front(xs, z_sh)
        bk = F.back(xs, z_sh)
        y_s = 0.5 * (fr[0][1] + bk[0][1]) if fr and bk else 0.0
        specs = [("back", sgn * 0.075 * bs, z_top - 0.02), ("back", xs, z_top + 0.035 * bs), ("top", xs, y_s),
                 ("front", sgn * 0.092, z_cl - 0.05 * bs), ("front", sgn * 0.105, z_cl - 0.16 * bs),
                 ("tside", z_mid + 0.01, sgn), ("back", sgn * 0.12 * bs, z_bot + 0.045 * bs)]
        pts, nr = _surf_path(F, specs, 0.009)
        if len(pts) < 4:
            continue
        path = catmull(pts, 3)
        nn = catmull(nr, 3)
        path = F.push_out(path, 0.006, 2, True)
        _ribbon(mb, path, nn, 0.040 * bs, 0.012, "canvas", _wspine_fn)
    return _finish(mb, F, "backpack", "backpack", "canvas", True, "Cloth", p, 0.85, push=0.002)


def _wspine_fn(P):
    return [_wspine(q[2]) for q in np.asarray(P)]


def b_shoulder_bag(F, p):
    bs = F.bs
    mb = MB()
    mb.tile = "leather"
    z_top = F.bone("spine_01")[2] + 0.005
    hb, wb, tb = 0.21 * bs, 0.27 * bs, 0.095 * bs
    zc = z_top - hb / 2
    cy, _ = F.outline(zc, 24)
    xs = 0
    for z in np.linspace(z_top - hb, z_top, 4):
        for y in (cy - 0.35 * wb, cy, cy + 0.35 * wb):
            r = F.side(y, z, +1)
            if r is not None:
                xs = max(xs, r[0][0])
    xc = xs + 0.008 + tb / 2
    c = (xc, cy, zc)
    P = superquad(c, tb / 2, wb / 2, hb / 2, 0.32, 0.30, 10, 24)
    wfn = lambda Q: [W1("spine_02")] * len(Q)
    mb.grid(P, None, wfn, out="ring", tri=(c, TILE_M["leather"]))
    # flap over the upper half of the outer face + top
    Pf = superquad((xc + 0.004, cy, zc + hb * 0.02), tb / 2 * 1.07, wb / 2 * 1.04, hb / 2 * 1.04, 0.32, 0.30, 14, 24)
    sub = Pf[8:, 6:19]
    mb.grid(sub, None, wfn, out="ring", tri=(c, TILE_M["leather"]))
    # strap over the right shoulder (diagonal across the chest)
    z_sh = F.bone("clavicle_r")[2]
    zz = lambda f: z_top + f * (z_sh - z_top)
    y_s = 0.0
    fr, bk = F.front(-0.10, z_sh), F.back(-0.10, z_sh)
    if fr and bk:
        y_s = 0.5 * (fr[0][1] + bk[0][1])
    attach_f = np.array([xc, cy - 0.38 * wb, z_top + 0.003])
    attach_b = np.array([xc, cy + 0.38 * wb, z_top + 0.003])
    specs = [("front", 0.12, zz(0.16)), ("front", 0.03, zz(0.42)), ("front", -0.05, zz(0.70)), ("top", -0.10, y_s),
             ("back", -0.07, zz(0.70)), ("back", 0.03, zz(0.42)), ("back", 0.12, zz(0.16))]
    pts, nr = _surf_path(F, specs, 0.008)
    pts = np.vstack([attach_f[None], pts, attach_b[None]])
    nr = np.vstack([[0.3, -0.9, 0.2], nr, [0.3, 0.9, 0.2]])
    path = catmull(pts, 3)
    nn = catmull(nr, 3)
    path = F.push_out(path, 0.005, 2, True)
    _ribbon(mb, path, nn, 0.034 * bs, 0.007, "leather", wfn)
    return _finish(mb, F, "shoulder_bag", "shoulder_bag", "leather", True, "Cloth", p, 0.6, push=0.002)


def b_scarf(F, p):
    bs = F.bs
    mb = MB()
    mb.tile = "knit"
    chin = F.chin
    zc0 = chin[2] - 0.036 * bs
    cy, _ = F.outline(zc0 - 0.02, 16)
    n = 28
    az = -PI + 2 * PI * np.arange(n + 1) / n
    rings = []
    for wi, (dzw, grow, wd) in enumerate([(0.0, 0.0, 0.066), (-0.036 * bs, 0.006, 0.052)]):
        pts, nr = [], []
        for a in az:
            z = zc0 + dzw + 0.028 * (1 - math.cos(a)) / 2 - 0.004
            dv = np.array([math.sin(a), -math.cos(a), 0.0])
            r = 0.0
            for dz in (0.0, 0.3 * wd * bs):
                h = F.ray((0, cy, z + dz), dv, 0.5, True)
                if h is not None:
                    r = max(r, h[2])
            r = min(r or 0.06, 0.080 * bs)
            pts.append(np.array([0, cy, z]) + dv * (r + 0.016 + grow))
            nr.append(dv)
        pts = np.array(pts)[:-1]
        nr = np.array(nr)[:-1]
        P = sweep_grid(pts, nr, wd * bs, 0.026 - 0.004 * wi, 8, closed=True)
        P = np.concatenate([P, P[:1]], 0)
        wt = (lambda Q, wi=wi: [_wblend("neck_01", "spine_03", 0.0 if wi == 0 else 0.3)] * len(Q))
        mb.grid(P, sweep_uv(P, TILE_M["knit"]), wt, out="ring")
        rings.append(pts)
    # knot + tails at the front-left of the neck
    k = rings[0][int(round(n * (0.5 + 0.08)))]
    K = k + np.array([0.0, -0.010, -0.012])
    Pk = superquad(K, 0.026 * bs, 0.020 * bs, 0.024 * bs, 1.0, 1.0, 6, 10)
    wk = lambda Q: [_wblend("neck_01", "spine_03", 0.2)] * len(Q)
    mb.grid(Pk, None, wk, out="ring", tri=(K, TILE_M["knit"]))
    for ti, (dx, L) in enumerate([(0.012, 0.30), (-0.022, 0.21)]):
        specs = []
        for t in np.linspace(0, 1, 6):
            z = K[2] - 0.012 - t * L * bs
            specs.append(("front", K[0] + dx + 0.012 * t * (1 if ti == 0 else -1), z))
        pts, nr = _surf_path(F, specs, 0.012)
        if len(pts) < 3:
            continue
        pts[0] = K + np.array([dx * 0.5, -0.004, -0.014])
        path = catmull(pts, 2)
        nn = catmull(nr, 2)
        path = F.push_out(path, 0.008, 2, True)
        wt = lambda Q, K=K: [_wblend("neck_01", "spine_03", float(_sm(K[2] - 0.01, K[2] - 0.12, q[2]))) for q in np.asarray(Q)]
        _ribbon(mb, path, nn, 0.062 * bs, 0.014, "knit", wt)
    return _finish(mb, F, "scarf", "scarf", "knit", True, "Cloth", p, 0.95, push=0.004)


def _apron_w(z_w, bs, upper):
    def w(P):
        out = []
        for q in np.asarray(P):
            tc = float(_sm(z_w + 0.02, z_w + 0.12, q[2])) if upper else 0.0
            tw = 0.55 * float(_sm(0.04, 0.36 * bs, z_w - q[2]))
            d = {}
            if tc > 1e-4:
                d["spine_02"] = tc
            rest = 1 - tc
            if rest * (1 - tw) > 1e-4:
                d["pelvis"] = rest * (1 - tw)
            s = float(_sm(-0.16 * bs, 0.16 * bs, q[0]))
            if rest * tw * s > 1e-4:
                d["thigh_l"] = rest * tw * s
            if rest * tw * (1 - s) > 1e-4:
                d["thigh_r"] = rest * tw * (1 - s)
            out.append(d)
        return out
    return w


def _front_env(F, xs, z, win):
    """Frontmost surface y over a window of +-win around each x (drapes cloth over both thighs)."""
    xx = np.linspace(-0.40, 0.40, 81)
    ys = np.full(len(xx), np.inf)
    for i, x in enumerate(xx):
        r = F.front(x, z)
        if r is not None:
            ys[i] = r[0][1]
    out = []
    for x in xs:
        m = np.abs(xx - x) <= win
        out.append(ys[m].min())
    out = np.array(out)
    if not np.isfinite(out).all():
        out[~np.isfinite(out)] = out[np.isfinite(out)].mean() if np.isfinite(out).any() else 0.0
    return out


def _apron(F, p, bib):
    bs = F.bs
    mb = MB()
    mb.tile = "canvas"
    z_w = F.bone("spine_01")[2] + 0.025 * bs
    hem = z_w - (0.55 if not bib else 0.58) * bs
    z_cl = F.bone("clavicle_r")[2]
    z_t = (z_cl - 0.065 * bs) if bib else z_w
    cy, out = F.outline(z_w - 0.03, 40)
    hw_w = 0.5 * (out[30] + out[10]) * 0.96
    nc = 13
    nr = 22 if bib else 16
    cols = np.linspace(-1, 1, nc)
    P = np.zeros((nr, nc, 3))
    for i in range(nr):
        t = i / (nr - 1)
        z = z_t - (z_t - hem) * t
        if bib:
            hwz = 0.095 * bs + (hw_w - 0.095 * bs) * float(_sm(z_t, z_w, z))
            if z < z_w:
                hwz = hw_w * (1.0 + 0.10 * float(_sm(z_w, hem, z)))
        else:
            hwz = hw_w * (1.0 + 0.10 * float(_sm(z_w, hem, z)))
        xs = cols * hwz
        ys = _front_env(F, xs, z, 0.045)
        flare = 0.010 + 0.030 * bs * t ** 1.4 + (0.004 if i == 0 else 0)
        P[i, :, 0] = xs
        P[i, :, 1] = ys - flare + 0.0 * xs
        P[i, :, 2] = z
        # slight cloth sag toward the sides
        P[i, :, 1] += 0.008 * (np.abs(cols) ** 2) * (1 if i else 0) * min(1.0, t * 3)
    wpan = _apron_w(z_w, bs, bib)
    uvc = (0, cy, 0.5 * (z_t + hem))
    mb.grid(P, None, wpan, out=(0, -1, 0), tri=(uvc, TILE_M["canvas"]))
    # waist band all around + ties
    n = 28
    pts, nr_ = [], []
    for k in range(n):
        a = -PI + 2 * PI * k / n
        dv = np.array([math.sin(a), -math.cos(a), 0.0])
        h = F.ray((0, cy, z_w), dv, 0.6, True)
        r = h[2] if h is not None else 0.14
        if abs(a) < 0.6:
            fr = F.front(r * math.sin(a), z_w)
        pts.append(np.array([0, cy, z_w]) + dv * (r + 0.004))
        nr_.append(dv)
    pts, nr_ = np.array(pts), np.array(nr_)
    # make the band meet the panel top at the front
    pts = F.push_out(pts, 0.004, 2, True)
    wband = lambda Q: [_wblend("pelvis", "spine_01", 0.4)] * len(Q)
    _ribbon(mb, pts, nr_, 0.040 * bs, 0.007, "canvas", wband, closed=True)
    # bow tails at the back
    bk = F.back(0.0, z_w, 0.006)
    if bk is not None:
        for sgn in (-1, 1):
            tp = np.array([bk[0], bk[0] + np.array([sgn * 0.02, 0.010, -0.05 * bs]), bk[0] + np.array([sgn * 0.035, 0.014, -0.11 * bs])])
            tp = F.push_out(catmull(tp, 3), 0.005, 2, True)
            tn = np.tile(np.array([0, 1.0, 0]), (len(tp), 1))
            _ribbon(mb, tp, tn, 0.026 * bs, 0.004, "canvas", wpan)
    if bib:
        xs0 = 0.095 * bs * 0.9
        y_s = 0.0
        fr, bk2 = F.front(0.075, z_cl), F.back(0.075, z_cl)
        if fr and bk2:
            y_s = 0.5 * (fr[0][1] + bk2[0][1])
        zn = F.bone("neck_01")[2] + 0.005
        specs = [("front", -xs0, z_t - 0.01), ("front", -0.08, z_t + 0.6 * (z_cl - z_t)), ("top", -0.075, y_s),
                 ("back", -0.045, zn), ("back", 0.045, zn), ("top", 0.075, y_s),
                 ("front", 0.08, z_t + 0.6 * (z_cl - z_t)), ("front", xs0, z_t - 0.01)]
        pts, nr2 = _surf_path(F, specs, 0.007)
        if len(pts) >= 4:
            path = F.push_out(catmull(pts, 3), 0.004, 2, True)
            nn = catmull(nr2, 3)
            _ribbon(mb, path, nn, 0.026 * bs, 0.004, "canvas", lambda Q: [W1("spine_03")] * len(Q))
    return _finish(mb, F, "bib_apron" if bib else "waist_apron", "bib_apron" if bib else "waist_apron", "canvas", True, "Cloth", p, 0.9, push=0.003)


def b_waist_apron(F, p):
    return _apron(F, p, False)


def b_bib_apron(F, p):
    return _apron(F, p, True)


# ----------------------------------------------------------------------------------------------
# hand-held items (right hand)
# ----------------------------------------------------------------------------------------------
def _grip(F, side="r"):
    H, M = F.bone("hand_" + side), F.bone("middle_01_" + side)
    I, Pk = F.bone("index_01_" + side), F.bone("pinky_01_" + side)
    fdir = _unit(M - H)
    lat = _unit(I - Pk)
    n = _unit(np.cross(fdir, lat))
    if n[0] * H[0] > 0:      # palm normal must point to the body
        n = -n
    C = H + 0.5 * (M - H)
    h = F.ray(C + n * 0.10, -n, 0.3)
    surf = h[0] if h is not None else C
    return C, surf, n, fdir, lat


def b_cane(F, p):
    bs = F.bs
    mb = MB()
    mb.tile = "wood"
    side = p.get('hand', 'r')
    hb = 'hand_' + side
    C, surf, n, fdir, lat = _grip(F, side)
    r_h = 0.0125 * bs
    G = surf + n * (r_h - 0.001)
    R = 0.045 * bs
    z_top_shaft = G[2] + 0.085 * bs
    pts = []
    for z in np.linspace(0.0, z_top_shaft, 8):
        pts.append([G[0], G[1], z])
    for ph in np.linspace(0, PI, 10)[1:]:
        pts.append([G[0], G[1] - R + R * math.cos(ph), z_top_shaft + R * math.sin(ph)])
    pts = np.array(pts)
    # tapered radius: ferrule at the foot, fat handle crook
    s = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(pts, axis=0), axis=1))])
    rad = np.interp(s, [0, 0.03, 0.12, s[-1] * 0.55, s[-1]], [0.0075, 0.0085, 0.0085, 0.0100, 0.0115]) * bs
    nn = np.tile(np.array([1.0, 0, 0]), (len(pts), 1))
    P = sweep_grid(pts, nn, rad * 2, rad * 2, 8)
    uv = sweep_uv(P, TILE_M["wood"])
    mb.grid(P, uv, W1(hb), out="ring")
    # rubber tip cap
    cap = superquad(pts[0] + np.array([0, 0, 0.004]), rad[0] * 1.05, rad[0] * 1.05, 0.008, 0.5, 1.0, 4, 8)
    mb.grid(cap, None, W1(hb), out="ring", tri=(pts[0], TILE_M["wood"]))
    return _finish(mb, F, "cane", "cane", "wood", True, "Cloth", p, 0.55)


def _cell_uv(P, cell, wrap=True):
    R, C = P.shape[:2]
    u0, v0, u1, v1 = CELLS[cell]
    ins = 0.015
    u = u0 + ins + (np.linspace(0, 1, C))[None, :] * (u1 - u0 - 2 * ins)
    v = v0 + ins + (np.linspace(0, 1, R))[:, None] * (v1 - v0 - 2 * ins)
    uv = np.zeros((R, C, 2))
    uv[..., 0] = u
    uv[..., 1] = v
    return uv


def _ribbon_quad(mb, p1, p2, side, w, wt, cen, cell):
    P = np.array([[p1 - side * w / 2, p1 + side * w / 2], [p2 - side * w / 2, p2 + side * w / 2]])
    uv = _cell_uv(P, cell)
    out = lambda c: np.array([c[0] - cen[0], c[1] - cen[1], 0.0])
    mb.grid(P, uv, wt, out=out)
    mb.grid(P + 0.0003 * np.array([0, 0, 0]), uv, wt, out=out, flip=True)


def b_string_bag(F, p):
    bs = F.bs
    mb = MB()
    mb.tile = "avoska"
    side = p.get('hand', 'r')
    hb = 'hand_' + side
    C, surf, n, fdir, lat = _grip(F, side)
    G = surf + n * 0.006
    wt = W1(hb)
    wf = lambda Q: [wt] * len(Q)
    Ln = 0.38 * bs
    a, b = 0.100 * bs, 0.074 * bs
    z_top = G[2] - 0.085 * bs
    z_bot = z_top - Ln
    cx, cyb = G[0], G[1]
    K, M = 6, 8

    def prof(t):
        if t < 0.45:
            return math.sin(PI / 2 * t / 0.45) ** 0.8
        return math.sqrt(max(0.0, 1 - ((t - 0.45) / 0.55) ** 2))
    pts = np.zeros((K + 1, M, 3))
    for k in range(K + 1):
        t = k / K
        f = prof(t)
        for m in range(M):
            th = 2 * PI * m / M + 0.2
            pts[k, m] = [cx + a * f * math.cos(th), cyb + b * f * math.sin(th), z_top - t * Ln]
    cen = np.array([cx, cyb, 0])
    wr = 0.0045
    for m in range(M):                    # meridians
        for k in range(K):
            p1, p2 = pts[k, m], pts[k + 1, m]
            side = _unit(np.cross(p2 - p1, np.array([p1[0] - cx, p1[1] - cyb, 0.0]) + 1e-9))
            if np.linalg.norm(side) < 1e-6:
                side = np.array([0, 0, 1.0])
            _ribbon_quad(mb, p1, p2, _unit(np.cross(p2 - p1, [p1[0] - cx, p1[1] - cyb, 0.001])), wr, wf, cen, "cord")
    for k in range(1, K):                 # rings
        for m in range(M):
            p1, p2 = pts[k, m], pts[k, (m + 1) % M]
            rad = np.array([(p1[0] + p2[0]) / 2 - cx, (p1[1] + p2[1]) / 2 - cyb, 0.0])
            side = _unit(np.cross(p2 - p1, rad))
            _ribbon_quad(mb, p1, p2, side, wr, wf, cen, "cord")
    # handle cords up to the fist
    T = np.array([cx, cyb, z_top])
    for sgn in (-1, 1):
        way = np.array([T, T + np.array([sgn * 0.016, -sgn * 0.006, 0.035 * bs]), G + np.array([sgn * 0.004, 0, -0.01]), G])
        path = catmull(way, 3)
        nn = np.tile(np.array([0, -1.0, 0]), (len(path), 1))
        Pc = sweep_grid(path, nn, 0.0045, 0.0045, 4)
        mb.grid(Pc, _cell_uv(Pc, "cord"), wf, out="ring")
    # groceries
    zb = z_bot
    def blob(c, ax, ay, az, e1, e2, nph, nth, cell, rot=None):
        Pq = superquad(c, ax, ay, az, e1, e2, nph, nth, rot)
        mb.grid(Pq, _cell_uv(Pq, cell), wf, out="ring")
    s_ = bs
    blob((cx - 0.060 * s_, cyb + 0.018 * s_, zb + 0.050 * s_ + 0.100 * s_), 0.027 * s_, 0.027 * s_, 0.100 * s_, 0.18, 1.0, 6, 10, "bottle")
    blob((cx - 0.060 * s_, cyb + 0.018 * s_, zb + 0.050 * s_ + 0.225 * s_), 0.0145 * s_, 0.0145 * s_, 0.035 * s_, 0.3, 1.0, 4, 8, "bottle")
    blob((cx - 0.060 * s_, cyb + 0.018 * s_, zb + 0.050 * s_ + 0.262 * s_), 0.0165 * s_, 0.0165 * s_, 0.007 * s_, 0.4, 1.0, 3, 8, "foil")
    blob((cx + 0.030 * s_, cyb + 0.012 * s_, zb + 0.082 * s_), 0.046 * s_, 0.046 * s_, 0.044 * s_, 1.0, 1.0, 6, 10, "cabbage")
    blob((cx - 0.012 * s_, cyb - 0.038 * s_, zb + 0.086 * s_), 0.028 * s_, 0.028 * s_, 0.026 * s_, 1.0, 1.0, 5, 8, "apple")
    blob((cx + 0.060 * s_, cyb - 0.034 * s_, zb + 0.060 * s_), 0.028 * s_, 0.028 * s_, 0.026 * s_, 1.0, 1.0, 5, 8, "apple")
    rot = np.array([[math.cos(0.45), 0, math.sin(0.45)], [0, 1, 0], [-math.sin(0.45), 0, math.cos(0.45)]])
    blob((cx + 0.005 * s_, cyb - 0.020 * s_, zb + 0.195 * s_), 0.083 * s_, 0.029 * s_, 0.028 * s_, 0.8, 0.8, 6, 12, "bread", rot)
    return _finish(mb, F, "string_bag", "string_bag", "avoska", False, "Cloth", p, 0.8, uv_fit=False)


def b_shopping_bag(F, p):
    bs = F.bs
    mb = MB()
    mb.tile = "plastic"
    side = p.get('hand', 'r')
    hb = 'hand_' + side
    C, surf, n, fdir, lat = _grip(F, side)
    G = surf + n * 0.006
    wf = lambda Q: [W1(hb)] * len(Q)
    Ls = 0.34 * bs
    z_top = G[2] - 0.10 * bs
    a0, b0 = 0.118 * bs, 0.046 * bs
    cx, cyb = G[0], G[1]
    K, M = 10, 20
    P = np.zeros((K + 1, M + 1, 3))
    for k in range(K + 1):
        t = k / K
        f = 0.20 + 0.80 * float(_sm(0, 0.26, t)) ** 0.7
        if t > 0.90:
            f *= math.sqrt(max(0.0, 1 - ((t - 0.90) / 0.10) ** 2))
        if k == K:
            f = 0.0
        bf = 0.30 + 0.70 * f
        for m in range(M + 1):
            th = 2 * PI * m / M
            wr = 1 + 0.035 * math.sin(5 * th + 9 * t) * float(_sm(0.1, 0.4, t)) * (t < 0.95)
            P[k, m] = [cx + a0 * f * math.cos(th) * wr, cyb + b0 * bf * math.sin(th) * wr, z_top - t * Ls]
    mb.grid(P, None, wf, out="ring", tri=((cx, cyb, z_top - Ls / 2), TILE_M["plastic"]))
    T_l = np.array([cx - a0 * 0.22, cyb, z_top])
    T_r = np.array([cx + a0 * 0.22, cyb, z_top])
    for sgn in (-1, 1):
        way = np.array([T_l, T_l + np.array([0.012, sgn * 0.018, 0.04 * bs]),
                        G + np.array([0.0, sgn * 0.006, -0.012]),
                        T_r + np.array([-0.012, sgn * 0.018, 0.04 * bs]), T_r])
        path = catmull(way, 3)
        nn = np.tile(np.array([0, float(sgn), 0]), (len(path), 1))
        Pc = sweep_grid(path, nn, 0.020 * bs, 0.0025, 4)
        mb.grid(Pc, sweep_uv(Pc, TILE_M["plastic"]), wf, out="ring")
    return _finish(mb, F, "shopping_bag", "shopping_bag", "plastic", True, "Cloth", p, 0.35)


# ----------------------------------------------------------------------------------------------
# public API
# ----------------------------------------------------------------------------------------------
_BUILDERS = {}
for _k in KINDS:
    _f = globals().get("b_" + _k)
    if _f is not None:
        _BUILDERS[_k] = _f

_fit_cache = {}


def add(kind, ctx, params=None):
    """Builds accessory `kind` fitted to ctx['basemesh'] and returns the (rigidly skinned) mesh object."""
    if kind not in KINDS:
        raise ValueError("unknown accessory kind %r" % kind)
    params = dict(params or {})
    key = (ctx["basemesh"].name, ctx["armature"].name)
    fit = Fit(ctx) if params.pop("_refit", True) else _fit_cache.get(key) or Fit(ctx)
    _fit_cache[key] = fit
    obj = _BUILDERS[kind](fit, params)
    obj.name = "acc_%s" % kind
    return obj


def add_many(kinds, ctx, params=None):
    """Fits the body once and builds several accessories ({kind: params} optional)."""
    fit = Fit(ctx)
    out = []
    for k in kinds:
        p = dict((params or {}).get(k, {}))
        o = _BUILDERS[k](fit, p)
        o.name = "acc_%s" % k
        out.append(o)
    return out
