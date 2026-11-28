"""Motor ship «Полдень»: the furniture, things and people-spots of the LOWER DECK D and the MAIN DECK C.

    furnish_cd(B, S)        B = the citykit Building of motor_ship.py, S = _shipframe (ship frame <-> building frame)

Everything here is written in SHIP coordinates (x from the stern to the bow, y starboard positive, z from the waterline,
ship yaw 0 = facing the bow, 90 = starboard, 180 = the stern, 270 = port) and goes through S.fr / S.p / S.thing / S.spot.
The shell (hull, decks, bulkheads, door holes and leaves, stairs, deck fittings) is _shipshell.py's; this file only puts
furniture into B.detail (interior_kit pieces and a few ship pieces of its own, built with kit._P), hidden collision cores
into B.m (through the kit's solid=True), and declares the things and the spots of DESIGN's contract.

Rooms (R1 5.3, 5.4): vestibule/reception, dining saloon, servery, galley (seen through the half-door), starboard
corridor, engine-room look-in, cabin corridor, cabins 1, 2, 7 furnished (3-6, 8-10 closed: doors, plates, berth spots),
foredeck and aft-deck things; lower deck: stair-foot lobby, cabin 15 furnished (16-20 closed), hold door, showers,
baggage room, crew quarters (spots only).

Ids of the contract: see DESIGN.md ("Main deck C (W2)", "Lower deck D (W2)", "Spots for people") and W2_report.md.
Dining chairs are numbered by table: table t (1..9) has dining_chair_(4t-3)..(4t), the captain's table has 37..40.
"""
import math
import os

import interior_kit as kit
from interior_kit import (ALU, BRASS, LAQ, STEEL, WOOD, WOODL, _P, _ax, _knob, _legs, _pad, _ring_rect, _rrect, _soft,
                          _torus)

FWD, STBD, AFT, PORT = 0.0, 90.0, 180.0, 270.0
WHITE, LINEN, GLASS, OPAL = "CT_TileWhite", "CT_Paper", "CT_Glass", "CT_Opal"
RED, GREEN, BLUE, YELLOW, PINK = "CT_PlasterRed", "CT_PlasterGreen", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterPink"
ENGINE = "CT_Baize"                       # the diesel's paint
CABIN2_STYLE = "granny"              # W10 berths бабушка Фаина + Сеня in cabin 2 ("honeymoon" is the other set of props)
# No B.lamp here: the shell (motor_ship.lamps) owns the 24 real lights; these fixtures only glow (CT_Opal).


# ================================================================================================ frame helper
class _F:
    """Ship-frame placing of kit pieces, own pieces, things and spots."""

    def __init__(self, B, S):
        self.B, self.S = B, S
        self.tris0 = B.detail.tri_count()
        self.count = {}

    def put(self, fn, x, y, z, yaw=0.0, **kw):
        """A kit piece fn at ship (x, y, z) facing ship yaw."""
        bx, by, bz, ky = self.S.fr(x, y, z, yaw)
        return fn(self.B, bx, by, bz, ky, **kw)

    def pc(self, name, x, y, z, yaw=0.0, kind="floor"):
        """A piece under construction (kit._P): local front = -y = the ship yaw's facing, local +x = to the right of
        someone looking at its front."""
        bx, by, bz, ky = self.S.fr(x, y, z, yaw)
        p = _P(self.B, bx, by, bz, ky, name)
        p.kind = kind
        return p

    @staticmethod
    def loc(x, y, yaw, lx, ly):
        """Ship (x, y) of the local point (lx, ly) of a piece placed at (x, y) facing ship yaw."""
        a = math.radians(yaw)
        fx, fy = math.cos(a), math.sin(a)                     # facing, in ship axes
        return (x + lx * fy - ly * fx, y - lx * fx - ly * fy)

    def thing(self, tid, kind, x, y, z, yaw=0.0, **props):
        self.S.thing(self.B, tid, kind, (x, y, z), yaw, **props)

    def spot(self, sid, kind, x, y, z, yaw=0.0):
        self.S.spot(self.B, sid, kind, (x, y, z), yaw)

    def seat(self, tid, r, i=0, spot=True, **props):
        """The seat thing (and the seat spot of the same id) of a kit seat's dict."""
        s = r["seats"][i]
        self.B.thing(tid, "seat", (s[0], s[1], s[2]), s[3], height=round(r.get("height", 0.45), 2), **props)
        if spot:
            self.B.spot(tid, "seat", (s[0], s[1], round(s[2] - r.get("height", 0.45), 3)), s[3])

    def windows(self, x0, x1, y0, y1, z0, z1, side_only=False):
        """The shell's window cards whose middle is inside the ship-frame box: [(x, y, z, inward ship yaw, w, h, shape)]."""
        out = []
        for w_ in self.B.windows:
            c, n = w_["centre"], w_["normal"]
            x, y, z = self.S.unp(*c)
            if not (x0 <= x <= x1 and y0 <= y <= y1 and z0 <= z <= z1) or abs(n[2]) > 0.5:
                continue
            if side_only and (abs(n[1]) < 0.7 or w_.get("shape") == "round"):
                continue
            # inward normal in ship axes: building (nx, ny) -> ship (nx, -ny), inward = minus
            yaw = math.degrees(math.atan2(n[1], -n[0])) % 360.0
            out.append((x, y, z, yaw, w_["size"][0], w_["size"][1], w_.get("shape", "rect")))
        out.sort()
        return out


def _dirv(yaw):
    a = math.radians(yaw)
    return math.cos(a), math.sin(a)


# ================================================================================================ small ship pieces
_SEG = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc", "7": "abc",
        "8": "abcdefg", "9": "abcdfg"}


def _digits(p, mat, text, cx, zc, y, h):
    """Engraved-looking numerals (seven strokes a figure) on a front face at plane y, middle (cx, zc), height h."""
    w, t = 0.5 * h, 0.15 * h
    pitch = w + 0.3 * h
    x0 = cx - (len(text) * pitch - 0.3 * h) / 2
    zt, zb = zc + h / 2, zc - h / 2
    for i, ch in enumerate(text):
        a = x0 + i * pitch
        b = a + w
        for s in _SEG[ch]:
            r = {"a": (a, b, zt - t, zt), "g": (a, b, zc - t / 2, zc + t / 2), "d": (a, b, zb, zb + t),
                 "f": (a, a + t, zc + t / 2, zt - t), "b": (b - t, b, zc + t / 2, zt - t),
                 "e": (a, a + t, zb + t, zc - t / 2), "c": (b - t, b, zb + t, zc - t / 2)}[s]
            p.box(mat, r[0], r[1], y, y + 0.002, r[2], r[3], f="y")


def number_plate(F, x, y, z, yaw, text, zc=1.62, h=0.085):
    """[wall] a brass door plate with a cabin number."""
    p = F.pc("number_plate", x, y, z, yaw, "wall")
    w = 0.09 + 0.07 * len(text)
    p.bev(BRASS, -w / 2, w / 2, -0.008, 0.0, zc - h / 2, zc + h / 2, 0.003)
    _digits(p, LAQ, text, 0.0, zc, -0.0115, h * 0.56)
    return p.done()


def plafond(F, x, y, deck, blue=False):
    """[ceiling] an opal-glass ceiling lamp with a chrome rim (the low deckheads take no pendants): it only glows.
    Skipped when one of the shell's real lamps hangs within 0.9 m."""
    for l_ in F.B.lamps:
        lx, ly, lz = F.S.unp(*l_["pos"])
        if abs(lz - F.S.ceil(deck)) < 0.6 and math.hypot(lx - x, ly - y) < 0.9:
            return None
    zc = F.S.ceil(deck)
    p = F.pc("plafond", x, y, zc, 0.0, "ceiling")
    p.lathe(ALU, 0, 0, [(0.185, -0.03), (0.2, 0.0)], 8, top=False, bottom=True, a0=0.0)
    p.lathe(BLUE if blue else OPAL, 0, 0, [(0.0, -0.105), (0.11, -0.085), (0.17, -0.03)], 8, top=False, a0=0.0)
    return p.done(pin=(-0.2, 0.2, -0.2, 0.2, -0.105, 0.0))


def lamp_mounts(F):
    """A chrome ceiling plate and four thin rods over each of the shell's opal lamp boxes on decks D and C (the boxes
    hang 0.17 m under the deckhead: this is what holds them)."""
    S = F.S
    for l_ in F.B.lamps:
        x, y, z = S.unp(*l_["pos"])
        deck = "D" if z < S.Z_C else ("C" if z < S.Z_B else None)
        if deck is None:
            continue
        gap = S.ceil(deck) - (z + 0.08)
        if gap < 0.02 or gap > 0.6:
            continue
        p = F.pc("lamp_mount", x, y, S.ceil(deck), 0.0, "ceiling")
        p.box(ALU, -0.2, 0.2, -0.2, 0.2, -0.015, 0.0, f="xXyYz")
        for sx in (-0.16, 0.16):
            for sy in (-0.16, 0.16):
                p.rod(ALU, (sx, sy, -0.015), (sx, sy, -gap), 0.012, 3)
        p.box(ALU, -0.19, 0.19, -0.19, 0.19, -gap - 0.004, -gap + 0.012, f="xXyY")
        p.done()


def bulkhead_light(F, x, y, z, yaw, zc=2.0):
    """[wall] a caged engine-room / crew lamp: an opal jar in a steel guard."""
    p = F.pc("bulkhead_light", x, y, z, yaw, "wall")
    p.box(STEEL, -0.07, 0.07, -0.03, 0.0, zc - 0.07, zc + 0.07, f="xXyZz")
    _ax(p, OPAL, (0.0, 0.0, zc), "y", -0.2, -0.03, 0.055, r1=0.06, segs=6)
    for a in (0.0, 1.57):
        dx, dz = 0.075 * math.cos(a), 0.075 * math.sin(a)
        p.rod(STEEL, (dx, -0.03, zc + dz), (dx, -0.22, zc + dz), 0.008, 3)
        p.rod(STEEL, (-dx, -0.03, zc - dz), (-dx, -0.22, zc - dz), 0.008, 3)
    return p.done()


def handrail(F, x0, y0, x1, y1, z, yaw, zr=0.92):
    """[wall] a brass handrail along a wall face from ship (x0, y0) to (x1, y1); yaw = the way the wall faces."""
    L = math.hypot(x1 - x0, y1 - y0)
    p = F.pc("handrail", (x0 + x1) / 2, (y0 + y1) / 2, z, yaw, "prop")
    p.rod(BRASS, (-L / 2, -0.075, zr), (L / 2, -0.075, zr), 0.036, 6, caps=True)
    n = max(2, int(L / 1.3) + 1)
    for k in range(n):
        u = -L / 2 + 0.12 + k * (L - 0.24) / (n - 1)
        p.rod(BRASS, (u, 0.0, zr - 0.07), (u, -0.075, zr - 0.012), 0.016, 4)
    return p.done()


def fire_box(F, x, y, z, yaw, zc=1.25):
    """[wall] a red fire-hose box with a glazed door, the coiled hose seen inside."""
    p = F.pc("fire_box", x, y, z, yaw, "wall")
    w, h, d = 0.56, 0.62, 0.17
    p.box("CT_SteelRed", -w / 2, w / 2, -d, 0.0, zc - h / 2, zc + h / 2, f="xXyZz")
    p.box(LINEN, -w / 2 + 0.06, w / 2 - 0.06, -d - 0.004, -d, zc - h / 2 + 0.06, zc + h / 2 - 0.06, f="y")
    _torus(p, "CT_Leather", (0.0, -d - 0.02, zc), 0.15, 0.035, axis="y", segs=8, prof=4)
    _ax(p, BRASS, (0.0, 0.0, zc), "y", -d - 0.06, -d - 0.004, 0.04, segs=6)
    p.rod(BRASS, (w / 2 - 0.035, -d - 0.02, zc - 0.07), (w / 2 - 0.035, -d - 0.02, zc + 0.07), 0.014, 4)
    return p.done()


def loudspeaker(F, x, y, z, yaw, zc=2.05):
    """[wall] the ship's broadcast box: plywood, cloth front, one volume knob."""
    p = F.pc("loudspeaker", x, y, z, yaw, "wall")
    p.bev(WOODL, -0.14, 0.14, -0.1, 0.0, zc - 0.11, zc + 0.11, 0.012)
    p.box(YELLOW, -0.1, 0.1, -0.104, -0.1, zc - 0.05, zc + 0.08, f="y")
    _knob(p, LAQ, 0.0, zc - 0.08, 0.014, -0.1, -0.115, 6)
    return p.done()


def coat_hooks(F, x, y, z, yaw, n=3, zc=1.7, coat=None, hat=False):
    """[wall] a board of brass hooks; coat = a material: one garment hangs on the first hook; hat: a straw hat on the last."""
    p = F.pc("coat_hooks", x, y, z, yaw, "wall")
    w = 0.16 * n + 0.08
    p.bev(WOOD, -w / 2, w / 2, -0.018, 0.0, zc - 0.04, zc + 0.04, 0.006)
    for k in range(n):
        u = -w / 2 + 0.12 + k * 0.16
        p.rod(BRASS, (u, -0.018, zc), (u, -0.07, zc + 0.03), 0.012, 4, caps=True)
    if coat:
        u = -w / 2 + 0.12
        p.loft(coat, [_rrect(u - 0.17, u + 0.17, -0.14, -0.022, zc - 0.95, 0.03, 1),
                      _rrect(u - 0.2, u + 0.2, -0.15, -0.022, zc - 0.25, 0.04, 1),
                      _rrect(u - 0.06, u + 0.06, -0.1, -0.03, zc + 0.0, 0.02, 1)], top=True, bottom=True)
    if hat:
        u = -w / 2 + 0.12 + (n - 1) * 0.16
        _ax(p, YELLOW, (u, 0.0, zc - 0.12), "y", -0.05, -0.035, 0.17, segs=10)
        _ax(p, YELLOW, (u, 0.0, zc - 0.12), "y", -0.12, -0.05, 0.085, r1=0.09, segs=8)
        _ax(p, RED, (u, 0.0, zc - 0.12), "y", -0.065, -0.05, 0.094, segs=8, caps=False)
    return p.done()


