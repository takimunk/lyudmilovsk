"""Offscreen check of the Gastronom No. 7 pavilion (blender/academgorodok/grocery.py + gastronom.py) in Lvl_Scene01: the hall
from several angles (overview, counter, vitrinas, back cases, cashier's booth, ceiling, floor, back door), close shots of the
people in it (the assistant orlova_tanya at her counter, customers sent to shop), and the trade screen with her many goods
before and after scrolling (wheel and keyboard selection). GASTROTEST lines; screenshots in the order of the 'shot' lines.

  unreal/scripts/test_gastronom.sh      (GASTRO_STEPS=hall,people,trade for some)
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
sys.path.insert(0, os.path.join(REPO, "blender", "academgorodok"))
import world_map as W  # noqa: E402
import grocery  # noqa: E402

FACE_Y = 790.0
ONLY = [s for s in os.environ.get("GASTRO_STEPS", "").split(",") if s]
FRAME = grocery.frame()
S = {"t": 0.0, "i": 0, "next": 12.0, "shots": 0}
# the seller's counter slot (Content/PeopleData/people_places.json "gastronom"), Unreal metres, yaw = the way she looks
SELLER = (-215.56, 676.03, -35.46, -101.0)
CUSTOMERS = ["zotov_lev", "korneeva_nina", "guseva_valentina"]


def log(msg):
    unreal.log_warning("GASTROTEST " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def hall(p):
    """A point of the hall in the pavilion's house frame (metres, floor z = 0.7) -> Unreal cm."""
    e = FRAME(p)
    mx, my = W.academ_to_map(e[0], e[1])
    return W.to_unreal(mx, my, W.academ_z() + p[2], FACE_Y)


def place(w, a, b):
    """Camera (the hero's eye) at a looking at b, both Unreal cm; the hero flies, no collision."""
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if hasattr(pawn, "character_movement"):
        pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    eye = 0.0
    cam = pawn.get_component_by_class(unreal.CameraComponent)
    if cam:
        eye = cam.get_world_location().z - pawn.get_actor_location().z
    pawn.set_actor_enable_collision(False)
    pawn.set_actor_location(unreal.Vector(a[0], a[1], a[2] - eye), False, True)
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                           yaw=math.degrees(math.atan2(dy, dx))))


def shot(w, name, ui=False):
    log("shot %02d %s" % (S["shots"], name))
    S["shots"] += 1
    cmd(w, "shot showui" if ui else "shot")


def at_seller(fwd, side, z, look_z, look_fwd=0.0):
    """Unreal cm: fwd metres in front of the seller (the way she looks), side metres to her left, z over her floor."""
    x, y, fz, yaw = SELLER
    f = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
    l = (-f[1], f[0])
    a = ((x + f[0] * fwd + l[0] * side) * 100, (y + f[1] * fwd + l[1] * side) * 100, (fz + z) * 100)
    b = ((x + f[0] * look_fwd) * 100, (y + f[1] * look_fwd) * 100, (fz + look_z) * 100)
    return a, b


def hall_shot(name, cam, tgt):
    return ("hall", name, lambda w: place(w, hall(cam), hall(tgt)), 3.0, False)


def people_shot(name, *args):
    return ("people", name, lambda w: place(w, *at_seller(*args)), 2.5, False)


def setup_people(w):
    cmd(w, "lyud.People.Order orlova_tanya work_shop gastronom")
    for k in CUSTOMERS:
        cmd(w, "lyud.People.Order %s shop gastronom" % k)
    place(w, hall((10.0, -4.0, 2.3)), hall((10.0, 0.0, 1.5)))


def probe(w):
    """Traces down on a 10 cm grid round the seller's slot (her feet's ground) and logs what is higher than the floor."""
    x, y, fz, _ = SELLER
    bumps = []
    for i in range(-6, 7):
        for j in range(-6, 7):
            px, py = (x + i * 0.1) * 100, (y + j * 0.1) * 100
            hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(px, py, fz * 100 + 50), unreal.Vector(px, py, fz * 100 - 60),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
            if hit:
                t = hit.to_tuple()
                z = t[4].z if hasattr(t[4], "z") else None
                actor = t[9].get_name() if len(t) > 9 and t[9] else "?"
                if z is not None and abs(z - fz * 100) > 2.0:
                    bumps.append("(%+.1f,%+.1f) %.0f cm %s" % (i * 0.1, j * 0.1, z - fz * 100, actor))
    log("probe floor round the seller: %d of 169 points off the floor; %s" % (len(bumps), "; ".join(bumps[:40])))


def report(w):
    cmd(w, "lyud.People.Report orlova_tanya")
    for k in CUSTOMERS:
        cmd(w, "lyud.People.Report " + k)


