"""Showroom of the dressing props (blender/city/dress_kit.py): every piece in a small room, linted (stretched UVs,
degenerate faces) and shot. Not a building type (nothing is exported to the game).

    python3 blender/city/dress_showroom.py -- [--no-render] [--samples 24] [--res 1400x800] [--out DIR]
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_argv = list(sys.argv)
ARGS = _argv[_argv.index("--") + 1:] if "--" in _argv else []

import citykit  # noqa: E402
import dress_kit as dk  # noqa: E402
import interior_kit as kit  # noqa: E402
from citykit import geom  # noqa: E402


def opt(name, default):
    return ARGS[ARGS.index(name) + 1] if name in ARGS else default


def build(B):
    """A room 9 x 5 m, the back wall at y = 5 facing -y; pieces along the wall and on two tables."""
    geom.box(B.m, "CT_Floor", -5, 5, -1, 5, -0.2, 0.0)
    geom.box(B.m, "CT_Plaster", -5, 5, 5, 5.2, 0.0, 3.2)
    geom.box(B.m, "CT_Plaster", -5.2, -5, -1, 5.2, 0.0, 3.2)
    B.lamp((0.0, 2.5, 2.9), lumens=2500, radius=9)
    with B.dress("wall_winter"):
        dk.wall_hooks(B, -3.4, 5.0, 0.0, 0, w=1.5, season="winter", seed=1)
        dk.sledge(B, -1.9, 5.0, 0.0, 0)
        dk.boots(B, -3.7, 4.55, 0.0, 0, kind="felt", seed=1)
        dk.boots(B, -3.2, 4.55, 0.0, 10, kind="galoshes", seed=2)
    with B.dress("wall_autumn"):
        dk.wall_hooks(B, -0.3, 5.0, 0.0, 0, w=1.5, season="autumn", seed=2)
        dk.umbrella_closed(B, 0.8, 5.0, 0.0, 0, seed=3)
        dk.umbrella_stand(B, 1.25, 4.75, 0.0, 0, n=3, seed=4)
        dk.boots(B, -0.6, 4.55, 0.0, -8, kind="rubber", seed=3)
    with B.dress("wall_summer"):
        dk.wall_hooks(B, 2.9, 5.0, 0.0, 0, w=1.5, season="summer", seed=3)
        dk.boots(B, 2.7, 4.55, 0.0, 5, kind="sandals", seed=4)
        dk.school_bag(B, 3.9, 4.75, 0.0, 0, seed=5)
    with B.dress("floor_rain"):
        dk.umbrella_open(B, -2.6, 2.4, 0.0, 30, seed=5)
        dk.umbrella_open(B, -1.4, 1.6, 0.0, 200, seed=6)
        dk.doormat_wet(B, -2.4, 0.3, 0.0, 0)
    t2 = kit.table(B, 3.4, 2.2, 0.0, 0, w=1.2, d=0.8, solid=False)
    for k, st in enumerate((0, 2, 4, 6)):
        tj = kit.table(B, -0.2 + k * 0.85 - 1.2, 3.4, 0.0, 0, w=0.8, d=0.65, solid=False)
        with B.dress("jigsaw_%d" % st):
            dk.jigsaw(B, -0.2 + k * 0.85 - 1.25, 3.4, tj["top"], 180, stage=st, stages=6, seed=7)
    with B.dress("table_left"):
        dk.vase_wilted(B, 3.0, 2.0, t2["top"], seed=8)
        dk.plate_crumbs(B, 3.45, 1.95, t2["top"], seed=9)
        dk.book_face_down(B, 3.75, 2.35, t2["top"], 20, seed=10)
        dk.mitten_pair(B, 3.4, 2.45, t2["top"], 0, seed=11)
        dk.towel_hat(B, 2.95, 2.45, t2["top"], 0, seed=12)
    return B


def main():
    B = citykit.Building("dress_showroom")
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
    cams = [(0.0, -0.6, 1.65, 0.0 + 180.0, -14.0, "wide"), (2.4, 0.6, 1.5, 180.0 + 10.0, -30.0, "tables"),
            (-2.0, 1.6, 1.3, 180.0 - 10.0, -24.0, "hooks")]
    si.shoot(B, "dress_showroom", cams, samples=int(opt("--samples", 24)), res=opt("--res", "1400x800"), out=out,
             dress={d["id"] for d in B.dresses}, lens=18.0)


if __name__ == "__main__":
    main()
