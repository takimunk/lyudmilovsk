"""Offscreen motion recordings of people, animals and birds (for judging animation smoothness, A/B of fixes).

  unreal/scripts/shoot_motion.sh [tag]   (runs this in Lvl_Scene01 -game -RenderOffscreen -benchmark -fps=30)

The game runs at a fixed step of 1/30 s (-benchmark -fps=30), so every rendered frame is 1/30 s of game time however
slow the render is. For each spot a camera actor is placed (static or following a subject) and every frame is saved
with HighResShot (motion_<tag>_<spot>_f####.png in Saved/Screenshots/<platform>) while one MOTION line per frame logs
the subject (actor and mesh location, yaw, foot / end bone world positions, a component-space bone to see whether the
pose was updated that frame). tools/motion_sheet.py turns them into contact sheets, mp4s and numbers.

Spots (SPOTS=a,b,... limits; MOTION_FRAMES=90 the length):
  street   busy promenade, static side-on camera 7 m from a walker (several townsfolk logged)
  follow   one walker followed from the side at 3.5 m (foot slide, stutter)
  manners  people standing / talking with gestures, static 4 m
  sit      someone seated (bench / chair), static 3 m
  bench    a walker ordered to sit_bench, followed; the last frames before and after sitting down are kept
  animal   a cat or dog of the animal life, followed 5 m; then animal_run after lyud.Animals.Scare
  birdgnd  a bird on the ground (hopping / walking / pecking), static 3 m; then birdup: scared, followed into the air
  gull     a gull (or any flying bird) followed in flight 10 m
  landing  a scared bird followed until it lands (ring buffer: the last frames before landing and after are kept)
Extra console commands for the whole run: MOTION_CMDS="lyud.Some.Cvar 0;..." (A/B of fixes).
"""
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import city_plan as C  # noqa: E402
import world_map as W  # noqa: E402

TAG = os.environ.get("MOTION_TAG", "base")
NF = int(os.environ.get("MOTION_FRAMES", "90"))
ALL = ["street", "follow", "far", "manners", "sit", "bench", "bench2", "bench3", "child", "animal", "birdgnd", "gull", "landing"]
SPOTS = [s for s in os.environ.get("SPOTS", ",".join(ALL)).split(",") if s]
WHEN = unreal.DateTime(2026, 10, 21, 12, 30, 0, 0)   # a Wednesday, noon
RES = os.environ.get("MOTION_RES", "960x540")
T0, T1 = -39.5, -36.2
RING = 150


def p(s, y, z):
    x, yy = C.to_map(s, y)
    return W.to_unreal(x, yy, z, 790.0)


CENTRES = [p(110, 880, T1), p(40, 900, T0), p(150, 905, T1), p(200, 1120, T1)]

st = {"frame": 0, "t": 0.0, "queue": [], "cur": None, "cam": None, "err": 0, "used": set(), "files": []}


def log(msg):
    unreal.log_warning("MOTION " + msg)


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == "Lvl_Scene01" and unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def cmd(w, c):
    unreal.SystemLibrary.execute_console_command(w, c)


def life(w):
    return unreal.PeopleLifeSubsystem.get_people_life(w)


def V(x, y, z):
    return unreal.Vector(float(x), float(y), float(z))


def ename(e):
    return str(e).split(".")[-1].split(":")[0].strip("<> ").upper()


# ------------------------------------------------------------------ camera


def camera(w):
    """The hero pawn is the camera (no actor spawning from python in -game): flying, no collision, its mesh hidden."""
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    if not st.get("pawn_ready") and pawn:
        if hasattr(pawn, "character_movement"):
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            pawn.character_movement.stop_movement_immediately()
        pawn.set_actor_enable_collision(False)
        for m in pawn.get_components_by_class(unreal.PrimitiveComponent):
            try:
                m.set_visibility(False, False)
            except Exception:  # noqa: BLE001
                pass
        cam = pawn.get_component_by_class(unreal.CameraComponent)
        st["camoff"] = (cam.get_world_location().z - pawn.get_actor_location().z) if cam else 0.0
        if cam:
            cam.set_editor_property("field_of_view", float(os.environ.get("MOTION_FOV", "55")))
        st["pawn_ready"] = True
    return pc, pawn


def look(w, eye, target):
    """The camera at eye looking at target."""
    pc, pawn = camera(w)
    dx, dy, dz = target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]
    pawn.set_actor_location(V(eye[0], eye[1], eye[2] - st.get("camoff", 0.0)), False, True)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))),
                                           yaw=math.degrees(math.atan2(dy, dx))))
    st["eye"] = eye


