"""Offscreen check of the quests' depth (docs/quests.md): talk variants (.if.<cond>), several .again lines said in turn, and
remarks (quest_scenes.json): QUESTDEPTH lines, the dialogue's "NpcDialogue: line" keys and Quest.Dump's QUESTTEST state.
  b) waxwing on prepare: Sorokin says waxwing_tips.1 with the pocket as at the start, tips_feeder_in_hand.1 with a feeder
  c) waxwing on wait after sunset while he is still on his bench: rowan_till_dawn.1 (the talk logs its sun state)
  d) waxwing on wait by day: waxwing_wait.1, waxwing_wait_hint.1, waxwing_wait_snow.1, waxwing_wait_hint.1
  e) waxwing on prepare, the hero 3 m from Tanya in the Gastronom: "QuestDirector: remark tanya_rowan_doorway" once, not again
     after walking off and back, nor after a save and a load
test_quest_depth.sh also checks the director's count line and runs tools/check_quest_talk.py and tools/check_devdata.py.

  sh unreal/scripts/test_quest_depth.sh
"""
import unreal

state = {"t": 0.0, "step": 0, "shots": 0}
PLAN = []
T = [12.0]


def at(cmd, label=None, gap=0.6):
    PLAN.append((T[0], cmd, label))
    T[0] += gap


def go(person, m=1.3):
    at("Quest.GoToPerson %s %s" % (person, m), None, 3.0)
    at("Quest.GoToPerson %s %s" % (person, m), None, 2.0)


def conversation(n=2):
    """one conversation of a single line: open it, close it"""
    for _ in range(n):
        at("Npc.Talk", None, 1.2)


# b) variant by the pocket
at("Quest.Reset", None, 1.0)
at("Dev.Do pocket.reset", None, 1.0)
at("Quest.SetStage waxwing prepare", None, 1.0)
at("lyud.Birds.Time 10", None, 2.5)
go("sorokin_oleg")
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
at("lyud.Birds.Time 18.4", None, 2.5)
go("sorokin_oleg")
at("Quest.SetStage waxwing wait", None, 1.0)
at("QUESTDEPTH mark c_dusk")
conversation()
at("shot showui", "sorokin_dusk")
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
    state["t"] += min(dt, 0.25)
    while state["step"] < len(PLAN) and state["t"] > PLAN[state["step"]][0]:
        _, cmd, label = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("QUESTDEPTH %s %s" % (cmd, label or ""))
        if cmd.startswith("QUESTDEPTH"):
            continue
        if cmd.startswith("shot"):
            unreal.log_warning("QUESTDEPTH shot %02d %s" % (state["shots"], label))
            state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("QUESTDEPTH finished")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
