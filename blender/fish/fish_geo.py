"""Fish geometry in pure numpy (no bpy): lofted body, fins, eyes, extras, UV layout.

Frame while building ("upright" fish): nose +X, back +Z, left side +Y, metres, origin on the axis at half the total length.
A flatfish is built upright with a thin body and rotated onto its left side at the end (eyed = right side up).
t = position along the total length, 0 nose .. 1 tail tip.  Every mesh part is returned as arrays; UVs come from a
Layout (texture atlas plan) so LOD0 / LOD1 share one mapping.
"""
import math

import numpy as np

W, H = 1024, 512
PAD = 4


# ------------------------------------------------------------------ small helpers
def curve(pts):
    """Monotone cubic (PCHIP) through [(t, v), ...]; a number gives a constant."""
    if np.isscalar(pts):
        c = float(pts)
        return lambda t: np.full(np.shape(t), c)
    x = np.array([p[0] for p in pts], float)
    y = np.array([p[1] for p in pts], float)
    n = len(x)
    h = np.diff(x)
    d = np.diff(y) / h
    m = np.zeros(n)
    for k in range(1, n - 1):
        if d[k - 1] * d[k] > 0:
            w1 = 2 * h[k] + h[k - 1]
            w2 = h[k] + 2 * h[k - 1]
            m[k] = (w1 + w2) / (w1 / d[k - 1] + w2 / d[k])
    if n == 2:
        m[:] = d[0]
    else:
        for k, (h0, h1, d0, d1) in ((0, (h[0], h[1], d[0], d[1])), (n - 1, (h[-1], h[-2], d[-1], d[-2]))):
            v = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1)
            if np.sign(v) != np.sign(d0):
                v = 0.0
            elif np.sign(d0) != np.sign(d1) and abs(v) > 3 * abs(d0):
                v = 3 * d0
            m[k] = v

    def f(t):
        t = np.clip(np.asarray(t, float), x[0], x[-1])
        i = np.clip(np.searchsorted(x, t, side="right") - 1, 0, n - 2)
        s = (t - x[i]) / h[i]
        s2, s3 = s * s, s * s * s
        return ((2 * s3 - 3 * s2 + 1) * y[i] + (s3 - 2 * s2 + s) * h[i] * m[i]
                + (-2 * s3 + 3 * s2) * y[i + 1] + (s3 - s2) * h[i] * m[i + 1])
    return f


def smoothstep(a, b, x):
    s = np.clip((np.asarray(x, float) - a) / (b - a), 0, 1)
    return s * s * (3 - 2 * s)


# ------------------------------------------------------------------ body
class Body:
    """Superellipse cross-sections along the length from species curves (all heights/widths are fractions of L)."""

    def __init__(self, spec, L):
        self.L = L
        self.spec = spec
        p = spec["profile"]
        self.top = curve(p["top"])
        self.bot = curve(p["bot"])
        self.wid = curve(p["wid"])
        self.n = curve(p.get("n", 2.2))
        self.shift = curve(p.get("shift", 0.0))
        self.tend = spec.get("body_end", 0.86)
        self.tmax = self.tend + spec.get("tail_cap", 0.008)      # the end cap's apex sits a little behind the last ring
        self.nose = spec.get("nose_len", 0.03)
        self.nose_bot = spec.get("nose_len_bot", self.nose)
        hd = spec.get("head", {})
        self.tg = hd.get("gill_t", 0.2)
        self.ridge = hd.get("gill_ridge", 0.0)
        self.mouth_len = hd.get("mouth_len", 0.04)
        self.mouth_depth = hd.get("mouth_depth", 0.0)
        self.mouth_p = hd.get("mouth_p", 0.6)
        self.mouth_hold = hd.get("mouth_hold", 0.0)     # fraction of mouth_len where the gape groove keeps full depth (beaks)
        self.mouth_w = hd.get("mouth_w", 0.17)          # angular half-width of the groove (rad)
        self.theta_m = math.pi * self.mouth_p
        self.flat = bool(spec.get("flat"))

    def set_mouth_theta(self, th):
        self.theta_m = th

    @staticmethod
    def cap(t, ln):
        tt = np.clip(np.asarray(t, float) / ln, 0, 1)
        return np.sqrt(np.maximum(1 - (1 - tt) ** 2, 0))

    def point(self, t, th, side):
        """Surface point(s), metres. t, th broadcastable; side = +1 left (+Y) / -1 right / 0 centre."""
        t = np.asarray(t, float)
        th = np.asarray(th, float)
        c, s = np.cos(th), np.sin(th)
        e = 2.0 / self.n(t)
        top = self.top(t) * self.cap(t, self.nose)
        bot = self.bot(t) * self.cap(t, self.nose_bot)
        wid = self.wid(t) * self.cap(t, 0.5 * (self.nose + self.nose_bot))
        z = np.where(c >= 0, top * np.abs(c) ** e, -bot * np.abs(c) ** e)
        z = z + self.shift(t) * np.minimum(self.cap(t, self.nose), self.cap(t, self.nose_bot))
        y = wid * np.abs(s) ** e
        if self.ridge:   # operculum edge: a low step across the cheek, sharper behind
            d = t - self.tg
            bump = np.where(d < 0, np.exp(-(d / 0.022) ** 2), np.exp(-(d / 0.0045) ** 2))
            y = y * (1 + self.ridge * bump * s ** 2)
        if self.mouth_depth:  # gape line pulled in
            hold = self.mouth_hold
            tm = np.clip((t / self.mouth_len - hold) / (1 - hold), 0, 1)
            gt = np.where(t < self.mouth_len, np.sqrt(np.maximum(1 - tm ** 2, 0)) * 0.6 + 0.4 * (1 - tm), 0)
            f = 1 - self.mouth_depth * gt * np.exp(-((th - self.theta_m) / self.mouth_w) ** 2)
            y, z = y * f, z * f
        x = (0.5 - t) * self.L
        return np.stack(np.broadcast_arrays(x, side * y * self.L, z * self.L), -1)

    def arcs(self, t, nth=129):
        """Cumulative half-ring arc length from the dorsal midline: (len(t), nth) metres, plus the theta grid."""
        th = np.linspace(0, math.pi, nth)
        p = self.point(np.asarray(t)[:, None], th[None, :], -1)
        seg = np.linalg.norm(np.diff(p[..., 1:], axis=1), axis=-1)
        return np.concatenate([np.zeros((len(t), 1)), np.cumsum(seg, 1)], 1), th

    def max_arc(self):
        t = np.linspace(0.002, self.tend, 120)
        return float(self.arcs(t)[0][:, -1].max())

    def q_theta(self, t, q):
        """theta whose height fraction from the dorsal edge is q (0 dorsal .. 1 ventral)."""
        th = np.linspace(0, math.pi, 257)
        p = self.point(t, th, -1)[:, 2]
        top, bot = p.max(), p.min()
        qq = (top - p) / max(top - bot, 1e-9)
        return float(np.interp(q, qq, th))

    def q_field(self, t, th):
        """height fraction from the dorsal edge for texture painting (arrays)."""
        z = self.point(t, th, -1)[..., 2]
        top = self.point(t, 0.0, 0)[..., 2]
        bot = self.point(t, math.pi, 0)[..., 2]
        return (top - z) / np.maximum(top - bot, 1e-9)

    def flex(self, t):
        return smoothstep(self.tg * 0.9, 0.98, t) ** 1.15


