"""Cat and dog anatomy for the animal builds (pure numpy, no bpy): breeds, skeletons and the signed-distance bodies.

Frame: metres, the animal standing in its bind pose on the ground at the origin (between its four feet), facing +X,
its left +Y, up +Z (Blender's axes; glTF -> Unreal gives forward +X, up +Z, the left -Y).

Breeds (docs/animals.md): one cat, and three yard dogs of a Baltic town: a laika-like mongrel (prick ears, curled
tail), a shepherd mix (bigger, longer muzzle, a bushy hanging tail) and a small mutt (short legs, soft half-drop ears).
Joint positions come from measured proportions (a domestic cat ~25 cm at the withers, a laika ~55 cm), the body is
built over the skeleton from ellipsoids and round cones (sdf.py), each tied to a bone for the skin weights.
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "characters", "clerk"))
import sdf  # noqa: E402
from sdf import Piece, Prim, ellipsoid, frame, round_cone, sphere, v3  # noqa: E402

SIDES = (("l", 1.0), ("r", -1.0))


def P(f, l, u):
    return v3((f, l, u))


# ====================================================================== breeds
# Each breed: the joints of its bind pose (left side; the right mirrors), body radii and fur lengths (cm on the body,
# the head, the legs, the tail). Coats are painted by coats.py.
BREEDS = {
    "Cat": dict(
        kind="cat", voxel=0.0018, smooth=0.026,
        # withers 0.25 m, nose to tail base 0.47 m, tail 0.28 m
        joints=dict(
            pelvis=P(-0.13, 0, 0.215), spine_01=P(-0.07, 0, 0.215), spine_02=P(0.0, 0, 0.215), chest=P(0.06, 0, 0.222),
            neck_01=P(0.128, 0, 0.232), neck_02=P(0.160, 0, 0.256), head=P(0.188, 0, 0.280), head_end=P(0.259, 0, 0.268),
            jaw=P(0.206, 0, 0.258), jaw_end=P(0.245, 0, 0.250),
            ear=P(0.204, 0.029, 0.312), ear_end=P(0.199, 0.047, 0.352),
            scapula=P(0.085, 0.035, 0.238), upperarm=P(0.128, 0.040, 0.165), forearm=P(0.072, 0.040, 0.105),
            hand=P(0.085, 0.036, 0.024), paw=P(0.093, 0.035, 0.009), paw_end=P(0.118, 0.035, 0.006),
            thigh=P(-0.150, 0.042, 0.190), shin=P(-0.090, 0.046, 0.108), foot=P(-0.178, 0.040, 0.058),
            toe=P(-0.158, 0.038, 0.010), toe_end=P(-0.132, 0.038, 0.006),
            tail=[P(-0.215, 0, 0.214), P(-0.255, 0, 0.205), P(-0.295, 0, 0.193), P(-0.335, 0, 0.180), P(-0.375, 0, 0.167),
                  P(-0.415, 0, 0.155), P(-0.452, 0, 0.145), P(-0.488, 0, 0.138)],
        ),
        trunk_f=(-0.228, 0.168),
        trunk=[(0.0, 0.222, 0.168, 0.030), (0.08, 0.236, 0.150, 0.042), (0.2, 0.238, 0.142, 0.046), (0.35, 0.232, 0.135, 0.043),
               (0.5, 0.228, 0.128, 0.044), (0.62, 0.233, 0.116, 0.048), (0.75, 0.243, 0.108, 0.049), (0.86, 0.247, 0.112, 0.045),
               (0.95, 0.238, 0.130, 0.038), (1.0, 0.222, 0.150, 0.030)],
        neck_r=(0.036, 0.031), head_r=(0.036, 0.043, 0.032), muzzle=(0.013, 0.011, 0.011), muzzle_len=0.022,
        ear=dict(w=0.035, t=0.0026, kind="prick"), eye_r=0.0108, eye=P(0.242, 0.0178, 0.285), eye_out=14.0,
        leg_r=dict(upperarm=(0.021, 0.016), forearm=(0.014, 0.011), hand=(0.010, 0.010), paw=(0.016, 0.012, 0.009),
                   thigh=(0.034, 0.024), shin=(0.019, 0.012), foot=(0.010, 0.009), toe=(0.016, 0.012, 0.009)),
        tail_r=(0.016, 0.011), nose=(0.006, 0.0075, 0.005),
        fur=dict(body=1.8, belly=2.4, neck=2.0, head=0.7, muzzle=0.35, ear=0.35, leg=1.1, paw=0.7, tail=2.4, cheek=1.3),
        shells=12, whiskers=True,
    ),
    "Laika": dict(
        kind="dog", voxel=0.0036, smooth=0.045,
        # withers 0.55 m: a laika-like yard dog, wedge head, prick ears, tail curled over the back
        joints=dict(
            pelvis=P(-0.24, 0, 0.47), spine_01=P(-0.13, 0, 0.475), spine_02=P(-0.01, 0, 0.48), chest=P(0.10, 0, 0.49),
            neck_01=P(0.25, 0, 0.50), neck_02=P(0.32, 0, 0.575), head=P(0.37, 0, 0.635), head_end=P(0.57, 0, 0.60),
            jaw=P(0.40, 0, 0.595), jaw_end=P(0.545, 0, 0.565),
            ear=P(0.402, 0.042, 0.724), ear_end=P(0.394, 0.055, 0.838),
            scapula=P(0.16, 0.070, 0.52), upperarm=P(0.275, 0.085, 0.395), forearm=P(0.170, 0.085, 0.272),
            hand=P(0.195, 0.080, 0.070), paw=P(0.210, 0.078, 0.028), paw_end=P(0.262, 0.078, 0.012),
            thigh=P(-0.235, 0.085, 0.430), shin=P(-0.128, 0.095, 0.290), foot=P(-0.300, 0.085, 0.140),
            toe=P(-0.270, 0.082, 0.024), toe_end=P(-0.222, 0.082, 0.012),
            tail=[P(-0.32, 0, 0.495), P(-0.385, 0, 0.500), P(-0.445, 0, 0.500), P(-0.500, 0, 0.495), P(-0.550, 0, 0.488),
                  P(-0.595, 0, 0.480), P(-0.635, 0, 0.472)],
        ),
        trunk_f=(-0.355, 0.335),
        trunk=[(0.0, 0.500, 0.400, 0.060), (0.08, 0.530, 0.360, 0.085), (0.2, 0.535, 0.345, 0.090), (0.35, 0.522, 0.330, 0.085),
               (0.48, 0.515, 0.305, 0.086), (0.6, 0.522, 0.285, 0.098), (0.72, 0.540, 0.265, 0.105), (0.84, 0.555, 0.270, 0.102),
               (0.93, 0.530, 0.300, 0.088), (1.0, 0.480, 0.335, 0.062)],
        neck_r=(0.080, 0.058), head_r=(0.075, 0.068, 0.062), muzzle=(0.034, 0.030, 0.030), muzzle_len=0.095,
        ear=dict(w=0.05, t=0.006, kind="prick"), eye_r=0.0125, eye=P(0.475, 0.036, 0.665), eye_out=22.0,
        leg_r=dict(upperarm=(0.050, 0.034), forearm=(0.028, 0.022), hand=(0.020, 0.019), paw=(0.033, 0.026, 0.018),
                   thigh=(0.075, 0.050), shin=(0.040, 0.025), foot=(0.019, 0.018), toe=(0.034, 0.026, 0.018)),
        tail_r=(0.032, 0.018), nose=(0.017, 0.021, 0.014),
        fur=dict(body=4.2, belly=3.2, neck=5.8, head=1.4, muzzle=0.6, ear=0.9, leg=1.8, paw=1.1, tail=7.0, cheek=2.6),
        shells=14, whiskers=False,
    ),
}


def _derive(base, name, scale=1.0, body=1.0, legs=1.0, head=1.0, muzzle=1.0, **over):
    """A breed from another one: overall scale, body length, leg length (from the ground), head size, muzzle length."""
    import copy
    b = copy.deepcopy(BREEDS[base])
    J = b["joints"]
    chest_u = J["chest"][2]
    # legs: everything below the chest scales by legs (the body drops onto shorter legs)
    drop = chest_u * (1.0 - legs)

    def warp(p, part):
        p = v3(p).copy()
        if part == "leg":
            p[2] = p[2] * legs
        else:
            p[2] -= drop
        p[0] *= body
        return p
    for k, v in list(J.items()):
        if k == "tail":
            J[k] = [warp(q, "body") for q in v]
            continue
        part = "leg" if k in ("upperarm", "forearm", "hand", "paw", "paw_end", "shin", "foot", "toe", "toe_end") else "body"
        J[k] = warp(v, part)
    # the head about its base
    hb = J["head"].copy()
    for k in ("head", "head_end", "jaw", "jaw_end", "ear", "ear_end"):
        d = J[k] - hb
        if k in ("head_end", "jaw_end"):
            d[0] = d[0] * muzzle
        J[k] = hb + d * head
    b["eye"] = hb + (v3(b["eye"]) - hb) * head
    b["head_r"] = tuple(r * head for r in b["head_r"])
    b["muzzle"] = tuple(r * head for r in b["muzzle"])
    b["muzzle_len"] *= head * muzzle
    b["eye_r"] *= head ** 0.7
    b["nose"] = tuple(r * head for r in b["nose"])
    b["ear"] = dict(b["ear"], w=b["ear"]["w"] * head)
    b["trunk_f"] = tuple(f * body for f in b["trunk_f"])
    b["trunk"] = [(q[0], q[1] - drop, q[2] - drop, q[3]) for q in b["trunk"]]
    # then everything by scale
    for k, v in list(J.items()):
        J[k] = [q * scale for q in v] if k == "tail" else v * scale
    for k in ("neck_r", "head_r", "muzzle", "nose", "tail_r"):
        b[k] = tuple(r * scale for r in b[k])
    b["trunk_f"] = tuple(f * scale for f in b["trunk_f"])
    b["trunk"] = [(q[0], q[1] * scale, q[2] * scale, q[3] * scale) for q in b["trunk"]]
    b["muzzle_len"] *= scale
    b["eye"] = b["eye"] * scale
    b["eye_r"] *= scale ** 0.8
    b["ear"] = dict(b["ear"], w=b["ear"]["w"] * scale, t=b["ear"]["t"] * max(scale, 0.7))
    b["leg_r"] = {k: tuple(r * scale for r in v) for k, v in b["leg_r"].items()}
    b["voxel"] *= max(scale, 0.75)
    b["smooth"] *= scale
    for k, v in over.items():
        if isinstance(v, dict) and isinstance(b.get(k), dict):
            b[k] = dict(b[k], **v)
        else:
            b[k] = v
    BREEDS[name] = b


# a shepherd mix: bigger, a longer body and muzzle, bigger ears, a bushy tail hanging low
_derive("Laika", "Shepherd", scale=1.1, body=1.1, legs=1.0, head=1.05, muzzle=1.18,
        ear=dict(w=0.072), fur=dict(body=4.6, neck=6.0, tail=7.5, belly=3.6), shells=15)
# a small mutt: short legs, a big head for its size, a short muzzle, soft half-drop ears, a short smooth coat
_derive("Laika", "Mutt", scale=0.66, body=0.98, legs=0.8, head=1.18, muzzle=0.82,
        ear=dict(kind="drop", w=0.060), fur=dict(body=2.2, neck=2.8, tail=3.0, belly=2.0, head=1.0, leg=1.3, cheek=1.6),
        shells=12)
# Maru, the club's dog (Dima's own, 2026-10-26): a slim, leggy cream laika mix, ears with folded tips, a plume tail
# curled over the back; the generated body (gen/Maru) gives her shape and coat, this only the skeleton and the clips
_derive("Laika", "Maru", scale=0.92, body=0.98, legs=1.04, head=0.96, muzzle=1.0,
        fur=dict(body=4.0, neck=5.2, tail=7.5, belly=3.0, head=0.6), shells=14)
# her generated head's own painted eyes (iris, pupil, the dark lid rims) are better than any ball set into them
# (2026-10-28: "no lids, scary"): build_animal keeps the paint, the face denser, the fur on it short and bare
# further round the eyes (the shells hid half of each), the strands finer
BREEDS["Maru"].update(painted_eyes=True, face_tris=9000, strand_scale=12.5, eye_bare=1.6)


# ====================================================================== skeleton
def bones(b):
    """[(name, head, tail, parent)] for the breed's bind pose."""
    J = b["joints"]
    out = [("root", P(0, 0, 0), P(0, 0, 0.1 * J["chest"][2]), None),
           ("pelvis", J["pelvis"], J["spine_01"], "root"),
           ("spine_01", J["spine_01"], J["spine_02"], "pelvis"),
           ("spine_02", J["spine_02"], J["chest"], "spine_01"),
           ("chest", J["chest"], J["neck_01"], "spine_02"),
           ("neck_01", J["neck_01"], J["neck_02"], "chest"),
           ("neck_02", J["neck_02"], J["head"], "neck_01"),
           ("head", J["head"], J["head_end"], "neck_02"),
           ("jaw", J["jaw"], J["jaw_end"], "head")]
    tail = J["tail"]
    for i in range(len(tail) - 1):
        out.append(("tail_%02d" % (i + 1), tail[i], tail[i + 1], "pelvis" if i == 0 else "tail_%02d" % i))
    for s, k in SIDES:
        def m(p):
            return v3((p[0], p[1] * k, p[2]))
        ear_tail = J["ear_end"]
        if b["ear"]["kind"] == "drop":
            mid = (J["ear"] + J["ear_end"]) * 0.5
            out.append(("ear_" + s, m(J["ear"]), m(mid), "head"))
            out.append(("eartip_" + s, m(mid), m(ear_tail), "ear_" + s))
        else:
            out.append(("ear_" + s, m(J["ear"]), m(ear_tail), "head"))
        out += [("scapula_" + s, m(J["scapula"]), m(J["upperarm"]), "chest"),
                ("upperarm_" + s, m(J["upperarm"]), m(J["forearm"]), "scapula_" + s),
                ("forearm_" + s, m(J["forearm"]), m(J["hand"]), "upperarm_" + s),
                ("hand_" + s, m(J["hand"]), m(J["paw"]), "forearm_" + s),
                ("paw_" + s, m(J["paw"]), m(J["paw_end"]), "hand_" + s),
                ("thigh_" + s, m(J["thigh"]), m(J["shin"]), "pelvis"),
                ("shin_" + s, m(J["shin"]), m(J["foot"]), "thigh_" + s),
                ("foot_" + s, m(J["foot"]), m(J["toe"]), "shin_" + s),
                ("toe_" + s, m(J["toe"]), m(J["toe_end"]), "foot_" + s)]
    return out


