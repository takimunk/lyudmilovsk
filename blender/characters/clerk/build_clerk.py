"""Middle-aged Japanese office clerk who paints pictures in Excel. Seated, rigged, animated.

Sculpted procedurally: every body part is a signed-distance model (blender/characters/clerk/sdf.py)
meshed with OpenVDB, decimated, skinned from the same primitives and posed in the seat at
Station 09 of Scene 01 (office CRT with a spreadsheet). The rest pose IS the seated pose.

Character space: metres, origin on the floor under the chair-seat centre, facing -Y (like the
Zoomer). Writes blender/characters/clerk/clerk.blend and blender/export/character_clerk.glb
(skinned mesh, his own ball mouse on a bone, animation Clerk_ExcelIdle).

    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/characters/clerk/build_clerk.py [-- --preview]
"""
import math
import os
import sys
import time

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sdf  # noqa: E402
from sdf import Piece, capsule, chain, ellipsoid, frame, halfspace, norm, plate, rbox, rot, round_cone, sphere, v3  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
BLEND = os.path.join(HERE, "clerk.blend")
GLB = os.path.join(ROOT, "blender", "export", "character_clerk.glb")
RENDERS = os.path.join(ROOT, "renders")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = next((a.split("=", 1)[1].split(",") for a in ARGS if a.startswith("--only=")), None)

FPS = 30

# ------------------------------------------------------------------ station geometry (character space)
# Station 09 in Scene 01 (blender/export/scene01_layout.json "clerk"): glass desk, top at 0.762,
# front edge 0.42 ahead of the seat centre, beige 90s keyboard straight ahead, mouse pad to the
# right, CRT behind the keyboard. Bentwood chair squared to the desk, seat top at 0.478.
SEAT_Z = 0.478
DESK_Z = 0.762
DESK_FRONT_Y = -0.42
MOUSE = v3((-0.32, -0.565, DESK_Z + 0.004))       # centre of the mouse pad (pad is 4 mm thick)
KEYB_C, KEYB_H = v3((0.0, -0.56, DESK_Z + 0.018)), v3((0.23, 0.085, 0.0175))
SCREEN = v3((0.0, -0.690, 1.013))          # crt_grey17 screen glass (layout "clerk".screen)

# ------------------------------------------------------------------ skeleton (rest = seated pose)
J = {}
J["root"] = v3((0, 0, 0))
J["pelvis"] = v3((0, -0.02, 0.60))
J["spine"] = v3((0, -0.005, 0.66))
J["chest"] = v3((0, -0.03, 0.83))
J["neck"] = v3((0, -0.09, 1.06))
J["head"] = v3((0, -0.115, 1.14))
J["head_end"] = v3((0, -0.14, 1.30))
for s, sx in ((".L", 1), (".R", -1)):
    J["clavicle" + s] = v3((0.025 * sx, -0.075, 1.03))
    J["upper_arm" + s] = v3((0.18 * sx, -0.08, 1.02))
    J["thigh" + s] = v3((0.092 * sx, -0.07, 0.575))
J["shin.L"], J["shin.R"] = v3((0.115, -0.47, 0.535)), v3((-0.125, -0.46, 0.53))
J["foot.L"], J["foot.R"] = v3((0.125, -0.50, 0.095)), v3((-0.14, -0.52, 0.095))
J["toe.L"], J["toe.R"] = J["foot.L"] + v3((0.012, -0.15, -0.06)), J["foot.R"] + v3((-0.012, -0.15, -0.06))
J["hand.R"] = v3((-0.318, -0.450, 0.795))     # wrist, palm on the mouse (lifted to clear it later)
J["hand.L"] = v3((0.165, -0.462, 0.800))      # wrist, fingers resting on the left end of the keyboard


def solve2(a, target, l1, l2, pole):
    d = target - a
    dist = min(max(np.linalg.norm(d), abs(l1 - l2) + 1e-4), l1 + l2 - 1e-4)
    dirn = d / np.linalg.norm(d)
    x = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist)
    h = math.sqrt(max(l1 * l1 - x * x, 0))
    side = pole - dirn * np.dot(pole, dirn)
    side = side / np.linalg.norm(side)
    return a + dirn * x + side * h


UPPER, FORE = 0.29, 0.255
ELBOW_POLE = {".R": v3((-0.75, 0.35, -0.6)), ".L": v3((0.75, 0.35, -0.6))}


def place_elbows():
    for s in (".L", ".R"):
        J["forearm" + s] = solve2(J["upper_arm" + s], J["hand" + s], UPPER, FORE, ELBOW_POLE[s])

# Head frame: nodding 12 degrees towards the screen.
HEAD_R = rot((1, 0, 0), 12)
HEAD_O = J["head"] + HEAD_R @ v3((0, -0.012, 0.055))


def H(p):
    """Head-local point -> character space. Head local: x left, -y forward, z up."""
    return HEAD_O + HEAD_R @ v3(p)


def HR(R=None):
    return HEAD_R if R is None else HEAD_R @ R


# Hand frames: columns (side towards the thumb, forward along the fingers, up = back of the hand).
HAND_F = {
    ".R": frame(v3((0.03, -1.0, 0.10)), v3((0.08, 0, 1))),
    ".L": frame(v3((-0.10, -1.0, -0.03)), v3((-0.12, 0.0, 1))),
}
# frame() returns columns (s, f, u) with s = f x u, which points to the right of the hand's
# forward direction. The thumb of a palm-down right hand points to +X (towards the body).
THUMB_SIGN = {".R": 1.0, ".L": -1.0}


def hand_axes(side):
    """(forward along the fingers, side towards the thumb, up out of the back of the hand)."""
    M = HAND_F[side]
    sv = M[:, 0] if np.dot(M[:, 0], v3((THUMB_SIGN[side], 0, 0))) > 0 else -M[:, 0]
    return M[:, 1], sv, M[:, 2]


def hand_pt(side, f, s, u):
    F, S, U = hand_axes(side)
    return J["hand" + side] + F * f + S * s + U * u


# ------------------------------------------------------------------ head
def build_head():
    p = Piece("Head", 0.0011)
    b = "head"
    # skull and face masses
    p.add(ellipsoid(H((0, 0.010, 0.030)), (0.074, 0.097, 0.086), HR()), 0, b)
    p.add(ellipsoid(H((0, 0.048, -0.005)), (0.066, 0.058, 0.066), HR()), 0.02, b)
    p.add(ellipsoid(H((0, -0.040, -0.040)), (0.060, 0.058, 0.062), HR()), 0.025, b)        # maxilla
    for sx in (1, -1):
        p.add(ellipsoid(H((0.050 * sx, -0.060, -0.027)), (0.023, 0.022, 0.016), HR()), 0.022, b)   # cheekbone
        p.add(ellipsoid(H((0.045 * sx, -0.048, -0.060)), (0.020, 0.024, 0.026), HR()), 0.03, b)    # cheek
        p.add(round_cone(H((0.060 * sx, 0.004, -0.050)), H((0.020 * sx, -0.070, -0.110)), 0.017, 0.016), 0.022, b)  # jaw
        p.add(ellipsoid(H((0.038 * sx, -0.045, -0.098)), (0.020, 0.028, 0.019), HR()), 0.03, b)    # jowl
    p.add(ellipsoid(H((0, -0.077, -0.112)), (0.026, 0.019, 0.021), HR()), 0.018, b)          # chin
    p.add(ellipsoid(H((0, -0.050, -0.122)), (0.040, 0.040, 0.016), HR()), 0.02, b)           # under the chin
    p.add(ellipsoid(H((0, -0.072, -0.072)), (0.031, 0.024, 0.030), HR()), 0.012, b)          # muzzle
    # brow ridge and forehead
    p.add(ellipsoid(H((0, -0.080, 0.014)), (0.058, 0.016, 0.013), HR()), 0.016, b)
    p.add(ellipsoid(H((0, -0.070, 0.050)), (0.060, 0.025, 0.040), HR()), 0.02, b)
    # eye sockets, eyes sit at (+-0.031, -0.071, -0.004)
    for sx in (1, -1):
        p.sub(ellipsoid(H((0.031 * sx, -0.090, -0.004)), (0.016, 0.014, 0.0105), HR()), 0.006)
        # upper lid fold (monolid: full) and lower lid
        p.add(ellipsoid(H((0.031 * sx, -0.0778, 0.0030)), (0.0158, 0.0090, 0.0068), HR(rot((0, 1, 0), -9 * sx))), 0.003, b)
        p.add(ellipsoid(H((0.0175 * sx, -0.0835, -0.004)), (0.004, 0.004, 0.0055), HR()), 0.003, b)   # inner corner
        p.add(ellipsoid(H((0.031 * sx, -0.0795, -0.0130)), (0.0135, 0.0060, 0.0042), HR(rot((0, 1, 0), 6 * sx))), 0.003, b)
        p.add(ellipsoid(H((0.033 * sx, -0.082, -0.0225)), (0.0135, 0.0065, 0.0055), HR()), 0.006, b)   # tired bags
    # nose: low bridge, wide alae
    p.add(round_cone(H((0, -0.084, 0.002)), H((0, -0.1075, -0.040)), 0.0058, 0.0105), 0.008, b)
    p.add(ellipsoid(H((0, -0.1085, -0.043)), (0.0115, 0.0105, 0.0105), HR()), 0.006, b)
    for sx in (1, -1):
        p.add(ellipsoid(H((0.0135 * sx, -0.0975, -0.047)), (0.0085, 0.009, 0.0075), HR()), 0.004, b)
        p.sub(ellipsoid(H((0.0068 * sx, -0.1045, -0.0525)), (0.0038, 0.0052, 0.0026), HR()), 0.0015)
        # nasolabial folds and the corner of the mouth
        p.sub(round_cone(H((0.019 * sx, -0.100, -0.050)), H((0.029 * sx, -0.087, -0.084)), 0.0016, 0.0012), 0.005)
    # lips
    p.add(ellipsoid(H((0, -0.0925, -0.0725)), (0.0235, 0.0088, 0.0062), HR(rot((1, 0, 0), -10))), 0.004, b)
    p.add(ellipsoid(H((0, -0.0905, -0.0835)), (0.0205, 0.0088, 0.0062), HR(rot((1, 0, 0), 12))), 0.004, b)
    p.sub(rbox(H((0, -0.101, -0.0782)), (0.0225, 0.012, 0.0006), HR()), 0.0012)
    p.add(ellipsoid(H((0, -0.094, -0.064)), (0.006, 0.004, 0.006), HR()), 0.004, b)            # philtrum
    # ears
    for sx in (1, -1):
        R = HR(rot((0, 0, 1), -12 * sx) @ rot((1, 0, 0), -10))
        p.add(ellipsoid(H((0.078 * sx, 0.014, -0.010)), (0.0095, 0.021, 0.031), R), 0.004, b)
        p.sub(ellipsoid(H((0.0865 * sx, 0.011, -0.014)), (0.0055, 0.0125, 0.017), R), 0.003)
        p.add(ellipsoid(H((0.0815 * sx, 0.010, -0.018)), (0.0035, 0.0055, 0.0055), R), 0.003, b)  # tragus
        p.add(ellipsoid(H((0.073 * sx, 0.004, -0.008)), (0.008, 0.012, 0.022), R), 0.006, b)       # ear root
    # neck down into the collar
    p.add(round_cone(J["neck"] + v3((0, 0.02, -0.04)), H((0, 0.02, -0.075)), 0.056, 0.050), 0.03, "neck")
    p.add(ellipsoid(H((0, -0.020, -0.110)), (0.04, 0.04, 0.03), HR()), 0.02, "neck")          # throat
    # skin irregularity
    lo, hi = p.bounds()
    p.disp(lambda X, Y, Z: 0.00035 * sdf.vnoise(X, Y, Z, 450.0, 3), lo, hi)
    return p


