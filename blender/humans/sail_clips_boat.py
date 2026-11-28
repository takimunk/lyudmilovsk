"""The boat clips (sail_clips.py): the helm on the side deck, hiking, tacking and gybing, light airs, the optimist's
child, the cadet's crew, capsizing, righting and climbing back in. Everything authored for the port side (the body on
+X, the tiller in the right hand); sail_clips.py poses the _Stbd mirror image.

A clip's function poses the body at time t and returns its hand targets ({role: grip}); a role with "pivot" is the
tiller's extension, with "from" a rope. sail_grips.json carries them to the engine (hand IK onto the real tiller and
sheet: docs/yacht_school.md "Clips (part D)")."""
import math

from mathutils import Vector

from sail_core import (BOATS, TAU, Z, arm, body, clip, grip, grip_frame, lerp, look, relax, sm, win, yaw_axes)


# ---------------------------------------------------------------------------------------------- tracks

def _cr(p0, p1, p2, p3, s):
    return 0.5 * ((2 * p1) + (-p0 + p2) * s + (2 * p0 - 5 * p1 + 4 * p2 - p3) * s * s + (-p0 + 3 * p1 - 3 * p2 + p3) * s ** 3)


def track(keys, t, loop_T=None):
    """Catmull-Rom through [(time, value)] (floats or Vectors), clamped at the ends (or wrapped over loop_T)."""
    n = len(keys)
    if t <= keys[0][0] and loop_T is None:
        return keys[0][1].copy() if hasattr(keys[0][1], "copy") else keys[0][1]
    if t >= keys[-1][0] and loop_T is None:
        return keys[-1][1].copy() if hasattr(keys[-1][1], "copy") else keys[-1][1]
    i = max(j for j in range(n) if keys[j][0] <= t) if t >= keys[0][0] else 0
    i = min(i, n - 2)
    t0, t1 = keys[i][0], keys[i + 1][0]
    s = (t - t0) / max(t1 - t0, 1e-6)
    v = lambda j: keys[max(0, min(n - 1, j))][1]  # noqa: E731
    return _cr(v(i - 1), v(i), v(i + 1), v(i + 2), s)


def state_at(keys, t):
    """A body state (dict of floats/Vectors) between keyed states."""
    out = {}
    for name in keys[0][1]:
        out[name] = track([(kt, ks[name]) for kt, ks in keys], t)
    return out


def harm(t, T, *terms):
    """A loop-safe wiggle: sum of amp * sin(TAU * (n t / T) + phase) over (amp, n, phase)."""
    return sum(a * math.sin(TAU * n * t / T + ph) for a, n, ph in terms)


def scan(t, T, targets):
    """The eyes' round over a loop: [(dwell share, point)], quick turns between holds."""
    tot = sum(d for d, _ in targets)
    x = (t / T) % 1.0 * tot
    acc = 0.0
    for i, (d, q) in enumerate(targets):
        if x < acc + d:
            nxt = targets[(i + 1) % len(targets)][1]
            u = (x - acc) / d
            w = sm((u - 0.8) / 0.2)               # holds 80 %, turns in the last 20 %
            return Vector(q).lerp(Vector(nxt), w)
        acc += d
    return Vector(targets[0][1])


# ---------------------------------------------------------------------------------------------- the helm

def kb(B):
    return 1.0 / B["scale"]


def tiller_pivot(B, a):
    """The tiller's end (the extension's joint) with the tiller `a` degrees to port."""
    k = kb(B)
    rud = Vector(B["rudder"]) * k
    r = math.radians(a)
    return rud + Vector((math.sin(r), -math.cos(r), 0.0)) * B["tiller_len"] * k


def helm_state(B, s=1, h=0.0, gust=0.0):
    """Seated (h 0), half hiked (0.5, knees bent, bum on the edge) or full hiked (1, legs straight, hips out over the
    water) on the side deck of side s (+1 port, -1 starboard), facing inboard and forward, feet under the strap."""
    k = kb(B)

    def l3(a, b, c):
        return lerp(a, b, h * 2) if h < 0.5 else lerp(b, c, (h - 0.5) * 2)
    hx = l3(B["sit_x"] * k, B["half_beam"] * k + 0.02, B["half_beam"] * k + 0.11)
    pel = Vector((s * hx, 0.0, B["deck_z"] * k + l3(0.10, 0.075, 0.035)))
    yaw = s * l3(-55.0, -60.0, -64.0)
    f, left = yaw_axes(yaw)
    sx = B["strap"][0] * k
    fwd_foot, aft_foot = ("l", "r") if s > 0 else ("r", "l")
    fa = Vector((s * (sx + l3(0.16, 0.17, 0.2)), l3(-0.36, -0.34, -0.3), 0.085))
    aa = Vector((s * (sx + l3(0.11, 0.13, 0.17)), l3(-0.10, -0.08, -0.05), 0.085))
    toes = (f * 0.95 + Vector((0, 0, l3(-0.12, -0.05, 0.05)))).normalized()
    pole = (Vector((0, 0, 1)) + f * 0.4).normalized()
    return {"pelvis": pel, "yaw": yaw, "tilt": l3(-8.0, -14.0, -18.0), "lean": l3(13.0, -16.0, -34.0) - 6.0 * gust * (0.3 + h),
            "side": 0.0, "twist": s * l3(14.0, 10.0, 6.0),
            fwd_foot + "a": fa, aft_foot + "a": aa, "lt": toes, "rt": toes.copy(), "lp": pole, "rp": pole.copy(),
            "h": h}


def pose_state(r, st):
    legs = {sd: (st[sd + "a"], st[sd + "t"], st[sd + "p"]) for sd in "lr"}
    return body(r, st["pelvis"], st["yaw"], st["tilt"], st["lean"], st["side"], st["twist"], legs)


