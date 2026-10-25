"""Standing loops for the club owner: get up from the PC, walk to the booth window, watch the
player, walk back, sit down. All in place (no root motion); the level moves/rotates the actor.

  StandUp    2.4 s  from Sit_Idle (seated at the PC) to standing, ends 0.12 m in front of the
                          chair point (toward the desk) - move the actor there when it ends
  SitDown    2.4 s  exact reverse: starts standing 0.12 m in front of the chair, ends in Sit_Idle
  Walk       1.4 s  loop, in place, 0.75 m/s forward (+Y of the UE mesh); translate the actor at
                          that speed and turn it (~180 deg/s) while this plays
  StandLook  8 s    loop: standing at the window looking straight ahead at a visitor, small
                          nods and weight shifts, hands loosely clasped in front
"""
import math

import bpy
from mathutils import Vector

import seat_anims as S

FPS = S.FPS
LOOPS = {"StandUp": 2.4, "SitDown": 2.4, "Walk": 1.4, "StandLook": 8.0}
STAND_Y = -0.12
SIT_STYLE = dict(hip_back=0.0, feet_fwd=0.42, feet_out=0.13, lean=-0.04, tilt=-0.14)   # = Sit_Idle


def _smooth(x):
    x = min(1.0, max(0.0, x))
    return x * x * (3 - 2 * x)


def _standing(r, y=0.0, bob=0.0, sway=0.0):
    k = r.k
    pel = S._w(r.arm, "pelvis")
    pelvis = Vector((sway, y + 0.0, pel.z - 0.012 * k + bob))
    knees = {s: Vector((sx * 0.12, y - 1.5, 0.5 * k)) for s, sx in (("l", 1), ("r", -1))}
    elbows = {s: Vector((sx * 0.45, y + 0.4, pel.z + 0.25 * k)) for s, sx in (("l", 1), ("r", -1))}
    return pelvis, knees, elbows


def _arms_down(r, y, swing=0.0, clasp=0.0):
    """Wrists hanging by the thighs (swing: + = right hand forward), or clasped in front."""
    k = r.k
    pel = S._w(r.arm, "pelvis")
    wr, hands = {}, {}
    for s, sx in (("l", 1), ("r", -1)):
        side = Vector((sx * 0.21 * k, y + 0.01 + (swing if s == "l" else -swing) * 0.18 * k, pel.z - 0.1 * k))
        front = Vector((sx * 0.05, y - 0.13 * k, pel.z - 0.04 * k))
        wr[s] = side.lerp(front, clasp)
        hands[s] = (Vector((0, -0.1, -1)).lerp(Vector((-sx * 1, -0.3, -0.6)), clasp),
                    Vector((-sx, 0, 0)).lerp(Vector((0, 0.3, 1)), clasp))
    return wr, hands


def stand_up(r, t, T, reverse=False):
    u = t / T
    if reverse:
        u = 1 - u
    k = r.k
    sit_pel, sit_feet = r.seated_layout(SIT_STYLE["hip_back"], SIT_STYLE["feet_fwd"], SIT_STYLE["feet_out"])
    st_pel, knees_st, elbows_st = _standing(r, STAND_Y)
    a = _smooth(u / 0.35)                         # feet slide back under the body first
    b = _smooth((u - 0.2) / 0.6)                  # then the hips go forward and up
    feet = {s: sit_feet[s].lerp(Vector((sx * 0.11 * k, STAND_Y - 0.02, 0.075 * k)), a)
            for s, sx in (("l", 1), ("r", -1))}
    pelvis = sit_pel.lerp(st_pel, b)
    pelvis.y += -0.08 * math.sin(math.pi * b)     # the body travels over the feet
    lean = SIT_STYLE["lean"] * (1 - b) + 0.7 * math.sin(math.pi * min(1, b * 1.15))
    tilt = SIT_STYLE["tilt"] * (1 - b) + 0.35 * math.sin(math.pi * b)
    # hands: from the desk (Sit_Idle) pushing off, then down by the sides
    desk = {"l": Vector((0.17, -0.42, S.DESK + 0.04)), "r": Vector((-0.2, -0.44, S.DESK + 0.04))}
    down, hands_down = _arms_down(r, STAND_Y)
    c = _smooth((u - 0.45) / 0.45)
    wr = {s: desk[s].lerp(down[s], c) for s in "lr"}
    hands = {s: ((Vector((-0.4 if s == "l" else 0.4, -1, -0.1))).lerp(hands_down[s][0], c),
                 S.DOWN.lerp(hands_down[s][1], c)) for s in "lr"}
    knees = {s: Vector((sx * 0.13 * k * 1.1, -1.5, r.hip_z)).lerp(knees_st[s], b) for s, sx in (("l", 1), ("r", -1))}
    look = S.SCREEN.lerp(Vector((0, STAND_Y - 1.5, 1.5 * k)), b)
    r.place(r.f, wrists=wr, hands=hands, lean=lean - r.style.get("lean", 0), tilt=tilt - r.style.get("tilt", 0),
            look=look - Vector(r.style.get("look", (0, 0, 0))), curls={(s, f): 0.4 for s in "lr" for f in S.FINGERS},
            pelvis=pelvis, feet_at=feet, knees=knees, elbows=elbows_st)


