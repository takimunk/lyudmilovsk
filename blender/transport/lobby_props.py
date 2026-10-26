"""The station lobbies' signature props and the universal vending machine (docs/transport.md "Lobbies"): our own clean
low-poly models in the language of furniture.py (Dima's rule: enterable and interactive things are modelled by us,
metre UVs, boxes and lathes). SM_TR_Lobby_<Name>.glb in design/transport/game.

  Blender -b -P blender/transport/lobby_props.py [-- Name ...]      (then pack.py, unreal/scripts/import_transport.sh)

Everything stands on the floor at its middle, its back to -X, facing +X (like furniture.py), unless it hangs (origin at
its hinge). Material slots the game recolours (ALyudLobby): TR_Skin (a machine's paint), TR_Brass, TR_Wood,
TR_Glow_Amber / _Fire / _Aqua (emissive colours), TR_Fish (a fish's tint); the rest are the kit's own.

  Vending      the universal machine: 1.0 m wide, front at x=+0.375 (AVendingMachine's FlapLocal x=37.5), a glass window with
               three shelves (0.56, 0.94, 1.32 m: AVendingMachine::StockShelvesUniversal), the delivery flap at y=+0.10,
               z 0.08..0.34, nine buttons and a display on the right, a lit header
  CoffeeBar    a counter with an espresso machine, cups, a canopy with three pendant lamps, three bar stools (Aton)
  Chandelier   a sputnik chandelier: hub, 24 radial rods with glowing globes; origin at the hub (Zenit); Rod: 1 m hanger
  PendulumBob  the Foucault pendulum's bob (origin at the wire's lower end, hangs down) (Akademgorodok); Plate: the floor ring
  Stove        a tiled stove with a cast-iron door and a glowing fire, a log basket, a brass samovar on a stand (Polarnaya)
  BeaconLens   a lighthouse lens: a drum of prism rings round a lamp, hung from its top (Mayak)
  AmberCase    a glass vitrine of amber on a lit honeycomb of amber glass (Yantarny bereg)
  Aquarium     a long tank on a cabinet, sand, rocks, a light hood (Gorizont); Fish: a perch, 12 cm, origin at its middle, nose +X
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import furniture as F
from furniture import box, tube, lathe, sphere

OUT = F.OUT
MATS = F.MATS + ["TR_Skin", "TR_Brass", "TR_Wood", "TR_Glow_Amber", "TR_Glow_Fire", "TR_Glow_Aqua", "TR_Fish"]
COL = dict(F.COL)
COL.update({"TR_Skin": (0.7, 0.2, 0.15), "TR_Brass": (0.8, 0.6, 0.2), "TR_Wood": (0.4, 0.25, 0.12),
            "TR_Glow_Amber": (1, 0.6, 0.15), "TR_Glow_Fire": (1, 0.35, 0.05), "TR_Glow_Aqua": (0.4, 0.8, 1), "TR_Fish": (0.7, 0.7, 0.75)})
EN, AM, CH, TZ, SEAT, DK, MO, GLOW, POSTER, GLS, PLATE, STEEL = range(12)
SKIN, BRASS, WOOD, GAMB, GFIRE, GAQUA, FISH = range(12, 19)


def ring(bm, r, z, t, mi, seg=24, h=0.03):
    """A flat ring of outer radius r, thickness t, height h at z."""
    lathe(bm, [(r - t, z), (r, z), (r, z + h), (r - t, z + h)], [mi, mi, mi], seg, cap0=False, cap1=False)


def vending():
    bm = bmesh.new()
    xb, xf = -0.425, 0.375                       # back, front
    D = xf - xb
    box(bm, (xb + D / 2, 0, 0.04), (D, 1.0, 0.08), DK)                                       # plinth
    # the front: panels round the window (y -0.46..0.28, z 0.5..1.78), the flap recess (y -0.10..0.30, z 0.08..0.34), the panel right
    box(bm, (xb + 0.02, 0, 1.0), (0.04, 1.0, 1.9), SKIN)                                     # back
    for sy in (-1, 1):
        box(bm, (xb + D / 2, sy * 0.49, 0.99), (D, 0.02, 1.82), SKIN)                       # sides
    box(bm, (xb + D / 2, 0, 1.91), (D, 1.0, 0.04), SKIN)                                     # top
    box(bm, (xf - 0.02, -0.48 + 0.04, 0.99), (0.04, 0.08, 1.82), SKIN)                      # the left post (y -0.48..-0.40)
    box(bm, (xf - 0.02, 0.29 + 0.01, 1.14), (0.04, 0.02, 1.28), CH)                          # chrome strip between window and panel
    box(bm, (xf - 0.02, 0.39, 1.14), (0.04, 0.2, 1.28), SKIN)                                # the right panel (y 0.29..0.49)
    box(bm, (xf - 0.02, -0.06, 0.45), (0.04, 0.84, 0.1), SKIN)                               # under the window (z 0.40..0.50)
    box(bm, (xf - 0.02, -0.06, 1.82), (0.04, 0.84, 0.08), SKIN)                              # over the window
    box(bm, (xf - 0.02, -0.28, 0.24), (0.04, 0.36, 0.32), SKIN)                              # left of the flap (y -0.46..-0.10)
    box(bm, (xf - 0.02, 0.39, 0.24), (0.04, 0.2, 0.32), SKIN)                                # right of the flap
    box(bm, (xf - 0.02, -0.06, 0.34 + 0.03), (0.04, 0.84, 0.06), SKIN)                       # over the flap, under the window's panel: z 0.34..0.40
    # inside the window: a dark back, three shelves of glass-and-steel, a lit strip over each
    box(bm, (xb + 0.06, -0.09, 1.14), (0.02, 0.74, 1.28), DK)
    for z in (0.555, 0.935, 1.315):
        box(bm, (0.06, -0.09, z), (0.72, 0.72, 0.02), STEEL)
        box(bm, (0.40 - 0.07, -0.09, z + 0.01), (0.012, 0.72, 0.015), CH)                    # the shelf's front lip
    for z in (0.93, 1.31, 1.75):
        box(bm, (0.19, -0.09, z), (0.05, 0.7, 0.012), GLOW)                                  # light strips under the shelves above
    box(bm, (xf - 0.005, -0.09, 1.14), (0.006, 0.74, 1.28), GLS)                             # the glass
    # the flap: a recess behind a swing door of smoked plastic, the tray is the code's
    box(bm, (xf - 0.1, 0.10, 0.21), (0.2, 0.4, 0.26), DK)
    box(bm, (xf - 0.015, 0.10, 0.215), (0.02, 0.4, 0.26), GLS)
    # the panel: nine buttons in three rows, a coin slot, a display
    for i in range(9):
        r, c = i // 3, i % 3
        box(bm, (xf + 0.006, 0.325 + c * 0.065, 1.55 - r * 0.075), (0.016, 0.045, 0.045), CH)
    box(bm, (xf + 0.004, 0.39, 1.72), (0.012, 0.16, 0.06), GLOW)                              # the display
    box(bm, (xf + 0.004, 0.39, 1.1), (0.012, 0.12, 0.2), DK)                                  # coin plate
    box(bm, (xf + 0.01, 0.39, 1.14), (0.012, 0.012, 0.09), CH)                                # the slot
    # the header: a lit box over the window
    box(bm, (xf - 0.03, 0.0, 1.855), (0.06, 0.9, 0.09), GLOW)
    for sy in (-1, 1):
        box(bm, (xf - 0.03, sy * 0.455, 1.855), (0.07, 0.02, 0.1), CH)
    return bm


def coffee_bar():
    bm = bmesh.new()
    W = 2.6
    # the counter: a panelled amber front, a terrazzo top with an overhang, a steel foot rail
    box(bm, (-0.05, 0, 0.51), (0.55, W, 1.0), WOOD)
    box(bm, (0.24, 0, 0.5), (0.02, W - 0.1, 0.9), AM)
    for k in range(-5, 6):
        box(bm, (0.255, k * 0.23, 0.5), (0.012, 0.012, 0.9), CH)                                # fluting
    box(bm, (0.0, 0, 1.03), (0.72, W + 0.06, 0.05), TZ)
    tube(bm, (0.38, -W / 2 + 0.1, 0.12), (0.38, W / 2 - 0.1, 0.12), 0.022, CH)
    for y in (-W / 2 + 0.1, W / 2 - 0.1):
        tube(bm, (0.38, y, 0.12), (0.2, y, 0.12), 0.015, CH, 6)
    # the espresso machine at the left end: a chrome body, boiler dome, two group heads with portafilters, a steam wand
    bx, by, bz = -0.1, -0.7, 1.055
    box(bm, (bx, by, bz + 0.17), (0.42, 0.62, 0.34), CH)
    box(bm, (bx, by, bz + 0.37), (0.4, 0.6, 0.06), DK)
    lathe(bm, [(0.0, 0.0), (0.12, 0.0), (0.14, 0.05), (0.1, 0.14), (0.0, 0.16)], [CH, CH, CH, CH], 16, (bx - 0.05, by, bz + 0.4))
    for dy in (-0.15, 0.15):
        box(bm, (bx + 0.22, by + dy, bz + 0.18), (0.06, 0.1, 0.06), DK)
        tube(bm, (bx + 0.25, by + dy, bz + 0.17), (bx + 0.4, by + dy, bz + 0.2), 0.02, DK, 8)
        sphere(bm, (bx + 0.41, by + dy, bz + 0.2), 0.03, WOOD, 8, 4)
    tube(bm, (bx + 0.2, by + 0.28, bz + 0.25), (bx + 0.3, by + 0.28, bz + 0.05), 0.008, CH, 6)  # the steam wand
    box(bm, (bx + 0.22, by, bz + 0.03), (0.1, 0.3, 0.015), DK)                                # the drip tray
    # cups: a stack and a few on a saucer row
    for i in range(5):
        lathe(bm, [(0.035, 0.0), (0.045, 0.01), (0.05, 0.08)], [EN, EN, EN], 10, (0.05, 0.2 + i * 0.12, bz), cap1=False)
    lathe(bm, [(0.04, 0.0), (0.05, 0.12), (0.045, 0.12)], [EN, EN, EN], 10, (0.0, 0.62, bz))
    for i in range(3):
        lathe(bm, [(0.035, 0.0), (0.04, 0.02), (0.05, 0.075)], [EN, EN, EN], 10, (0.05, 0.62, bz + 0.12 + i * 0.07), cap1=False)
    # the back: a menu board (dark, amber rim, a coffee cup's emblem in glowing lines) and a shelf with jars
    box(bm, (-0.43, 0.2, 1.75), (0.03, 1.5, 0.55), DK)
    for sz in (-1, 1):
        box(bm, (-0.41, 0.2, 1.75 + sz * 0.29), (0.04, 1.56, 0.03), AM)
    for sy in (-1, 1):
        box(bm, (-0.41, 0.2 + sy * 0.77, 1.75), (0.04, 0.03, 0.61), AM)
    for k in range(6):
        box(bm, (-0.405, -0.35 + k * 0.28, 1.78), (0.01, 0.2, 0.012), GAMB)
        box(bm, (-0.405, -0.35 + k * 0.28, 1.66), (0.01, 0.12, 0.012), GAMB)
    box(bm, (-0.38, 0.2, 1.38), (0.15, 1.6, 0.025), WOOD)
    for k in range(6):
        lathe(bm, [(0.05, 0.0), (0.055, 0.05), (0.055, 0.15), (0.03, 0.16), (0.03, 0.18)], [GLS, GLS, GLS, BRASS], 10, (-0.38, -0.4 + k * 0.28, 1.395))
    # the canopy: two posts and a beam at 2.4 m, three pendant lamps
    for y in (-W / 2 + 0.03, W / 2 - 0.03):
        tube(bm, (0.2, y, 1.06), (0.2, y, 2.42), 0.02, CH, 8)
    tube(bm, (0.2, -W / 2 + 0.03, 2.42), (0.2, W / 2 - 0.03, 2.42), 0.025, CH, 8)
    for y in (-0.7, 0.0, 0.7):
        tube(bm, (0.2, y, 2.42), (0.2, y, 2.0), 0.006, CH, 5)
        lathe(bm, [(0.0, 0.0), (0.07, 0.04), (0.18, 0.16), (0.19, 0.17)], [AM, AM, AM], 16, (0.2, y, 1.84), cap0=True, cap1=False)
        sphere(bm, (0.2, y, 1.93), 0.065, GLOW, 12, 6)
    # three bar stools in front: a chrome pole, an amber seat, a foot ring
    for y in (-0.8, 0.0, 0.8):
        sx = 0.7
        lathe(bm, [(0.2, 0.0), (0.2, 0.025), (0.03, 0.04)], [CH, CH, CH], 14, (sx, y, 0))
        tube(bm, (sx, y, 0.03), (sx, y, 0.72), 0.022, CH, 8)
        ring(bm, 0.2, 0.28, 0.015, CH, 16, 0.012)
        lathe(bm, [(0.0, 0.72), (0.17, 0.72), (0.18, 0.76), (0.15, 0.8), (0.0, 0.8)], [SEAT, SEAT, SEAT, SEAT], 16, (sx, y, 0.0), cap0=False, cap1=False)
        lathe(bm, [(0.17, 0.72), (0.0, 0.72)], [CH], 16, (sx, y, 0.0), cap0=False, cap1=False)
    return bm


def chandelier():
    """A sputnik: hub at the origin, 24 rods in rings of 8 (up, level and down), each ending in a glowing globe."""
    bm = bmesh.new()
    sphere(bm, (0, 0, 0), 0.17, CH, 16, 8)
    for k, el in enumerate((50, 15, -25)):
        for i in range(8):
            az = math.radians(i * 45 + k * 15)
            e = math.radians(el)
            L = 0.75 + 0.18 * (k % 2) + (0.1 if el > 0 else 0)
            d = Vector((math.cos(e) * math.cos(az), math.cos(e) * math.sin(az), math.sin(e)))
            tube(bm, d * 0.15, d * L, 0.009, CH, 6)
            sphere(bm, d * (L + 0.02), 0.06, GLOW, 8, 4)
            # a small chrome star cap where the rod leaves the hub
            sphere(bm, d * 0.19, 0.025, CH, 6, 3)
    tube(bm, (0, 0, 0.1), (0, 0, 0.3), 0.04, CH, 10)
    return bm


def rod():
    bm = bmesh.new()
    tube(bm, (0, 0, 0), (0, 0, 1.0), 0.012, CH, 8)
    return bm


def pendulum_bob():
    """Origin at the top (the wire's end); the bob hangs below: a brass sphere 0.2 m on a short collar, a point at the bottom."""
    bm = bmesh.new()
    lathe(bm, [(0.0, 0.0), (0.03, 0.0), (0.03, -0.1), (0.1, -0.16)], [CH, CH, BRASS], 12, cap0=True, cap1=False)
    sphere(bm, (0, 0, -0.34), 0.2, BRASS, 24, 12)
    tube(bm, (0, 0, -0.52), (0, 0, -0.62), 0.012, CH, 8)
    lathe(bm, [(0.012, -0.62), (0.0, -0.66)], [CH], 8, cap0=False, cap1=False)
    return bm


def pendulum_plate():
    """A floor ring 3.0 m across with a brass compass scale and 48 pegs (the swing knocks them over in the real thing)."""
    bm = bmesh.new()
    lathe(bm, [(1.55, 0.0), (1.55, 0.02), (1.38, 0.02)], [DK, BRASS, BRASS], 48, cap0=False, cap1=False)
    for i in range(48):
        a = math.radians(i * 7.5)
        tube(bm, (1.46 * math.cos(a), 1.46 * math.sin(a), 0.02), (1.46 * math.cos(a), 1.46 * math.sin(a), 0.1 if i % 6 else 0.17), 0.012 if i % 6 else 0.018, BRASS, 6)
    return bm


def stove():
    bm = bmesh.new()
    # the tiled body, 0.8 x 0.8 x 2.0 with a cornice and a plinth, tile joints as thin dark lines
    box(bm, (-0.1, -0.35, 0.05), (0.8, 0.9, 0.1), DK)
    box(bm, (-0.1, -0.35, 1.05), (0.7, 0.8, 1.9), EN)
    box(bm, (-0.1, -0.35, 2.0), (0.78, 0.88, 0.06), AM)
    for z in range(1, 10):
        box(bm, (0.26, -0.35, 0.1 + z * 0.2), (0.004, 0.8, 0.006), DK)
    for y in range(-3, 4):
        box(bm, (0.26, -0.35 + y * 0.115, 1.05), (0.004, 0.006, 1.9), DK)
    # the firebox door: a cast-iron frame, a glowing window, a brass handle, the ash door below
    box(bm, (0.265, -0.35, 0.75), (0.03, 0.4, 0.34), DK)
    box(bm, (0.285, -0.35, 0.75), (0.01, 0.3, 0.24), GFIRE)
    tube(bm, (0.3, -0.5, 0.58), (0.3, -0.2, 0.58), 0.008, BRASS, 6)
    box(bm, (0.265, -0.35, 0.3), (0.03, 0.4, 0.2), DK)
    for i in range(4):
        box(bm, (0.285, -0.35, 0.26 + i * 0.04), (0.01, 0.34, 0.012), STEEL)
    # a brass flue pipe up to the ceiling's height of 2.6 m (the hall's higher than that; the game stretches the next part)
    tube(bm, (-0.1, -0.35, 2.03), (-0.1, -0.35, 2.6), 0.07, STEEL, 12)
    # beside it: a wooden log basket, with a heap of logs
    lathe(bm, [(0.26, 0.0), (0.28, 0.35), (0.26, 0.36)], [WOOD, WOOD, WOOD], 14, (0.1, 0.3, 0.0), cap1=False)
    for i in range(7):
        a = i * 0.9
        tube(bm, (0.1 + 0.1 * math.cos(a), 0.3 + 0.1 * math.sin(a), 0.3 + 0.03 * i % 0.08), (0.1 - 0.1 * math.cos(a + 1), 0.3 - 0.1 * math.sin(a + 1), 0.42), 0.04, WOOD, 8)
    # the samovar on a low stand: brass body, a tap, a lid with a small kettle, two glass holders beside it
    sx, sy = 0.2, 0.95
    box(bm, (sx, sy, 0.45), (0.5, 0.5, 0.04), WOOD)
    for dx, dy in ((-0.2, -0.2), (0.2, -0.2), (-0.2, 0.2), (0.2, 0.2)):
        tube(bm, (sx + dx, sy + dy, 0.0), (sx + dx, sy + dy, 0.45), 0.025, WOOD, 8)
    lathe(bm, [(0.0, 0.0), (0.12, 0.0), (0.2, 0.08), (0.21, 0.3), (0.17, 0.42), (0.1, 0.46), (0.07, 0.5)], [BRASS] * 6, 20, (sx, sy, 0.47), cap0=True, cap1=False)
    tube(bm, (sx + 0.19, sy, 0.62), (sx + 0.29, sy, 0.6), 0.012, BRASS, 6)
    sphere(bm, (sx + 0.3, sy, 0.6), 0.025, GFIRE, 6, 3)
    lathe(bm, [(0.07, 0.97), (0.11, 0.99), (0.08, 1.07)], [BRASS, BRASS, BRASS], 12, (sx, sy, 0.0), cap1=True)
    for dy in (-0.17, 0.17):
    # a low bench beside the samovar table is the hall's own (furniture)
    return bm


def beacon_lens():
    """Hung by its hanger: origin at the top of the hanger. A drum 0.55 m across and 1.3 m tall of prism rings round a lamp,
    caps top and bottom, so that when it turns, its light sweeps."""
    bm = bmesh.new()
    tube(bm, (0, 0, 0), (0, 0, -0.5), 0.03, CH, 10)
    lathe(bm, [(0.0, -0.5), (0.3, -0.5), (0.34, -0.56), (0.3, -0.62)], [BRASS, BRASS, BRASS], 24, cap0=True, cap1=False)
    # the drum's prism rings: a glass lens between z -0.62 and -1.7, banded
    for k in range(9):
        z = -0.62 - k * 0.12
        lathe(bm, [(0.3, z), (0.34, z - 0.04), (0.34, z - 0.08), (0.3, z - 0.12)], [GLS, GLS, GLS], 24, cap0=False, cap1=False)
        ring(bm, 0.355, z - 0.12, 0.03, BRASS, 24, 0.012)
    sphere(bm, (0, 0, -1.2), 0.11, GAMB, 14, 7)                                   # the lamp
    lathe(bm, [(0.3, -1.7), (0.34, -1.7), (0.3, -1.78), (0.0, -1.8)], [BRASS, BRASS, BRASS], 24, cap0=False, cap1=True)
    # four braces holding the lens apart (the lamp's frame)
    for i in range(4):
        a = math.radians(45 + 90 * i)
        tube(bm, (0.3 * math.cos(a), 0.3 * math.sin(a), -0.62), (0.3 * math.cos(a), 0.3 * math.sin(a), -1.7), 0.012, BRASS, 6)
    return bm


def amber_case():
    bm = bmesh.new()
    W, Dp, H = 1.9, 0.55, 1.95
    # the lit honeycomb of amber glass behind (hexagons made of boxes turned 0/60/120), a base cabinet, a steel frame, glass
    box(bm, (-0.25, 0, 0.45), (0.5, W, 0.9), WOOD)
    box(bm, (-0.01, 0, 0.45), (0.02, W - 0.1, 0.8), AM)
    box(bm, (-0.25, 0, 0.92), (0.54, W + 0.04, 0.04), TZ)
    box(bm, (-0.46, 0, 1.45), (0.04, W, 1.1), DK)                                                    # the back wall
    cell = 0.2
    for r in range(6):
        for c in range(9):
            y = -W / 2 + 0.12 + c * cell * 0.87 + (0.087 if r % 2 else 0)
            z = 0.96 + r * cell * 0.75 + 0.06
            if abs(y) < W / 2 - 0.08:
                for a in (0, 60, 120):
                    box(bm, (-0.43, y, z), (0.01, 0.17, 0.045), GAMB if (r + c) % 3 else AM, Matrix.Rotation(math.radians(a), 3, "X"))
    # the amber on shelves: raw lumps, a polished egg, a necklace on a stand, and the great chunk with an insect
    for z in (1.18, 1.52):
        box(bm, (-0.1, 0, z), (0.32, W - 0.15, 0.02), GLS)
    rnd = [(-0.7, 0.08, 1.2), (-0.25, 0.1, 1.2), (0.3, 0.07, 1.2), (0.65, 0.09, 1.2), (-0.5, 0.06, 1.54), (0.1, 0.08, 1.54), (0.55, 0.06, 1.54)]
    for y, r, z in rnd:
        sphere(bm, (-0.1, y, z + 0.01 + r), r, GAMB, 10, 5)
    lathe(bm, [(0.0, 0.0), (0.12, 0.0), (0.18, 0.12), (0.1, 0.26), (0.0, 0.28)], [GAMB] * 4, 10, (-0.1, -0.1, 0.94), cap0=True, cap1=False)   # the great chunk on the cabinet
    box(bm, (0.0, 0.5, 0.98), (0.14, 0.14, 0.06), BRASS)
    # a frame of steel posts, a glass front and glass top
    for y in (-W / 2, W / 2):
        for x in (-0.46, 0.0):
            tube(bm, (x, y, 0.9), (x, y, H), 0.02, STEEL, 8)
    box(bm, (0.0, 0, H), (0.5, W + 0.04, 0.03), STEEL)
    box(bm, (0.0, 0, 0.9), (0.03, W, 0.03), STEEL)
    box(bm, (0.0, 0, 1.42), (0.008, W - 0.06, 1.0), GLS)
    for y in (-W / 2, W / 2):
        box(bm, (-0.23, y, 1.42), (0.5, 0.008, 1.0), GLS)
    box(bm, (-0.23, 0, H - 0.01), (0.46, W, 0.008), GAMB)                                              # the lit ceiling
    return bm


def aquarium():
    bm = bmesh.new()
    W, Dp = 2.3, 0.65
    # the cabinet (dark, a brass trim), the tank (glass on three sides, the back blue-green), sand, rocks, a hood with light
    box(bm, (0, 0, 0.4), (Dp, W, 0.8), DK)
    box(bm, (0.0, 0, 0.8), (Dp + 0.04, W + 0.04, 0.04), BRASS)
    for y in (-W / 2 + 0.1, W / 2 - 0.1):
        box(bm, (Dp / 2 - 0.01, y, 0.4), (0.02, 0.5, 0.65), AM)
    box(bm, (-0.3, 0, 1.35), (0.04, W - 0.06, 0.95), GAQUA)                                         # the back: a lit blue panel
    box(bm, (0.0, 0, 0.9), (Dp - 0.1, W - 0.06, 0.12), PLATE)                                       # sand
    box(bm, (0.0, 0, 1.85), (Dp, W, 0.1), DK)                                                         # the hood
    box(bm, (0.12, 0, 1.79), (0.3, W - 0.2, 0.025), GLOW)
    for y in (-W / 2 + 0.02, W / 2 - 0.02):
        box(bm, (0.0, y, 1.35), (Dp - 0.02, 0.04, 0.95), GLS)
    box(bm, (Dp / 2 - 0.02, 0, 1.35), (0.008, W - 0.06, 0.95), GLS)
    for y, r in ((-0.7, 0.16), (-0.35, 0.11), (0.1, 0.18), (0.6, 0.13), (0.85, 0.09)):
        sphere(bm, (0.02, y, 0.96 + r * 0.5), r, STEEL, 8, 4)
    # a sunken ship's wheel, a ring of brass with spokes, in the sand
    for i in range(8):
        a = math.radians(i * 45)
        tube(bm, (0.05, -0.3, 1.1), (0.05, -0.3 + 0.14 * math.cos(a), 1.1 + 0.14 * math.sin(a)), 0.008, BRASS, 5)
    return bm


def fish():
    """A perch: a flattened body of 6 rings from the nose (+X) to the tail, dorsal fin, two tail lobes. 12 cm. Tinted by TR_Fish."""
    bm = bmesh.new()
    prof = [(0.0, 0.0, 0.0), (0.02, 0.014, 0.012), (0.045, 0.026, 0.022), (0.0, 0.03, 0.02), (-0.04, 0.022, 0.015), (-0.075, 0.009, 0.008), (-0.09, 0.003, 0.003)]
    # profile entries: x, half-width, half-height; built as rings of 8 around the x axis
    rings = []
    for x, w, h in [(0.06, 0.0005, 0.0005), (0.05, 0.012, 0.012), (0.025, 0.022, 0.026), (0.0, 0.025, 0.03), (-0.03, 0.02, 0.024), (-0.06, 0.01, 0.012), (-0.075, 0.004, 0.005)]:
        rings.append([bm.verts.new((x, w * math.cos(2 * math.pi * i / 8), h * math.sin(2 * math.pi * i / 8))) for i in range(8)])
    for k in range(len(rings) - 1):
        for i in range(8):
            j = (i + 1) % 8
            bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i])).material_index = FISH
    bm.faces.new(rings[-1]).material_index = FISH
    bm.faces.new(rings[0][::-1]).material_index = FISH
    def tri(a, b, c, mi=AM):
        bm.faces.new([bm.verts.new(v) for v in (a, b, c)]).material_index = mi
        bm.faces.new([bm.verts.new(v) for v in (c, b, a)]).material_index = mi
    tri((0.02, 0, 0.026), (-0.03, 0, 0.026), (-0.02, 0, 0.055))                      # dorsal fin
    tri((-0.07, 0, 0.0), (-0.115, 0.0, 0.035), (-0.1, 0.0, 0.0))                     # tail, upper lobe
    tri((-0.07, 0, 0.0), (-0.1, 0.0, 0.0), (-0.115, 0.0, -0.035))                    # lower lobe
    tri((0.0, 0.02, -0.02), (-0.02, 0.03, -0.035), (-0.03, 0.02, -0.015))            # pectoral fins
    tri((0.0, -0.02, -0.02), (-0.03, -0.02, -0.015), (-0.02, -0.03, -0.035))
    return bm


PROPS = [("Vending", vending), ("CoffeeBar", coffee_bar), ("Chandelier", chandelier), ("ChandelierRod", rod),
         ("PendulumBob", pendulum_bob), ("PendulumPlate", pendulum_plate), ("Stove", stove), ("BeaconLens", beacon_lens),
         ("AmberCase", amber_case), ("Aquarium", aquarium), ("Fish", fish)]


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    want = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    os.makedirs(OUT, exist_ok=True)
    for name, f in PROPS:
        if want and name not in want:
            continue
        F.export(f(), "SM_TR_Lobby_" + name, MATS, COL, OUT)


if __name__ == "__main__":
    main()