def cam_now(w):
    return unreal.GameplayStatics.get_player_controller(w, 0).player_camera_manager.get_camera_location()


# ------------------------------------------------------------------ subjects


def mesh_of(a):
    m = getattr(a, "mesh", None) if hasattr(a, "mesh") else None
    if m is None:
        try:
            m = a.get_editor_property("body")
        except Exception:  # noqa: BLE001
            m = None
    if m is None or not isinstance(m, unreal.SkeletalMeshComponent):
        m = a.get_component_by_class(unreal.SkeletalMeshComponent)
    return m


def all_bodies(w):
    return [a for a in unreal.ObjectIterator(unreal.NpcBody) if a.get_world() == w and not a.get_name().startswith("Default__")]


def drawn(a):
    try:
        m = mesh_of(a)
        return m is not None and m.is_visible() and not a.is_hidden_ed() if hasattr(a, "is_hidden_ed") else m.is_visible()
    except Exception:  # noqa: BLE001
        return False


def records(w):
    out = []
    l = life(w)
    for i in range(l.num_people()):
        r = l.get_person(i)
        if isinstance(r, tuple):
            r = r[1] if r[0] else None
        if r and r.pose != unreal.PersonPose.HIDDEN:
            out.append(r)
    return out


def people(w):
    """Drawn bodies: (actor, pose, key, record)."""
    recs = records(w)
    out = []
    for a in all_bodies(w):
        m = mesh_of(a)
        if not m or not m.is_visible():
            continue
        loc = a.get_actor_location()
        best, d0 = None, 1e9
        for r in recs:
            d = math.hypot(r.location.x - loc.x, r.location.y - loc.y)
            if d < d0:
                d0, best = d, r
        if best is not None and d0 < 300.0:
            out.append((a, ename(best.pose), best.key, best))
        else:
            out.append((a, "?", a.get_name(), None))
    return out


def rec_of(w, key):
    for r in records(w):
        if r.key == key:
            return r
    return None


def near_cam(w):
    e = st.get("eye")
    return V(*e) if e else cam_now(w)


def pick_person(w, poses, centre=None, maxd=6000.0):
    c = centre or near_cam(w)
    best = None
    for b in people(w):
        a, pose, key, r = b
        if pose not in poses:
            continue
        loc = a.get_actor_location()
        if abs(loc.z - c.z) > 1500.0:
            continue
        d = math.hypot(loc.x - c.x, loc.y - c.y) + (3000.0 if key in st["used"] else 0.0)
        if d < maxd and (best is None or d < best[0]):
            best = (d, b)
    return best[1] if best else None


def facing(a):
    """Unit XY vector the body faces / walks (velocity if moving, else mesh +Y, else actor forward)."""
    v = a.get_velocity()
    if math.hypot(v.x, v.y) > 20.0:
        n = math.hypot(v.x, v.y)
        return v.x / n, v.y / n
    m = mesh_of(a)
    f = m.get_right_vector() if (m and isinstance(a, unreal.NpcBody)) else a.get_actor_forward_vector()
    n = math.hypot(f.x, f.y) or 1.0
    return f.x / n, f.y / n


def sun_dir(w):
    """XY direction the sunlight travels (the brightest directional light), so cameras can keep the sun behind them."""
    if "sun" not in st:
        st["sun"] = None
        best = -1.0
        for l in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DirectionalLight):
            try:
                comp = l.get_component_by_class(unreal.DirectionalLightComponent)
                inten = comp.get_editor_property("intensity") if comp else 0.0
            except Exception:  # noqa: BLE001
                inten = 0.0
            f = (comp or l).get_forward_vector() if comp else l.get_actor_forward_vector()
            if inten > best:
                best = inten
                n = math.hypot(f.x, f.y) or 1.0
                st["sun"] = (f.x / n, f.y / n)
        log("sun travels %s" % (st["sun"],))
    return st["sun"]


def side_eye(a, dist, up, ahead=0.0, front=0.0, key=None):
    """A point `dist` to the side of the subject (perpendicular to its heading), `front` towards its face. With a key
    the side is chosen once (sun behind the camera) and kept, so a turn of the subject doesn't flip the camera over."""
    t = a.get_actor_location()
    fx, fy = facing(a)
    sx, sy = -fy, fx
    if key is not None:
        sides = st.setdefault("sides", {})
        if key not in sides:
            sun = sun_dir(a.get_world()) or (1.0, 0.0)
            # the camera looks along -side: that should go with the light
            sides[key] = 1.0 if (-sx * sun[0] - sy * sun[1]) >= 0.0 else -1.0
            sides[key + "_v"] = (sx * sides[key], sy * sides[key])
        # keep to the same side of the path: flip the perpendicular if it turned past 90 deg from the first one
        v0 = sides[key + "_v"]
        sgn = 1.0 if (sx * v0[0] + sy * v0[1]) >= 0.0 else -1.0
        sx, sy = sx * sgn, sy * sgn
        sides[key + "_v"] = (sx, sy)
    return (t.x + sx * dist + fx * (ahead + front), t.y + sy * dist + fy * (ahead + front), t.z + up), (t.x + fx * ahead, t.y + fy * ahead, t.z)


