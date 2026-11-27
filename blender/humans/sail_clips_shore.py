"""The shore, class and coach clips (sail_clips.py): rigging, the trolley, launching, rinsing, carrying the mast, knots
and theory in the classroom, the coach with the megaphone, the whistle and the launch, the flag, watching the sea,
cheering. Standing clips: origin on the ground between the feet, facing -Y; seated ones on the house chair (origin under
the seat's centre, seat top 0.47 m, the shared seated pelvis (0, -0.01, 0.567)); Sail_Coach_Drive on the launch's
aft thwart (origin under its centre, seat top 0.40). Walk loops are in place: the actor moves at the speed given."""
import math

from mathutils import Vector

from sail_core import TAU, Z, arm, body, clip, grip, grip_frame, head_turn, look, relax, sm, stand_legs, yaw_axes
from sail_clips_boat import harm, scan, track

SEAT_PELVIS = Vector((0.0, -0.01, 0.567))
_rest = {}


def rest_pelvis(r):
    if "z" not in _rest:
        r.reset()
        _rest["z"] = r.head("pelvis").z
        _rest["hip"] = r.head("thigh_l").z
        _rest["leg"] = (r.head("calf_l") - r.head("thigh_l")).length + (r.head("foot_l") - r.head("calf_l")).length
        _rest["ankle"] = r.head("foot_l").z
    return _rest["z"]


def stand(r, t, T, yaw=0.0, lean=0.0, side=0.0, twist=0.0, spread=0.12, drop=0.015, sway=1.0, ahead=0.0, shift=Vector((0, 0, 0))):
    """Standing on both feet, weight shifting a little (sway), knees soft (drop: the pelvis that much under its rest)."""
    z = rest_pelvis(r) - drop
    f, left = yaw_axes(yaw)
    sx = harm(t, T, (0.012 * sway, 1, 0.0))
    pel = Vector((0, 0, z)) + left * sx + Vector(shift) + Vector((0, 0, harm(t, T, (0.004 * sway, 2, 0.5))))
    legs = stand_legs(r, spread=spread, ahead=ahead, f=f, left=left)
    return body(r, pel, yaw, tilt=2.0, lean=lean, side=side - sx * 60.0, twist=twist, legs=legs)


def hang(r, side, t, f=Vector((0, -1, 0)), left=Vector((1, 0, 0)), out=0.0):
    """An arm hanging relaxed by the side, the palm to the thigh, swinging a hair."""
    sign = 1 if side == "l" else -1
    s = r.head("upperarm_" + side)
    w = s + Vector((0, 0, -0.5)) + left * (sign * (0.04 + out)) + f * (0.04 + 0.01 * math.sin(t * 1.1 + sign))
    arm(r, side, w, (Vector((0, 0, -1)) + f * 0.15).normalized(), -left * sign, follow=False)
    relax(r, side, 1.2, t)


def hip_hand(r, side, f=Vector((0, -1, 0)), left=Vector((1, 0, 0))):
    """A hand on the hip, the elbow out."""
    sign = 1 if side == "l" else -1
    p = r.head("pelvis") + left * (sign * 0.17) + Vector((0, 0, 0.08)) + f * 0.02
    arm(r, side, p - f * 0.02 + left * sign * 0.03, (-f * 0.6 + Vector((0, 0, -0.8))).normalized(), -left * sign,
        pole=left * sign + (-f) * 0.5, follow=False)
    r.curl(side, 25.0, thumb=5.0)


