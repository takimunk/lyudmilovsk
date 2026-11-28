"""The sailing school's meshes in Unreal (docs/yacht_school.md "Meshes (part C)", blender/sailing/README.md).

  materials       /Game/Sailing/Materials: the masters (make_sailing_materials.py), M_Sail_Flat (the flat master of the
                  Academgorodok, academ_materials.build_flat), MI_SL_<slot> for every MAT_SPEC slot of
                  blender/sailing/sailkit.py; the town's MI_CT_<slot> (/Game/City/Materials) for the building slots (a
                  flat MI_SL_CT_<slot> from blender/city/citymats.py when the town's is not there); the pictures
                  (blender/sailing/out/textures: the sails, the digits, the atlas, the flags) -> /Game/Sailing/Textures;
                  the signs (textures/localized/<lang>/sail_sign_<name>.png, tools/make_sailing_signs.py) ->
                  /Game/Sailing/Localized/<lang>/T_Sail_Sign_<name>, MI_SL_Sign_<name> showing Russian.
  import <kit>    one kit of blender/sailing/out (sailing_boats.glb | sailing_gear.glb | sailing_house.glb; Interchange
                  takes one scene per process, import_sailing.sh runs one process per kit) -> /Game/Sailing/<asset>
                  by blender/sailing/manifest.json: materials by slot name, Nanite OFF (glTF import turns it on), collision
                  by the manifest ("exact" complex as simple, "box" the manifest's boxes, "none"), and every "_LOD1" mesh
                  copied into LOD1 of its "lod_of" mesh (then deleted).
Idempotent: assets are replaced in place.

Headless:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_sailing.py materials"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_sailing.py import sailing_boats.glb"
"""
import ast
import json
import os
import sys
import traceback

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
SAIL = os.path.join(REPO, "blender", "sailing")
OUT = os.path.join(SAIL, "out")
TEXDIR = os.path.join(OUT, "textures")
MANIFEST = os.path.join(SAIL, "manifest.json")
DEST = "/Game/Sailing"
MESHES = DEST                  # the contract: /Game/Sailing/SM_Sail_<Name>
MATS = DEST + "/Materials"
TEXS = DEST + "/Textures"
CITY_MATS = "/Game/City/Materials"
SIGNS = ("name", "door")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning("SAILING " + msg)


def _dict_literal(path, name):
    tree = ast.parse(open(path).read())
    for n in tree.body:
        if isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "") == name:
            return ast.literal_eval(n.value)
    raise RuntimeError("no %s in %s" % (name, path))


def _lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def manifest():
    with open(MANIFEST) as f:
        return json.load(f)["meshes"]


def _tex(path, name, srgb=True, sub=TEXS):
    import academ_materials as A
    if not os.path.exists(path):
        log("missing picture %s (python3 blender/sailing/build_all.py)" % os.path.relpath(path, REPO))
        return None
    t = A.import_image(path, sub, name, sub="")
    t.set_editor_property("srgb", srgb)
    eal.save_loaded_asset(t, False)
    return t


