"""Retargets clips from other humanoid rigs onto SK_Human_Skeleton, in world space, for the generated cast.

    Blender -b -P blender/humans/retarget_clips.py -- [--only Walk,Talk] [--preview]

Reads blender/humans/clips.json (which clip comes from which file and action, how its pelvis is placed) and bakes
every clip on the reference body (`target` there: a cast member's export glb, the body the clips' retarget source
SK_<Name> is made from) into Saved/LifeAnims/life_anims.glb as actions Life_<Clip>, which
unreal/scripts/import_life_anims.py imports as /Game/Characters/Humans/Anims/A_Human_<Clip>.

Why world space: SK_Human_Skeleton's bone frames (rolls) are MakeHuman's, posed rigidly from the donor's A-pose into
each model's T-pose. A clip made on another rig (or on the old A-pose donor) can't be copied as local rotations: the
wrists roll and the legs cross. Here every bone gets the source's rotation *relative to the source's reference pose*
(UAL: its A_TPose action; our old clips: the rig's rest), applied to the target's reference pose:

    Q_target(f) = Q_source(f) * Q_source_ref^-1 * Q_target_ref

where Q_target_ref is the target's rest rotation turned (shortest arc) so each bone points where the source's
reference bone points (T-pose to T-pose that is almost identity; old A-pose clips swing the arms down).
Bone lengths stay the target's; only root and pelvis translate:
    pelvis "scale": the source's pelvis offset from its reference, times hip height ratio (standing, walking)
    calm [action, w]: every bone's rotation only w of the way from that (calmer) clip toward this one
    pelvis "seat":  the seated pelvis lands on the house's chair point (UE (0, 1, 56.7) cm for a body like Guy_a,
                    seat top 47 cm, docs in life_anims.py); offsets from the source's seated pelvis are scaled.
Clips are in place (root stays at the origin), 30 fps, Blender character space faces -Y.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "Saved", "LifeAnims")
FPS = 30
SEAT_PELVIS = Vector((0.0, -0.01, 0.567))      # Blender m, the seated pelvis of the house's chair (Guy_a's size)


def log(m):
    print("RETARGET " + str(m), flush=True)


def resolve(p):
    p = os.path.expanduser(p)
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


def imp(path):
    before_o, before_a = set(bpy.data.objects), set(bpy.data.actions)
    if path.lower().endswith(".fbx"):
        bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    else:
        bpy.ops.import_scene.gltf(filepath=path, bone_heuristic="BLENDER")
    objs = [o for o in bpy.data.objects if o not in before_o]
    arm = next(o for o in objs if o.type == "ARMATURE")
    acts = {a.name.split("|")[-1]: a for a in bpy.data.actions if a not in before_a}
    for o in objs:
        o.hide_render = o.type != "MESH" or o.parent != arm
    return arm, objs, acts


def strip_object_curves(act):
    """FBX takes key the armature object's own transform (UE's axis conversion) next to the bones: drop those, so the
    reference pose (no action) and the clip share one object transform."""
    bags = []
    try:
        for layer in act.layers:
            for st in layer.strips:
                for slot in act.slots:
                    cb = st.channelbag(slot)
                    if cb:
                        bags.append(cb.fcurves)
    except AttributeError:
        bags.append(act.fcurves)
    n = 0
    for fcs in bags:
        for fc in [f for f in fcs if not f.data_path.startswith("pose.bones")]:
            fcs.remove(fc)
            n += 1
    return n


def set_action(arm, act):
    if not arm.animation_data:
        arm.animation_data_create()
    arm.animation_data.action = act
    if act is not None and hasattr(act, "slots") and len(act.slots):
        arm.animation_data.action_slot = act.slots[0]


def world_mats(arm):
    mw = arm.matrix_world
    return {b.name: mw @ b.matrix for b in arm.pose.bones}


def find_bone(arm, name, names_map):
    n = names_map.get(name, name)
    if n in arm.pose.bones:
        return n
    low = {b.name.lower(): b.name for b in arm.pose.bones}
    return low.get(n.lower())


class Source:
    def __init__(self, spec):
        self.path = resolve(spec["file"])
        self.arm, self.objs, self.acts = imp(self.path)
        for k, a in self.acts.items():                          # our own baked clips are named Life_* too
            a.name = "src_" + k
            if self.path.lower().endswith(".fbx"):
                strip_object_curves(a)
        self.names = spec.get("names", {})
        ref = spec.get("ref", "rest")
        if ref == "rest":
            set_action(self.arm, None)
            for b in self.arm.pose.bones:
                b.matrix_basis = Matrix()
        else:
            set_action(self.arm, self.acts[ref])
            bpy.context.scene.frame_set(int(self.acts[ref].frame_range[0]))
        bpy.context.view_layer.update()
        self.ref = world_mats(self.arm)
        self.dir = bone_dirs(self.ref, self.bone)

        for o in self.objs:                                      # only the target shows in the previews
            o.hide_render = True
        h = self.ref[self.bone("thigh_l")].translation.z
        log("source %s: %d actions, hip height %.3f" % (os.path.basename(self.path), len(self.acts), h))
        self.hip = h

    def bone(self, name):
        return find_bone(self.arm, name, self.names)

    def pose(self, act, frame):
        set_action(self.arm, act)
        bpy.context.scene.frame_set(int(math.floor(frame)), subframe=frame - math.floor(frame))
        bpy.context.view_layer.update()
        return world_mats(self.arm)


# which joint each bone points at: bone frames differ between importers (FBX joints aren't along Y), joints don't
AIM = {"spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "neck_01", "neck_01": "head"}
for _s in "lr":
    AIM.update({"clavicle_" + _s: "upperarm_" + _s, "upperarm_" + _s: "lowerarm_" + _s, "lowerarm_" + _s: "hand_" + _s,
                "hand_" + _s: "middle_01_" + _s, "thigh_" + _s: "calf_" + _s, "calf_" + _s: "foot_" + _s,
                "foot_" + _s: "ball_" + _s})
    for _f in ("thumb", "index", "middle", "ring", "pinky"):
        AIM.update({"%s_01_%s" % (_f, _s): "%s_02_%s" % (_f, _s), "%s_02_%s" % (_f, _s): "%s_03_%s" % (_f, _s)})


def bone_dirs(mats, name_of=lambda n: n):
    """Unit vectors from each bone's joint to the joint it aims at (AIM), in the pose `mats` (world matrices by bone)."""
    out = {}
    for b, c in AIM.items():
        sb, sc = name_of(b), name_of(c)
        if sb in mats and sc in mats:
            d = mats[sc].translation - mats[sb].translation
            if d.length > 1e-5:
                out[sb] = d.normalized()
    return out


