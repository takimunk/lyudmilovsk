"""Offscreen check of the beach at play (docs/beach.md) by a lifeguard tower: the shore under the hero, a skipping
stone, a sandcastle the raised tide takes, the waterline's finds after a storm, the swim's post and buoy. No PIE, no
input, the desktop is left alone (unreal/scripts/shoot_beach.sh runs it through the UE queue):

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py <this file>"

BEACHSHOT lines say what each step did; the console's BEACH lines are the subsystem's own. Screenshots ("shot showui":
the prompt and the HUD are in them) in Saved/Screenshots in the order of the steps. BEACH_X/BEACH_Y: where to start
looking for the wet sand (world cm; the sea is toward +X).
"""
import math
import os

import unreal

X0 = float(os.environ.get("BEACH_X", "15400"))
Y0 = float(os.environ.get("BEACH_Y", "86800"))
LO = float(os.environ.get("BEACH_LO", "3"))
HI = float(os.environ.get("BEACH_HI", "12"))
st = {"t": 0.0, "step": 0, "t0": 0.0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def log(s):
    unreal.log_warning("BEACHSHOT " + s)


def actor(w, cls):
    a = unreal.GameplayStatics.get_all_actors_of_class(w, cls)
    return a[0] if a else None


def height_over_water(sea, x, y):
    """cm the ground at x, y stands over the water now (negative: under water)."""
    return -sea.depth_at_level(unreal.Vector(x, y, 0.0), sea.sea_level())


def wet_sand(sea, y):
    """The first x (from X0 toward the sea) where the ground is BEACH_LO..BEACH_HI cm over the water (the waterline)."""
    for k in range(0, 400):
        x = X0 + k * 50.0
        h = height_over_water(sea, x, y)
        if LO <= h <= HI:
            return x, sea.sea_level() + h
        if h < LO:
            return x - 50.0, sea.sea_level() + height_over_water(sea, x - 50.0, y)
    return None, None


def props(w):
    """The beach's shapes: the plain Actor whose shapes include the skipping stone (a Sphere 0.07 x 0.06 x 0.018,
    ULyudBeach::MakeProps)."""
    if st.get("props"):
        return st["props"]
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        if a.get_class() != unreal.Actor.static_class():
            continue
        comps = a.get_components_by_class(unreal.StaticMeshComponent)
        for c in comps:
            m = c.static_mesh
            sc = c.get_editor_property("relative_scale3d")
            if m and m.get_name() == "Sphere" and abs(sc.x - 0.07) < 0.005 and abs(sc.z - 0.018) < 0.004:
                st["props"] = comps
                return comps
    return []


def census(w):
    seen = {}
    for c in props(w):
        m = c.static_mesh
        sc = c.get_editor_property("relative_scale3d")
        k = (m.get_name() if m else None, round(sc.x, 3), round(sc.z, 3), c.is_visible())
        seen[k] = seen.get(k, 0) + 1
    log("census %s" % sorted(seen.items(), key=lambda kv: str(kv[0])))


def shapes(w, mesh, scale):
    out = []
    for c in props(w):
        m = c.static_mesh
        s = c.get_editor_property("relative_scale3d")
        if m and m.get_name() == mesh and abs(s.x - scale[0]) < 0.02 and abs(s.z - scale[2]) < 0.05 and c.is_visible():
            out.append(c.get_world_location())
    return out


def place(w, x, y, z, yaw, pitch):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if hasattr(pawn, "character_movement"):
        pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    pawn.set_actor_location(unreal.Vector(x, y, z + 100.0), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def look_at(w, frm, to):
    d = to - frm
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))


def shot(w, name):
    log("shot %s" % name)
    cmd(w, "shot showui")


# (seconds after the step before, what to do)
def steps():
    return [
        (1.0, "clock"), (4.0, "start"), (4.0, "stand"), (2.0, "throw"), (0.7, "shot:throw_a"), (0.8, "shot:throw_b"), (1.5, "shot:throw_c"),
        (5.0, "info"), (0.5, "castle"), (1.0, "castle_view"), (3.0, "shot:castle"), (0.5, "tide_up"), (75.0, "shot:castle_washed"), (0.5, "tide_off"),
        (3.0, "storm"), (0.5, "walk"), (0.0, "post"), (6.0, "shot:post"), (0.5, "buoy"), (5.0, "shot:buoy"), (2.0, "end"),
    ]