def ext_role(B, st, s, a, side, reach=None):
    """The tiller extension in the aft hand: where the hand wants to be (in front of the stomach, higher hiked) brought
    onto the extension's reach from the tiller's end."""
    k = kb(B)
    f, left = yaw_axes(st["yaw"])
    aft = -s * left
    P = tiller_pivot(B, a)
    pel = st["pelvis"]
    Hd = reach if reach is not None else pel + f * 0.36 + aft * 0.10 + Vector((0, 0, 0.30 + 0.16 * st.get("h", 0.0)))
    d = Hd - P
    L = min(d.length, B["ext_len"] * k - 0.04)
    G = P + d.normalized() * L
    return {"grip": G, "axis": d.normalized(), "knuckles": f + Vector((0, 0, 0.3)), "radius": 0.012,
            "pivot": P, "rudder": Vector(B["rudder"]) * k, "beyond": B["ext_len"] * k - L, "hand_side": side}


def sheet_role(B, st, s, trim, side, block=None, up=0.0):
    k = kb(B)
    f, left = yaw_axes(st["yaw"])
    aft = -s * left
    S = Vector(block) if block is not None else Vector(B["sheet_block"]) * k
    pel = st["pelvis"]
    Hs = pel + f * 0.40 - aft * 0.13 + Vector((0, 0, 0.22 + 0.13 * st.get("h", 0.0) + up)) + (-f * 0.12 + Vector((0, 0, 0.08))) * trim
    e = (Hs - S).normalized()
    return {"grip": Hs, "axis": e, "knuckles": f - aft * 0.3, "radius": 0.005, "from": S, "to": Hs + e * 0.12 + Vector((0, 0, -0.25)),
            "hand_side": side}


def hold(r, role, side, curl=62.0, pole=None, open_=0.0):
    """The hand onto a role's bar or rope (open_ 0..1 opens the fist on the way between)."""
    fd, n = grip_frame(side, role["axis"], role["knuckles"])
    wrist = role["grip"] - fd * 0.072 - n * (0.026 + role["radius"])
    arm(r, side, wrist, fd, n, pole)
    c = curl * (1 - 0.75 * open_)
    r.curl(side, {"index": c - 4, "middle": c, "ring": c + 3, "pinky": c + 6}, thumb=30.0 * (1 - 0.5 * open_))
    out = dict(role)
    out.update({"palm": n, "fingers": fd})
    return out


def blend_hold(r, side, ra, rb, w, curl=62.0, pole=None):
    """The hand on its way from role ra to role rb (w 0..1): the wrist and its frame blended, the fist opening between."""
    if w <= 0.001:
        return hold(r, ra, side, curl, pole)
    if w >= 0.999:
        return hold(r, rb, side, curl, pole)
    fa, na = grip_frame(side, ra["axis"], ra["knuckles"])
    fb, nb = grip_frame(side, rb["axis"], rb["knuckles"])
    wa = ra["grip"] - fa * 0.072 - na * (0.026 + ra["radius"])
    wb = rb["grip"] - fb * 0.072 - nb * (0.026 + rb["radius"])
    lift = Vector((0, 0, 0.06 * math.sin(math.pi * w)))
    wrist = wa.lerp(wb, w) + lift
    fd = fa.lerp(fb, w).normalized()
    n = na.lerp(nb, w)
    n = (n - fd * n.dot(fd)).normalized()
    arm(r, side, wrist, fd, n, pole)
    c = curl * (1 - 0.75 * math.sin(math.pi * w))
    r.curl(side, {"index": c - 4, "middle": c, "ring": c + 3, "pinky": c + 6}, thumb=30.0 * (1 - 0.5 * math.sin(math.pi * w)))
    return None


def helm_targets(B, mast_k):
    k = kb(B)
    my = B["mast_y"] * k
    return [(1.2, (0.0, my + 0.35, 2.0 * k)), (1.0, (-2.5, -9.0, 1.3)), (0.9, (-0.5, my + 0.6, 2.6 * k)),
            (0.7, (6.0, 4.0, 1.0)), (1.1, (-1.2, -9.0, 1.4)), (0.8, (-1.6, 0.0, 1.9 * k))]


def helm_loop(r, B, t, T, h, s=1, gust_amp=1.0, look_k=1.0):
    st = helm_state(B, s, h, gust=0.5 + 0.5 * math.sin(TAU * t / T))
    st["lean"] += harm(t, T, (2.0 * gust_amp, 2, 0.4), (1.0, 3, 1.1))
    st["pelvis"] += Vector((0, 0, harm(t, T, (0.006, 4, 0.0))))
    # looking over the shoulder: the chest turns with the head
    tg = scan(t, T, helm_targets(B, 1.0))
    over = max(0.0, (tg.y - 1.0) / 3.0)
    st["twist"] += -s * 16.0 * min(1.0, over)
    pose_state(r, st)
    look(r, tg, 0.95 * look_k)
    a = 3.0 * s + harm(t, T, (2.4, 2, 0.0), (1.1, 5, 1.0))
    trim = harm(t, T, (0.55, 1, 0.3), (0.25, 3, 2.0))
    aft, fwd = ("r", "l") if s > 0 else ("l", "r")
    ext = ext_role(B, st, s, a, aft)
    sh = sheet_role(B, st, s, trim, fwd)
    out = {"tiller": hold(r, ext, aft), "sheet": hold(r, sh, fwd), "boom": -s * (22.0 + 6.0 * trim)}
    return out


@clip("Sail_Helm", 8.0, mirror=True, frame="boat", boat="luch", scene="boat",
      about="Luch helm sitting on the windward side deck: small tiller corrections, trimming, looking at the luff, ahead, over the shoulder")
def sail_helm(r, t, T):
    return helm_loop(r, BOATS["luch"], t, T, 0.0)


@clip("Sail_Hike_Half", 8.0, mirror=True, frame="boat", boat="luch", scene="boat",
      about="hiking with bent knees, the seat on the gunwale's edge (blend 0.5 of the hike; same loop phase as Sail_Helm)")
def sail_hike_half(r, t, T):
    return helm_loop(r, BOATS["luch"], t, T, 0.5, look_k=0.8)


@clip("Sail_Hike_Full", 8.0, mirror=True, frame="boat", boat="luch", scene="boat",
      about="full hike: legs nearly straight under the strap, hips out over the water, back flat (blend 1)")