def build_hair():
    p = Piece("Hair", 0.0009)
    b = "head"
    # a shell over the skull: thicker on top, short on the sides and the back
    p.add(ellipsoid(H((0, 0.008, 0.034)), (0.0835, 0.1065, 0.0945), HR()), 0, b)
    p.add(ellipsoid(H((0.004, -0.010, 0.058)), (0.075, 0.094, 0.074), HR()), 0.02, b)        # volume on top
    p.add(ellipsoid(H((-0.010, -0.052, 0.072)), (0.052, 0.050, 0.044), HR()), 0.02, b)       # combed front swoop
    # hairline: slightly receding at the temples, above the ears, short at the nape
    p.sub(ellipsoid(H((0, -0.125, -0.034)), (0.068, 0.075, 0.110), HR()), 0.016)             # the face
    for sx in (1, -1):
        p.sub(ellipsoid(H((0.058 * sx, -0.095, 0.052)), (0.018, 0.030, 0.018), HR()), 0.012)  # receding temples
        p.sub(ellipsoid(H((0.090 * sx, 0.014, -0.032)), (0.028, 0.040, 0.036), HR()), 0.008)  # around the ears
    p.clip(halfspace(H((0, 0.07, -0.050)), HR() @ v3((0, 0.25, -1))), 0.01)
    p.clip(halfspace(H((0, 0, -0.030)), HEAD_R @ v3((0, -0.5, -1))), 0.01)
    # side parting on the left
    p.sub(round_cone(H((0.032, -0.092, 0.098)), H((0.036, 0.03, 0.136)), 0.0016, 0.0011), 0.0015)

    def strands(X, Y, Z):
        d = np.stack(np.broadcast_arrays(X - HEAD_O[0], Y - HEAD_O[1], Z - HEAD_O[2]))
        x, y, z = (HEAD_R.T @ d.reshape(3, -1)).reshape(d.shape)
        top = np.clip((z - 0.045) / 0.035, 0, 1)
        # right of the parting the hair is combed across the top, left of it down the side
        across = top * (y + 0.25 * x) + (1 - top) * (z - 0.25 * y)
        warp = 1.6 * sdf.vnoise(X, Y, Z, 30, 5)
        fine = np.sin(across * 2 * math.pi / 0.0045 + warp * 2.0)
        clumps = np.sin(across * 2 * math.pi / 0.017 + warp)
        amp = 0.5 + 0.5 * sdf.vnoise(X, Y, Z, 70, 6)
        return 0.0003 * amp * fine + 0.0005 * clumps
    lo, hi = p.bounds()
    p.disp(strands, lo, hi)
    return p


def build_brows(head):
    p = Piece("Brows", 0.0006)
    fwd = HEAD_R @ v3((0, 1, 0))       # into the face
    for sx in (1, -1):
        raw = [H((0.011 * sx, -0.15, 0.0130)), H((0.022 * sx, -0.15, 0.0155)), H((0.034 * sx, -0.15, 0.0165)),
               H((0.046 * sx, -0.15, 0.0150)), H((0.055 * sx, -0.15, 0.0105))]
        pts = head.surface(np.array(raw), fwd, offset=-0.0006)
        for c in chain(list(pts), [0.0021, 0.0026, 0.0025, 0.0020, 0.0011]):
            p.add(c, 0.002, "head")
    lo, hi = p.bounds()
    p.disp(lambda X, Y, Z: 0.0003 * sdf.vnoise(X, Y, Z, 900, 9), lo, hi)
    return p


# ------------------------------------------------------------------ hands
FINGERS = [  # name, knuckle f, knuckle s, phalanx lengths, thickness scale, splay (deg towards the thumb)
    ("index", 0.088, 0.027, (0.043, 0.025, 0.021), 1.00, 4.0),
    ("middle", 0.092, 0.008, (0.047, 0.029, 0.022), 1.03, 0.0),
    ("ring", 0.087, -0.010, (0.044, 0.027, 0.021), 0.96, -5.0),
    ("pinky", 0.078, -0.026, (0.034, 0.020, 0.019), 0.84, -11.0),
]
SEG_R = [(0.0096, 0.0087), (0.0085, 0.0077), (0.0077, 0.0066)]
CURL = {  # base curl per joint (MCP, PIP, DIP); the fit scales it until the fingertip touches
    ".R": {"index": (14, 22, 10), "middle": (16, 24, 12), "ring": (26, 36, 16), "pinky": (30, 40, 18), "thumb": (8, 12, 10)},
    ".L": {"index": (10, 20, 10), "middle": (12, 22, 10), "ring": (14, 24, 12), "pinky": (16, 26, 12), "thumb": (6, 10, 8)},
}
FIT = {".R": {}, ".L": {}}          # curl scale per finger after fitting
FINGER_REST = {}                    # bone -> (head, tail, u) in rest


def finger_chain(side, name, scale=1.0):
    F, S, U = hand_axes(side)
    if name == "thumb":
        p = hand_pt(side, 0.020, 0.022, -0.013)
        d = norm(0.55 * F + 0.72 * S - 0.42 * U)
        u = norm(0.55 * U + 0.6 * S)
        u = norm(u - d * np.dot(u, d))
        lens = (0.040, 0.031, 0.026)
    else:
        _, fb, sb, lens, _, splay = next(f for f in FINGERS if f[0] == name)
        p = hand_pt(side, fb, sb, 0.0015)
        sgn = 1.0 if np.dot(np.cross(U, F), S) > 0 else -1.0
        d = rot(U, sgn * splay) @ F
        u = U.copy()
    joints, ups = [p], []
    for L, ang in zip(lens, CURL[side][name]):
        R = rot(np.cross(u, d), ang * scale)
        d, u = R @ d, R @ u
        ups.append(u)
        p = p + d * L
        joints.append(p)
    return joints, ups


def finger_prims(piece, side, name, scale):
    joints, ups = finger_chain(side, name, scale)
    if name == "thumb":
        radii = [(0.0125, 0.0108), (0.0106, 0.0096), (0.0094, 0.0080)]
        k = 1.0
    else:
        k = next(f for f in FINGERS if f[0] == name)[4]
        radii = SEG_R
    for i in range(3):
        bone = "%s_0%d%s" % (name, i + 1, side)
        a, b, u = joints[i], joints[i + 1], ups[i]
        r0, r1 = radii[i][0] * k, radii[i][1] * k
        d = norm(b - a)
        piece.add(round_cone(a, b, r0, r1), 0.0035 if (i or name == "thumb") else 0.008, bone)
        L = np.linalg.norm(b - a)
        # fleshy pad on the palm side, a little knuckle on the back
        Rf = np.stack([np.cross(d, u), d, u], axis=1)
        piece.add(ellipsoid(a + d * L * 0.55 - u * r1 * 0.35, (r1 * 0.95, L * 0.42, r1 * 0.75), Rf), 0.004, bone)
        if i > 0:
            piece.add(sphere(a + u * 0.0012, r0 * 1.04), 0.003, bone)
        if i == 2:
            nail_c = a + d * L * 0.62 + u * r1 * 0.72
            piece.add(ellipsoid(nail_c, (r1 * 0.78, L * 0.40, r1 * 0.22), Rf), 0.0015, bone)
            NAILS.append((nail_c, (r1 * 0.85, L * 0.48, r1 * 0.5), Rf))
        FINGER_REST[bone] = (a, b, u)
    return joints


NAILS = []


