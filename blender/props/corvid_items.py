"""Small props that wild corvids (crows, jays, magpies) carry in their bills, cache and steal.

    /Applications/Blender.app/Contents/MacOS/Blender -b --python blender/props/corvid_items.py -- [Names] [--no-render] [--no-icons]

16 items, real sizes, origin at the centre of mass, +X along the long axis, Z up, lying the way they would lie on the ground:
  food    Acorn Hazelnut Walnut PineCone Mussel Crust Fish FeedPile
  shiny   BottleCap Foil Ring Button Coin Key SeaGlass
  other   Stick
Per item (blender/props/export/corvid_items/):
  <Name>.fbx, <Name>_LOD1.fbx   static mesh + a ~25% LOD (the FBX has the metre-scaled mesh: 1 uu = 1 cm in Unreal)
  T_<Name>.png                  one small albedo texture (256, fish 512), baked from a 3D colour function (own UVs). The alpha
                                channel is a metal mask ONLY for the items that mix paint and metal (BottleCap, Coin)
  meta.json                     sizes, triangle counts, material numbers (roughness, metal) for unreal/scripts/import_corvid_items.py
  renders/<Name>.png            close-ups, contact_sheet.png (+ contact_sheet_lod1.png), icons in textures/localized/<lang>/icon_find_<name>.jpg
The meshes are linted with blender/birds/lint_bird.py (degenerate / uv / z-fighting / faces inside faces), see the LINT lines.
Everything is opaque; the look is roughness + metal. Textures are baked by rasterising the UV triangles and calling a colour
function of the 3D position, normal and two per-vertex numbers (a, b), so no UV stretch can smear a picture.
"""
import json
import math
import os
import struct
import subprocess
import sys
import zlib

import bmesh
import bpy
import numpy as np
from mathutils import Euler, Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(HERE, "export", "corvid_items")
RENDERS = os.path.join(OUT, "renders")
ICON_LANGS = ("en", "ru", "ja")
sys.path.insert(0, os.path.join(ROOT, "blender", "birds"))
import lint_bird  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
CM = 0.01
rng = np.random.default_rng(7)


# ---------------------------------------------------------------- noise and colour helpers

def _hash(ix, iy, iz, seed):
    h = (ix * 374761393 + iy * 668265263 + iz * 1274126177 + seed * 144269504) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1103515245) & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFFFF) / float(0xFFFFFF)


def vnoise(P, seed=0):
    P = np.asarray(P, float)
    i = np.floor(P).astype(np.int64)
    f = P - i
    u = f * f * (3 - 2 * f)
    out = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (u[:, 0] if dx else 1 - u[:, 0]) * (u[:, 1] if dy else 1 - u[:, 1]) * (u[:, 2] if dz else 1 - u[:, 2])
                out = out + w * _hash(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return out


def fbm(P, octaves=4, seed=0):
    P = np.asarray(P, float)
    s, amp, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        s = s + amp * vnoise(P * (2 ** o), seed + o * 17)
        tot += amp
        amp *= 0.5
    return s / tot


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def mix(c0, c1, t):
    c0 = np.asarray(c0, float)
    c1 = np.asarray(c1, float)
    t = np.asarray(t, float)
    if t.ndim == 1:
        t = t[:, None]
    return c0 * (1 - t) + c1 * t


def C(r, g, b):
    return np.array([r, g, b], float)


def noise1(x, y=0.0, z=0.0, seed=0):
    x = np.asarray(x, float)
    return vnoise(np.stack([x, np.zeros_like(x) + y, np.zeros_like(x) + z], 1), seed)


def worley(P, seed=0):
    """F1, F2 distances to the nearest feature points of a jittered lattice (cell size 1)."""
    P = np.asarray(P, float)
    i0 = np.floor(P).astype(np.int64)
    f1 = np.full(len(P), 9.0)
    f2 = np.full(len(P), 9.0)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                c = i0 + np.array([dx, dy, dz])
                jit = np.stack([_hash(c[:, 0], c[:, 1], c[:, 2], seed + k * 7) for k in range(3)], 1)
                d = np.linalg.norm(c + jit - P, axis=1)
                lo = d < f1
                f2 = np.where(lo, f1, np.minimum(f2, d))
                f1 = np.where(lo, d, f1)
    return f1, f2


# ---------------------------------------------------------------- the mesh builder

class M:
    """Plain verts/faces + per-vertex a, b + per-face kind and part. Units: metres. Parts are lint parts and orientation groups."""

    def __init__(s):
        s.v, s.a, s.b = [], [], []
        s.f, s.fk, s.fp = [], [], []
        s.parts, s.sheet = [], set()
        s.cur = -1
        s.part("main")

    def part(s, name, sheet=False):
        s.parts.append(name)
        s.cur = len(s.parts) - 1
        if sheet:
            s.sheet.add(s.cur)
        return s.cur

    def vert(s, p, a=0.0, b=0.0):
        s.v.append([float(p[0]), float(p[1]), float(p[2])])
        s.a.append(float(a))
        s.b.append(float(b))
        return len(s.v) - 1

    def face(s, idx, kind=0):
        s.f.append([int(i) for i in idx])
        s.fk.append(int(kind))
        s.fp.append(s.cur)

    def V(s):
        return np.array(s.v, float)

    def lathe(s, prof, segs, loop=False, kind=0, b_from_angle=True):
        """Revolve prof about the X axis. prof: (x, ry, rz, oy, oz, a, twist); the ry = rz = 0 rings are poles (one vertex).
        loop: the profile is a closed loop (a torus-like solid)."""
        rings = []
        for p in prof:
            p = list(p) + [0] * (7 - len(p))
            x, ry, rz, oy, oz, a, tw = p
            if ry < 1e-9 and rz < 1e-9 and not loop:
                rings.append([s.vert((x, oy, oz), a, 0.5)])
            else:
                rings.append([s.vert((x, oy + ry * math.cos(2 * math.pi * j / segs + tw), oz + rz * math.sin(2 * math.pi * j / segs + tw)),
                                     a, j / segs) for j in range(segs)])
        n = len(rings)
        for i in range(n if loop else n - 1):
            r0, r1 = rings[i], rings[(i + 1) % n]
            if len(r0) == 1 and len(r1) == 1:
                continue
            for j in range(segs):
                j1 = (j + 1) % segs
                if len(r0) == 1:
                    s.face([r0[0], r1[j1], r1[j]], kind)
                elif len(r1) == 1:
                    s.face([r0[j], r0[j1], r1[0]], kind)
                else:
                    s.face([r0[j], r0[j1], r1[j1], r1[j]], kind)
        return rings

    def tube(s, path, radii, sides, cap0=True, cap1=True, a_cap=1.0, kind=0, wob=None):
        """A tube along a polyline (Nx3), radius per point. Caps get their own vertices (a = a_cap, kind 1)."""
        path = np.asarray(path, float)
        rings = []
        up = np.array([0, 0, 1.0])
        for i, p in enumerate(path):
            t = path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]
            t /= np.linalg.norm(t)
            u = np.cross(t, up)
            if np.linalg.norm(u) < 1e-6:
                u = np.cross(t, np.array([0, 1.0, 0]))
            u /= np.linalg.norm(u)
            w = np.cross(t, u)
            r = []
            for j in range(sides):
                ang = 2 * math.pi * j / sides
                rr = radii[i] * (1 + (wob(i, j) if wob else 0))
                r.append(s.vert(p + rr * (math.cos(ang) * u + math.sin(ang) * w), 0.0, j / sides))
            rings.append(r)
        for i in range(len(rings) - 1):
            for j in range(sides):
                j1 = (j + 1) % sides
                s.face([rings[i][j], rings[i][j1], rings[i + 1][j1], rings[i + 1][j]], kind)
        for flag, ring, pt in ((cap0, rings[0], path[0]), (cap1, rings[-1], path[-1])):
            if flag:
                c = s.vert(pt, a_cap, 0.5)
                ids = [s.vert(s.v[k], a_cap, 0.5) for k in ring]
                for j in range(sides):
                    s.face([ids[(j + 1) % sides], ids[j], c] if ring is rings[0] else [ids[j], ids[(j + 1) % sides], c], 1)
        return rings

    def poly(s, pts, kind=0, a=0.0):
        ids = [s.vert(p, a) for p in pts]
        s.face(ids, kind)

    # --- geometry post-processing
    def displace(s, fn):
        V = s.V()
        s.v = fn(V).tolist()

    def orient(s):
        """Flip the faces of every closed part whose signed volume is negative (open parts: majority of normals pointing away from the centroid)."""
        V = s.V()
        for pi in range(len(s.parts)):
            if pi in s.sheet:
                continue
            fi = [i for i in range(len(s.f)) if s.fp[i] == pi]
            if not fi:
                continue
            edges = {}
            for i in fi:
                f = s.f[i]
                for k in range(len(f)):
                    e = tuple(sorted((f[k], f[(k + 1) % len(f)])))
                    edges[e] = edges.get(e, 0) + 1
            closed = all(c == 2 for c in edges.values())
            vol, vote = 0.0, 0.0
            cen = V[sorted({v for i in fi for v in s.f[i]})].mean(0)
            for i in fi:
                f = s.f[i]
                for k in range(1, len(f) - 1):
                    a_, b_, c_ = V[f[0]], V[f[k]], V[f[k + 1]]
                    vol += np.dot(a_, np.cross(b_, c_)) / 6.0
                    n = np.cross(b_ - a_, c_ - a_)
                    vote += np.dot(n, (a_ + b_ + c_) / 3 - cen)
            flip = (vol < 0) if closed else (vote < 0)
            if flip:
                for i in fi:
                    s.f[i] = s.f[i][::-1]
        return s

    def append(s, o, d=(0, 0, 0)):
        n, pn = len(s.v), len(s.parts)
        s.v += [[v[0] + d[0], v[1] + d[1], v[2] + d[2]] for v in o.v]
        s.a += o.a
        s.b += o.b
        s.f += [[i + n for i in f] for f in o.f]
        s.fk += o.fk
        s.fp += [p + pn for p in o.fp]
        s.parts += o.parts
        s.sheet |= {p + pn for p in o.sheet}

    def cycle_axes(s):
        """(x, y, z) -> (y, z, x): a lathe about X becomes a lathe about Z."""
        s.v = [[v[1], v[2], v[0]] for v in s.v]

    def volume_centroid(s):
        V = s.V()
        tot, acc = 0.0, np.zeros(3)
        for i, f in enumerate(s.f):
            if s.fp[i] in s.sheet:
                continue
            for k in range(1, len(f) - 1):
                a_, b_, c_ = V[f[0]], V[f[k]], V[f[k + 1]]
                vv = np.dot(a_, np.cross(b_, c_)) / 6.0
                tot += vv
                acc += vv * (a_ + b_ + c_) / 4.0
        if abs(tot) < 1e-14:
            return (V.min(0) + V.max(0)) / 2
        return acc / tot


