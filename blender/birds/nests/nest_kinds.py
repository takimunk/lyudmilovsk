"""Open nests of the kit: stick platforms, stick dome, woven / mud cups, scrapes, reed mounds, the swallow's half cup.

build_open(id, spec, stage) -> MB.  stage in B1 B2 Full Old.  The same seed builds the same element list for every
stage; B1 / B2 keep the first third / two thirds of it, Full adds the lining, Old drops and flattens and greys.
"""
import math
import zlib

import numpy as np

from nest_geo import MB, A, nrm

FRAC = {"B1": 0.34, "B2": 0.67, "Full": 1.0, "Old": 1.0}
WEATHER = {"bark_brown": "bark_silver", "bark_grey": "bark_silver", "bark_dark": "bark_grey", "bark_pale": "bark_silver",
           "bark_green": "bark_grey", "grass_dry": "grass_bleached", "grass_fine": "grass_bleached",
           "moss_green": "moss_dry", "mud_wet": "mud_dry", "earth": "mud_dry", "feather": "down", "rootlet": "bark_dark",
           "reed": "grass_bleached", "seaweed": "bark_dark", "leaf_dead": "moss_dry", "wool": "down"}


def wc(cell, old):
    return WEATHER.get(cell, cell) if old else cell


def pick(rng, table):
    w = np.array([x[1] for x in table], float)
    return table[int(rng.choice(len(table), p=w / w.sum()))][0]


def seg_min_dist2(c, u, h):
    t = np.clip(-np.dot(c[:2], u[:2]), -h, h)
    q = c[:2] + t * u[:2]
    return float(np.hypot(*q))


# =============================================================================================== stick platform
def sticks_platform(mb, sp, stage, rng, R):
    old = stage == "Old"
    t, d, rin, N = sp["t"], sp["d"], sp["rin"], sp["N"]
    rr, Lr, sides = sp["rr"], sp["L"], sp.get("sides", 5)
    f0, f1 = sp.get("f", (0.4, 0.33))
    lined = sp.get("lining") is not None
    out = []
    n0, n1 = int(N * f0), int(N * (f0 + f1))
    for i in range(N):
        layer = 0 if i < n0 else (1 if i < n1 else 2)
        r = rng.uniform(*rr) / 1000.0
        L = rng.uniform(*Lr)
        cell = pick(rng, sp["cells"])
        if layer == 0:
            rho = R * 0.85 * math.sqrt(rng.random())
            phi = rng.uniform(0, 6.283)
            c = A(rho * math.cos(phi), rho * math.sin(phi), 0)
            a = rng.uniform(0, 6.283)
            u = A(math.cos(a), math.sin(a), 0)
            k = i / max(n0 - 1, 1)
            zc = -t + k * (t - 2.2 * r) + 0.25 * t * (rho / R) ** 2 * (1 - k)
            if not lined and k > 0.7:
                zc = -r
            zc = min(zc, -r - (0.0015 if lined else 0.0))
        else:
            ok = False
            for _ in range(12):
                rho = rng.uniform(rin * 1.05, R * 0.82)
                phi = rng.uniform(0, 6.283)
                c = A(rho * math.cos(phi), rho * math.sin(phi), 0)
                tang = A(-math.sin(phi), math.cos(phi), 0)
                rad = A(math.cos(phi), math.sin(phi), 0)
                dl = rng.normal(0, 0.5)
                u = tang * math.cos(dl) + rad * math.sin(dl)
                if seg_min_dist2(c, u, L / 2) > rin * 1.03:
                    ok = True
                    break
            if not ok:
                u = tang
                rho = max(rho, rin * 1.12)
                c = A(rho * math.cos(phi), rho * math.sin(phi), 0)
            hw = d * max(0.0, 1 - ((rho - rin) / max(R - rin, 1e-6)) ** 1.5)
            lo, hi = ((-t * 0.3, hw * 0.55) if layer == 1 else (hw * 0.5, hw * 0.95))
            zc = rng.uniform(min(lo, hi), max(lo, hi))
            zc = min(zc, d - r)
        # keep both ends inside R
        h = L / 2
        for _ in range(12):
            if max(np.hypot(*(c[:2] + u[:2] * h)), np.hypot(*(c[:2] - u[:2] * h))) <= R * 0.995:
                break
            h *= 0.85
        h = max(h, 0.02)
        c = A(c[0], c[1], zc)
        np_ = 4 if h * 2 > 0.35 else 3
        side = nrm(np.cross(u, A(0, 0, 1)))
        pts = []
        sag = rng.normal(0, 0.035) * h * 2
        for k in range(np_):
            s = -1 + 2 * k / (np_ - 1)
            bow = (1 - s * s)
            pts.append(c + u * h * s + side * sag * bow + A(0, 0, rng.normal(0, 0.003) + 0.02 * h * bow * rng.normal()))
        out.append(dict(layer=layer, pts=pts, r=r, cell=cell, sides=sides if r > 0.0022 else 4, i=i))
    return out