# ------------------------------------------------------------------ per-frame log


BONES_H = ["foot_l", "foot_r", "hand_r", "head"]


def track(a, label):
    try:
        m = mesh_of(a)
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        s = "%s a=%.1f,%.1f,%.1f yaw=%.1f" % (label, loc.x, loc.y, loc.z, rot.yaw)
        if m:
            ml = m.get_world_location()
            mr = m.get_world_rotation()
            s += " m=%.1f,%.1f,%.1f myaw=%.1f" % (ml.x, ml.y, ml.z, mr.yaw)
            names = st.get("bones_" + a.get_name())
            if names is None:
                n = m.get_num_bones()
                allb = [str(m.get_bone_name(i)) for i in range(n)]
                if "foot_l" in allb:
                    names = [b for b in BONES_H if b in allb]
                else:
                    names = [allb[i] for i in sorted({n - 1, n // 2, max(0, n // 4)})] if n else []
                st["bones_" + a.get_name()] = names
            for b in names:
                p_ = m.get_socket_location(b)
                s += " %s=%.1f,%.1f,%.1f" % (b, p_.x, p_.y, p_.z)
            if names:
                # component space of the last bone: unchanged between frames = the pose was not updated
                cs = m.get_socket_transform(names[-1], unreal.RelativeTransformSpace.RTS_COMPONENT).translation
                s += " cs=%.2f,%.2f,%.2f" % (cs.x, cs.y, cs.z)
        extra = ""
        if isinstance(a, unreal.BirdActor):
            extra = " state=%s sp=%s" % (ename(a.get_state()), a.get_editor_property("species"))
        elif isinstance(a, unreal.AnimalActor):
            extra = " act=%s" % ename(a.get_activity())
        return s + extra
    except Exception as e:  # noqa: BLE001
        return "%s err %s" % (label, e)


# ------------------------------------------------------------------ the shot machine
# A spot is a list of phases: ("do", fn) runs once; ("wait", frames, fn|None) runs fn(w, i) per frame; ("rec", frames,
# fn) shoots every frame and calls fn(w, i) before; ("ring", maxframes, fn) shoots every frame keeping the last RING,
# fn(w, i) returns the frames still to go after the event (None: no event yet).

SPOT = {}


def spot(name):
    def deco(fn):
        SPOT[name] = fn
        return fn
    return deco


def shot(w, name, i):
    fn = "motion_%s_%s_f%04d" % (TAG, name, i)
    cmd(w, "HighResShot %s filename=%s" % (RES, fn))
    return fn


def subj_lines(w, name, i, subjects):
    cam = cam_now(w)
    parts = ["cam=%.1f,%.1f,%.1f" % (cam.x, cam.y, cam.z)]
    for k, a in enumerate(subjects):
        if a is None:
            continue
        parts.append(track(a, "s%d" % k))
    log("%s f%04d %s" % (name, i, " | ".join(parts)))


def s_start(w):
    for c in ["lyud.Prefetch 0", "r.MotionBlurQuality 0", "au.LinearGainScalarForFinalOutut 0", "lyud.Weather.Preset clear",
              "lyud.Birds.IgnorePlayer 1", "lyud.Animals.IgnorePlayer 1", "lyud.People.Manner auto"] + \
            [c for c in os.environ.get("MOTION_CMDS", "").split(";") if c]:
        cmd(w, c)
    for cal in unreal.ObjectIterator(unreal.GameCalendar):
        if cal.get_world() == w:
            cal.set_now(WHEN)
    cmd(w, "ghost")
    c = CENTRES[0]
    look(w, (c[0] - 700.0, c[1], c[2] + 250.0), (c[0], c[1], c[2] + 100.0))
    log("start tag=%s spots=%s frames=%d people=%d" % (TAG, ",".join(SPOTS), NF, life(w).num_people()))


def census(w, label):
    ps = people(w)
    c = near_cam(w)
    log("%s: drawn people %d: %s" % (label, len(ps), ", ".join("%s(%s %.0fm)" % (k, pose, math.hypot(
        a.get_actor_location().x - c.x, a.get_actor_location().y - c.y) / 100.0) for a, pose, k, _ in ps[:40])))


# --- people


@spot("street")
def sp_street(ctx):
    def setup(w):
        go_busy(w, ("WALK",))

    def choose(w, i):
        if i == 0:
            census(w, "street")
            b = pick_person(w, ("WALK",))
            if not b:
                b = pick_person(w, ("WALK",), maxd=15000.0)
            ctx["subj"] = b[0] if b else None
            if b:
                st["used"].add(b[2])
                eye, tgt = side_eye(b[0], 700.0, 160.0, ahead=250.0, key="street")
                ctx["eye"], ctx["tgt"] = eye, (tgt[0], tgt[1], tgt[2] + 90.0)
                look(w, ctx["eye"], ctx["tgt"])
                log("street subject %s at %s" % (b[2], b[0].get_actor_location()))
            else:
                log("street: no walker")

    def each(w, i):
        if i == 0:
            ctx["others"] = [b[0] for b in people(w) if b[0] != ctx.get("subj")][:4]
        subj_lines(w, "street", i, [ctx.get("subj")] + ctx.get("others", []))

    return [("do", setup), ("wait", 150, None), ("wait", 1, choose), ("wait", 15, None), ("rec", NF, each, "street")]


COVERS = [c for c in os.environ.get("MOTION_COVERS", "0.85,1.0,1.15").split(",") if c]


def follow_spot(ctx, label, poses_pick, nseg):
    """One walker followed from the side at 3.5 m; a recording per lyud.Anim.GaitCover value (same walker)."""
    def choose(w, i):
        b = poses_pick(w)
        ctx["subj"] = b[0] if b else None
        if b:
            st["used"].add(b[2])
            r = b[3]
            log("%s subject %s age %s" % (label, b[2], getattr(r, "age", "?") if r else "?"))
        else:
            log("%s: nobody" % label)

    def cam(w, i):
        a = ctx.get("subj")
        if a:
            eye, tgt = side_eye(a, 350.0, 110.0, front=60.0, key=label)
            look(w, eye, (tgt[0], tgt[1], tgt[2] + 60.0))

    def each(name):
        def f(w, i):
            cam(w, i)
            subj_lines(w, name, i, [ctx.get("subj")])
        return f

    q = [("wait", 1, choose), ("wait", 20, cam)]
    for c in COVERS:
        name = "%s_c%03d" % (label, int(round(float(c) * 100)))
        q += [("do", (lambda c: lambda w: (cmd(w, "lyud.Anim.GaitCover %s" % c), log("GaitCover %s" % c)))(c)),
              ("wait", 10, cam), ("rec", nseg, each(name), name)]
    q.append(("do", lambda w: cmd(w, "lyud.Anim.GaitCover 1.0")))
    return q


@spot("follow")
def sp_follow(ctx):
    return follow_spot(ctx, "follow", lambda w: pick_person(w, ("WALK",)) or pick_person(w, ("WALK",), maxd=15000.0), NF)


def pick_child(w):
    rs = [r for r in records(w) if ename(r.pose) == "WALK" and 0 < r.age < 14]
    log("walking children: %d %s" % (len(rs), ", ".join("%s(%d)" % (r.key, r.age) for r in rs[:8])))
    if not rs:
        return None
    c = near_cam(w)
    rs.sort(key=lambda r: math.hypot(r.location.x - c.x, r.location.y - c.y))
    r = rs[0]
    t = r.location
    look(w, (t.x - 600.0, t.y, t.z + 300.0), (t.x, t.y, t.z + 80.0))
    st["child"] = r.key
    return None


@spot("child")
def sp_child(ctx):
    def find(w, i):
        pick_child(w)

    def get(w):
        key = st.get("child")
        if not key:
            return None
        for b in people(w):
            if b[2] == key:
                return b
        log("child %s has no body near" % key)
        return None

    return [("wait", 1, find), ("wait", 120, None)] + follow_spot(ctx, "child", get, 45)


@spot("manners")
def sp_manners(ctx):
    def setup(w):
        go_busy(w, ("TALK", "STAND", "LOOK"))

    def choose(w, i):
        census(w, "manners")
        b = pick_person(w, ("TALK", "STAND", "LOOK")) or pick_person(w, ("TALK", "STAND", "LOOK"), maxd=20000.0)
        ctx["subj"] = b[0] if b else None
        if b:
            st["used"].add(b[2])
            eye, tgt = side_eye(b[0], 250.0, 150.0, front=300.0, key="manners")
            look(w, eye, (tgt[0], tgt[1], tgt[2] + 100.0))
            ctx["others"] = [x[0] for x in people(w) if x[0] != b[0] and x[1] in ("TALK", "STAND", "LOOK") and
                             (x[0].get_actor_location() - b[0].get_actor_location()).length() < 400.0][:2]
            log("manners subject %s pose %s, near %d" % (b[2], b[1], len(ctx["others"])))
        else:
            log("manners: nobody standing")

    def each(w, i):
        subj_lines(w, "manners", i, [ctx.get("subj")] + ctx.get("others", []))

    return [("do", setup), ("wait", 120, None), ("wait", 1, choose), ("wait", 15, None), ("rec", NF, each, "manners")]


@spot("sit")
def sp_sit(ctx):
    def choose(w, i):
        if i == 0 and not pick_person(w, ("SIT", "READ", "EAT"), maxd=3000.0):
            go_busy(w, ("SIT", "READ", "EAT"))
            ctx["moved"] = 1
            return
        b = pick_person(w, ("SIT", "READ", "EAT")) or pick_person(w, ("SIT", "READ", "EAT"), maxd=20000.0)
        ctx["subj"] = b[0] if b else None
        if b:
            st["used"].add(b[2])
            eye, tgt = side_eye(b[0], 180.0, 110.0, front=220.0, key="sit")
            look(w, eye, (tgt[0], tgt[1], tgt[2] + 60.0))
            log("sit subject %s pose %s" % (b[2], b[1]))
        else:
            log("sit: nobody seated")

    def each(w, i):
        subj_lines(w, "sit", i, [ctx.get("subj")])

    def choose2(w, i):
        if ctx.get("moved") and i == 0:
            choose(w, 1)

    return [("wait", 1, choose), ("wait", 150, None), ("wait", 1, choose2), ("wait", 15, None), ("rec", NF, each, "sit")]


def bench_spot(ctx, label, centre_i):
    def choose(w, i):
        c = CENTRES[centre_i]
        b = pick_person(w, ("WALK", "STAND", "LOOK"), centre=V(*c), maxd=8000.0) or pick_person(w, ("WALK", "STAND", "LOOK"))
        ctx["subj"] = b[0] if b else None
        if b:
            ctx["key"] = b[2]
            st["used"].add(b[2])
            cmd(w, "lyud.People.Order %s sit_bench" % b[2])
            log("%s subject %s ordered to sit_bench (pose %s)" % (label, b[2], b[1]))

    def ring(w, i):
        a = ctx.get("subj")
        if not a:
            return 0
        r = rec_of(w, ctx["key"])
        try:
            eye, tgt = side_eye(a, 330.0, 120.0, front=80.0, key=label)
            look(w, eye, (tgt[0], tgt[1], tgt[2] + 60.0))
        except Exception:  # noqa: BLE001
            pass
        subj_lines(w, label, i, [a])
        pose = ename(r.pose) if r else "?"
        if i % 30 == 0:
            log("%s f%d %s pose %s state %s place %s act %s" % (label, i, ctx["key"], pose, r.state if r else "?",
                                                              r.place if r else "?", r.activity if r else "?"))
        if pose == "SIT" and "after" not in ctx:
            ctx["after"] = i
            log("%s sat down at f%d" % (label, i))
        if "after" in ctx and (i - ctx["after"]) in (0, 30, 74) and r:
            l = a.get_actor_location()
            m = mesh_of(a)
            ml = m.get_world_location() if m else l
            # the slot (sim) against where the body ended, and what is under / round it
            hits = []
            for dx, dy in ((0, 0), (40, 0), (-40, 0), (0, 40), (0, -40)):
                hr = unreal.SystemLibrary.line_trace_single(w, V(r.location.x + dx, r.location.y + dy, r.location.z + 200.0),
                    V(r.location.x + dx, r.location.y + dy, r.location.z - 200.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                    False, [a], unreal.DrawDebugTrace.NONE, True)
                try:
                    if isinstance(hr, tuple):
                        hr = hr[-1]
                    t = hr.to_tuple()
                    hits.append("%+d,%+d:z%.0f %s" % (dx, dy, t[4].z, t[9].get_name() if t[9] else "-"))
                except Exception as e:  # noqa: BLE001
                    hits.append("err %s" % e)
            log("%s seat check +%d: place %s slot %s yaw %.1f | actor %s yaw %.1f | mesh %s myaw %.1f | traces %s" % (
                label, i - ctx["after"], r.place, r.location, r.yaw, l, a.get_actor_rotation().yaw, ml,
                m.get_world_rotation().yaw if m else 0.0, "; ".join(hits)))
        if "after" in ctx:
            return max(0, 75 - (i - ctx["after"]))
        return None

    return [("wait", 1, choose), ("ring", 1500, ring, label)]


@spot("bench")
def sp_bench(ctx):
    return bench_spot(ctx, "bench", 3)


@spot("bench2")
def sp_bench2(ctx):
    return bench_spot(ctx, "bench2", 0)


@spot("bench3")
def sp_bench3(ctx):
    return bench_spot(ctx, "bench3", 2)


@spot("far")
def sp_far(ctx):
    """One walker (on screen) seen side-on from 10, 25 and 50 m: is its pose updated every frame?"""
    def choose(w, i):
        b = pick_person(w, ("WALK",)) or pick_person(w, ("WALK",), maxd=15000.0)
        ctx["subj"] = b[0] if b else None
        if b:
            st["used"].add(b[2])
            log("far subject %s" % b[2])

    def cam(d):
        def f(w, i):
            a = ctx.get("subj")
            if a:
                eye, tgt = side_eye(a, d, 160.0, key="far")
                look(w, eye, (tgt[0], tgt[1], tgt[2] + 90.0))
        return f

    def each(d, name):
        g = cam(d)

        def f(w, i):
            g(w, i)
            subj_lines(w, name, i, [ctx.get("subj")])
        return f

    q = [("wait", 1, choose)]
    for d in (1000.0, 2500.0, 5000.0):
        name = "far%d" % int(d / 100)
        q += [("wait", 10, cam(d)), ("rec", 45, each(d, name), name)]
    return q


# --- animals


def animals(w):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AnimalActor) if mesh_of(a) and mesh_of(a).is_visible()]


def pick_actor(lst, centre, prefer=None):
    best = None
    for a in lst:
        d = (a.get_actor_location() - centre).length()
        if prefer and not prefer(a):
            d += 1e6
        if best is None or d < best[0]:
            best = (d, a)
    return best[1] if best else None


@spot("animal")
def sp_animal(ctx):
    def find(w, i):
        lst = list(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AnimalActor))
        log("animals: %d actors: %s" % (len(lst), ", ".join("%s %s %s" % (a.get_name(), ename(a.get_activity()),
            a.get_actor_location()) for a in lst[:12])))
        moving = lambda a: ename(a.get_activity()) in ("WALK", "TROT", "RUN", "FOLLOW", "APPROACH", "CHASE", "FLEE")  # noqa: E731
        a = pick_actor(lst, near_cam(w), moving)
        if not a:
            c = near_cam(w)
            cmd(w, "lyud.Animals.SpawnNear Laika 0 %.0f %.0f 2" % (c.x + 400.0, c.y))
            log("animals: none, spawned Laika")
        ctx["subj"] = a
        if a:
            l = a.get_actor_location()
            look(w, (l.x - 500.0, l.y - 300.0, l.z + 150.0), (l.x, l.y, l.z + 30.0))
            log("animal subject %s %s at %s" % (a.get_name(), ename(a.get_activity()), l))

    def refind(w, i):
        if not ctx.get("subj"):
            ctx["subj"] = pick_actor(list(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AnimalActor)), near_cam(w))
        cam(w, i)

    def cam(w, i):
        a = ctx.get("subj")
        if a:
            eye, tgt = side_eye(a, 450.0, 90.0, front=40.0, key="animal")
            look(w, eye, (tgt[0], tgt[1], tgt[2] + 25.0))

    def each(name):
        def f(w, i):
            cam(w, i)
            subj_lines(w, name, i, [ctx.get("subj")])
        return f

    def scare(w, i):
        a = ctx.get("subj")
        if a:
            cmd(w, "lyud.Animals.IgnorePlayer 0")
            pc = unreal.GameplayStatics.get_player_controller(w, 0)
            l = a.get_actor_location()
            pc.get_controlled_pawn().set_actor_location(V(l.x - 250.0, l.y, l.z + 100.0), False, True)
            cmd(w, "lyud.Animals.Scare 12")
            log("animal scared")

    return [("wait", 1, find), ("wait", 60, refind), ("rec", NF, each("animal"), "animal"), ("wait", 1, scare), ("wait", 6, cam),
            ("rec", NF, each("animal_run"), "animal_run"), ("do", lambda w: cmd(w, "lyud.Animals.IgnorePlayer 1"))]