def suitcase(F, x, y, z, yaw, mat="CT_Leather", w=0.62, d=0.2, h=0.42, flat=False):
    """A fibre suitcase standing on its long edge (flat=True: lying), two straps, a handle."""
    p = F.pc("suitcase", x, y, z, yaw, "prop")
    if flat:
        d, h = h, d
    p.bev(mat, -w / 2, w / 2, -d / 2, d / 2, 0.0, h, 0.02, 0.03, bottom=True)
    for sx in (-w * 0.27, w * 0.27):
        p.box(LAQ, sx - 0.017, sx + 0.017, -d / 2 - 0.004, d / 2 + 0.004, 0.0, h + 0.004, f="xXyYZ")
    if flat:
        p.tube(LAQ, [(-0.07, -d / 2 - 0.004, h * 0.5), (-0.07, -d / 2 - 0.04, h * 0.5), (0.07, -d / 2 - 0.04, h * 0.5),
                     (0.07, -d / 2 - 0.004, h * 0.5)], 0.016, 3)
    else:
        p.tube(LAQ, [(-0.07, 0.0, h), (-0.07, 0.0, h + 0.045), (0.07, 0.0, h + 0.045), (0.07, 0.0, h)], 0.016, 3)
    return p.done(top=h)


def rucksack(F, x, y, z, yaw, mat=GREEN):
    p = F.pc("rucksack", x, y, z, yaw, "prop")
    _soft(p, mat, -0.19, 0.19, -0.13, 0.13, 0.0, 0.5, 0.03, 0.06, 1, 0.9)
    _soft(p, mat, -0.13, 0.13, -0.2, -0.12, 0.06, 0.3, 0.015, 0.03, 1)
    for sx in (-0.1, 0.1):
        p.box("CT_Leather", sx - 0.015, sx + 0.015, -0.135, -0.13, 0.3, 0.47, f="y")
    return p.done(top=0.5)


def washbasin(F, x, y, z, yaw, towel=LINEN):
    """[wall] a white enamel cabin washbasin: bowl, two brass taps, splash-back, glass shelf with a tumbler, the trap
    under it, a towel on a rail. Returns the dict with "tap" = the point a thing goes."""
    p = F.pc("washbasin", x, y, z, yaw, "wall")
    p.loft(WHITE, [_rrect(-0.17, 0.17, -0.30, -0.01, 0.70, 0.05, 1), _rrect(-0.23, 0.23, -0.37, 0.0, 0.85, 0.05, 1),
                   _rrect(-0.2, 0.2, -0.34, -0.03, 0.85, 0.04, 1), _rrect(-0.13, 0.13, -0.27, -0.09, 0.76, 0.04, 1)],
           top=True)
    p.box(WHITE, -0.25, 0.25, -0.012, 0.0, 0.85, 1.12, f="xXyZ")
    for sx in (-0.1, 0.1):
        p.cyl(BRASS, sx, -0.045, 0.85, 0.91, 0.013, 5)
        p.rod(BRASS, (sx, -0.045, 0.905), (sx, -0.12, 0.89), 0.016, 4, caps=True)
        p.rod(BRASS, (sx - 0.03, -0.045, 0.925), (sx + 0.03, -0.045, 0.925), 0.012, 4, caps=True)
    p.rod(ALU, (0.0, -0.17, 0.72), (0.0, -0.17, 0.48), 0.036, 5)
    p.rod(ALU, (0.0, -0.17, 0.48), (0.0, 0.0, 0.48), 0.036, 5)
    p.box(GLASS, -0.22, 0.22, -0.11, 0.0, 1.2, 1.208, f="xXyZz")
    p.cyl(WHITE, -0.13, -0.055, 1.208, 1.3, 0.03, 6, r1=0.035)
    p.box(PINK, 0.06, 0.14, -0.08, -0.03, 1.208, 1.228, f="xXyYZ")
    p.rod(BRASS, (0.27, -0.09, 0.78), (0.42, -0.09, 0.78), 0.012, 4, caps=True)
    p.box(towel, 0.285, 0.405, -0.1, -0.08, 0.5, 0.79, f="xXyYZz")
    p.core(-0.15, 0.15, -0.28, -0.03, 0.71, 0.75)
    return p.done(solid=True, top=0.85, footprint=(-0.23, 0.23, -0.37, 0.0), tap=(0.0, -0.3, 0.95))


def folding_table(F, x, y, z, yaw, w=0.7, d=0.44, mat=WOODL):
    """[wall] the folding table under a cabin window: a flap on two brass struts, a low fiddle on the open edge."""
    p = F.pc("folding_table", x, y, z, yaw, "wall")
    _pad(p, mat, -w / 2, w / 2, -d, 0.0, 0.7, 0.725, 0.05, 0.006, 1, bottom=True)
    p.box(WOOD, -w / 2 + 0.05, w / 2 - 0.05, -d, -d + 0.012, 0.725, 0.74, f="xXyYZ")
    for sx in (-w / 2 + 0.1, w / 2 - 0.1):
        p.rod(BRASS, (sx, -d + 0.1, 0.7), (sx, -0.01, 0.34), 0.016, 4)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d + 0.02, -0.02, 0.705, 0.72)
    return p.done(solid=True, top=0.725, footprint=(-w / 2, w / 2, -d, 0.0))


def berth(F, x, y, z, yaw, L=1.9, W=0.72, upper=False, made=True, head=1, blanket=BLUE, wood=WOOD, lamp=True,
          fold=None):
    """[wall] a ship's berth along a wall: (x, y) = the wall face at the middle of its length, it stands out W toward
    the yaw's facing; head = +1: the pillow at the local +x end (to the right of someone looking at it), -1: left.
    Lower: a boxed base with two drawers; upper: a Pullman shelf on two brass stays. made: mattress, blanket with the
    sheet turned back, pillow; made=False: the bare striped mattress rolled up at the head, a folded blanket on the
    slats (nobody travels on it). fold = a material: a folded shawl / rug across the foot.
    Returns the dict plus "bed" = ship (x, y, z) of the middle of the mattress top, "bed_yaw" = ship yaw from the head
    toward the foot, "height" = the mattress top over the floor."""
    p = F.pc("berth", x, y, z, yaw, "wall")
    hl = L / 2
    if upper:
        zm = 1.44
        p.box(wood, -hl, hl, -W, 0.0, zm - 0.05, zm, f="xXyZz")
        for sx in (-hl + 0.12, hl - 0.12):
            p.rod(BRASS, (sx, -W + 0.03, zm + 0.17), (sx, -0.015, zm + 0.72), 0.012, 3)
    else:
        zm = 0.34
        p.box(wood, -hl, hl, -W + 0.03, 0.0, 0.0, zm, f="xXyZ")
        for s in (-1, 1):
            c = s * hl / 2
            p.box(wood, c - 0.4, c + 0.4, -W + 0.016, -W + 0.03, 0.07, 0.28, f="xXyZz")
            p.rod(BRASS, (c - 0.07, -W - 0.004, 0.18), (c + 0.07, -W - 0.004, 0.18), 0.014, 4, caps=True)
    # the lee board: low along the whole berth, raised at both ends; head and foot boards
    p.box(wood, -hl, hl, -W, -W + 0.022, zm - 0.02, zm + 0.09, f="xXyYZz")
    for s in (-1, 1):
        a, b = sorted((s * hl, s * (hl - 0.42)))
        p.box(wood, a, b, -W, -W + 0.022, zm + 0.09, zm + 0.17, f="xXyYZ")
        a, b = sorted((s * hl, s * (hl - 0.022)))
        p.box(wood, a, b, -W + 0.022, 0.0, zm, zm + 0.24, f="xXyYZ")
    h = head
    top = zm + 0.13
    if made:
        _soft(p, LINEN, -hl + 0.03, hl - 0.03, -W + 0.03, -0.01, zm, zm + 0.11, 0.01, 0.04, 1)
        a, b = sorted((-h * (hl - 0.035), h * (hl * 0.3)))
        _soft(p, blanket, a, b, -W + 0.025, -0.006, zm + 0.02, zm + 0.135, 0.008, 0.04, 1)
        a, b = sorted((h * (hl * 0.3 - 0.02), h * (hl * 0.3 + 0.14)))
        _pad(p, WHITE, a, b, -W + 0.027, -0.008, zm + 0.12, zm + 0.146, 0.02, 0.006, 1)
        a, b = sorted((h * (hl - 0.07), h * (hl - 0.5)))
        _soft(p, WHITE, a, b, -W + 0.1, -0.08, zm + 0.11, zm + 0.21, 0.02, 0.05, 1, 0.8)
        if fold:
            a, b = sorted((-h * (hl - 0.1), -h * (hl - 0.48)))
            _pad(p, fold, a, b, -W + 0.06, -0.05, zm + 0.135, zm + 0.19, 0.03, 0.012, 1)
    else:
        for k in range(7):                                        # the slats
            u = -hl + 0.12 + k * (L - 0.24) / 6
            p.box(WOODL, u - 0.05, u + 0.05, -W + 0.03, -0.01, zm, zm + 0.012, f="xXZ")
        c = h * (hl - 0.25)
        _ax(p, "CT_PlasterGrey", (c, 0.0, zm + 0.19), "y", -W + 0.06, -0.04, 0.17, segs=8)
        for v in (-W + 0.2, -W + 0.36, -0.2):
            _ax(p, BLUE, (c, 0.0, zm + 0.19), "y", v - 0.012, v + 0.012, 0.174, segs=8, caps=False)
        a, b = sorted((-h * (hl - 0.12), -h * (hl - 0.6)))
        _pad(p, blanket, a, b, -W + 0.1, -0.1, zm + 0.012, zm + 0.11, 0.03, 0.012, 1)
        _pad(p, WHITE, a + 0.04, b - 0.04, -W + 0.14, -0.14, zm + 0.11, zm + 0.15, 0.03, 0.01, 1)
    if lamp:                                                      # the reading lamp over the pillow
        c = h * (hl - 0.3)
        _ax(p, BRASS, (c, 0.0, zm + 0.62), "y", -0.11, 0.0, 0.05, r1=0.022, segs=6)
        _ax(p, OPAL, (c, 0.0, zm + 0.62), "y", -0.116, -0.11, 0.04, segs=6)
    if not upper:
        p.core(-hl + 0.03, hl - 0.03, -W + 0.06, -0.03, 0.05, zm - 0.02)
    r = p.done(solid=not upper, top=top, footprint=(-hl, hl, -W, 0.0))
    bx, by = F.loc(x, y, yaw, 0.0, -W / 2)
    fx, fy = F.loc(0.0, 0.0, yaw, -h, 0.0)                        # from the head toward the foot
    r["bed"] = (bx, by, z + top)
    r["bed_yaw"] = math.degrees(math.atan2(fy, fx)) % 360.0
    r["height"] = round(top, 2)
    return r


def berth_ladder(F, x, y, z, yaw, h=1.42):
    """A short ladder hooked to an upper berth's lee board (leans a little)."""
    p = F.pc("berth_ladder", x, y, z, yaw, "prop")
    for sx in (-0.17, 0.17):
        p.rod(WOODL, (sx, -0.2, 0.0), (sx, 0.0, h + 0.1), 0.03, 4)
    for k in range(4):
        t = (k + 0.7) / 4.4
        p.rod(WOODL, (-0.17, -0.2 * (1 - t), (h + 0.1) * t), (0.17, -0.2 * (1 - t), (h + 0.1) * t), 0.026, 4)
    return p.done()


def lifejackets(F, x, y, z, yaw, n=2):
    """[prop] cork lifejackets stacked (on top of a wardrobe, on a shelf): orange blocks with white tapes."""
    p = F.pc("lifejackets", x, y, z, yaw, "prop")
    for k in range(n):
        z0 = k * 0.1
        _pad(p, "CT_SteelRed", -0.3, 0.3, -0.2, 0.2, z0, z0 + 0.09, 0.03, 0.012, 1)
        p.box(WHITE, -0.02, 0.02, -0.203, 0.203, z0 + 0.01, z0 + 0.094, f="yYZ")
    return p.done(top=n * 0.1)


def carpet_runner(F, x0, x1, y, z, w=0.9):
    """A strip carpet along a corridor (ship x0..x1 at y): red with green borders."""
    p = F.pc("carpet_runner", (x0 + x1) / 2, y, z, STBD, "prop")
    hl = abs(x1 - x0) / 2
    p.box(RED, -hl, hl, -w / 2 + 0.09, w / 2 - 0.09, 0.0, 0.008, f="Z")
    for s in (-1, 1):
        a, b = sorted((s * w / 2, s * (w / 2 - 0.09)))
        p.box(GREEN, -hl, hl, a, b, 0.0, 0.008, f="Z" + ("y" if s < 0 else "Y"))
    p.box(RED, -hl, hl, -w / 2 + 0.09, w / 2 - 0.09, 0.0, 0.008, f="xX")
    return p.done()


