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


# ---------------------------------------------------------------- small things that answer a touch (backlog item 31)

def _facing(yaw):
    """The world direction (x, y) a piece of kit yaw `yaw` faces (0 = -y, 90 = +x)."""
    r = math.radians(yaw)
    return math.sin(r), -math.cos(r)


def switch(B, tid, x, y, yaw, z=0.0, zc=1.3, scope="room"):
    """[wall] a light switch (docs/city_interiors.md "Wall clocks and light switches"): a white plate with a rocker,
    (x, y) on the wall, facing yaw, its middle zc over the floor z; the thing `tid` (what=switch) just in front of it."""
    p = kit._P(B, x, y, z, yaw, "switch")
    p.kind = "wall"
    p.box("CT_SteelWhite", -0.04, 0.04, -0.012, 0.0, zc - 0.06, zc + 0.06, f="xXyzZ")
    p.box("CT_Steel", -0.012, 0.012, -0.02, -0.012, zc - 0.025, zc + 0.03, f="xXyzZ")
    p.done()
    fx, fy = _facing(yaw)
    B.thing(tid, "machine", (x + fx * 0.03, y + fy * 0.03, z + zc), yaw, what="switch", switch=scope)


def mailbox_thing(B, tid, x, y, yaw, z=0.0, rows=3):
    """The thing of a bank of mailboxes() hung at (x, y) facing yaw (what=mailbox: a peek through the slots, the post
    of the house; docs/city_interiors.md "Small things that answer a touch")."""
    fx, fy = _facing(yaw)
    B.thing(tid, "board", (x + fx * 0.12, y + fy * 0.12, z + 1.0 + rows * 0.1), yaw, what="mailbox")


def tap_thing(B, tid, x, y, yaw, z=0.0):
    """The thing of a kitchen sink's tap at (x, y) (the middle of the sink module, its back on the wall), facing yaw."""
    fx, fy = _facing(yaw)
    B.thing(tid, "machine", (x + fx * 0.3, y + fy * 0.3, z + 0.95), yaw, what="tap")


def window_latch(B, tid, near, z=0.0, what="window", reach=5.0, room=None):
    """A window that opens (docs/city_interiors.md "Windows that open"): of B's window cards with their middle 0.9..2.6 m
    over the floor z, the one nearest `near` (x, y) gets a handle on the inner face of its wall and the thing `tid` 0.25 m
    in front of that face, facing into the room. what: "window" (a casement), "vent" (a top-hung vent), "balcony_door".
    room: a test (x, y) -> bool, true inside the room (a Home's inside test): the inner face is where it starts to hold
    along the window's inward normal (without it: 0.3 m in from the card)."""
    best = None
    for w_ in B.windows:
        c, n = w_["centre"], w_["normal"]
        if abs(n[2]) > 0.3 or not (z + 0.9 <= c[2] <= z + 2.6) or w_.get("shape") == "round":
            continue
        depth = 0.3
        if room is not None:
            depth = next((k * 0.05 for k in range(1, 25) if room(c[0] - n[0] * k * 0.05, c[1] - n[1] * k * 0.05)), None)
            if depth is None:
                continue
        d = math.hypot(c[0] - near[0], c[1] - near[1])
        if d <= reach and (best is None or d < best[0]):
            best = (d, w_, depth)
    if best is None:
        raise ValueError("window_latch %s: no window card 0.9..2.6 m over z %.2f within %.1f m of %s" % (tid, z, reach, near))
    _, w_, depth = best
    c, n, (ww, wh) = w_["centre"], w_["normal"], w_["size"]
    yaw = yaw_facing(-n[0], -n[1])
    # the handle: a short lever on the side of the frame, on the wall's inner face
    hx, hy = c[0] - n[0] * depth, c[1] - n[1] * depth
    p = kit._P(B, hx, hy, z, yaw, "window_handle")
    p.kind = "prop"
    side = max(0.05, ww / 2 - 0.07)
    hz = min(c[2], z + 1.45) - z
    p.box("CT_SteelWhite", side - 0.015, side + 0.015, -0.02, 0.0, hz - 0.04, hz + 0.04, f="xXyzZ")
    p.box("CT_SteelWhite", side - 0.012, side + 0.012, -0.05, -0.02, hz - 0.11, hz - 0.08, f="xXyzZ")
    p.done()
    k = depth + 0.25
    B.thing(tid, "board", (c[0] - n[0] * k, c[1] - n[1] * k, min(c[2], z + 1.5)), yaw, what=what)
    return w_["id"]


# ---------------------------------------------------------------- things to open and pick up in the homes (room details)
# docs/city_interiors.md "Things to open and pick up": a wardrobe's doors and a kitchen cupboard open on E (what is inside,
# by season and weather), a kettle goes on, a photo on the bedside table is picked up and turned over (its back is the
# home's own: an id rule per plot), a note left on the kitchen table says something new each day of the week.

