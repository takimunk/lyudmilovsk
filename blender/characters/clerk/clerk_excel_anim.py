"""Excel-painting loop for the realistic clerk (blender/humans clerk, shared UE human skeleton).

Bakes Sit_Excel (8 s) on the clerk's own rig with the seated-loop helpers of
blender/humans/seat_anims.py, fitted to Station 09 of Scene 01: right hand on his ball mouse,
moving it cell by cell and clicking, a trip to the toolbar for the fill colour, left hand resting
on the keyboard, eyes following the cursor. The mouse itself is the separate SK_ClerkDesk
(build_clerk.py --desk), animated with the same path; both loops are 8 s and start together.

Writes blender/characters/clerk/clerk_excel.blend and blender/export/clerk_excel_anim.glb
(armature + Sit_Excel only; imported onto /Game/Characters/Humans/SK_Human_Skeleton).

    Blender -b -P blender/characters/clerk/clerk_excel_anim.py [-- --preview]
"""
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
HUMANS = os.environ.get("HUMANS_DIR") or next(
    p for p in (os.path.join(ROOT, "blender", "humans"),
                os.path.expanduser("~/projects/lyudmilovsk-wt/humans/blender/humans"))
    if os.path.exists(os.path.join(p, "clerk.blend")))
sys.path.insert(0, HUMANS)
sys.path.insert(0, HERE)
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

bpy.ops.wm.open_mainfile(filepath=os.path.join(HUMANS, "clerk.blend"))
import seat_anims as S  # noqa: E402
from excel_path import CLICKS, LOOP, mouse_at, pulse  # noqa: E402

# Station 09 (layout "clerk"), character space: origin on the floor under the seat, facing -Y.
S.SEAT, S.DESK = 0.478, 0.762
S.KEYS = Vector((0.0, -0.56, S.DESK + 0.02))
S.MOUSE = Vector((-0.32, -0.565, S.DESK + 0.02))
S.SCREEN = Vector((0.0, -0.69, 1.013))
GAZE0 = S.SCREEN + Vector((0.02, 0.0, -0.01))
DOWN, FWD = Vector((0, 0, -1)), Vector((0, -1, 0))
W = S._wave


def excel(r, t, T):
    m, moving, dx = mouse_at(t)
    ahead, _, _ = mouse_at(t + 0.18)
    click = pulse(t, CLICKS, 0.04, 0.05, 0.06)
    wr = {"l": S.KEYS + Vector((0.13, 0.085, 0.032)),
          "r": S.MOUSE + Vector((0.004, 0.072, 0.046 + 0.003 * moving)) + Vector((m[0], m[1], 0))}
    hands = {"l": (FWD + Vector((-0.25, 0, -0.12)), DOWN),
             "r": (FWD + Vector((0.05 + 0.4 * m[0], 0, -0.22)), Vector((1.0, 0, -0.35)))}
    curls = {("r", "index"): 0.28 + 0.22 * click, ("r", "middle"): 0.36, ("r", "thumb"): 0.2,
             ("r", "ring"): 0.6, ("r", "pinky"): 0.72}
    curls.update({("l", f): 0.42 for f in S.FINGERS})
    look = GAZE0 + Vector((ahead[0] * 6.5, 0, -ahead[1] * 6.5))
    r.place(r.f, wrists=wr, hands=hands, lean=0.24, tilt=0.06, hip_back=-0.07, feet_fwd=0.46, look=look,
            curls=curls, elbows_out=0.12, breath=0.02 * W(t, T / 2))


S.LOOPS["Sit_Excel"] = LOOP
S.MAKERS["Sit_Excel"] = excel


def main():
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    for a in [a for a in bpy.data.actions if a.name.startswith("Sit_")]:
        a.use_fake_user = False
    S.bake_all(arm, ["Sit_Excel"])
    act = bpy.data.actions["Sit_Excel"]
    arm.animation_data.action = act
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "clerk_excel.blend"))
    # armature-only glb with the one action, for the shared UE skeleton
    out = os.path.join(ROOT, "blender", "export", "clerk_excel_anim.glb")
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    # Interchange imports animation only along with a skinned mesh: carry the teeth (dropped in UE)
    next(o for o in arm.children if "teeth" in o.name).select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_format="GLB",
                              export_skins=True, export_animations=True, export_animation_mode="ACTIONS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_anim_single_armature=True, export_reset_pose_bones=True,
                              export_def_bones=False, export_yup=True, export_apply=False)
    print("EXCEL ANIM", out)
    if "--preview" in ARGS:
        import preview_station
        preview_station.render(arm, act, os.path.join(ROOT, "renders", "clerk_real"), S, [0, 18, 75, 130, 200])


main()
