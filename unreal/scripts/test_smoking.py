"""Offscreen check of the cigarettes (docs/smoking.md) in Lvl_Scene01: the started pack on the terrace taken with its lighter, a
cigarette taken out (the pack in the hand, the lid, one shaken up to the lips, the hand taking it), lit (left in the lips, the lighter
to the tip), a click, a drag, a drag held into a cough, put out and thrown away; one taken out and put back unlit; the pack's
words in en and ja. SMOKETEST lines in the log (and the game's SMOKE / HANDUSE lines), screenshots in order.

  sh unreal/scripts/test_smoking.sh          (FRAMES=1: also a frame per tick through the drag and the cough, for a video)
"""
import os

import unreal

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Lyudmilovsk", "Saved", "SmokeTest")

state = {"t": 0.0, "step": 0, "shots": 0, "frames": False, "frame": 0}
FRAMES = os.environ.get("SMOKETEST_FRAMES", "0") == "1"
# on the terrace out of the club's forecourt, looking down at the pack lying at (-250, 1650)
TERRACE = "BugItGo -250 1790 172 -52 -90 0"
STAND = "BugItGo -250 1900 172 -12 90 0"

PLAN = [(8.5, "lyud.Birds.Time 13", None),
        (9.0, STAND, None),
        # a fresh pack of ten: its words in ru, en, ja as it comes up and opens
        (10.0, "lyud.Smoke.Give", None),
        (11.0, "lyud.Smoke.Take", None),
        (12.15, "shot", "pack_ru_ten"),
        (12.45, "shot", "pack_ru_raised"),
        (16.0, "lyud.Smoke.Out", None),
        (17.0, "lyud.Language en", None),
        (18.0, "lyud.Smoke.Take", None),
        (19.15, "shot", "pack_en"),
        (23.0, "lyud.Smoke.Out", None),
        (24.0, "lyud.Language ja", None),
        (25.0, "lyud.Smoke.Take", None),
        (26.15, "shot", "pack_ja"),
        (30.0, "lyud.Smoke.Out", None),
        (31.0, "lyud.Language ru", None),
        (31.5, "lyud.Smoke.State", None),
        # the started pack on the terrace (7 left), taken with its lighter
        (32.0, TERRACE, None),
        (35.0, "shot", "terrace_pack"),
        (35.5, "lyud.Smoke.Pick", None),
        (36.4, "shot", "terrace_reach"),
        (39.0, "lyud.Smoke.State", None),
        (39.5, STAND, None),
        # take one out: pack up, lid, one shaken up, to the lips, the hand takes it
        (40.0, "lyud.Smoke.Take", None),
        (41.15, "shot", "pack_up_open"),
        (41.6, "shot", "pack_raised"),
        (42.2, "shot", "pack_to_lips"),
        (42.8, "shot", "pack_down_cig_in_lips"),
        (43.9, "shot", "hand_takes_from_lips"),
        (45.0, "shot", "held_unlit"),
        (45.2, "lyud.Smoke.State", None),
        # light it: to the lips, left there, the lighter up, the flame
        (46.0, "lyud.Hand.Click", None),
        (46.75, "shot", "unlit_at_lips"),
        (47.5, "shot", "left_in_lips"),
        (48.3, "shot", "lighter_up"),
        (48.85, "shot", "flame_at_tip"),
        (49.4, "shot", "lit_in_lips"),
        (51.2, "shot", "taken_lit"),
        (52.0, "lyud.Smoke.State", None),
        (53.5, "shot", "held_lit_smoke"),
        # a click: only to the lips
        (54.0, "lyud.Hand.Click", None),
        (54.45, "shot", "click_at_lips"),
        (55.5, "lyud.Smoke.State", None),
        # a drag: hold, the ember bright, let go, the breath out; then held too long: a cough
        (56.0, "lyud.Hand.Press 1", "frames_on"),
        (57.3, "shot", "drag_glow"),
        (58.0, "lyud.Hand.Press 0", None),
        (58.75, "shot", "exhale"),
        (59.5, "lyud.Smoke.State", None),
        (61.0, "lyud.Hand.Press 1", None),
        (64.6, "shot", "cough"),
        (65.2, "shot", "cough_smoke"),
        (66.5, "lyud.Hand.Press 0", "frames_off"),
        (67.0, "lyud.Smoke.State", None),
        (68.0, "BugItGo -250 1900 172 -35 90 0", None),
        (68.5, "lyud.Smoke.Out", None),
        (68.8, "shot", "butt_flying"),
        (71.0, "lyud.Smoke.State", None),
        (71.5, "shot", "butt_on_ground"),
        # one out and back, unlit
        (72.0, STAND, None),
        (73.0, "lyud.Smoke.Take", None),
        (78.0, "lyud.Smoke.State", None),
        (78.2, "lyud.Smoke.Out", None),
        (80.0, "lyud.Smoke.State", None)]


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def tick(dt):
    w = world()
    if not w:
        return
    # game time (the run uses a fixed step, test_smoking.sh: the plan lands on the same frames however slowly it renders)
    state["t"] = unreal.GameplayStatics.get_time_seconds(w)
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, name = PLAN[state["step"]]
        state["step"] += 1
        if name == "frames_on":
            state["frames"] = FRAMES
            name = None
        elif name == "frames_off":
            state["frames"] = False
            name = None
        if name:
            # named shots: frames recorded meanwhile can't shift them
            unreal.log_warning("SMOKETEST shot %02d %s" % (state["shots"], name))
            cmd = "lyud.Smoke.Shot %s/%02d_%s.png" % (os.path.abspath(OUT), state["shots"], name)
            state["shots"] += 1
        else:
            unreal.log_warning("SMOKETEST %s" % cmd)
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["frames"]:
        unreal.log_warning("SMOKETEST frame %05d" % state["frame"])
        unreal.SystemLibrary.execute_console_command(w, "lyud.Smoke.Shot %s/frames/%05d.png" % (os.path.abspath(OUT), state["frame"]))
        state["frame"] += 1
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("SMOKETEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
