"""One bird species, rigged and animated, from Content/BirdsData/species.json (size, wingspan, flight style) and
blender/birds/species_art.py (shape and plumage).

    Blender -b --python blender/birds/build_bird.py -- Rook [--previews] [--no-export] [--lods]

Generalises Murmuration's hero starling (tools/blender/starling_hero.py): the starling is the template, every species is
that bird warped (torso, neck, head, beak), with its own wing and tail lengths and shapes, legs, and its own feather atlas
painted from a field-guide description of each feather tract. Built at template size, then scaled so beak tip to tail
tip is the species' length and the spread wings its wingspan.

Coordinates while building: Y up, +Z forward, +X the bird's left, metres (to_blender() -> Blender X forward, Z up).
Origin = between the feet when perched. Bind pose: body level, wings spread, legs down.

Output (blender/birds/export/<Id>/): <Id>.glb (skinned mesh + clips), <Id>_rig.json (eye positions, sizes),
previews/*.png. Clips: Idle Look Alert Hop Walk Peck Eat Preen Call Takeoff Flap Glide Bound Land Sleep, 30 fps, in place.
Flap is one wingbeat over exactly 1 s (play at WingbeatHz); Hop and Walk one stride over 1 s.
"""
import json
import math
import os
import random
import sys
import zlib

sys.path.insert(0, os.path.dirname(__file__))
import bpy  # noqa: E402
import mathutils  # noqa: E402
import numpy as np  # noqa: E402

import bird_common as C  # noqa: E402
import plumage as PL  # noqa: E402
import species_art as ART  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SPECIES = argv[0] if argv else "Starling"
FLAGS = set(argv[1:])
FPS = 30
TABLE = {s["id"]: s for s in json.load(open(os.path.join(C.ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "species.json")))["species"]}
SP = TABLE[SPECIES]
A = ART.get(SPECIES)          # {"morph": {...}, "plumage": {...}}
M = dict(ART.MORPH_DEFAULTS, **A["morph"])
PLUM = A["plumage"]
rng = random.Random(zlib.crc32(SPECIES.encode()))   # stable across runs (hash() is salted)
OUTDIR = os.path.join(C.OUT, SPECIES)
# --hero: the body the player flies in: ~3-4x the feathers, a 4096 atlas, the same skeleton (SK_<Id>_Hero on SKEL_<Id>)
HERO = "--hero" in FLAGS
TAG = SPECIES + ("_Hero" if HERO else "")
if HERO:
    PL.CELL = {k: (w * 2, h * 2) for k, (w, h) in PL.CELL.items()}
os.makedirs(os.path.join(OUTDIR, "previews"), exist_ok=True)


# ====================================================================== small linear algebra
def V(*a):
    return np.array(a, dtype=float)


def nrm(v):
    v = np.asarray(v, float)
    return v / (np.linalg.norm(v) + 1e-12)


def rot_axis(axis, ang):
    axis = nrm(axis)
    x, y, z = axis
    c, s = math.cos(ang), math.sin(ang)
    C1 = 1 - c
    return np.array([[c + x * x * C1, x * y * C1 - z * s, x * z * C1 + y * s],
                     [y * x * C1 + z * s, c + y * y * C1, y * z * C1 - x * s],
                     [z * x * C1 - y * s, z * y * C1 + x * s, c + z * z * C1]])


def rx(a):
    return rot_axis((1, 0, 0), a)


def ry(a):
    return rot_axis((0, 1, 0), a)


def rz(a):
    return rot_axis((0, 0, 1), a)


def nose_up(deg):
    return rx(-math.radians(deg))


def frame_of(d, u):
    d = nrm(d)
    u = nrm(np.asarray(u, float) - d * np.dot(u, d))
    return np.column_stack([d, u, np.cross(d, u)])


def frame_map(d0, u0, d1, u1):
    return frame_of(d1, u1) @ frame_of(d0, u0).T


def rot_between(a, b):
    a, b = nrm(a), nrm(b)
    c = np.cross(a, b)
    s = np.linalg.norm(c)
    if s < 1e-9:
        return np.eye(3)
    return rot_axis(c, math.atan2(s, float(np.dot(a, b))))


def nlerp(a, b, t):
    return nrm(np.asarray(a) * (1 - t) + np.asarray(b) * t)


def smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


# ====================================================================== the template warp (starling -> species)
CYB = 0.0505                      # body centre height of the template
H0 = V(0, 0.0605, 0.064)          # template head centre
EYE_T = V(0.0104, 0.0628, 0.0688)  # template eye (left)
BEAK_Z0 = 0.0795                  # template beak base
LEGLIFT = 0.0                     # set by setup_legs so the feet stand on y = 0


def head_centre():
    return V(0, CYB + (H0[1] - CYB) * M["body_h"] + M["neck_up"], 0.032 * M["body_len"] + (H0[2] - 0.032) * M["neck"])


def warp(p):
    p = np.asarray(p, float)
    x, y, z = p
    body = V(x * M["body_w"], CYB + (y - CYB) * M["body_h"], z * M["body_len"])
    head = head_centre() + (p - H0) * M["head"]
    t = smooth(0.024, 0.058, z)   # a long blend: a short neck must not crease
    out = body * (1 - t) + head * t
    out[1] += LEGLIFT
    return out


def warp_dir(p, d, eps=1e-4):
    return nrm(warp(np.asarray(p) + eps * nrm(d)) - warp(np.asarray(p) - eps * nrm(d)))


# ====================================================================== skeleton (species space; scaled by S at the end)
BONES = {}


def bone(name, head, tail, parent):
    BONES[name] = (V(*head), V(*tail), parent)


NECKS = ["neck", "neck2", "neck3"][:max(1, min(3, M["n_neck"]))]


def resample(arr, n):
    """A per-feather list (lengths, a profile) stretched to n feathers."""
    return list(np.interp(np.linspace(0, 1, n), np.linspace(0, 1, len(arr)), arr))


# feather counts by family (species_art morph): songbirds 9 visible primaries, crows, gulls, pigeons, swifts, raptors and
# the cormorant 10; true secondaries 6 in songbirds up to ~17 in gulls and the cormorant; tail feather pairs 5 (swift) to 7
TF_N = M["n_tail"]
WING = {}       # SHOULDER ELBOW WRIST HANDTIP, P_LEN S_LEN
P_N = M["n_prim"]
S_N = M["n_sec"]         # true secondaries; + 3 tertials + 3 humerals
S_TOTAL = S_N + 6
P_LEN_T = resample([0.056, 0.058, 0.061, 0.064, 0.067, 0.070, 0.073, 0.075, 0.072], P_N)
S_LEN_T = resample([0.054, 0.049], S_N) + [0.045, 0.041, 0.037, 0.034, 0.031, 0.027]
SHOULDER_T, ELBOW_T, WRIST_T, HANDTIP_T = V(0.016, 0.057, 0.020), V(0.050, 0.058, 0.012), V(0.098, 0.059, 0.020), V(0.130, 0.059, 0.012)


def p_root(i):
    t = i / (P_N - 1.0)
    W = WING
    return W["WRIST"] + (W["HANDTIP"] - W["WRIST"]) * (0.12 + 0.88 * t) + V(0, 0.0007 * (P_N - 1 - i) * W["k"], 0)


def p_dir(i):
    b = math.radians(M["prim_angle0"] - i * M["prim_fan"] / (P_N - 1))
    return V(math.cos(b), 0, -math.sin(b))


def s_root(i):
    W = WING
    if i > S_N + 2:
        t = (i - S_N - 2) / 3.6
        return W["ELBOW"] + V(0.004, 0, 0) * W["k"] + (W["SHOULDER"] + V(0.004, 0, 0) * W["k"] - W["ELBOW"]) * t + V(0, 0.0004 * i - 0.0008, 0) * W["k"]
    t = i / (S_N + 2.0)
    return W["WRIST"] + (W["ELBOW"] + V(0.004, 0, 0) * W["k"] - W["WRIST"]) * t + V(0, 0.0004 * i - 0.0008, 0) * W["k"]


def s_dir(i):
    b = math.radians(78 + i * 20 / (S_N + 2))
    return V(math.cos(b), 0, -math.sin(b))


def tail_len(k):
    sh = M["tail_shape"]
    u = k / (TF_N - 1.0)
    m = 1.0
    if sh == "fork":
        m = 1 + M["fork"] * u ** 1.6
    elif sh == "round":
        m = 1 - 0.22 * u ** 2
    elif sh == "wedge":
        m = 1 - M["fork"] * u ** 1.1
    elif sh == "notch":
        m = 1 + 0.07 * u
    if k == TF_N - 1:
        m += M["streamer"]
    return (0.064 - 0.0012 * k) * M["tail"] * m


def build_skeleton(wk):
    """wk: wing scale factor (solved for the wingspan)."""
    BONES.clear()
    bone("root", (0, 0, 0), (0, 0.012, 0), None)
    bone("body", warp((0, 0.046, -0.018)), warp((0, 0.050, 0.028)), "root")
    # the neck: one bone, or a chain of 2-3 for long-necked birds (an S-curve needs more than one joint)
    a, b = V(0, 0.054, 0.034), V(0, 0.059, 0.052)
    parent = "body"
    for j, nm in enumerate(NECKS):
        bone(nm, warp(a + (b - a) * j / len(NECKS)), warp(a + (b - a) * (j + 1) / len(NECKS)), parent)
        parent = nm
    bone("head", warp((0, 0.059, 0.053)), warp((0, 0.062, 0.078)), parent)
    bone("jaw", warp((0, 0.0566, BEAK_Z0 - 0.002)), warp((0, 0.0560, BEAK_Z0 + 0.027 * M["beak_len"])), "head")
    # the eyes as bones: GetSocketLocation("eye_l") works on them (the bird's own camera)
    bone("eye_l", warp(EYE_T), warp(EYE_T + V(0.004, 0, 0)), "head")
    bone("eye_r", warp(EYE_T * V(-1, 1, 1)), warp(EYE_T * V(-1, 1, 1) + V(-0.004, 0, 0)), "head")
    bone("tail", warp((0, 0.050, -0.040)), warp((0, 0.050, -0.058)), "body")
    for k in range(TF_N):
        for side, sx in (("L", 1), ("R", -1)):
            a = math.radians((1.5 + 2.4 * k * 5.0 / (TF_N - 1)) * M["tail_fan"]) * sx
            h = warp((sx * (0.0015 + 0.0019 * k * 5.0 / (TF_N - 1)) * M["tail_width"], 0.0492 - 0.0003 * k, -0.043))
            d = V(math.sin(a), 0, -math.cos(a))
            bone(f"tf.{side}.{k + 1}", h, h + d * tail_len(k), "tail")
    W = WING
    W["k"] = wk
    W["SHOULDER"] = warp(SHOULDER_T)
    W["ELBOW"] = W["SHOULDER"] + (ELBOW_T - SHOULDER_T) * M["arm"] * wk
    W["WRIST"] = W["ELBOW"] + (WRIST_T - ELBOW_T) * M["arm"] * wk
    W["HANDTIP"] = W["WRIST"] + (HANDTIP_T - WRIST_T) * M["hand"] * wk
    prof = resample(M["prim_profile"], P_N)
    if M["p_outer"] != 1.0:    # the outermost primary (p10) of crows, pigeons and gulls: shorter than the next
        prof[-1] *= M["p_outer"]
    W["P_LEN"] = [P_LEN_T[i] * M["prim"] * wk * prof[i] for i in range(P_N)]
    W["S_LEN"] = [S_LEN_T[i] * M["sec"] * wk for i in range(S_TOTAL)]
    for side, sx in (("L", 1), ("R", -1)):
        Mx = np.diag([sx, 1, 1])
        bone(f"humerus.{side}", Mx @ W["SHOULDER"], Mx @ W["ELBOW"], "body")
        bone(f"forearm.{side}", Mx @ W["ELBOW"], Mx @ W["WRIST"], f"humerus.{side}")
        bone(f"hand.{side}", Mx @ W["WRIST"], Mx @ W["HANDTIP"], f"forearm.{side}")
        bone(f"primaries.{side}", Mx @ (W["WRIST"] + V(0.004, 0, -0.001) * wk), Mx @ (W["WRIST"] + V(0.014, 0, -0.003) * wk), f"hand.{side}")
        for i in range(P_N):
            r = p_root(i)
            bone(f"p{i + 1}.{side}", Mx @ r, Mx @ (r + p_dir(i) * W["P_LEN"][i]), f"primaries.{side}")
        for i in range(S_TOTAL):
            r = s_root(i)
            bone(f"s{i + 1}.{side}", Mx @ r, Mx @ (r + s_dir(i) * W["S_LEN"][i]), f"forearm.{side}" if i < S_N + 3 else f"humerus.{side}")
    setup_legs()


LEG = {}