def walk_legs(r, t, T, speed, lean=8.0, yaw=0.0, twist_k=1.0):
    """A walk in place at `speed` m/s (one cycle = two steps in T s): planted feet move back at exactly the speed (no
    sliding at rate 1), heel strike, toe off, the knee through; the pelvis bobs (highest mid-stance), yaws with the step
    and shifts over the standing foot. Returns (f, left, phase)."""
    rest = rest_pelvis(r)
    duty = 0.62
    stride = speed * T                       # ground covered per cycle
    stance = stride * duty
    land, off = -0.5 * stance, 0.5 * stance
    ph = (t / T) % 1.0
    f, left = yaw_axes(yaw)
    ankle_z = _rest["ankle"]
    reach = 0.97 * _rest["leg"]
    hip_drop = rest - _rest["hip"]
    pz = ankle_z + math.sqrt(max(reach ** 2 - land ** 2, 0.1)) + hip_drop - 0.02
    bob = 0.018 * math.cos(2 * TAU * (ph - duty / 2))
    sway = 0.018 * math.cos(TAU * (ph - duty / 2))
    pel = Vector((0, 0, pz + bob)) + left * sway
    legs = {}
    for s, sign, off_ph in (("l", 1, 0.0), ("r", -1, 0.5)):
        q = (ph - off_ph) % 1.0
        if q < duty:
            u = q / duty
            y = land + (off - land) * u
            zf = 0.075 * sm((u - 0.6) / 0.4)
            pitch = 8.0 * (1 - sm(u / 0.15)) - 26.0 * sm((u - 0.6) / 0.4)
        else:
            u = (q - duty) / (1 - duty)
            y = off + (land - off) * sm(u * 1.05)
            zf = 0.09 * math.sin(math.pi * min(1.0, u * 1.1)) ** 1.2
            zf += 0.075 * (1 - sm(u / 0.25))
            pitch = -26.0 + 41.0 * sm(u / 0.5) - 7.0 * sm((u - 0.8) / 0.2)
        a = Vector((0, 0, ankle_z + zf)) + left * (sign * 0.10) + f * (-y)
        pr = math.radians(pitch)
        toes = (f * math.cos(pr) * 0.95 + Vector((0, 0, math.sin(pr) - 0.42 * math.cos(pr)))).normalized()
        legs[s] = (a, toes, (f + left * sign * 0.08 + Vector((0, 0, 0.1))).normalized())
    yaw_hip = 5.0 * math.cos(TAU * (ph + 0.02)) * twist_k
    body(r, pel, yaw + yaw_hip, tilt=3.0, lean=lean, side=-sway * 40.0, twist=-yaw_hip * 1.5, legs=legs)
    return f, left, ph


def seated(r, t, T, lean=6.0, look_at=None, feet="normal", twist=0.0):
    """On the house chair (seat top 0.47, origin under its centre), feet on the floor."""
    fwd = {"normal": 0.44, "tucked": 0.32, "out": 0.6}[feet]
    legs = {}
    for s, sign in (("l", 1), ("r", -1)):
        legs[s] = (Vector((sign * 0.12, -fwd, 0.085)), Vector((sign * 0.08, -0.95, -0.42)).normalized(), Vector((sign * 0.05, -1.0, 0.25)))
    pel = SEAT_PELVIS + Vector((0, 0, harm(t, T, (0.003, 2, 0.0))))
    f, left = body(r, pel, 0.0, tilt=-6.0, lean=lean + harm(t, T, (1.0, 1, 0.3)), twist=twist, legs=legs)
    if look_at is not None:
        look(r, look_at)
    return f, left


def rope(a, b, color="rope", r_=0.006):
    return {"grip": Vector(b), "from": Vector(a), "axis": (Vector(b) - Vector(a)).normalized(), "radius": r_, "color": color}


# ---------------------------------------------------------------------------------------------- shore work