def make_materials():
    import academ_materials as A
    import make_sailing_materials as SM
    masters = SM.build()
    flat = A.build_flat(MATS + "/M_Sail_Flat")
    clear = A.build_glass_clear(MATS + "/M_Sail_GlassClear")
    atlas = _tex(os.path.join(TEXDIR, "sail_atlas.png"), "T_Sail_Atlas")
    picture = A.build_picture(MATS + "/M_Sail_Picture", atlas)
    out = {}
    spec = _dict_literal(os.path.join(SAIL, "sailkit.py"), "MAT_SPEC")
    digits = _tex(os.path.join(TEXDIR, "sail_digits.png"), "T_Sail_Digits", srgb=True)
    for slot, (hx, rough, metal, emissive) in spec.items():
        c = _lin(hx)
        path = "%s/MI_%s" % (MATS, slot)
        if slot in ("SL_Hull", "SL_HullDeck"):
            mi = A.instance(path, masters["hull"])
            A._set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
            A._set(mi, "s", "Roughness", rough)
        elif slot == "SL_Glass":
            mi = A.instance(path, clear)
            A._set(mi, "v", "Color", unreal.LinearColor(0.05, 0.07, 0.07, 1))
            A._set(mi, "s", "Opacity", 0.15)
        elif slot == "SL_Sail":
            continue         # one per class below
        else:
            mi = A.instance(path, flat)
            A._set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
            A._set(mi, "s", "Roughness", rough)
            A._set(mi, "s", "Metallic", metal)
            A._set(mi, "s", "Emissive", emissive)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        out[slot] = mi
    # the sails: one instance per class (its picture and number boxes from the manifest)
    rows = {r["asset"]: r for r in manifest()}
    # cloth variants: "" (new, the mesh's default), "_Old" (yellowed), "_Patched" (repaired); the engine picks one per
    # boat by setting the SL_Sail slot's material to MI_SL_Sail_<Class>_<Part><Variant>
    for cls in ("Luch", "Optimist", "Cadet"):
      for var, vfile in (("", ""), ("_Old", "_old"), ("_Patched", "_patched")):
        for part, pic in (("Main", "sail_%s%s.png" % (cls.lower(), vfile)), ("Jib", "sail_cadet_jib%s.png" % vfile)):
            row = rows.get("SM_Sail_%s_%s" % (cls, part))
            if row is None:
                continue
            tex = _tex(os.path.join(TEXDIR, pic), "T_Sail_%s_%s%s" % (cls, part, var))
            mi = A.instance("%s/MI_SL_Sail_%s_%s%s" % (MATS, cls, part, var), masters["cloth"])
            if tex:
                A._set(mi, "t", "Image", tex)
            if digits:
                A._set(mi, "t", "Digits", digits)
            nb = row.get("number_boxes_uv")
            if nb:
                A._set(mi, "v", "NumStbd", unreal.LinearColor(*nb["num_stbd"]))
                A._set(mi, "v", "NumPort", unreal.LinearColor(*nb["num_port"]))
            mel.update_material_instance(mi)
            eal.save_loaded_asset(mi, False)
            if var:
                continue
            out["SL_Sail@SM_Sail_%s_%s" % (cls, part)] = mi
            out["SL_Sail@SM_Sail_%s_%s_LOD1" % (cls, part)] = mi
            if part == "Main":
                out["SL_Sail@SM_Sail_%s_Far" % cls] = mi
    # pictures: the atlas, the flags (cloth), the signs per language
    mi = A.instance(MATS + "/MI_SLP_Atlas", picture)
    if atlas:
        A._set(mi, "t", "Image", atlas)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)
    out["SLP_Atlas"] = mi
    flags = _tex(os.path.join(TEXDIR, "sail_flags.png"), "T_Sail_Flags")
    mi = A.instance(MATS + "/MI_SLP_Flags", masters["flag"])
    if flags:
        A._set(mi, "t", "Image", flags)
    A._set(mi, "s", "Size", 90.0)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)
    out["SLP_Flags"] = mi
    for slot, hx in (("SL_Orange", spec["SL_Orange"][0]), ("SL_White", spec["SL_White"][0])):
        mi = A.instance("%s/MI_Windsock_%s" % (MATS, slot), masters["windsock"])
        c = _lin(hx)
        A._set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
        A._set(mi, "s", "Size", 150.0)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        out["%s@SM_Sail_Windsock" % slot] = mi
    loc = os.path.join(REPO, "textures", "localized")
    for lang in sorted(os.listdir(loc)) if os.path.isdir(loc) else []:
        for name in SIGNS:
            f = os.path.join(loc, lang, "sail_sign_%s.png" % name)
            if os.path.exists(f):
                tex = _tex(f, "T_Sail_Sign_" + name, sub=DEST + "/Localized/" + lang)
                if lang == "ru" and tex:
                    mi = A.instance("%s/MI_SLP_Sign_%s" % (MATS, name.capitalize()), picture)
                    A._set(mi, "t", "Image", tex)
                    mel.update_material_instance(mi)
                    eal.save_loaded_asset(mi, False)
                    out["SLP_Sign_" + name.capitalize()] = mi
    mi = A.instance(MATS + "/MI_SLP_Notice", flat)
    A._set(mi, "v", "Color", unreal.LinearColor(0.85, 0.82, 0.74, 1))
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)
    out["SLP_Notice"] = mi
    # the town's building slots
    cityspec = _dict_literal(os.path.join(REPO, "blender", "city", "citymats.py"), "MATERIALS")
    for slot, row in cityspec.items():
        town = "%s/MI_%s" % (CITY_MATS, slot)
        if eal.does_asset_exist(town):
            out[slot] = eal.load_asset(town)
            continue
        mi = A.instance("%s/MI_SL_%s" % (MATS, slot), flat)
        c = _lin(row[0])
        A._set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
        A._set(mi, "s", "Roughness", row[1])
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        out[slot] = mi
    eal.save_directory(DEST)
    log("materials: %d" % len(out))
    return out


