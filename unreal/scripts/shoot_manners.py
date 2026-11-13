"""Offscreen close-ups of the townsfolk's manners (docs/town_manners.md); no window, no PIE:

  unreal/scripts/shoot_manners.sh  (runs this in Lvl_Scene01 -game -RenderOffscreen)

For each manner: `lyud.People.Manner <m>` (everybody drawn does it), wait for the gesture to ease in, pick a drawn
person near the promenade / Sun Square in a fitting pose (seated for the newspaper, walking for the bag), and shoot
from ~3.5 m, front three-quarter. Then a wide shot with `lyud.People.Manner auto`.
Prints MANNERSHOT lines; screenshots go to Saved/Screenshots/Mac (the .sh renames them into Saved/MannerShots).
MANNERS=a,b,c limits the list.
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import city_plan as C  # noqa: E402
import world_map as W  # noqa: E402

MANNERS = ["fold_arms", "hands_hips", "hands_back", "check_watch", "yawn", "look_sky", "shade_eyes", "rub_hands",
           "wave", "phone_call", "phone_scroll", "smoke", "photo", "umbrella", "newspaper", "ice_cream", "drink",
           "carry_bag", "feed_birds", "knit", "binoculars", "fish", "paint"]
if os.environ.get("MANNERS"):
    MANNERS = os.environ["MANNERS"].split(",")
WANT = {"newspaper": ("SIT",), "knit": ("SIT", "STAND"), "carry_bag": ("WALK",)}
T0, T1 = -39.5, -36.2
WHEN = unreal.DateTime(2026, 9, 12, 16, 30, 0, 0)

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0, "used": {}}


def p(s, y, z):
    x, yy = C.to_map(s, y)
    return W.to_unreal(x, yy, z, 790.0)


CENTRES = [p(110, 880, T1), p(40, 900, T0), p(150, 905, T1), p(200, 1120, T1)]


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def life(w):
    return unreal.PeopleLifeSubsystem.get_people_life(w)


def log(msg):
    unreal.log_warning("MANNERSHOT " + msg)


def people(w):
    l = life(w)
    out = []
    for i in range(l.num_people()):
        r = l.get_person(i)
        if isinstance(r, tuple):
            r = r[1] if r[0] else None
        if r and r.pose != unreal.PersonPose.HIDDEN and r.actor and not r.actor.is_hidden():
            out.append(r)
    return out


def place(w, eye, target):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if hasattr(pawn, "character_movement"):
        pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    pawn.set_actor_enable_collision(False)
    off = 0.0
    cam = pawn.get_component_by_class(unreal.CameraComponent)
    if cam:
        off = cam.get_world_location().z - pawn.get_actor_location().z
    pawn.set_actor_location(unreal.Vector(eye[0], eye[1], eye[2] - off), False, True)
    dx, dy, dz = target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                           yaw=math.degrees(math.atan2(dy, dx))))


def pick(w, manner):
    want = WANT.get(manner, ("STAND", "TALK", "LOOK", "WALK", "SIT"))
    best = None
    for r in people(w):
        pose = str(r.pose).split(".")[-1].upper()
        if pose not in want:
            continue
        loc = r.location
        d = min(math.hypot(loc.x - c[0], loc.y - c[1]) for c in CENTRES)
        rank = want.index(pose) * 3000.0 + d + st["used"].get(r.id, 0) * 1500.0
        if best is None or rank < best[0]:
            best = (rank, r)
    return best[1] if best else None


def frame(w, r, back=350.0, side=0.55, up=150.0):
    a = r.actor
    t = a.get_actor_location() if a else r.location
    yaw = math.radians(r.yaw)
    fx, fy = math.cos(yaw), math.sin(yaw)
    ex = t.x + fx * back - fy * back * side
    ey = t.y + fy * back + fx * back * side
    ground = r.location.z
    place(w, (ex, ey, ground + up), (t.x, t.y, ground + 115.0))


STEPS = []


def step(fn, wait=1.0):
    STEPS.append((fn, wait))


def s_start(w):
    for c in ["ghost", "r.MotionBlurQuality 0", "t.MaxFPS 0", "au.LinearGainScalarForFinalOutut 0",
              "lyud.Weather.Preset clear"]:
        cmd(w, c)
    for cal in unreal.ObjectIterator(unreal.GameCalendar):
        if cal.get_world() == w:
            cal.set_now(WHEN)
    c = CENTRES[0]
    place(w, (c[0] - 1500.0, c[1], c[2] + 900.0), (c[0], c[1], c[2]))
    log("start people=%d drawn=%d" % (life(w).num_people(), len(people(w))))


step(s_start, 20.0)


def s_census(w):
    ps = people(w)
    log("drawn %d: %s" % (len(ps), ", ".join("%s(%s)" % (r.key, str(r.pose).split(".")[-1]) for r in ps[:40])))


step(s_census, 1.0)

for m in MANNERS:
    def _force(w, m=m):
        cmd(w, "lyud.People.Manner %s" % m)
        r = pick(w, m)
        st["who"] = r.id if r else None
        if r:
            st["used"][r.id] = st["used"].get(r.id, 0) + 1
            frame(w, r)
    step(_force, 2.5)

    def _shoot(w, m=m):
        if st.get("who") is None:
            log("%s: nobody" % m)
            return
        r = life(w).get_person(st["who"])
        if isinstance(r, tuple):
            r = r[1]
        frame(w, r)
        man = life(w).get_manner(r.id)
        log("%s: %s pose %s activity %s manner now %s" % (m, r.key, r.pose, r.activity, man))
        cmd(w, "HighResShot 1600x900 filename=manner_%s" % m)
    step(_shoot, 2.0)


def s_auto(w):
    cmd(w, "lyud.People.Manner auto")
    c = CENTRES[0]
    place(w, (c[0] - 1200.0, c[1] - 600.0, c[2] + 450.0), (c[0], c[1] + 300.0, c[2] + 100.0))


step(s_auto, 6.0)
step(lambda w: cmd(w, "HighResShot 1600x900 filename=manner_auto_wide"), 3.0)


def s_end(w):
    log("done")
    cmd(w, "quit")


step(s_end, 1.0)


def tick(dt):
    try:
        _tick(dt)
    except Exception as e:  # noqa: BLE001
        if not st.get("err"):
            st["err"] = 1
            log("tick failed: %s" % e)
            cmd(world(), "quit")


def _tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 8.0:
        return
    if st["t"] - st["t0"] < st["wait"] or st["i"] >= len(STEPS):
        return
    fn, wait = STEPS[st["i"]]
    st["i"] += 1
    try:
        fn(w)
    except Exception as e:  # noqa: BLE001
        log("step %d failed: %s" % (st["i"], e))
    st["t0"] = st["t"]
    st["wait"] = wait


unreal.register_slate_post_tick_callback(tick)
