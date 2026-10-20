"""Builds the realistic Lyudmilovsk humans from MakeHuman (MPFB2) assets.

    Blender -b -P blender/humans/build_humans.py -- zoomer clerk [--preview] [--no-export]

For each character: blender/humans/<name>.blend and blender/humans/export/human_<name>.glb
(skeleton root/pelvis/spine_01..., 15 finger bones per hand, jaw + eye_l/eye_r, 52 ARKit + 15 viseme
shape keys). --preview renders renders/humans/<name>_{front,side,face}.png.
Binaries stay local (git LFS quota); everything is rebuilt from this script + the asset packs.
"""
import math
import os
import sys

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)

import mpfb_pipeline as P  # noqa: E402
import tailor  # noqa: E402
import preview  # noqa: E402
import seat_anims  # noqa: E402

EXPORT = os.path.join(HERE, "export")
RENDERS = os.path.join(ROOT, "renders", "humans")

TORSO = ["pelvis", "spine_01", "spine_02", "spine_03", "clavicle_l", "clavicle_r"]
ARMS = ["upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r"]
HEAD = ["head", "neck_01", "jaw"]


def wsum(w, names):
    out = None
    for n in names:
        if n in w:
            out = w[n] if out is None else out + w[n]
    return np.zeros(0) if out is None else np.clip(out, 0, 1)


def _z(co):
    return co[:, 2]


# ------------------------------------------------------------------ garment rules (metres)

def oversized_hoodie(co, w):
    torso, arms, head = wsum(w, TORSO), wsum(w, ARMS), wsum(w, HEAD)
    z = _z(co)
    hem = np.clip((1.0 - z) / 0.15, 0, 1)               # hangs loose below the belt
    return 0.042 * torso + 0.034 * arms + 0.025 * hem + 0.012 * head


def hoodie_blend(co, w):
    head = wsum(w, HEAD)
    return 1.0 - np.clip(head * 1.2, 0, 1)              # the hood keeps its own shape


def wide_jeans(co, w):
    z = _z(co)
    t = np.clip((0.85 - z) / 0.75, 0, 1)                # 0 at the hips -> 1 at the ankle
    return 0.010 + 0.050 * t ** 0.8


def baggy_tee(co, w):
    return 0.02 * wsum(w, TORSO) + 0.015 * wsum(w, ARMS) + 0.006


# --------------------------------------------------------------------------- characters