# ---------------------------------------------------------------- blender object, unwrap, bake

def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scn = bpy.context.scene
    scn.unit_settings.system = "METRIC"
    return scn


def to_object(m, name, shift=None, smooth_mode="smooth", angle=40.0):
    """Blender object with the faces triangulated, lint_part / kind / a / b attributes, sharp edges by angle."""
    V = m.V()
    if shift is None:
        shift = np.zeros(3)
    V = V - shift
    me = bpy.data.meshes.new(name)
    me.from_pydata(V.tolist(), [], m.f)
    me.update()
    me.attributes.new("lint_part", "INT", "FACE").data.foreach_set("value", m.fp)
    me.attributes.new("kind", "INT", "FACE").data.foreach_set("value", m.fk)
    me.attributes.new("a", "FLOAT", "POINT").data.foreach_set("value", m.a)
    me.attributes.new("b", "FLOAT", "POINT").data.foreach_set("value", m.b)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method="BEAUTY", ngon_method="BEAUTY")
    bm.to_mesh(me)
    bm.free()
    shade(me, smooth_mode, angle)
    ob["shift"] = [float(x) for x in shift]
    return ob


def shade(me, smooth_mode, angle=40.0):
    bm = bmesh.new()
    bm.from_mesh(me)
    for f in bm.faces:
        f.smooth = smooth_mode == "smooth"
    thr = math.radians(angle)
    for e in bm.edges:
        e.smooth = True
        if smooth_mode == "smooth":
            if len(e.link_faces) == 2:
                if e.link_faces[0].normal.angle(e.link_faces[1].normal, 0) > thr:
                    e.smooth = False
            elif len(e.link_faces) == 1:
                e.smooth = False
    bm.to_mesh(me)
    bm.free()
    me.update()


def unwrap(ob, margin=0.012):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    bpy.ops.uv.pack_islands(margin=margin, rotate=True)
    bpy.ops.object.mode_set(mode="OBJECT")


