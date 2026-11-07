"""Mesh / UV / vertex-colour lint for the sea-life builds (pure numpy, on the same arrays that go into Blender).

Counts per mesh ('ok' when all zero):
    degenerate   zero-area or sliver triangles (longest^2 / 2A above lint_fish.SLIVER)
    uv_zero      triangles with uv area below 0.02 texel^2          uv_range   uvs outside 0..1
    uv_outside   uvs leaving the face's own rect                     uv_pad     rects closer than 4 px
    uv_overlap   real (non-mirror) faces whose texels overlap        uv_mirror  mirror / instance faces not covered by their original
    uv_density   body faces whose texel density is far (x2.6) from the part's median
    zfight       coplanar overlapping faces of different parts
    open_edges   boundary edges on parts that must be closed (bodies, legs, antennae; welded by position per part / lens)
    nonmanifold  edges with more than two faces                       inverted   closed parts: negative volume or faces against their neighbourhood (folds); open caps: normal towards the inside
    vcol         Col out of 0..1, R != clip(0.5 - x / r_len), G jumping on a body, B off the paired appendages
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
FISH = os.path.join(os.path.dirname(HERE), "fish")
if FISH not in sys.path:
    sys.path.insert(0, FISH)

from lint_fish import _raster, _uv_area, _overlap2d, SLIVER, COS  # noqa: E402
from sealife_geo import W, H, PAD  # noqa: E402


def combine(parts):
    V, F, U, pid, mir, col = [], [], [], [], [], []
    off = 0
    for k, p in enumerate(parts):
        V.append(p.V)
        F.append(p.F + off)
        U.append(p.uv)
        pid.append(np.full(len(p.F), k))
        mir.append(p.mirror)
        col.append(np.stack([p.R, p.G, p.B, np.ones(len(p.V))], 1))
        off += len(p.V)
    return (np.concatenate(V), np.concatenate(F), np.concatenate(U), np.concatenate(pid), np.concatenate(mir), np.concatenate(col))


def _edges_count(V, F):
    key = np.round(V * 1e7).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    wv = inv.reshape(-1)[F]
    E = np.concatenate([wv[:, [0, 1]], wv[:, [1, 2]], wv[:, [2, 0]]])
    E = np.sort(E, 1)
    E = E[E[:, 0] != E[:, 1]]
    _, cnt = np.unique(E, axis=0, return_counts=True)
    return int((cnt == 1).sum()), int((cnt > 2).sum())


def lint(parts, layout, spec, label, verbose=True):
    V, F, U, part, mirror, col = combine(parts)
    P = V[F]
    UV = U[F]
    n = len(F)
    res = {}
    e1, e2 = P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]
    cr = np.cross(e1, e2)
    area = 0.5 * np.linalg.norm(cr, axis=1)
    nrmv = cr / np.maximum(np.linalg.norm(cr, axis=1), 1e-30)[:, None]
    edges = np.stack([np.linalg.norm(P[:, 1] - P[:, 0], axis=1), np.linalg.norm(P[:, 2] - P[:, 1], axis=1),
                      np.linalg.norm(P[:, 0] - P[:, 2], axis=1)], 1)
    aspect = edges.max(1) ** 2 / np.maximum(2 * area, 1e-30)
    bad = (area < 1e-12) | (aspect > SLIVER)
    res["degenerate"] = int(bad.sum())
    deg_at = [(parts[part[k]].name, round(float(aspect[k]), 1)) for k in np.where(bad)[0][:8]]

    ua = _uv_area(UV)
    res["uv_zero"] = int((ua * W * H < 0.02).sum())
    res["uv_range"] = int(((UV < 0) | (UV > 1)).any(axis=(1, 2)).sum())
    px = UV[..., 0] * W
    py = (1 - UV[..., 1]) * H
    out = 0
    for k in range(len(parts)):
        sel = part == k
        x0, y0, w, h = layout.rects[parts[k].rect_key]
        o = (px[sel].min(1) < x0 - 0.01) | (px[sel].max(1) > x0 + w + 0.01) | (py[sel].min(1) < y0 - 0.01) | (py[sel].max(1) > y0 + h + 0.01)
        out += int(o.sum())
    res["uv_outside"] = out
    rl = list(layout.rects.values())
    pad = 0
    for i in range(len(rl)):
        for j in range(i + 1, len(rl)):
            a, b = rl[i], rl[j]
            gx = max(b[0] - (a[0] + a[2]), a[0] - (b[0] + b[2]))
            gy = max(b[1] - (a[1] + a[3]), a[1] - (b[1] + b[3]))
            if max(gx, gy) < PAD:
                pad += 1
    res["uv_pad"] = pad

    real = np.where(~mirror)[0]
    own = np.full(W * 2 * H * 2, -1, np.int32)
    over = set()
    for idx, px_ in zip(real, _raster(UV[real])):
        prev = own[px_]
        hit = prev >= 0
        if hit.sum() > 3:
            over.add(int(idx))
            over.update(int(v) for v in np.unique(prev[hit]))
        own[px_] = np.where(prev < 0, idx, prev)
    res["uv_overlap"] = len(over)
    over_parts = sorted(set(parts[part[i]].name for i in over))
    mi = np.where(mirror)[0]
    badm = 0
    for idx, px_ in zip(mi, _raster(UV[mi])):
        if len(px_) and (own[px_] < 0).mean() > 0.12:
            badm += 1
    res["uv_mirror"] = badm

    dens_bad = 0
    for k, p in enumerate(parts):
        if p.kind != "body":
            continue
        sel = np.where((part == k) & ~mirror)[0]
        if len(sel) < 8:
            continue
        d = np.sqrt(np.maximum(ua[sel] * W * H, 1e-9) / np.maximum(area[sel], 1e-14))
        big = area[sel] > 0.15 * np.median(area[sel])           # tiny cap triangles are allowed to drift
        med = np.median(d)
        dens_bad += int((((d < med / 2.6) | (d > med * 2.6)) & big).sum())
    res["uv_density"] = dens_bad

    # z-fighting between parts
    L = spec["length"]
    ZF = max(1e-6, 0.0004 * L)
    cen = P.mean(1)
    rad = np.linalg.norm(P - cen[:, None], axis=2).max(1)
    dpl = np.einsum("ij,ij->i", nrmv, cen)
    zf = 0
    zf_at = []
    for i in range(n):
        near = np.where((part != part[i]) & (np.linalg.norm(cen - cen[i], axis=1) < rad + rad[i]))[0]
        near = near[near > i]
        if len(near) == 0:
            continue
        near = near[np.abs(nrmv[near] @ nrmv[i]) > COS]
        if len(near) == 0:
            continue
        dA = np.abs(np.einsum("j,nkj->nk", nrmv[i], P[near]) - dpl[i]).max(1)
        dB = np.abs(np.einsum("nj,nkj->nk", nrmv[near], P[i][None]) - dpl[near][:, None]).max(1)
        close = near[(dA < ZF) & (dB < ZF)]
        if len(close) == 0:
            continue
        ax = np.cross(nrmv[i], [0.3, 0.5, 0.8])
        ax /= np.linalg.norm(ax)
        ay = np.cross(nrmv[i], ax)
        A2 = np.stack([P[i] @ ax, P[i] @ ay], 1)
        B2 = np.stack([P[close] @ ax, P[close] @ ay], -1)
        if _overlap2d(A2, B2).any():
            zf += 1
            zf_at.append((parts[part[i]].name, tuple(np.round(cen[i], 4))))
    res["zfight"] = zf

    # watertight parts (domes: top + bottom welded together)
    groups = {}
    for k, p in enumerate(parts):
        g = getattr(p, "lens_group", None)
        if g is not None:
            groups.setdefault(("lens", g, bool(p.mirror.all())), []).append(k)
        elif p.closed:
            groups[("p", k)] = [k]
    ob = nm = 0
    open_at = []
    for g, ks in groups.items():
        Vg = np.concatenate([parts[k].V for k in ks])
        offs = np.cumsum([0] + [len(parts[k].V) for k in ks])
        Fg = np.concatenate([parts[k].F + o for k, o in zip(ks, offs)])
        o_, n_ = _edges_count(Vg, Fg)
        ob += o_
        nm += n_
        if o_ or n_:
            open_at.append((parts[ks[0]].name, o_, n_))
    res["open_edges"] = ob
    res["nonmanifold"] = nm

    inv = 0
    inv_at = []
    for k, p in enumerate(parts):
        Pp = p.V[p.F]
        nn = np.cross(Pp[:, 1] - Pp[:, 0], Pp[:, 2] - Pp[:, 0])
        if p.closed:
            # watertight part: positive signed volume, and no face against its welded neighbourhood (folds)
            vol = np.einsum("ij,ij->i", Pp[:, 0], np.cross(Pp[:, 1], Pp[:, 2])).sum() / 6
            key = np.round(p.V * 1e7).astype(np.int64)
            _, wv = np.unique(key, axis=0, return_inverse=True)
            wv = wv.reshape(-1)
            vn = np.zeros((wv.max() + 1, 3))
            va = np.zeros(wv.max() + 1)
            for c3 in range(3):
                np.add.at(vn, wv[p.F[:, c3]], nn)
                np.add.at(va, wv[p.F[:, c3]], np.linalg.norm(nn, axis=1))
            coh = np.linalg.norm(vn, axis=1) / np.maximum(va, 1e-30)      # ~0 on knife edges (flipper tips): no defined normal
            d = np.einsum("ij,ij->i", nn, vn[wv[p.F]].sum(1))
            sharp = (coh[wv[p.F]] < 0.35).any(1)
            c = int(((d < 0) & ~sharp).sum()) + (len(p.F) if vol < 0 else 0)
        elif p.ref is not None:
            r = p.ref[p.F].mean(1)
            d = np.einsum("ij,ij->i", nn, Pp.mean(1) - r)
            c = int((d < 0).sum())
        else:
            continue
        if c:
            inv_at.append((p.name, c))
        inv += c
    res["inverted"] = inv

    vc = []
    co = V
    Lr = spec.get("r_len", L)
    if (col < -1e-6).any() or (col > 1 + 1e-6).any() or not np.isfinite(col).all():
        vc.append("range")
    if np.abs(col[:, 0] - np.clip(0.5 - co[:, 0] / Lr, 0, 1)).max() > 0.012:
        vc.append("R")
    gv = col[:, 1]
    for k, p in enumerate(parts):
        if p.kind != "body":
            continue
        Fk = F[part == k]
        if (np.abs(gv[Fk[:, 0]] - gv[Fk[:, 1]]).max() > 0.2 or np.abs(gv[Fk[:, 1]] - gv[Fk[:, 2]]).max() > 0.2):
            vc.append("G:" + p.name)
    for k, p in enumerate(parts):
        want = 1.0 if p.Bval > 0.5 else 0.0
        if np.abs(p.B - want).max() > 1e-6:
            vc.append("B:" + p.name)
    res["vcol"] = len(vc)

    badd = {k: v for k, v in res.items() if v}
    line = "LINT %s ok" % label if not badd else "LINT %s %s" % (label, " ".join("%s %d" % kv for kv in badd.items()))
    if verbose:
        print(line)
        if res["degenerate"]:
            print("   degenerate:", deg_at)
        if res["uv_overlap"]:
            print("   uv_overlap parts:", over_parts)
        if zf_at:
            print("   zfight:", zf_at[:6])
        if open_at:
            print("   open/nonmanifold:", open_at[:8])
        if inv_at:
            print("   inverted:", inv_at[:8])
        if vc:
            print("   vcol:", vc)
    return res, line
