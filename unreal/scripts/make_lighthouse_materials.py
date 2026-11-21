"""The Mayak lighthouse's own materials under /Game/South/Materials (docs/lighthouse.md), small and committed:

  M_South_LanternGlass  the lantern's twelve panes (lighthouse.glb slot): dark see-through glass by day, warm when the lamp
                        burns; scalars Glow (0 out .. 1 full, with the flash on top: ALyudLighthouse sets it) and Day
  M_South_LensGlass     the Fresnel lens's panels (lighthouse_lens.glb): faint rings by day, lit from inside by Glow
  M_South_Beam          the beam's crossed cards (lighthouse_beam.glb): additive, unlit, two-sided; Strength, fading to
                        nothing along U (the card's length) and across V, and near the camera so a card never shows its edge

Called by import_south.py's materials step (before the import, so the slots find them), or alone:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_lighthouse_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_sea_materials as S  # noqa: E402

X = unreal
MP = unreal.MaterialProperty
DEST = "/Game/South/Materials"
WARM = (1.0, 0.82, 0.55)


def _log(msg):
    unreal.log_warning("SOUTH lighthouse materials: " + msg)


def _set(m, **props):
    for k, v in props.items():
        try:
            m.set_editor_property(k, v)
        except Exception as e:  # noqa: BLE001
            _log("%s: %s" % (k, e))


def _uv(g):
    return g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=0)


def lantern_glass():
    g = S.Graph("M_South_LanternGlass", dest=DEST)
    _set(g.m, blend_mode=unreal.BlendMode.BLEND_TRANSLUCENT, shading_model=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True,
         used_with_static_lighting=False)
    glow = g.param("Glow", 0.0)
    day = g.param("Day", 1.0)
    c = g.custom(r"""
float l = saturate(Glow);
// by day: the sky's grey-blue sheen on dark glass, brighter toward the top of a pane
float3 sky = float3(0.055, 0.065, 0.075) * (0.6 + 0.6 * UV.y) * Day;
float3 lamp = float3(1.0, 0.82, 0.55) * (Glow * 18.0);
return sky * (1.0 - l) + lamp;
""", [("Glow", glow), ("Day", day), ("UV", _uv(g))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="LanternGlass")
    g.out(c, MP.MP_EMISSIVE_COLOR)
    op = g.custom("return 0.42 + 0.4 * saturate(Glow);", [("Glow", glow)], desc="GlassOpacity")
    g.out(op, MP.MP_OPACITY)
    return g.finish()


def lens_glass():
    g = S.Graph("M_South_LensGlass", dest=DEST)
    _set(g.m, blend_mode=unreal.BlendMode.BLEND_TRANSLUCENT, shading_model=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True,
         used_with_static_lighting=False)
    glow = g.param("Glow", 0.0)
    c = g.custom(r"""
float r = length(UV - 0.5) * 2.0;
// the bull's-eye's concentric prisms: bright rims, darker between
float rings = pow(0.5 + 0.5 * cos(r * 42.0), 6.0);
float3 day = float3(0.10, 0.12, 0.11) * (0.5 + 0.8 * rings);
float3 lit = float3(1.0, 0.85, 0.6) * Glow * (3.0 + 9.0 * rings) * (1.2 - 0.6 * saturate(r));
return day + lit;
""", [("Glow", glow), ("UV", _uv(g))], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="LensGlass")
    g.out(c, MP.MP_EMISSIVE_COLOR)
    g.out(g.custom("return 0.55 + 0.3 * saturate(Glow);", [("Glow", glow)], desc="LensOpacity"), MP.MP_OPACITY)
    return g.finish()


def beam():
    g = S.Graph("M_South_Beam", dest=DEST)
    _set(g.m, blend_mode=unreal.BlendMode.BLEND_ADDITIVE, shading_model=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True,
         used_with_static_lighting=False)
    try:
        g.m.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
    except Exception as e:  # noqa: BLE001
        _log("translucency_pass: %s" % e)
    strength = g.param("Strength", 0.0)
    depth = g.node(X.MaterialExpressionPixelDepth)
    c = g.custom(r"""
float along = saturate(UV.x);
float across = sin(3.14159 * saturate(UV.y));
// brightest just out of the lantern, gone at the far end; a soft core across the card
float fall = pow(1.0 - along, 1.6) * smoothstep(0.0, 0.015, along);
// near the camera the cards would show as planes: fade them out within ~25 m
float nearFade = saturate((Depth - 300.0) / 2200.0);
return float3(1.0, 0.86, 0.62) * (Strength * 0.9 * fall * across * across * nearFade);
""", [("Strength", strength), ("UV", _uv(g)), ("Depth", depth)], out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="Beam")
    g.out(c, MP.MP_EMISSIVE_COLOR)
    return g.finish()


def make_all():
    n = 0
    for fn in (lantern_glass, lens_glass, beam):
        try:
            fn()
            n += 1
        except Exception as e:  # noqa: BLE001
            _log("%s failed: %s" % (fn.__name__, e))
    unreal.EditorAssetLibrary.save_directory(DEST)
    _log("%d of 3 made" % n)
    return n


if __name__ == "__main__":
    make_all()
