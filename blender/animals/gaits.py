"""Poses and clips of the cats and dogs (pure numpy): a small pose solver over the breed's skeleton (anatomy.bones) and
the clip library. A pose says how the body is carried (root offset and turn about the body's middle, the back's bend,
the neck and head, the jaw, the ears, the tail) and where each foot is; the legs are solved by IK (front: shoulder blade
swing + two-bone humerus/forearm with the pastern; hind: two-bone with the metatarsus kept parallel to the femur, the
quadruped's rule) or set by joint angles (sitting, lying). The result is, per bone, its rotation from the bind pose in
the armature's space; build_animal.py turns those into pose-bone rotations.

Locomotion clips are one stride cycle in place: the feet slide back at the clip's speed while on the ground, so the
game plays them at rate = speed / clip speed (Walk, Trot, Run, Stalk report their speeds in <Breed>_rig.json).
Footfalls: walk = lateral sequence (LH, LF, RH, RF a quarter apart, duty 0.62), trot = diagonal pairs (duty 0.42),
gallop = rotary for dogs and a half-bound for cats (duty 0.3, with the back flexing and stretching).
"""
import math

import numpy as np

import anatomy as AN

LEGS = ("LF", "RF", "LH", "RH")


def rx(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def ry(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def rz(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def rot_between(a, b):
    a = a / np.linalg.norm(a)
    b = b / np.linalg.norm(b)
    v = np.cross(a, b)
    c = float(np.dot(a, b))
    if c < -0.999999:
        axis = np.cross(a, (1, 0, 0))
        if np.linalg.norm(axis) < 1e-6:
            axis = np.cross(a, (0, 1, 0))
        axis = axis / np.linalg.norm(axis)
        return axis_angle(axis, math.pi)
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx * (1.0 / (1.0 + c))


def axis_angle(axis, ang):
    a = np.asarray(axis, float)
    a = a / np.linalg.norm(a)
    c, s = math.cos(ang), math.sin(ang)
    x, y, z = a
    return np.array([[c + x * x * (1 - c), x * y * (1 - c) - z * s, x * z * (1 - c) + y * s],
                     [y * x * (1 - c) + z * s, c + y * y * (1 - c), y * z * (1 - c) - x * s],
                     [z * x * (1 - c) - y * s, z * y * (1 - c) + x * s, c + z * z * (1 - c)]])


def two_bone(a, t, l1, l2, pole):
    """Joint position of a two-bone chain from a to t (lengths l1, l2), bending toward pole (a direction)."""
    d = t - a
    L = float(np.linalg.norm(d))
    L = min(max(L, abs(l1 - l2) + 1e-4), l1 + l2 - 1e-4)
    dn = d / max(np.linalg.norm(d), 1e-9)
    x = (l1 * l1 - l2 * l2 + L * L) / (2 * L)
    h = math.sqrt(max(l1 * l1 - x * x, 0.0))
    p = pole - dn * np.dot(pole, dn)
    if np.linalg.norm(p) < 1e-6:
        p = np.cross(dn, (0, 1, 0))
    p = p / np.linalg.norm(p)
    return a + dn * x + p * h


def smoothstep(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def lerp(a, b, t):
    return a + (b - a) * t


class Rig:
    def __init__(self, breed):
        self.b = breed
        self.bones = AN.bones(breed)
        self.idx = {n: i for i, (n, h, t, p) in enumerate(self.bones)}
        self.head0 = {n: np.array(h) for n, h, t, p in self.bones}
        self.tail0 = {n: np.array(t) for n, h, t, p in self.bones}
        self.parent = {n: p for n, h, t, p in self.bones}
        self.dir0 = {n: (self.tail0[n] - self.head0[n]) for n in self.head0}
        self.len = {n: float(np.linalg.norm(self.dir0[n])) for n in self.head0}
        self.tails = sorted([n for n in self.head0 if n.startswith("tail_")])
        J = breed["joints"]
        self.cat = breed["kind"] == "cat"
        self.H = J["chest"][2]
        # the body's middle: rotations of the body turn about it
        self.pivot = (J["pelvis"] + J["chest"]) * 0.5
        self.drop_ears = breed["ear"]["kind"] == "drop"

    # ------------------------------------------------------------------ solve
    def solve(self, pose):
        """pose: dict (see neutral()). Returns {bone: (delta 3x3, head position)}."""
        D, Pp = {}, {}
        b = self.b
        R0 = rz(pose["yaw"]) @ ry(pose["pitch"]) @ rx(pose["roll"])
        t = np.asarray(pose["offset"], float)
        piv = self.pivot + np.asarray(pose.get("pivot", (0, 0, 0)), float)
        root_pos = piv + t - R0 @ piv
        D["root"], Pp["root"] = R0, root_pos

        def place(n, delta):
            p = self.parent[n]
            Pp[n] = Pp[p] + D[p] @ (self.head0[n] - self.head0[p])
            D[n] = delta

        def rel(n, R):
            """bone n turned by R (in its parent's turned frame)."""
            p = self.parent[n]
            place(n, D[p] @ R)

        # the back: pelvis, two loin bones, the chest; bends (pitch: + arches the back up) and side bends spread over them
        flex, side, twist = pose["spine_flex"], pose["spine_side"], pose["spine_twist"]
        # an arch (flex > 0): the pelvis tips its front up, then each joint turns the back down again (a hump)
        rel("pelvis", rz(side * 0.15) @ ry(-flex * 0.55) @ rx(twist * 0.3))
        rel("spine_01", rz(side * 0.3) @ ry(flex * 0.4))
        rel("spine_02", rz(side * 0.3) @ ry(flex * 0.4) @ rx(-twist * 0.3))
        rel("chest", rz(side * 0.25) @ ry(flex * 0.3) @ rx(-twist * 0.4))
        # neck and head: pitch + = nose up
        rel("neck_01", rz(pose["neck_yaw"] * 0.5) @ ry(-pose["neck_pitch"] * 0.55 - flex * 0.6))
        rel("neck_02", rz(pose["neck_yaw"] * 0.5) @ ry(-pose["neck_pitch"] * 0.45))
        rel("head", rz(pose["head_yaw"]) @ ry(-pose["head_pitch"]) @ rx(pose["head_roll"]))
        rel("jaw", ry(pose["jaw"]))
        for s, k in AN.SIDES:
            e = pose["ears"][s]      # (back: + turns the ear back, flat: + lays it out sideways, droop)
            ear_R = rz(-k * e[0] * 0.9) @ rx(k * e[1]) @ ry(e[0] * 0.5)
            rel("ear_" + s, ear_R)
            if self.drop_ears:
                rel("eartip_" + s, ry(e[2] if len(e) > 2 else 0.0))
        # tail: base lift, a curl along it, a side curve and a wave
        nt = len(self.tails)
        for i, n in enumerate(self.tails):
            u = i / max(nt - 1, 1)
            lift = pose["tail_lift"] if i == 0 else 0.0
            curl = pose["tail_curl"] * (0.4 + 1.2 * u) / nt * 2.0
            sidec = pose["tail_side"] / nt + pose["tail_wave"] * math.sin(pose["tail_phase"] - u * 2.2) * (0.4 + u) / nt * 2.0
            rel(n, rz(sidec) @ ry(lift + curl))      # the tail points back: + lifts it
        # legs
        for leg in LEGS:
            self.solve_leg(leg, pose["legs"][leg], D, Pp)
        return D, Pp

    def solve_leg(self, leg, L, D, Pp):
        s = "l" if leg[0] == "L" else "r"
        front = leg[1] == "F"
        k = 1.0 if s == "l" else -1.0
        if front:
            sc, ua, fa, ha, pa = "scapula_" + s, "upperarm_" + s, "forearm_" + s, "hand_" + s, "paw_" + s
            p = self.parent[sc]
            Pp[sc] = Pp[p] + D[p] @ (self.head0[sc] - self.head0[p])
            D[sc] = D[p] @ ry(L.get("scap", 0.0))
            shoulder = Pp[sc] + D[sc] @ (self.head0[ua] - self.head0[sc])
            pts = self.leg_points(L, front, shoulder, D[p], s)
            if pts is not None:
                self.aim_chain([ua, fa, ha, pa], sc, shoulder, pts, D, Pp)
                return
            if "fk" in L:
                # joint angles (pitch, + folds the joint): humerus, elbow, carpus, toes
                a_ua, a_el, a_ca, a_to = L["fk"]
                self.fk_chain([ua, fa, ha, pa], [ry(a_ua), ry(-a_el), ry(a_ca), ry(a_to)], D, Pp, sc, shoulder)
                return
            paw_t = np.asarray(L["foot"], float)
            if L.get("frame") == "body":
                paw_t = D["root"] @ paw_t + Pp["root"]
            hdir = np.asarray(L.get("hand", (0.12, 0, -1.0)), float)
            hdir = hdir / np.linalg.norm(hdir)
            wrist = paw_t - hdir * self.len[ha]
            elbow = two_bone(shoulder, wrist, self.len[ua], self.len[fa], D[sc] @ np.array([-1.0, 0.15 * k, 0.0]))
            self.aim(ua, sc, shoulder, elbow, D, Pp)
            self.aim(fa, ua, elbow, wrist, D, Pp)
            self.aim(ha, fa, wrist, paw_t, D, Pp)
            tdir = np.asarray(L.get("toes", (1.0, 0, -0.1)), float)
            self.aim(pa, ha, paw_t, paw_t + tdir / np.linalg.norm(tdir) * self.len[pa], D, Pp)
        else:
            th, sh, ft, to = "thigh_" + s, "shin_" + s, "foot_" + s, "toe_" + s
            p = self.parent[th]
            hip = Pp[p] + D[p] @ (self.head0[th] - self.head0[p])
            pts = self.leg_points(L, front, hip, D[p], s)
            if pts is not None:
                self.aim_chain([th, sh, ft, to], p, hip, pts, D, Pp)
                return
            if "fk" in L:
                a_th, a_kn, a_ho, a_to = L["fk"]   # femur pitch (+ forward), stifle, hock, toes
                self.fk_chain([th, sh, ft, to], [ry(-a_th), ry(a_kn), ry(-a_ho), ry(a_to)], D, Pp, p, hip)
                return
            toe_t = np.asarray(L["foot"], float)
            if L.get("frame") == "body":
                toe_t = D["root"] @ toe_t + Pp["root"]
            lt, ls, lf = self.len[th], self.len[sh], self.len[ft]
            # the femur and the metatarsus parallel: a two-bone chain of (femur + metatarsus) and the tibia
            hock_flex = L.get("hock", 0.0)
            mid = two_bone(hip, toe_t, lt + lf, ls, D[p] @ np.array([1.0, 0.1 * k, 0.0]))
            u = (mid - hip) / np.linalg.norm(mid - hip)
            knee = hip + u * lt
            w = (toe_t - (knee + u * lf))
            hock = knee + w / np.linalg.norm(w) * ls if np.linalg.norm(w) > 1e-6 else knee
            if hock_flex:
                # turn the metatarsus about the hock (toe target is then approximate)
                v = toe_t - hock
                ax = np.cross(v, (0, 0, 1))
                if np.linalg.norm(ax) > 1e-6:
                    toe_t = hock + axis_angle(ax, hock_flex) @ v
            self.aim(th, p, hip, knee, D, Pp)
            self.aim(sh, th, knee, hock, D, Pp)
            self.aim(ft, sh, hock, toe_t, D, Pp)
            tdir = np.asarray(L.get("toes", (1.0, 0, -0.1)), float)
            self.aim(to, ft, toe_t, toe_t + tdir / np.linalg.norm(tdir) * self.len[to], D, Pp)

    def leg_points(self, L, front, top, Dp, side):
        """World targets of the leg's joints (elbow/knee, wrist/hock, paw/toe, toes' end) for the posed modes, or None."""
        s = self.H / (0.25 if self.cat else 0.55)
        if "body" in L:
            # joints given in the bind pose's frame relative to the leg's top joint, in cat-sized metres (scaled to the
            # animal by its height at the withers), the side's sign mirrored; they move with the body
            k = 1.0 if side == "l" else -1.0
            sb = self.H / 0.25
            return [top + Dp @ (np.asarray(q, float) * np.array([1.0, k, 1.0]) * sb) for q in L["body"]]
        mode = L.get("mode")
        if mode is None:
            return None
        names = (("upperarm", "forearm", "hand", "paw") if front else ("thigh", "shin", "foot", "toe"))
        ln = [self.len[n + "_" + side] for n in names]
        gz = L.get("ground", 0.012 * s)
        fwd = np.array([math.cos(L.get("yaw", 0.0)), math.sin(L.get("yaw", 0.0)), 0.0])
        if mode == "sit" or mode == "tuck":
            # hind: the thigh forward (a little down), the hock and the metatarsus on the ground pointing forward
            a = math.radians(L.get("thigh", 10.0))
            knee = top + fwd * ln[0] * math.cos(a) - np.array([0, 0, 1.0]) * ln[0] * math.sin(a)
            dz = max(min(knee[2] - gz, ln[1] * 0.98), -ln[1] * 0.98)
            hock = knee - fwd * math.sqrt(max(ln[1] ** 2 - dz ** 2, 1e-8)) - np.array([0, 0, dz])
            toe = hock + fwd * ln[2] + np.array([0, 0, 0.002])
            return [knee, hock, toe, toe + fwd * ln[3]]
        if mode == "sphinx":
            # front: the elbow on the ground under the shoulder, the forearm and the paw forward along the ground
            dz = max(min(top[2] - gz * 1.6, ln[0] * 0.98), -ln[0] * 0.98)
            elbow = top - fwd * math.sqrt(max(ln[0] ** 2 - dz ** 2, 1e-8)) * L.get("back", 1.0) - np.array([0, 0, dz])
            elbow[2] = max(elbow[2], gz * 1.6)
            wrist = elbow + fwd * ln[1]
            paw = wrist + fwd * ln[2] - np.array([0, 0, 0.003])
            return [elbow, wrist, paw, paw + fwd * ln[3]]
        return None

    def aim_chain(self, names, parent, start, pts, D, Pp):
        pos = start
        prev = parent
        for n, t in zip(names, pts):
            d = np.asarray(t, float) - pos
            if np.linalg.norm(d) < 1e-6:
                d = D[prev] @ self.dir0[n]
            self.aim(n, prev, pos, pos + d, D, Pp)
            pos = pos + d / np.linalg.norm(d) * self.len[n]
            prev = n

    def aim(self, n, parent, head, tail, D, Pp):
        Pp[n] = head
        inherited = D[parent] @ self.dir0[n]
        D[n] = rot_between(inherited, tail - head) @ D[parent]

    def fk_chain(self, names, rots, D, Pp, parent, start):
        prev = parent
        pos = start
        for n, R in zip(names, rots):
            Pp[n] = pos
            D[n] = D[prev] @ R
            pos = pos + D[n] @ self.dir0[n]
            prev = n

    # ------------------------------------------------------------------ the neutral stance
    def rest_foot(self, leg):
        s = "l" if leg[0] == "L" else "r"
        if leg[1] == "F":
            return self.head0["paw_" + s].copy()
        return self.head0["toe_" + s].copy()

    def neutral(self):
        legs = {}
        for leg in LEGS:
            f = self.rest_foot(leg)
            legs[leg] = dict(foot=f, toes=(1.0, 0, -0.08))
            if leg[1] == "F":
                s = "l" if leg[0] == "L" else "r"
                hd = self.head0["paw_" + s] - self.head0["hand_" + s]
                legs[leg]["hand"] = hd / np.linalg.norm(hd)
        return dict(offset=(0.0, 0.0, 0.0), yaw=0.0, pitch=0.0, roll=0.0, spine_flex=0.0, spine_side=0.0, spine_twist=0.0,
                    neck_pitch=0.0, neck_yaw=0.0, head_pitch=0.0, head_yaw=0.0, head_roll=0.0, jaw=0.0,
                    ears=dict(l=[0.0, 0.0, 0.0], r=[0.0, 0.0, 0.0]),
                    tail_lift=0.0, tail_curl=0.0, tail_side=0.0, tail_wave=0.0, tail_phase=0.0, legs=legs,
                    scale=dict())


def copy_pose(p):
    import copy
    return copy.deepcopy(p)


def blend(a, b, t):
    """Pose between a and b (numbers and arrays mixed; leg modes taken from b when t > 0.5)."""
    out = copy_pose(a)
    for k, v in b.items():
        if k == "legs":
            for leg in LEGS:
                la, lb = a["legs"][leg], b["legs"][leg]
                if set(la) != set(lb) or la.get("mode") != lb.get("mode"):
                    out["legs"][leg] = copy_pose(lb if t > 0.5 else la)
                    continue
                for kk in lb:
                    out["legs"][leg][kk] = lerp(np.asarray(la[kk], float), np.asarray(lb[kk], float), t)
        elif k == "ears":
            for s in ("l", "r"):
                out["ears"][s] = list(lerp(np.asarray(a["ears"][s], float), np.asarray(v[s], float), t))
        elif k == "scale":
            out["scale"] = {n: lerp(a["scale"].get(n, 1.0), v.get(n, 1.0), t) for n in set(a["scale"]) | set(v)}
        elif isinstance(v, (int, float)):
            out[k] = lerp(a[k], v, t)
        else:
            out[k] = tuple(lerp(np.asarray(a[k], float), np.asarray(v, float), t))
    return out


# ====================================================================== clips
class Library:
    """Clip name -> (seconds, loop, fn(t) -> pose); speeds of the locomotion clips (m/s)."""

    def __init__(self, rig):
        self.r = rig
        self.cat = rig.cat
        self.H = rig.H
        self.clips = {}
        self.speeds = {}
        r = rig
        H = self.H
        cat = self.cat
        if cat:
            self.gait("Walk", period=0.72, stride=0.40, duty=0.62, phases=dict(LH=0.0, LF=0.25, RH=0.5, RF=0.75), lift=0.18, bob=0.006)
            self.gait("Trot", period=0.46, stride=0.78, duty=0.42, phases=dict(LF=0.0, RH=0.0, RF=0.5, LH=0.5), lift=0.24, bob=0.01)
            self.gait("Run", period=0.30, stride=1.35, duty=0.30, phases=dict(LH=0.0, RH=0.06, LF=0.48, RF=0.56), lift=0.32, bob=0.03, gallop=1.0)
            self.gait("Stalk", period=1.6, stride=0.45, duty=0.78, phases=dict(LH=0.0, LF=0.25, RH=0.5, RF=0.75), lift=0.12, bob=0.003, low=0.32)
        else:
            self.gait("Walk", period=0.86, stride=1.0, duty=0.62, phases=dict(LH=0.0, LF=0.25, RH=0.5, RF=0.75), lift=0.16, bob=0.012)
            self.gait("Trot", period=0.56, stride=1.34, duty=0.42, phases=dict(LF=0.0, RH=0.0, RF=0.5, LH=0.5), lift=0.22, bob=0.02)
            self.gait("Run", period=0.40, stride=2.2, duty=0.30, phases=dict(RH=0.0, LH=0.1, LF=0.48, RF=0.58), lift=0.3, bob=0.05, gallop=1.0)
        self.statics()

    # --------------------------------------------------------------- gaits
    def gait(self, name, period, stride, duty, phases, lift, bob, gallop=0.0, low=0.0):
        r, H = self.r, self.H
        size = H / (0.25 if self.cat else 0.55)
        stride *= size
        speed = stride / period
        self.speeds[name] = speed
        base = r.neutral()

        def fn(t):
            ph = (t / period) % 1.0
            p = copy_pose(base)
            lowz = -low * H
            p["offset"] = (0.0, 0.0, lowz)
            for leg in LEGS:
                q = (ph - phases[leg]) % 1.0
                f0 = r.rest_foot(leg)
                front = leg[1] == "F"
                if low:
                    # stalking: the legs flexed under a low body, the feet a little wider and further under
                    f0 = f0 + np.array([(-0.02 if front else 0.025) * size, 0.006 * np.sign(f0[1]) * size, 0])
                if q < duty:
                    u = q / duty
                    x = stride * (0.5 - u)
                    z = 0.0
                    toes = (1.0, 0, -0.08)
                    hand = (0.1, 0, -1.0)
                else:
                    u = (q - duty) / (1 - duty)
                    e = smoothstep(u)
                    x = stride * (-0.5 + e)
                    z = lift * H * math.sin(math.pi * u) ** (0.8 if front else 1.0)
                    # the front paw folds back at the wrist in the swing, the hind toes curl
                    fold = math.sin(math.pi * min(u * 1.3, 1.0))
                    hand = (0.1 - 1.1 * fold, 0, -1.0 + 0.4 * fold)
                    toes = (1.0, 0, -0.08 - 1.4 * fold) if front else (1.0, 0, -0.08 - 0.9 * fold)
                foot = f0 + np.array([x * 0.5, 0, z])
                p["legs"][leg]["foot"] = foot
                p["legs"][leg]["toes"] = toes
                if front:
                    p["legs"][leg]["hand"] = hand
                    # the shoulder blade swings with the leg
                    p["legs"][leg]["scap"] = -0.35 * (x / max(stride, 1e-6)) * (1.0 + gallop * 0.6)
            # the body: a bob twice a stride (walk, trot), a roll and sway with the hind legs; galloping, the back flexes and
            # stretches once a stride and the body pitches
            w = 2 * math.pi * ph
            p["offset"] = (0.0, 0.0, lowz - bob * size * math.cos(2 * w) * (1 - gallop) + gallop * bob * size * math.sin(w))
            p["roll"] = 0.03 * math.sin(w) * (1 - gallop)
            p["spine_side"] = 0.06 * math.sin(w) * (1 - gallop)
            if gallop:
                p["spine_flex"] = 0.32 * math.sin(w + 0.5) if self.cat else 0.2 * math.sin(w + 0.5)
                p["pitch"] = 0.06 * math.sin(w - 0.6)
            # the head: steady (birds of prey and cats keep the eyes level), a nod in the walk
            p["neck_pitch"] = (-0.25 if low else -0.05) + 0.04 * math.cos(2 * w) * (1 - gallop) - gallop * 0.15
            p["head_pitch"] = (0.25 if low else 0.08) - p["neck_pitch"] * 0.6 + (0.1 if gallop else 0.0)
            if low:
                p["ears"] = dict(l=[0.1, 0.0, 0.0], r=[0.1, 0.0, 0.0])
            # the tail: carried up and curving (cats walking), straight out (running), low (stalking); a laika's curled
            if self.cat:
                p["tail_lift"] = -0.35 if low else (0.6 if not gallop else 0.1)
                p["tail_curl"] = -0.5 if low else (0.9 if not gallop else -0.2)
                p["tail_wave"] = 0.15
                p["tail_phase"] = w
            else:
                self.dog_tail(p, w, 0.25 + 0.1 * gallop)
            return p
        self.clips[name] = (period, True, fn)

    def dog_tail(self, p, w, wag):
        breed = self.r.b.get("name", "")
        if breed == "Laika" or breed == "Mutt":
            p["tail_lift"] = 1.05
            p["tail_curl"] = 2.4 if breed == "Laika" else 1.2
        else:
            # a shepherd's tail hangs low (a generated body's rest tail already hangs: lift it a little off the legs)
            p["tail_lift"] = 0.35 if self.r.b.get("gen") else -0.55
            p["tail_curl"] = -0.4
        p["tail_wave"] = wag
        p["tail_phase"] = w * 1.5

    # --------------------------------------------------------------- still poses and one-shots
    def statics(self):
        r, H, cat = self.r, self.H, self.cat
        size = H / (0.25 if cat else 0.55)
        J = r.b["joints"]
        stand = r.neutral()
        if cat:
            stand["tail_lift"], stand["tail_curl"] = 0.25, 0.6
        else:
            self.dog_tail(stand, 0.0, 0.05)
        hip_z = J["thigh"][2]

        def loop(name, seconds, fn):
            self.clips[name] = (seconds, True, fn)

        def once(name, seconds, fn):
            self.clips[name] = (seconds, False, fn)

        # Idle: breathing, weight shifts, a look round, ear flicks, the tail's slow sway
        def idle(t):
            p = copy_pose(stand)
            w = 2 * math.pi * t / 4.0
            p["spine_flex"] = 0.015 * math.sin(2 * math.pi * t / 1.6)
            p["offset"] = (0.004 * size * math.sin(w), 0.0, 0.0)
            p["head_yaw"] = 0.25 * math.sin(w * 0.5) + 0.12 * math.sin(w * 1.7)
            p["head_pitch"] = 0.05 + 0.04 * math.sin(w * 0.8)
            flick = max(0.0, math.sin(2 * math.pi * t / 4.0 * 3.0)) ** 12
            p["ears"] = dict(l=[0.15 * flick, 0.0, 0.0], r=[0.0, 0.0, 0.0])
            p["tail_wave"] = 0.18 if cat else 0.08
            p["tail_phase"] = w * 0.5
            return p
        loop("Idle", 4.0, idle)

        # Watch: alert, head up and forward, ears pricked, still; the tail tip twitches (cats)
        def watch(t):
            p = copy_pose(stand)
            p["neck_pitch"] = 0.2
            p["head_pitch"] = -0.05
            p["ears"] = dict(l=[-0.15, 0.0, 0.0], r=[-0.15, 0.0, 0.0])
            p["offset"] = (0.0, 0.0, 0.01 * size)
            p["tail_wave"] = 0.06 if not cat else 0.12 * math.sin(2 * math.pi * t * 0.9) ** 2
            p["tail_phase"] = 2 * math.pi * t * 1.3
            if cat:
                p["tail_lift"], p["tail_curl"] = -0.1, -0.3
            return p
        loop("Watch", 2.0, watch)

        # Sniff: the nose to the ground, working
        def sniff(t):
            p = copy_pose(stand)
            p["neck_pitch"] = -0.85 if not cat else -0.8
            p["head_pitch"] = -0.6 + 0.06 * math.sin(2 * math.pi * t * 3.0)
            p["head_yaw"] = 0.2 * math.sin(2 * math.pi * t / 2.0)
            p["pitch"] = 0.3
            p["offset"] = (0.015 * size, 0.0, -0.01 * size)
            for leg in ("LF", "RF"):
                p["legs"][leg]["foot"] = r.rest_foot(leg) + np.array([0.025 * size, 0, 0])
            p["spine_flex"] = -0.05
            return p
        loop("Sniff", 2.0, sniff)

        # Eat: head down to the bowl, chewing
        def eat(t):
            p = sniff(0.0)
            p["neck_pitch"] = -0.95
            p["head_pitch"] = -0.75 + 0.05 * math.sin(2 * math.pi * t * 2.2)
            p["pitch"] = 0.36
            p["head_yaw"] = 0.04 * math.sin(2 * math.pi * t * 0.5)
            p["jaw"] = 0.12 * max(0.0, math.sin(2 * math.pi * t * 2.2))
            p["ears"] = dict(l=[0.25, 0.1, 0.0], r=[0.25, 0.1, 0.0])
            return p
        loop("Eat", 2.0, eat)

        # Sit: haunches down, the hind legs folded, the front legs straight; tail round the feet (cats) or on the ground
        def sit_pose():
            p = copy_pose(stand)
            ang = 0.62 if cat else 0.55
            p["pitch"] = -ang
            p["pivot"] = (J["pelvis"][0] - r.pivot[0], 0, 0)
            # drop so the haunches touch the ground
            p["offset"] = (0.03 * size if cat else 0.06 * size, 0.0, -(hip_z * (0.62 if cat else 0.5)))
            p["spine_flex"] = 0.12
            p["neck_pitch"] = -0.05 if cat else 0.1
            p["head_pitch"] = -0.35 if cat else -0.3
            for leg in ("LH", "RH"):
                p["legs"][leg] = dict(mode="sit", thigh=-8.0 if cat else -5.0)
            for leg in ("LF", "RF"):
                f = r.rest_foot(leg)
                p["legs"][leg]["foot"] = f + np.array([-0.045 * size, 0.0, 0.0])
                p["legs"][leg]["hand"] = (0.15, 0, -1.0)
            if cat:
                p["tail_lift"], p["tail_curl"], p["tail_side"] = -1.2, 0.0, 2.4
            else:
                p["tail_lift"], p["tail_curl"] = -1.1, -0.2
            return p
        sit = sit_pose()

        def sitf(t):
            p = copy_pose(sit)
            p["head_yaw"] = 0.3 * math.sin(2 * math.pi * t / 6.0)
            p["spine_flex"] += 0.015 * math.sin(2 * math.pi * t / 1.7)
            p["tail_wave"] = 0.05
            p["tail_phase"] = 2 * math.pi * t / 3.0
            return p
        loop("Sit", 6.0, sitf)

        # Rest: lying with the head up (a cat's loaf, paws tucked; a dog on its chest with the forelegs out)
        def rest_pose():
            p = copy_pose(stand)
            belly = r.b["trunk"][5][2] if cat else r.b["trunk"][6][2]
            p["offset"] = (0.0, 0.0, -belly * 0.92)
            p["neck_pitch"] = 0.0 if cat else 0.25
            p["head_pitch"] = 0.05
            for leg in ("LF", "RF"):
                p["legs"][leg] = dict(mode="sphinx", scap=0.0, back=0.6 if cat else 0.8)
            for leg in ("LH", "RH"):
                p["legs"][leg] = dict(mode="tuck", thigh=30.0)
            if cat:
                p["tail_lift"], p["tail_curl"], p["tail_side"] = -0.5, 0.0, 1.6
            else:
                p["tail_lift"], p["tail_curl"], p["tail_side"] = -0.5, 0.0, 0.8
                p["spine_side"] = 0.15
            return p
        rest = rest_pose()

        def restf(t):
            p = copy_pose(rest)
            p["spine_flex"] = 0.02 * math.sin(2 * math.pi * t / 2.0)
            p["head_yaw"] = 0.25 * math.sin(2 * math.pi * t / 8.0)
            return p
        loop("Rest", 8.0, restf)

        # Sleep: curled up on the side, nose to the tail: the body rolled onto its side, the back curled (flexed) round
        def sleepf(t):
            p = copy_pose(rest)
            p["roll"] = 1.42
            hw = max(q[3] for q in r.b["trunk"])
            p["offset"] = (0.0, 0.0, -r.pivot[2] + hw * 1.1)
            p["spine_side"] = 0.0
            p["spine_flex"] = (1.9 if cat else 1.5) + 0.03 * math.sin(2 * math.pi * t / 3.0)
            p["neck_pitch"] = -0.8
            p["neck_yaw"] = 0.0
            p["head_yaw"] = 0.25
            p["head_pitch"] = -0.55
            p["head_roll"] = 0.0
            p["ears"] = dict(l=[0.3, 0.2, 0.0], r=[0.3, 0.2, 0.0])
            p["tail_lift"], p["tail_curl"], p["tail_side"] = -0.5, -2.2, 0.2
            for leg in ("LF", "RF"):
                p["legs"][leg] = dict(body=[(-0.02, 0, -0.075), (0.04, 0, -0.09), (0.06, 0, -0.088), (0.085, 0, -0.088)], scap=0.2)
            for leg in ("LH", "RH"):
                p["legs"][leg] = dict(body=[(0.085, 0, -0.06), (0.005, 0, -0.075), (0.055, 0, -0.085), (0.08, 0, -0.085)])
            return p
        loop("Sleep", 3.0, sleepf)

        # Groom: a cat washing its paw and face; a dog scratching behind its ear with a hind foot
        if cat:
            def groom(t):
                p = copy_pose(sit)
                c = (t % 2.4) / 2.4
                lick = c < 0.6
                p["legs"]["RF"] = dict(body=[(0.03, 0.005, -0.05), (0.075, -0.004, 0.0), (0.085, -0.004, 0.025), (0.09, -0.004, 0.045)], scap=-0.3)
                p["neck_pitch"] = -0.1
                p["neck_yaw"] = -0.15
                p["head_pitch"] = -0.55 + (0.12 * math.sin(2 * math.pi * t * 3.0) if lick else 0.25 * math.sin(math.pi * (c - 0.6) / 0.4))
                p["head_yaw"] = -0.25
                p["jaw"] = 0.12 * max(0.0, math.sin(2 * math.pi * t * 3.0)) if lick else 0.0
                p["ears"] = dict(l=[0.2, 0.0, 0.0], r=[0.2, 0.0, 0.0])
                return p
            loop("Groom", 2.4, groom)
        else:
            def scratch(t):
                p = copy_pose(sit)
                a = 0.25 * math.sin(2 * math.pi * t * 6.0)
                p["legs"]["RH"] = dict(body=[(0.10, 0, -0.03 + a * 0.05), (0.12, -0.02, 0.06 + a * 0.08), (0.16, -0.03, 0.12 + a * 0.06), (0.18, -0.03, 0.15 + a * 0.06)])
                p["head_roll"] = -0.35
                p["head_yaw"] = -0.3
                p["neck_yaw"] = -0.2
                p["roll"] = 0.12
                p["jaw"] = 0.05
                return p
            loop("Groom", 1.5, scratch)

        # Crouch: a cat ready to pounce (low, hind feet under, the rear wiggling, the tail tip twitching)
        def crouch_pose(t):
            p = copy_pose(stand)
            p["offset"] = (-0.02 * size, 0.0, -0.4 * H)
            p["pitch"] = 0.06
            for leg in ("LF", "RF"):
                f = r.rest_foot(leg) + np.array([0.02 * size, 0, 0])
                p["legs"][leg]["foot"] = f
                p["legs"][leg]["hand"] = (0.35, 0, -1.0)
            for leg in ("LH", "RH"):
                p["legs"][leg]["foot"] = r.rest_foot(leg) + np.array([0.05 * size, 0, 0])
            p["neck_pitch"] = -0.2
            p["head_pitch"] = 0.25
            p["ears"] = dict(l=[-0.2, 0.0, 0.0], r=[-0.2, 0.0, 0.0])
            p["spine_side"] = 0.05 * math.sin(2 * math.pi * t * 2.5)
            p["roll"] = 0.04 * math.sin(2 * math.pi * t * 2.5)
            p["tail_lift"], p["tail_curl"] = -0.4, -0.6
            p["tail_wave"] = 0.35
            p["tail_phase"] = 2 * math.pi * t * 2.0
            return p
        loop("Crouch", 1.2, crouch_pose)

        # Pounce: the spring from the crouch: hind legs push, the body flies forward, the forepaws reach and land
        def pounce(t):
            T = 0.55
            u = t / T
            p = crouch_pose(0.0)
            if u < 0.25:
                e = smoothstep(u / 0.25)
                p["offset"] = (lerp(-0.02, 0.05, e) * size, 0, lerp(-0.4, 0.05, e) * H)
                p["pitch"] = lerp(0.06, -0.3, e)
                for leg in ("LH", "RH"):
                    p["legs"][leg]["foot"] = r.rest_foot(leg) + np.array([lerp(0.05, -0.12, e) * size, 0, 0])
            else:
                e = smoothstep((u - 0.25) / 0.75)
                p["offset"] = (lerp(0.05, 0.0, e) * size, 0, (0.05 + 0.25 * math.sin(math.pi * e) - 0.05 * e) * H)
                p["pitch"] = lerp(-0.3, 0.18, e)
                p["spine_flex"] = lerp(-0.25, 0.1, e)
                for leg in ("LF", "RF"):
                    p["legs"][leg]["foot"] = r.rest_foot(leg) + np.array([lerp(0.12, 0.04, e) * size, 0, lerp(0.12, 0.0, e) * H])
                    p["legs"][leg]["hand"] = (lerp(0.9, 0.3, e), 0, -1.0)
                for leg in ("LH", "RH"):
                    p["legs"][leg]["foot"] = r.rest_foot(leg) + np.array([lerp(-0.12, 0.0, e) * size, 0, 0.08 * H * math.sin(math.pi * e)])
            return p
        once("Pounce", 0.55, pounce)

        # Hiss (cats): the back arched high, up on straight legs, ears flat, mouth open, tail up and puffed
        def hiss(t):
            p = copy_pose(stand)
            p["offset"] = (0.0, 0.0, 0.035 * size)
            p["spine_flex"] = 1.35
            for leg in LEGS:
                p["legs"][leg]["foot"] = r.rest_foot(leg) + np.array([(-0.055 if leg[1] == "F" else 0.06) * size, 0, 0])
            p["neck_pitch"] = -0.55
            p["head_pitch"] = 0.35
            p["head_yaw"] = -0.5
            p["jaw"] = 0.45 + 0.05 * math.sin(2 * math.pi * t * 4.0)
            p["ears"] = dict(l=[0.9, 0.8, 0.0], r=[0.9, 0.8, 0.0])
            p["tail_lift"], p["tail_curl"] = 1.3, -0.4
            p["scale"] = {n: 1.55 for n in r.tails}
            p["scale"].update({"chest": 1.12, "spine_02": 1.15, "spine_01": 1.15})
            return p
        loop("Hiss", 1.0, hiss)

        # Climb (cats): up a trunk, nose up, the legs bounding against it (the actor holds it on the trunk)
        if cat:
            def climb(t):
                p = copy_pose(stand)
                w = 2 * math.pi * t / 0.5
                p["pitch"] = -1.35
                p["pivot"] = (0.0, 0.0, 0.0)
                p["offset"] = (-0.12 * size, 0.0, 0.18 * size)
                p["spine_flex"] = 0.25 * math.sin(w)
                for leg in LEGS:
                    front = leg[1] == "F"
                    f = r.rest_foot(leg)
                    reach = (0.06 if front else 0.05) * size * math.sin(w + (0.0 if front else math.pi))
                    p["legs"][leg]["foot"] = f + np.array([0.03 * size + reach, 0, 0.02 * size + max(0.0, reach) * 0.5])
                    p["legs"][leg]["frame"] = "body"
                p["neck_pitch"] = 0.6
                p["tail_lift"], p["tail_curl"] = -0.6, 0.0
                p["tail_wave"] = 0.2
                p["tail_phase"] = w
                return p
            loop("Climb", 0.5, climb)

            # Perch: loafed up a tree, the tail hanging
            def perch(t):
                p = restf(t)
                p["tail_lift"], p["tail_curl"], p["tail_side"] = -1.3, -0.2, 0.0
                p["tail_wave"] = 0.08
                p["tail_phase"] = 2 * math.pi * t / 4.0
                return p
            loop("Perch", 8.0, perch)

            # Rub: walking along the player's legs, head rolled into them, the tail up with its tip hooked
            walk = self.clips["Walk"][2]

            def rub(t):
                p = walk(t * 0.5)
                p["head_roll"] = 0.55 + 0.15 * math.sin(2 * math.pi * t / 1.44)
                p["head_yaw"] = 0.35
                p["neck_yaw"] = 0.25
                p["roll"] = 0.12
                p["tail_lift"], p["tail_curl"] = 1.2, 1.3
                p["ears"] = dict(l=[0.15, 0.1, 0.0], r=[0.15, 0.1, 0.0])
                return p
            loop("Rub", 1.44, rub)
        else:
            # Bark: braced, the head forward, two barks a cycle with a jolt each
            def bark(t):
                p = copy_pose(stand)
                c = (t % 0.7) / 0.7
                k = math.exp(-((c - 0.2) / 0.08) ** 2)
                p["offset"] = (-0.01 * size - 0.012 * size * k, 0, -0.01 * size)
                p["neck_pitch"] = 0.1 + 0.08 * k
                p["head_pitch"] = 0.1 + 0.12 * k
                p["jaw"] = 0.5 * k
                p["ears"] = dict(l=[-0.2 + 0.2 * k, 0.0, 0.0], r=[-0.2 + 0.2 * k, 0.0, 0.0])
                for leg in ("LF", "RF"):
                    p["legs"][leg]["foot"] = r.rest_foot(leg) + np.array([0.03 * size, 0, 0])
                p["tail_lift"] = stand["tail_lift"] + 0.2
                p["tail_wave"] = 0.1
                p["tail_phase"] = 2 * math.pi * t * 2.0
                return p
            loop("Bark", 1.4, bark)

            # PlayBow: elbows down, the rear up, the tail wagging high
            def bow(t):
                p = copy_pose(stand)
                p["pitch"] = 0.42
                p["pivot"] = (J["pelvis"][0] - r.pivot[0], 0, 0)
                p["offset"] = (-0.02 * size, 0, -0.03 * size)
                for leg in ("LF", "RF"):
                    p["legs"][leg] = dict(mode="sphinx", scap=-0.2, back=0.5)
                p["neck_pitch"] = 0.55
                p["head_pitch"] = 0.15
                p["jaw"] = 0.15
                self.dog_tail(p, 2 * math.pi * t * 3.0, 0.8)
                p["tail_lift"] += 0.3
                return p
            loop("PlayBow", 1.0, bow)

            # Play: bounces on the spot, turning side to side (between rushes)
            def play(t):
                p = copy_pose(stand)
                w = 2 * math.pi * t / 0.8
                hop = max(0.0, math.sin(w))
                p["offset"] = (0.0, 0.0, 0.06 * size * hop)
                p["yaw"] = 0.35 * math.sin(w * 0.5)
                p["pitch"] = -0.15 * hop
                for leg in ("LF", "RF"):
                    p["legs"][leg]["foot"] = r.rest_foot(leg) + np.array([0.04 * size * hop, 0, 0.08 * size * hop])
                    p["legs"][leg]["hand"] = (0.2 - 0.6 * hop, 0, -1.0)
                p["jaw"] = 0.25
                self.dog_tail(p, w * 2.0, 0.7)
                return p
            loop("Play", 1.6, play)

            # Beg: sitting, one forepaw lifted, the head tilted
            def beg(t):
                p = copy_pose(sit)
                p["legs"]["RF"] = dict(body=[(0.04, 0, -0.12), (0.14, 0, -0.10), (0.17, 0, -0.09), (0.21, 0, -0.09)], scap=-0.3)
                p["head_roll"] = 0.25 * math.sin(2 * math.pi * t / 3.0)
                p["head_pitch"] = 0.2
                p["neck_pitch"] = 0.45
                p["ears"] = dict(l=[0.35, 0.1, 0.0], r=[0.35, 0.1, 0.0])
                p["tail_lift"], p["tail_curl"], p["tail_side"] = -1.1, 0.0, 0.0
                p["tail_wave"] = 0.4
                p["tail_phase"] = 2 * math.pi * t * 2.5
                return p
            loop("Beg", 3.0, beg)

    def sample(self, name, fps=30):
        T, looped, fn = self.clips[name]
        n = int(round(T * fps))
        return [fn(i / fps) for i in range(n + (0 if looped else 1))]