def lining_bowl(mb, sp, rin, d, cell, floor=0.0, depth_scale=1.0, rough=0.02, n=24, part=None, rim=1.12):
    dd = d * depth_scale
    prof = [(0, floor), (0.62 * rin, floor), (0.82 * rin, floor + 0.08 * dd), (0.93 * rin, floor + 0.35 * dd),
            (0.99 * rin, floor + 0.7 * dd), (rim * rin, floor + dd)]
    wav = min(0.0005, rin * 0.004)
    p = part or mb.part("lining")
    rings, s0 = mb.revolve(prof, n, cell, p, amp=rough, wavy=wav)
    # keep the floor itself flat at floor level: re-set the centre ring exactly (eggs rest on it)
    mb.orient(s0, mb.nfaces(), lambda c: A(-c[0] * 0.3, -c[1] * 0.3, 1.0))
    return rings


def build_platform(mb, sp, stage, rng, R):
    old = stage == "Old"
    fr = FRAC[stage]
    st = sticks_platform(mb, sp, stage, rng, R)
    keep_layer = {"B1": 0, "B2": 1, "Full": 2, "Old": 2}[stage]
    lined = sp.get("lining") is not None
    rin, d, t = sp["rin"], sp["d"], sp["t"]
    for s in st:
        if s["layer"] > keep_layer:
            continue
        if old and rng.random() < 0.38:
            continue
        if stage == "Full" and lined and s["layer"] == 0 and max(np.hypot(p[0], p[1]) for p in s["pts"]) < rin * 0.9:
            continue
        p = mb.part("stick")
        cell = wc(s["cell"], old)
        mb.tube(s["pts"], s["r"], cell, p, sides=s["sides"])
    if sp.get("core", True) and lined:
        ccell = wc("bark_dark", old)
        if stage == "B1":
            cp = [(0, -t), (0.5 * R, -t), (0.76 * R, -t * 0.8), (0.78 * R, -t * 0.5), (0.6 * R, -t * 0.32), (0, -t * 0.32)]
        elif stage == "B2":
            cp = [(0, -t), (0.5 * R, -t), (0.76 * R, -t * 0.8), (0.78 * R, -t * 0.4), (0.72 * R, 0.12 * d), (rin * 1.2, 0.3 * d),
                  (rin * 1.0, 0.1 * d), (0.7 * rin, -0.003), (0, -0.003)]
        else:
            cp = [(0, -t), (0.5 * R, -t), (0.76 * R, -t * 0.8), (0.78 * R, -t * 0.3), (0.7 * R, 0.3 * d), (rin * 1.15, 0.55 * d),
                  (rin * 1.0, 0.5 * d), (0.9 * rin, 0.15 * d), (0.7 * rin, -0.0035), (0, -0.0035)]
        rr_, s0 = mb.revolve(cp, 24 if R < 0.5 else 30, ccell, mb.part("core"), amp=0.02, wavy=0.0)
        mb.orient_group(s0, mb.nfaces(), A(0, 0, -1))
    if stage in ("Full", "Old") and lined:
        cin = wc(sp["lining"], old)
        lining_bowl(mb, sp, rin, d, cin, depth_scale=0.55 if old else 1.0, rough=0.03 if old else 0.02, n=24 if rin > 0.06 else 18)
        # a few strands of the lining curling on the floor / over the rim
        for k in range(sp.get("curls", 10) // (2 if old else 1)):
            ph = rng.uniform(0, 6.283)
            rho = rin * rng.uniform(0.35, 0.8)
            pts = []
            for q in range(3):
                a = ph + q * 0.55
                rr_ = rho * (1 + 0.1 * q)
                pts.append(A(rr_ * math.cos(a), rr_ * math.sin(a), 0.0012 + 0.001 * q))
            cell = wc(sp.get("curl_cell", "grass_fine"), old)
            mb.tube(pts, 0.0012 * (1 if sp["rin"] < 0.1 else 1.6), cell, mb.part("curl"), sides=3, flat=0.6)
    if old:
        for k in range(5):
            phi = rng.uniform(0, 6.283)
            rho = R * rng.uniform(0.7, 0.95)
            p0 = A(rho * math.cos(phi), rho * math.sin(phi), d * 0.2)
            dr = A(math.cos(phi), math.sin(phi), 0)
            Ls = R * rng.uniform(0.25, 0.5)
            pts = [p0, p0 + dr * Ls * 0.3 - A(0, 0, Ls * 0.15), p0 + dr * Ls * 0.45 - A(0, 0, Ls * 0.55)]
            mb.tube(pts, 0.0012 * (R / 0.3) ** 0.5, "grass_bleached", mb.part("hang"), sides=3, flat=0.5)
        s = 0.7
    return mb


# =============================================================================================== magpie dome
def build_dome(mb, sp, stage, rng, R):
    old = stage == "Old"
    base = dict(sp, t=0.07, d=0.045, rin=0.11, N=sp["Nbase"], rr=(3, 6), L=(0.15, 0.5), f=(0.55, 0.45), lining="mud_wet",
                curls=0)
    keep = {"B1": 0, "B2": 1, "Full": 2, "Old": 2}[stage]
    st = sticks_platform(mb, base, stage, rng, R)
    for s in st:
        if s["layer"] > min(keep, 1):
            continue
        if old and rng.random() < 0.4:
            continue
        mb.tube(s["pts"], s["r"], wc(s["cell"], old), mb.part("stick"), sides=s["sides"])
    if stage in ("Full", "Old"):
        lining_bowl(mb, base, 0.11, 0.045, wc("mud_wet", old), depth_scale=1.0, rough=0.02, n=22)
    # dome sticks on a sphere; the doorway faces +X
    Rs, zc = 0.285, 0.06
    door = A(math.cos(0.0) * math.cos(0.45), 0, math.sin(0.45))
    n_dome = sp["Ndome"]
    made = 0
    tries = 0
    while made < n_dome and tries < n_dome * 20:
        tries += 1
        e1, e2 = rng.uniform(0.05, 1.45), rng.uniform(0.05, 1.45)
        a1 = rng.uniform(-math.pi, math.pi)
        a2 = a1 + rng.normal(0, 1.1)
        if max(e1, e2) < 0.25:
            continue
        def sph(e, a, rr=1.0):
            return A(math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)) * rr
        p1, p2 = sph(e1, a1), sph(e2, a2)
        if np.dot(p1, p2) < -0.2:
            continue
        pts = []
        bad = False
        for k in range(3):
            s = k / 2
            q = nrm(p1 * (1 - s) + p2 * s)
            if np.dot(q, door) > 0.93:
                bad = True
            pts.append(q * Rs * (1.0 + rng.uniform(-0.01, 0.07)) + A(0, 0, zc))
        if bad:
            continue
        lowest = max(e1, e2)
        layer = 1 if lowest < 0.7 else 2
        if layer > keep:
            made += 1
            continue
        if old and rng.random() < 0.45:
            made += 1
            continue
        # extend both ends a little
        pts[0] = pts[0] + (pts[0] - pts[1]) * 0.12
        pts[2] = pts[2] + (pts[2] - pts[1]) * 0.12
        pts[1] = pts[1] + A(0, 0, 0)
        r = rng.uniform(0.0022, 0.0045)
        mb.tube(pts, r, wc(pick(rng, sp["cells"]), old), mb.part("stick"), sides=4, up=(0, 0, 1))
        made += 1
    return mb