def sail_hike_full(r, t, T):
    return helm_loop(r, BOATS["luch"], t, T, 1.0, gust_amp=1.5, look_k=0.7)


@clip("Sail_Cadet_Helm", 8.0, mirror=True, frame="boat", boat="cadet", scene="boat",
      about="cadet helm (a teenager) aft on the side deck, centre mainsheet from the block behind")
def sail_cadet_helm(r, t, T):
    B = BOATS["cadet"]
    st = helm_state(B, 1, 0.3, gust=0.5 + 0.5 * math.sin(TAU * t / T))
    st["lean"] += harm(t, T, (2.0, 2, 0.4))
    tg = scan(t, T, helm_targets(B, 1.0))
    st["twist"] += -16.0 * min(1.0, max(0.0, (tg.y - 1.0) / 3.0))
    pose_state(r, st)
    look(r, tg, 0.9)
    a = 3.0 + harm(t, T, (2.4, 2, 0.0), (1.1, 5, 1.0))
    trim = harm(t, T, (0.55, 1, 0.3), (0.25, 3, 2.0))
    ext = ext_role(B, st, 1, a, "r")
    # centre main: the sheet comes forward from the block on the thwart behind and up to the forward hand
    sh = sheet_role(B, st, 1, trim, "l")
    return {"tiller": hold(r, ext, "r"), "sheet": hold(r, sh, "l"), "boom": -(20.0 + 6.0 * trim)}


# ---------------------------------------------------------------------------------------------- tack and gybe

def crouch_state(B, x, y, yaw, z, lean, feet, twist=0.0, side=0.0, tilt=10.0):
    f, left = yaw_axes(yaw)
    st = {"pelvis": Vector((x, y, z)), "yaw": yaw, "tilt": tilt, "lean": lean, "side": side, "twist": twist}
    for sd, (ax, ay) in feet.items():
        st[sd + "a"] = Vector((ax, ay, 0.085))
        st[sd + "t"] = (f * 0.95 + Vector((0, 0, -0.4))).normalized()
        sign = 1 if sd == "l" else -1
        st[sd + "p"] = (f + left * (sign * 0.35) + Vector((0, 0, 0.3))).normalized()
    st["h"] = 0.0
    return st


def _fix_feet(st, B):
    """helm_state names the feet by fore/aft: give every state both sides' toes and poles."""
    return st


def tack_states(B, quick=False):
    """Keyed body states of a tack facing forward from port to starboard (a gybe: quicker, lower)."""
    k = kb(B)
    q = 0.72 if quick else 1.0
    z_duck = (B["boom_z"] * k - 0.98) + (0.30 if quick else 0.36)        # the head well under the boom
    A = helm_state(B, 1, 0.15)
    Bst = helm_state(B, -1, 0.15)
    keys = [
        (0.0, A),
        (0.38 * q, dict(A, pelvis=A["pelvis"] + Vector((-0.05, 0.0, 0.03)), lean=24.0, twist=6.0)),
        (0.78 * q, crouch_state(B, 0.30 * k, -0.04, -22.0, z_duck + 0.06, 40.0, {"l": (0.22 * k, -0.42), "r": (0.04, -0.06)}, twist=8.0)),
        (1.08 * q, crouch_state(B, 0.0, -0.06, 0.0, z_duck, 44.0, {"l": (0.12 * k, -0.40), "r": (-0.16 * k, -0.10)})),
        (1.42 * q, crouch_state(B, -0.34 * k, -0.04, 30.0, z_duck + 0.12, 26.0, {"l": (0.02, -0.12), "r": (-0.17 * k, -0.40)}, twist=-6.0)),
        (1.72 * q, dict(Bst, lean=Bst["lean"] + 8.0)),
        (2.2 * q, Bst),
    ]
    return keys


def _feet_complete(keys):
    """Every keyed state has both feet's toes and poles (helm_state gives them; crouch_state too)."""
    for _, st in keys:
        for sd in "lr":
            st.setdefault(sd + "t", Vector((0, -1, -0.3)).normalized())
            st.setdefault(sd + "p", Vector((0, -0.3, 1)).normalized())
    return keys