@clip("Sail_Rig_Mast", 5.0, frame="shore", about="standing at the stepped mast, hands up threading the sail's sleeve up it, hand over hand, looking up")
def sail_rig_mast(r, t, T):
    mast = Vector((0.0, -0.36, 0.0))
    f, left = stand(r, t, T, lean=-6.0, spread=0.13, sway=0.6)
    ph = (t / T) % 1.0
    # each hand in turn pushes the sleeve up: grips low, slides up to high, lets go and comes back down outside
    out = {"draw": [("rod", mast, mast + Vector((0, 0, 3.0)), 0.03, "spar")]}
    for s, sign, off in (("l", 1, 0.0), ("r", -1, 0.5)):
        q = (ph + off) % 1.0
        up = sm(q / 0.5)
        down = sm((q - 0.55) / 0.4)
        zh = 1.45 + 0.42 * up - 0.42 * down
        on = q < 0.52
        at = mast + Vector((sign * 0.035, 0.0, zh))
        if on:
            g = grip(r, s, at, Vector((0, 0, 1)), Vector((-sign * 0.6, -0.8, 0)), radius=0.035, curl=55.0)
            g["hand_side"] = s
            out["mast_" + s] = g
        else:
            w = at + Vector((sign * 0.12, 0.08, -0.05))
            arm(r, s, w, Vector((-sign * 0.3, -0.8, 0.5)).normalized(), Vector((-sign, 0.2, 0)).normalized())
            relax(r, s, 1.0, t)
    look(r, mast + Vector((0, 0, 2.2 + 0.3 * math.sin(TAU * t / T))), 0.9)
    return out


@clip("Sail_Rig_Knot", 4.0, frame="shore", about="crouched at the boat on its trolley, tying off at the boom/cleat about 0.5 m up, fingers working")
def sail_rig_knot(r, t, T):
    rest_pelvis(r)
    pel = Vector((0.0, 0.10, 0.50 + harm(t, T, (0.01, 1, 0.0))))
    legs = {"l": (Vector((0.17, -0.02, 0.085)), Vector((0.2, -0.95, -0.4)).normalized(), Vector((0.4, -1.0, 0.2)).normalized()),
            "r": (Vector((-0.17, 0.06, 0.13)), Vector((-0.2, -0.95, -0.6)).normalized(), Vector((-0.4, -1.0, 0.2)).normalized())}
    body(r, pel, 0.0, tilt=18.0, lean=30.0 + harm(t, T, (3.0, 2, 0.0)), legs=legs)
    look(r, Vector((0.0, -0.5, 0.52)), 1.0)
    boom = Vector((0.0, -0.55, 0.52))
    out = {"draw": [("rod", boom + Vector((-0.8, 0, 0)), boom + Vector((0.8, 0, 0)), 0.03, "spar")]}
    for s, sign in (("l", 1), ("r", -1)):
        ang = TAU * t / T * 2 + (0 if s == "l" else math.pi)
        c = boom + Vector((sign * 0.06 + 0.03 * math.cos(ang), 0.05 + 0.03 * math.sin(ang), 0.03 + 0.03 * math.sin(ang)))
        fd = Vector((-sign * 0.5, -0.8, -0.25 + 0.2 * math.sin(ang))).normalized()
        palm = Vector((-sign * 0.3, 0.2, -1.0))
        arm(r, s, c - fd * 0.08, fd, palm)
        k = 30.0 + 25.0 * (0.5 + 0.5 * math.sin(ang * 1.5))
        r.curl(s, {"index": k - 10, "middle": k, "ring": k + 10, "pinky": k + 15}, thumb=25.0)
    out["cord"] = dict(rope(r.head("hand_l") + Vector((-0.04, -0.07, 0)), r.head("hand_r") + Vector((0.04, -0.07, 0))), color="rope2")
    return out


@clip("Sail_Trolley_Push", 1.25, frame="shore", about="walk loop in place at 0.65 m/s pushing the launching trolley by its bow handle (bar 0.78 m up, 0.5 m ahead), leaning in")
def sail_trolley_push(r, t, T):
    f, left, ph = walk_legs(r, t, T, 0.65, lean=16.0)
    look(r, Vector((0.0, -4.0, 0.6)), 0.8)
    bar = Vector((0.0, -0.52, 0.80))
    out = {"draw": [("rod", bar + Vector((-0.3, 0, 0)), bar + Vector((0.3, 0, 0)), 0.018, "spar"),
                    ("rod", bar, bar + Vector((0, -1.4, -0.4)), 0.02, "spar")]}
    for s, sign in (("l", 1), ("r", -1)):
        at = bar + Vector((sign * 0.19, 0.0, 0.0))
        g = grip(r, s, at, Vector((sign, 0, 0)), Vector((0, -0.5, 1.0)), radius=0.018, curl=62.0)
        g["hand_side"] = s
        out["handle_" + s] = g
    return out


