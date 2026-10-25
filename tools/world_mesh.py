"""Builds the outdoor map's placeholder geometry (tools/world_map.py) as one .glb: the concrete-slab
path, the club's entrance, the beach promenade and the cable car (the trees are AProceduralForest
actors, the ground is the landscape from tools/terrain.py and unreal/scripts/import_terrain.py;
everything here stands on terrain.ground_z). Pure Python (no bpy, no numpy), so the Unreal build can
run it itself and the .glb never needs committing.

  python3 tools/world_mesh.py out.glb [textures_dir]

glTF is Y-up and right-handed; Interchange turns it into Unreal the same way as Blender's export:
map (X, Y, Z) m -> glTF (X, Z, Y) -> Unreal (X, Y, Z) cm. Positions are absolute map metres plus
the Unreal portal offset, so the imported actors sit at the origin.
"""
import array
import json
import math
import os
import random
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402

# Materials: name -> (rgb, roughness, texture file or None, uv metres per tile)
MATERIALS = {
    "World_Grass": ((0.32, 0.42, 0.2), 0.95, "grass.jpg", 3.0),
    "World_Sand": ((0.52, 0.45, 0.33), 0.9, None, 1.0),
    "World_SlabConcrete": ((0.55, 0.54, 0.5), 0.9, "ceiling.jpg", 2.0),
    "World_Roof_Flat": ((0.25, 0.25, 0.26), 0.9, None, 1.0),
    "World_Steel": ((0.35, 0.36, 0.38), 0.5, None, 1.0),
    "World_Cabin_Red": ((0.7, 0.08, 0.06), 0.5, None, 1.0),
    "World_Cabin_Glass": ((0.05, 0.07, 0.09), 0.08, None, 1.0),
}


class Prim:
    """Triangles of one material, flat or smooth."""

    def __init__(self):
        self.pos, self.nrm, self.uv, self.idx = array.array("f"), array.array("f"), array.array("f"), array.array("I")

    def vert(self, p, n, uv):
        # map -> glTF axes
        self.pos.extend((p[0], p[2], p[1]))
        self.nrm.extend((n[0], n[2], n[1]))
        self.uv.extend(uv)
        return len(self.pos) // 3 - 1

    def tri(self, a, b, c):
        """Indices a, b, c; flipped if needed so the front faces along the vertex normals."""
        P = self.pos
        ax, ay, az = P[3 * a:3 * a + 3]
        bx, by, bz = P[3 * b:3 * b + 3]
        cx, cy, cz = P[3 * c:3 * c + 3]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        N = self.nrm
        hx = N[3 * a] + N[3 * b] + N[3 * c]
        hy = N[3 * a + 1] + N[3 * b + 1] + N[3 * c + 1]
        hz = N[3 * a + 2] + N[3 * b + 2] + N[3 * c + 2]
        if nx * hx + ny * hy + nz * hz < 0:
            b, c = c, b
        self.idx.extend((a, b, c))

    def quad(self, ps, n, tile=1.0):
        """Flat quad, corners in order; UVs projected on the quad's plane."""
        u_ax = [ps[1][i] - ps[0][i] for i in range(3)]
        lu = math.sqrt(sum(v * v for v in u_ax)) or 1.0
        u_ax = [v / lu for v in u_ax]
        v_ax = [n[1] * u_ax[2] - n[2] * u_ax[1], n[2] * u_ax[0] - n[0] * u_ax[2], n[0] * u_ax[1] - n[1] * u_ax[0]]
        ids = []
        for p in ps:
            d = [p[i] - ps[0][i] for i in range(3)]
            ids.append(self.vert(p, n, (sum(d[i] * u_ax[i] for i in range(3)) / tile,
                                        sum(d[i] * v_ax[i] for i in range(3)) / tile)))
        self.tri(ids[0], ids[1], ids[2])
        self.tri(ids[0], ids[2], ids[3])

    def poly(self, ps, n, tile=1.0):
        """Flat convex polygon (fan)."""
        ids = [self.vert(p, n, (p[0] / tile, p[1] / tile + p[2] / tile)) for p in ps]
        for i in range(1, len(ids) - 1):
            self.tri(ids[0], ids[i], ids[i + 1])


