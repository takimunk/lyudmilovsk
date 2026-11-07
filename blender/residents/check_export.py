"""Offline check of the built residents (blender/residents/export) against what the Unreal import and the game expect,
no Blender or engine needed:

  python3 blender/residents/check_export.py [--all] [--only R_x,R_y]

For each model listed in Content/PeopleData/town_looks.json whose sidecar matches its look (--all: every .glb there):
one skin on the 56 bones of SK_Human_Skeleton, one mesh in four sections Skin/Cloth/Hair/Eyes (their order is free),
the six face morphs on every section and none of them on by default (the mesh's weights 0), UV0-2 + normals +
joints/weights on every section. With numpy also: finite positions, joint indices inside the skin, skin weights
summing to 1, UV1-2 (the tint) inside 0..1, UV0 inside its section's atlas tiles (atlases/layout.json; a UV in
another tile or outside the tiles' padding shows the wrong texture), indices in range, no triangle of zero area.
Exit status 1 when anything is wrong.
"""
import argparse
import json
import os
import struct
import sys

try:
    import numpy as np
except ImportError:                     # the structural checks still run
    np = None

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
EXPORT = os.path.join(HERE, "export")
ATLASES = os.path.join(HERE, "atlases")
LOOKS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "PeopleData", "town_looks.json")

# SK_Human_Skeleton (blender/humans/mpfb_pipeline.add_ue_rig: MPFB's game_engine rig + jaw and eyes)
BONES = {
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head", "jaw", "eye_l", "eye_r",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r",
} | {"%s_0%d_%s" % (f, i, s) for f in ("thumb", "index", "middle", "ring", "pinky") for i in (1, 2, 3) for s in "lr"}
MORPHS = {"eyeBlinkLeft", "eyeBlinkRight", "jawOpen", "mouthFunnel", "mouthSmile", "browUp"}   # ULyudBodyAnimInstance
SECTIONS = {"Skin", "Cloth", "Hair", "Eyes"}
ATTRS = ("POSITION", "NORMAL", "TEXCOORD_0", "TEXCOORD_1", "TEXCOORD_2", "JOINTS_0", "WEIGHTS_0")
NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
CTYPE = {5120: "i1", 5121: "u1", 5122: "<i2", 5123: "<u2", 5125: "<u4", 5126: "<f4"}


def load(path):
    with open(path, "rb") as f:
        b = f.read()
    magic, version, length = struct.unpack_from("<III", b, 0)
    if magic != 0x46546C67 or version != 2:
        raise ValueError("not a glTF 2 binary")
    off, js, binc = 12, None, b""
    while off < length:
        size, kind = struct.unpack_from("<II", b, off)
        chunk = b[off + 8: off + 8 + size]
        if kind == 0x4E4F534A:
            js = json.loads(chunk)
        elif kind == 0x004E4942:
            binc = chunk
        off += 8 + size
    return js, binc


def accessor(js, binc, i):
    a = js["accessors"][i]
    bv = js["bufferViews"][a["bufferView"]]
    dt = np.dtype(CTYPE[a["componentType"]])
    n, k = a["count"], NCOMP[a["type"]]
    stride = bv.get("byteStride") or dt.itemsize * k
    start = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    raw = np.frombuffer(binc, np.uint8, stride * (n - 1) + dt.itemsize * k, start)
    rows = np.lib.stride_tricks.as_strided(raw, (n, stride), (stride, 1))[:, :dt.itemsize * k]
    out = np.ascontiguousarray(rows).view(dt).reshape(n, k)
    if a.get("normalized"):
        out = out.astype(np.float64) / np.iinfo(dt).max
    return out


def tiles_of(layout):
    """{section: (tile rects (N, 4) u0 v0 u1 v1 from the bottom, slack u, slack v: the padding)} from layout.json."""
    out = {}
    for s, sec in layout["sections"].items():
        w, h = sec["size"]
        out[s] = (np.array(list(sec["tiles"].values()), dtype=np.float64), layout["pad"] / w, layout["pad"] / h)
    return out