def rest_local(arm, name):
    b = arm.data.bones[name]
    return (b.parent.matrix_local.inverted() @ b.matrix_local) if b.parent else b.matrix_local.copy()


def ordered(arm):
    out = []

    def walk(b):
        out.append(b.name)
        for c in b.children:
            walk(c)
    for b in arm.data.bones:
        if b.parent is None:
            walk(b)
    return out


def twist_y(rel):
    """The twist (deg) of a relative rotation about the bone's own axis (Y, along the bone)."""
    return math.degrees(rel.to_swing_twist("Y")[1])


def wrist(pose, b, q, flip, rl, trest):
    """Forearms have no twist bones, so a wrist turned far about the arm wrings the sleeve like a towel:
    - a hand twisted past 120 deg against the forearm was baked upside down (seat_anims' right hand on the mouse):
      turn it, and its fingers, the other way round;
    - half of what twist is left goes to the forearm, spreading the wringing between the elbow and the wrist."""
    lower = "lowerarm_" + b[-1]
    ql = pose[lower].to_quaternion()
    rest_rel = (trest[lower].inverted() @ trest[b]).to_quaternion()
    t = twist_y(rest_rel.inverted() @ (ql.inverted() @ q))
    if abs(t) > 120.0:
        r = q @ Quaternion((0, 1, 0), math.radians(-180.0 if t > 0 else 180.0)) @ q.inverted()
        flip[b[-1]] = r
        q = r @ q
        t = twist_y(rest_rel.inverted() @ (ql.inverted() @ q))
    ql2 = ql @ Quaternion((0, 1, 0), math.radians(0.5 * t))
    pose[lower] = Matrix.Translation(pose[lower].translation) @ ql2.to_matrix().to_4x4()
    return q


