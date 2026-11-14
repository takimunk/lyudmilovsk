"""Offscreen check that developer mode drives the quests (docs/dev_mode.md "Quests"): every stage of "A Waxwing for Sorokin"
by Dev.Quest (each stage's dev block: teleport, time, give, exec), then the panel's Quests tab, then `Dev.Quest <quest> play`
for every quest of docs/quests.md ("DEV play <quest> done", or "STUCK at stage <id>"). QUESTDEV lines with Quest.Dump's
QUESTTEST state and Dev.Where between the jumps.

  sh unreal/scripts/test_quest_dev.sh
"""
import unreal

STAGES = ["find", "prepare", "journey", "lure", "wait", "tame", "return"]
QUESTS = ["waxwing", "waxwing_after", "evening_paper", "crow_gift", "fisherman_cheese", "window_feeder"]
state = {"t": 0.0, "step": 0, "shots": 0}
PLAN = [(12.0, "Quest.Reset", None)]
t = 12.5
for s in STAGES:
    PLAN += [(t, "Dev.Quest waxwing %s" % s, None), (t + 4.0, "Quest.Dump", "stage_" + s), (t + 4.3, "Dev.Where", None),
             (t + 4.6, "shot showui", "dev_" + s)]
    t += 6.0
PLAN += [(t, "Dev.Quest waxwing lure", None), (t + 3.0, "Dev.Panel open", None), (t + 4.0, "Dev.Panel clickkey ui.dev.tab.quests", None),
         (t + 6.0, "shot showui", "panel_quests"), (t + 7.0, "Dev.Panel close", None)]
t += 8.0
for q in QUESTS:
    PLAN += [(t, "Dev.Quest %s forget" % q, None), (t + 0.5, "Dev.Quest %s play" % q, None), (t + 6.0, "Quest.Dump", "play_" + q)]
    t += 7.0


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
        unreal.log_warning("QUESTDEV %s %s" % (cmd, label or ""))
        if cmd.startswith("shot"):
            unreal.log_warning("QUESTDEV shot %02d %s" % (state["shots"], label))
            state["shots"] += 1
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 1.0:
        unreal.log_warning("QUESTDEV finished")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
