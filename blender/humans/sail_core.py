"""The yacht school clips' framework (sail_clips.py): the boats' numbers, posing mirrored, the body, the arms and
grips, the clips' registry and the checks. Blender character space: faces -Y, its left +X, up +Z; metres."""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import poser  # noqa: E402
from poser import X, Y, Z, FPS, ROOT, Poser  # noqa: E402

TAU = 2 * math.pi
SOCKETS = os.path.join(ROOT, "blender", "sailing", "sockets.json")
GRIPS = os.path.join(HERE, "sail_grips.json")

# Real dimensions, metres, in each crew station's frame (see the doc above). Parametrises every boat clip.
BOATS = {
    "luch": {
        "scale": 1.0,                 # the body it is posed for (1 = a grown-up like Guy_a)
        "deck_z": 0.25,               # side deck over the cockpit sole
        "half_beam": 0.685,           # the gunwale's edge
        "cockpit_half": 0.26,         # the sole's half width (the deck from here out)
        "length": (-2.6, 1.6),        # bow .. transom along y from the helm's station
        "sit_x": 0.56,                # the seat bones on the side deck
        "strap": (0.0, 0.13),         # the hiking strap: x, height (along the centreline)
        "sheet_block": (0.0, -0.32, 0.07),   # the mainsheet's ratchet block on the floor
        "rudder": (0.0, 1.55, 0.36),  # the tiller's pivot on the rudder head
        "tiller_len": 0.95,           # rudder head to the tiller's end (where the extension's joint is)
        "ext_len": 1.0,               # the tiller extension
        "boom_z": 1.0,                # the boom's underside over the sole (0.75 over the deck)
        "mast_y": -1.75,
    },
    "optimist": {
        "scale": 0.74,                # a child of ~1.35 m
        "deck_z": 0.30,               # the side (gunwale) a child sits on, over the bottom
        "half_beam": 0.565,
        "cockpit_half": 0.48,
        "length": (-1.35, 0.95),
        "sit_x": 0.50,
        "strap": (0.0, 0.11),
        "sheet_block": (0.0, -0.05, 0.05),
        "boom_block": (0.0, 0.12, 0.78),     # the sheet comes down from the boom's block overhead
        "rudder": (0.0, 0.98, 0.30),
        "tiller_len": 0.62,
        "ext_len": 0.55,
        "boom_z": 0.80,
        "mast_y": -0.95,
    },
    "cadet": {
        "scale": 0.86,                # a teenager of ~1.55 m
        "deck_z": 0.27,
        "half_beam": 0.63,
        "cockpit_half": 0.40,
        "length": (-2.1, 1.1),
        "sit_x": 0.55,
        "strap": (0.15, 0.12),
        "sheet_block": (0.0, 0.25, 0.10),    # centre main: the block on the thwart behind the helm
        "rudder": (0.0, 1.12, 0.33),
        "tiller_len": 0.75,
        "ext_len": 0.75,
        "boom_z": 0.98,
        "mast_y": -1.45,
        "crew_dy": -0.75,             # the crew's station forward of the helm's
        "jib_lead": (-0.42, -0.95, 0.30),    # the leeward jib fairlead, from the helm's station (crew clips use crew_*)
        "crew_jib_lead": (-0.42, -0.2, 0.30),
    },
}


ANKLE_Z = 0.08                        # the clips' grown-up's ankle over the floor (a child's: times its scale)