# ------------------------------------------------------------------ fin shapes
def fin_profile(fr, shape, peak=0.5, ends=(0.0, 0.0), power=1.0):
    """Height fraction (0..1) along a fin base fr in 0..1 (front to back)."""
    fr = np.asarray(fr, float)
    peak = float(np.clip(peak, 0.05, 0.95))
    g = np.where(fr < peak, fr / peak, (1 - fr) / (1 - peak))      # triangle 0..1..0
    if shape == "rounded":
        h = np.sqrt(np.maximum(1 - (1 - g) ** 2, 0))
    elif shape == "pointed":                                       # leading ray tallest, sickle back edge
        h = np.where(fr < peak, g ** 0.8, g ** 1.6)
    elif shape == "spiny":                                         # stiff spines: steep front, long slanted back
        h = np.where(fr < peak, g ** 0.5, g ** 1.2)
    elif shape == "trapezoid":
        h = np.clip(np.minimum(fr, 1 - fr) * 6, 0, 1) ** 0.6
    elif shape == "fringe":                                        # continuous flat fin fringe
        h = np.clip(np.minimum(fr, 1 - fr) * 14, 0, 1) ** 0.7
    else:
        h = g
    h = h ** power
    e0, e1 = ends
    return np.maximum(h, np.maximum(e0 * (1 - fr) + e1 * fr, 0.16))


def caudal_ext(v, shape, fork=0.4):
    """Extension fraction of the caudal trailing edge at v (0 bottom .. 1 top)."""
    v = np.asarray(v, float)
    a = np.abs(2 * v - 1)
    if shape == "forked":
        return 1 - fork * (1 - a) ** 1.15
    if shape == "emarginate":
        return 1 - fork * (1 - a) ** 1.6
    if shape == "truncate":
        return np.ones_like(v) - 0.04 * (1 - a)
    if shape == "pointed":
        return 0.35 + 0.65 * (1 - a ** 1.4)
    if shape == "oval":                                         # parabolic cap: finite slope at the corners (no sliver triangles on narrow tails)
        return 0.48 + 0.52 * (1 - a ** 2)
    return 0.40 + 0.60 * np.sqrt(np.maximum(1 - a ** 2, 0))   # rounded


# ------------------------------------------------------------------ parts
class Part:
    def __init__(self, name, kind, V, F, uvl, R, G, B, mirror=None, rect_key=None, extra=None):
        self.name, self.kind = name, kind
        self.V = np.asarray(V, float)
        self.F = np.asarray(F, int).reshape(-1, 3)
        self.uvl = np.asarray(uvl, float)          # local uv per vertex: for fins/eyes (fu, fv) in 0..1; body: texel px (x, y)
        self.R, self.G, self.B = (np.asarray(a, float) for a in (R, G, B))
        self.mirror = np.zeros(len(self.F), bool) if mirror is None else np.asarray(mirror, bool)
        self.rect_key = rect_key
        self.extra = extra or {}


def _orient(V, F, centre_fn, want_out=True):
    """Return F with winding fixed so normals point away from centre_fn(face centroid) (majority vote)."""
    P = V[F]
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    cen = P.mean(1)
    out = np.einsum("ij,ij->i", n, cen - centre_fn(cen))
    if (out < 0).sum() > (out > 0).sum():
        F = F[:, ::-1]
    return F


