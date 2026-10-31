"""Offscreen check of the diary's pockets grid with long item names (the grocery feeds) in Japanese and Russian.
Run with -ExecCmds="py <abs path>" on Lvl_Scene01; INVJA lines in the log, screenshots inv_ja, inv_ru (shot showui)."""
import unreal

state = {"t": 0.0, "step": 0}
FEEDS = ["feed_suet", "feed_niger", "feed_rowan", "feed_mealworm", "feed_grain", "feed_peanuts"]
PLAN = [(12.0, "BugItGo -193 1500 150 0 90 0")]
PLAN += [(13.0 + 0.1 * i, "Inventory.Give %s 5" % f) for i, f in enumerate(FEEDS)]
PLAN += [
    (14.0, "Inventory.Give feeder 1"),
    (14.5, "lyud.Language ja"),
    (15.0, "Screen.Open inventory"),
    (18.0, "SHOT inv_ja"),
    (18.5, "lyud.Language ru"),
    (21.0, "SHOT inv_ru"),
    (22.0, "Screen.Open none"),
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
        cmd = PLAN[state["step"]][1]
        state["step"] += 1
        unreal.log_warning("INVJA " + cmd)
        unreal.SystemLibrary.execute_console_command(w, "shot showui" if cmd.startswith("SHOT ") else cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("INVJA done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
