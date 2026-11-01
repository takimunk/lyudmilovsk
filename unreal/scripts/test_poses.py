"""Offscreen check of people's poses (docs/characters.md "Clips"): the club's computer workers from the side (elbows),
the Gastronom's seller at her counter full length (legs), a few standing and seated people. For each shot the joints'
angles are measured on the skinned bones: POSETEST lines give each elbow's and knee's bend (180 = straight), how far each
hand sits from where the clip put it, and each foot's height over the floor under the person. Screenshots in
Saved/Screenshots (HighResShot, in order of the "shot" lines).

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed -nosound
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"
  unreal/scripts/test_poses.sh runs it.
"""
import math
import os

import unreal

PC = os.environ.get("POSE_PC", "kid_boy,guy_a,girl_b,streamer,crowd_11,clerk").split(",")
# (key, activity) ordered before the shots: the seller at the Gastronom, people standing about, sitting on a bench
ORDERS = [tuple(o.split(":")) for o in os.environ.get(
    "POSE_ORDERS", "orlova_tanya:work_shop,korneeva_nina:shop,khuazheva_susanna:work_cheese,guseva_valentina:work_corn").split(",") if o]
st = {"t": 0.0, "steps": [], "i": 0, "t0": 0.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def life(w):
    return unreal.PeopleLifeSubsystem.get_people_life(w)


def person(w, key):
    l = life(w)
    r = l.get_person(l.find_person(key))
    if isinstance(r, tuple):
        return r[1] if r[0] else None
    return r


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(m):
    unreal.log_warning("POSETEST " + m)


def look_at(w, key, dist, side, up, aim_z):
    p = person(w, key)
    if not p:
        log("no %s" % key)
        return
    a = math.radians(p.yaw)
    fx, fy = math.cos(a), math.sin(a)
    eye = unreal.Vector(p.location.x + fx * dist - fy * dist * side, p.location.y + fy * dist + fx * dist * side, p.location.z + up)
    tgt = unreal.Vector(p.location.x, p.location.y, p.location.z + aim_z)
    r = unreal.MathLibrary.find_look_at_rotation(eye, tgt)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pc.get_controlled_pawn().set_actor_location(eye, False, True)
    pc.set_control_rotation(r)
    st["eye"] = (eye, r)


def fix_eye(w):
    """The camera sits above the pawn's origin: move the pawn so the camera is where it was meant to be."""
    if "eye" not in st:
        return
    eye, r = st["eye"]
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    cam = pc.player_camera_manager.get_camera_location()
    pawn.set_actor_location(pawn.get_actor_location() + (eye - cam), False, True)
    pc.set_control_rotation(r)


def body_mesh(w, key):
    p = person(w, key)
    if not p or not p.actor:
        return None
    for c in p.actor.get_components_by_class(unreal.SkeletalMeshComponent):
        if c.does_socket_exist("hand_l"):
            return c
    return None


def angle(a, b, c):
    u, v = a - b, c - b
    n = u.length() * v.length()
    return math.degrees(math.acos(max(-1.0, min(1.0, u.dot(v) / n)))) if n > 1e-6 else 0.0


def measure(w, key):
    m = body_mesh(w, key)
    p = person(w, key)
    if not m:
        log("%s: no body (actor %s)" % (key, p.actor.get_name() if p and p.actor else None))
        return
    s = m.get_socket_location
    el = [angle(s("upperarm_" + x), s("lowerarm_" + x), s("hand_" + x)) for x in "lr"]
    kn = [angle(s("thigh_" + x), s("calf_" + x), s("foot_" + x)) for x in "lr"]
    reach = [(s("hand_" + x) - s("upperarm_" + x)).length() for x in "lr"]
    arm = [(s("lowerarm_" + x) - s("upperarm_" + x)).length() + (s("hand_" + x) - s("lowerarm_" + x)).length() for x in "lr"]
    base = m.get_owner().get_actor_location().z
    feet = [s("foot_" + x).z - base for x in "lr"]
    log("%s pose %s: elbows %.0f/%.0f deg (reach %.0f/%.0f of %.0f/%.0f cm), knees %.0f/%.0f deg, ankles %.1f/%.1f cm over the origin, pelvis %.0f"
        % (key, p.pose, el[0], el[1], reach[0], reach[1], arm[0], arm[1],
           kn[0], kn[1], feet[0], feet[1], s("pelvis").z - base))


def shot(w, name, key):
    measure(w, key)
    cmd(w, "HighResShot 1280x720")
    log("shot %s" % name)


def step(fn, wait):
    st["steps"].append((fn, wait))


step(lambda w: (cmd(w, "ghost"), cmd(w, "r.MotionBlurQuality 0"), cmd(w, "lyud.Face.Log 1")), 2.0)
for k, a in ORDERS:
    step(lambda w, k=k, a=a: log("order %s %s: %s" % (k, a, life(w).order(life(w).find_person(k), a, ""))), 0.5)
# the computer workers: from the side at elbow height, then a little from behind over the shoulder
for k in PC:
    for n, (side, d) in enumerate([(4.0, -40.0), (1.1, -110.0)]):
        step(lambda w, k=k, side=side, d=d: look_at(w, k, d, side, 100.0, 75.0), 0.3)
        step(fix_eye, 0.3)
        step(lambda w, k=k, n=n: shot(w, "%s_pc_%d" % (k, n), k), 3.0)
# the sellers have had time to walk to their counters: full length from the front, the side and behind
step(lambda w: None, float(os.environ.get("POSE_WALK", "60")))
for k, a in ORDERS:
    step(lambda w, k=k: log("%s: %s" % (k, life(w).describe(life(w).find_person(k)))), 0.2)
    # the camera there first: a body is made only near it, once its model has loaded in the background
    step(lambda w, k=k: look_at(w, k, 230.0, 0.0, 120.0, 85.0), 0.3)
    step(fix_eye, float(os.environ.get("POSE_SHOW", "12")))
    for n, (side, d) in enumerate([(0.0, 230.0), (3.0, 70.0), (0.35, -200.0)]):
        step(lambda w, k=k, side=side, d=d: look_at(w, k, d, side, 120.0, 85.0), 0.3)
        step(fix_eye, 0.3)
        step(lambda w, k=k, n=n: shot(w, "%s_stand_%d" % (k, n), k), 3.0)
step(lambda w: (log("done"), cmd(w, "quit")), 1.0)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < float(os.environ.get("POSE_WARMUP", "70")):
        return
    if st["i"] >= len(st["steps"]):
        return
    fn, wait = st["steps"][st["i"]]
    if st["t"] - st["t0"] >= wait or st["i"] == 0:
        try:
            fn(w)
        except Exception as e:
            log("step %d failed: %s" % (st["i"], e))
        st["i"] += 1
        st["t0"] = st["t"]


unreal.register_slate_post_tick_callback(tick)
