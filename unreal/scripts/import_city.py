"""The town's buildings (blender/city, docs/city_buildings.md) in Unreal.

  import: blender/export/city/<type>.glb (built by blender/city/build_city.py) -> /Game/City/Buildings/<type>/
          SM_<type> (Nanite, exact collision), SM_<type>_Detail (Nanite, no collision), SM_<type>_Windows (classic
          mesh, no collision, no shadow, M_City_Window); the materials /Game/City/Materials/MI_CT_<slot> from
          blender/city/citymats.py on the Academgorodok's masters and textures, plus M_City_Window.
  place:  place(face_y) puts an ALyudCityBuilding on every plot of unreal/Lyudmilovsk/Content/City/city_plan.json
          whose "building" type has been built (origin = the plot's "front", yaw = the plot's yaw: the way the front
          faces), fills its window table from blender/export/city/<type>.json, and fits a door in every street
          entrance (place_doors.py). Everything is tagged "City" (lyud.City 0 hides it).

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_city.py import [types]"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_city.py place"
(unreal/scripts/import_city.sh runs both.)
"""
import importlib.util
import json
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools"))
import world_map as W  # noqa: E402

EXPORT = os.path.join(REPO, "blender", "export", "city")
PLAN = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "City", "city_plan.json")
DEST = "/Game/City"
MATS = DEST + "/Materials"
ACADEM_MATS = "/Game/Academgorodok/Materials"
TAG = "City"
FOLDER = "Outside/City/Buildings"
# public rooms' open hours by building type (the rest: 8..21)
HOURS = {"shell_market": (7, 19), "department_store": (9, 20), "saucer_pavilion": (8, 22), "workers_club": (10, 23),
         "school": (7.5, 17), "clinic": (8, 20), "bus_stop_kiosk": (8, 22), "culture_block": (8, 22),
         "hyperboloid_tower": (0, 24), "round_sanatorium": (0, 24), "lifeguard_tower": (9, 19),
         "cog_club": (10, 23), "capsule_library": (7, 21), "shell_chapel_hall": (9, 23)}   # staff come before opening

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty


def log(msg):
    unreal.log_warning("CITY " + msg)


