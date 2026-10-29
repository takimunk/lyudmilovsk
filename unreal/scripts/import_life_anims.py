"""Imports the shared human life clips (Saved/LifeAnims/life_anims.glb, made by
blender/humans/retarget_clips.py from blender/humans/clips.json) as /Game/Characters/Humans/Anims/A_Human_<Clip> on SK_Human_Skeleton.

  UnrealEditor <uproject> -run=pythonscript -script="<this file>" -unattended -nullrhi -nosplash

One import per process (Interchange misses a second import in the same process). The glb carries a
one-triangle proxy mesh bound to the shared skeleton; it is deleted after the import. Root motion off.
Each clip's retarget source is SK_Guy_a (the body they are baked on).
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
# LIFE_OWNER=<Name> LIFE_GLB=Saved/LifeAnims/own_<id>.glb: a person's own clips -> /Game/Characters/Humans/<Name>/Anims/
# A_<Name>_<Clip>, retarget source SK_<Name> (blender/humans/bake_own_clips.py + retarget_clips.py --own)
OWNER = os.environ.get("LIFE_OWNER", "")
GLB = os.path.join(ROOT, os.environ.get("LIFE_GLB", os.path.join("Saved", "LifeAnims", "life_anims.glb")))
BASE = "/Game/Characters/Humans"
DEST = BASE + ("/%s/Anims" % OWNER if OWNER else "/Anims")
PREFIX = "A_%s_" % OWNER if OWNER else "A_Human_"
TMP = DEST + "/_import"
SKELETON = BASE + "/SK_Human_Skeleton"
SOURCE_MESH = BASE + ("/%s/SK_%s" % (OWNER, OWNER) if OWNER else "/Guy_a/SK_Guy_a")
PIPELINE = BASE + "/Import/IP_LifeAnims"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
AL = unreal.AnimationLibrary

# Blinks are no longer keyed into the clips: ULyudBodyAnimInstance blinks (and shuts the eyes in sleep) at run time.
EYES = {}


def log(msg):
    unreal.log_warning("LIFEANIMS " + str(msg))


def pipeline():
    if eal.does_asset_exist(PIPELINE):
        eal.delete_asset(PIPELINE)
    src = unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline")
    pipe = tools.duplicate_asset(PIPELINE.split("/")[-1], PIPELINE.rsplit("/", 1)[0], src)
    common = pipe.get_editor_property("common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("skeleton", unreal.load_asset(SKELETON))
    eal.save_loaded_asset(pipe)
    return pipe.get_path_name()


def eye_keys(spec, length):
    if spec == "shut":
        return [0.0, length], [1.0, 1.0]
    if isinstance(spec, tuple):
        kind, t0, t1 = spec
        a, b = (0.0, 1.0) if kind == "close" else (1.0, 0.0)
        ts = [0.0, t0, t1, length]
        return ts, [a, a, b, b]
    ts, vs = [0.0], [0.0]
    for t in spec:
        ts += [t, t + 0.06, t + 0.16]
        vs += [0.0, 1.0, 0.0]
    ts.append(length)
    vs.append(0.0)
    return ts, vs


def add_eyes(anim, clip, skel):
    spec = EYES.get(clip)
    if not spec:
        return
    length = anim.get_play_length()
    ts, vs = eye_keys(spec, length)
    for c in ("eyeBlinkLeft", "eyeBlinkRight"):
        try:
            if AL.does_curve_exist(anim, c, unreal.RawCurveTrackTypes.RCT_FLOAT):
                AL.remove_curve(anim, c)
            AL.add_curve(anim, c, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
            AL.add_float_curve_keys(anim, c, ts, vs)
            try:
                if c not in [str(n) for n in AL.get_curve_meta_data_names(skel)]:
                    AL.add_curve_meta_data(skel, c)
                AL.set_curve_meta_data_morph_target(skel, c, True)
            except Exception as e:                       # metadata API differs between versions
                log("  curve metadata %s: %s" % (c, e))
        except Exception as e:
            log("  WARNING eye curve %s on %s: %s" % (c, clip, e))


def main():
    if not os.path.isfile(GLB):
        raise RuntimeError("missing " + GLB)
    if eal.does_directory_exist(TMP):
        eal.delete_directory(TMP)
    src = unreal.InterchangeManager.create_source_data(GLB)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath("/Interchange/Pipelines/DefaultGLTFPipeline.DefaultGLTFPipeline"))
    params.override_pipelines.append(unreal.SoftObjectPath(pipeline()))
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(TMP, src, params)
    mgr.wait_until_all_tasks_done(True)

    skel = unreal.load_asset(SKELETON)
    src_mesh = unreal.load_asset(SOURCE_MESH)
    made = []
    for p in eal.list_assets(TMP, True):
        a = eal.load_asset(p)
        if not isinstance(a, unreal.AnimSequence):
            continue
        nm = a.get_name()
        # a glb of one clip (retarget_clips.py --only X) comes in named after the file: LIFE_ONLY=X names it
        if "Life_" not in nm and os.environ.get("LIFE_ONLY"):
            nm = "Life_" + os.environ["LIFE_ONLY"]
        if "Life_" not in nm:
            log("  skipping %s" % nm)
            continue
        clip = nm[nm.find("Life_") + 5:]
        new = "%s/%s%s" % (DEST, PREFIX, clip)
        if eal.does_asset_exist(new):
            eal.delete_asset(new)
        eal.rename_asset(p, new)
        anim = unreal.load_asset(new)
        if anim.get_skeleton().get_path_name() != skel.get_path_name():
            log("  WARNING %s is on %s" % (clip, anim.get_skeleton().get_path_name()))
        anim.set_editor_property("enable_root_motion", False)
        if src_mesh:
            anim.set_retarget_source_asset(src_mesh)
        add_eyes(anim, clip, skel)
        eal.save_loaded_asset(anim)
        made.append((new, round(anim.get_play_length(), 3), anim.get_editor_property("number_of_sampled_frames")
                     if hasattr(anim, "number_of_sampled_frames") else None))
    for p in eal.list_assets(TMP, True):
        eal.delete_asset(p)
    eal.delete_directory(TMP)
    if eal.does_asset_exist(PIPELINE):
        eal.delete_asset(PIPELINE)
    eal.save_directory(DEST)
    eal.save_loaded_asset(skel)
    for m in made:
        log("made %s length %s s" % (m[0], m[1]))
    log("done %d clips" % len(made))


main()