# --- birds


def birds(w):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.BirdActor) if mesh_of(a) and mesh_of(a).is_visible()]


def bird_census(w, label):
    lst = birds(w)
    c = near_cam(w)
    lst.sort(key=lambda a: (a.get_actor_location() - c).length())
    log("%s: birds %d: %s" % (label, len(lst), ", ".join("%s %s %.0fm" % (a.get_editor_property("species"), ename(a.get_state()),
        (a.get_actor_location() - c).length() / 100.0) for a in lst[:25])))
    return lst


GROUND = ("HOP", "WALK", "PECK", "EAT", "PERCH", "LOOK", "PREEN", "CALL", "ALERT")
AIR = ("FLY", "GLIDE", "TAKEOFF")


@spot("birdgnd")
def sp_birdgnd(ctx):
    def find(w, i):
        lst = bird_census(w, "birdgnd")
        c = near_cam(w)
        cand = [a for a in lst if ename(a.get_state()) in GROUND]
        # on the ground, not on a wire or roof: near the terrain under it (birds walk/hop on the ground)
        a = pick_actor(cand, c, lambda a: ename(a.get_state()) in ("HOP", "WALK", "PECK", "EAT"))
        if not a:
            cmd(w, "lyud.Birds.SpawnNear Pigeon %.0f %.0f %.0f" % (c.x + 300.0, c.y, c.z - 200.0))
            log("birdgnd: none on the ground, spawned a pigeon")
        ctx["subj"] = a
        if a:
            l = a.get_actor_location()
            sd = sun_dir(w) or (1.0, 0.0)
            ctx["off"] = (-sd[0] * 300.0, -sd[1] * 300.0)
            ctx["eye"] = (l.x + ctx["off"][0], l.y + ctx["off"][1], l.z + 60.0)
            look(w, ctx["eye"], (l.x, l.y, l.z + 5.0))
            log("birdgnd subject %s %s at %s" % (a.get_editor_property("species"), ename(a.get_state()), l))

    def refind(w, i):
        if not ctx.get("subj"):
            find(w, i)

    def each_static(w, i):
        a = ctx.get("subj")
        if a and i % 10 == 0:
            l = a.get_actor_location()
            look(w, ctx["eye"], (l.x, l.y, l.z + 5.0))   # re-aim (a hopping bird wanders), the camera stays
        subj_lines(w, "birdgnd", i, [a])

    def scare(w, i):
        a = ctx.get("subj")
        if a:
            cmd(w, "lyud.Birds.IgnorePlayer 0")
            l = a.get_actor_location()
            unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn().set_actor_location(
                V(l.x - 300.0, l.y, l.z + 120.0), False, True)
            cmd(w, "lyud.Birds.Scare 8")
            log("birdgnd scared")

    def each_up(w, i):
        a = ctx.get("subj")
        if a:
            l = a.get_actor_location()
            e = ctx["eye"]
            k = min(1.0, i / 45.0)
            o = ctx.get("off", (-300.0, 0.0))
            look(w, (e[0] + (l.x + o[0] - e[0]) * k, e[1] + (l.y + o[1] - e[1]) * k, e[2] + (l.z + 60.0 - e[2]) * k),
                 (l.x, l.y, l.z))
        subj_lines(w, "birdup", i, [a])

    return [("wait", 1, find), ("wait", 45, refind), ("rec", NF, each_static, "birdgnd"), ("wait", 1, scare), ("rec", NF, each_up, "birdup"),
            ("do", lambda w: cmd(w, "lyud.Birds.IgnorePlayer 1"))]