def setup_legs():
    global LEGLIFT
    for side, sx in (("L", 1), ("R", -1)):
        hip = warp((sx * 0.010, 0.043, -0.004))
        knee = warp((sx * 0.0125, 0.036, 0.006))
        ankle = knee + V(-0.0007 * sx, -0.010, -0.011) * M["shin"]
        foot = ankle + V(0.0002 * sx, -0.0244, 0.008) * M["tarsus"]
        LEG[side] = [hip, knee, ankle, foot]
    # stand on y = 0
    lift = -LEG["L"][3][1] + 0.0016
    if abs(lift) > 1e-7:
        LEGLIFT += lift
        for side in LEG:
            LEG[side] = [p + V(0, lift, 0) for p in LEG[side]]
        return "again"
    for side, sx in (("L", 1), ("R", -1)):
        hip, knee, ankle, foot = LEG[side]
        bone(f"thigh.{side}", hip, knee, "body")
        bone(f"shin.{side}", knee, ankle, f"thigh.{side}")
        bone(f"tarsus.{side}", ankle, foot, f"shin.{side}")
        bone(f"toes.{side}", foot, foot + V(0, -0.0004, 0.019 * M["toes"]), f"tarsus.{side}")
        hd = FEET[M["foot"]](sx)[3][0]
        bone(f"hallux.{side}", foot, foot + V(math.sin(hd), -0.04, math.cos(hd)) * 0.011 * M["toes"] * M["hallux"], f"tarsus.{side}")


def order():
    out = []

    def add(n):
        if n in out:
            return
        p = BONES[n][2]
        if p:
            add(p)
        out.append(n)
    for n in BONES:
        add(n)
    return out


# ====================================================================== the plumage map
FALLBACK = {
    "forehead": "crown", "crown": "head", "supercilium": "crown", "eyestripe": "ear", "lore": "ear", "ear": "head",
    "malar": "ear", "nape": "head", "neckside": "nape", "head": "upper",
    "chin": "throat", "throat": "breast", "breast": "under", "breastside": "breast", "belly": "under", "vent": "belly",
    "flank": "belly", "mantle": "upper", "back": "mantle", "rump": "back", "uppertail": "rump", "scapular": "mantle",
    "undertail": "vent", "lcovert": "mantle", "mcovert": "lcovert", "gcovert": "mcovert", "pcovert": "gcovert",
    "alula": "pcovert", "underwing": "under", "axillary": "underwing", "tertial": "secondary", "secondary": "primary",
    "primary": "upper", "tail": "upper", "trousers": "flank", "crest": "crown",
}


class Q:
    """Where a feather sits, for the plumage rules (template coordinates)."""

    def __init__(self, z, th, region):
        self.z, self.th = z, th
        self.s = math.sin(th)
        self.c = abs(math.cos(th))
        p = surf(z, th)
        self.x, self.y = p[0], p[1]
        self.dz = z - EYE_T[2]
        self.dy = p[1] - EYE_T[1]
        self.region = region
        self.head = z > 0.050


def region_at(z, th):
    s, c = math.sin(th), abs(math.cos(th))
    p = surf(z, th)
    if z > 0.050:
        dz, dy = z - EYE_T[2], p[1] - EYE_T[1]
        if s < -0.45:
            return "chin" if z > 0.071 else "throat"
        if s > 0.62:
            return "forehead" if z > 0.073 else ("crown" if z > 0.057 else "nape")
        if dz > 0.0025 and -0.0035 < dy < 0.0022:
            return "lore"
        if 0.0022 <= dy < 0.0050 and dz > -0.011:
            return "supercilium" if z < 0.074 else "forehead"
        if -0.0016 < dy < 0.0022 and dz <= 0.0025:
            return "eyestripe" if dz > -0.012 else "neckside"
        if -0.0085 < dy <= -0.0016 and dz > -0.012:
            return "ear" if dz < 0.0035 else "malar"
        if dy <= -0.0085:
            return "malar" if dz > -0.006 else "throat"
        if dy >= 0.0050:
            return "crown" if z > 0.057 else "nape"
        return "neckside" if s < 0.2 else "nape"
    if z > 0.036:
        if s > 0.35:
            return "nape"
        if s < -0.35:
            return "throat" if z > 0.044 else "breast"
        return "neckside"
    if s > 0.35:
        return "mantle" if z > 0.012 else ("back" if z > -0.018 else ("rump" if z > -0.040 else "uppertail"))
    if s < -0.35:
        return "breast" if z > 0.012 else ("belly" if z > -0.025 else "vent")
    return "breastside" if z > 0.010 else "flank"


def resolve(region):
    r = region
    while r is not None:
        if r in PLUM["regions"]:
            return PLUM["regions"][r]
        r = FALLBACK.get(r)
    raise KeyError(f"{SPECIES}: no style for region {region} (define 'upper' and 'under')")


def style_for(z, th, jitter=0.0006):
    zz = z + rng.uniform(-jitter, jitter)
    q = Q(zz, th + rng.uniform(-0.05, 0.05) * (jitter > 0), None)
    q.region = region_at(q.z, q.th)
    for pred, name in PLUM.get("rules", []):
        if pred(q):
            return name
    return resolve(q.region)


def tract(name, i=None):
    """Style of a wing / tail tract feather (a name, or a list indexed by i): the tract's own entry in "tracts" or
    "regions", else its fallback's."""
    tr, rg = PLUM.get("tracts", {}), PLUM["regions"]
    r = name
    v = None
    while r is not None and v is None:
        v = tr.get(r, rg.get(r))
        r = FALLBACK.get(r)
    if v is None:
        raise KeyError(f"{SPECIES}: no style for tract {name}")
    if isinstance(v, (list, tuple)):
        # a list describes the tract feather by feather (p1 -> outermost, tail centre -> outer): stretch it over the
        # species' own feather count
        n = {"primary": P_N, "secondary": S_N, "tail": TF_N}.get(name, len(v))
        j = int(round((i or 0) * (len(v) - 1) / max(n - 1, 1))) if n != len(v) else (i or 0)
        return v[min(j, len(v) - 1)]
    return v


# ====================================================================== atlas
ATL = PL.Atlas(4096 if HERO else 2048)
SHAPE = M.get("feather_shapes", {})


def cell(kind, style, variant=0):
    if PLUM["styles"].get(style, {}).get("bare"):
        style = "__bare__" + style
    w, h = PL.CELL[kind if kind in PL.CELL else "contour"]
    key = f"{kind}:{style}:{variant}"
    if ATL.cells and key not in ATL.cells:   # packed already (a LOD built after LOD0): reuse another variant
        other = next((k for k in ATL.cells if k.startswith(f"{kind}:{style}:")), None)
        if other:
            return other
        raise KeyError(f"{SPECIES}: cell {key} not in the packed atlas")
    ATL.request(key, kind, style, w, h, zlib.crc32(key.encode()) % 100000, SHAPE.get(kind, {}))
    return key


def bare_cells():
    for k, (w, h) in {"body": (256, 256), "beak": (128, 128), "legs": (128, 64), "eye": (64, 64), "eyering": (32, 32)}.items():
        ATL.request(k, k, None, w, h, 0, {})


def paint_atlas(variant=""):
    if not variant:
        ATL.pack()
    n = ATL.size
    col = np.zeros((n, n, 3))
    alpha = np.zeros((n, n))
    height = np.zeros((n, n))
    rough = np.full((n, n), 0.55)
    irid = np.zeros((n, n))
    B = PLUM["bare"]
    for key, kind, style, w, h, seed, shape in ATL.requests:
        x0, y0, _, _ = ATL.cells[key]
        if kind == "body":
            c, a, hh, r, ir = paint_body(w, h)
        elif kind == "beak":
            c, a, hh, r, ir = paint_beak(w, h, B)
        elif kind == "legs":
            yy, xx = np.mgrid[0:h, 0:w]
            sc = 0.5 + 0.5 * np.sin(yy * 2 * math.pi / 8.0)
            c = PL.L(B["legs"]) * (0.82 + 0.18 * sc)[..., None]
            a, hh, r, ir = np.ones((h, w)), sc, np.full((h, w), 0.5), np.zeros((h, w))
        elif kind == "eye":
            yy, xx = np.mgrid[0:h, 0:w] + 0.5
            rr = np.hypot(xx / w - 0.5, yy / h - 0.5) * 2
            c = np.where((rr < 0.42)[..., None], PL.L("#050404"), PL.L(B["iris"]) * (0.8 + 0.2 * np.sin(np.arctan2(yy - h / 2, xx - w / 2) * 24))[..., None])
            c = np.where((rr > 0.95)[..., None], PL.L(B["iris"]) * 0.5, c)
            a, hh, r, ir = np.ones((h, w)), np.zeros((h, w)), np.full((h, w), 0.05), np.zeros((h, w))
        elif kind == "eyering":
            c = np.broadcast_to(PL.L(B.get("eyering", "#2a2522")), (h, w, 3)).copy()
            a, hh, r, ir = np.ones((h, w)), np.zeros((h, w)), np.full((h, w), 0.45), np.zeros((h, w))
        else:
            st = PLUM["styles"][style.replace("__bare__", "")]
            c, a, hh, r, ir = PL.paint_cell(kind, st, w, h, seed, shape)
        col[y0:y0 + h, x0:x0 + w] = c
        alpha[y0:y0 + h, x0:x0 + w] = a
        height[y0:y0 + h, x0:x0 + w] = hh
        rough[y0:y0 + h, x0:x0 + w] = r
        irid[y0:y0 + h, x0:x0 + w] = ir
    # colour under transparent texels: the nearest opaque colour, so mips don't halo
    col = bleed(col, alpha)
    srgb = C.lin_to_srgb(col)
    rgba = np.concatenate([srgb, alpha[..., None]], axis=2).astype(np.float32)
    albedo = save_image(f"T_{TAG}_Albedo" + (f"_{variant}" if variant else ""), rgba, alpha=True)
    if variant:
        return albedo
    nmap = C.normal_from_height(height, 0.35)
    normal = save_image(f"T_{TAG}_Normal", np.concatenate([nmap, np.ones((n, n, 1))], axis=2).astype(np.float32), data=True)
    params = np.stack([rough, irid, alpha, np.ones((n, n))], axis=2).astype(np.float32)
    par = save_image(f"T_{TAG}_Params", params, data=True)
    return albedo, normal, par


def bleed(col, alpha, it=6):
    c = col * alpha[..., None]
    w = alpha.copy()
    for _ in range(it):
        cs = sum(np.roll(np.roll(c, dy, 0), dx, 1) for dy in (-2, 0, 2) for dx in (-2, 0, 2))
        ws = sum(np.roll(np.roll(w, dy, 0), dx, 1) for dy in (-2, 0, 2) for dx in (-2, 0, 2))
        fill = (w < 0.5) & (ws > 0)
        c = np.where(fill[..., None], cs / np.maximum(ws, 1e-6)[..., None], c)
        w = np.where(fill, 1.0, w)
    return np.where(alpha[..., None] > 0.5, col, np.where(w[..., None] > 0, c, col))


def save_image(name, rgba, alpha=False, data=False):
    h, w = rgba.shape[:2]
    im = bpy.data.images.new(name, w, h, alpha=True, float_buffer=False)
    if data:
        im.colorspace_settings.name = "Non-Color"
    im.pixels.foreach_set(rgba.ravel())
    path = os.path.join(OUTDIR, name + ".png")
    im.filepath_raw = path
    im.file_format = "PNG"
    im.save()
    im.source = "FILE"
    im.reload()
    if data:
        im.colorspace_settings.name = "Non-Color"
    im.pack()
    im.alpha_mode = "STRAIGHT" if alpha else "NONE"
    return im


NZ_BODY, NA_BODY = (66, 42) if HERO else (44, 28)


def body_zs():
    return [-0.056 + (0.0825 + 0.056) * (0.5 - 0.5 * math.cos(math.pi * i / NZ_BODY)) for i in range(NZ_BODY + 1)]


def paint_body(w, h):
    """The skin under the feathers, coloured by tract (seen between feathers and on far LODs)."""
    zs = body_zs()
    c = np.zeros((h, w, 3))
    nz = C.tile_noise(max(w, h), 91, 1.2)[:h, :w]
    save = rng.getstate()
    for yi in range(h):
        z = zs[0] + (zs[-1] - zs[0]) * 0
        fz = yi / (h - 1) * NZ_BODY
        z = np.interp(fz, range(NZ_BODY + 1), zs)
        for xi in range(w):
            th = 2 * math.pi * (xi / (w - 1))
            st = PLUM["styles"][style_for(z, th, jitter=0.0)]
            c[yi, xi] = PL.L(st.get("skin", st["c"]))
    rng.setstate(save)
    c = c * (1 + 0.08 * nz)[..., None]
    return np.clip(c, 0, 1), np.ones((h, w)), 0.3 * nz, np.full((h, w), 0.6), np.zeros((h, w))