def tack_like(r, t, T, B, quick=False):
    k = kb(B)
    q = 0.72 if quick else 1.0
    keys = _feet_complete(tack_states(B, quick))
    st = state_at(keys, t)
    st["h"] = 0.15
    pose_state(r, st)
    f, left = yaw_axes(st["yaw"])
    # the head: up at the sail, down and forward under the boom, then to the new side's luff and ahead
    my = B["mast_y"] * k
    tg = track([(0.0, Vector((0.0, my + 0.4, 2.0 * k))), (0.5 * q, Vector((-0.6, my, 1.5 * k))), (0.95 * q, Vector((0.0, -2.0, 0.2))),
                (1.4 * q, Vector((1.5, -4.0, 1.0))), (1.9 * q, Vector((0.0, my + 0.4, 2.0 * k))), (T, Vector((1.5, -9.0, 1.3)))], t)
    look(r, tg, 0.9)
    # the tiller: pushed away to leeward (starboard), across the middle, eased back to a little weather helm on the new side
    a = track([(0.0, 3.0), (0.35 * q, -24.0), (0.9 * q, -30.0), (1.4 * q, -12.0), (1.8 * q, -4.0), (T, -3.0)], t)
    # the old tiller hand (right) keeps the extension across behind the back until the left takes it over
    reach_old = None
    if t > 0.7 * q:
        behind = st["pelvis"] + (-f) * 0.22 + Vector((0, 0, 0.22)) + left * -0.12
        reach_old = behind
    ext_r = ext_role(B, st, -1 if t > 1.1 * q else 1, a, "r", reach=reach_old)
    ext_l = ext_role(B, st, -1, a, "l")
    sh_l = sheet_role(B, st, 1, 0.0, "l")
    sh_r = sheet_role(B, st, -1, 0.0, "r")
    out = {"boom": track([(0.0, -22.0), (0.7 * q, -18.0), (1.0 * q, 0.0), (1.3 * q, 20.0), (T, 23.0)], t)}
    # right hand: extension -> (behind the back) -> lets go (t1) -> the sheet (t2)
    t1, t2 = 1.85 * q, 2.15 * q
    if t < t1:
        out["tiller"] = hold(r, ext_r, "r")
    else:
        w = sm((t - t1) / (t2 - t1))
        blend_hold(r, "r", ext_r, sh_r, w)
        if w >= 0.999:
            out["sheet"] = hold(r, sh_r, "r")
    # left hand: the sheet -> (quick: grabs the falls overhead to throw the boom across) -> the extension behind the back
    l0, l1 = (1.45 * q, 1.75 * q)
    if quick:
        falls = {"grip": Vector((-0.05, 0.25, B["boom_z"] * k - 0.12)), "axis": Vector((0, 0, 1)), "knuckles": f,
                 "radius": 0.008, "from": Vector(B["sheet_block"]) * k, "to": Vector((0.0, 0.3, B["boom_z"] * k)), "hand_side": "l"}
        if t < 0.35 * q:
            out["sheet"] = hold(r, sh_l, "l")
        elif t < 0.6 * q:
            blend_hold(r, "l", sh_l, falls, sm((t - 0.35 * q) / (0.25 * q)))
        elif t < 0.95 * q:
            falls["grip"] = falls["grip"] + Vector((-0.4 * sm((t - 0.6 * q) / (0.35 * q)), 0, 0))
            out["falls"] = hold(r, falls, "l")
        elif t < l0:
            falls["grip"] = falls["grip"] + Vector((-0.4, 0, 0))
            blend_hold(r, "l", falls, sh_l, sm((t - 0.95 * q) / (l0 - 0.95 * q)))
        elif t < l1:
            blend_hold(r, "l", sh_l, ext_l, sm((t - l0) / (l1 - l0)))
        else:
            out["tiller_new"] = hold(r, ext_l, "l")
    else:
        if t < l0:
            out["sheet"] = hold(r, sh_l, "l")
        elif t < l1:
            blend_hold(r, "l", sh_l, ext_l, sm((t - l0) / (l1 - l0)))
        else:
            out["tiller_new"] = hold(r, ext_l, "l")
    return out


@clip("Sail_Tack", 2.2, loop=False, mirror=True, frame="boat", boat="luch", scene="boat",
      about="one-shot tack facing forward: tiller away, duck under the boom, cross, sit on the new side, hands swapped behind the back; _Port starts on port and ends as Sail_Helm_Stbd")
def sail_tack(r, t, T):
    return tack_like(r, t, T, BOATS["luch"])


@clip("Sail_Gybe", 1.6, loop=False, mirror=True, frame="boat", boat="luch", scene="boat",
      about="one-shot gybe: quicker, lower duck, the sheet hand grabs the falls and throws the boom across; ends as the other side's Sail_Helm")
def sail_gybe(r, t, T):
    return tack_like(r, t, T, BOATS["luch"], quick=True)


# ---------------------------------------------------------------------------------------------- light airs

@clip("Sail_Lull", 8.0, mirror=True, frame="boat", boat="luch", scene="boat",
      about="light air: kneeling low in the cockpit's middle, leaning to leeward to heel the boat, gentle steering")
def sail_lull(r, t, T):
    B = BOATS["luch"]
    k = kb(B)
    yaw = -30.0 + harm(t, T, (3.0, 1, 0.5))
    f, left = yaw_axes(yaw)
    # kneeling on the left (forward) knee, sitting back on the heel; the right foot flat, knee up; the body leaned to
    # leeward (starboard: -X, the body's right) to heel her
    pel = Vector((0.04, 0.05, 0.37 + harm(t, T, (0.008, 2, 0.0))))
    legs = {"l": (Vector((0.16, 0.22, 0.075)), (Vector((0.05, 0.95, -0.1))).normalized(), Vector((0.1, -1.0, -0.35))),
            "r": (Vector((-0.10, -0.30, 0.085)), (Vector((-0.2, -0.95, 0.0)) + Vector((0, 0, -0.42))).normalized(), (f + Vector((0, 0, 0.8))).normalized())}
    body(r, pel, yaw, tilt=6.0, lean=18.0 + harm(t, T, (2.0, 2, 1.0)), side=-14.0 + harm(t, T, (2.0, 1, 0.0)), twist=-8.0, legs=legs)
    look(r, scan(t, T, [(1.5, (0.0, B["mast_y"] * k + 0.4, 2.2)), (1.0, (-2.0, -9.0, 1.2)), (1.0, (0.3, -1.5, 2.5)), (1.2, (-4.0, -6.0, 0.6))]), 0.9)
    st = {"pelvis": pel, "yaw": yaw, "h": 0.0}
    a = 1.0 + harm(t, T, (1.5, 2, 0.0), (0.6, 3, 1.4))
    ext = ext_role(B, st, 1, a, "r", reach=pel + f * 0.30 + left * -0.18 + Vector((0, 0, 0.18)))
    trim = harm(t, T, (0.4, 1, 0.0))
    sh = sheet_role(B, st, 1, trim, "l")
    sh["grip"] = pel + f * 0.32 + left * 0.08 + Vector((0, 0, 0.15 + 0.05 * trim))
    sh["axis"] = (sh["grip"] - sh["from"]).normalized()
    return {"tiller": hold(r, ext, "r", curl=50.0), "sheet": hold(r, sh, "l", curl=58.0), "boom": -(45.0 + 4.0 * trim)}


# ---------------------------------------------------------------------------------------------- the optimist (a child)

def opti_state(B, s=1, h=0.0):
    st = helm_state(B, s, h)
    k = kb(B)
    st["pelvis"] = Vector((s * B["sit_x"] * k, 0.0, B["deck_z"] * k + 0.08))
    st["lean"] = 8.0
    return st


@clip("Sail_Opti_Helm", 7.0, mirror=True, frame="boat", boat="optimist", scene="boat",
      about="optimist child on the side: short extension in the aft hand, the forward hand on the sheet coming down from the boom")
