"""The snow pusher clip: a janitor pushing a wide scraper along a path (docs/winter.md). The legs and the lean are
UAL's Push_Loop (already on the clips' body by retarget_clips.py --only PushSource); the arms are redone by IK so
both hands hold the pusher's handle bar at the hips, a little in front, palms down (the source pushes a wall at
head height). Writes Saved/ClipSources/push_scraper.glb (action Hand_Push), which clips.json's Push reads.

    Blender -b -P blender/humans/retarget_clips.py -- --only PushSource
    Blender -b -P blender/humans/push_clip.py
    Blender -b -P blender/humans/retarget_clips.py -- --only Push --preview

Blender character space: faces -Y, its left is +X, up +Z; metres.
"""
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import poser as P  # noqa: E402

SRC = os.path.join(P.ROOT, "Saved", "LifeAnims", "life_anims.glb")
OUT = os.path.join(P.ROOT, "Saved", "ClipSources", "push_scraper.glb")
# the handle bar: centre in front of the pelvis, height over it; the hands apart on it
GRIP_AHEAD, GRIP_UP, GRIP_HALF = 0.40, 0.06, 0.21


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = P.FPS
    bpy.ops.import_scene.gltf(filepath=SRC, bone_heuristic="BLENDER")
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    act = next(a for a in bpy.data.actions if "PushSource" in a.name)
    arm.animation_data_create()
    arm.animation_data.nla_tracks.clear() if hasattr(arm.animation_data.nla_tracks, "clear") else None
    for tr in list(arm.animation_data.nla_tracks):
        arm.animation_data.nla_tracks.remove(tr)
    arm.animation_data.action = act
    p = P.Poser(arm)
    n = int(act.frame_range[1] - act.frame_range[0]) + 1
    frames = []
    for i in range(n):
        bpy.context.scene.frame_set(int(act.frame_range[0]) + i)
        bpy.context.view_layer.update()
        snap = p.snapshot()
        arm.animation_data.action = None
        p.apply(snap)
        pel = p.head("pelvis")
        centre = Vector((0.0, pel.y - GRIP_AHEAD, pel.z + GRIP_UP))
        for side, sx in (("l", 1.0), ("r", -1.0)):
            grip = centre + Vector((sx * GRIP_HALF, 0.0, 0.0))
            # the elbows bend down and out, the hand's fingers wrap over the bar (palm down, fingers forward)
            p.ik("upperarm_" + side, "lowerarm_" + side, "hand_" + side, grip, (sx * 0.6, 0.35, -1.0))
            p.hand(side, (sx * -0.25, -0.75, -0.6), (0.0, -0.3, -1.0))
            p.curl(side, 72.0, thumb=35.0)
        frames.append(p.snapshot())
        arm.animation_data.action = act
    arm.animation_data.action = None
    p.apply(frames[0])
    baked = P.bake(arm, p, "Push", frames)
    P.export(arm, [baked], OUT)
    print("PUSHCLIP wrote %s (%d frames)" % (os.path.relpath(OUT, P.ROOT), n), flush=True)


main()