def paint_beak(w, h, B):
    """u around the beak (0..0.5 upper mandible, 0.5..1 lower; 0.95.. the mouth), v base -> tip."""
    yy, xx = np.mgrid[0:h, 0:w] + 0.5
    u, v = xx / w, yy / h
    base, tip = PL.L(B["beak"]), PL.L(B.get("beak_tip", B["beak"]))
    lower = PL.L(B.get("lower", B["beak"]))
    lower_tip = PL.L(B.get("lower_tip", B.get("beak_tip", B.get("lower", B["beak"]))))
    k = np.clip((v - B.get("tip_from", 0.55)) / 0.35, 0, 1)[..., None]
    up = base * (1 - k) + tip * k
    lo = lower * (1 - k) + lower_tip * k
    c = np.where((u < 0.5)[..., None], up, lo)
    if B.get("gonys"):  # gull's red spot near the tip of the lower mandible
        m = (u > 0.62) & (u < 0.88) & (v > 0.66) & (v < 0.86)
        c = np.where(m[..., None], PL.L(B["gonys"]), c)
    if B.get("base_skin"):  # pale bare base (rook)
        m = v < B.get("base_skin_len", 0.25)
        c = np.where(m[..., None], PL.L(B["base_skin"]), c)
    if B.get("cere"):  # pigeons: white cere on top at the base
        m = (u < 0.5) & (u > 0.12) & (u < 0.38) & (v < 0.32)
        c = np.where(m[..., None], PL.L(B["cere"]), c)
    mouth = u > 0.93
    c = np.where(mouth[..., None], PL.L(B.get("mouth", "#c86a6a")), c)
    c = c * (0.94 + 0.06 * np.sin(v * 40))[..., None]
    r = np.where(mouth, 0.4, B.get("beak_rough", 0.52))   # horn is matte
    return np.clip(c, 0, 1), np.ones((h, w)), 0.2 * np.sin(v * 40), r, np.zeros((h, w))


# ====================================================================== mesh builder with skin weights
PLUMAGE, BARE, EYE = range(3)


class SkinBuilder:
    def __init__(self):
        self.verts, self.weights, self.faces = [], [], []
        self.w = {"body": 1.0}

    def v(self, p, w=None):
        self.verts.append(tuple(float(c) for c in p))
        self.weights.append(dict(w if w is not None else self.w))
        return len(self.verts) - 1

    def f(self, idx, uvs, mat):
        self.faces.append((tuple(idx), mat, uvs))

    def mirror_from(self, start_v, start_f, rename):
        off = len(self.verts) - start_v
        for i in range(start_v, start_v + off):
            p = self.verts[i]
            self.verts.append((-p[0], p[1], p[2]))
            self.weights.append({rename(k): w for k, w in self.weights[i].items()})
        nf = len(self.faces)
        for j in range(start_f, nf):
            idx, m, uvs = self.faces[j]
            self.faces.append((tuple(reversed([i + off for i in idx])), m, list(reversed(uvs)) if uvs else None))


def swap_side(name):
    return name.replace(".L", ".R") if name.endswith(".L") or ".L." in name else name


def blend_w(z):
    def ramp(a, b):
        return min(1.0, max(0.0, (z - a) / (b - a)))
    w = {}
    t_tail = 1 - ramp(-0.050, -0.036)
    t_neck = ramp(0.026, 0.044)
    t_head = ramp(0.046, 0.058)
    wt = t_tail * 0.8
    wh = t_head
    wn = t_neck * (1 - t_head)
    wb = max(0.0, 1 - wt - wn - wh)
    for k, val in (("tail", wt), ("head", wh), ("body", wb)):
        if val > 1e-3:
            w[k] = val
    if wn > 1e-3:   # shared out along the neck chain by where along the neck the point is
        u = min(max((z - 0.034) / 0.018, 0.0), 0.9999) * len(NECKS) - 0.5
        for j, nm in enumerate(NECKS):
            share = max(0.0, 1.0 - abs(u - j)) if len(NECKS) > 1 else 1.0
            if j == 0 and u < 0:
                share = 1.0
            if j == len(NECKS) - 1 and u > j:
                share = 1.0
            if share > 1e-3:
                w[nm] = w.get(nm, 0.0) + wn * share
    return w


# ---------------------------------------------------------------- body surface (template space)
STATIONS = [  # z, half-width, half-height up, half-height down, centre y
    (-0.056, 0.006, 0.004, 0.004, 0.0525), (-0.050, 0.011, 0.0075, 0.008, 0.0525), (-0.040, 0.0162, 0.012, 0.0125, 0.0515),
    (-0.026, 0.0208, 0.0168, 0.0185, 0.0500), (-0.010, 0.0236, 0.0198, 0.0220, 0.0490), (0.006, 0.0242, 0.0208, 0.0230, 0.0495),
    (0.020, 0.0228, 0.0200, 0.0215, 0.0515), (0.032, 0.0192, 0.0180, 0.0180, 0.0545), (0.042, 0.0160, 0.0160, 0.0145, 0.0575),
    (0.052, 0.0150, 0.0150, 0.0128, 0.0595), (0.062, 0.0140, 0.0134, 0.0118, 0.0608), (0.070, 0.0118, 0.0108, 0.0098, 0.0602),
    (0.076, 0.0086, 0.0080, 0.0072, 0.0590), (0.0805, 0.0052, 0.0050, 0.0046, 0.0578), (0.0825, 0.0, 0.0, 0.0, 0.0575),
]


def station(z):
    zs = [s[0] for s in STATIONS]
    z = min(max(z, zs[0]), zs[-1])
    i = max(j for j in range(len(zs) - 1) if zs[j] <= z)
    i = min(i, len(zs) - 2)
    t = (z - zs[i]) / (zs[i + 1] - zs[i])
    P = [STATIONS[max(i - 1, 0)], STATIONS[i], STATIONS[i + 1], STATIONS[min(i + 2, len(STATIONS) - 1)]]
    out = []
    for k in range(1, 5):
        p0, p1, p2, p3 = (q[k] for q in P)
        out.append(0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    return out


def surf(z, th):
    hw, hu, hd, cy = station(z)
    c, s = math.cos(th), math.sin(th)
    kh = smooth(0.044, 0.056, z)          # the species' throat / crown fullness, blended in over the neck
    kb = 1 - smooth(-0.03, -0.012, z)     # and the belly, over the rear body (no steps: a step stands feathers up)
    hd *= 1 + (M["throat"] - 1) * kh + (M["belly"] - 1) * kb + M["keel"] * math.exp(-((z - 0.008) / 0.024) ** 2)
    hu *= 1 + (M["crown_h"] - 1) * kh
    if M["neck_thin"]:   # a slender neck between body and head (swan, cormorant)
        kn = M["neck_thin"] * math.exp(-((z - 0.043) / 0.008) ** 2)
        hw, hu, hd = hw * (1 - kn), hu * (1 - kn), hd * (1 - kn)
    if M["rear_taper"]:   # a slim, tapering rear (swift, cormorant)
        kr = 1 - M["rear_taper"] * smooth(-0.012, -0.05, z)
        hw, hu, hd = hw * kr, hu * kr, hd * kr
    hh = hu if s > 0 else hd
    ex = 2.3
    x = hw * np.sign(c) * abs(c) ** (2 / ex)
    y = cy + hh * np.sign(s) * abs(s) ** (2 / ex)
    if s > 0:
        y -= 0.0012 * s * M["flat_crown"] * kh
    return V(x, y, z)


def surf_normal(z, th):
    e = 1e-4
    a = surf(z + e, th) - surf(z - e, th)
    b = surf(z, th + e) - surf(z, th - e)
    n = nrm(np.cross(b, a))
    hw, hu, hd, cy = station(z)
    if np.dot(n, surf(z, th) - V(0, cy, z)) < 0:
        n = -n
    return n


def build_body(sb, lod=0):
    """The skin: rings along the body, sampled the same way the body texture is painted (lod 3: a coarse one)."""
    zall = body_zs()
    step = 4 if lod >= 3 else 1
    idx = list(range(0, NZ_BODY + 1, step))
    if idx[-1] != NZ_BODY:
        idx.append(NZ_BODY)
    na = 10 if lod >= 3 else NA_BODY
    rings = []
    for i in idx:
        z = zall[i]
        rings.append([sb.v(warp(surf(z, 2 * math.pi * k / na)), blend_w(z)) for k in range(na + 1)])
    for r in range(len(idx) - 1):
        v0, v1 = idx[r] / NZ_BODY, idx[r + 1] / NZ_BODY
        for k in range(na):
            uv = [ATL.uv_rect("body", k / na, v0), ATL.uv_rect("body", k / na, v1),
                  ATL.uv_rect("body", (k + 1) / na, v1), ATL.uv_rect("body", (k + 1) / na, v0)]
            sb.f((rings[r][k], rings[r + 1][k], rings[r + 1][k + 1], rings[r][k + 1]), uv, PLUMAGE)


def beak_section(t):
    """Beak half-width, upper height, lower depth, centre y, z at t (0 base .. 1 tip), template space."""
    Lb = 0.027 * M["beak_len"]
    z = BEAK_Z0 + t * Lb
    taper = max(0.0, 1 - t ** M["beak_round"]) ** M["beak_taper"]   # beak_round > 1: a fuller, convex bill
    hw = (0.0064 * taper + 0.0002) * M["beak_width"]
    hu = (0.0072 * taper + 0.0002) * M["beak_depth"] * 0.55
    hd = (0.0072 * taper + 0.0002) * M["beak_depth"] * 0.45
    cy = 0.0577 - 0.0016 * t - 0.0006 * t * t - M["beak_curve"] * t ** 2.2
    if M["beak_knob"] > 0:   # the mute swan's black knob on top of the bill at its base
        hu *= 1 + M["beak_knob"] * math.exp(-((t - 0.1) / 0.11) ** 2)
    if M["beak_gonys"] > 0:  # gulls: the lower mandible deepens to an angle near the tip
        hd *= 1 + M["beak_gonys"] * math.exp(-((t - 0.72) / 0.12) ** 2)
    return hw, hu, hd, cy, z


def build_beak_eyes(sb, lod=0):
    """Upper mandible on 'head', lower on 'jaw'; flat cutting edges meet at the gape line (mouth colour inside)."""
    n = 4 if lod >= 3 else 10
    nt = 4 if lod >= 3 else 16
    hook = M["beak_hook"]
    for half, (a0, a1), bonew, u0 in (("upper", (0.0, math.pi), {"head": 1.0}, 0.0), ("lower", (math.pi, 2 * math.pi), {"jaw": 1.0}, 0.5)):
        rings = []
        for i in range(nt + 1):
            t = i / nt
            hw, hu, hd, cy, z = beak_section(t)
            ring = []
            for k in range(n + 1):
                a = a0 + (a1 - a0) * k / n
                c, s = math.cos(a), math.sin(a)
                y = cy + (hu if s > 0 else hd) * s
                x = hw * c * (1 - 0.18 * max(s, 0))
                ring.append(sb.v(warp((x, y, z)), bonew))
            rings.append(ring)
        for i in range(nt):
            for k in range(n):
                uv = [ATL.uv_rect("beak", u0 + 0.45 * k / n, i / nt), ATL.uv_rect("beak", u0 + 0.45 * k / n, (i + 1) / nt),
                      ATL.uv_rect("beak", u0 + 0.45 * (k + 1) / n, (i + 1) / nt), ATL.uv_rect("beak", u0 + 0.45 * (k + 1) / n, i / nt)]
                q = (rings[i][k], rings[i + 1][k], rings[i + 1][k + 1], rings[i][k + 1])
                sb.f(q, uv, BARE)
        # tip: the upper mandible may hook over the lower
        hw, hu, hd, cy, z = beak_section(1.0)
        if half == "upper":
            tip = sb.v(warp((0, cy - 0.0006 - hook * 0.004, z + 0.0007 + hook * 0.0015)), bonew)
        else:
            tip = sb.v(warp((0, cy - 0.0004, z - hook * 0.0008)), bonew)
        for k in range(n):
            sb.f((rings[-1][k], tip, rings[-1][k + 1]), [ATL.uv_rect("beak", u0 + 0.45 * k / n, 0.98)] * 3, BARE)
        # the cutting edge face, mouth-coloured (seen when the beak opens)
        for i in range(nt):
            q = (rings[i][0], rings[i + 1][0], rings[i + 1][n], rings[i][n])
            sb.f(q if half == "lower" else tuple(reversed(q)), [ATL.uv_rect("beak", 0.97, i / nt)] * 4, BARE)
    # eyes
    for sx in (1, -1):
        c = V(sx * EYE_T[0], EYE_T[1], EYE_T[2])
        r = 0.0029 * M["eye"]
        nl, na = (3, 5) if lod >= 3 else (8, 12)
        grid = []
        for i in range(nl + 1):
            ph = -math.pi / 2 + math.pi * i / nl
            row = []
            for k in range(na + 1):
                d = V(sx * math.cos(ph) * math.cos(2 * math.pi * k / na), math.sin(ph), math.cos(ph) * math.sin(2 * math.pi * k / na))
                row.append((sb.v(warp(c + r * d), {"head": 1.0}), d))
            grid.append(row)
        for i in range(nl):
            for k in range(na):
                q = [grid[i][k], grid[i + 1][k], grid[i + 1][k + 1], grid[i][k + 1]]
                uv = [ATL.uv_rect("eye", 0.5 + 0.5 * d[2], 0.5 + 0.5 * d[1]) for _, d in q]
                idx = tuple(g[0] for g in q)
                sb.f(idx if sx < 0 else tuple(reversed(idx)), uv if sx < 0 else list(reversed(uv)), EYE)
        if lod >= 3:
            continue
        ring_pts = []
        rr = 0.0031 * M["eye"]
        wr = 0.82 - 0.25 * M["eyering_w"]
        for k in range(17):
            a = 2 * math.pi * k / 16
            ring_pts.append(c + V(sx * 0.0006, rr * math.cos(a), rr * math.sin(a)) * (1 + 0.3 * M["eyering_w"]))
        for k in range(16):
            a, b = ring_pts[k], ring_pts[k + 1]
            q = [sb.v(warp(a), {"head": 1.0}), sb.v(warp(b), {"head": 1.0}), sb.v(warp(c + (b - c) * wr + V(sx * 0.0008, 0, 0)), {"head": 1.0}),
                 sb.v(warp(c + (a - c) * wr + V(sx * 0.0008, 0, 0)), {"head": 1.0})]
            sb.f(tuple(q) if sx > 0 else tuple(reversed(q)), [ATL.uv_rect("eyering", 0.5, 0.5)] * 4, BARE)
    sb.w = {"body": 1.0}


def body_uv_at(z, th):
    """The body cell's texel for a spot on the body (template z, angle): tubes that should look like that plumage."""
    zs = body_zs()
    v = float(np.interp(z, zs, np.linspace(0, 1, len(zs))))
    return ATL.uv_rect("body", (th % (2 * math.pi)) / (2 * math.pi), v)


def tube(sb, pts, radii, weights, mat, n=8, uv_key=None, close=True, uv_const=None):
    rings = []
    for i, p in enumerate(pts):
        d = nrm(pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)])
        hint = V(0, 0, 1) if abs(d[2]) < 0.9 else V(0, 1, 0)
        s = nrm(np.cross(d, hint))
        u = nrm(np.cross(s, d))
        r = radii[i]
        r0, r1 = (r, r) if np.isscalar(r) else r
        rings.append([sb.v(p + s * r0 * math.cos(2 * math.pi * k / n) + u * r1 * math.sin(2 * math.pi * k / n), weights[i]) for k in range(n + 1)])
    for i in range(len(rings) - 1):
        for k in range(n):
            uv = None
            if uv_const is not None:
                uv = [uv_const] * 4
            elif uv_key:
                uv = [ATL.uv_rect(uv_key, k / n, i / len(rings)), ATL.uv_rect(uv_key, k / n, (i + 1) / len(rings)),
                      ATL.uv_rect(uv_key, (k + 1) / n, (i + 1) / len(rings)), ATL.uv_rect(uv_key, (k + 1) / n, i / len(rings))]
            sb.f((rings[i][k], rings[i + 1][k], rings[i + 1][k + 1], rings[i][k + 1]), uv, mat)
    if close:
        c0 = sb.v(pts[0], weights[0])
        c1 = sb.v(pts[-1], weights[-1])
        e0 = [uv_const or ATL.uv_rect(uv_key or "legs", 0.5, 0.0)] * 3
        e1 = [uv_const or ATL.uv_rect(uv_key or "legs", 0.5, 1.0)] * 3
        for k in range(n):
            sb.f((c0, rings[0][k + 1], rings[0][k]), e0, mat)
            sb.f((c1, rings[-1][k], rings[-1][k + 1]), e1, mat)