@clip("Sail_Launch_Hold", 6.0, frame="shore", about="standing thigh-deep (water 0.6 m) holding the boat head to wind by the bow (stem 0.55 m ahead, 0.75 m up), riding the waves")
def sail_launch_hold(r, t, T):
    wave = math.sin(TAU * t / T * 2)
    f, left = stand(r, t, T, lean=8.0 + 2.0 * wave, spread=0.18, sway=1.6, ahead=0.12)
    look(r, scan(t, T, [(1.0, (0.0, -0.9, 1.2)), (1.0, (-3.0, -8.0, 1.0)), (0.8, (4.0, -2.0, 1.4))]), 0.8)
    stem = Vector((0.0, -0.58, 0.78 + 0.04 * wave))
    out = {"draw": [("box", Vector((-1.2, -1.5, 0.58)), Vector((1.2, 0.8, 0.6)), "water"),
                    ("box", stem + Vector((-0.12, -1.2, -0.25)), stem + Vector((0.12, 0.0, 0.05)), "boat")]}
    for s, sign in (("l", 1), ("r", -1)):
        at = stem + Vector((sign * 0.13, -0.08, 0.04))
        g = grip(r, s, at, Vector((0, sign * 0.3, 1.0)), Vector((-sign, -0.3, 0.2)), radius=0.03, curl=55.0)
        g["hand_side"] = s
        out["bow_" + s] = g
    return out


@clip("Sail_Hose", 6.0, frame="shore", about="rinsing the hull on its trolley with a hose: the nozzle in the right hand sweeping along, the hose in the left")
def sail_hose(r, t, T):
    sweep = math.sin(TAU * t / T)
    f, left = stand(r, t, T, yaw=8.0 * sweep, lean=10.0, spread=0.14, sway=0.8, twist=10.0 * sweep)
    tgt = Vector((-0.6 * sweep, -1.3, 0.55))
    look(r, tgt, 0.9)
    G = Vector((-0.12 - 0.12 * sweep, -0.48, 1.0))
    d = (tgt - G).normalized()
    fd, n = grip_frame("r", -d, Vector((0, 0, -1)) + d * 0.2)
    fd2 = (d + Vector((0, 0, -0.5))).normalized()
    arm(r, "r", G - fd2 * 0.07, fd2, Vector((0.6, 0, -0.8)).normalized())
    r.curl("r", {"index": 35, "middle": 62, "ring": 66, "pinky": 70}, thumb=30)
    H = Vector((0.18, -0.25, 0.95))
    g2 = grip(r, "l", H, (G - H).normalized(), Vector((0.3, -0.5, -0.8)), radius=0.012, curl=60.0)
    nozzle_tip = r.head("hand_r") + fd2 * 0.08 + d * 0.16
    return {"draw": [("rod", r.head("hand_r") + fd2 * 0.07, nozzle_tip, 0.014, "prop"),
                     ("rod", nozzle_tip, nozzle_tip + d * 0.9, 0.004, "water"),
                     ("box", Vector((-1.2, -1.6, 0.3)), Vector((1.2, -1.25, 0.65)), "boat")],
            "hose": dict(rope(Vector((0.4, 0.6, 0.0)), H, "rope2", 0.012)), "hose_l": dict(g2, hand_side="l")}