# ================================================================================================ dining saloon pieces
def saloon_table(F, x, y, z, yaw, w=1.2, d=0.8, captain=False, seed=0):
    """A fixed saloon table for four: white cloth to the knees, four covers (two a long side), a water carafe with
    glasses and a little vase in the middle; captain: a table runner, a brass stand with the company pennant, a bigger
    bouquet. Local x = the long side. Returns "chairs": [(lx, ly, facing 0 = the table's yaw | 180)]."""
    p = F.pc("saloon_table", x, y, z, yaw)
    h = 0.75
    hw, hd = w / 2, d / 2
    p.loft(WHITE, [_ring_rect(-hw - 0.07, hw + 0.07, -hd - 0.07, hd + 0.07, 0.3, 0.02),
                   _ring_rect(-hw - 0.025, hw + 0.025, -hd - 0.025, hd + 0.025, h - 0.02, 0.02),
                   _ring_rect(-hw - 0.01, hw + 0.01, -hd - 0.01, hd + 0.01, h, 0.02)], top=True)
    for lx in (-hw + 0.08, hw - 0.08):
        for ly in (-hd + 0.08, hd - 0.08):
            p.box(WOOD, lx - 0.025, lx + 0.025, ly - 0.025, ly + 0.025, 0.0, 0.3, f="xXyY")
    p.core(-hw + 0.02, hw - 0.02, -hd + 0.02, hd - 0.02, h - 0.05, h - 0.03)
    if captain:
        p.box(RED, -hw - 0.012, hw + 0.012, -0.11, 0.11, h, h + 0.004, f="Z")
    for sx in (-w * 0.25, w * 0.25):
        for sy in (-1, 1):
            v = sy * (hd - 0.16)
            p.cyl(WHITE, sx, v, h, h + 0.014, 0.1, 8, r1=0.115)
            p.cyl(GLASS, sx + 0.15, v - sy * 0.09, h, h + 0.1, 0.026, 5, r1=0.033)
            for k in (-1, 1):
                p.box(ALU, sx + k * 0.14 - 0.009, sx + k * 0.14 + 0.009, v - 0.08, v + 0.08, h, h + 0.006, f="Z")
    cx = -0.1 if captain else 0.0
    p.lathe(BLUE, cx, 0.0, [(0.05, h), (0.062, h + 0.08), (0.024, h + 0.18), (0.03, h + 0.23)], 6)
    vx = 0.12 if not captain else 0.22
    p.lathe(BLUE if not captain else WHITE, vx, 0.0, [(0.03, h), (0.042, h + 0.05), (0.018, h + 0.11), (0.026, h + 0.14)], 6,
            top=False)
    rnd = kit._rng(seed, x, y, "saloon")
    for k in range(5 if captain else 2):
        a = k * 2.3 + rnd.uniform(0, 1)
        r = 0.035 if captain else 0.02
        p.lathe(rnd.choice([RED, YELLOW, PINK, WHITE]), vx + r * math.cos(a), r * math.sin(a),
                [(0.0, h + 0.16), (0.028, h + 0.19), (0.0, h + 0.22)], 5)
    if captain:                                                   # the pennant on a brass stand
        p.cyl(BRASS, -0.42, 0.0, h, h + 0.012, 0.045, 8)
        p.rod(BRASS, (-0.42, 0.0, h), (-0.42, 0.0, h + 0.3), 0.01, 4)
        tri = [(-0.42, 0.0, h + 0.3), (-0.42, 0.0, h + 0.2), (-0.26, 0.0, h + 0.25)]
        p.face(RED, tri)
        p.face(RED, tri[::-1])
        p.face(YELLOW, [(-0.42, -0.001, h + 0.27), (-0.42, -0.001, h + 0.23), (-0.35, -0.001, h + 0.25)])
        p.face(YELLOW, [(-0.42, 0.001, h + 0.27), (-0.42, 0.001, h + 0.23), (-0.35, 0.001, h + 0.25)][::-1])
    else:
        p.box(LINEN, -0.2, -0.12, -0.03, 0.03, h, h + 0.07, f="xXyYZ")       # paper napkins in a holder
    chairs = [(sx, sy * (hd + 0.3), 0.0 if sy > 0 else 180.0) for sx in (-w * 0.25, w * 0.25) for sy in (-1, 1)]
    r = p.done(solid=True, top=h, footprint=(-hw - 0.07, hw + 0.07, -hd - 0.07, hd + 0.07))
    r["chairs"] = chairs
    return r


def sideboard(F, x, y, z, yaw, w=2.8, d=0.5):
    """[wall] the saloon's sideboard under the serving hatch: doors, a brass fiddle, stacks of plates, carafes, cups."""
    p = F.pc("sideboard", x, y, z, yaw, "wall")
    h = 0.9
    p.box(LAQ, -w / 2 + 0.04, w / 2 - 0.04, -d + 0.05, 0.0, 0.0, 0.08, f="xXy")
    p.box(WOOD, -w / 2, w / 2, -d, 0.0, 0.08, h - 0.03, f="xXyz")
    p.bev(WOODL, -w / 2 - 0.03, w / 2 + 0.03, -d - 0.03, 0.0, h - 0.03, h, 0.01)
    n = 4
    for k in range(n):
        a = -w / 2 + 0.03 + k * (w - 0.06) / n
        b = a + (w - 0.06) / n - 0.02
        p.box(WOOD, a + 0.01, b, -d - 0.012, -d, 0.12, h - 0.07, f="xXyZz")
        p.box(WOODL, a + 0.08, b - 0.07, -d - 0.017, -d - 0.012, 0.2, h - 0.15, f="xXyZz")
        kx = b - 0.04 if k % 2 == 0 else a + 0.05
        p.cyl(BRASS, kx, -d - 0.03, h * 0.5, h * 0.5 + 0.02, 0.014, 5, bottom=True)
    # the fiddle: a low brass rail round the top
    zr = h + 0.045
    pts = [(-w / 2, -0.02, zr), (-w / 2, -d, zr), (w / 2, -d, zr), (w / 2, -0.02, zr)]
    p.tube(BRASS, pts, 0.014, 4)
    for u in (-w / 2, -w / 4, 0.0, w / 4, w / 2):
        p.rod(BRASS, (u, -d, h), (u, -d, zr), 0.012, 4)
    for u, nn in ((-1.1, 7), (-0.85, 4), (-0.62, 6)):             # plate stacks
        p.cyl(WHITE, u, -0.26, h, h + 0.009 * nn, 0.095, 10, r1=0.1)
        p.cyl(WHITE, u, -0.26, h + 0.009 * nn, h + 0.009 * nn + 0.012, 0.07, 10, r1=0.105)
    for u in (0.75, 0.93, 1.11):                                  # carafes
        p.lathe(BLUE, u, -0.3, [(0.05, h), (0.06, h + 0.07), (0.022, h + 0.17), (0.03, h + 0.22)], 6)
    for k in range(6):                                            # tea glasses in holders, in two rows
        u, v = 0.2 + (k % 3) * 0.1, -0.2 - (k // 3) * 0.1
        p.cyl(ALU, u, v, h, h + 0.06, 0.03, 6)
        p.cyl(GLASS, u, v, h + 0.06, h + 0.1, 0.028, 6, r1=0.032)
    p.box(WOODL, -0.4, -0.05, -0.36, -0.14, h, h + 0.05, f="xXyYZ")         # a bread tray under a napkin
    _soft(p, LINEN, -0.39, -0.06, -0.35, -0.15, h + 0.05, h + 0.1, 0.01, 0.03, 1)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -d + 0.02, -0.02, 0.1, h - 0.05)
    return p.done(solid=True, top=h, footprint=(-w / 2, w / 2, -d, 0.0))


def crockery(F, x, y, z, yaw):
    """[prop] two stacks of plates and a pile of bowls on a work top."""
    p = F.pc("crockery", x, y, z, yaw, "prop")
    for u, nn, r in ((-0.22, 8, 0.1), (0.0, 5, 0.1), (0.22, 4, 0.075)):
        p.cyl(WHITE, u, 0.0, 0.0, 0.01 * nn, r * 0.95, 8, r1=r)
        p.cyl(WHITE, u, 0.0, 0.01 * nn, 0.01 * nn + 0.012, r * 0.7, 8, r1=r * 1.05)
    return p.done(top=0.1)


def titan(F, x, y, z, yaw):
    """The water boiler («титан») of the servery: a tall nickel cylinder on a stand, a gauge glass, a brass tap."""
    p = F.pc("titan", x, y, z, yaw)
    p.box(STEEL, -0.22, 0.22, -0.22, 0.22, 0.0, 0.5, f="xXyYZ")
    p.lathe(ALU, 0, 0, [(0.2, 0.5), (0.2, 1.35), (0.13, 1.47), (0.05, 1.5)], 10)
    p.cyl(STEEL, 0, 0, 1.5, 1.95, 0.04, 6)
    p.rod(BRASS, (0.0, -0.2, 0.68), (0.0, -0.3, 0.66), 0.03, 5, caps=True)
    p.rod(GLASS, (0.12, -0.17, 0.75), (0.12, -0.17, 1.25), 0.016, 4)
    p.core(-0.18, 0.18, -0.18, 0.18, 0.05, 1.3)
    return p.done(solid=True, top=1.5)


def pot_rack(F, x, y, z, yaw, w=1.6, zc=1.85):
    """[wall] the galley's rail of ladles and pans over the range."""
    p = F.pc("pot_rack", x, y, z, yaw, "wall")
    p.rod(ALU, (-w / 2, -0.06, zc), (w / 2, -0.06, zc), 0.018, 4, caps=True)
    for u in (-w / 2 + 0.05, w / 2 - 0.05):
        p.rod(ALU, (u, 0.0, zc), (u, -0.06, zc), 0.014, 4)
    n = int(w / 0.27)
    for k in range(n):
        u = -w / 2 + 0.2 + k * (w - 0.4) / max(1, n - 1)
        if k % 2:
            p.rod(ALU, (u, -0.07, zc), (u, -0.07, zc - 0.3), 0.012, 4)
            _ax(p, ALU, (u, 0.0, zc - 0.34), "y", -0.11, -0.05, 0.055, r1=0.03, segs=6)
        else:
            r = 0.1 + 0.03 * (k % 3)
            p.rod(LAQ, (u, -0.07, zc), (u, -0.07, zc - 0.16), 0.02, 4)
            _ax(p, LAQ if k % 4 else BRASS, (u, 0.0, zc - 0.16 - r), "y", -0.1, -0.055, r, segs=8)
    return p.done()


def half_door_shelf(F, x, y, z, yaw, w=1.1):
    """The serving ledge on the galley half-door's lower leaf, with a bell and a dish of pies (what the cook hands
    out): (x, y) = the middle of the doorway on its public face, the ledge stands out toward the yaw's facing."""
    p = F.pc("half_door_shelf", x, y, z, yaw, "wall")
    _pad(p, WOODL, -w / 2 + 0.03, w / 2 - 0.03, -0.22, 0.0, 1.0, 1.05, 0.03, 0.008, 1, bottom=True)
    for sx in (-w / 2 + 0.12, w / 2 - 0.12):
        p.rod(BRASS, (sx, -0.18, 1.0), (sx, 0.0, 0.82), 0.014, 4)
    p.lathe(BRASS, -0.3, -0.07, [(0.045, 1.05), (0.04, 1.075), (0.012, 1.09)], 8)
    p.cyl(WHITE, 0.2, -0.06, 1.05, 1.062, 0.11, 8, r1=0.12)
    for k in range(4):
        a = k * 1.57
        _soft(p, YELLOW, 0.2 + 0.05 * math.cos(a) - 0.04, 0.2 + 0.05 * math.cos(a) + 0.04, -0.06 + 0.05 * math.sin(a) - 0.03,
              -0.06 + 0.05 * math.sin(a) + 0.03, 1.062, 1.1, 0.008, 0.02, 1)
    return p.done(top=1.05)


# ================================================================================================ vestibule pieces
def key_board(F, x, y, z, yaw, w=1.0, h=0.66, zc=1.6, out=(1, 2, 7, 12, 15)):
    """[wall] the purser's key board: 20 brass hooks in four rows, a key with a wooden tag on each (the keys of the
    cabins in `out` are with their passengers), a brass strip on top."""
    p = F.pc("key_board", x, y, z, yaw, "wall")
    p.bev(WOOD, -w / 2, w / 2, -0.025, 0.0, zc - h / 2, zc + h / 2, 0.008)
    p.box(BRASS, -w / 2 + 0.05, w / 2 - 0.05, -0.029, -0.025, zc + h / 2 - 0.075, zc + h / 2 - 0.035, f="y")
    for n in range(20):
        r, c = divmod(n, 5)
        u = -w / 2 + 0.12 + c * (w - 0.24) / 4
        zz = zc + h / 2 - 0.16 - r * 0.125
        p.rod(BRASS, (u, -0.025, zz), (u, -0.05, zz + 0.012), 0.009, 3, caps=True)
        if (n + 1) in out:
            continue
        p.box(ALU, u - 0.006, u + 0.006, -0.045, -0.04, zz - 0.035, zz, f="xXy")
        p.box(WOODL if n % 3 else RED, u - 0.022, u + 0.022, -0.046, -0.038, zz - 0.085, zz - 0.035, f="xXyz")
    return p.done()


def post_box(F, x, y, z, yaw, zc=1.3):
    """[wall] the ship's post box: blue, a slot under a little roof, a white collection card."""
    p = F.pc("post_box", x, y, z, yaw, "wall")
    p.bev(BLUE, -0.17, 0.17, -0.16, 0.0, zc - 0.25, zc + 0.22, 0.02, 0.02, bottom=True)
    p.box(BLUE, -0.19, 0.19, -0.2, 0.0, zc + 0.22, zc + 0.245, f="xXyZz")
    p.box(LAQ, -0.11, 0.11, -0.164, -0.16, zc + 0.11, zc + 0.135, f="y")
    p.box(LINEN, -0.09, 0.09, -0.164, -0.16, zc - 0.14, zc + 0.02, f="y")
    p.box(BRASS, -0.03, 0.03, -0.165, -0.16, zc - 0.21, zc - 0.17, f="y")
    return p.done()


