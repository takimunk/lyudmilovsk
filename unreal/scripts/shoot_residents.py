"""Offscreen shots of the townsfolk's new activities (docs/town_life.md "The new activities", backlog item 26); no window,
no PIE:

  unreal/scripts/shoot_residents.sh   (runs this in Lvl_Scene01 -game -RenderOffscreen)

Scenes (a summer weekday, clear): the allotment at 08:00 (tend_allotment: dig/weed with the spade), the beach at 11:00
(skip_stones), the Rassvet's cinema hall at 15:10 (cinema_day, seated), the sanatorium's lounge at 16:00 (dominoes).
For each: the clock is set (a clock jump places everybody where they would be), the people at that activity are listed,
the nearest drawn one is shot close from the front three-quarter, then a wider view of the place. If nobody is at it, a
few people are sent there with PeopleLife Order and the scene waits for them.
Prints RESSHOT lines; screenshots res_<scene>_<n>.png go to Saved/Screenshots/MacEditor (the .sh moves them).
SCENES=allot,beach,cinema,domino limits the list.
"""
import math
import os

import unreal

SCENES = os.environ.get("SCENES", "allot,beach,cinema,domino").split(",")


def D(h, m):
    return unreal.DateTime(2027, 6, 12, h, m, 0, 0)    # a Saturday in June (the keepers and the children free)


# scene: when, activity, place centre (life metres = world cm / 100), place id, people to order if nobody is there
SCENE = {
    "allot": dict(when=D(int(os.environ.get("ALLOT_H", "8")), int(os.environ.get("ALLOT_M", "0"))), act="tend_allotment", centre=(-529.7, 528.0, -29.0), place="P-T2H2-ALLOTMENT",
                  manners=("dig", "weed")),
    "beach": dict(when=D(11, 0), act="skip_stones", centre=(150.0, 700.0, -42.6), place="S-BEACH", manners=("skip_stone",)),
    "cinema": dict(when=D(15, 10), act="cinema_day", centre=(-78.0, 1075.0, -36.0), place="P-CLUB2-CINEMA", manners=()),
    "domino": dict(when=D(16, 0), act="dominoes", centre=(-712.4, 1256.4, -8.0), place="P-SANATORIUM-LOUNGE",
                   manners=("dominoes",), eyes=((230.0, 20.0, 150.0), (0.0, 210.0, 160.0), (0.0, -210.0, 160.0))),
}

st = {"t": 0.0, "i": 0, "t0": 0.0, "wait": 0.0, "tries": 0}
try:
    import json
    _tp = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Lyudmilovsk", "Content", "PeopleData",
                                      "town_places.json")))
    KEEPERS = sorted({s.get("owner") for p in _tp["places"] if p["id"] == "P-T2H2-ALLOTMENT" for s in p["slots"] if s.get("owner")})
except Exception:  # noqa: BLE001
    KEEPERS = []


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def life(w):
    return unreal.PeopleLifeSubsystem.get_people_life(w)


def log(msg):
    unreal.log_warning("RESSHOT " + msg)


def people(w):
    l = life(w)
    out = []
    for i in range(l.num_people()):
        r = l.get_person(i)
        if isinstance(r, tuple):
            r = r[1] if r[0] else None
        if r:
            out.append(r)
    return out


def all_bodies(w):
    return [a for a in unreal.ObjectIterator(unreal.NpcBody) if a.get_world() == w and not a.get_name().startswith("Default__")]


def always_tick(w):
    # offscreen a body's pose ticks only when rendered: without this a shot shows the pose of the frame before
    for a in all_bodies(w):
        try:
            a.root_component.set_editor_property("visibility_based_anim_tick_option",
                                                 unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        except Exception:  # noqa: BLE001
            pass


def place(w, eye, target):
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if hasattr(pawn, "character_movement"):
        pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    pawn.set_actor_enable_collision(False)
    off = 0.0
    cam = pawn.get_component_by_class(unreal.CameraComponent)
    if cam:
        off = cam.get_world_location().z - pawn.get_actor_location().z
    pawn.set_actor_location(unreal.Vector(eye[0], eye[1], eye[2] - off), False, True)
    dx, dy, dz = target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]
    pc.set_control_rotation(unreal.Rotator(roll=0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                           yaw=math.degrees(math.atan2(dy, dx))))


def cm(c):
    return (c[0] * 100.0, c[1] * 100.0, c[2] * 100.0)


def at_activity(w, act):
    return [r for r in people(w) if r.activity == act]


