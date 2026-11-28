"""The yacht school's body clips (docs/yacht_school.md "Clips (part D)"): sailing a dinghy, capsizing and righting it,
the shore work round the boats, the class and the coach. Posed on the clips' body (Guy_a) with poser.py, carried over
to /Game/Characters/Humans/Anims/A_Human_Sail_* by retarget_clips.py (clips.json source "sail", pelvis "abs").

    Blender -b -P blender/humans/sail_clips.py -- [--only Sail_Helm,Sail_Tack] [--preview DIR] [--standin] [--no-export]

    --only      clip base names (the _Port/_Stbd pair of a boat clip comes together)
    --preview   contact sheets DIR/<Clip>.png: side, front and top at 5 moments, a stand-in boat built from the numbers
    --standin   pose on standin_rig.py's body instead of Guy_a's glb (the cloud; check only, never import that bake)
    --no-export skip Saved/ClipSources/sail_clips.glb

Writes Saved/ClipSources/sail_clips.glb (one action S_<Clip> per clip) and blender/humans/sail_grips.json: for every
clip the hand targets the engine's hand IK takes over (ULyudBodyAnimInstance::SetBikeRider hands, docs), in UE mesh
space (cm: +Y the way the body faces, +X its left, +Z up; origin = the actor's origin = the clip's origin).

Frames (Blender character space: the body faces -Y, its left is +X, up +Z; metres):
  boat clips     origin on the boat's centreline at the crew station, on the cockpit sole; the bow -Y, port +X. The
                 actor is attached to the boat's crew socket facing the bow (UE: actor yaw = boat yaw - 90, like any
                 human mesh). _Port: the body on the port side deck (the wind over port), the tiller in the right
                 hand; _Stbd: its mirror image (made by posing mirrored, not by flipping bones).
  capsized       origin where the centreboard leaves the hull, on the water's surface; the hull lies on its side to -X,
                 the board sticks out horizontally along +X, the up gunwale is over -X.
  shore, coach   origin on the ground between the feet (standing) or under the house chair's seat centre (seated: seat
                 top 0.47, the shared clips' seated pelvis (0, -0.01, 0.567)), like every life clip.
Boat numbers are BOATS below (real dimensions), replaced by blender/sailing/sockets.json (part C) when it is there:
re-run this script and the bake when the boats change. A child's boat (optimist; the cadet's crew) is posed for a
grown-up's body in a boat scaled up by 1/scale, so a child body of that scale (SK_Human_Skeleton scaled; ANpcBody's
BodyScale) fits the real boat.
"""
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import poser  # noqa: E402
from poser import FPS, ROOT, Poser  # noqa: E402
from sail_core import BOATS, CLIPS, GRIPS, OTHER, R, check, load_sockets, log  # noqa: E402
import sail_clips_boat  # noqa: E402,F401  (the boat clips register themselves)
import sail_clips_shore  # noqa: E402,F401  (the shore, class and coach clips)

# ---------------------------------------------------------------------------------------------- main

def variants(name, spec):
    if spec["mirror"]:
        return [(name + "_Port", 1), (name + "_Stbd", -1)]
    return [(name, 1)]


def to_ue(v, cm=True):
    """Blender character space (m) -> UE mesh space (cm): x stays (its left), y flips (UE meshes face +Y), z up."""
    s = 100.0 if cm else 1.0
    return [round(v[0] * s, 2), round(-v[1] * s, 2), round(v[2] * s, 2)]


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    out_dir = argv[argv.index("--preview") + 1] if "--preview" in argv else None
    src = load_sockets()
    log("boats from " + src)
    if "--standin" in argv:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.scene.render.fps = FPS
        import standin_rig
        arm_obj = standin_rig.build()
    else:
        arm_obj = poser.load()
    p = Poser(arm_obj)
    acts, grips, report = [], {}, {}
    try:
        grips = json.load(open(GRIPS)).get("clips", {}) if only else {}
    except Exception:  # noqa: BLE001
        grips = {}
    for name, spec in CLIPS.items():
        if only and name not in only:
            continue
        for full, m in variants(name, spec):
            r = R(p, m)
            T = spec["T"]
            n = int(round(T * FPS)) + 1
            frames, bad = [], {}
            g0 = None
            for i in range(n):
                t = i / FPS
                g = spec["fn"](r, t, T) or {}
                if i == 0:
                    g0 = g
                frames.append(p.snapshot())
                for what, cm in check(r, spec, t):
                    if cm > bad.get(what, (0, 0))[0]:
                        bad[what] = (cm, round(t, 2))
            acts.append(poser.bake(arm_obj, p, full, frames, prefix="S_"))
            arm_obj.animation_data.action = None          # or the baked action plays over the next clip's posing
            report[full] = {"seconds": T, "loop": spec["loop"], "frames": n, "issues": bad}
            # the hand targets at the clip's first frame, in UE mesh space, mirrored as posed
            hands = {}
            for role, h in (g0 or {}).items():
                if isinstance(h, dict) and "grip" in h and "palm" in h:
                    mv = lambda a: [m * a[0], a[1], a[2]]  # noqa: E731
                    hands[role] = {"hand": h.get("hand_side", "r") if m > 0 else OTHER[h.get("hand_side", "r")],
                                   "grip_cm": to_ue(mv(h["grip"])), "axis": to_ue(mv(h["axis"]), False),
                                   "palm": to_ue(mv(h["palm"]), False), "fingers": to_ue(mv(h["fingers"]), False),
                                   "radius_cm": round(h["radius"] * 100, 2)}
            grips[full] = {"seconds": T, "loop": spec["loop"], "frame": spec["frame"], "boat": spec["boat"],
                           "about": spec["about"], "hands": hands}
            log("%s: %.2f s %s, %s" % (full, T, "loop" if spec["loop"] else "one-shot",
                                       ("issues " + str(bad)) if bad else "clean"))
            if out_dir:
                import sail_preview
                sail_preview.sheet(r, spec, full, out_dir, BOATS)
    json.dump({"_doc": "blender/humans/sail_clips.py: each A_Human_<clip>'s hand targets at its first frame, UE mesh space "
                       "(cm; +Y the way the mesh faces, +X its left, +Z up; origin = the clip's origin, docs/yacht_school.md). "
                       "boats: the numbers it was posed with.",
               "boats": {k: {kk: (list(vv) if isinstance(vv, tuple) else vv) for kk, vv in b.items()} for k, b in BOATS.items()},
               "boats_from": src, "clips": grips}, open(GRIPS, "w"), indent=1)
    os.makedirs(os.path.join(ROOT, "Saved", "ClipSources"), exist_ok=True)
    json.dump(report, open(os.path.join(ROOT, "Saved", "ClipSources", "sail_clips_report.json"), "w"), indent=1)
    if "--no-export" not in argv and acts:
        p.reset()
        poser.export(arm_obj, acts, os.path.join(ROOT, "Saved", "ClipSources", "sail_clips.glb"))


if __name__ == "__main__":
    main()
