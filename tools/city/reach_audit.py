#!/usr/bin/env python3
"""Static reachability audit of the town's interactive things (docs/city_interiors.md, "Reach audit").

    python3 tools/city/reach_audit.py [type ...] [-v] [--warn-only]

Reads Content/City/interiors.json (things, lift links), Content/City/city_plan.json (plot -> building export) and
blender/export/city/<type>[__<plot>].json + .glb (the collision shell SM_<type>, the first mesh: B.m). No bpy: the glb is
parsed with numpy. The export json has no floors or stairs, so the walk graph comes from the shell's triangles:

* a 0.2 m grid; every non-vertical triangle is rasterised (vertical ray hits: z, facing up or down);
  a walkable node = an up-facing hit (normal z > 0.6) with >= 1.6 m to the next hit above and no wall
  (near-vertical shell faces, sampled as z intervals, grown by one cell ~ hero radius) 0.45..1.7 m over it;
* virtual ground at z 0 wherever the shell has nothing within 0.35 m of 0 (the landscape under/around the plot);
* edges between 4-neighbour nodes within 0.5 m of height (stair steps 0.16 m, the hero climbs 0.45);
* seeds: virtual ground on the grid border (the street, 3 m outside the shell); lift links (things with `floors` in
  interiors.json) carry reachability from the door's standing place to the arrival spot (the game's 7 candidates,
  floor 2.5 m under to 0.5 m over the arrival thing's point).

Per thing (standing places: 0.6..1.2 m in front along its yaw, +-0.35 m to the side; seats and beds: also beside and
behind, floor under the cushion within 0.6 m of pos.z - height):
  a  floor: a walkable-height up-facing hit 0..2.2 m under the thing's point (seats: 0.6 m round the seat's floor)
  b  clear: the standing node has 1.6 m over it and no wall within ~0.2-0.4 m (no furniture core: those are B.m boxes)
  c  reach: its node is connected to the street (stairs, ramps, door holes, lift links)
  d  every lift stop's arrival spot has a walkable floor
  e  headroom >= 1.8 m over the standing node (shell faces only: B.detail has no collision and is ignored)
  +  line of sight from 1.5 m over the standing node to the thing's point does not cross a wall (last 0.4 m free)
Precision: 0.2 m in plan, exact in height for shell faces (walls: 0.05 m bins). Doors (ALyudDoor) are open holes here; railings and all
B.detail are ignored (no collision); the terrain is assumed flat at the building's z 0.
Exit 1 on errors.
"""
import json
import math
import os
import struct
import sys
from collections import defaultdict, deque

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
EXPORTS = os.path.join(ROOT, "blender", "export", "city")
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "City")
CELL = 0.2
MARGIN = 3.0
CLEAR_WALK = 1.6
HEADROOM = 1.8
STEP = 0.5
BAND = (0.45, 1.7)
ZBIN = 0.05       # wall z bins (0.1 put a stair's riser two steps ahead into the 0.45 m band: steep flights read as walls)
KIND_RISE = {"seat": 0.45, "bed": 0.5, "shelf": 1.3, "board": 1.5, "counter": 1.0, "machine": 1.1, "play": 0.8,
             "sport": 1.0, "craft": 0.9, "screen": 1.6, "take": 0.8, "door": 1.2}


# ------------------------------------------------------------------ glb
def load_shell(path):
    data = open(path, "rb").read()
    jl = struct.unpack("<I", data[12:16])[0]
    j = json.loads(data[20:20 + jl])
    off = 20 + jl
    bl = struct.unpack("<I", data[off:off + 4])[0]
    binc = data[off + 8:off + 8 + bl]

    def acc(i):
        a = j["accessors"][i]
        bv = j["bufferViews"][a["bufferView"]]
        dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a["componentType"]]
        n = {"SCALAR": 1, "VEC3": 3, "VEC2": 2, "VEC4": 4}[a["type"]]
        start = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        arr = np.frombuffer(binc, dtype=dt, count=a["count"] * n, offset=start)
        return arr.reshape(-1, n) if n > 1 else arr

    tris = []
    for p in j["meshes"][0]["primitives"]:
        v = acc(p["attributes"]["POSITION"]).astype(np.float64)
        idx = acc(p["indices"]).astype(np.int64) if "indices" in p else np.arange(len(v))
        v = np.stack([v[:, 0], -v[:, 2], v[:, 1]], axis=1)          # glTF y-up -> building z-up
        tris.append(v[idx.reshape(-1, 3)])
    return np.concatenate(tris) if tris else np.zeros((0, 3, 3))