@spot("gull")
def sp_gull(ctx):
    def find(w, i):
        lst = bird_census(w, "gull")
        c = near_cam(w)
        air = [a for a in lst if ename(a.get_state()) in AIR]
        a = pick_actor(air, c, lambda a: "Gull" in str(a.get_editor_property("species")))
        if not a:
            # the beach: gulls glide over the shore; move there and look again
            log("gull: no bird in the air near, trying the beach")
            c2 = p(110, 760, T0)
            look(w, (c2[0], c2[1], c2[2] + 800.0), (c2[0] + 1000.0, c2[1] - 1000.0, c2[2] + 600.0))
        ctx["subj"] = a
        if a:
            log("gull subject %s %s at %s" % (a.get_editor_property("species"), ename(a.get_state()), a.get_actor_location()))

    def refind(w, i):
        if not ctx.get("subj") and i in (60, 150):
            find(w, i)

    def cam(w, i):
        a = ctx.get("subj")
        if a:
            eye, tgt = side_eye(a, 1000.0, 150.0, key="gull")
            look(w, eye, tgt)

    def each(w, i):
        cam(w, i)
        subj_lines(w, "gull", i, [ctx.get("subj")])

    return [("wait", 1, find), ("wait", 200, refind), ("wait", 1, lambda w, i: cam(w, i)), ("rec", NF, each, "gull")]


