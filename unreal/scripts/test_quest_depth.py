"""Offscreen check of the quests' depth (docs/quests.md): talk variants (.if.<cond>), several .again lines said in turn, and
remarks (quest_scenes.json): QUESTDEPTH lines, the dialogue's "NpcDialogue: line" keys and Quest.Dump's QUESTTEST state.
  l) awakening on owner: Guy_b's first exchange, then his aside owner_says_odd.1, then everyday lines
  m) explore on walk: Guy_b's asides valley_way, four_places_two_by_cable, never_got_swift in turn, then everyday lines
  n) the Owner: his first exchange on awakening's owner stage (the stage finishes); on outside door_never_locked.1 (or by the sun)
  b) waxwing on prepare: Sorokin says waxwing_tips.1 with the pocket as at the start, tips_feeder_in_hand.1 with a feeder
  c) waxwing on wait after sunset while he is still on his bench: rowan_till_dawn.1 (the talk logs its sun state)
  d) waxwing on wait by day: waxwing_wait.1, waxwing_wait_hint.1, waxwing_wait_snow.1, waxwing_wait_hint.1
  g) evening_paper done: Nina's after-talk (quest.evening_paper.done) once, then an everyday line
  h) fisherman_cheese done, first_catch on learn: Lange says catch_learn (an active quest first); first_catch done: the two
     after-talks one after another, then everyday lines
  j) a great tit beside Dasha at 17:30 (Aya on the same bench): exactly one person answers
  k) waxwing on return: a call by Valentina's cart and one by Sorokin both get his waxwing_seen lines and finish the quest
  e) waxwing on prepare, the hero 3 m from Tanya in the Gastronom: "QuestDirector: remark tanya_rowan_doorway" once, not again
     after walking off and back, nor after a save and a load
test_quest_depth.sh also checks the director's count line and runs tools/check_quest_talk.py and tools/check_devdata.py.

  sh unreal/scripts/test_quest_depth.sh
"""
import os

import unreal

state = {"t": 0.0, "step": 0, "shots": 0, "find": None}
LOG = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "test_quest_depth.log")


def seen_in_log(person):
    try:
        with open(LOG, encoding="utf-8", errors="replace") as f:
            lines = [l for l in f if "QUESTTEST goto %s:" % person in l]
    except OSError:
        return False
    return bool(lines) and ", seen)" in lines[-1]


def find_tick(w, dt):
    """FIND <person> <m> <h,h,...>: each hour in turn, go to them three times (their body is drawn by then), until seen"""
    f = state["find"]
    f["wait"] -= dt
    if f["wait"] > 0.0:
        return True
    if f["phase"] == 0:
        unreal.SystemLibrary.execute_console_command(w, "lyud.Birds.Time %s" % f["hours"][f["i"]])
        f["phase"], f["wait"] = 1, 2.5
    elif f["phase"] < 4:
        unreal.SystemLibrary.execute_console_command(w, "Quest.GoToPerson %s %s" % (f["person"], f["m"]))
        f["phase"], f["wait"] = f["phase"] + 1, 3.5
    else:
        if seen_in_log(f["person"]) or f["i"] + 1 >= len(f["hours"]):
            unreal.log_warning("QUESTDEPTH found %s at %s h: %s" % (f["person"], f["hours"][f["i"]], seen_in_log(f["person"])))
            state["find"] = None
            if state["step"] < len(PLAN):
                state["t"] = PLAN[state["step"]][0] - 0.3
            return False
        f["i"], f["phase"], f["wait"] = f["i"] + 1, 0, 0.0
    return True
PLAN = []
T = [12.0]


def at(cmd, label=None, gap=0.6):
    PLAN.append((T[0], cmd, label))
    T[0] += gap


def go(person, m=1.3, hours=None):
    if hours:
        at("FIND %s %s %s" % (person, m, ",".join(str(h) for h in hours)), None, 1.0)
    else:
        for _ in range(3):
            at("Quest.GoToPerson %s %s" % (person, m), None, 3.5)


def conversation(n=2):
    """one conversation of a single line: open it, close it"""
    for _ in range(n):
        at("Npc.Talk", None, 1.2)


# l) asides wait for the first conversation: awakening on owner, Guy_b (the club): first.1.., then owner_says_odd.1, then everyday
at("Quest.Reset", None, 1.0)
at("Quest.SetStage awakening owner", None, 1.0)
go("guy_b", 1.3)
at("Npc.Where")
at("QUESTDEPTH mark l_guy_b")
for _ in range(10):
    at("Npc.Talk", None, 1.2)
# m) explore on walk: valley_way, four_places_two_by_cable, never_got_swift, then everyday
at("Quest.Reset", None, 1.0)
at("Quest.Start explore", None, 1.0)
at("QUESTDEPTH mark m_guy_b")
for _ in range(12):
    at("Npc.Talk", None, 1.2)
# n) the Owner: on awakening's owner stage his first exchange (the stage finishes); on outside door_never_locked (or the sun's variant)
at("Quest.Reset", None, 1.0)
at("Quest.SetStage awakening owner", None, 1.0)
go("owner", 1.3)
at("Npc.Where")
at("QUESTDEPTH mark n_owner")
for _ in range(6):
    at("Npc.Talk", None, 1.2)
