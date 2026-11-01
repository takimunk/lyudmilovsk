"""The street food stalls (docs/food.md, Content/Food/stalls.json): the Adyghe cheese tent by the club's door and the boiled corn
cart in the Academgorodok's square, each one static mesh with its goods on display, for ULyudStallSubsystem.

    Blender -b --factory-startup --python blender/food/build_stalls.py -- [--previews] [--no-export]

Writes blender/food/export/stall_kit.glb (SM_Stall_CheeseTent, SM_Stall_Corn) on the food kit's atlas (food_atlas.py first).
Frame: origin on the ground at the stall's middle, its front (the counter, the customers' side) +x, metres. The seller stands
behind the counter (stalls.json "seller"), the customers in front. Cloth is two layers 5 mm apart (both sides show); every
part is its own lint piece, and nothing coplanar touches (kit.lint).
"""
import math
import os
import random
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import build_food as BF  # noqa: E402
import kit  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PREVIEWS = "--previews" in argv
EXPORT = "--no-export" not in argv
PREV = os.path.join(kit.OUT, "previews")


def cloth(b, cell, quad, uv=((0, 0), (1, 0), (1, 1), (0, 1)), gap=0.005):
    """A sheet of fabric (4 points) as two faces back to back, gap apart along its normal."""
    p = [Vector(q) for q in quad]
    n = (p[1] - p[0]).cross(p[3] - p[0]).normalized()
    b.new_part().face(cell, [tuple(q + n * gap / 2) for q in p], list(uv))
    b.new_part().face(cell, [tuple(q - n * gap / 2) for q in p[::-1]], list(uv[::-1]))


def post(b, cell, x, y, z0, z1, w=0.03):
    # 2 mm short of what it holds up: no face lies on the underside it meets
    b.new_part().box(cell, (x - w / 2, y - w / 2, z0), (x + w / 2, y + w / 2, z1 - 0.002), scale=4.0, skip=("-z",))


def place_mesh(b, src, M):
    """Copies a built food object's faces into builder b moved by matrix M (its uvs as they are)."""
    me = src.data
    uvl = me.uv_layers[0].data
    b.new_part()
    base = len(b.verts)
    for v in me.vertices:
        b.verts.append(M @ v.co)
    for poly in me.polygons:
        b.faces.append(tuple(base + i for i in poly.vertices))
        b.uvs.append([tuple(uvl[li].uv) for li in poly.loop_indices])
        b.parts.append(b.part)
        b.mats.append(None)


def cheap_cob(b, M, rnd):
    """A display cob: the kernels only in the texture, a low lathe."""
    prof = [(0.0, -0.09), (0.017, -0.088), (0.024, -0.07), (0.024, 0.04), (0.02, 0.08), (0.008, 0.095), (0.0, 0.097)]
    b2 = kit.Builder()
    b2.lathe("kernel", prof, axis="y", sides=10, centre=(0, 0, 0))
    for v in b2.verts:
        b.verts.append(M @ v)
    off = len(b.verts) - len(b2.verts)
    b.new_part()
    for f, uv in zip(b2.faces, b2.uvs):
        b.faces.append(tuple(off + i for i in f))
        b.uvs.append(uv)
        b.parts.append(b.part)
        b.mats.append(None)


def cheap_braid(b, M, length=0.2, sides=5):
    for s in range(3):
        ph = 2 * math.pi * s / 3
        path, radii = [], []
        steps = 24
        for i in range(steps + 1):
            y = -length / 2 + length * i / steps
            a = ph + 2 * math.pi * y / 0.075
            path.append(tuple(M @ Vector((0.0058 * math.cos(a), y, 0.0058 * math.sin(a)))))
            radii.append(0.0066 * (0.55 if i in (0, steps) else 1.0))
        b.new_part().tube("smoked", path, radii, sides=sides, cap0="smoked", cap1="smoked")