# =============================================================================================== cups & mounds
class Ctx:
    def __init__(self, sp, R, frac):
        self.R, self.t, self.d, self.rin = R, sp["t"], sp["d"], sp["rin"]
        tab = sp.get("profile", [(0, 0), (0.55, 0), (0.80, 0.08), (0.95, 0.22), (1.0, 0.4), (0.97, 0.65), (0.9, 0.88)])
        self.rim_a = sp["rin"] / R + sp.get("wall", 0.18)
        self.tab = list(tab) + [(self.rim_a, 1.0)]
        self.bk = np.array([p[1] for p in self.tab])
        self.ak = np.array([p[0] for p in self.tab]) * R
        self.frac = frac

    def rho(self, b):
        return float(np.interp(b, self.bk, self.ak))

    def z(self, b):
        return -self.t + b * (self.d + self.t)

    def xyz(self, b, phi, off=0.0):
        e = 0.01
        r0, r1 = self.rho(max(b - e, 0)), self.rho(min(b + e, 1))
        z0, z1 = self.z(max(b - e, 0)), self.z(min(b + e, 1))
        tg = nrm(A(r1 - r0, z1 - z0, 0))
        n2 = (tg[1], -tg[0])
        rho = self.rho(b) + n2[0] * off
        return A(rho * math.cos(phi), rho * math.sin(phi), self.z(b) + n2[1] * off)

    def frame(self, b, phi):
        e = 0.01
        r0, r1 = self.rho(max(b - e, 0)), self.rho(min(b + e, 1))
        z0, z1 = self.z(max(b - e, 0)), self.z(min(b + e, 1))
        tg = nrm(A(r1 - r0, z1 - z0, 0))
        rad = A(math.cos(phi), math.sin(phi), 0)
        mer = rad * tg[0] + A(0, 0, tg[1])
        nrm_ = rad * tg[1] - A(0, 0, tg[0])
        tph = A(-math.sin(phi), math.cos(phi), 0)
        return np.stack([tph, mer, nrm_], 1)


