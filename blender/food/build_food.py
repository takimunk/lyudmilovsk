"""Street food in the hand (docs/food.md): a boiled corn cob in a napkin, a smoked Adyghe cheese braid, a small and a big round of
fresh Adyghe cheese, each as bite stages SM_Food_<id>_<k> (k = 0 whole .. N-1 the last bite) for ALyudFoodItem.

    Blender -b --factory-startup --python blender/food/build_food.py -- [--only <id>] [--previews] [--no-export]

Writes blender/food/export/food_kit.glb (+ previews/*.png). Run food_atlas.py first (the textures).

Every mesh is authored in the right palm's frame, so the game puts it on the hand as it is (Content/Food/food.json keeps palm_loc
and palm_rot 0): Blender x = the fingers' way, z = out of the palm, +y = the thumb's side (Unreal -Y); origin the palm's middle.
Things lie across the palm with their eaten end out of the thumb's side, the side the bites go into facing +z (toward the mouth
when the hand is up at it: food.json mouth_full / mouth_empty, Unreal cm, are where the bites run from and to).
"""
import math
import os
import random
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import kit  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = argv[argv.index("--only") + 1] if "--only" in argv else None
PREVIEWS = "--previews" in argv
EXPORT = "--no-export" not in argv
PREV = os.path.join(kit.OUT, "previews")


# ------------------------------------------------------------------------------------------------ boiled corn

CORN = dict(butt=-0.040, tip=0.150, rows=14, per_row=34, x=0.005, z=0.027)


def corn_radii(y):
    """Core and outer radius along the cob (fat at the butt, rounded off at the tip)."""
    t = (y - CORN["butt"]) / (CORN["tip"] - CORN["butt"])
    core = 0.0135 - 0.004 * t
    outer = 0.0245 - 0.005 * t - 0.012 * max(0.0, (t - 0.86) / 0.14) ** 2
    return core, outer


def corn_stage(k, n):
    """Kernels gone from the tip toward the hand on the side facing +z (the bitten rows), ragged at the edge."""
    rnd = random.Random(40 + k)
    b = kit.Builder()
    X, Z = CORN["x"], CORN["z"]
    y0, y1 = CORN["butt"], CORN["tip"]
    # the bites run from 2 cm in from the tip (mouth_full 13 cm) to 1 cm past the palm (mouth_empty 1 cm)
    cut = 0.15 - (k / (n - 1)) * 0.14 if k > 0 else 1.0
    # the core (pith): a lathe along y, its butt in the napkin, rounded at the tip
    prof = [(0.0, y0 - 0.004)]
    for i in range(16):
        y = y0 + (y1 - y0) * i / 15
        c, _ = corn_radii(y)
        prof.append((c + 0.0015, y))
    prof.append((0.0, y1 + 0.002))
    b.lathe("cob", prof, axis="y", sides=16, centre=(X, 0.0, Z))
    # the kernels: rows round the cob, each a low dome on a tapered base
    for r in range(CORN["rows"]):
        a = 2 * math.pi * r / CORN["rows"]
        for j in range(CORN["per_row"]):
            yc = y0 + 0.012 + (y1 - y0 - 0.02) * (j + 0.5 * (r % 2) * 0.5) / CORN["per_row"]
            if yc > y1 - 0.004:
                continue
            core, outer = corn_radii(yc)
            facing = math.cos(a - math.pi / 2)            # 1 toward +z (the mouth)
            edge = cut + rnd.uniform(-0.004, 0.004)
            h = 1.0
            if facing > 0.25 and yc > edge:
                continue                                  # bitten off
            if facing > 0.0 and yc > edge - 0.006 and k > 0:
                h = rnd.uniform(0.25, 0.55)               # stubs at the edge of the bite
            _kernel(b.new_part(), X, Z, a, yc, core, core + (outer - core) * h, (y1 - y0 - 0.02) / CORN["per_row"],
                    2 * math.pi / CORN["rows"])
    # the stalk's stub and the napkin round the butt (3+ mm off the kernels)
    b.new_part()
    b.lathe("cob", [(0.0, y0 - 0.016), (0.009, y0 - 0.016), (0.0095, y0 - 0.004), (0.0, y0 - 0.004)], axis="y", sides=10,
            centre=(X, 0.0, Z))
    _napkin(b.new_part(), X, Z, rnd)
    return b


