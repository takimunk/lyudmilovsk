"""The brook's bird places (the Aton coast's brook, tools/south.py, docs/south.md) for tools/bird_habitats.py: where the
wild birds of the bird life (docs/bird_life.md) can be along the brook and at its mouth, taken from the brook's own course
(south.brook_water, south.brook_at), never drawn by hand. Read-only use of tools/south.py: nothing here changes it. On a
branch without tools/south.py (or without its brook) this adds nothing.

  python3 tools/bird_habitats_brook.py        a summary of the brook's sites (no files written)

What goes where (the kinds are LyudBirds::EHabitat's, BirdSim.h; who likes which is BirdEcology.cpp's):

  Brook   the valley's and the reedbed's slow bends: every 25-40 m a small patch on the water's edge, on the inside of
          the bend (where the gravel and silt bar is), never in the stream (the bird life keeps a walking bird on the
          patch); a few stones on the ravine's bed edge
  Mouth   the fan where the brook spreads over the beach to the sea: wet sand on both sides of the channel, laid bare at
          low water and covered at high water like the beach (FWorld::Taste)
  Roof    the fishermen's shed by the mouth (south.things() `fisher_shed`): its ridge, where the cormorant dries its wings

The sites have their own random stream: adding or moving them never moves a site of the rest of the map.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402

try:
    import south as SO  # noqa: E402  (the Aton coast; absent on older branches)
except ImportError:
    SO = None

SEED = 4343
VALLEY_EVERY = (25.0, 40.0)    # m along the brook between two patches on its edge
VALLEY_R = 0.45                # the patch's radius: the bird keeps to it (BirdSim.cpp OnPatch)...
VALLEY_EDGE = 0.6              # ...and its middle this far from the water's edge, so the patch never reaches the water
RAVINE_EVERY = (25.0, 35.0)
RAVINE_STONES = 4
RAVINE_EDGE = 0.45             # a stone of radius 0.3 off the edge
STONE_TOP = 0.25               # a stone's top over the ground beside it
MOUTH_AT = (14.0, 6.5)         # m inland of the waterline: the fan's upper and lower sand, a site each side of the channel
MOUTH_R = 2.5                  # the fan's sand is wide: a gull flock's patch
MOUTH_EDGE = 2.8               # m from the channel's water to the patch's middle (its radius plus a margin)
SHED_RIDGE = 3.2               # blender/south/props_places.py fisher_shed: RIDGE over its origin
SHED_HALF = 2.0                # half the ridge's length along its front axis (walls SY = 2.1)


def _course():
    """[(x, y, surface z, water half width, kind, metres along)] every ~6 m, or [] when there is no brook."""
    if SO is None or not hasattr(SO, "brook_water"):
        return []
    c = SO._course()
    out = []
    for x, y, z, half in SO.brook_water():
        b = SO.brook_at(x, y)
        if b is None:
            continue
        out.append((x, y, z, half, c["kind"][b[1]], c["s"][b[1]]))
    return out


def _edge(pts, i, side, off):
    """The point `off` metres beyond the water's edge at sample i, on side +1 (left looking downstream) or -1."""
    x, y, _, half = pts[i][:4]
    a, b = pts[max(i - 1, 0)], pts[min(i + 1, len(pts) - 1)]
    tx, ty = b[0] - a[0], b[1] - a[1]
    tl = math.hypot(tx, ty) or 1.0
    d = side * (half + off)
    return x - ty / tl * d, y + tx / tl * d


def _inner(pts, i, rnd):
    """The inside of the bend at sample i (+1 left, -1 right): the slack water and the bar; either on a straight."""
    a, p, b = pts[max(i - 1, 0)], pts[i], pts[min(i + 1, len(pts) - 1)]
    cross = (p[0] - a[0]) * (b[1] - p[1]) - (p[1] - a[1]) * (b[0] - p[0])
    if abs(cross) < 0.4:
        return 1 if rnd.random() < 0.5 else -1
    return 1 if cross > 0 else -1


def add_extras(add, ground_z):
    """Adds the brook's, the mouth's and the shed's sites through add(kind, p, q, radius, cap); returns {what: sites}."""
    pts = _course()
    if not pts:
        return {}
    rnd = random.Random(SEED)
    out = {"brook bends": 0, "ravine stones": 0, "mouth": 0, "shed ridge": 0}

    # the bends of the valley and the reedbed
    nxt = None
    for i, (x, y, z, half, kind, s) in enumerate(pts):
        if kind not in ("valley", "reed"):
            continue
        if nxt is None:
            nxt = s + rnd.uniform(8.0, 16.0)
        if s < nxt:
            continue
        nxt = s + rnd.uniform(*VALLEY_EVERY)
        ex, ey = _edge(pts, i, _inner(pts, i, rnd), VALLEY_EDGE)
        add("Brook", (ex, ey, max(ground_z(ex, ey), z) + 0.02), radius=VALLEY_R, cap=6)
        out["brook bends"] += 1

    # stones on the ravine's bed edge (wagtails; the dipper's, when it comes)
    nxt, side = None, 1 if rnd.random() < 0.5 else -1
    for i, (x, y, z, half, kind, s) in enumerate(pts):
        if kind != "ravine" or out["ravine stones"] >= RAVINE_STONES:
            continue
        if nxt is None:
            nxt = s + rnd.uniform(4.0, 12.0)
        if s < nxt:
            continue
        nxt = s + rnd.uniform(*RAVINE_EVERY)
        side = -side
        ex, ey = _edge(pts, i, side, RAVINE_EDGE)
        add("Brook", (ex, ey, max(ground_z(ex, ey), z) + STONE_TOP), radius=0.3, cap=2)
        out["ravine stones"] += 1

    # the fan on the beach: the samples nearest each MOUTH_AT inland, a patch on each side of the channel
    beach = [i for i, p in enumerate(pts) if p[4] == "bank" and W.inland(p[0], p[1]) < W.BEACH_W]
    for at in MOUTH_AT:
        if not beach:
            break
        i = min(beach, key=lambda k: abs(W.inland(pts[k][0], pts[k][1]) - at))
        for side in (1, -1):
            ex, ey = _edge(pts, i, side, MOUTH_EDGE)
            add("Mouth", (ex, ey, ground_z(ex, ey) + 0.02), radius=MOUTH_R, cap=20)
            out["mouth"] += 1

    # the fishermen's shed: the ridge runs along its front axis (the roof falls to its sides)
    for t in SO.things() if hasattr(SO, "things") else []:
        if t["kind"] != "fisher_shed":
            continue
        a = math.radians(t["yaw"])
        ux, uy = math.cos(a), math.sin(a)
        z = ground_z(t["x"], t["y"]) + SHED_RIDGE + 0.05
        add("Roof", (t["x"] - ux * SHED_HALF, t["y"] - uy * SHED_HALF, z), (t["x"] + ux * SHED_HALF, t["y"] + uy * SHED_HALF, z),
            radius=0.0, cap=6)
        out["shed ridge"] += 1
    return out


if __name__ == "__main__":
    import terrain as T

    sites = []

    def _add(kind, p, q=None, radius=0.0, cap=8, flags=0):
        sites.append((kind, p, q or p, radius, cap))

    print(add_extras(_add, T.ground_z))
    for s in sites:
        print(s[0], tuple(round(v, 2) for v in s[1]), tuple(round(v, 2) for v in s[2]), s[3], s[4])
