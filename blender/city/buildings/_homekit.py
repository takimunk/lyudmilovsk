"""Small helpers shared by the residential buildings' interiors (twin_cylinder_house, origami_row, stacked_beam_tower,
terrace_riser_housing, stacked_boxes): a bank of mailboxes, a bicycle, a pram, a short furnishing wrapper and a
"is every piece inside the rooms" check. Everything is built from interior_kit primitives."""
import math

import interior_kit as kit


def mailboxes(B, x, y, z=0.0, yaw=0.0, cols=4, rows=3, mat="CT_Steel", door="CT_SteelRed"):
    """[wall] a bank of letter boxes (0.26 x 0.2 m cells), (x, y) = the middle of its back on the wall, bottom at z+1.0."""
    p = kit._P(B, x, y, z, yaw, "mailboxes")
    p.kind = "wall"
    cw, ch, d = 0.26, 0.2, 0.1
    w, h = cols * cw, rows * ch
    z0 = 1.0
    p.box(mat, -w / 2, w / 2, -d, 0.0, z0, z0 + h, f="xXyZz")
    for c in range(cols):
        for r in range(rows):
            u0 = -w / 2 + c * cw + 0.018
            u1 = u0 + cw - 0.036
            v0 = z0 + r * ch + 0.018
            v1 = v0 + ch - 0.036
            col = door if (c + r) % 3 == 0 else ("CT_PlasterBlue" if (c + r) % 3 == 1 else "CT_PlasterYellow")
            p.box(col, u0, u1, -d - 0.012, -d, v0, v1, f="yxXZz")
            p.box("CT_Paper", u0 + 0.05, u1 - 0.05, -d - 0.016, -d - 0.012, v1 - 0.065, v1 - 0.03, f="y")
            p.box("CT_Steel", u0 + 0.06, u1 - 0.06, -d - 0.016, -d - 0.012, v0 + 0.05, v0 + 0.065, f="y")
    return p.done(size_w=w)


