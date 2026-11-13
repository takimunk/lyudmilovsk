"""Offscreen check of the four short quests (docs/quests.md): "The evening paper" (evening_paper: a 'bring' objective) and
"The crows' gift" (crow_gift), "The fisherman's cheese" (fisherman_cheese: bring) and
"The window feeder" (window_feeder: feeder_fill / feeder_visit with target *), first by the quests' own events, then the paper once more through a real talk (Npc.Talk in
front of Nina Korneeva) so the dialogue's quest lines and the <stage>_empty talk show in the log. QUESTSHORT lines with
Quest.Dump's QUESTTEST state between the steps.

  sh unreal/scripts/test_quest_short.sh
"""
import os

import unreal

state = {"t": 0.0, "step": 0, "shots": 0, "find": None}
LOG = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "test_quest_short.log")
# the hours tried for a person who is to be seen (drawn, not away) when FIND runs; the first that works is kept
HOURS = [9, 17, 18, 19, 20, 8, 10, 11, 12, 13, 14, 15, 16, 21]


def seen_in_log(person):
    """The last "QUESTTEST goto <person>: ..." line says seen?"""
    try:
        with open(LOG, encoding="utf-8", errors="replace") as f:
            lines = [l for l in f if "QUESTTEST goto %s:" % person in l]
    except OSError:
        return False
    return bool(lines) and ", seen)" in lines[-1]


def find_tick(w, dt):
    """FIND <person>: each hour in turn, go to them twice (the second time their body is drawn) and read the log."""
    f = state["find"]
    f["wait"] -= dt
    if f["wait"] > 0.0:
        return True
    if f["phase"] == 0:
        h = HOURS[f["i"]]
        unreal.log_warning("QUESTSHORT find %s at %d h" % (f["person"], h))
        unreal.SystemLibrary.execute_console_command(w, "lyud.Birds.Time %d" % h)
        f["phase"], f["wait"] = 1, 2.5
    elif f["phase"] in (1, 2):
        unreal.SystemLibrary.execute_console_command(w, "Quest.GoToPerson %s 1.3" % f["person"])
        f["phase"], f["wait"] = f["phase"] + 1, 3.0
    else:
        if seen_in_log(f["person"]) or f["i"] + 1 >= len(HOURS):
            unreal.log_warning("QUESTSHORT found %s at %d h: %s" % (f["person"], HOURS[f["i"]], seen_in_log(f["person"])))
            state["find"] = None
            state["t"] = PLAN[state["step"]][0] - 0.5 if state["step"] < len(PLAN) else state["t"]
            return False
        f["i"], f["phase"], f["wait"] = f["i"] + 1, 0, 0.0
    return True
