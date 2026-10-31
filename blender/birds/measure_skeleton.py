"""The skeleton of species, measured like a museum skin (no mesh, a second or two each):

    Blender -b --python blender/birds/measure_skeleton.py -- Id [Id ...]

Prints one "SKEL {json}" line per species: total length L (cm), tail (central feather) / L, tarsus / L, shin / L,
neck (body to head bone) / L, bill / L, arm (shoulder to wrist) / half span, mean secondary / half span (the inner
wing's chord: aspect ratio ~ span / chord), folded wingtip past the tail tip / L (primary projection, + = beyond),
perched eye height / L, perched body pitch. species_art.SKELETON holds the field-guide targets these are fitted to.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = open(os.path.join(HERE, "build_bird.py")).read().replace("\nmain()\n", "\n")
ids = sys.argv[sys.argv.index("--") + 1:]
for sp in ids:
    g = {"__file__": os.path.join(HERE, "build_bird.py"), "__name__": "measure"}
    sys.argv = ["blender", "--", sp, "--no-export"]
    exec(compile(SRC, "build_bird.py", "exec"), g)
    g["solve_size"]()
    S, B, W, M, L = g["S"], g["BONES"], g["WING"], g["M"], g["SP"]["lengthCm"] / 100.0

    def ln(b):
        return float(np.linalg.norm(B[b][1] - B[b][0])) * S

    half = g["SP"]["wingspanCm"] / 200.0
    R, H, _, _ = g["solve_pose"](g["PERCH"])
    P_N, TF_N = g["P_N"], g["TF_N"]
    tip = min(H[f"p{i + 1}.L"][2] + (R[f"p{i + 1}.L"] @ (B[f"p{i + 1}.L"][1] - B[f"p{i + 1}.L"][0]))[2] for i in range(P_N))
    tail = min(H[f"tf.L.{k + 1}"][2] + (R[f"tf.L.{k + 1}"] @ (B[f"tf.L.{k + 1}"][1] - B[f"tf.L.{k + 1}"][0]))[2] for k in range(TF_N))
    eye = H["eye_l"]
    necks = g["NECKS"]
    out = dict(
        id=sp, L=round(L * 100, 1), S=round(S, 3),
        tail=ln("tf.L.1") / L, tail_outer=ln(f"tf.L.{TF_N}") / L,
        tarsus=ln("tarsus.L") / L, shin=ln("shin.L") / L, toes=ln("toes.L") / L,
        neck=float(np.linalg.norm(B["head"][0] - B["body"][1])) * S / L,
        bill=float(np.linalg.norm(B["jaw"][1] - B["jaw"][0])) * S / L,
        arm=float(np.linalg.norm(W["WRIST"] - W["SHOULDER"])) * S / half,
        chord=float(np.mean(W["S_LEN"][:g["S_N"]])) * S / half,
        prim=float(max(W["P_LEN"])) * S / half,
        wing=float(g["wing_length"]()) * S / L,
        proj=(tail - tip) * S / L,
        eye_h=float(eye[1]) * S / L, pitch=M["perch_pitch"],
    )
    print("SKEL " + json.dumps({k: (round(v, 3) if isinstance(v, float) else v) for k, v in out.items()}))