def build_body(body, spec, lod, layout):
    L = body.L
    m = lod["m"]
    split = body.flat                              # flat: blind side gets its own texture region
    # snap mouth to a ring vertex
    kmouth = max(1, min(m - 1, int(round(body.mouth_p * m))))
    body.set_mouth_theta(math.pi * kmouth / m)
    # ring positions: rounded nose cap, then uniform
    nn = lod["nose_rings"]
    phis = (np.arange(1, nn + 1) / nn) * (math.pi / 2)
    tn = body.nose * (1 - np.cos(phis))
    nrest = lod["rings"] - nn
    tn = tn[:-1] if body.nose < body.tend * 0.9 else tn[:0]
    t0 = body.nose
    ts = np.concatenate([tn, np.linspace(t0, body.tend, nrest + 1)[0:]])
    ts = np.unique(np.round(ts, 7))
    nr = len(ts)
    tr = ts[:, None]
    ths_R = np.pi * np.arange(0, m + 1) / m              # right side theta, dorsal -> ventral
    # vertex table: entries (side, j) ; non split shares j=0,m
    entries = []
    index = {}
    for side in (-1, 1):
        for j in range(m + 1):
            if not split and side == 1 and j in (0, m):
                index[(side, j)] = index[(-1, j)]
                continue
            index[(side, j)] = len(entries)
            entries.append((side, j))
    nper = len(entries)
    V = []
    UV = []
    Rr, Gg = [], []
    arcs, th_grid = body.arcs(ts)
    centres = []
    for i, t in enumerate(ts):
        for side, j in entries:
            th = ths_R[j]
            p = body.point(t, th, side if j not in (0, m) or split else 0)[()]
            if split and j in (0, m):
                p = body.point(t, th, 0)
            V.append(p)
            a = np.interp(th, th_grid, arcs[i])
            u, v = layout.body_uv(t, a, side if split else -1)
            UV.append((u, v))
            Rr.append(t)
            Gg.append(float(body.flex(t)))
    V = np.array(V).reshape(nr, nper, 3)
    UV = np.array(UV).reshape(nr, nper, 2)
    Rr = np.array(Rr).reshape(nr, nper)
    Gg = np.array(Gg).reshape(nr, nper)
    # apex vertices at the nose and the tail end
    def apex(i, tt, end):
        zc = float(body.point(tt, 0.0, 0)[2] + body.point(tt, math.pi, 0)[2]) / 2
        x = (0.5 - tt) * L
        return np.array([x, 0.0, zc])
    allV = [V.reshape(-1, 3)]
    allUV = [UV.reshape(-1, 2)]
    allR = [Rr.ravel()]
    allG = [Gg.ravel()]
    nv = nr * nper
    apex_idx = {}
    for end, i in (("nose", 0), ("tail", nr - 1)):
        for side in ((-1, 1) if split else (-1,)):
            tt = ts[i]
            if end == "nose":
                p = np.array([(0.5) * L, 0.0, 0.0])
                tt = 0.0
            else:
                p = apex(i, tt, True)
                p[0] = (0.5 - body.tmax) * L
                tt = body.tmax
            a_mid = arcs[i][-1] / 2
            u, v = layout.body_uv(tt, a_mid, side if split else -1)
            apex_idx[(end, side)] = nv
            nv += 1
            allV.append(p[None])
            allUV.append(np.array([[u, v]]))
            allR.append(np.array([tt]))
            allG.append(np.array([float(body.flex(tt))]))
    # triangles
    def vid(i, side, j):
        return i * nper + index[(side, j)]
    tris, mir, grp = [], [], []
    for side in (-1, 1):
        for i in range(nr - 1):
            for j in range(m):
                a, b = vid(i, side, j), vid(i, side, j + 1)
                c, d = vid(i + 1, side, j + 1), vid(i + 1, side, j)
                tris.append((a, b, c))
                tris.append((a, c, d))
                mir += [side == 1 and not split] * 2
                grp += [side * 10 + 1] * 2
    # nose / tail fans
    for side in (-1, 1):
        sd = side if split else -1
        for j in range(m):
            tris.append((apex_idx[("nose", sd)], vid(0, side, j), vid(0, side, j + 1)))
            mir.append(side == 1 and not split)
            grp.append(side * 10 + 2)
            tris.append((apex_idx[("tail", sd)], vid(nr - 1, side, j), vid(nr - 1, side, j + 1)))
            mir.append(side == 1 and not split)
            grp.append(side * 10 + 3)
    # nose ring 0 is t=0 only when there is no tn (nose cap rings begin >0): ring 0 = ts[0]
    V = np.concatenate(allV)
    F = np.array(tris)
    grp = np.array(grp)
    mir = np.array(mir)
    # orient each group outward
    for g in np.unique(grp):
        sel = grp == g
        if g % 10 == 2 or g % 10 == 3:   # fans: outward from the apex axis
            F[sel] = _orient(V, F[sel], lambda c: np.stack([c[:, 0], np.zeros(len(c)), np.zeros(len(c))], 1))
        else:
            F[sel] = _orient(V, F[sel], lambda c: np.stack([c[:, 0], np.zeros(len(c)), np.zeros(len(c))], 1))
    B = np.zeros(len(V))
    part = Part("body", "body", V, F, np.concatenate(allUV), np.concatenate(allR), np.concatenate(allG), B, mir)
    # a flat species swaps sides later; the mirror flag marks the half drawn on shared texels
    return part


def _fan_normals_ok(part):
    return part


# ------------------------------------------------------------------ fins
def _grid_tris(nc, nr):
    """Triangles of a (nc+1) x nr vertex grid (column-major: v = c*nr + r)."""
    T = []
    for c in range(nc):
        for r in range(nr - 1):
            a, b = c * nr + r, c * nr + r + 1
            cc, d = (c + 1) * nr + r + 1, (c + 1) * nr + r
            T += [(a, b, cc), (a, cc, d)]
    return T


