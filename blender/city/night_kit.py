"""Props of the night state of public rooms (backlog item 29, docs/city_night.md): what a closed café, club, library or shop
looks like after hours, and the cleaner's things in the morning before it opens. Used inside B.dress blocks
(docs/city_dress.md) with open=False (the closed hours) or hours=(7, 10) (the cleaner's round), the day's chairs moved into
an open=True set at the same places.

Same conventions as interior_kit and dress_kit: x, y on the floor or a surface, z = where it stands, yaw 0 = the piece faces
-y; every piece is solid=False (dressing has no collision) and returns the kit's dict.

    import night_kit as nk
    with B.dress("night_chairs_t1", open=False):
        nk.chairs_up_round(B, cx, cy, top, n=3, r=0.5, kind="tube", mats=[...], seed=1)
    with B.dress("night_cleaning", hours=(7, 10), open=False):
        nk.mop_bucket(B, x, y, 0.0, 30)
        nk.wet_floor(B, x + 0.8, y, 0.0, 0, w=2.0, d=1.4)

CATALOGUE
  chair_up         a chair upside down on a table (the seat on the top, the legs up, the back hanging over the edge
                   toward yaw+180, i.e. the back faces outward when yaw points at the table's middle); kind wood | tube | school
  chairs_up_round  n chairs up on a round table (r its radius), backs outward, evenly round it, turned from a seed
  chairs_up_rect   chairs up on a rectangular table (w x d), two per long side, backs outward
  stool_up         a stool or a bar stool upside down on a counter or a table (kind stool | bar)
  chairs_stacked   a stack of n chairs against a wall (clubs after an evening: the hall cleared)
  mop_bucket       a galvanised bucket on the floor with a mop standing in it, a rag over the rim
  wet_floor        a freshly mopped patch: a thin glossy darker film on the floor (w x d), streaks
  broom_dustpan    a broom and a dustpan leaning against the wall [wall]
  torch            a hand torch (the watchman's): a short tube, its lens toward -y; for a prop, its own mesh
  night_lamp       a lone desk lamp left on (shade, stem, base) for the one light of a closed room: the light itself is
                   B.lamp(..., kind="closed") in the building, outside the dress block
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import interior_kit as kit  # noqa: E402
from interior_kit import _P, _rng  # noqa: E402
from citykit import Mesh  # noqa: E402

GALV = "CT_Aluminium"      # a galvanised bucket (light zinc grey)
WATER = "CT_Glass"         # the mopped film (glossy)
WET = "CT_Wet"             # the damp streaks a mop leaves (glossy, a shade darker than any floor)
RAG = "CT_Concrete"        # a grey floor cloth
MOP_HEAD = "CT_TileWhite"


def _flip_into(B, build, x, y, top, yaw, seat_h):
    """Builds a seat piece with build(stub) at the origin facing -y, turns it upside down (half a turn about its x axis:
    the seat's top onto the surface, the legs up) and puts it into B.detail at (x, y) on a surface at height top, turned
    by yaw. A half turn is a rotation (no mirror), so the faces keep their winding and their UVs."""
    stub = kit.Building_stub()
    info = build(stub)
    m = stub.detail
    a = math.radians(yaw)
    ca, sa = math.cos(a), math.sin(a)
    lift = top + seat_h + 0.002

    def fn(p):
        u, v, h = p[0], -p[1], lift - p[2]
        return (round(x + u * ca - v * sa, 4), round(y + u * sa + v * ca, 4), round(h, 4))

    B.detail.extend(m.transformed(fn))
    xs = [fn(p) for p in m.verts]
    return {"name": "flipped_" + (info.get("name") if isinstance(info, dict) else "piece"), "pos": (x, y, top),
            "yaw": yaw, "tris": m.tri_count(), "z1": max(p[2] for p in xs) if xs else top, "z0": min(p[2] for p in xs) if xs else top}


def chair_up(B, x, y, top, yaw=0.0, kind="wood", mat=None, frame=None):
    """A chair upside down on a table at height top. Upright it would face yaw; upside down its back (behind the sitter,
    +y) comes round to -y local, so the back hangs toward yaw+180... put it with yaw pointing from the table's middle
    outward and the back hangs over the edge."""
    return _flip_into(B, lambda S: kit.chair(S, 0.0, 0.0, 0.0, 0.0, kind=kind, mat=mat, frame=frame), x, y, top, yaw, 0.45)


def chairs_up_round(B, cx, cy, top, n=3, r=0.5, kind="tube", mats=None, seed=0, a0=None):
    """n chairs up on a round table of radius r (centre cx, cy, its top at top): each 0.2 m out from the middle toward
    its side, its back hanging over the edge outward. 2..4 chairs."""
    rnd = _rng(seed, cx, cy, "chairs_up")
    n = max(1, min(4, n))
    start = rnd.uniform(0, 360) if a0 is None else a0
    out = []
    for k in range(n):
        ac = math.radians(start + 360.0 * k / n)
        d = 0.2 if n > 2 else 0.17
        px, py = cx + d * math.cos(ac), cy + d * math.sin(ac)
        # the chair's local +y (its back) after the flip points to local -y; to hang outward the piece faces inward:
        # yaw such that its -y (front) points out from the middle -> yaw = the outward direction's angle
        yaw = (math.degrees(math.atan2(math.cos(ac), -math.sin(ac)))) % 360.0
        mat = mats[k % len(mats)] if mats else None
        out.append(chair_up(B, px, py, top, yaw + rnd.uniform(-6, 6), kind=kind, mat=mat))
    return out


def chairs_up_rect(B, cx, cy, top, yaw=0.0, w=1.2, d=0.8, per_side=2, kind="wood", mats=None, seed=0):
    """Chairs up on a rectangular table (w along its x, d along its y, turned by yaw), per_side on each long side, backs
    hanging over the long edges."""
    rnd = _rng(seed, cx, cy, "chairs_up_rect")
    a = math.radians(yaw)
    ca, sa = math.cos(a), math.sin(a)
    out = []
    k = 0
    for side in (-1, 1):
        for i in range(per_side):
            u = (i + 0.5) / per_side * w - w / 2
            v = side * max(0.05, d / 2 - 0.22)
            px, py = cx + u * ca - v * sa, cy + u * sa + v * ca
            # the outward direction is local +-y of the table; the chair's front (-y) must point that way
            out_ang = yaw + (0.0 if side < 0 else 180.0)
            mat = mats[k % len(mats)] if mats else None
            out.append(chair_up(B, px, py, top, out_ang + rnd.uniform(-4, 4), kind=kind, mat=mat))
            k += 1
    return out


def stool_up(B, x, y, top, yaw=0.0, kind="stool", mat=None, h=None):
    """A stool (kind stool, 0.45 m) or a bar stool (kind bar, 0.75 m) upside down on a counter or a table."""
    if kind == "bar":
        hh = h or 0.75
        return _flip_into(B, lambda S: kit.bar_stool(S, 0.0, 0.0, 0.0, 0.0, mat=mat or "CT_PlasterRed", h=hh), x, y, top, yaw, hh)
    hh = h or 0.45
    return _flip_into(B, lambda S: kit.stool(S, 0.0, 0.0, 0.0, 0.0, mat=mat or kit.WOOD, h=hh), x, y, top, yaw, hh)


def chairs_stacked(B, x, y, z=0.0, yaw=0.0, n=5, kind="school", mat=None):
    """[wall] A stack of n chairs against a wall: each one 0.09 m over the one under it and a little back (the way school
    and club chairs stack), facing -y (out of the wall)."""
    out = None
    for k in range(n):
        out = kit.chair(B, x, y - 0.0 + 0.02 * k, z + 0.09 * k, yaw, kind=kind, mat=mat)
    return out


def mop_bucket(B, x, y, z=0.0, yaw=0.0, seed=0):
    """A galvanised bucket (0.3 m, a little water) with a mop standing in it, leaning, and a rag over the rim."""
    p = _P(B, x, y, z, yaw, "mop_bucket")
    rnd = _rng(seed, x, y, "mop")
    r0, r1, h = 0.13, 0.155, 0.3
    p.lathe(GALV, 0, 0, [(r0 - 0.01, 0.0), (r0, 0.01), (r1, h - 0.012), (r1 + 0.006, h)], 12, top=False, bottom=True)
    # the inside wall (seen from above) and the water a third up
    p.lathe(GALV, 0, 0, [(r1 - 0.004, h), (r0 - 0.004, 0.012)], 12, top=False)
    wz = h * 0.4
    rw = r0 + (r1 - r0) * 0.4 - 0.004
    pts = [(rw * math.cos(2 * math.pi * k / 12), rw * math.sin(2 * math.pi * k / 12), wz) for k in range(12)]
    p.face("CT_ConcreteDark", pts)
    # the ribs and the handle
    p.lathe(GALV, 0, 0, [(r0 + (r1 - r0) * 0.33 + 0.004, h * 0.33 - 0.006), (r0 + (r1 - r0) * 0.33 + 0.006, h * 0.33 + 0.006)], 12, top=False)
    p.tube("CT_Steel", [(-r1 - 0.004, 0, h - 0.03), (-r1 * 0.7, 0, h + 0.1), (0, 0, h + 0.14), (r1 * 0.7, 0, h + 0.1),
                        (r1 + 0.004, 0, h - 0.03)], 0.006, 3)
    # the mop: a wooden pole leaning out of the bucket, its head (grey strands) down in the water
    lean = rnd.uniform(14, 20)
    la = math.radians(lean)
    base = (0.02, 0.0, wz - 0.02)
    tip = (0.02 + math.sin(la) * 1.25, 0.0, wz - 0.02 + math.cos(la) * 1.25)
    p.rod(kit.WOOD, base, tip, 0.026, 6)
    for k in range(10):
        a = 2 * math.pi * k / 10
        ex, ey = 0.07 * math.cos(a), 0.07 * math.sin(a)
        p.rod(MOP_HEAD, (base[0], base[1], base[2] + 0.06), (base[0] + ex, base[1] + ey, max(0.03, base[2] - 0.1)), 0.018, 3)
    # a grey rag hung over the rim
    p.slab(RAG, (r1 * 0.2, -r1 - 0.01, h + 0.005), (-r1 * 0.5, -r1 - 0.01, h + 0.005), (-r1 * 0.5, -r1 - 0.03, h - 0.14),
           (r1 * 0.2, -r1 - 0.03, h - 0.15), 0.006)
    return p.done(top=h)


def wet_floor(B, x, y, z=0.0, yaw=0.0, w=2.0, d=1.4, seed=0):
    """A freshly mopped patch: the mop's figure-of-eight sweeps as thin glossy arcs a hair over the floor, each a
    crescent 4-8 cm wide, with dry floor between the strokes, over w x d."""
    p = _P(B, x, y, z, yaw, "wet_floor")
    rnd = _rng(seed, x, y, "wet")
    rows = max(2, int(d / 0.45))
    k = 0
    for r in range(rows):
        v = -d / 2 + (r + 0.5) * d / rows
        u = -w / 2
        while u < w / 2 - 0.3:
            span = rnd.uniform(0.45, 0.7)
            bow = rnd.uniform(0.06, 0.11) * (1 if (k % 2) else -1)
            wd = rnd.uniform(0.04, 0.08)
            zz = 0.0015 + 0.0004 * (k % 4)
            segs = 6
            outer, inner = [], []
            for i in range(segs + 1):
                t = i / segs
                uu = u + span * t
                vv = v + bow * math.sin(math.pi * t)
                half = wd * 0.5 * math.sin(math.pi * min(1.0, 0.15 + t * 0.85))
                outer.append((uu, vv + half, zz))
                inner.append((uu, vv - half, zz))
            for i in range(segs):
                p.face(WET, [inner[i], inner[i + 1], outer[i + 1], outer[i]])
            u += span * rnd.uniform(1.05, 1.5)   # dry gaps between the strokes
            k += 1
    return p.done(footprint=(-w / 2, w / 2, -d / 2, d / 2))


def broom_dustpan(B, x, y, z=0.0, yaw=0.0):
    """[wall] A broom of twigs and a tin dustpan leaning against the wall (x, y on the wall's face)."""
    p = _P(B, x, y, z, yaw, "broom_dustpan")
    p.kind = "wall"
    # the broom: pole leaning on the wall, the twig head on the floor 0.3 m out
    p.rod(kit.WOOD, (0.0, -0.3, 0.25), (0.0, -0.02, 1.35), 0.026, 6)
    p.lathe("CT_Straw" if False else "CT_WoodLight", 0, -0.32, [(0.02, 0.32), (0.07, 0.15), (0.11, 0.0)], 8, top=False, bottom=True)
    # the dustpan standing on its lip against the wall
    p.slab(GALV, (0.25, -0.02, 0.0), (0.53, -0.02, 0.0), (0.53, -0.08, 0.26), (0.25, -0.08, 0.26), 0.004)
    p.rod(GALV, (0.39, -0.08, 0.26), (0.39, -0.1, 0.45), 0.018, 4)
    return p.done()


def torch(B, x, y, z=0.0, yaw=0.0):
    """A hand torch (the watchman's «фонарик»): a black tube 0.2 m with a chrome head, the lens toward -y."""
    p = _P(B, x, y, z, yaw, "torch")
    p.rod("CT_Rubber", (0.0, 0.08, 0.0), (0.0, -0.06, 0.0), 0.032, 8)
    p.rod("CT_Steel", (0.0, -0.06, 0.0), (0.0, -0.1, 0.0), 0.048, 8)
    p.slab("CT_Opal", (-0.018, -0.1015, -0.018), (0.018, -0.1015, -0.018), (0.018, -0.1015, 0.018), (-0.018, -0.1015, 0.018), 0.002)
    p.box("CT_SteelRed", -0.006, 0.006, -0.03, -0.01, 0.015, 0.022)          # the switch
    return p.done()


def night_lamp(B, x, y, z, yaw=0.0):
    """A lone desk lamp left on (a green-shaded kit lamp): pair it with B.lamp((x, y, z + 0.4), kind="closed") outside
    the dress block so the room has the one light of a closed place."""
    return kit.lamp_green(B, x, y, z, yaw, light=False, solid=False)


def lone_lamp(B, x, y, z, yaw=0.0, lumens=240.0, colour="#FFD9A0", radius=5.0):
    """The one light a shut room keeps (docs/city_night.md): the green desk lamp on a counter or a desk at height z, and
    its real light, kind "closed" (Unreal: on from dusk to dawn while the building is closed; the room's public windows
    glow faintly then). Call it OUTSIDE any dress block (a set may hold no lamps)."""
    r = night_lamp(B, x, y, z, yaw)
    B.lamp((x, y, z + 0.42), lumens=lumens, colour=colour, kind="closed", radius=radius, fixture=False)
    return r


class Gather:
    """Builds pieces into a mesh of their own instead of B.detail, for a dress set written later in one block (a set's
    block may hold no spots or things, but the code that makes the day's chairs often makes the seats' spots too):

        g = nk.Gather(B)
        for t in tables:
            with g:
                chs = kit.around_table(B, ...)        # the chairs go into g.mesh
            B.spot(...)                               # spots and things as before, outside
        g.dress("day_reading", open=True)

    Pieces must be solid=False (the shell is not redirected: what goes to B.m stays in the building)."""

    def __init__(self, B):
        self.B, self.mesh, self._keep = B, Mesh(), None

    def __enter__(self):
        if self.B._dressing is not None:
            raise RuntimeError("nk.Gather inside the dress block %s" % self.B._dressing)
        self._keep, self.B.detail = self.B.detail, self.mesh
        return self.mesh

    def __exit__(self, et, ev, tb):
        self.B.detail, self._keep = self._keep, None
        return False

    def dress(self, did, **when):
        with self.B.dress(did, **when):
            self.B.detail.extend(self.mesh)