@spot("landing")
def sp_landing(ctx):
    def find(w, i):
        c = CENTRES[1]
        look(w, (c[0] - 600.0, c[1], c[2] + 200.0), (c[0], c[1], c[2] + 50.0))

    def find2(w, i):
        lst = bird_census(w, "landing")
        cand = [a for a in lst if ename(a.get_state()) in GROUND]
        a = pick_actor(cand, near_cam(w), lambda a: (a.get_actor_location() - near_cam(w)).length() < 3000.0)
        ctx["subj"] = a
        if a:
            l = a.get_actor_location()
            cmd(w, "lyud.Birds.IgnorePlayer 0")
            unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn().set_actor_location(
                V(l.x - 300.0, l.y, l.z + 120.0), False, True)
            cmd(w, "lyud.Birds.Scare 6")
            log("landing subject %s %s scared at %s" % (a.get_editor_property("species"), ename(a.get_state()), l))

    def ring(w, i):
        a = ctx.get("subj")
        if not a:
            return 0
        try:
            eye, tgt = side_eye(a, 600.0, 80.0, key="landing")
            look(w, eye, tgt)
        except Exception:  # noqa: BLE001
            return 0
        subj_lines(w, "landing", i, [a])
        s = ename(a.get_state())
        if i == 20:
            cmd(w, "lyud.Birds.IgnorePlayer 1")
        if i > 30 and s in ("LAND",) and "after" not in ctx:
            ctx["after"] = i
            log("landing: LAND at f%d" % i)
        if i > 30 and s in GROUND and "after" not in ctx:
            ctx["after"] = i
            log("landing: on the ground (%s) at f%d" % (s, i))
        if "after" in ctx:
            return max(0, 45 - (i - ctx["after"]))
        return None

    return [("wait", 1, find), ("wait", 90, None), ("wait", 1, find2), ("ring", 1200, ring, "landing")]


