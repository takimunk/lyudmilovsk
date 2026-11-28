"""In-game check shots of imported birds on /Game/Dev/L_BirdSpecies (make_bird_species_level.py <Ids>), offscreen -game:
perch close-up, walking and pecking frames, flight from above with SetWingPose at 1.0 / 0.5 / 0.1, the body at 4 / 15 /
50 m (LODs), and the hero body in first person (SetFirstPerson) looking ahead and down at itself.
BIRD_SHOTS = folder, BIRD_IDS = comma-separated species.
  UnrealEditor <uproject> /Game/Dev/L_BirdSpecies -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed
      -dpcvars=lyud.MainMenuOnStart=0 "-ExecCmds=py <this file>"
"""
import json
import math
import os

import unreal

SHOTS = os.environ.get("BIRD_SHOTS", "/tmp/bird_check")
IDS = [s for s in os.environ.get("BIRD_IDS", "Rook").split(",") if s]
WARMUP = int(os.environ.get("BIRD_WARMUP", "400"))
HOLD = 80
TABLE = {s["id"]: s for s in json.load(open(os.path.join(unreal.Paths.project_content_dir(), "BirdsData", "species.json")))["species"]}
st = {"t": 0, "i": 0, "steps": None, "cam": None}


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "L_BirdSpecies" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w
    return None


def find(w, label):
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        if a.get_actor_label() == label:
            return a
    return None


def look(cam, loc, target, fov):
    d = target - loc
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    cam.set_actor_location_and_rotation(loc, unreal.Rotator(0, pitch, yaw), False, False)
    cam.camera_component.set_editor_property("field_of_view", fov)


def steps_for(w, sid):
    P, F, H = find(w, "Perched_" + sid), find(w, "Flying_" + sid), find(w, "Hero_" + sid)
    L = TABLE[sid]["lengthCm"]
    span = TABLE[sid]["wingspanCm"]
    out = []
    if P:
        p = P.get_actor_location()
        c = p + unreal.Vector(0, 0, L * 0.35)
        out.append(("perch_close", lambda: None, lambda: (p + unreal.Vector(L * 0.9, -L * 1.5, L * 0.55), c, 40.0)))
        out.append(("walk", lambda: P.set_state(unreal.BirdState.WALK, 0.1), lambda: (p + unreal.Vector(0, -L * 2.2, L * 0.4), c, 40.0)))
        out.append(("peck", lambda: P.set_state(unreal.BirdState.PECK, 0.1), lambda: (p + unreal.Vector(L * 0.4, -L * 2.0, L * 0.5), c, 40.0)))
        for dm in (4, 15, 50):
            dd = dm * 100.0
            fov = max(2 * math.degrees(math.atan(L * 1.4 / dd)), 1.0)
            out.append(("lod_%02dm" % dm, lambda: P.set_state(unreal.BirdState.PERCH, 0.1),
                        (lambda dd=dd, fov=fov: (p + unreal.Vector(0, -dd, L * 0.4), c, fov))))
    if F:
        f = F.get_actor_location()
        for s in (1.0, 0.5, 0.1):
            out.append(("wingpose_%03d" % int(s * 100), (lambda s=s: F.set_wing_pose(s, s, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0)),
                        lambda: (f + unreal.Vector(-span * 0.15, 0, span * 1.4), f, 60.0), True))
    if H:
        body = H.get_editor_property("body")

        def eye():
            a, b = body.get_socket_location("eye_l"), body.get_socket_location("eye_r")
            return unreal.Vector((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2)
        out.append(("fpv_ahead", lambda: H.set_first_person(True), lambda: (eye(), eye() + H.get_actor_forward_vector() * 100.0, 90.0)))
        out.append(("fpv_down", lambda: H.set_first_person(True),
                    lambda: (eye(), eye() + H.get_actor_forward_vector() * 30.0 + unreal.Vector(0, 0, -60.0), 90.0)))
        hp = H.get_actor_location()
        out.append(("hero_close", lambda: H.set_first_person(False),
                    lambda: (hp + unreal.Vector(L * 0.8, -L * 1.3, L * 0.5), hp + unreal.Vector(0, 0, L * 0.35), 40.0)))
    return [(sid,) + s for s in out]


def tick(dt):
    st["t"] += 1
    w = world()
    if w is None:
        return
    if st["steps"] is None:
        if st["t"] < WARMUP:
            return
        st["steps"] = [x for sid in IDS for x in steps_for(w, sid)]
        cams = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)
        st["cam"] = cams[0]
        st["t"] = 0
        os.makedirs(SHOTS, exist_ok=True)
        unreal.log_warning("BIRDCHECK %d shots" % len(st["steps"]))
    steps = st["steps"]
    i = st["i"]
    if i >= len(steps):
        unreal.SystemLibrary.execute_console_command(w, "QUIT")
        return
    step = steps[i]
    sid, name, setup, view = step[0], step[1], step[2], step[3]
    every_tick = len(step) > 4 and step[4]
    t = st["t"]
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if t == 1 or every_tick:
        setup()
    loc, tgt, fov = view()
    look(st["cam"], loc, tgt, fov)
    if t == 1:
        pc.set_view_target_with_blend(st["cam"], 0.0)
    if t == HOLD:
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 1280x720 filename=%s/%s_%s.png" % (SHOTS, sid, name))
        unreal.log_warning("BIRDCHECK %s %s" % (sid, name))
    if t == HOLD + 20:
        st["i"] += 1
        st["t"] = 0


unreal.register_slate_post_tick_callback(tick)