CHARACTERS = {
    # Hero: Russian gen-z guy, ~20, oversized hoodie, wide jeans, chunky white sneakers
    "zoomer": dict(
        macro=dict(gender=1.0, age=0.47, muscle=0.45, weight=0.42, height=0.53, proportions=0.6,
                   race=dict(caucasian=0.95, asian=0.05, african=0.0)),
        targets={"head/head-square": 0.35, "chin/chin-width-incr": 0.4, "chin/chin-prominent-incr": 0.3,
                 "chin/chin-bones-incr": 0.3},
        skin="young_caucasian_male", hair="culturalibre_hair_05", eyebrows="eyebrow010",
        clothes=["elvs_hooded_sweat_jacket1", "punkduck_male_classic_jeans", "punkduck_tennis_shoes"],
        tailor={"elvs_hooded_sweat_jacket1": (oversized_hoodie, hoodie_blend),
                "punkduck_male_classic_jeans": (wide_jeans, None)},
        tint={"elvs_hooded_sweat_jacket1": ((0.30, 0.36, 0.26), 1.0, 0.3),
              "punkduck_male_classic_jeans": ((0.32, 0.42, 0.55), 0.6, 1.0),
              "hair": ((0.16, 0.11, 0.07), 0.7, 1.2)},
        height=1.80),
    # Station 09 clerk: Japanese office worker ~45, shirt + tie, black arm covers, glasses
    "clerk": dict(
        macro=dict(gender=1.0, age=0.655, muscle=0.35, weight=0.58, height=0.5, proportions=0.45,
                   race=dict(asian=0.95, caucasian=0.05, african=0.0)),
        targets={"stomach/stomach-pregnant-incr": 0.25, "head/head-age-incr": 0.6,
                 "eyes/l-eye-bag-incr": 0.5, "eyes/r-eye-bag-incr": 0.5, "head/head-fat-incr": 0.25},
        skin="middleage_asian_male", hair="short01", eyebrows="eyebrow002",
        clothes=["elvs_male_shirt_tie_tucked1", "elvs_male_trouser", "mindfront_shoes_oxford_male",
                 "spamrakuen_tbm_glasses_frames_01"],
        shirt_tie=("elvs_male_shirt_tie_tucked1", ((0.90, 0.91, 0.90), (0.06, 0.08, 0.20), (0.45, 0.10, 0.10))),
        rough={"mindfront_shoes_oxford_male": 0.35},
        tint={"elvs_male_trouser": ((0.20, 0.21, 0.24), 0.9, 1.0),
              "hair": ((0.05, 0.045, 0.04), 0.9, 1.0)},
        arm_covers="elvs_male_shirt_tie_tucked1",
        height=1.68),
    # Club owner: grey-haired Japanese man ~70 with a full head of hair, sits in his booth
    "owner": dict(
        macro=dict(gender=1.0, age=0.86, muscle=0.35, weight=0.5, height=0.42, proportions=0.45,
                   race=dict(asian=0.95, caucasian=0.05, african=0.0)),
        targets={"head/head-age-incr": 1.0, "eyes/l-eye-bag-incr": 0.7, "eyes/r-eye-bag-incr": 0.7,
                 "head/head-square": 0.3},
        skin="old_asian_male", hair="culturalibre_hair_05", eyebrows="eyebrow006",
        clothes=["toigo_fisherman_sweater", "elvs_male_trouser", "mindfront_shoes_monk_strap_male"],
        tint={"toigo_fisherman_sweater": ((0.17, 0.13, 0.10), 1.0, 0.8),
              "elvs_male_trouser": ((0.32, 0.30, 0.26), 0.9, 1.0),
              "hair": ((0.80, 0.80, 0.78), 1.0, 0.9), "eyebrows": ((0.75, 0.74, 0.72), 1.0, 1.0)}),
    # --- the crowd at the other computers (a few bodies x outfits) ---
    "streamer": dict(
        macro=dict(gender=0.0, age=0.44, muscle=0.4, weight=0.45, height=0.5, proportions=0.6,
                   race=dict(asian=0.5, caucasian=0.5, african=0.0)),
        skin="young_asian_female", hair="o4saken_long01", eyebrows="eyebrow009", eyelashes="eyelashes02",
        clothes=["punkduck_high_neck_crop_top", "punkduck_female_tight_jeans", "punkduck_comfortable_sneakers"],
        tint={"hair": ((0.78, 0.55, 0.75), 0.85, 1.0),
              "punkduck_female_tight_jeans": ((0.12, 0.12, 0.14), 0.9, 1.0)},
        headset=True),
    "kid_boy": dict(
        macro=dict(gender=1.0, age=0.235, muscle=0.45, weight=0.45, height=0.5,
                   race=dict(caucasian=0.95, asian=0.05, african=0.0)),
        skin="young_caucasian_male", hair="short02", eyebrows="eyebrow001",
        clothes=["toigo_basic_tucked_t-shirt", "punkduck_male_classic_jeans", "punkduck_running_shoes_01"],
        tailor={"toigo_basic_tucked_t-shirt": (baggy_tee, None)},
        tint={"toigo_basic_tucked_t-shirt": ((0.75, 0.20, 0.18), 1.0, 0.6),
              "hair": ((0.35, 0.24, 0.14), 0.8, 1.0)}),
    "kid_girl": dict(
        macro=dict(gender=0.0, age=0.225, muscle=0.45, weight=0.45, height=0.5,
                   race=dict(caucasian=0.9, asian=0.1, african=0.0)),
        skin="young_caucasian_female", hair="ponytail01", eyebrows="eyebrow003",
        clothes=["joepal_crude_t-shirt_female", "punkduck_female_tight_jeans", "punkduck_comfortable_sneakers"],
        tint={"joepal_crude_t-shirt_female": ((0.55, 0.65, 0.85), 1.0, 0.6),
              "hair": ((0.55, 0.40, 0.22), 0.8, 1.0)}),
    "guy_a": dict(
        macro=dict(gender=1.0, age=0.48, muscle=0.5, weight=0.55, height=0.5,
                   race=dict(caucasian=0.9, asian=0.1, african=0.0)),
        targets={"head/head-round": 0.3},
        skin="young_caucasian_male2", hair="short04", eyebrows="eyebrow004",
        clothes=["elvs_male_logo_tshirt1", "mindfront_male_trousers_1", "shoes05"],
        tailor={"elvs_male_logo_tshirt1": (baggy_tee, None)}),
    "guy_b": dict(
        macro=dict(gender=1.0, age=0.5, muscle=0.45, weight=0.4, height=0.55,
                   race=dict(asian=0.85, caucasian=0.15, african=0.0)),
        skin="young_asian_male", hair="short02", eyebrows="eyebrow002",
        clothes=["namuhekam_male_polo_shirt", "punkduck_male_classic_jeans", "shoes02"],
        tint={"namuhekam_male_polo_shirt": ((0.12, 0.14, 0.18), 1.0, 0.6),
              "hair": ((0.06, 0.05, 0.05), 0.9, 1.0)}),
    "girl_a": dict(
        macro=dict(gender=0.0, age=0.46, muscle=0.4, weight=0.5, height=0.45,
                   race=dict(caucasian=0.95, asian=0.05, african=0.0)),
        skin="young_caucasian_female2", hair="toigo_blunt_bob_with_bangs", eyebrows="eyebrow008",
        eyelashes="eyelashes03",
        clothes=["punkduck_off-shoulder_long-sleeve_top", "punkduck_female_tight_jeans", "punkduck_comfortable_sneakers"],
        tint={"punkduck_off-shoulder_long-sleeve_top": ((0.85, 0.82, 0.75), 0.8, 0.8),
              "hair": ((0.10, 0.07, 0.05), 0.9, 1.0)}),
    "girl_b": dict(
        macro=dict(gender=0.0, age=0.5, muscle=0.4, weight=0.55, height=0.5,
                   race=dict(caucasian=0.8, asian=0.2, african=0.0)),
        skin="young_caucasian_female", hair="long01", eyebrows="eyebrow007",
        clothes=["elvs_hooded_sweat_jacket1", "punkduck_female_tight_jeans", "punkduck_running_shoes_01"],
        tailor={"elvs_hooded_sweat_jacket1": (oversized_hoodie, hoodie_blend)},
        tint={"elvs_hooded_sweat_jacket1": ((0.62, 0.58, 0.66), 1.0, 0.3),
              "hair": ((0.45, 0.30, 0.15), 0.8, 1.0)}),
}