# ====================================================================== primitives
def ear_prim(base, tip, out, width, thick, cup=0.35):
    """A triangular ear: base centre, tip, `out` = the direction its front (inner) face looks; rounded rim, a hollow
    on the inner face (cup: depth as a share of the width)."""
    base, tip, out = v3(base), v3(tip), sdf.norm(out)
    up = tip - base
    h = float(np.linalg.norm(up))
    up = up / h
    out = out - up * np.dot(out, up)
    out = out / np.linalg.norm(out)
    side = np.cross(up, out)
    hw = width * 0.5

    def fn(X, Y, Z):
        dx, dy, dz = X - base[0], Y - base[1], Z - base[2]
        u = dx * side[0] + dy * side[1] + dz * side[2]          # across
        v = dx * up[0] + dy * up[1] + dz * up[2]                # up the ear
        w = dx * out[0] + dy * out[1] + dz * out[2]             # through
        # the triangle (base hw at v=0, a rounded tip at v=h), as a 2-d distance
        t = np.clip(v / h, 0.0, 1.0)
        half = hw * (1.0 - t) ** 0.72 + 0.1 * hw
        d2 = np.maximum(np.abs(u) - half, np.maximum(-v, v - h))
        # thickness, thinner to the rim and the tip, bent forward (the cup)
        bend = cup * width * (1.0 - (u / np.maximum(half, 1e-4)) ** 2).clip(0, 1) * (0.3 + 0.7 * np.sin(np.pi * t))
        dw = np.abs(w + bend * 0.5) - thick * (1.0 - 0.6 * t)
        return np.maximum(d2, dw)
    m = max(h, hw) + thick
    c = (base + tip) * 0.5
    return Prim(fn, c - m, c + m)