@clip("Sail_Carry_Mast", 1.1, frame="shore", about="walk loop in place at 1.0 m/s, the mast on the right shoulder, both hands on it in front")
def sail_carry_mast(r, t, T):
    f, left, ph = walk_legs(r, t, T, 1.0, lean=4.0, twist_k=0.5)
    look(r, Vector((0.6, -5.0, 1.4)), 0.7)
    sh = r.head("clavicle_r").lerp(r.head("upperarm_r"), 0.7) + Vector((0, 0, 0.08))
    a = sh + Vector((0.0, 2.0, 0.08))
    b = sh + Vector((0.0, -2.5, -0.1))
    axis = (b - a).normalized()
    out = {"draw": [("rod", a, b, 0.035, "spar")]}
    p_r = a.lerp(b, (2.0 + 0.28) / 4.5)
    p_l = a.lerp(b, (2.0 + 0.55) / 4.5)
    g = grip(r, "r", p_r + Vector((0, 0, 0.0)), -axis, Vector((0.3, 0, 1.0)), radius=0.035, curl=58.0)
    out["mast_r"] = dict(g, hand_side="r")
    g = grip(r, "l", p_l, axis, Vector((0.2, 0, 1.0)), radius=0.035, curl=58.0)
    out["mast_l"] = dict(g, hand_side="l")
    return out


# ---------------------------------------------------------------------------------------------- the classroom

@clip("Sail_Knots_Sit", 6.0, frame="seat", scene="seat", about="seated at a desk (house chair; desk 0.76), tying a knot on a cord: loop, tuck, pull tight, shake it out")
def sail_knots_sit(r, t, T):
    u = (t / T) % 1.0
    f, left = seated(r, t, T, lean=16.0)
    look(r, Vector((0.0, -0.40, 0.85)), 1.0)
    base = Vector((0.0, -0.38, 0.86))
    out = {"draw": [("box", Vector((-0.6, -0.9, 0.73)), Vector((0.6, -0.30, 0.76)), "wood")]}
    for s, sign in (("l", 1), ("r", -1)):
        loop_ = math.sin(TAU * u * 2 + (0 if s == "l" else 1.2))
        pull = sm((u - 0.62) / 0.12) * (1 - sm((u - 0.85) / 0.12))
        c = base + Vector((sign * (0.07 + 0.12 * pull), -0.02 + 0.03 * loop_, 0.02 * math.cos(TAU * u * 2)))
        fd = Vector((-sign * 0.55, -0.75, 0.2 + 0.15 * loop_)).normalized()
        palm = Vector((-sign * 0.5, 0.3, -0.7))
        arm(r, s, c - fd * 0.07, fd, palm)
        k = 25.0 + 30.0 * (0.5 + 0.5 * loop_) * (1 - pull) + 40.0 * pull
        r.curl(s, {"index": k - 12, "middle": k, "ring": k + 10, "pinky": k + 14}, thumb=28.0)
    out["cord"] = dict(rope(r.head("hand_l") + Vector((-0.05, -0.06, 0.0)), r.head("hand_r") + Vector((0.05, -0.06, 0.0)), "rope2"))
    return out


@clip("Sail_Listen_Class", 10.0, frame="seat", scene="seat", about="seated at a desk listening, looking at the blackboard (3.5 m ahead, 1.6 m up) and following the coach along it, a nod")
def sail_listen_class(r, t, T):
    f, left = seated(r, t, T, lean=8.0 + 4.0 * win(t, 6.5, 9.0))
    tg = Vector((1.2 * math.sin(TAU * t / T), -3.5, 1.6 + 0.15 * math.sin(TAU * t / T * 2)))
    look(r, tg, 0.95)
    nod = math.sin(TAU * t * 1.5) * win(t, 3.0, 4.4, 0.4)
    head_turn(r, pitch=-6.0 * nod)
    out = {"draw": [("box", Vector((-0.6, -0.9, 0.73)), Vector((0.6, -0.30, 0.76)), "wood"),
                    ("box", Vector((-1.5, -3.6, 0.9)), Vector((1.5, -3.55, 2.1)), "dark")]}
    for s, sign in (("l", 1), ("r", -1)):
        c = Vector((sign * 0.17, -0.40, 0.79))
        fd = Vector((-sign * 0.6, -0.8, -0.05)).normalized()
        arm(r, s, c, fd, Vector((sign * 0.15, 0, -1)).normalized(), pole=Vector((sign * 0.6, 0.6, -0.6)))
        relax(r, s, 1.5, t)
    return out