# --------------------------------------------------------------------------------- extras

def arm_covers(shirt, name, rgb=(0.04, 0.04, 0.045)):
    """Japanese office sleeve covers (udenuki): a cylinder of the shirt sleeve from the wrist to
    just below the elbow, pushed out a little, elastic at both ends."""
    import bmesh
    lw = wsum({g.name: tailor.group_weights(shirt, [g.name]) for g in shirt.vertex_groups},
              ["lowerarm_l", "lowerarm_r", "hand_l", "hand_r"])
    bm = bmesh.new()
    bm.from_mesh(shirt.data)
    bm.verts.ensure_lookup_table()
    keep = [f for f in bm.faces if all(lw[v.index] > 0.55 for v in f.verts)]
    drop = [f for f in bm.faces if f not in set(keep)]
    bmesh.ops.delete(bm, geom=drop, context="FACES")
    for v in bm.verts:
        v.co += v.normal * 0.006
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    shirt.users_collection[0].objects.link(ob)
    ob.parent = shirt.parent
    ob.matrix_world = shirt.matrix_world
    for g in shirt.vertex_groups:            # same skin weights as the shirt (vertex order kept)
        ob.vertex_groups.new(name=g.name)
    # bmesh delete keeps the order of the remaining verts; re-transfer weights by nearest vertex
    dt = ob.modifiers.new("dt", "DATA_TRANSFER")
    dt.object = shirt
    dt.use_vert_data = True
    dt.data_types_verts = {"VGROUP_WEIGHTS"}
    dt.vert_mapping = "NEAREST"
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier="dt")
    arm = ob.modifiers.new("Armature", "ARMATURE")
    arm.object = shirt.parent
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Roughness"].default_value = 0.75
    bsdf.inputs["Sheen Weight"].default_value = 0.3
    me.materials.append(mat)
    return ob