def oriented_ellipsoid(c, r, fwd, up=(0, 0, 1)):
    return ellipsoid(c, r, frame(fwd, up))


def body_piece(b, voxel=None):
    """The whole animal as one signed-distance piece, every primitive tied to its bone."""
    J = b["joints"]
    pc = Piece(b.get("name", "Body"), voxel or b["voxel"])
    k = b["smooth"]
    cat = b["kind"] == "cat"
    # trunk: a loft of ellipsoids along the back, from the rump to the front of the chest, by the breed's profile
    prof = b["trunk"]
    S = np.array([q[0] for q in prof])
    def at(s, i):
        return float(np.interp(s, S, [q[i] for q in prof]))
    f0, f1 = b["trunk_f"]
    n = 30
    step = (f1 - f0) / n
    for i in range(n + 1):
        s_ = i / n
        f = f0 + (f1 - f0) * s_
        top, bot, w = at(s_, 1), at(s_, 2), at(s_, 3)
        c = P(f, 0, (top + bot) * 0.5)
        bone = "pelvis" if s_ < 0.26 else ("spine_01" if s_ < 0.47 else ("spine_02" if s_ < 0.62 else "chest"))
        pc.add(ellipsoid(c, (step * 2.2, w, (top - bot) * 0.5)), k * 0.5, bone)
    # the shoulder blades under the skin of the withers
    for s, sk in SIDES:
        sc = J["scapula"] * v3((1, sk, 1))
        sa = J["upperarm"] * v3((1, sk, 1))
        sc = sc + P(0, -sk * b["leg_r"]["upperarm"][0] * 0.35, -b["leg_r"]["upperarm"][0] * 0.3)
        pc.add(round_cone(sc, sa, b["leg_r"]["upperarm"][0] * 0.7, b["leg_r"]["upperarm"][0] * 0.95), k * 0.8, "scapula_" + s)
    # neck
    nr = b["neck_r"]
    pc.add(round_cone(J["neck_01"] + P(-0.01 if cat else -0.03, 0, -nr[0] * 0.25), J["neck_02"], nr[0], (nr[0] + nr[1]) / 2), k * 0.8, "neck_01")
    pc.add(round_cone(J["neck_02"], J["head"] + P(0.0, 0, -b["head_r"][2] * 0.25), (nr[0] + nr[1]) / 2, nr[1]), k * 0.6, "neck_02")
    head_piece(pc, b)
    # legs
    lr = b["leg_r"]
    for s, sk in SIDES:
        def m(p):
            return v3((p[0], p[1] * sk, p[2]))
        ua, fa, ha, pa, pe = m(J["upperarm"]), m(J["forearm"]), m(J["hand"]), m(J["paw"]), m(J["paw_end"])
        pc.add(round_cone(ua, fa, lr["upperarm"][0], lr["upperarm"][1]), k * 0.45, "upperarm_" + s)
        pc.add(round_cone(fa + (ua - fa) * 0.1, ha, lr["forearm"][0], lr["forearm"][1]), k * 0.25, "forearm_" + s)
        pc.add(round_cone(ha, pa, lr["hand"][0], lr["hand"][1]), k * 0.2, "hand_" + s)
        paw(pc, pa, pe, lr["paw"], k, "paw_" + s, cat)
        th, sh, ft, to, te = m(J["thigh"]), m(J["shin"]), m(J["foot"]), m(J["toe"]), m(J["toe_end"])
        # the thigh: a big flat muscle mass from the croup down to the knee, on the body's side
        top = th + P(-lr["thigh"][0] * 0.6, 0, lr["thigh"][0] * 0.35)
        pc.add(round_cone(top, sh + (th - sh) * 0.12, lr["thigh"][0] * 1.05, lr["thigh"][1] * 0.95), k * 0.9, "thigh_" + s)
        pc.add(ellipsoid(th + (sh - th) * 0.45 + P(-lr["thigh"][0] * 0.45, sk * lr["thigh"][0] * 0.05, 0), (lr["thigh"][0] * 0.95, lr["thigh"][0] * 0.62, lr["thigh"][0] * 1.25),
                         frame(sh - th, P(1, 0, 0))), k * 0.7, "thigh_" + s)
        # the shank: the gaskin muscle behind the tibia, thinning to the hock
        pc.add(round_cone(sh, ft, lr["shin"][0], lr["shin"][1]), k * 0.4, "shin_" + s)
        pc.add(round_cone(sh + (ft - sh) * 0.1 + P(-lr["shin"][0] * 0.4, 0, 0), sh + (ft - sh) * 0.62, lr["shin"][0] * 1.05, lr["shin"][1] * 0.9), k * 0.45, "shin_" + s)
        pc.add(round_cone(ft, to, lr["foot"][0], lr["foot"][1]), k * 0.2, "foot_" + s)
        paw(pc, to, te, lr["toe"], k, "toe_" + s, cat)
    # tail
    T = J["tail"]
    n = len(T) - 1
    r0, r1 = b["tail_r"]
    for i in range(n):
        t0, t1 = i / n, (i + 1) / n
        ra = r0 + (r1 - r0) * t0 ** 0.8
        rb = r0 + (r1 - r0) * t1 ** 0.8
        if i == n - 1:
            rb *= 0.75
        pc.add(round_cone(T[i], T[i + 1], ra, rb), k * (0.5 if i == 0 else 0.15), "tail_%02d" % (i + 1))
    return pc


