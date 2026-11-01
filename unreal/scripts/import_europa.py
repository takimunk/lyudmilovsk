"""The hatch to Europa into the project (docs/europa.md): design/europa/game/europa_kit.glb (blender/europa/europa_kit.py)
-> /Game/Europa/Meshes/SM_EU_*, textures/europa/*.png (tools/europa/europa_textures.py) -> /Game/Europa/Textures, and the
materials -> /Game/Europa/Materials. Run headless (unreal/scripts/import_europa.sh):

  UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_europa.py

Materials: M_EU_Surface (the kit's tiling tx_* surfaces: colour + roughness in one texture, metre UVs / tile), M_EU_Ice
(Europa's ice: two scales of the ice texture against tiling, its normal map), M_EU_Water (the room's water, translucent),
M_EU_Screen (the airlock computer: the water gauge and a status lamp, drawn from Level and Mode), M_EU_Planet (the planet
over Europa: ocean, island and clouds lit by SunDir / SunLight, a blue rim of air). The glTF's own flat materials stay on
the small parts (rubber, the yellow rungs, the red wheels, the lamps). Nanite off; complex collision on what is walked on.
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(REPO, "design", "europa", "game", "europa_kit.glb")
TEX_SRC = os.path.join(REPO, "textures", "europa")
ROOT = "/Game/Europa"
MESHES = ROOT + "/Meshes"
TEX = ROOT + "/Textures"
MAT = ROOT + "/Materials"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
X = unreal
TILE_M = {"paint": 2.0, "steel": 1.0, "concrete": 2.0, "algae": 1.0, "ice": 16.0}
METAL = {"steel": 0.7}


def log(msg):
    unreal.log_warning("EUROPA " + msg)


def import_textures():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    names = sorted(f for f in os.listdir(TEX_SRC) if f.startswith("T_EU_") and f.endswith(".png"))
    tasks = []
    for f in names:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(TEX_SRC, f)
        t.destination_path = TEX
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    for f in names:
        a = eal.load_asset(TEX + "/" + f[:-4])
        if f.endswith("_N.png"):
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            a.set_editor_property("srgb", False)
        elif f == "T_EU_clouds.png":
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
            a.set_editor_property("srgb", False)
        if "planet" in f or "clouds" in f:
            a.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        eal.save_loaded_asset(a)
    return len(names)


class Graph:
    """A new material and small helpers for wiring its nodes (as import_sky.py's)."""

    def __init__(self, name, shading=None, blend=None, two_sided=False):
        if eal.does_asset_exist(MAT + "/" + name):
            eal.delete_asset(MAT + "/" + name)
        self.m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
        if shading is not None:
            self.m.set_editor_property("shading_model", shading)
        if blend is not None:
            self.m.set_editor_property("blend_mode", blend)
        self.m.set_editor_property("two_sided", two_sided)
        self.y = 0

    def node(self, cls, x=-600, **props):
        self.y += 120
        e = mel.create_material_expression(self.m, cls, x, self.y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, src, dst, pin="", src_pin=""):
        if not mel.connect_material_expressions(src, src_pin, dst, pin):
            raise RuntimeError("%s: no pin %r on %s" % (self.m.get_name(), pin, dst.get_name()))

    def op(self, cls, a, b, a_pin="", b_pin=""):
        e = self.node(cls, -300)
        self.link(a, e, "A", a_pin)
        self.link(b, e, "B", b_pin)
        return e

    def scalar(self, name, v):
        return self.node(X.MaterialExpressionScalarParameter, -900, parameter_name=name, default_value=float(v))

    def vector(self, name, v):
        return self.node(X.MaterialExpressionVectorParameter, -900, parameter_name=name, default_value=unreal.LinearColor(*v))

    def custom(self, code, inputs, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
        c = self.node(X.MaterialExpressionCustom, -450, code=code, output_type=out)
        ins = []
        for name, _ in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", name)
            ins.append(ci)
        c.set_editor_property("inputs", ins)
        for name, src in inputs:
            s, p = src if isinstance(src, tuple) else (src, "")
            self.link(s, c, name, p)
        return c

    def out(self, src, prop, src_pin=""):
        if not mel.connect_material_property(src, src_pin, prop):
            raise RuntimeError("%s: could not connect %s" % (self.m.get_name(), prop))

    def finish(self):
        mel.recompile_material(self.m)
        eal.save_loaded_asset(self.m)
        return self.m


def tex(name):
    return eal.load_asset(TEX + "/" + name)


def surface_master():
    g = Graph("M_EU_Surface")
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    k = g.scalar("TileInv", 0.5)
    uvs = g.op(X.MaterialExpressionMultiply, uv, k)
    t = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="BaseTex", texture=tex("T_EU_paint_C"))
    g.link(uvs, t, "UVs")
    tint = g.vector("Tint", (1, 1, 1, 1))
    g.out(g.op(X.MaterialExpressionMultiply, t, tint, "RGB", ""), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(t, unreal.MaterialProperty.MP_ROUGHNESS, "A")
    g.out(g.scalar("Metallic", 0.0), unreal.MaterialProperty.MP_METALLIC)
    return g.finish()


def ice_master():
    g = Graph("M_EU_Ice")
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    near = g.op(X.MaterialExpressionMultiply, uv, g.scalar("TileInv", 1.0 / 16.0))
    far = g.op(X.MaterialExpressionMultiply, uv, g.scalar("FarInv", 1.0 / 237.0))
    c = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="IceTex", texture=tex("T_EU_ice_C"))
    g.link(near, c, "UVs")
    c2 = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="IceTexFar", texture=tex("T_EU_ice_C"))
    g.link(far, c2, "UVs")
    n = g.node(X.MaterialExpressionTextureSampleParameter2D, parameter_name="IceNormal", texture=tex("T_EU_ice_N"),
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    g.link(near, n, "UVs")
    # the far sample's brightness swings the near one: no 16 m grid on the plain
    mix = g.custom("float k = lerp(0.82, 1.12, saturate((F.r - 0.6) * 3.0 + 0.5)); return C.rgb * k * Tint.rgb;",
                   [("C", (c, "RGBA")), ("F", (c2, "RGBA")), ("Tint", (g.vector("Tint", (1, 1, 1, 1)), "RGBA"))])
    g.out(mix, unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(c, unreal.MaterialProperty.MP_ROUGHNESS, "A")
    g.out(n, unreal.MaterialProperty.MP_NORMAL, "RGB")
    g.out(g.scalar("Specular", 0.6), unreal.MaterialProperty.MP_SPECULAR)
    return g.finish()


def water():
    g = Graph("M_EU_Water", blend=unreal.BlendMode.BLEND_TRANSLUCENT, two_sided=True)
    g.m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    t = g.node(X.MaterialExpressionTime)
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    pan = g.custom("return UV * 3.0 + float2(T * 0.03, T * 0.017);", [("UV", uv), ("T", t)],
                   out=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    n = g.node(X.MaterialExpressionTextureSample, texture=tex("T_EU_ice_N"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    g.link(pan, n, "UVs")
    g.out(g.vector("Color", (0.04, 0.16, 0.15, 1)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar("Opacity", 0.62), unreal.MaterialProperty.MP_OPACITY)
    g.out(g.scalar("Roughness", 0.06), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(n, unreal.MaterialProperty.MP_NORMAL, "RGB")
    return g.finish()


SCREEN_HLSL = r"""
float3 col = float3(0.008, 0.03, 0.026);
float2 g = abs(frac(UV * float2(16.0, 10.0)) - 0.5);
col += 0.02 * step(0.46, max(g.x, g.y));
float3 mc = Mode < 0.5 ? float3(0.15, 0.55, 1.0) : (Mode > 1.5 && Mode < 2.5) ? float3(0.25, 1.0, 0.4) : float3(1.0, 0.6, 0.12);
bool busy = (Mode > 0.5 && Mode < 1.5) || Mode > 2.5;
// the gauge: the water in the lock, a column on the left
float2 gu = (UV - float2(0.07, 0.1)) / float2(0.16, 0.8);
if (gu.x > 0 && gu.x < 1 && gu.y > 0 && gu.y < 1)
{
    float fill = step(1.0 - gu.y, Level);
    col = lerp(float3(0.015, 0.04, 0.04), mc * 0.8, fill);
    if (abs(frac(gu.y * 10.0) - 0.5) > 0.46) col *= 0.4;
}
float2 gb = abs(UV - float2(0.15, 0.5)) - float2(0.085, 0.405);
col += mc * 0.5 * step(max(gb.x, gb.y), 0.0) * step(-0.008, max(gb.x, gb.y));
// the level's trace across the screen, a little wave on it while it runs
float2 pu = (UV - float2(0.32, 0.38)) / float2(0.6, 0.5);
if (pu.x > 0 && pu.x < 1 && pu.y > 0 && pu.y < 1)
{
    float h = Level + (busy ? 0.015 * sin(pu.x * 30.0 - T * 4.0) : 0.0);
    col += mc * smoothstep(0.025, 0.0, abs((1.0 - pu.y) - h));
    col += mc * 0.12 * step(1.0 - pu.y, h);
}
// the status lamp: steady when it rests, blinking while it works
float blink = busy ? step(0.5, frac(T * 1.4)) : 1.0;
float d = length((UV - float2(0.83, 0.2)) * float2(1.6, 1.0));
col += mc * smoothstep(0.075, 0.05, d) * (0.3 + 1.5 * blink);
return col * Gain;
"""


def screen():
    g = Graph("M_EU_Screen", shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    c = g.custom(SCREEN_HLSL, [("UV", uv), ("Level", g.scalar("Level", 1.0)), ("Mode", g.scalar("Mode", 0.0)),
                               ("T", g.node(X.MaterialExpressionTime)), ("Gain", g.scalar("Gain", 6.0))])
    g.out(c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return g.finish()


PLANET_HLSL = r"""
// the normal from the sphere's centre (whatever the mesh's own normals and winding make of it)
float3 n = normalize(P - C);
float3 l = normalize(SunDir.xyz);
float3 v = normalize(V);
float ndl = dot(n, l);
float3 alb = Albedo.SampleLevel(AlbedoSampler, UV, 0).rgb;
float c = Clouds.SampleLevel(CloudsSampler, UV + float2(T * 0.0004, 0.0), 0).r;
float3 surf = lerp(alb, float3(0.93, 0.94, 0.96), c);
float lit = saturate(ndl);
float3 col = surf * SunLight.rgb * lit;
// the air over the day side: the blue a whole ocean world shows from afar (Rayleigh), strongest toward the limb
float mu = saturate(dot(n, v));
col += float3(0.05, 0.11, 0.26) * SunLight.rgb * saturate(ndl + 0.1) * (0.6 + 0.8 * (1.0 - mu));
// the sun's glint on the open sea
float3 h = normalize(l + v);
col += SunLight.rgb * pow(saturate(dot(n, h)), 160.0) * (1.0 - c) * 0.35 * step(0.0, ndl);
// a thin blue rim of air, lit on the day side and a little past the terminator
float rim = pow(1.0 - saturate(dot(n, v)), 4.0);
col += float3(0.25, 0.5, 1.0) * rim * SunLight.rgb * 0.45 * saturate(ndl + 0.3);
return col * Gain;
"""


def planet():
    g = Graph("M_EU_Planet", shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    uv = g.node(X.MaterialExpressionTextureCoordinate)
    pos = g.node(X.MaterialExpressionWorldPosition)
    ctr = g.node(X.MaterialExpressionObjectPositionWS)
    cam = g.node(X.MaterialExpressionCameraVectorWS)
    alb = g.node(X.MaterialExpressionTextureObjectParameter, parameter_name="Albedo", texture=tex("T_EU_planet_C"))
    clo = g.node(X.MaterialExpressionTextureObjectParameter, parameter_name="Clouds", texture=tex("T_EU_clouds"))
    c = g.custom(PLANET_HLSL, [("UV", uv), ("P", pos), ("C", ctr), ("V", cam), ("SunDir", (g.vector("SunDir", (0.3, 0.2, 0.9, 0)), "RGBA")),
                               ("SunLight", (g.vector("SunLight", (12.7, 10.5, 8.6, 0)), "RGBA")), ("T", g.node(X.MaterialExpressionTime)),
                               ("Gain", g.scalar("Gain", 2.2)), ("Albedo", alb), ("Clouds", clo)])
    g.out(c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return g.finish()


def instance(name, parent, scalars=None, textures=None, vectors=None):
    path = MAT + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MAT, unreal.MaterialInstanceConstant,
                                                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def main():
    log("textures: %d" % import_textures())
    surf = surface_master()
    ice = ice_master()
    slots = {}
    for name, tile in TILE_M.items():
        if name == "ice":
            continue
        slots["tx_" + name] = instance("MI_EU_" + name, surf, {"TileInv": 1.0 / tile, "Metallic": METAL.get(name, 0.0)},
                                       {"BaseTex": tex("T_EU_%s_C" % name)})
    # Europa reflects most of the light, but a pure white plain under a sun with no air to soften it blows out: the
    # albedo held to ~0.6 of the texture's
    slots["tx_ice"] = instance("MI_EU_ice", ice, vectors={"Tint": (0.62, 0.64, 0.68, 1)})
    # frosted steel and ice-crusted concrete for the collar on Europa
    ice_crust = instance("MI_EU_ice_crust", ice, {"TileInv": 1.0 / 2.0, "FarInv": 1.0 / 9.0}, vectors={"Tint": (0.55, 0.58, 0.62, 1)})
    slots["EU_screen"] = screen()
    slots["EU_water"] = water()
    slots["EU_planet"] = planet()

    if not os.path.isfile(KIT):
        log("no %s: keeping the committed meshes" % KIT)
        return
    tmp = MESHES + "/europa_kit"
    if eal.does_directory_exist(tmp):
        eal.delete_directory(tmp)
    src = unreal.InterchangeManager.create_source_data(KIT)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    mgr.import_scene(MESHES, src, params)
    mgr.wait_until_all_tasks_done(True)
    walk = {"SM_EU_Airlock", "SM_EU_Collar", "SM_EU_Lid", "SM_EU_Console", "SM_EU_ShaftB", "SM_EU_Ground", "SM_EU_IceBlocks"}
    n = 0
    for path in eal.list_assets(tmp, True):
        a = eal.load_asset(path)
        if not isinstance(a, unreal.StaticMesh):
            continue
        name = a.get_name()
        dest = MESHES + "/" + name
        if eal.does_asset_exist(dest):
            eal.delete_asset(dest)
        eal.rename_asset(path, dest)
        a = eal.load_asset(dest)
        s = a.get_editor_property("nanite_settings")
        s.enabled = False
        a.set_editor_property("nanite_settings", s)
        body = a.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if name in walk
                                     else unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        mats = a.get_editor_property("static_materials")
        for i, m in enumerate(mats):
            slot = str(m.get_editor_property("material_slot_name"))
            key = next((k for k in slots if slot.startswith(k)), None)
            if key:
                # (setting the struct in the array copy doesn't stick: set_material does)
                a.set_material(i, slots[key])
            else:
                mi = m.get_editor_property("material_interface")
                # glTF's metalness is right only on steel
                if isinstance(mi, unreal.MaterialInstanceConstant):
                    for pname in mel.get_scalar_parameter_names(mi):
                        if "etal" in str(pname):
                            mel.set_material_instance_scalar_parameter_value(mi, str(pname), 0.0)
                    mel.update_material_instance(mi)
                    eal.save_loaded_asset(mi)
        eal.save_loaded_asset(a)
        log("mesh %s: %d triangles, slots %s" % (name, a.get_num_triangles(0), ["%s=%s" % (m.get_editor_property("material_slot_name"), m.get_editor_property("material_interface").get_name()) for m in a.get_editor_property("static_materials")]))
        n += 1
    # (Europa's collar is the sea's with MI_EU_ice_crust on its concrete and crust: ALyudEuropaSite sets it)
    if eal.does_asset_exist(MESHES + "/SM_EU_IceCollar"):
        eal.delete_asset(MESHES + "/SM_EU_IceCollar")
    _ = ice_crust
    eal.save_directory(ROOT)
    # what Interchange left in the level (the scene's actors) and the empty import folder
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for act in eas.get_all_level_actors():
        if act.get_name().startswith("SM_EU_") or "europa_kit" in str(act.get_folder_path()):
            eas.destroy_actor(act)
    log("kit: %d meshes into %s" % (n, MESHES))


main()
