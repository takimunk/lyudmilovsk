"""Offscreen close-ups of the club's people at work and of someone leaving (docs/characters.md "Clips"):
for each station person a few frames from in front and to the side (their computer loops changing), then the boy
sent to the vending machine and afterwards out of the club (`town`), followed frame by frame.
CLUBSHOT lines name the screenshots (Saved/Screenshots, in order).

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed -nosound
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"
"""
import math
import os

import unreal

KEYS = os.environ.get("CLUB_KEYS", "kid_boy,guy_a,girl_b,streamer,kid_girl,crowd_11,owner,clerk").split(",")
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
    unreal.log_warning("CLUBSHOT " + m)


def look_at(w, key, dist, side, up, aim_z=85.0):
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
    pawn = pc.get_controlled_pawn()
    pawn.set_actor_location(eye, False, True)
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


def shot(w, name):
    p = person(w, name.split("_")[0] if not name.startswith(("kid_", "crowd_", "girl_", "guy_")) else "_".join(name.split("_")[:2]))
    if p:
        log("%s at (%.0f, %.0f, %.0f) yaw %.0f pose %s" % (name, p.location.x, p.location.y, p.location.z, p.yaw, p.pose))
    cmd(w, "HighResShot 1280x720")
    log("shot %s" % name)


def step(fn, wait):
    st["steps"].append((fn, wait))


step(lambda w: (cmd(w, "ghost"), cmd(w, "r.MotionBlurQuality 0"), cmd(w, "lyud.Face.Log 1")), 2.0)
for k in KEYS:
    for n in range(int(os.environ.get("CLUB_FRAMES", "3"))):
        step(lambda w, k=k: look_at(w, k, -110.0, 1.1, 95.0, 75.0), 0.3)
        step(fix_eye, 0.3)
        step(lambda w, k=k, n=n: shot(w, "%s_%d" % (k, n)), 4.0)
FLOW = os.environ.get("CLUB_FLOW", "1") != "0"
if FLOW:
    step(lambda w: log("order kid_boy snack_club: %s" % life(w).order(life(w).find_person("kid_boy"), "snack_club", "")), 0.5)
    for n in range(10):
        step(lambda w: look_at(w, "kid_boy", 260.0, 0.8, 90.0, 90.0), 0.2)
        step(fix_eye, 0.15)
        step(lambda w, n=n: shot(w, "kid_boy_snack_%02d" % n), 0.6)
    step(lambda w: log("order kid_boy town: %s" % life(w).order(life(w).find_person("kid_boy"), "town", "")), 0.5)
    for n in range(14):
        step(lambda w: look_at(w, "kid_boy", 300.0, -0.9, 120.0, 90.0), 0.2)
        step(fix_eye, 0.15)
        step(lambda w, n=n: shot(w, "kid_boy_leaving_%02d" % n), 0.6)
step(lambda w: (log("done"), cmd(w, "quit")), 1.0)


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < float(os.environ.get("CLUB_WARMUP", "70")):
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
