"""Offscreen baseline of the cable-car station halls (the vestibule rooms on the towers) in Lvl_Scene01: per station,
at eye height (EYE, default 1.65 m over the hall floor), the views
  core     from the lift's top landing (Transport.Goto <s> top) out into the hall,
  corner   from a corner of the room across it, through its centre,
  vending  at the vending machine from 2 m (Lobby.Look <i> vending, then stepped back and turned round),
  piece    at the hall's signature piece (Lobby.Look <i> piece, or centre for a hanging one; Aton's is the coffee bar),
first at midday (12:30), then at 22:00. The hall's frame is the placed SM_TR_Vestibule_<Style> mesh's own transform
(vestibules.json is in that frame: room centre/radius, core_exit, floor_hole). TRANSPORTTEST lines; screenshots in the
order of the 'shot' lines (test_station_halls.sh copies them to Saved/StationHalls/<n>_<name>.png).

  STATIONS=aton,academ TIMES=12.5,22 unreal/scripts/test_station_halls.sh
"""
import json
import math
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
TR = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Transport")
BUILT = json.load(open(os.path.join(TR, "network_built.json")))
VEST = json.load(open(os.path.join(TR, "vestibules.json")))
HALLS = json.load(open(os.path.join(TR, "halls.json"))) if os.path.exists(os.path.join(TR, "halls.json")) else {}
ONLY = [s for s in os.environ.get("STATIONS", "").split(",") if s]
INDEX = {s["id"]: i for i, s in enumerate(BUILT["stations"])}
STATIONS = [s for s in BUILT["stations"] if not ONLY or s["id"] in ONLY]
TIMES = [float(t) for t in os.environ.get("TIMES", "12.5,22").split(",")]
EYE = float(os.environ.get("EYE", "165"))
S = {"t": 0.0, "step": 0, "shots": 0, "plan": [], "frames": {}, "eye_off": None}
EXIT = {"+x": (1, 0), "-x": (-1, 0), "+y": (0, 1), "-y": (0, -1)}


