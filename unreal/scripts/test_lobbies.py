"""Offscreen check of the station lobbies (docs/transport.md "Lobbies") in Lvl_Scene01: per station the hall's vending machine and
its signature piece, from the front (Lobby.Look), and with LOBBYBUY=1 a purchase at the machine (Vending.Buy) with the roubles and
the pocket printed. LOBBYTEST lines; screenshots in the order of the 'shot' lines.   STATIONS=aton,zenit unreal/scripts/test_lobbies.sh
"""
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
BUILT = json.load(open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Transport", "network_built.json")))
ONLY = [s for s in os.environ.get("STATIONS", "").split(",") if s]
S = {"shots": 0}
PIECE = {"wing": "piece", "dome": "centre", "saucer": "centre", "star": "piece", "lantern": "centre", "arches": "piece", "folded": "piece"}


def log(msg):
    unreal.log_warning("LOBBYTEST " + msg)


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


def plan():
    t = 85.0
    out = []

    def add(dt, f):
        nonlocal t
        out.append((t, f))
        t += dt
    for i, st in enumerate(BUILT["stations"]):
        if ONLY and st["id"] not in ONLY:
            continue
        sid = st["id"]
        add(0.5, lambda w, s=sid: cmd(w, "Transport.Goto %s vestibule" % s))
        add(2.0, lambda w, i=i: cmd(w, "Lobby.Look %d vending" % i))
        add(3.0, lambda w, s=sid: shot(w, "%s_vending" % s))
        if os.environ.get("LOBBYBUY"):
            add(0.5, lambda w: cmd(w, "Vending.Buy 1"))
            add(3.0, lambda w, s=sid: shot(w, "%s_buy" % s))
        add(0.5, lambda w, i=i, k=PIECE[st["style"]]: cmd(w, "Lobby.Look %d %s" % (i, k)))
        add(3.0, lambda w, s=sid: shot(w, "%s_piece" % s))
        add(0.5, lambda w, i=i: cmd(w, "Lobby.Look %d centre" % i))
        add(3.0, lambda w, s=sid: shot(w, "%s_centre" % s))
    add(1.0, lambda w: log("done"))
    add(1.0, lambda w: cmd(w, "quit"))
    return out


STATE = {"t": 0.0, "i": 0, "plan": []}


def tick(dt):
    w = world()
    if not w:
        return
    STATE["t"] += min(dt, 0.25)
    if not STATE["plan"]:
        STATE["plan"] = plan()
        cmd(w, "lyud.Language ru")
        cmd(w, "lyud.Birds.Time 12 30")
    P = STATE["plan"]
    while STATE["i"] < len(P) and STATE["t"] > P[STATE["i"]][0]:
        P[STATE["i"]][1](w)
        STATE["i"] += 1


unreal.register_slate_post_tick_callback(tick)