def sail_opti_helm(r, t, T):
    B = BOATS["optimist"]
    k = kb(B)
    st = opti_state(B, 1)
    st["lean"] += harm(t, T, (3.0, 2, 0.3), (1.5, 3, 1.0))
    st["pelvis"] += Vector((0, 0, harm(t, T, (0.008, 4, 0.0))))
    tg = scan(t, T, [(1.2, (0.0, B["mast_y"] * k + 0.2, 2.0)), (1.0, (-2.0, -8.0, 1.0)), (0.8, (5.0, 3.0, 0.9)), (1.0, (-1.0, 0.4, 1.8))])
    st["twist"] += -14.0 * min(1.0, max(0.0, (tg.y - 1.0) / 3.0))
    pose_state(r, st)
    look(r, tg, 0.9)
    f, left = yaw_axes(st["yaw"])
    a = 4.0 + harm(t, T, (3.0, 2, 0.0), (1.2, 5, 1.0))
    ext = ext_role(B, st, 1, a, "r")
    # the sheet hangs from the boom's block: the hand up in front of the face, pulling down to trim
    trim = harm(t, T, (0.6, 1, 0.3), (0.2, 3, 2.0))
    blk = Vector(B["boom_block"]) * k
    G = st["pelvis"] + f * 0.30 + left * 0.10 + Vector((0, 0, 0.62 - 0.12 * trim))
    sh = {"grip": G, "axis": (blk - G).normalized() * -1.0, "knuckles": f + Vector((0, 0, 0.2)), "radius": 0.005,
          "from": blk, "to": G + Vector((0.02, 0.05, -0.45)), "hand_side": "l"}
    return {"tiller": hold(r, ext, "r", curl=58.0), "sheet": hold(r, sh, "l"), "boom": -(28.0 + 6.0 * trim)}


@clip("Sail_Opti_Bail", 4.0, mirror=True, frame="boat", boat="optimist", scene="boat",
      about="optimist child kneeling on the bottom, bailing with the scoop and throwing the water over the lee side, the extension held low")
def sail_opti_bail(r, t, T):
    B = BOATS["optimist"]
    k = kb(B)
    ph = (t / T) % 1.0
    # one scoop a second: down to the bottom (0..0.45), up and the throw over the side (0.45..0.75), back
    cyc = (t * 2.0 / T) % 1.0
    down = sm(cyc / 0.35) * (1 - sm((cyc - 0.4) / 0.2))
    throw = sm((cyc - 0.45) / 0.15) * (1 - sm((cyc - 0.72) / 0.25))
    yaw = -40.0 + 15.0 * throw
    f, left = yaw_axes(yaw)
    pel = Vector((0.12 * k, 0.05, 0.43 - 0.05 * down))
    legs = {"l": (Vector((0.28, 0.20, 0.075)), Vector((0.05, 0.95, -0.12)).normalized(), Vector((0.1, -1.0, -0.25))),
            "r": (Vector((0.04, 0.22, 0.075)), Vector((-0.05, 0.95, -0.12)).normalized(), Vector((-0.1, -1.0, -0.25)))}
    body(r, pel, yaw, tilt=8.0, lean=25.0 + 35.0 * down - 10.0 * throw, side=-8.0 * throw, twist=-12.0 * throw, legs=legs)
    look(r, track([(0.0, Vector((0.0, -0.5, 0.0))), (0.5, Vector((-0.2, -0.5, 0.0))), (0.6, Vector((-2.0, -1.0, 0.3))),
                   (0.85, Vector((-1.0, -4.0, 1.0))), (1.0, Vector((0.0, -0.5, 0.0)))], cyc), 0.85)
    st = {"pelvis": pel, "yaw": yaw, "h": 0.0}
    a = 2.0 + harm(t, T, (1.5, 1, 0.0))
    ext = ext_role(B, st, 1, a, "r", reach=pel + f * 0.15 + left * -0.25 + Vector((0, 0, 0.05)))
    out = {"tiller": hold(r, ext, "r", curl=55.0), "boom": -30.0}
    # the bailer in the left hand: the scoop's handle along the fingers, mouth forward
    floor = Vector((-0.05, -0.35, 0.08))
    over = Vector((-B["half_beam"] * k - 0.05, -0.2, B["deck_z"] * k + 0.35))
    rest = pel + f * 0.25 + left * 0.15 + Vector((0, 0, 0.05))
    G = rest.lerp(floor, down).lerp(over, throw)
    fd = (f * (1 - throw) + Vector((-1, 0, 0.3)) * throw + Vector((0, 0, -0.6)) * down).normalized()
    palm = Vector((0.0, 0.0, -1.0)).lerp(Vector((0.3, 0.6, -0.6)), throw)
    palm = (palm - fd * palm.dot(fd)).normalized()
    arm(r, "l", G - fd * 0.06, fd, palm)
    r.curl("l", {"index": 55, "middle": 62, "ring": 66, "pinky": 70}, thumb=35)
    out["draw"] = [("box", G + fd * 0.03 + Vector((-0.06, -0.05, -0.03)), G + fd * 0.17 + Vector((0.06, 0.05, 0.04)), "prop")]
    return out


# ---------------------------------------------------------------------------------------------- the cadet's crew

def crew_state(B, s=1, h=0.3):
    st = helm_state(B, s, h)
    k = kb(B)
    st["pelvis"] = st["pelvis"] + Vector((0, 0.0, 0.0))
    return st


@clip("Sail_Crew_Jib", 6.0, mirror=True, frame="boat", boat="cadet", scene="boat",
      about="cadet crew (a teenager) forward on the windward side, hiking with the helm, trimming the jib sheet from the lee fairlead")