def write_png(path, rgb, alpha=None):
    h, w = rgb.shape[:2]
    ch = 3 if alpha is None else 4
    arr = np.clip(np.round(rgb * 255), 0, 255).astype(np.uint8)
    if alpha is not None:
        arr = np.concatenate([arr, np.clip(np.round(alpha[..., None] * 255), 0, 255).astype(np.uint8)], 2)
    raw = b"".join(b"\x00" + arr[y].tobytes() for y in range(h))

    def chunk(k, d):
        return struct.pack(">I", len(d)) + k + d + struct.pack(">I", zlib.crc32(k + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6 if ch == 4 else 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def bake(ob, colorfn, size, shift, mixed, out_name):
    """Rasterise the UV triangles at 2x and call colorfn(P_cm_in_builder_frame, N, a, b, kind) -> (rgb, metal|None)."""
    me = ob.data
    me.calc_loop_triangles()
    nv, nl = len(me.vertices), len(me.loops)
    P = np.empty(nv * 3)
    me.vertices.foreach_get("co", P)
    P = P.reshape(-1, 3)
    NL = np.empty(nl * 3)
    me.corner_normals.foreach_get("vector", NL)
    NL = NL.reshape(-1, 3)
    UV = np.empty(nl * 2)
    me.uv_layers.active.data.foreach_get("uv", UV)
    UV = UV.reshape(-1, 2)
    A = np.empty(nv)
    me.attributes["a"].data.foreach_get("value", A)
    B = np.empty(nv)
    me.attributes["b"].data.foreach_get("value", B)
    K = np.empty(len(me.polygons), int)
    me.attributes["kind"].data.foreach_get("value", K)
    nt = len(me.loop_triangles)
    vi = np.empty(nt * 3, int)
    me.loop_triangles.foreach_get("vertices", vi)
    vi = vi.reshape(-1, 3)
    li = np.empty(nt * 3, int)
    me.loop_triangles.foreach_get("loops", li)
    li = li.reshape(-1, 3)
    pi = np.empty(nt, int)
    me.loop_triangles.foreach_get("polygon_index", pi)
    S = size * 2
    pos, nor, aa, bb, kk, tx, ty = [], [], [], [], [], [], []
    for t in range(nt):
        uv = UV[li[t]] * S
        x0, x1 = int(max(0, math.floor(uv[:, 0].min() - 0.5))), int(min(S - 1, math.ceil(uv[:, 0].max() + 0.5)))
        y0, y1 = int(max(0, math.floor(uv[:, 1].min() - 0.5))), int(min(S - 1, math.ceil(uv[:, 1].max() + 0.5)))
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        gx, gy = gx.ravel(), gy.ravel()
        d = (uv[1, 1] - uv[2, 1]) * (uv[0, 0] - uv[2, 0]) + (uv[2, 0] - uv[1, 0]) * (uv[0, 1] - uv[2, 1])
        if abs(d) < 1e-12:
            continue
        w0 = ((uv[1, 1] - uv[2, 1]) * (gx - uv[2, 0]) + (uv[2, 0] - uv[1, 0]) * (gy - uv[2, 1])) / d
        w1 = ((uv[2, 1] - uv[0, 1]) * (gx - uv[2, 0]) + (uv[0, 0] - uv[2, 0]) * (gy - uv[2, 1])) / d
        w2 = 1 - w0 - w1
        eps = 0.02
        ok = (w0 >= -eps) & (w1 >= -eps) & (w2 >= -eps)
        if not ok.any():   # a face smaller than a texel: its centre
            c = uv.mean(0)
            gx, gy = np.array([math.floor(c[0]) + 0.5]), np.array([math.floor(c[1]) + 0.5])
            w0 = ((uv[1, 1] - uv[2, 1]) * (c[0] - uv[2, 0]) + (uv[2, 0] - uv[1, 0]) * (c[1] - uv[2, 1])) / d + 0 * gx
            w1 = ((uv[2, 1] - uv[0, 1]) * (c[0] - uv[2, 0]) + (uv[0, 0] - uv[2, 0]) * (c[1] - uv[2, 1])) / d + 0 * gx
            w2 = 1 - w0 - w1
            ok = np.ones(1, bool)
        w = np.stack([w0[ok], w1[ok], w2[ok]], 1)
        w = np.clip(w, 0, None)
        w /= w.sum(1, keepdims=True)
        v = vi[t]
        pos.append(w @ P[v])
        nn = w @ NL[li[t]]
        nor.append(nn / np.maximum(np.linalg.norm(nn, axis=1, keepdims=True), 1e-9))
        aa.append(w @ A[v])
        bb.append(w @ B[v])
        kk.append(np.full(len(w), K[pi[t]]))
        tx.append(np.clip(np.floor(gx[ok]).astype(int), 0, S - 1))
        ty.append(np.clip(np.floor(gy[ok]).astype(int), 0, S - 1))
    pos = np.concatenate(pos) + np.asarray(shift)
    rgb, metal = colorfn(pos / CM, np.concatenate(nor), np.concatenate(aa), np.concatenate(bb), np.concatenate(kk))
    tx, ty = np.concatenate(tx), np.concatenate(ty)
    img = np.zeros((S, S, 4))
    cnt = np.zeros((S, S))
    np.add.at(img, (ty, tx, slice(0, 3)), rgb)
    np.add.at(cnt, (ty, tx), 1.0)
    if metal is not None:
        np.add.at(img, (ty, tx, 3), metal)
    cov = cnt > 0
    img[cov] /= cnt[cov][:, None]
    # pad the islands (bleeding at the edges and mips)
    for _ in range(8):
        grow = ~cov
        if not grow.any():
            break
        acc = np.zeros_like(img)
        n = np.zeros((S, S))
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx == 0 and dy == 0:
                    continue
                sh = np.roll(np.roll(img * cov[..., None], dy, 0), dx, 1)
                sc = np.roll(np.roll(cov.astype(float), dy, 0), dx, 1)
                acc += sh
                n += sc
        fill = grow & (n > 0)
        img[fill] = acc[fill] / n[fill][:, None]
        cov = cov | fill
    # down to size (weights are all 1 after the padding)
    img = img.reshape(size, 2, size, 2, 4).mean((1, 3))
    img = img[::-1]   # PNG rows start at the top = v 1
    path = os.path.join(OUT, out_name)
    write_png(path, img[..., :3], img[..., 3] if mixed else None)
    return path


# ---------------------------------------------------------------- the items

def L(x, ry, rz=None, **kw):   # lathe profile point in cm
    return (x * CM, ry * CM, (ry if rz is None else rz) * CM)


def dense(xs, ys, n):
    """Smoothly resample a table (cm) to n values at the given x positions (cm)."""
    xx = np.linspace(xs[0], xs[-1], 400)
    yy = np.interp(xx, xs, ys)
    k = 15
    ker = np.hanning(2 * k + 1)
    ker /= ker.sum()
    pad = np.concatenate([np.full(k, yy[0]), yy, np.full(k, yy[-1])])
    yy = np.convolve(pad, ker, "valid")
    return yy


ITEMS = {}


def item(name, size=256, rough=0.6, rough_metal=None, metal=0.0, mixed=False, mode="smooth", lod=0.25, icon=None, lod_build=False):
    def deco(fn):
        ITEMS[name] = dict(build=fn, size=size, rough=rough, rough_metal=rough if rough_metal is None else rough_metal, metal=metal,
                           mixed=mixed, mode=mode, lod=lod, icon=icon, name=name, lod_build=lod_build)
        return fn
    return deco


# --- Acorn: the cup and the nut are one lathe, the cup's rim turns in and meets the nut
@item("Acorn", rough=0.5, icon=dict(rot=(0, 28, -62)))
def build_acorn():
    m = M()
    # (x, r) cm; a = 1 on the cup. The cup covers the stalk end, its rim turns in and meets the nut
    cup = [(-1.25, 0.0), (-1.23, 0.20), (-1.12, 0.40), (-0.95, 0.54), (-0.72, 0.61), (-0.50, 0.64), (-0.40, 0.635)]
    nut = [(-0.38, 0.57), (-0.20, 0.585), (0.10, 0.585), (0.45, 0.545), (0.8, 0.44), (1.05, 0.30), (1.2, 0.15), (1.27, 0.055), (1.29, 0.0)]
    prof = [(x * CM, r * CM, r * CM * 0.97, 0, 0, 1.0) for x, r in cup] + [(x * CM, r * CM, r * CM * 0.97, 0, 0, 0.0) for x, r in nut]
    m.lathe(prof, 12)
    V = m.V()
    rr = np.hypot(V[:, 1], V[:, 2])
    ang = np.arctan2(V[:, 2], V[:, 1])
    cupm = np.array(m.a) > 0.5
    ring = np.round((V[:, 0] / CM + 1.25) / 0.15)
    f = 1 + 0.045 * np.sin(ang * 6 + (ring % 2) * math.pi / 2 * 2) * cupm * (rr > 1e-6)
    V[:, 1] *= f
    V[:, 2] *= f
    m.v = V.tolist()
    return m


def c_acorn(P, N, a, b, k):
    x = P[:, 0]
    ang = np.arctan2(P[:, 2], P[:, 1])
    st = noise1(ang * 2.2, x * 0.45, 0.0, 3) * 0.6 + noise1(ang * 5.0, x * 1.0, 1.3, 4) * 0.4
    nut = mix(C(.50, .32, .12), C(.27, .16, .06), smooth(0.42, 0.68, st))
    nut = mix(nut, C(.66, .55, .34), smooth(-0.05, -0.38, x) * 0.7)                 # pale band where the cup sat
    nut = mix(nut, C(.20, .13, .06), smooth(0.95, 1.28, x))                         # dark tip
    nut = nut * (0.9 + 0.2 * fbm(P * 8, 2, 4))[:, None]
    u = (x + 1.25) / 0.15
    sc = np.abs(np.sin(np.pi * u)) * np.abs(np.sin(3 * ang + (np.floor(u) % 2) * np.pi / 2 * 2))
    cup = mix(C(.27, .23, .15), C(.47, .42, .29), smooth(0.05, 0.7, sc))
    cup = cup * (0.85 + 0.3 * fbm(P * 7, 2, 9))[:, None]
    return mix(nut, cup, smooth(0.4, 0.6, a)), None


ITEMS["Acorn"]["color"] = c_acorn


# --- Hazelnut
@item("Hazelnut", rough=0.5, icon=dict(rot=(0, 20, -50)))
def build_hazelnut():
    m = M()
    pts = [(-0.88, 0.0), (-0.84, 0.27), (-0.72, 0.52), (-0.45, 0.71), (-0.12, 0.76), (0.25, 0.68), (0.52, 0.52), (0.74, 0.34), (0.90, 0.16), (1.0, 0.05), (1.03, 0.0)]
    m.lathe([(x * CM, r * CM, r * CM * 0.95) for x, r in pts], 16)
    return m


def c_hazel(P, N, a, b, k):
    x = P[:, 0]
    ang = np.arctan2(P[:, 2], P[:, 1])
    st = noise1(ang * 7.0, x * 0.5, 0.0, 5) * 0.6 + noise1(ang * 14, x * 1.0, 0.4, 6) * 0.4
    base = mix(C(.62, .41, .17), C(.42, .25, .09), smooth(0.3, 0.75, st))
    base = mix(base, C(.30, .17, .06), smooth(0.8, 1.0, x) * 0.6)
    scar = smooth(-0.50, -0.62, x + 0.1 * (fbm(P * 8, 2, 1) - 0.5))
    scar_c = mix(C(.80, .68, .46), C(.62, .48, .28), fbm(P * 7, 3, 2))
    return mix(base, scar_c, scar), None


ITEMS["Hazelnut"]["color"] = c_hazel


# --- Walnut
@item("Walnut", rough=0.78, icon=dict(rot=(0, 25, -40)))
def build_walnut():
    m = M()
    n = 12
    prof = []
    for i in range(n + 1):
        t = i / n
        ph = math.pi * t
        x = -1.75 * math.cos(ph)
        pinch = 1 - 0.18 * smooth(0.55, 1.0, t)
        r = 1.5 * math.sin(ph) * pinch
        prof.append((x * CM, r * CM, r * CM * 0.93))
    m.lathe(prof, 16)

    def wr(V):
        d = V / CM
        u = d / np.array([1.75, 1.5, 1.4])
        ln = np.maximum(np.linalg.norm(u, axis=1, keepdims=True), 1e-9)
        dirn = u / ln
        f1, f2 = worley(dirn * 3.0, 4)
        ridge = 1 - smooth(0.0, 0.28, f2 - f1)               # 1 on the cell borders: raised ridges between pits
        seam = np.exp(-(d[:, 1] / 0.16) ** 2) * 0.07
        h = (ridge - 0.45) * 0.16 + (fbm(dirn * 2.0, 2, 21) - 0.5) * 0.22 + seam
        m.a = np.clip(1 - ridge * 0.9, 0, 1).tolist()
        return V + (V / np.maximum(np.linalg.norm(V, axis=1, keepdims=True), 1e-9)) * h[:, None] * CM
    m.displace(wr)
    return m


def c_walnut(P, N, a, b, k):
    d = P / np.array([1.75, 1.5, 1.4])
    f1, f2 = worley(d * 3.0, 4)
    g1 = 1 - smooth(0.0, 0.22, f2 - f1)
    f1b, f2b = worley(d * 7.0 + 3, 9)
    g2 = 1 - smooth(0.0, 0.16, f2b - f1b)
    base = mix(C(.64, .48, .30), C(.50, .36, .21), fbm(P * 3.0, 3, 8))
    base = mix(base, C(.72, .58, .38), smooth(0.45, 0.8, fbm(P * 9, 2, 3)) * 0.3)
    base = mix(base, C(.30, .20, .12), np.clip(g1 * 0.55 + g2 * 0.25, 0, 1))
    seam = np.exp(-(P[:, 1] / 0.10) ** 2)
    base = mix(base, C(.36, .26, .15), seam * 0.5)
    return base, None


ITEMS["Walnut"]["color"] = c_walnut


# --- PineCone: a diamond lattice of pyramids tiles the whole ovoid
@item("PineCone", rough=0.82, mode="flat", icon=dict(rot=(0, 22, -55)))
def build_pinecone():
    m = M()
    Mn, K = 8, 12
    Lc, R = 5.0, 1.2

    def rad(s_):
        return R * 2.6 * math.sqrt(s_) * (1 - s_) ** 0.85 * 0.97
    node = {}
    X = lambda k: ((k / K) - 0.5) * Lc
    for k in range(K + 1):
        if k in (0, K):
            node[(k, 0)] = m.vert((X(k) * CM, 0, 0), 0.0, 0.5)
            continue
        r = rad(k / K)
        for j in range(Mn):
            th = 2 * math.pi * (j + 0.5 * (k % 2)) / Mn
            node[(k, j)] = m.vert((X(k) * CM, r * CM * math.cos(th), r * CM * math.sin(th)), 0.0, j / Mn)

    def nd(k, j):
        return node[(k, 0)] if k in (0, K) else node[(k, j % Mn)]
    for k in range(1, K):
        s_ = k / K
        r = rad(s_)
        p = k % 2
        for j in range(Mn):
            thc = 2 * math.pi * (j + 0.5 + 0.5 * p) / Mn           # centre of the gap j..j+1 on row k
            D, B = nd(k, j), nd(k, j + 1)
            A_, C_ = nd(k - 1, j + p), nd(k + 1, j + p)
            h = 0.30 * (0.55 + 0.45 * math.sin(math.pi * s_)) * (r / R + 0.35)
            ap = m.vert(((X(k) + 0.06) * CM, (r + h) * CM * math.cos(thc), (r + h) * CM * math.sin(thc)), 1.0, 0.5)
            m.face([D, A_, ap])
            m.face([A_, B, ap])
            m.face([B, C_, ap])
            m.face([C_, D, ap])
    return m


def c_pinecone(P, N, a, b, k):
    ang = np.arctan2(P[:, 2], P[:, 1])
    n = fbm(P * 3.5, 3, 4)
    edge = mix(C(.22, .13, .06), C(.33, .20, .10), n)
    apex = mix(C(.50, .35, .18), C(.62, .46, .26), n)
    apex = mix(apex, C(.30, .20, .10), smooth(0.92, 1.0, a) * 0.7)       # dark umbo
    col = mix(edge, apex, smooth(0.0, 0.85, a))
    x = P[:, 0]
    return mix(col, C(.45, .36, .24), smooth(-1.9, -2.15, x) * 0.6), None


ITEMS["PineCone"]["color"] = c_pinecone


# --- Mussel: closed, lying on one valve's side; the shell is a lathe along X with the dorsal edge straight
@item("Mussel", rough=0.34, icon=dict(rot=(0, 30, -35)))
def build_mussel():
    m = M()
    n, segs = 13, 18
    Lm = 5.0
    prof = []
    nn = 2.5
    for i in range(n + 1):
        u = i / n
        w = 1.28 * math.sin(math.pi * u ** 1.4) ** 0.8
        h = 0.98 * math.sin(math.pi * u ** 1.25) ** 0.8
        x = (u - 0.38) * Lm
        prof.append((x * CM, 0.0 if u in (0, 1) else w * CM, h * CM, (0.0 - w * 0.25 if u not in (0, 1) else 0) * CM, 0, u))
    rings = m.lathe(prof, segs)
    V = m.V()
    # superellipse cross-section (sharp commissure at +-y) and bow
    idx = 0
    for ri, p in enumerate(prof):
        if len(rings[ri]) == 1:
            continue
        w, h = p[1], p[2]
        for j, vi in enumerate(rings[ri]):
            t = 2 * math.pi * j / segs
            c, s_ = math.cos(t), math.sin(t)
            V[vi, 1] = p[3] + w * math.copysign(abs(c) ** (2 / nn), c)
            V[vi, 2] = h * math.copysign(abs(s_) ** (2 / nn), s_)
    m.v = V.tolist()
    return m


def c_mussel(P, N, a, b, k):
    d = np.hypot(P[:, 0] + 1.9, P[:, 1] * 1.0)           # distance from the umbo
    rings = np.sin(d * 9.0 + fbm(P * 2.0, 2, 3) * 3.0)
    base = mix(C(.04, .05, .10), C(.15, .18, .34), smooth(-0.3, 0.9, rings) * 0.6 + 0.25 * fbm(P * 4, 3, 2))
    base = mix(base, C(.18, .12, .22), smooth(0.35, 0.8, P[:, 0] * 0.0 + 0.25 + 0.5 * (1 - np.abs(P[:, 2]) / 1.0)) * 0.0)
    # the commissure line and a lighter blue ridge along the dorsal edge
    base = mix(base, C(.02, .02, .04), np.exp(-(P[:, 2] / 0.10) ** 2) * 0.8)
    # chalky wear patch near the umbo on the upper valve
    patch = smooth(1.7, 0.6, np.hypot(P[:, 0] + 1.2, (P[:, 1] + 0.1) * 1.3)) * smooth(-0.1, 0.4, P[:, 2]) * smooth(0.25, 0.55, fbm(P * 4, 3, 7) + 0.3)
    chalk = mix(C(.62, .60, .58), C(.46, .46, .50), fbm(P * 8, 2, 5))
    return mix(base, chalk, patch), None


ITEMS["Mussel"]["color"] = c_mussel


# --- Crust: a torn arch of bread crust
@item("Crust", rough=0.92, icon=None)
def build_crust():
    m = M()
    n, segs = 11, 14
    prof = []
    for i in range(n + 1):
        u = i / n
        e = -1 + 2 * u
        r = 1.65 * (1 - abs(e) ** 3.2) ** 0.55 + 0.25 * (1 - abs(e)) * 0
        r = max(r, 0.5)
        prof.append((e * 3.0 * CM, r * CM, 0.62 * r * CM, 0, 0, 0))
    m.lathe(prof, segs)
    # cap the two torn ends (fans) at slightly ragged x
    V = m.V()
    ang = np.arctan2(V[:, 2], V[:, 1])
    # the crumb face is flat and ragged and looks UP; the crust skin is the underside and the rim
    V[:, 2] *= -1
    zc = 0.12 * CM
    fl = V[:, 2] > zc
    V[fl, 2] = zc + (V[fl, 2] - zc) * 0.10
    nz = fbm(V / CM * 1.8, 3, 6) - 0.5
    V += (np.stack([nz, np.roll(nz, 3), np.roll(nz, 7)], 1)) * 0.5 * CM
    V[fl, 2] += (fbm(V[fl] / CM * 3.5, 2, 3) - 0.5) * 0.14 * CM
    # a: 1 = crust skin, 0 = crumb
    a = np.where(V[:, 2] > zc - 0.12 * CM, 0.0, 1.0)
    xe = np.abs(V[:, 0] / CM)
    a = np.minimum(a, np.where(xe > 2.62, 0.0, 1.0))
    m.a = a.tolist()
    m.v = V.tolist()
    # close the ends
    ends = [(0, -1), (n, 1)]
    verts_by_ring = []
    k = 0
    for p in prof:
        verts_by_ring.append(list(range(k, k + segs)))
        k += segs
    for ri, sgn in ends:
        ring = verts_by_ring[ri]
        cx = np.mean([V[i] for i in ring], 0)
        c = m.vert(cx + np.array([0.15 * CM * sgn, 0, 0]), 0.0, 0.5)
        for j in range(segs):
            j1 = (j + 1) % segs
            m.face([ring[j1], ring[j], c] if sgn < 0 else [ring[j], ring[j1], c])
    return m


def c_crust(P, N, a, b, k):
    n = fbm(P * 2.2, 4, 12)
    crust = mix(C(.52, .29, .10), C(.30, .15, .05), smooth(0.35, 0.75, n))
    crust = mix(crust, C(.68, .42, .16), smooth(0.7, 0.9, fbm(P * 7, 2, 3)) * 0.6)
    h = fbm(P * 6.0, 2, 9)
    holes = smooth(0.42, 0.32, h) + 0.5 * smooth(0.36, 0.26, fbm(P * 12, 2, 4))
    crumb = mix(C(.93, .82, .58), C(.84, .68, .42), fbm(P * 3, 3, 4))
    crumb = mix(crumb, C(.60, .45, .24), np.clip(holes, 0, 1) * 0.6)
    # toasted brown where crumb meets crust
    edge = smooth(0.15, 0.5, a) * smooth(0.95, 0.5, a)
    crumb = mix(crumb, C(.62, .40, .16), smooth(0.0, 0.25, a) * 0.6)
    return mix(crumb, crust, smooth(0.35, 0.7, a)), None


ITEMS["Crust"]["color"] = c_crust


# --- Fish: Baltic herring on its side. Head +X, back +Y, flank +Z
@item("Fish", size=512, rough=0.42, rough_metal=0.32, metal=0.45, icon=None)
def build_fish():
    m = M()
    xs = [0, 0.03, 0.08, 0.18, 0.30, 0.42, 0.55, 0.70, 0.85, 1.0]
    hy = [0.0, 0.55, 1.0, 1.6, 2.0, 2.1, 2.0, 1.5, 0.85, 0.5]
    Ln = 17.0
    n, segs = 32, 16
    prof = []
    ss = np.linspace(0, 1, n + 1)
    hh = dense(xs, hy, n)
    full = np.interp(ss, np.linspace(xs[0], xs[-1], 400), hh)
    for i, s in enumerate(ss):
        half = float(full[i])
        x = (0.5 - s) * Ln                    # nose at +x
        yc = 0.0
        zt = half * (0.50 + 0.12 * (1 - smooth(0, 0.3, s)))
        if i == 0:
            prof.append((x * CM + 0.4 * CM, 0.0, 0.0, 0, 0, 0.0))
            prof.append((x * CM, 0.35 * CM, 0.30 * CM, -0.08 * CM, 0, s))
            continue
        # belly is straighter than the back: shift the section down a little
        prof.append((x * CM, half * CM, zt * CM, -0.10 * half * CM, 0, s))
    # tail end closed
    prof.append(((0.5 - 1.0) * Ln * CM - 0.0, 0.0, 0.0, 0, 0, 1.0))
    # drop the duplicate first ring made above (s = 0)
    m.lathe(prof, segs)
    # fins: single flat sheets (the material is two-sided), in the XY plane
    m.part("fins", sheet=True)
    xb = lambda s: (0.5 - s) * Ln

    def fin(pts):
        m.poly([(x * CM, y * CM, z * CM) for x, y, z in pts], 2, a=0.0)
    yb = lambda s: float(np.interp(s, np.linspace(xs[0], xs[-1], 400), hh))
    # tail: forked
    x_end = xb(1.0)
    fin([(x_end + 0.5, 0.45, 0), (x_end - 2.6, 2.4, 0.0), (x_end - 1.4, 0.0, 0), (x_end - 2.6, -2.4, 0), (x_end + 0.5, -0.45, 0)])
    # dorsal
    fin([(xb(0.43), yb(0.43) * 0.78, 0), (xb(0.56), yb(0.56) * 0.78, 0), (xb(0.52), yb(0.5) + 1.25, 0), (xb(0.43), yb(0.43) + 0.8, 0)])
    # pelvic and anal
    fin([(xb(0.47), -yb(0.47) * 0.92, 0), (xb(0.52), -yb(0.52) * 0.92, 0), (xb(0.55), -yb(0.5) - 0.8, 0)])
    fin([(xb(0.70), -yb(0.70) * 0.92, 0), (xb(0.84), -yb(0.84) * 0.9, 0), (xb(0.86), -yb(0.84) - 0.55, 0), (xb(0.72), -yb(0.7) - 0.7, 0)])
    return m


def c_fish(P, N, a, b, k):
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    s = np.clip(0.5 - x / 17.0, 0, 1.2)
    half = np.interp(np.clip(s, 0, 1), np.linspace(0, 1, 10), [0.4, 0.55, 1.0, 1.6, 2.0, 2.1, 2.0, 1.5, 0.85, 0.5])
    yn = np.clip(y / np.maximum(half, 0.3), -1.2, 1.2)           # +1 back, -1 belly
    scale = fbm(P * np.array([7.0, 7.0, 7.0]), 2, 5)
    back = mix(C(.09, .20, .27), C(.14, .30, .36), scale)
    flank = mix(C(.62, .68, .72), C(.80, .84, .86), scale)
    belly = mix(C(.80, .82, .80), C(.92, .93, .90), scale)
    col = mix(belly, flank, smooth(-0.85, -0.25, yn))
    col = mix(col, back, smooth(0.15, 0.62, yn))
    # lateral stripe: a pinkish-blue sheen mid flank
    col = mix(col, C(.55, .62, .75), np.exp(-((yn - 0.2) / 0.18) ** 2) * 0.35 * smooth(0.0, 0.3, z / np.maximum(half * 0.5, 0.2)))
    # head: darker top, gill cover arc (s ~ 0.2), eye on the +z flank, mouth
    head = smooth(0.27, 0.2, s)
    col = mix(col, mix(C(.13, .24, .27), C(.52, .60, .62), smooth(-0.3, 0.6, -yn)), head * 0.5)
    gill = np.exp(-((s - 0.215 + 0.04 * yn ** 2) / 0.008) ** 2)
    col = mix(col, C(.35, .40, .42), gill * 0.8 * (z > 0.3))
    gcov = smooth(0.19, 0.22, s) * smooth(0.28, 0.24, s) * (np.abs(yn) < 0.75) * (z > 0.2)
    col = mix(col, col * 1.08 + 0.04, gcov * 0.5)
    ex, ey = (0.5 - 0.075) * 17.0, 0.35
    de = np.hypot(x - ex, y - ey)
    eye_on = (z > 0.1)
    col = mix(col, C(.80, .78, .66), smooth(0.52, 0.42, de) * eye_on)
    col = mix(col, C(.02, .02, .02), smooth(0.30, 0.24, de) * eye_on)
    col = mix(col, C(.95, .95, .95), smooth(0.07, 0.03, np.hypot(x - ex + 0.1, y - ey - 0.1)) * eye_on * 0.0)
    mouth = np.exp(-(((y + 0.15) + 0.0) / 0.05) ** 2) * smooth(0.04, 0.012, s) * 0 + 0
    col = mix(col, C(.20, .22, .22), np.exp(-((y + 0.5 * (s) - -0.35) / 0.045) ** 2) * smooth(0.075, 0.03, s) * 0.7)
    # fins: grey-brown, rayed
    fin = (k == 2)
    rays = 0.5 + 0.5 * np.sin(np.arctan2(y, x + 8.5 * 0 + 0.001) * 0 + (x * 11 + y * 17))
    fcol = mix(C(.28, .30, .30), C(.46, .48, .46), rays * 0.7 + 0.3 * fbm(P * 5, 2, 3))
    col = mix(col, fcol, fin)
    return col, None


ITEMS["Fish"]["color"] = c_fish


# --- FeedPile: loose seeds on the ground
@item("FeedPile", rough=0.62, mode="flat", icon=None)
def build_feedpile():
    m = M()
    r = np.random.default_rng(11)
    placed = []     # (x, y, radius)
    SEEDS = {
        0: dict(L=1.0, W=0.46, T=0.2, n=4, rings=[(-0.5, 0.0), (-0.15, 1.0), (0.25, 0.8), (0.5, 0.0)]),     # sunflower
        1: dict(L=0.62, W=0.28, T=0.25, n=5, rings=[(-0.5, 0.0), (-0.1, 1.0), (0.25, 0.8), (0.5, 0.0)]),    # wheat
        2: dict(L=0.24, W=0.22, T=0.22, n=4, rings=[(-0.5, 0.0), (0.0, 1.0), (0.5, 0.0)]),                  # millet
    }
    plan = [0] * 10 + [1] * 14 + [2] * 14
    r.shuffle(plan)
    for si, kd in enumerate(plan):
        sd = SEEDS[kd]
        for _ in range(500):
            rad = abs(r.normal(0, 2.4)) if r.random() < 0.8 else r.uniform(2.5, 5.6)
            ang = r.uniform(0, 2 * math.pi)
            x, y = rad * math.cos(ang), rad * math.sin(ang)
            ra = sd["L"] / 2 + 0.04
            if all(math.hypot(x - px, y - py) > ra + pr for px, py, pr in placed):
                placed.append((x, y, ra))
                break
        else:
            continue
        yaw = r.uniform(0, 2 * math.pi)
        tint = r.random()
        cy, sy = math.cos(yaw), math.sin(yaw)
        m.part("seed%d" % si)
        vr = []
        for (u, w) in sd["rings"]:
            xl = u * sd["L"]
            pts = [(xl, 0.0, 0.0)] if w == 0 else [(xl, 0.5 * sd["W"] * w * math.cos(2 * math.pi * j / sd["n"] + 0.4),
                                                    0.5 * sd["T"] * w * math.sin(2 * math.pi * j / sd["n"] + 0.4)) for j in range(sd["n"])]
            vr.append([m.vert(((x + px * cy - py * sy) * CM, (y + px * sy + py * cy) * CM, (pz + sd["T"] * 0.5) * CM), a=px / sd["L"], b=tint)
                       for (px, py, pz) in pts])
        for i in range(len(vr) - 1):
            r0, r1 = vr[i], vr[i + 1]
            n = max(len(r0), len(r1))
            for j in range(n):
                j1 = (j + 1) % n
                if len(r0) == 1:
                    m.face([r0[0], r1[j1], r1[j]], kd)
                elif len(r1) == 1:
                    m.face([r0[j], r0[j1], r1[0]], kd)
                else:
                    m.face([r0[j], r0[j1], r1[j1], r1[j]], kd)
    return m


def c_feed(P, N, a, b, k):
    up = np.clip(N[:, 2], -1, 1)
    # sunflower: pale grey with dark edge lines and black tip
    sun_pale = mix(C(.70, .68, .62), C(.50, .48, .44), b)
    sun = mix(C(.08, .07, .06), sun_pale, smooth(0.15, 0.55, np.abs(up) + 0.25 * np.cos(a * 8)))
    sun = mix(sun, C(.07, .06, .05), smooth(0.36, 0.5, a) * 0.8)
    wheat = mix(C(.62, .42, .17), C(.78, .58, .26), b)
    wheat = mix(wheat, C(.35, .22, .08), smooth(0.2, 0.0, np.abs(a - 0.1)) * 0.0 + smooth(0.0, 1.0, np.abs(up) < 0.4) * 0.35)
    mil = mix(C(.80, .62, .14), C(.92, .78, .30), b)
    col = np.where((k == 0)[:, None], sun, np.where((k == 1)[:, None], wheat, mil))
    return col, None


ITEMS["FeedPile"]["color"] = c_feed


# --- BottleCap: crown cork, top up, 16 flutes, red enamel on top, gold skirt
@item("BottleCap", rough=0.36, rough_metal=0.28, mixed=True, metal=0.0, mode="smooth", icon=dict(rot=(32, 0, 20)))
def build_bottlecap():
    m = M()
    segs = 32
    # (axis position z, radius, flute depth factor) cm
    pr = [(0.30, 0.0, 0), (0.30, 0.85, 0), (0.24, 1.02, 0.2), (0.0, 1.16, 0.85), (-0.16, 1.26, 1.0), (-0.30, 1.30, 1.0),
          (-0.30, 1.19, 1.0), (-0.12, 1.00, 0.7), (-0.02, 0.5, 0.0), (0.0, 0.0, 0)]
    rings = []
    for z, r, fl in pr:
        if r == 0.0:
            rings.append([m.vert((0, 0, z * CM), 0.0, 0.5)])
            continue
        ring = []
        for j in range(segs):
            peak = 1.0 if j % 2 == 0 else -1.0
            rr = r * (1 + 0.075 * fl * peak)
            t = 2 * math.pi * j / segs
            ring.append(m.vert((rr * CM * math.cos(t), rr * CM * math.sin(t), z * CM), fl, j / segs))
        rings.append(ring)
    for i in range(len(rings) - 1):
        r0, r1 = rings[i], rings[i + 1]
        for j in range(segs):
            j1 = (j + 1) % segs
            if len(r0) == 1 and len(r1) == 1:
                continue
            if len(r0) == 1:
                m.face([r0[0], r1[j1], r1[j]])
            elif len(r1) == 1:
                m.face([r0[j], r0[j1], r1[0]])
            else:
                m.face([r0[j], r0[j1], r1[j1], r1[j]])
    return m


def c_cap(P, N, a, b, k):
    r = np.hypot(P[:, 0], P[:, 1])
    z = P[:, 2]
    ang = np.arctan2(P[:, 1], P[:, 0])
    gold = mix(C(.86, .66, .20), C(.70, .50, .12), fbm(P * 3, 2, 2))
    red = mix(C(.72, .06, .05), C(.55, .03, .03), fbm(P * 4, 3, 5))
    top = (N[:, 2] > 0.55) & (z > 0.2)
    ring = np.exp(-((r - 0.66) / 0.06) ** 2) + smooth(0.26, 0.20, r) + np.exp(-((r - 0.40) / 0.025) ** 2) * 0.9
    face = np.where(top[:, None], mix(red, gold, np.clip(ring, 0, 1)), 0)
    skirt = mix(gold, red, smooth(-0.1, 0.18, z) * 0.0)
    # the shoulder is red paint worn off at the flute peaks, the skirt gold
    wear = fbm(P * 9, 2, 7)
    skirt = mix(skirt, C(.82, .82, .80), smooth(0.7, 0.92, wear) * 0.7)
    shoulder = mix(red, gold, smooth(0.08, -0.05, z))
    col = np.where(top[:, None], face, np.where((z > 0.02)[:, None], shoulder, skirt))
    under = N[:, 2] < -0.2
    col = np.where(under[:, None], mix(C(.55, .55, .52), C(.4, .4, .38), fbm(P * 5, 2, 2)), col)
    # metal mask: gold and bare steel are metal, enamel red mostly not
    mred = (np.linalg.norm(col - red, axis=1) < 0.15) | (np.linalg.norm(col - shoulder, axis=1) < 0.12)
    metal = np.where(top & (np.linalg.norm(col - gold, axis=1) > 0.2), 0.05, np.where(under, 1.0, np.where(z < 0.02, 0.9, 0.85)))
    metal = np.where(top, np.clip(ring, 0, 1) * 0.9 + 0.05, metal)
    return col, metal


ITEMS["BottleCap"]["color"] = c_cap


# --- Foil: crumpled candy wrapper with twisted ends
@item("Foil", rough=0.30, metal=1.0, mode="flat", icon=dict(rot=(0, 18, -30)))
def build_foil():
    m = M()
    tab = [(-2.0, 0.0), (-1.9, 0.10), (-1.55, 0.22), (-1.15, 0.58), (-0.65, 0.88), (-0.2, 0.98), (0.2, 0.98), (0.65, 0.88), (1.15, 0.58), (1.55, 0.22), (1.9, 0.10), (2.0, 0.0)]
    prof = []
    for x, r in tab:
        tw = (x - 1.0 * np.sign(x)) * 1.6 if abs(x) > 1.0 else 0.0
        prof.append((x * CM, r * CM, r * CM * 0.78, 0, 0, 0, float(tw)))
    m.lathe(prof, 14)

    def crumple(V):
        d = V / CM
        rid = 1 - np.abs(2 * fbm(d * 3.4 + 3.0, 3, 31) - 1)
        rid2 = 1 - np.abs(2 * fbm(d * 5.0, 2, 41) - 1)
        h = (rid - 0.55) * 0.55 + (rid2 - 0.5) * 0.22
        rr = np.maximum(np.hypot(d[:, 1], d[:, 2]), 1e-6)
        grow = smooth(0.05, 0.5, rr)
        m.a = np.clip(0.5 + h * 3, 0, 1).tolist()
        dirn = np.stack([np.zeros(len(d)), d[:, 1] / rr, d[:, 2] / rr], 1)
        jit = (np.random.default_rng(3).random((len(d), 3)) - 0.5) * 0.14 * grow[:, None]
        out = d + dirn * (h * grow)[:, None] + np.stack([(rid2 - 0.5) * 0.12 * grow, 0 * rr, 0 * rr], 1) + jit
        return out * CM
    m.displace(crumple)
    return m


def c_foil(P, N, a, b, k):
    blot = fbm(P * 0.9 + 5, 2, 8)
    gold = mix(C(.90, .68, .26), C(.78, .55, .15), fbm(P * 3, 2, 3))
    silv = mix(C(.86, .87, .90), C(.68, .70, .74), fbm(P * 3, 2, 4))
    col = mix(silv, gold, smooth(0.52, 0.62, blot))
    col = mix(col * 0.55, col, smooth(0.12, 0.55, a))
    col = mix(col, C(.96, .96, .94), smooth(0.8, 0.95, a) * 0.5)
    return col, None


ITEMS["Foil"]["color"] = c_foil


# --- Ring: thin brass band lying flat
@item("Ring", lod_build=True, rough=0.34, metal=1.0, mode="smooth", icon=dict(rot=(52, 0, 0)))
def build_ring(lod=0):
    m = M()
    if lod:   # LOD1: a plain rectangular band, 12 sides (own small texture)
        prof = [(-0.11, 0.90), (-0.11, 0.985), (0.11, 0.985), (0.11, 0.90)]
    else:
        prof = [(-0.11, 0.90), (-0.11, 0.955), (-0.065, 0.995), (0.065, 0.995), (0.11, 0.955), (0.11, 0.90), (0.065, 0.88), (-0.065, 0.88)]
    m.lathe([(z * CM, r * CM, r * CM) for z, r in prof], 12 if lod else 28, loop=True)
    m.cycle_axes()
    return m


def c_ring(P, N, a, b, k):
    n = fbm(P * 8, 3, 6)
    col = mix(C(.76, .55, .20), C(.58, .38, .12), n)
    ang = np.arctan2(P[:, 1], P[:, 0])
    col = mix(col, C(.38, .24, .08), smooth(0.65, 0.9, fbm(P * 14, 2, 2)) * 0.5)
    col = mix(col, C(.90, .75, .40), smooth(0.8, 1.0, np.abs(N[:, 2])) * 0.0 + smooth(0.55, 1.0, np.hypot(P[:, 0], P[:, 1]) - 0.93) * 0.35)
    return col, None


ITEMS["Ring"]["color"] = c_ring


# --- Button: 4-hole coat button, dark; holes cut with a boolean
@item("Button", lod_build=True, rough=0.28, mode="smooth", icon=dict(rot=(48, 0, 0)))
def build_button(lod=0):
    m = M()
    prof = [(-0.13, 0.0), (-0.13, 0.90), (-0.10, 0.97), (-0.04, 1.0), (0.07, 0.995), (0.13, 0.94), (0.13, 0.78), (0.10, 0.70), (0.03, 0.62), (0.03, 0.0)]
    if lod:
        prof = [(-0.13, 0.0), (-0.13, 0.96), (0.13, 0.96), (0.13, 0.70), (0.03, 0.62), (0.03, 0.0)]
    m.lathe([(z * CM, r * CM, r * CM) for z, r in prof], 8 if lod else 20)
    m.cycle_axes()
    m.orient()
    ob = to_object(m, "btn_body", smooth_mode="flat")
    cyl = M()
    for (hx, hy) in ((0.2, 0.2), (-0.2, 0.2), (0.2, -0.2), (-0.2, -0.2)):
        h = M()
        h.lathe([(-0.3 * CM, 0.0, 0.0), (-0.3 * CM, 0.08 * CM, 0.08 * CM), (0.3 * CM, 0.08 * CM, 0.08 * CM), (0.3 * CM, 0.0, 0.0)], 4 if lod else 6)
        h.cycle_axes()
        cyl.append(h, (hx * CM, hy * CM, 0.0))
    cyl.orient()
    co = to_object(cyl, "btn_holes", smooth_mode="flat")
    if lod:   # LOD1: no holes in the mesh (painted in its own texture)
        bpy.data.objects.remove(co, do_unlink=True)
        return ob
    r = boolean(ob, [co], "DIFFERENCE")
    return r


def c_button(P, N, a, b, k):
    n = fbm(P * 6, 3, 8)
    col = mix(C(.075, .060, .050), C(.13, .10, .08), n)
    r = np.hypot(P[:, 0], P[:, 1])
    col = mix(col, C(.18, .14, .11), np.exp(-((r - 0.99) / 0.05) ** 2) * 0.7)
    hole = np.min([np.hypot(P[:, 0] - hx, P[:, 1] - hy) for hx in (-0.2, 0.2) for hy in (-0.2, 0.2)], axis=0)
    col = mix(col, C(.01, .01, .01), smooth(0.10, 0.07, hole) * (N[:, 2] > 0.5))
    return col, None


ITEMS["Button"]["color"] = c_button


# --- Coin
@item("Coin", rough=0.40, rough_metal=0.4, metal=1.0, mode="smooth", icon=dict(rot=(60, 0, 15)))
def build_coin():
    m = M()
    prof = [(-0.09, 0.0), (-0.09, 0.94), (-0.075, 0.99), (0.075, 0.99), (0.09, 0.94), (0.085, 0.86), (0.05, 0.84), (0.05, 0.50), (0.075, 0.47), (0.075, 0.0)]
    m.lathe([(z * CM, r * CM, r * CM) for z, r in prof], 20)
    m.cycle_axes()
    return m


def c_coin(P, N, a, b, k):
    r = np.hypot(P[:, 0], P[:, 1])
    ang = np.arctan2(P[:, 1], P[:, 0])
    n = fbm(P * 7, 3, 5)
    col = mix(C(.74, .54, .20), C(.58, .40, .12), n)
    # bead ring in the field and an emblem: a five-pointed star in the middle
    beads = smooth(0.5, 0.9, np.cos(ang * 20)) * np.exp(-((r - 0.70) / 0.06) ** 2)
    star = np.cos(ang * 5) * 0.12 + 0.28
    emblem = smooth(star + 0.03, star - 0.03, r) * (r < 0.5)
    col = mix(col, C(.88, .70, .32), np.clip(beads + emblem, 0, 1) * 0.8)
    col = mix(col, C(.40, .27, .10), smooth(0.75, 0.95, fbm(P * 12, 2, 4)) * 0.55)
    col = mix(col, C(.92, .78, .45), np.exp(-((r - 0.97) / 0.03) ** 2) * 0.5)
    return col, None


ITEMS["Coin"]["color"] = c_coin


# --- Key: bow, collar, shaft and bit, fused with a boolean union
@item("Key", lod_build=True, rough=0.5, metal=0.85, mode="smooth", icon=dict(rot=(55, 0, 28)))
def build_key(lod=0):
    parts = []
    # bow: a torus about Z
    t = M()
    bp = [(-0.13, 0.55), (-0.13, 0.67), (0.0, 0.74), (0.13, 0.67), (0.13, 0.55), (0.0, 0.50)]
    if lod:
        bp = [(-0.12, 0.50), (-0.12, 0.74), (0.12, 0.74), (0.12, 0.50)]
    t.lathe([(z * CM, r * CM, r * CM) for z, r in bp], 8 if lod else 20, loop=True)
    # lathe about X -> make it about Z (axis cycle) and move
    t.cycle_axes()
    t.v = [[v[0] - 1.80 * CM, v[1], v[2]] for v in t.v]
    t.orient()
    parts.append(to_object(t, "key_bow", smooth_mode="smooth"))
    # shaft + collar
    s = M()
    sp = [(-1.16 * CM, 0.0, 0.0), (-1.16 * CM, 0.13 * CM, 0.13 * CM), (-0.95 * CM, 0.13 * CM, 0.13 * CM), (-0.95 * CM, 0.22 * CM, 0.22 * CM),
          (-0.80 * CM, 0.22 * CM, 0.22 * CM), (-0.80 * CM, 0.13 * CM, 0.13 * CM), (2.30 * CM, 0.13 * CM, 0.13 * CM), (2.30 * CM, 0.0, 0.0)]
    if lod:
        sp = [sp[0], sp[1], sp[6], sp[7]]
    s.lathe(sp, 4 if lod else 8)
    s.orient()
    parts.append(to_object(s, "key_shaft", smooth_mode="smooth"))
    # bit: a plate with two notches (cut with boxes)
    bm_ = M()
    x0, x1, y0, y1, h = 1.30, 2.50, -0.95, -0.02, 0.07

    def box(x0, x1, y0, y1, z0, z1):
        b = M()
        p = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        ids = [b.vert((a * CM, c * CM, d * CM)) for a, c, d in p]
        for f in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            b.face([ids[i] for i in f])
        b.orient()
        return b
    parts.append(to_object(box(x0, x1, y0, y1, -h, h), "key_bit", smooth_mode="flat"))
    cut1 = to_object(box(1.55, 1.78, y0 - 0.1, y0 + 0.38, -0.3, 0.3), "key_cut1", smooth_mode="flat")
    cut2 = to_object(box(2.05, 2.28, y0 + 0.30, y0 + 0.62, -0.3, 0.3), "key_cut2", smooth_mode="flat")
    bit = boolean(parts[2], [cut1, cut2], "DIFFERENCE")
    whole = boolean(parts[0], [parts[1], bit], "UNION")
    return whole


def c_key(P, N, a, b, k):
    n = fbm(P * 5, 3, 4)
    col = mix(C(.40, .29, .13), C(.22, .15, .07), n)
    col = mix(col, C(.62, .46, .20), smooth(0.7, 1.0, fbm(P * 11, 2, 9)) * 0.55)      # bright worn spots
    col = mix(col, C(.16, .11, .06), smooth(0.55, 0.85, fbm(P * 3 + 4, 3, 2)) * 0.5)
    return col, None


ITEMS["Key"]["color"] = c_key


# --- SeaGlass: a frosted, tumbled flat pebble
@item("SeaGlass", rough=0.62, mode="smooth", icon=dict(rot=(0, 28, -20)))
def build_seaglass():
    m = M()
    n, segs = 9, 14
    prof = []
    for i in range(n + 1):
        u = i / n
        x = -1.25 * math.cos(math.pi * u)
        rr = math.sin(math.pi * u)
        prof.append((x * CM, 0.0 if i in (0, n) else 0.95 * rr * CM, 0.0 if i in (0, n) else 0.42 * rr ** 0.8 * CM))
    m.lathe(prof, segs)

    def tumble(V):
        d = V / CM
        h = (fbm(d * 0.9 + 2, 2, 5) - 0.5) * 0.45
        rr = np.maximum(np.hypot(d[:, 1], d[:, 2]), 1e-6)
        out = d.copy()
        out[:, 1] += d[:, 1] / rr * h * 0.8 * smooth(0, 0.3, rr)
        out[:, 2] *= 1 + h * 0.4
        out[:, 0] += (fbm(d * 0.8 + 9, 2, 6) - 0.5) * 0.25 * smooth(0, 0.3, rr)
        return out * CM
    m.displace(tumble)
    return m


def c_glass(P, N, a, b, k):
    edge = 1 - np.abs(N[:, 2])
    base = mix(C(.30, .60, .44), C(.42, .72, .54), fbm(P * 1.4, 2, 3))
    frost = fbm(P * 13, 3, 12)
    col = mix(base, C(.80, .90, .84), smooth(0.35, 0.75, frost) * (0.50 + 0.25 * edge))
    col = mix(col, C(.12, .30, .20), smooth(0.78, 0.92, fbm(P * 22, 1, 7)) * 0.45)
    return col, None


ITEMS["SeaGlass"]["color"] = c_glass


# --- Stick: a dry twig with a side fork
@item("Stick", rough=0.88, mode="smooth", icon=None)
def build_stick():
    m = M()
    n = 13
    xs = np.linspace(-15, 15, n)
    path = np.stack([xs, 0.9 * np.sin(xs / 9.0) + 0.2, 0.25 * np.sin(xs / 6.0 + 1.0) + 0.05], 1) * CM
    rad = np.interp(xs, [-15, -3, 15], [0.52, 0.42, 0.20]) * CM
    rr = np.random.default_rng(5)
    wob = lambda i, j: 0.10 * math.sin(i * 1.7 + j * 2.3) + 0.04 * ((i * 7 + j * 3) % 5 - 2) / 2.0
    m.part("main")
    m.tube(path, rad, 7, wob=wob)
    # fork at x ~ -2: leaves at ~38 degrees
    bi = int(np.argmin(np.abs(xs + 2.0)))
    b0 = path[bi] - np.array([0, 0.0, 0]) * CM
    d = np.array([math.cos(math.radians(38)), math.sin(math.radians(38)), 0.10])
    d /= np.linalg.norm(d)
    fp = [b0 + d * (k * 2.4 + (-0.2)) * CM + np.array([0, 0, 0.3 * math.sin(k * 0.9) * 0.1]) * CM for k in range(6)]
    fr = [0.24 * CM, 0.22 * CM, 0.19 * CM, 0.16 * CM, 0.13 * CM, 0.10 * CM]
    m.part("fork")
    m.tube(np.array(fp), fr, 6, cap0=False, wob=lambda i, j: 0.08 * math.sin(i * 2.1 + j))
    # two stubs of broken twiglets
    for xx, sgn in ((6.0, -1), (-9.0, 1)):
        i = int(np.argmin(np.abs(xs - xx)))
        p0 = path[i]
        dd = np.array([0.25, -sgn * 1.0, 0.25])
        dd /= np.linalg.norm(dd)
        m.part("stub")
        m.tube(np.array([p0, p0 + dd * 0.5 * CM, p0 + dd * 1.1 * CM]), [0.16 * CM, 0.13 * CM, 0.09 * CM], 5, cap0=False)
    return m


def c_stick(P, N, a, b, k):
    x = P[:, 0]
    ang = np.arctan2(P[:, 2], P[:, 1] - 0.9 * np.sin(x / 9.0) * 0)
    st = noise1(b * 6.0, x * 0.35, 0.3, 3) * 0.6 + noise1(b * 14.0, x * 0.9, 1.0, 4) * 0.4
    bark = mix(C(.36, .29, .22), C(.22, .17, .12), smooth(0.35, 0.72, st))
    bark = mix(bark, C(.50, .46, .40), smooth(0.7, 0.95, fbm(P * 2.2, 3, 8)) * 0.6)      # grey lichen / dried patches
    cut = mix(C(.72, .58, .38), C(.52, .38, .22), fbm(P * 9, 2, 3))
    return mix(bark, cut, smooth(0.3, 0.8, a)), None


ITEMS["Stick"]["color"] = c_stick


# ---------------------------------------------------------------- boolean helper

def boolean(ob, others, op):
    bpy.context.view_layer.objects.active = ob
    for o in others:
        md = ob.modifiers.new("bool", "BOOLEAN")
        md.operation = op
        md.solver = "EXACT"
        md.object = o
        bpy.ops.object.modifier_apply(modifier=md.name)
    for o in others:
        bpy.data.objects.remove(o, do_unlink=True)
    return ob


# ---------------------------------------------------------------- building one item

def finish_object(ob, spec, is_obj):
    """For items the builder returns a finished object (boolean); wrap: pull geometry back into an M with attributes zeroed."""
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=8e-5)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges[:], dist=8e-5)
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(0.5), verts=bm.verts[:], edges=bm.edges[:], delimit={"NORMAL"})
    bm.to_mesh(me)
    bm.free()
    m = M()
    m.v = [list(v.co) for v in me.vertices]
    m.a = [0.0] * len(m.v)
    m.b = [0.0] * len(m.v)
    m.f = [list(p.vertices) for p in me.polygons]
    m.fk = [0] * len(m.f)
    m.fp = [0] * len(m.f)
    bpy.data.objects.remove(ob, do_unlink=True)
    return m