def tick(dt):
    try:
        step(dt)
    except Exception as e:                            # never hang offscreen on a script error
        unreal.log_error("BEACHSHOT script error: %r" % e)
        w = world()
        if w:
            cmd(w, "quit")
        unreal.unregister_slate_post_tick_callback(handle)


def step(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 10.0:
        return
    sea = actor(w, unreal.LyudSea)
    S = steps()
    if st["step"] >= len(S):
        return
    if st.get("walking"):
        walk(w, sea)
        return
    wait, what = S[st["step"]]
    if st["t"] - st["t0"] < wait:
        return
    st["t0"] = st["t"]
    st["step"] += 1
    if what == "clock":
        cmd(w, "lyud.Weather.Preset clear")
        cmd(w, "lyud.Weather.Cloud 0")
        cmd(w, "lyud.Birds.Time 13")
        cmd(w, "lyud.Beach.Reset")
        cmd(w, "Inventory.Equip none")     # hands empty: the beach needs them (the bird feed is in hand in a new game)
    elif what == "start":
        x, z = wet_sand(sea, Y0)
        log("sea level %.0f, datum %.0f, wet line %.0f; wet sand at x %s z %s" % (sea.sea_level(), sea.datum_z(), sea.wet_line_z(), x, z))
        st["x"], st["z"] = x or X0, z or sea.sea_level()
        place(w, st["x"], Y0, st["z"], 0.0, -6.0)
    elif what == "stand":
        pawn = unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()
        log("hero at %s on ground %s; props shapes %d" % (pawn.get_actor_location(), pawn.character_movement.is_moving_on_ground(), len(props(w))))
        cmd(w, "lyud.Beach.Info")
        shot(w, "stand")
    elif what == "throw":
        cmd(w, "lyud.Beach.Stone 0.95")
        pc = unreal.GameplayStatics.get_player_controller(w, 0)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=-4.0, yaw=0.0))
        cmd(w, "lyud.Beach.Throw 0.9")
    elif what.startswith("shot:"):
        shot(w, what[5:])
        if what == "shot:castle_washed":
            cmd(w, "lyud.Beach.Info")
    elif what == "info":
        cmd(w, "lyud.Beach.Info")
    elif what == "castle":
        pc = unreal.GameplayStatics.get_player_controller(w, 0)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=-30.0, yaw=0.0))
        cmd(w, "lyud.Beach.Castle 6")
        cmd(w, "lyud.Beach.Info")
    elif what == "castle_view":
        pawn = unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn()
        p = pawn.get_actor_location()
        place(w, p.x - 220.0, p.y - 120.0, p.z - 100.0, 0.0, 0.0)
        look_at(w, unreal.Vector(p.x - 220.0, p.y - 120.0, p.z + 65.0), unreal.Vector(p.x + 80.0, p.y, p.z - 90.0))
    elif what == "tide_up":
        # the water 60 cm over the castle's foot (the hero stays; a step back to the land so he does not swim)
        level_m = (st["z"] + 60.0 - sea.datum_z()) / 100.0
        place(w, st["x"] - 600.0, Y0, st["z"] + 60.0, 0.0, -14.0)
        cmd(w, "lyud.TideOverride %.2f" % level_m)
        log("tide pinned at %.2f m over the datum" % level_m)
    elif what == "tide_off":
        cmd(w, "lyud.TideOverride -999")
    elif what == "storm":
        census(w)
        cmd(w, "lyud.Beach.Storm 1")
        st["walking"] = True
        st["walk_i"] = 0
        st["walk_t"] = st["t"]
        st["finds"] = []
    elif what == "post":
        posts = shapes(w, "Cylinder", (0.08, 0.08, 1.3))
        buoys = shapes(w, "Sphere", (0.7, 0.7, 0.55))
        log("posts shown %d %s; buoys shown %d %s" % (len(posts), [tuple(round(v) for v in (p.x, p.y, p.z)) for p in posts],
                                                    len(buoys), [tuple(round(v) for v in (b.x, b.y, b.z)) for b in buoys]))
        if not posts:
            posts = [unreal.Vector(16339.5, 86943.3, -4328.5 + 65.0)]
            buoys = [unreal.Vector(33339.5, 86943.3, sea.surface_z(unreal.Vector(33339.5, 86943.3, 0.0)))]
            log("posts from the log: %s, buoy %s" % (posts[0], buoys[0]))
        st["posts"], st["buoys"] = posts, buoys
        if posts:
            p = min(posts, key=lambda p: (p.x - X0) ** 2 + (p.y - Y0) ** 2)
            place(w, p.x - 400.0, p.y - 250.0, p.z - 130.0, 0.0, 0.0)
            look_at(w, unreal.Vector(p.x - 400.0, p.y - 250.0, p.z + 40.0), p)
            st["post"] = p
        cmd(w, "lyud.Beach.Info")
    elif what == "buoy":
        if st.get("buoys") and st.get("post"):
            p = st["post"]
            b = min(st["buoys"], key=lambda b: (b.x - p.x) ** 2 + (b.y - p.y) ** 2)
            # from the post's foot toward the buoy (the hero stands where the post is)
            place(w, p.x - 200.0, p.y, p.z - 65.0, 0.0, 0.0)
            eye = unreal.Vector(p.x - 200.0, p.y, p.z + 100.0)
            look_at(w, eye, b)
            cmd(w, "fov 12")
            log("buoy %.0f m from its post" % (math.hypot(b.x - p.x, b.y - p.y) / 100.0))
    elif what == "end":
        cmd(w, "fov 0")
        cmd(w, "lyud.Beach.Info")
        cmd(w, "quit")