def hand_piece(side):
    p = Piece("Hand" + side, 0.0007)
    F, S, U = hand_axes(side)
    Rh = np.stack([S, F, U], axis=1)
    hb, fb = "hand" + side, "forearm" + side
    # forearm stub up into the sleeve, wrist, palm
    fdir = norm(J["hand" + side] - J["forearm" + side])
    p.add(round_cone(J["hand" + side] - fdir * 0.07, hand_pt(side, -0.010, 0, 0.0), 0.024, 0.022), 0, fb)
    p.add(ellipsoid(hand_pt(side, -0.008, -0.002, 0.0), (0.029, 0.024, 0.0175), Rh), 0.01, hb)
    p.add(rbox(hand_pt(side, 0.048, 0.0, -0.001), (0.040, 0.044, 0.0115), Rh, 0.0095), 0.01, hb)
    p.add(ellipsoid(hand_pt(side, 0.052, 0.0, 0.003), (0.041, 0.046, 0.011), Rh), 0.008, hb)        # back of the hand
    p.add(ellipsoid(hand_pt(side, 0.030, 0.026, -0.009), (0.016, 0.030, 0.0135), Rh @ rot((0, 0, 1), -20)), 0.01, hb)  # thenar
    p.add(ellipsoid(hand_pt(side, 0.042, -0.030, -0.006), (0.012, 0.036, 0.011), Rh), 0.01, hb)     # hypothenar
    p.add(ellipsoid(hand_pt(side, -0.014, -0.026, 0.006), (0.006, 0.006, 0.005), Rh), 0.004, hb)    # ulna head
    for name, kf, ks, *_ in FINGERS:
        p.add(sphere(hand_pt(side, kf - 0.006, ks, 0.0055), 0.0082), 0.006, hb)                    # knuckle
    # veins on the back of the hand
    for a, b in (((0.0, 0.012), (0.06, 0.020)), ((0.005, -0.008), (0.065, -0.012)), ((0.02, 0.012), (0.04, -0.004))):
        p.add(capsule(hand_pt(side, a[0], a[1], 0.0125), hand_pt(side, b[0], b[1], 0.0135), 0.0011), 0.003, hb)
    for name in ("index", "middle", "ring", "pinky", "thumb"):
        finger_prims(p, side, name, FIT[side].get(name, 1.0))
    lo, hi = p.bounds()
    p.disp(lambda X, Y, Z: 0.00012 * sdf.vnoise(X, Y, Z, 900, 21), lo, hi)
    return p


def support(P):
    """Distance to what the hands rest on: the mouse, the keyboard, the desk."""
    d = MOUSE_PIECE.eval(P)
    d = np.minimum(d, rbox(KEYB_C, KEYB_H, None, 0.004)(P[:, 0], P[:, 1], P[:, 2]))
    return np.minimum(d, P[:, 2] - DESK_Z)


def fit_hands():
    """Lift each palm until it rests on its support, then curl every finger until its tip touches."""
    for side in (".R", ".L"):
        pts = np.array([hand_pt(side, f, s, -0.0125) for f in np.linspace(0.0, 0.09, 10) for s in np.linspace(-0.035, 0.035, 8)])
        lift = max(0.0, 0.0012 - support(pts).min())
        J["hand" + side] = J["hand" + side] + v3((0, 0, lift))
        for name in ("index", "middle", "ring", "pinky", "thumb"):
            best = 1.0
            for scale in np.arange(0.3, 2.21, 0.05):
                joints, ups = finger_chain(side, name, scale)
                tip = joints[-1] - ups[-1] * 0.006 - norm(joints[-1] - joints[-2]) * 0.004
                if support(tip[None])[0] <= 0.0015:
                    best = scale
                    break
                best = scale
            FIT[side][name] = best
        print("FIT", side, "lift %.4f" % lift, {k: round(v, 2) for k, v in FIT[side].items()})
    place_elbows()


# ------------------------------------------------------------------ his own mouse
MOUSE_PIECE = None
CABLE = []


def build_mouse():
    p = Piece("Mouse", 0.0007)
    c = MOUSE
    p.add(ellipsoid(c + v3((0, 0.004, 0.010)), (0.030, 0.053, 0.0245)), 0, "mouse")
    p.add(ellipsoid(c + v3((0, 0.016, 0.012)), (0.0285, 0.034, 0.024)), 0.01, "mouse")
    p.clip(halfspace(c + v3((0, 0, 0.0005)), (0, 0, -1)), 0.002)
    p.sub(rbox(c + v3((0, -0.033, 0.03)), (0.0005, 0.024, 0.02)), 0.0004)          # between the buttons
    p.sub(rbox(c + v3((0, -0.009, 0.03)), (0.04, 0.0005, 0.02)), 0.0004)           # back edge of the buttons
    pts = [c + v3(o) for o in ((0, -0.047, 0.010), (0, -0.062, 0.004), (0.006, -0.085, 0.0022),
                                (0.03, -0.13, 0.0022), (0.085, -0.19, 0.0022), (0.15, -0.23, 0.0022))]
    for i, cone in enumerate(chain(pts, [0.0021] * len(pts))):
        p.add(cone, 0.001, "mouse" if i < 2 else "root")
    return p


# ------------------------------------------------------------------ clothes
def axis_disp(a, b, fn):
    """Displacement around the segment a-b: fn(t along, angle around, radial distance) -> offset."""
    a, b = v3(a), v3(b)
    d = norm(b - a)
    e1 = norm(np.cross(d, v3((0, 0, 1))) if abs(d[2]) < 0.9 else np.cross(d, v3((1, 0, 0))))
    e2 = np.cross(d, e1)

    def f(X, Y, Z):
        vx, vy, vz = X - a[0], Y - a[1], Z - a[2]
        t = vx * d[0] + vy * d[1] + vz * d[2]
        r1 = vx * e1[0] + vy * e1[1] + vz * e1[2]
        r2 = vx * e2[0] + vy * e2[1] + vz * e2[2]
        return fn(t, np.arctan2(r2, r1), np.sqrt(r1 * r1 + r2 * r2))
    return f


def build_shirt():
    p = Piece("Shirt", 0.0026)
    p.add(ellipsoid((0, -0.02, 0.69), (0.150, 0.110, 0.075)), 0, "spine")
    p.add(ellipsoid((0, -0.072, 0.748), (0.130, 0.083, 0.095)), 0.04, "spine")          # middle-aged belly
    p.add(ellipsoid((0, -0.03, 0.86), (0.150, 0.103, 0.11)), 0.05, "chest")
    p.add(ellipsoid((0, -0.052, 0.958), (0.160, 0.090, 0.082)), 0.04, "chest")
    p.add(ellipsoid((0, 0.008, 0.945), (0.143, 0.068, 0.10)), 0.04, "chest")             # back
    for s, sx in ((".L", 1), (".R", -1)):
        p.add(ellipsoid((0.058 * sx, -0.100, 0.952), (0.056, 0.035, 0.045)), 0.03, "chest")
        p.add(round_cone(J["neck"] + v3((0.03 * sx, 0.012, -0.01)), J["upper_arm" + s] + v3((-0.015 * sx, 0.004, 0.012)),
                         0.042, 0.048), 0.03, "clavicle" + s)
        p.add(sphere(J["upper_arm" + s] + v3((0.0, 0.0, -0.004)), 0.046), 0.035, "upper_arm" + s)
        p.add(round_cone(J["upper_arm" + s], J["forearm" + s], 0.046, 0.042), 0.02, "upper_arm" + s)
        p.add(sphere(J["forearm" + s], 0.043), 0.012, "forearm" + s)
        F, _, _ = hand_axes(s)
        fdir = norm(J["hand" + s] - J["forearm" + s])
        p.add(round_cone(J["forearm" + s], J["hand" + s] - fdir * 0.03, 0.042, 0.030), 0.012, "forearm" + s)
    # neck hole and collar
    ndir = norm(J["head"] - J["neck"])
    na, nb = J["neck"] + v3((0, 0.012, -0.018)), J["neck"] + ndir * 0.040 + v3((0, 0.006, 0))
    p.sub(capsule(na - ndir * 0.06, nb + ndir * 0.06, 0.057), 0.006)
    p.add(round_cone(na, nb, 0.069, 0.064), 0.006, "neck")
    p.sub(capsule(na - ndir * 0.06, nb + ndir * 0.06, 0.0585), 0.0015)
    p.sub(ellipsoid(nb + v3((0, -0.068, 0.006)), (0.011, 0.03, 0.03)), 0.003)            # collar opening at the front
    for sx in (1, -1):                                                                   # collar points
        R = frame(v3((0, -1, 0.35)), v3((0.0, 0.25, 1))) @ rot((0, 1, 0), 28 * sx)
        p.add(ellipsoid(J["neck"] + v3((0.029 * sx, -0.068, -0.022)), (0.020, 0.0035, 0.026), R), 0.004, "chest")
    # breast pocket, pen in it
    pc = p.surface(np.array([[0.072, -0.40, 0.925]]), (0, 1, 0), offset=0.0, reach=0.4)[0]
    n = surface_normal(p, pc)
    POCKET.update(c=pc, n=n)
    Rp = frame(n, v3((0, 0, 1)))
    p.add(plate(pc - n * 0.001, (0.055, 0.0028, 0.062), Rp, 0.003), 0.001, "chest")
    # folds: bloused over the belt, at the elbows, cloth irregularity
    p.disp(lambda X, Y, Z: 0.0018 * np.sin(np.arctan2(Y + 0.02, X) * 11 + 3 * sdf.vnoise(X, Y, Z, 20, 31))
           * np.exp(-((Z - 0.712) / 0.025) ** 2), (-0.2, -0.25, 0.65), (0.2, 0.15, 0.78))
    for s in (".L", ".R"):
        e = J["forearm" + s]
        p.disp(axis_disp(J["upper_arm" + s], e, lambda t, ang, r: 0.0016 * np.sin(t * 2 * math.pi / 0.022 + 1.5 * np.cos(ang))
                         * np.exp(-((t - UPPER) / 0.05) ** 2)), e - 0.08, e + 0.08)
    lo, hi = p.bounds()
    p.disp(lambda X, Y, Z: 0.0009 * sdf.vnoise(X, Y, Z, 30, 33) + 0.0003 * sdf.vnoise(X, Y, Z, 110, 34), lo, hi)
    return p


