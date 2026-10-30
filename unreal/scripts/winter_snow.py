"""Winter pieces other material scripts put into their materials (docs/winter.md):

  collection()          /Game/Winter/MPC_LyudWinter (ULyudWinterSubsystem fills it; created once, only new parameters added)
  ground_wpo(m)         the landscape's material: lifted by the snow's depth (RT_SnowDepth), import_terrain.py calls it
  leaf_snow(m, base)    leaves and needles: their colour turns white on the upper side by the snow on the branches

The HLSL is in tools/winter/winter_hlsl.py.
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "winter"))
import winter_hlsl as H  # noqa: E402

WINTER = "/Game/Winter"
MPC_PATH = WINTER + "/MPC_LyudWinter"
DEPTH_RT = WINTER + "/RT_SnowDepth"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
EXCL = unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS

VECTORS = [("FieldRect", (0, 0, 0, 0)), ("FieldTexel", (0, 0, 100, 0)), ("CoverRect", (0, 0, 0, 0)), ("CoverTexel", (0, 0, 200, 0)),
           ("IceRect", (0, 0, 0, 0)), ("Snow", (0, 1, 0, 0)), ("SnowB", (0, 0, -43, 0)), ("Ice", (10, 3, 0, 0)),
           ("IceRail", (0, 0, -1, 4)), ("IceDrift", (0, 0, -4300, 1947)),
           ("NearRect", (0, 0, 0, 400)), ("NearGround", (0, 0, 100, 48)), ("NearTrail", (5, 4000, 0, 0))]
# render targets: name, format, filter
TARGETS = [("RT_SnowDepth", "RTF_R16F"), ("RT_SnowCover", "RTF_RG16F"), ("RT_SeaIce", "RTF_R16F"), ("RT_NearGround", "RTF_RG32F"),
           ("RT_SnowTrail", "RTF_R8")]
WRAP = ("RT_SnowTrail",)


def _log(msg):
    unreal.log_warning("WINTER MATERIALS " + msg)


def collection():
    if not eal.does_directory_exist(WINTER):
        eal.make_directory(WINTER)
    if eal.does_asset_exist(MPC_PATH):
        mpc = unreal.load_asset(MPC_PATH)
    else:
        mpc = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MPC_LyudWinter", WINTER, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    have = {str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("vector_parameters")}
    vs = list(mpc.get_editor_property("vector_parameters"))
    for name, v in VECTORS:
        if name not in have:
            p = unreal.CollectionVectorParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", unreal.LinearColor(*[float(c) for c in v]))
            vs.append(p)
    mpc.set_editor_property("vector_parameters", vs)
    eal.save_loaded_asset(mpc)
    return mpc


def render_targets():
    """The winter's data textures (the subsystem resizes them and writes them from the CPU)."""
    out = {}
    for name, fmt in TARGETS:
        path = WINTER + "/" + name
        if eal.does_asset_exist(path):
            rt = unreal.load_asset(path)
        else:
            rt = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, WINTER, unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
        rt.set_editor_property("render_target_format", getattr(unreal.TextureRenderTargetFormat, fmt))
        rt.set_editor_property("size_x", 64)
        rt.set_editor_property("size_y", 64)
        rt.set_editor_property("clear_color", unreal.LinearColor(0, 0, 0, 0))
        addr = unreal.TextureAddress.TA_WRAP if name in WRAP else unreal.TextureAddress.TA_CLAMP
        rt.set_editor_property("address_x", addr)
        rt.set_editor_property("address_y", addr)
        rt.set_editor_property("auto_generate_mips", False)
        rt.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
        eal.save_loaded_asset(rt)
        out[name] = rt
    return out


def _node(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def _custom(m, code, inputs, x, y, desc, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, extra=()):
    c = _node(m, unreal.MaterialExpressionCustom, x, y, code=code, output_type=out, description=desc)
    ins = []
    for name, _ in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    outs = []
    for name, typ in extra:
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", typ)
        outs.append(o)
    if outs:
        c.set_editor_property("additional_outputs", outs)
    for name, src in inputs:
        s, p = src if isinstance(src, tuple) else (src, "")
        if not mel.connect_material_expressions(s, p, c, name):
            raise RuntimeError("%s: no pin %r" % (desc, name))
    return c


def mpc_node(m, mpc, name, x, y):
    e = _node(m, unreal.MaterialExpressionCollectionParameter, x, y, collection=mpc)
    e.set_editor_property("parameter_name", name)
    return e


def ground_wpo(m, x=-700, y=1200):
    """Lifts a landscape material's ground by the snow (call before recompiling; the landscape is not Nanite)."""
    mpc = collection()
    rts = render_targets()
    pos = _node(m, unreal.MaterialExpressionWorldPosition, x - 400, y, world_position_shader_offset=EXCL)
    dep = _node(m, unreal.MaterialExpressionTextureObject, x - 400, y + 120, texture=rts["RT_SnowDepth"])
    c = _custom(m, H.GROUND_WPO, [("P", pos), ("Dep", dep), ("Rect", mpc_node(m, mpc, "FieldRect", x - 400, y + 240)),
                                  ("B", mpc_node(m, mpc, "SnowB", x - 400, y + 360)),
                                  ("N", mpc_node(m, mpc, "NearRect", x - 400, y + 480)),
                                  ("Gt", _node(m, unreal.MaterialExpressionTextureObject, x - 400, y + 600, texture=rts["RT_NearGround"])),
                                  ("NG", mpc_node(m, mpc, "NearGround", x - 400, y + 720))], x, y, "WinterSnowWPO")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return c


def branch_snow(m, snow_param, x=-700, y=500):
    """A bark's Snow parameter (the plant's own) or the snow on the branches now (MPC SnowB.x), whichever is more."""
    mpc = collection()
    b = mpc_node(m, mpc, "SnowB", x - 200, y + 120)
    r = _node(m, unreal.MaterialExpressionComponentMask, x, y + 120, r=True, g=False, b=False, a=False)
    mel.connect_material_expressions(b, "", r, "")
    mx = _node(m, unreal.MaterialExpressionMax, x + 150, y)
    mel.connect_material_expressions(snow_param, "", mx, "A")
    mel.connect_material_expressions(r, "", mx, "B")
    return mx


def leaf_snow(m, base, x=-250, y=-500):
    """base colour -> base colour whitened on the upper side by the snow on the branches (MPC SnowB.x)."""
    mpc = collection()
    nrm = _node(m, unreal.MaterialExpressionVertexNormalWS, x - 400, y)
    pos = _node(m, unreal.MaterialExpressionWorldPosition, x - 400, y + 120, world_position_shader_offset=EXCL)
    k = _custom(m, H.LEAF_SNOW, [("N", nrm), ("P", pos), ("B", mpc_node(m, mpc, "SnowB", x - 400, y + 240))], x, y, "WinterLeafSnow",
                out=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    white = _node(m, unreal.MaterialExpressionConstant3Vector, x, y + 150, constant=unreal.LinearColor(0.82, 0.85, 0.9, 1))
    lerp = _node(m, unreal.MaterialExpressionLinearInterpolate, x + 200, y)
    mel.connect_material_expressions(base, "", lerp, "A")
    mel.connect_material_expressions(white, "", lerp, "B")
    mel.connect_material_expressions(k, "", lerp, "Alpha")
    return lerp