def basket(b, x, y, z, r, h):
    """A low wicker basket: a flared bowl with a rim, open on top (two walls 6 mm apart)."""
    b.new_part().lathe("wicker", [(r * 0.8, 0.0), (r, h)], axis="z", sides=20, centre=(x, y, z))
    b.new_part().lathe("wicker", [(r - 0.006, h), (r * 0.8 - 0.006, 0.006)], axis="z", sides=20, centre=(x, y, z))
    b.new_part().lathe("wicker", [(0.0, 0.0), (r * 0.8 - 0.002, 0.0)][::-1], axis="z", sides=20, centre=(x, y, z - 0.0005))
    b.new_part().lathe("wicker", [(0.0, 0.006), (r * 0.8 - 0.008, 0.006)], axis="z", sides=20, centre=(x, y, z))
    b.new_part().tube("wicker", [(x + (r + 0.004) * math.cos(a), y + (r + 0.004) * math.sin(a), z + h)
                                 for a in [2 * math.pi * i / 24 for i in range(25)]], [0.008] * 25, sides=6)


# ------------------------------------------------------------------------------------------------ the cheese tent

def cheese_tent(rounds):
    rnd = random.Random(5)
    b = kit.Builder()
    X, Y = 1.25, 1.5           # half depth (x), half width (y)
    EAVE, RIDGE = 2.15, 2.62
    # the frame: four legs, eave rails, the ridge
    for x in (-X, X):
        for y in (-Y, Y):
            post(b, "steel", x, y, 0.0, EAVE)
    for y in (-Y, Y):
        b.new_part().tube("steel", [(-X, y, EAVE - 0.02), (X, y, EAVE - 0.02)], [0.014, 0.014], sides=6)
    for x in (-X, X):
        b.new_part().tube("steel", [(x, -Y, EAVE - 0.05), (x, Y, EAVE - 0.05)], [0.014, 0.014], sides=6)
    # the canopy: a gable roof along y over the eaves, 6 cm over the frame, and its valance all round
    o = 0.06
    cloth(b, "canvas", [(X + o, -Y - o, EAVE + 0.02), (X + o, Y + o, EAVE + 0.02), (0.0, Y + o, RIDGE), (0.0, -Y - o, RIDGE)])
    cloth(b, "canvas", [(-X - o, Y + o, EAVE + 0.02), (-X - o, -Y - o, EAVE + 0.02), (0.0, -Y - o, RIDGE), (0.0, Y + o, RIDGE)])
    for (a, c) in (((X + o, -Y - o), (X + o, Y + o)), ((-X - o, Y + o), (-X - o, -Y - o)), ((X + o, Y + o), (-X - o, Y + o)),
                   ((-X - o, -Y - o), (X + o, -Y - o))):
        # the valance: scalloped along its foot
        segs = 10
        for i in range(segs):
            t0, t1 = i / segs, (i + 1) / segs
            p0 = Vector((a[0] + (c[0] - a[0]) * t0, a[1] + (c[1] - a[1]) * t0, 0))
            p1 = Vector((a[0] + (c[0] - a[0]) * t1, a[1] + (c[1] - a[1]) * t1, 0))
            cloth(b, "canvas", [(p0.x, p0.y, EAVE - 0.2), (p1.x, p1.y, EAVE - 0.2), (p1.x, p1.y, EAVE + 0.02), (p0.x, p0.y, EAVE + 0.02)],
                  uv=((t0, 0), (t1, 0), (t1, 0.2), (t0, 0.2)))
    # the gables (triangles) and the back wall, the sides closed over their back half
    for y in (-Y - o, Y + o):
        s = 1 if y > 0 else -1
        tri = [(-X - o, y, EAVE + 0.02), (X + o, y, EAVE + 0.02), (0.0, y, RIDGE)]
        if s < 0:
            tri = tri[::-1]
        p = [Vector(q) for q in tri]
        n = (p[1] - p[0]).cross(p[2] - p[0]).normalized()
        b.new_part().face("canvas", [tuple(q + n * 0.0025) for q in p], [(0, 0), (1, 0), (0.5, 0.3)])
        b.new_part().face("canvas", [tuple(q - n * 0.0025) for q in p[::-1]], [(0.5, 0.3), (1, 0), (0, 0)])
    cloth(b, "canvas", [(-X - 0.03, Y, 0.05), (-X - 0.03, -Y, 0.05), (-X - 0.03, -Y, EAVE - 0.05), (-X - 0.03, Y, EAVE - 0.05)])
    for y in (-Y - 0.03, Y + 0.03):
        q = [(-X, y, 0.05), (0.0, y, 0.05), (0.0, y, EAVE - 0.05), (-X, y, EAVE - 0.05)]
        cloth(b, "canvas", q if y > 0 else [q[1], q[0], q[3], q[2]], uv=((0, 0), (0.5, 0), (0.5, 1), (0, 1)))
    # the counter: a trestle table under oilcloth hanging down its front and sides
    T0, T1, TW, TZ = 0.25, 0.95, 1.25, 0.84
    b.new_part().box("wood", (T0, -TW, TZ - 0.03), (T1, TW, TZ - 0.005), scale=1.0)
    b.new_part().box("oilcloth", (T0 - 0.005, -TW - 0.005, TZ - 0.005), (T1 + 0.005, TW + 0.005, TZ), scale=1.0, skip=("-z",))
    cloth(b, "oilcloth", [(T1 + 0.012, -TW - 0.01, 0.42), (T1 + 0.012, TW + 0.01, 0.42), (T1 + 0.012, TW + 0.01, TZ - 0.006),
                          (T1 + 0.012, -TW - 0.01, TZ - 0.006)], uv=((0, 0), (1, 0), (1, 0.18), (0, 0.18)))
    for x in (T0 + 0.05, T1 - 0.05):
        for y in (-TW + 0.06, TW - 0.06):
            post(b, "wood", x, y, 0.0, TZ - 0.03, w=0.045)
    # the sign card leaning on the table's front, on the ground
    sx = T1 + 0.09
    sw = 0.32
    b.new_part().face("sign:cheese", [(sx + 0.08, -sw, 0.0), (sx + 0.08, sw, 0.0), (sx, sw, 0.64), (sx, -sw, 0.64)],
                      [(0, 0), (1, 0), (1, 1), (0, 1)])
    b.new_part().face("card", [(sx - 0.004, -sw, 0.64), (sx - 0.004, sw, 0.64), (sx + 0.076, sw, 0.0), (sx + 0.076, -sw, 0.0)],
                      [(0, 0), (1, 0), (1, 0.05), (0, 0.05)])
    # baskets of rounds: small ones on the left, big ones in the middle, braids heaped on the right
    small, big = rounds
    basket(b, 0.62, 0.62, TZ + 0.002, 0.22, 0.09)
    for i, (dx, dy, rz) in enumerate(((-0.07, -0.08, 20), (0.08, -0.04, 75), (-0.02, 0.09, 140), (0.1, 0.11, 200))):
        M = Matrix.Translation((0.62 + dx, 0.62 + dy, TZ + 0.012)) @ Matrix.Rotation(math.radians(rz), 4, "Z") @ \
            Matrix.Rotation(math.radians(-90), 4, "X") @ Matrix.Translation((-0.01, 0.0, -0.057)) @ Matrix.Rotation(math.radians(0), 4, "Z")
        place_mesh(b, small, Matrix.Translation((0, 0, 0.0225 - 0.0)) @ M)
    basket(b, 0.6, 0.0, TZ + 0.002, 0.26, 0.1)
    for i, (dx, dy, rz) in enumerate(((-0.08, -0.06, 10), (0.1, 0.06, 100))):
        M = Matrix.Translation((0.6 + dx, dy, TZ + 0.012)) @ Matrix.Rotation(math.radians(rz), 4, "Z") @ \
            Matrix.Rotation(math.radians(-90), 4, "X") @ Matrix.Translation((-0.01, 0.0, -0.0865))
        place_mesh(b, big, Matrix.Translation((0, 0, 0.0375)) @ M)
    for i in range(6):
        M = Matrix.Translation((0.6 + rnd.uniform(-0.12, 0.12), -0.62 + rnd.uniform(-0.15, 0.15), TZ + 0.008 + 0.012 * (i // 3))) @ \
            Matrix.Rotation(math.radians(rnd.uniform(-60, 60)), 4, "Z")
        cheap_braid(b, M)
    # braids hanging off the front eave rail on strings, a row of eight
    for i in range(8):
        y = -1.05 + 2.1 * i / 7
        z_top = EAVE - 0.08
        b.new_part().tube("napkin", [(X - 0.02, y, EAVE - 0.035), (X - 0.02, y, z_top)], [0.0015, 0.0015], sides=4)
        M = Matrix.Translation((X - 0.02, y, z_top - 0.11)) @ Matrix.Rotation(math.radians(90), 4, "X")
        cheap_braid(b, M, length=0.2)
    return b.build("SM_Stall_CheeseTent", merge=0.0)


# ------------------------------------------------------------------------------------------------ the corn cart

def corn_cart():
    rnd = random.Random(9)
    b = kit.Builder()
    W, D, H = 0.55, 0.3, 0.82          # half width (y), half depth (x), top
    # the cart's body: a green enamelled box on two wheels, a handle out of the back
    b.new_part().box("steel", (-D, -W, 0.26), (D, W, H), scale=2.0)
    for y in (-W - 0.035, W + 0.035):
        s = 1 if y > 0 else -1
        b.new_part().lathe("steel", [(0.0, -0.025), (0.17, -0.025), (0.19, -0.01), (0.19, 0.01), (0.17, 0.025), (0.0, 0.025)],
                           axis="y", sides=20, centre=(0.0, y, 0.19))
    b.new_part().tube("steel", [(-D - 0.01, -W + 0.08, 0.7), (-D - 0.32, -W + 0.08, 0.86)], [0.012, 0.012], sides=6)
    b.new_part().tube("steel", [(-D - 0.01, W - 0.08, 0.7), (-D - 0.32, W - 0.08, 0.86)], [0.012, 0.012], sides=6)
    b.new_part().tube("steel", [(-D - 0.32, -W + 0.08, 0.86), (-D - 0.32, W - 0.08, 0.86)], [0.014, 0.014], sides=6)
    post(b, "steel", D - 0.04, -W + 0.04, 0.0, 0.26, w=0.03)
    # the pot, its lid leaning on it, cobs standing in it
    px, py = 0.0, 0.22
    b.new_part().lathe("alu", [(0.0, 0.0), (0.2, 0.0), (0.21, 0.01), (0.21, 0.3), (0.218, 0.305), (0.212, 0.31), (0.2, 0.31), (0.2, 0.02)],
                       axis="z", sides=28, centre=(px, py, H + 0.002))
    b.new_part().lathe("alu", [(0.0, 0.05), (0.199, 0.05)][::-1], axis="z", sides=28, centre=(px, py, H))     # the water's level
    for side in (-1, 1):
        b.new_part().tube("alu", [(px, py + side * 0.21, H + 0.26), (px, py + side * 0.25, H + 0.27), (px, py + side * 0.25, H + 0.24)],
                          [0.007] * 3, sides=6)
    lid = Matrix.Translation((px - 0.03, py - 0.24, H + 0.21)) @ Matrix.Rotation(math.radians(70), 4, "X")
    lb = kit.Builder()
    lb.lathe("alu", [(0.0, 0.025), (0.06, 0.022), (0.2, 0.006), (0.215, 0.0)], axis="z", sides=24, centre=(0, 0, 0))
    lb.lathe("alu", [(0.205, -0.004), (0.0, -0.004)], axis="z", sides=24, centre=(0, 0, 0))
    b.new_part()
    off = len(b.verts)
    b.verts.extend(lid @ v for v in lb.verts)
    for f, uv in zip(lb.faces, lb.uvs):
        b.faces.append(tuple(off + i for i in f))
        b.uvs.append(uv)
        b.parts.append(b.part)
        b.mats.append(None)
    for i in range(7):
        a = 2 * math.pi * i / 7
        M = Matrix.Translation((px + 0.11 * math.cos(a), py + 0.11 * math.sin(a), H + 0.17)) @ \
            Matrix.Rotation(math.radians(rnd.uniform(-20, 20)), 4, "Z") @ Matrix.Rotation(math.radians(-90 + rnd.uniform(-14, 14)), 4, "X") @ \
            Matrix.Rotation(a, 4, "Y")
        cheap_cob(b, M, rnd)
    # a tray of cobs on the front right, a stack of napkins, the salt
    b.new_part().box("alu", (0.02, -0.5, H + 0.002), (0.26, -0.1, H + 0.014), scale=2.0)
    for i in range(4):
        M = Matrix.Translation((0.14, -0.43 + 0.085 * i, H + 0.036)) @ Matrix.Rotation(math.radians(90 + rnd.uniform(-8, 8)), 4, "Z")
        cheap_cob(b, M, rnd)
    b.new_part().box("napkin", (-0.2, -0.45, H + 0.002), (-0.06, -0.3, H + 0.052), scale=6.0)
    b.new_part().lathe("alu", [(0.0, 0.0), (0.022, 0.0), (0.022, 0.07), (0.016, 0.085), (0.0, 0.088)], axis="z", sides=12,
                       centre=(-0.16, -0.18, H + 0.002))
    # the beach umbrella over it all (two layers), its pole through the cart's back corner
    ux, uy = -D + 0.04, -W + 0.04
    b.new_part().tube("steel", [(ux, uy, 0.3), (ux, uy, 2.18)], [0.012, 0.012], sides=8)
    R, top, rim = 1.0, 2.2, 1.92
    ribs = 8
    for k in range(ribs):
        a0, a1 = 2 * math.pi * k / ribs, 2 * math.pi * (k + 1) / ribs
        q = [(ux, uy, top), (ux + R * math.cos(a0), uy + R * math.sin(a0), rim), (ux + R * math.cos(a1), uy + R * math.sin(a1), rim)]
        p = [Vector(v) for v in q]
        n = (p[1] - p[0]).cross(p[2] - p[0]).normalized()
        if n.z < 0:
            p = [p[0], p[2], p[1]]
            n = -n
        uvq = [(k / ribs + 0.5 / ribs, 1.0), (k / ribs, 0.0), ((k + 1) / ribs, 0.0)]
        b.new_part().face("umbrella", [tuple(v + n * 0.0025) for v in p], uvq)
        b.new_part().face("umbrella", [tuple(v - n * 0.0025) for v in p[::-1]], uvq[::-1])
    # the sign on the cart's front
    b.new_part().face("sign:corn", [(D + 0.006, -0.22, 0.36), (D + 0.006, 0.22, 0.36), (D + 0.006, 0.22, 0.78), (D + 0.006, -0.22, 0.78)],
                      [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)])
    # the seller's folding stool behind
    b.new_part().box("wood", (-D - 0.6, -0.17, 0.44), (-D - 0.3, 0.17, 0.47), scale=2.0)
    for x in (-D - 0.57, -D - 0.33):
        for y in (-0.14, 0.14):
            post(b, "steel", x, y, 0.0, 0.44, w=0.02)
    return b.build("SM_Stall_Corn", merge=0.0)


def main():
    t0 = time.time()
    BF.clear()
    small = BF.round_stage("tmp_small", 0.055, 0.0225, 0, 8, 0.025)
    big = BF.round_stage("tmp_big", 0.0845, 0.0375, 0, 10, 0.03)
    tent = cheese_tent((small, big))
    cart = corn_cart()
    for o in (small, big):
        bpy.data.objects.remove(o, do_unlink=True)
    objs = [tent, cart]
    for ob in objs:
        kit.lint(ob)
    if PREVIEWS:
        os.makedirs(PREV, exist_ok=True)
        kit.preview([tent], os.path.join(PREV, "tent_front.png"), (4.2, -2.4, 1.9), (0, 0, 1.1), lens=35)
        kit.preview([tent], os.path.join(PREV, "tent_counter.png"), (1.9, -0.6, 1.55), (0.6, 0.1, 0.85), lens=35)
        kit.preview([cart], os.path.join(PREV, "corn_front.png"), (2.6, -1.6, 1.6), (0, 0, 0.9), lens=35)
        kit.preview([cart], os.path.join(PREV, "corn_top.png"), (0.9, -0.5, 1.5), (0, 0.1, 0.9), lens=35)
    if EXPORT:
        kit.export(objs, os.path.join(kit.OUT, "stall_kit.glb"))
    print("done", round(time.time() - t0, 1), "s")


if __name__ == "__main__":
    main()