def barometer(F, x, y, z, yaw, zc=1.7, r=0.11):
    """[wall] a brass aneroid barometer."""
    p = F.pc("barometer", x, y, z, yaw, "wall")
    _ax(p, BRASS, (0.0, 0.0, zc), "y", -0.05, 0.0, r, segs=12)
    ang = [2 * math.pi * k / 12 for k in range(12)]
    p.face(LINEN, [(r * 0.86 * math.cos(a), -0.052, zc + r * 0.86 * math.sin(a)) for a in ang])
    p.box(LAQ, -0.004, 0.004, -0.055, -0.053, zc - 0.01, zc + r * 0.7, f="y")
    p.box("CT_SteelRed", -0.06, 0.0, -0.055, -0.053, zc - 0.003, zc + 0.003, f="y")
    _knob(p, BRASS, 0.0, zc, 0.012, -0.053, -0.062, 6)
    return p.done()


def postcard_rack(F, x, y, z, yaw, top):
    """[prop] a revolving postcard stand on the kiosk counter."""
    p = F.pc("postcard_rack", x, y, z + top, yaw, "prop")
    p.cyl(STEEL, 0, 0, 0.0, 0.015, 0.09, 8)
    p.rod(STEEL, (0, 0, 0.0), (0, 0, 0.5), 0.012, 4)
    cells = ["travel_0", "travel_1", "travel_2", "travel_3"]
    for k in range(4):
        a = k * math.pi / 2
        c, s = math.cos(a), math.sin(a)
        for j in range(2):
            z0 = 0.08 + j * 0.2
            o, hw = 0.07, 0.055
            bl = (c * o + s * hw, s * o - c * hw, z0)
            br = (c * o - s * hw, s * o + c * hw, z0)
            p.card(cells[(k + j) % 4], bl, br, (br[0], br[1], z0 + 0.16), (bl[0], bl[1], z0 + 0.16))
    return p.done(top=0.5)


# ================================================================================================ the engine room
def _gauge(p, x, zc, y, r=0.07, needle=0.6):
    _knob(p, BRASS, x, zc, r, y, y - 0.03, 8, cap=False)
    ang = [2 * math.pi * k / 8 for k in range(8)]
    p.face(LINEN, [(x + r * 0.9 * math.cos(a), y - 0.02, zc + r * 0.9 * math.sin(a)) for a in ang])
    a = math.pi * (1.25 - 1.5 * needle)
    p.rod("CT_SteelRed", (x, y - 0.024, zc), (x + r * 0.75 * math.cos(a), y - 0.024, zc + r * 0.75 * math.sin(a)), 0.008, 3)


def diesel(F, x, y, z, top_z, L=5.2):
    """The main engine: a six-cylinder trunk diesel on its bedplate (local x = toward the bow, the piece's front = the
    starboard side, where the gallery is): crankcase with inspection doors, six heads with rocker covers and indicator
    cocks, the lagged exhaust manifold along the port side rising into the casing (to top_z over the plates), the
    charge-air pipe and the fuel pumps on the starboard side, the flywheel aft with its guard, the control stand with
    gauges, hand wheel and levers at the fore end."""
    p = F.pc("diesel", x, y, z, STBD)
    hl = L / 2
    p.box(STEEL, -hl - 0.2, hl + 0.1, -0.72, 0.72, 0.0, 0.22, f="xXyYZ")
    p.box(ENGINE, -hl + 0.1, hl - 0.1, -0.58, 0.58, 0.22, 1.1, f="xXyYZ")
    p.box(ENGINE, -hl + 0.2, hl - 0.2, -0.46, 0.46, 1.1, 1.62, f="xXyYZ")
    pitch = (L - 0.9) / 6
    for k in range(6):
        c = -hl + 0.45 + pitch * (k + 0.5)
        for s in (-1, 1):                                         # crankcase doors
            a, b = sorted((s * 0.58, s * 0.6))
            p.box(ALU, c - 0.26, c + 0.26, a, b, 0.4, 0.95, f="xXZz" + ("y" if s < 0 else "Y"))
            p.box(LAQ, c - 0.03, c + 0.03, min(s * 0.6, s * 0.625), max(s * 0.6, s * 0.625), 0.64, 0.7,
                  f="xXZz" + ("y" if s < 0 else "Y"))
        p.bev(ENGINE, c - 0.3, c + 0.3, -0.4, 0.4, 1.62, 1.84, 0.03, 0.03)
        p.bev(ALU, c - 0.22, c + 0.22, -0.2, 0.2, 1.84, 1.97, 0.03, 0.04)
        p.cyl(BRASS, c + 0.2, -0.3, 1.84, 1.98, 0.022, 5)
        p.rod(BRASS, (c + 0.2, -0.3, 1.96), (c + 0.2, -0.38, 1.96), 0.014, 4, caps=True)
        _ax(p, LINEN, (c, 0.0, 1.74), "y", 0.4, 0.66, 0.1, segs=6, caps=False)              # exhaust branch
        p.rod(BRASS, (c - 0.15, -0.46, 1.3), (c - 0.15, -0.52, 1.72), 0.014, 3)             # fuel pipe
        p.box(STEEL, c - 0.08, c + 0.08, -0.74, -0.58, 1.12, 1.32, f="xXyZz")               # fuel pump
        p.cyl(BRASS, c, -0.66, 1.32, 1.4, 0.03, 6)
    _ax(p, LINEN, (0.0, 0.76, 1.74), "x", -hl + 0.3, hl - 0.1, 0.17, segs=8)                # the lagged manifold
    for u in (-hl + 0.9, -hl + 1.9, hl - 2.1, hl - 1.1):
        _ax(p, ALU, (0.0, 0.76, 1.74), "x", u - 0.02, u + 0.02, 0.176, segs=8, caps=False)
    p.cyl(LINEN, hl + 0.05, 0.76, 1.6, top_z, 0.24, 8, top=False)                           # the uptake to the funnel
    for zz in (2.3, 3.3, 4.3):
        if zz < top_z - 0.2:
            p.cyl(ALU, hl + 0.05, 0.76, zz, zz + 0.05, 0.247, 8, top=False)
    _ax(p, ALU, (0.0, -0.52, 1.5), "x", -hl + 0.3, hl - 0.25, 0.085, segs=6)                # charge air
    p.box(STEEL, -hl + 0.2, hl - 0.3, -0.76, -0.56, 1.02, 1.1, f="xXyZz")                   # the camshaft trough
    # the flywheel and its guard, the shaft running aft
    _ax(p, STEEL, (0.0, 0.0, 0.72), "x", -hl - 0.42, -hl - 0.12, 0.68, segs=14)
    _ax(p, "CT_SteelRed", (0.0, 0.0, 0.72), "x", -hl - 0.45, -hl - 0.42, 0.3, segs=8)
    _ax(p, ALU, (0.0, 0.0, 0.72), "x", -hl - 1.7, -hl - 0.42, 0.09, segs=6)
    p.box(ENGINE, -hl - 1.5, -hl - 1.15, -0.25, 0.25, 0.0, 0.95, f="xXyYZ")
    for s in (-1, 1):
        p.tube(YELLOW, [(-hl - 0.6, s * 0.78, 0.0), (-hl - 0.6, s * 0.78, 1.5), (-hl + 0.05, s * 0.78, 1.5),
                        (-hl + 0.05, s * 0.78, 0.22)], 0.035, 4)
    p.rod(YELLOW, (-hl - 0.6, -0.78, 1.5), (-hl - 0.6, 0.78, 1.5), 0.035, 4)
    # the control stand at the fore end, its board of gauges facing the gallery (the piece's front)
    cx = hl + 0.75
    p.box(ENGINE, cx - 0.5, cx + 0.5, -0.5, -0.1, 0.0, 1.0, f="xXyYZ")
    p.box(LAQ, cx - 0.55, cx + 0.55, -0.34, -0.3, 1.0, 1.9, f="xXyYZ")
    for i in range(4):
        _gauge(p, cx - 0.36 + i * 0.24, 1.68, -0.34, 0.085, (0.3, 0.55, 0.7, 0.45)[i])
    for i in range(3):
        _gauge(p, cx - 0.24 + i * 0.24, 1.42, -0.34, 0.06, (0.6, 0.35, 0.5)[i])
    p.card_front("panel_control", cx - 0.4, cx + 0.4, 1.04, 1.3, -0.345)
    _torus(p, STEEL, (cx - 0.15, -0.62, 0.8), 0.2, 0.02, axis="y", segs=10, prof=4)         # the manoeuvring wheel
    _ax(p, STEEL, (cx - 0.15, 0.0, 0.8), "y", -0.63, -0.5, 0.03, segs=5)
    for a in (0.0, 2.09, 4.19):
        p.rod(STEEL, (cx - 0.15, -0.62, 0.8), (cx - 0.15 + 0.2 * math.cos(a), -0.62, 0.8 + 0.2 * math.sin(a)), 0.016, 3)
    for i, col in enumerate(("CT_SteelRed", LAQ)):                                           # start and fuel levers
        u = cx + 0.2 + i * 0.16
        p.rod(STEEL, (u, -0.52, 0.7), (u + 0.05, -0.74, 1.12), 0.022, 4)
        _ax(p, col, (u + 0.05, 0.0, 1.12), "y", -0.8, -0.72, 0.035, segs=6)
    p.core(-hl - 0.2, hl + 0.1, -0.7, 0.7, 0.05, 1.6)
    p.core(cx - 0.5, cx + 0.5, -0.5, -0.1, 0.05, 1.0)
    return p.done(solid=True, top=1.97, footprint=(-hl - 0.5, hl + 1.3, -0.8, 0.8))


def generator(F, x, y, z, yaw=STBD, top_z=2.2):
    """A diesel generator set: a four-cylinder engine and its alternator on one bed (local x = toward the bow)."""
    p = F.pc("generator", x, y, z, yaw)
    p.box(STEEL, -1.4, 1.4, -0.42, 0.42, 0.0, 0.16, f="xXyYZ")
    p.box(ENGINE, -1.3, 0.2, -0.3, 0.3, 0.16, 0.95, f="xXyYZ")
    for k in range(4):
        c = -1.12 + k * 0.38
        p.bev(ALU, c - 0.15, c + 0.15, -0.2, 0.2, 0.95, 1.1, 0.025, 0.03)
    _ax(p, LINEN, (0.0, 0.34, 0.92), "x", -1.25, 0.15, 0.08, segs=6)
    p.cyl(LINEN, 0.2, 0.34, 0.85, top_z, 0.1, 6, top=False)
    _ax(p, BLUE, (0.0, 0.0, 0.6), "x", 0.35, 1.25, 0.36, segs=10)
    _ax(p, STEEL, (0.0, 0.0, 0.6), "x", 0.2, 0.35, 0.3, segs=10)
    p.box(LAQ, 0.55, 1.05, -0.2, 0.2, 0.96, 1.12, f="xXyYZ")
    p.core(-1.35, 1.3, -0.35, 0.35, 0.05, 0.9)
    return p.done(solid=True, top=1.12)


def switchboard(F, x, y, z, yaw, w=2.6, h=1.75):
    """[wall] the main switchboard: grey panels, rows of meters, knife switches, the control legend."""
    p = F.pc("switchboard", x, y, z, yaw, "wall")
    p.box("CT_PlasterGrey", -w / 2, w / 2, -0.25, 0.0, 0.15, 0.15 + h, f="xXyZ")
    p.box(STEEL, -w / 2 + 0.03, w / 2 - 0.03, -0.2, 0.0, 0.0, 0.15, f="xXy")
    n = 4
    for k in range(n):
        c = -w / 2 + (k + 0.5) * w / n
        p.box(STEEL, c - w / n / 2 - 0.006, c - w / n / 2 + 0.006, -0.254, -0.25, 0.18, 0.12 + h, f="y")
        _gauge(p, c - 0.13, 0.15 + h - 0.25, -0.25, 0.09, 0.3 + 0.15 * k)
        _gauge(p, c + 0.13, 0.15 + h - 0.25, -0.25, 0.09, 0.7 - 0.1 * k)
        for j in range(2):
            zz = 0.15 + h - 0.62 - j * 0.3
            p.box(LAQ, c - 0.16, c + 0.16, -0.262, -0.25, zz - 0.08, zz + 0.08, f="xXyZz")
            p.rod(BRASS, (c - 0.08, -0.262, zz - 0.05), (c - 0.08, -0.33, zz + 0.07), 0.014, 4, caps=True)
            p.rod(BRASS, (c + 0.08, -0.262, zz - 0.05), (c + 0.08, -0.33, zz + 0.07), 0.014, 4, caps=True)
            p.rod(LAQ, (c - 0.1, -0.33, zz + 0.07), (c + 0.1, -0.33, zz + 0.07), 0.024, 4, caps=True)
        p.box("CT_SteelRed" if k == 0 else OPAL, c - 0.03, c + 0.03, -0.256, -0.25, 0.5, 0.56, f="y")
    p.card_front("panel_control", -0.5, 0.5, 0.26, 0.6, -0.256)
    p.core(-w / 2 + 0.02, w / 2 - 0.02, -0.22, -0.02, 0.2, h)
    return p.done(solid=True, top=0.15 + h, footprint=(-w / 2, w / 2, -0.25, 0.0))


def pipe_run(F, x, y, z, yaw, L, runs):
    """[wall] pipes along a wall: runs = [(height, radius, material, stand-off)], with flanges and clips."""
    p = F.pc("pipe_run", x, y, z, yaw, "prop")
    for (zz, r, mat, off) in runs:
        _ax(p, mat, (0.0, -off - r, zz), "x", -L / 2, L / 2, r, segs=6, caps=False)
        n = max(2, int(L / 1.6))
        for k in range(n):
            u = -L / 2 + (k + 0.5) * L / n
            _ax(p, STEEL, (0.0, -off - r, zz), "x", u - 0.025, u + 0.025, r * 1.35, segs=6)
    return p.done()