class Mesh:
    def __init__(self, name):
        self.name = name
        self.prims = {}

    def p(self, mat):
        return self.prims.setdefault(mat, Prim())


def _norm(v):
    l = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / l for c in v)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def box(prim, c, size, yaw=0.0, tile=1.0, top=True, bottom=False):
    """Box centred at c (x, y, z bottom) of size (w, d, h), turned by yaw around Z."""
    w, d, h = size
    ca, sa = math.cos(yaw), math.sin(yaw)

    def P(u, v, z):
        return (c[0] + u * ca - v * sa, c[1] + u * sa + v * ca, c[2] + z)

    corners = [(-w / 2, -d / 2), (w / 2, -d / 2), (w / 2, d / 2), (-w / 2, d / 2)]
    for i in range(4):
        u0, v0 = corners[i]
        u1, v1 = corners[(i + 1) % 4]
        ps = [P(u0, v0, 0), P(u1, v1, 0), P(u1, v1, h), P(u0, v0, h)]
        e = (ps[1][0] - ps[0][0], ps[1][1] - ps[0][1], 0.0)
        n = _norm((e[1], -e[0], 0.0))
        mid = ((ps[0][0] + ps[1][0]) / 2 - c[0], (ps[0][1] + ps[1][1]) / 2 - c[1])
        if n[0] * mid[0] + n[1] * mid[1] < 0:
            n = (-n[0], -n[1], 0.0)
        prim.quad(ps, n, tile)
    if top:
        prim.quad([P(u, v, h) for u, v in corners], (0, 0, 1), tile)
    if bottom:
        prim.quad([P(u, v, 0) for u, v in corners], (0, 0, -1), tile)


def path(mesh):
    """Soviet concrete slabs, 2 x 3 m, laid along the path a little crooked."""
    rnd = random.Random(3)
    pts = W.path_points(0.5)
    s = 0.0
    seg = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        seg.append((x0, y0, x1, y1, math.hypot(x1 - x0, y1 - y0)))
    total = sum(sg[4] for sg in seg)
    pr = mesh.p("World_SlabConcrete")
    while s + 3.0 <= total:
        c = s + 1.5
        acc = 0.0
        for x0, y0, x1, y1, l in seg:
            if acc + l >= c:
                t = (c - acc) / l
                x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
                yaw = math.atan2(y1 - y0, x1 - x0) - math.pi / 2
                break
            acc += l
        z = min(T.ground_z(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1.5, 0, 1.5))
        box(pr, (x, y, z - 0.12), (2.0, 2.95, 0.2 + rnd.uniform(0, 0.04)), yaw + rnd.uniform(-0.03, 0.03), tile=2.0)
        s += 3.05


def promenade(mesh):
    """Concrete promenade along the beach in front of the town."""
    pr = mesh.p("World_SlabConcrete")
    y = W.PROMENADE_Y[0]
    while y < W.PROMENADE_Y[1]:
        x = W.shore_x(y + 5) - W.BEACH_W + 5.0
        z = T.ground_z(x, y + 5)
        box(pr, (x, y + 5, z - 0.6), (8.0, 10.2, 1.0), math.atan2(W.shore_x(y + 10) - W.shore_x(y), 10.0) * -1,
            tile=2.0)
        y += 10.0


def portal(mesh):
    """Makes the club's entrance read as a bunker door in the hillside (the scene's concrete portal
    wall stays as it is): stepped retaining walls holding the mound on both sides, a concrete
    canopy over the door, a kerb along the apron."""
    conc = mesh.p("World_SlabConcrete")
    n = 13
    for side in (-1, 1):
        for i in range(n):
            d = (i + 0.5) / n * W.FORECOURT_LEN
            x = side * (W.forecourt_half(d) + 0.22)
            top = max(W.mound(x + side * 0.6, d) + 0.25, 0.6)   # a little over the earth it holds
            box(conc, (x, d, -0.4), (0.45, W.FORECOURT_LEN / n / math.cos(math.atan(W.WING_SPLAY)) + 0.02,
                                     top + 0.4), side * -math.atan(W.WING_SPLAY), tile=2.0)
    box(conc, (W.DOOR_X, 0.95, 2.85), (3.6, 1.9, 0.22), 0.0, tile=2.0, bottom=True)
    for side in (-1, 1):
        box(conc, (W.DOOR_X + side * 3.15, 1.55, -0.2), (0.25, 3.1, 0.32), 0.0, tile=2.0)


