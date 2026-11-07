"""Checks the placed town buildings in -game (works with -nullrhi): lyud.City.Report at 13:00, 22:00 and 22:00 with lyud.City.Lights 1, and for every
plot with a placed building a downward line trace 5 m inland of the plot's front (should hit the building's shell,
or the ground inside an open building) and 5 m out in front (the street: the terrain), each compared with the
plot's ground z in city_plan.json. Lines "CITYCHECK ..." in the log.

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -nullrhi -dpcvars=lyud.MainMenuOnStart=0
      "-ExecCmds=py unreal/scripts/check_city.py"
"""
import json
import math
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
with open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "City", "city_plan.json")) as f:
    PLAN = json.load(f)
FACE_Y = PLAN.get("portal_face_y", 790.0)
PLOTS = {p["id"]: p for p in PLAN["plots"]}
st = {"t": 0.0, "step": 0}


def log(m):
    unreal.log_warning("CITYCHECK " + m)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def trace(w, x, y, z):
    a, b = unreal.Vector(x, y, z + 20000.0), unreal.Vector(x, y, z - 20000.0)
    hit = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                  unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None, "nothing", ""
    t = hit.to_tuple()
    actor, comp = t[9], t[10]
    return t[4].z, actor.get_actor_label() if actor else "?", comp.get_name() if comp else "?"


def check_plots(w):
    cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudCityBuilding")
    actors = unreal.GameplayStatics.get_all_actors_of_class(w, cls)
    log("%d LyudCityBuilding actors" % len(actors))
    off_terrain = 0
    for a in actors:
        loc = a.get_actor_location()
        # the plot whose front the actor stands on (PlotId isn't readable from Python in -game)
        pid = min(PLOTS, key=lambda k: (PLOTS[k]["front"][0] * 100.0 - loc.x) ** 2
                  + (FACE_Y + PLOTS[k]["front"][1] * 100.0 - loc.y) ** 2)
        p = PLOTS.get(pid)
        if not p:
            log("%s: plot %s not in the plan" % (a.get_actor_label(), pid))
            continue
        fx, fy, fz = p["front"]
        r = math.radians(p["yaw"])
        fdir = (math.cos(r), math.sin(r))
        res = []
        for name, d in (("inland5", -5.0), ("street5", 5.0)):
            x, y = (fx + fdir[0] * d) * 100.0, FACE_Y + (fy + fdir[1] * d) * 100.0
            z, who, comp = trace(w, x, y, fz * 100.0)
            dz = (z - fz * 100.0) / 100.0 if z is not None else None
            res.append("%s z %s (plan %.1f, %s m) on %s/%s" % (
                name, "%.1f" % (z / 100.0) if z is not None else "-", fz, "%+.1f" % dz if dz is not None else "-",
                who, comp))
            if name == "street5" and (dz is None or abs(dz) > 0.5):
                off_terrain += 1
        log("%s %s at z %.1f: %s" % (pid, p.get("building"), loc.z / 100.0, "; ".join(res)))
    log("street ground off the plan z by > 0.5 m at %d of %d plots" % (off_terrain, len(actors)))


def tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 10:
        return
    s = st["step"]
    if s == 0:
        check_plots(w)
        cmd(w, "lyud.Birds.Time 13")
        log("time 13")
        st["step"], st["t0"] = 1, st["t"]
    elif s == 1 and st["t"] - st["t0"] > 5:
        cmd(w, "lyud.City.Report")
        cmd(w, "lyud.Birds.Time 22")
        log("time 22")
        st["step"], st["t0"] = 2, st["t"]
    elif s == 2 and st["t"] - st["t0"] > 5:
        cmd(w, "lyud.City.Report")
        cmd(w, "lyud.City.Lights 1")
        log("lights 1")
        st["step"], st["t0"] = 3, st["t"]
    elif s == 3 and st["t"] - st["t0"] > 3:
        cmd(w, "lyud.City.Report")
        cmd(w, "lyud.City.Lights 0")
        st["step"], st["t0"] = 4, st["t"]
    elif s == 4 and st["t"] - st["t0"] > 1:
        cmd(w, "quit")
        st["step"] = 5


unreal.register_slate_post_tick_callback(tick)
