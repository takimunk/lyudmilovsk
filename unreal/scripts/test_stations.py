"""Offscreen check of the cable-car stations' procedural part and vestibules (docs/transport.md "Station layout and the
vestibule connector") in Lvl_Scene01: per station the views from outside, the vestibule, the gallery, the lift's door,
the stair, a ride up in the lift (the player stands in the cab; it must carry them to the top deck), the top landing,
the platform; a line pylon from close by; Transport.Probe (traces down at landings, decks, galleries: STATIONPROBE).
TRANSPORTTEST lines; screenshots in the order of the 'shot' lines.

  unreal/scripts/test_stations.sh [station ...]
"""
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
BUILT = json.load(open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Transport", "network_built.json")))
ONLY = [s for s in os.environ.get("STATIONS", "").split(",") if s]
STATIONS = [s for s in BUILT["stations"] if not ONLY or s["id"] in ONLY]
FACE_Y = BUILT["frame"]["portal_face_y"]
S = {"t": 0.0, "step": 0, "shots": 0, "plan": []}


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
    cmd(w, "shot showui")


def pawn_z(w, sid, want):
    p = unreal.GameplayStatics.get_player_pawn(w, 0)
    z = p.get_actor_location().z
    ok = abs(z - want) < 160
    log("lift %s: player at z %.0f, top deck %.0f: %s" % (sid, z, want, "ok" if ok else "FAIL"))


def plan():
    t = 85.0
    out = [(84.0, lambda w: cmd(w, "Transport.Probe"))]

    def add(dt, f):
        nonlocal t
        out.append((t, f))
        t += dt
    for st in STATIONS:
        sid = st["id"]
        top = st["levels"][-1]["z"] * 100.0
        for view in ("tower", "outside", "vestibule", "lift", "stair"):
            add(0.5, lambda w, s=sid, v=view: cmd(w, "Transport.Goto %s %s" % (s, v)))
            add(3.0, lambda w, s=sid, v=view: shot(w, "%s_%s" % (s, v)))
        if os.environ.get("FLAGS"):
            # what draws on the gallery's floor: the shot again without the landscape, then without instances
            add(0.5, lambda w, s=sid: cmd(w, "Transport.Goto %s gallery" % s))
            for flag in os.environ["FLAGS"].split(","):
                add(0.5, lambda w, f=flag: cmd(w, "ShowFlag.%s 0" % f))
                add(2.5, lambda w, s=sid, f=flag: shot(w, "%s_gallery_no%s" % (s, f.lower())))
                add(0.5, lambda w, f=flag: cmd(w, "ShowFlag.%s 2" % f))
        # the lift: wait by its ground door (it comes down), step in; after a while it must be up at the top deck
        add(16.0, lambda w, s=sid: cmd(w, "Transport.Goto %s lift" % s))
        add(1.0, lambda w, s=sid: cmd(w, "Transport.Goto %s liftin" % s))
        add(2.5, lambda w, s=sid: shot(w, "%s_liftin" % s))
        add(30.0, lambda w: None)
        add(0.5, lambda w, s=sid, z=top: pawn_z(w, s, z + 96.0))
        add(2.5, lambda w, s=sid: shot(w, "%s_lift_top" % s))
        for view in ("top", "platform", "edge"):
            add(0.5, lambda w, s=sid, v=view: cmd(w, "Transport.Goto %s %s" % (s, v)))
            add(3.0, lambda w, s=sid, v=view: shot(w, "%s_%s" % (s, v)))
    # a line pylon from 7 m off, looking up its shaft (the stretched shaft used to tear)
    pyl = [p for ln in BUILT["lines"] for p in ln["pylons"] if not p.get("corner") and p["top"] - p["ground"] > 25]
    if pyl:
        p = pyl[0]
        x, y, z = p["x"] * 100.0, FACE_Y + p["y"] * 100.0, p["ground"] * 100.0
        for k, (dx, dy, dz, pitch, yaw) in enumerate([(700, 0, 250, 25, 180), (900, 300, 1200, 5, 198)]):
            add(0.5, lambda w, a=(x + dx, y + dy, z + dz, pitch, yaw): cmd(w, "BugItGo %.0f %.0f %.0f %.0f %.0f 0" % a))
            add(3.0, lambda w, k=k: shot(w, "pylon_%d" % k))
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
