"""Shots of L_PlantNorth (make_plant_north_level.py): the ages at chosen days, each species close up, the season rows (each
plant held at its own day), the forest bakes near and far, the herb patches; then each plant's DescribeState. Offscreen
-game; NORTH_SHOTS is the folder, NORTH_ONLY a comma list of camera names.
  UnrealEditor <uproject> /Game/Dev/L_PlantNorth -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 "-ExecCmds=py <this file>"
"""
import os
import unreal

SHOTS = os.environ.get("NORTH_SHOTS", "/tmp/northshots")
ONLY = [s for s in os.environ.get("NORTH_ONLY", "").split(",") if s]
SPECIES = ["SiberianDwarfPine", "DwarfAlder", "GoldenRhododendron", "Fireweed", "MountainAvens", "CottonGrass",
           "SakhalinWillow", "BarWillow", "KamchatkaMeadowsweet", "CowParsnip", "RiverSedge"]
# (camera, day): the forest and the patches follow the calendar's dev day as well
PLAN = [("Ages", 175), ("Ages", 210), ("Ages", 262), ("Seasons", 0)]
PLAN += [(s, d) for s in SPECIES for d in (175, 210)]
PLAN += [("Forest", 175), ("Forest", 262), ("ForestFar", 175), ("Patches", 175), ("Patches", 215), ("Patches", 262)]
if ONLY:
    PLAN = [p for p in PLAN if p[0] in ONLY]
WARMUP = 400
HOLD = 150
state = {"t": 0, "i": -1}
plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")
grass_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralGrass")
forest_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralForest")


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("L_PlantNorth") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def camera(w, label):
    for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor):
        if c.get_actor_label() == "Cam_" + label:
            return c


def set_day(w, day):
    """Every plant to day (the season rows to their own), the forest and the patches too."""
    for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
        label = p.get_actor_label()
        own = float(label.split("_")[1]) if label.startswith("Season_") else float(day)
        p.set_dev_time(-1.0, own)
    for cls in (grass_cls, forest_cls):
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, cls):
            a.set_editor_property("dev_day", float(day) if day else 175.0)


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += 1
    t = state["t"]
    if state["i"] < 0:
        if t >= WARMUP:
            state["i"], state["t"] = 0, 0
            for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
                if p.get_actor_label().startswith("Age_"):
                    unreal.log_warning("PLANTNORTH %s %s" % (p.get_actor_label(), p.debug_report(p.get_age(), 200.0)))
        return
    i = state["i"]
    if i < len(PLAN):
        label, day = PLAN[i]
        if t == 1:
            set_day(w, day)
            unreal.GameplayStatics.get_player_controller(w, 0).set_view_target_with_blend(camera(w, label), 0.0)
        if t == HOLD:
            unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/%s_%03d.png" % (SHOTS, label, day))
            unreal.log_warning("PLANTNORTH shot %s %d" % (label, day))
            if label in ("Forest", "Patches"):
                for cls in (forest_cls, grass_cls):
                    for a in unreal.GameplayStatics.get_all_actors_of_class(w, cls):
                        unreal.log_warning("PLANTNORTH   %s: %s" % (a.get_actor_label(), a.describe_state()))
        if t == HOLD + 20:
            state["i"], state["t"] = i + 1, 0
        return
    unreal.log_warning("PLANTNORTH done")
    unreal.SystemLibrary.execute_console_command(w, "QUIT")
    state["i"] = 10 ** 6


unreal.register_slate_post_tick_callback(tick)
