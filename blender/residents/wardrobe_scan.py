"""Scans every wardrobe asset once: its faces, UV islands, the tint slot of each face and the slot masks in texture space.

    Blender -b -P blender/residents/wardrobe_scan.py -- [names...]      (or python3 with pip bpy, see docs/residents.md)

For each MPFB asset the residents use (wardrobe.py: garments, hair, eyebrows, eyelashes, eyes, teeth, tongue) it fits the
asset on a default man or woman with the game rig, and writes blender/residents/cache/wardrobe/<subdir>__<name>.json:
  faces, verts, tris, uv_bounds, texture (the diffuse map), alpha (the map has see-through texels),
  slots (garments: the tint slot of every face, in the mesh's face order) and slot_area;
plus <subdir>__<name>__<slot>.png: where each slot lies on the texture (white = the slot), which
tools/residents/atlases.py uses to turn tinted slots into grey detail. build_residents.py reads the slot list (the face
order of a fitted asset is always the one of its .obj).

Slot rules (wardrobe.py) are decided per UV island (a piece of cloth): "legs" if the island's vertices are skinned more
to the legs than to anything else, "color:<family>" if most of its texels are of that colour family.
"""
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "blender", "humans"))
import wardrobe as W  # noqa: E402

OUT = os.path.join(HERE, "cache", "wardrobe")
LEG_BONES = ("thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r", "ball_l", "ball_r")


def assets():
    """(subdir, name, sex of the body to fit it on)."""
    out = []
    for name, g in W.GARMENTS.items():
        out.append(("clothes", name, "f" if g["sex"] == "f" else "m"))
    for name, h in W.HAIR.items():
        out.append(("hair", name, "f" if h["sex"] == "f" else "m"))
    for name in W.EYEBROWS:
        out.append(("eyebrows", name, "m"))
    for name in sorted(set(W.EYELASHES_M + W.EYELASHES_F)):
        out.append(("eyelashes", name, "m"))
    out += [("eyes", "low-poly", "m"), ("eyes", "high-poly", "m"), ("teeth", "teeth_base", "m"), ("tongue", "tongue01", "m")]
    return out


def asset_dir(subdir, name):
    import mpfb_pipeline as P
    return os.path.join(P.services()["LocationService"].get_user_data(subdir), name)


def mhmat_textures(subdir, name):
    d = asset_dir(subdir, name)
    out = {}
    for f in os.listdir(d):
        if f.endswith(".mhmat"):
            for line in open(os.path.join(d, f), encoding="utf-8", errors="replace"):
                parts = line.split(None, 1)
                if len(parts) == 2 and parts[0].endswith("Texture"):
                    out[parts[0]] = os.path.normpath(os.path.join(d, parts[1].strip()))
    return out


def load_rgba(path):
    from PIL import Image
    im = Image.open(path).convert("RGBA")
    return np.asarray(im, dtype=np.float32) / 255.0


def family(rgb):
    r, g, b = (float(x) for x in rgb[:3])
    mx, mn = max(r, g, b), min(r, g, b)
    v = mx
    s = (mx - mn) / mx if mx > 1e-6 else 0.0
    if v < 0.16:
        return "dark"
    if s < 0.2:
        return "white" if v > 0.68 else "grey"
    if mx == r:
        h = (60 * ((g - b) / (mx - mn))) % 360
    elif mx == g:
        h = 60 * ((b - r) / (mx - mn)) + 120
    else:
        h = 60 * ((r - g) / (mx - mn)) + 240
    if h < 18 or h >= 320:
        return "red"
    if h < 45:
        return "skin"
    if h < 70:
        return "yellow"
    if h < 165:
        return "green"
    if h < 265:
        return "blue"
    return "purple"


def sample(img, uv):
    h, w = img.shape[:2]
    x = int(min(w - 1, max(0, (uv[0] % 1.0) * w)))
    y = int(min(h - 1, max(0, (1.0 - (uv[1] % 1.0)) * h)))
    return img[y, x]