ROUGH = {"skin": 0.55, "hair": 0.5, "eyes": 0.08, "teeth": 0.3, "tongue": 0.45, "eyebrows": 0.7,
         "eyelashes": 0.7}


def finish_materials(bm, objs, spec):
    tailor.set_roughness(bm, ROUGH["skin"])
    for key, ob in objs.items():
        if ob is None:
            continue
        r = spec.get("rough", {}).get(key, ROUGH.get(key))
        if r is None:                    # garments: matte cloth unless the spec says otherwise
            r = 0.3 if "glasses" in key else 0.88
            if "glasses" not in key:
                tailor.opaque(ob)
            tailor.set_roughness(ob, r, sheen=0.0 if "glasses" in key else 0.25)
        else:
            tailor.set_roughness(ob, r)


def _tube(name, pts, radius):
    """Mesh tube along pts (a bevelled poly curve converted to mesh)."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 3
    sp = cu.splines.new("POLY")
    sp.points.add(len(pts) - 1)
    for p, c in zip(sp.points, pts):
        p.co = (*c, 1)
    cu.use_fill_caps = True
    ob = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(ob)
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    bpy.data.objects.remove(ob)
    return me


def headset(bm, arm, name, rgb=(0.05, 0.05, 0.06)):
    """Gaming headset with a boom mic, rigidly skinned to the head bone, sized from the head."""
    import bmesh
    from mathutils import Matrix, Vector
    dg = bpy.context.evaluated_depsgraph_get()
    ev = bm.evaluated_get(dg)
    head = arm.matrix_world @ arm.data.bones["head"].head_local
    co = [arm.matrix_world @ v.co for v in ev.data.vertices]
    ear_z = head.z + 0.045
    band = [c for c in co if abs(c.z - ear_z) < 0.01 and abs(c.y - head.y) < 0.03]
    ear_x = max(abs(c.x) for c in band) if band else 0.075
    skull = max(c.z for c in co if abs(c.x) < 0.02 and abs(c.y - head.y) < 0.03)
    front = min(c.y for c in co if abs(c.x) < 0.01 and abs(c.z - (head.z - 0.03)) < 0.01)
    cy = head.y
    pts = [(math.cos(a) * (ear_x + 0.016), cy, ear_z + math.sin(a) * (skull + 0.012 - ear_z))
           for a in [math.pi * k / 24 for k in range(25)]]
    bmh = bmesh.new()
    bmh.from_mesh(_tube(name + "_band", pts, 0.008))
    for sx in (-1, 1):
        bmesh.ops.create_cone(bmh, cap_ends=True, segments=24, radius1=0.043, radius2=0.04, depth=0.034,
                              matrix=Matrix.LocRotScale(Vector((sx * (ear_x + 0.016), cy, ear_z - 0.005)),
                                                        Vector((0, 0, 1)).rotation_difference(Vector((sx, 0, 0))), None))
    mouth = Vector((0.025, front - 0.012, head.z - 0.03))
    p0 = Vector((ear_x + 0.03, cy - 0.01, ear_z - 0.03))
    mid = Vector((ear_x * 0.8, (cy + mouth.y) / 2 - 0.02, (p0.z + mouth.z) / 2))
    bmh.from_mesh(_tube(name + "_boom", [tuple(p0), tuple(mid), tuple(mouth)], 0.0035))
    bmesh.ops.create_uvsphere(bmh, u_segments=12, v_segments=8, radius=0.009, matrix=Matrix.Translation(mouth))
    me = bpy.data.meshes.new(name)
    bmh.to_mesh(me)
    bmh.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    bm.users_collection[0].objects.link(ob)
    ob.parent = arm
    ob.matrix_world = arm.matrix_world.inverted() @ arm.matrix_world  # identity relative to world
    ob.matrix_parent_inverse = arm.matrix_world.inverted()
    vg = ob.vertex_groups.new(name="head")
    vg.add(list(range(len(me.vertices))), 1.0, "REPLACE")
    ob.modifiers.new("Armature", "ARMATURE").object = arm
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    b = mat.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Roughness"].default_value = 0.4
    me.materials.append(mat)
    return ob


def build_one(name, spec, do_preview, do_export):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.preferences.addon_enable(module="bl_ext.user_default.mpfb")
    s = dict(spec, name=name)
    bm, arm, objs = P.build(s)
    for folder, (rule, blend_rule) in spec.get("tailor", {}).items():
        g = objs[folder]
        blend = None
        if blend_rule:
            co = np.array([tuple(g.matrix_world @ v.co) for v in g.data.vertices])
            w = {gr.name: tailor.group_weights(g, [gr.name]) for gr in g.vertex_groups}
            blend = blend_rule(co, w)
        tailor.loosen(g, bm, rule, blend=blend)
    for key, (rgb, strength, contrast) in spec.get("tint", {}).items():
        tailor.tint_object(objs[key], rgb, strength, contrast, name="%s_%s" % (name, key))
        tailor.drop_normal_maps(objs[key])          # their baked wear/fold maps read as dirt
    if spec.get("shirt_tie"):
        folder, (shirt_rgb, tie_rgb, stripe_rgb) = spec["shirt_tie"]
        shirt = objs[folder]
        tie = tailor.split_tie(shirt, name + "_tie")
        tailor.flat_fabric(shirt.data.materials[0], shirt_rgb, name + "_shirt_tex", noise=0.03)
        tailor.flat_fabric(tie, tie_rgb, name + "_tie_tex", noise=0.04, stripes=(22, 4, stripe_rgb))
    tailor.recolour_saturated(objs["eyes"], spec.get("iris", (0.20, 0.11, 0.05)), 0.15, name + "_eyes")
    finish_materials(bm, objs, spec)
    if spec.get("headset"):
        objs["headset"] = headset(bm, arm, name + ".headset")
    if spec.get("arm_covers"):
        objs["arm_covers"] = arm_covers(objs[spec["arm_covers"]], name + ".arm_covers")
    if "--no-anims" not in sys.argv:
        seat_anims.bake_all(arm)
    os.makedirs(HERE, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, name + ".blend"))
    if do_preview:
        os.makedirs(RENDERS, exist_ok=True)
        ev = bm.evaluated_get(bpy.context.evaluated_depsgraph_get())
        preview.standard_shots(os.path.join(RENDERS, name),
                               height=max((bm.matrix_world @ v.co).z for v in ev.data.vertices))
    if do_export:
        root, _ = P.bake_for_export(bm)
        P.export_glb(root, os.path.join(EXPORT, "human_%s.glb" % name),
                     actions=[a for a in bpy.data.actions if a.name.startswith("Sit_")])
    ev = bm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    top = max((bm.matrix_world @ v.co).z for v in ev.data.vertices)
    print("HUMAN %s height %.3f m, %d bones" % (name, top, len(arm.data.bones)))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    names = [a for a in argv if not a.startswith("--")] or list(CHARACTERS)
    for n in names:
        build_one(n, CHARACTERS[n], "--preview" in argv, "--no-export" not in argv)


if __name__ == "__main__":
    main()