# ------------------------------------------------------------------ walk grid
class Grid:
    def __init__(self, tris):
        self.tris = tris
        lo = tris.reshape(-1, 3).min(0)
        hi = tris.reshape(-1, 3).max(0)
        self.x0, self.y0 = lo[0] - MARGIN, lo[1] - MARGIN
        self.nx = int(math.ceil((hi[0] - lo[0] + 2 * MARGIN) / CELL)) + 1
        self.ny = int(math.ceil((hi[1] - lo[1] + 2 * MARGIN) / CELL)) + 1
        self.z0 = min(lo[2], 0.0) - 1.0
        self.nz = int(math.ceil((max(hi[2], 0.0) + 3.0 - self.z0) / ZBIN)) + 1
        e1 = tris[:, 1] - tris[:, 0]
        e2 = tris[:, 2] - tris[:, 0]
        n = np.cross(e1, e2)
        ln = np.linalg.norm(n, axis=1)
        ok = ln > 1e-9
        tris, n, ln = tris[ok], n[ok], ln[ok]
        nz = n[:, 2] / ln
        self._raster(tris[np.abs(nz) > 0.2], nz[np.abs(nz) > 0.2])
        self._walls(tris[np.abs(nz) <= 0.3])
        self._nodes()

    def cell_of(self, x, y):
        return int(math.floor((x - self.x0) / CELL)), int(math.floor((y - self.y0) / CELL))

    def _raster(self, T, nz):
        cx = lambda x: np.floor((x - self.x0) / CELL - 0.5).astype(np.int64)   # cell centre test
        ix0 = np.clip(cx(T[:, :, 0].min(1)) , 0, self.nx - 1)
        ix1 = np.clip(cx(T[:, :, 0].max(1)) + 1, 0, self.nx - 1)
        iy0 = np.clip(np.floor((T[:, :, 1].min(1) - self.y0) / CELL - 0.5).astype(np.int64), 0, self.ny - 1)
        iy1 = np.clip(np.floor((T[:, :, 1].max(1) - self.y0) / CELL - 0.5).astype(np.int64) + 1, 0, self.ny - 1)
        cw = ix1 - ix0 + 1
        ch = iy1 - iy0 + 1
        cnt = cw * ch
        cells, zs, ups = [], [], []
        CH = 200000
        order = np.arange(len(T))
        start = 0
        csum = np.cumsum(cnt)
        while start < len(T):
            # chunk by total expanded size
            stop = int(np.searchsorted(csum, (csum[start - 1] if start else 0) + 3_000_000, "right"))
            stop = max(stop, start + 1)
            sl = order[start:stop]
            rep = np.repeat(sl, cnt[sl])
            first = np.repeat(np.cumsum(cnt[sl]) - cnt[sl], cnt[sl])
            k = np.arange(len(rep)) - first
            ix = ix0[rep] + k % cw[rep]
            iy = iy0[rep] + k // cw[rep]
            px = self.x0 + (ix + 0.5) * CELL
            py = self.y0 + (iy + 0.5) * CELL
            a, b, c = T[rep, 0], T[rep, 1], T[rep, 2]
            d = (b[:, 0] - a[:, 0]) * (c[:, 1] - a[:, 1]) - (c[:, 0] - a[:, 0]) * (b[:, 1] - a[:, 1])
            w1 = ((px - a[:, 0]) * (c[:, 1] - a[:, 1]) - (c[:, 0] - a[:, 0]) * (py - a[:, 1])) / d
            w2 = ((b[:, 0] - a[:, 0]) * (py - a[:, 1]) - (px - a[:, 0]) * (b[:, 1] - a[:, 1])) / d
            w0 = 1 - w1 - w2
            e = -1e-6
            inside = (w0 >= e) & (w1 >= e) & (w2 >= e)
            z = w0 * a[:, 2] + w1 * b[:, 2] + w2 * c[:, 2]
            cells.append((ix * self.ny + iy)[inside])
            zs.append(z[inside])
            ups.append(nz[rep][inside])
            start = stop
        self.h_cell = np.concatenate(cells) if cells else np.zeros(0, np.int64)
        self.h_z = np.concatenate(zs) if zs else np.zeros(0)
        self.h_nz = np.concatenate(ups) if ups else np.zeros(0)
        o = np.lexsort((self.h_z, self.h_cell))
        self.h_cell, self.h_z, self.h_nz = self.h_cell[o], self.h_z[o], self.h_nz[o]
        self.h_key = self.h_cell * 10000.0 + (self.h_z - self.z0)

    def _walls(self, T):
        occ = np.zeros((self.nx, self.ny, self.nz + 1), np.int32)
        if len(T):
            p = T[:, :, :2]
            span = p.max(1) - p.min(1)
            ax = np.argmax(span, axis=1)
            u = p[np.arange(len(T)), :, :][..., 0] * (ax == 0)[:, None] + p[..., 1] * (ax == 1)[:, None]
            # line through the triangle's 2D projection: xy as a function of u (via its two extreme points)
            imin = np.argmin(u, 1)
            imax = np.argmax(u, 1)
            r = np.arange(len(T))
            pa, pb = p[r, imin], p[r, imax]
            ua, ub = u[r, imin], u[r, imax]
            ns = np.maximum(1, np.ceil((ub - ua) / 0.08).astype(np.int64)) + 1
            rep = np.repeat(r, ns)
            first = np.repeat(np.cumsum(ns) - ns, ns)
            k = np.arange(len(rep)) - first
            t = k / np.maximum(ns[rep] - 1, 1)
            us = ua[rep] + t * (ub[rep] - ua[rep])
            xy = pa[rep] + t[:, None] * (pb[rep] - pa[rep])
            zlo = np.full(len(rep), np.inf)
            zhi = np.full(len(rep), -np.inf)
            for i, j in ((0, 1), (1, 2), (2, 0)):
                u1, u2 = u[rep, i], u[rep, j]
                z1, z2 = T[rep, i, 2], T[rep, j, 2]
                lo, hi = np.minimum(u1, u2), np.maximum(u1, u2)
                hit = (us >= lo - 1e-6) & (us <= hi + 1e-6)
                den = np.where(np.abs(u2 - u1) < 1e-9, 1.0, u2 - u1)
                zz = np.where(np.abs(u2 - u1) < 1e-9, np.maximum(z1, z2), z1 + (us - u1) / den * (z2 - z1))
                zlo = np.where(hit, np.minimum(zlo, np.where(np.abs(u2 - u1) < 1e-9, np.minimum(z1, z2), zz)), zlo)
                zhi = np.where(hit, np.maximum(zhi, zz), zhi)
            ok = np.isfinite(zlo)
            ix = np.floor((xy[ok, 0] - self.x0) / CELL).astype(np.int64)
            iy = np.floor((xy[ok, 1] - self.y0) / CELL).astype(np.int64)
            b0 = np.clip(np.floor((zlo[ok] - self.z0) / ZBIN).astype(np.int64), 0, self.nz)
            b1 = np.clip(np.floor((zhi[ok] - self.z0) / ZBIN).astype(np.int64) + 1, 0, self.nz)
            good = (ix >= 0) & (ix < self.nx) & (iy >= 0) & (iy < self.ny) & (b1 > b0)
            np.add.at(occ, (ix[good], iy[good], b0[good]), 1)
            np.add.at(occ, (ix[good], iy[good], b1[good]), -1)
        raw = np.cumsum(occ, axis=2)[:, :, :self.nz] > 0
        self.raw = raw
        d = raw.copy()
        d[1:] |= raw[:-1]
        d[:-1] |= raw[1:]
        e = d.copy()                       # a cross (not the 3x3 square): diagonal doorways keep a 4-connected way
        e[:, 1:] |= raw[:, :-1]
        e[:, :-1] |= raw[:, 1:]
        self.cum = np.concatenate([np.zeros((self.nx, self.ny, 1), np.int32), np.cumsum(e, axis=2, dtype=np.int32)], axis=2)

    def wall_in(self, ix, iy, za, zb):
        b0 = np.clip(np.floor((za - self.z0) / ZBIN).astype(np.int64), 0, self.nz)
        b1 = np.clip(np.floor((zb - self.z0) / ZBIN).astype(np.int64) + 1, 0, self.nz)
        return self.cum[ix, iy, b1] - self.cum[ix, iy, b0] > 0

    def _nodes(self):
        up = self.h_nz > 0.6
        j = np.searchsorted(self.h_key, self.h_key + 0.05, "right")
        jj = np.minimum(j, len(self.h_key) - 1)
        same = (j < len(self.h_key)) & (self.h_cell[jj] == self.h_cell)
        clear = np.where(same, self.h_z[jj] - self.h_z, 99.0)
        c = self.h_cell[up]
        z = self.h_z[up]
        cl = clear[up]
        # virtual ground at z 0 where the shell has nothing near 0
        allc = np.arange(self.nx * self.ny)
        near0 = np.zeros(self.nx * self.ny, bool)
        m = (np.abs(self.h_z) < 0.35) & (self.h_nz > 0.6)   # a down-facing face at 0 (a reveal's sill) is no ground
        near0[self.h_cell[m]] = True
        vg = allc[~near0]
        k = np.searchsorted(self.h_key, vg * 10000.0 + (0.05 - self.z0), "left")
        kk = np.minimum(k, len(self.h_key) - 1)
        s2 = (k < len(self.h_key)) & (self.h_cell[kk] == vg)
        vcl = np.where(s2, self.h_z[kk], 99.0)
        c = np.concatenate([c, vg])
        z = np.concatenate([z, np.zeros(len(vg))])
        cl = np.concatenate([cl, vcl])
        virt = np.concatenate([np.zeros(len(up.nonzero()[0]), bool), np.ones(len(vg), bool)])
        o = np.lexsort((z, c))
        self.n_cell, self.n_z, self.n_clear, self.n_virt = c[o], z[o], cl[o], virt[o]
        ix, iy = self.n_cell // self.ny, self.n_cell % self.ny
        top = self.n_z + np.minimum(BAND[1], np.maximum(self.n_clear - 0.05, BAND[0]))
        self.n_wall = self.wall_in(ix, iy, self.n_z + BAND[0], top)
        self.n_walk = (self.n_clear >= CLEAR_WALK) & ~self.n_wall
        self.n_key = self.n_cell * 10000.0 + (self.n_z - self.z0)
        self._components()

    def _components(self):
        n = len(self.n_cell)
        parent = np.arange(n)
        walk = self.n_walk
        ix, iy = self.n_cell // self.ny, self.n_cell % self.ny
        edges = []
        for dx, dy in ((1, 0), (0, 1)):
            nx_, ny_ = ix + dx, iy + dy
            ok = walk & (nx_ < self.nx) & (ny_ < self.ny)
            tc = nx_ * self.ny + ny_
            lo = np.searchsorted(self.n_key, tc * 10000.0 + (self.n_z - STEP - self.z0), "left")
            hi = np.searchsorted(self.n_key, tc * 10000.0 + (self.n_z + STEP - self.z0), "right")
            for off in range(3):
                jx = lo + off
                m = ok & (jx < hi)
                jx = np.minimum(jx, n - 1)
                m &= walk[jx]
                edges.append(np.stack([np.nonzero(m)[0], jx[m]], 1))
        E = np.concatenate(edges) if edges else np.zeros((0, 2), np.int64)
        # vectorised union-find by label propagation
        lab = np.arange(n)
        a, b = E[:, 0], E[:, 1]
        while True:
            m = np.minimum(lab[a], lab[b])
            new = lab.copy()
            np.minimum.at(new, a, m)
            np.minimum.at(new, b, m)
            new = new[new]
            new = new[new]
            if np.array_equal(new, lab):
                break
            lab = new
        self.comp = lab
        border = (ix == 0) | (iy == 0) | (ix == self.nx - 1) | (iy == self.ny - 1)
        self.street = set(np.unique(lab[border & walk & self.n_virt]).tolist())

    def nodes_at(self, x, y):
        ix, iy = self.cell_of(x, y)
        if not (0 <= ix < self.nx and 0 <= iy < self.ny):
            return []
        c = ix * self.ny + iy
        lo = np.searchsorted(self.n_cell, c, "left")
        hi = np.searchsorted(self.n_cell, c, "right")
        return list(range(lo, hi))

    def up_hits_at(self, x, y):
        ix, iy = self.cell_of(x, y)
        if not (0 <= ix < self.nx and 0 <= iy < self.ny):
            return []
        c = ix * self.ny + iy
        lo = np.searchsorted(self.h_cell, c, "left")
        hi = np.searchsorted(self.h_cell, c, "right")
        return [self.h_z[i] for i in range(lo, hi) if self.h_nz[i] > 0.6]

    def los(self, p, q, skip=0.4):
        L = math.dist(p, q)
        if L <= skip:
            return True
        n = int((L - skip) / 0.08) + 1
        for i in range(n):
            t = i * 0.08 / L
            x, y, z = (p[k] + (q[k] - p[k]) * t for k in range(3))
            ix, iy = self.cell_of(x, y)
            b = int((z - self.z0) / ZBIN)
            if 0 <= ix < self.nx and 0 <= iy < self.ny and 0 <= b < self.nz and self.raw[ix, iy, b]:
                return False
        return True