def build_item(name, spec, lod=0, shift=None):
    if not lod:
        clear()
    res = spec["build"](lod=1) if lod else spec["build"]()
    if isinstance(res, bpy.types.Object):
        m = finish_object(res, spec, True)
    else:
        m = res
    m.orient()
    if shift is None:
        if name == "FeedPile":
            c = m.V().mean(0)
            shift = np.array([c[0], c[1], m.V()[:, 2].mean()])
        else:
            shift = m.volume_centroid()
    ob = to_object(m, name + ("_LOD1" if lod else ""), shift=shift, smooth_mode=spec["mode"])
    unwrap(ob)
    path = bake(ob, spec["color"], 128 if lod else spec["size"], shift, spec["mixed"], "T_%s%s.png" % (name, "_LOD1" if lod else ""))
    return m, ob, shift, path


def make_lod1(ob, ratio, spec):
    lod = ob.copy()
    lod.data = ob.data.copy()
    lod.name = ob.name + "_LOD1"
    bpy.context.scene.collection.objects.link(lod)
    bpy.context.view_layer.objects.active = lod
    md = lod.modifiers.new("dec", "DECIMATE")
    md.decimate_type = "COLLAPSE"
    md.ratio = ratio
    md.use_collapse_triangulate = True
    md.delimit = set() if os.environ.get("LOD_FREE") else {"UV"}
    md.use_symmetry = False
    bpy.ops.object.modifier_apply(modifier=md.name)
    shade(lod.data, spec["mode"])
    return lod


