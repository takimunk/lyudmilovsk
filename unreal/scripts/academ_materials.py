"""Materials of the Academgorodok (Laboratory No. 2 housing, Moscow 1945-48) for Unreal: imports the tileable
texture sets of textures/academ/ (tools/fal_academ_textures.py) and builds

  M_Academ_Master   opaque master: BaseColor texture x Tint, Normal, roughness = Mask.R x RoughnessScale,
                    ambient occlusion = lerp(1, Mask.G, AOStrength), an optional world-aligned grime/streak
                    overlay (GrimeAmount; walls only, 1 tile = GrimeTileCm), all textures on UV 0 x Tiling
                    (the meshes are unwrapped 1 UV unit = 1 m, so Tiling = 1 / metres per texture tile)
  M_Academ_Flat     opaque, parameter-only (Color, Roughness, Metallic, Specular): frames, iron, zinc
  M_Academ_Glass    opaque window glass: dark, low roughness, raised specular (no translucency, for speed)
  MI_AC_<slot>      one MaterialInstanceConstant per slot of blender/academgorodok/common.py MATERIALS

The slot -> texture/tint/roughness mapping is shared with the Blender preview: this file imports
blender/academgorodok/materials.py (it has no bpy at import time). All masters carry the Nanite and instanced
static mesh usage flags (a Nanite mesh whose material lacks the Nanite usage draws the default material).

Used from a headless commandlet:
  import academ_materials; mats = academ_materials.make_materials()        # {slot: MaterialInterface}
or  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import importlib.util
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
TEX_DIR = os.path.join(REPO, "textures", "academ")
DEST = "/Game/Academgorodok/Materials"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty


def log(msg):
    unreal.log_warning("ACADEM materials: " + msg)


def _mapping():
    """blender/academgorodok/materials.py (SLOTS, tint, roughness_scale, ...) and its common module."""
    path = os.path.join(REPO, "blender", "academgorodok", "materials.py")
    spec = importlib.util.spec_from_file_location("academ_material_map", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# ------------------------------------------------------------------------------------------- helpers

def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def scalar(m, name, x, y, v):
    return node(m, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


def vec(m, name, x, y, col):
    return node(m, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                default_value=unreal.LinearColor(col[0], col[1], col[2], 1))


def master(path):
    """An empty material at path. An existing one is emptied in place (not deleted), so instances and meshes
    that already point at it stay valid."""
    m = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if m is not None and isinstance(m, unreal.Material):
        mel.delete_all_material_expressions(m)
        return m
    if m is not None:
        eal.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def set_usage(m):
    """Nanite + (instanced) static mesh usage. UE 5.8: SetBaseMaterialUsage(Material, EMaterialUsage, bValue);
    the old bUsedWithNanite UPROPERTY still exists but is deprecated in C++, so it is only a fallback."""
    usages = [("MATUSAGE_NANITE", "used_with_nanite"),
              ("MATUSAGE_INSTANCED_STATIC_MESHES", "used_with_instanced_static_meshes")]
    for enum_name, prop in usages:
        done = False
        usage = getattr(unreal.MaterialUsage, enum_name, None) if hasattr(unreal, "MaterialUsage") else None
        if usage is not None and hasattr(mel, "set_base_material_usage"):
            try:
                mel.set_base_material_usage(m, usage, True)
                done = True
            except Exception as e:      # noqa: BLE001  (keep going with the property fallback)
                log("set_base_material_usage %s failed: %s" % (enum_name, e))
        if not done:
            try:
                m.set_editor_property(prop, True)
                done = True
            except Exception as e:      # noqa: BLE001
                log("set %s failed: %s" % (prop, e))
        if usage is not None and hasattr(mel, "has_material_usage") and not mel.has_material_usage(m, usage):
            log("WARNING %s lacks %s" % (m.get_name(), enum_name))


def finish(m):
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    set_usage(m)
    mel.layout_material_expressions(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m, False)


# ------------------------------------------------------------------------------------------- textures

def import_textures(tex_names, tex_dir, dest):
    """textures/academ/<name>_{D,N,R}.jpg and academ_grime_mask.jpg -> dest/Textures/T_Academ_*;
    returns {(name, suffix): Texture2D}, suffix "D", "N", "R" ("G" for the grime mask)."""
    files = []
    for name in tex_names:
        for suf in ("D", "N", "R"):
            files.append((name, suf, os.path.join(tex_dir, "%s_%s.jpg" % (name, suf)), "T_Academ_%s_%s" % (name, suf)))
    files.append(("grime", "G", os.path.join(tex_dir, "academ_grime_mask.jpg"), "T_Academ_GrimeMask"))
    tasks = []
    for name, suf, path, asset in files:
        if not os.path.exists(path):
            raise RuntimeError("missing texture %s (run tools/fal_academ_textures.py)" % path)
        t = unreal.AssetImportTask()
        t.filename = path
        t.destination_path = dest + "/Textures"
        t.destination_name = asset
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    out = {}
    for (name, suf, path, asset), task in zip(files, tasks):
        tex = eal.load_asset("%s/Textures/%s" % (dest, asset)) if eal.does_asset_exist("%s/Textures/%s" % (dest, asset)) else None
        if tex is None:
            # the first texture of a fresh process sometimes goes missing (Interchange): once more alone
            tools.import_asset_tasks([task])
            got = list(task.get_editor_property("imported_object_paths") or [])
            tex = eal.load_asset(got[0]) if got else None
        if tex is None:
            raise RuntimeError("import failed: " + path)
        if suf == "D":
            tex.set_editor_property("srgb", True)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        elif suf == "N":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", False)     # already DirectX green
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        else:                                                       # R mask and grime: linear data
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_SPECULAR)
        eal.save_loaded_asset(tex, False)
        out[(name, suf)] = tex
    return out


# ------------------------------------------------------------------------------------------- masters

def build_master(path, d_tex, n_tex, r_tex, grime_tex, grime_colour, grime_tile_m):
    m = master(path)
    # UV 0 (metres) x Tiling
    uv0 = node(m, unreal.MaterialExpressionTextureCoordinate, -2200, 0, coordinate_index=0)
    tiling = scalar(m, "Tiling", -2200, 120, 1.0)
    uv = node(m, unreal.MaterialExpressionMultiply, -2000, 40)
    link(uv0, "", uv, "A"); link(tiling, "", uv, "B")
    d = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1700, -400, parameter_name="BaseColor",
             texture=d_tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    n = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1700, 300, parameter_name="Normal",
             texture=n_tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    r = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1700, 0, parameter_name="Mask",
             texture=r_tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    for t in (d, n, r):
        link(uv, "", t, "UVs")
    # base colour = D x Tint, darkened toward GrimeColour by the grime mask
    tint = vec(m, "Tint", -1700, -600, (1, 1, 1))
    col = node(m, unreal.MaterialExpressionMultiply, -1350, -450)
    link(d, "RGB", col, "A"); link(tint, "", col, "B")
    # world-aligned grime: U along the wall (x + y), V down the height (texture top = streak start)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -2600, -1000,
              world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    wx = node(m, unreal.MaterialExpressionComponentMask, -2400, -1100, r=True, g=False, b=False, a=False)
    wy = node(m, unreal.MaterialExpressionComponentMask, -2400, -1000, r=False, g=True, b=False, a=False)
    wz = node(m, unreal.MaterialExpressionComponentMask, -2400, -900, r=False, g=False, b=True, a=False)
    for c in (wx, wy, wz):
        link(wp, "", c, "")
    gu = node(m, unreal.MaterialExpressionAdd, -2250, -1050)
    link(wx, "", gu, "A"); link(wy, "", gu, "B")
    gv = node(m, unreal.MaterialExpressionMultiply, -2250, -900, const_b=-1.0)
    link(wz, "", gv, "A")
    guv = node(m, unreal.MaterialExpressionAppendVector, -2100, -1000)
    link(gu, "", guv, "A"); link(gv, "", guv, "B")
    gtile = scalar(m, "GrimeTileCm", -2100, -850, grime_tile_m * 100.0)
    guv2 = node(m, unreal.MaterialExpressionDivide, -1950, -1000)
    link(guv, "", guv2, "A"); link(gtile, "", guv2, "B")
    g = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1750, -1000, parameter_name="GrimeMask",
             texture=grime_tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    link(guv2, "", g, "UVs")
    # only on walls: 1 - |N.z| of the vertex normal
    vn = node(m, unreal.MaterialExpressionVertexNormalWS, -1750, -750)
    vz = node(m, unreal.MaterialExpressionComponentMask, -1600, -750, r=False, g=False, b=True, a=False)
    link(vn, "", vz, "")
    vabs = node(m, unreal.MaterialExpressionAbs, -1480, -750)
    link(vz, "", vabs, "")
    wall = node(m, unreal.MaterialExpressionOneMinus, -1380, -750)
    link(vabs, "", wall, "")
    gm1 = node(m, unreal.MaterialExpressionMultiply, -1300, -950)
    link(g, "R", gm1, "A"); link(wall, "", gm1, "B")
    amount = scalar(m, "GrimeAmount", -1300, -850, 0.0)
    gm2 = node(m, unreal.MaterialExpressionMultiply, -1150, -900)
    link(gm1, "", gm2, "A"); link(amount, "", gm2, "B")
    gsat = node(m, unreal.MaterialExpressionSaturate, -1030, -900)
    link(gm2, "", gsat, "")
    gcol = vec(m, "GrimeColour", -1150, -650, grime_colour)
    dirty = node(m, unreal.MaterialExpressionMultiply, -1000, -550)
    link(col, "", dirty, "A"); link(gcol, "", dirty, "B")
    base = node(m, unreal.MaterialExpressionLinearInterpolate, -800, -450)
    link(col, "", base, "A"); link(dirty, "", base, "B"); link(gsat, "", base, "Alpha")
    mel.connect_material_property(base, "", MP.MP_BASE_COLOR)
    # roughness = saturate(Mask.R x RoughnessScale)
    rs = scalar(m, "RoughnessScale", -1350, 150, 1.0)
    rough = node(m, unreal.MaterialExpressionMultiply, -1150, 50)
    link(r, "R", rough, "A"); link(rs, "", rough, "B")
    rsat = node(m, unreal.MaterialExpressionSaturate, -1000, 50)
    link(rough, "", rsat, "")
    mel.connect_material_property(rsat, "", MP.MP_ROUGHNESS)
    # ambient occlusion = lerp(1, Mask.G, AOStrength)
    one = node(m, unreal.MaterialExpressionConstant, -1350, 250, r=1.0)
    aos = scalar(m, "AOStrength", -1350, 330, 0.5)
    ao = node(m, unreal.MaterialExpressionLinearInterpolate, -1150, 250)
    link(one, "", ao, "A"); link(r, "G", ao, "B"); link(aos, "", ao, "Alpha")
    mel.connect_material_property(ao, "", MP.MP_AMBIENT_OCCLUSION)
    # normal, with a strength (lerp from flat)
    flat = node(m, unreal.MaterialExpressionConstant3Vector, -1350, 450, constant=unreal.LinearColor(0, 0, 1, 1))
    ns = scalar(m, "NormalStrength", -1350, 550, 1.0)
    nl = node(m, unreal.MaterialExpressionLinearInterpolate, -1150, 450)
    link(flat, "", nl, "A"); link(n, "RGB", nl, "B"); link(ns, "", nl, "Alpha")
    mel.connect_material_property(nl, "", MP.MP_NORMAL)
    mel.connect_material_property(scalar(m, "Metallic", -800, 100, 0.0), "", MP.MP_METALLIC)
    mel.connect_material_property(scalar(m, "Specular", -800, 200, 0.5), "", MP.MP_SPECULAR)
    finish(m)
    return m


def build_flat(path):
    m = master(path)
    col = vec(m, "Color", -500, -100, (0.5, 0.5, 0.5))
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    glow = node(m, unreal.MaterialExpressionMultiply, -250, -250)       # lit rooms, lamp glass
    link(col, "", glow, "A"); link(scalar(m, "Emissive", -500, -300, 0.0), "", glow, "B")
    mel.connect_material_property(glow, "", MP.MP_EMISSIVE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", -500, 50, 0.5), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Metallic", -500, 150, 0.0), "", MP.MP_METALLIC)
    mel.connect_material_property(scalar(m, "Specular", -500, 250, 0.5), "", MP.MP_SPECULAR)
    finish(m)
    return m


def build_glass(path):
    """Opaque glass: the room behind is not modelled, so a dark base with a strong, slightly blurred reflection
    reads right from the street; a little Fresnel lift of the base at grazing angles stands in for the sky."""
    m = master(path)
    colr = vec(m, "Color", -700, -200, (0.012, 0.022, 0.028))
    fres = node(m, unreal.MaterialExpressionFresnel, -700, -50)
    fres.set_editor_property("exponent", 4.0)
    lift = vec(m, "GrazingColor", -700, 50, (0.05, 0.06, 0.07))
    lr = node(m, unreal.MaterialExpressionLinearInterpolate, -400, -150)
    link(colr, "", lr, "A"); link(lift, "", lr, "B"); link(fres, "", lr, "Alpha")
    mel.connect_material_property(lr, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", -400, 100, 0.05), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Specular", -400, 200, 0.8), "", MP.MP_SPECULAR)
    mel.connect_material_property(scalar(m, "Metallic", -400, 300, 0.0), "", MP.MP_METALLIC)
    finish(m)
    return m


# ------------------------------------------------------------------------------------------- instances

def instance(path, parent):
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if mi is not None and not isinstance(mi, unreal.MaterialInstanceConstant):
        eal.delete_asset(path)
        mi = None
    if mi is None:
        folder, name = path.rsplit("/", 1)
        mi = tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    return mi


def _set(mi, kind, name, value):
    fn = {"s": mel.set_material_instance_scalar_parameter_value,
          "v": mel.set_material_instance_vector_parameter_value,
          "t": mel.set_material_instance_texture_parameter_value}[kind]
    fn(mi, name, value)          # UE 5.8 returns False even when the value is set


def _srgb_to_linear(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def make_materials(dest=DEST, tex_dir=TEX_DIR, extra=None):
    """Imports the textures, (re)builds the masters and one MI_AC_<slot> per common.MATERIALS key under dest.
    extra: {slot: (hex, roughness, emissive)} more flat-colour slots (rooms behind windows, the shop).
    Returns {slot name: MaterialInterface}."""
    mp = _mapping()
    common = mp.common
    info = mp.load_texture_info(tex_dir)
    tex_names = sorted({s[0] for s in mp.SLOTS.values() if s[0]})
    tex = import_textures(tex_names, tex_dir, dest)
    first = "plaster_ochre" if "plaster_ochre" in tex_names else tex_names[0]
    m_tex = build_master(dest + "/M_Academ_Master", tex[(first, "D")], tex[(first, "N")], tex[(first, "R")],
                         tex[("grime", "G")], mp.GRIME_MULT,
                         mp.GRIME_TILE_M)
    m_flat = build_flat(dest + "/M_Academ_Flat")
    m_glass = build_glass(dest + "/M_Academ_Glass")
    out = {}
    for slot, (hexcol, rough, what) in common.MATERIALS.items():
        texname, grime, metallic, ao = mp.SLOTS[slot]
        path = "%s/MI_%s" % (dest, slot)
        t = mp.tint(slot, info)
        if slot == "AC_Glass":
            mi = instance(path, m_glass)
            _set(mi, "v", "Color", unreal.LinearColor(t[0], t[1], t[2], 1))
            _set(mi, "s", "Roughness", rough)
        elif texname is None:
            mi = instance(path, m_flat)
            _set(mi, "v", "Color", unreal.LinearColor(t[0], t[1], t[2], 1))
            _set(mi, "s", "Roughness", rough)
            _set(mi, "s", "Metallic", metallic)
        else:
            mi = instance(path, m_tex)
            _set(mi, "t", "BaseColor", tex[(texname, "D")])
            _set(mi, "t", "Normal", tex[(texname, "N")])
            _set(mi, "t", "Mask", tex[(texname, "R")])
            _set(mi, "v", "Tint", unreal.LinearColor(t[0], t[1], t[2], 1))
            _set(mi, "s", "Tiling", 1.0 / mp.tile_m(texname, info))
            _set(mi, "s", "RoughnessScale", mp.roughness_scale(slot, info))
            _set(mi, "s", "AOStrength", ao)
            _set(mi, "s", "GrimeAmount", grime)
            _set(mi, "s", "Metallic", metallic)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        out[slot] = mi
    for slot, (hexcol, rough, emissive) in (extra or {}).items():
        mi = instance("%s/MI_%s" % (dest, slot), m_flat)
        c = _srgb_to_linear(hexcol)
        _set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
        _set(mi, "s", "Roughness", rough)
        _set(mi, "s", "Emissive", emissive)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        out[slot] = mi
    log("%d instances under %s (master Nanite usage: %s)" % (
        len(out), dest, mel.has_material_usage(m_tex, unreal.MaterialUsage.MATUSAGE_NANITE)
        if hasattr(unreal, "MaterialUsage") else "?"))
    return out


if __name__ == "__main__":
    make_materials()
