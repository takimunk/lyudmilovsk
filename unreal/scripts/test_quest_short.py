"""Offscreen check of the two short quests (docs/quests.md): "The evening paper" (evening_paper: a 'bring' objective) and
"The crows' gift" (crow_gift), first by the quests' own events, then the paper once more through a real talk (Npc.Talk in
front of Nina Korneeva) so the dialogue's quest lines and the <stage>_empty talk show in the log. QUESTSHORT lines with
Quest.Dump's QUESTTEST state between the steps.

  sh unreal/scripts/test_quest_short.sh
"""
import unreal

state = {"t": 0.0, "step": 0, "shots": 0}
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
    # the paper again through a real conversation with Nina (her lines are logged by the talk director)
    (23.0, "Quest.Reset", None),
    (23.5, "lyud.Birds.Time 15", None),
    (26.0, "Quest.GoToPerson korneeva_nina 1.3", None),
    (29.0, "Npc.Where", None),
    (29.5, "Npc.Talk", None),
    (30.0, "Quest.Dump", "talk_real_1"),
    (30.5, "shot showui", "nina_speaks"),
    (31.0, "Npc.Talk", None), (32.0, "Npc.Talk", None), (33.0, "Npc.Talk", None), (34.0, "Npc.Talk", None),
    (35.0, "Npc.Talk", None), (36.0, "Npc.Talk", None), (37.0, "Npc.Talk", None),
    (38.0, "Quest.Dump", "talk_real_1_end"),
    (39.0, "Npc.Talk", None), (40.0, "Npc.Talk", None), (41.0, "Npc.Talk", None), (42.0, "Npc.Talk", None),
    (43.0, "Quest.Dump", "talk_real_2_empty"),
    (44.0, "Inventory.Give daily_paper 1", None),
    (45.0, "Npc.Talk", None), (46.0, "Npc.Talk", None), (47.0, "Npc.Talk", None), (48.0, "Npc.Talk", None),
    (49.0, "Npc.Talk", None), (50.0, "Npc.Talk", None),
    (51.0, "Quest.Dump", "talk_real_3_brought"),
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
    state["t"] += min(dt, 0.25)
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, label = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("QUESTSHORT %s %s" % (cmd, label or ""))
        if cmd.startswith("shot"):
            unreal.log_warning("QUESTSHORT shot %02d %s" % (state["shots"], label))
            state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 1.0:
        unreal.log_warning("QUESTSHORT finished")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