def tri_count(ob):
    ob.data.calc_loop_triangles()
    return len(ob.data.loop_triangles)


def export_fbx(ob, path):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, global_scale=1.0, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", axis_forward="-Y", axis_up="Z", mesh_smooth_type="EDGE",
                             use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False)


# ---------------------------------------------------------------- rendering

def material_for(spec, png, name):
    mt = bpy.data.materials.new(name)
    mt.use_nodes = True
    nt = mt.node_tree
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    img = bpy.data.images.load(png, check_existing=False)
    img.colorspace_settings.name = "sRGB"
    img.alpha_mode = "CHANNEL_PACKED"
    tx = nt.nodes.new("ShaderNodeTexImage")
    tx.image = img
    nt.links.new(tx.outputs["Color"], b.inputs["Base Color"])
    if spec["mixed"]:   # roughness and metal from the mask, like the Unreal master
        mr = nt.nodes.new("ShaderNodeMapRange")
        mr.inputs[3].default_value = spec["rough"]
        mr.inputs[4].default_value = spec["rough_metal"]
        nt.links.new(tx.outputs["Alpha"], mr.inputs[0])
        nt.links.new(mr.outputs[0], b.inputs["Roughness"])
        nt.links.new(tx.outputs["Alpha"], b.inputs["Metallic"])
    else:
        b.inputs["Roughness"].default_value = spec["rough"] if spec["metal"] < 0.5 else spec["rough_metal"]
        b.inputs["Metallic"].default_value = spec["metal"]
    return mt