def log(msg):
    unreal.log_warning("TRANSPORTTEST " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def shot(w, name):
    log("shot %02d %s" % (S["shots"], name))
    S["shots"] += 1
    cmd(w, "shot")


def frames(w):
    """station id -> (world transform of its vestibule mesh) from the placed SM_TR_Vestibule_<Style> components"""
    by_mesh = {}
    for c in unreal.ObjectIterator(unreal.StaticMeshComponent):
        try:
            m = c.static_mesh
            if not m or not m.get_name().startswith("SM_TR_Vestibule_") or m.get_name().endswith("_Glass"):
                continue
            if c.get_world() != w:
                continue
            by_mesh.setdefault(m.get_name(), []).append(c.get_world_transform())
        except Exception:
            continue
    for st in STATIONS:
        mesh = VEST[st["style"]]["mesh"]
        cand = by_mesh.get(mesh, [])
        want = unreal.Vector(st["vestibule"]["at"][0] * 100.0, BUILT["frame"]["portal_face_y"] + st["vestibule"]["at"][1] * 100.0, 0)
        best = min(cand, key=lambda t: (unreal.Vector(t.translation.x, t.translation.y, 0) - want).length(), default=None)
        if best:
            S["frames"][st["id"]] = best
            r = best.rotation.rotator()
            log("frame %s %s at %.0f %.0f %.0f yaw %.1f (json yaw %.1f)" % (st["id"], mesh, best.translation.x, best.translation.y,
                                                                         best.translation.z, r.yaw, st["vestibule"]["yaw"]))
        else:
            log("frame %s %s: no placed mesh FAIL" % (st["id"], mesh))


def world_pt(sid, x, y, z=0.0):
    t = S["frames"][sid]
    return t.transform_location(unreal.Vector(x, y, z))


def world_yaw(sid, local_yaw):
    return S["frames"][sid].rotation.rotator().yaw + local_yaw


def go(w, p, pitch, yaw):
    """the eye (camera) at p; BugItGo places the pawn, so subtract the measured pawn->camera offset"""
    off = S["eye_off"] if S["eye_off"] is not None else 64.0
    cmd(w, "BugItGo %.0f %.0f %.0f %.1f %.1f 0" % (p.x, p.y, p.z - off, pitch, yaw))


def measure_eye(w):
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    cam = unreal.GameplayStatics.get_player_camera_manager(w, 0)
    off = cam.get_camera_location().z - pawn.get_actor_location().z
    if 0 < off < 200:
        S["eye_off"] = off
    log("eye offset %.1f cm (using %s)" % (off, S["eye_off"]))


def view(w, sid, kind):
    v = VEST[[s for s in STATIONS if s["id"] == sid][0]["style"]]
    room = v["room"]
    cx, cy = room["centre"][0], room["centre"][1]
    r = room["radius"]
    h = v["floor_hole"]
    hx, hy = (h[0] + h[2]) * 0.5, (h[1] + h[3]) * 0.5
    ex, ey = EXIT[v["core_exit"]]
    if kind == "core":
        # just out of the core's door, looking out into the hall along the exit, a little down
        half = (h[2] - h[0]) * 0.5 if ex else (h[3] - h[1]) * 0.5
        p = world_pt(sid, hx + ex * (half + 70.0), hy + ey * (half + 70.0), EYE)
        go(w, p, -6.0, world_yaw(sid, math.degrees(math.atan2(ey, ex))))
    elif kind == "corner":
        # in the room's corner 135 deg round from the exit, 0.8 r out, looking across through the centre
        a = math.atan2(ey, ex) + math.radians(135.0)
        lx, ly = cx + math.cos(a) * r * 0.8, cy + math.sin(a) * r * 0.8
        p = world_pt(sid, lx, ly, EYE)
        go(w, p, -10.0, world_yaw(sid, math.degrees(math.atan2(cy - ly, cx - lx))))


def lobby(w, sid, what):
    cmd(w, "Lobby.Look %d %s" % (INDEX[sid], what))


DIST = {"vending": 200.0, "piece": 300.0, "centre": 330.0}


def lobby_fix(w, sid, what):
    """Lobby.Look stands the hero on the piece's own spot, facing away along it; step DIST out along that look, turn
    round and look back at the piece, the eye at EYE over the hall floor (the vestibule frame's z)"""
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    l = pawn.get_actor_location()
    yaw = pc.get_control_rotation().yaw
    floor = S["frames"][sid].translation.z if sid in S["frames"] else l.z - 92.0
    d = DIST.get(what, 250.0)
    x, y = l.x + math.cos(math.radians(yaw)) * d, l.y + math.sin(math.radians(yaw)) * d
    go(w, unreal.Vector(x, y, floor + EYE), 18.0 if what == "centre" else -10.0, yaw + 180.0)


def top_fix(w, sid):
    """Transport.Goto <s> top: on the lift's top landing facing out of the core; eye at EYE, a little down"""
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    l = pawn.get_actor_location()
    floor = S["frames"][sid].translation.z if sid in S["frames"] else l.z - 100.0
    go(w, unreal.Vector(l.x, l.y, floor + EYE), -6.0, pc.get_control_rotation().yaw)


def staff_view(w, sid, style, role):
    """2 m in front of a v3 hall's barista or attendant (halls.json staff, station frame), looking at them"""
    for p in HALLS.get(style, {}).get("staff", []):
        if p.get("role") == role:
            a = math.radians(p.get("yaw", 0.0))
            x, y = p["at"][0] + math.cos(a) * 200.0, p["at"][1] + math.sin(a) * 200.0
            go(w, world_pt(sid, x, y, EYE), -8.0, world_yaw(sid, p.get("yaw", 0.0) + 180.0))
            return
    log("hall %s: no %s in halls.json" % (sid, role))


def plan():
    t = 80.0
    out = []

    def add(dt, f):
        nonlocal t
        out.append((t, f))
        t += dt
    add(1.0, frames)
    for hour in TIMES:
        hh, mm = int(hour), int(round((hour - int(hour)) * 60))
        tag = "%02d%02d" % (hh, mm)
        add(6.0, lambda w, hh=hh, mm=mm: cmd(w, "lyud.Birds.Time %d %d" % (hh, mm)))
        for st in STATIONS:
            sid = st["id"]
            # the station's streaming and the eye's adaptation: stand in the hall a while first
            add(0.5, lambda w, s=sid: cmd(w, "Transport.Goto %s vestibule" % s))
            add(0.5, lambda w, s=sid: (s in S["frames"]) and view(w, s, "core"))
            add(0.5, measure_eye)
            add(5.0, lambda w, s=sid: (s in S["frames"]) and view(w, s, "core"))
            add(0.3, lambda w, s=sid: cmd(w, "Transport.Goto %s top" % s))
            add(3.2, lambda w, s=sid: top_fix(w, s))
            add(0.5, lambda w, s=sid, g=tag: shot(w, "%s_%s_core" % (s, g)))
            if HALLS and hour == TIMES[0]:
                # the v3 hall: its report (dressing, lamps, staff, passengers) and its two staff from 2 m
                add(0.5, lambda w, s=sid: cmd(w, "Transport.Hall %d" % INDEX[s]))
                for role in ("barista", "attendant"):
                    add(3.5, lambda w, s=sid, st=st["style"], r=role: (s in S["frames"]) and staff_view(w, s, st, r))
                    add(0.5, lambda w, s=sid, g=tag, r=role: shot(w, "%s_%s_%s" % (s, g, r)))
            add(3.5, lambda w, s=sid: (s in S["frames"]) and view(w, s, "corner"))
            add(0.5, lambda w, s=sid, g=tag: shot(w, "%s_%s_corner" % (s, g)))
            # the hall's signature piece: a standing one (piece) or one hanging beside the core (centre)
            piece = "centre" if VEST[st["style"]].get("lobby", {}).get("centre") else "piece"
            for what in ("vending", piece):
                add(0.3, lambda w, s=sid, x=what: lobby(w, s, x))
                add(3.2, lambda w, s=sid, x=what: lobby_fix(w, s, x))
                add(0.5, lambda w, s=sid, x=what, g=tag: shot(w, "%s_%s_%s" % (s, g, x)))
    add(1.0, lambda w: cmd(w, "quit"))
    return out


def tick(dt):
    w = world()
    if not w:
        return
    S["t"] += min(dt, 0.25)
    if not S["plan"]:
        S["plan"] = plan()
        cmd(w, "lyud.Language ru")
        cmd(w, "lyud.Birds.Time 12 30")
    P = S["plan"]
    while S["step"] < len(P) and S["t"] > P[S["step"]][0]:
        P[S["step"]][1](w)
        S["step"] += 1


unreal.register_slate_post_tick_callback(tick)