PLAN = [
    (12.0, "Quest.Reset", None),
    (12.2, "Quest.List", None),
    # the evening paper by events: the talk that gave it does not finish it; the paper in the pocket; the talk that brings it
    (12.5, "Quest.Start evening_paper", None),
    (13.0, "Quest.Dump", "paper_started"),
    (13.5, "Quest.Event talk Korneeva_nina", None),
    (14.0, "Quest.Dump", "paper_talked"),
    (14.5, "Quest.Event talk Korneeva_nina", None),
    (15.0, "Quest.Dump", "paper_talked_again_empty"),
    (15.5, "Inventory.Give daily_paper 1", None),
    (16.0, "Quest.Dump", "paper_bought"),
    (16.5, "Quest.Event talk Korneeva_nina", None),
    (17.5, "Quest.Dump", "paper_brought"),
    # the crows' gift by events
    (18.0, "Quest.Start crow_gift", None),
    (18.5, "Quest.Dump", "crow_started"),
    (19.0, "Quest.Event corvid_feed_thrown", None),
    (19.2, "Quest.Event corvid_feed_thrown", None),
    (19.4, "Quest.Event corvid_feed_thrown", None),
    (20.0, "Quest.Dump", "crow_fed"),
    (20.5, "Quest.Event corvid_gift_taken", None),
    (21.0, "Quest.Dump", "crow_gift"),
    (21.5, "Quest.Event talk Sato_yuki", None),
    (22.5, "Quest.Dump", "crow_told"),
    # the fisherman's cheese by events: talk, the cheese in the pocket, the talk that brings it (the cheese is taken)
    (23.0, "Quest.Start fisherman_cheese", None),
    (23.3, "Quest.Event talk Lange_pyotr", None),
    (23.8, "Quest.Dump", "cheese_talked"),
    (24.0, "Quest.Event talk Lange_pyotr", None),
    (24.5, "Quest.Dump", "cheese_talked_again_empty"),
    (25.0, "Inventory.Give cheese_braid 1", None),
    (25.5, "Quest.Dump", "cheese_bought"),
    (26.0, "Quest.Event talk Lange_pyotr", None),
    (26.8, "Quest.Dump", "cheese_brought"),
    # the window feeder by events: any feed, any five birds (targets "*")
    (27.2, "Quest.Start window_feeder", None),
    (27.5, "Quest.Event talk Kimura_dasha", None),
    (28.0, "Quest.Dump", "feeder_talked"),
    (28.3, "Quest.Event feeder_fill feed_sunflower", None),
    (28.8, "Quest.Dump", "feeder_filled"),
    (29.0, "Quest.Event feeder_visit GreatTit", None),
    (29.2, "Quest.Event feeder_visit BlueTit", None),
    (29.4, "Quest.Event feeder_visit Greenfinch 2", None),
    (29.8, "Quest.Dump", "feeder_4_visits"),
    (30.0, "Quest.Event feeder_visit HouseSparrow", None),
    (30.5, "Quest.Dump", "feeder_5_visits"),
    (31.0, "Quest.Event talk Kimura_dasha", None),
    (32.0, "Quest.Dump", "feeder_told"),
    # the paper again through a real conversation with Nina (her lines are logged by the talk director)
    (35.0, "Quest.Reset", None),
    (35.5, "FIND korneeva_nina", None),
    (38.0, "Quest.GoToPerson korneeva_nina 1.3", None),
    (41.0, "Npc.Where", None),
    (41.5, "Npc.Talk", None),
    (42.0, "Quest.Dump", "talk_real_1"),
    (42.5, "shot showui", "nina_speaks"),
    (43.0, "Npc.Talk", None), (44.0, "Npc.Talk", None), (45.0, "Npc.Talk", None), (46.0, "Npc.Talk", None),
    (47.0, "Npc.Talk", None), (48.0, "Npc.Talk", None), (49.0, "Npc.Talk", None),
    (50.0, "Quest.Dump", "talk_real_1_end"),
    (51.0, "Npc.Talk", None), (52.0, "Npc.Talk", None), (53.0, "Npc.Talk", None), (54.0, "Npc.Talk", None),
    (55.0, "Quest.Dump", "talk_real_2_empty"),
    (56.0, "Inventory.Give daily_paper 1", None),
    (57.0, "Npc.Talk", None), (58.0, "Npc.Talk", None), (59.0, "Npc.Talk", None), (60.0, "Npc.Talk", None),
    (61.0, "Npc.Talk", None), (62.0, "Npc.Talk", None),
    (63.0, "Quest.Dump", "talk_real_3_brought"),
]


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def tick(dt):
    w = world()
    if not w:
        return
    dt = min(dt, 0.25)
    if state["find"] is not None and find_tick(w, dt):
        return
    state["t"] += dt
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, label = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("QUESTSHORT %s %s" % (cmd, label or ""))
        if cmd.startswith("FIND "):
            state["find"] = {"person": cmd.split()[1], "i": 0, "phase": 0, "wait": 0.0}
            return
        if cmd.startswith("shot"):
            unreal.log_warning("QUESTSHORT shot %02d %s" % (state["shots"], label))
            state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 1.0:
        unreal.log_warning("QUESTSHORT finished")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