POCKET = {}


def surface_normal(piece, c, h=0.002):
    P = np.array([c + v3(o) for o in ((h, 0, 0), (-h, 0, 0), (0, h, 0), (0, -h, 0), (0, 0, h), (0, 0, -h))])
    d = piece.eval(P)
    return norm(v3((d[0] - d[1], d[2] - d[3], d[4] - d[5])))


def build_pen():
    p = Piece("Pen", 0.0006)
    c, n = POCKET["c"], POCKET["n"]
    a = c + n * 0.006 + v3((0.035, 0, 0.035))
    b = a + v3((0.0, 0.004, -0.10))
    p.add(capsule(a, b, 0.0048), 0, "chest")
    p.add(rbox(a + n * 0.005 + v3((0, 0, -0.018)), (0.0025, 0.0012, 0.02), frame(n, v3((0, 0, 1))), 0.001), 0.001, "chest")
    return p


def build_arm_covers():
    p = Piece("ArmCovers", 0.0018)
    for s in (".L", ".R"):
        e, w = J["forearm" + s], J["hand" + s]
        d = norm(w - e)
        a, b = e + d * 0.030, w - d * 0.010
        p.add(round_cone(a, b, 0.0475, 0.039), 0, "forearm" + s)
        p.add(sphere(a, 0.0485), 0.012, "forearm" + s)
        L = np.linalg.norm(b - a)
        p.disp(axis_disp(a, b, lambda t, ang, r: 0.0016 * np.sin(ang * 13 + 2 * np.sin(t * 40))
                         * (np.exp(-(t / 0.025) ** 2) + np.exp(-((t - L) / 0.022) ** 2))
                         + 0.0012 * np.sin(t * 2 * math.pi / 0.03 + ang) * 0.6), np.minimum(a, b) - 0.06, np.maximum(a, b) + 0.06)
    return p


def build_trousers():
    p = Piece("Trousers", 0.0028)
    p.add(ellipsoid((0, -0.012, 0.618), (0.163, 0.123, 0.085)), 0, "pelvis")
    p.add(ellipsoid((0, 0.03, 0.585), (0.158, 0.100, 0.075)), 0.03, "pelvis")
    p.add(ellipsoid((0, -0.022, 0.676), (0.154, 0.114, 0.035)), 0.02, "pelvis")
    p.add(ellipsoid((0, -0.095, 0.567), (0.075, 0.07, 0.055)), 0.04, "pelvis")
    for s in (".L", ".R"):
        p.add(round_cone(J["thigh" + s], J["shin" + s], 0.088, 0.064), 0.04, "thigh" + s)
        p.add(sphere(J["shin" + s] + v3((0, -0.006, 0.004)), 0.062), 0.02, "shin" + s)
        p.add(round_cone(J["shin" + s], J["foot" + s] + v3((0, 0.004, 0.035)), 0.058, 0.051), 0.012, "shin" + s)
    p.sub(rbox((0, 0.02, SEAT_Z - 0.06), (0.3, 0.205, 0.06), None, 0.01), 0.025)          # sitting on the seat
    for s in (".L", ".R"):
        k = J["shin" + s]
        p.disp(axis_disp(J["thigh" + s], k, lambda t, ang, r: 0.002 * np.sin(t * 2 * math.pi / 0.03 + 2 * np.sin(ang))
                         * np.exp(-((t - 0.40) / 0.06) ** 2)), k - 0.1, k + 0.1)
        h = J["thigh" + s]
        p.disp(axis_disp(h, k, lambda t, ang, r: 0.0015 * np.sin(ang * 5 + t * 30) * np.exp(-((t - 0.05) / 0.06) ** 2)),
               h - 0.12, h + 0.12)
    lo, hi = p.bounds()
    p.disp(lambda X, Y, Z: 0.0008 * sdf.vnoise(X, Y, Z, 25, 41), lo, hi)
    return p


def build_belt():
    p = Piece("Belt", 0.0013)
    c = v3((0, -0.024, 0.678))
    p.add(ellipsoid(c, (0.1585, 0.1185, 0.03)), 0, "pelvis")
    p.clip(halfspace(c + v3((0, 0, 0.015)), (0, 0, 1)))
    p.clip(halfspace(c - v3((0, 0, 0.015)), (0, 0, -1)))
    front = c + v3((0, -0.1185, 0))
    p.add(rbox(front + v3((0, -0.002, 0)), (0.019, 0.004, 0.0165), None, 0.002), 0.001, "pelvis")
    p.sub(rbox(front + v3((0, -0.006, 0)), (0.013, 0.004, 0.0105), None, 0.0015), 0.0005)
    p.add(rbox(front + v3((0, -0.003, 0)), (0.013, 0.003, 0.0018), None, 0.0008), 0.0005, "pelvis")
    return p


def build_shoes():
    p = Piece("Shoes", 0.0015)
    for s in (".L", ".R"):
        a, t = J["foot" + s], J["toe" + s]
        fwd = norm(v3((t[0] - a[0], t[1] - a[1], 0)))
        heel = v3((a[0], a[1], 0.0)) - fwd * 0.045
        toe = v3((t[0], t[1], 0.0)) + fwd * 0.03
        mid = (heel + toe) / 2
        R = frame(fwd, v3((0, 0, 1)))
        L = np.linalg.norm(toe - heel) / 2
        p.add(ellipsoid(mid + v3((0, 0, 0.032)), (0.046, L, 0.042), R), 0, "foot" + s)
        p.add(ellipsoid(toe - fwd * 0.04 + v3((0, 0, 0.024)), (0.043, 0.06, 0.027), R), 0.015, "foot" + s)
        p.add(round_cone(a + v3((0, 0.008, -0.04)), a + v3((0, 0.005, 0.03)), 0.042, 0.038), 0.02, "foot" + s)
        p.add(rbox(mid + v3((0, 0, 0.011)), (0.047, L + 0.004, 0.011), R, 0.008), 0.004, "foot" + s)   # sole
        p.add(round_cone(a + v3((0, 0, 0.02)), a + v3((0, 0.004, 0.10)), 0.036, 0.038), 0.006, "shin" + s)  # sock
        p.sub(ellipsoid(a + v3((0, -0.01, 0.055)), (0.03, 0.04, 0.012)), 0.004)        # opening
    p.clip(halfspace((0, 0, 0.0), (0, 0, -1)))
    return p


def build_tie_knot(shirt):
    p = Piece("TieKnot", 0.0009)
    kc = shirt.surface(np.array([[0.0, -0.4, 1.035]]), (0, 1, 0), offset=0.0, reach=0.4)[0]
    n = surface_normal(shirt, kc)
    Rk = frame(n, v3((0, 0.2, 1)))
    p.add(round_cone(kc + n * 0.009 + Rk[:, 2] * 0.010, kc + n * 0.008 - Rk[:, 2] * 0.012, 0.0115, 0.0085), 0, "neck")
    p.disp(lambda X, Y, Z: 0.0005 * sdf.vnoise(X, Y, Z, 300, 51), kc - 0.03, kc + 0.03)
    TIE["knot"] = kc + n * 0.008 - Rk[:, 2] * 0.012
    return p


def build_tie_blade(shirt, material):
    """The blade as a ribbon lying on the shirt: ~4 mm thick, widening downwards, pointed tip."""
    zs = np.linspace(TIE["knot"][2] + 0.004, 0.722, 40)
    raw = np.array([[0.003 * math.sin(z * 25), -0.4, z] for z in zs])
    pts = shirt.surface(raw, (0, 1, 0), offset=0.0, reach=0.4)
    nrm = np.array([surface_normal(shirt, q) for q in pts])
    pts = pts + nrm * 0.0042
    seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    arc = np.concatenate([[0], np.cumsum(seg)])
    total = arc[-1]
    wmax = 0.084
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    rings = []
    for i, (q, n) in enumerate(zip(pts, nrm)):
        down = norm(pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)])
        across = norm(np.cross(n, down))
        half = 0.018 + (wmax / 2 - 0.018) * (arc[i] / total) ** 1.3
        half = min(half, max(total - arc[i], 0.0) * 1.0)
        t = 0.0019
        ring = [bm.verts.new(q + across * x + n * y) for x, y in ((-half, t), (half, t), (half, -t), (-half, -t))]
        rings.append((ring, half, arc[i]))
    for (r0, h0, s0), (r1, h1, s1) in zip(rings, rings[1:]):
        for k in range(4):
            f = bm.faces.new([r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]])
            for lp in f.loops:
                ring_i = 0 if lp.vert in r0 else 1
                h, sv = (h0, s0) if ring_i == 0 else (h1, s1)
                idx = (r0 if ring_i == 0 else r1).index(lp.vert)
                x = -h if idx in (0, 3) else h
                lp[uvl].uv = ((x + wmax / 2) / wmax, sv / wmax)
    top = bm.faces.new(list(reversed(rings[0][0])))
    for lp in top.loops:
        lp[uvl].uv = (0.5, 0.0)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("TieBlade")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("TieBlade", me)
    COLL.objects.link(ob)
    me.materials.append(material)
    group(ob, {"chest": np.ones(len(me.vertices))})
    return ob


