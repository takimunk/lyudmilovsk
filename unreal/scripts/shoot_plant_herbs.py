"""Shots of L_PlantHerbs (make_plant_herbs_level.py) from its Cam_* cameras at chosen days of the year, then the
frame time with the 51-pot room in view and out of view. Offscreen -game; HERB_SHOTS is the folder.
  UnrealEditor <uproject> /Game/Dev/L_PlantHerbs -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 "-ExecCmds=py <this file>"
"""
import os
import time
import unreal

SHOTS = os.environ.get("HERB_SHOTS", "/tmp/shots")
ONLY = os.environ.get("HERB_ONLY", "")
INDOOR = ["RubberPlant", "WeepingFig", "Lemon", "Dracaena", "Monstera", "SnakePlant", "Aspidistra", "SpiderPlant", "Tradescantia",
          "Geranium", "Begonia", "AfricanViolet", "KentiaPalm", "Aloe", "Cactus", "Dieffenbachia", "AsparagusFern", "Amaryllis", "Avocado"]
PLAN = [("Bed", 200), ("Bed", 293), ("Bed", 304), ("Bed", 120), ("BedClose", 200), ("BedClose", 293), ("AsterClose", 200), ("AsterClose", 293), ("AsterClose", 330), ("Hedge", 145), ("Hedge", 200), ("Hedge", 293), ("HedgeClose", 200), ("IndoorRow", 293)]
PLAN += [(s, 293) for s in INDOOR] + [("Amaryllis", 20), ("Amaryllis", 60)]
if os.environ.get("HERB_QUICK"):
    PLAN = [("Hedge", 200), ("HedgeClose", 200), ("HedgeClose", 293), ("HedgeClose", 20), ("Hedge", 145)]
if ONLY:
    PLAN = [p for p in PLAN if p[0] in ONLY.split(",")]
PERF = [("Room", 0), ("Empty", 0)]
WARMUP = 300
HOLD = 120
state = {"t": 0, "i": -1, "dts": [], "last": 0.0, "perf": {}}
plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("L_PlantHerbs") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def camera(w, label):
    for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor):
        if c.get_actor_label() == "Cam_" + label:
            return c


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += 1
    t = state["t"]
    if state["i"] < 0:
        if t >= WARMUP:
            state["i"], state["t"] = 0, 0
            reports = [p.debug_report(p.get_age(), 293.0) for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls) if p.get_actor_label().startswith("In_") or p.get_actor_label() in ("Privet_1", "Spiraea_1")]
            for r in reports:
                unreal.log_warning("PLANTHERBS " + r)
            # bounds: an old house plant of any seed fits 2.3 x 0.9 m (the species' own caps are tighter)
            import re
            worst = {}
            for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
                if p.get_actor_label().startswith("Bound_"):
                    m = re.search(r"h ([0-9.]+) r ([0-9.]+)", p.debug_report(60.0, 20.0))
                    sp = p.get_actor_label().split("_")[1]
                    if not m:
                        continue  # the potted trees (weeping fig, lemon) are bounded by the tree model
                    h, r = float(m.group(1)), float(m.group(2))
                    o = worst.get(sp, (0, 0))
                    worst[sp] = (max(o[0], h), max(o[1], r))
            for sp, (h, r) in sorted(worst.items()):
                unreal.log_warning("PLANTBOUND %s h %.2f r %.2f %s" % (sp, h, r, "ok" if h <= 2.3 and r <= 0.9 else "FAIL"))
        return
    i = state["i"]
    if i < len(PLAN):
        label, day = PLAN[i]
        if t == 1:
            for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
                p.set_dev_time(-1.0, float(day))
            unreal.GameplayStatics.get_player_controller(w, 0).set_view_target_with_blend(camera(w, label), 0.0)
        if t == HOLD:
            unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/%s_%03d.png" % (SHOTS, label, day))
            unreal.log_warning("PLANTHERBS shot %s %d" % (label, day))
            for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
                if p.get_actor_label() in ("Marigold_10", "Aster_4", "Privet_1", "Spiraea_1"):
                    unreal.log_warning("PLANTHERBS   %s: %s" % (p.get_actor_label(), p.describe_state()))
        if t == HOLD + 20:
            state["i"], state["t"] = i + 1, 0
        return
    k = i - len(PLAN)
    if k >= len(PERF):
        unreal.log_warning("PLANTHERBS perf " + " ".join("%s %.2f ms" % kv for kv in state["perf"].items()))
        unreal.SystemLibrary.execute_console_command(w, "QUIT")
        state["i"] = 10 ** 6
        return
    label, _ = PERF[k]
    if t == 1:
        unreal.GameplayStatics.get_player_controller(w, 0).set_view_target_with_blend(camera(w, label), 0.0)
        state["dts"] = []
    if 60 < t <= 360:
        state["dts"].append(dt)
    if t == 200 and label == "Room":
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/Room.png" % SHOTS)
    if t == 361:
        d = sorted(state["dts"])
        state["perf"][label] = 1000.0 * sum(d) / len(d)
        state["perf"][label + "_p95"] = 1000.0 * d[int(len(d) * 0.95)]
        state["i"], state["t"] = i + 1, 0


unreal.register_slate_post_tick_callback(tick)