def bake(target, src, name, spec):
    act = src.acts[spec["action"]] if spec.get("action") else next(iter(src.acts.values()))
    tarm = target
    f0, f1 = act.frame_range
    if spec.get("range"):                                        # seconds into the source action
        a, b = spec["range"]
        f0, f1 = f0 + a * FPS, f0 + b * FPS
    n = int(round(f1 - f0)) + 1
    names = [b for b in ordered(tarm)]
    rl = {b: rest_local(tarm, b) for b in names}
    trest = {b: tarm.data.bones[b].matrix_local for b in names}
    sb = {b: (None if b in ("eye_l", "eye_r", "jaw") else src.bone(b)) for b in names}    # the face is the game's
    # the target's reference rotations, each bone turned to point where the source's reference bone points
    tref = {}
    tdir = bone_dirs(trest)
    for b in names:
        q = trest[b].to_quaternion()
        s = sb[b]
        if s and spec.get("align", True) and b in tdir and s in src.dir:
            q = tdir[b].rotation_difference(src.dir[s]) @ q
        tref[b] = q
    k = target_hip / src.hip
    pel = src.bone("pelvis")
    ref_pel = src.ref[pel].translation
    trest_pel = trest["pelvis"].translation
    seat_src = None
    if spec.get("pelvis") == "seat":
        # the source's seated pelvis: the lowest pelvis over the clip
        zs = []
        for i in range(n):
            f = f0 + (f1 - f0) * i / max(1, n - 1)
            zs.append(src.pose(act, f)[pel].translation.copy())
        seat_src = min(zs, key=lambda v: v.z)
    calm = (src.acts[spec["calm"][0]], spec["calm"][1]) if spec.get("calm") else None
    hand_of = {c.name for s_ in "lr" for c in tarm.data.bones["hand_" + s_].children_recursive}
    new = bpy.data.actions.new("Life_" + name)
    set_action(tarm, new)
    prev = {}
    for i in range(n):
        f = f0 + (f1 - f0) * i / max(1, n - 1)
        P = src.pose(act, f)
        flip = {}
        if calm:                                                     # the calmer clip at the same moment of its loop
            ca, cw = calm
            g0, g1 = ca.frame_range
            C = src.pose(ca, g0 + ((f - f0) % (g1 - g0)))
        pose = {}
        for b in names:
            parent = tarm.data.bones[b].parent
            s = sb[b]
            if s:
                qs = P[s].to_quaternion()
                d = qs @ src.ref[s].to_quaternion().inverted()
                if calm:                                             # toned down: part of the way toward the calm clip
                    dc = C[s].to_quaternion() @ src.ref[s].to_quaternion().inverted()
                    if dc.dot(d) < 0:
                        dc = -dc
                    d = dc.slerp(d, cw)
                q = d @ tref[b]
            else:                                                    # jaw, eyes: ride on the parent
                q = (pose[parent.name] @ rl[b]).to_quaternion() if parent else trest[b].to_quaternion()
            side = b[-1]
            if b in hand_of and side in flip:                        # a finger of a flipped hand turns with it
                q = flip[side] @ q
            if b in ("hand_l", "hand_r"):
                q = wrist(pose, b, q, flip, rl, trest)
            if b == "root":
                loc = Vector((0, 0, 0))
            elif b == "pelvis":
                p = P[pel].translation
                if spec.get("pelvis") == "abs":                    # beds: the old clips' absolute layout
                    loc = p.copy()
                elif seat_src is not None:
                    loc = SEAT_PELVIS * (target_hip / GUY_HIP) + (p - seat_src) * k
                else:
                    loc = trest_pel + (p - ref_pel) * k
                loc = loc + Vector(spec.get("pelvis_offset", (0, 0, 0)))
            else:
                loc = (pose[parent.name] @ rl[b]).translation
            pose[b] = Matrix.Translation(loc) @ q.to_matrix().to_4x4()
        for b in names:
            pb = tarm.pose.bones[b]
            parent = tarm.data.bones[b].parent
            local = (pose[parent.name].inverted() @ pose[b]) if parent else pose[b]
            basis = rl[b].inverted() @ local
            t, r, _ = basis.decompose()
            if b in prev and prev[b].dot(r) < 0:
                r = -r
            prev[b] = r
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = r
            pb.keyframe_insert("rotation_quaternion", frame=i + 1, group=b)
            if b in ("root", "pelvis") or t.length > 1e-4:
                pb.location = t
                pb.keyframe_insert("location", frame=i + 1, group=b)
    new.use_fake_user = True
    report = {"frames": n, "seconds": round((n - 1) / FPS, 3), "source": os.path.basename(src.path), "action": act.name}
    if spec.get("walk"):
        report["speed_m_s"] = measure_walk(tarm, new, n)
    log("%s <- %s: %s" % (name, os.path.basename(src.path), report))
    return new, report