def tie_material():
    """Navy silk with thin diagonal maroon and silver stripes (regimental)."""
    n = 128
    img = bpy.data.images.new("Clerk_TieStripes", n, n)
    i, j = np.meshgrid(np.arange(n), np.arange(n), indexing="xy")
    k = (i + j) % 32
    col = np.zeros((n, n, 4), dtype=np.float32)
    col[..., :3] = (0.025, 0.04, 0.13)
    col[(k >= 2) & (k < 7)] = (0.40, 0.06, 0.07, 1)
    col[(k == 9)] = (0.55, 0.55, 0.58, 1)
    col[..., 3] = 1
    img.pixels.foreach_set(col.ravel())
    img.pack()
    m = bpy.data.materials.new("Clerk_Tie")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(x for x in nt.nodes if x.type == "BSDF_PRINCIPLED")
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Closest"
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (2.0, 2.0, 1.0)
    uv = nt.nodes.new("ShaderNodeTexCoord")
    nt.links.new(uv.outputs["UV"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], tex.inputs["Vector"])
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.4
    return m


TIE = {}


# ------------------------------------------------------------------ glasses and eyelids
def build_glasses(head_hair):
    frame_p, lens_p = Piece("Glasses", 0.00035), Piece("Lenses", 0.0004)
    b = "head"
    tilt = HR(rot((1, 0, 0), -7))
    for sx in (1, -1):
        c = H((0.0315 * sx, -0.1035, -0.0045))
        frame_p.add(plate(c, (0.0257, 0.0013, 0.0170), tilt, 0.0095), 0, b)
        frame_p.sub(plate(c, (0.0232, 0.004, 0.0145), tilt, 0.0080), 0.0002)
        lens_p.add(plate(c, (0.0238, 0.0006, 0.0152), tilt, 0.0084), 0, b)
        # hinge and temple, the temple fitted along the side of the head and over the ear
        hinge = H((0.0575 * sx, -0.1005, 0.0015))
        frame_p.add(rbox(hinge, (0.0028, 0.004, 0.0035), HR(), 0.001), 0.0005, b)
        raw = np.array([H((0.074 * sx, y, 0.0045 - 0.06 * max(0.0, y - 0.0) ** 1.0)) for y in (-0.085, -0.06, -0.035, -0.01, 0.010)])
        side = HEAD_R @ v3((-sx, 0, 0))
        fitted = head_hair.surface(raw + -side * 0.03, side, offset=0.0012, reach=0.06)
        pts = [hinge + HEAD_R @ v3((0, 0.003, 0))] + list(fitted) + [H((0.0745 * sx, 0.030, -0.022))]
        for cone in chain(pts, [0.0012] * len(pts)):
            frame_p.add(cone, 0.0008, b)
        # nose pads on little arms
        pad = H((0.0105 * sx, -0.0955, -0.012))
        frame_p.add(ellipsoid(pad, (0.0012, 0.0035, 0.0048), HR(rot((0, 0, 1), 25 * sx))), 0.0004, b)
        frame_p.add(capsule(pad + HEAD_R @ v3((0.001 * sx, -0.002, 0.002)), H((0.0095 * sx, -0.1032, 0.002)), 0.0005), 0.0004, b)
    frame_p.add(capsule(H((0.0082, -0.1035, 0.0045)), H((0.0, -0.1045, 0.0068)), 0.0011), 0.0005, b)     # bridge
    frame_p.add(capsule(H((-0.0082, -0.1035, 0.0045)), H((0.0, -0.1045, 0.0068)), 0.0011), 0.0005, b)
    return frame_p, lens_p


EYE_C = {".L": (0.031, -0.0715, -0.004), ".R": (-0.031, -0.0715, -0.004)}
EYE_R = 0.0118


def build_lids():
    p = Piece("Lids", 0.00035)
    for s, c in EYE_C.items():
        cw = H(c)
        p.add(sphere(cw, EYE_R + 0.0017), 0, "lid" + s)
        p.sub(sphere(cw, EYE_R + 0.0005), 0.0003)
    # keep only the front upper cap of each shell
    p.clip(halfspace(H((0, -0.0715 + 0.003, 0)), HEAD_R @ v3((0, 1, 0))), 0.0004)
    p.clip(halfspace(H((0, -0.083, -0.0028)), HEAD_R @ v3((0, 0.28, -1))), 0.0004)
    return p


# ------------------------------------------------------------------ mesh helpers
COLL = None


def to_object(name, verts, quads, tris, decimate=None):
    me = bpy.data.meshes.new(name)
    faces = [tuple(int(i) for i in q) for q in quads] + [tuple(int(i) for i in t) for t in tris]
    me.from_pydata([tuple(v) for v in verts], [], faces)
    me.validate()
    ob = bpy.data.objects.new(name, me)
    COLL.objects.link(ob)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    if decimate:
        n = len(me.polygons) * 2
        if n > decimate:
            mod = ob.modifiers.new("Decimate", "DECIMATE")
            mod.ratio = decimate / n
            mod.use_collapse_triangulate = True
            apply_mods(ob)
    for poly in me.polygons:
        poly.use_smooth = True
    return ob


