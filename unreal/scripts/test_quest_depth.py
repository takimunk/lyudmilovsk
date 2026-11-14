"""Offscreen check of the quests' depth (docs/quests.md): talk variants (.if.<cond>), several .again lines said in turn, and
remarks (quest_scenes.json): QUESTDEPTH lines, the dialogue's "NpcDialogue: line" keys and Quest.Dump's QUESTTEST state.
  l) awakening on owner: Guy_b's first exchange, then his aside owner_says_odd.1, then everyday lines
  m) explore on walk: Guy_b's asides valley_way, four_places_two_by_cable, never_got_swift in turn, then everyday lines
  n) the Owner: his first exchange on awakening's owner stage (the stage finishes); on outside door_never_locked.1 (or by the sun)
  o) Sato_yuki's everyday talks in the engine's order (her first, crow_gift's after-talk, again1..4, sun lines)
  b) waxwing on prepare: Sorokin says waxwing_tips.1 with the pocket as at the start, tips_feeder_in_hand.1 with a feeder
  c) waxwing on wait after sunset while he is still on his bench: rowan_till_dawn.1 (the talk logs its sun state)
  d) waxwing on wait by day: waxwing_wait.1, waxwing_wait_hint.1, waxwing_wait_snow.1, waxwing_wait_hint.1
  g) evening_paper done: Nina's after-talk (quest.evening_paper.done) once, then an everyday line
  h) Lange on first_catch's catch: catch_wait, catch_wait_hint, catch_wait_nets, braid_three_days, round again; done:
     angler_spoon_dawn, then everyday lines
  p) waxwing on return, soda_science on taste: the waxwing's call beside Sorokin (Galina on a bench near by) is his
  r) waxwing on return, a call by Valentina's cart: at 13:00 she answers (pointing to Sorokin's bench), at 15:00 he does
  s) the diary (journal entries, Quest.Dump's count): +1 when Tanya's remark is heard, none on the second approach nor
     after save and load; the waxwing's call heard by Sorokin adds its entry once, a second call none
  t) fisherman_cheese on ask with a braid in the pocket: Lange's cheese_smoked_pocket.1-2, the quest moves on
  u) soda on drink, the hero by Tyoma's station: remark tyoma_soda_advice
  v) a great tit beside Vika in the yard: one line.li_vika.* reaction
  q) two remarks in range at once (the Clerk, Sonya): the second begins after the first has finished
  j) a great tit beside Dasha at 17:30 (Aya on the same bench): exactly one person answers
  k) waxwing on return: a call by Valentina's cart and one by Sorokin both get his waxwing_seen lines and finish the quest
  e) waxwing on prepare, the hero from afar to 3 m from Tanya in the Gastronom: "QuestDirector: remark tanya_rowan_doorway" once, not again
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
# Guy_b sits at a club computer (head about 65, -152, 148 cm): stand behind him in the aisle, eyes on his head
at("BugItGo -60 -152 100 -5 0 0", None, 3.0)
at("Npc.Where")
# the club's regulars hear birds too: a pigeon beside Guy_b, before any talk (a talk in progress cuts the director's speech)
at("Quest.BirdCall FeralPigeon guy_b", None, 6.0)
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
# the Owner talks through his window: stand in its call zone (40, 390) facing him (head about 163, 640)
at("BugItGo 40 390 100 0 64 0", None, 10.0)
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
# o) everyday talks in turn: Sato_yuki (crow_gift finished so that no quest talk comes first: talking to her begins it)
at("Quest.Reset", None, 1.0)
at("Quest.SetStage crow_gift done", None, 1.0)
go("sato_yuki", 1.3, [16.5, 17, 9, 10, 11, 12, 15, 18])
at("QUESTDEPTH mark o_yuki")
for _ in range(16):
    at("Npc.Talk", None, 1.2)
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
# h) Lange, first_catch on catch (worms in the pocket, by day): catch_wait.1, catch_wait_hint.1, catch_wait_nets.1,
#    braid_three_days.1-2, then round again; first_catch done: angler_spoon_dawn (the after-talk), then everyday lines
at("Quest.Reset", None, 1.0)
at("Quest.SetStage fisherman_cheese done", None, 1.0)
at("Inventory.Give bait_worm 10")
at("Quest.SetStage first_catch catch", None, 1.0)
go("lange_pyotr", 1.3, [14, 15, 13, 16])
at("QUESTDEPTH mark h_catch")
for _ in range(11):
    at("Npc.Talk", None, 1.2)
at("Quest.SetStage first_catch done", None, 1.0)
at("QUESTDEPTH mark h_after")
for _ in range(9):
    at("Npc.Talk", None, 1.2)
at("Quest.Dump", "h_done")
# p) waxwing on return and soda_science on taste: the waxwing called beside Sorokin with Galina on a bench near by: he answers
at("Quest.Reset", None, 1.0)
at("Quest.SetStage soda_science taste", None, 1.0)
at("Quest.SetStage waxwing return", None, 1.0)
go("sorokin_oleg", 6, [18.2, 18.1, 18.25])
at("Quest.Scenes", "p_listeners", 0.5)
at("QUESTDEPTH mark p_call")
at("Quest.BirdCall Waxwing sorokin_oleg", None, 10.0)
at("Quest.Dump", "p_done")
# r) waxwing on return, the call beside Valentina's cart: at 13:00 (Sorokin away from his bench) she points to his bench and the
#    quest stays on return; at 15:00 (Sorokin on his bench 4.6 m from her cart) the same call is his and the quest ends
at("Quest.Reset", None, 1.0)
at("Quest.SetStage waxwing return", None, 1.0)
go("guseva_valentina", 4, [13, 13.25])
at("Quest.Scenes", "r_listeners_13h", 0.5)
at("QUESTDEPTH mark r_call_13h")
at("Quest.BirdCall Waxwing guseva_valentina", None, 8.0)
at("Quest.Dump", "r_after_13h")
go("guseva_valentina", 4, [15, 15.5, 16])
at("QUESTDEPTH mark r_call_15h")
at("Quest.BirdCall Waxwing guseva_valentina", None, 10.0)
at("Quest.Dump entries", "r_after_15h")
at("Quest.SetStage waxwing return", None, 1.0)
at("Quest.Dump", "s_before_second_call")
at("Quest.BirdCall Waxwing sorokin_oleg", None, 10.0)
at("Quest.Dump entries", "s_after_second_call")
# q) two remarks in range at once (awakening on owner: the Clerk's at once, Sonya's at once): one voice at a time; where Sonya's
#    waits, Quest.Scenes says why; a line trace from the camera to her head names what is in between
at("Quest.Reset", None, 1.0)
at("BugItGo -193 1500 150 0 90 0", None, 2.0)
at("Quest.SetStage awakening owner", None, 2.0)
at("QUESTDEPTH mark q_walkway_station7")
at("BugItGo -107 -1235 100 0 0 0", None, 10.0)
at("Quest.Scenes", "q_walkway", 0.5)
at("TRACE 52.8 -1235.1 118.9", None, 1.0)
at("QUESTDEPTH mark q_hero_seat")
at("BugItGo -17.5 -1585 100 0 90 0", None, 8.0)
at("Quest.Scenes", "q_seat", 0.5)
at("TRACE 52.8 -1235.1 118.9", None, 1.0)
# t) fisherman_cheese jumped to ask with a cheese braid in the pocket: Lange's cheese_smoked_pocket.1-2, the quest moves on
at("Quest.Reset", None, 1.0)
at("Inventory.Give cheese_braid 1", None, 0.5)
at("Quest.SetStage fisherman_cheese ask", None, 1.0)
go("lange_pyotr", 1.3, [14, 15, 13, 16])
at("QUESTDEPTH mark t_lange_cheese")
for _ in range(6):
    at("Npc.Talk", None, 1.2)
at("Quest.Dump", "t_done")
# u) soda on drink, the hero from the street to Tyoma's station 6 in the club: remark tyoma_soda_advice (with its diary line)
at("Quest.Reset", None, 1.0)
at("Quest.Start soda", None, 1.0)
at("BugItGo -193 1500 150 0 90 0", None, 2.5)
at("QUESTDEPTH mark u_tyoma")
at("BugItGo -107 -1082 100 0 0 0", None, 10.0)
at("Quest.Scenes", "u_scenes", 0.5)
at("Quest.Dump entries", "u_done")
# v) round ten's listeners: a great tit beside Vika in the Academgorodok's yard: one line.li_vika.* reaction
at("Quest.Reset", None, 1.0)
go("li_vika", 4, [18.25, 18.5, 18.1])
at("QUESTDEPTH mark v_vika")
at("Quest.BirdCall GreatTit li_vika", None, 6.0)
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
at("Dev.Tp academ", None, 2.5)                  # farther than 1.5 x within_m while on prepare: the remark is armed
at("Quest.Dump", "s_before_remark")
at("QUESTDEPTH mark e_remark")
at("Quest.GoToPerson orlova_tanya 3", None, 3.0)
at("Quest.GoToPerson orlova_tanya 3", None, 3.0)      # the second time her body is drawn
at("Quest.Scenes", "e_after_first", 0.5)
at("Quest.Dump entries", "s_after_remark")
at("shot showui", "tanya_remark", 1.0)
at("Dev.Tp academ", None, 4.0)
at("Quest.GoToPerson orlova_tanya 3", None, 3.0)
at("Quest.GoToPerson orlova_tanya 3", None, 4.0)
at("Quest.Scenes", "e_after_second", 0.5)
at("Quest.Dump entries", "s_after_second_approach")
at("lyud.Save questdepth", None, 3.0)
at("lyud.Load 0", None, 15.0)
at("lyud.Birds.Time 12", None, 2.5)
at("Quest.GoToPerson orlova_tanya 3", None, 3.0)
at("Quest.GoToPerson orlova_tanya 3", None, 4.0)
at("Quest.Scenes", "e_after_load", 0.5)
at("Quest.Dump entries", "e_done", 1.0)


def trace(w, to):
    """the camera to a point (cm): what blocks the view, as the remarks' sight test would see it"""
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    cam = pc.player_camera_manager.get_camera_location()
    end = unreal.Vector(*to)
    hit = unreal.SystemLibrary.line_trace_single(w, cam, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pc.get_controlled_pawn()],
                                                 unreal.DrawDebugTrace.NONE, True)
    try:
        t = hit.to_tuple() if hit is not None else ()
    except Exception:
        t = ()
    actor = next((v for v in t if isinstance(v, unreal.Actor)), None)
    comp = next((v for v in t if isinstance(v, unreal.ActorComponent)), None)
    blocked = bool(t) and bool(t[0]) and actor is not None
    unreal.log_warning("QUESTDEPTH trace from %s to %s: %s" % (cam, end, ("hit %s / %s" % (actor.get_name(), comp.get_name() if comp else None))
                                                              if blocked else "clear"))


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
        if cmd.startswith("TRACE "):
            trace(w, [float(v) for v in cmd.split()[1:4]])
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
