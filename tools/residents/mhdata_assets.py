#!/usr/bin/env python3
"""MakeHuman assets for the resident generator, from the npm package makehuman-data@0.0.2.

The MakeHuman asset packs (files.makehumancommunity.org) are what blender/humans/fetch_assets.sh installs on the Mac.
Where that host can't be reached (the cloud machines), the same system assets come from the npm registry: the package
`makehuman-data` holds MakeHuman 1.1's proxies (clothes, hair, eyebrows, eyelashes, eyes, teeth, tongue) as three.js
JSON *with their fitting data* (3 base-mesh vertices, weights and an offset per vertex, the delete-verts mask) and the
skins, all with 512 px textures. This script turns them back into MPFB asset folders (.mhclo + .obj + .mhmat +
textures), so MPFB2 fits them to any body exactly as it fits the original packs.

    python3 tools/residents/mhdata_assets.py [--out DIR] [--tarball PATH]

DIR defaults to MPFB's user data folder (the one `bpy.utils.extension_path_user` gives for bl_ext.user_default.mpfb,
see blender/humans/fetch_assets.sh), so the generator finds the assets by name. Folders that already exist (the
original, higher-resolution packs on the Mac) are never overwritten.

Licences: only assets that are CC0 or CC-BY are converted (ALLOWED below, with the reason). MakeHuman team assets
(Manuel Bastioni, Jonas Hauquier, Thomas Larsson: system hair, eyebrows, eyelashes, eyes, teeth, tongue, shoes, the
casual/elegant/work suits, skins) carry an old AGPL tag in this 2016 package but were re-released as CC0 with
MakeHuman 1.1 (makehuman_system_assets_cc0, the pack blender/humans/SOURCES.md already uses). Community assets keep
their own licence (CC BY 3.0/4.0 need the attribution in blender/residents/SOURCES.md); AGPL community files are
skipped.
"""
import argparse
import base64
import hashlib
import io
import json
import os
import shutil
import sys
import tarfile
import urllib.request

NPM_URL = "https://registry.npmjs.org/makehuman-data/-/makehuman-data-0.0.2.tgz"
NPM_SHA512 = "wIXbgAjbcmkgcPaMiqynSKYMcz8QR5a7lBSJKY/20BOhIxh5X2hYabJTdhB+DxfjM7O39drB+0fHKVjTygpWuA=="

MH_TEAM = "MakeHuman team (CC0 since MakeHuman 1.1, makehuman_system_assets_cc0)"

