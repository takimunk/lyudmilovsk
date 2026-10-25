"""Offscreen check of the menu's Settings page: opens it, moves the music volume (SETTINGSTEST lines show the soundscape got it),
switches the language to English and Japanese and back, hides the HUD; a screenshot each (docs/ui.md).

  UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_settings.py"
"""
import unreal

state = {"t": 0.0, "step": 0}
PLAN = [
    (10.0, "lyud.Menu settings", None),
    (11.5, "shot showui", "settings_ru"),
    (12.0, "lyud.Settings.Volume music 0.2", None),
    (12.5, "lyud.Settings.Volume master 0.8", None),
    (13.0, "lyud.Menu close", None),
    (13.5, "lyud.Settings.Language en", None),
    (14.0, "lyud.Menu settings", None),
    (15.5, "shot showui", "settings_en"),
    (16.0, "lyud.Menu close", None),
    (16.5, "lyud.Settings.Language ja", None),
    (17.0, "lyud.Menu settings", None),
    (18.5, "shot showui", "settings_ja"),
    (19.0, "lyud.Menu close", None),
    (19.5, "lyud.Settings.Language ru", None),
    (20.0, "lyud.Settings.Volume music 0.45", None),
    (20.2, "lyud.Settings.Volume master 1", None),
    (20.5, "lyud.Hud 0", None),
    (22.0, "shot showui", "hud_off"),
    (22.5, "lyud.Hud 1", None),
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
        _, cmd, name = PLAN[state["step"]]
        state["step"] += 1
        unreal.log_warning("SETTINGSTEST %s" % (("shot " + name) if name else cmd))
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("SETTINGSTEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