def apply_mods(ob):
    bpy.context.view_layer.objects.active = ob
    for o in bpy.context.selected_objects:
        o.select_set(False)
    ob.select_set(True)
    for m in list(ob.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def build_piece(piece, decimate):
    t = time.time()
    v, q, tr = piece.mesh()
    ob = to_object(piece.name, v, q, tr, decimate)
    ob["piece"] = piece.name
    print("PIECE %-10s %7d faces -> %6d  %.1fs" % (piece.name, len(q) + len(tr), len(ob.data.polygons), time.time() - t))
    return ob


# ------------------------------------------------------------------ materials
def mat(name, color, rough=0.5, metal=0.0, alpha=1.0, sss=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if alpha < 1.0:
        bsdf.inputs["Alpha"].default_value = alpha
    if sss:
        bsdf.inputs["Subsurface Weight"].default_value = sss
        bsdf.inputs["Subsurface Radius"].default_value = (0.012, 0.005, 0.003)
    return m


def assign(ob, *mats):
    for m in mats:
        ob.data.materials.append(m)


def regions(ob, fn):
    """fn(centres Nx3) -> material index per face."""
    me = ob.data
    c = np.array([p.center for p in me.polygons])
    idx = fn(c)
    me.polygons.foreach_set("material_index", idx.astype(np.int32))


def in_ell(P, c, r, R=None):
    d = P - c
    if R is not None:
        d = d @ R
    return ((d / r) ** 2).sum(axis=1) < 1.0


def head_local(P):
    return (P - HEAD_O) @ HEAD_R


# ------------------------------------------------------------------ eyes
GAZE0 = SCREEN + v3((0.02, 0.0, -0.01))     # where he looks in the rest pose


def build_eyes():
    out = []
    white = mat("Clerk_EyeWhite", (0.82, 0.80, 0.76), 0.12)
    iris = mat("Clerk_Iris", (0.075, 0.042, 0.022), 0.15)
    pupil = mat("Clerk_Pupil", (0.004, 0.004, 0.004), 0.05)
    for s, c_l in EYE_C.items():
        c = H(c_l)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=40, ring_count=28, radius=EYE_R, location=tuple(c))
        ob = bpy.context.active_object
        ob.name = "Eye" + s
        for col in ob.users_collection:
            col.objects.unlink(ob)
        COLL.objects.link(ob)
        look = norm(GAZE0 - c)
        ob.rotation_mode = "QUATERNION"
        ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(Vector(look))
        bpy.context.view_layer.update()
        ob.data.transform(ob.matrix_basis)
        ob.matrix_basis = Matrix.Identity(4)
        assign(ob, white, iris, pupil)
        P = np.array([p.center for p in ob.data.polygons])
        cos = norm_rows(P - c) @ look
        idx = np.where(cos > 0.955, 2, np.where(cos > 0.78, 1, 0))
        ob.data.polygons.foreach_set("material_index", idx.astype(np.int32))
        for poly in ob.data.polygons:
            poly.use_smooth = True
        group(ob, {"eye" + s: np.ones(len(ob.data.vertices))})
        out.append(ob)
    return out


def norm_rows(A):
    return A / np.linalg.norm(A, axis=1, keepdims=True)


def group(ob, weights):
    for bone, w in weights.items():
        g = ob.vertex_groups.get(bone) or ob.vertex_groups.new(name=bone)
        for i in np.nonzero(w > 1e-3)[0]:
            g.add([int(i)], float(w[i]), "REPLACE")


def skin(ob, piece, falloff=0.012):
    P = np.array([v.co for v in ob.data.vertices])
    dists = piece.bone_distances(P)
    if len(dists) == 1:
        group(ob, {next(iter(dists)): np.ones(len(P))})
    else:
        group(ob, sdf.skin_weights(dists, falloff))


# ------------------------------------------------------------------ armature
def bone_list():
    B = [("root", None, J["root"], v3((0, 0, 0.12))),
         ("pelvis", "root", J["pelvis"], J["spine"]),
         ("spine", "pelvis", J["spine"], J["chest"]),
         ("chest", "spine", J["chest"], J["neck"]),
         ("neck", "chest", J["neck"], J["head"]),
         ("head", "neck", J["head"], J["head_end"]),
         ("mouse", "root", MOUSE, MOUSE + v3((0, -0.06, 0)))]
    for s in (".L", ".R"):
        F, _, _ = hand_axes(s)
        B += [("clavicle" + s, "chest", J["clavicle" + s], J["upper_arm" + s]),
              ("upper_arm" + s, "clavicle" + s, J["upper_arm" + s], J["forearm" + s]),
              ("forearm" + s, "upper_arm" + s, J["forearm" + s], J["hand" + s]),
              ("hand" + s, "forearm" + s, J["hand" + s], J["hand" + s] + F * 0.085)]
        for name in ("thumb", "index", "middle", "ring", "pinky"):
            for i in range(3):
                bn = "%s_0%d%s" % (name, i + 1, s)
                a, b, _ = FINGER_REST[bn]
                B.append((bn, "hand" + s if i == 0 else "%s_0%d%s" % (name, i, s), a, b))
        B += [("thigh" + s, "pelvis", J["thigh" + s], J["shin" + s]),
              ("shin" + s, "thigh" + s, J["shin" + s], J["foot" + s]),
              ("foot" + s, "shin" + s, J["foot" + s], J["toe" + s])]
        c = H(EYE_C[s])
        B += [("eye" + s, "head", c, c + norm(GAZE0 - c) * 0.02),
              ("lid" + s, "head", c, c + HEAD_R @ v3((0, -0.02, 0.0)))]
    return B


def build_armature():
    data = bpy.data.armatures.new("Clerk")
    arm = bpy.data.objects.new("Clerk_Armature", data)
    COLL.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail in bone_list():
        b = data.edit_bones.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        if parent:
            b.parent = data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


# ------------------------------------------------------------------ animation
LOOP = 8.0
# Mouse path (seconds, dx, dy in metres; -x moves the cursor right, -y moves it up the screen).
MOUSE_KEYS = [(0.0, 0, 0), (0.30, -0.004, 0), (0.95, -0.004, 0), (1.25, -0.008, 0.0005), (1.85, -0.008, 0.0005),
              (2.35, 0.005, -0.022), (2.85, 0.005, -0.022), (3.05, 0.008, -0.0265), (3.55, 0.008, -0.0265),
              (4.05, -0.012, 0.002), (4.55, -0.012, 0.002), (4.85, -0.016, 0.002), (5.45, -0.016, 0.002),
              (5.75, -0.020, 0.003), (6.45, -0.020, 0.003), (7.35, 0.0, 0.0), (8.0, 0.0, 0.0)]
CLICKS = [0.62, 1.55, 2.6, 3.3, 4.3, 5.15, 6.02, 6.2]
BLINKS = [1.95, 5.6]


def ease(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def mouse_at(t):
    t = t % LOOP
    for (t0, x0, y0), (t1, x1, y1) in zip(MOUSE_KEYS, MOUSE_KEYS[1:]):
        if t0 <= t <= t1:
            k = ease((t - t0) / max(t1 - t0, 1e-6))
            moving = (x0, y0) != (x1, y1)
            return v3((x0 + (x1 - x0) * k, y0 + (y1 - y0) * k, 0.0)), (math.sin(k * math.pi) if moving else 0.0), (x1 - x0)
    return v3((0, 0, 0)), 0.0, 0.0


def pulse(t, times, up, hold, down):
    v = 0.0
    for c in times:
        dt = t - c
        if 0 <= dt < up:
            v = max(v, dt / up)
        elif up <= dt < up + hold:
            v = 1.0
        elif up + hold <= dt < up + hold + down:
            v = max(v, 1 - (dt - up - hold) / down)
    return ease(v)


def rotdiff(a, b):
    a, b = norm(a), norm(b)
    ax = np.cross(a, b)
    s = np.linalg.norm(ax)
    if s < 1e-9:
        return np.eye(3)
    return rot(ax, math.degrees(math.atan2(s, np.dot(a, b))))


def T(p):
    M = np.eye(4)
    M[:3, 3] = p
    return M


def R4(R):
    M = np.eye(4)
    M[:3, :3] = R
    return M


def pose_at(t, rest):
    """Returns bone -> 4x4 armature-space pose matrix."""
    heads = {n: v3(h) for n, (_, h, _) in rest.items()}
    G, Rw = {}, {}
    local, absolute = {}, {}
    breath = math.sin(2 * math.pi * t / 4.0)
    local["spine"] = rot((1, 0, 0), 0.4 * breath)
    local["chest"] = rot((1, 0, 0), -0.7 * breath)
    for s in (".L", ".R"):
        local["clavicle" + s] = rot((0, 1, 0), (0.6 if s == ".L" else -0.6) * breath)
    m, moving, dx = mouse_at(t)
    gaze_m, _, _ = mouse_at(t + 0.18)                   # the eyes run slightly ahead of the hand
    gaze = GAZE0 + v3((gaze_m[0] * 6.5, 0, -gaze_m[1] * 6.5))
    yaw = math.radians(2.5) * moving * (1 if dx > 0 else -1 if dx < 0 else 0)
    absolute["mouse"] = rot((0, 0, 1), math.degrees(yaw))
    local["neck"] = rot((0, 0, 1), 1.5 * math.sin(2 * math.pi * t / LOOP)) @ rot((1, 0, 0), 0.8 * math.sin(2 * math.pi * t / LOOP * 2 + 1))
    local["head"] = rot((0, 0, 1), 6 * gaze_m[0] * 6.5 * 0.4) @ rot((1, 0, 0), -6 * gaze_m[1] * 6.5 * 0.3)
    blink = pulse(t, BLINKS, 0.07, 0.04, 0.12)
    click = pulse(t, CLICKS, 0.04, 0.05, 0.06)
    ctrl = pulse(t, [4.25], 0.1, 0.4, 0.15)
    order = list(rest)
    for name in order:
        parent, h, _ = rest[name]
        Gp = G[parent] if parent else np.eye(4)
        Rp = Rw[parent] if parent else np.eye(3)
        hw = (Gp @ np.append(h, 1))[:3]
        if name == "mouse":
            R = absolute["mouse"]
            hw = h + m
        elif name.startswith("eye"):
            R = rotdiff(GAZE0 - h, gaze - hw) @ Rp
        elif name.startswith("lid"):
            ax = Rp @ (HEAD_R @ v3((1, 0, 0)))
            R = rot(ax, 34 * blink) @ Rp
        elif name in ("upper_arm.R", "forearm.R", "hand.R"):
            R = None
        else:
            L = local.get(name, np.eye(3))
            if name.startswith(("index", "middle")) and name.endswith(".R"):
                a, b, u = FINGER_REST[name]
                k = {"01": 6.0, "02": 3.0, "03": 1.0}[name.split("_")[1][:2]] * (1.0 if name.startswith("index") else 0.15)
                L = rot(np.cross(u, norm(b - a)), k * click)
            if name.startswith(("ring", "pinky")) and name.endswith(".L"):
                a, b, u = FINGER_REST[name]
                L = rot(np.cross(u, norm(b - a)), (5.0 if name.startswith("pinky") else 2.0) * ctrl * (name.endswith("01.L") or 0.3))
            R = Rp @ L
        if R is not None:
            Rw[name] = R
            G[name] = T(hw) @ R4(R) @ T(-h)
            if name == "clavicle.R":
                # right arm: IK to the wrist that rides on the mouse
                sh = (G[name] @ np.append(heads["upper_arm.R"], 1))[:3]
                wrist = J["hand.R"] + m + v3((0, 0, 0.0025 * moving))
                elbow = solve2(sh, wrist, UPPER, FORE, ELBOW_POLE[".R"] + 0.0 * sh)
                Rua = rotdiff(heads["forearm.R"] - heads["upper_arm.R"], elbow - sh)
                Rw["upper_arm.R"], G["upper_arm.R"] = Rua, T(sh) @ R4(Rua) @ T(-heads["upper_arm.R"])
                Rfa = rotdiff(heads["hand.R"] - heads["forearm.R"], wrist - elbow)
                Rw["forearm.R"], G["forearm.R"] = Rfa, T(elbow) @ R4(Rfa) @ T(-heads["forearm.R"])
                Rh = absolute["mouse"]
                Rw["hand.R"], G["hand.R"] = Rh, T(wrist) @ R4(Rh) @ T(-heads["hand.R"])
    return G


def animate(arm):
    rest = {}
    for b in arm.data.bones:
        rest[b.name] = (b.parent.name if b.parent else None, np.array(b.head_local), np.array(b.tail_local))
    # hierarchy order
    order, seen = [], set()

    def visit(b):
        order.append(b.name)
        for c in b.children:
            visit(c)
    for b in arm.data.bones:
        if b.parent is None:
            visit(b)
    rest = {n: rest[n] for n in order}
    ML = {b.name: np.array(b.matrix_local) for b in arm.data.bones}
    ad = arm.animation_data_create()
    act = bpy.data.actions.new("Clerk_ExcelIdle")
    ad.action = act
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    n = int(LOOP * FPS)
    prev = {}
    for f in range(n + 1):
        G = pose_at(f / FPS, rest)
        for name in order:
            pose = G[name] @ ML[name]
            parent = rest[name][0]
            if parent:
                ppose = G[parent] @ ML[parent]
                basis = np.linalg.inv(np.linalg.inv(ML[parent]) @ ML[name]) @ np.linalg.inv(ppose) @ pose
            else:
                basis = np.linalg.inv(ML[name]) @ pose
            pb = arm.pose.bones[name]
            mb = Matrix(basis.tolist())
            loc, q, _ = mb.decompose()
            if name in prev and q.dot(prev[name]) < 0:
                q.negate()
            prev[name] = q.copy()
            pb.rotation_quaternion = q
            pb.location = loc
            pb.keyframe_insert("rotation_quaternion", frame=f)
            pb.keyframe_insert("location", frame=f)
    act.use_fake_user = True
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = 0, n
    return act


# ------------------------------------------------------------------ preview props (not exported)
def preview_props():
    coll = bpy.data.collections.new("PreviewProps")
    bpy.context.scene.collection.children.link(coll)

    def boxo(name, c, size, m):
        bpy.ops.mesh.primitive_cube_add(size=1, location=tuple(c))
        ob = bpy.context.active_object
        ob.scale = size
        for col in ob.users_collection:
            col.objects.unlink(ob)
        coll.objects.link(ob)
        ob.data.materials.append(m)
        return ob
    glass = mat("P_Glass", (0.3, 0.38, 0.38), 0.05, alpha=0.35)
    beige = mat("P_Beige", (0.72, 0.66, 0.53), 0.55)
    wood = mat("P_Wood", (0.25, 0.13, 0.06), 0.45)
    pad = mat("P_Pad", (0.05, 0.12, 0.3), 0.8)
    boxo("Desk", (0, DESK_FRONT_Y - 0.4, DESK_Z - 0.006), (1.3, 0.8, 0.012), glass)
    boxo("Keyboard", KEYB_C, tuple(KEYB_H * 2), beige)
    boxo("Pad", (MOUSE[0], MOUSE[1], DESK_Z + 0.002), (0.22, 0.19, 0.004), pad)
    boxo("CRT", (0, -0.94, DESK_Z + 0.22), (0.42, 0.49, 0.43), beige)
    boxo("Seat", (0, 0, SEAT_Z - 0.0175), (0.42, 0.42, 0.035), wood)
    boxo("Back", (0, 0.2, 0.8), (0.32, 0.03, 0.2), wood)
    return coll


# ------------------------------------------------------------------ preview
def preview(name, cams, frame=0):
    scn = bpy.context.scene
    scn.frame_set(frame)
    scn.render.engine = "BLENDER_EEVEE"
    scn.render.resolution_x, scn.render.resolution_y = 1000, 1000
    world = bpy.data.worlds.get("W") or bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.09, 0.085, 0.08, 1)
    scn.world = world
    if not bpy.data.objects.get("Key"):
        for nm, loc, energy in (("Key", (-0.9, -1.5, 1.9), 120), ("Fill", (1.2, -1.1, 1.3), 40), ("Rim", (0.5, 1.2, 1.9), 80)):
            ld = bpy.data.lights.new(nm, "AREA")
            ld.energy, ld.size = energy, 0.9
            lo = bpy.data.objects.new(nm, ld)
            scn.collection.objects.link(lo)
            lo.location = loc
            lo.rotation_euler = (Vector((0, -0.3, 0.9)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    cam = bpy.data.objects.get("Cam")
    if not cam:
        cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
        scn.collection.objects.link(cam)
    scn.camera = cam
    for tag, loc, target, lens in cams:
        cam.location = loc
        cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        scn.render.filepath = os.path.join(RENDERS, "clerk_%s_%s.png" % (name, tag))
        bpy.ops.render.render(write_still=True)
        print("RENDER", scn.render.filepath)


# ------------------------------------------------------------------ the Excel picture on his CRT
TEX_OUT = os.path.join(ROOT, "textures", "clerk")
EXCEL_W, EXCEL_H = 1024, 768
GRID_COLS, GRID_ROWS = 52, 36
CELL_W, CELL_H = 18, 16
GRID_X0, GRID_Y0 = 22, 115 + 14          # top-left of cell A1 in pixels (below the column headers)
SCREEN_W, SCREEN_H = 0.336, 0.312


def load_pixels(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1]     # top row first
    bpy.data.images.remove(img)
    return a


def save_png(name, a):
    h, w = a.shape[:2]
    img = bpy.data.images.new(name, w, h, alpha=True)
    img.pixels.foreach_set(np.ascontiguousarray(a[::-1]).ravel())
    os.makedirs(TEX_OUT, exist_ok=True)
    img.filepath_raw = os.path.join(TEX_OUT, name + ".png")
    img.file_format = "PNG"
    img.save()
    return img


def resample(a, w, h):
    ys = (np.arange(h) + 0.5) * a.shape[0] / h
    xs = (np.arange(w) + 0.5) * a.shape[1] / w
    return a[ys.astype(int)][:, xs.astype(int)]


def excel_art():
    """Mt Fuji at sunset over a lake, with a cherry branch: GRID_COLS x GRID_ROWS cells, Excel 97 palette.
    Alpha holds the order in which he fills the cells (whole colour regions at a time, like Excel artists)."""
    W, Hh = GRID_COLS, GRID_ROWS
    y, x = np.mgrid[0:Hh, 0:W].astype(np.float64)
    pal = {"navy": (0, 0, 128), "blue": (51, 102, 255), "sky": (153, 204, 255), "pale": (204, 236, 255),
           "lav": (204, 153, 255), "rose": (255, 153, 204), "peach": (255, 204, 153), "orange": (255, 153, 0),
           "red": (255, 0, 0), "white": (255, 255, 255), "snow": (230, 230, 255), "slate": (51, 51, 153),
           "indigo": (51, 51, 102), "green": (0, 128, 0), "lgreen": (153, 204, 0), "teal": (0, 128, 128),
           "dteal": (0, 51, 102), "brown": (153, 51, 0), "pink": (255, 102, 204), "lpink": (255, 204, 255)}
    img = np.zeros((Hh, W, 3))
    region = np.zeros((Hh, W), dtype=int)
    bands = ["navy", "slate", "blue", "sky", "pale", "lav", "rose", "peach", "orange"]
    horizon = 25
    for i, b in enumerate(bands):
        sel = (y >= i * horizon / len(bands) - 0.01 * x) & (y < (i + 1) * horizon / len(bands) + 2)
        img[sel] = pal[b]
    region[:] = 0
    sun = (x - 41) ** 2 + ((y - 15) * 1.15) ** 2 < 16
    img[sun] = pal["red"]
    img[sun & ((x - 41) ** 2 + ((y - 15) * 1.15) ** 2 < 5)] = pal["orange"]
    region[sun] = 1
    ridge = 6 + np.abs(x - 22) * 1.05 - np.maximum(np.abs(x - 22) - 7, 0) * 0.6
    mount = (y >= ridge) & (y < horizon)
    img[mount] = pal["slate"]
    img[mount & (x > 22 + (y - 6) * 0.35)] = pal["indigo"]
    region[mount] = 2
    snow = mount & (y < 12.5 + 1.6 * np.sin(x * 1.9) + (np.abs(x - 22) < 3) * 1.5)
    img[snow] = pal["snow"]
    img[snow & (x > 23)] = pal["white"]
    region[snow] = 3
    ground = y >= horizon
    img[ground] = pal["dteal"]
    img[ground & (y >= horizon + 1)] = pal["teal"]
    refl = ground & (np.abs(x - 22) < (36 - y) * 0.9) & (y < 31)
    img[refl] = pal["slate"]
    img[refl & (y < 27) & (np.abs(x - 22) < (28 - y) * 1.2)] = pal["snow"]
    sunr = ground & (np.abs(x - 41) < 2.5 - (y - horizon) * 0.3) & (y % 2 == 0)
    img[sunr] = pal["orange"]
    shore = y >= 32
    img[shore] = pal["green"]
    img[shore & ((x + y) % 5 == 0)] = pal["lgreen"]
    region[ground] = 4
    # cherry branch from the top-left corner
    t = np.linspace(0, 1, 60)
    bx, by = 0 + 17 * t, 2 + 6 * t ** 1.5
    branch = np.zeros_like(region, dtype=bool)
    for px, py in zip(bx, by):
        branch |= ((x - px) ** 2 + (y - py) ** 2) < (1.2 - 0.7 * px / 17) ** 2
    rng = np.random.RandomState(4)
    blossom = np.zeros_like(branch)
    for px, py in zip(bx[8::5], by[8::5]):
        for _ in range(4):
            cx, cy = px + rng.uniform(-2.5, 2.5), py + rng.uniform(-2.5, 3)
            blossom |= ((x - cx) ** 2 + (y - cy) ** 2) < rng.uniform(0.6, 1.8)
    img[blossom] = pal["lpink"]
    img[blossom & ((x * 7 + y * 3) % 4 == 0)] = pal["pink"]
    img[branch] = pal["brown"]
    region[blossom | branch] = 5
    # fill order: region by region, top to bottom inside each, with some jitter between rows
    spans = [(0.0, 0.34), (0.34, 0.40), (0.40, 0.62), (0.62, 0.70), (0.70, 0.88), (0.88, 0.995)]
    order = np.zeros((Hh, W))
    for r, (a, b) in enumerate(spans):
        sel = region == r
        key = y[sel] * W + x[sel]
        rank = np.argsort(np.argsort(key))
        order[sel] = a + (b - a) * (rank + 0.5) / max(sel.sum(), 1)
    out = np.zeros((Hh, W, 4), dtype=np.float32)
    out[..., :3] = img / 255.0                    # byte image: values are stored as they are (sRGB)
    out[..., 3] = order
    return out


def excel_base():
    """Excel 97 window with columns squeezed to little square cells, empty sheet."""
    src = load_pixels(os.path.join(ROOT, "textures", "podyezd", "screen_spreadsheet.jpg"))
    sh, sw = src.shape[:2]
    out = np.zeros((EXCEL_H, EXCEL_W, 4), dtype=np.float32)
    out[..., 3] = 1
    top = resample(src[:int(sh * 270 / 1792)], EXCEL_W, 115)
    bot = resample(src[int(sh * 1680 / 1792):], EXCEL_W, EXCEL_H - (GRID_Y0 + GRID_ROWS * CELL_H) - 0)
    out[:115] = top
    out[EXCEL_H - bot.shape[0]:] = bot
    grey = np.array([0.55, 0.55, 0.55, 1], dtype=np.float32)       # linear grey of the window frame
    mid = slice(115, GRID_Y0 + GRID_ROWS * CELL_H)
    out[mid] = grey
    gx1, gy1 = GRID_X0 + GRID_COLS * CELL_W, GRID_Y0 + GRID_ROWS * CELL_H
    out[GRID_Y0:gy1, GRID_X0:gx1] = 1.0                           # white cells
    line = np.array([0.62, 0.62, 0.62, 1], dtype=np.float32)
    for c in range(GRID_COLS + 1):
        out[115:gy1, GRID_X0 + c * CELL_W - 1 if c else GRID_X0] = line
    for r in range(GRID_ROWS + 1):
        out[GRID_Y0 + r * CELL_H - 1 if r else GRID_Y0, 0:gx1] = line
    out[115:GRID_Y0 - 1, 0:gx1] = np.array([0.66, 0.66, 0.66, 1], dtype=np.float32)       # column headers
    out[GRID_Y0:gy1, 0:GRID_X0 - 1] = np.array([0.66, 0.66, 0.66, 1], dtype=np.float32)   # row headers
    for c in range(GRID_COLS + 1):
        out[115:GRID_Y0 - 1, GRID_X0 + c * CELL_W - 1 if c else GRID_X0] = 0.35
    for r in range(GRID_ROWS + 1):
        out[GRID_Y0 + r * CELL_H - 1 if r else GRID_Y0, 0:GRID_X0 - 1] = 0.35
    # vertical scrollbar
    out[115:gy1, gx1 + 30:] = np.array([0.7, 0.7, 0.7, 1], dtype=np.float32)
    return out


def make_excel_textures():
    base = excel_base()
    art = excel_art()
    save_png("excel_base", base)
    save_png("excel_art", art)
    # composite preview of the finished picture
    full = base.copy()
    for r in range(GRID_ROWS):
        for c in range(GRID_COLS):
            y0, x0 = GRID_Y0 + r * CELL_H, GRID_X0 + c * CELL_W
            full[y0:y0 + CELL_H - 1, x0:x0 + CELL_W - 1, :3] = art[r, c, :3]
    save_png("excel_finished_preview", full)
    EXCEL.update(grid=((GRID_X0 / EXCEL_W, GRID_Y0 / EXCEL_H),
                       (GRID_COLS * CELL_W / EXCEL_W, GRID_ROWS * CELL_H / EXCEL_H)))
    return full


EXCEL = {}


def build_screen(full):
    """A plane 3 mm in front of the Station 09 CRT screen; Unreal gives it the animated Excel material."""
    c = SCREEN + v3((0, 0.004, 0))
    hw, hh = SCREEN_W / 2, SCREEN_H / 2
    # seen from the chair (looking along -Y) the screen's left edge is at +X
    verts = [c + v3((hw, 0, hh)), c + v3((-hw, 0, hh)), c + v3((-hw, 0, -hh)), c + v3((hw, 0, -hh))]
    me = bpy.data.meshes.new("ExcelScreen")
    me.from_pydata([tuple(v) for v in verts], [], [(0, 3, 2, 1)])
    uv = me.uv_layers.new(name="UVMap")
    corner = {0: (0, 1), 1: (1, 1), 2: (1, 0), 3: (0, 0)}
    for lp in me.loops:
        uv.data[lp.index].uv = corner[lp.vertex_index]
    me.update()
    ob = bpy.data.objects.new("ExcelScreen", me)
    COLL.objects.link(ob)
    if me.polygons[0].normal.y < 0:
        me.flip_normals()
    # placeholder (Unreal swaps in M_ClerkExcel); the finished picture is textures/clerk/excel_finished_preview.png
    m = mat("Clerk_ExcelScreen", (0.8, 0.85, 0.9), 0.15)
    me.materials.append(m)
    group(ob, {"root": np.ones(4)})
    return ob


# ------------------------------------------------------------------ main
def main():
    global COLL, MOUSE_PIECE
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scn = bpy.context.scene
    scn.render.fps = FPS
    COLL = bpy.data.collections.new("Clerk")
    scn.collection.children.link(COLL)

    M = {
        "skin": mat("Clerk_Skin", (0.70, 0.52, 0.41), 0.5, sss=0.15),
        "lips": mat("Clerk_Lips", (0.60, 0.37, 0.32), 0.45, sss=0.15),
        "nail": mat("Clerk_Nail", (0.78, 0.62, 0.55), 0.3),
        "hair": mat("Clerk_Hair", (0.035, 0.032, 0.03), 0.45),
        "shirt": mat("Clerk_Shirt", (0.80, 0.81, 0.79), 0.75),
        "cover": mat("Clerk_ArmCover", (0.025, 0.025, 0.03), 0.35),
        "trousers": mat("Clerk_Trousers", (0.10, 0.10, 0.11), 0.8),
        "belt": mat("Clerk_Belt", (0.03, 0.022, 0.018), 0.35),
        "buckle": mat("Clerk_Buckle", (0.75, 0.72, 0.65), 0.25, 1.0),
        "shoe": mat("Clerk_Shoes", (0.02, 0.018, 0.017), 0.25),
        "sole": mat("Clerk_Sole", (0.05, 0.045, 0.04), 0.7),
        "sock": mat("Clerk_Socks", (0.03, 0.035, 0.06), 0.9),
        "tie": mat("Clerk_TieKnot", (0.025, 0.04, 0.13), 0.4),
        "metal": mat("Clerk_GlassesFrame", (0.18, 0.18, 0.19), 0.3, 1.0),
        "lens": mat("Clerk_Lens", (0.85, 0.9, 0.92), 0.02, alpha=0.12),
        "pen": mat("Clerk_Pen", (0.04, 0.06, 0.25), 0.3),
        "mouse": mat("Clerk_Mouse", (0.72, 0.66, 0.53), 0.5),
        "cable": mat("Clerk_Cable", (0.62, 0.58, 0.48), 0.6),
    }
    objs = []

    def make(piece, decimate, mats, fn=None, falloff=0.012):
        ob = build_piece(piece, decimate)
        assign(ob, *[M[m] for m in mats])
        if fn:
            regions(ob, fn)
        skin(ob, piece, falloff)
        objs.append(ob)
        return ob

    # head
    head_p, hair_p = build_head(), build_hair()
    make(head_p, 26000, ["skin", "lips"],
         lambda P: in_ell(head_local(P), v3((0, -0.091, -0.078)), v3((0.022, 0.012, 0.0115))).astype(int), 0.02)

    make(hair_p, 18000, ["hair"])
    make(build_brows(head_p), 3000, ["hair"])
    objs.extend(build_eyes())
    make(build_lids(), 2000, ["skin"])
    both = Piece("HeadHair", 1)
    both.ops = head_p.ops + [("add", op[1], 0.0, op[3]) if op[0] == "add" else op for op in []]

    class Union:
        def eval(self, P):
            return np.minimum(head_p.eval(P), hair_p.eval(P))

        def surface(self, P, d, offset=0.0, reach=0.08):
            return sdf.march(self.eval, P, d, offset, reach)
    gl, le = build_glasses(Union())
    make(gl, 5000, ["metal"])
    make(le, 600, ["lens"])

    # mouse first: the right hand is fitted onto it
    MOUSE_PIECE = build_mouse()
    fit_hands()
    mo = make(MOUSE_PIECE, 3000, ["mouse", "cable"],
              lambda P: ((P[:, 1] < MOUSE[1] - 0.052) | (P[:, 2] < MOUSE[2] + 0.0045)).astype(int) * 1)
    for s in (".R", ".L"):
        NAILS.clear()
        hp = hand_piece(s)
        make(hp, 8000, ["skin", "nail"],
             lambda P: np.any([in_ell(P, c, r, R) for c, r, R in NAILS], axis=0).astype(int), 0.008)

    shirt_p = build_shirt()
    make(shirt_p, 18000, ["shirt"], None, 0.03)
    make(build_pen(), 600, ["pen"])
    make(build_arm_covers(), 5000, ["cover"], None, 0.03)
    make(build_tie_knot(shirt_p), 1500, ["tie"])
    objs.append(build_tie_blade(shirt_p, tie_material()))
    make(build_trousers(), 12000, ["trousers"], None, 0.035)
    make(build_belt(), 1500, ["belt", "buckle"],
         lambda P: ((P[:, 1] < -0.135) & (np.abs(P[:, 0]) < 0.022)).astype(int))
    make(build_shoes(), 4000, ["shoe", "sole", "sock"],
         lambda P: np.where(P[:, 2] > 0.105, 2, np.where(P[:, 2] < 0.021, 1, 0)))

    objs.append(build_screen(make_excel_textures()))
    arm = build_armature()
    for ob in objs:
        ob.select_set(False)
    bpy.context.view_layer.objects.active = objs[0]
    for ob in objs:
        ob.select_set(True)
    bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active
    body.name = "Clerk_Mesh"
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    print("MESH", len(body.data.vertices), "verts", sum(len(p.vertices) - 2 for p in body.data.polygons), "tris")
    animate(arm)
    os.makedirs(os.path.dirname(GLB), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    for o in bpy.context.selected_objects:
        o.select_set(False)
    arm.select_set(True)
    body.select_set(True)
    bpy.ops.export_scene.gltf(filepath=GLB, export_format="GLB", use_selection=True, export_animations=True,
                              export_animation_mode="ACTIONS", export_skins=True, export_apply=False,
                              export_yup=True)
    print("GLB", GLB)
    if "--preview" in ARGS:
        preview_props()
        hf = H((0, -0.07, -0.02))
        mr = MOUSE + v3((0, 0.02, 0.03))
        preview("body", [
            ("front", (-0.55, -1.55, 1.25), (0, -0.2, 0.85), 30),
            ("side", (1.6, -0.35, 1.0), (0, -0.25, 0.7), 32),
            ("face", tuple(hf + v3((-0.12, -0.42, 0.04))), tuple(hf), 70),
            ("hands", (-0.62, -0.88, 1.08), (-0.18, -0.52, 0.80), 40),
            ("back", (-0.9, 1.0, 1.4), (0, -0.2, 0.8), 32),
        ], frame=0)
    if "--anim" in ARGS:
        if not bpy.data.collections.get("PreviewProps"):
            preview_props()
        for f in (0, 18, 58, 75, 100, 170):
            preview("anim%03d" % f, [("hands", (-0.62, -0.88, 1.08), (-0.18, -0.52, 0.80), 40),
                                     ("face", tuple(H((0, -0.07, -0.02)) + v3((-0.12, -0.42, 0.04))), tuple(H((0, -0.07, -0.02))), 70)],
                    frame=f)


main()
