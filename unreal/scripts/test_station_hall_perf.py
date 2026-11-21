"""What a v3 station hall costs, in ONE process (docs/benchmark.md: compare only inside a process): the player stands in a
hall (HALL, default zenit) at a busy hour (HOUR, default 8), then the variants run in turn, ROUNDS times:
  base       everything on
  nopeople   lyud.Transport.HallPeople 0 (no staff, no passengers)
  nolights   ShowFlag.PointLights 0 (the hall's lamps; also any other point light in view)
For each: 4 s to settle, then the mean and 95th percentile frame time over 8 s (wall clock between ticks) and the
process's resident memory (ps). HALLPERF lines; Transport.Hall at the start.

  unreal/scripts/test_station_hall_perf.sh
"""
import json
import os
import subprocess
import time

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
BUILT = json.load(open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Transport", "network_built.json")))
HALL = os.environ.get("HALL", "zenit")
HOUR = int(os.environ.get("HOUR", "8"))
ROUNDS = int(os.environ.get("ROUNDS", "2"))
INDEX = {s["id"]: i for i, s in enumerate(BUILT["stations"])}
VARIANTS = [("base", [], []),
            ("nopeople", ["lyud.Transport.HallPeople 0"], ["lyud.Transport.HallPeople 1"]),
            ("nolights", ["ShowFlag.PointLights 0"], ["ShowFlag.PointLights 2"])]
S = {"t": 0.0, "plan": None, "step": 0, "last": None, "frames": None, "res": {}}


def log(msg):
    unreal.log_warning("HALLPERF " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def rss_mb():
    try:
        return int(subprocess.check_output(["ps", "-o", "rss=", "-p", str(os.getpid())]).strip()) / 1024.0
    except Exception:
        return -1.0


def start(name):
    S["frames"] = []
    S["cur"] = name


def stop(name):
    f = sorted(S["frames"] or [0.0])
    mean = sum(f) / len(f)
    p95 = f[min(len(f) - 1, int(len(f) * 0.95))]
    m = rss_mb()
    S["res"].setdefault(name, []).append((mean, p95, m))
    log("%s: %.2f ms mean, %.2f ms p95 over %d frames, rss %.0f MB" % (name, mean, p95, len(f), m))
    S["frames"] = None


def summary(w):
    base = S["res"].get("base", [])
    bm = sum(x[0] for x in base) / max(1, len(base))
    for name, r in S["res"].items():
        mean = sum(x[0] for x in r) / len(r)
        log("summary %s: %.2f ms mean (%+.2f ms vs base), rss %.0f MB" % (name, mean, mean - bm, r[-1][2]))


def plan():
    t = 80.0
    out = []

    def add(dt, f):
        nonlocal t
        out.append((t, f))
        t += dt
    add(0.5, lambda w: cmd(w, "lyud.Birds.Time %d 0" % HOUR))
    add(1.0, lambda w: cmd(w, "Transport.Goto %s vestibule" % HALL))
    add(1.0, lambda w: cmd(w, "Lobby.Look %d vending" % INDEX[HALL]))
    add(14.0, lambda w: cmd(w, "Transport.Hall %d" % INDEX[HALL]))
    add(0.5, lambda w: cmd(w, "shot"))
    for _ in range(ROUNDS):
        for name, on, off in VARIANTS:
            add(4.0, lambda w, c=on: [cmd(w, x) for x in c])
            add(8.0, lambda w, n=name: start(n))
            add(0.2, lambda w, n=name: stop(n))
            add(0.5, lambda w, c=off: [cmd(w, x) for x in c])
    add(0.5, lambda w: cmd(w, "Transport.Hall %d" % INDEX[HALL]))
    add(0.5, summary)
    add(1.0, lambda w: cmd(w, "quit"))
    return out


def tick(dt):
    now = time.perf_counter()
    if S["frames"] is not None and S["last"] is not None:
        S["frames"].append((now - S["last"]) * 1000.0)
    S["last"] = now
    w = world()
    if not w:
        return
    S["t"] += min(dt, 0.25)
    if S["plan"] is None:
        S["plan"] = plan()
        cmd(w, "lyud.Language ru")
    P = S["plan"]
    while S["step"] < len(P) and S["t"] > P[S["step"]][0]:
        P[S["step"]][1](w)
        S["step"] += 1


unreal.register_slate_post_tick_callback(tick)