FEET = {   # (angle from forward, length, bone) per toe; the order is middle, inner, outer, then the fourth
    # most birds: three toes forward, the hallux back (anisodactyl)
    "aniso": lambda sx: [(0.0, 0.021, "toes"), (-sx * 0.42, 0.0155, "toes"), (sx * 0.45, 0.0165, "toes"), (math.pi, 0.0145 * M["hallux"], "hallux")],
    # swifts: four short toes that all reach forward, for clinging to walls (pamprodactyl)
    "pampro": lambda sx: [(sx * 0.12, 0.016, "toes"), (-sx * 0.25, 0.014, "toes"), (sx * 0.5, 0.0135, "toes"), (-sx * 0.62, 0.012, "hallux")],
    # cormorants: the hallux turned forward and inward, all four joined by webs (totipalmate)
    "toti": lambda sx: [(sx * 0.05, 0.021, "toes"), (-sx * 0.38, 0.018, "toes"), (sx * 0.45, 0.022, "toes"), (-sx * 0.95, 0.016, "hallux")],
}


def build_legs(sb, lod=0):
    lt = M["leg_thick"]
    for side, sx in (("L", 1), ("R", -1)):
        hip, knee, ankle, foot = LEG[side]
        tro = cell("contour", tract("trousers"), 0)
        if lod >= 3:   # far: a stick and three toes
            tube(sb, [knee, ankle, foot], [0.004 * lt, 0.0021 * lt, 0.0018 * lt], [{f"shin.{side}": 1}, {f"tarsus.{side}": 1}, {f"tarsus.{side}": 1}],
                 BARE, n=4, uv_key="legs", close=False)
            for ang in (0.0, -sx * 0.45, sx * 0.45):
                d = V(math.sin(ang), 0, math.cos(ang))
                tube(sb, [foot, foot + d * 0.018 * M["toes"]], [0.0013 * lt, 0.001 * lt], [{f"toes.{side}": 1}] * 2, BARE, n=3, uv_key="legs", close=False)
            continue
        # waders (bare_shin): the feathers stop part way down the shin, the rest is bare skin like the tarsus
        bs = M["bare_shin"]
        fe = ankle + V(0, 0.002, 0) * M["shin"] if bs <= 0 else knee + (ankle - knee) * (1 - bs)
        tube(sb, [hip, knee, fe], [0.006, 0.0052 * lt, (0.0034 if bs <= 0 else 0.0028) * lt],
             [{f"thigh.{side}": 1}, {f"thigh.{side}": 0.5, f"shin.{side}": 0.5}, {f"shin.{side}": 1}], PLUMAGE, n=10,
             uv_const=body_uv_at(-0.012, -math.pi * 0.35))   # the flank's plumage
        if bs > 0:
            tube(sb, [knee + (ankle - knee) * (1 - bs - 0.08), ankle], [0.0024 * lt, 0.0021 * lt], [{f"shin.{side}": 1}] * 2, BARE, n=8,
                 uv_key="legs")
        # feather fringe over the knee so the "trousers" read as feathers
        for k in range(5):
            a = 2 * math.pi * k / 5
            r = knee + V(sx * 0.004 * math.cos(a), -0.001, 0.004 * math.sin(a))
            card(sb, r, nrm(ankle - knee + V(0, 0, 0)), nrm(V(sx * math.cos(a), 0, math.sin(a))), 0.010 * M["shin"] ** 0.5 * M["trousers"],
                 0.008 * M["trousers"] ** 0.5, tro, {f"shin.{side}": 1}, segs=2, lift=0.0006 * M["trousers"])
        pts = [ankle + (foot - ankle) * t for t in np.linspace(0, 1, 9)]
        rad = [(0.0021 - 0.0003 * t + 0.00018 * (i % 2)) * lt for i, t in enumerate(np.linspace(0, 1, 9))]
        tube(sb, pts, rad, [{f"tarsus.{side}": 1}] * 9, BARE, n=8, uv_key="legs")
        toes = FEET[M["foot"]](sx)
        tips = []
        for (ang, Lt, bn) in toes:
            d = V(math.sin(ang), 0, math.cos(ang))
            Lt *= M["toes"]
            pts = [foot + d * Lt * t + V(0, -0.0003 * t, 0) for t in np.linspace(0, 1, 5)]
            tube(sb, pts, [r * lt for r in (0.0015, 0.0013, 0.0012, 0.0011, 0.0009)], [{f"{bn}.{side}": 1}] * 5, BARE, n=6, uv_key="legs")
            tipp = pts[-1]
            tips.append(tipp)
            cl = [tipp, tipp + d * 0.0022 * M["claw"] + V(0, -0.0006, 0), tipp + d * 0.0038 * M["claw"] + V(0, -0.0016, 0)]
            tube(sb, cl, [0.0009 * lt, 0.0006 * lt, 0.0001], [{f"{bn}.{side}": 1}] * 3, BARE, n=5, uv_key="eyering")
        if M["webbed"] or M["foot"] == "toti":  # webs between the front toes (the cormorant: all four), both faces
            pairs = ((3, 1), (1, 0), (0, 2)) if M["foot"] == "toti" else ((1, 0), (0, 2))
            for a_, b_ in pairs:
                for t0, t1 in ((0.0, 0.5), (0.5, 0.95)):
                    pa0, pa1 = foot + (tips[a_] - foot) * t0, foot + (tips[a_] - foot) * t1
                    pb0, pb1 = foot + (tips[b_] - foot) * t0, foot + (tips[b_] - foot) * t1
                    q = [sb.v(p + V(0, -0.0002, 0), {f"toes.{side}": 1}) for p in (pa0, pa1, pb1, pb0)]
                    uv = [ATL.uv_rect("legs", 0.5, 0.5)] * 4
                    sb.f(tuple(q), uv, BARE)
                    sb.f(tuple(reversed(q)), uv, BARE)
        sb.w = {"body": 1.0}


# ---------------------------------------------------------------- feather cards
SKIP = set(os.environ.get("BIRD_SKIP", "").split(","))   # debugging: leave out cards of these kinds


def card(sb, root, d, up, Lc, Wc, key, weights, segs=5, camber=0.0, flip=False, lift=0.0, tw=0.0, mat=PLUMAGE):
    if key.split(":")[0] in SKIP:
        return
    d, up = nrm(d), nrm(up)
    side = nrm(np.cross(d, up))
    up = nrm(np.cross(side, d))
    rows = []
    for i in range(segs + 1):
        t = i / segs
        row = []
        for s in (-1.0, 0.0, 1.0):
            p = root + d * Lc * t + side * (-s) * Wc * 0.5 + up * (camber * t * t + lift * t - 0.0005 * abs(s) * min(1, t * 3) * (Wc / 0.01) ** 0.5 + tw * s * t)
            if s == 0:
                p = p + up * 0.00025 * (Wc / 0.01) ** 0.5
            row.append(sb.v(p, weights))
        rows.append(row)
    for i in range(segs):
        for j in range(2):
            s0, s1 = -1 + j, j
            uv = [ATL.uv(key, s0, i / segs, flip), ATL.uv(key, s0, (i + 1) / segs, flip), ATL.uv(key, s1, (i + 1) / segs, flip), ATL.uv(key, s1, i / segs, flip)]
            sb.f((rows[i][j], rows[i + 1][j], rows[i + 1][j + 1], rows[i][j + 1]), uv, mat)


def spacing(points, i):
    a = points[max(i - 1, 0)]
    b = points[min(i + 1, len(points) - 1)]
    return np.linalg.norm(b - a) / (2 if 0 < i < len(points) - 1 else 1)


