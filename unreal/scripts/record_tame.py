"""Offscreen recordings of befriending a bird (UBirdTameSubsystem): runs a scripted scenario in a -game session without a
window and takes a screenshot (with the UI) every FRAME_DT seconds of game time; tools/tame_frames.py turns them into a
contact sheet and an animation. Scenarios:

  vend      walk to the vending machine, buy feed (slot 5), the hand takes the bag, Tab: take it in the hand, crouch, palm out
  success   feed in the hand, crouch, creep up slowly on the nearest bird with the palm out, wait: it comes and eats
  crouch    seen from the side: crouching, creeping, the palm out (for the body's pose)
  scared    the same bird, but walking upright, fast, with sharp turns of the view: it flies off

  UnrealEditor <uproject> /Game/Dev/L_TameTest -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0,lyud.Birds.AnyMap=1 -abslog=<log> "-ExecCmds=py unreal/scripts/record_tame.py <scenario> [warmup s] [species]"

Prints TAME lines to the log (positions, the hero's crouch, the birds' events).
"""
import sys

import unreal

SCENARIO = sys.argv[1] if len(sys.argv) > 1 else "vend"
WARMUP = float(sys.argv[2]) if len(sys.argv) > 2 else 40.0
FRAME_DT = 0.2

state = {"t": 0.0, "i": 0, "next_shot": 0.0, "shots": True, "done": False, "dual": False, "n": 0}


def find_world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_TameTest" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.log_warning("TAME cmd %s" % c)
    unreal.SystemLibrary.execute_console_command(w, c)


BIRD = (500.0, -380.0, 0.0)  # where the test bird is put (lyud.Birds.SpawnNear): on the lawn, off the line to the vending machine
SPECIES = sys.argv[3] if len(sys.argv) > 3 else "Robin"


def bird_target(w):
    return BIRD


# (seconds after the warm-up, action) - an action is a console command or a python callable taking the world
VEND = [
    (0.0, "Tame.Walk 400 0 110 140 0 90 145"),
    (3.5, "Vending.Buy 5"),
    (9.0, "Inventory.Open"),
    (11.0, "Inventory.Equip bird_feed"),
    (11.2, "Inventory.Open 0"),
    (13.0, "Hero.SideCam 160 60 10"),
    (15.0, "Hero.SideCam off"),
    (15.2, "Tame.Walk -500 200 0 100 1 45 0"),
    (21.0, "Tame.Offer 1"),
    (24.0, "Hero.SideCam 150 80 0 follow"),
    (27.0, "Hero.SideCam off"),
    (29.0, "Tame.Walk off"),
    (31.0, None),
]


def look_at(target, extra_yaw=0.0):
    """Turns the view at once toward a world point (a jerk of the mouse when it is far from where the hero looks)."""
    def go(w):
        pc = unreal.GameplayStatics.get_player_controller(w, 0)
        pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
        if not pc or not pawn:
            return
        eye = pawn.get_actor_location() + unreal.Vector(0, 0, 70)
        d = unreal.Vector(*target) - eye
        rot = unreal.MathLibrary.make_rot_from_x(d)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=max(rot.pitch, -30.0), yaw=rot.yaw + extra_yaw))
        unreal.log_warning("TAME look yaw %.0f pitch %.0f" % (rot.yaw + extra_yaw, rot.pitch))
    return go


def dual(on, cam=None, follow=False):
    """From now on every other screenshot is taken through a camera beside the hero (placed once, here, by
    Hero.SideCam right forward up), so the recording shows the eyes' view and the scene from outside, alternately."""
    def go(w):
        if cam:
            cmd(w, "Hero.SideCam %d %d %d follow" % cam)
            cmd(w, "Hero.SideCam off")
        state["dual"] = on
        state["dual_cmd"] = ("Hero.SideCam %d %d %d follow" % cam) if follow else "Hero.SideCam"
    return go