def paw(pc, a, e, r, k, bone, cat):
    """A paw: the pad block and the toes (four bumps in front)."""
    d = e - a
    L = float(np.linalg.norm(d))
    fw = d / L
    fw = sdf.norm(fw + P(0, 0, 0.15))
    c = a + d * 0.45 + P(0, 0, r[2] * 0.25)
    pc.add(ellipsoid(c, (r[0], r[1], r[2]), frame(fw, P(0, 0, 1))), k * 0.25, bone)
    side = np.cross(P(0, 0, 1), fw)
    for i, (o, f) in enumerate(((-1.5, 0.82), (-0.5, 1.0), (0.5, 1.0), (1.5, 0.82))):
        tc = a + d * (0.78 + 0.12 * f) + side * (o * r[1] * 0.42) + P(0, 0, r[2] * 0.05)
        rr = r[1] * (0.36 if cat else 0.34)
        pc.add(ellipsoid(tc, (rr * 1.15, rr, rr * 0.9), frame(fw, P(0, 0, 1))), k * 0.12, bone)


def head_piece(pc, b):
    """Cranium, cheeks, brow, muzzle and nose on 'head', the lower jaw on 'jaw', ears, eye sockets and the mouth slit."""
    J = b["joints"]
    cat = b["kind"] == "cat"
    if not cat:
        return dog_head(pc, b)
    hr = b["head_r"]
    hb, he = J["head"], J["head_end"]
    fwd = sdf.norm(he - hb)
    k = b["smooth"] * 0.4
    # the skull: behind the eyes, wide over the cheekbones
    cr = hb + fwd * hr[0] * (0.85 if cat else 0.85) + P(0, 0, hr[2] * 0.25)
    pc.add(ellipsoid(cr, hr, frame(fwd, P(0, 0, 1))), k, "head")
    eye = v3(b["eye"])
    er = b["eye_r"]
    for s, sk in SIDES:
        e = v3((eye[0], eye[1] * sk, eye[2]))
        if cat:
            # wide cheeks under and behind the eyes, and the forehead's slope down to the nose
            cheek = e + P(-er * 1.3, sk * er * 0.75, -er * 1.55)
            pc.add(ellipsoid(cheek, (er * 2.1, er * 1.75, er * 1.6), frame(fwd, P(0, 0, 1))), k * 0.9, "head")
        else:
            cheek = e + P(-hr[0] * 0.35, sk * hr[1] * 0.25, -hr[2] * 0.45)
            pc.add(ellipsoid(cheek, (hr[0] * 0.55, hr[1] * 0.42, hr[2] * 0.45), frame(fwd, P(0, 0, 1))), k, "head")
            # the brow over the eye
            pc.add(ellipsoid(e + P(-er * 0.2, sk * er * 0.1, er * 0.85), (er * 1.3, er * 1.1, er * 0.6)), k * 0.6, "head")
    # the muzzle: from the stop to the nose; cats: two whisker pads; dogs: a long tapering snout
    mz = b["muzzle"]
    nose_c = he + P(-b["nose"][0] * 0.6, 0, -b["nose"][2] * 0.2)
    stop = eye + P(er * 0.6, 0, -er * 0.2)
    stop[1] = 0.0
    if cat:
        for s, sk in SIDES:
            pad = nose_c + P(-mz[0] * 1.05, sk * mz[1] * 0.72, -mz[2] * 1.0)
            pc.add(ellipsoid(pad, (mz[0] * 0.8, mz[1] * 0.85, mz[2] * 0.78), frame(fwd, P(0, 0, 1))), k * 0.45, "head")
        bridge = stop + (nose_c - stop) * 0.45 + P(-er * 0.3, 0, -er * 0.15)
        pc.add(ellipsoid(bridge, (er * 1.45, er * 1.15, er * 0.8), frame(nose_c - stop + P(0, 0, -0.004), P(0, 0, 1))), k * 0.6, "head")
    # the nose leather
    pc.add(ellipsoid(nose_c, b["nose"], frame(fwd, P(0, 0, 1))), k * 0.25, "head")
    # the lower jaw and chin
    ja, je = J["jaw"], J["jaw_end"]
    jfw = sdf.norm(je - ja)
    if cat:
        pc.add(round_cone(ja + P(0, 0, -mz[2] * 0.2), je + P(-mz[0] * 0.5, 0, -mz[2] * 0.25), mz[1] * 1.35, mz[1] * 0.62), k * 0.5, "jaw")
        pc.add(ellipsoid(je + P(-mz[0] * 0.7, 0, -mz[2] * 0.3), (mz[0] * 0.6, mz[1] * 0.62, mz[2] * 0.45), frame(jfw, P(0, 0, 1))), k * 0.35, "jaw")
    else:
        pc.add(round_cone(ja + P(0, 0, -mz[2] * 0.15), je + P(-mz[0] * 0.25, 0, 0), mz[1] * 0.9, mz[1] * 0.6), k * 0.5, "jaw")
        pc.add(ellipsoid(je + P(-mz[0] * 0.35, 0, -mz[2] * 0.15), (mz[0] * 0.5, mz[1] * 0.52, mz[2] * 0.42), frame(jfw, P(0, 0, 1))), k * 0.4, "jaw")
    # ears
    eb, et = J["ear"], J["ear_end"]
    ew, eth = b["ear"]["w"], b["ear"]["t"]
    for s, sk in SIDES:
        base = v3((eb[0], eb[1] * sk, eb[2]))
        tip = v3((et[0], et[1] * sk, et[2]))
        out = P(0.95, sk * (0.35 if cat else 0.25), 0.1)
        if b["ear"]["kind"] == "drop":
            # half-drop: up a third, then the flap folds forward and down
            mid = base + (tip - base) * 0.45
            flop = mid + P(0.6, sk * 0.25, -0.75) * float(np.linalg.norm(tip - base)) * 0.7
            pc.add(ear_prim(base, mid + (mid - base) * 0.15, out, ew, eth, cup=0.2), k * 0.4, "ear_" + s)
            pc.add(ear_prim(mid, flop, P(-0.5, -sk * 0.2, 0.8), ew * 0.85, eth * 0.9, cup=0.1), k * 0.2, "eartip_" + s)
        else:
            pc.add(ear_prim(base, tip, out, ew, eth, cup=0.35), k * 0.35, "ear_" + s)
    # eye sockets: the lids round the eyeball (the eyeball is its own mesh)
    for s, sk in SIDES:
        e = v3((eye[0], eye[1] * sk, eye[2]))
        pc.sub(ellipsoid(e + v3((b["eye_r"] * 0.25, sk * b["eye_r"] * 0.1, 0)), (b["eye_r"] * 0.95, b["eye_r"] * 1.05, b["eye_r"] * 0.88),
                         frame(P(math.cos(math.radians(b["eye_out"])), sk * math.sin(math.radians(b["eye_out"])), 0), P(0, 0, 1))), k * 0.15)
    # the mouth: a slit from the nose back to the corner, so the jaw can open
    mouth_a = nose_c + P(-b["nose"][0] * (1.6 if cat else 1.6), 0, -mz[2] * (1.75 if cat else 1.0))
    corner = ja + (je - ja) * (0.25 if cat else 0.18) + P(0, 0, mz[2] * 0.35)
    slit = round_cone(corner, mouth_a, mz[2] * 0.025, mz[2] * 0.035)
    sl_fn = slit.fn
    width = mz[1] * 1.6

    def mouth_fn(X, Y, Z, sl_fn=sl_fn):
        # flattened sideways: distance with the lateral axis squeezed, a blade across the mouth
        return sl_fn(X, Y * (mz[2] * 0.1 / width), Z)
    pc.sub(Prim(mouth_fn, np.minimum(corner, mouth_a) - width, np.maximum(corner, mouth_a) + width), 0.0)
    b["_mouth"] = (corner, mouth_a)