def _mod(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def built_types():
    # a glb counts only with its json (committed): stale glbs of removed variants are not imported
    if not os.path.isdir(EXPORT):
        return []
    return sorted(f[:-4] for f in os.listdir(EXPORT) if f.endswith(".glb") and os.path.exists(os.path.join(EXPORT, f[:-4] + ".json")))


# ------------------------------------------------------------------------------------------------ materials
def _lights_default():
    """T_City_LightsDefault: 4x4 dark texels, linear (the window master's Lights parameter default)."""
    import struct
    import zlib
    name = "T_City_LightsDefault"
    t = eal.load_asset("%s/%s" % (MATS, name)) if eal.does_asset_exist("%s/%s" % (MATS, name)) else None
    if t is None:
        tmp = os.path.join(REPO, "unreal", "Lyudmilovsk", "Saved", "city_defaults")
        os.makedirs(tmp, exist_ok=True)
        f = os.path.join(tmp, name + ".png")
        raw = b"".join(b"\x00" + bytes((0, 128, 0, 255)) * 4 for _ in range(4))

        def chunk(k, d):
            return struct.pack(">I", len(d)) + k + d + struct.pack(">I", zlib.crc32(k + d) & 0xffffffff)
        with open(f, "wb") as fh:
            fh.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
        task = unreal.AssetImportTask()
        for k, v in (("filename", f), ("destination_path", MATS), ("destination_name", name), ("automated", True),
                     ("replace_existing", True), ("save", False), ("factory", unreal.TextureFactory())):
            task.set_editor_property(k, v)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        t = eal.load_asset("%s/%s" % (MATS, name))
    t.set_editor_property("srgb", False)
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    t.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    eal.save_loaded_asset(t)
    return t


def _window_master(path):
    """M_City_Window: one opaque (masked) card per window. By day dark glass with a sky reflection and, behind it,
    a faint room and curtain; at night the room glows when its texel of the building's Lights texture is lit
    (UV 1 = the texel: R = light, G = how warm the lamp is, B = how much curtain). UV 0 runs 0..1 over the pane;
    2..3 marks a round window (the circle is cut by the opacity mask)."""
    import academ_materials as AM
    m = AM.master(path)
    node, link, scalar, vec = AM.node, AM.link, AM.scalar, AM.vec
    uv0 = node(m, unreal.MaterialExpressionTextureCoordinate, -2400, 200, coordinate_index=0)
    uv1 = node(m, unreal.MaterialExpressionTextureCoordinate, -2400, -300, coordinate_index=1)
    lights = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -2100, -300, parameter_name="Lights",
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    lights.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS)
    # a linear (non-sRGB) default, so the LINEAR_COLOR sampler compiles; every building's MID sets its own texture
    lights.set_editor_property("texture", _lights_default())
    link(uv1, "", lights, "UVs")
    # pane coordinates 0..1 and the round flag
    fr = node(m, unreal.MaterialExpressionFrac, -2100, 200)
    link(uv0, "", fr, "")
    ux = node(m, unreal.MaterialExpressionComponentMask, -2100, 350, r=True, g=False, b=False, a=False)
    link(uv0, "", ux, "")
    # round: distance to the centre < 0.5 where UV0.x > 1.5
    centre = node(m, unreal.MaterialExpressionConstant2Vector, -2100, 450, r=0.5, g=0.5)
    dist = node(m, unreal.MaterialExpressionDistance, -1900, 300)
    link(fr, "", dist, "A"); link(centre, "", dist, "B")
    circle = node(m, unreal.MaterialExpressionIf, -1700, 300)
    half = node(m, unreal.MaterialExpressionConstant, -1900, 420, r=0.5)
    one = node(m, unreal.MaterialExpressionConstant, -1900, 480, r=1.0)
    zero = node(m, unreal.MaterialExpressionConstant, -1900, 540, r=0.0)
    link(dist, "", circle, "A"); link(half, "", circle, "B")
    link(one, "", circle, "A < B"); link(zero, "", circle, "A > B"); link(zero, "", circle, "A == B")
    isround = node(m, unreal.MaterialExpressionIf, -1500, 350)
    onefive = node(m, unreal.MaterialExpressionConstant, -1700, 450, r=1.5)
    link(ux, "", isround, "A"); link(onefive, "", isround, "B")
    link(circle, "", isround, "A > B"); link(one, "", isround, "A < B"); link(one, "", isround, "A == B")
    mel.connect_material_property(isround, "", MP.MP_OPACITY_MASK)
    # the room behind: a curtain gradient across the pane (more curtain at the sides) and a darker floor band
    fx = node(m, unreal.MaterialExpressionComponentMask, -1900, 0, r=True, g=False, b=False, a=False)
    fy = node(m, unreal.MaterialExpressionComponentMask, -1900, 80, r=False, g=True, b=False, a=False)
    link(fr, "", fx, ""); link(fr, "", fy, "")
    cx = node(m, unreal.MaterialExpressionSubtract, -1750, 0, const_b=0.5)
    link(fx, "", cx, "A")
    ax = node(m, unreal.MaterialExpressionAbs, -1650, 0)
    link(cx, "", ax, "")
    curtain_amt = node(m, unreal.MaterialExpressionComponentMask, -1900, -150, r=False, g=False, b=True, a=False)
    link(lights, "RGB", curtain_amt, "")
    # curtain = smoothstep over |x-0.5| from (0.5 - amount*0.35)
    edge = node(m, unreal.MaterialExpressionMultiply, -1650, -150, const_b=-0.35)
    link(curtain_amt, "", edge, "A")
    edge2 = node(m, unreal.MaterialExpressionAdd, -1500, -150, const_b=0.5)
    link(edge, "", edge2, "A")
    cur = node(m, unreal.MaterialExpressionSubtract, -1400, -50)
    link(ax, "", cur, "A"); link(edge2, "", cur, "B")
    cur2 = node(m, unreal.MaterialExpressionMultiply, -1300, -50, const_b=12.0)
    link(cur, "", cur2, "A")
    curs = node(m, unreal.MaterialExpressionSaturate, -1200, -50)
    link(cur2, "", curs, "")
    # light level and colour: warm (G) from 2700 K-ish to a cooler 4000 K-ish
    lvl = node(m, unreal.MaterialExpressionComponentMask, -1900, -300, r=True, g=False, b=False, a=False)
    warm = node(m, unreal.MaterialExpressionComponentMask, -1900, -220, r=False, g=True, b=False, a=False)
    link(lights, "RGB", lvl, ""); link(lights, "RGB", warm, "")
    c_warm = vec(m, "WarmColor", -1700, -420, (1.0, 0.55, 0.22))
    c_cool = vec(m, "CoolColor", -1700, -340, (1.0, 0.82, 0.62))
    lc = node(m, unreal.MaterialExpressionLinearInterpolate, -1450, -380)
    link(c_cool, "", lc, "A"); link(c_warm, "", lc, "B"); link(warm, "", lc, "Alpha")
    # rooms are brighter near the ceiling (the lamp) and dimmer at the floor
    vgrad = node(m, unreal.MaterialExpressionLinearInterpolate, -1450, 80)
    c06 = node(m, unreal.MaterialExpressionConstant, -1600, 120, r=0.55)
    link(c06, "", vgrad, "A"); link(one, "", vgrad, "B"); link(fy, "", vgrad, "Alpha")
    curtain_glow = node(m, unreal.MaterialExpressionLinearInterpolate, -1100, -50)
    c12 = node(m, unreal.MaterialExpressionConstant, -1250, 40, r=1.35)
    link(one, "", curtain_glow, "A"); link(c12, "", curtain_glow, "B"); link(curs, "", curtain_glow, "Alpha")
    e1 = node(m, unreal.MaterialExpressionMultiply, -1000, -300)
    link(lc, "", e1, "A"); link(lvl, "", e1, "B")
    e2 = node(m, unreal.MaterialExpressionMultiply, -850, -250)
    link(e1, "", e2, "A"); link(vgrad, "", e2, "B")
    e3 = node(m, unreal.MaterialExpressionMultiply, -700, -200)
    link(e2, "", e3, "A"); link(curtain_glow, "", e3, "B")
    inten = scalar(m, "Intensity", -700, -50, 12.0)
    e4 = node(m, unreal.MaterialExpressionMultiply, -550, -150)
    link(e3, "", e4, "A"); link(inten, "", e4, "B")
    mel.connect_material_property(e4, "", MP.MP_EMISSIVE_COLOR)
    # base: dark glass, a bit lighter at grazing angles
    colr = vec(m, "Color", -700, 150, (0.012, 0.02, 0.025))
    fres = node(m, unreal.MaterialExpressionFresnel, -700, 250)
    fres.set_editor_property("exponent", 4.0)
    lift = vec(m, "GrazingColor", -700, 350, (0.05, 0.06, 0.07))
    base = node(m, unreal.MaterialExpressionLinearInterpolate, -450, 200)
    link(colr, "", base, "A"); link(lift, "", base, "B"); link(fres, "", base, "Alpha")
    mel.connect_material_property(base, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", -450, 350, 0.05), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Specular", -450, 450, 0.8), "", MP.MP_SPECULAR)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    AM.set_usage(m)
    mel.layout_material_expressions(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


def make_materials():
    import academ_materials as AM
    CM = _mod("citymats", os.path.join(REPO, "blender", "city", "citymats.py"))
    mp = _mod("academ_material_map", os.path.join(REPO, "blender", "academgorodok", "materials.py"))
    info = mp.load_texture_info(os.path.join(REPO, "textures", "academ"))
    m_tex = eal.load_asset(ACADEM_MATS + "/M_Academ_Master")
    m_flat = eal.load_asset(ACADEM_MATS + "/M_Academ_Flat")
    if m_tex is None or m_flat is None:
        raise RuntimeError("the Academgorodok's masters are missing: run import_academgorodok.sh import first")
    out = {}
    for slot, (hx, rough, texname, tile, what) in CM.MATERIALS.items():
        path = "%s/MI_%s" % (MATS, slot)
        c = mp.hex_lin(hx)
        tex_d = ACADEM_MATS + "/Textures/T_Academ_%s_D" % texname if texname else None
        if slot == "CT_Window":
            continue
        if texname and eal.does_asset_exist(tex_d):
            mi = AM.instance(path, m_tex)
            made = mp.hex_lin(info.get(texname, {}).get("target_hex", hx))
            AM._set(mi, "t", "BaseColor", eal.load_asset(tex_d))
            AM._set(mi, "t", "Normal", eal.load_asset(ACADEM_MATS + "/Textures/T_Academ_%s_N" % texname))
            AM._set(mi, "t", "Mask", eal.load_asset(ACADEM_MATS + "/Textures/T_Academ_%s_R" % texname))
            AM._set(mi, "v", "Tint", unreal.LinearColor(*[a / max(b, 1e-4) for a, b in zip(c, made)], 1))
            AM._set(mi, "s", "Tiling", 1.0 / tile)
            AM._set(mi, "s", "RoughnessScale", rough / max(info.get(texname, {}).get("roughness", rough), 0.05))
            AM._set(mi, "s", "AOStrength", 0.5)
            AM._set(mi, "s", "GrimeAmount", 0.35 if slot.startswith("CT_Concrete") else 0.15)
        else:
            mi = AM.instance(path, m_flat)
            AM._set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
            AM._set(mi, "s", "Roughness", rough)
            AM._set(mi, "s", "Metallic", 0.6 if slot == "CT_Aluminium" else (0.3 if "Steel" in slot else 0.0))
            AM._set(mi, "s", "Emissive", {"CT_Neon": 6.0, "CT_Opal": 3.0}.get(slot, 0.0))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        out[slot] = mi
    out["CT_Window"] = _window_master(MATS + "/M_City_Window")
    log("%d materials under %s" % (len(out), MATS))
    return out


# ------------------------------------------------------------------------------------------------ import
def import_type(type_id, mats):
    glb = os.path.join(EXPORT, type_id + ".glb")
    dest = "%s/Buildings/%s" % (DEST, type_id)
    if eal.does_directory_exist(dest):
        eal.delete_directory(dest)
    tmp = dest + "/_import"
    src = unreal.InterchangeManager.create_source_data(glb)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_asset(tmp, src, params)
    mgr.wait_until_all_tasks_done(True)
    n = 0
    for path in eal.list_assets(tmp, True):
        sm = eal.load_asset(path)
        if not isinstance(sm, unreal.StaticMesh):
            continue
        name = sm.get_name()
        windows = name.endswith("_Windows")
        detail = name.endswith("_Detail")
        for i, sl in enumerate(sm.get_editor_property("static_materials")):
            key = str(sl.material_slot_name)
            mi = mats.get(key) or next((v for k, v in mats.items() if key.startswith(k)), None)
            if mi is not None:
                sm.set_material(i, mi)
            else:
                log("no material for slot %s on %s" % (key, name))
        body = sm.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        settings = sm.get_editor_property("nanite_settings")
        settings.enabled = not windows                 # the masked window cards stay a classic mesh
        settings.fallback_target = unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
        settings.fallback_percent_triangles = 1.0      # collision = the full mesh (stairs stay stairs)
        sm.set_editor_property("nanite_settings", settings)
        if windows or detail:
            if body:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        target = "%s/%s" % (dest, name if name.startswith("SM_") else "SM_" + name)
        eal.save_loaded_asset(sm)
        eal.rename_asset(sm.get_path_name(), target)
        n += 1
    eal.delete_directory(tmp)
    eal.save_directory(dest)
    log("imported %s: %d meshes" % (type_id, n))


def import_all(types=None):
    mats = make_materials()
    for t in (types or built_types()):
        import_type(t, mats)
    eal.save_directory(DEST)


# ------------------------------------------------------------------------------------------------ place
def plots():
    with open(PLAN) as f:
        return json.load(f)["plots"]


def place(face_y):
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            eas.destroy_actor(a)
    cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudCityBuilding")
    kinds = {"flat": unreal.LyudWindowKind.FLAT, "stair": unreal.LyudWindowKind.STAIR,
             "public": unreal.LyudWindowKind.PUBLIC}
    n = 0
    doors = []
    for p in plots():
        t = p.get("building")
        tv = "%s__%s" % (t, p["id"].lower().replace("-", "_")) if t else None
        if tv and os.path.exists(os.path.join(EXPORT, tv + ".json")):
            t = tv                                          # this plot's own variant of the type
        jpath = os.path.join(EXPORT, "%s.json" % t) if t else None
        dest = "%s/Buildings/%s" % (DEST, t)
        if not t or not jpath or not os.path.exists(jpath) or not eal.does_asset_exist("%s/SM_%s" % (dest, t)):
            continue
        with open(jpath) as f:
            data = json.load(f)
        fx, fy, fz = p["front"]
        a_rad = math.radians(p["yaw"])
        fdir = (math.cos(a_rad), math.sin(a_rad))          # the way the front faces (map = Unreal axes)
        right = (fdir[1], -fdir[0])                         # building +x: to the right of someone facing the front
        inland = (-fdir[0], -fdir[1])                       # building +y

        def to_ue(lx, ly, lz, fx=fx, fy=fy, fz=fz, right=right, inland=inland):
            x = fx + lx * right[0] + ly * inland[0]
            y = fy + lx * right[1] + ly * inland[1]
            return W.to_unreal(x, y, fz + lz, face_y)
        # the mesh: Blender x -> Unreal +X, Blender y -> Unreal -Y; its front (-y) is Unreal +Y: turn +Y onto fdir
        yaw = p["yaw"] - 90.0
        loc = unreal.Vector(*to_ue(0, 0, 0))
        b = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(0.0, 0.0, yaw))
        b.set_actor_label("City_%s_%s" % (p["id"], t))
        b.set_folder_path(FOLDER)
        b.tags = [TAG, "NoGrass", "NoTree", "Roof"]
        for comp, suffix in (("shell", ""), ("detail", "_Detail"), ("windows", "_Windows")):
            sm = eal.load_asset("%s/SM_%s%s" % (dest, t, suffix)) if eal.does_asset_exist("%s/SM_%s%s" % (dest, t, suffix)) else None
            c = b.get_editor_property(comp)
            if sm is not None:
                c.set_static_mesh(sm)
            c.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
        b.get_editor_property("windows").set_editor_property("ld_max_draw_distance", 120000.0)
        wins = sorted(data["windows"], key=lambda w: w["slot"])
        b.set_editor_property("plot_id", p["id"])
        b.set_editor_property("type_id", t)
        b.set_editor_property("window_ids", [w["id"] for w in wins])
        b.set_editor_property("window_kinds", [kinds.get(w["kind"], kinds["public"]) if w["kind"] in kinds else
                                               (kinds["flat"] if w.get("flat") else kinds["public"]) for w in wins])
        b.set_editor_property("window_flats", [w.get("flat") or "" for w in wins])
        lamps = data.get("lamps", [])
        b.set_editor_property("lamp_positions", [unreal.Vector(l["pos"][0] * 100, -l["pos"][1] * 100, l["pos"][2] * 100) for l in lamps])
        b.set_editor_property("lamp_lumens", [float(l["lumens"]) for l in lamps])
        b.set_editor_property("lamp_colors", [unreal.LinearColor(*[(int(l["colour"][k:k + 2], 16) / 255.0) ** 2.2 for k in (1, 3, 5)], 1.0) for l in lamps])
        b.set_editor_property("lamp_radii", [float(l.get("radius", 7.0)) * 100 for l in lamps])
        b.set_editor_property("lamp_kinds", [0 if l["kind"] == "public" else 1 for l in lamps])
        oh = HOURS.get(t.split("__")[0], (8, 21))
        b.set_editor_property("open_hour", float(oh[0]))
        b.set_editor_property("close_hour", float(oh[1]))
        n += 1
        for e in data.get("entrances", []):
            if e.get("kind") not in ("street", None):
                continue
            yaw_e = math.radians(e.get("yaw", 0.0))
            out_dir = (math.sin(yaw_e), -math.cos(yaw_e))                 # outward, building frame
            at = (e["pos"][0] - out_dir[0] * 0.15, e["pos"][1] - out_dir[1] * 0.15, e["pos"][2])
            doors.append(({"name": "%s_%s" % (p["id"], e["id"]), "at": list(at), "out": [-out_dir[0], -out_dir[1]],
                           "hole": [e.get("width", 1.1), e.get("height", 2.2)], "wall": 0.3,
                           "leaves": e.get("leaves", 1), "hinge": "left", "frame": True, "open": False,
                           "locked": False}, to_ue))
    if doors:
        import place_doors
        for entry, fn in doors:
            place_doors.spawn_doors([entry], fn, tags=[TAG], folder=FOLDER + "/Doors", label_prefix="City_Door_", log=log)
    log("placed %d buildings, %d doors" % (n, len(doors)))


if __name__ == "__main__":
    args = sys.argv[1:]
    if "import" in args:
        rest = [a for a in args if a not in ("import", "place")]
        import_all(rest[0].split(",") if rest else None)
    if "place" in args:
        with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
            face_y = json.load(f).get("outside", {}).get("portal_face_y", 790.0)
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not les.load_level("/Game/Scene01/Lvl_Scene01"):
            raise RuntimeError("no /Game/Scene01/Lvl_Scene01")
        place(face_y)
        les.save_current_level()
        log("done")