def build_median_fin(body, f, lod, flat_unused=None):
    L = body.L
    nc = max(3, int(round(f.get("segs", 9) * lod["fin_seg"])))
    nrow = lod["fin_rows"]
    kind = f["kind"]
    if kind == "caudal":
        return build_caudal(body, f, lod, nc, nrow)
    sign = 1.0 if f["side"] == "dorsal" else -1.0
    fr = np.linspace(0, 1, nc + 1)
    t0, t1 = f["t0"], f["t1"]
    ts = t0 + (t1 - t0) * fr
    th = 0.0 if sign > 0 else math.pi
    pb = body.point(ts, th, 0)
    sink = np.minimum(0.0035 * L, 0.25 * np.abs(pb[:, 2]))
    base = pb.copy()
    base[:, 2] -= sign * sink
    hmax = f["h"] * L
    h = hmax * fin_profile(fr, f.get("shape", "rounded"), f.get("peak", 0.5), tuple(f.get("ends", (0, 0))), f.get("power", 1.0))
    rake = math.radians(f.get("rake", 14))
    edge = base.copy()
    edge[:, 0] -= h * math.tan(rake)          # leans back (towards -X)
    edge[:, 2] += sign * h
    rows = np.linspace(0, 1, nrow)
    V = np.zeros((nc + 1, nrow, 3))
    for k, r in enumerate(rows):
        V[:, k] = base + r * (edge - base)
    uvl = np.zeros((nc + 1, nrow, 2))
    uvl[..., 0] = fr[:, None]
    uvl[..., 1] = rows[None, :]
    tt = (0.5 - V[..., 0] / L)
    R = tt
    G = body.flex(tt) * (1 - rows[None, :]) + rows[None, :]
    F = np.array(_grid_tris(nc, nrow))
    P = V.reshape(-1, 3)[F]
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    if n[:, 1].sum() < 0:
        F = F[:, ::-1]
    base_len = float(np.linalg.norm(np.diff(base, axis=0), axis=1).sum())
    ray_len = float(np.max(np.linalg.norm(edge - base, axis=1)))
    return Part(f["name"], "fin", V.reshape(-1, 3), F, uvl.reshape(-1, 2), R.ravel(), G.ravel(), np.zeros(V.shape[0] * V.shape[1]),
                extra=dict(spec=f, dims=(base_len, ray_len)))


def build_caudal(body, f, lod, nc, nrow):
    L = body.L
    nc = max(3, int(round(f.get("segs", 12) * lod["fin_seg"])))
    tc0 = body.tend - f.get("overlap", 0.012)
    tb = np.array([tc0])
    top0 = float(body.top(tb)[0]) * L * 0.80
    bot0 = float(body.bot(tb)[0]) * L * 0.80
    if "base_h" in f:
        top0 = bot0 = f["base_h"] * L
    st, sb = f["span_top"] * L, f["span_bot"] * L
    v = np.linspace(0, 1, nc + 1)
    xb = (0.5 - tc0) * L
    base = np.stack([np.full_like(v, xb), np.zeros_like(v), -bot0 + (top0 + bot0) * v], 1)
    ext = caudal_ext(v, f.get("shape", "rounded"), f.get("fork", 0.4)) * (1 - tc0) * L
    far = np.stack([xb - ext, np.zeros_like(v), -sb + (st + sb) * v], 1)
    rows = np.linspace(0, 1, nrow)
    V = np.zeros((nc + 1, nrow, 3))
    for k, r in enumerate(rows):
        V[:, k] = base + (r ** 1.0) * (far - base)
    uvl = np.zeros((nc + 1, nrow, 2))
    uvl[..., 0] = v[:, None]
    uvl[..., 1] = rows[None, :]
    tt = 0.5 - V[..., 0] / L
    G = body.flex(tt) * (1 - rows[None, :]) + rows[None, :]
    F = np.array(_grid_tris(nc, nrow))
    P = V.reshape(-1, 3)[F]
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    if n[:, 1].sum() < 0:
        F = F[:, ::-1]
    return Part(f["name"], "fin", V.reshape(-1, 3), F, uvl.reshape(-1, 2), tt.ravel(), G.ravel(), np.zeros(V.shape[0] * V.shape[1]),
                extra=dict(spec=f, dims=(st + sb, float(ext.max()))))


