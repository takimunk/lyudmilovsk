"""The diner's seats (docs/diner.md): the counter's stools, the six booths along the street windows (two benches, a table, the
jukebox selector) and the curved bench with its two round tables in the nose. Every seat the hero or a customer can sit on is
listed by build() for layout.json ("hero_seats").

  build(parts) adds the geometry to parts["inside"] (benches, tables: collision), parts["stools"] (no collision),
  parts["small"] (small fittings, no collision) and returns {"hero_seats": [...]}.
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


def stool(bm, x, y):
    lathe(bm, [(0.17, 0.0), (0.17, 0.02), (0.06, 0.04)], M("DN_Chrome"), seg=16, c=(x, y, 0.0), cap0=False)
    tube(bm, (x, y, 0.04), (x, y, STOOL_SEAT - 0.06), 0.035, M("DN_Chrome"), seg=10, caps=False)
    lathe(bm, [(0.05, STOOL_SEAT - 0.07), (0.19, STOOL_SEAT - 0.05), (0.195, STOOL_SEAT - 0.02)], M("DN_Chrome"), seg=20, c=(x, y, 0.0), cap0=False, cap1=False)
    lathe(bm, [(0.195, STOOL_SEAT - 0.02), (0.19, STOOL_SEAT + 0.01), (0.15, STOOL_SEAT + 0.035), (0.0, STOOL_SEAT + 0.045)],
          M("DN_Vinyl"), seg=20, c=(x, y, 0.0), cap0=False)


def booth(bm, xa, xb):
    """Two benches facing over a table, the backs at xa and xb, against the street front's inner face."""
    y0, y1 = YI_F, BOOTH_Y
    for xs, s in ((xa, 1), (xb, -1)):
        xback = xs + s * 0.09
        seat_out = xs + s * 0.55
        lo, hi = sorted((xs, xback))
        bx(bm, M("DN_Mahogany"), lo, hi, y0, y1, 0.0, 1.12, faces="xXYZ")                 # the back
        lo, hi = sorted((xback, seat_out))
        bx(bm, M("DN_Mahogany"), lo, hi, y0, y1 - 0.04, 0.0, 0.38, faces="xXY" if s > 0 else "xXY")
        bx(bm, M("DN_Vinyl"), lo, hi, y0, y1, 0.38, 0.46, faces="xXYZ")                    # the seat cushion
        # the back cushion facing the table
        bc0, bc1 = (xback, xback + s * 0.07)
        lo, hi = sorted((bc0, bc1))
        bx(bm, M("DN_Vinyl"), lo, hi, y0, y1 - 0.03, 0.5, 1.05, faces=("X" if s > 0 else "x") + "YZz")
    # the table: Formica top with a chrome edge on a pedestal, its long side at the wall
    tx0, tx1 = xa + 0.62, xb - 0.62
    bx(bm, M("DN_Formica"), tx0, tx1, y0, y1 - 0.08, 0.735, 0.765, faces="Z")
    bx(bm, M("DN_Chrome"), tx0, tx1, y0, y1 - 0.08, 0.735, 0.765, faces="xXYz")
    tube(bm, ((tx0 + tx1) / 2, (y0 + y1) / 2, 0.0), ((tx0 + tx1) / 2, (y0 + y1) / 2, 0.735), 0.04, M("DN_Chrome"), seg=10, caps=False)
    lathe(bm, [(0.22, 0.0), (0.2, 0.025), (0.04, 0.04)], M("DN_Chrome"), seg=16, c=((tx0 + tx1) / 2, (y0 + y1) / 2, 0.0), cap0=False)
    # a jukebox selector on the wall over the table, between the windows' sills
    cx = (tx0 + tx1) / 2
    bx(bm, M("DN_Chrome"), cx - 0.11, cx + 0.11, y0, y0 + 0.09, 0.8, 0.93, faces="xXYZz")


def nose_bench(bm):
    """A curved bench along the nose's inner wall (-60..60 degrees) and two round tables."""
    r_wall = R - TH
    for j in range(NOSE_SEG):
        a0 = -math.pi / 2 + math.pi * j / NOSE_SEG
        a1 = -math.pi / 2 + math.pi * (j + 1) / NOSE_SEG
        am = (a0 + a1) / 2
        if abs(am) > math.radians(62):
            continue
        for r0, r1, z0, z1, mi, keep in ((r_wall - 0.55, r_wall, 0.38, 0.46, M("DN_Vinyl"), "top"),
                                         (r_wall - 0.52, r_wall, 0.0, 0.38, M("DN_Mahogany"), "front"),
                                         (r_wall - 0.09, r_wall, 0.46, 1.1, M("DN_Mahogany"), "back")):
            q = lambda r, a, z: (XE + r * math.cos(a), r * math.sin(a), z)  # noqa: E731
            if keep == "top":
                oq(bm, [q(r0, a0, z1), q(r0, a1, z1), q(r1, a1, z1), q(r1, a0, z1)], mi, (0, 0, 1))
                oq(bm, [q(r0, a0, z0), q(r0, a1, z0), q(r0, a1, z1), q(r0, a0, z1)], mi, (-math.cos(am), -math.sin(am), 0))
            elif keep == "front":
                oq(bm, [q(r0, a0, z0), q(r0, a1, z0), q(r0, a1, z1), q(r0, a0, z1)], mi, (-math.cos(am), -math.sin(am), 0))
            else:
                oq(bm, [q(r0, a0, z0), q(r0, a1, z0), q(r0, a1, z1), q(r0, a0, z1)], mi, (-math.cos(am), -math.sin(am), 0))
                oq(bm, [q(r0, a0, z1), q(r0, a1, z1), q(r1, a1, z1), q(r1, a0, z1)], mi, (0, 0, 1))
        # the bench's open ends
    for am in (math.radians(-60), math.radians(60)):
        for r0, r1, z0, z1, mi in ((r_wall - 0.55, r_wall, 0.0, 0.46, M("DN_Mahogany")), (r_wall - 0.09, r_wall, 0.46, 1.1, M("DN_Mahogany"))):
            s = -1 if am < 0 else 1
            t = Vector((-math.sin(am), math.cos(am), 0)) * s
            a = Vector((XE + r0 * math.cos(am), r0 * math.sin(am), 0))
            b = Vector((XE + r1 * math.cos(am), r1 * math.sin(am), 0))
            oq(bm, [(a.x, a.y, z0), (b.x, b.y, z0), (b.x, b.y, z1), (a.x, a.y, z1)], mi, tuple(t))
    for x, y in NOSE_TABLES:
        lathe(bm, [(0.0, 0.765), (0.34, 0.765)], M("DN_Formica"), seg=24, c=(x, y, 0), cap0=False, cap1=False)
        lathe(bm, [(0.34, 0.765), (0.345, 0.75), (0.34, 0.735), (0.0, 0.735)], M("DN_Chrome"), seg=24, c=(x, y, 0), cap0=False, cap1=False)
        tube(bm, (x, y, 0.0), (x, y, 0.735), 0.04, M("DN_Chrome"), seg=10, caps=False)
        lathe(bm, [(0.22, 0.0), (0.2, 0.025), (0.04, 0.04)], M("DN_Chrome"), seg=16, c=(x, y, 0.0), cap0=False)


def build(parts):
    for x in STOOLS:
        stool(parts["stools"], x, STOOL_Y)
    for xa, xb in BOOTHS:
        booth(parts["inside"], min(xa, xb), max(xa, xb))
    nose_bench(parts["inside"])
    return {"hero_seats": []}
