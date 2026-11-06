"""Street furniture round the doors of a building: add_surrounds(B, ...) is called at the END of build().

For every street entrance (B.entrances of kind "street"/"shop" standing at ground level) it lays out, in the frame of
the door (t = along the wall to the viewer's right, d = out of the wall):
  * a paving apron (CT_Paving, 3 cm proud) in front of the door,
  * a house number plate (blue tile plate with a white label bar) on the wall beside the door,
  * a bench (timber slats on steel legs, a "bench" spot is added), a bike rack of 4 steel loops, a litter bin,
  * a lamp post (concrete or steel pole, steel head, glowing CT_Neon lens) with a real B.lamp(kind "lobby", radius 10),
  * two small planters (concrete box + turf + shrub blob) and a pair of low hedges (CT_Grass boxes).
Each item tries several lateral positions and is skipped if it would leave `bounds`, overlap an earlier item, or
touch a vertical face of the building already in B.m / B.detail (posts, walls, railings, steps): nothing blocks the
door or the building's own details. Solid things people walk around (pole, planters, hedges) go into B.m, the
small stuff (bench, racks, bins, plates) into B.detail.

Parameters: bounds = (x0, x1, y0, y1) the area the furniture may occupy (default: the footprint's x range, y from
0.3 m to the footprint's far edge); kinds = entrance kinds that get a surround; entrances = list of entrance ids to
restrict to; max_z = highest door level that gets a surround (0.2 = street level only); seed = layout variation (which side gets what); gap = lateral distance of the first side items from the
door centre (3.6 m clears a 4 m canopy); max_lamps; hedges / bikes / benches flags.
Material requests: none.
"""
import math
import random

from citykit import box, prism

STEEL, WOOD = "CT_Steel", "CT_Wood"


class _Frame:
    """Door frame: local (t, d) -> world (x, y); yaw snapped to a multiple of 90 deg so boxes stay axis-aligned."""

    def __init__(self, ent):
        self.yaw = (round(ent["yaw"] / 90.0) * 90) % 360
        a = math.radians(self.yaw)
        self.o = (round(math.sin(a)), -round(math.cos(a)))      # outward
        self.r = (round(math.cos(a)), round(math.sin(a)))       # to the viewer's right
        self.p = (ent["pos"][0], ent["pos"][1])
        self.z = ent["pos"][2]

    def xy(self, t, d):
        return (self.p[0] + self.r[0] * t + self.o[0] * d, self.p[1] + self.r[1] * t + self.o[1] * d)

    def rect(self, t0, t1, d0, d1):
        (xa, ya), (xb, yb) = self.xy(t0, d0), self.xy(t1, d1)
        return (min(xa, xb), max(xa, xb), min(ya, yb), max(ya, yb))

    def box(self, mesh, mat, t0, t1, d0, d1, z0, z1, faces="all"):
        x0, x1, y0, y1 = self.rect(t0, t1, d0, d1)
        box(mesh, mat, x0, x1, y0, y1, self.z + z0, self.z + z1, faces=faces)


def _overlap(a, b, m=0.0):
    return a[0] < b[1] + m and b[0] < a[1] + m and a[2] < b[3] + m and b[2] < a[3] + m


def _obstacles(B):
    """xy bounding boxes of the building's vertical faces that reach the pedestrian zone (z < 1.6 and > 0.12)."""
    out = set()
    for mesh in (B.m, B.detail):
        V = mesh.verts
        for _, idx, _ in mesh.faces:
            zs = [V[i][2] for i in idx]
            if max(zs) - min(zs) < 0.05 or min(zs) > 1.6 or max(zs) < 0.12:
                continue
            xs = [V[i][0] for i in idx]
            ys = [V[i][1] for i in idx]
            out.add((round(min(xs), 2), round(max(xs), 2), round(min(ys), 2), round(max(ys), 2)))
    return list(out)