def studio(scn, size, samples):
    scn.render.engine = "CYCLES"
    scn.cycles.samples = samples
    scn.cycles.use_denoising = True
    scn.cycles.device = "CPU"
    scn.render.resolution_x = scn.render.resolution_y = size
    scn.render.film_transparent = True
    scn.render.image_settings.file_format = "PNG"
    scn.render.image_settings.color_mode = "RGBA"
    scn.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("w")
    scn.world = w
    w.use_nodes = True
    nt = w.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputWorld")
    bg = nt.nodes.new("ShaderNodeBackground")
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    rng_ = nt.nodes.new("ShaderNodeMapRange")
    rng_.inputs[1].default_value, rng_.inputs[2].default_value = -1.0, 1.0
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.30
    ramp.color_ramp.elements[0].color = (0.10, 0.09, 0.09, 1)
    ramp.color_ramp.elements[1].position = 0.80
    ramp.color_ramp.elements[1].color = (0.95, 0.93, 0.90, 1)
    # the sky as a vertical gradient (the world's "Position"/"Normal" is the ray direction)
    nt.links.new(geo.outputs["Normal"], sep.inputs[0])
    nt.links.new(sep.outputs["Z"], rng_.inputs[0])
    nt.links.new(rng_.outputs[0], ramp.inputs[0])
    nt.links.new(ramp.outputs[0], bg.inputs[0])
    bg.inputs[1].default_value = 0.6
    nt.links.new(bg.outputs[0], out.inputs[0])