def win(t, a, b, rise=0.25):
    return sm((t - a) / rise + 0.5) * sm((b - t) / rise + 0.5)


# ---------------------------------------------------------------------------------------------- the coach

def megaphone_hand(r, f, left, call=1.0):
    """The megaphone's pistol grip in the right fist under the chin, the cone forward from the mouth."""
    h = r.head("head")
    hf, hl = r.axes("head")
    mouth = h + hf * 0.10 + Vector((0, 0, 0.02))
    G = mouth + hf * 0.04 + Vector((0, 0, -0.12)) - hl * 0.0
    g = grip(r, "r", G, Vector((0, 0, 1)), hf + hl * 0.4, radius=0.016, curl=66.0, pole=-hl * 0.8 + Vector((0, 0, -0.6)))
    return g, mouth, hf


@clip("Sail_Coach_Point", 6.0, frame="shore", about="the coach standing, pointing with the right arm at the blackboard / out to sea, the megaphone hanging in the left hand")
def sail_coach_point(r, t, T):
    u = (t / T) % 1.0
    which = sm((u - 0.45) / 0.1) * (1 - sm((u - 0.92) / 0.08))
    tgt = Vector((-1.5, -3.0, 1.4)).lerp(Vector((2.5, -9.0, 1.0)), which)
    f, left = stand(r, t, T, yaw=-12.0 + 22.0 * which, lean=3.0, twist=6.0 * which)
    look(r, tgt, 0.95)
    sh = r.head("upperarm_r")
    d = (tgt - sh).normalized()
    pt = sh + d * 0.62 + Vector((0, 0, 0.03 * math.sin(TAU * t * 1.2)))
    arm(r, "r", pt, d, Vector((0, 0, -1)) - d * d.z)
    r.curl("r", {"index": 2, "middle": 70, "ring": 75, "pinky": 78}, thumb=30)
    hang(r, "l", t, f, left)
    hl = r.head("hand_l")
    return {"draw": [("rod", hl + Vector((0, -0.02, -0.06)), hl + Vector((0, -0.25, -0.12)), 0.05, "prop")]}


@clip("Sail_Coach_Megaphone", 5.0, frame="shore", about="the coach calling through the megaphone held to the mouth, leaning into the calls, the left hand on the hip")
def sail_coach_megaphone(r, t, T):
    call = win(t, 0.4, 2.0, 0.3) + win(t, 2.8, 4.3, 0.3)
    yaw = 15.0 * math.sin(TAU * t / T)
    f, left = stand(r, t, T, yaw=yaw * 0.3, lean=4.0 + 6.0 * call, twist=yaw * 0.4, spread=0.15)
    look(r, Vector((-3.0 + 6.0 * (0.5 + 0.5 * math.sin(TAU * t / T)), -12.0, 1.6)), 0.8)
    g, mouth, hf = megaphone_hand(r, f, left, call)
    hip_hand(r, "l", f, left)
    return {"megaphone": dict(g, hand_side="r"), "draw": [("rod", mouth + hf * 0.02, mouth + hf * 0.32, 0.06, "prop")]}


