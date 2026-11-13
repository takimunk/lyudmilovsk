"""Topi's props (docs/topi.md): the warning plate on its post, the Ural's board sign, three beach towels and a path stake with a rag.

  python3 blender/topi/props.py                               (bpy as a Python module: pip install bpy)
  Blender -b --python blender/topi/props.py                   (or Blender's own Python)
  -> blender/topi/out/SM_Topi_{WarnSign,UralSign,Towel_A,Towel_B,Towel_C,Stake}.glb

Metres, origin at ground level, exported with export_yup (glTF): Blender x -> Unreal X, y -> -Y, so a sign's front (Blender -y)
faces Unreal +Y = map north at yaw 0. Blender's glTF export writes V as 1 - v and Unreal imports it as is, so UVs here
are plain Blender ones: v = 1 at the top of a picture. Material slots (named, read by unreal/scripts/import_topi.py):
TP_Wood, TP_Metal, TP_Rag, TP_Towel flat/tiling; TP_SignWarn and TP_SignUral have 0..1 UVs over the plate's / board's front
(tools/make_topi_signs.py draws them; ULyudTopiProps shows the player's language). Everything is low poly with its own UVs.
"""
import math
import os
import random

import bpy  # noqa: F401  (first: it registers bmesh)
import bmesh  # noqa: E402
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")

# slot: (hex colour for the placeholder, roughness, metallic)
MAT_SPEC = {
    "TP_Wood": ("#6F675C", 0.9, 0.0),
    "TP_Metal": ("#7B756C", 0.55, 0.8),
    "TP_SignWarn": ("#D9D2B8", 0.5, 0.0),
    "TP_SignUral": ("#3A342D", 0.85, 0.0),
    "TP_Towel": ("#B7A58A", 0.95, 0.0),
    "TP_Rag": ("#A8483C", 0.95, 0.0),
}
WOOD_TILE = 0.5            # metres of wood per texture tile