def build_paired_fin(body, f, lod, side):
    """Pectoral / pelvic fan on the body side; side -1 right, +1 left."""
    L = body.L
    nc = max(3, int(round(f.get("segs", 7) * lod["fin_seg"])))
    nrow = lod["fin_rows"]
    a = np.linspace(0, 1, nc + 1)
    tb = f["t"] + f.get("dt", 0.0) * a
    qb = f["q0"] + (f["q1"] - f["q0"]) * a
    base = np.zeros((nc + 1, 3))
    nrm = np.zeros((nc + 1, 3))
    for k in range(nc + 1):
        th = body.q_theta(tb[k], qb[k])
        p = body.point(tb[k], th, side)
        e = 1e-3
        pu = body.point(tb[k], th + e, side)
        pv = body.point(tb[k] + e, th, side)
        n = np.cross(pu - p, pv - p)
        n = n / np.linalg.norm(n)
        if n[1] * side < 0:
            n = -n
        nrm[k] = n
        base[k] = p - n * f.get("sink", 0.004) * L
    phi = np.radians(f.get("ang0", -30) + (f.get("ang1", 50) - f.get("ang0", -30)) * a)
    splay = math.radians(f.get("splay", 30))
    back = np.stack([-np.cos(phi), np.zeros_like(phi), -np.sin(phi)], 1)      # towards the tail (-X), phi down
    d = np.cos(splay) * back + np.sin(splay) * nrm
    # remove the part into the body: keep rays tangential-ish
    d /= np.linalg.norm(d, axis=1)[:, None]
    shape = f.get("shape", "rounded")
    if shape == "rounded":
        ln = 0.55 + 0.45 * np.sqrt(np.maximum(1 - (2 * a - 1) ** 2, 0))
    elif shape == "pointed":
        ln = 1 - 0.7 * a ** 1.2
    elif shape == "fan":
        ln = 0.85 + 0.15 * np.sin(np.pi * a)
    else:
        ln = np.ones_like(a)
    ln = ln * f["len"] * L
    rows = np.linspace(0, 1, nrow)
    V = np.zeros((nc + 1, nrow, 3))
    for k, r in enumerate(rows):
        V[:, k] = base + r * ln[:, None] * d
    uvl = np.zeros((nc + 1, nrow, 2))
    uvl[..., 0] = a[:, None]
    uvl[..., 1] = rows[None, :]
    tt = 0.5 - V[..., 0] / L
    G = body.flex(tt) * (1 - rows[None, :]) + rows[None, :]
    F = np.array(_grid_tris(nc, nrow))
    P = V.reshape(-1, 3)[F]
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    # keep the face towards the outside (arbitrary but consistent per side)
    if n[:, 1].sum() * side < 0:
        F = F[:, ::-1]
    blen = float(np.linalg.norm(np.diff(base, axis=0), axis=1).sum()) + 1e-4
    return Part(f["name"] + ("_L" if side > 0 else "_R"), "fin", V.reshape(-1, 3), F, uvl.reshape(-1, 2), tt.ravel(), G.ravel(),
                np.ones(V.shape[0] * V.shape[1]), extra=dict(spec=f, dims=(blen, float(ln.max())), side=side))


def mirrored(part, name, body_flat=False):
    """Mirror a right-side part to the left (y -> -y) with flipped winding; same UVs, mirror flag set."""
    V = part.V * np.array([1, -1, 1])
    return Part(name, part.kind, V, part.F[:, ::-1], part.uvl.copy(), part.R.copy(), part.G.copy(), part.B.copy(),
                mirror=np.ones(len(part.F), bool), rect_key=part.rect_key, extra=dict(part.extra, side=1))


# ------------------------------------------------------------------ eyes and extras
def build_eye(body, e, lod, side):
    """A low ellipsoidal dome proud of the skin, planar uv (local a, b) -> disc in its rect."""
    L = body.L
    r = e["r"] * L
    t = e["t"]
    th = body.q_theta(t, e["q"])
    P = body.point(t, th, side)
    e1 = 1e-3
    pu = body.point(t, th + e1, side)
    pv = body.point(t + e1, th, side)
    n = np.cross(pu - P, pv - P)
    n = n / np.linalg.norm(n)
    if n[1] * side < 0:
        n = -n
    ax = np.array([-1.0, 0, 0])
    ax = ax - n * ax.dot(n)
    ax /= np.linalg.norm(ax)
    bx = np.cross(n, ax)
    hz = e.get("height", 0.75) * r
    sunk = e.get("sunk", 0.30) * r
    C = P - n * sunk
    nl, ns = lod["eye_lat"], lod["eye_seg"]
    pts = [(0.0, 0.0, 1.0)]              # local (a, b, c)
    for i in range(1, nl + 1):
        phi = (i / nl) * (math.pi / 2) * 0.98
        for k in range(ns):
            az = 2 * math.pi * k / ns
            pts.append((math.sin(phi) * math.cos(az), math.sin(phi) * math.sin(az), math.cos(phi)))
    pts = np.array(pts)
    V = C + (pts[:, 0:1] * ax + pts[:, 1:2] * bx) * r + pts[:, 2:3] * n * hz
    uvl = np.stack([0.5 + 0.5 * pts[:, 0] * 0.96, 0.5 + 0.5 * pts[:, 1] * 0.96], 1)
    # keep the planar (a, b) uv; the pupil sits at the apex
    F = []
    for k in range(ns):
        F.append((0, 1 + k, 1 + (k + 1) % ns))
    for i in range(1, nl):
        for k in range(ns):
            a, b = 1 + (i - 1) * ns + k, 1 + (i - 1) * ns + (k + 1) % ns
            c, d = 1 + i * ns + (k + 1) % ns, 1 + i * ns + k
            F += [(a, d, c), (a, c, b)]
    F = np.array(F)
    Pf = V[F]
    nn = np.cross(Pf[:, 1] - Pf[:, 0], Pf[:, 2] - Pf[:, 0])
    cen = Pf.mean(1)
    if np.einsum("ij,ij->i", nn, cen - C).sum() < 0:
        F = F[:, ::-1]
    tt = 0.5 - V[:, 0] / L
    return Part("eye", "eye", V, F, uvl, tt, body.flex(tt), np.zeros(len(V)), extra=dict(spec=e, dims=(2 * r, 2 * r), side=side))