def valve_wheel(F, x, y, z, yaw, zc=1.2, col="CT_SteelRed"):
    """[wall] a hand-wheel valve on a pipe stub."""
    p = F.pc("valve_wheel", x, y, z, yaw, "prop")
    _ax(p, STEEL, (0.0, 0.0, zc), "y", -0.2, 0.0, 0.035, segs=5)
    _torus(p, col, (0.0, -0.2, zc), 0.11, 0.015, axis="y", segs=8, prof=3)
    for a in (0.0, 1.57):
        dx, dz = 0.11 * math.cos(a), 0.11 * math.sin(a)
        p.rod(col, (-dx, -0.2, zc - dz), (dx, -0.2, zc + dz), 0.012, 3)
    return p.done()


def gauge_board(F, x, y, z, yaw, zc=1.7):
    """[wall] the engine-room gauge board the passengers are shown: a varnished board, a brass clock and five gauges."""
    p = F.pc("gauge_board", x, y, z, yaw, "wall")
    p.bev(WOOD, -0.75, 0.75, -0.035, 0.0, zc - 0.36, zc + 0.36, 0.01)
    _ax(p, BRASS, (0.0, 0.0, zc + 0.08), "y", -0.075, -0.035, 0.17, segs=12, caps=False)
    kit._round_dial(p, "clock_0", 0.0, zc + 0.08, 0.155, -0.07, 12)
    for i, (u, v) in enumerate(((-0.52, 0.12), (-0.3, -0.16), (0.3, -0.16), (0.52, 0.12), (0.0, -0.23))):
        _gauge(p, u, zc + v, -0.035, 0.09 if i < 4 else 0.06, (0.35, 0.6, 0.5, 0.75, 0.4)[i])
    p.box(BRASS, -0.3, 0.3, -0.039, -0.035, zc + 0.29, zc + 0.33, f="y")
    return p.done()


def oil_drum(F, x, y, z, yaw=0.0, mat=BLUE):
    p = F.pc("oil_drum", x, y, z, yaw)
    p.lathe(mat, 0, 0, [(0.28, 0.0), (0.28, 0.3), (0.29, 0.31), (0.28, 0.32), (0.28, 0.58), (0.29, 0.59), (0.28, 0.6),
                        (0.28, 0.88)], 10)
    p.cyl(STEEL, 0.14, 0.0, 0.88, 0.9, 0.03, 5)
    p.core(-0.2, 0.2, -0.2, 0.2, 0.05, 0.85)
    return p.done(solid=True, top=0.88)


# ================================================================================================ rooms
def drapes(F, x, y, z, yaw, w, h, mat, open_=0.45):
    """[wall] window curtains on a brass rod, gathered to the sides (seen from the room only: one-sided, four folds a
    drape; the kit's curtains cost three times as much)."""
    p = F.pc("drapes", x, y, z, yaw, "prop")
    p.rod(BRASS, (-w / 2 - 0.12, -0.07, h), (w / 2 + 0.12, -0.07, h), 0.022, 4, caps=True)
    dw = w * (1 - open_) / 2 + 0.1
    rows = [(h - 0.03, 0.9, 0.012), (h * 0.42, 0.72, 0.03), (0.0, 1.0, 0.035)]
    for s in (-1, 1):
        xo = s * (w / 2 + 0.1)
        rr = []
        for zz, share, amp in rows:
            rr.append([(xo - s * dw * share * k / 4, -0.07 - amp * (1 if k % 2 else -0.4), zz) for k in range(5)])
        for r0, r1 in zip(rr, rr[1:]):
            for k in range(4):
                q = [r1[k], r1[k + 1], r0[k + 1], r0[k]]
                p.face(mat, q if s < 0 else q[::-1])
    return p.done()


def _curtains(F, wins, z, mat, drop=0.1):
    """Curtains on a brass rod at every window of the list (F.windows): gathered to the sides."""
    for (x, y, zc, yaw, w, h, _shape) in wins:
        fx, fy = _dirv(yaw)
        # the wall's inner face: about 0.07 in from the card
        drapes(F, x + fx * 0.07, y + fy * 0.07, zc - h / 2 - drop, yaw, w + 0.1, h + drop + 0.1, mat)


def _dining_saloon(F):
    S, B = F.S, F.B
    z = S.Z_C
    tables = [(9.7, 3.55), (9.7, -3.55), (12.2, 3.55), (12.2, -3.55), (14.7, 3.55), (14.7, -3.55),
              (12.2, 1.62), (12.2, -1.62), (14.7, -1.62)]
    n = 0
    for t, (x, y) in enumerate(tables):
        r = saloon_table(F, x, y, z, FWD, seed=t)
        for (lx, ly, face) in r["chairs"]:
            n += 1
            cx, cy = F.loc(x, y, FWD, lx, ly)
            c = F.put(kit.chair, cx, cy, z, (FWD + face) % 360, kind="wood", mat=WOOD if t % 2 else "CT_Leather")
            F.seat("dining_chair_%d" % n, c)
        F.spot("table_%d" % (t + 1), "table", x, y, z, FWD)
    # the captain's table: aft, on the centreline, its long side fore and aft
    x, y = 10.0, 0.0
    r = saloon_table(F, x, y, z, STBD, w=1.5, d=0.9, captain=True, seed=99)
    for (lx, ly, face) in r["chairs"]:
        n += 1
        cx, cy = F.loc(x, y, STBD, lx, ly)
        c = F.put(kit.chair, cx, cy, z, (STBD + face) % 360, kind="wood", mat=RED)
        F.seat("dining_chair_%d" % n, c)
    F.thing("captains_table", "board", x, y, z + 0.9, AFT)
    F.spot("table_captain", "table", x, y, z, STBD)
    # the fore bulkhead: sideboard under the serving hatch, the painting, the clock and the broadcast box
    xw = 18.0 - S.WALL_T / 2
    sideboard(F, xw, 0.0, z, AFT)
    F.thing("dining_hatch", "counter", xw - 0.3, 0.0, z + 1.15, AFT, role="steward", service="canteen")
    F.spot("staff_steward", "work", 18.8, 0.0, z, AFT)
    F.spot("dining_hatch_queue", "counter", xw - 1.0, 0.0, z, FWD)
    F.put(kit.picture, xw, 3.9, z, AFT, name="painting_sea_calm", w=1.5, zc=1.55, frame=BRASS)
    F.put(kit.picture, xw, -4.25, z, AFT, name="painting_lighthouse", w=0.7, zc=1.55)
    F.put(kit.wall_clock, xw, -2.0, z, AFT, zc=2.05, r=0.15, face="clock_1")
    loudspeaker(F, xw, 2.0 - 0.75, z, AFT, zc=2.12)
    F.put(kit.menu_board, xw, 1.5, z, AFT, name="menu", w=0.34, zc=1.5)
    # aft bulkhead, both sides of the double door: a fire point and the life-saving notice
    xa = 8.0 + S.WALL_T / 2
    F.put(kit.fire_extinguisher, xa, -1.25, z, FWD)
    F.put(kit.poster, xa, 1.3, z, FWD, name="poster_safety", w=0.42, zc=1.55)
    # windows: cream curtains tied back; lamps: three lit plafonds on the centreline, four that only glow
    _curtains(F, F.windows(8.0, 18.0, -5.2, 5.2, z + 0.5, z + 2.3), z, LINEN)
    for x in (10.0, 13.0, 16.0):
        plafond(F, x, 0.0, "C")
    for x in (11.0, 14.8):
        for s in (-1, 1):
            plafond(F, x, s * 3.3, "C")
    F.put(kit.plant, 17.45, 4.35, z, 0.0, kind="ficus", pot=BLUE, h=1.25, seed=4)
    F.put(kit.plant, 8.4, 2.55, z, 0.0, kind="palm", pot=RED, h=1.5, seed=7)
    for i, (x, y) in enumerate(((8.6, 0.0), (11.0, 0.0), (13.5, 0.0), (16.2, 0.0), (16.8, 2.25), (16.8, -3.05),
                                (11.0, 2.55), (11.0, -2.55), (13.5, 2.6), (13.5, -2.6), (9.2, 1.9), (9.2, -1.9))):
        F.spot("path_c_saloon_%d" % (i + 1), "path", x, y, z, FWD)
    F.spot("stand_saloon_1", "stand", 16.9, 3.3, z, FWD)
    F.spot("stand_saloon_2", "stand", 8.9, -2.6, z, AFT)


def _servery_galley(F):
    S = F.S
    z = S.Z_C
    xw = 18.0 + S.WALL_T / 2
    # servery: the work top under the hatch, the boiler, crockery shelves on the galley bulkhead
    F.put(kit.table, xw + 0.24, 0.0, z, FWD, w=2.6, d=0.42, h=0.9, kind="steel", mat=ALU)
    crockery(F, xw + 0.26, -0.75, z + 0.9, FWD)
    F.put(kit.teapot, xw + 0.24, 0.75, z + 0.9, 40.0)
    titan(F, 19.65, 1.05, z, AFT)
    for zt in (1.2, 1.65):
        F.put(kit.wall_shelf, 20.0 - S.WALL_T / 2, -0.4, z, AFT, w=1.5, zt=zt, seed=3 + int(zt * 10))
    F.put(kit.wall_shelf, 20.0 - S.WALL_T / 2, -4.2, z, AFT, w=0.9, zt=1.5, seed=5)
    F.put(kit.cardboard_box, 19.6, -4.4, z, 20.0, seed=2)
    F.put(kit.sack, 19.55, -3.8, z, 10.0)
    plafond(F, 19.0, -0.4, "C")
    # galley (looked into over the half-door at x 27, y -3.6..-2.5): the range on the casing bulkhead, the sink
    # and cupboards on the ship's side, a steel table in the middle, the big pot, the rail of ladles
    yc = -1.5 - S.WALL_T / 2
    ys = -(S.half(23.5) - S.SIDE_T)
    F.put(kit.kitchen_unit, 23.3, yc, z, PORT, modules="cupboard,stove,cupboard", wall_cupboards=False)
    pot_rack(F, 23.3, yc, z, PORT, w=2.0, zc=1.95)
    F.put(kit.dumpling_pot, 25.35, yc - 0.33, z, PORT, steamers=1)
    F.put(kit.kitchen_unit, 22.9, ys, z, STBD, modules="fridge,cupboard,sink,cupboard,cupboard")
    F.put(kit.wall_shelf, 25.3, ys, z, STBD, w=1.2, zt=1.55, seed=8)
    t = F.put(kit.table, 23.4, -3.15, z, FWD, w=0.8, d=1.8, h=0.88, kind="steel", mat=ALU)
    crockery(F, 23.4, -3.6, t["top"], STBD)
    F.put(kit.kitchen_scales, 23.35, -2.75, t["top"], 200.0)
    F.put(kit.cardboard_box, 23.45, -3.1, t["top"], 80.0, seed=5)
    F.put(kit.sack, 20.5, -4.3, z, 30.0)
    F.put(kit.sack, 20.45, -3.75, z, 70.0)
    F.put(kit.cardboard_box, 20.5, -1.95, z, 0.0, w=0.5, d=0.4, h=0.35, seed=9)
    plafond(F, 22.2, -3.2, "C")
    plafond(F, 25.2, -3.2, "C")
    half_door_shelf(F, 27.0 + S.WALL_T / 2, -3.05, z, FWD)
    F.thing("galley_halfdoor", "counter", 27.0 + S.WALL_T / 2 + 0.1, -3.05, z + 1.1, FWD, role="cook", service="galley")
    F.spot("staff_cook", "work", 26.2, -3.05, z, FWD)
    F.spot("cook_range", "work", 23.4, -2.35, z, STBD)
    F.spot("galley_queue", "counter", 27.75, -3.05, z, AFT)


def _stbd_corridor(F):
    S = F.S
    z = S.Z_C
    yc = 1.5 + S.WALL_T / 2                # the casing's face
    yw = 3.0 - S.WALL_T / 2                # the washrooms' face
    handrail(F, 18.4, yc, 22.7, yc, z, STBD)
    handrail(F, 24.4, yc, 26.7, yc, z, STBD)
    fire_box(F, 25.6, yc, z, STBD, zc=1.5)
    F.put(kit.fire_extinguisher, 26.35, yc, z, STBD)
    F.put(kit.poster, 21.0, yc, z, STBD, name="poster_safety", w=0.4, zc=1.6)
    F.put(kit.picture, 19.4, yc, z, STBD, name="painting_dunes", w=0.6, zc=1.6)
    for k, x in enumerate((19.5, 22.5, 25.5)):
        plafond(F, x, 2.25, "C", blue=(k == 1))
    carpet_runner(F, 18.3, 26.8, 2.25, z)
    F.thing("engine_door", "door", 23.55, yc + 0.02, z + 1.1, STBD)
    for i, x in enumerate((18.6, 20.5, 22.6, 24.6, 26.6)):
        F.spot("path_c_stbd_%d" % (i + 1), "path", x, 2.25, z, FWD)
    return yw


def _strip_plain_engine(F):
    """Look-dev only (env SHIP_W2_STRIP_ENGINE): drop the shell's plain engine masses (W1_requests.md asks the shell
    to delete them) so that the previews show this file's engine alone."""
    S = F.S
    lo = S.p(18.9, 3.6, S.Z_D + 0.0)
    hi = S.p(26.95, -3.6, S.Z_D + 2.45)
    for m in (F.B.m, F.B.detail):
        keep = []
        for face in m.faces:
            vs = [m.verts[i] for i in face[1]]
            inside = all(lo[0] <= v[0] <= hi[0] and lo[1] <= v[1] <= hi[1] and lo[2] - 0.001 <= v[2] <= hi[2] for v in vs)
            if inside and max(v[2] for v in vs) > lo[2] + 0.05:
                continue
            keep.append(face)
        m.faces[:] = keep