def dplan(L):
    def fn(rho, z, n):
        P = 2 * L + math.pi * rho
        out = []
        for k in range(n):
            s = P * k / (n - 1)
            if s < L:
                x, y = -L + s, rho
            elif s < L + math.pi * rho:
                th = (s - L) / rho
                x, y = rho * math.sin(th), rho * math.cos(th)
            else:
                x, y = -(s - L - math.pi * rho) , -rho
                x = -(s - L - math.pi * rho)
            out.append(A(x, y, z))
        return out
    return fn


def build_cup(mb, sp, stage, rng, R):
    old = stage == "Old"
    frac = FRAC[stage]
    ctx = Ctx(sp, R, frac)
    t, d, rin = sp["t"], sp["d"], sp["rin"]
    n = sp.get("n", 26 if R < 0.12 else 32)
    ringfn = None
    closed = True
    nn = n
    if sp["kind"] == "MudCup":
        L = sp["L"]
        ringfn = dplan(L)
        closed = False
        nn = n
    zt = -t + frac * (d + t)
    zf = -t * (1 - frac)
    H = frac * d
    bs = sorted(set([b for b in ctx.bk if b < frac - 0.09] + [frac]))
    amp = 0.0 if sp.get("smooth") else sp.get("amp", 0.025)
    if sp.get("shell", True):
        cout = wc(sp["cell_out"], old)
        prof = [(ctx.rho(b), ctx.z(b)) for b in bs]
        if sp["kind"] == "MudCup":
            prof[0] = (0.2 * R, prof[0][1])
        rings, s0 = mb.revolve(prof, nn, cout, mb.part("shell_out"), amp=amp * (2 if old else 1), ringfn=ringfn, closed=closed,
                               wavy=sp.get("wavy", 0.0005))
        mb.orient(s0, mb.nfaces(), lambda c: A(c[0], c[1], c[2] + t * 1.0 + 0.002) if ringfn is None else A(c[0] + 0.0, c[1], c[2] + t * 0.5 + 0.002))
        outer_top = rings[-1]
        # inner skin
        cin = sp["cell_in"] if stage in ("Full", "Old") else sp.get("cell_in2", sp["cell_out"])
        cin = wc(cin, old)
        rin_s = rin * (1.0 if frac >= 1 else 1.05)
        iprof = [(0.18 * rin if ringfn else 0, zf), (0.72 * rin_s, zf), (0.88 * rin_s, zf + 0.2 * H),
                 (0.96 * rin_s, zf + 0.55 * H), (rin_s, zf + H)]
        if ringfn:
            iprof = iprof[1:]
            iprof = [(0.2 * rin, zf)] + iprof
        irings, s1 = mb.revolve(iprof, nn, cin, mb.part("shell_in"), amp=amp * 0.6, ringfn=ringfn, closed=closed,
                                wavy=min(0.0004, rin * 0.004))
        mb.orient(s1, mb.nfaces(), lambda c: A(-c[0] * 0.3, -c[1] * 0.3, 1.0) if ringfn is None else A(0, 0, 1.0))
        # rim cap
        ot = rings[-1]
        it = irings[-1]
        mid = []
        for a_, b_ in zip(ot, it):
            q = (a_ + 0 if False else None)
        mid_ring = []
        for k in range(len(ot)):
            pa, pb = mb.v[ot[k]], mb.v[it[k]]
            mid_ring.append(mb.vert((pa + pb) / 2 + A(0, 0, 0.12 * np.linalg.norm((pa - pb)[:2]))))
        s2 = mb.nfaces()
        mb.bridge(it, mid_ring, cin, mb.part("rim"), closed=closed)
        mb.bridge(mid_ring, ot, cout, mb.part("rim"), closed=closed)
        mb.orient(s2, mb.nfaces(), lambda c: A(0, 0, 1.0))
        if ringfn:   # floor fan
            r0 = irings[0]
            cen = mb.vert(sum(mb.v[k] for k in r0) / len(r0))
            f0 = mb.nfaces()
            p = mb.part("floor")
            for k in range(len(r0)):
                mb.face((cen, r0[(k + 1) % len(r0)], r0[k]), cin, p)
            mb.orient(f0, mb.nfaces(), lambda c: A(0, 0, 1.0))
    # ---- strands & clumps on the outer surface
    if ringfn is None:
        for kind, cnt, cell, rmm in sp.get("strands", []):
            cnt = int(cnt * (0.55 if old else 1.0))
            for i in range(cnt):
                b = rng.uniform(0.0, 1.0)
                phi = rng.uniform(0, 6.283)
                r = rng.uniform(*rmm) / 1000.0
                rn = rng.random(6)
                if b > frac + 0.03:
                    continue
                cc = wc(cell, old)
                if kind in ("grass", "twig", "reed", "rootlet"):
                    span = (0.5 + rn[0] * 0.9) * (0.45 if sp["kind"] == "Mound" and kind == "reed" else 1.0) * (1.3 if kind == "reed" else 1)
                    np_ = 4 if kind in ("twig", "reed") else 3
                    sg = 1 if rn[1] < 0.5 else -1
                    pts = []
                    for q in range(np_):
                        s = q / (np_ - 1)
                        bb = float(np.clip(b + 0.07 * math.sin(s * 3 + phi) * (0.4 + 0.6 * rn[2]), 0, min(1.0, frac + 0.02)))
                        pts.append(ctx.xyz(bb, phi + sg * span * s, off=r * (0.3 + 0.5 * rn[3])))
                    sides = 3 if kind in ("grass", "rootlet") else (5 if r > 0.002 else 4)
                    mb.tube(pts, r, cc, mb.part(kind), sides=sides, flat=0.55 if kind in ("grass",) else 1.0)
                else:
                    fr = ctx.frame(b, phi)
                    sq = 0.3 if kind in ("lichen", "leaf") else 0.7
                    c = ctx.xyz(b, phi, off=-0.35 * r * sq if sq < 0.5 else 0.0)
                    mb.blob(c, np.array([r, r * (0.7 + 0.5 * rn[4]), r * sq]), cc, mb.part(kind), rough=0.3, frame=fr)
    else:
        # mud pellets on the half cup: blobs along rows
        for i in range(int(sp.get("pellets", 60) * (0.5 if old else 1))):
            b = rng.uniform(0.0, 1.0)
            if b > frac:
                continue
            phi = rng.uniform(-math.pi / 2, math.pi / 2)
            rho = ctx.rho(b)
            L = sp["L"]
            if rng.random() < 0.3:     # on the straight flanks
                x = -rng.uniform(0, L)
                c = A(x, rho * rng.choice([-1, 1]), ctx.z(b))
                fr = np.eye(3)
            else:
                c = A(rho * math.sin(phi), rho * math.cos(phi) * rng.choice([-1, 1]) if False else rho * math.cos(phi), ctx.z(b))
                c = A(rho * math.cos(phi), rho * math.sin(phi), ctx.z(b))
                fr = ctx.frame(b, phi)
            r = rng.uniform(1.6, 2.6) / 1000
            mb.blob(c + fr[:, 2] * 0.0, np.array([r, r, r * 0.7]), wc(sp["cell_out"], old), mb.part("pellet"), rough=0.3, frame=fr)
    # ---- inside: lining extras (feathers, down tufts) at the rim
    if stage in ("Full", "Old") and sp.get("rim_extra"):
        kind, cnt, cell = sp["rim_extra"]
        for i in range(int(cnt * (0.4 if old else 1))):
            ph = rng.uniform(0, 6.283) if ringfn is None else rng.uniform(-1.4, 1.4)
            rho = rin * rng.uniform(0.85, 1.0)
            base = A(rho * math.cos(ph), rho * math.sin(ph), d * rng.uniform(0.6, 1.0))
            dirv = nrm(A(-math.cos(ph) * 0.5, -math.sin(ph) * 0.5, 0.8) + A(rng.normal(0, .3), rng.normal(0, .3), 0))
            Lf = rin * rng.uniform(0.5, 0.9)
            pts = [base, base + dirv * Lf * 0.5 + A(0, 0, 0.002), base + dirv * Lf + A(rng.normal(0, 0.004), rng.normal(0, 0.004), -0.004)]
            mb.tube(pts, rin * 0.07, wc(cell, old), mb.part("feather"), sides=4, flat=0.3,
                    rfun=lambda tt: math.sin(math.pi * (0.15 + 0.7 * tt)) + 0.15, up=(0, 0, 1))
    if old:
        for k in range(4):
            phi = rng.uniform(0, 6.283)
            p0 = ctx.xyz(rng.uniform(0.5, 0.9) * frac, phi, 0.0005)
            dr = A(math.cos(phi), math.sin(phi), 0)
            Ls = R * rng.uniform(0.5, 0.9)
            pts = [p0, p0 + dr * Ls * 0.25 - A(0, 0, Ls * 0.1), p0 + dr * Ls * 0.35 - A(0, 0, Ls * 0.55)]
            mb.tube(pts, max(0.0006, R * 0.012), "grass_bleached", mb.part("hang"), sides=3, flat=0.5)
        s = 0.7
    return mb


