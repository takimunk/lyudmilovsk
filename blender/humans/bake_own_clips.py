"""Bakes a character's own seated loops (the club computers: Sit_KeyboardMouse, Sit_Gamepad, Sit_Browse, Sit_Idle, ...)
on its old MakeHuman body, where seat_anims.py's IK set-up was made (hand frames, knee poles), for
blender/humans/retarget_clips.py --own <id> to carry them over in world space onto the generated body.

    Blender -b <humans worktree>/blender/humans/<id>.blend -P blender/humans/bake_own_clips.py -- <id>

Which loops: the ones the game has (/Game/Characters/Humans/<Name>/Anims/A_<Name>_Sit_*); the sitting style from
build_humans.py's CHARACTERS entry. Played as local rotations on the new T-pose rigs these clips twisted the
wrists, and baking them straight on the new rig doesn't work either (seat_anims aims the hands by MakeHuman's
bone axes). Writes Saved/ClipSources/own_<id>.glb (armature only, actions Sit_*).
"""
import ast
import os
import sys

import bpy
from mathutils import Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import seat_anims  # noqa: E402
import stand_anims  # noqa: E402,F401  (registers the owner's loops)


def spec_of(cid):
    """loops and style of build_humans.CHARACTERS[cid], read without running build_humans (it needs MPFB)."""
    tree = ast.parse(open(os.path.join(HERE, "build_humans.py")).read())
    for node in tree.body:
        if isinstance(node, ast.Assign) and getattr(node.targets[0], "id", "") == "CHARACTERS":
            for k, v in zip(node.value.keys, node.value.values):
                if ast.literal_eval(k) == cid:
                    kw = {a.arg: a.value for a in v.keywords}
                    return {n: ast.literal_eval(kw[n]) for n in ("loops", "style") if n in kw}
    return {}


def main():
    cid = sys.argv[sys.argv.index("--") + 1]
    name = cid.capitalize()
    anims = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Characters", "Humans", name, "Anims")
    have = sorted(f[len("A_%s_" % name):-7] for f in os.listdir(anims) if f.startswith("A_%s_Sit_" % name)) \
        if os.path.isdir(anims) else []
    spec = spec_of(cid)
    loops = [l for l in have if l in seat_anims.MAKERS] or spec.get("loops", [])
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    if arm.animation_data:
        arm.animation_data_clear()
    for a in list(bpy.data.actions):                          # the blend keeps the loops of its own build
        bpy.data.actions.remove(a)
    for b in arm.pose.bones:
        b.matrix_basis = Matrix()
    print("OWNCLIPS %s: loops %s, style %s" % (cid, loops, spec.get("style")))
    acts = seat_anims.bake_all(arm, loops, spec.get("style"))
    arm.animation_data_create()
    for a in acts:
        tr = arm.animation_data.nla_tracks.new()
        tr.name = a.name
        tr.strips.new(a.name, 1, a)
        tr.mute = True
    out = os.path.join(ROOT, "Saved", "ClipSources", "own_%s.glb" % cid)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    body = max((o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm), key=lambda o: len(o.data.vertices))
    body.select_set(True)                                     # a skinned mesh makes the importer build an armature
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB", export_skins=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS", export_force_sampling=True,
                              export_def_bones=False, export_yup=True, export_morph=False, export_materials="NONE")
    print("OWNCLIPS wrote %s (%d loops)" % (os.path.relpath(out, ROOT), len(acts)))


main()