def _engine_room(F):
    S = F.S
    zc, zd = S.Z_C, S.Z_D
    top = S.ceil("C") - zd
    if os.environ.get("SHIP_W2_STRIP_ENGINE"):
        _strip_plain_engine(F)
    diesel(F, 22.7, 0.0, zd, top)
    generator(F, 20.6, -2.95, zd)
    generator(F, 20.6, 2.95, zd)
    switchboard(F, 27.0 - 0.06, 0.1, zd, AFT, w=2.4)
    oil_drum(F, 26.3, 3.6, zd)
    oil_drum(F, 25.65, 3.75, zd, mat=YELLOW)
    F.put(kit.tools, 25.2, -3.9, zd, 30.0)
    # the casing's port wall, straight across from the gallery: pipes, valves, the gauge board, caged lamps
    yp = -1.5 + S.WALL_T / 2
    pipe_run(F, 23.5, yp, zc, STBD, 6.6, [(2.05, 0.07, "CT_SteelRed", 0.04), (1.8, 0.05, BLUE, 0.04),
                                           (0.35, 0.09, YELLOW, 0.04), (0.15, 0.045, ALU, 0.04)])
    gauge_board(F, 23.6, yp, zc, STBD, zc=1.2)
    valve_wheel(F, 21.6, yp, zc, STBD, zc=1.8, col=BLUE)
    valve_wheel(F, 25.4, yp, zc, STBD, zc=2.05)
    bulkhead_light(F, 21.9, yp, zc, STBD, zc=1.25)
    bulkhead_light(F, 25.3, yp, zc, STBD, zc=1.25)
    # down in the engine room: lamps on the fore and aft bulkheads
    bulkhead_light(F, 27.0 - S.WALL_T / 2, 0.6, zd, AFT, zc=2.0)
    bulkhead_light(F, 18.0 + S.WALL_T / 2, -1.0, zd, FWD, zc=2.0)
    bulkhead_light(F, 18.0 + S.WALL_T / 2, 1.0, zd, FWD, zc=2.0)
    F.thing("engine_gallery", "board", 23.75, 0.36, zc + 1.1, STBD, topic="engine")
    F.spot("staff_engineer", "work", 25.0, 0.95, zc, PORT)
    F.spot("stand_engine_gallery", "stand", 23.2, 0.95, zc, PORT)


def _vestibule(F):
    S, B = F.S, F.B
    z = S.Z_C
    ysp = -(S.half(29.0) - S.SIDE_T)       # port side, inner face
    yss = -ysp
    # purser's office, port: the desk faces the stairs, the key board and the safe behind it on the ship's side
    d = F.put(kit.reception_desk, 29.1, -3.45, z, STBD, w=2.0, mat=WOOD, front=GREEN)
    F.thing("purser_desk", "counter", 29.1, -3.2, z + 1.1, STBD, role="purser", service="reception")
    F.spot("staff_purser", "work", 29.1, -4.2, z, STBD)
    F.spot("purser_queue", "counter", 29.1, -2.45, z, PORT)
    key_board(F, 29.1, ysp, z, STBD)
    F.thing("key_board", "board", 29.1, ysp + 0.08, z + 1.6, STBD)
    F.put(kit.safe, 27.75, ysp, z, STBD)
    F.put(kit.telephone, 28.45, -3.72, z + 0.75, 250.0)
    F.put(kit.table_lamp, 29.85, -3.7, z + 0.75, 0.0, shade=GREEN)
    # kiosk, starboard: a counter with postcards, shelves of souvenirs behind it
    c = F.put(kit.shop_counter, 28.7, 3.5, z, PORT, w=1.9, mat=WOODL, front=BLUE)
    F.thing("ship_kiosk", "counter", 28.7, 3.2, z + 1.05, PORT, role="purser", service="shop")
    F.put(kit.shop_shelf, 28.7, yss, z, PORT, w=1.9, goods="toys", mat=WOODL, seed=6, h=1.9)
    postcard_rack(F, 28.05, 3.42, z, PORT, 1.05)
    F.put(kit.magazine_rack, 27.0 + S.WALL_T / 2, 3.85, z, FWD, w=0.8, seed=2)
    F.spot("kiosk_staff", "work", 28.7, 4.2, z, PORT)
    F.spot("kiosk_queue", "counter", 28.7, 2.5, z, STBD)
    # the fore bulkhead (x 32): notice board and post box to starboard of the corridor door, clock, barometer and
    # the route map to port
    xf = 32.0 - S.WALL_T / 2
    F.put(kit.notice_board, xf, 2.2, z, AFT, w=1.1, zc=1.55, seed=1)
    F.thing("notice_board", "board", xf - 0.08, 2.2, z + 1.55, AFT, topic="programme")
    post_box(F, xf, 3.5, z, AFT)
    F.thing("post_box", "machine", xf - 0.2, 3.5, z + 1.35, AFT, what="post_box")
    F.put(kit.wall_map, xf, -2.3, z, AFT, w=1.3, zc=1.6, name="map_coast")
    F.put(kit.wall_clock, xf, -3.6, z, AFT, zc=1.95, r=0.15, face="clock_2")
    barometer(F, xf, -3.6, z, AFT, zc=1.55)
    F.thing("barometer", "board", xf - 0.1, -3.6, z + 1.55, AFT, topic="weather")
    loudspeaker(F, xf, 1.25, z, AFT, zc=2.15)
    # lamps and people
    plafond(F, 31.5, 0.0, "C")
    for i, (x, y) in enumerate(((27.4, 2.25), (27.4, -2.3), (29.5, 2.2), (29.5, -2.2), (31.6, 2.2), (31.6, -2.2),
                                (31.6, 0.0), (31.1, 4.0), (31.1, -4.0))):
        F.spot("path_c_vest_%d" % (i + 1), "path", x, y, z, FWD)
    F.spot("stand_vestibule_1", "stand", 31.3, 2.3, z, FWD)
    F.spot("stand_vestibule_2", "stand", 31.2, -2.6, z, FWD)
    F.spot("stand_vestibule_3", "stand", 30.6, 3.6, z, STBD)


# ---------------------------------------------------------------------------------------------- cabins
CABINS_C = {1: (43.2, 46.0, 1, 2), 2: (43.2, 46.0, -1, 2), 3: (40.4, 43.2, 1, 2), 4: (40.4, 43.2, -1, 2),
            5: (37.6, 40.4, 1, 4), 6: (37.6, 40.4, -1, 4), 7: (34.8, 37.6, 1, 4), 8: (34.8, 37.6, -1, 4),
            9: (32.0, 34.8, 1, 4), 10: (32.0, 34.8, -1, 4)}
CABINS_D = {15: (33.0, 36.5, 1, 4), 16: (33.0, 36.5, -1, 4), 17: (36.5, 40.0, 1, 4), 18: (36.5, 40.0, -1, 4),
            19: (40.0, 43.5, 1, 4), 20: (40.0, 43.5, -1, 4)}
OPEN_CABINS = (1, 2, 7, 15)             # door holes without a leaf in the shell: the leaf is drawn here, folded open
DOOR_OFF = 0.80                         # the middle of a cabin door from the cabin's aft bulkhead line (hole +-0.55)
WALL_C, WALL_D = 0.80, 0.75             # the corridor walls' lines on decks C and D


def _side(S, deck, x):
    """|y| of the inner face of the ship's side at x (deck C: the side, deck D: the hull's lining)."""
    return S.half(x) - S.SIDE_T if deck == "C" else S.deck_half("D", x)


def _side_yaw(S, deck, x, s):
    """Ship yaw of the inward normal of the ship's side at x on side s (+1 starboard)."""
    dh = (_side(S, deck, x + 0.3) - _side(S, deck, x - 0.3)) / 0.6
    return math.degrees(math.atan2(-s, dh)) % 360.0


def _xfaces(S, n):
    """Ship x of the inner faces of cabin n's aft and fore bulkheads."""
    deck = "C" if n in CABINS_C else "D"
    x0, x1 = (CABINS_C if deck == "C" else CABINS_D)[n][:2]
    ta = 0.06 if (deck == "D" and x0 == 33.0) else S.WALL_T / 2
    tf = S.SIDE_T if (deck == "C" and x1 == 46.0) else (0.06 if (deck == "D" and x1 == 43.5) else S.WALL_T / 2)
    return x0 + ta, x1 - tf


def _cabin_geom(S, n):
    """(x0, x1, s, berths, deck, z, |y| of the corridor wall's cabin face, |y| of the side's inner face where the aft
    berth ends, the same for the fore berth)."""
    deck = "C" if n in CABINS_C else "D"
    x0, x1, s, nb = (CABINS_C if deck == "C" else CABINS_D)[n]
    yi = (WALL_C if deck == "C" else WALL_D) + S.WALL_T / 2
    ya = min(_side(S, deck, x0 + 0.06), _side(S, deck, x0 + 0.8))
    yf = min(_side(S, deck, x1 - 0.06), _side(S, deck, x1 - 0.8))
    return x0, x1, s, nb, deck, S.Z[deck], yi, ya, yf


def _berth_plan(S, n):
    """Where the berths of cabin n are: [(letter, wall x, y of the berth's middle, the wall's yaw, upper)]: two
    athwartships against the aft and the fore bulkhead at the ship's side, heads inboard; uppers over them in the
    four-berth cabins."""
    x0, x1, s, nb, deck, z, yi, ya, yf = _cabin_geom(S, n)
    xa, xf = _xfaces(S, n)
    out = [("a", xa, s * (ya - 0.06 - 0.95), FWD, False), ("b", xf, s * (yf - 0.06 - 0.95), AFT, False)]
    if nb == 4:
        out += [("c", xa, s * (ya - 0.06 - 0.95), FWD, True), ("d", xf, s * (yf - 0.06 - 0.95), AFT, True)]
    return out


def door_leaf(F, x, y, z, yaw, text, w=1.06, h=2.04, card=True, flip=False):
    """A cabin door standing open, folded back against the bulkhead: (x, y) = its hinge edge on the floor, the leaf runs
    w toward local +x; yaw = the way its inner face looks. Timber, brass handle, the number plate on the far side and
    the passenger's notice («Памятка пассажиру», typed) on the inner face."""
    p = F.pc("door_leaf", x, y, z, yaw, "prop")
    p.box("CT_Door", 0.0, w, 0.0, 0.04, 0.02, h, f="xXyYZz")
    p.box(WOOD, 0.1, w - 0.1, -0.006, 0.0, 1.15, h - 0.12, f="xXyZz")
    p.box(WOOD, 0.1, w - 0.1, -0.006, 0.0, 0.14, 0.95, f="xXyZz")
    ha, hb = (0.09, 0.2) if flip else (w - 0.09, w - 0.2)      # flip: the hinge is at the local +x end
    p.rod(BRASS, (ha, -0.05, 1.03), (hb, -0.05, 1.03), 0.018, 4, caps=True)
    p.rod(BRASS, (ha, -0.006, 1.03), (ha, -0.05, 1.03), 0.014, 4)
    p.rod(BRASS, (ha, 0.09, 1.03), (hb, 0.09, 1.03), 0.018, 4, caps=True)
    p.rod(BRASS, (ha, 0.04, 1.03), (ha, 0.09, 1.03), 0.014, 4)
    if card:
        p.card_front("paper_typed", w / 2 - 0.1, w / 2 + 0.1, 1.42, 1.42 + 0.2 * 1.39, -0.008)
    # the number on the corridor face (the back)
    hh = 0.085
    ww = 0.09 + 0.07 * len(text)
    p.box(BRASS, w / 2 - ww / 2, w / 2 + ww / 2, 0.04, 0.047, 1.62 - hh / 2, 1.62 + hh / 2, f="xXYZz")
    return p.done()


def _cabin_door(F, n):
    """The door thing (on the shell's closed leaf, or on the leaf drawn folded open here), the brass number plate on
    the corridor wall beside it and the path spot before it."""
    S = F.S
    x0, x1, s, nb, deck, z, yi, ya, yf = _cabin_geom(S, n)
    yc = yi - S.WALL_T                    # the wall's corridor face
    inb, out = (PORT, STBD) if s > 0 else (STBD, PORT)
    xd = x0 + DOOR_OFF
    number_plate(F, xd + 0.78, s * yc, z, inb, "%d" % n)
    if n in OPEN_CABINS:
        # hinged on the aft jamb, swung into the cabin: it stands along the aft bulkhead, 0.2 off it, inner face forward
        hx, hy = x0 + 0.27, s * (yi + 0.02)
        # the leaf runs outboard from the hinge: local +x of a piece facing the bow is to port
        if s > 0:
            door_leaf(F, hx, s * (yi + 0.02 + 1.06), z, FWD, "%d" % n, flip=True)
        else:
            door_leaf(F, hx, hy, z, FWD, "%d" % n)
        F.thing("cabin%d_door" % n, "door", hx + 0.05, s * (yi + 0.55), z + 1.1, FWD, cabin=n, open=1)
    else:
        F.thing("cabin%d_door" % n, "door", xd, s * (yc - 0.03), z + 1.1, inb, cabin=n)
    F.spot("path_%s_cabin%d" % (deck.lower(), n), "path", xd, s * (yc - 0.4), z, out)


def _berth_spots(F, n, furnished=None):
    """A bed spot for every berth of cabin n (id berth_<cabin>_<letter>): on the mattress, facing from the head toward
    the foot. furnished: {letter: berth dict} of the berths that were built."""
    S = F.S
    x0, x1, s, nb, deck, z, yi, ya, yf = _cabin_geom(S, n)
    for (k, xw, ym, yaw, upper) in _berth_plan(S, n):
        if furnished and k in furnished:
            b = furnished[k]
            F.spot("berth_%d_%s" % (n, k), "bed", b["bed"][0], b["bed"][1], b["bed"][2], b["bed_yaw"])
            continue
        fx, _fy = _dirv(yaw)
        F.spot("berth_%d_%s" % (n, k), "bed", xw + fx * 0.36, ym, z + (1.57 if upper else 0.47), STBD if s > 0 else PORT)


