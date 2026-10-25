"""Offscreen recordings of befriending a bird (UBirdTameSubsystem): runs a scripted scenario in a -game session without a
window and takes a screenshot (with the UI) every FRAME_DT seconds of game time; tools/tame_frames.py turns them into a
contact sheet and an animation. Scenarios:

  vend      walk to the vending machine, buy feed (slot 5), the hand takes the bag, Tab: take it in the hand, crouch, palm out
  success   feed in the hand, crouch, creep up slowly on the nearest bird with the palm out, wait: it comes and eats
  crouch    seen from the side: crouching, creeping, the palm out (for the body's pose)
  scared    the same bird, but walking upright, fast, with sharp turns of the view: it flies off

  UnrealEditor <uproject> /Game/Dev/L_TameTest -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/record_tame.py <scenario>"

Prints TAME lines to the log (positions, the hero's crouch, the birds' events).
"""
import sys

import unreal

SCENARIO = sys.argv[1] if len(sys.argv) > 1 else "vend"
WARMUP = float(sys.argv[2]) if len(sys.argv) > 2 else 40.0
FRAME_DT = 0.2

state = {"t": 0.0, "i": 0, "next_shot": 0.0, "shots": True, "done": False}


def find_world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_TameTest" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.log_warning("TAME cmd %s" % c)
    unreal.SystemLibrary.execute_console_command(w, c)


def bird_target(w):
    """The bird to walk up to: TAME_BIRD x y z from the bird-life debug command, else a point 6 m ahead."""
    return state.get("bird", (600.0, 0.0, 0.0))


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


def approach(speed, crouch, turn):
    def go(w):
        x, y, z = bird_target(w)
        cmd(w, "Tame.Walk %.0f %.0f %.0f %.0f %d %.0f 110" % (x, y, z, speed, crouch, turn))
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
    (13.0, "Tame.Walk -600 300 0 50 1 30 0"),
    (18.0, "Tame.Walk -600 300 0 50 0 30 0"),
    (21.0, None),
]

SUCCESS = [
    (0.0, "Tame.Give 5"),
    (0.5, "Birds.TameTestBird"),
    (2.0, approach(70, 1, 35)),
    (4.0, "Tame.Offer 1"),
    (40.0, "Hero.SideCam 220 100 30"),
    (44.0, "Hero.SideCam off"),
    (60.0, None),
]

SCARED = [
    (0.0, "Tame.Give 5"),
    (0.5, "Birds.TameTestBird"),
    (2.0, approach(600, 0, 400)),
    (2.2, "Tame.Offer 1"),
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
        cmd(w, "shot showui")
        pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
        if pawn:
            p = pawn.get_actor_location()
            try:
                crouched = pawn.get_editor_property("is_crouched")
            except Exception:
                crouched = "?"
            unreal.log_warning("TAME t=%.2f hero %.0f %.0f %.0f crouched=%s" % (t, p.x, p.y, p.z, crouched))


unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("TAME start %s" % SCENARIO)