def rope(prim, p0, p1, r=0.09):
    """4-sided tube from p0 to p1 (map metres); thicker than a real 5 cm rope so the line reads from afar."""
    d = [p1[i] - p0[i] for i in range(3)]
    l = math.sqrt(sum(v * v for v in d))
    f = [v / l for v in d]
    up = (0.0, 0.0, 1.0)
    side = _norm(_cross(f, up))
    up = _cross(side, f)
    for i in range(4):
        a0, a1 = i * math.pi / 2, (i + 1) * math.pi / 2

        def at(p, aa):
            return tuple(p[k] + r * (math.cos(aa) * side[k] + math.sin(aa) * up[k]) for k in range(3))

        am = (a0 + a1) / 2
        n = tuple(math.cos(am) * side[k] + math.sin(am) * up[k] for k in range(3))
        prim.quad([at(p0, a0), at(p1, a0), at(p1, a1), at(p0, a1)], n)


def cable_car(mesh):
    """Two stations, the towers, two ropes (up and down line) sagging between supports, red cabins."""
    cc = W.cable_car()
    sup = cc["supports"]
    (x0, y0), (x1, y1) = cc["upper"], cc["lower"]
    yaw = math.atan2(y1 - y0, x1 - x0)               # along the line
    ca, sa = math.cos(yaw), math.sin(yaw)
    nx, ny = -sa, ca                                 # across it
    conc, steel = mesh.p("World_SlabConcrete"), mesh.p("World_Steel")
    for (sx, sy), back in ((cc["upper"], -1), (cc["lower"], 1)):
        z = T.ground_z(sx, sy)
        # a concrete hall with a flat roof, open toward the line; the bull wheel's housing on top
        box(conc, (sx + back * 3 * ca, sy + back * 3 * sa, z - 1.0), (14, 12, 1.2), yaw, tile=2.0)
        for side in (-1, 1):
            box(conc, (sx + back * 3 * ca + side * 5.6 * nx, sy + back * 3 * sa + side * 5.6 * ny, z), (14, 0.8, 5.6),
                yaw, tile=2.0)
        box(conc, (sx + back * 9.6 * ca, sy + back * 9.6 * sa, z), (0.8, 12, 5.6), yaw, tile=2.0)
        box(mesh.p("World_Roof_Flat"), (sx + back * 3 * ca, sy + back * 3 * sa, z + 5.6), (15, 13, 0.5), yaw)
        box(steel, (sx, sy, z + W.ROPE_Z - 0.4), (5.0, 7.0, 0.4), yaw)
    for tx, ty in cc["towers"]:
        tz = T.ground_z(tx, ty)
        box(steel, (tx, ty, tz - 0.5), (1.2, 1.2, W.TOWER_H + 0.5), yaw)
        box(steel, (tx, ty, tz + W.TOWER_H - 0.3), (1.0, 7.4, 0.6), yaw)
    for side in (-3.0, 3.0):
        pts = []
        for (ax, ay, az), (bx, by, bz) in zip(sup, sup[1:]):
            span = math.hypot(bx - ax, by - ay)
            n = max(2, int(span / 8))
            for k in range(n):
                t = k / n
                sag = 4 * t * (1 - t) * span * 0.012
                pts.append((ax + (bx - ax) * t + side * nx, ay + (by - ay) * t + side * ny, az + (bz - az) * t - sag))
        bx, by, bz = sup[-1]
        pts.append((bx + side * nx, by + side * ny, bz))
        for p0, p1 in zip(pts, pts[1:]):
            rope(steel, p0, p1)
        for f in ((0.2, 0.55, 0.85) if side < 0 else (0.35, 0.7)):
            gondola(mesh, pts[int(len(pts) * f)], yaw)


