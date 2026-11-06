"""Soak helper for tools/soak.sh: once the benchmark has the camera (frame ~200, forests baked), runs the game clock at
SOAK_SCALE (default 60: an in-game hour a minute) so one run sees day, dusk, night and dawn, and every SOAK_EVERY
seconds (default 60) logs SOAK lines: wall time, game time, frames and mean frame ms over the interval, live UObject
counts of a few classes that leak if anything does (actors, components, MIDs, textures, sounds)."""
import os
import unreal

SCALE = float(os.environ.get("SOAK_SCALE", "60"))
EVERY = float(os.environ.get("SOAK_EVERY", "60"))
s = {"t": 0.0, "last": 0.0, "frames": 0, "acc": 0.0, "scaled": False}
CLASSES = [("actors", unreal.Actor), ("components", unreal.ActorComponent), ("mids", unreal.MaterialInstanceDynamic),
           ("static_meshes", unreal.StaticMesh), ("textures", unreal.Texture2D), ("rts", unreal.TextureRenderTarget2D),
           ("audio", unreal.AudioComponent)]


def world():
    return unreal.UnrealEditorSubsystem().get_game_world()


def count(cls):
    n = 0
    for _ in unreal.ObjectIterator(cls):
        n += 1
    return n


def tick(dt):
    s["t"] += dt
    s["frames"] += 1
    s["acc"] += dt
    w = world()
    if not w:
        return
    if not s["scaled"] and s["t"] > 120.0:
        cals = [c for c in unreal.ObjectIterator(unreal.GameCalendar) if c.get_world() == w]
        if cals:
            cals[0].set_time_scale(SCALE)
            s["scaled"] = True
            unreal.log_warning("SOAK clock x%.0f from %s" % (SCALE, cals[0].now()))
    if s["t"] - s["last"] >= EVERY:
        cals = [c for c in unreal.ObjectIterator(unreal.GameCalendar) if c.get_world() == w]
        counts = " ".join("%s=%d" % (n, count(c)) for n, c in CLASSES)
        unreal.log_warning("SOAK t=%.0f game=%s frames=%d mean_ms=%.2f %s" % (
            s["t"], cals[0].now() if cals else "?", s["frames"], 1000.0 * s["acc"] / max(1, s["frames"]), counts))
        s["last"] = s["t"]; s["frames"] = 0; s["acc"] = 0.0


unreal.register_slate_post_tick_callback(tick)
unreal.log_warning("SOAK start")