def measure_walk(arm, act, n):
    """How fast the feet move backward while planted: the actor must move forward at that speed."""
    set_action(arm, act)
    zs, ys = {"l": [], "r": []}, {"l": [], "r": []}
    for i in range(n):
        bpy.context.scene.frame_set(i + 1)
        bpy.context.view_layer.update()
        for s in "lr":
            p = (arm.matrix_world @ arm.pose.bones["ball_" + s].matrix).translation
            zs[s].append(p.z)
            ys[s].append(p.y)
    v = []
    for s in "lr":
        lo = min(zs[s])
        for i in range(n - 1):
            if zs[s][i] < lo + 0.02 and zs[s][i + 1] < lo + 0.02:
                v.append((ys[s][i + 1] - ys[s][i]) * FPS)
    return round(sum(v) / len(v), 3) if v else None


def preview(arm, clips, out):
    """A contact sheet: each clip at 6 moments, front three-quarter view."""
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 300, 420
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.6, 0.62, 1)
    sc.world = w
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 2.1
    sc.collection.objects.link(cam)
    sc.camera = cam
    yaw = math.radians(float(os.environ.get("PREVIEW_YAW", "35")))
    cam.location = (4 * math.sin(yaw), -4 * math.cos(yaw), 0.95)
    cam.rotation_euler = (math.radians(90), 0, yaw)
    os.makedirs(out, exist_ok=True)
    files = []
    for name, act in clips:
        set_action(arm, act)
        n = int(act.frame_range[1])
        for j in range(6):
            sc.frame_set(1 + int((n - 1) * j / 6))
            sc.render.filepath = os.path.join(out, "%s_%d.png" % (name, j))
            bpy.ops.render.render(write_still=True)
            files.append(sc.render.filepath)
    return files


def proxy(arm):
    """One triangle on the pelvis: the glb needs a skinned mesh for the importer to bind the clips to the skeleton."""
    me = bpy.data.meshes.new("LifeProxy")
    me.from_pydata([(0, 0, 0.9), (0.01, 0, 0.9), (0, 0.01, 0.9)], [], [(0, 1, 2)])
    o = bpy.data.objects.new("LifeProxy", me)
    bpy.context.scene.collection.objects.link(o)
    g = o.vertex_groups.new(name="pelvis")
    g.add([0, 1, 2], 1.0, "REPLACE")
    o.parent = arm
    o.modifiers.new("Armature", "ARMATURE").object = arm
    return o


