"""The public spaces' one material (docs/city/public_spaces.md), committed under /Game/City (small):

  M_CityProp   every code-built prop and paving block (Private/City/CityKit.cpp): colour from the vertex colour (stored
               sRGB, linearised here), roughness in its alpha; the surface pattern from UV1 (x = kind: 1 slabs, 2 setts,
               3 boards, 4 masonry courses, 5 gravel, 6 rubber; y = slab size m) drawn over UV0 (metres on the surface);
               lamp glass glows from UV2.x x the Glow parameter (the lamps' MID, 0 by day, 1 at night)

  M_City_Atlas the interiors' picture atlas (blender/city/interior_kit.py, slot CT_Atlas): textures/city/atlas_D.jpg
               as T_City_Atlas_D (sRGB), untinted and untiled, under /Game/City/Materials; import_city.py makes it too

ALyudPublicSpaces sets Glow. Run headless:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_city_materials.py -unattended
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_sea_materials as S  # noqa: E402

DEST = "/Game/City"
X = unreal
MP = unreal.MaterialProperty
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

COMMON = r"""
struct FCityNoise
{
    float H(float2 c) { return frac(sin(dot(c, float2(127.1, 311.7))) * 43758.5453); }
    float N(float2 p)
    {
        float2 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(H(i), H(i + float2(1, 0)), f.x), lerp(H(i + float2(0, 1)), H(i + float2(1, 1)), f.x), f.y);
    }
    // 0 on a slab, 1 in a joint of width w (metres) on a grid of cells c (metres), rows offset by o (fraction)
    float Joint(float2 uv, float2 c, float w, float o, out float2 cell)
    {
        float2 g = uv / c;
        g.x += o * floor(g.y);
        cell = floor(g);
        float2 d = min(frac(g), 1.0 - frac(g)) * c;     // metres to the nearest edge in each axis
        float e = min(d.x, d.y);
        return 1.0 - smoothstep(w * 0.4, w, e);
    }
};
FCityNoise Nz;
"""

# out: linear colour; in: VC (vertex colour, sRGB rgb, roughness a), K (UV1: kind, tile), UV (UV0 metres), WP (world cm)
COLOR = COMMON + r"""
float3 c = pow(saturate(VC.rgb), 2.2);
float kind = K.x, t = max(K.y, 0.05);
float2 cell = 0;
float j = 0.0, tone = 0.0;
float2 wp = WP.xy * 0.01;
if (kind > 0.5 && kind < 1.5)        // slabs: t x t, every other row half-offset when t >= 1
{
    j = Nz.Joint(UV, float2(t, t), 0.012, t >= 1.0 ? 0.0 : 0.5, cell);
    tone = Nz.H(cell + floor(wp * 0.001)) - 0.5;
    c *= 1.0 + tone * 0.14;
    c *= 1.0 - 0.07 * smoothstep(0.55, 0.9, Nz.N(UV * 3.1 + 7.0));     // stains
    c *= 1.0 - 0.55 * j;
}
else if (kind > 1.5 && kind < 2.5)   // granite setts: t x 2t, rows offset by half
{
    j = Nz.Joint(UV, float2(t * 1.6, t), 0.012, 0.5, cell);
    tone = Nz.H(cell) - 0.5;
    c *= 1.0 + tone * 0.35;
    c *= 1.0 - 0.6 * j;
}
else if (kind > 2.5 && kind < 3.5)   // boards along x, t wide, butt joints every ~3 m staggered
{
    float2 bc;
    j = Nz.Joint(UV.yx, float2(t, 3.1), 0.008, 0.37, bc);
    tone = Nz.H(bc) - 0.5;
    float grain = Nz.N(float2(UV.x * 1.5, UV.y * 40.0)) - 0.5;
    c *= (1.0 + tone * 0.25) * (1.0 + grain * 0.18);
    c *= 1.0 - 0.6 * j;
}
else if (kind > 3.5 && kind < 4.5)   // masonry courses: t high, blocks of 2.5 t staggered
{
    j = Nz.Joint(UV, float2(t * 2.5, t), 0.01, 0.5, cell);
    tone = Nz.H(cell) - 0.5;
    c *= 1.0 + tone * 0.12;
    c *= 1.0 - 0.4 * j;
}
else if (kind > 4.5 && kind < 5.5)   // gravel: speckle
{
    float n = Nz.N(wp * 60.0) * 0.6 + Nz.N(wp * 180.0) * 0.4;
    c *= 0.82 + 0.36 * n;
    c *= 1.0 - 0.1 * smoothstep(0.6, 0.85, Nz.N(wp * 0.7));         // tracks and damp patches
}
else if (kind > 5.5 && kind < 6.5)   // poured rubber
{
    c *= 0.92 + 0.12 * Nz.N(wp * 120.0);
}
// every surface: slow weathering in world space so repeats don't read
c *= 0.94 + 0.12 * Nz.N(wp * 0.13 + 3.0);
return c;
"""

ROUGH = COMMON + r"""
float kind = K.x, t = max(K.y, 0.05);
float2 cell;
float j = 0.0;
if (kind > 0.5 && kind < 1.5) j = Nz.Joint(UV, float2(t, t), 0.012, t >= 1.0 ? 0.0 : 0.5, cell);
else if (kind > 1.5 && kind < 2.5) j = Nz.Joint(UV, float2(t * 1.6, t), 0.012, 0.5, cell);
return saturate(VC.a + 0.15 * j);
"""


def prop():
    g = S.Graph("M_CityProp", dest=DEST)
    vc = g.node(X.MaterialExpressionVertexColor)
    uv0 = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=0)
    uv1 = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=1)
    uv2 = g.node(X.MaterialExpressionTextureCoordinate, coordinate_index=2)
    wp = g.node(X.MaterialExpressionWorldPosition, world_position_shader_offset=S.EXCL)
    # the vertex colour with its alpha: an Append of RGB and A (a Custom input takes one pin)
    rgba = g.node(X.MaterialExpressionAppendVector)
    g.link(vc, rgba, "A", "")
    g.link(vc, rgba, "B", "A")
    col = g.custom(COLOR, [("VC", rgba), ("K", uv1), ("UV", uv0), ("WP", wp)], out=F3, desc="CityColour")
    rough = g.custom(ROUGH, [("VC", rgba), ("K", uv1), ("UV", uv0)], out=F1, desc="CityRough")
    g.out(col, MP.MP_BASE_COLOR)
    g.out(rough, MP.MP_ROUGHNESS)
    # lamp glass: warm, a glow over the base colour; Glow (0..1) x GlowStrength
    glow = g.param("Glow", 0.0)
    strength = g.param("GlowStrength", 30.0)
    em = g.custom("return UV2.x * Glow * Strength * float3(1.0, 0.72, 0.42);",
                  [("UV2", uv2), ("Glow", glow), ("Strength", strength)], out=F3, desc="LampGlow")
    g.out(em, MP.MP_EMISSIVE_COLOR)
    m = g.m
    for k in ("used_with_instanced_static_meshes",):
        try:
            m.set_editor_property(k, True)
        except Exception as e:  # noqa: BLE001
            unreal.log_warning("CITY MATERIALS %s: %s" % (k, e))
    return g.finish()


REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
ATLAS = os.path.join(REPO, "textures", "city", "atlas_D.jpg")


def atlas(dest=DEST + "/Materials"):
    """M_City_Atlas: the picture atlas on UV0 as it is, no tint, no tiling, no flip. Orientation: the kit writes
    Blender UVs with v up from the image's bottom row (v = 1 - y / 4096); the glTF export stores 1 - v, and Unreal
    samples from the top row, so the cells land upright with no flip here. Cells have 6 px of edge padding: the
    default mips are fine at room distances (a little bleed from the neighbouring cell past ~20 m)."""
    import academ_materials as AM
    tex = AM.import_image(ATLAS, dest, "T_City_Atlas_D")
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    S.eal.save_loaded_asset(tex, False)
    return AM.build_picture(dest + "/M_City_Atlas", tex)


def build():
    if not S.eal.does_directory_exist(DEST):
        S.eal.make_directory(DEST)
    prop()
    atlas()
    unreal.log_warning("CITY MATERIALS done")


if __name__ == "__main__":
    build()