# ------------------------------------------------------------------ driver


def build_queue():
    q = [("do", s_start), ("wait", int(os.environ.get("MOTION_WARM", "900")), None)]   # 30 s of game time: streaming, shaders, bodies
    for name in SPOTS:
        if name not in SPOT:
            log("unknown spot %s" % name)
            continue
        ctx = {"name": name}
        q.append(("do", (lambda n: lambda w: log("spot %s begin" % n))(name)))
        q.extend(SPOT[name](ctx))
        q.append(("do", (lambda n: lambda w: log("spot %s end" % n))(name)))
    q.append(("wait", 30, None))
    q.append(("do", lambda w: (log("done"), cmd(w, "quit"))))
    return q


def tick(dt):
    try:
        _tick(dt)
    except Exception as e:  # noqa: BLE001
        st["err"] += 1
        log("tick failed: %s" % e)
        if st["err"] > 20:
            cmd(world(), "quit")


def _tick(dt):
    w = world()
    if not w:
        return
    st["t"] += dt
    if st["t"] < 5.0 and not st["queue"]:
        return
    if not st["queue"] and st["cur"] is None:
        st["queue"] = build_queue()
    if st["cur"] is None:
        if not st["queue"]:
            return
        ph = st["queue"].pop(0)
        st["cur"] = [ph, 0]
        if ph[0] == "do":
            try:
                ph[1](w)
            except Exception as e:  # noqa: BLE001
                log("do failed: %s" % e)
            st["cur"] = None
            return
    ph, i = st["cur"]
    kind = ph[0]
    try:
        if kind == "wait":
            if ph[2]:
                ph[2](w, i)
        elif kind == "rec":
            ph[2](w, i)
            shot(w, ph[3], i)
        elif kind == "ring":
            left = ph[2](w, i)
            st["files"].append(shot(w, ph[3], i))
            if len(st["files"]) > RING:
                st["drop"] = st["files"].pop(0)
                log("ring drop %s" % st["drop"])
            if left is not None and left <= 0:
                st["files"] = []
                st["cur"] = None
                return
    except Exception as e:  # noqa: BLE001
        log("phase %s f%d failed: %s" % (kind, i, e))
    st["cur"][1] = i + 1
    if st["cur"][1] >= ph[1]:
        if kind == "ring":
            st["files"] = []
        st["cur"] = None


unreal.register_slate_post_tick_callback(tick)