def approach(speed, crouch, turn, stop=110):
    def go(w):
        x, y, z = bird_target(w)
        cmd(w, "Tame.Walk %.0f %.0f %.0f %.0f %d %.0f %.0f" % (x, y, z, speed, crouch, turn, stop))
    return go


CROUCH = [
    (0.0, "Tame.Give 5"),
    (0.2, "Hero.SideCam 150 20 0"),
    (0.3, "Hero.SideCam 150 20 0"),
    (1.0, "Hero.DumpBones"),
    (2.0, "Tame.Walk 0 600 0 0 1 90 1000"),
    (4.0, "Hero.DumpBones"),
    (4.1, "Hero.SideCam -150 20 0 follow"),
    (6.0, "Tame.Walk -600 0 0 100 1 90 0"),
    (11.0, "Tame.Offer 1"),
    (11.5, "lyud.HeroDebugBones 1"),
    (12.0, "Hero.SideCam 0 110 -10 follow"),
    (12.3, "Hero.SideCam 110 40 0 follow"),
    (12.5, "Hero.DumpBones"),
    (12.6, "Hero.SideCam off"),
    (13.0, "Tame.Walk -600 300 0 50 1 30 0"),
    (18.0, "Tame.Walk -600 300 0 50 0 30 0"),
    (21.0, None),
]

SUCCESS = [
    (0.0, "Tame.Give 5"),
    (0.5, "lyud.Birds.SpawnNear %s %.0f %.0f %.0f" % ((SPECIES,) + BIRD)),
    (0.6, look_at(BIRD)),
    (3.0, approach(55, 1, 25, 230)),
    (4.0, "Tame.Offer 1"),
    (12.0, dual(True, (170, 150, 10))),
    (130.0, None),
]

SCARED = [
    (0.0, "Tame.Give 5"),
    (0.5, "lyud.Birds.SpawnNear %s %.0f %.0f %.0f" % ((SPECIES,) + BIRD)),
    (0.6, look_at(BIRD)),
    (3.0, "Tame.Offer 1"),
    (4.0, dual(True, (-260, 160, 90), True)),
    # upright, the view jerked about, then a run at the bird
    (5.0, look_at(BIRD, 70.0)),
    (5.15, look_at(BIRD, -60.0)),
    (5.3, look_at(BIRD)),
    (6.0, approach(600, 0, 300, 60)),
    (12.0, "Tame.Walk off"),
    (12.1, look_at((BIRD[0], BIRD[1] - 800.0, 300.0))),
    (16.0, None),
]

PLAN = {"vend": VEND, "crouch": CROUCH, "success": SUCCESS, "scared": SCARED}[SCENARIO]


def tick(dt):
    if state["done"]:
        return
    w = find_world()
    if not w:
        return
    state["t"] += dt
    t = state["t"] - WARMUP
    if t < 0:
        return
    while state["i"] < len(PLAN) and PLAN[state["i"]][0] <= t:
        action = PLAN[state["i"]][1]
        state["i"] += 1
        if action is None:
            unreal.log_warning("TAME done %s" % SCENARIO)
            state["done"] = True
            cmd(w, "QUIT")
            return
        if callable(action):
            action(w)
        else:
            cmd(w, action)
    if t >= state["next_shot"]:
        state["next_shot"] = t + FRAME_DT
        if state["dual"]:
            state["n"] += 1
            cmd(w, state["dual_cmd"] if state["n"] % 2 else "Hero.SideCam off")
        cmd(w, "shot showui")
        pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
        if pawn:
            p = pawn.get_actor_location()
            try:
                crouched = pawn.get_editor_property("is_crouched")
            except Exception:
                crouched = "?"
            pc = unreal.GameplayStatics.get_player_controller(w, 0)
            r = pc.get_control_rotation() if pc else unreal.Rotator()
            unreal.log_warning("TAME t=%.2f hero %.0f %.0f %.0f crouched=%s pitch %.0f yaw %.0f" % (t, p.x, p.y, p.z, crouched, r.pitch, r.yaw))


unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("TAME start %s" % SCENARIO)