at("Quest.Dump", "n_after_first")
at("Quest.SetStage awakening outside", None, 1.0)
at("QUESTDEPTH mark n_outside")
for _ in range(3):
    at("Npc.Talk", None, 1.2)
at("Quest.Dump", "n_done")
# b) variant by the pocket
at("Quest.Reset", None, 1.0)
at("Dev.Do pocket.reset", None, 1.0)
at("Quest.SetStage waxwing prepare", None, 1.0)
go("sorokin_oleg", 1.3, [10, 11, 9, 15, 16])
at("QUESTDEPTH mark b_no_feeder")
conversation()
at("Inventory.Give feeder 1", None, 1.0)
at("QUESTDEPTH mark b_feeder")
conversation()
at("Quest.Dump", "b_done")
# d) several .again lines by day
at("Quest.SetStage waxwing wait", None, 1.0)
at("QUESTDEPTH mark d_day_again")
for _ in range(4):
    conversation()
# c) after sunset, still on his bench (14:30-18:30)
go("sorokin_oleg", 1.3, [18.4, 18.3, 18.45])
at("Quest.SetStage waxwing wait", None, 1.0)
at("QUESTDEPTH mark c_dusk")
conversation()
at("shot showui", "sorokin_dusk")
# g) a finished quest's after-talk once, then everyday talk (Nina strolls in the Academgorodok at 9)
at("Quest.Reset", None, 1.0)
at("Quest.SetStage evening_paper done", None, 1.0)
go("korneeva_nina", 1.3, [9, 18.25, 10, 17, 11, 16, 12])
at("QUESTDEPTH mark g_nina")
for _ in range(7):
    at("Npc.Talk", None, 1.2)
# h) an active quest before an after-talk; then the two after-talks one after another, then everyday (Lange mends nets 12:30-17)
at("Quest.Reset", None, 1.0)
at("Quest.SetStage fisherman_cheese done", None, 1.0)
at("Quest.SetStage first_catch learn", None, 1.0)
at("lyud.Birds.Time 14", None, 2.5)
go("lange_pyotr")
at("QUESTDEPTH mark h_learn")
for _ in range(7):
    at("Npc.Talk", None, 1.2)
at("Quest.SetStage first_catch done", None, 1.0)
at("QUESTDEPTH mark h_after")
for _ in range(14):
    at("Npc.Talk", None, 1.2)
at("Quest.Dump", "h_done")
# j) one listener answers: Dasha and Aya on the square's bench at 17:30, a great tit calls beside Dasha
at("Quest.Reset", None, 1.0)
at("lyud.Birds.Time 17.5", None, 2.5)
go("kimura_dasha", 4)
at("Quest.Scenes", "j_listeners", 0.5)
at("QUESTDEPTH mark j_call")
at("Quest.BirdCall GreatTit kimura_dasha", None, 6.0)
# k) waxwing on return: the call beside Valentina's corn cart (Sorokin's bench within his 14 m) is answered by him, not her
at("Quest.SetStage waxwing return", None, 1.0)
at("lyud.Birds.Time 16", None, 2.5)
go("guseva_valentina", 4)
at("Quest.Scenes", "k_listeners", 0.5)
at("QUESTDEPTH mark k_call_near_cart")
at("Quest.BirdCall Waxwing guseva_valentina", None, 10.0)
at("Quest.Dump", "k_after_cart_call")
at("Quest.SetStage waxwing return", None, 1.0)
go("sorokin_oleg", 6)
at("QUESTDEPTH mark k_call_sorokin")
at("Quest.BirdCall Waxwing sorokin_oleg", None, 10.0)
at("Quest.Dump", "k_done")
# e) the remark in the Gastronom
at("Quest.Reset", None, 1.0)
at("Quest.SetStage waxwing prepare", None, 1.0)
at("lyud.Birds.Time 12", None, 2.5)
at("QUESTDEPTH mark e_remark")
at("Quest.GoToPerson orlova_tanya 3", None, 3.0)
at("Quest.Scenes", "e_after_first", 0.5)
at("shot showui", "tanya_remark", 1.0)
at("Dev.Tp academ", None, 4.0)
at("Quest.GoToPerson orlova_tanya 3", None, 4.0)
at("Quest.Scenes", "e_after_second", 0.5)
at("lyud.Save questdepth", None, 3.0)
at("lyud.Load 0", None, 15.0)
at("lyud.Birds.Time 12", None, 2.5)
at("Quest.GoToPerson orlova_tanya 3", None, 4.0)
at("Quest.Scenes", "e_after_load", 0.5)
at("Quest.Dump", "e_done", 1.0)


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
        unreal.log_warning("QUESTDEPTH %s %s" % (cmd, label or ""))
        if cmd.startswith("QUESTDEPTH"):
            continue
        if cmd.startswith("FIND "):
            _, person, m, hours = cmd.split()
            state["find"] = {"person": person, "m": m, "hours": hours.split(","), "i": 0, "phase": 0, "wait": 0.0}
            return
        if cmd.startswith("shot"):
            unreal.log_warning("QUESTDEPTH shot %02d %s" % (state["shots"], label))
            state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("QUESTDEPTH finished")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
