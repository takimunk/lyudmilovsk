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
WHEN = unreal.DateTime(2026, 10, 21, 15, 0, 0, 0)

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


def all_bodies(w):
    return [a for a in unreal.ObjectIterator(unreal.NpcBody) if a.get_world() == w and not a.get_name().startswith("Default__")]


def bodies(w):
    """Drawn townsfolk bodies: (actor, facing unit vector (mesh +Y), pose name of the nearest person record)."""
    out = []
    recs = []
    l = life(w)
    for i in range(l.num_people()):
        r = l.get_person(i)
        if isinstance(r, tuple):
            r = r[1] if r[0] else None
        if r and r.pose != unreal.PersonPose.HIDDEN:
            recs.append(r)
    for a in all_bodies(w):
        root = a.root_component
        if not root or not root.is_visible():
            continue
        loc = a.get_actor_location()
        f = root.get_right_vector()
        pose, key, d0 = "STAND", "?", 1e9
        for r in recs:
            d = math.hypot(r.location.x - loc.x, r.location.y - loc.y)
            if d < d0:
                d0, pose, key = d, str(r.pose).split(".")[-1].upper(), r.key
        if d0 > 400.0:
            pose, key = "STAND", a.get_name()
        out.append((a, f, pose, key))
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


def cam_loc(w):
    return unreal.GameplayStatics.get_player_controller(w, 0).player_camera_manager.get_camera_location()


def pick(w, manner, near=None):
    """The drawn body nearest to the camera (or `near`) in a fitting pose; those already shot count as farther."""
    want = WANT.get(manner, ("STAND", "TALK", "LOOK", "WALK", "SIT"))
    c = near or cam_loc(w)
    best = None
    for b in bodies(w):
        a, f, pose, key = b
        loc = a.get_actor_location()
        if abs(loc.z - c.z) > 1500.0:
            continue
        d = math.hypot(loc.x - c.x, loc.y - c.y)
        if d > 6000.0:
            continue
        rank = d + st["used"].get(a.get_name(), 0) * 900.0 + (0.0 if pose in want else 4000.0)
        if best is None or rank < best[0]:
            best = (rank, b)
    return best[1] if best else None


def always_tick(w):
    # offscreen only the HighResShot frames render, and a body's pose ticks only when rendered: without this every
    # shot shows the pose of the shot before
    for a in all_bodies(w):
        try:
            a.root_component.set_editor_property("visibility_based_anim_tick_option",
                                                 unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        except Exception as e:  # noqa: BLE001
            if not st.get("tickerr"):
                st["tickerr"] = 1
                log("tick option: %s" % e)


def visible(b):
    try:
        return b[0].root_component.is_visible()
    except Exception:  # noqa: BLE001
        return False


def frame(w, b, back=350.0, side=0.55, up=150.0):
    a, _, _, _ = b
    t = a.get_actor_location()
    f = a.root_component.get_right_vector()
    fx, fy = f.x, f.y
    n = math.hypot(fx, fy) or 1.0
    fx, fy = fx / n, fy / n
    ex = t.x + fx * back - fy * back * side
    ey = t.y + fy * back + fx * back * side
    place(w, (ex, ey, t.z + up), (t.x, t.y, t.z + 110.0))


STEPS = []


def step(fn, wait=1.0):
    STEPS.append((fn, wait))


def s_start(w):
    for c in ["lyud.Prefetch 0", "ghost", "r.MotionBlurQuality 0", "t.MaxFPS 0", "au.LinearGainScalarForFinalOutut 0",
              "lyud.Weather.Preset clear"] + [c for c in os.environ.get("MANNER_CMDS", "").split(";") if c]:
        cmd(w, c)
    for cal in unreal.ObjectIterator(unreal.GameCalendar):
        if cal.get_world() == w:
            cal.set_now(WHEN)
    c = CENTRES[0]
    place(w, (c[0] - 600.0, c[1], c[2] + 300.0), (c[0], c[1], c[2] + 100.0))
    log("start people=%d" % life(w).num_people())


step(s_start, 20.0)


def s_census(w):
    l = life(w)
    alls = all_bodies(w)
    vis = [a for a in alls if a.root_component and a.root_component.is_visible()]
    log("all NpcBody objects anywhere: %d" % len([a for a in unreal.ObjectIterator(unreal.NpcBody)]))
    dup = 0
    try:
        log("instances: %s" % (l.instance_report(),))
    except Exception as e:  # noqa: BLE001
        log("instance_report: %s" % e)
    log("report: %s" % l.report()[:600])
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    cam = pc.player_camera_manager.get_camera_location()
    log("bodies %d, visible %d; camera %s" % (len(alls), len(vis), cam))
    recs = []
    for i in range(l.num_people()):
        r = l.get_person(i)
        if isinstance(r, tuple):
            r = r[1] if r[0] else None
        if r and r.pose != unreal.PersonPose.HIDDEN:
            recs.append((math.hypot(r.location.x - cam.x, r.location.y - cam.y), r))
    recs.sort(key=lambda x: x[0])
    log("nearest people: " + ", ".join("%s %s %s %.0fm z%.0f" % (r.key, str(r.pose).split(".")[-1], r.place, d / 100.0,
        r.location.z) for d, r in recs[:15]))
    out = [x for x in recs if x[1].place.startswith("S-")]
    for d, r in out[:3]:
        log("near %s: model %s town %s actor %s | %s" % (r.key, l.shown_model(r.id), l.is_townsperson(r.id), r.actor,
                                                       l.describe(r.id)[:300]))
    if not vis and out:
        # move to where people are out of doors
        t = out[0][1].location
        st["centre"] = (t.x, t.y, t.z)
        place(w, (t.x - 500.0, t.y, t.z + 250.0), (t.x, t.y, t.z + 100.0))
        log("moved the camera to %s by %s" % (t, out[0][1].key))


step(s_census, 6.0)
step(lambda w: log("after 6 s: %d bodies" % len(all_bodies(w))), 10.0)
step(lambda w: log("after 16 s: %d bodies" % len(all_bodies(w))), 1.0)


def s_census2(w):
    cmd(w, "HighResShot 1600x900 filename=manner_census")
    if st.get("centre"):
        CENTRES.insert(0, st["centre"])
    bs = bodies(w)
    log("drawn %d: %s" % (len(bs), ", ".join("%s(%s %.0fm)" % (k, p, min(math.hypot(a.get_actor_location().x - c[0],
        a.get_actor_location().y - c[1]) for c in CENTRES) / 100.0) for a, _, p, k in bs[:60])))


step(s_census2, 1.0)

for m in MANNERS:
    def _force(w, m=m):
        cmd(w, "lyud.People.Manner %s" % m)
        always_tick(w)
        b = pick(w, m)
        st["who"] = b
        if b:
            st["used"][b[0].get_name()] = st["used"].get(b[0].get_name(), 0) + 1
            frame(w, b)
    step(_force, 2.5)

    def _check(w, m=m):
        b = st.get("who")
        if b and not visible(b):
            b = pick(w, m)
            st["who"] = b
            log("%s: subject gone, now %s" % (m, b[0].get_name() if b else None))
        if b:
            always_tick(w)
            frame(w, b)
    step(_check, 1.5)

    def _shoot(w, m=m):
        if st.get("who") is None:
            log("%s: nobody" % m)
            return
        b = st["who"]
        if not visible(b):
            b = pick(w, m)
            if not b:
                log("%s: lost the subject, nobody else" % m)
                return
            st["who"] = b
        frame(w, b)
        log("%s: %s (%s) pose %s at %s" % (m, b[3], b[0].get_name(), b[2], b[0].get_actor_location()))
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