def build_barbel(body, e, lod):
    L = body.L
    t = e["t"]
    th = math.pi * e.get("p", 0.97)
    p0 = body.point(t, th, 0)
    ln = e["len"] * L
    r0 = e["r"] * L
    nseg = lod["bar_seg"]
    ns = lod["bar_ring"]
    dirv = np.array([-e.get("back", 0.15), 0, -1.0])
    dirv /= np.linalg.norm(dirv)
    sidev = np.array([0, 1.0, 0])
    third = np.cross(dirv, sidev)
    V = []
    for i in range(nseg + 1):
        f = i / nseg
        c = p0 + dirv * ln * f + dirv * (-0.002 * L)
        rad = r0 * (1 - 0.85 * f)
        for k in range(ns + 1):          # last column repeats the first (uv seam)
            az = 2 * math.pi * (k % ns) / ns
            V.append(c + rad * (math.cos(az) * sidev + math.sin(az) * third))
    V = np.array(V)
    uvl = np.array([(i / nseg, k / ns) for i in range(nseg + 1) for k in range(ns + 1)])
    F = []
    for i in range(nseg):
        for k in range(ns):
            a, b = i * (ns + 1) + k, i * (ns + 1) + k + 1
            c, d = (i + 1) * (ns + 1) + k + 1, (i + 1) * (ns + 1) + k
            F += [(a, b, c), (a, c, d)]
    F = np.array(F)
    Pf = V[F]
    nn = np.cross(Pf[:, 1] - Pf[:, 0], Pf[:, 2] - Pf[:, 0])
    cen = Pf.mean(1)
    axis_c = p0 + dirv * ((cen - p0) @ dirv)[:, None] if False else p0 + np.outer((cen - p0) @ dirv, dirv)
    if np.einsum("ij,ij->i", nn, cen - axis_c).sum() < 0:
        F = F[:, ::-1]
    tt = 0.5 - V[:, 0] / L
    return Part("barbel", "extra", V, F, uvl, tt, body.flex(tt), np.zeros(len(V)), extra=dict(spec=e, dims=(ln, 2 * r0)))


# ------------------------------------------------------------------ bumps: cones (tubercles, spines) and the sucker disc
def surf_frame(body, t, th, side):
    """Surface point and outward unit normal at (t, theta, side)."""
    p = body.point(t, th, side)
    e = 1e-3
    pu = body.point(t, th + e if th < math.pi - 2 * e else th - e, side)
    pv = body.point(t + e, th, side)
    n = np.cross(pu - p, pv - p)
    n = n / max(np.linalg.norm(n), 1e-12)
    c = np.array([p[0], 0.0, 0.5 * float(body.point(t, 0.0, 0)[2] + body.point(t, math.pi, 0)[2])])
    if np.dot(n, p - c) < 0:
        n = -n
    return p, n


def build_cones(body, e, lod, name="cones"):
    """Small cones / domes standing on the skin: tubercles, spines.  Items by `items` (t, q, r, h, lean, mid) and/or `rows`
    (q, t0, t1, n, r, h, jit, qjit, zig, var, lean, mid).  Both flanks unless mid / q == 0.  All cones share one small uv rect
    (first cone real, the rest flagged as mirror copies)."""
    L = body.L
    rng = np.random.default_rng(int(e.get("seed", 1)))
    items = [dict(it) for it in e.get("items", [])]
    for row in e.get("rows", []):
        n = int(row["n"])
        ts = np.linspace(row["t0"], row["t1"], n)
        step = (row["t1"] - row["t0"]) / max(n - 1, 1)
        for i, t in enumerate(ts):
            sc = 1 + (rng.random() - 0.5) * row.get("var", 0.3)
            items.append(dict(t=t + (rng.random() - 0.5) * row.get("jit", 0.3) * step,
                              q=row["q"] + (rng.random() - 0.5) * row.get("qjit", 0.0) + (row.get("zig", 0.0) if i % 2 else 0.0),
                              r=row["r"] * sc, h=row["h"] * sc, lean=row.get("lean", 0.0), mid=row.get("mid", row["q"] <= 0.0)))
    items = items[::max(1, int(lod.get("bump_stride", 1)))]
    ns = max(3, int(round(e.get("seg", 6) * lod.get("bump_k", 1.0))))
    bands = e.get("bands", 1) if lod.get("bump_k", 1.0) >= 1.0 else 1
    sink = e.get("sink", 0.35)
    Vs, Fs, UVs, Ms = [], [], [], []
    off = 0
    first = True
    for it in items:
        sides = [0] if it.get("mid") else [-1, 1]
        for s in sides:
            th = 0.0 if s == 0 else body.q_theta(it["t"], it["q"])
            p, n = surf_frame(body, it["t"], th, s)
            back = np.array([-1.0, 0.0, 0.0])
            back = back - n * back.dot(n)
            back /= np.linalg.norm(back)
            lean = it.get("lean", 0.0)
            d = n * (1 - lean) + back * lean
            d /= np.linalg.norm(d)
            r, h = it["r"] * L, it["h"] * L
            C = p - d * sink * h
            a1 = np.cross(d, back if abs(d.dot(back)) < 0.95 else np.array([0, 1.0, 0]))
            a1 /= np.linalg.norm(a1)
            a2 = np.cross(d, a1)
            levels = [(0.0, 1.0)] if bands == 1 else [(0.0, 1.0), (0.5, 0.62)]
            V, UV = [], []
            for fz, fr in levels:
                for k in range(ns + 1):
                    az = 2 * math.pi * (k % ns) / ns
                    ring = (math.cos(az) * a1 + math.sin(az) * a2) * r * fr
                    V.append(C + ring if fz == 0 else C + d * h * (sink + fz) + ring)
                    UV.append((k / ns, fz))
            V.append(p + d * h)
            UV.append((0.5, 1.0))
            V, UV = np.array(V), np.array(UV)
            tip = len(V) - 1
            F = []
            for b in range(len(levels)):
                for k in range(ns):
                    a, bb = b * (ns + 1) + k, b * (ns + 1) + k + 1
                    if b + 1 < len(levels):
                        c, dd = (b + 1) * (ns + 1) + k + 1, (b + 1) * (ns + 1) + k
                        F += [(a, bb, c), (a, c, dd)]
                    else:
                        F.append((a, bb, tip))
            F = np.array(F)
            Pf = V[F]
            nn = np.cross(Pf[:, 1] - Pf[:, 0], Pf[:, 2] - Pf[:, 0])
            cen = Pf.mean(1)
            axp = C + np.outer((cen - C) @ d, d)
            if np.einsum("ij,ij->i", nn, cen - axp).sum() < 0:
                F = F[:, ::-1]
            Vs.append(V)
            Fs.append(F + off)
            UVs.append(UV)
            Ms.append(np.full(len(F), not first))
            first = False
            off += len(V)
    V = np.concatenate(Vs)
    F = np.concatenate(Fs)
    tt = 0.5 - V[:, 0] / L
    rr = float(np.mean([it["r"] for it in items])) * L
    hh = float(np.mean([it["h"] for it in items])) * L
    return Part(name, "bump", V, F, np.concatenate(UVs), tt, body.flex(tt), np.zeros(len(V)), mirror=np.concatenate(Ms),
                extra=dict(spec=e, dims=(2 * math.pi * rr, hh * (1 + sink))))