target_hip = None
GUY_HIP = None


def own_config(cid):
    """A character's own seated loops (bake_own_clips.py on its old MakeHuman body) onto its generated body."""
    new = "blender/characters/%s/export/human_%s.glb" % (cid, cid)
    src = "Saved/ClipSources/own_%s.glb" % cid
    cfg = {"target": new, "sources": {"old": {"file": src, "ref": "rest"}}, "clips": {}}
    import struct
    with open(resolve(src), "rb") as f:                       # the glb's animation names, without importing it twice
        f.read(12)
        n, _ = struct.unpack("<II", f.read(8))
        names = [a["name"] for a in json.loads(f.read(n)).get("animations", [])]
    for a in names:
        cfg["clips"][a.split(".")[0]] = {"source": "old", "action": a, "pelvis": "abs"}
    return cfg, "own_" + cid


def main():
    global target_hip, GUY_HIP
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    cfg = json.load(open(os.path.join(HERE, "clips.json")))
    out_name = "life_anims"
    if "--own" in argv:
        cfg, out_name = own_config(argv[argv.index("--own") + 1])
    only = None
    if "--only" in argv:
        only = argv[argv.index("--only") + 1].split(",")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.fps = FPS
    tarm, tobjs, _ = imp(resolve(cfg["target"]))
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    if tarm.animation_data:
        tarm.animation_data_clear()
    for b in tarm.pose.bones:
        b.matrix_basis = Matrix()
    bpy.context.view_layer.update()
    target_hip = tarm.data.bones["thigh_l"].head_local.z
    GUY_HIP = cfg.get("seat_hip", target_hip)
    log("target %s, hip %.3f" % (cfg["target"], target_hip))
    # only the sources the clips being made use (a hand-made source may not exist yet: push_clip.py makes its own)
    used = {c["source"] for n, c in cfg["clips"].items() if not only or n in only}
    sources = {k: Source(v) for k, v in cfg["sources"].items() if k in used}
    made, report = [], {}
    for name, spec in cfg["clips"].items():
        if only and name not in only:
            continue
        act, rep = bake(tarm, sources[spec["source"]], name, spec)
        made.append((name, act))
        report[name] = rep
    os.makedirs(OUT, exist_ok=True)
    # each clip as its own NLA track so the glTF exporter writes them all
    set_action(tarm, None)
    tarm.animation_data_create()
    for name, act in made:
        tr = tarm.animation_data.nla_tracks.new()
        tr.name = act.name
        st = tr.strips.new(act.name, 1, act)
        tr.mute = True
    for b in tarm.pose.bones:
        b.matrix_basis = Matrix()
    if "--preview" in argv:
        files = preview(tarm, made, os.path.join(OUT, "frames"))
        report["_frames"] = [os.path.relpath(f, ROOT) for f in files]
    for o in list(bpy.data.objects):
        if o not in (tarm,):
            bpy.data.objects.remove(o)
    p = proxy(tarm)
    bpy.ops.object.select_all(action="DESELECT")
    tarm.select_set(True)
    p.select_set(True)
    bpy.context.view_layer.objects.active = tarm
    glb = os.path.join(OUT, out_name + ".glb")
    bpy.ops.export_scene.gltf(filepath=glb, use_selection=True, export_format="GLB", export_skins=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS", export_force_sampling=True,
                              export_frame_range=False, export_def_bones=False, export_yup=True, export_morph=False)
    json.dump(report, open(os.path.join(OUT, out_name + "_report.json"), "w"), indent=1)
    log("wrote %s (%d clips)" % (os.path.relpath(glb, ROOT), len(made)))


if __name__ == "__main__":
    main()