def _lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def make_material(name):
    hx, rough, metal = MAT_SPEC[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    c = _lin(hx)
    bsdf.inputs["Base Color"].default_value = (c[0], c[1], c[2], 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    m.diffuse_color = (c[0], c[1], c[2], 1.0)
    return m


class Builder:
    """One mesh object: a bmesh with a UV layer and an ordered list of material slots."""

    def __init__(self, name, slots):
        self.name = name
        self.slots = list(slots)
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.cache = {}

    def vert(self, p):
        """A vertex at p, shared with any other face that meets there (smooth shading needs shared vertices)."""
        key = (round(p.x, 6), round(p.y, 6), round(p.z, 6))
        v = self.cache.get(key)
        if v is None:
            v = self.cache[key] = self.bm.verts.new(p)
        return v

    def mat(self, slot):
        return self.slots.index(slot)

    def face(self, pts, slot, uvs, smooth=False):
        """A face from points (Vectors, counter-clockwise from outside) and one UV per point."""
        vs = [self.vert(p) for p in pts]
        f = self.bm.faces.new(vs)
        f.material_index = self.mat(slot)
        f.smooth = smooth
        for loop, uv in zip(f.loops, uvs):
            loop[self.uv].uv = uv
        return f

    def box(self, centre, size, slot, warp=None, rng=None, jitter=0.0, grain="z"):
        """A cuboid. warp(p) moves each corner; jitter: random corner offset (m). UVs: metre-scale projection, wood grain along `grain`."""
        c = Vector(centre)
        h = Vector(size) * 0.5
        corners = []
        for idx in range(8):
            p = Vector((c.x + (h.x if idx & 1 else -h.x), c.y + (h.y if idx & 2 else -h.y), c.z + (h.z if idx & 4 else -h.z)))
            if warp:
                p = warp(p)
            if rng and jitter:
                p = p + Vector((rng.uniform(-jitter, jitter) for _ in range(3)))
            corners.append(p)
        # outward faces, counter-clockwise seen from outside
        faces = ((1, 3, 7, 5), (0, 4, 6, 2), (2, 6, 7, 3), (0, 1, 5, 4), (4, 5, 7, 6), (0, 2, 3, 1))
        for fi in faces:
            pts = [corners[i] for i in fi]
            n = (pts[1] - pts[0]).cross(pts[2] - pts[1])
            ax = max(range(3), key=lambda a: abs(n[a]))
            others = [a for a in range(3) if a != ax]
            gi = "xyz".index(grain)
            if gi in others:                      # grain runs along v
                va = gi
                ua = [a for a in others if a != gi][0]
            else:
                ua, va = others
            uvs = [(p[ua] / WOOD_TILE, p[va] / WOOD_TILE) for p in pts]
            self.face(pts, slot, uvs)

    def slab(self, nx, ny, front, back, slot_front, slot_back, slot_edge, uv_front, uv_back=None, smooth=True):
        """A thin solid over a grid: front(i, j) and back(i, j) are Vectors; the front's normal is d/di x d/dj. uv_front(i, j) -> (u, v)."""
        uv_back = uv_back or uv_front
        F = [[front(i, j) for j in range(ny + 1)] for i in range(nx + 1)]
        B = [[back(i, j) for j in range(ny + 1)] for i in range(nx + 1)]
        for i in range(nx):
            for j in range(ny):
                q = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                self.face([F[a][b] for a, b in q], slot_front, [uv_front(a, b) for a, b in q], smooth)
                self.face([B[a][b] for a, b in reversed(q)], slot_back, [uv_back(a, b) for a, b in reversed(q)], smooth)

        def tangents(i, j):
            i0, i1 = max(i - 1, 0), min(i + 1, nx)
            j0, j1 = max(j - 1, 0), min(j + 1, ny)
            return F[i1][j] - F[i0][j], F[i][j1] - F[i][j0]

        def edge(a, b, outward):
            """Quad between border points a and b (grid indices); outward: which way the border faces in grid space."""
            pts = [F[a[0]][a[1]], F[b[0]][b[1]], B[b[0]][b[1]], B[a[0]][a[1]]]
            ei, ej = tangents(*a)
            out = {"-j": -ej, "+j": ej, "-i": -ei, "+i": ei}[outward]
            n = (pts[1] - pts[0]).cross(pts[2] - pts[1])
            if n.dot(out) < 0:
                pts.reverse()
                order = [b, a, a, b]
            else:
                order = [a, b, b, a]
            self.face(pts, slot_edge, [uv_front(*k) for k in order], False)
        for i in range(nx):
            edge((i, 0), (i + 1, 0), "-j")
            edge((i, ny), (i + 1, ny), "+j")
        for j in range(ny):
            edge((0, j), (0, j + 1), "-i")
            edge((nx, j), (nx, j + 1), "+i")

    def build(self):
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        for slot in self.slots:
            me.materials.append(MATERIALS[slot])
        obj = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(obj)
        return obj


MATERIALS = {}


def smoothstep(a, b, x):
    t = min(max((x - a) / (b - a), 0.0), 1.0)
    return t * t * (3 - 2 * t)


# ------------------------------------------------------------------------------------------------ warning sign

def warn_sign():
    rng = random.Random(11)
    b = Builder("SM_Topi_WarnSign", ["TP_Wood", "TP_SignWarn", "TP_Metal"])
    # the post: 9 x 9 cm, 0.5 m in the ground, 2.2 m above it; tapers a little, the top is cut aslant and uneven
    def post_warp(p):
        k = (p.z + 0.5) / 2.7
        s = 1.0 - 0.08 * k
        z = p.z + (0.03 * (p.y / 0.045) if p.z > 1.0 else 0.0)
        return Vector((p.x * s, p.y * s, z))
    b.box((0, 0, 0.85), (0.09, 0.09, 2.7), "TP_Wood", warp=post_warp, rng=rng, jitter=0.004)
    # the plate: 70 x 50 cm, 4 mm, front on the post's -y side, bent at one corner, dented, bowed a little
    W, H, T = 0.70, 0.50, 0.004
    NX, NY = 14, 10
    zc = 1.65
    yf = -0.045 - 0.006

    def plate_front(i, j):
        u, v = i / NX, j / NY
        x, z = (u - 0.5) * W, zc + (v - 0.5) * H
        bend = 0.022 * smoothstep(0.55, 1.0, (u + v) * 0.5 + 0.18 * (u - v)) ** 1.5          # top right corner bent toward the viewer
        bow = 0.006 * (1 - ((u - 0.5) * 2) ** 2)
        dent = 0.011 * math.exp(-(((u - 0.30) / 0.11) ** 2 + ((v - 0.34) / 0.13) ** 2))     # pushed in
        dent2 = 0.005 * math.exp(-(((u - 0.78) / 0.07) ** 2 + ((v - 0.18) / 0.10) ** 2))
        wob = rng_plate[i][j]
        return Vector((x, yf - bow - bend + dent + dent2 + wob, z + 0.012 * smoothstep(0.7, 1.0, (u + v) / 2) * (1 if v > 0.5 else 0)))
    rng_plate = [[rng.uniform(-0.0008, 0.0008) for _ in range(NY + 1)] for _ in range(NX + 1)]

    def plate_back(i, j):
        p = plate_front(i, j)
        return Vector((p.x, p.y + T, p.z))
    b.slab(NX, NY, plate_front, plate_back, "TP_SignWarn", "TP_Metal", "TP_Metal", lambda i, j: (i / NX, j / NY))
    # four bolts holding it (the picture shows their rust)
    for sx in (-1, 1):
        for sz in (-1, 1):
            b.box((sx * 0.30, yf - 0.006, zc + sz * 0.20), (0.022, 0.008, 0.022), "TP_Metal", rng=rng, jitter=0.0015)
    return b.build()


# ------------------------------------------------------------------------------------------------ Ural's board sign

def ural_sign():
    rng = random.Random(23)
    b = Builder("SM_Topi_UralSign", ["TP_Wood", "TP_SignUral"])
    # two hand-planed posts, 7 x 7 cm, 0.4 m in the ground
    for sx, top in ((-1, 1.40), (1, 1.34)):
        height = top + 0.4
        b.box((sx * 0.47, 0.0, top - height / 2), (0.07, 0.07, height), "TP_Wood", rng=rng, jitter=0.004)
    # the board: 110 x 35 cm, 3 cm, nailed over the posts' front, a degree off the level, edges uneven
    W, H, T = 1.10, 0.35, 0.03
    NX, NY = 8, 2
    zc = 1.08
    yf = -0.035 - T
    tilt = math.radians(2.0)
    noise = [[rng.uniform(-1, 1) for _ in range(NY + 1)] for _ in range(NX + 1)]

    def board_front(i, j):
        u, v = i / NX, j / NY
        x, z = (u - 0.5) * W, (v - 0.5) * H
        border = 1.0 if (j in (0, NY)) else 0.0
        z += 0.004 * noise[i][j] * border
        y = yf + 0.004 * math.sin(u * 3.1) + 0.0025 * noise[i][j] * (1 - border)
        # rotate about y by the tilt around the board's centre
        xr = x * math.cos(tilt) - z * math.sin(tilt)
        zr = x * math.sin(tilt) + z * math.cos(tilt)
        return Vector((xr, y, zc + zr))

    def board_back(i, j):
        p = board_front(i, j)
        return Vector((p.x, p.y + T, p.z))
    b.slab(NX, NY, board_front, board_back, "TP_SignUral", "TP_Wood", "TP_Wood", lambda i, j: (i / NX, j / NY),
           uv_back=lambda i, j: (j / NY * H / WOOD_TILE, i / NX * W / WOOD_TILE))
    return b.build()


# ------------------------------------------------------------------------------------------------ towels

class Waves:
    """Cloth-like height: a few ripple trains (mostly across the length) plus a fine one."""

    def __init__(self, seed, amp=1.0, scale=1.0):
        r = random.Random(seed)
        self.w = []
        for period, a in ((0.55, 0.012), (0.32, 0.009), (0.17, 0.0045), (0.09, 0.0018)):
            for _ in range(2):
                ang = r.uniform(-0.5, 0.5) + (math.pi / 2 if r.random() < 0.7 else 0.0)
                self.w.append((math.cos(ang) / period * scale, math.sin(ang) / period * scale, r.uniform(0, 6.28), a * amp))

    def __call__(self, x, y):
        return sum(a * math.sin(2 * math.pi * (kx * x + ky * y) + ph) for kx, ky, ph, a in self.w)


def towel(name, variant, seed):
    LEN, WID, T = 1.6, 0.8, 0.010
    NX, NY = 10, 22
    waves = Waves(seed, amp=0.6)
    r = random.Random(seed + 5)
    edge_noise = [[r.uniform(-1, 1) for _ in range(NY + 1)] for _ in range(NX + 1)]
    heap = Waves(seed + 9, amp=2.2, scale=1.6)
    fold_n = Vector((1, 1, 0)).normalized()
    fold_d = (0.4 + 0.8) / math.sqrt(2) - 0.30 / math.sqrt(2)
    fold_r = 0.025

    def top(i, j):
        u, v = i / NX, j / NY
        x, y = (u - 0.5) * WID, (v - 0.5) * LEN
        eb = max(0.0, 1 - 2 * min(u, 1 - u) / 0.25) ** 2 + max(0.0, 1 - 2 * min(v, 1 - v) / 0.1) ** 2    # edges curl up and wander
        x += 0.006 * edge_noise[i][j] * (1.0 if i in (0, NX) else 0.0)
        z = waves(x, y) + 0.006 * min(eb, 1.5)
        if variant == "A":
            z += 0.012 * smoothstep(0.55, 1.0, v) * math.sin(u * 14 + 1.2)          # one short end ruffled up
        elif variant == "B":
            s = fold_n.x * x + fold_n.y * y - fold_d
            if s > 0:
                if s < math.pi * fold_r:
                    q, h = fold_r * math.sin(s / fold_r), fold_r * (1 - math.cos(s / fold_r))
                else:
                    q, h = -(s - math.pi * fold_r), 2 * fold_r + T * 0.5
                x += (q - s) * fold_n.x
                y += (q - s) * fold_n.y
                z = z * 0.5 + h
        elif variant == "C":
            k = smoothstep(0.30, 0.70, v)                     # the upper half is a crumpled heap
            y0 = -0.8 + 0.30 * LEN
            y = y0 + (y - y0) * (1 - 0.42 * k)
            x = x * (1 - 0.30 * k)
            z += k * (0.04 + 0.6 * heap(x * 1.2 + 0.3, y * 1.2)) + 0.5 * k * abs(heap(y * 1.7, x * 1.7))
        return Vector((x, y, z))

    tops = [[top(i, j) for j in range(NY + 1)] for i in range(NX + 1)]

    def normal(i, j):
        i0, i1 = max(i - 1, 0), min(i + 1, NX)
        j0, j1 = max(j - 1, 0), min(j + 1, NY)
        n = (tops[i1][j] - tops[i0][j]).cross(tops[i][j1] - tops[i][j0])
        return n.normalized() if n.length > 1e-9 else Vector((0, 0, 1))

    # lowest point at the origin plane, a centimetre sunk into the sand
    lowest = min((tops[i][j] - normal(i, j) * T).z for i in range(NX + 1) for j in range(NY + 1))
    shift = -lowest - 0.01
    for row in tops:
        for p in row:
            p.z += shift
    b = Builder(name, ["TP_Towel"])
    b.slab(NX, NY, lambda i, j: tops[i][j], lambda i, j: tops[i][j] - normal(i, j) * T,
           "TP_Towel", "TP_Towel", "TP_Towel", lambda i, j: (i / NX, j / NY))
    return b.build()


# ------------------------------------------------------------------------------------------------ stake with a rag

def stake():
    rng = random.Random(31)
    b = Builder("SM_Topi_Stake", ["TP_Wood", "TP_Rag"])
    SIDES = 6
    rings = [(-0.30, 0.004), (-0.16, 0.020), (0.00, 0.026), (0.45, 0.025), (0.90, 0.024), (1.30, 0.022)]
    ring_pts = []
    for zr, rad in rings:
        wob = Vector((rng.uniform(-0.003, 0.003), rng.uniform(-0.003, 0.003), 0))
        pts = []
        for k in range(SIDES):
            a = 2 * math.pi * k / SIDES + 0.3
            rr = rad * rng.uniform(0.93, 1.07)
            z = zr + (0.035 * math.cos(a - 0.8) if zr > 1.0 else 0.0)        # the top is cut aslant
            pts.append(Vector((rr * math.cos(a), rr * math.sin(a), z)) + wob)
        ring_pts.append(pts)
    for r in range(len(rings) - 1):
        for k in range(SIDES):
            k2 = (k + 1) % SIDES
            q = [ring_pts[r][k], ring_pts[r][k2], ring_pts[r + 1][k2], ring_pts[r + 1][k]]
            # counter-clockwise from outside: rings go counter-clockwise seen from above, so this order faces out
            uvs = [(k / SIDES * 0.16 / WOOD_TILE * SIDES, q[0].z / WOOD_TILE), ((k + 1) / SIDES * 0.16 / WOOD_TILE * SIDES, q[1].z / WOOD_TILE),
                   ((k + 1) / SIDES * 0.16 / WOOD_TILE * SIDES, q[2].z / WOOD_TILE), (k / SIDES * 0.16 / WOOD_TILE * SIDES, q[3].z / WOOD_TILE)]
            b.face(q, "TP_Wood", uvs)
    cap = ring_pts[-1]
    b.face(cap, "TP_Wood", [(p.x / WOOD_TILE, p.y / WOOD_TILE) for p in cap])
    tip = ring_pts[0]
    b.face(list(reversed(tip)), "TP_Wood", [(p.x / WOOD_TILE, p.y / WOOD_TILE) for p in reversed(tip)])
    # the rag: a knotted band round the stake and two tails the wind drives toward +x
    zr = 1.10
    SEG = 8
    for k in range(SEG):
        a0, a1 = 2 * math.pi * k / SEG, 2 * math.pi * (k + 1) / SEG
        rr = 0.034
        q = [Vector((rr * math.cos(a0), rr * math.sin(a0), zr)), Vector((rr * math.cos(a1), rr * math.sin(a1), zr)),
             Vector((rr * 1.02 * math.cos(a1), rr * 1.02 * math.sin(a1), zr + 0.04)), Vector((rr * 1.02 * math.cos(a0), rr * 1.02 * math.sin(a0), zr + 0.04))]
        b.face(q, "TP_Rag", [(k / SEG, 0.0), ((k + 1) / SEG, 0.0), ((k + 1) / SEG, 0.2), (k / SEG, 0.2)], True)

    def tail(angle, length, width, wind, phase):
        ca, sa = math.cos(angle), math.sin(angle)
        base = Vector((0.034 * ca, 0.034 * sa, zr + 0.01))
        along = Vector((-sa, ca, 0.0))           # across the strip
        NS = 8
        rows = []
        for s in range(NS + 1):
            t = s / NS
            drop = -length * t
            out = Vector((ca, sa, 0.0)) * (0.012 * t) + Vector((wind, 0, 0)) * (t ** 1.5) * length * 0.6
            flutter = 0.012 * math.sin(t * 9 + phase) * t
            centre = base + Vector((0, 0, drop * (1 - 0.18 * t))) + out + Vector((0, flutter, 0))
            tw = 0.3 * math.sin(t * 6 + phase) * t
            half = along * (width * 0.5 * (1 - 0.15 * t))
            lift = Vector((0, 0, 1)).cross(half) * tw * 0.5
            rows.append((centre - half + lift, centre + half - lift))
        for s in range(NS):
            q = [rows[s][0], rows[s][1], rows[s + 1][1], rows[s + 1][0]]
            b.face(q, "TP_Rag", [(0.0, 0.2 + 0.8 * s / NS), (1.0, 0.2 + 0.8 * s / NS), (1.0, 0.2 + 0.8 * (s + 1) / NS), (0.0, 0.2 + 0.8 * (s + 1) / NS)], True)
    tail(0.2, 0.30, 0.04, 1.0, 0.4)
    tail(-0.6, 0.20, 0.035, 1.0, 2.0)
    return b.build()


# ------------------------------------------------------------------------------------------------ export

def export(obj, path):
    for o in bpy.context.scene.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    kw = dict(filepath=path, use_selection=True, export_format="GLB", export_yup=True, export_apply=True)
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})


def main():
    os.makedirs(OUT, exist_ok=True)
    for name in MAT_SPEC:
        MATERIALS[name] = make_material(name)
    made = [warn_sign(), ural_sign(), towel("SM_Topi_Towel_A", "A", 1), towel("SM_Topi_Towel_B", "B", 2),
            towel("SM_Topi_Towel_C", "C", 3), stake()]
    for obj in made:
        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        export(obj, os.path.join(OUT, obj.name + ".glb"))
        print("TOPI %s: %d triangles, %d materials -> %s" % (obj.name, tris, len(obj.data.materials), os.path.join(OUT, obj.name + ".glb")))


if __name__ == "__main__":
    main()