def islands(bm, uv_layer):
    """Face index lists of UV islands (faces joined through edges whose two loops share UVs)."""
    seen = set()
    out = []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack = [f]
        seen.add(f.index)
        isl = []
        while stack:
            cur = stack.pop()
            isl.append(cur.index)
            for loop in cur.loops:
                e = loop.edge
                if len(e.link_faces) != 2:
                    continue
                other_loop = loop.link_loop_radial_next
                nb = other_loop.face
                if nb.index in seen:
                    continue
                # the edge's ends must have the same UVs on both faces
                a0, a1 = loop[uv_layer].uv, loop.link_loop_next[uv_layer].uv
                b0, b1 = other_loop.link_loop_next[uv_layer].uv, other_loop[uv_layer].uv
                if (a0 - b0).length < 1e-5 and (a1 - b1).length < 1e-5:
                    seen.add(nb.index)
                    stack.append(nb)
        out.append(isl)
    return out


def leg_share(obj):
    """Per vertex: how much of its skinning is on the legs (0..1)."""
    n = len(obj.data.vertices)
    legs = np.zeros(n)
    tot = np.zeros(n)
    gi = {vg.index: vg.name for vg in obj.vertex_groups}
    for v in obj.data.vertices:
        for g in v.groups:
            name = gi.get(g.group, "")
            if name in ("body", "HelperGeometry") or name.startswith("joint-"):
                continue
            tot[v.index] += g.weight
            if name in LEG_BONES:
                legs[v.index] += g.weight
    return np.where(tot > 0, legs / np.maximum(tot, 1e-9), 0.0)


def classify(obj, rules, img):
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    uvl = bm.loops.layers.uv.active
    share = leg_share(obj)
    slots = [None] * len(bm.faces)
    for isl in islands(bm, uvl):
        verts = set()
        fams = {}
        area = 0.0
        for fi in isl:
            f = bm.faces[fi]
            for v in f.verts:
                verts.add(v.index)
            c = sum((l[uvl].uv for l in f.loops), start=type(f.loops[0][uvl].uv)((0.0, 0.0))) / len(f.loops)
            fam = family(sample(img, c)) if img is not None else "grey"
            a = f.calc_area()
            fams[fam] = fams.get(fam, 0.0) + a
            area += a
        legs = float(np.mean([share[v] for v in verts])) if verts else 0.0
        top_fam = max(fams.items(), key=lambda kv: kv[1])[0] if fams else "grey"
        slot = None
        for rule, name in rules:
            if rule == "*" or (rule == "legs" and legs > 0.5) or (rule.startswith("color:") and rule[6:] == top_fam):
                slot = name
                break
        for fi in isl:
            slots[fi] = slot or rules[-1][1]
    bm.free()
    return slots


def uv_polys(me):
    uv = me.uv_layers.active.data
    for p in me.polygons:
        yield p.index, [tuple(uv[li].uv) for li in p.loop_indices]


def write_masks(me, slots, size, prefix):
    from PIL import Image, ImageDraw
    names = sorted(set(slots))
    imgs = {s: Image.new("L", size, 0) for s in names}
    draws = {s: ImageDraw.Draw(imgs[s]) for s in names}
    w, h = size
    for fi, poly in uv_polys(me):
        pts = [((u % 1.0 if u > 1.0 or u < 0.0 else u) * w, (1.0 - v) * h) for u, v in poly]
        draws[slots[fi]].polygon(pts, fill=255, outline=255)
    out = {}
    for s, im in imgs.items():
        p = "%s__%s.png" % (prefix, s)
        im.save(p)
        out[s] = os.path.relpath(p, OUT)
    return out


def body(sex):
    import mpfb_pipeline as P
    s = P.services()
    macro = dict(s["TargetService"].get_default_macro_info_dict())
    macro.update(dict(gender=1.0 if sex == "m" else 0.0, age=0.5, muscle=0.5, weight=0.5, height=0.5, proportions=0.5))
    bm = s["HumanService"].create_human(macro_detail_dict=macro)
    bm.name = "scan_" + sex
    arm = P.add_ue_rig(bm)
    return bm, arm