def shelf_over_door(F, x, y, z, yaw, n=2, zt=2.13):
    """[wall] the lifejacket shelf over a cabin door, with the cork jackets on it."""
    p = F.pc("lifejacket_shelf", x, y, z, yaw, "prop")
    p.box(WOODL, -0.4, 0.4, -0.3, 0.0, zt - 0.02, zt, f="xXyZz")
    for sx in (-0.34, 0.34):
        p.rod(BRASS, (sx, -0.26, zt - 0.02), (sx, 0.0, zt - 0.2), 0.012, 3)
    for k in range(n):
        u = -0.2 + k * 0.4 if n > 1 else 0.0
        _pad(p, "CT_SteelRed", u - 0.17, u + 0.17, -0.27, -0.03, zt, zt + 0.15, 0.03, 0.012, 1)
        p.box(WHITE, u - 0.02, u + 0.02, -0.273, -0.027, zt + 0.01, zt + 0.154, f="yZ")
    return p.done()


STYLE = {  # blanket, towel, curtains
    "hero": (BLUE, LINEN, BLUE), "teacher": (GREEN, YELLOW, GREEN), "granny": (RED, BLUE, YELLOW),
    "honeymoon": (PINK, PINK, PINK)}


def _cabin_c(F, n, style):
    """A furnished cabin of the main deck. style: "hero" (cabin 7: one berth made up, three bare), "teacher"
    (cabin 1, Вера Игнатьевна), "granny" (cabin 2, бабушка Фаина and Сеня) or "honeymoon"."""
    S, B = F.S, F.B
    x0, x1, s, nb, deck, z, yi, ya, yf = _cabin_geom(S, n)
    out, inb = (STBD, PORT) if s > 0 else (PORT, STBD)
    xa, xf = _xfaces(S, n)
    xc = (xa + xf) / 2
    yo = _side(S, deck, xc)
    blank, towel, drape = STYLE[style]
    built = {}
    for (k, xw, ym, yaw, upper) in _berth_plan(S, n):
        # looking at a berth on the aft bulkhead (it faces the bow) inboard is at the viewer's right on the starboard side
        right_in = (yaw == FWD) == (s > 0)
        made = not (style == "hero" and k != "a")
        fold = {"teacher": YELLOW, "granny": "CT_PlasterGrey"}.get(style) if k == "a" else None
        b = berth(F, xw, ym, z, yaw, upper=upper, made=made, head=1 if right_in else -1, blanket=blank, fold=fold)
        built[k] = b
        F.thing("cabin%d_bed_%s" % (n, k), "bed", b["bed"][0], b["bed"][1], b["bed"][2], b["bed_yaw"],
                height=min(1.2, b["height"]), rest="private", cabin=n)
    if nb == 4:
        berth_ladder(F, xa + 0.72, s * (ya - 0.42), z, FWD)
    # the folding table under the windows (turned with the ship's side), the stool, the rug
    syaw = _side_yaw(S, deck, xc, s)
    folding_table(F, xc, s * (yo - 0.01), z, syaw, w=0.62)
    st = F.put(kit.stool, xc, s * (yo - 0.8), z, out)
    F.seat("cabin%d_desk" % n, st, cabin=n)
    F.put(kit.rug, xc, s * (yo - 1.15), z, out, w=0.9, d=1.6, name="rug", seed=n)
    # inboard half, fore bulkhead: wardrobe by the corridor wall, then the washbasin with its mirror
    by = (yf - 0.06 - 1.9) - 0.32
    ww = max(0.6, min(0.85, by - 0.27 - yi - 0.1))
    wy = yi + 0.06 + ww / 2
    F.put(kit.wardrobe, xf, s * wy, z, AFT, w=ww - 0.06, h=1.9, mat=WOOD)
    F.thing("cabin%d_wardrobe" % n, "board", xf - 0.72, s * wy, z + 1.2, AFT, what="wardrobe")
    washbasin(F, xf, s * by, z, AFT, towel=towel)
    F.put(kit.mirror, xf, s * by, z, AFT, zc=1.6, w=0.4, h=0.5, frame=BRASS)
    F.thing("cabin%d_washbasin" % n, "machine", xf - 0.3, s * by, z + 0.95, AFT, what="tap")
    # aft bulkhead, outboard of the open door's leaf: loudspeaker, hooks; over the door: the lifejacket shelf
    hy = min(yi + 1.6, ya - 0.06 - 1.9 - 0.3)
    loudspeaker(F, xa, s * hy, z, FWD, zc=2.1)
    F.thing("cabin%d_radio" % n, "machine", xa + 0.12, s * hy, z + 2.1, FWD, what="loudspeaker")
    shelf_over_door(F, x0 + DOOR_OFF, s * yi, z, out, n=2)
    # windows: curtains, and the one nearest the table opens
    wins = F.windows(x0, x1, min(s * 0.5, s * 5.2), max(s * 0.5, s * 5.2), z + 0.6, z + 2.3, side_only=True)
    _curtains(F, wins, z, drape)
    if wins:
        w0 = min(wins, key=lambda w_: abs(w_[0] - xc))
        fx, fy = _dirv(w0[3])
        F.thing("cabin%d_porthole" % n, "machine", w0[0] + fx * 0.4, w0[1] + fy * 0.4, w0[2], w0[3], what="window")
    else:
        F.thing("cabin%d_porthole" % n, "machine", xc, s * (yo - 0.25), z + 1.4, inb, what="window")
    plafond(F, xc, s * (yi + 1.3), deck)
    bx, by_, bz, ky = S.fr(x0 + DOOR_OFF + 0.78, s * yi, z, out)
    import _homekit as hk
    hk.switch(B, "cabin%d_switch" % n, bx, by_, ky, z=bz, zc=1.3, scope="room")
    ty = s * (yo - 0.22)
    top = z + 0.725
    wtop = z + 1.9
    if style == "hero":
        coat_hooks(F, xa, s * hy, z, FWD, n=3, zc=1.65)
        suitcase(F, xa + 0.14, s * hy, z, FWD, mat="CT_Leather")
        F.put(kit.glass, xc + 0.2, ty, top, 0.0)
        F.put(kit.newspaper, xc - 0.08, ty, top, 20.0)
    elif style == "teacher":
        coat_hooks(F, xa, s * hy, z, FWD, n=3, zc=1.65, coat="CT_PlasterGrey", hat=True)
        suitcase(F, xf - 0.32, s * wy, wtop, AFT, mat=GREEN, w=0.56, flat=True)
        F.put(kit.globe, xc - 0.15, ty, top, 30.0)
        F.put(kit.books_stack, xc + 0.17, ty, top, 15.0, n=4, seed=3)
        F.put(kit.wall_map, xa, s * (ya - 1.0), z, FWD, w=1.0, zc=1.5, name="map_world")
        F.put(kit.poster, x0 + DOOR_OFF + 1.4, s * yi, z, out, name="chart_birds", w=0.5, zc=1.5)
        F.thing("cabin%d_globe" % n, "board", xc - 0.15, ty, top + 0.3, inb, topic="geography")
    elif style == "granny":
        coat_hooks(F, xa, s * hy, z, FWD, n=3, zc=1.65, coat=BLUE, hat=True)
        suitcase(F, xf - 0.32, s * wy, wtop, AFT, mat="CT_Leather", w=0.6, flat=True)
        suitcase(F, xf - 0.3, s * wy, wtop + 0.2, AFT, mat=YELLOW, w=0.4, h=0.3, d=0.14, flat=True)
        F.put(kit.teapot, xc - 0.13, ty, top, 30.0, mat=RED)
        F.put(kit.cup, xc + 0.14, ty, top, 0.0)
        F.put(kit.poster, xa, s * (ya - 1.25), z, FWD, name="kids", w=0.42, zc=1.4, seed=2)
        F.put(kit.poster, xa, s * (ya - 0.7), z, FWD, name="kids", w=0.34, zc=1.3, seed=5)
        # the boy's sailing boat on the floor by the table
        p = F.pc("toy_boat", xc + 0.6, s * (yo - 0.42), z, inb, "prop")
        p.loft(RED, [[(-0.16, 0.0, 0.02), (0.0, -0.05, 0.02), (0.16, 0.0, 0.02), (0.0, 0.05, 0.02)],
                     [(-0.2, 0.0, 0.08), (0.0, -0.07, 0.08), (0.22, 0.0, 0.08), (0.0, 0.07, 0.08)]], top=True, bottom=True)
        p.rod(WOODL, (0.0, 0.0, 0.08), (0.0, 0.0, 0.42), 0.01, 3)
        tri = [(0.008, 0.0, 0.4), (0.008, 0.0, 0.12), (0.17, 0.0, 0.12)]
        p.face(WHITE, tri)
        p.face(WHITE, tri[::-1])
        p.done()
    else:
        coat_hooks(F, xa, s * hy, z, FWD, n=3, zc=1.65, coat=LAQ, hat=True)
        suitcase(F, xf - 0.32, s * wy, wtop, AFT, mat=BLUE, w=0.6, flat=True)
        F.put(kit.vase, xc - 0.12, ty, top, 0.0, seed=3)
        F.put(kit.glass, xc + 0.15, ty, top, 0.0)
        F.put(kit.picture, xa, s * (ya - 1.0), z, FWD, name="painting_sea_night", w=0.5, zc=1.45)
    _berth_spots(F, n, built)
    F.spot("stand_cabin%d" % n, "stand", xc, s * (yi + 1.5), z, out)


def _cabin_15(F):
    """The hold cabin of the cheap ticket: two steel two-tier bunks, lockers, a washbasin, a table under the
    portholes, pipes under the deckhead, other people's luggage."""
    S, B = F.S, F.B
    n = 15
    x0, x1, s, nb, deck, z, yi, ya, yf = _cabin_geom(S, n)
    yo = min(ya, yf)
    xa, xf = _xfaces(S, n)
    xc = (x0 + x1) / 2
    ym = yo - 0.08 - 1.04
    # the aft bunk: head inboard, its ladder toward the middle of the cabin; the fore one: head outboard
    F.put(kit.bunk_bed, xa + 0.52, s * ym, z, STBD, blanket="CT_PlasterGrey")
    F.put(kit.bunk_bed, xf - 0.52, s * ym, z, PORT, blanket="CT_PlasterGrey")
    for k, x, yaw, up in (("a", xa + 0.52, STBD, False), ("b", xf - 0.52, PORT, False),
                          ("c", xa + 0.52, STBD, True), ("d", xf - 0.52, PORT, True)):
        zz = z + (1.46 if up else 0.46)
        F.thing("cabin15_bed_%s" % k, "bed", x, s * ym, zz, yaw, height=1.2 if up else 0.46, rest="private", cabin=n)
        F.spot("berth_15_%s" % k, "bed", x, s * ym, zz, yaw)
    folding_table(F, xc, s * (yo - 0.01), z, PORT, w=0.7, mat=WOODL)
    st = F.put(kit.stool, xc - 0.1, s * (yo - 0.8), z, STBD, mat=WOODL)
    F.seat("cabin15_desk", st, cabin=n)
    F.put(kit.stool, xc + 0.3, s * (yo - 1.7), z, 40.0, mat=WOODL)
    top = z + 0.725
    F.put(kit.cup, xc - 0.15, s * (yo - 0.2), top, 0.0, mat=BLUE, saucer=False)
    F.put(kit.newspaper, xc + 0.12, s * (yo - 0.22), top, 110.0)
    # fore bulkhead inboard: two lockers and the washbasin; aft bulkhead beyond the door's leaf: hooks, a rucksack
    F.put(kit.lockers, xf, s * (yi + 0.5), z, AFT, n=2, mat=GREEN)
    by = yi + 1.3
    washbasin(F, xf, s * by, z, AFT)
    F.put(kit.mirror, xf, s * by, z, AFT, zc=1.6, w=0.36, h=0.45, frame=STEEL)
    F.thing("cabin15_washbasin", "machine", xf - 0.3, s * by, z + 0.95, AFT, what="tap")
    coat_hooks(F, xa, s * (yi + 1.42), z, FWD, n=3, zc=1.65, coat=BLUE)
    rucksack(F, xa + 0.2, s * (yi + 1.42), z, FWD)
    suitcase(F, xa + 0.6, s * (yo - 0.45), z, FWD, mat="CT_PlasterGrey", w=0.55, flat=True)
    loudspeaker(F, xf, s * (yi + 0.5), z, AFT, zc=2.05)
    F.thing("cabin15_radio", "machine", xf - 0.12, s * (yi + 0.5), z + 2.05, AFT, what="loudspeaker")
    pipe_run(F, xc, s * yi, z, STBD, x1 - x0 - 0.2, [(2.2, 0.045, "CT_SteelWhite", 0.02)])
    pipe_run(F, xf, s * (yi + 1.6), z, AFT, 3.0, [(2.2, 0.03, "CT_SteelRed", 0.02)])
    wins = F.windows(x0, x1, 0.5, 5.2, z + 1.0, z + 2.3)
    if wins:
        w0 = min(wins, key=lambda w_: abs(w_[0] - xc))
        fx, fy = _dirv(w0[3])
        F.thing("cabin15_porthole", "machine", w0[0] + fx * 0.4, w0[1] + fy * 0.4, w0[2], w0[3], what="window")
    else:
        F.thing("cabin15_porthole", "machine", x0 + 1.0, s * (yo - 0.3), z + 1.6, PORT, what="window")
    plafond(F, xc, s * (yi + 1.2), "D")
    bx, by_, bz, ky = S.fr(x0 + DOOR_OFF + 0.78, s * yi, z, STBD)
    import _homekit as hk
    hk.switch(B, "cabin15_switch", bx, by_, ky, z=bz, zc=1.3, scope="room")
    F.spot("stand_cabin15", "stand", xc, s * (yi + 1.3), z, STBD)


