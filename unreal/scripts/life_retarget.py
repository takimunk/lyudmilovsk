"""Makes the shared A_Human_* clips fit every human body on SK_Human_Skeleton.

The clips are baked on Guy_a (1.69 m). With the skeleton's default translation retargeting
(Animation for every bone) a kid playing them would get Guy_a's bone lengths. This sets
  root -> Animation, pelvis -> AnimationScaled, every other bone -> Skeleton
on SK_Human_Skeleton, and points every AnimSequence's retarget source at its own mesh
(A_Human_* -> SK_Guy_a, A_<Name>_* -> SK_<Name>), so the per-character Sit_* clips stay exact
(AnimationScaled with source = target scales by 1).

  UnrealEditor <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import unreal

eal = unreal.EditorAssetLibrary
BASE = "/Game/Characters/Humans"
SKELETON = BASE + "/SK_Human_Skeleton"


def log(m):
    unreal.log_warning("LIFERETARGET " + str(m))


def set_modes(sk):
    """Through ULyudAnimTools (C++): FBoneNode is read-only from Python."""
    T = unreal.LyudAnimTools
    T.set_bone_translation_retargeting(sk, "root", 1, True)        # everything Skeleton ...
    T.set_bone_translation_retargeting(sk, "root", 0, False)       # ... root Animation
    T.set_bone_translation_retargeting(sk, "pelvis", 2, False)     # ... pelvis AnimationScaled
    log("modes: %s" % [(b, T.get_bone_translation_retargeting(sk, b)) for b in ("root", "pelvis", "spine_01", "thigh_l", "hand_r")])
    eal.save_loaded_asset(sk)


def set_sources():
    for p in eal.list_assets(BASE, True):
        a = eal.load_asset(p) if p.split(".")[0].split("/")[-1].startswith("A_") else None
        if not isinstance(a, unreal.AnimSequence):
            continue
        nm = a.get_name()
        owner = "Guy_a" if nm.startswith("A_Human_") else nm[2:].split("_Sit_")[0].split("_Stand")[0]
        for suffix in ("_Walk", "_SitDown", "_StandUp", "_StandLook"):
            if owner.endswith(suffix):
                owner = owner[: -len(suffix)]
        mesh = unreal.load_asset("%s/%s/SK_%s" % (BASE, owner, owner))
        if not mesh:
            log("  no mesh for %s (%s)" % (nm, owner))
            continue
        a.set_retarget_source_asset(mesh)
        eal.save_loaded_asset(a)
        log("  %s -> %s" % (nm, mesh.get_name()))


sk = unreal.load_asset(SKELETON)
try:
    set_modes(sk)
except Exception as e:
    log("FAILED to set retargeting modes from python: %s" % e)
set_sources()
log("done")