# A gondola: a rounded cabin (superellipse plan, bellied sides, domed roof) with a band of windows
# and corner posts, hung under the rope on a hanger arm from a grip. Rings of (height, plan scale).
CABIN_W, CABIN_D = 1.9, 1.7
CABIN_RINGS = [(0.0, 0.82), (0.22, 0.96), (0.9, 1.0), (1.75, 1.0), (2.0, 0.93), (2.15, 0.62), (2.24, 0.0)]
CABIN_GLASS = (0.9, 1.75)
CABIN_SIDES = 28
HANGER = 1.3                                        # rope to roof


def _superellipse(t, a, b, p=4.0):
    c, s = math.cos(t), math.sin(t)
    return (a * math.copysign(abs(c) ** (2 / p), c), b * math.copysign(abs(s) ** (2 / p), s))


def gondola(mesh, at, yaw):
    """A cabin hanging from the rope point at (x, y, z), its long side along yaw."""
    x, y, z = at
    red, glass, steel = mesh.p("World_Cabin_Red"), mesh.p("World_Cabin_Glass"), mesh.p("World_Steel")
    ca, sa = math.cos(yaw), math.sin(yaw)
    floor = z - HANGER - CABIN_RINGS[-1][0]

    def world(u, v, h):
        return (x + u * ca - v * sa, y + u * sa + v * ca, floor + h)

    def ring(h, k):
        pts = []
        for i in range(CABIN_SIDES):
            t = 2 * math.pi * i / CABIN_SIDES
            u, v = _superellipse(t, CABIN_W / 2 * k, CABIN_D / 2 * k)
            nu, nv = _superellipse(t, 1.0 / (CABIN_W / 2), 1.0 / (CABIN_D / 2))   # roughly outward
            pts.append((world(u, v, h), _norm((nu * ca - nv * sa, nu * sa + nv * ca, 0.0))))
        return pts

    rings = [ring(h, k) for h, k in CABIN_RINGS]
    for r0, r1, (h0, _), (h1, _) in zip(rings, rings[1:], CABIN_RINGS, CABIN_RINGS[1:]):
        prim = glass if CABIN_GLASS[0] <= h0 and h1 <= CABIN_GLASS[1] else red
        up = 0.0 if h1 <= CABIN_GLASS[1] else 0.7            # the roof leans its normals upward
        for i in range(CABIN_SIDES):
            j = (i + 1) % CABIN_SIDES
            quad = [r0[i], r0[j], r1[j], r1[i]]
            ids = [prim.vert(p, _norm((n[0], n[1], up)), (i / CABIN_SIDES, h0 if q < 2 else h1))
                   for q, (p, n) in enumerate(quad)]
            prim.tri(ids[0], ids[1], ids[2])
            prim.tri(ids[0], ids[2], ids[3])
    red.poly([p for p, _ in reversed(rings[0])], (0, 0, -1))   # the floor
    # corner posts and a sill and lintel round the glass, a little proud of it
    for t in (math.pi / 4, 3 * math.pi / 4, 5 * math.pi / 4, 7 * math.pi / 4, 0.0, math.pi):
        u, v = _superellipse(t, CABIN_W / 2 + 0.015, CABIN_D / 2 + 0.015)
        cx, cy, cz = world(u, v, CABIN_GLASS[0])
        box(red, (cx, cy, cz), (0.09, 0.09, CABIN_GLASS[1] - CABIN_GLASS[0]), yaw + t)
    # the hanger: a bent arm from the grip on the rope down beside the cabin and over its roof
    roof = floor + CABIN_RINGS[-1][0]
    off = CABIN_D / 2 * 0.55
    ox, oy = -sa * off, ca * off
    rope(steel, (x, y, z - 0.15), (x + ox, y + oy, z - 0.55), r=0.08)
    rope(steel, (x + ox, y + oy, z - 0.55), (x + ox, y + oy, roof + 0.05), r=0.08)
    rope(steel, (x + ox, y + oy, roof + 0.05), (x, y, roof - 0.02), r=0.08)
    box(steel, (x, y, z - 0.22), (0.55, 0.28, 0.3), yaw)       # the grip clamped on the rope


