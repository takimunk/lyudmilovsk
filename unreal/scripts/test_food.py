"""Offscreen check of the street food (docs/food.md): the cheese tent by the club's door and its seller's trade screen, a buy
handed over across the counter, the eating in the hand (a click, a hold, holding on) for every food, the corn cart in the
Academgorodok. FOODTEST lines in the log (and the game's HANDUSE / FOOD / Stalls / Trade lines), screenshots in order.

  sh unreal/scripts/test_food.sh
"""
import os

import unreal

state = {"t": 0.0, "step": 0, "shots": 0}
TENT = "BugItGo -480 1350 100 -8 180 0"            # 1.8 m in front of the tent's counter, looking at it (capsule centre ~1 m up:
                                                   # higher, the head is in the canopy and the hero lands on the roof)
TENT_SIDE = "BugItGo -230 1080 100 -8 150 0"
SIGN = "BugItGo -425 1365 100 -32 180 0"           # over the sign leaning on the counter's front
HANDS = "BugItGo -330 1940 100 -12 0 0"            # out on the forecourt facing the path: the hand against the view
CORN = "BugItGo -25735 61405 -3510 -10 172 0"       # in front of the corn cart

PLAN = [(8.0, "lyud.People.Order khuazhev_aslan work_cheese", None),
        (8.2, "lyud.People.Order guseva_valentina work_corn", None),
        (8.5, TENT_SIDE, None),
        (14.0, "shot", "tent_side"),
        (14.5, TENT, None),
        (20.0, "lyud.People.Report khuazhev_aslan", None),
        (20.5, "shot", "tent_front"),
        (21.0, "Inventory.Earn 300", None),
        (21.5, "lyud.Trade.Open khuazhev_aslan", None),
        (23.0, "shot showui", "tent_shop"),
        (23.5, "Shop.Open 0", None),
        (24.0, "Shop.Open 1", None),
        (24.5, "Shop.Open close", None),
        (25.6, "shot", "handover_reach"),
        (26.6, "shot", "handover_carry"),
        (28.0, SIGN, None),
        (31.0, "shot", "tent_sign_ru"),
        (31.5, "lyud.Language en", None),
        (33.0, "shot", "tent_sign_en"),
        (33.5, "lyud.Language ja", None),
        (35.0, "shot", "tent_sign_ja"),
        (35.5, "lyud.Language ru", None),
        (36.0, HANDS, None)]
t = 40.0
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
         (t + 6.0, "shot", "corn_cart_wide"),
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

if os.environ.get("FOODTEST_ONLY") == "hand":
    # the hand at the lips only, with the miss logged (lyud.HandUse.Debug)
    PLAN = [(8.0, HANDS, None), (9.0, "lyud.HandUse.Debug 1", None)]
    t = 12.0
    for food in ("corn_cob", "cheese_round_small", "cheese_braid"):
        PLAN += [(t, "lyud.Food.Hold " + food, None), (t + 2.0, "shot", food + "_carry"), (t + 2.5, "lyud.Hand.Press 1", None),
                 (t + 4.0, "shot", food + "_lips"), (t + 4.5, "lyud.Hand.Press 0", None), (t + 6.0, "lyud.Hand.PutAway", None)]
        t += 7.0

if os.environ.get("FOODTEST_ONLY") == "sign":
    PLAN = [(8.0, SIGN, None), (12.0, "shot", "sign_ru"), (12.5, "lyud.Language ja", None), (14.0, "shot", "sign_ja"),
            (14.5, "lyud.Language ru", None), (15.0, TENT, None), (19.0, "shot", "tent_front")]


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