def _local(x, y, yaw, lx, ly):
    """A kit piece's local point (lx, ly) (its front toward -ly) in the building's plan."""
    r = math.radians(yaw)
    return x + lx * math.cos(r) - ly * math.sin(r), y + lx * math.sin(r) + ly * math.cos(r)


def wardrobe_thing(B, tid, x, y, yaw, z=0.0, d=0.58):
    """The thing of a kit.wardrobe(x, y, z, yaw) (what=wardrobe): before its doors at handle height."""
    px, py = _local(x, y, yaw, 0.0, -d - 0.12)
    B.thing(tid, "board", (px, py, z + 1.2), yaw, what="wardrobe")


def cupboard_thing(B, tid, x, y, yaw, z=0.0, modules="cupboard,stove,sink,fridge", k=0):
    """The thing of module k (a cupboard) of a kit.kitchen_unit(x, y, z, yaw, modules) (what=cupboard): before its door."""
    mods = modules.split(",")
    assert mods[k] == "cupboard", "cupboard_thing %s: module %d is %s" % (tid, k, mods[k])
    px, py = _local(x, y, yaw, -0.3 * len(mods) + 0.6 * k + 0.3, -0.75)
    B.thing(tid, "board", (px, py, z + 0.6), yaw, what="cupboard")


def kettle(B, tid, x, y, yaw, z=0.0, modules="cupboard,stove,sink,fridge"):
    """An enamel kettle on the hob of a kit.kitchen_unit(x, y, z, yaw, modules) and its thing (what=kettle)."""
    mods = modules.split(",")
    k = mods.index("stove")
    lx = -0.3 * len(mods) + 0.6 * k + 0.17
    kx, ky = _local(x, y, yaw, lx, -0.45)
    p = kit._P(B, kx, ky, z + 0.9, yaw, "kettle")
    p.kind = "prop"
    p.lathe("CT_PlasterRed", 0, 0, [(0.09, 0.0), (0.11, 0.035), (0.105, 0.11), (0.07, 0.145), (0.045, 0.155)], 10)
    p.cyl("CT_PlasterRed", 0, 0, 0.155, 0.16, 0.045, 10)
    p.cyl("CT_Wood", 0, 0, 0.16, 0.185, 0.014, 5)
    p.rod("CT_PlasterRed", (0.09, 0.0, 0.07), (0.18, 0.0, 0.15), 0.022, 5, caps=True)
    p.rod("CT_Wood", (-0.09, 0.0, 0.13), (0.0, 0.0, 0.24), 0.012, 4, caps=True)
    p.rod("CT_Wood", (0.0, 0.0, 0.24), (0.09, 0.0, 0.13), 0.012, 4, caps=True)
    p.done(top=0.18)
    px, py = _local(x, y, yaw, lx, -0.8)
    B.thing(tid, "machine", (px, py, z + 1.05), yaw, what="kettle")


def photo_frame(B, tid, x, y, yaw, z=0.0, top=0.55, side=0.12):
    """A small standing photo frame on a bedside table (kit.bedside_table(x, y, z, yaw): its top `top` over z), beside its
    lamp, and its thing (what=photo): picked up, looked at, turned over."""
    fx, fy = _local(x, y, yaw, side, 0.02)
    p = kit._P(B, fx, fy, z + top, yaw + 12.0, "photo_frame")
    p.kind = "prop"
    p.box("CT_Wood", -0.06, 0.06, -0.012, 0.0, 0.0, 0.16, f="xXyYzZ")
    p.box("CT_Paper", -0.045, 0.045, -0.0135, -0.012, 0.018, 0.142, f="y")
    p.rod("CT_Wood", (0.0, 0.0, 0.1), (0.0, 0.06, 0.0), 0.006, 3)
    p.done()
    px, py = _local(x, y, yaw, side, -0.3)
    B.thing(tid, "take", (px, py, z + top + 0.1), yaw, what="photo")


def note_left(B, tid, x, y, z, yaw=0.0):
    """A folded note left on a table top at (x, y, z) and its thing (what=note): the resident's word of the day."""
    p = kit._P(B, x, y, z, yaw + 20.0, "note_left")
    p.kind = "prop"
    p.face("CT_Paper", [(-0.07, -0.05, 0.002), (0.07, -0.05, 0.002), (0.07, 0.0, 0.02), (-0.07, 0.0, 0.02)])
    p.face("CT_Paper", [(-0.07, 0.0, 0.02), (0.07, 0.0, 0.02), (0.07, 0.05, 0.002), (-0.07, 0.05, 0.002)])
    p.done()
    B.thing(tid, "take", (x, y, z + 0.05), yaw, what="note")
