"""The diner's counter and back bar (docs/diner.md): the counter with its foot rail and the things on it (napkin holders, sugar
pourers, salt and pepper, the register), the back bar (cupboards, the grill and its hood, the two coffee urns, the pie case,
the staff's mugs), and the self-service coffee table at the west end.

  build(parts) adds the geometry to parts["inside"] (big pieces: collision) and parts["small"] (things on tops: no collision)
  and returns {"coffee": {...}} (the coffee table's tap, mug places and where the hero stands; read by ALyudDiner).
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


def counter(bm):
    x0, x1 = COUNTER_X
    y0, y1 = COUNTER_Y
    # the body: a recessed dark kick, the stainless front, a mahogany rail under the top
    bx(bm, M("DN_Dark"), x0 + 0.05, x1 - 0.05, y0 + 0.06, y1, 0.0, 0.12, faces="xXyYZ")
    bx(bm, M("DN_Steel"), x0, x1, y0, y1, 0.12, 0.86, faces="xXyYz")
    bx(bm, M("DN_Mahogany"), x0, x1, y0 - 0.03, y1, 0.86, COUNTER_TOP - 0.04, faces="xXyYzZ")
    bx(bm, M("DN_Formica"), x0 - 0.04, x1 + 0.04, 0.28, 1.02, COUNTER_TOP - 0.04, COUNTER_TOP, faces="Z")
    bx(bm, M("DN_Chrome"), x0 - 0.04, x1 + 0.04, 0.28, 1.02, COUNTER_TOP - 0.04, COUNTER_TOP, faces="xXyYz")
    # the foot rail on posts
    tube(bm, (x0 + 0.1, 0.24, 0.2), (x1 - 0.1, 0.24, 0.2), 0.022, M("DN_Chrome"), seg=8)
    for xp in (x0 + 0.25, (x0 + x1) / 2, x1 - 0.25):
        tube(bm, (xp, 0.24, 0.18), (xp, y0 + 0.005, 0.18), 0.016, M("DN_Chrome"), seg=6, caps=False)
    # the staff side: open shelves under the top
    bx(bm, M("DN_Steel"), x0, x1, y1, y1 + 0.02, 0.12, 0.86, faces="Y")
    for z in (0.12, 0.5):
        bx(bm, M("DN_Steel"), x0 + 0.02, x1 - 0.02, y1 + 0.02, 1.0, z, z + 0.02, faces="Zz")
    # napkin holders, sugar pourers, salt and pepper every two stools; a cash register at the east end
    for i, x in enumerate(STOOLS[::2]):
        xx = x + 0.36
        if xx > x1 - 0.4:
            break
        bx(bm, M("DN_Steel"), xx - 0.06, xx + 0.06, 0.62, 0.68, COUNTER_TOP, COUNTER_TOP + 0.1, faces="xXyYZ")
        lathe(bm, [(0.03, COUNTER_TOP), (0.032, COUNTER_TOP + 0.09), (0.02, COUNTER_TOP + 0.11)], M("DN_Glass"), seg=10, c=(xx + 0.12, 0.65, 0), cap0=False)
        lathe(bm, [(0.02, COUNTER_TOP + 0.11), (0.012, COUNTER_TOP + 0.14), (0.0, COUNTER_TOP + 0.145)], M("DN_Chrome"), seg=10, c=(xx + 0.12, 0.65, 0), cap0=False)
        for k_, dx in enumerate((-0.15, -0.11)):
            lathe(bm, [(0.017, COUNTER_TOP), (0.017, COUNTER_TOP + 0.07), (0.0, COUNTER_TOP + 0.085)], M("DN_Glass") if k_ == 0 else M("DN_Glass"),
                  seg=8, c=(xx + dx, 0.65, 0), cap0=False)
    rx = x1 - 0.32
    bx(bm, M("DN_Dark"), rx - 0.2, rx + 0.2, 0.6, 0.98, COUNTER_TOP, COUNTER_TOP + 0.12, faces="xXyYZ")
    ybox(bm, M("DN_Chrome"), (rx, 0.72, COUNTER_TOP + 0.2), (0.38, 0.22, 0.14), 0.0)
    ybox(bm, M("DN_Dark"), (rx, 0.9, COUNTER_TOP + 0.3), (0.3, 0.06, 0.12), 0.0)


def back_bar(bm):
    x0, x1 = BACKBAR_X
    y0, y1 = BACKBAR_Y, YI_B
    bx(bm, M("DN_Dark"), x0, x1, y0 + 0.06, y1, 0.0, 0.1, faces="xXyZ")
    bx(bm, M("DN_Steel"), x0, x1, y0, y1, 0.1, BACKBAR_TOP - 0.03, faces="xXyz")
    bx(bm, M("DN_Steel"), x0 - 0.02, x1 + 0.02, y0 - 0.03, y1, BACKBAR_TOP - 0.03, BACKBAR_TOP, faces="xXyzZ")
    # cupboard doors (raised 6 mm off the front)
    n = int((x1 - x0) // 0.6)
    w = (x1 - x0) / n
    for i in range(n):
        a = x0 + i * w + 0.02
        bx(bm, M("DN_Steel"), a, a + w - 0.04, y0 - 0.016, y0, 0.14, BACKBAR_TOP - 0.08, faces="yxXzZ")
        tube(bm, (a + w / 2 - 0.08, y0 - 0.03, BACKBAR_TOP - 0.15), (a + w / 2 + 0.08, y0 - 0.03, BACKBAR_TOP - 0.15), 0.008, M("DN_Chrome"), seg=6)
    # the stainless backsplash up to the menu board and the window sills, with a mahogany shelf
    bx(bm, M("DN_Steel"), x0, x1, y1 - 0.015, y1, BACKBAR_TOP, 1.4, faces="y")
    bx(bm, M("DN_Mahogany"), x0, x1, y1 - 0.2, y1 - 0.015, 1.4, 1.43, faces="xXyzZ")
    # the grill with its hood
    gx0, gx1 = GRILL_X
    bx(bm, M("DN_Dark"), gx0, gx1, y0 + 0.02, y1 - 0.1, BACKBAR_TOP, BACKBAR_TOP + 0.03, faces="xXyYZ")
    bx(bm, M("DN_Steel"), gx0 - 0.1, gx1 + 0.1, y1 - 0.55, y1 - 0.015, 1.95, 2.25, faces="xXyzZ")
    # the urns: two tall stainless urns with brass-less chrome taps facing the counter
    for x in URNS_X:
        urn(bm, x, y1 - 0.3, BACKBAR_TOP, -1)
    # the pie case at the back bar's west end
    px = -2.9
    bx(bm, M("DN_Chrome"), px - 0.35, px + 0.35, y1 - 0.45, y1 - 0.1, BACKBAR_TOP, BACKBAR_TOP + 0.03, faces="xXyYZ")
    bx(bm, M("DN_Glass"), px - 0.34, px + 0.34, y1 - 0.44, y1 - 0.11, BACKBAR_TOP + 0.03, BACKBAR_TOP + 0.4, faces="xXyYZ")
    for z in (BACKBAR_TOP + 0.03, BACKBAR_TOP + 0.21):
        for k_, dx in enumerate((-0.17, 0.17)):
            lathe(bm, [(0.11, z + 0.002), (0.11, z + 0.02), (0.1, z + 0.08), (0.0, z + 0.085)], M("DN_Crust"), seg=16,
                  c=(px + dx, y1 - 0.27, 0), cap0=False)
    # mugs upside down on the shelf (the staff's)
    for i in range(6):
        lathe(bm, [(0.042, 1.43), (0.042, 1.525), (0.0, 1.527)], M("DN_China"), seg=12, c=(x0 + 0.35 + i * 0.12, y1 - 0.1, 0), cap0=False)


def urn(bm, x, y, z, face):
    """A stainless coffee urn standing at z on three short legs, its tap facing face (+1 +y, -1 -y; 2 +x)."""
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.5
        tube(bm, (x + 0.12 * math.cos(a), y + 0.12 * math.sin(a), z), (x + 0.12 * math.cos(a), y + 0.12 * math.sin(a), z + 0.12), 0.012, M("DN_Chrome"), seg=6, caps=False)
    lathe(bm, [(0.0, z + 0.12), (0.155, z + 0.12), (0.16, z + 0.16), (0.16, z + 0.58), (0.15, z + 0.62), (0.09, z + 0.66), (0.0, z + 0.67)],
          M("DN_Steel"), seg=24, c=(x, y, 0), cap0=False)
    sphere(bm, (x, y, z + 0.69), 0.03, M("DN_Dark"), seg=10, rings=5)
    # a glass gauge tube up its side
    gdir = Vector((0.0, face * 1.0, 0.0)) if face in (-1, 1) else Vector((1.0, 0.0, 0.0))
    side = Vector((gdir.y, -gdir.x, 0)) * 0.1
    g = Vector((x, y, 0)) + gdir * 0.162 + side
    tube(bm, (g.x, g.y, z + 0.2), (g.x, g.y, z + 0.55), 0.008, M("DN_Glass"), seg=6)
    # the tap: a spout out of the body, the lever on top
    t0 = Vector((x, y, 0)) + gdir * 0.16
    t1 = Vector((x, y, 0)) + gdir * 0.25
    tube(bm, (t0.x, t0.y, z + 0.24), (t1.x, t1.y, z + 0.24), 0.014, M("DN_Chrome"), seg=8)
    tube(bm, (t1.x, t1.y, z + 0.24), (t1.x, t1.y, z + 0.2), 0.012, M("DN_Chrome"), seg=8)
    ybox(bm, M("DN_Dark"), (t1.x, t1.y, z + 0.29), (0.025, 0.025, 0.08), 0.0)
    return (t1.x, t1.y, z + 0.2)


def coffee_station(bm):
    x0, x1, y0, y1 = STATION
    bx(bm, M("DN_Mahogany"), x0, x1, y0, y1, 0.0, STATION_TOP - 0.03, faces="XyYZ")
    bx(bm, M("DN_Formica"), x0, x1 + 0.02, y0 - 0.02, y1 + 0.02, STATION_TOP - 0.03, STATION_TOP, faces="Z")
    bx(bm, M("DN_Chrome"), x0, x1 + 0.02, y0 - 0.02, y1 + 0.02, STATION_TOP - 0.03, STATION_TOP, faces="XyYz")
    ux, uy = URN_SELF
    tap = urn(bm, ux, uy, STATION_TOP, 2)
    # the drip tray under the tap
    bx(bm, M("DN_Chrome"), tap[0] - 0.07, tap[0] + 0.07, tap[1] - 0.08, tap[1] + 0.08, STATION_TOP, STATION_TOP + 0.015, faces="xXyYZ")
    # a tray of clean mugs upside down, a sugar pourer and a creamer
    mx, my = -6.85, -1.33
    bx(bm, M("DN_Steel"), mx - 0.16, mx + 0.16, my - 0.2, my + 0.2, STATION_TOP, STATION_TOP + 0.012, faces="xXyYZ")
    for i in range(2):
        for j in range(3):
            lathe(bm, [(0.042, STATION_TOP + 0.012), (0.042, STATION_TOP + 0.107), (0.0, STATION_TOP + 0.109)], M("DN_China"), seg=12,
                  c=(mx - 0.07 + i * 0.14, my - 0.12 + j * 0.12, 0), cap0=False)
    lathe(bm, [(0.03, STATION_TOP), (0.032, STATION_TOP + 0.09), (0.02, STATION_TOP + 0.11)], M("DN_Glass"), seg=10, c=(-6.95, -0.5, 0), cap0=False)
    lathe(bm, [(0.02, STATION_TOP + 0.11), (0.012, STATION_TOP + 0.14), (0.0, STATION_TOP + 0.145)], M("DN_Chrome"), seg=10, c=(-6.95, -0.5, 0), cap0=False)
    return {"tap": [round(v, 4) for v in tap], "mug_under_tap": [round(tap[0], 4), round(tap[1], 4), STATION_TOP + 0.015],
            "mugs": [mx, my, STATION_TOP + 0.012], "stand": [x1 + 0.45, (y0 + y1) / 2, 0.0]}


def build(parts):
    counter(parts["inside"])
    back_bar(parts["inside"])
    return {"coffee": coffee_station(parts["inside"])}