# =============================================================================================== scrape
def build_scrape(mb, sp, stage, rng, R):
    old = stage == "Old"
    fr = FRAC[stage]
    rin, zg = sp["rin"], sp["d"]
    cg = wc(sp["ground"] if stage in ("B1", "B2") or sp["ground"] == "pebbles" else sp.get("ground_full", sp["ground"]), old)
    hump = zg * 0.7 * {"B1": 0.3, "B2": 0.6, "Full": 1.0, "Old": 0.5}[stage]
    prof = [(R, zg - 0.03), (R * 1.0, zg - 0.012), (R * 0.86, zg - 0.002), (rin * 1.7, zg + hump * 0.3), (rin * 1.3, zg + hump),
            (rin * 1.05, zg * 0.9), (rin * 0.85, zg * 0.35), (rin * 0.55, 0.0), (0, 0.0)]
    depth = {"B1": 0.35, "B2": 0.7, "Full": 1.0, "Old": 0.6}[stage]
    prof = [(r_, z_ * (depth if (z_ > 0 and r_ < rin * 1.2) else 1.0)) for r_, z_ in prof]
    prof = prof[::-1]
    rings, s0 = mb.revolve(prof, 26, cg, mb.part("ground"), amp=0.03, wavy=0.0006)
    mb.orient(s0, mb.nfaces(), lambda c: A(0, 0, 1.0))
    n_ring = sp["ring"]
    for kind, cnt, cell, rmm in n_ring:
        cnt = int(cnt * (0.3 + 0.7 * fr) * (0.5 if old else 1))
        for i in range(cnt):
            phi = rng.uniform(0, 6.283)
            r = rng.uniform(*rmm) / 1000
            rho = rng.uniform(rin * 1.0, rin * 1.9)
            zc = zg + r * 0.8
            cc = wc(cell, old)
            if kind == "grass":
                tang = A(-math.sin(phi), math.cos(phi), 0)
                rad = A(math.cos(phi), math.sin(phi), 0)
                u = nrm(tang * 0.8 + rad * rng.normal(0, 0.7))
                Ls = rng.uniform(0.05, 0.13) * (R / 0.25)
                c = A(rho * math.cos(phi), rho * math.sin(phi), zc)
                pts = [c - u * Ls / 2, c + A(0, 0, r * 1.5) , c + u * Ls / 2]
                pts[1] = c + A(0, 0, r * 1.2)
                mb.tube(pts, r, cc, mb.part("grass"), sides=3, flat=0.6)
            else:
                c = A(rho * math.cos(phi), rho * math.sin(phi), zg + r * 0.4)
                mb.blob(c, np.array([r, r * rng.uniform(0.7, 1.2), r * 0.7]), cc, mb.part(kind), rough=0.3)
    if stage in ("Full",) and sp.get("lining"):
        lining_bowl(mb, sp, rin * 0.78, zg * 0.55, sp["lining"], floor=0.0012, depth_scale=0.5, rough=0.03, n=20, rim=1.1)
    return mb


BUILDERS = {"Platform": build_platform, "Dome": build_dome, "Cup": build_cup, "Mound": build_cup, "MudCup": build_cup,
            "Scrape": build_scrape}


def build_open(sid, sp, stage, R):
    rng = np.random.default_rng(zlib.crc32(sid.encode()))
    mb = MB(np.random.default_rng(zlib.crc32((sid + stage).encode())))
    if stage == "Old":      # flattened and spread, applied to every control point before the geometry is built
        mb.T = lambda v: A(v[0] * 1.03, v[1] * 1.03, v[2] * 0.7 if v[2] > 0 else v[2])
    BUILDERS[sp["kind"]](mb, sp, stage, rng, R)
    return mb
