"""The winter's assets under /Game/Winter (docs/winter.md), and its pieces in the ground's and the plants' materials:

  /Game/Winter/MPC_LyudWinter                      what ULyudWinterSubsystem tells the materials
  /Game/Winter/RT_SnowDepth, RT_SnowCover, RT_SeaIce  its data textures (written from the CPU at run time)
  /Game/Winter/M_SnowDecal                         one decal over the map: the snow on the ground and on everything
                                                   under the open sky
  /Game/Winter/M_SeaIce                            the sea's ice sheet (ALyudSeaIce)
  /Game/Scene01/Terrain/M_Ground                   lifted by the snow (winter_snow.ground_wpo; import_terrain.py adds it
                                                   when it builds the ground, this patches an existing one)
  /Game/Plants/M_PlantLeaf, M_PlantNeedle          rebuilt by their scripts with snow on the branches

HLSL in tools/winter/winter_hlsl.py. Headless:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_winter_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import winter_snow as W  # noqa: E402

H = W.H
X = unreal
MP = unreal.MaterialProperty
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3


def new_material(name):
    path = W.WINTER + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, W.WINTER, unreal.Material, unreal.MaterialFactoryNew())


def decal_material(mpc, rts):
    m = new_material("M_SnowDecal")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("tangent_space_normal", False)
    pos = W._node(m, X.MaterialExpressionWorldPosition, -1200, 0)
    cam = W._node(m, X.MaterialExpressionCameraPositionWS, -1200, 120)
    dep = W._node(m, X.MaterialExpressionTextureObject, -1200, 240, texture=rts["RT_SnowDepth"])
    cov = W._node(m, X.MaterialExpressionTextureObject, -1200, 360, texture=rts["RT_SnowCover"])
    ins = [("WP", pos), ("Cam", cam), ("Dep", dep), ("Cov", cov)]
    for i, (pin, name) in enumerate((("FRect", "FieldRect"), ("FT", "FieldTexel"), ("CRect", "CoverRect"), ("CT", "CoverTexel"),
                                     ("S", "Snow"), ("B", "SnowB"))):
        ins.append((pin, W.mpc_node(m, mpc, name, -1200, 480 + i * 120)))
    c = W._custom(m, H.DECAL, ins, -500, 0, "WinterSnowDecal", out=F3,
                  extra=(("Opacity", F1), ("Normal", F3), ("Roughness", F1)))
    mel.connect_material_property(c, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(c, "Opacity", MP.MP_OPACITY)
    mel.connect_material_property(c, "Normal", MP.MP_NORMAL)
    mel.connect_material_property(c, "Roughness", MP.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    W._log("M_SnowDecal built")


def ice_material(mpc, rts):
    m = new_material("M_SeaIce")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    pos = W._node(m, X.MaterialExpressionWorldPosition, -1200, 0)
    tex = W._node(m, X.MaterialExpressionTextureObject, -1200, 120, texture=rts["RT_SeaIce"])
    ins = [("P", pos), ("Tex", tex)]
    for i, (pin, name) in enumerate((("R", "IceRect"), ("I", "Ice"), ("Rail", "IceRail"), ("Dr", "IceDrift"), ("S", "Snow"))):
        ins.append((pin, W.mpc_node(m, mpc, name, -1200, 240 + i * 120)))
    c = W._custom(m, H.ICE, ins, -500, 0, "WinterSeaIce", out=F3, extra=(("Mask", F1), ("Roughness", F1)))
    mel.connect_material_property(c, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(c, "Mask", MP.MP_OPACITY_MASK)
    mel.connect_material_property(c, "Roughness", MP.MP_ROUGHNESS)
    spec = W._node(m, X.MaterialExpressionConstant, -300, 300, r=0.5)
    mel.connect_material_property(spec, "", MP.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    W._log("M_SeaIce built")


def patch_ground():
    path = "/Game/Scene01/Terrain/M_Ground"
    if not eal.does_asset_exist(path):
        W._log("no M_Ground (build the terrain first)")
        return
    m = unreal.load_asset(path)
    if mel.get_material_property_input_node(m, MP.MP_WORLD_POSITION_OFFSET):
        W._log("M_Ground already lifted by the snow")
        return
    W.ground_wpo(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    W._log("M_Ground lifted by the snow")


def rebuild_plants():
    # the plants' own scripts build the leaves and needles with leaf_snow
    for name in ("make_plant_materials.py", "make_needle_material.py"):
        with open(os.path.join(HERE, name)) as f:
            code = compile(f.read(), name, "exec")
        exec(code, {"__name__": "__winter__", "__file__": os.path.join(HERE, name)})
        W._log("%s rebuilt" % name)


mpc = W.collection()
rts = W.render_targets()
decal_material(mpc, rts)
ice_material(mpc, rts)
patch_ground()
if os.environ.get("WINTER_PLANTS", "1") == "1":
    rebuild_plants()
W._log("done")
