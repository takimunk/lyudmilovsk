"""Shots of L_PlantBloom (make_plant_bloom_level.py): each species at its peak bloom day from Cam_*_Tree and Cam_*_Twigs (live),
Cam_*_Bake (summary_distance 0: the bake forests draw, flowers on its nearest LODs) and Cam_*_Far (120 m, bake only: the crown takes the
bloom colour); then a before / peak / after series for a few species, then the cost: milliseconds per frame with and without bloom.
BLOOM_SHOTS is the folder; BLOOM_ONLY a comma list of species; BLOOM_QUICK a short plan.
  UnrealEditor <uproject> /Game/Dev/L_PlantBloom -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 "-ExecCmds=py <this file>"
"""
import os
import unreal

SHOTS = os.environ.get("BLOOM_SHOTS", "/tmp/bloomshots")
ONLY = [s for s in os.environ.get("BLOOM_ONLY", "").split(",") if s]
PEAK = {"PyramidalPoplar": 100, "BlackPoplar": 102, "Aspen": 90, "BlackAlder": 76, "SilverBirch": 118, "DownyBirch": 122, "WeepingBirch": 118,
        "StoneBirch": 128, "DwarfBirch": 138, "SwampWhiteOak": 133, "Apple": 134, "Rowan": 150, "Maple": 120, "SeaBuckthorn": 108,
        "ScotsPine": 150, "DunePine": 150, "NorwaySpruce": 138, "Juniper": 124}
SERIES = {"Apple": (100, 134, 160), "SilverBirch": (90, 118, 150), "PyramidalPoplar": (70, 100, 125), "Rowan": (130, 150, 180)}
species = [s for s in PEAK if not ONLY or s in ONLY]
# (species, camera, day or 0 = each plant's own peak day, summary_distance: 1e9 live, 0 its bake)
PLAN = []
for s in species:
    if os.environ.get("BLOOM_QUICK") and s not in ("Apple", "SilverBirch", "PyramidalPoplar", "NorwaySpruce"):
        continue
    PLAN += [(s, "Tree", 0, 1.0e9), (s, "Twigs", 0, 1.0e9)]
for s in species:
    if os.environ.get("BLOOM_QUICK") and s not in ("Apple", "SilverBirch", "PyramidalPoplar", "NorwaySpruce"):
        continue
    PLAN += [(s, "Bake", 0, 0.0), (s, "Mid", 0, 0.0), (s, "Far", 0, 0.0)]
for s, days in SERIES.items():
    if s in species:
        PLAN += [(s, "Tree", d, 1.0e9) for d in days if d != PEAK[s]]
for s, days in SERIES.items():
    if s in species:
        PLAN += [(s, "Mid", d, 0.0) for d in days]
WARMUP = 500
HOLD = 90
BAKE_WAIT = 400
state = {"t": 0, "i": -1, "dts": [], "perf": {}}
plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("L_PlantBloom") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def camera(w, name):
    for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor):
        if c.get_actor_label() == "Cam_" + name:
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
            for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
                sp = str(p.get_editor_property("species"))
                if sp in PEAK:
                    unreal.log_warning("PLANTBLOOM " + p.debug_report(p.get_age(), float(PEAK[sp])))
        return
    i = state["i"]
    if i < len(PLAN):
        sp, view, day, cut = PLAN[i]
        if t == 1:
            # only this species' plant changes (its bake is made in a few seconds; all 18 at once take minutes)
            for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
                if str(p.get_editor_property("species")) == sp:
                    p.set_editor_property("summary_distance", cut)
                    p.set_dev_time(-1.0, float(day or PEAK[sp]))
            state["wait"] = BAKE_WAIT if cut < 1.0e8 else 40
            unreal.GameplayStatics.get_player_controller(w, 0).set_view_target_with_blend(camera(w, "%s_%s" % (sp, view)), 0.0)
        hold = HOLD + state["wait"]
        if t == hold:
            unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/%s_%s_%03d.png" % (SHOTS, sp, view, day or PEAK[sp]))
            for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
                if p.get_actor_label() == "Bloom_" + sp:
                    unreal.log_warning("PLANTBLOOM shot %s %s day %d: %s" % (sp, view, day or PEAK[sp], p.describe_state()))
        if t == hold + 12:
            state["i"], state["t"] = i + 1, 0
        return
    # cost: every species in bloom against none (lyud.PlantBloom), the camera on the apple tree, live and as bakes (a bake only re-makes
    # itself when the day moves 5 days, so the passes are a week apart)
    k = i - len(PLAN)
    modes = [("live_bloom", 1.0e9, 1, 0), ("live_off", 1.0e9, 0, 0), ("bake_off", 0.0, 0, 7), ("bake_bloom", 0.0, 1, 0)]
    if k >= len(modes):
        unreal.log_warning("PLANTBLOOM perf " + " ".join("%s %.2f ms" % kv for kv in state["perf"].items()))
        unreal.SystemLibrary.execute_console_command(w, "QUIT")
        state["i"] = 10 ** 6
        return
    name, cut, on, shift = modes[k]
    if t == 1:
        unreal.SystemLibrary.execute_console_command(w, "lyud.PlantBloom %d" % on)
        for p in unreal.GameplayStatics.get_all_actors_of_class(w, plant_cls):
            sp = str(p.get_editor_property("species"))
            p.set_editor_property("summary_distance", cut)
            p.set_dev_time(-1.0, float(PEAK.get(sp, 100) + shift))
        unreal.GameplayStatics.get_player_controller(w, 0).set_view_target_with_blend(camera(w, "Apple_Mid" if cut == 0.0 else "Apple_Tree"), 0.0)
        state["dts"] = []
    if (BAKE_WAIT if cut == 0.0 else 120) < t <= (BAKE_WAIT if cut == 0.0 else 120) + 300:
        state["dts"].append(dt)
    if t == (BAKE_WAIT if cut == 0.0 else 120) + 301:
        d = sorted(state["dts"])
        state["perf"][name] = 1000.0 * sum(d) / len(d)
        state["i"], state["t"] = i + 1, 0


unreal.register_slate_post_tick_callback(tick)
