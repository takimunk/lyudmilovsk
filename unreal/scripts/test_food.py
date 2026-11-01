"""Offscreen check of the street food (docs/food.md): the cheese tent by the club's door and its seller's trade screen, a buy
handed over across the counter, the eating in the hand (a click, a hold, holding on) for every food, the corn cart in the
Academgorodok. FOODTEST lines in the log (and the game's HANDUSE / FOOD / Stalls / Trade lines), screenshots in order.

  sh unreal/scripts/test_food.sh
"""
import unreal

state = {"t": 0.0, "step": 0, "shots": 0}
TENT = "BugItGo -600 1940 170 -8 180 0"            # 1.8 m in front of the tent's counter, looking at it
TENT_SIDE = "BugItGo -330 1700 190 -10 160 0"
CORN = "BugItGo -25735 61405 -3440 -10 172 0"       # in front of the corn cart

PLAN = [(8.0, "lyud.People.Order khuazheva_susanna work_cheese", None),
        (8.2, "lyud.People.Order guseva_valentina work_corn", None),
        (8.5, TENT_SIDE, None),
        (14.0, "shot", "tent_side"),
        (14.5, TENT, None),
        (20.0, "lyud.People.Report khuazheva_susanna", None),
        (20.5, "shot", "tent_front"),
        (21.0, "Inventory.Earn 300", None),
        (21.5, "lyud.Trade.Open khuazheva_susanna", None),
        (23.0, "shot showui", "tent_shop"),
        (23.5, "Shop.Open 0", None),
        (24.0, "Shop.Open 1", None),
        (24.5, "Shop.Open close", None),
        (25.6, "shot", "handover_reach"),
        (26.6, "shot", "handover_carry")]
t = 30.0
for food in ("cheese_braid", "cheese_round_small", "cheese_round_big", "corn_cob"):
    PLAN += [(t, "lyud.Food.Hold " + food, None),
             (t + 2.0, "shot", food + "_carry"),
             (t + 2.2, "lyud.Hand.Click", None),
             (t + 2.5, "shot", food + "_click_lips"),
             (t + 3.5, "lyud.Hand.Press 1", None),
             (t + 3.9, "shot", food + "_hold_lips"),
             (t + 6.5, "shot", food + "_steady"),
             (t + 7.5, "lyud.Hand.Press 0", None),
             (t + 8.5, "shot", food + "_after"),
             (t + 9.0, "lyud.Hand.PutAway", None)]
    t += 10.0
PLAN += [(t, CORN, None),
         (t + 8.0, "lyud.People.Report guseva_valentina", None),
         (t + 8.5, "shot", "corn_cart"),
         (t + 9.0, "lyud.Trade.Open guseva_valentina", None),
         (t + 10.5, "shot showui", "corn_shop"),
         (t + 11.0, "Shop.Open 0", None),
         (t + 11.5, "Shop.Open close", None),
         (t + 13.0, "shot", "corn_handover"),
         (t + 16.0, "lyud.Food.Hold corn_cob", None),
         (t + 18.0, "lyud.Hand.Press 1", None),
         (t + 30.0, "shot", "corn_eaten_long"),
         (t + 40.0, "lyud.Hand.Press 0", None),
         (t + 41.0, "lyud.Hand.PutAway", None)]


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += min(dt, 0.25)
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, name = PLAN[state["step"]]
        state["step"] += 1
        if name:
            unreal.log_warning("FOODTEST shot %02d %s" % (state["shots"], name))
            state["shots"] += 1
        else:
            unreal.log_warning("FOODTEST %s" % cmd)
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 3.0:
        unreal.log_warning("FOODTEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