def add_surrounds(B, bounds=None, kinds=("street", "shop"), entrances=None, seed=1, gap=3.6, max_lamps=3,
                  hedges=True, bikes=True, benches=True, max_z=0.2):
    rnd = random.Random(seed)
    if bounds is None:
        fx = [p[0] for p in B.footprint] or [-10, 10]
        fy = [p[1] for p in B.footprint] or [3, 12]
        bounds = (min(fx), max(fx), 0.3, max(fy))
    obst = _obstacles(B)
    occ = []                                   # rectangles taken by furniture
    lamps = [0]
    ents = [e for e in B.entrances if e["kind"] in kinds and e["pos"][2] < max_z and (entrances is None or e["id"] in entrances)]

    def free(r, solid=True):
        if r[0] < bounds[0] or r[1] > bounds[1] or r[2] < bounds[2] or r[3] > bounds[3]:
            return False
        if any(_overlap(r, o, 0.15) for o in occ):
            return False
        return not any(_overlap(r, o, 0.12) for o in obst)

    for ei, e in enumerate(ents):
        fr = _Frame(e)
        sides = [1, -1] if rnd.random() < 0.5 else [-1, 1]
        w = max(e.get("width", 1.1), 1.1)

        def place(w_t, d0, d1, centres, draw):
            for c in centres:
                r = fr.rect(c - w_t / 2, c + w_t / 2, d0, d1)
                if free(r):
                    occ.append(r)
                    draw(c)
                    return c
            return None

        # ---- apron: paving in front of the door, clipped to the bounds
        r = fr.rect(-1.6, 1.6, 0.0, 2.6)
        r = (max(r[0], bounds[0]), min(r[1], bounds[1]), max(r[2], bounds[2]), min(r[3], bounds[3]))
        if r[1] - r[0] > 1.0 and r[3] - r[2] > 0.6:
            box(B.m, "CT_Paving", r[0], r[1], r[2], r[3], fr.z, fr.z + 0.03, faces="zZ" if False else "Z")
            box(B.detail, "CT_ConcreteDark", r[0], r[1], r[2], r[3], fr.z, fr.z + 0.03, faces="xXyY")
        # ---- house number plate on the wall beside the door
        for s in sides:
            t = s * (w / 2 + 0.45)
            px, py = fr.xy(t, 0.01)
            if any(o[0] - 0.05 <= px <= o[1] + 0.05 and o[2] - 0.05 <= py <= o[3] + 0.05 for o in obst) and \
                    not any(abs(px - fr.xy(t, 0.4)[0]) < 0.01 and False for o in obst):
                fr.box(B.detail, "CT_Tile", t - 0.17, t + 0.17, 0.0, 0.03, 1.62, 1.88)
                fr.box(B.detail, "CT_FrameWhite", t - 0.12, t + 0.12, 0.03, 0.04, 1.68, 1.82)
                break
        # ---- bench, bin, bike rack, lamp, planters, hedges on the two sides
        if benches:
            def bench(c):
                for dx in (-0.75, 0.75):
                    fr.box(B.detail, STEEL, c + dx - 0.04, c + dx + 0.04, 0.45, 0.95, 0.0, 0.42)
                fr.box(B.detail, WOOD, c - 0.9, c + 0.9, 0.45, 0.95, 0.42, 0.47)
                fr.box(B.detail, WOOD, c - 0.9, c + 0.9, 0.45, 0.50, 0.47, 0.85)
                x, y = fr.xy(c, 0.75)
                B.spot("sbench_%s_%d" % (e["id"], len(B.spots)), "bench", (x, y, fr.z), fr.yaw)
            place(1.8, 0.4, 1.0, [s * g for g in (gap, gap + 1.6, gap + 3.2) for s in sides], bench)

        def lamp(c):
            x, y = fr.xy(c, 0.9)
            ptype = "CT_Concrete" if (ei + int(c)) % 2 else STEEL
            poly = [(x + 0.08 * math.cos(math.pi / 4 * k + 0.4), y + 0.08 * math.sin(math.pi / 4 * k + 0.4)) for k in range(8)]
            prism(B.m, ptype, poly, fr.z, fr.z + 4.4, top=True, bottom=False)
            box(B.m, "CT_ConcreteDark", x - 0.18, x + 0.18, y - 0.18, y + 0.18, fr.z, fr.z + 0.25)
            # head: an arm reaching towards the street with a lens underneath
            ox, oy = fr.o
            hx, hy = x + ox * 0.45, y + oy * 0.45
            box(B.detail, STEEL, min(x, hx) - 0.05, max(x, hx) + 0.05, min(y, hy) - 0.05, max(y, hy) + 0.05, fr.z + 4.4, fr.z + 4.48)
            box(B.detail, STEEL, hx - 0.28, hx + 0.28, hy - 0.2, hy + 0.2, fr.z + 4.48, fr.z + 4.58)
            box(B.detail, "CT_Neon", hx - 0.22, hx + 0.22, hy - 0.15, hy + 0.15, fr.z + 4.42, fr.z + 4.48)
            B.lamp((hx, hy, fr.z + 4.3), lumens=1100, colour="#FFD9A8", kind="lobby", radius=10, fixture=False)
            lamps[0] += 1
        if lamps[0] < max_lamps:
            place(0.5, 0.6, 1.2, [s * g for g in (gap, gap + 1.0, gap + 2.0, gap + 3.0) for s in reversed(sides)], lamp)

        def binf(c):
            fr.box(B.detail, STEEL, c - 0.2, c + 0.2, 0.3, 0.7, 0.0, 0.85)
            fr.box(B.detail, "CT_ConcreteDark", c - 0.23, c + 0.23, 0.27, 0.73, 0.85, 0.92)
        place(0.5, 0.25, 0.75, [s * g for g in (2.6, 3.0, gap + 0.9, gap + 2.5) for s in sides], binf)

        if bikes:
            def rack(c):
                for i in range(4):
                    t = c - 0.9 + i * 0.6
                    for d in (0.45, 1.05):
                        fr.box(B.detail, STEEL, t - 0.02, t + 0.02, d - 0.02, d + 0.02, 0.0, 0.8)
                    fr.box(B.detail, STEEL, t - 0.02, t + 0.02, 0.43, 1.07, 0.78, 0.82)
                fr.box(B.detail, "CT_ConcreteDark", c - 1.1, c + 1.1, 0.3, 1.2, 0.0, 0.03)
            place(2.4, 0.3, 1.2, [s * g for g in (gap + 2.6, gap + 4.3, gap + 6.0, gap + 8.0) for s in reversed(sides)], rack)

        def planter(c):
            fr.box(B.m, "CT_Concrete", c - 0.4, c + 0.4, 0.35, 1.15, 0.0, 0.55)
            fr.box(B.m, "CT_Grass", c - 0.34, c + 0.34, 0.41, 1.09, 0.55, 0.6, faces="Z")
            fr.box(B.detail, "CT_Grass", c - 0.22, c + 0.22, 0.58, 0.98, 0.6, 1.05)
        for g in (gap - 1.5 if gap > 3 else gap + 1.0, gap + 5.5):
            place(0.8, 0.35, 1.15, [s * g for s in sides] + [s * (g + 1.2) for s in sides], planter)

        if hedges:
            def hedge(c):
                fr.box(B.m, "CT_Grass", c - 1.6, c + 1.6, 0.45, 1.05, 0.0, 0.65)
            for g in (gap + 4.5, gap + 8.5):
                place(3.2, 0.45, 1.05, [s * (g + 1.6) for s in sides], hedge)
