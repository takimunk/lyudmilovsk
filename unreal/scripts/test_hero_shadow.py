"""Offscreen check of the hero's shadow (UHeroShadowMesh: the head is in it) and of the F7 developer panel taking clicks
with a held item and an E prompt on screen. Outdoors on a clear summer afternoon:

  - first person, looking down along the shadow, lyud.HeroShadowHead 1 and 0, and down at the chest (no dark patch)
  - a camera beside the hero (the body's head hidden again, as in first person), 1 and 0
  - walking and crouch-walking seen from the side; every frame the copy's bones are compared with the body's (SHADOWTEST lag)
  - F7 panel open over the held feed and a prompt: Slate-routed clicks on the hour slider, the speed slider and the
    "real time" button must reach them and move the calendar (DEVPANEL lines)

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_hero_shadow.py [warmup s]"
Screenshots land in Saved/Screenshots (shot showui), in the order of the SHADOWTEST shot lines.
"""
import sys

import unreal

WARMUP = float(sys.argv[1]) if len(sys.argv) > 1 else 45.0
OUT = [0.0, 3000.0, 200.0]  # replaced by find_open: the first spot with open sky around it
BONES = ["head", "hand_r", "foot_l", "spine_03", "lowerarm_l"]

state = {"t": 0.0, "step": 0, "lag": [], "track": False}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.log_warning("SHADOWTEST cmd %s" % c)
    unreal.SystemLibrary.execute_console_command(w, c)


def sun_yaw(w):
    best = None
    for c in unreal.ObjectIterator(unreal.DirectionalLightComponent):
        if c.get_world() != w or not c.is_visible():
            continue
        f = c.get_forward_vector()
        if f.z < -0.05 and (best is None or c.intensity > best[0]):
            best = (c.intensity, f)
    if not best:
        return 0.0, None
    f = best[1]
    return unreal.MathLibrary.make_rot_from_x(unreal.Vector(f.x, f.y, 0)).yaw, f


def trace(w, a, b):
    hit = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    if not t[0]:
        return None
    return [v for v in t if isinstance(v, unreal.Vector)][0]


def find_open(w):
    """A spot on the ground with open sky over it and 4 m around (so the shadow falls on open ground)."""
    for r in range(0, 6000, 500):
        for k in range(8 if r else 1):
            import math
            x = -193.0 + r * math.cos(k * math.pi / 4)
            y = 3000.0 + r * math.sin(k * math.pi / 4)
            ground = trace(w, unreal.Vector(x, y, 5000), unreal.Vector(x, y, -5000))
            if not ground:
                continue
            open_sky = all(trace(w, unreal.Vector(x + dx, y + dy, ground.z + 30), unreal.Vector(x + dx, y + dy, ground.z + 4000)) is None
                           for dx, dy in [(0, 0), (400, 0), (-400, 0), (0, 400), (0, -400)])
            flat = all(g and abs(g.z - ground.z) < 40 for g in
                       [trace(w, unreal.Vector(x + dx, y + dy, ground.z + 300), unreal.Vector(x + dx, y + dy, ground.z - 300))
                        for dx, dy in [(300, 0), (-300, 0), (0, 300), (0, -300)]])
            if open_sky and flat:
                OUT[:] = [x, y, ground.z + 100]
                unreal.log_warning("SHADOWTEST open spot %.0f %.0f %.0f" % tuple(OUT))
                return
    unreal.log_warning("SHADOWTEST no open spot found, staying at %s" % OUT)


def look(pitch, along_shadow=True, extra=0.0):
    def go(w):
        yaw, f = sun_yaw(w)
        pc = unreal.GameplayStatics.get_player_controller(w, 0)
        pc.set_control_rotation(unreal.Rotator(roll=0, pitch=pitch, yaw=(yaw if along_shadow else yaw + 180) + extra))
        unreal.log_warning("SHADOWTEST look pitch %.0f yaw %.0f sun %s" % (pitch, yaw, f))
    return go


def set_time(w):
    cal = [c for c in unreal.ObjectIterator(unreal.GameCalendar) if c.get_world() == w][0]
    cal.set_now(unreal.DateTime(2027, 6, 20, 16, 30, 0))
    cal.set_time_scale(1.0)


def hide_head(w):
    """Hero.SideCam shows the head; hide it again, as the first-person body has it."""
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pawn.mesh.hide_bone_by_name("head", unreal.PhysBodyOp.PBO_NONE)


def shot(name):
    def go(w):
        unreal.log_warning("SHADOWTEST shot %s" % name)
        unreal.SystemLibrary.execute_console_command(w, "shot showui")
    return go


