"""Offscreen -game shots of /Game/Dev/L_HeroPigeon (make_hero_pigeon_level.py): every camera, then the first-person
check: the FPV bird in SetFirstPerson with SetWingPose through a wingbeat, the camera on its eye (GetEyeTransform).
PIGEON_SHOTS is the folder.
  UnrealEditor <uproject> /Game/Dev/L_HeroPigeon -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 "-ExecCmds=py <this file>"
"""
import os

import unreal

SHOTS = os.environ.get("PIGEON_SHOTS", "/tmp/pigeon_shots")
WARMUP = int(os.environ.get("PIGEON_WARMUP", "600"))
HOLD = 60
FPV = [("glide", 0.0, 0.0), ("up", 0.0, 1.0), ("mid_down", 0.25, 1.0), ("bottom", 0.5, 1.0), ("mid_up", 0.75, 1.0), ("half_fold", -1, 0.0)]
st = {"t": 0, "i": 0, "cams": None, "fpv": None, "phase": 0}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_HeroPigeon" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w
    return None


def find(w, label):
    return next((a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor) if a.get_actor_label() == label), None)


def tick(dt):
    st["t"] += 1
    w = world()
    if w is None:
        return
    if st["cams"] is None:
        if st["t"] < WARMUP:
            return
        cams = [c for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor) if not c.get_actor_label().startswith("FPV")]
        if os.environ.get("PIGEON_ONLY_FPV"):
            cams = []
        st["cams"] = sorted(cams, key=lambda c: c.get_actor_label())
        st["fpv"] = find(w, "FPV")
        st["t"] = 0
        os.makedirs(SHOTS, exist_ok=True)
        unreal.log_warning("PIGEONSHOT %d cameras" % len(st["cams"]))
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    t = st["t"]
    i = st["i"]
    cams = st["cams"]
    b = st["fpv"]
    if i < len(cams):
        if t == 1:
            pc.set_view_target_with_blend(cams[i], 0.0)
        if t == HOLD:
            unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/%s.png" % (SHOTS, cams[i].get_actor_label()))
        if t == HOLD + 15:
            st["i"] += 1
            st["t"] = 0
        return
    k = i - len(cams)
    if k >= len(FPV) * 3:
        unreal.SystemLibrary.execute_console_command(w, "QUIT")
        return
    name, phase, amp = FPV[k % len(FPV)]
    inside = k < len(FPV)
    view = "in" if inside else ("out" if k < 2 * len(FPV) else "side")
    spread = 0.5 if phase < 0 else 1.0
    b.set_wing_pose(spread, spread, max(phase, 0.0), max(phase, 0.0), amp, 0.0, 0.0, 0.0)
    # the flight thread squeezes its first-person lens so the wings beside the eye come into the screen's edges; without
    # that lens this check sweeps them further forward instead
    b.set_editor_property("view_sweep_deg", float(os.environ.get("PIGEON_FPV_SWEEP", "75")))
    b.set_first_person(inside)
    c = find(w, {"in": "FPVCam", "out": "FPVOut", "side": "FPVSide"}[view])
    if inside:
        c.set_actor_transform(b.get_eye_transform(), False, False)
    if t == 1:
        pc.set_view_target_with_blend(c, 0.0)
    if t == HOLD:
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/fpv_%s_%s.png" % (SHOTS, view, name))
        sk = b.get_editor_property("body")
        rel = lambda bn: b.get_actor_transform().inverse_transform_location(sk.get_socket_location(bn))  # noqa: E731
        unreal.log_warning("PIGEONSHOT fpv %s %s tips L %s R %s hand L %s R %s" % (name, view, rel("p10_L"), rel("p10_R"), rel("hand_L"), rel("hand_R")))
    if t == HOLD + 15:
        st["i"] += 1
        st["t"] = 0


unreal.register_slate_post_tick_callback(tick)