# a dog's head as two lofts along its axis (skull and muzzle on 'head', the lower jaw on 'jaw'): station s from the
# occiput (0) to the nose (1), half width, half height and the centre's height over the axis, as shares of the head's
# length; the laika's wedge (a broad skull, the stop, a tapering muzzle), stretched for the other breeds by their joints
DOG_HEAD = [(0.0, 0.15, 0.174, 0.065), (0.15, 0.27, 0.30, 0.074), (0.35, 0.285, 0.30, 0.08), (0.5, 0.21, 0.225, 0.07),
            (0.62, 0.135, 0.15, 0.035), (0.8, 0.10, 0.108, 0.03), (0.93, 0.08, 0.082, 0.04), (1.0, 0.058, 0.06, 0.058)]
DOG_JAW = [(0.33, 0.14, 0.07), (0.5, 0.11, 0.058), (0.7, 0.075, 0.042), (0.88, 0.055, 0.032), (0.96, 0.04, 0.025)]


def dog_head(pc, b):
    J = b["joints"]
    hb, he = J["head"], J["head_end"]
    fwd = sdf.norm(he - hb)
    up = sdf.norm(P(0, 0, 1) - fwd * float(np.dot(fwd, P(0, 0, 1))))
    L = float(np.linalg.norm(he - hb)) * 1.15
    o = hb - fwd * L * 0.1                      # the occiput, a little behind the skull's joint
    k = b["smooth"] * 0.35
    S = np.array([q[0] for q in DOG_HEAD])
    wide = b.get("head_wide", 1.0)

    def at(tab, s_, i):
        return float(np.interp(s_, [q[0] for q in tab], [q[i] for q in tab]))
    n = 26
    for i in range(n + 1):
        s_ = i / n * 0.955
        c = o + fwd * (L * s_) + up * (L * at(DOG_HEAD, s_, 3))
        r = (L / n * 1.6, L * at(DOG_HEAD, s_, 1) * wide, L * at(DOG_HEAD, s_, 2))
        pc.add(ellipsoid(c, r, frame(fwd, up)), k, "head")
    # the lower jaw under the muzzle
    for i in range(14):
        s_ = 0.33 + (0.93 - 0.33) * i / 13
        top = at(DOG_HEAD, s_, 3) - at(DOG_HEAD, s_, 2)
        hh = at(DOG_JAW, s_, 2)
        c = o + fwd * (L * s_) + up * (L * (top - hh * 0.75))
        pc.add(ellipsoid(c, (L / 13 * 1.6, L * at(DOG_JAW, s_, 1) * wide, L * hh), frame(fwd, up)), k * 0.8, "jaw")
    # the nose leather at the tip, a little up
    nose_c = o + fwd * (L * 0.995) + up * (L * 0.075)
    b["_nose_c"] = nose_c
    nr = (L * 0.05, L * 0.068, L * 0.05)
    b["nose"] = nr
    pc.add(ellipsoid(nose_c, nr, frame(fwd, up)), k * 0.3, "head")
    # the brow over each eye, the eye sockets
    eye = v3(b["eye"])
    er = b["eye_r"]
    for s, sk in SIDES:
        e = v3((eye[0], eye[1] * sk, eye[2]))
        pc.add(ellipsoid(e + P(-er * 0.5, sk * er * 0.1, er * 0.7), (er * 1.4, er * 1.0, er * 0.45), frame(fwd, up)), k * 0.8, "head")
        a = math.radians(b["eye_out"])
        pc.sub(ellipsoid(e + v3((er * 0.25, sk * er * 0.1, 0)), (er * 0.95, er * 1.05, er * 0.8),
                         frame(P(math.cos(a), sk * math.sin(a), 0), P(0, 0, 1))), k * 0.12)
    # ears: set high on the back of the skull
    eb, et = J["ear"], J["ear_end"]
    ew, eth = b["ear"]["w"], b["ear"]["t"]
    for s, sk in SIDES:
        base = v3((eb[0], eb[1] * sk, eb[2]))
        tip = v3((et[0], et[1] * sk, et[2]))
        out = P(0.97, sk * 0.15, 0.1)
        if b["ear"]["kind"] == "drop":
            mid = base + (tip - base) * 0.45
            flop = mid + P(0.55, sk * 0.3, -0.75) * float(np.linalg.norm(tip - base)) * 0.75
            pc.add(ear_prim(base, mid + (mid - base) * 0.15, out, ew, eth, cup=0.2), k * 0.5, "ear_" + s)
            pc.add(ear_prim(mid, flop, P(-0.4, -sk * 0.3, 0.85), ew * 0.85, eth * 0.9, cup=0.08), k * 0.25, "eartip_" + s)
        else:
            pc.add(ear_prim(base, tip, out, ew, eth, cup=0.32), k * 0.45, "ear_" + s)
    # the mouth: a slit between the muzzle and the jaw, from the corner to just behind the nose
    s0, s1 = 0.55, 0.96
    corner = o + fwd * (L * s0) + up * (L * (at(DOG_HEAD, s0, 3) - at(DOG_HEAD, s0, 2) + 0.01))
    front = o + fwd * (L * s1) + up * (L * (at(DOG_HEAD, s1, 3) - at(DOG_HEAD, s1, 2) + 0.005))
    width = L * 0.25 * wide
    slit = round_cone(corner, front, L * 0.004, L * 0.006)
    sl_fn = slit.fn

    def mouth_fn(X, Y, Z, sl_fn=sl_fn):
        return sl_fn(X, Y * (L * 0.01 / width), Z)
    pc.sub(Prim(mouth_fn, np.minimum(corner, front) - width, np.maximum(corner, front) + width), 0.0)
    b["_mouth"] = (corner, front)