def sail_crew_jib(r, t, T):
    B = BOATS["cadet"]
    k = kb(B)
    h = 0.4 + 0.25 * (0.5 + 0.5 * math.sin(TAU * t / T))
    st = helm_state(B, 1, h)
    st["lean"] += harm(t, T, (2.0, 2, 0.7))
    tg = scan(t, T, [(1.3, (-0.6, -1.4, 1.6)), (1.0, (-3.0, -9.0, 1.2)), (0.7, (5.0, -3.0, 1.0)), (1.0, (-0.8, -0.6, 1.2))])
    pose_state(r, st)
    look(r, tg, 0.9)
    f, left = yaw_axes(st["yaw"])
    trim = harm(t, T, (0.6, 1, 0.0), (0.3, 2, 1.0))
    lead = Vector(B["crew_jib_lead"]) * k
    G = st["pelvis"] + f * 0.42 + left * 0.05 + Vector((0, 0, 0.25 + 0.12 * h)) + (-f * 0.12 + Vector((0, 0, 0.06))) * trim
    sh = {"grip": G, "axis": (G - lead).normalized(), "knuckles": f + Vector((0, 0, 0.3)), "radius": 0.005, "from": lead,
          "to": G + Vector((0.05, 0.1, -0.3)), "color": "rope2", "hand_side": "l"}
    out = {"jib": hold(r, sh, "l")}
    # the aft hand holds the sheet's tail a little behind (or steadies on the strap) - here: on the sheet's tail
    tail = {"grip": G + (G - lead).normalized() * 0.22 + Vector((0, 0.05, -0.04)), "axis": (G - lead).normalized(),
            "knuckles": f - left * 0.3, "radius": 0.005, "hand_side": "r"}
    out["jib_tail"] = hold(r, tail, "r", curl=58.0)
    out["boom"] = -(18.0 + 3.0 * trim)
    return out


@clip("Sail_Crew_Tack", 2.4, loop=False, mirror=True, frame="boat", boat="cadet", scene="boat",
      about="cadet crew tacking: lets the old jib sheet fly, crosses under the boom, sits on the new side and takes the new sheet")
def sail_crew_tack(r, t, T):
    B = BOATS["cadet"]
    k = kb(B)
    keys = _feet_complete(tack_states(B))
    st = state_at(keys, t * 2.2 / T)
    pose_state(r, st)
    f, left = yaw_axes(st["yaw"])
    look(r, track([(0.0, Vector((-0.6, -1.4, 1.6))), (0.8, Vector((0.0, -1.5, 0.4))), (1.5, Vector((0.6, -1.4, 1.6))),
                   (T, Vector((3.0, -9.0, 1.2)))], t), 0.9)
    old = Vector(B["crew_jib_lead"]) * k
    new = Vector((-old.x, old.y, old.z))
    sA = {"grip": st["pelvis"] + f * 0.42 + Vector((0, 0, 0.27)), "axis": f, "knuckles": f + Vector((0, 0, 0.3)), "radius": 0.005,
          "from": old, "color": "rope2", "hand_side": "l"}
    sA["axis"] = (sA["grip"] - old).normalized()
    sB = {"grip": st["pelvis"] + f * 0.42 + Vector((0, 0, 0.27)), "axis": f, "knuckles": f + Vector((0, 0, 0.3)), "radius": 0.005,
          "from": new, "color": "rope2", "hand_side": "r"}
    sB["axis"] = (sB["grip"] - new).normalized()
    out = {"boom": track([(0.0, -18.0), (0.9, -12.0), (1.15, 0.0), (1.4, 18.0), (T, 18.0)], t)}
    # left lets the old sheet go at 0.45 s (the hand rides along to the middle, then onto the gunwale/thigh); right
    # takes the new sheet from 1.6 s
    if t < 0.45:
        out["jib"] = hold(r, sA, "l")
    else:
        relax(r, "l", 1.2, t)
        w = st["pelvis"] + f * 0.12 + left * 0.25 + Vector((0, 0, 0.12))
        arm(r, "l", w, (f + Vector((0, 0, -0.6))).normalized(), -left)
    if t > 1.6:
        out["jib"] = hold(r, sB, "r") if t > 1.95 else blend_hold(r, "r", dict(sB, grip=st["pelvis"] - left * 0.2 + Vector((0, 0, 0.2))), sB, sm((t - 1.6) / 0.35))
    else:
        relax(r, "r", 1.2, t)
        w = st["pelvis"] + f * 0.15 - left * 0.22 + Vector((0, 0, 0.14))
        arm(r, "r", w, (f + Vector((0, 0, -0.6))).normalized(), left)
    return out


# ---------------------------------------------------------------------------------------------- capsize set

@clip("Sail_Capsize_Fall", 1.5, loop=False, mirror=True, frame="boat", boat="luch", scene="boat",
      about="one-shot: the boat goes over, the helm slides off the high side into the water; ends treading water at local (1.25, -0.1, -0.95) (water at -0.05)")
def sail_capsize_fall(r, t, T):
    B = BOATS["luch"]
    u = t / T
    A = helm_state(B, 1, 0.3)
    end_p = Vector((1.25, -0.1, -0.95))
    p0 = A["pelvis"]
    # slide back out over the gunwale, the legs coming free of the strap, a half roll onto the back, then upright in the water
    pel = track([(0.0, p0), (0.25, p0 + Vector((0.12, 0.0, 0.02))), (0.5, Vector((0.95, -0.05, 0.05))), (0.75, Vector((1.2, -0.08, -0.6))),
                 (1.0, end_p)], u)
    yaw = track([(0.0, A["yaw"]), (0.5, -75.0), (1.0, -90.0)], u)
    lean = track([(0.0, A["lean"]), (0.3, -30.0), (0.55, -60.0), (0.8, -20.0), (1.0, 5.0)], u)
    f, left = yaw_axes(yaw)
    fa, aa = A["la"], A["ra"]
    fl = track([(0.0, fa), (0.35, fa + Vector((0.1, 0.0, 0.1))), (0.6, Vector((0.6, -0.4, 0.35))), (1.0, end_p + Vector((-0.15, -0.15, -0.75)))], u)
    fr = track([(0.0, aa), (0.35, aa + Vector((0.12, 0.0, 0.12))), (0.6, Vector((0.55, -0.1, 0.4))), (1.0, end_p + Vector((-0.05, 0.15, -0.7)))], u)
    toes = (f * 0.95 + Vector((0, 0, -0.42))).normalized()
    pole = (f + Vector((0, 0, 0.3))).normalized()
    body(r, pel, yaw, tilt=-10.0 * (1 - u), lean=lean, side=6.0 * math.sin(math.pi * u), twist=0.0,
         legs={"l": (fl, toes, pole), "r": (fr, toes, pole)})
    look(r, track([(0.0, Vector((0.0, -1.4, 2.0))), (0.35, Vector((-0.5, -1.0, 3.0))), (0.7, Vector((1.0, -3.0, 2.0))),
                   (1.0, Vector((0.0, -0.2, 0.2)))], u), 0.8)
    # arms fly up and out, then scull at the surface
    for s, sign in (("l", 1), ("r", -1)):
        up = math.sin(math.pi * min(1.0, u * 1.4))
        w = pel + left * (sign * (0.35 + 0.15 * up)) + f * (0.2 - 0.1 * up) + Vector((0, 0, 0.35 + 0.45 * up))
        if u > 0.8:
            w = w.lerp(pel + left * (sign * 0.38) + f * 0.25 + Vector((0, 0, 0.62)), sm((u - 0.8) / 0.2))
        arm(r, s, w, (left * sign + Vector((0, 0, 0.4))).normalized(), Vector((0, 0, -1)))
        relax(r, s, 0.6, t)
    water = -0.05
    return {"draw": [("box", Vector((0.7, -1.2, water - 0.02)), Vector((1.9, 1.0, water)), "water")]}