# proxy folder in the package -> (MPFB subdir, our asset name, licence note)
ALLOWED = {
    # body parts every resident gets
    "eyes/HighPolyEyes": ("eyes", "high-poly", MH_TEAM),
    "eyes/Low-Poly": ("eyes", "low-poly", MH_TEAM),
    "teeth/Teeth_Base": ("teeth", "teeth_base", MH_TEAM),
    "tongue/tongue01": ("tongue", "tongue01", MH_TEAM),
    **{"eyebrows/eyebrow%03d" % i: ("eyebrows", "eyebrow%03d" % i, MH_TEAM) for i in range(1, 13)},
    "eyelashes/Eyelashes01": ("eyelashes", "eyelashes01", MH_TEAM),
    "eyelashes/eyelashes02": ("eyelashes", "eyelashes02", MH_TEAM),
    "eyelashes/eyelashes03": ("eyelashes", "eyelashes03", MH_TEAM),
    "eyelashes/eyelashes04": ("eyelashes", "eyelashes04", MH_TEAM),
    # system hair
    "hair/short01": ("hair", "short01", MH_TEAM),
    "hair/short02": ("hair", "short02", MH_TEAM),
    "hair/short03": ("hair", "short03", MH_TEAM),
    "hair/short04": ("hair", "short04", MH_TEAM),
    "hair/long01": ("hair", "long01", MH_TEAM),
    "hair/ponytail01": ("hair", "ponytail01", MH_TEAM),
    "hair/bob01": ("hair", "bob01", MH_TEAM),
    "hair/bob02": ("hair", "bob02", MH_TEAM),
    "hair/afro01": ("hair", "afro01", MH_TEAM),
    "hair/Braid01": ("hair", "braid01", MH_TEAM),
    # system clothes
    "clothes/male_casualsuit01": ("clothes", "male_casualsuit01", MH_TEAM),
    "clothes/male_casualsuit02": ("clothes", "male_casualsuit02", MH_TEAM),
    "clothes/male_casualsuit03": ("clothes", "male_casualsuit03", MH_TEAM),
    "clothes/male_casualsuit04": ("clothes", "male_casualsuit04", MH_TEAM),
    "clothes/male_casualsuit05": ("clothes", "male_casualsuit05", MH_TEAM),
    "clothes/male_casualsuit06": ("clothes", "male_casualsuit06", MH_TEAM),
    "clothes/male_elegantsuit01": ("clothes", "male_elegantsuit01", MH_TEAM),
    "clothes/male_worksuit01": ("clothes", "male_worksuit01", MH_TEAM),
    "clothes/female_casualsuit01": ("clothes", "female_casualsuit01", MH_TEAM),
    "clothes/female_casualsuit02": ("clothes", "female_casualsuit02", MH_TEAM),
    "clothes/female_elegantsuit01": ("clothes", "female_elegantsuit01", MH_TEAM),
    "clothes/female_sportsuit01": ("clothes", "female_sportsuit01", MH_TEAM),
    **{"clothes/shoes%02d" % i: ("clothes", "shoes%02d" % i, MH_TEAM) for i in range(1, 7)},
    # community, CC0
    "clothes/female_top_01": ("clothes", "wolgade_female_top_01", "CC0, wolgade"),
    # community, CC BY 4.0 (Mindfront, Sweden)
    "clothes/F_Dress_01": ("clothes", "mindfront_f_dress_01", "CC BY 4.0, Mindfront (Sweden)"),
    "clothes/F_Dress_02": ("clothes", "mindfront_f_dress_02", "CC BY 4.0, Mindfront (Sweden)"),
    "clothes/F_Dress_04": ("clothes", "mindfront_f_dress_04", "CC BY 4.0, Mindfront (Sweden)"),
    "clothes/F_Beach_Dress_01": ("clothes", "mindfront_f_beach_dress_01", "CC BY 4.0, Mindfront (Sweden)"),
    "clothes/Tank_Top_01": ("clothes", "mindfront_tank_top_01", "CC BY 4.0, Mindfront (Sweden)"),
    "clothes/Fishing_Hat_01": ("clothes", "mindfront_fishing_hat_01", "CC BY 4.0, Mindfront (Sweden)"),
    # community, CC BY 3.0 (punkduck)
    "clothes/Coat": ("clothes", "punkduck_coat", "CC BY 3.0, punkduck"),
    "clothes/JeansSkirt": ("clothes", "punkduck_jeans_skirt", "CC BY 3.0, punkduck"),
    "clothes/ShortJeans": ("clothes", "punkduck_short_jeans", "CC BY 3.0, punkduck"),
    "clothes/Sleeveless": ("clothes", "punkduck_sleeveless", "CC BY 3.0, punkduck"),
    "clothes/Tightjeans": ("clothes", "punkduck_tight_jeans", "CC BY 3.0, punkduck"),
    "clothes/VNeckTop": ("clothes", "punkduck_vneck_top", "CC BY 3.0, punkduck"),
    "clothes/WinterBoots": ("clothes", "punkduck_winter_boots", "CC BY 3.0, punkduck"),
    "clothes/miniskirt": ("clothes", "punkduck_miniskirt", "CC BY 3.0, punkduck"),
    "clothes/glasses": ("clothes", "punkduck_glasses", "CC BY 3.0, punkduck"),
    # community, CC BY (DredNicolson)
    "clothes/short_tail_camo_tee": ("clothes", "drednicolson_short_tail_tee", "CC BY, DredNicolson"),
}

SKINS = ["young", "middleage", "old"]


def fetch(tarball):
    if not os.path.isfile(tarball):
        os.makedirs(os.path.dirname(tarball), exist_ok=True)
        print("download", NPM_URL)
        with urllib.request.urlopen(NPM_URL) as r, open(tarball + ".part", "wb") as f:
            shutil.copyfileobj(r, f)
        os.replace(tarball + ".part", tarball)
    digest = base64.b64encode(hashlib.sha512(open(tarball, "rb").read()).digest()).decode()
    if digest != NPM_SHA512:
        raise SystemExit("makehuman-data tarball sha512 mismatch: %s" % digest)
    return tarfile.open(tarball, "r:gz")