def build_wing(sb, lod=0):
    """Left wing (+X) in the bind (spread) pose; mirrored to the right."""
    W = WING
    k = W["k"]
    sv, sf = len(sb.verts), len(sb.faces)
    up = V(0, 1, 0)
    chain = [W["SHOULDER"] + V(-0.006, -0.001, 0), W["SHOULDER"], (W["SHOULDER"] + W["ELBOW"]) / 2, W["ELBOW"], (W["ELBOW"] + W["WRIST"]) / 2,
             W["WRIST"], (W["WRIST"] + W["HANDTIP"]) / 2, W["HANDTIP"]]
    wts = [{"body": 1}, {"humerus.L": 1}, {"humerus.L": 1}, {"humerus.L": 0.5, "forearm.L": 0.5}, {"forearm.L": 1},
           {"forearm.L": 0.5, "hand.L": 0.5}, {"hand.L": 1}, {"hand.L": 1}]
    kr = k ** 0.6
    rad = [(0.0055, 0.004), (0.0058, 0.0042), (0.006, 0.004), (0.0055, 0.0036), (0.005, 0.0033), (0.0045, 0.003), (0.0035, 0.0026), (0.0012, 0.0012)]
    chain = [c + V(0, -0.0012, 0) * kr for c in chain]
    tube(sb, chain, [(r[1] * kr * 0.62, r[0] * kr * 0.62) for r in rad], wts, PLUMAGE, n=4 if lod >= 3 else 10,
         uv_const=body_uv_at(0.015, math.pi * 0.3))   # the arm wears the shoulder's plumage
    fseg = 2 if lod >= 3 else None
    proots = [p_root(i) for i in range(P_N)]
    sroots = [s_root(i) for i in range(S_TOTAL)]
    pw = M["prim_width"]
    for i in range(P_N):
        Lp = W["P_LEN"][i]
        wc = max(Lp * 96 / 576 * 1.05 * pw, spacing(proots, i) * 2.0)
        card(sb, proots[i], p_dir(i), up, Lp, wc, cell("primary", tract("primary", i)), {f"p{i + 1}.L": 1}, camber=-0.004 * k, segs=fseg or (9 if HERO else 6))
    for i in range(S_TOTAL):
        kind = "tertial" if i >= S_N else "secondary"
        cw = max(W["S_LEN"][i] * (96 / 416 if kind == "tertial" else 96 / 480) * 1.08 * M["sec_width"], spacing(sroots, i) * 2.2)
        nm = tract(kind, i - S_N if kind == "tertial" else i)
        card(sb, sroots[i], s_dir(i), up, W["S_LEN"][i], cw, cell(kind, nm), {f"s{i + 1}.L": 1}, camber=-0.002 * k, segs=fseg or (8 if HERO else 5))
    ck = k * M["sec"]
    for i in range(S_TOTAL):
        r, d = sroots[i], s_dir(i)
        fwd = V(0, 0, 0.003) * ck
        gw = max(0.026 * 128 / 384 * 1.25 * ck, spacing(sroots, i) * 2.0)
        card(sb, r + fwd + V(0, 0.0022, 0) * ck, d, up, 0.026 * ck * M["covert"], gw, cell("gcovert", tract("gcovert", i)), {f"s{i + 1}.L": 1},
             camber=-0.0015 * ck, segs=fseg or 3)
        if lod >= 3:
            continue
        if lod <= 1:
            card(sb, r + fwd * 2.2 + V(0, 0.0034, 0) * ck, d, up, 0.016 * ck * M["covert"], max(0.0105 * ck, spacing(sroots, i) * 1.8),
                 cell("contour", tract("mcovert", i), i % 2), {f"s{i + 1}.L": 1}, camber=-0.001 * ck, segs=2)
            for row in range(2):
                card(sb, r + fwd * (3.4 + 1.2 * row) + V(0.001 * row, 0.0041 + 0.0006 * row, 0) * ck, d, up, (0.010 - 0.002 * row) * ck,
                     max(0.0075 * ck, spacing(sroots, i) * 1.6), cell("contour", tract("lcovert", i), (i + row) % 2), {f"s{i + 1}.L": 1},
                     camber=-0.0006 * ck, segs=2)
        card(sb, r + V(0, -0.0032, 0.001) * ck, d, V(0, -1, 0), 0.024 * ck, max(0.009 * ck, spacing(sroots, i) * 1.9),
             cell("underwing", tract("underwing", i)), {f"s{i + 1}.L": 1}, camber=0.001 * ck, segs=2)
    for i in range(P_N if lod < 3 else 0):
        r, d = proots[i], p_dir(i)
        sp = spacing(proots, i)
        card(sb, r + V(0, 0.0026, 0.002) * k - d * 0.002 * k, d, up, 0.022 * k * M["covert"], max(0.022 * 128 / 320 * 1.2 * k, sp * 2.0),
             cell("pcovert", tract("pcovert", i)), {f"p{i + 1}.L": 1}, camber=-0.001 * k, segs=3)
        if lod <= 1:
            card(sb, r + V(0, 0.0036, 0.004) * k - d * 0.003 * k, d, up, 0.011 * k, max(0.008 * k, sp * 1.6), cell("contour", tract("lcovert"), i % 2),
                 {f"p{i + 1}.L": 1}, segs=2)
        card(sb, r + V(0, -0.003, 0.001) * k, d, V(0, -1, 0), 0.02 * k, max(0.008 * k, sp * 1.8), cell("underwing", tract("underwing")), {f"p{i + 1}.L": 1}, segs=2)
    for j in range(3 if lod < 3 else 0):
        card(sb, W["WRIST"] + V(0.002 * j, 0.004 + 0.0004 * j, 0.004) * k, nrm(V(1, 0, 0.25 - 0.1 * j)), up, (0.016 - 0.002 * j) * k, 0.006 * k,
             cell("alula", tract("alula")), {"hand.L": 1}, segs=3)
    for row in range(2 if lod < 3 else 0):   # scapulars: two overlapping rows from the shoulder back over the folded wing's base
        for j in range(5):
            r = W["SHOULDER"] + V(-0.006 + 0.0035 * j + 0.002 * row, 0.0058 - 0.0008 * row, 0.006 - 0.007 * row) * M["body_w"]
            card(sb, r, nrm(V(0.04 * j - 0.06, -0.06 - 0.03 * row, -1)), V(0.35 + 0.1 * j, 1, 0), (0.018 - 0.002 * row) * M["body_len"] * M["fluff"],
                 0.0085 * M["body_w"], cell("contour", tract("scapular"), j % 2), {"body": 1.0}, camber=-0.0035, tw=0.0008 * (j - 2), segs=3)
    sb.mirror_from(sv, sf, swap_side)


def build_tail(sb, lod=0):
    for k in range(TF_N - 1, -1, -1):
        for side, sx in (("L", 1), ("R", -1)):
            h, t, _ = BONES[f"tf.{side}.{k + 1}"]
            d = t - h
            Lt = np.linalg.norm(d)
            wd = (0.064 - 0.0012 * k) * M["tail"] * 96 / 528 * 1.15 * M["tail_width"]
            if k == TF_N - 1 and M["streamer"] > 0:
                wd *= 0.8
            card(sb, h + V(0, 0.0004 * (TF_N - k), 0), d, V(0, 1, 0), Lt, wd, cell("tail", tract("tail", k)), {f"tf.{side}.{k + 1}": 1},
                 camber=0.0015, segs=2 if lod >= 3 else (6 if Lt > 0.07 else 5), flip=(sx < 0))
    if lod >= 3:
        return
    for k in range(6):
        for sx in (1, -1):
            r = warp(V(sx * 0.003 * k, 0.0455, -0.040))
            card(sb, r, nrm(V(sx * 0.1 * k, 0.05, -1)), V(0, -1, 0), (0.026 - 0.002 * k) * M["tail"] ** 0.5 * M["body_len"], 0.012 * M["body_w"],
                 cell("contour", tract("undertail"), k % 2), {"tail": 1}, segs=3)
    for k in range(5):  # uppertail coverts over the tail base
        for sx in (1, -1):
            r = warp(V(sx * 0.0028 * k, 0.0555, -0.036))
            card(sb, r, nrm(V(sx * 0.08 * k, -0.08, -1)), V(0, 1, 0), (0.022 - 0.002 * k) * M["tail"] ** 0.5 * M["body_len"], 0.012 * M["body_w"],
                 cell("contour", resolve("uppertail"), k % 2), {"tail": 1}, segs=2, lift=0.0005)


def build_contour(sb, lod=0):
    n = 0
    z = -0.052
    fl = M["fluff"] * (1.35 if lod == 1 else 1.0)
    while z < 0.0775:
        Lc = 0.017 if z < -0.02 else (0.015 if z < 0.03 else (0.011 if z < 0.05 else 0.0072))
        if z > 0.068:
            Lc = 0.0058
        Lc *= fl
        hw, hu, hd, cy = station(z)
        per = math.pi * (hw + (hu + hd) / 2)
        Wc = Lc * 0.8
        kk = max(6, int(per / (Wc * (0.22 if HERO else 0.45))))
        phase = rng.uniform(0, 1)
        for j in range(kk):
            th = 2 * math.pi * (j + phase + rng.uniform(-0.2, 0.2)) / kk
            zz = z + rng.uniform(-0.15, 0.15) * Lc
            p = surf(zz, th)
            if any(np.linalg.norm(p - V(sx * EYE_T[0], EYE_T[1], EYE_T[2])) < 0.0050 * M["eye"] for sx in (1, -1)):   # keep the eye clear
                continue
            if zz > 0.0785:
                continue
            stname = style_for(zz, th)
            st = PLUM["styles"][stname]
            if st.get("bare"):
                continue
            nn = surf_normal(zz, th)
            back = surf(zz - 0.002, th) - surf(zz + 0.002, th)
            down = V(0, -1, 0) * (0.35 if abs(math.sin(th)) < 0.6 else 0.0)
            flow = nrm(back / (np.linalg.norm(back) + 1e-9) + down)
            flow = nrm(flow - nn * np.dot(flow, nn))
            Ls = Lc * rng.uniform(0.85, 1.15)
            root = p - flow * Ls * 0.28 - nn * 0.0004
            # card in template space, then warped vertex by vertex: it follows the species' shape
            w = blend_w(zz)
            key = cell("contour", stname, rng.randrange(2))
            s0 = len(sb.verts)
            card(sb, root, flow, nn, Ls, Wc * rng.uniform(0.9, 1.1), key, w, segs=2, lift=(0.0005 + 0.0003 * rng.random()) * st.get("loft", 1.0),
                 flip=rng.random() < 0.5)
            if lod == 0 and zz < 0.070 and rng.random() < min(1.0, M["fluff_layer"] * (1.4 if HERO else 1.0)):
                # a plume over it: mostly loose barbs, lifted and turned a little, so the surface reads soft and
                # the silhouette fuzzy (the down and semiplumes showing between contour feathers)
                turn = rot_axis(nn, rng.uniform(-0.45, 0.45))
                card(sb, root + nn * 0.0007 + flow * Ls * 0.15, turn @ flow, nn, Ls * rng.uniform(0.65, 0.85), Wc * rng.uniform(0.85, 1.05),
                     cell("plume", stname, rng.randrange(2)), w, segs=2, lift=(0.0011 + 0.0006 * rng.random()) * st.get("loft", 1.0),
                     flip=rng.random() < 0.5)
            for vi in range(s0, len(sb.verts)):
                sb.verts[vi] = tuple(warp(sb.verts[vi]))
            n += 1
        z += Lc * ((0.19 if HERO else 0.33) if lod == 0 else 0.6)
    # crest (waxwing): long lanceolate feathers from the crown, swept back and up
    if M["crest"] > 0:
        for j in range(9):
            zz = 0.060 + 0.0022 * j
            th = math.pi / 2 + rng.uniform(-0.5, 0.5)
            p = surf(zz, th)
            Ls = 0.022 * M["crest"] * (0.6 + 0.4 * j / 8)
            s0 = len(sb.verts)
            card(sb, p - V(0, 0.0005, 0), nrm(V(0, 0.55, -1)), V(0, 1, 0.2), Ls, 0.0075 * M["crest"] ** 0.5, cell("crest", resolve("crest")), {"head": 1.0}, segs=3,
                 camber=-0.002)
            for vi in range(s0, len(sb.verts)):
                sb.verts[vi] = tuple(warp(sb.verts[vi]))
    return n


# ====================================================================== assemble
S = 1.0   # global scale (species space -> metres)
EXT = 1.0


def solve_size():
    """Template-space length -> S for the species length; then the wing factor for its wingspan."""
    global S
    build_skeleton(1.0)
    while setup_legs() == "again":
        pass
    tip = warp((0, 0.056, BEAK_Z0 + 0.027 * M["beak_len"] + 0.0007))[2]
    tail = min(BONES[f"tf.L.{k + 1}"][1][2] for k in range(TF_N))
    S = SP["lengthCm"] / 100.0 / (tip - tail)
    global EXT
    EXT = (tip - tail) / 0.20   # template length relative to the starling's: preview cameras step back by it
    reach1 = max((p_root(i) + p_dir(i) * WING["P_LEN"][i])[0] for i in range(P_N)) - WING["SHOULDER"][0]
    half = SP["wingspanCm"] / 100.0 / 2 / S
    wk = (half - WING["SHOULDER"][0]) / reach1
    build_skeleton(wk)
    while setup_legs() == "again":
        pass
    span = 2 * max((p_root(i) + p_dir(i) * WING["P_LEN"][i])[0] for i in range(P_N)) * S
    print(f"SIZE {SPECIES}: S={S:.3f} wing k={wk:.3f} length {SP['lengthCm']} cm span {span * 100:.1f} cm (want {SP['wingspanCm']})")