def check(path, tiles=None):
    """The problems found in one .glb (empty: fine); tiles: tiles_of(layout.json), for the UV0 check."""
    js, binc = load(path)
    out = []
    skins = js.get("skins", [])
    joints = len(skins[0]["joints"]) if skins else 0
    if len(skins) != 1:
        out.append("%d skins" % len(skins))
    else:
        names = {js["nodes"][j].get("name") for j in skins[0]["joints"]}
        if names != BONES:
            out.append("bones: extra %s, missing %s" % (sorted(names - BONES)[:5], sorted(BONES - names)[:5]))
    meshes = js.get("meshes", [])
    if len(meshes) != 1:
        return out + ["%d meshes" % len(meshes)]
    mesh = meshes[0]
    morphs = set(mesh.get("extras", {}).get("targetNames", []))
    if morphs != MORPHS:
        out.append("morphs %s" % sorted(morphs))
    if any(abs(w) > 1e-6 for w in mesh.get("weights", [])):
        out.append("morphs on by default (mesh weights %s)" % mesh["weights"])
    sections = [js["materials"][p["material"]]["name"] if "material" in p else None for p in mesh["primitives"]]
    if len(sections) != len(SECTIONS) or set(sections) != SECTIONS:
        out.append("sections %s" % sections)
    for prim, sec in zip(mesh["primitives"], sections):
        at = prim["attributes"]
        out += ["%s: no %s" % (sec, a) for a in ATTRS if a not in at]
        if len(prim.get("targets", [])) != len(MORPHS):
            out.append("%s: %d morph targets" % (sec, len(prim.get("targets", []))))
        if np is None or any(a not in at for a in ATTRS) or "indices" not in prim:
            continue
        pos = accessor(js, binc, at["POSITION"])
        if not np.isfinite(pos).all():
            out.append("%s: positions not finite" % sec)
        weights = accessor(js, binc, at["WEIGHTS_0"]).astype(np.float64)
        w = weights.sum(1)
        if len(w) and abs(w - 1).max() > 0.02:
            out.append("%s: skin weights sum to %.3f..%.3f" % (sec, w.min(), w.max()))
        if ((accessor(js, binc, at["JOINTS_0"]) >= joints) & (weights > 0)).any():
            out.append("%s: weights on joint indices past the skin's %d" % (sec, joints))
        for t in ("TEXCOORD_1", "TEXCOORD_2"):
            uv = accessor(js, binc, at[t])
            if len(uv) and (uv.min() < -0.002 or uv.max() > 1.002):
                out.append("%s: %s outside 0..1 (%.3f..%.3f)" % (sec, t, uv.min(), uv.max()))
        uv = accessor(js, binc, at["TEXCOORD_0"]).astype(np.float64)
        if tiles and sec in tiles and len(uv):
            rects, su, sv = tiles[sec]
            u, v = uv[:, 0:1], 1.0 - uv[:, 1:2]           # glTF's V runs down, the layout's up
            inside = ((u >= rects[:, 0] - su) & (u <= rects[:, 2] + su) &
                      (v >= rects[:, 1] - sv) & (v <= rects[:, 3] + sv)).any(1)
            if not inside.all():
                out.append("%s: %d UV0 points outside the section's atlas tiles" % (sec, int((~inside).sum())))
        elif len(uv) and (uv.min() < -0.002 or uv.max() > 1.002):
            out.append("%s: TEXCOORD_0 outside 0..1 (%.3f..%.3f)" % (sec, uv.min(), uv.max()))
        tri = accessor(js, binc, prim["indices"]).reshape(-1, 3)
        if len(tri) and tri.max() >= len(pos):
            out.append("%s: indices out of range" % sec)
            continue
        p = pos.astype(np.float64)
        area = 0.5 * np.linalg.norm(np.cross(p[tri[:, 1]] - p[tri[:, 0]], p[tri[:, 2]] - p[tri[:, 0]]), axis=1)
        flat = int((area < 1e-10).sum())
        if flat:
            out.append("%s: %d triangles of zero area" % (sec, flat))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--all", action="store_true", help="every .glb in export/, current or not")
    ap.add_argument("--only", default="", help="models (R_x,R_y)")
    ap.add_argument("--export", default=EXPORT)
    ap.add_argument("--atlases", default=ATLASES, help="where layout.json is (the UV0 tile check)")
    a = ap.parse_args()
    tiles, note = None, ""
    lay = os.path.join(a.atlases, "layout.json")
    if np is not None and os.path.isfile(lay):
        with open(lay, encoding="utf-8") as f:
            tiles = tiles_of(json.load(f))
    elif np is not None:
        note = " (no %s: UV0 checked against 0..1 only)" % lay
    with open(LOOKS, encoding="utf-8") as f:
        looks = json.load(f)["people"]
    want = {v["model"].split("/")[-1]: v["hash"] for v in looks.values()}
    only = {m.strip() for m in a.only.split(",") if m.strip()}
    models, stale, missing = [], 0, 0
    names = sorted(f[:-4] for f in os.listdir(a.export) if f.endswith(".glb")) if a.all else sorted(want)
    for m in names:
        if only and m not in only:
            continue
        glb, side = os.path.join(a.export, m + ".glb"), os.path.join(a.export, m + ".json")
        if not os.path.isfile(glb) or not os.path.isfile(side):
            missing += 1
            continue
        with open(side, encoding="utf-8") as f:
            if not a.all and json.load(f).get("hash") != want[m]:
                stale += 1
                continue
        models.append(glb)
    bad = 0
    for glb in models:
        try:
            problems = check(glb, tiles)
        except Exception as e:          # a broken file is a finding, not a crash
            problems = ["unreadable: %r" % e]
        if problems:
            bad += 1
            print("%s: %s" % (os.path.basename(glb)[:-4], "; ".join(problems)))
    print("check_export: %d models checked, %d with problems%s; %d not built, %d built from an older look%s" % (
        len(models), bad, note if np is not None else " (no numpy: structure only)", missing, stale,
        "" if not (missing or stale) else " (build_residents.py)"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