def _cabins(F):
    styles = {1: "teacher", 2: CABIN2_STYLE, 7: "hero"}
    for n in sorted(CABINS_C):
        _cabin_door(F, n)
        if n in styles:
            _cabin_c(F, n, styles[n])
        else:
            _berth_spots(F, n)
    for n in sorted(CABINS_D):
        _cabin_door(F, n)
        if n == 15:
            _cabin_15(F)
        else:
            _berth_spots(F, n)


def _corridor_c(F):
    S = F.S
    z = S.Z_C
    yw = WALL_C - S.WALL_T / 2
    carpet_runner(F, 32.3, 45.7, 0.0, z, w=0.95)
    # handrails between the doors (a door's hole: DOOR_OFF +- 0.55 from each cabin's aft bulkhead)
    for (x0, x1, s, nb) in CABINS_C.values():
        a, b = x0 + DOOR_OFF + 0.98, min(x1 + 0.1, 45.8)
        handrail(F, a, s * yw, b, s * yw, z, PORT if s > 0 else STBD)
    for k, x in enumerate((33.4, 39.2, 45.0)):
        plafond(F, x, 0.0, "C", blue=(k == 1))
    for i, x in enumerate((32.6, 35.0, 37.5, 40.0, 42.5, 45.2)):
        F.spot("path_c_corr_%d" % (i + 1), "path", x, 0.0, z, FWD)


def _foredeck_aft(F, fittings):
    """The things of the open decks (the fittings themselves are the shell's, _shipkit.FITTINGS gives their places;
    R1's numbers when a fitting is not there)."""
    S = F.S
    z = S.Z_C

    def at(name, default):
        return tuple(fittings.get(name, default))
    bx, by, bz = at("ship_bell", (50.4, 0.0, S.foredeck_z(50.4) + 1.75))
    F.thing("ship_bell", "machine", bx - 0.12, by, bz, AFT, what="bell")
    wx, wy, wz = at("windlass", (55.6, 0.0, S.foredeck_z(55.6) + 0.6))
    F.thing("windlass", "board", wx, wy, wz + 0.2, AFT, topic="windlass")
    rx, ry, rz = at("bow", (60.6, 0.0, S.foredeck_z(60.6)))
    F.thing("bow_rail", "board", rx, ry, rz + 1.05, AFT, topic="bow")
    hx, hy, hz = at("cargo_hatch", (48.8, 0.0, S.foredeck_z(48.3) + 0.6))
    F.thing("cargo_hatch", "door", hx, 1.44, hz - 0.15, STBD)
    cx, cy, cz = at("capstan", (3.0, 0.0, z + 0.5))
    F.thing("capstan", "board", cx, cy, cz + 0.35, FWD, topic="capstan")
    F.spot("staff_bosun", "work", 52.6, 1.5, S.foredeck_z(52.6), AFT)
    for i, (x, sy) in enumerate(((48.5, 1), (48.5, -1), (52.0, 1), (52.0, -1), (55.0, 1), (55.0, -1))):
        y = sy * (S.half(x) - 0.75)
        F.spot("stand_bow_%d" % (i + 1), "stand", x, y, S.foredeck_z(x), STBD if sy > 0 else PORT)
    F.spot("stand_bow_7", "stand", 59.6, 0.0, S.foredeck_z(59.6), FWD)
    for i, (x, y) in enumerate(((46.5, 0.0), (46.5, 2.4), (46.5, -2.4), (50.5, 2.4), (50.5, -2.4), (53.8, 0.0), (58.2, 0.0))):
        F.spot("path_c_bow_%d" % (i + 1), "path", x, y, S.foredeck_z(x), FWD)
    for i, (x, y, yaw) in enumerate(((1.4, 0.0, AFT), (4.5, -3.3, PORT), (5.5, 3.6, STBD))):
        F.spot("stand_aft_%d" % (i + 1), "stand", x, y, z, yaw)
    for i, (x, y) in enumerate(((7.2, 0.0), (5.0, -1.0), (4.6, -2.4), (7.8, 2.15))):
        F.spot("path_c_aft_%d" % (i + 1), "path", x, y, z, AFT)


def shower_room(F):
    """The showers of the lower deck (starboard of the stair core, door at x 31.5-32.6 in the wall y 1.5): three
    stalls along the ship's side behind curtains, washbasins on the stair wall, a bench and hooks by the door."""
    S = F.S
    z = S.Z_D
    xa = 27.0 + 0.06
    xs = [xa + 0.02 + k * 0.95 for k in range(4)]
    yo = min(S.deck_half("D", x) for x in (27.0, 30.0))
    y0 = yo - 1.05
    p = F.pc("shower_stalls", xa, yo, z, PORT)          # local x = toward the stern, front = inboard
    L = xs[-1] - xa
    # in this piece's frame: the stalls run from local x = -L (fore end) to 0 (the aft bulkhead), depth 1.05 toward -y
    p.box(WHITE, -L - 0.03, 0.0, -1.05, 0.0, 0.0, 0.06, f="xXyZ")
    for k in range(4):
        u = -(xs[k] - xa)
        if k:
            p.box(WHITE, u - 0.025, u + 0.025, -1.05, 0.0, 0.06, 1.95, f="xXyZ")
            p.core(u - 0.02, u + 0.02, -1.03, -0.02, 0.1, 1.9)
        if k < 3:
            c = u - 0.475
            p.rod(ALU, (c, -0.03, 1.05), (c, -0.03, 2.0), 0.022, 4)
            p.rod(ALU, (c, -0.03, 2.0), (c, -0.3, 2.05), 0.022, 4)
            p.lathe(ALU, c, -0.32, [(0.07, 1.99), (0.075, 2.0), (0.02, 2.05)], 8, bottom=True)
            for sx, col in ((-0.08, "CT_SteelRed"), (0.08, BLUE)):
                _knob(p, col, c + sx, 1.1, 0.028, -0.03, -0.07, 6)
            p.box(STEEL, c - 0.06, c + 0.06, -0.6, -0.48, 0.06, 0.065, f="Z")
    p.rod(ALU, (-L - 0.03, -1.05, 1.98), (0.0, -1.05, 1.98), 0.022, 4)
    p.done(footprint=(-L - 0.03, 0.0, -1.05, 0.0))
    for k in range(3):                                     # curtains: half drawn, different every stall
        c = (xs[k] + xs[k + 1]) / 2
        drapes(F, c, y0 + 0.07, z + 0.12, PORT, 0.7, 1.86, (BLUE, WHITE, GREEN)[k], open_=(0.6, 0.1, 0.75)[k])
    F.thing("showers", "machine", (xs[1] + xs[2]) / 2, y0 - 0.25, z + 1.3, PORT, what="shower")
    # the stair wall: two washbasins with mirrors; the fore bulkhead: a bench under hooks with towels
    yw = 1.5 + S.WALL_T / 2
    for x in (28.3, 29.4):
        washbasin(F, x, yw, z, STBD)
        F.put(kit.mirror, x, yw, z, STBD, zc=1.55, w=0.4, h=0.5, frame=ALU)
    F.thing("showers_tap", "machine", 28.3, yw + 0.3, z + 0.95, STBD, what="tap")
    xf = 33.0 - 0.06
    b = F.put(kit.bench, xf - 0.26, 3.2, z, AFT, w=1.5, kind="gym", mat=WOODL)
    F.seat("showers_bench", b)
    coat_hooks(F, xf, 3.2, z, AFT, n=4, zc=1.7, coat=LINEN)
    F.put(kit.poster, xf, 1.95, z, AFT, name="poster_health", w=0.4, zc=1.55)
    for i, (x, y) in enumerate(((32.05, 2.1), (30.6, 2.9), (28.6, 2.9))):
        F.spot("path_d_showers_%d" % (i + 1), "path", x, y, z, AFT)


def _lower_deck(F):
    S = F.S
    z = S.Z_D
    # the stair-foot lobby (x 31..33 before the stairs): the baggage room's closed door to port, the showers' doorway
    # to starboard, the door to the hold cabins' corridor forward; a fire point on the bulkhead
    F.thing("baggage_room", "door", 32.05, -1.42, z + 1.1, STBD)
    xb = 33.0 - 0.06
    fire_box(F, xb, 1.05, z, AFT, zc=1.45)
    F.put(kit.fire_extinguisher, xb, -1.05, z, AFT)
    plafond(F, 32.0, 0.0, "D")
    shower_room(F)
    # the corridor of the hold cabins: handrails, a pipe under the deckhead, lamps that glow, the hold door at its end
    F.thing("hold_door", "door", 43.41, 0.0, z + 1.1, AFT)
    yw = WALL_D - S.WALL_T / 2
    for (x0, x1, s, nb) in CABINS_D.values():
        a, b = x0 + DOOR_OFF + 0.98, min(x1 + 0.1, 43.3)
        handrail(F, a, s * yw, b, s * yw, z, PORT if s > 0 else STBD)
    pipe_run(F, 38.25, -yw, z, STBD, 10.0, [(2.2, 0.04, "CT_SteelWhite", 0.02)])
    for k, x in enumerate((34.6, 41.4)):
        plafond(F, x, 0.0, "D", blue=(k == 1))
    for i, x in enumerate((32.0, 33.6, 36.0, 38.5, 41.0, 42.9)):
        F.spot("path_d_corr_%d" % (i + 1), "path", x, 0.0, z, FWD)
    F.spot("stand_d_lobby", "stand", 32.2, -0.7, z, AFT)
    # crew quarters (x 8..18, a closed box behind the engine room: no way in for the hero): 16 berths in four cabins
    # of four along the sides, the mess in the middle with eight seats
    k = 0
    for (cx, s) in ((9.6, 1), (9.6, -1), (13.2, 1), (13.2, -1)):
        for (dx, up) in ((-0.8, False), (0.8, False), (-0.8, True), (0.8, True)):
            k += 1
            F.spot("crew_berth_%d" % k, "bed", cx + dx, s * 3.1, z + (1.46 if up else 0.46), STBD if s > 0 else PORT)
    for i in range(8):
        s = 1 if i < 4 else -1
        F.spot("crew_mess_%d" % (i + 1), "seat", 15.4 + (i % 4) * 0.6, s * 0.75, z, PORT if s > 0 else STBD)
    for i, (x, y) in enumerate(((17.1, -3.6), (16.0, -2.0), (12.0, 0.0), (9.0, 0.0))):
        F.spot("path_d_crew_%d" % (i + 1), "path", x, y, z, AFT)


# ================================================================================================ entry
def furnish_cd(B, S, fittings=None):
    """Furnish and declare decks D and C. fittings: {name: (x, y, z)} ship positions of the shell's deck fittings
    (ship_bell, windlass, bow_rail, cargo_hatch, capstan, gangway); default S.FITTINGS when the shell publishes it."""
    F = _F(B, S)
    try:
        import _shipkit
        shell = dict(_shipkit.FITTINGS)
    except ImportError:
        shell = {}
    fittings = dict(shell, **(fittings or {}))
    for fn in (_dining_saloon, _servery_galley, _stbd_corridor, _engine_room, _vestibule, _corridor_c, _cabins,
               lambda F_: _foredeck_aft(F_, fittings), _lower_deck, lamp_mounts):
        t0 = B.detail.tri_count()
        fn(F)
        F.count[getattr(fn, "__name__", "decks")] = B.detail.tri_count() - t0
    # the gangway (shell door, starboard, x 30.4..31.8): the link to the pier's launch
    gx, gy = fittings.get("gangway", (31.1, 4.4, S.Z_C))[:2]
    if not any(t["id"] == "gangway" for t in B.things):
        F.thing("gangway", "door", gx, gy, S.Z_C + 1.1, PORT if gy > 0 else STBD, to="P-PIER/launch_gate")
    # the launch to the Far Light (L1: the landing on Saturday; the way back is P-FARLIGHT/jetty_gate): beside the gangway
    if not any(t["id"] == "launch_ashore" for t in B.things):
        F.thing("launch_ashore", "door", gx + 1.5, gy, S.Z_C + 1.1, PORT if gy > 0 else STBD, to="P-FARLIGHT/jetty_gate")
    # named points of ULyudCruise::GetShipPoint that lie on these decks
    have = {sp["id"] for sp in B.spots}
    if "gangway" not in have:
        F.spot("gangway", "mark", gx, gy - 0.5 * (1 if gy > 0 else -1), S.Z_C, PORT if gy > 0 else STBD)
    if "bow" not in have:
        F.spot("bow", "mark", 60.6, 0.0, S.foredeck_z(60.6), FWD)
    if os.environ.get("SHIP_W2_PREVIEW_LAMPS"):
        # look-dev only (shoot_interior traces shadows, the game's lamps cast none): light the engine well and the
        # galley the way the game's shadowless lamps do. Never set in a real build.
        S.lamp(B, (23.5, -0.4, S.Z_C + 1.6), lumens=450.0, radius=8.0, fixture=False)
        S.lamp(B, (22.5, -0.2, S.Z_D + 2.6), lumens=450.0, radius=7.0, fixture=False)
        S.lamp(B, (23.6, -3.2, S.ceil("C") - 0.3), lumens=700.0, radius=6.0, fixture=False)
    B.note("decks D and C furnished: %d triangles of furniture (%s)" % (
        B.detail.tri_count() - F.tris0, ", ".join("%s %d" % (k.strip("_"), v) for k, v in F.count.items())))
    return F