def make_materials(albedo, normal, params):
    def tex(nt, im, loc):
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = im
        t.location = loc
        return t
    mats = []
    for name, alpha, rough, spec in (("Plumage", True, 0.55, 0.35), ("Bare", False, 0.35, 0.5), ("Eye", False, 0.03, 0.9)):
        m = bpy.data.materials.new(f"M_{SPECIES}_{name}")
        m.use_nodes = True
        nt = m.node_tree
        p = C.principled(m)
        ta = tex(nt, albedo, (-600, 300))
        nt.links.new(ta.outputs["Color"], p.inputs["Base Color"])
        tp = tex(nt, params, (-600, -200))
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(tp.outputs["Color"], sep.inputs["Color"])
        nt.links.new(sep.outputs[0], p.inputs["Roughness"])
        tn = tex(nt, normal, (-600, 0))
        nmn = nt.nodes.new("ShaderNodeNormalMap")
        nmn.inputs["Strength"].default_value = 0.8
        nt.links.new(tn.outputs["Color"], nmn.inputs["Color"])
        nt.links.new(nmn.outputs["Normal"], p.inputs["Normal"])
        if alpha:
            nt.links.new(ta.outputs["Alpha"], p.inputs["Alpha"])
            if hasattr(m, "surface_render_method"):
                m.surface_render_method = "DITHERED"
            if "Thin Film Thickness" in p.inputs:  # iridescence where the params' G says so
                mul = nt.nodes.new("ShaderNodeMath")
                mul.operation = "MULTIPLY"
                mul.inputs[1].default_value = 480.0
                nt.links.new(sep.outputs[1], mul.inputs[0])
                nt.links.new(mul.outputs[0], p.inputs["Thin Film Thickness"])
                p.inputs["Thin Film IOR"].default_value = 1.6
            if "Subsurface Weight" in p.inputs:
                p.inputs["Subsurface Weight"].default_value = 0.0
            if "Sheen Weight" in p.inputs:
                p.inputs["Sheen Weight"].default_value = 0.0
        if "Specular IOR Level" in p.inputs:
            p.inputs["Specular IOR Level"].default_value = spec
        if name == "Eye" and "Coat Weight" in p.inputs:
            p.inputs["Coat Weight"].default_value = 1.0
            p.inputs["Coat Roughness"].default_value = 0.02
        mats.append(m)
    return mats


def build_mesh(mats, lod=0):
    sb = SkinBuilder()
    build_body(sb, lod)
    build_beak_eyes(sb, lod)
    build_legs(sb, lod)
    build_wing(sb, lod)
    build_tail(sb, lod)
    nc = build_contour(sb, lod) if lod <= 1 else 0
    print("contour feathers", nc, "lod", lod)
    return sb


def bake_mesh(sb, mats, name):
    me = bpy.data.meshes.new(name)
    me.from_pydata([C.to_blender(tuple(np.asarray(p) * S)) for p in sb.verts], [], [f[0] for f in sb.faces])
    uvl = me.uv_layers.new(name="UVMap")
    for poly, f in zip(me.polygons, sb.faces):
        poly.material_index = f[1]
        poly.use_smooth = True
        if f[2]:
            for li, uv in zip(poly.loop_indices, f[2]):
                uvl.data[li].uv = ATL.resolve(uv)
        else:
            for li in poly.loop_indices:
                uvl.data[li].uv = ATL.resolve(ATL.uv_rect("body", 0.5, 0.5))
    me.validate(clean_customdata=False)
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for m in mats:
        me.materials.append(m)
    groups = {n: ob.vertex_groups.new(name=n) for n in BONES}
    for i, w in enumerate(sb.weights):
        tot = sum(w.values()) or 1.0
        for k, val in w.items():
            groups[k].add([i], val / tot, "REPLACE")
    return ob


def build_armature():
    ad = bpy.data.armatures.new(f"{SPECIES}Rig")
    arm = bpy.data.objects.new(f"{SPECIES}Rig", ad)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name in order():
        h, t, parent = BONES[name]
        eb = ad.edit_bones.new(name)
        eb.head = C.to_blender(tuple(h * S))
        eb.tail = C.to_blender(tuple(t * S))
        eb.roll = 0.0
        if parent:
            eb.parent = ad.edit_bones[parent]
            eb.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


# ====================================================================== posing (same solver as the hero starling, plus jaw, head bob, leg IK)
TB = np.array([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1]], float)  # build coords -> blender
TB_INV = np.linalg.inv(TB)


def wing_targets(f):
    W = WING
    out = {}
    fold = {"humerus": (V(0.17, -0.25, -0.95), V(1, 0.1, 0)), "forearm": (V(-0.02, 0.10, 1.0), V(1, 0.0, 0)), "hand": (V(-0.02, 0.0, -1.0), V(1, 0.0, 0))}
    for b, (h0, h1) in (("humerus", (W["SHOULDER"], W["ELBOW"])), ("forearm", (W["ELBOW"], W["WRIST"])), ("hand", (W["WRIST"], W["HANDTIP"]))):
        d0 = nrm(h1 - h0)
        out[b] = (d0, V(0, 1, 0), nlerp(d0, fold[b][0], f), nlerp(V(0, 1, 0), fold[b][1], f))
    for i in range(P_N):
        d0 = p_dir(i)
        d1 = nrm(V(-0.055 - 0.003 * i, -0.09 - 0.004 * (P_N - 1 - i), -1))
        out[f"p{i + 1}"] = (d0, V(0, 1, 0), nlerp(d0, d1, f), nlerp(V(0, 1, 0), V(1, 0.12, 0), f))
    for i in range(S_TOTAL):
        d0 = s_dir(i)
        d1 = nrm(V(-0.05 + 0.002 * i, -0.16 + 0.01 * i, -1))
        out[f"s{i + 1}"] = (d0, V(0, 1, 0), nlerp(d0, d1, f), nlerp(V(0, 1, 0), V(1, 0.02 + 0.02 * i, 0), f))
    out["primaries"] = out["hand"]
    return out


def two_bone(K, F, a, b, pole):
    """Ankle position for thigh->knee K, foot F, shin length a, tarsus length b; bends toward pole."""
    d = F - K
    dl = np.linalg.norm(d)
    dl = min(dl, (a + b) * 0.999)
    dn = nrm(d)
    x = (a * a - b * b + dl * dl) / (2 * dl)
    hgt = math.sqrt(max(a * a - x * x, 0.0))
    pl = nrm(pole - dn * np.dot(pole, dn))
    return K + dn * x + pl * hgt


def solve_pose(P):
    R, H, D = {}, {}, {}

    def deform(b, Rw, head=None):
        h0 = BONES[b][0]
        par = BONES[b][2]
        if head is None:
            head = D[par](h0) if par else h0
        R[b], H[b] = Rw, head
        D[b] = lambda p, Rw=Rw, head=head, h0=h0: head + Rw @ (np.asarray(p) - h0)

    deform("root", np.eye(3))
    Rb = nose_up(P.get("pitch", 0.0)) @ rz(math.radians(P.get("roll", 0.0))) @ ry(math.radians(P.get("bodyyaw", 0.0)))
    deform("body", Rb, head=BONES["body"][0] + V(0, P.get("lift", 0.0), P.get("fwd", 0.0)))
    yaw = math.radians(P.get("yaw", 0.0))
    # the neck's bend is shared along the chain; neck_s curls it into an S (back at the base, forward above)
    n = len(NECKS)
    curl = [0.0] if n == 1 else ([-1.0, 1.0] if n == 2 else [-1.0, 2.0, -1.0])
    Rn = Rb
    for j, nm in enumerate(NECKS):
        Rn = Rn @ ry(yaw * 0.45 / n) @ nose_up(P.get("neck", 0.0) / n + P.get("neck_s", M["neck_s"]) * curl[j])
        deform(nm, Rn)
    hd = D[NECKS[-1]](BONES["head"][0]) + Rb @ V(0, P.get("headup", 0.0), P.get("headfwd", 0.0))
    deform("head", Rb @ ry(yaw) @ nose_up(P.get("head", 0.0)) @ rz(math.radians(P.get("tilt", 0.0))), head=hd)
    deform("jaw", R["head"] @ nose_up(-P.get("gape", 0.0)))
    deform("eye_l", R["head"])
    deform("eye_r", R["head"])
    Rt = Rb @ nose_up(P.get("tailpitch", 0.0)) @ ry(math.radians(P.get("tailyaw", 0.0)))
    deform("tail", Rt)
    spread = P.get("tailspread", 0.0)
    for k in range(TF_N):
        for side, sx in (("L", 1), ("R", -1)):
            roof = P.get("tailroof", 0.0) * max(0.0, 1.0 - spread)
            deform(f"tf.{side}.{k + 1}", Rt @ ry(-sx * math.radians(spread * (k + 0.5) * 3.2 * M["tail_fan"])) @ rz(-sx * math.radians(roof * (k + 0.5) * 6.5)))
    for side, sx in (("L", 1), ("R", -1)):
        f = P.get(f"fold{side}", P.get("fold", 1.0))
        el = math.radians(P.get(f"elev{side}", P.get("elev", 0.0)))
        sw = math.radians(P.get("sweep", 0.0))
        tw = math.radians(P.get("twist", 0.0))
        tg = wing_targets(f)
        G = rz(el) @ ry(sw) @ rx(tw)
        Ghand = rx(math.radians(P.get("handtwist", 0.0))) @ ry(math.radians(P.get("handsweep", 0.0))) @ rz(math.radians(P.get("handelev", 0.0)))
        Mx = np.diag([sx, 1, 1])
        for b in ["humerus", "forearm", "hand", "primaries"] + [f"p{i + 1}" for i in range(P_N)] + [f"s{i + 1}" for i in range(S_TOTAL)]:
            d0, u0, d1, u1 = tg[b]
            Rl = G @ frame_map(d0, u0, d1, u1)
            if (b.startswith("p") and b != "primaries") or b == "hand":
                Rl = G @ Ghand @ frame_map(d0, u0, d1, u1)
                if b.startswith("p") and P.get("fingers", 0.0):  # spread the primaries (soaring "fingers")
                    i = int(b[1:]) - 1
                    Rl = Rl @ ry(math.radians(P["fingers"] * (i - (P_N - 1) * 0.6) * 1.2 * (1 - f)))
            Rl = Mx @ Rl @ Mx
            deform(f"{b}.{side}", Rb @ Rl)
    perch = P.get("perch", 1.0)
    tuck = P.get("tuck", 0.0)
    reach = math.radians(P.get("reach", 0.0))
    for side, sx in (("L", 1), ("R", -1)):
        deform(f"thigh.{side}", Rb @ nose_up(-25 * tuck))
        knee = H[f"thigh.{side}"] + R[f"thigh.{side}"] @ (BONES[f"shin.{side}"][0] - BONES[f"thigh.{side}"][0])
        hip, k0, a0, f0 = LEG[side]
        a_len, b_len = np.linalg.norm(a0 - k0), np.linalg.norm(f0 - a0)
        crouch = P.get("crouch", 0.0)
        if perch > 0.5:
            step = P.get(f"step{side}", 0.0)        # foot forward (+) / back (-), template m
            raise_ = P.get(f"raise{side}", 0.0)     # foot lift
            foot = f0 + V(0, raise_, step) + V(0, 0, P.get("feetfwd", 0.0))
            ankle = two_bone(knee, foot, a_len, b_len, Rb @ V(0, -0.2, -1))
            if crouch:
                ankle = two_bone(knee, foot, a_len, b_len * 1.0, Rb @ V(0, -0.2, -1))
            d_rest = BONES[f"shin.{side}"][1] - BONES[f"shin.{side}"][0]
            deform(f"shin.{side}", rot_between(d_rest, ankle - knee), head=knee)
            t_rest = BONES[f"tarsus.{side}"][1] - BONES[f"tarsus.{side}"][0]
            deform(f"tarsus.{side}", rot_between(t_rest, foot - ankle), head=ankle)
            curl = math.radians(P.get(f"curl{side}", 0.0))
            deform(f"toes.{side}", rx(curl) @ rx(math.radians(P.get("toepitch", 0.0))), head=foot)
            deform(f"hallux.{side}", rx(-curl * 0.5), head=foot)
        else:
            # songbirds fold the feet forward under the belly; long-legged birds (fly_trail) stretch them back past the tail
            trail = M["fly_trail"] * tuck
            deform(f"shin.{side}", Rb @ nose_up(-30 * tuck * (1 - trail) - 80 * trail), head=knee)
            Rtar = Rb @ rx(math.radians(-66 * tuck * (1 - trail) + 92 * trail) + reach)
            deform(f"tarsus.{side}", Rtar)
            deform(f"toes.{side}", Rtar @ rx(math.radians(70 * tuck * (1 - trail) + 88 * trail)))
            deform(f"hallux.{side}", Rtar @ rx(math.radians(-40 * tuck * (1 - trail) - 122 * trail)))
    return R, H, P.get("breath", 0.0), P.get("fluffup", 0.0)


def apply_pose(arm, P, frame, prev):
    R, H, breath, fluff = solve_pose(P)
    rest = {b.name: np.array(b.matrix_local) for b in arm.data.bones}
    Dm = {}
    for b in BONES:
        T = np.eye(4)
        T[:3, :3] = R[b]
        T[:3, 3] = (H[b] - R[b] @ BONES[b][0]) * S
        Dm[b] = TB @ T @ TB_INV
    for b in BONES:
        pb = arm.pose.bones[b]
        par = BONES[b][2]
        Dp = Dm[par] if par else np.eye(4)
        basis = np.linalg.inv(rest[b]) @ np.linalg.inv(Dp) @ Dm[b] @ rest[b]
        loc, q, sc = mathutils.Matrix(basis.tolist()).decompose()
        if b in prev and prev[b].dot(q) < 0:
            q.negate()
        prev[b] = q.copy()
        pb.location = loc
        pb.rotation_quaternion = q
        pb.keyframe_insert("location", frame=frame)
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        if b == "body":
            s = 1 + fluff
            pb.scale = (s * (1 + breath * 0.3), s * (1 + breath * 0.6), s * (1 + breath))  # blender x = forward
            pb.keyframe_insert("scale", frame=frame)