def _kernel(b, X, Z, a, yc, r0, r1, dy, da):
    """One kernel: base quad on the core, a 3 x 3 domed top at r1."""
    w0, w1 = 0.82, 0.92
    ca, sa = math.cos(a), math.sin(a)
    t = Vector((-sa, 0.0, ca))                            # round the cob
    rad = Vector((ca, 0.0, sa))
    c0 = Vector((X, yc, Z))

    def p(r, u, v, lift=0.0):
        return c0 + rad * (r + lift) + t * (u * r * da / 2) + Vector((0, v * dy / 2, 0))
    base = [p(r0, -w0, -w0), p(r0, w0, -w0), p(r0, w0, w0), p(r0, -w0, w0)]
    dome = 0.25 * (r1 - r0) + 0.0008
    top = [[p(r1, (i - 1) * w1, (j - 1) * w1, dome * (1.0 if (i, j) == (1, 1) else 0.45 if 1 in (i, j) else 0.0))
            for i in range(3)] for j in range(3)]
    for j in range(2):
        for i in range(2):
            pts = [top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]]
            uv = [(i / 2, j / 2), ((i + 1) / 2, j / 2), ((i + 1) / 2, (j + 1) / 2), (i / 2, (j + 1) / 2)]
            b.face("kernel", pts, uv)
    ring = [top[0][0], top[0][1], top[0][2], top[1][2], top[2][2], top[2][1], top[2][0], top[1][0]]
    bring = [base[0], None, base[1], None, base[2], None, base[3], None]
    for i in range(8):
        i1 = (i + 1) % 8
        b0 = bring[i] if bring[i] is not None else (bring[i - 1] + bring[i1]) / 2
        b1 = bring[i1] if bring[i1] is not None else (bring[i] + bring[(i1 + 1) % 8]) / 2
        b.face("kernel", [b0, b1, ring[i1], ring[i]], [(0, 0), (1, 0), (1, 1), (0, 1)])
    _fix_winding_last(b, 12, c0 + rad * (r0 + r1) * 0.5)


def _fix_winding_last(b, count, inside):
    """Makes the last count faces face away from a point inside them."""
    for fi in range(len(b.faces) - count, len(b.faces)):
        f = b.faces[fi]
        pts = [b.verts[i] for i in f]
        nrm = (pts[1] - pts[0]).cross(pts[2] - pts[0])
        cen = sum(pts, Vector()) / len(pts)
        if nrm.dot(cen - inside) < 0:
            b.faces[fi] = tuple(reversed(f))
            b.uvs[fi] = list(reversed(b.uvs[fi]))


def _napkin(b, X, Z, rnd):
    """A paper napkin wrapped round the butt: an open cone, crumpled, two layers 0.6 mm apart (both sides show)."""
    y0 = CORN["butt"]
    sides, rings = 18, 6
    for layer, off in ((0, 0.0), (1, -0.0006)):
        pts = []
        for i in range(rings + 1):
            t = i / rings
            y = y0 - 0.006 + t * 0.042
            row = []
            for k in range(sides + 1):
                a = -0.7 + (2 * math.pi - 0.3) * k / sides            # a slit along the top where it's folded over
                r = 0.0295 + 0.004 * t + 0.0018 * math.sin(k * 2.3 + i * 1.7) + (0.004 * t * math.sin(k * 1.3)) + off
                yy = y + (0.006 * t * math.sin(k * 0.9 + 1.0))         # the ragged top edge
                row.append(Vector((X + r * math.cos(a), yy, Z + r * math.sin(a))))
            pts.append(row)
        for i in range(rings):
            for k in range(sides):
                q = [pts[i][k], pts[i][k + 1], pts[i + 1][k + 1], pts[i + 1][k]]
                uv = [(k / sides, i / rings), ((k + 1) / sides, i / rings), ((k + 1) / sides, (i + 1) / rings), (k / sides, (i + 1) / rings)]
                if layer == 1:
                    q, uv = q[::-1], uv[::-1]
                b.face("napkin", q, uv)
        # outward / inward check on the outer layer's first face
        if layer == 0:
            _fix_winding_last(b, rings * sides, Vector((X, y0 + 0.015, Z)))
        else:
            for fi in range(len(b.faces) - rings * sides, len(b.faces)):
                f = b.faces[fi]
                pp = [b.verts[i] for i in f]
                nrm = (pp[1] - pp[0]).cross(pp[2] - pp[0])
                cen = sum(pp, Vector()) / len(pp)
                if nrm.dot(cen - Vector((X, cen.y, Z))) > 0:
                    b.faces[fi] = tuple(reversed(f))
                    b.uvs[fi] = list(reversed(b.uvs[fi]))


def build_corn():
    n = 8
    return [corn_stage(k, n).build("SM_Food_corn_cob_%d" % k) for k in range(n)]


# ------------------------------------------------------------------------------------------------ the smoked braid

BRAID = dict(butt=-0.045, tip=0.170, z=0.019, helix=0.0058, pitch=0.075)