@clip("Sail_Coach_Drive", 8.0, frame="seat", scene="launch", about="the coach on the launch's aft thwart (seat 0.40) steering the outboard's tiller behind on the right, looking round the fleet")
def sail_coach_drive(r, t, T):
    seat = 0.40
    look_yaw = 70.0 * math.sin(TAU * t / T) + 20.0 * math.sin(TAU * t / T * 3 + 1.0)
    pel = Vector((0.0, -0.01, seat + 0.097))
    legs = {"l": (Vector((0.15, -0.42, 0.085)), Vector((0.1, -0.95, -0.42)).normalized(), Vector((0.1, -1.0, 0.3))),
            "r": (Vector((-0.10, -0.36, 0.085)), Vector((-0.05, -0.95, -0.42)).normalized(), Vector((-0.1, -1.0, 0.3)))}
    f, left = body(r, pel, 0.0, tilt=-4.0, lean=4.0, twist=-15.0 + look_yaw * 0.25, legs=legs)
    a = math.radians(-look_yaw)
    look(r, pel + Vector((math.sin(-a) * -6.0, -6.0 * math.cos(a), 1.0)), 0.9)
    # the outboard's tiller arm: from the motor on the transom (0.55 behind, on the centreline) forward to the right hand
    motor = Vector((0.0, 0.62, 0.55))
    steer = math.radians(8.0 * math.sin(TAU * t / T * 2))
    G = motor + Vector((-0.28 - 0.04 * math.sin(steer), -0.42, 0.02))
    axis = (G - motor).normalized()
    g = grip(r, "r", G, axis, Vector((-0.4, 0, 1.0)), radius=0.017, curl=60.0)
    w = pel + Vector((0.24, -0.28, 0.08))
    arm(r, "l", w, Vector((0.1, -1.0, -0.5)).normalized(), Vector((0.2, 0, -1)).normalized())
    relax(r, "l", 1.6, t)
    return {"tiller": dict(g, hand_side="r"),
            "draw": [("box", Vector((-0.75, -0.18, seat - 0.03)), Vector((0.75, 0.18, seat)), "wood"),
                     ("box", Vector((-0.8, -2.5, -0.03)), Vector((0.8, 0.7, 0.0)), "deck"),
                     ("rod", motor, G + axis * 0.08, 0.02, "dark"), ("box", motor + Vector((-0.12, 0.0, -0.2)), motor + Vector((0.12, 0.35, 0.2)), "dark")]}


@clip("Sail_Whistle", 3.0, frame="shore", about="the coach blowing the whistle (on its lanyard) in the right hand, the left arm up as the signal")
def sail_whistle(r, t, T):
    blow = win(t, 0.5, 1.0, 0.1) + win(t, 1.3, 2.2, 0.1)
    f, left = stand(r, t, T, lean=-2.0 + 3.0 * blow, spread=0.15)
    look(r, Vector((-1.0, -10.0, 1.8)), 0.8)
    h = r.head("head")
    hf, hl = r.axes("head")
    mouth = h + hf * 0.10 + Vector((0, 0, 0.0))
    # the whistle between the thumb and the curled index at the lips
    fd = (hl * 0.9 + hf * 0.1 + Vector((0, 0, 0.3))).normalized()
    arm(r, "r", mouth + hf * 0.02 - fd * 0.11 + Vector((0, 0, -0.03)), fd, -hf, pole=-hl + Vector((0, 0, -0.8)))
    r.curl("r", {"index": 55, "middle": 75, "ring": 80, "pinky": 82}, thumb=25)
    up = sm((t / T) / 0.15) * (1 - sm(((t / T) - 0.85) / 0.15))
    sh = r.head("upperarm_l")
    w = sh + Vector((0.12, -0.08, 0.15 + 0.4 * up)) + Vector((0.1, 0, 0)) * (1 - up)
    arm(r, "l", w, Vector((0.1, -0.1, 1.0)).normalized(), Vector((0, 1, 0)), pole=Vector((1, 0.2, -0.3)))
    relax(r, "l", 0.4, t)
    return {"draw": [("rod", mouth + hf * 0.01, mouth + hf * 0.05, 0.008, "prop")]}


