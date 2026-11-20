"""The Rassvet's newsreel materials (docs/cinema.md), committed under /Game/Cinema (small, finished):

  M_FilmScreen  the film's picture laid on the hall's screen (ULyudCinema): unlit, two-sided, emissive = the "Film" texture
                (a UMediaTexture at run time) x Brightness x EmissiveGain, a soft vignette and the old projector's flicker
                (18 steps a second, a few per cent), so even a clean scan reads as projected light
  M_FilmBeam    the projector's beam from the booth's porthole to the screen (a pyramid ULyudCinema builds, UV.y 0 at the
                booth, 1 on the screen): translucent additive unlit, two-sided, dust drifting through it (world-space noise),
                strongest at the booth, soft at its side faces, gone within a metre and a half of the camera (no wall of light
                when the hero's head is in it). Intensity: 0 off, 1 a film running

Run headless on the Mac (one run, via tools/wait_ue_queue.sh):
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_cinema_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_sea_materials as S  # noqa: E402

DEST = "/Game/Cinema"
X = unreal
MP = unreal.MaterialProperty
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

SCREEN = r"""
float2 p = UV - 0.5;
float vig = saturate(1.0 - dot(p, p) * 1.2);
float h = frac(sin(floor(T * 18.0) * 12.9898) * 43758.5453);
float flicker = 0.95 + 0.05 * h;
return Img * Brightness * Gain * vig * flicker;
"""

BEAM = r"""
float3 q = WP * 0.035 + float3(T * 0.07, T * 0.05, T * 0.11);
float3 i = floor(q), f = frac(q);
f = f * f * (3.0 - 2.0 * f);
float n000 = frac(sin(dot(i, float3(127.1, 311.7, 74.7))) * 43758.5453);
float n100 = frac(sin(dot(i + float3(1, 0, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
float n010 = frac(sin(dot(i + float3(0, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
float n110 = frac(sin(dot(i + float3(1, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
float n001 = frac(sin(dot(i + float3(0, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
float n101 = frac(sin(dot(i + float3(1, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
float n011 = frac(sin(dot(i + float3(0, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
float n111 = frac(sin(dot(i + float3(1, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
float n = lerp(lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y), lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y), f.z);
float dust = 0.55 + 0.9 * n * n;
float along = lerp(1.0, 0.35, saturate(UV.y));
float side = sin(saturate(UV.x) * 3.14159);
float nearCam = saturate((distance(WP, Cam) - 60.0) / 90.0);
return float3(1.0, 0.93, 0.80) * Intensity * Strength * dust * along * (0.35 + 0.65 * side) * nearCam;
"""


def screen():
    g = S.Graph("M_FilmScreen", unreal.MaterialShadingModel.MSM_UNLIT, dest=DEST)
    g.m.set_editor_property("two_sided", True)
    tex = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="Film",
                 texture=unreal.load_asset("/Engine/EngineResources/Black"))
    uv = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=0)
    t = g.node(X.MaterialExpressionTime)
    em = g.custom(SCREEN, [("Img", (tex, "RGB")), ("UV", uv), ("T", t), ("Brightness", g.param("Brightness", 1.0)),
                           ("Gain", g.param("EmissiveGain", 2.0))], out=F3, desc="FilmPicture")
    g.out(em, MP.MP_EMISSIVE_COLOR)
    return g.finish()


def beam():
    g = S.Graph("M_FilmBeam", unreal.MaterialShadingModel.MSM_UNLIT, dest=DEST)
    g.m.set_editor_property("two_sided", True)
    g.m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    wp = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=S.EXCL)
    cam = g.node(X.MaterialExpressionCameraPositionWS)
    uv = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=0)
    t = g.node(X.MaterialExpressionTime)
    em = g.custom(BEAM, [("WP", wp), ("Cam", cam), ("UV", uv), ("T", t), ("Intensity", g.param("Intensity", 1.0)),
                         ("Strength", g.param("Strength", 0.06))], out=F3, desc="FilmBeam")
    g.out(em, MP.MP_EMISSIVE_COLOR)
    return g.finish()


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST):
        unreal.EditorAssetLibrary.make_directory(DEST)
    screen()
    beam()
    unreal.log_warning("CINEMA materials: M_FilmScreen, M_FilmBeam saved under " + DEST)


main()