def mixp(a, b, t):
    out = dict(a)
    for k in set(a) | set(b):
        va, vb = a.get(k, 0.0), b.get(k, 0.0)
        out[k] = va + (vb - va) * t
    return out


def ease(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


PERCH = dict(lift=-M["perch_low"], tailroof=1.0, pitch=M["perch_pitch"], neck=M["perch_neck"], neck_s=M["neck_s"], head=M["perch_head"], fold=1.0, perch=1.0, tailpitch=M["perch_tail"], tailspread=0.0)
# (herons fly with the neck drawn back into an S: fly_neck / fly_neck_s / fly_head, legs trailing: fly_trail)
FLIGHT = dict(pitch=0, neck=M["fly_neck"], neck_s=M["neck_s"] if M["fly_neck_s"] is None else M["fly_neck_s"], head=M["fly_head"], fold=0.0, perch=0.0, tuck=1.0, tailpitch=0, tailspread=0.0)
FS = SP["flight"]
FLAP = {  # amplitude (deg), offset, upstroke fold, hand flick
    "Flapping": (52, -8, 0.38, -14), "Bounding": (62, -4, 0.55, -22), "Undulating": (54, -6, 0.42, -16),
    "Clapping": (64, 2, 0.30, -10), "Soaring": (40, -6, 0.32, -10), "Flicker": (34, -10, 0.18, -26),
}[FS]


def flap_pose(ph):
    amp, off, upf, flick = FLAP
    c = math.cos(2 * math.pi * ph)
    p = dict(FLIGHT)
    p["elev"] = amp * c + off
    p["sweep"] = 6 * math.sin(2 * math.pi * ph) + M["glide_sweep"] * 0.3
    p["twist"] = -10 * math.sin(2 * math.pi * ph)
    up_t = 0.0 if ph < 0.55 else math.sin(math.pi * (ph - 0.55) / 0.45)
    p["fold"] = upf * up_t
    p["handtwist"] = flick * up_t
    p["lift"] = -0.0025 * math.sin(2 * math.pi * ph)
    p["tailspread"] = 0.5
    return p


def glide_pose(t):
    g = dict(FLIGHT, elev=M["glide_elev"] + 2.0 * math.sin(2 * math.pi * t), sweep=M["glide_sweep"], handsweep=M["glide_hand"], twist=-2,
             tailspread=0.9 if FS not in ("Flicker",) else 0.3, roll=2.5 * math.sin(2 * math.pi * t), head=FLIGHT["head"] + 1.5 * math.sin(2 * math.pi * t),
             fingers=M["fingers"], handelev=M["glide_handelev"])
    return g


def make_action(arm, name, keys):
    act = bpy.data.actions.new(f"{name}")
    act.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = act
    prev = {}
    for fr, P in keys:
        apply_pose(arm, P, fr, prev)
    try:
        for layer in act.layers:
            for strip in layer.strips:
                for cb in strip.channelbags:
                    for fc in cb.fcurves:
                        for kp in fc.keyframe_points:
                            kp.interpolation = "LINEAR"
    except AttributeError:
        for fc in act.fcurves:
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
    act.frame_range = (keys[0][0], keys[-1][0])
    act.use_frame_range = True
    return act


def clip(arm, name, seconds, fn, step=1):
    """fn(t 0..1, frame) -> pose; keys every `step` frames at 30 fps."""
    n = max(2, int(round(seconds * FPS)))
    keys = [(i, fn(i / n)) for i in list(range(0, n, step)) + [n]]
    return make_action(arm, name, keys)


def keyed(track, t):
    """Piecewise-eased value from [(t, v), ...]."""
    for (t0, a0), (t1, a1) in zip(track, track[1:]):
        if t0 <= t <= t1:
            return a0 + (a1 - a0) * ease((t - t0) / max(1e-6, t1 - t0))
    return track[-1][1]


def build_actions(arm):
    acts = []
    gait = SP["gait"]
    # Idle: 4 s loop: breathing, small head turns (one eye then the other), a tail flick
    yaw = [(0, 0), (0.15, 0), (0.22, 30), (0.42, 30), (0.48, -10), (0.62, -10), (0.68, -35), (0.85, -35), (0.93, 0), (1, 0)]

    def idle(t):
        p = dict(PERCH)
        p["breath"] = 0.02 * math.sin(2 * math.pi * t * 7)
        p["yaw"] = keyed(yaw, t)
        p["tilt"] = 0.3 * p["yaw"]
        p["head"] = PERCH["head"] + 3 * math.sin(2 * math.pi * t * 2)
        flick = math.exp(-((t - 0.72) / 0.02) ** 2) * M["tail_flick"]
        p["tailpitch"] = PERCH["tailpitch"] + 18 * flick
        if M["tail_wag"]:  # wagtail: the tail pumps all the time
            p["tailpitch"] = PERCH["tailpitch"] + 14 * max(0, math.sin(2 * math.pi * t * 6))
        p["fold"] = 1.0 - 0.05 * flick
        return p
    acts.append(clip(arm, "Idle", 4.0, idle))

    # Look: quick head jerks, one eye on the thing, tilted
    look = [(0, 0), (0.08, 55), (0.30, 55), (0.36, -40), (0.55, -40), (0.6, 20), (0.8, 20), (0.86, 0), (1, 0)]

    def lookf(t):
        p = dict(PERCH)
        p["yaw"] = keyed(look, t)
        p["tilt"] = 0.45 * p["yaw"]
        p["neck"] = PERCH["neck"] + 6
        p["breath"] = 0.02 * math.sin(2 * math.pi * t * 5)
        return p
    acts.append(clip(arm, "Look", 3.0, lookf))

    # Alert: tall, sleek, tail up, small jerks
    def alert(t):
        p = dict(PERCH, pitch=PERCH["pitch"] + 12, neck=PERCH["neck"] + 16, head=PERCH["head"] - 4, tailpitch=PERCH["tailpitch"] - 6,
                 fluffup=-0.04, headup=0.003)
        p["yaw"] = 18 * math.sin(2 * math.pi * t * 2) * (abs(math.sin(2 * math.pi * t * 2)) > 0.6)
        p["breath"] = 0.03 * math.sin(2 * math.pi * t * 10)
        return p
    acts.append(clip(arm, "Alert", 1.5, alert))

    # Hop: one hop over 1 s (in place): crouch, spring up, land
    def hop(t):
        p = dict(PERCH)
        if t < 0.25:
            p["crouch"] = ease(t / 0.25)
            p["pitch"] = PERCH["pitch"] - 8 * ease(t / 0.25)
            p["lift"] = PERCH["lift"] - 0.004 * ease(t / 0.25)
        elif t < 0.8:
            u = (t - 0.25) / 0.55
            air = math.sin(math.pi * u)
            p.update(perch=0.0, tuck=0.35 * air, lift=PERCH["lift"] + 0.022 * air, pitch=PERCH["pitch"] - 8 + 4 * air, fold=0.92, elev=6 * air,
                     tailpitch=PERCH["tailpitch"] + 10 * air, reach=-20 * air)
        else:
            u = (t - 0.8) / 0.2
            p["lift"] = PERCH["lift"] - 0.004 * (1 - ease(u))
        return p
    acts.append(clip(arm, "Hop", 1.0, hop))

    # Walk: one stride over 1 s, feet alternate; pigeons and others with a head bob hold the head still, then thrust it
    stride = 0.020

    def walk(t):
        p = dict(PERCH)
        for side, ph in (("L", 0.0), ("R", 0.5)):
            u = (t + ph) % 1.0
            if u < 0.5:   # stance: the foot slides back under the body
                p[f"step{side}"] = stride * (0.5 - 2 * u)
                p[f"raise{side}"] = 0.0
                p[f"curl{side}"] = 0.0
            else:         # swing: lift and bring forward
                w = (u - 0.5) / 0.5
                p[f"step{side}"] = stride * (-0.5 + w)
                p[f"raise{side}"] = 0.006 * math.sin(math.pi * w)
                p[f"curl{side}"] = 35 * math.sin(math.pi * w)
        p["lift"] = PERCH["lift"] + 0.0015 * abs(math.sin(2 * math.pi * t))
        p["roll"] = 4 * math.sin(2 * math.pi * t)
        if M["head_bob"]:
            for k in (0.0, 0.5):
                u = (t - k) % 0.5 / 0.5
                if u < 0.6:
                    p["headfwd"] = M["head_bob"] * (-u / 0.6 + 0.5)
                else:
                    p["headfwd"] = M["head_bob"] * (-0.5 + (u - 0.6) / 0.4)
        p["tailpitch"] = PERCH["tailpitch"] + 4 * math.sin(4 * math.pi * t)
        return p
    acts.append(clip(arm, "Walk", 1.0, walk))

    # Peck: 0.6 s, body tips, neck and head jab
    def peck(t):
        k = math.sin(math.pi * min(1, t / 0.75)) if t < 0.75 else 0.0
        jab = ease(t / 0.35) if t < 0.5 else 1 - ease((t - 0.5) / 0.3)
        return dict(PERCH, pitch=PERCH["pitch"] - M["peck_depth"] * k, neck=PERCH["neck"] - 30 * jab, head=PERCH["head"] - 25 * jab,
                    tailpitch=PERCH["tailpitch"] + 20 * k, crouch=0.4 * k, gape=8 * math.exp(-((t - 0.42) / 0.06) ** 2))
    acts.append(clip(arm, "Peck", 0.6, peck))

    # Eat: peck, lift, three mandible works, swallow (2 s)
    def eat(t):
        if t < 0.3:
            return peck(t / 0.3 * 0.999)
        u = (t - 0.3) / 0.7
        p = dict(PERCH, head=PERCH["head"] + 10 * ease(u * 3), neck=PERCH["neck"] + 6 * ease(u * 3))
        p["gape"] = 14 * max(0.0, math.sin(2 * math.pi * u * 3)) * (u < 0.75)
        p["headup"] = 0.002 * math.exp(-((u - 0.85) / 0.05) ** 2)
        p["breath"] = 0.02 * math.sin(2 * math.pi * u * 4)
        return p
    acts.append(clip(arm, "Eat", 2.0, eat))

    # Preen: head turns back to the wing, the beak works through the feathers, the wing lifts a little (3 s)
    def preen(t):
        e = ease(t / 0.15) * (1 - ease((t - 0.85) / 0.15))
        p = dict(PERCH, yaw=150 * e, neck=PERCH["neck"] - 20 * e, head=PERCH["head"] - 25 * e + 6 * e * math.sin(2 * math.pi * t * 6),
                 tilt=25 * e, elevL=8 * e * max(0, math.sin(2 * math.pi * t * 2)), foldL=1 - 0.12 * e, fluffup=0.04 * e)
        p["gape"] = 6 * e * max(0, math.sin(2 * math.pi * t * 9))
        return p
    acts.append(clip(arm, "Preen", 3.0, preen))

    # Call: head up, beak open, the body pumps (1 s)
    def call(t):
        e = math.sin(math.pi * t)
        return dict(PERCH, head=PERCH["head"] + 18 * e, neck=PERCH["neck"] + 8 * e, gape=M["call_gape"] * max(0, math.sin(2 * math.pi * t * 3)) * e,
                    tailpitch=PERCH["tailpitch"] - 8 * e, breath=0.04 * math.sin(2 * math.pi * t * 6))
    acts.append(clip(arm, "Call", 1.0, call))

    # Takeoff: 0.5 s, crouch, spring, wings open into the first downstroke; ends in flight pose (flap phase 0)
    def takeoff(t):
        if t < 0.25:
            u = ease(t / 0.25)
            return mixp(PERCH, dict(PERCH, crouch=1.0, pitch=10, fold=0.85, elev=10, tailpitch=-2, lift=-0.004), u)
        if t < 0.5:
            u = ease((t - 0.25) / 0.25)
            p = mixp(dict(PERCH, crouch=1.0, pitch=10, fold=0.85, elev=10, tailpitch=-2, perch=0.0),
                     dict(FLIGHT, pitch=14, elev=FLAP[0] + 4, fold=0.1, sweep=-10, tuck=0.2, lift=0.02, tailspread=1.2), u)
            p["perch"] = 0.0
            p["tuck"] = 0.2 * u
            return p
        u = (t - 0.5) / 0.5
        p = mixp(dict(FLIGHT, pitch=14, elev=FLAP[0] + 4, fold=0.1, sweep=-10, tuck=0.2, lift=0.02, tailspread=1.2), flap_pose(0.0), ease(u))
        p["elev"] = (FLAP[0] + 4) * (1 - ease(u)) + flap_pose(0.0)["elev"] * ease(u) - 50 * math.sin(math.pi * u)
        p["perch"] = 0.0
        return p
    acts.append(clip(arm, "Takeoff", 0.5, takeoff))

    acts.append(clip(arm, "Flap", 1.0, flap_pose))
    acts.append(clip(arm, "Glide", 2.0, glide_pose, step=3))

    # Bound: the closed-wing dip of tits and finches (and a swift's flick between glides opens wings instead)
    def bound(t):
        return dict(FLIGHT, fold=1.0, elev=0, pitch=-4 + 3 * math.sin(2 * math.pi * t), tailspread=0.0, tuck=1.0)
    acts.append(clip(arm, "Bound", 1.0, bound, step=3))

    # Land: 0.7 s, glide -> flare -> perched
    def land(t):
        if t < 0.45:
            u = ease(t / 0.45)
            p = mixp(glide_pose(0), dict(FLIGHT, elev=48, sweep=-22, twist=24, pitch=38, tailspread=2.2, tailpitch=-24, neck=-24, head=-14,
                                         tuck=0.0, reach=-40), u)
            p["perch"] = 0.0
            p["tuck"] = 1.0 - u
            return p
        if t < 0.7:
            u = (t - 0.45) / 0.25
            p = dict(FLIGHT, elev=48 * math.cos(math.pi * u) - 4, sweep=-22 + 10 * u, twist=24 - 20 * u, pitch=38 - 6 * u, tailspread=2.2,
                     tailpitch=-24, neck=-24, head=-14, tuck=0.0, reach=-40 + 40 * u, perch=0.0, fold=0.15 * u)
            p["lift"] = 0.012 * (1 - u)
            return p
        u = ease((t - 0.7) / 0.3)
        p = mixp(dict(PERCH, elev=-4, pitch=32, tailspread=1.5, tailpitch=-20, fold=0.25, crouch=1.0, neck=-24, head=-14), PERCH, u)
        p["perch"] = 1.0
        return p
    acts.append(clip(arm, "Land", 0.7, land))

    # Sleep: head turned back into the scapulars, fluffed (4 s loop, slow breathing)
    def sleep(t):
        return dict(PERCH, pitch=PERCH["pitch"] - 6, yaw=165, neck=PERCH["neck"] - 24, head=PERCH["head"] - 30, tilt=30, fluffup=0.07,
                    breath=0.025 * math.sin(2 * math.pi * t * 2), lift=PERCH["lift"] - 0.004, crouch=1.0)
    acts.append(clip(arm, "Sleep", 4.0, sleep, step=2))
    return acts


# ====================================================================== previews
def render(name, cam, tgt, lens=50, res=(900, 700), bg=(0.55, 0.6, 0.7)):
    sc = bpy.context.scene
    C.set_engine(sc)
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.view_settings.view_transform = "AgX"
    w = sc.world or bpy.data.worlds.new("W")
    sc.world = w
    w.use_nodes = True
    b = next(n for n in w.node_tree.nodes if n.type == "BACKGROUND")
    b.inputs["Color"].default_value = (*bg, 1)
    b.inputs["Strength"].default_value = 0.9
    cd = bpy.data.cameras.get("PCam") or bpy.data.cameras.new("PCam")
    cd.lens = lens
    cd.clip_start = 0.002
    co = bpy.data.objects.get("PCam") or bpy.data.objects.new("PCam", cd)
    if co.name not in sc.collection.objects:
        sc.collection.objects.link(co)
    cam = np.asarray(tgt) + (np.asarray(cam) - np.asarray(tgt)) * EXT
    co.location = C.to_blender(tuple(np.asarray(cam) * S))
    d = mathutils.Vector(C.to_blender(tuple(np.asarray(tgt) * S))) - co.location
    co.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    sc.camera = co
    if not bpy.data.objects.get("PSun"):
        sd = bpy.data.lights.new("PSun", "SUN")
        sd.energy = 3.5
        sd.color = (1.0, 0.92, 0.82)
        sun = bpy.data.objects.new("PSun", sd)
        sc.collection.objects.link(sun)
        sun.rotation_euler = mathutils.Vector(C.to_blender((-0.4, -0.75, -0.5))).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = os.path.join(OUTDIR, "previews", name + ".png")
    bpy.ops.render.render(write_still=True)
    print("PREVIEW", sc.render.filepath)


def show(arm, act, frame):
    arm.animation_data.action = act
    bpy.context.scene.frame_set(int(frame))


def main():
    C.reset()
    sc = bpy.context.scene
    sc.render.fps = FPS
    solve_size()
    bare_cells()
    sb = build_mesh(None, 0)
    albedo, normal, params = paint_atlas()
    # the same cells recoloured: an albedo per plumage variant (winter, female, female in winter)
    for suffix, parts in (("Winter", ["winter"]), ("Female", ["female"]), ("FemaleWinter", ["female", "winter"])):
        if not all(PLUM.get(p) for p in parts):
            continue
        base = (dict(PLUM["styles"]), dict(PLUM["bare"]))
        for p in parts:
            PLUM["styles"].update(PLUM[p].get("styles", {}))
            PLUM["bare"].update(PLUM[p].get("bare", {}))
        paint_atlas(variant=suffix)
        PLUM["styles"], PLUM["bare"] = base
    mats = make_materials(albedo, normal, params)
    ob = bake_mesh(sb, mats, f"SK_{SPECIES}")
    arm = build_armature()
    ob.parent = arm
    mod = ob.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    ob.data.calc_loop_triangles()
    print("TRIS", len(ob.data.loop_triangles), "ATLAS", ATL.size)
    R, H, _, _ = solve_pose(PERCH)
    tipz = min(H[f"p{i + 1}.L"][2] + (R[f"p{i + 1}.L"] @ (BONES[f"p{i + 1}.L"][1] - BONES[f"p{i + 1}.L"][0]))[2] for i in range(P_N))
    tailz = min(H[f"tf.L.{k + 1}"][2] + (R[f"tf.L.{k + 1}"] @ (BONES[f"tf.L.{k + 1}"][1] - BONES[f"tf.L.{k + 1}"][0]))[2] for k in range(TF_N))
    print(f"FOLD {SPECIES}: folded wingtip {tipz * S * 100:.1f} cm, tail tip {tailz * S * 100:.1f} cm (z, forward +)")
    acts = build_actions(arm)
    Acts = {a.name: a for a in acts}
    # rig facts for the importer (cm, Unreal axes: X forward, Y right, Z up)
    def ue(p):
        b = C.to_blender(tuple(np.asarray(p) * S))
        return [b[0] * 100, -b[1] * 100, b[2] * 100]
    rig = {"species": SPECIES, "scale": S, "eye_l": ue(warp(EYE_T)), "eye_r": ue(warp(EYE_T * V(-1, 1, 1))),
           "head": ue(BONES["head"][0]), "lengthCm": SP["lengthCm"], "wingspanCm": SP["wingspanCm"], "tris": len(ob.data.loop_triangles),
           "atlas": ATL.size, "clips": sorted(Acts), "gloss_hue": M.get("gloss_hue", 0.0)}
    json.dump(rig, open(os.path.join(OUTDIR, f"{TAG}_rig.json"), "w"), indent=1)
    if "--debug-top" in FLAGS:
        show(arm, Acts["Idle"], 0)
        dg = bpy.context.evaluated_depsgraph_get()
        ev = ob.evaluated_get(dg).data
        from collections import Counter
        pts = sorted(((v.co.z, i) for i, v in enumerate(ev.vertices)), reverse=True)[:60]
        names = Counter()
        for z, i in pts:
            g = max(ob.data.vertices[i].groups, key=lambda g: g.weight)
            names[ob.vertex_groups[g.group].name] += 1
        print("TOP", [(round(z, 4)) for z, i in pts[:5]], names.most_common(6))
    if "--previews" in FLAGS:
        L0 = SP["lengthCm"] / 100 / S
        show(arm, Acts["Idle"], 0)
        render(f"{SPECIES}_side", (-0.30, 0.06, 0.012), (0, 0.05, 0.006), lens=50, res=(900, 700))
        if "--head" in FLAGS:
            hc = warp(H0)
            render(f"{SPECIES}_head", tuple(hc + V(-0.06, 0.03, 0.05)), tuple(hc), lens=50, res=(700, 700))
        render(f"{SPECIES}_34", (0.2, 0.11, 0.21), (0, 0.05, 0.005), lens=50)
        if "--all-views" in FLAGS:
            render(f"{SPECIES}_front", (0.0, 0.08, 0.32), (0, 0.05, 0.0), lens=50)
            render(f"{SPECIES}_back", (0.12, 0.16, -0.22), (0, 0.05, -0.01), lens=50)
        show(arm, Acts["Flap"], 9)
        render(f"{SPECIES}_fly_top", (0.0, 0.55, -0.05), (0, 0.05, -0.01), lens=40)
        show(arm, Acts["Glide"], 0)
        render(f"{SPECIES}_glide_below", (0.05, -0.32, 0.18), (0, 0.05, 0.0), lens=40)
        show(arm, Acts["Walk" if SP["gait"] == "Walk" else "Hop"], 10)
        render(f"{SPECIES}_gait", (-0.30, 0.06, 0.03), (0, 0.04, 0.0), lens=50)
        del L0
    if "--no-export" in FLAGS:
        return
    for o in list(bpy.data.objects):
        if o.name in ("PCam", "PSun"):
            bpy.data.objects.remove(o)
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    if HERO:   # one mesh, no LODs, no clips (it plays SKEL_<Id>'s) and no far poses
        ob.name = f"SK_{SPECIES}_Hero"
        bpy.ops.object.select_all(action="DESELECT")
        ob.select_set(True)
        arm.select_set(True)
        bpy.context.view_layer.objects.active = arm
        path = os.path.join(OUTDIR, f"{SPECIES}_Hero.glb")
        kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=False, export_normals=True,
                  export_texcoords=True, export_materials="EXPORT", export_skins=True, export_animations=False, export_vertex_color="NONE",
                  export_def_bones=False, export_image_format="NONE")
        props = bpy.ops.export_scene.gltf.get_rna_type().properties
        bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})
        print("EXPORTED", path, os.path.getsize(path), "bytes")
        return
    lods = build_lods(arm, mats)
    # one glb, the LODs named LODn_<mesh>: Interchange folds them into one skeletal mesh with LODs (a second
    # Interchange import in the same Unreal process comes back empty, so everything rides in this one)
    ob.name = f"LOD0_SK_{SPECIES}"
    bpy.ops.object.select_all(action="DESELECT")
    for o in [ob] + list(lods.values()):
        o.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    path = os.path.join(OUTDIR, f"{SPECIES}.glb")
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=False, export_normals=True,
              export_texcoords=True, export_materials="EXPORT", export_skins=True, export_animations=True,
              export_animation_mode="ACTIONS", export_force_sampling=True, export_optimize_animation_size=False, export_vertex_color="NONE",
              export_def_bones=False, export_frame_step=1, export_anim_single_armature=True,
              export_image_format="NONE")
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    kw = {k: v for k, v in kw.items() if k in props or k == "filepath"}
    bpy.ops.export_scene.gltf(**kw)
    print("EXPORTED", path, os.path.getsize(path), "bytes")
    export_poses(arm, lods[3], Acts, kw)