def lights(scn):
    def area(name, loc, size, energy, color=(1, 1, 1)):
        ld = bpy.data.lights.new(name, "AREA")
        ld.energy = energy
        ld.size = size
        ld.color = color
        lo = bpy.data.objects.new(name, ld)
        scn.collection.objects.link(lo)
        lo.location = loc
        lo.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    area("key", (-0.30, -0.35, 0.45), 0.45, 4.0, (1.0, 0.95, 0.88))
    area("fill", (0.40, -0.20, 0.25), 0.55, 1.0, (0.85, 0.9, 1.0))
    area("rim", (0.10, 0.45, 0.30), 0.30, 1.6, (1, 1, 1))


def frame_camera(scn, verts, direction, margin, fov_deg):
    cam_d = bpy.data.cameras.new("cam")
    cam_d.lens_unit = "FOV"
    cam_d.angle = math.radians(fov_deg)
    cam_d.clip_start = 0.002
    cam_d.clip_end = 10.0
    cam = bpy.data.objects.new("cam", cam_d)
    scn.collection.objects.link(cam)
    scn.camera = cam
    d = Vector(direction).normalized()
    rot = (-d).to_track_quat("-Z", "Y")
    right, up = rot @ Vector((1, 0, 0)), rot @ Vector((0, 1, 0))
    V = np.array(verts)
    px, py, along = V @ np.array(right), V @ np.array(up), V @ np.array(d)
    cx, cy = (px.min() + px.max()) / 2, (py.min() + py.max()) / 2
    px, py = px - cx, py - cy
    t = math.tan(math.radians(fov_deg) / 2) * margin
    dist = max(np.max(np.abs(px) / t + along), np.max(np.abs(py) / t + along))
    cam.location = d * dist + right * cx + up * cy
    cam.rotation_euler = rot.to_euler()


