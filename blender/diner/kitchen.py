"""The diner's kitchen annex fittings and its lamps (docs/diner.md): the range and its hood, the sink, the fridge, the shelves,
the worktable; the opal globes along the car's crown, the kitchen's lights and the gooseneck lamp over the street door.

  build(parts) adds the geometry to parts["inside"] (big pieces: collision) and parts["small"] (lamps, small things: no
  collision) and returns {"lights": [...]} (point lights ALyudDiner makes: at, lumens, colour, kind inside|kitchen|outside).
"""
import math
import os
import sys

import bmesh  # noqa: F401
from mathutils import Matrix, Vector  # noqa: F401

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import diner_common as C  # noqa: E402,F401
from diner_common import M, bx, box, rbox, tube, lathe, sphere  # noqa: E402,F401
from plan import *  # noqa: E402,F401,F403
from building_util import oq, ybox  # noqa: E402,F401


def kitchen(bm):
    """Stove, sink, worktable, shelves and a fridge in the annex, clear of both doors' swing zones."""
    xi0, xi1, yi0, yi1 = AX0 + ATH, AX1 - ATH, YB, AY1 - ATH
    # the range against the east wall, a hood over it
    bx(bm, M("DN_Dark"), xi1 - 0.7, xi1, 3.0, 4.5, 0.0, 0.9, faces="xyYZ")
    for i in range(2):
        for j in range(2):
            lathe(bm, [(0.11, 0.9), (0.11, 0.92), (0.0, 0.92)], M("DN_Steel"), seg=12, c=(xi1 - 0.5 + i * 0.3, 3.4 + j * 0.65, 0), cap0=False)
    bx(bm, M("DN_Steel"), xi1 - 0.75, xi1, 2.95, 4.55, 1.85, 2.1, faces="xyYz")
    # the sink and the fridge along the back wall, west of the back door's clear zone
    bx(bm, M("DN_Steel"), xi0, xi0 + 1.2, yi1 - 0.6, yi1, 0.0, 0.9, faces="XyZ")
    bx(bm, M("DN_Dark"), xi0 + 0.35, xi0 + 0.85, yi1 - 0.5, yi1 - 0.1, 0.9, 0.92, faces="xXyYZ")
    bx(bm, M("DN_EnamelCream"), xi0 + 1.3, xi0 + 2.0, yi1 - 0.7, yi1, 0.0, 1.8, faces="xXyZ")
    tube(bm, (xi0 + 1.38, yi1 - 0.73, 1.0), (xi0 + 1.38, yi1 - 0.73, 1.5), 0.012, M("DN_Chrome"), seg=6)
    # shelves on the west wall
    for z in (1.0, 1.4, 1.8):
        bx(bm, M("DN_Steel"), xi0, xi0 + 0.35, 3.9, 5.3, z, z + 0.02, faces="XyYzZ")
    for k in range(5):
        lathe(bm, [(0.09, 1.02), (0.09, 1.2), (0.0, 1.2)], M("DN_Steel"), seg=12, c=(xi0 + 0.18, 4.05 + k * 0.28, 0), cap0=False)
    # a worktable in the middle
    bx(bm, M("DN_Steel"), -5.0, -3.9, 4.0, 4.7, 0.86, 0.9, faces="xXyYzZ")
    for x in (-4.95, -3.95):
        for y in (4.05, 4.65):
            tube(bm, (x, y, 0.0), (x, y, 0.86), 0.02, M("DN_Steel"), seg=6, caps=False)


def lamps(bm):
    """Opal globes on chrome stems along the crown, one in the nose, two in the kitchen, one over the door outside."""
    out = []
    for x in (-6.0, -3.6, -1.2, 1.2, 3.6, 6.0, XE + 1.2):
        top = CROWN_IN - 0.01 if x <= XE else EAVE_IN + (CROWN_IN - EAVE_IN) * math.sqrt(max(0.0, 1 - ((x - XE) / (R - TH)) ** 2)) - 0.01
        tube(bm, (x, 0, top), (x, 0, top - 0.25), 0.01, M("DN_Chrome"), seg=6, caps=False)
        lathe(bm, [(0.0, top - 0.24), (0.06, top - 0.25), (0.07, top - 0.28), (0.0, top - 0.285)], M("DN_Chrome"), seg=14, c=(x, 0, 0), cap0=False)
        sphere(bm, (x, 0, top - 0.4), 0.13, M("DN_Opal"), seg=16, rings=8)
        out.append({"at": [x, 0.0, top - 0.4], "lumens": 900, "colour": "#FFE2B8", "kind": "inside"})
    for x, y in ((-5.8, 3.9), (-3.5, 4.9)):
        z = A_TOP - 0.12
        bx(bm, M("DN_Opal"), x - 0.5, x + 0.5, y - 0.1, y + 0.1, z - 0.06, z, faces="xXyYz")
        out.append({"at": [x, y, z - 0.2], "lumens": 1100, "colour": "#F4F1EA", "kind": "kitchen"})
    # the gooseneck lamp over the front door (outside)
    tube(bm, (DOOR_X, YF + 0.0, 2.32), (DOOR_X, YF - 0.3, 2.42), 0.018, M("DN_Chrome"), seg=6, caps=False)
    lathe(bm, [(0.0, 2.44), (0.12, 2.36), (0.13, 2.33), (0.0, 2.34)], M("DN_EnamelRed"), seg=16, c=(DOOR_X, YF - 0.42, 0), cap0=False, cap1=False)
    sphere(bm, (DOOR_X, YF - 0.42, 2.335), 0.05, M("DN_Opal"), seg=10, rings=5)
    out.append({"at": [DOOR_X, YF - 0.42, 2.25], "lumens": 600, "colour": "#FFD8A0", "kind": "outside"})
    return out


def build(parts):
    kitchen(parts["inside"])
    return {"lights": lamps(parts["small"])}