def member(tar, path):
    f = tar.extractfile("package/public/data/" + path)
    if f is None:
        raise KeyError(path)
    return f.read()


def faces_from_threejs(d):
    """three.js JSON 3.1 'faces' -> list of (vertex indices, uv indices)."""
    fa = d["faces"]
    n_uv_layers = len([u for u in d.get("uvs", []) if u])
    out = []
    i = 0
    while i < len(fa):
        t = fa[i]
        i += 1
        quad = t & 1
        nv = 4 if quad else 3
        verts = fa[i:i + nv]
        i += nv
        if t & 2:
            i += 1                                   # material index
        if t & 4:
            i += n_uv_layers                         # face uv
        uvs = None
        if t & 8:
            uvs = fa[i:i + nv]                       # first layer
            i += nv * n_uv_layers
        if t & 16:
            i += 1
        if t & 32:
            i += nv
        if t & 64:
            i += 1
        if t & 128:
            i += nv
        out.append((verts, uvs))
    return out


def write_obj(path, d):
    v = d["vertices"]
    uv = d["uvs"][0] if d.get("uvs") and d["uvs"][0] else []
    lines = ["# converted from makehuman-data (three.js JSON) by tools/residents/mhdata_assets.py"]
    for k in range(0, len(v), 3):
        lines.append("v %.5f %.5f %.5f" % (v[k], v[k + 1], v[k + 2]))
    for k in range(0, len(uv), 2):
        lines.append("vt %.5f %.5f" % (uv[k], uv[k + 1]))
    for verts, uvs in faces_from_threejs(d):
        if uvs:
            lines.append("f " + " ".join("%d/%d" % (a + 1, b + 1) for a, b in zip(verts, uvs)))
        else:
            lines.append("f " + " ".join(str(a + 1) for a in verts))
    open(path, "w").write("\n".join(lines) + "\n")


def write_mhclo(path, name, d, licence_note):
    m = d["metadata"]
    lic = m.get("license", {})
    out = ["# MakeHuman proxy, converted from makehuman-data@0.0.2 by tools/residents/mhdata_assets.py",
           "# author %s" % lic.get("author", "unknown"),
           "# licence used: %s" % licence_note,
           "name %s" % name,
           "uuid %s" % m.get("uuid", hashlib.md5(name.encode()).hexdigest())]
    for t in m.get("tags", []):
        out.append("tag %s" % str(t).replace(" ", "_"))
    out += ["obj_file %s.obj" % name, "material %s.mhmat" % name, "z_depth %d" % int(m.get("z_depth", 50)), "verts 0"]
    refs, wts, offs = d["ref_vIdxs"], d["weights"], d["offsets"]
    for r, w, o in zip(refs, wts, offs):
        out.append("%d %d %d %.5f %.5f %.5f %.5f %.5f %.5f" % (r[0], r[1], r[2], w[0], w[1], w[2], o[0], o[1], o[2]))
    out.append("")
    dele = [i for i, x in enumerate(m.get("deleteVerts", [])) if x]
    if dele:
        out.append("delete_verts")
        for k in range(0, len(dele), 16):
            out.append(" ".join(str(i) for i in dele[k:k + 16]))
        out.append("")
    open(path, "w").write("\n".join(out) + "\n")


def write_mhmat(path, name, mat, transparent):
    out = ["# converted from makehuman-data@0.0.2 (three.js material)", "name %s" % name]
    c = mat.get("colorDiffuse", [1, 1, 1])
    out.append("diffuseColor %.4f %.4f %.4f" % tuple(c[:3]))
    for key, ours in (("mapDiffuse", "diffuseTexture"), ("mapNormal", "normalmapTexture"), ("mapAO", "aomapTexture")):
        if mat.get(key):
            out.append("%s %s" % (ours, mat[key]))
    out.append("transparent %s" % ("True" if transparent else "False"))
    out.append("backfaceCull False")
    open(path, "w").write("\n".join(out) + "\n")