def sit_down(r, t, T):
    stand_up(r, t, T, reverse=True)


def walk(r, t, T):
    """Two steps per loop; feet slide back on the ground at walking speed during stance."""
    k = r.k
    L = 0.75 * T / 2                              # step length so that 2 steps = 0.75 m/s * T
    feet = {}
    for s, sx, ph in (("l", 1, 0.0), ("r", -1, 0.5)):
        u = (t / T + ph) % 1.0
        if u < 0.6:                               # stance: from front (-L/2) to back (+L/2)
            v = u / 0.6
            y, lift = -L / 2 + L * v, 0.0
        else:                                     # swing: back to front, foot lifted
            v = (u - 0.6) / 0.4
            y, lift = L / 2 - L * _smooth(v), 0.06 * k * math.sin(math.pi * v)
        feet[s] = Vector((sx * 0.1 * k, y, 0.075 * k + lift))
    bob = -0.015 * k * math.cos(4 * math.pi * t / T)
    sway = 0.012 * math.sin(2 * math.pi * t / T)
    pelvis, knees, elbows = _standing(r, 0.0, bob, sway)
    pelvis.z -= 0.02 * k
    swing = math.sin(2 * math.pi * t / T)
    wr, hands = _arms_down(r, 0.0, swing=swing * 0.9)
    look = Vector((0, -3.0, 1.5 * k))
    r.place(r.f, wrists=wr, hands=hands, lean=0.12 - r.style.get("lean", 0), tilt=0.04 - r.style.get("tilt", 0),
            look=look - Vector(r.style.get("look", (0, 0, 0))), twist=-0.08 * swing,
            curls={(s, f): 0.35 for s in "lr" for f in S.FINGERS},
            pelvis=pelvis, feet_at=feet, knees=knees, elbows=elbows, breath=0.0)


def stand_look(r, t, T):
    k = r.k
    shift = 0.015 * math.sin(2 * math.pi * t / T)
    pelvis, knees, elbows = _standing(r, 0.0, 0.004 * math.sin(4 * math.pi * t / T), shift)
    feet = {s: Vector((sx * 0.12 * k, 0.0, 0.075 * k)) for s, sx in (("l", 1), ("r", -1))}
    wr, hands = _arms_down(r, 0.0, clasp=1.0)
    nod = 0.03 * max(0.0, math.sin(2 * math.pi * (t / T * 2 + 0.2))) ** 4
    look = Vector((0.06 * math.sin(2 * math.pi * t / T * 1), -1.2, 1.5 * k - nod))
    r.place(r.f, wrists=wr, hands=hands, lean=0.05 - r.style.get("lean", 0), tilt=0.0 - r.style.get("tilt", 0),
            look=look - Vector(r.style.get("look", (0, 0, 0))), curls={(s, f): 0.55 for s in "lr" for f in S.FINGERS},
            pelvis=pelvis, feet_at=feet, knees=knees, elbows=elbows, breath=0.02 * math.sin(2 * math.pi * t / T * 2))


MAKERS = {"StandUp": stand_up, "SitDown": sit_down, "Walk": walk, "StandLook": stand_look}
S.MAKERS.update(MAKERS)
S.LOOPS.update(LOOPS)