@clip("Sail_Flag_Hoist", 2.4, frame="shore", about="hoisting a flag on the signal mast's halyard, hand over hand (halyard 0.33 m ahead)")
def sail_flag_hoist(r, t, T):
    ph = (t / T) % 1.0
    f, left = stand(r, t, T, lean=-4.0 + 4.0 * math.sin(TAU * ph * 2), spread=0.14, sway=0.5)
    line = Vector((0.0, -0.33, 0.0))
    look(r, line + Vector((0, 0, 3.0)), 0.7)
    out = {"draw": [("rod", line + Vector((0, -0.08, 0)), line + Vector((0, -0.08, 3.2)), 0.04, "spar")]}
    for s, sign, off in (("l", 1, 0.0), ("r", -1, 0.5)):
        q = (ph + off) % 1.0
        if q < 0.55:                                   # holding: pulls down from high to low
            zh = 1.85 - 0.75 * sm(q / 0.55)
            at = line + Vector((0.0, 0.0, zh))
            g = grip(r, s, at, Vector((0, 0, -1)), Vector((-sign * 0.5, -0.8, 0)), radius=0.006, curl=70.0)
            out["halyard_" + s] = dict(g, hand_side=s)
        else:                                          # lets go and reaches up again
            u = (q - 0.55) / 0.45
            zh = 1.10 + 0.75 * sm(u)
            w = line + Vector((sign * 0.09, 0.08, zh - 0.05))
            arm(r, s, w, Vector((-sign * 0.2, -0.6, 0.8)).normalized(), Vector((-sign, 0.3, 0)).normalized())
            relax(r, s, 1.0, t)
    out["halyard"] = rope(line, line + Vector((0, 0, 3.0)), "rope")
    return out


@clip("Sail_Watch_Sea", 8.0, frame="shore", about="standing on the shore with a hand shading the eyes, slowly scanning the sea")
def sail_watch_sea(r, t, T):
    yaw = 30.0 * math.sin(TAU * t / T)
    f, left = stand(r, t, T, yaw=yaw * 0.25, lean=-2.0, twist=yaw * 0.3, sway=0.8)
    look(r, Vector((yaw * 0.25, -20.0, 1.5)), 0.9)
    hf, hl = r.axes("head")
    brow = r.head("head") + hf * 0.12 + Vector((0, 0, 0.12))
    fd = (hl * 0.85 + hf * 0.2).normalized()
    arm(r, "r", brow - fd * 0.06 + Vector((0, 0, 0.02)), fd, Vector((0, 0, -1)), pole=-hl + Vector((0, 0, -0.5)))
    r.curl("r", 12.0, thumb=5.0)
    hip_hand(r, "l", f, left)
    return {}


@clip("Sail_Cheer", 3.0, frame="shore", about="spectators and finishers cheering: arms up with fists pumping and a bounce, then clapping")
def sail_cheer(r, t, T):
    u = (t / T) % 1.0
    up = sm(u / 0.1) * (1 - sm((u - 0.5) / 0.1))
    bounce = abs(math.sin(TAU * t * 1.0)) * up
    f, left = stand(r, t, T, lean=-6.0 * up, spread=0.15, drop=0.015 + 0.04 * bounce)
    look(r, Vector((0.0, -8.0, 2.5 * up + 1.4)), 0.8)
    clap = (1 - up) * (0.5 + 0.5 * math.cos(TAU * t * 2.5))
    for s, sign in (("l", 1), ("r", -1)):
        sh = r.head("upperarm_" + s)
        hi = sh + Vector((sign * 0.18, -0.08, 0.5 + 0.06 * math.sin(TAU * t * 2.0 + sign)))
        lo = Vector((sign * (0.03 + 0.08 * clap), -0.35, 1.25))
        w = lo.lerp(hi, up)
        fd = Vector((-sign * 0.8, -0.3, 0.3)).normalized().lerp(Vector((sign * 0.1, 0, 1.0)), up).normalized()
        palm = Vector((-sign, 0, 0)).lerp(Vector((0, -1, 0)), up)
        arm(r, s, w, fd, palm, pole=Vector((sign, 0.3, -0.4)))
        r.curl(s, 15.0 + 60.0 * up, thumb=20.0 + 20.0 * up)
    return {}