def convert_proxy(tar, src, subdir, name, licence_note, out_root, names):
    folder = os.path.join(out_root, subdir, name)
    if os.path.isdir(folder) and not os.path.isfile(os.path.join(folder, ".from_mhdata")):
        print("keep", subdir, name, "(already installed from the original packs)")
        return
    os.makedirs(os.path.join(folder, "textures"), exist_ok=True)
    base = src.split("/")[-1]
    d = json.loads(member(tar, "proxies/%s/%s.json" % (src, base)))
    write_obj(os.path.join(folder, name + ".obj"), d)
    write_mhclo(os.path.join(folder, name + ".mhclo"), name, d, licence_note)
    transparent = subdir in ("hair", "eyebrows", "eyelashes")
    mats = d.get("materials", [])
    write_mhmat(os.path.join(folder, name + ".mhmat"), name, mats[0] if mats else {}, transparent)
    if subdir == "eyes":                                    # every iris colour as its own material
        os.makedirs(os.path.join(out_root, "eyes", "materials"), exist_ok=True)
        for mat in mats:
            mname = mat.get("DbgName", "brown")
            write_mhmat(os.path.join(out_root, "eyes", "materials", mname + ".mhmat"), mname,
                        dict(mat, mapDiffuse="../%s/%s" % (name, mat["mapDiffuse"])), False)
    for n in names:
        p = "proxies/%s/textures/" % src
        if n.startswith("package/public/data/" + p):
            fn = n.split("/")[-1]
            open(os.path.join(folder, "textures", fn), "wb").write(member(tar, p + fn))
    open(os.path.join(folder, ".from_mhdata"), "w").write(licence_note + "\n")
    print("ok", subdir, name, len(d["ref_vIdxs"]), "verts")


def convert_skins(tar, out_root, names):
    root = os.path.join(out_root, "skins")
    for n in names:
        parts = n.split("/")
        if len(parts) == 6 and parts[3] == "skins" and parts[5].endswith(".json") and parts[4].split("_")[0] in SKINS:
            folder_name, fn = parts[4], parts[5]
            mat = json.loads(member(tar, "skins/%s/%s" % (folder_name, fn)))
            sname = fn[:-5]
            if "special_suit" in sname:
                continue
            folder = os.path.join(root, sname)
            if os.path.isdir(folder) and not os.path.isfile(os.path.join(folder, ".from_mhdata")):
                continue
            os.makedirs(os.path.join(folder, "textures"), exist_ok=True)
            tex = mat["mapDiffuse"].split("/")[-1]
            open(os.path.join(folder, "textures", tex), "wb").write(member(tar, "skins/%s/textures/%s" % (folder_name, tex)))
            out = ["# converted from makehuman-data@0.0.2; MakeHuman team skin, CC0 since MakeHuman 1.1",
                   "name %s" % sname, "diffuseColor 1.0 1.0 1.0", "diffuseTexture textures/%s" % tex,
                   "transparent False", "backfaceCull True"]
            open(os.path.join(folder, sname + ".mhmat"), "w").write("\n".join(out) + "\n")
            open(os.path.join(folder, ".from_mhdata"), "w").write(MH_TEAM + "\n")
            print("ok skin", sname)


def default_out():
    try:
        import bpy  # noqa
        return os.path.join(bpy.utils.extension_path_user("bl_ext.user_default.mpfb", create=True), "data")
    except Exception:
        home = os.path.expanduser("~")
        for v in ("5.2", "5.3", "5.1"):
            for base in (os.path.join(home, ".config/blender", v), os.path.join(home, "Library/Application Support/Blender", v)):
                p = os.path.join(base, "extensions/.user/user_default/mpfb/data")
                if os.path.isdir(os.path.dirname(p)):
                    return p
    raise SystemExit("MPFB user data folder not found: pass --out")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--tarball", default=os.path.join(here, "..", "..", "blender", "residents", "_vendor", "makehuman-data-0.0.2.tgz"))
    a = ap.parse_args()
    out = a.out or default_out()
    os.makedirs(out, exist_ok=True)
    tar = fetch(os.path.abspath(a.tarball))
    names = tar.getnames()
    for src, (subdir, name, note) in ALLOWED.items():
        convert_proxy(tar, src, subdir, name, note, out, names)
    convert_skins(tar, out, names)
    print("assets in", out)


if __name__ == "__main__":
    sys.exit(main())