# ------------------------------------------------------------------ things
def fwd(yaw):
    a = math.radians(yaw)
    return math.sin(a), -math.cos(a)


def candidates(t):
    x, y, z = t["pos"]
    fx, fy = fwd(t["yaw"])
    sx, sy = fy * -1, fx        # right-hand side (not important which)
    out = []
    dists = (0.6, 0.8, 1.0, 1.2)
    for d in dists:
        for s in (0.0, 0.35, -0.35):
            out.append((x + fx * d + sx * s, y + fy * d + sy * s, "front"))
    if (t.get("props") or {}).get("topic") == "view":      # a view: the hero may stand behind it, looking out the same way
        for d in (0.6, 0.8, 1.0):
            out.append((x - fx * d, y - fy * d, "behind"))
    for d in (0.6, 0.8, 1.0):       # seats and beds are left from the side or behind; other things: a yaw warning ("y")
        out += [(x + sx * d, y + sy * d, "side"), (x - sx * d, y - sy * d, "side"), (x - fx * d, y - fy * d, "behind")]
    return out


def lift_spots(t):
    x, y, z = t["pos"]
    fx, fy = fwd(t["yaw"])
    sx, sy = fy * -1, fx
    c = [(0.6, 0), (1.0, 0), (0.6, 0.5), (0.6, -0.5), (0.3, 0.75), (0.3, -0.75), (1.4, 0)]
    return [(x + fx * d + sx * s, y + fy * d + sy * s) for d, s in c]