def tread(r, t, pel, yaw, depth=1.0):
    """Treading water: upright, the legs cycling slowly, the hands sculling at the surface."""
    f, left = yaw_axes(yaw)
    legs = {}
    for s, sign, ph in (("l", 1, 0.0), ("r", -1, math.pi)):
        a = pel + left * (sign * 0.14) + f * (0.12 * math.sin(t * 3.0 + ph)) + Vector((0, 0, -0.78 + 0.08 * math.cos(t * 3.0 + ph)))
        legs[s] = (a, (f * 0.3 + Vector((0, 0, -1.0))).normalized(), (f + Vector((0, 0, 0.2))).normalized())
    body(r, pel, yaw, tilt=5.0, lean=8.0, legs=legs)
    for s, sign in (("l", 1), ("r", -1)):
        sw = math.sin(t * 3.4 + (0 if s == "l" else 0.4))
        w = pel + left * (sign * (0.42 + 0.08 * sw)) + f * (0.25 + 0.05 * sw) + Vector((0, 0, 0.58))
        arm(r, s, w, (left * sign * 0.6 + f * 0.6).normalized(), Vector((-sign * 0.2 * sw, 0, -1)).normalized())
        relax(r, s, 0.4, t)


@clip("Sail_Swim_ToBoard", 3.0, loop=False, mirror=True, frame="capsized", scene="capsized",
      about="one-shot from treading water at local (0.75, 0, -0.95): reach the centreboard (x 0.25..0.8 at z 0.25), haul up onto it and stand, hands to the up gunwale; ends as Sail_Right")
def sail_swim_toboard(r, t, T):
    u = t / T
    yaw = 90.0          # facing -X: the hull
    f, left = yaw_axes(yaw)
    board_z = 0.25
    gun = Vector((-0.25, 0.0, 1.15))
    pel = track([(0.0, Vector((0.80, 0.0, -0.95))), (0.25, Vector((0.62, 0.0, -0.80))), (0.5, Vector((0.55, 0.0, -0.25))),
                 (0.7, Vector((0.50, 0.0, 0.45))), (0.85, Vector((0.48, 0.0, 0.85))), (1.0, Vector((0.55, 0.0, 1.05)))], u)
    lean = track([(0.0, 8.0), (0.3, 20.0), (0.55, 55.0), (0.75, 45.0), (1.0, -12.0)], u)
    # feet: kicking, then the right knee onto the board, then both feet on it near the hull
    fl = track([(0.0, Vector((0.85, 0.14, -1.75))), (0.4, Vector((0.75, 0.15, -1.2))), (0.65, Vector((0.62, 0.15, -0.3))),
                (0.8, Vector((0.38, 0.15, board_z + 0.08))), (1.0, Vector((0.32, 0.16, board_z + 0.085)))], u)
    fr = track([(0.0, Vector((0.80, -0.14, -1.7))), (0.4, Vector((0.72, -0.14, -1.0))), (0.6, Vector((0.52, -0.14, board_z + 0.02))),
                (0.8, Vector((0.42, -0.15, board_z + 0.085))), (1.0, Vector((0.30, -0.16, board_z + 0.085)))], u)
    toes = (f * 0.95 + Vector((0, 0, -0.42))).normalized()
    body(r, pel, yaw, tilt=5.0, lean=lean, legs={"l": (fl, toes, (f + Vector((0, 0, 0.4))).normalized()),
                                                 "r": (fr, toes, (f + Vector((0, 0, 0.4))).normalized())})
    look(r, track([(0.0, Vector((0.3, 0.0, 0.3))), (0.5, Vector((0.3, 0.0, 0.2))), (0.8, gun), (1.0, gun + Vector((0, 0, 0.3)))], u), 0.85)
    out = {"draw": [("box", Vector((-0.2, -1.5, -0.05)), Vector((1.3, 1.5, -0.03)), "water"),
                    ("box", Vector((0.0, -0.18, board_z - 0.02)), Vector((0.85, 0.18, board_z)), "boat"),
                    ("box", Vector((-0.35, -2.0, -0.4)), Vector((0.0, 2.0, 1.2)), "deck")]}
    for s, sign in (("l", 1), ("r", -1)):
        on_board = Vector((0.65, sign * 0.12, board_z + 0.02))
        on_gun = gun + Vector((0.0, sign * 0.28, 0.0))
        w = track([(0.0, pel + left * sign * 0.4 + Vector((0, 0, 0.55))), (0.3, on_board + Vector((0.05, 0, 0.06))),
                   (0.55, on_board + Vector((-0.1, 0, 0.06))), (0.75, Vector((0.3, sign * 0.2, 0.95))), (1.0, on_gun + Vector((0.05, 0, -0.08)))], u)
        palm = Vector((0, 0, -1)) if u < 0.65 else Vector((-0.6, 0, -0.8)).normalized()
        arm(r, s, w, (f + Vector((0, 0, 0.15))).normalized(), palm)
        r.curl(s, 40.0 + 20.0 * sm((u - 0.85) / 0.15), thumb=20.0)
    return out