def render_view(ob, spec, png, out, direction, size=512, samples=48, rot=(0, 0, 0), margin=0.80, fov=30.0):
    """Render a copy of ob (rotated by rot degrees, XYZ) on a shadow-catcher floor in a throwaway scene; transparent PNG."""
    scn = bpy.data.scenes.new("r")
    me = ob.data.copy()
    ob2 = bpy.data.objects.new("o", me)
    scn.collection.objects.link(ob2)
    R = Euler([math.radians(a) for a in rot], "XYZ").to_matrix().to_4x4()
    ob2.matrix_world = R
    me.materials.clear()
    me.materials.append(material_for(spec, png, "m_" + ob.name))
    wv = [tuple(R @ v.co) for v in me.vertices]
    zmin = min(v[2] for v in wv)
    studio(scn, size, samples)
    lights(scn)
    fm = bpy.data.meshes.new("floor")
    fm.from_pydata([(-0.6, -0.6, zmin), (0.6, -0.6, zmin), (0.6, 0.6, zmin), (-0.6, 0.6, zmin)], [], [(0, 1, 2, 3)])
    fo = bpy.data.objects.new("floor", fm)
    scn.collection.objects.link(fo)
    fo.is_shadow_catcher = True
    frame_camera(scn, wv, direction, margin, fov)
    scn.render.filepath = out
    bpy.ops.render.render(write_still=True, scene=scn.name)
    bpy.data.scenes.remove(scn)
    bpy.data.objects.remove(ob2, do_unlink=True)


VIEW = (-0.55, -0.75, 0.62)


def main():
    names = [a for a in ARGS if not a.startswith("--")]
    do_render = "--no-render" not in ARGS
    do_icons = do_render and "--no-icons" not in ARGS
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(RENDERS, exist_ok=True)
    meta_path = os.path.join(OUT, "meta.json")
    meta = json.load(open(meta_path)) if os.path.exists(meta_path) else {}
    for name, spec in ITEMS.items():
        if names and name not in names:
            continue
        m, ob, shift, png = build_item(name, spec)
        tris = tri_count(ob)
        res = lint_bird.lint(ob, verbose=True)
        lod_png = None
        if spec["lod_build"]:      # rebuilt coarser (decimation twists thin revolved shapes), own small texture
            _, lod, _, lod_png = build_item(name, spec, lod=1, shift=shift)
        else:
            lod = make_lod1(ob, spec["lod"], spec)
        ltris = tri_count(lod)
        for o in (ob, lod):
            o.data.materials.clear()
            o.data.materials.append(bpy.data.materials.new("M_" + name))
        export_fbx(ob, os.path.join(OUT, name + ".fbx"))
        export_fbx(lod, os.path.join(OUT, name + "_LOD1.fbx"))
        V = np.array([v.co[:] for v in ob.data.vertices])
        ext = (V.max(0) - V.min(0)) / CM
        meta[name] = dict(tris=tris, lod1_tris=ltris, size_cm=[round(float(e), 2) for e in ext], ground_offset_cm=round(float(-V[:, 2].min() / CM), 3),
                          texture="T_%s.png" % name, texture_size=spec["size"], lod1_texture=("T_%s_LOD1.png" % name) if lod_png else None, mixed_metal_mask=spec["mixed"], roughness=spec["rough"],
                          roughness_metal=spec["rough_metal"], metal=spec["metal"], lint_issues=int(res["issues"]),
                          lint=dict((k, res[k]) for k in ("degenerate", "uv_zero", "uv_stretch", "uv_outside", "zfight", "inside", "near_layers", "crossing")))
        print("ITEM %s: %d tris, LOD1 %d tris, size cm %s, origin %.2f cm above the lowest point, lint issues %d" % (
            name, tris, ltris, meta[name]["size_cm"], meta[name]["ground_offset_cm"], res["issues"]))
        json.dump(meta, open(meta_path, "w"), indent=1)
        if do_render:
            fast = "--fast" in ARGS
            render_view(ob, spec, png, os.path.join(RENDERS, name + ".png"), VIEW, samples=12 if fast else 40)
            if fast and "--lod" in ARGS:
                render_view(lod, spec, lod_png or png, os.path.join(RENDERS, name + "_LOD1.png"), VIEW, samples=10)
            if not fast:
                render_view(lod, spec, lod_png or png, os.path.join(RENDERS, name + "_LOD1.png"), VIEW, samples=24)
                render_view(ob, spec, png, os.path.join(RENDERS, name + "_top.png"), (0.05, -0.15, 1.0), samples=24)
            if do_icons and spec["icon"]:
                render_view(ob, spec, png, os.path.join(RENDERS, "icon_%s.png" % name), (-0.38, -0.55, 0.75), size=1024, samples=96,
                            rot=spec["icon"]["rot"], margin=0.80, fov=26.0)
    if do_render:
        subprocess.run(["python3", os.path.join(HERE, "corvid_items_sheet.py"), OUT, ROOT], check=False)


if __name__ == "__main__":
    main()
