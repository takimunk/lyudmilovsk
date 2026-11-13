"""Offscreen check of the menu's Graphics page and the lyud.Gfx.* commands (docs/graphics.md): the page in Russian,
English and Japanese, then Low, a custom mix (one group, a fixed resolution, a frame limit) and back to Auto, with a
screenshot of the page at each step. Every lyud.Gfx.* command logs a GFXTEST line with what the settings come to.

  tools/wait_ue_queue.sh && UnrealEditor <uproject> /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen -ResX=1600 -ResY=900
      -windowed -nosplash -dpcvars=lyud.MainMenuOnStart=0 -abslog=<log> "-ExecCmds=py unreal/scripts/test_graphics.py"

Offscreen runs start at the default levels and do not store the page's settings (docs/graphics.md, "What runs where"),
so the run leaves GameUserSettings.ini's [Lyudmilovsk.Graphics] alone; the language goes back to Russian at the end.
The screenshots land in Saved/Screenshots/<platform>/ in the order of the "GFXTEST shot" lines.
"""
import unreal

state = {"t": 0.0, "step": 0}
PLAN = [
    (10.0, "lyud.Gfx", None),
    (10.5, "lyud.Menu graphics", None),
    (12.0, "shot showui", "gfx_ru"),
    (12.5, "lyud.Menu close", None),
    (13.0, "lyud.Settings.Language en", None),
    (13.5, "lyud.Menu graphics", None),
    (15.0, "shot showui", "gfx_en"),
    (15.5, "lyud.Menu close", None),
    (16.0, "lyud.Settings.Language ja", None),
    (16.5, "lyud.Menu graphics", None),
    (18.0, "shot showui", "gfx_ja"),
    (18.5, "lyud.Menu close", None),
    (19.0, "lyud.Settings.Language ru", None),
    # a level reaches one forest and one grass patch a frame, and the pause menu stops them: change it with the menu shut
    (19.5, "lyud.Gfx.Preset 0", None),
    (25.0, "lyud.Menu graphics", None),
    (26.5, "shot showui", "gfx_low"),
    (27.0, "lyud.Menu close", None),
    (27.5, "lyud.Gfx.Group shadow 3", None),
    (28.0, "lyud.Gfx.Resolution 67", None),
    (28.5, "lyud.Gfx.FrameLimit 30", None),
    (29.0, "lyud.Menu graphics", None),
    (30.5, "shot showui", "gfx_custom"),
    (31.0, "lyud.Menu close", None),
    (31.5, "lyud.Gfx.Resolution 0", None),
    (32.0, "lyud.Gfx.FrameLimit 60", None),
    (32.5, "lyud.Gfx.Preset auto", None),
    (38.0, "lyud.Menu graphics", None),
    (39.5, "shot showui", "gfx_auto"),
    (40.0, "lyud.Menu close", None),
    (40.5, "lyud.Gfx", None),
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
        unreal.log_warning("GFXTEST step %s" % (("shot " + name) if name else cmd))
        unreal.SystemLibrary.execute_console_command(w, cmd)
    if state["step"] >= len(PLAN) and state["t"] > PLAN[-1][0] + 2.0:
        unreal.log_warning("GFXTEST done")
        unreal.SystemLibrary.execute_console_command(w, "quit")


unreal.register_slate_post_tick_callback(tick)