def walk(w, sea):
    """Along the waterline 8 m at a time (a cell each), 12 cells; a shot at the first find seen near."""
    if st["t"] - st["walk_t"] < 1.2:
        return
    st["walk_t"] = st["t"]
    i = st["walk_i"]
    if i >= 12:
        st["walking"] = False
        st["t0"] = st["t"]
        return
    if i % 100 == 0 and st.get("find_shot"):
        pass
    y = Y0 - 48.0 * 100.0 + i * 800.0
    x, z = wet_sand(sea, y)
    if x is None:
        st["walk_i"] += 1
        return
    place(w, x - 300.0, y, z + 20.0, 0.0, -20.0)
    cmd(w, "lyud.Beach.Info")
    finds = [c.get_world_location() for c in props(w) if c.static_mesh and c.static_mesh.get_name() == "Sphere" and c.is_visible()
             and c.get_editor_property("relative_scale3d").x < 0.3 and abs(c.get_editor_property("relative_scale3d").x - 0.07) > 0.004]
    if finds and not st.get("find_shot"):
        f = min(finds, key=lambda f: (f.x - x) ** 2 + (f.y - y) ** 2)
        hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(f.x, f.y, f.z + 300.0), unreal.Vector(f.x, f.y, f.z - 300.0),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        gz = hit.to_tuple()[5].z if hit else None
        log("finds visible %d; nearest at %s; visibility trace ground z %s; sea's ground z %.1f" % (
            len(finds), (round(f.x), round(f.y), round(f.z, 1)), gz, sea.sea_level() - sea.depth_at_level(f, sea.sea_level())))
        cmd(w, "r.MotionBlurQuality 0")
        place(w, f.x - 120.0, f.y, f.z, 0.0, 0.0)
        look_at(w, unreal.Vector(f.x - 120.0, f.y, f.z + 165.0), f)
        cmd(w, "fov 30")
        st["find_shot"] = "pending"
        st["walk_t"] = st["t"] + 2.0
        return
    if st.get("find_shot") == "pending":
        st["find_shot"] = "done"
        shot(w, "find")
        cmd(w, "fov 0")
    st["walk_i"] += 1


handle = unreal.register_slate_post_tick_callback(tick)