def build_sucker(body, e, lod, name="sucker"):
    """Round sucker disc on the belly (lumpsucker): raised rim, dished centre.  t, a (half-length, fraction of L), b (half-width, fraction of L
    measured around the skin), h (rim height), planar disc uv."""
    L = body.L
    nr, nsg = lod.get("sucker_rings", 5), lod.get("sucker_seg", 20)
    t0, a, b, h = e["t"], e["a"], e["b"] * L, e["h"] * L
    sink = e.get("sink", 0.15) * h

    def pt(rho, az):
        lon, lat = rho * math.cos(az) * a, rho * math.sin(az) * b
        t = t0 + lon
        arcs, thg = body.arcs(np.array([t]))
        ar = arcs[0]
        s = ar[-1] - abs(lat)
        th = float(np.interp(s, ar, thg))
        side = 0 if abs(lat) < 1e-9 else (1 if lat > 0 else -1)
        p, n = surf_frame(body, t, th, side)
        ss = smoothstep(0.74, 1.0, rho)
        hp = h * (1 - ss) * (0.40 + 0.60 * smoothstep(0.0, 0.70, rho)) - sink * ss
        return p + n * hp, n
    V, UV, N = [], [], []
    p, n = pt(0.0, 0.0)
    V.append(p); N.append(n); UV.append((0.5, 0.5))
    for i in range(1, nr + 1):
        rho = i / nr
        for k in range(nsg):
            az = 2 * math.pi * k / nsg
            p, n = pt(rho, az)
            V.append(p); N.append(n)
            UV.append((0.5 + 0.5 * 0.96 * rho * math.cos(az), 0.5 + 0.5 * 0.96 * rho * math.sin(az)))
    V, UV, N = np.array(V), np.array(UV), np.array(N)
    F = [(0, 1 + k, 1 + (k + 1) % nsg) for k in range(nsg)]
    for i in range(1, nr):
        for k in range(nsg):
            a_, b_ = 1 + (i - 1) * nsg + k, 1 + (i - 1) * nsg + (k + 1) % nsg
            c_, d_ = 1 + i * nsg + (k + 1) % nsg, 1 + i * nsg + k
            F += [(a_, d_, c_), (a_, c_, b_)]
    F = np.array(F)
    Pf = V[F]
    nn = np.cross(Pf[:, 1] - Pf[:, 0], Pf[:, 2] - Pf[:, 0])
    nb = N[F].mean(1)
    if np.einsum("ij,ij->i", nn, nb).sum() < 0:
        F = F[:, ::-1]
    tt = 0.5 - V[:, 0] / L
    return Part(name, "bump", V, F, UV, tt, body.flex(tt), np.zeros(len(V)),
                extra=dict(spec=e, dims=(2 * a * L, 2 * b)))