def boat_from_sockets(B, d):
    """BOATS numbers from part C's entry for a boat (its frame: metres, origin at the waterline amidships, +X forward,
    +Y port, +Z up). The clip's origin is on the centreline abreast the helm's seat (helm_port's x), on the floor under
    the helm's feet (helm_port's feet z less the ankle's height): origin_in_boat, which the engine attaches the body at."""
    P, S, dims = d.get("parts", {}), d.get("sockets", {}), d.get("dims", {})
    helm = S.get("helm_port")
    if not helm:
        return False
    sx = helm["pos"][0]
    sole = helm["feet"][2] - ANKLE_Z * B["scale"]

    def c(p):                          # boat frame -> the clip's frame (port +X, bow -Y, up +Z)
        return (round(p[1], 4), round(-(p[0] - sx), 4), round(p[2] - sole, 4))
    B["origin_in_boat"] = (sx, 0.0, round(sole, 4))
    if "deck_z_mid" in dims:
        B["deck_z"] = round(dims["deck_z_mid"] - sole, 4)
    if "beam" in dims:
        B["half_beam"] = round(dims["beam"] * 0.5, 4)
    if "loa" in dims:
        B["length"] = (round(-(dims["loa"] * 0.5 - sx), 4), round(dims["loa"] * 0.5 + sx, 4))
    B["sit_x"] = round(abs(helm["pos"][1]), 4)
    if "strap" in S:
        B["strap"] = (round(abs(S["strap"]["from"][1]), 4), round(S["strap"]["from"][2] - sole, 4))
        # the floor reaches at least a foot's length outboard of the strap (part C gives no floor width)
        B["cockpit_half"] = max(B["cockpit_half"], round(B["strap"][0] + 0.2, 4))
    if "mainsheet_block" in S:
        B["sheet_block"] = c(S["mainsheet_block"]["pos"])
    if "Tiller" in P:
        B["rudder"] = c(P["Tiller"]["pivot"])
        if "Ext" in P:
            B["tiller_len"] = round(P["Ext"]["pivot"][0] - P["Tiller"]["pivot"][0], 4)
    if "hand_tiller_ext" in S:
        B["ext_len"] = round(S["hand_tiller_ext"]["pos"][0] + 0.06, 4)
    if "Boom" in P:
        B["boom_z"] = round(P["Boom"]["pivot"][2] - sole - 0.03, 4)
        if "boom_block" in B and "boom_block" in S:
            bp, bb = P["Boom"]["pivot"], S["boom_block"]["pos"]
            B["boom_block"] = c((bp[0] + bb[0], bp[1] + bb[1], bp[2] + bb[2]))
    if "Mast" in P:
        B["mast_y"] = round(-(P["Mast"]["pivot"][0] - sx), 4)
    if "crew_dy" in B and "crew_port" in S:
        B["crew_dy"] = round(-(S["crew_port"]["pos"][0] - sx), 4)
    return True


def load_sockets():
    """Part C's crew sockets (blender/sailing/sockets.json) over the defaults (boat_from_sockets)."""
    if not os.path.exists(SOCKETS):
        return "defaults (no blender/sailing/sockets.json yet)"
    try:
        data = json.load(open(SOCKETS))
    except Exception as e:  # noqa: BLE001
        return "defaults (sockets.json unreadable: %s)" % e
    got = [b for b, d in data.items() if b in BOATS and isinstance(d, dict) and boat_from_sockets(BOATS[b], d)]
    return "blender/sailing/sockets.json (%s)" % ", ".join(got)


def log(m):
    print("SAILCLIPS " + m, flush=True)