def audit_building(name, ex, G, things, links, verbose=False):
    """things: [(full id, local thing dict)]; links: {full id: [full ids]} (lift stops). Returns (errors, warns)."""
    res = {}
    for fid, t in things:
        z = t["pos"][2]
        props = t.get("props") or {}
        best = None
        seat_floor = None
        if t["kind"] in ("seat", "bed"):
            base = z - float(props.get("height", KIND_RISE[t["kind"]]))
            hits = [h for h in G.up_hits_at(t["pos"][0], t["pos"][1]) if abs(h - base) <= 0.6] + \
                [n for n in (G.n_z[i] for i in G.nodes_at(t["pos"][0], t["pos"][1]) if G.n_virt[i]) if abs(n - base) <= 0.6]
            seat_floor = base if hits else None
        for cx, cy, where in candidates(t):
            for i in G.nodes_at(cx, cy):
                nz = G.n_z[i]
                if t["kind"] in ("seat", "bed"):
                    if not (abs(nz - (z - float(props.get("height", KIND_RISE[t["kind"]])))) <= 0.3):
                        continue
                elif not (z - (5.0 if t["kind"] in ("board", "screen") or any(k in t["id"] for k in ("clock", "hoop", "score"))
                               else 2.2 if t["kind"] in ("seat", "bed", "play", "sport", "craft", "counter") else 2.9)
                          <= nz <= z + 0.2):      # boards, screens, clocks, hoops: looked at from below
                    continue
                clear = bool(G.n_walk[i])
                reach = clear and G.comp[i] in G.reach
                head = G.n_clear[i] >= HEADROOM
                los = G.los((cx, cy, nz + 1.5), t["pos"]) if reach else False
                score = (1, clear, reach, los, head, where == "front", -abs(nz - (z - KIND_RISE[t["kind"]])))
                if best is None or score > best[0]:
                    best = (score, i, (cx, cy, nz), where)
        probs = []
        if best is not None and best[0][2] and best[3] != "front" and t["kind"] not in ("seat", "bed") \
                and props.get("topic") != "view":
            probs.append(("y", "only reachable %s it (%.1f, %.1f, %.2f): check its yaw" % (
                "beside" if best[3] == "side" else "behind", *best[2])))
        if best is None:
            probs.append(("a", "no floor under it (0..2.2 m; looked-at things 2.9, boards and screens 5 m) 0.6-1.2 m in front, beside or behind"))
        else:
            _, clear, reach, los, head, _, _ = best[0]
            p = best[2]
            if not clear:
                probs.append(("b", "standing place (%.1f, %.1f, %.2f) blocked (%s)" % (p[0], p[1], p[2],
                              "low ceiling/furniture %.2f m" % G.n_clear[best[1]] if G.n_clear[best[1]] < CLEAR_WALK else "wall within 0.3 m")))
            elif not reach:
                probs.append(("c", "standing place (%.1f, %.1f, %.2f) not connected to the street" % p))
            elif not los:
                probs.append(("los", "a wall between the standing place (%.1f, %.1f, %.2f) and the thing" % p))
            elif not head:
                probs.append(("e", "headroom %.2f m at the standing place (%.1f, %.1f, %.2f)" % ((G.n_clear[best[1]],) + p)))
        if t["kind"] in ("seat", "bed") and seat_floor is None:
            probs.append(("a", "no floor under the cushion within 0.6 m of pos.z - height"))
        res[fid] = (best, probs)
    return res