# ------------------------------------------------------------------ layout (texture atlas plan)
class Layout:
    def __init__(self, body, spec, parts_dims):
        self.body = body
        L = body.L
        self.flat = body.flat
        nface = 2 if self.flat else 1
        self.dens_u = (W - 2 * PAD) / (body.tmax * L)
        maxarc = body.max_arc()
        best = None
        for kv in (1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.42):
            bh = int(math.ceil(nface * maxarc * self.dens_u * kv)) + 2 * PAD + (PAD if nface == 2 else 0)
            for fs in (1.0, 0.85, 0.7, 0.55, 0.45, 0.35, 0.28):
                rects = self._pack(parts_dims, fs, bh)
                if rects is not None:
                    score = min(kv, fs) + 0.01 * kv
                    if best is None or score > best[0]:
                        best = (score, kv, fs, bh, rects)
        if best is None:
            raise RuntimeError("texture layout does not fit")
        _, self.kv, self.fs, self.body_h, self.rects = best
        self.dens_v = self.dens_u * self.kv
        self.nface = nface
        self.face_h = int((self.body_h - 2 * PAD - (PAD if nface == 2 else 0)) // nface)

    def _pack(self, dims, fs, y0):
        items = []
        for key, (w, h, minpx) in dims.items():
            pw = max(minpx, int(math.ceil(w * self.dens_u * fs)))
            ph = max(minpx, int(math.ceil(h * self.dens_u * fs)))
            items.append((key, pw, ph))
        items.sort(key=lambda it: -it[2])
        x, y, rowh = PAD, y0, 0
        out = {}
        for key, pw, ph in items:
            if pw > W - 2 * PAD:
                return None
            if x + pw + PAD > W:
                x, y, rowh = PAD, y + rowh + PAD, 0
            out[key] = (x, y, pw, ph)
            x += pw + PAD
            rowh = max(rowh, ph)
            if y + ph + PAD > H:
                return None
        return out

    def body_uv(self, t, arc, side):
        """texel position of a body point -> uv; side -1 right / mirror region, +1 left (flat only)."""
        x = PAD + t / self.body.tmax * (W - 2 * PAD)
        row0 = PAD
        if self.flat and side > 0:
            row0 = PAD + self.face_h + PAD
        y = row0 + arc * self.dens_v
        return x / W, 1 - y / H

    def rect_uv(self, key, uvl):
        x0, y0, w, h = self.rects[key]
        x = x0 + 0.5 + uvl[:, 0] * (w - 1)
        y = y0 + 0.5 + (1 - uvl[:, 1]) * (h - 1)
        return np.stack([x / W, 1 - y / H], 1)


# ------------------------------------------------------------------ assemble
LODS = {
    0: dict(m=8, rings=40, nose_rings=4, fin_seg=1.0, fin_rows=3, eye_lat=4, eye_seg=12, bar_seg=4, bar_ring=5,
            bump_stride=1, bump_k=1.0, sucker_rings=5, sucker_seg=20),
    1: dict(m=5, rings=14, nose_rings=2, fin_seg=0.5, fin_rows=2, eye_lat=2, eye_seg=6, bar_seg=2, bar_ring=3,
            bump_stride=2, bump_k=0.6, sucker_rings=3, sucker_seg=12),
}


def lod_params(spec, lod):
    d = dict(LODS[lod])
    d.update(spec.get("lod%d" % lod, {}))
    return d


def make_parts(spec, lod_idx, layout=None, lod_override=None, body=None):
    L = spec["length"]
    body = body or Body(spec, L)
    lod = lod_override or lod_params(spec, lod_idx)
    flat = body.flat
    dummy = layout is None
    if dummy:
        class _L:       # dims pass: no real uv
            def body_uv(self, t, a, s):
                return 0.0, 0.0
        lay = _L()
    else:
        lay = layout
    parts = [build_body(body, spec, lod, lay)]
    for ei, e in enumerate(spec.get("eyes", [])):
        eye = build_eye(body, e, lod, 1 if e.get("left") else -1)
        eye.name = "eye%d" % ei
        eye.rect_key = eye.name
        parts.append(eye)
        if e.get("both", not flat):
            m = mirrored(eye, eye.name + "_L")
            m.rect_key = eye.rect_key
            parts.append(m)
    for f in spec.get("fins", []):
        if f["kind"] in ("median", "caudal"):
            p = build_median_fin(body, f, lod)
            p.rect_key = p.name
            parts.append(p)
        else:
            p = build_paired_fin(body, f, lod, -1)
            p.rect_key = p.name
            parts.append(p)
            if flat:
                q = build_paired_fin(body, f, lod, +1)
                q.rect_key = q.name
                q.extra["blind"] = True
                parts.append(q)
            else:
                q = mirrored(p, p.name.replace("_R", "_L"))
                q.rect_key = p.rect_key
                parts.append(q)
    for xi, ex in enumerate(spec.get("extras", [])):
        if ex["kind"] == "barbel":
            p = build_barbel(body, ex, lod)
            p.rect_key = "barbel"
            parts.append(p)
        elif ex["kind"] in ("cones", "sucker"):
            p = (build_cones if ex["kind"] == "cones" else build_sucker)(body, ex, lod, ex.get("name", "%s%d" % (ex["kind"], xi)))
            p.rect_key = "bump%d" % xi
            parts.append(p)
    return body, parts


def plan_layout(spec):
    body, parts = make_parts(spec, 0, None)
    dims = {}
    for p in parts:
        if p.kind == "body" or p.mirror.all():
            continue
        k = p.rect_key
        w, h = p.extra["dims"]
        if p.kind == "eye":
            w, h = w * 1.7, h * 1.7
        dims[k] = (w, h, 10 if p.kind == "fin" else 32 if p.kind == "eye" else 16)
    return Layout(body, spec, dims)


def finalize(spec, lod_idx, layout, lod_override=None):
    """Build all parts with real UVs; returns (body, parts) with part.uv (n,2) set, rotated for flat species."""
    body, parts = make_parts(spec, lod_idx, layout, lod_override)
    for p in parts:
        if p.kind == "body":
            p.uv = p.uvl.copy()
        else:
            p.uv = layout.rect_uv(p.rect_key, p.uvl)
    if body.flat:   # lie on the left side, right (eyed) side up:  (y, z) -> (z, -y)
        for p in parts:
            p.V = np.stack([p.V[:, 0], p.V[:, 2], -p.V[:, 1]], 1)
    return body, parts


def tri_count(parts):
    return sum(len(p.F) for p in parts)


def tuned_lod(spec, lod_idx, target):
    """Pick the ring count so the part total comes out close to the target triangle count."""
    lod = lod_params(spec, lod_idx)
    if lod_idx == 1 and "m" not in spec.get("lod1", {}):
        lod["m"] = max(4, int(round(lod["m"] * 0.6)))
    for _ in range(4):
        b, parts = make_parts(spec, lod_idx, None, lod)
        tot = tri_count(parts)
        per_ring = (len(parts[0].F) - 4 * lod["m"]) / max(lod["rings"] - 1, 1)
        lod["rings"] = int(max(lod["nose_rings"] + 4, round(lod["rings"] + (target - tot) / per_ring)))
    return lod
