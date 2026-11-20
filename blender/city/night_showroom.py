"""Showroom of the night-state props (blender/city/night_kit.py): a café corner by day and by night in one room (the
day set on the left, the same tables closed on the right), the cleaner's things, the watchman's torch; linted (stretched
UVs, degenerate faces) and shot. Not a building type (nothing is exported to the game).

    python3 blender/city/night_showroom.py -- [--no-render] [--samples 24] [--res 1400x800] [--out DIR]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_argv = list(sys.argv)
ARGS = _argv[_argv.index("--") + 1:] if "--" in _argv else []

import citykit  # noqa: E402
import interior_kit as kit  # noqa: E402
import night_kit as nk  # noqa: E402
from citykit import geom  # noqa: E402

COLS = ["CT_SteelRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen"]


def opt(name, default):
    return ARGS[ARGS.index(name) + 1] if name in ARGS else default


def build(B):
    """A room 10 x 6 m, the back wall at y = 6; left half by day (chairs round the tables), right half closed."""
    geom.box(B.m, "CT_Floor", -5.5, 5.5, -1, 6, -0.2, 0.0)
    geom.box(B.m, "CT_Plaster", -5.5, 5.5, 6, 6.2, 0.0, 3.2)
    geom.box(B.m, "CT_Plaster", -5.7, -5.5, -1, 6.2, 0.0, 3.2)
    B.lamp((0.0, 3.0, 2.9), lumens=2500, radius=10)
    import math
    for side, x0 in (("day", -3.0), ("night", 2.2)):
        for k, (dx, dy) in enumerate(((0.0, 1.6), (1.7, 3.6), (-0.6, 4.2))):
            cx, cy = x0 + dx, dy
            t = kit.table_round(B, cx, cy, 0.0, 0.0, r=0.5, mat="CT_WoodLight" if k % 2 else "CT_TileWhite", solid=False)
            n = 3 if k != 1 else 2
            if side == "day":
                with B.dress("day_t%d" % k, open=True):
                    for s in range(n):
                        ac = math.radians(40 + 360.0 * s / n)
                        sx, sy = cx + 0.85 * math.cos(ac), cy + 0.85 * math.sin(ac)
                        yw = math.degrees(math.atan2(cx - sx, -(cy - sy))) % 360.0
                        kit.chair(B, sx, sy, 0.0, yw, kind="tube", mat=COLS[(k + s) % 4])
            else:
                with B.dress("night_t%d" % k, open=False):
                    nk.chairs_up_round(B, cx, cy, t["top"], n=n, r=0.5, kind="tube", mats=COLS[k:] + COLS[:k], seed=k, a0=40)
    # a rectangular table with wooden chairs up, bar stools up on a counter, a stack of chairs by the wall
    t2 = kit.table(B, 4.3, 1.4, 0.0, 90, w=1.2, d=0.8, solid=False)
    cnt = kit.shop_counter(B, 0.6, 5.4, 0.0, 180, w=2.4, d=0.6, solid=False)
    with B.dress("night_misc", open=False):
        nk.chairs_up_rect(B, 4.3, 1.4, t2["top"], yaw=90, w=1.2, d=0.8, per_side=2, kind="wood", seed=3)
        for k in range(3):
            nk.stool_up(B, 0.0 + k * 0.6, 5.35, cnt.get("top") or 1.05, 15 * k, kind="bar", mat=COLS[k])
        nk.chairs_stacked(B, -4.6, 5.75, 0.0, 0, n=6, kind="school")
        nk.night_lamp(B, 1.5, 5.4, cnt.get("top") or 1.05, 180)
    with B.dress("cleaner", hours=(7, 10)):
        nk.mop_bucket(B, -1.0, 0.6, 0.0, 20, seed=1)
        nk.wet_floor(B, -0.2, 0.9, 0.0, 0, w=2.2, d=1.6, seed=2)
        nk.broom_dustpan(B, -5.5 + 0.0, 2.5, 0.0, 90)
        nk.torch(B, 1.2, 5.35, (cnt.get("top") or 1.05) + 0.03, 30)
    return B


def main():
    B = citykit.Building("night_showroom")
    build(B)
    if "--no-render" in ARGS:
        for d in B.dresses:
            print("DRESS %-12s %6d tris  bounds %s" % (d["id"], d["mesh"].tri_count(), d["bounds"]))
        return
    sys.argv = [_argv[0], "--", "--no-render"]
    import build_city as bc  # noqa: E402
    import shoot_interior as si  # noqa: E402
    for line in bc.lint(B):
        print("LINT " + line)
    out = opt("--out", os.path.join(bc.ROOT, "renders", "city", "interior"))
    cams = [(0.0, -1.2, 1.7, 180.0, -16.0, "wide"), (2.6, -0.2, 1.4, 180.0 + 8.0, -26.0, "night_tables"),
            (-1.6, -0.6, 1.2, 180.0 - 12.0, -28.0, "cleaner")]
    si.shoot(B, "night_showroom", cams, samples=int(opt("--samples", 24)), res=opt("--res", "1400x800"), out=out,
             dress={d["id"] for d in B.dresses}, lens=18.0)


if __name__ == "__main__":
    main()
