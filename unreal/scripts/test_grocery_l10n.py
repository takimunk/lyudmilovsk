"""Offscreen check of the grocery's feeds in English and Japanese (docs/bird_taming.md "Feeds and the feeder"): for each
language the six packets' fronts (lyud.Goods.Show: T_Label_<id> of that language) and the diary's inventory with the feeds
(T_Sketch_/T_Icon_<id>), one in the hand. Runs in /Game/Dev/L_TameTest (make_tame_test_level.py); GROCERYL10N lines,
screenshots in order (Saved/Screenshots).

  UnrealEditor <uproject> /Game/Dev/L_TameTest -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_grocery_l10n.py"
"""
import unreal

FEEDS = ["feed_suet", "feed_niger", "feed_rowan", "feed_mealworm", "feed_grain", "feed_peanuts"]
state = {"t": 0.0, "step": 0, "shots": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_TameTest" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def shot(name):
    def go(w):
        unreal.log_warning("GROCERYL10N shot %02d %s" % (state["shots"], name))
        state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, "shot showui")
    return go


def level(w):
    unreal.GameplayStatics.get_player_controller(w, 0).set_control_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))


PLAN = [(3.0, "lyud.Language en")] + [(4.0, "Inventory.Give %s 5" % f) for f in FEEDS] + [(4.2, "Inventory.Equip feed_suet")]
t = 5.0
for lang in ("en", "ja"):
    PLAN += [(t, "lyud.Language " + lang), (t + 0.2, level)]
    t += 1.0
    for f in FEEDS:
        PLAN += [(t, "lyud.Goods.Show %s 30" % f), (t + 1.5, shot("%s_packet_%s" % (lang, f)))]
        t += 2.0
    PLAN += [(t, "lyud.Goods.Show off"), (t + 0.5, "Inventory.Open"), (t + 2.5, shot("%s_inventory" % lang)), (t + 3.0, "Inventory.Open 0")]
    t += 4.0
PLAN.append((t, None))


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += min(dt, 0.25)
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        a = PLAN[state["step"]][1]
        state["step"] += 1
        if a is None:
            unreal.log_warning("GROCERYL10N done")
            unreal.SystemLibrary.execute_console_command(w, "quit")
            return
        if callable(a):
            a(w)
        else:
            unreal.log_warning("GROCERYL10N " + a)
            unreal.SystemLibrary.execute_console_command(w, a)


unreal.register_slate_post_tick_callback(tick)
