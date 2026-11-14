"""Offscreen check of Topi's sequences and their save (docs/topi.md); no PIE, no input. Lines TOPITEST in the log.

  tools/wait_ue_queue.sh && LOG=<log> tools/play_offscreen.sh /Game/Scene01/Lvl_Scene01 unreal/scripts/test_topi.py

1. the props: lyud.Topi.Props (TOPIPROPS: plates, stakes, the Ural sign, towels)
2. on a bike: the hero is put by the nearest bike and mounts it (Bike.Hero mount), then lyud.Topi.Dissolve: he must get
   off first (T1), then the dive starts; ~13 s later he must wake at PlayerStart_WakeUp with TimesDissolved 1
3. the save: lyud.Save, lyud.Load 0; after the load TimesDissolved must still be 1 (T2)
4. developer mode: Dev.Do topi.turn (needs Topi's sand under him: put at topi_dunes by day) and Dev.Do topi.dissolve
5. his thoughts: at night on the path (line.hero.topi.glow.1), then at the bank (river.1 or towels.1): LyudTopi: says lines
"""
import unreal

st = {"t": 0.0, "step": 0, "t0": 0.0, "loads": 0, "world": None}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name().startswith("Lvl_") and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def say(msg):
    unreal.log_warning("TOPITEST " + msg)


def topi(w):
    cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudTopi")
    found = unreal.GameplayStatics.get_all_actors_of_class(w, cls) if cls else []
    return found[0] if found else None


def times(w):
    t = topi(w)
    if not t:
        return None
    mind = t.get_editor_property("mind")
    return mind.get_editor_property("times_dissolved") if mind else None


def wake_start(w):
    for s in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart):
        if "WakeUp" in s.get_name() or "WakeUp" in str(s.get_editor_property("player_start_tag")):
            return s


def nearest_bike(w, at):
    cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudBike")
    bikes = unreal.GameplayStatics.get_all_actors_of_class(w, cls) if cls else []
    return min(bikes, key=lambda b: (b.get_actor_location() - at).length()) if bikes else None


def to_map(v):
    return v.x / 100.0, (v.y - 790.0) / 100.0


# (seconds after the step began, next step) is handled in tick
def tick(dt):
    st["t"] += dt
    w = world()
    if not w:
        return
    if w != st["world"]:
        st["world"] = w
        st["loaded_at"] = st["t"]
        st["loads"] += 1
        say("world %s (load %d)" % (w.get_name(), st["loads"]))
    if st["t"] - st["loaded_at"] < 8.0:
        return
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn() if pc else None
    if not pawn:
        return
    el = st["t"] - st["t0"]
    s = st["step"]

    def next_step():
        st["step"] += 1
        st["t0"] = st["t"]

    if s == 0:
        cmd(w, "t.MaxFPS 0")
        cmd(w, "lyud.Topi.Props")
        say("times at start %s" % times(w))
        bike = nearest_bike(w, pawn.get_actor_location())
        if bike:
            loc = bike.get_actor_location()
            pawn.set_actor_location(loc + unreal.Vector(0, -120, 60), False, True)
            say("bike %s at %s" % (bike.get_name(), loc))
        else:
            say("no bike in the level")
        next_step()
    elif s == 1 and el > 2.0:
        cmd(w, "Bike.Hero mount")
        next_step()
    elif s == 2 and el > 4.0:
        cmd(w, "Bike.Report")
        cmd(w, "lyud.Birds.Time 23 30")
        cmd(w, "lyud.Topi.Night 1")
        cmd(w, "lyud.Topi.Dissolve")      # on the bike: must ask him off first
        say("dissolve asked while mounted, hero at %s" % pawn.get_actor_location())
        next_step()
    elif s == 3 and el > 3.0:
        cmd(w, "Bike.Report")
        cmd(w, "lyud.Topi.Dissolve")      # off by now: the dive starts
        say("dissolve asked again, hero at %s" % pawn.get_actor_location())
        next_step()
    elif s == 4 and el > 16.0:
        ws = wake_start(w)
        d = (pawn.get_actor_location() - ws.get_actor_location()).length() if ws else -1
        say("after the dive: hero %s, %.0f cm from %s, times %s" % (pawn.get_actor_location(), d, ws.get_name() if ws else None, times(w)))
        cmd(w, "lyud.Save topitest")
        next_step()
    elif s == 5 and el > 3.0:
        st["times_before_load"] = times(w)
        cmd(w, "lyud.Load 0")
        next_step()
    elif s == 6 and st["loads"] >= 2 and el > 2.0:
        say("after the load: times %s (before %s)" % (times(w), st.get("times_before_load")))
        cmd(w, "lyud.Birds.Time 14 0")
        cmd(w, "lyud.Topi.Night 0")
        cmd(w, "Dev.Tp topi_dunes")
        next_step()
    elif s == 7 and el > 6.0:
        x, y = to_map(pawn.get_actor_location())
        say("at topi_dunes: map %.0f %.0f" % (x, y))
        cmd(w, "lyud.Topi.Report")
        cmd(w, "Dev.Do topi.turn")
        next_step()
    elif s == 8 and el > 3.0:
        cmd(w, "lyud.Topi.Report")
        next_step()
    elif s == 9 and el > 8.0:
        x, y = to_map(pawn.get_actor_location())
        say("after the turn: map %.0f %.0f" % (x, y))
        cmd(w, "Dev.Tp ural_bank")
        next_step()
    elif s == 10 and el > 6.0:
        cmd(w, "Dev.Do topi.night")
        cmd(w, "Dev.Do topi.dissolve")
        next_step()
    elif s == 11 and el > 16.0:
        ws = wake_start(w)
        d = (pawn.get_actor_location() - ws.get_actor_location()).length() if ws else -1
        say("after Dev.Do topi.dissolve: %.0f cm from the wake-up start, times %s" % (d, times(w)))
        # 5. his thoughts (LyudTopi: says ... lines in the log): the glow on the path at night, then the river / towels
        cmd(w, "lyud.Birds.Time 23 30")
        cmd(w, "lyud.Topi.Night 1")
        cmd(w, "lyud.Topi.Calm 1")
        cmd(w, "Dev.Tp topi_path")
        next_step()
    elif s == 12 and el > 12.0:
        say("thoughts: on the path (expect glow.1 said)")
        cmd(w, "Dev.Tp ural_bank")
        next_step()
    elif s == 13 and el > 12.0:
        say("thoughts: at the bank (expect river.1 or towels.1 said)")
        cmd(w, "quit")
        next_step()


unreal.register_slate_post_tick_callback(tick)