def braid_stage(k, n):
    """Three strands twisted round each other, tied in a knot at the butt; bitten shorter from the tip."""
    rnd = random.Random(70 + k)
    b = kit.Builder()
    y0, y1, Z = BRAID["butt"], BRAID["tip"], BRAID["z"]
    end = y1 if k == 0 else y1 - (k / (n - 1)) * (y1 - 0.03) + 0.004
    steps = max(8, int((end - y0) / 0.003))
    for s in range(3):
        ph = 2 * math.pi * s / 3
        path, radii = [], []
        for i in range(steps + 1):
            y = y0 + (end - y0) * i / steps
            a = ph + 2 * math.pi * (y - y0) / BRAID["pitch"]
            taper = 1.0 if k > 0 else max(0.35, min(1.0, (y1 - y) / 0.03))
            path.append((BRAID["helix"] * math.cos(a), y, Z + BRAID["helix"] * math.sin(a)))
            radii.append(0.0068 * taper * (1.0 - 0.15 * (y - y0) / (y1 - y0)))
        b.new_part()
        rings = b.tube("smoked", path, radii, sides=8, cap0="fibre", cap1="fibre" if k > 0 else "smoked", uv_len=1.0)
        if k > 0:
            # a ragged bite: the cap's rim pulled back unevenly
            pass
    # the knot at the butt: a squashed ball of the same cheese
    b.new_part()
    prof = [(0.0, -0.011)] + [(0.0105 * math.sin(math.pi * i / 8), -0.011 * math.cos(math.pi * i / 8)) for i in range(1, 8)] + [(0.0, 0.011)]
    b.lathe("smoked", prof, axis="y", sides=12, centre=(0.0, y0 - 0.006, Z), cap_cell=None)
    return b


def build_braid():
    n = 8
    return [braid_stage(k, n).build("SM_Food_cheese_braid_%d" % k) for k in range(n)]


# ------------------------------------------------------------------------------------------------ the rounds of fresh cheese

def round_stage(name, R, T, k, n, top_from):
    """A round with soft edges lying in the palm on its rim (axis along y), bitten down from its top (+z) in crescent bites."""
    b = kit.Builder()
    X, Z = 0.010, R + 0.002
    e = min(0.010, T * 0.45)
    prof = []
    for i in range(7):
        a = math.pi / 2 * i / 6
        prof.append((R - e + e * math.sin(a), -T + e - e * math.cos(a)))
    for i in range(7):
        a = math.pi / 2 * i / 6
        prof.append((R - e + e * math.cos(a), T - e + e * math.sin(a)))
    b.lathe("rind", prof, axis="y", sides=40, centre=(X, 0.0, Z), cap_cell="rind")
    ob = b.build(name)
    if k == 0:
        return ob
    rnd = random.Random(hash(name) & 0xffff)
    top, bottom = 2 * R + 0.002, top_from
    cutters = []
    m = 2 * k
    for i in range(1, m + 1):
        zc = top + 0.006 - (i / (2 * (n - 1))) * (top - bottom)
        xc = X + (0.008 if i % 2 else -0.008) + rnd.uniform(-0.003, 0.003)
        cutters.append(kit.ellipsoid("cut", (xc, rnd.uniform(-0.002, 0.002), zc), (0.022, T + 0.012, 0.014), "curd", segs=28, rings=16))
    return kit.tidy(kit.subtract(ob, cutters))


def build_rounds():
    out = []
    for name, R, T, n, empty in (("cheese_round_small", 0.055, 0.0225, 8, 0.025), ("cheese_round_big", 0.0845, 0.0375, 10, 0.03)):
        out += [round_stage("SM_Food_%s_%d" % (name, k), R, T, k, n, empty) for k in range(n)]
    return out


# ------------------------------------------------------------------------------------------------ main

def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def main():
    t0 = time.time()
    clear()
    builds = {"corn_cob": build_corn, "cheese_braid": build_braid, "cheese_rounds": build_rounds}
    objs = []
    for key, fn in builds.items():
        if ONLY and ONLY not in key:
            continue
        got = fn()
        objs += got
        print("built", key, len(got), "stages", round(time.time() - t0, 1), "s")
    for ob in objs:
        kit.lint(ob)
    if PREVIEWS:
        os.makedirs(PREV, exist_ok=True)
        for ob in objs:
            if not ob.name.endswith(("_0", "_3", "_7")):
                continue
            c = Vector(ob.bound_box[0]).lerp(Vector(ob.bound_box[6]), 0.5)
            size = (Vector(ob.bound_box[6]) - Vector(ob.bound_box[0])).length
            kit.preview([ob], os.path.join(PREV, ob.name + ".png"), c + Vector((-0.6, 0.35, 0.9)) * size * 1.6, c, lens=60, res=(700, 520))
    if EXPORT:
        kit.export(objs, os.path.join(kit.OUT, "food_kit.glb" if not ONLY else "food_kit_%s.glb" % ONLY))
    print("done", round(time.time() - t0, 1), "s")


if __name__ == "__main__":
    main()