def track(on):
    def go(w):
        state["track"] = on
    return go


def measure(w):
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    copies = pawn.get_components_by_class(unreal.HeroShadowMesh) if pawn else []
    if not copies:
        return
    worst = 0.0
    for b in BONES:
        d = pawn.mesh.get_socket_location(b) - copies[0].get_socket_location(b)
        worst = max(worst, d.length())
    state["lag"].append(worst)


def side(r, f, u):
    def go(w):
        cmd(w, "Hero.SideCam %d %d %d follow" % (r, f, u))
        hide_head(w)
    return go


PLAN = [
    (0.0, "lyud.Weather.Preset clear"),
    (0.1, set_time),
    (0.15, find_open),
    (0.2, lambda w: cmd(w, "BugItGo %.0f %.0f %.0f 0 0 0" % tuple(OUT))),
    (1.0, "Inventory.Equip bird_feed"),
    (3.0, look(-50)),
    (5.0, shot("fpv_shadow_head1")),
    (5.5, "lyud.HeroShadowHead 0"),
    (7.0, shot("fpv_shadow_head0")),
    (7.5, "lyud.HeroShadowHead 1"),
    (8.0, look(-80, False)),
    (10.0, shot("fpv_chest_head1")),
    (10.5, "lyud.HeroShadowHead 0"),
    (12.0, shot("fpv_chest_head0")),
    (12.5, "lyud.HeroShadowHead 1"),
    (13.0, look(-80, True)),
    (15.0, shot("fpv_chest_sunbehind_head1")),
    (15.5, look(0)),
    (16.0, side(-260, 120, 140)),
    (18.0, shot("side_head1")),
    (18.5, "lyud.HeroShadowHead 0"),
    (20.0, shot("side_head0")),
    (20.5, "lyud.HeroShadowHead 1"),
    # walking and crouching away along the side camera's view
    (21.0, track(True)),
    (21.1, lambda w: cmd(w, "Tame.Walk %.0f %.0f %.0f 290 0 90 0" % (OUT[0] + 400, OUT[1], OUT[2]))),
    (22.0, side(-300, 0, 140)),
    (22.5, shot("walk_1")),
    (23.0, side(-300, 0, 140)),
    (23.3, shot("walk_2")),
    (24.5, lambda w: cmd(w, "Tame.Walk %.0f %.0f %.0f 120 1 90 0" % (OUT[0] - 200, OUT[1], OUT[2]))),
    (26.0, side(-300, 0, 140)),
    (26.5, shot("crouch_walk_1")),
    (27.5, side(-300, 0, 140)),
    (27.8, shot("crouch_walk_2")),
    (29.0, "Tame.Walk off"),
    (29.1, track(False)),
    (29.2, "Hero.SideCam off"),
    # the F7 panel over the held feed and an E prompt
    (30.0, look(-10)),
    (30.5, "Inventory.Prompt E - test prompt"),
    (31.0, "Dev.Panel open"),
    (32.0, shot("f7_panel_open")),
    (33.0, "Dev.Panel click SSlider 1 0.25"),   # hour -> ~6:00
    (34.0, shot("f7_after_hour")),
    (34.5, "Dev.Panel click SSlider 0 0.5"),    # date -> early July
    (35.0, "Dev.Panel click SSlider 2 0.6"),    # speed -> 1000x
    (37.0, "Dev.Panel log"),
    (37.5, "Dev.Panel click SButton 0 0.5"),    # real time
    (38.0, "Dev.Panel log"),
    (38.5, shot("f7_after_clicks")),
    (39.0, "Dev.Panel close"),
    (39.5, "Inventory.Prompt"),
]


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += min(dt, 0.25)
    t = state["t"] - WARMUP
    if state["track"]:
        measure(w)
    while state["step"] < len(PLAN) and t > PLAN[state["step"]][0]:
        _, action = PLAN[state["step"]]
        state["step"] += 1
        if callable(action):
            action(w)
        else:
            cmd(w, action)
    if state["step"] >= len(PLAN) and t > PLAN[-1][0] + 2.0:
        lag = state["lag"]
        if lag:
            unreal.log_warning("SHADOWTEST lag frames %d max %.2f cm mean %.2f cm" % (len(lag), max(lag), sum(lag) / len(lag)))
        else:
            unreal.log_warning("SHADOWTEST lag no frames (no HeroShadowMesh?)")
        unreal.log_warning("SHADOWTEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")
        state["step"] += 1000


unreal.register_slate_post_tick_callback(tick)