def materials_cached():
    """The instances by slot (loaded, not rebuilt: `materials` makes them)."""
    out = {}
    for path in eal.list_assets(MATS, False):
        a = eal.load_asset(path)
        if isinstance(a, unreal.MaterialInstanceConstant):
            n = a.get_name()
            if n.startswith("MI_SL_Sail_"):
                cls_part = n[len("MI_SL_Sail_"):]
                if cls_part.endswith(("_Old", "_Patched")):
                    continue          # cloth variants: chosen by the engine, never the mesh's default
                for suf in ("", "_LOD1"):
                    out["SL_Sail@SM_Sail_%s%s" % (cls_part, suf)] = a
                if cls_part.endswith("_Main"):
                    out["SL_Sail@SM_Sail_%s_Far" % cls_part[:-5]] = a
            elif n.startswith("MI_Windsock_"):
                out["%s@SM_Sail_Windsock" % n[len("MI_Windsock_"):]] = a
            elif n.startswith("MI_SL_CT_"):
                out[n[len("MI_SL_"):]] = a
            elif n.startswith("MI_"):
                out[n[3:]] = a
    for path in eal.list_assets(CITY_MATS, False) if eal.does_directory_exist(CITY_MATS) else []:
        a = eal.load_asset(path)
        n = a.get_name()
        if n.startswith("MI_CT_") and n[3:] not in out:
            out[n[3:]] = a
    return out


def _mesh_tools():
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    return sub if sub is not None else unreal.EditorStaticMeshLibrary


def _collision(sm, row):
    kind = row.get("collision", "none")
    body = sm.get_editor_property("body_setup")
    try:
        _mesh_tools().remove_collisions(sm)
    except Exception as e:      # noqa: BLE001
        log("%s: remove_collisions: %s" % (row["asset"], e))
    if body is None:
        return
    if kind == "exact":
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        return
    if kind == "box" and row.get("collision_boxes"):
        elems = []
        for b in row["collision_boxes"]:
            c, s = b["center"], b["size"]
            e = unreal.KBoxElem()
            # Blender (x, y, z) m -> Unreal (x, -y, z) cm; a right-handed yaw about +Z is a negative Unreal yaw
            e.set_editor_property("center", unreal.Vector(c[0] * 100.0, -c[1] * 100.0, c[2] * 100.0))
            e.set_editor_property("rotation", unreal.Rotator(0.0, 0.0, -float(b.get("rot_z_deg", 0.0))))
            e.set_editor_property("x", s[0] * 100.0)
            e.set_editor_property("y", s[1] * 100.0)
            e.set_editor_property("z", s[2] * 100.0)
            elems.append(e)
        geom = unreal.KAggregateGeom()
        geom.set_editor_property("box_elems", elems)
        body.set_editor_property("agg_geom", geom)
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)


def import_kit(fname):
    path = os.path.join(OUT, fname)
    if not os.path.exists(path):
        log("missing %s: run python3 blender/sailing/build_all.py first" % os.path.relpath(path, REPO))
        return 0
    rows = {r["asset"]: r for r in manifest()}
    mats = materials_cached()
    if not mats:
        mats = make_materials()
    tmp = DEST + "/_import_" + os.path.splitext(fname)[0]
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(path)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    done = {}
    for p in eal.list_assets(tmp, True):
        sm = eal.load_asset(p)
        if not isinstance(sm, unreal.StaticMesh):
            continue
        name = sm.get_name()
        if not name.startswith("SM_"):
            name = "SM_" + name
        row = rows.get(name)
        if row is None:
            log("%s is not in the manifest: skipped" % name)
            continue
        for i, sl in enumerate(sm.get_editor_property("static_materials")):
            key = str(sl.material_slot_name).split(".")[0]
            mi = mats.get("%s@%s" % (key, name)) or mats.get(key)
            if mi is not None:
                sm.set_material(i, mi)
            else:
                log("%s: no material for slot %s" % (name, key))
        nanite = sm.get_editor_property("nanite_settings")
        nanite.enabled = False
        sm.set_editor_property("nanite_settings", nanite)
        _collision(sm, row)
        target = "%s/%s" % (MESHES, name)
        if eal.does_asset_exist(target):
            eal.delete_asset(target)
        eal.save_loaded_asset(sm)
        eal.rename_asset(sm.get_path_name(), target)
        done[name] = target
    # LOD1s into their meshes
    tools = _mesh_tools()
    for name, target in list(done.items()):
        lod_of = rows[name].get("lod_of")
        if not lod_of or lod_of not in done:
            continue
        try:
            dst = eal.load_asset(done[lod_of])
            srcm = eal.load_asset(target)
            tools.set_lod_from_static_mesh(dst, 1, srcm, 0, True)
            eal.save_loaded_asset(dst)
            eal.delete_asset(target)
            del done[name]
        except Exception:       # noqa: BLE001
            log("%s: LOD1 into %s failed (kept as its own mesh):\n%s" % (name, lod_of, traceback.format_exc()))
    eal.delete_directory(tmp)
    eal.save_directory(DEST)
    log("imported %s: %d meshes" % (fname, len(done)))
    return len(done)


if __name__ == "__main__":
    a = sys.argv[1:]
    if "materials" in a:
        make_materials()
    if "import" in a:
        i = a.index("import")
        kits = a[i + 1:] or ["sailing_boats.glb", "sailing_gear.glb", "sailing_house.glb"]
        for k in kits[:1]:          # one scene per process (Interchange)
            import_kit(k)
