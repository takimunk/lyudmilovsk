"""One-process A/B of the cigarettes' cost (docs/smoking.md "Checks"): the same view on the terrace, rounds of nothing in the hand
and of a lit cigarette smoking (the stream from the tip, a drag and a breath out every few seconds), alternating, the frame time
of each round. SMOKEPERF lines in the log. Run alone: sh unreal/scripts/test_smoking_perf.sh (waits for an empty UE queue).
"""
import unreal

ROUND = 12.0
state = {"t": 0.0, "step": 0, "dts": [], "round": None, "next_drag": 0.0}
VIEW = "BugItGo -150 1060 172 -5 0 0"
SETUP = [(9.0, VIEW), (10.0, "lyud.Smoke.Give"), (10.5, "lyud.Smoke.Give"), (11.0, "t.MaxFPS 0")]
# base, smoke, base, smoke, base, smoke, base
ROUNDS = ["base", "smoke", "base", "smoke", "base", "smoke", "base"]
START = 16.0


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.log_warning("SMOKEPERF cmd %s" % c)
    unreal.SystemLibrary.execute_console_command(w, c)


def report(name, dts):
    if len(dts) < 10:
        return
    s = sorted(dts)
    mean = sum(s) / len(s) * 1000.0
    p95 = s[int(len(s) * 0.95)] * 1000.0
    unreal.log_warning("SMOKEPERF round %s frames %d mean %.2f ms p95 %.2f ms" % (name, len(s), mean, p95))


def tick(dt):
    w = world()
    if not w:
        return
    if state["t"] < -1e8:
        return
    state["t"] = unreal.GameplayStatics.get_time_seconds(w)
    t = state["t"]
    while state["step"] < len(SETUP) and t > SETUP[state["step"]][0]:
        cmd(w, SETUP[state["step"]][1])
        state["step"] += 1
    if t < START:
        return
    k = int((t - START) // ROUND)
    if k >= len(ROUNDS):
        report(state["round"], state["dts"])
        cmd(w, "lyud.Smoke.State")
        unreal.log_warning("SMOKEPERF done")
        unreal.SystemLibrary.execute_console_command(w, "quit")
        state["t"] = -1e9
        return
    name = "%s%d" % (ROUNDS[k], k)
    if name != state["round"]:
        report(state["round"], state["dts"])
        state["round"], state["dts"] = name, []
        inside = 0.0
        if ROUNDS[k] == "smoke":
            # take one out and light it; the timing of the round starts once it is lit and in the hand
            cmd(w, "lyud.Smoke.Take")
        else:
            cmd(w, "lyud.Smoke.Out")
    inside = (t - START) - k * ROUND
    if ROUNDS[k] == "smoke":
        if 4.5 < inside < 4.5 + dt:
            cmd(w, "lyud.Hand.Click")
        # drags every 2 s once lit (from ~8.5 s in), each 1 s
        if inside > 8.6:
            ph = (inside - 8.6) % 2.0
            if ph < dt:
                cmd(w, "lyud.Hand.Press 1")
            elif 1.0 <= ph < 1.0 + dt:
                cmd(w, "lyud.Hand.Press 0")
    # only the settled part of each round counts: the base from 2 s, the smoke once it smokes
    if (ROUNDS[k] == "base" and inside > 2.0) or (ROUNDS[k] == "smoke" and inside > 8.6):
        state["dts"].append(dt)


unreal.register_slate_post_tick_callback(tick)