POSES = {  # static meshes for instanced far birds: (clip, frame at 30 fps)
    "Perch": ("Idle", 0), "Peck": ("Peck", 9), "Hop": ("Hop", 15), "Walk": ("Walk", 8), "Glide": ("Glide", 0), "Sleep": ("Sleep", 0),
    "Flap0": ("Flap", 0), "Flap1": ("Flap", 8), "Flap2": ("Flap", 15), "Flap3": ("Flap", 23),
}


def build_lods(arm, mats):
    """Lighter skinned meshes on the same rig: 1 fewer, larger body feathers; 2 no body feathers (the painted body and
    the flight feathers); 3 a coarse body and the flight feathers only."""
    lods = {}
    for lod in (1, 2, 3):
        sb = build_mesh(None, lod)
        o = bake_mesh(sb, mats, f"LOD{lod}_SK_{SPECIES}")
        o.parent = arm
        m = o.modifiers.new("Armature", "ARMATURE")
        m.object = arm
        o.data.calc_loop_triangles()
        lods[lod] = o
        print("LOD", lod, len(o.data.loop_triangles), "tris")
    return lods


def export_poses(arm, far, Acts, kw):
    """<Id>_pose_<Pose>.glb: LOD3 frozen in each pose, unskinned (for instanced far birds)."""
    for name, (clip, frame) in POSES.items():
        show(arm, Acts[clip], frame)
        dg = bpy.context.evaluated_depsgraph_get()
        me = bpy.data.meshes.new_from_object(far.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
        st = bpy.data.objects.new(f"SM_{SPECIES}_{name}", me)
        bpy.context.scene.collection.objects.link(st)
        bpy.ops.object.select_all(action="DESELECT")
        st.select_set(True)
        bpy.context.view_layer.objects.active = st
        path = os.path.join(OUTDIR, f"{SPECIES}_pose_{name}.glb")
        bpy.ops.export_scene.gltf(**dict(kw, filepath=path, export_animations=False, export_skins=False))
        bpy.data.objects.remove(st)
    arm.animation_data.action = None
    print("POSES", len(POSES))


main()