@clip("Sail_Right", 3.0, mirror=True, frame="capsized", scene="capsized",
      about="righting loop: standing on the centreboard by the hull, hands on the up gunwale, leaning back in pulses until the boat comes up")
def sail_right(r, t, T):
    yaw = 90.0
    f, left = yaw_axes(yaw)
    board_z = 0.25
    gun = Vector((-0.25, 0.0, 1.15))
    pull = 0.5 + 0.5 * math.sin(TAU * t / T * 2 - 1.2)
    pel = Vector((0.62 + 0.12 * pull, 0.0, 0.88 - 0.1 * pull))
    toes = (f * 0.95 + Vector((0, 0, -0.42))).normalized()
    legs = {"l": (Vector((0.30, 0.16, board_z + 0.085)), toes, (f + Vector((0, 0, 0.5))).normalized()),
            "r": (Vector((0.28, -0.16, board_z + 0.085)), toes, (f + Vector((0, 0, 0.5))).normalized())}
    body(r, pel, yaw, tilt=-8.0, lean=-28.0 - 14.0 * pull, legs=legs)
    look(r, gun + Vector((0.0, 0.0, 0.4 + 0.3 * pull)), 0.7)
    out = {"draw": [("box", Vector((-0.2, -1.5, -0.05)), Vector((1.3, 1.5, -0.03)), "water"),
                    ("box", Vector((0.0, -0.18, board_z - 0.02)), Vector((0.85, 0.18, board_z)), "boat"),
                    ("box", Vector((-0.35, -2.0, -0.4)), Vector((0.0, 2.0, 1.2)), "deck")]}
    for s, sign in (("l", 1), ("r", -1)):
        at = gun + Vector((0.02, sign * 0.28, 0.0))
        g = grip(r, s, at, Vector((0, 1, 0)) * (1 if s == "l" else -1), Vector((-0.2, 0, -1.0)), radius=0.02, curl=70.0)
        g["hand_side"] = s
        out["gunwale_" + s] = g
    return out


@clip("Sail_Climb_In", 3.2, loop=False, mirror=True, frame="boat", boat="luch", scene="boat",
      about="one-shot from the water by the port side (local (1.0, 0, -0.95)): hands on the gunwale, kick and haul, chest over, a leg in, roll into the cockpit and kneel up")
def sail_climb_in(r, t, T):
    B = BOATS["luch"]
    u = t / T
    hb, dz = B["half_beam"], B["deck_z"]
    yaw = track([(0.0, 90.0), (0.55, 95.0), (0.8, 130.0), (1.0, 160.0)], u)
    f, left = yaw_axes(yaw)
    pel = track([(0.0, Vector((1.0, 0.0, -0.95))), (0.25, Vector((0.92, 0.0, -0.55))), (0.45, Vector((0.80, 0.0, 0.05))),
                 (0.62, Vector((0.62, 0.0, 0.42))), (0.8, Vector((0.35, 0.05, 0.40))), (1.0, Vector((0.08, 0.05, 0.30)))], u)
    lean = track([(0.0, 10.0), (0.3, 20.0), (0.5, 60.0), (0.65, 75.0), (0.8, 40.0), (1.0, 12.0)], u)
    toes = (f * 0.95 + Vector((0, 0, -0.42))).normalized()
    fl = track([(0.0, Vector((1.05, 0.15, -1.75))), (0.35, Vector((1.05, 0.15, -1.3))), (0.55, Vector((1.0, 0.18, -0.6))),
                (0.7, Vector((hb - 0.05, 0.25, dz + 0.15))), (0.85, Vector((0.2, 0.25, 0.085))), (1.0, Vector((-0.05, 0.35, 0.07)))], u)
    fr = track([(0.0, Vector((1.0, -0.15, -1.7))), (0.4, Vector((1.0, -0.15, -1.1))), (0.6, Vector((0.98, -0.18, -0.5))),
                (0.8, Vector((0.75, -0.2, 0.0))), (0.9, Vector((0.35, -0.25, 0.2))), (1.0, Vector((0.25, -0.3, 0.07)))], u)
    body(r, pel, yaw, tilt=5.0, lean=lean, side=track([(0.0, 0.0), (0.7, -15.0), (1.0, 0.0)], u), legs={
        "l": (fl, toes, (f + Vector((0, 0, 0.6))).normalized()), "r": (fr, toes, (f + Vector((0, 0, 0.6))).normalized())})
    look(r, track([(0.0, Vector((hb, 0.0, dz + 0.3))), (0.5, Vector((0.0, 0.0, 0.2))), (0.8, Vector((-0.5, -0.5, 0.2))),
                   (1.0, Vector((-1.0, -3.0, 1.2)))], u), 0.8)
    out = {"draw": [("box", Vector((hb, -1.5, -0.07)), Vector((2.0, 1.5, -0.05)), "water")]}
    for s, sign in (("l", 1), ("r", -1)):
        on_gun = Vector((hb - 0.02, sign * 0.25, dz + 0.02))
        on_far = Vector((-0.2, sign * 0.3, 0.05))
        w = track([(0.0, on_gun + Vector((0.08, 0, 0.03))), (0.45, on_gun + Vector((0.0, 0, 0.03))), (0.62, on_gun + Vector((-0.1, 0, 0.03))),
                   (0.8, on_far + Vector((0.2, 0, 0.1))), (1.0, Vector((0.05, sign * 0.25, 0.75)) if s == "r" else Vector((0.05, 0.2, 0.55)))], u)
        palm = Vector((0, 0, -1))
        arm(r, s, w, (f + Vector((0, 0, -0.3))).normalized(), palm)
        r.curl(s, 45.0 * (1 - sm((u - 0.85) / 0.15)) + 15.0, thumb=20.0)
    return out