def drawn(r):
    a = r.actor
    try:
        return a is not None and isinstance(a, unreal.NpcBody) and a.root_component.is_visible()
    except Exception:  # noqa: BLE001
        return False


def frame_body(w, a, back, side, up, look_z=100.0):
    t = a.get_actor_location()
    f = a.root_component.get_right_vector()     # the mesh faces +Y
    n = math.hypot(f.x, f.y) or 1.0
    fx, fy = f.x / n, f.y / n
    ex = t.x + fx * back - fy * back * side
    ey = t.y + fy * back + fx * back * side
    place(w, (ex, ey, t.z + up), (t.x, t.y, t.z + look_z))


def s_start(w):
    for c in ["EnableCheats", "lyud.Prefetch 0", "ghost", "r.MotionBlurQuality 0", "t.MaxFPS 0", "au.LinearGainScalarForFinalOutut 0",
              "lyud.Weather.Preset clear", "lyud.People.TownDrawDistance 6000"]:
        cmd(w, c)
    log("start people=%d" % life(w).num_people())


STEPS = [(s_start, 12.0)]


def step(fn, wait=1.0):
    STEPS.append((fn, wait))


def scene(name):
    sc = SCENE[name]
    c = cm(sc["centre"])

    def s_clock(w):
        for cal in unreal.ObjectIterator(unreal.GameCalendar):
            if cal.get_world() == w:
                cal.set_now(sc["when"])
        place(w, (c[0] - 700.0, c[1] - 300.0, c[2] + 400.0), (c[0], c[1], c[2] + 80.0))
        log("%s: clock %s" % (name, sc["when"]))
    step(s_clock, 14.0)

    def s_list(w):
        rs = at_activity(w, sc["act"])
        log("%s: %d at %s: %s" % (name, len(rs), sc["act"], ", ".join("%s(%d %s %s d%s)" % (
            r.key, r.age, r.state, str(r.pose).split(".")[-1], drawn(r)) for r in rs[:30])))
        if name == "allot":
            l = life(w)
            for k in KEEPERS:
                i = l.find_person(k)
                if i >= 0:
                    log("allot: keeper %s: %s" % (k, l.describe(i)[:400]))
        if len([r for r in rs if r.state == "doing"]) == 0:
            # nobody there yet: send the nearest few free people (children for the beach)
            l = life(w)
            cand = []
            for r in people(w):
                if r.activity == sc["act"] or r.state in ("away",) or r.place.startswith("P-") and "flat" in r.place:
                    continue
                if name == "beach" and not (6 <= r.age <= 15):
                    continue
                if name == "domino" and r.age < 58:
                    continue
                d = math.hypot(r.location.x - c[0], r.location.y - c[1])
                if d < 20000.0:
                    cand.append((d, r))
            cand.sort(key=lambda x: x[0])
            sent = []
            for d, r in cand[:12]:
                if l.order(r.id, sc["act"], sc["place"]):
                    sent.append("%s %.0fm" % (r.key, d / 100.0))
                if len(sent) >= 3:
                    break
            log("%s: ordered %s" % (name, sent))
            st["ordered"] = True
        else:
            st["ordered"] = False
    step(s_list, 1.0)

    def s_wait(w):
        # an ordered person walks there: give them game time (time scale up), then back to normal
        if st.get("ordered"):
            for cal in unreal.ObjectIterator(unreal.GameCalendar):
                if cal.get_world() == w:
                    try:
                        cal.set_time_scale(1.0)
                    except Exception:  # noqa: BLE001
                        pass
            cmd(w, "slomo 8")
    step(s_wait, 40.0)

    def s_waitdone(w):
        if st.get("ordered"):
            cmd(w, "slomo 1")
            rs = at_activity(w, sc["act"])
            log("%s: after waiting %d at it: %s" % (name, len(rs), ", ".join("%s(%s %s d%s)" % (
                r.key, r.state, str(r.pose).split(".")[-1], drawn(r)) for r in rs[:12])))
    # (slomo 8 for ~25 real s = ~3 game minutes of walking; enough from a hundred metres)
    step(s_waitdone, 2.0)

    def s_close(w):
        always_tick(w)
        if sc["manners"]:
            cmd(w, "lyud.People.Manner auto")
        rs = [r for r in at_activity(w, sc["act"]) if drawn(r)]
        rs.sort(key=lambda r: (r.state != "doing", math.hypot(r.location.x - c[0], r.location.y - c[1])))
        st["subjects"] = rs[:6]
        if not rs:
            log("%s: nobody drawn at %s" % (name, sc["act"]))
            return
        r = rs[0]
        l = life(w)
        log("%s: subject %s age %d state %s pose %s manner %s at %s yaw %.0f" % (
            name, r.key, r.age, r.state, str(r.pose).split(".")[-1], l.get_manner(r.id), r.location, r.yaw))
        frame_body(w, r.actor, 300.0, 0.6, 140.0 if name != "cinema" else 120.0, 80.0)
    step(s_close, 3.0)

    for n in range(2):
        def s_shot(w, n=n):
            rs = st.get("subjects") or []
            if not rs:
                return None
            l = life(w)
            pick = rs[0]
            if sc["manners"]:
                showing = [r for r in rs if str(l.get_manner(r.id)) in sc["manners"]]
                if not showing and st["tries"] < 14:
                    st["tries"] += 1
                    return 1.0
                if showing:
                    pick = showing[0]
            st["tries"] = 0
            always_tick(w)
            if sc.get("eyes"):
                mx = sum(r.location.x for r in rs) / len(rs)
                my = sum(r.location.y for r in rs) / len(rs)
                mz = sum(r.location.z for r in rs) / len(rs)
                e = sc["eyes"][n]
                place(w, (mx + e[0], my + e[1], mz + e[2]), (mx, my, mz + 50.0))
            elif n == 1:
                frame_body(w, pick.actor, 380.0, -0.7, 170.0, 80.0)
            else:
                frame_body(w, pick.actor, 300.0, 0.6, 140.0 if name != "cinema" else 120.0, 80.0)
            log("%s: shot %d of %s manner %s" % (name, n, pick.key, l.get_manner(pick.id)))
            st["shoot"] = "res_%s_%d" % (name, n)
            return None
        step(s_shot, 0.4)
        step(lambda w: st.get("shoot") and cmd(w, "HighResShot 1600x900 filename=%s" % st.pop("shoot")), 3.0)

    def s_wide(w):
        rs = [r for r in at_activity(w, sc["act"]) if drawn(r)]
        if rs:
            mx = sum(r.location.x for r in rs) / len(rs)
            my = sum(r.location.y for r in rs) / len(rs)
            mz = sum(r.location.z for r in rs) / len(rs)
        else:
            mx, my, mz = c
        if sc.get("eyes"):
            e = sc["eyes"][2]
            place(w, (mx + e[0], my + e[1], mz + e[2]), (mx, my, mz + 50.0))
        elif name == "cinema":
            # from the screen end, looking back over the rows (seats yaw ~354.5: they face about +X)
            place(w, (mx + 900.0, my - 150.0, mz + 260.0), (mx, my, mz + 60.0))
        else:
            place(w, (mx - 650.0, my - 450.0, mz + 380.0), (mx, my, mz + 80.0))
        always_tick(w)
    step(s_wide, 3.0)
    step(lambda w: cmd(w, "HighResShot 1600x900 filename=res_%s_wide" % name), 3.0)

    if name == "cinema":
        def s_wide2(w):
            rs = [r for r in at_activity(w, sc["act"]) if drawn(r)]
            if not rs:
                return
            mx = sum(r.location.x for r in rs) / len(rs)
            my = sum(r.location.y for r in rs) / len(rs)
            mz = sum(r.location.z for r in rs) / len(rs)
            place(w, (mx - 900.0, my + 150.0, mz + 260.0), (mx, my, mz + 60.0))
            always_tick(w)
        step(s_wide2, 3.0)
        step(lambda w: cmd(w, "HighResShot 1600x900 filename=res_cinema_wide2"), 3.0)


for s in SCENES:
    if s in SCENE:
        scene(s)


def s_end(w):
    log("done")
    cmd(w, "quit")


step(s_end, 1.0)


def tick(dt):
    try:
        _tick(dt)
    except Exception as e:  # noqa: BLE001
        if not st.get("err"):
            st["err"] = 1
            log("tick failed: %s" % e)
            cmd(world(), "quit")


def _tick(dt):
    st["t"] += dt
    w = world()
    if not w or st["t"] < 8.0:
        return
    if st["t"] - st["t0"] < st["wait"] or st["i"] >= len(STEPS):
        return
    fn, wait = STEPS[st["i"]]
    again = None
    try:
        again = fn(w)
    except Exception as e:  # noqa: BLE001
        log("step %d failed: %s" % (st["i"] + 1, e))
    if isinstance(again, float):
        wait = again
    else:
        st["i"] += 1
    st["t0"] = st["t"]
    st["wait"] = wait


unreal.register_slate_post_tick_callback(tick)