def scan_skins(only=None):
    for skins in W.SKINS.values():
        for name in skins:
            if only and name not in only:
                continue
            tex = mhmat_textures("skins", name)
            diffuse = tex.get("diffuseTexture")
            info = dict(subdir="skins", name=name, texture=os.path.relpath(diffuse, asset_dir("skins", name)),
                        texture_abs=diffuse, textures={k: os.path.relpath(v, asset_dir("skins", name)) for k, v in tex.items()})
            with open(os.path.join(OUT, "skins__%s.json" % name), "w", encoding="utf-8") as f:
                json.dump(info, f)


def scan(only=None):
    import mpfb_pipeline as P
    os.makedirs(OUT, exist_ok=True)
    scan_skins(only)
    bodies = {}
    done = 0
    for subdir, name, sex in assets():
        if only and name not in only:
            continue
        if sex not in bodies:
            bodies[sex] = body(sex)
        bm, arm = bodies[sex]
        mods_before = {m.name for m in bm.modifiers}
        atype = {"clothes": "Clothes", "hair": "Hair", "eyebrows": "Eyebrows", "eyelashes": "Eyelashes", "eyes": "Eyes",
                 "teeth": "Teeth", "tongue": "Tongue"}[subdir]
        obj = P.add_asset(bm, name, subdir, atype)
        me = obj.data
        tex = mhmat_textures(subdir, name)
        diffuse = tex.get("diffuseTexture")
        img = load_rgba(diffuse) if diffuse and os.path.isfile(diffuse) else None
        uvs = np.array([tuple(d.uv) for d in me.uv_layers.active.data]) if me.uv_layers else np.zeros((1, 2))
        info = dict(subdir=subdir, name=name, faces=len(me.polygons), verts=len(me.vertices),
                    tris=sum(len(p.vertices) - 2 for p in me.polygons),
                    uv_bounds=[round(float(x), 4) for x in (uvs[:, 0].min(), uvs[:, 1].min(), uvs[:, 0].max(), uvs[:, 1].max())],
                    texture=os.path.relpath(diffuse, asset_dir(subdir, name)) if diffuse else None,
                    texture_abs=diffuse,
                    textures={k: os.path.relpath(v, asset_dir(subdir, name)) for k, v in tex.items()},
                    alpha=bool(img is not None and float(img[..., 3].min()) < 0.99),
                    size=list(img.shape[1::-1]) if img is not None else None)
        prefix = os.path.join(OUT, "%s__%s" % (subdir, name))
        if subdir == "clothes":
            rules = W.GARMENTS[name]["slots"]
            slots = classify(obj, rules, img)
            info["slots"] = slots
            areas = {}
            for p in me.polygons:
                areas[slots[p.index]] = areas.get(slots[p.index], 0.0) + p.area
            tot = sum(areas.values()) or 1.0
            info["slot_area"] = {k: round(v / tot, 3) for k, v in sorted(areas.items())}
            if img is not None:
                info["masks"] = write_masks(me, slots, (img.shape[1], img.shape[0]), prefix)
        with open(prefix + ".json", "w", encoding="utf-8") as f:
            json.dump(info, f)
        print("scan %-10s %-30s faces %5d tris %5d uv %s %s" % (subdir, name, info["faces"], info["tris"], info["uv_bounds"],
                                                               info.get("slot_area", "")))
        # clean up: the asset and the delete masks it put on the body
        mesh = obj.data
        bpy.data.objects.remove(obj)
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)
        for m in list(bm.modifiers):
            if m.name not in mods_before:
                bm.modifiers.remove(m)
        done += 1
    print("scanned", done, "assets ->", OUT)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    bpy.ops.wm.read_factory_settings(use_empty=True)
    try:
        bpy.ops.preferences.addon_enable(module="bl_ext.user_default.mpfb")
    except Exception:
        pass
    scan(set(argv) or None)


if __name__ == "__main__":
    main()