def sm(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def win(t, a, b, rise=0.25):
    """1 between a and b (seconds), smooth edges `rise` long."""
    return sm((t - a) / rise + 0.5) * sm((b - t) / rise + 0.5)


def lerp(a, b, t):
    return a + (b - a) * t


def V(*a):
    return Vector(a)


def yaw_axes(yaw):
    """The facing and left of a body turned `yaw` degrees (to its left, about +Z) from facing -Y."""
    a = math.radians(yaw)
    return Vector((math.sin(a), -math.cos(a), 0.0)), Vector((math.cos(a), math.sin(a), 0.0))


# ---------------------------------------------------------------------------------------------- posing, mirrored

OTHER = {"l": "r", "r": "l"}


class R:
    """Poser with a mirror: everything is authored for the port side (or the body as is: m = 1); m = -1 poses its mirror
    image across the centreline (x -> -x, left <-> right bones, turns about mirrored axes the other way)."""

    def __init__(self, p, m=1):
        self.p, self.m = p, m

    def v(self, a):
        return Vector((self.m * a[0], a[1], a[2]))

    def s(self, side):
        return side if self.m > 0 else OTHER[side]

    def b(self, name):
        if self.m < 0 and len(name) > 2 and name[-2] == "_" and name[-1] in "lr":
            return name[:-1] + OTHER[name[-1]]
        return name

    def reset(self):
        self.p.reset()

    def head(self, name):
        return self.v(self.p.head(self.b(name)))

    def turn(self, name, deg, axis):
        a = Vector(axis)
        self.p.turn(self.b(name), deg * self.m, (self.m * a.x, a.y, a.z))

    def place(self, name, where):
        self.p.place(self.b(name), self.v(where))

    def aim(self, name, child, d):
        self.p.aim(self.b(name), self.b(child), self.v(d))

    def ik(self, upper, lower, end, target, pole):
        self.p.ik(self.b(upper), self.b(lower), self.b(end), self.v(target), self.v(pole))

    def hand(self, side, fd, palm, share=0.5):
        self.p.hand(self.s(side), self.v(fd), self.v(palm), share)

    def curl(self, side, amounts, thumb=None):
        self.p.curl(self.s(side), amounts, thumb)

    def axes(self, bone):
        """A bone's facing (the body's -Y at rest) and left (+X at rest) now, in the authoring frame."""
        pb = self.p.pb[self.b(bone)]
        rot = pb.matrix.to_3x3() @ pb.bone.matrix_local.to_3x3().inverted()
        # (a mirror swaps left and right: the posed body's own left, mirrored back, is the authored body's right)
        return self.v(rot @ Vector((0, -1, 0))), self.v(rot @ Vector((1, 0, 0))) * self.m


# ---------------------------------------------------------------------------------------------- the body

def body(r, pelvis, yaw=0.0, tilt=0.0, lean=0.0, side=0.0, twist=0.0, legs=None):
    """The trunk and legs: the pelvis at `pelvis` turned `yaw` (deg, to the left) and tipped `tilt` (deg, + forward),
    the spine bent `lean` forward, `side` to its left, chest turned `twist` to the left (deg, shared over the three
    spine bones); legs {side: (ankle, toes_dir, knee_pole)} by two-bone IK."""
    r.reset()
    r.place("pelvis", pelvis)
    if yaw:
        r.turn("pelvis", yaw, Z)
    f, left = yaw_axes(yaw)
    if tilt:
        r.turn("pelvis", tilt, left)
    for s, (ankle, toes, pole) in (legs or {}).items():
        r.ik("thigh_" + s, "calf_" + s, "foot_" + s, ankle, pole)
        r.aim("foot_" + s, "ball_" + s, toes)
    for bone, w in (("spine_01", 0.4), ("spine_02", 0.35), ("spine_03", 0.25)):
        if lean:
            r.turn(bone, lean * w, left)
        if side:
            r.turn(bone, -side * w, f)
        if twist:
            r.turn(bone, twist * w, Z)
    return f, left


def look(r, target, amount=1.0, extra_pitch=0.0, neck=0.45):
    """The head (and neck, `neck` of it) turned toward a point; amount < 1 only part of the way."""
    h = r.head("head") + Vector((0, 0, 0.09))
    fwd, _ = r.axes("head")
    d = Vector(target) - h
    yaw_err = math.degrees(math.atan2(fwd.x * d.y - fwd.y * d.x, fwd.x * d.x + fwd.y * d.y))
    yaw_err = max(-95.0, min(95.0, yaw_err)) * amount
    el_d = math.degrees(math.atan2(d.z, d.to_2d().length))
    el_f = math.degrees(math.atan2(fwd.z, fwd.to_2d().length))
    pitch = max(-55.0, min(45.0, (el_d - el_f) * amount + extra_pitch))
    r.turn("neck_01", yaw_err * neck, Z)
    r.turn("head", yaw_err * (1 - neck), Z)
    fwd, left = r.axes("head")
    r.turn("neck_01", -pitch * 0.4, left)
    r.turn("head", -pitch * 0.6, left)


def head_turn(r, yaw=0.0, pitch=0.0, roll=0.0):
    """The head turned on its own (deg: yaw to the left, pitch up, roll toward the left shoulder)."""
    r.turn("neck_01", yaw * 0.4, Z)
    r.turn("head", yaw * 0.6, Z)
    fwd, left = r.axes("head")
    r.turn("neck_01", -pitch * 0.4, left)
    r.turn("head", -pitch * 0.6, left)
    if roll:
        r.turn("head", -roll, fwd)


def shoulder(r, side, wrist):
    """The clavicle follows the hand: forward reaching ahead or across, up raising it (house rule: shoulders move)."""
    sign = 1 if side == "l" else -1
    s = r.head("upperarm_" + side)
    f, left = r.axes("spine_03")
    d = Vector(wrist) - s
    across = -d.dot(left) * sign                       # toward the other side
    prot = max(-6.0, min(16.0, d.dot(f) * 22.0 + max(0.0, across) * 18.0 - 3.0))
    elev = max(-5.0, min(30.0, d.z * 32.0 + 3.0))
    r.turn("clavicle_" + side, -sign * prot, Z)
    r.turn("clavicle_" + side, sign * elev, f)


def arm(r, side, wrist, fd, palm, pole=None, share=0.5, follow=True):
    """The hand's wrist to `wrist` by IK (the elbow toward `pole`), the hand's fingers along fd, its palm facing palm."""
    if follow:
        shoulder(r, side, wrist)
    if pole is None:
        sign = 1 if side == "l" else -1
        f, left = r.axes("spine_03")
        pole = left * (sign * 0.5) + f * -0.25 + Vector((0, 0, -1.0))
    r.ik("upperarm_" + side, "lowerarm_" + side, "hand_" + side, wrist, pole)
    r.hand(side, fd, palm, share)


def grip_frame(side, axis, knuckles):
    """A fist round a bar along `axis` (the thumb's side toward +axis), knuckles toward `knuckles`: (fingers, palm)."""
    e = Vector(axis).normalized()
    k = Vector(knuckles)
    fd = (k - e * k.dot(e)).normalized()
    # Blender's right-handed space: the right palm faces thumb x fingers, the left fingers x thumb (the thumb along +e)
    n = e.cross(fd) if side == "r" else fd.cross(e)
    return fd, n


def grip(r, side, at, axis, knuckles, radius=0.012, pole=None, curl=62.0, share=0.5, thumb=30.0):
    """The hand closed round a bar through `at` along `axis`: wrist placed so the bar lies across the palm."""
    fd, n = grip_frame(side, axis, knuckles)
    wrist = Vector(at) - fd * 0.072 - n * (0.026 + radius)
    arm(r, side, wrist, fd, n, pole, share)
    r.curl(side, {"index": curl - 4, "middle": curl, "ring": curl + 3, "pinky": curl + 6}, thumb=thumb)
    return {"grip": list(at), "axis": list(e_(axis)), "palm": list(n), "fingers": list(fd), "radius": radius}


def e_(a):
    return Vector(a).normalized()


def relax(r, side, amount=1.0, t=0.0):
    """Loose, slightly curled fingers that breathe a little."""
    c = 14.0 * amount + 3.0 * math.sin(t * 1.3 + (0 if side == "l" else 1.7))
    r.curl(side, {"index": c - 3, "middle": c, "ring": c + 3, "pinky": c + 6}, thumb=8.0)


def stand_legs(r, spread=0.12, ahead=0.0, z=0.0, toe_out=8.0, f=None, left=None, at=Vector((0, 0, 0))):
    """Feet planted on the ground (z) under a standing body: {side: (ankle, toes, pole)}."""
    f = f if f is not None else Vector((0, -1, 0))
    left = left if left is not None else Vector((1, 0, 0))
    out = {}
    for s, sign in (("l", 1), ("r", -1)):
        a = Vector(at) + left * (sign * spread) + f * (ahead * sign * 0.5)
        a.z = z + 0.08
        toe = (f * math.cos(math.radians(toe_out)) + left * (sign * math.sin(math.radians(toe_out)))) * 0.95 + Vector((0, 0, -0.42))
        out[s] = (a, toe, f * 1.0 + left * (sign * 0.1) + Vector((0, 0, 0.1)))
    return out


# ---------------------------------------------------------------------------------------------- the clips' registry

CLIPS = {}       # base name -> dict(fn, T, loop, mirror, frame, boat, scene, about)


def clip(name, T, loop=True, mirror=False, frame="shore", boat=None, scene=None, about=""):
    def deco(fn):
        CLIPS[name] = dict(fn=fn, T=T, loop=loop, mirror=mirror, frame=frame, boat=boat, scene=scene or frame, about=about)
        return fn
    return deco


# ---------------------------------------------------------------------------------------------- checks

def check(r, spec, t):
    """Limbs through the boat (deck, sole) or the ground, forearms through the trunk: [(what, cm)]."""
    p = r.p
    out = []
    pts = []
    for s in "lr":
        for a, b, rad in (("upperarm_", "lowerarm_", 0.045), ("lowerarm_", "hand_", 0.035), ("thigh_", "calf_", 0.07),
                          ("calf_", "foot_", 0.045), ("foot_", "ball_", 0.035)):
            A, B = r.head(a + s), r.head(b + s)
            for k in (0.0, 0.5, 1.0):
                pts.append((a + s, A.lerp(B, k), rad))
    pts.append(("pelvis", r.head("pelvis") + Vector((0, 0, -0.08)), 0.0))
    frame = spec["frame"]
    if frame in ("boat",):
        B = BOATS[spec["boat"]]
        k = 1.0 / B["scale"]
        y0, y1 = B["length"][0] * k, B["length"][1] * k
        for nm, q, rad in pts:
            if not (y0 < q.y < y1) or abs(q.x) > B["half_beam"] * k:
                continue
            floor = B["deck_z"] * k if abs(q.x) > B["cockpit_half"] * k else 0.0
            pen = floor - (q.z - rad * 0.8)
            if pen > 0.02:
                out.append((nm, round(pen * 100, 1)))
    elif frame in ("shore", "seat"):
        for nm, q, rad in pts:
            if nm.startswith(("foot", "calf")) and q.z - rad * 0.6 < -0.02:
                out.append((nm, round((rad * 0.6 - q.z) * 100, 1)))
    # forearms and hands not inside the trunk
    sp = [r.head(b) for b in ("pelvis", "spine_02", "spine_03", "neck_01")]
    for s in "lr":
        for b in ("lowerarm_", "hand_"):
            q = r.head(b + s)
            d = min(_seg_dist(q, sp[i], sp[i + 1]) for i in range(3))
            if d < 0.075:
                out.append((b + s + "~trunk", round((0.075 - d) * 100, 1)))
    return out


def _seg_dist(q, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (q - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return (q - (a + ab * t)).length