def arrival_nodes(G, t):
    z = t["pos"][2]
    for x, y in lift_spots(t):
        for i in G.nodes_at(x, y):
            if z - 2.5 <= G.n_z[i] <= z + 0.5 and G.n_walk[i]:
                return i
    return None


# ------------------------------------------------------------------ main
def main(argv):
    verbose = "-v" in argv
    only = [a for a in argv if not a.startswith("-")]
    plan = json.load(open(os.path.join(CONTENT, "city_plan.json")))
    inter = json.load(open(os.path.join(CONTENT, "interiors.json")))
    pf = plan["portal_face_y"] / 100.0
    places = {}
    for p in plan["plots"]:
        if p.get("building"):
            places[p["id"]] = (p["building"], p["id"].lower().replace("-", "_"))
    for u in plan.get("promenade_units", []):
        if u.get("variant") and u.get("front"):
            places[u["id"]] = ("promenade_unit", u["variant"])
    by_plot = defaultdict(list)
    for t in inter["things"]:
        by_plot[t["plot"]].append(t)
    # export file per plot
    groups = defaultdict(list)
    for plot, (btype, key) in places.items():
        for nm in ("%s__%s" % (btype, key), btype):
            if os.path.exists(os.path.join(EXPORTS, nm + ".json")):
                groups[nm].append(plot)
                break
    errors, warns = [], []
    report = defaultdict(list)
    for nm in sorted(groups):
        btype = nm.split("__")[0]
        if only and btype not in only and nm not in only:
            continue
        glb = os.path.join(EXPORTS, nm + ".glb")
        if not os.path.exists(glb):
            warns.append("%s: no glb" % nm)
            continue
        jp = os.path.join(EXPORTS, nm + ".json")
        if os.path.getmtime(jp) - os.path.getmtime(glb) > 120:
            warns.append("%s: the glb is %d min older than the json (rebuild the type: the shell may be stale)"
                         % (nm, (os.path.getmtime(jp) - os.path.getmtime(glb)) / 60))
        ex = json.load(open(jp))
        local = {t["id"]: t for t in ex.get("things") or []}
        spots = {s["id"]: s for s in ex.get("spots") or []}
        G = Grid(load_shell(glb))
        for plot in groups[nm]:
            things = []
            for t in by_plot.get(plot, []):
                lid = t["id"].split("/", 1)[1]
                lt = local.get(lid)
                if lt is None and lid in spots:
                    s = spots[lid]
                    lt = {"id": lid, "kind": t["kind"], "pos": [s["pos"][0], s["pos"][1], s["pos"][2] + KIND_RISE[t["kind"]]
                                                                  if "height" not in t["props"] else s["pos"][2] + float(t["props"]["height"])],
                          "yaw": s.get("yaw", 0.0), "props": t["props"], "auto": 1}
                if lt is None:
                    continue
                lt = dict(lt)
                lt["kind"] = t["kind"]
                lt["floors"] = [f["to"] for f in t.get("floors") or []]
                things.append((t["id"], lt))
            tmap = dict(things)
            # reachability with lift links (directed): street components, then lift door -> arrival spot, until stable
            G.reach = set(G.street)
            arrivals = {}
            for fid, t in things:
                for to in t["floors"]:
                    if to in tmap and to not in arrivals:
                        arrivals[to] = arrival_nodes(G, tmap[to])
            for _ in range(10):
                res = audit_building(nm, ex, G, things, {})
                grew = False
                for fid, t in things:
                    best = res[fid][0]
                    if not t["floors"] or best is None or not best[0][2]:
                        continue
                    for to in t["floors"]:
                        a = arrivals.get(to)
                        if a is not None and G.comp[a] not in G.reach:
                            G.reach.add(G.comp[a])
                            grew = True
                if not grew:
                    break
            for to, a in arrivals.items():
                if a is None:
                    errors.append("%s d lift stop %s: no walkable floor at any of the 7 arrival spots" % (nm, to))
                    report[nm].append(("d", to, "no walkable floor at the arrival spots"))
            for fid, t in things:
                for to in t["floors"]:
                    if to not in tmap:
                        warns.append("%s lift %s -> %s: another plot (not followed)" % (nm, fid, to))
                best, probs = res[fid]
                for code, msg in probs:
                    line = "%s %s %s (%s %s, pos %s): %s" % (nm, code, fid, t["kind"], "auto" if t.get("auto") else "",
                                                             ",".join("%.2f" % v for v in t["pos"]), msg)
                    (warns if code in ("los", "y") else errors).append(line)
                    report[nm].append((code, fid, msg))
        n_err = sum(1 for c, _, _ in report[nm] if c != "los")
        print("%-40s %4d things  %d problems" % (nm, sum(len(by_plot.get(p, [])) for p in groups[nm]), len(report[nm])), flush=True)
    print()
    for w in warns:
        print("WARN", w)
    for e in errors:
        print("ERROR", e)
    print("\nreach audit: %d errors, %d warnings" % (len(errors), len(warns)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