# --- glb writer --------------------------------------------------------------------------------

def write_glb(out, meshes, tex_dir):
    out_dir = os.path.dirname(os.path.abspath(out))
    gl = {"asset": {"version": "2.0", "generator": "lyudmilovsk world_mesh.py"}, "scene": 0,
          "scenes": [{"nodes": []}], "nodes": [], "meshes": [], "materials": [], "accessors": [],
          "bufferViews": [], "buffers": [], "images": [], "textures": [], "samplers": [{"wrapS": 10497, "wrapT": 10497}]}
    blob_ = bytearray()
    mat_ids = {}

    def material(name):
        if name in mat_ids:
            return mat_ids[name]
        rgb, rough, tex, _ = MATERIALS[name]
        m = {"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(rgb) + [1.0], "metallicFactor": 0.0,
                                                    "roughnessFactor": rough}}
        if tex and tex_dir and os.path.exists(os.path.join(tex_dir, tex)):
            shutil.copy(os.path.join(tex_dir, tex), os.path.join(out_dir, tex))
            gl["images"].append({"uri": tex})
            gl["textures"].append({"source": len(gl["images"]) - 1, "sampler": 0})
            m["pbrMetallicRoughness"]["baseColorTexture"] = {"index": len(gl["textures"]) - 1}
            m["pbrMetallicRoughness"]["baseColorFactor"] = [1, 1, 1, 1]
        gl["materials"].append(m)
        mat_ids[name] = len(gl["materials"]) - 1
        return mat_ids[name]

    def view(data, target):
        while len(blob_) % 4:
            blob_.append(0)
        off = len(blob_)
        blob_.extend(data)
        gl["bufferViews"].append({"buffer": 0, "byteOffset": off, "byteLength": len(data), "target": target})
        return len(gl["bufferViews"]) - 1

    def accessor(arr, comp, typ, n, target, minmax=False):
        a = {"bufferView": view(arr.tobytes(), target), "componentType": comp, "count": n, "type": typ}
        if minmax:
            k = 3
            a["min"] = [min(arr[i::k]) for i in range(k)]
            a["max"] = [max(arr[i::k]) for i in range(k)]
        gl["accessors"].append(a)
        return len(gl["accessors"]) - 1

    tris = 0
    for mesh in meshes:
        prims = []
        for mname, p in mesh.prims.items():
            if not p.idx:
                continue
            n = len(p.pos) // 3
            prims.append({"attributes": {"POSITION": accessor(p.pos, 5126, "VEC3", n, 34962, True),
                                         "NORMAL": accessor(p.nrm, 5126, "VEC3", n, 34962),
                                         "TEXCOORD_0": accessor(p.uv, 5126, "VEC2", n, 34962)},
                          "indices": accessor(p.idx, 5125, "SCALAR", len(p.idx), 34963),
                          "material": material(mname)})
            tris += len(p.idx) // 3
        gl["meshes"].append({"name": mesh.name, "primitives": prims})
        gl["nodes"].append({"name": mesh.name, "mesh": len(gl["meshes"]) - 1})
        gl["scenes"][0]["nodes"].append(len(gl["nodes"]) - 1)
    for k in ("images", "textures"):
        if not gl[k]:
            del gl[k]
    gl["buffers"].append({"byteLength": len(blob_)})
    js = json.dumps(gl, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    while len(blob_) % 4:
        blob_.append(0)
    with open(out, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob_)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(blob_), 0x004E4942) + bytes(blob_))
    return tris


def build(out, tex_dir=None):
    meshes = []
    for name, fn in (("World_Portal", portal), ("World_Path", path), ("World_Promenade", promenade),
                     ("World_CableCar", cable_car)):
        m = Mesh(name)
        fn(m)
        meshes.append(m)
    return write_glb(out, meshes, tex_dir)


if __name__ == "__main__":
    import time
    t0 = time.time()
    n = build(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
    print("%s: %d triangles, %.1f s" % (sys.argv[1], n, time.time() - t0))