STEPS = [
    ("hall", "setup", lambda w: [cmd(w, c) for c in ("t.MaxFPS 0", "lyud.SkyHourOffset 0", "r.MotionBlurQuality 0")], 1.0, None),
    hall_shot("hall_entrance", (3.5, -6.2, 2.4), (16.0, 0.5, 1.4)),
    hall_shot("hall_from_north", (17.6, -6.3, 2.4), (5.0, 0.0, 1.5)),
    hall_shot("hall_high", (10.4, -6.5, 4.0), (10.4, 0.5, 0.9)),
    hall_shot("counter_vitrina", (9.0, -2.4, 2.0), (10.5, -0.6, 1.2)),
    hall_shot("vitrina_close", (7.4, -1.6, 1.75), (7.6, -0.6, 1.05)),
    hall_shot("back_cases", (12.0, -2.2, 2.3), (12.0, 1.5, 1.9)),
    hall_shot("back_cases_close", (9.0, -0.1, 1.9), (9.0, 1.5, 1.7)),
    hall_shot("kassa", (6.4, -4.0, 2.1), (4.0, -1.5, 1.4)),
    hall_shot("scales", (13.0, -1.6, 1.9), (13.0, -0.6, 1.2)),
    hall_shot("ceiling_lamps", (10.0, -5.0, 1.9), (10.0, -2.0, 4.2)),
    hall_shot("floor", (8.0, -5.5, 2.3), (9.5, -3.5, 0.7)),
    hall_shot("back_door", (14.5, -0.1, 2.0), (17.25, 1.5, 1.8)),
    hall_shot("vestibule", (-1.8, -2.0, 2.3), (1.5, -6.5, 1.5)),
    hall_shot("windows_inside", (10.0, -2.5, 2.2), (10.0, -6.75, 2.0)),
    ("people", "setup", setup_people, 45.0, None),
    ("people", "probe", probe, 1.0, None),
    ("people", "report", report, 1.0, None),
    people_shot("seller_face", 1.2, 0.0, 1.6, 1.55),
    people_shot("seller_body", 2.8, 0.3, 1.5, 0.9),
    people_shot("seller_side_left", 0.2, 2.2, 1.2, 0.7),
    people_shot("seller_side_right", 0.2, -2.2, 1.2, 0.7),
    people_shot("seller_back", -0.9, 0.3, 1.5, 0.8),
    people_shot("seller_legs_back", -0.8, 0.6, 0.8, 0.3),
    ("people", "feet_ik_off", lambda w: cmd(w, "lyud.Body.FeetIK 0"), 1.0, None),
    people_shot("seller_side_left_noik", 0.2, 2.2, 1.2, 0.7),
    ("people", "feet_ik_on", lambda w: cmd(w, "lyud.Body.FeetIK 1"), 1.0, None),
    people_shot("customers", 4.5, 0.0, 1.7, 1.0, 2.2),
    people_shot("customers_low", 3.6, 1.0, 0.9, 0.5, 2.2),
    people_shot("customers_side", 2.2, 3.5, 1.3, 0.7, 2.2),
    ("trade", "go", lambda w: place(w, *at_seller(1.6, 0.0, 1.6, 1.4)), 2.0, None),
    ("trade", "open", lambda w: [cmd(w, "lyud.Trade.Open orlova_tanya"), cmd(w, "Shop.Scroll")], 1.5, None),
    ("trade", "trade_top", None, 0.5, True),
    ("trade", "wheel", lambda w: cmd(w, "Shop.Scroll wheel -3"), 1.0, None),
    ("trade", "trade_wheel_down", None, 0.5, True),
    ("trade", "end", lambda w: cmd(w, "Shop.Scroll end"), 1.0, None),
    ("trade", "trade_last_selected", None, 0.5, True),
    ("trade", "up", lambda w: cmd(w, "Shop.Scroll select -11"), 1.0, None),
    ("trade", "trade_first_selected", None, 0.5, True),
    ("trade", "close", lambda w: cmd(w, "Shop.Open close"), 1.0, None),
]
if ONLY:
    STEPS = [s for s in STEPS if s[0] in ONLY]


def tick(dt):
    S["t"] += dt
    if S["t"] < S["next"]:
        return
    w = world()
    if not w:
        return
    if S["i"] >= len(STEPS):
        log("done, %d shots" % S["shots"])
        cmd(w, "quit")
        S["next"] = 1e9
        return
    group, name, act, wait, ui = STEPS[S["i"]]
    S["i"] += 1
    if act:
        try:
            act(w)
        except Exception as e:  # noqa: BLE001
            log("step %s failed: %s" % (name, e))
        if ui is None:
            log("step %s" % name)
        # a camera move: wait, then the picture (an action step with ui None has no picture)
        if ui is not None:
            STEPS.insert(S["i"], (group, name, None, 0.5, ui))
        S["next"] = S["t"] + wait
        return
    shot(w, name, ui)
    S["next"] = S["t"] + wait


unreal.register_slate_post_tick_callback(tick)