def bike(B, x, y, yaw=0.0, mat="CT_SteelRed"):
    """A leaning city bicycle (local: length along y, front wheel toward -y)."""
    p = kit._P(B, x, y, 0.0, yaw, "bicycle")
    p.kind = "prop"
    R = 0.33
    for yc in (-0.55, 0.55):
        ring_pts = [(0.0, yc + R * math.cos(2 * math.pi * k / 14), R + R * math.sin(2 * math.pi * k / 14)) for k in range(15)]
        p.tube("CT_Rubber", ring_pts, 0.04, 4)
        p.rod(kit.ALU, (0.0, yc, R), (0.0, yc, R + 0.001), 0.04, 4)
    p.rod(mat, (0.0, 0.55, R), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod(mat, (0.0, 0.05, 0.62), (0.0, -0.4, 0.66), 0.03, 4)
    p.rod(mat, (0.0, 0.55, R), (0.0, 0.0, 0.3), 0.03, 4)
    p.rod(mat, (0.0, 0.0, 0.3), (0.0, 0.05, 0.62), 0.03, 4)
    p.rod(mat, (0.0, -0.4, 0.66), (0.0, -0.55, R), 0.03, 4)
    p.rod(kit.STEEL, (0.0, -0.42, 0.62), (0.0, -0.42, 0.98), 0.025, 4)
    p.rod(kit.STEEL, (-0.28, -0.42, 0.99), (0.28, -0.42, 0.99), 0.025, 4)
    p.rod(kit.LAQ, (0.0, 0.1, 0.62), (0.0, 0.1, 0.92), 0.025, 4)
    p.box(kit.LAQ, -0.06, 0.06, 0.02, 0.24, 0.92, 0.96)
    p.rod(kit.STEEL, (0.0, 0.0, 0.3), (0.0, -0.07, 0.3), 0.04, 4)
    return p.done()


def pram(B, x, y, yaw=0.0, mat="CT_PlasterBlue"):
    """A pram: a hooded body on a chassis with four wheels and a handle (local: handle toward +y)."""
    p = kit._P(B, x, y, 0.0, yaw, "pram")
    p.kind = "prop"
    for sx in (-0.26, 0.26):
        for yc in (-0.34, 0.34):
            pts = [(sx, yc + 0.14 * math.cos(2 * math.pi * k / 10), 0.14 + 0.14 * math.sin(2 * math.pi * k / 10)) for k in range(11)]
            p.tube("CT_Rubber", pts, 0.035, 4)
            p.rod(kit.ALU, (sx, yc, 0.14), (sx * 0.8, yc, 0.4), 0.02, 4)
    p.bev(mat, -0.27, 0.27, -0.46, 0.46, 0.4, 0.62, 0.03, 0.04)
    p.cushion("CT_TileWhite", -0.24, 0.24, -0.4, 0.36, 0.6, 0.66, 0.02)
    p.cushion(mat, -0.27, 0.27, -0.46, -0.04, 0.62, 0.98, 0.04)
    p.tube(kit.STEEL, [(-0.25, 0.42, 0.6), (-0.25, 0.62, 0.95), (0.25, 0.62, 0.95), (0.25, 0.42, 0.6)], 0.025, 4)
    return p.done()


class Home:
    """f(fn, x, y, yaw, z, **kw) places a kit piece (returns its dict); the dicts are remembered for check()."""

    def __init__(self, B, inside=None):
        self.B = B
        self.inside = inside          # callable (x, y) -> bool: is this floor point inside the rooms
        self.n0 = len(getattr(B, "kit_items", []))

    def f(self, fn, x, y, yaw=0.0, z=0.0, **kw):
        return fn(self.B, x, y, z, yaw, **kw)

    def seat(self, tid, r, i=0, **props):
        s = r["seats"][i]
        # the seat's height over its own floor (the kit's local "height"), not over the building's base: s[2] is a world z
        self.B.thing(tid, "seat", (s[0], s[1], s[2]), s[3], height=round(r.get("height", s[2]), 2), **props)

    def check(self, label="", overlaps=True):
        """Print kit.check warnings and every piece that has a foot corner outside inside()."""
        out = list(kit.check(self.B)) if overlaps else []
        if self.inside:
            for it in getattr(self.B, "kit_items", [])[self.n0:]:
                if it["kind"] != "floor":
                    continue
                bad = [pt for pt in it["foot"] if not self.inside(*pt)]
                if bad:
                    out.append("outside: %s %s" % (it["name"], [(round(a, 2), round(b, 2)) for a, b in bad[:2]]))
        for o in out:
            print("KIT %s %s" % (label, o))
        return out


def yaw_facing(fx, fy):
    """Kit yaw (0 = faces -y, 90 = +x) of a piece that faces the world direction (fx, fy)."""
    return math.degrees(math.atan2(fx, -fy)) % 360.0


def poly_test(poly):
    """A function (x, y, margin=0) -> True when the point is inside the polygon and at least `margin` from its edges."""
    n = len(poly)

    def test(x, y, margin=0.0):
        c = False
        dmin = 1e9
        for i in range(n):
            (ax, ay), (bx, by) = poly[i], poly[(i + 1) % n]
            if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
                c = not c
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy or 1.0)))
            dmin = min(dmin, math.hypot(x - ax - t * dx, y - ay - t * dy))
        return c and dmin >= margin
    return test


def cot(B, x, y, z=0.0, yaw=0.0, mat="CT_WoodLight", blanket="CT_PlasterYellow"):
    """A baby's cot 0.6 x 1.1 m with bars (head at +y)."""
    p = kit._P(B, x, y, z, yaw, "cot")
    w, l = 0.3, 0.55
    for sx in (-w, w):
        for sy in (-l, l):
            p.rod(mat, (sx, sy, 0.0), (sx, sy, 0.95), 0.035, 4)
    p.box(mat, -w, w, -l, l, 0.3, 0.33, f="xXyYZz")
    p.cushion("CT_Paper", -w + 0.02, w - 0.02, -l + 0.02, l - 0.02, 0.33, 0.42, 0.01)
    p.cushion(blanket, -w + 0.01, w - 0.01, -l + 0.03, 0.15, 0.4, 0.5, 0.01)
    for sy in (-l, l):
        p.rod(mat, (-w, sy, 0.9), (w, sy, 0.9), 0.03, 4)
    for sx in (-w, w):
        p.rod(mat, (sx, -l, 0.9), (sx, l, 0.9), 0.03, 4)
        for k in range(1, 7):
            yy = -l + k * (2 * l) / 7
            p.rod(mat, (sx, yy, 0.33), (sx, yy, 0.9), 0.012, 3)
    return p.done(top=0.42)
