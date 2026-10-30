"""Offscreen check of the F7 developer panel's weather section, its languages, the H keys list and F10 for the menu.
Outdoors at noon in March: the panel in ru, en and ja (shots), then in English Slate-routed clicks on presets, weather
types, phenomena check boxes, a value slider, the lightning and star-flare buttons and Auto; every click logs the
weather before and after (DEVPANEL lines), and a shot follows the ones worth seeing.

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1920 -ResY=1080 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_devpanel.py [warmup s]"
unreal/scripts/test_devpanel.sh runs it and checks the log. Shots land in Saved/Screenshots (shot showui), in the
order of the DEVTEST shot lines.
"""
import math
import sys

import unreal

WARMUP = float(sys.argv[1]) if len(sys.argv) > 1 else 45.0
state = {"t": 0.0, "step": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.log_warning("DEVTEST cmd %s" % c)
    unreal.SystemLibrary.execute_console_command(w, c)


def shot(name):
    def go(w):
        unreal.log_warning("DEVTEST shot %s" % name)
        unreal.SystemLibrary.execute_console_command(w, "shot showui")
    return go


def trace(w, a, b):
    hit = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return [v for v in t if isinstance(v, unreal.Vector)][0] if t[0] else None


def go_outside(w):
    """Noon on 10 March, standing on open ground (sky all round) looking out level."""
    cal = [c for c in unreal.ObjectIterator(unreal.GameCalendar) if c.get_world() == w][0]
    cal.set_now(unreal.DateTime(2027, 3, 10, 12, 0, 0))
    cal.set_time_scale(1.0)
    unreal.SystemLibrary.execute_console_command(w, "lyud.Weather.BlendSeconds 0")
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    for r in range(0, 6000, 500):
        for k in range(8 if r else 1):
            x = -193.0 + r * math.cos(k * math.pi / 4)
            y = 3000.0 + r * math.sin(k * math.pi / 4)
            ground = trace(w, unreal.Vector(x, y, 5000), unreal.Vector(x, y, -5000))
            if ground and trace(w, unreal.Vector(x, y, ground.z + 30), unreal.Vector(x, y, ground.z + 4000)) is None:
                pawn.teleport(unreal.Vector(x, y, ground.z + 100), unreal.Rotator(0, 0, 0))
                unreal.GameplayStatics.get_player_controller(w, 0).set_control_rotation(unreal.Rotator(roll=0, pitch=8, yaw=200))
                unreal.log_warning("DEVTEST outside %.0f %.0f %.0f" % (x, y, ground.z))
                return
    unreal.log_warning("DEVTEST outside: no open spot")


def lang(code, t):
    """Reopen the panel in a language (its words are taken when it opens) and shoot it and the keys list."""
    return [
        (t, "Dev.Panel close"),
        (t + 0.2, "lyud.Language %s" % code),
        (t + 0.5, "Dev.Panel open"),
        (t + 0.6, "Dev.Panel help"),
        (t + 1.5, shot("panel_%s" % code)),
        (t + 2.0, "Dev.Panel help"),
    ]


def click(key, t, n=0, snap=None, wait=2.5):
    steps = [(t, "Dev.Panel clickkey %s %d" % (key, n))]
    if snap:
        steps.append((t + wait, shot(snap)))
    return steps


PLAN = [(0.0, go_outside), (0.5, "lyud.Weather.Preset clear")]
PLAN += lang("ru", 3.0) + lang("en", 6.0) + lang("ja", 9.0)
t = 12.0
PLAN += [(t, "Dev.Panel close"), (t + 0.2, "lyud.Language en"), (t + 0.5, "Dev.Panel open")]
t += 1.0
# presets (the preset row's button comes first where a check box has the same word)
for name, snap in [("snow", "wx_snow"), ("blizzard", "wx_blizzard"), ("fog", "wx_fog"), ("thunder", None), ("rain", "wx_rain"),
                   ("overcast", None), ("lenticular", "wx_lenticular"), ("halo", "wx_halo"), ("rainbow", None), ("frost", "wx_frost"),
                   ("clear", None)]:
    PLAN += click("ui.dev.wx.preset." + name, t, 0, snap)
    t += 3.0 if snap else 1.0
PLAN += click("ui.dev.wx.ph.lightning", t, 0, "wx_lightning", 0.15)
t += 1.5
PLAN += click("ui.dev.wx.ph.sunflare", t)
t += 1.0
# weather types
for name in ["coldfront", "thunderylow", "arctichigh"]:
    PLAN += click("ui.dev.wx.regime." + name, t)
    t += 1.0
PLAN += click("ui.dev.wx.auto", t)
t += 1.0
# phenomena on top: every check box on, a shot, then each off again (index 1 where a preset button has the same word)
TOGGLES = [("thunder", 1), ("lenticular", 1), ("fog", 1), ("aurora", 1), ("halo", 1), ("rainbow", 1), ("nlc", 0), ("flare", 0),
           ("activestar", 0), ("snowcover", 0), ("drifts", 0), ("canopy", 0), ("seaice", 0), ("lowwater", 0)]
for name, n in TOGGLES:
    PLAN += click("ui.dev.wx.ph." + name, t, n)
    t += 0.6
PLAN += [(t + 2.0, shot("wx_all_phenomena")), (t + 2.5, "Dev.Panel log")]
t += 3.0
for name, n in TOGGLES:
    PLAN += click("ui.dev.wx.ph." + name, t, n)
    t += 0.6
PLAN += [(t, "Dev.Panel log")]
t += 1.0
# fold the world section so the values are on a 720-line screen too
PLAN += click("ui.dev.world", t, 0, "panel_world_folded", 1.0)
t += 2.0
# a value: precipitation to the middle, then back to auto at the far left (sliders: date, hour, speed, then the weather's)
PLAN += [(t, "Dev.Panel click SSlider 4 0.5"), (t + 2.0, shot("wx_rain_slider")), (t + 2.5, "Dev.Panel click SSlider 4 0.01")]
t += 3.5
# the change easing in over a minute, then Auto hands it back
PLAN += [(t, "Dev.Panel click SSlider 8 0.5"), (t + 0.5, "Dev.Panel clickkey ui.dev.wx.preset.downpour 0"),
         (t + 1.5, "Dev.Panel log"), (t + 2.0, "Dev.Panel click SSlider 8 0.0")]
t += 3.0
PLAN += click("ui.dev.wx.auto", t, 0, "wx_auto")
t += 3.0
PLAN += click("ui.dev.world", t)
t += 1.0
# the game menu on F10 (the editor's play stops on Esc, so F10 is the key there), the panel closed first
PLAN += [(t, "Dev.Panel close"), (t + 0.5, "Dev.Panel key F10"), (t + 1.5, shot("menu_f10")), (t + 2.0, "Dev.Panel key F10"),
         (t + 3.0, "Dev.Panel log")]


def tick(dt):
    w = world()
    if not w:
        return
    state["t"] += min(dt, 0.25)
    now = state["t"] - WARMUP
    while state["step"] < len(PLAN) and now > PLAN[state["step"]][0]:
        _, action = PLAN[state["step"]]
        state["step"] += 1
        if callable(action):
            action(w)
        else:
            cmd(w, action)
    if state["step"] >= len(PLAN) and now > PLAN[-1][0] + 2.0:
        unreal.log_warning("DEVTEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")
        state["step"] += 1000


unreal.register_slate_post_tick_callback(tick)
